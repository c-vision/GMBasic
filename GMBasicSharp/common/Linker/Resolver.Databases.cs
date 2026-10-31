// FILE: Linker/Resolver.Databases.cs — Specializzazioni database (DBF/ISAM/SPEED/LEDGER)
// Separate dal core condiviso Resolver.cs. Isolate per un'unificazione futura
// con compilazione condizionale (#if GMBASIC_DATABASES / GMBASIC_AUDIO).

using System.Collections.Immutable;
using System.Collections.Generic;
using System.Linq;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Risolve un'istruzione DbfUseStmt durante la Fase 2.5 (Resolver).
    /// Il suo compito principale è risolvere il nome del tipo specificato nella clausola `AS Tipo`
    /// nel suo `SymbolId` corrispondente e annotare l'AST con questo ID.
    /// Scende anche ricorsivamente per risolvere le espressioni standard come `FileName` e `IndexPaths`.
    /// </summary>
    /// <param name="stmt">Il nodo DbfUseStmt da risolvere.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Una nuova istanza di DbfUseStmt con il TypeSymbolId risolto e le espressioni interne risolte.</returns>
    /// <exception cref="BasicException">Lanciata se la clausola AS specifica un tipo non valido o non UDT.</exception>
    private static DbfUseStmt ResolveDbfUseStatement(DbfUseStmt stmt, BasicProgram program)
    {
        var resolvedTypeSymbolId = stmt.TypeSymbolId; // Mantieni -1 se non c'è AS

        if (stmt.OriginalTypeName != null)
        {
            var modulePath = program.FileIdToPathMap.GetValueOrDefault(stmt.Token.Position.FileId, string.Empty);
            var resolvedType = ResolveDataType(
                new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])),
                program,
                modulePath
            );

            if (resolvedType is UserDefinedType udt)
            {
                resolvedTypeSymbolId = udt.SymbolId;
            }
            else
            {
                // Se AS è specificato ma non è un UDT, è un errore.
                throw new BasicException(stmt.OriginalTypeName,
                    $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per DBF USE non è un tipo definito dall'utente (UDT) valido.",
                    ErrorCode.TypeMismatch);
            }
        }

        return stmt with
        {
            FileName = ResolveExpression(stmt.FileName, program, false),
            IndexPaths = [..stmt.IndexPaths.Select(p => ResolveExpression(p, program, false))],
            TypeSymbolId = resolvedTypeSymbolId // Inserisce l'ID del tipo nell'AST
        };
    }
    /// <summary>
    /// Helper specifico per risolvere tutte le sotto-espressioni all'interno di un'istruzione `IsamOpenStmt`.
    /// </summary>
    private static IsamOpenStmt ResolveIsamOpenStatement(IsamOpenStmt stmt, BasicProgram program)
    {
        // Enforcement: il tipo `OF <Type>` definisce un record a LUNGHEZZA FISSA →
        // rifiuta gli UDT con campi a lunghezza variabile (STRING/LIST/DICT/ANY).
        try
        {
            var _mp = program.FileIdToPathMap.GetValueOrDefault(stmt.Token.Position.FileId, string.Empty);
            var _rt = ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program, _mp);
            if (_rt is UserDefinedType _iudt && !_iudt.IsFixedSize)
                throw new BasicException(stmt.OriginalTypeName,
                    $"Il tipo '{_iudt.Name.Lexeme}' ha campi a lunghezza variabile (STRING/LIST/DICT/ANY) e non può essere usato come record a lunghezza fissa (ISAM OPEN).",
                    ErrorCode.TypeMismatch);
        }
        catch (BasicException) { throw; }
        catch { /* tipo non risolvibile qui: nessun enforcement */ }

        // Risolve le espressioni standard (non-DB context)
        var resolvedFileName = ResolveExpression(stmt.FileName, program, false);
        var resolvedEncoding = stmt.EncodingExpr != null ? ResolveExpression(stmt.EncodingExpr, program, false) : null;

        // Per INDEXED BY, le espressioni sono nomi di campo (UDF, non variabili).
        // NON usiamo inDatabaseContext: false perche' attiverebbe il late-binding
        // (_dbRecord#) che corrompe i nomi dei campi nello schema ISAM.
        IsamIndexedByClause? resolvedIndexedBy = null;
        if (stmt.IndexedBy != null)
        {
            resolvedIndexedBy = new IsamIndexedByClause(
                [
                    ..stmt.IndexedBy.KeyFields.Select(f =>
                        f with { KeyExpression = ResolveExpression(f.KeyExpression, program, inDatabaseContext: false) }
                    )
                ]
            );
        }

        return stmt with
        {
            FileName = resolvedFileName,
            EncodingExpr = resolvedEncoding,
            IndexedBy = resolvedIndexedBy
        };
    }
    /// <summary>
    /// Helper per il Resolver (Fase 2.5) che risolve un'istruzione `SpeedUseStmt`.
    /// Risolve il tipo UDT, aggiorna il TypeSymbolId e risolve le espressioni interne,
    /// **ma intenzionalmente non scende nella clausola `INDEXED BY`**, la cui risoluzione
    /// contestuale è demandata al Rewriter (Fase 3).
    /// </summary>
    private static SpeedUseStmt ResolveSpeedUseStatement(SpeedUseStmt stmt, BasicProgram program)
    {
        // FASE 2.5: Risolviamo il tipo di dato esplicito (clausola OF)
        // e lo usiamo per aggiornare il TypeSymbolId nel nodo AST.
        var modulePath = program.FileIdToPathMap.GetValueOrDefault(stmt.Token.Position.FileId, string.Empty);
        var resolvedType =
            ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program,
                modulePath);

        var typeSymbolId = resolvedType switch
        {
            UserDefinedType udt => udt.SymbolId,
            _ => -1
        };

        if (typeSymbolId == -1)
            throw new BasicException(stmt.OriginalTypeName,
                $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per SPEED USE non è un tipo definito dall'utente (UDT) valido.",
                ErrorCode.TypeMismatch);
        if (resolvedType is UserDefinedType uSpd && !uSpd.IsFixedSize)
            throw new BasicException(stmt.OriginalTypeName,
                $"Il tipo '{uSpd.Name.Lexeme}' ha campi a lunghezza variabile (STRING/LIST/DICT/ANY) e non può essere usato come record a lunghezza fissa (SPEED USE).",
                ErrorCode.TypeMismatch);

        // Ora scendiamo ricorsivamente per risolvere i SymbolId nelle sotto-espressioni,
        // MA ESCLUDIAMO la clausola `IndexedBy`. Lasciamo i SymbolId dei suoi campi a -1.
        // Questo è il passaggio chiave per evitare l'errore "Variabile non definita".
        return stmt with
        {
            TableName = ResolveExpression(stmt.TableName, program, false),
            DatabaseReference = ResolveExpression(stmt.DatabaseReference, program, false),
            // IndexedBy NON viene toccato qui.
            TypeSymbolId = typeSymbolId
        };
    }
    /// <summary>
    /// Risolve le espressioni in un'istruzione SpeedCreateIndexStmt,
    /// ma SALTA la KeyExpression, la cui risoluzione dipende dal contesto UDT
    /// che sarà disponibile solo nella fase di Rewriting (Fase 3).
    /// </summary>
    private static SpeedCreateIndexStmt ResolveSpeedCreateIndexStatement(SpeedCreateIndexStmt stmt,
        BasicProgram program)
    {
        return stmt with
        {
            IndexName = ResolveExpression(stmt.IndexName, program),
            Alias = ResolveExpression(stmt.Alias, program)
            // KeyExpression viene volutamente saltata.
        };
    }
    /// <summary>
    /// Risolve le espressioni in un'istruzione SpeedSetFilterStmt,
    /// ma SALTA l'espressione Condition, la cui risoluzione dipende dal contesto UDT.
    /// </summary>
    private static SpeedSetFilterStmt ResolveSpeedSetFilterStatement(SpeedSetFilterStmt stmt, BasicProgram program)
    {
        return stmt with
        {
            Alias = ResolveExpression(stmt.Alias, program)
            // Condition viene volutamente saltata.
        };
    }
    /// <summary>
    /// Risolve le espressioni in un'istruzione SpeedSetRelationStmt,
    /// ma SALTA la KeyExpression, la cui risoluzione dipende dal contesto UDT.
    /// </summary>
    private static SpeedSetRelationStmt ResolveSpeedSetRelationStatement(SpeedSetRelationStmt stmt,
        BasicProgram program)
    {
        return stmt with
        {
            ParentAlias = ResolveExpression(stmt.ParentAlias, program),
            ChildAlias = stmt.ChildAlias != null ? ResolveExpression(stmt.ChildAlias, program) : null
            // KeyExpression viene volutamente saltata.
        };
    }
    /// <summary>
    /// Risolve le espressioni contenute in una clausola di scope per comandi database (DBF/ISAM).
    /// Le clausole di scope (`FOR`, `WHILE`, `NEXT`) contengono condizioni o contatori che sono
    /// espressioni (`Expr`) e devono essere risolte per collegare variabili e funzioni.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Dispatching Polimorfico: Identifica il tipo specifico di clausola (<see cref="DbfForClause"/>, <see cref="DbfNextClause"/>, etc.).
    /// 2.  Risoluzione Espressioni:
    ///     - Per `FOR` e `WHILE`, risolve l'espressione booleana `Condition`.
    ///     - Per `NEXT`, risolve l'espressione numerica `Count`.
    ///     - Per `ALL` e `REST`, restituisce l'istanza originale (nessuna espressione da risolvere).
    /// 3.  Gestione Null: Se lo scope in input è `null` (clausola opzionale omessa), restituisce `null`.
    ///
    /// Questo metodo assicura che comandi come `DELETE FOR x > 10` abbiano la variabile `x`
    /// correttamente collegata al suo simbolo prima dell'esecuzione.
    /// </summary>
    /// <param name="scope">La clausola di scope da risolvere (opzionale).</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova istanza della clausola di scope con le espressioni risolte, o `null` se l'input era `null`.</returns>
    /// <param name="inDatabaseContext">True se l'espressione è all'interno di un comando DBF/ISAM/SPEED.</param>
    private static DbfScopeClause? ResolveDbfScope(DbfScopeClause? scope, BasicProgram program, bool inDatabaseContext)
    {
        if (scope == null) return null;
        return scope switch
        {
            DbfForClause c => c with { Condition = ResolveExpression(c.Condition, program, inDatabaseContext) },
            DbfWhileClause c => c with { Condition = ResolveExpression(c.Condition, program, inDatabaseContext) },
            DbfNextClause c => c with { Count = ResolveExpression(c.Count, program, false) },
            DbfRecordClause c => c with { RecordNumber = ResolveExpression(c.RecordNumber, program, false) },
            DbfAllClause => scope,
            DbfRestClause => scope,
            _ => throw new NotImplementedException($"ResolveDbfScope: Tipo sconosciuto {scope.GetType().Name}")
        };
    }
    /// <summary>
    /// Risolve le espressioni chiave all'interno di una clausola `INDEXED BY`.
    /// Questa clausola viene utilizzata nei comandi di apertura o definizione di indici ISAM/SPEED
    /// per specificare quali campi o espressioni compongono la chiave dell'indice.
    ///
    /// RESPONSABILITÀ:
    /// 1.  Iterazione Campi: Scorre la lista dei campi chiave (`KeyFields`) definiti nella clausola.
    /// 2.  Risoluzione Espressioni Chiave: Per ogni campo chiave, invoca <see cref="ResolveExpression"/>
    ///     sulla proprietà `KeyExpression`. Questo è cruciale perché una chiave può essere un'espressione
    ///     complessa (es. `UPPER(Nome) + STR(Eta)`), non solo un nome di campo.
    /// 3.  Ricostruzione Immutabile: Crea una nuova lista di campi chiave risolti e restituisce
    ///     una nuova istanza di <see cref="IsamIndexedByClause"/> che la contiene.
    /// 4.  Gestione Null: Se la clausola in input è `null`, restituisce `null` immediatamente.
    /// </summary>
    /// <param name="clause">La clausola `INDEXED BY` da risolvere (opzionale).</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova istanza della clausola con le espressioni chiave risolte, o `null`.</returns>
    private static IsamIndexedByClause? ResolveIsamIndexedBy(IsamIndexedByClause? clause, BasicProgram program)
    {
        if (clause == null) return null;
        return new IsamIndexedByClause(KeyFields:
        [
            ..clause.KeyFields
                .Select(f => f with { KeyExpression = ResolveExpression(f.KeyExpression, program) })
        ]);
    }
    /// <summary>
    /// Risolve ricorsivamente tutte le espressioni all'interno di un'istruzione `SPEED SELECT`.
    /// Questo include le espressioni nei campi selezionati, nelle clausole WHERE, GROUP BY, HAVING,
    /// ORDER BY e JOIN, nonché il target di destinazione (INTO).
    /// </summary>
    /// <param name="s">L'istruzione <see cref="SpeedSelectStmt"/> da risolvere.</param>
    /// <param name="program">L'istanza del programma contenente la SymbolTable.</param>
    /// <returns>Una nuova istanza di <see cref="SpeedSelectStmt"/> con tutte le sotto-espressioni risolte.</returns>
    private static SpeedSelectStmt ResolveSpeedSelect(SpeedSelectStmt s, BasicProgram program)
    {
        // Nota: Per le collezioni opzionali (SelectFields, GroupBy, OrderBy), dobbiamo gestire
        // il caso in cui siano null. Se sono null, le lasciamo null. Se non sono null, le risolviamo.
        // Usiamo ToImmutableArray() solo se la collezione originale esiste.

        return s with
        {
            // ReSharper disable once UseCollectionExpression
            SelectFields = s.SelectFields
                ?.Select(f => f with { Expression = ResolveExpression(f.Expression, program) }).ToImmutableArray(),

            IntoTarget = s.IntoTarget != null ? ResolveTarget(s.IntoTarget, program) : null,

            Join = s.Join != null
                ? s.Join with { Condition = ResolveExpression(s.Join.Condition, program) }
                : null,

            Where = s.Where != null ? ResolveExpression(s.Where, program) : null,

            // ReSharper disable once UseCollectionExpression
            GroupBy = s.GroupBy?.Select(e => ResolveExpression(e, program)).ToImmutableArray(),

            Having = s.Having != null ? ResolveExpression(s.Having, program) : null,

            // ReSharper disable once UseCollectionExpression
            OrderBy = s.OrderBy?.Select(o => o with { Expression = ResolveExpression(o.Expression, program) })
                .ToImmutableArray(),

            QueryExpression = s.QueryExpression != null ? ResolveExpression(s.QueryExpression, program) : null
        };
    }
}
