// FILE: Interpreter.Parser.Stmt.Isam.cs
/*
------------------------------------------------------------------------------------------------------------------------
      GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI ISAM (LEGACY PREFISSATA E QUALIFICATA FUSA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file supportano due stili di sintassi per ogni comando,
garantendo la retrocompatibilità e introducendo un'API moderna per il database ISAM.

La sintassi "Legacy Prefissata" è quella tradizionale del BASIC, preceduta dalla keyword `ISAM`.
La sintassi "Moderna" o "Qualificata fusa" usa `ISAM.VERBOFUSO` SENZA parentesi (direttiva AGENTS.md §2.2):
il verbo è fuso in un unico identificatore e le clausole (OF/ALIAS/INDEXED BY/TO/FOR/ON/INTO/...) sono
le STESSE della forma legacy multi-parola. I comandi che nella forma qualificata fusa omettono il
numero di work area operano sull'area attiva (handle -1).

--- Esempi di Doppia Sintassi ---

1.  APERTURA E SELEZIONE
  Legacy:  ISAM USE "clienti.isdb" OF TCliente ALIAS Clienti INDEXED BY ID
  Moderna: ISAM.USE "clienti.isdb" OF TCliente ALIAS Clienti INDEXED BY ID

  Legacy:  ISAM SELECT Clienti
  Moderna: ISAM.SELECT Clienti

  Legacy:  ISAM CLOSE 1
  Moderna: ISAM.CLOSE 1        ' o ISAM.CLOSE (area attiva)

2.  NAVIGAZIONE (la forma qualificata fusa opera sull'area attiva)
  Legacy:  ISAM GO 1, TOP
  Moderna: ISAM.GO TOP | ISAM.GO 5

  Legacy:  ISAM SKIP 1, -1
  Moderna: ISAM.SKIP -1

  Legacy:  ISAM SEEK 1, "key" USING "idx_name"
  Moderna: ISAM.SEEK "key" USING "idx_name"

3.  LETTURA E SCRITTURA DATI
  Legacy:  ISAM GET 1, recordVar
  Moderna: ISAM.GET recordVar

  Legacy:  ISAM PUT 1, recordVar UNSAFE
  Moderna: ISAM.PUT recordVar UNSAFE

4.  MODIFICA DATI
  Legacy:  ISAM APPEND Alias, nuovoRecord
  Moderna: ISAM.APPEND nuovoRecord

  Legacy:  ISAM DELETE 1
  Moderna: ISAM.DELETE [FOR Id > 100]

  Legacy:  ISAM RECALL 1 FOR Id > 100
  Moderna: ISAM.RECALL FOR Id > 100

5.  MANUTENZIONE E BACKUP
  Legacy:  ISAM PACK 1        | Moderna: ISAM.PACK
  Legacy:  ISAM REINDEX 1     | Moderna: ISAM.REINDEX
  Legacy:  ISAM VALIDATE 1    | Moderna: ISAM.VALIDATE
  Legacy:  ISAM REPAIR 1      | Moderna: ISAM.REPAIR
  Legacy:  ISAM BACKUP 1 TO "clienti.bak" | Moderna: ISAM.BACKUP "clienti.bak"

6.  INDICIZZAZIONE E ORDINAMENTO
  Legacy:  ISAM CREATE INDEX "idx_citta" ON 1 KEY ("UPPER(Citta)") LEN 30
  Moderna: ISAM.CREATEINDEX "idx_citta" ON 1 KEY ("UPPER(Citta)") LEN 30

  Legacy:  ISAM DROP INDEX "idx_citta" ON 1
  Moderna: ISAM.DROPINDEX "idx_citta" ON 1

7.  IMPORT / EXPORT / COPIA / CONVERSIONE
  Legacy:  ISAM IMPORT 1 FROM "data.csv" AS CSV
  Moderna: ISAM.IMPORT 1 FROM "data.csv" AS CSV

  Legacy:  ISAM EXPORT 1 TO "data.json" AS JSON
  Moderna: ISAM.EXPORT 1 TO "data.json" AS JSON

  Legacy:  ISAM COPY TO "new.isdb" ON 1 FOR Saldo > 0
  Moderna: ISAM.COPYTO "new.isdb" [FOR Saldo > 0]

  Legacy:  ISAM SORT TO "sorted.isdb" ON 1 ON Cognome, Nome
  Moderna: ISAM.SORTTO "sorted.isdb" ON Cognome, Nome

  Legacy:  ISAM CONVERT DBF "s.dbf" TO ISAM "d.isdb" TYPE TCliente KEY "ID" LEN 8
  Moderna: ISAM.CONVERT DBF "s.dbf" TO ISAM "d.isdb" TYPE TCliente KEY "ID" LEN 8

8.  LOCKING E TRANSAZIONI
  Legacy:  ISAM LOCK 1, RECORD
  Moderna: ISAM.LOCK [RECORD]

  Legacy:  ISAM UNLOCK 1
  Moderna: ISAM.UNLOCK [RECORD]

  Legacy:  ISAM BEGIN TRANSACTION
  Moderna: ISAM.BEGINTRANSACTION

9.  COMANDI SET
  Legacy:  ISAM SET ORDER TO "IndiceNome" ON 1
  Moderna: ISAM.SETORDER [TO] "IndiceNome"

  Legacy:  ISAM SET FILTER TO Saldo > 0 ON 1
  Moderna: ISAM.SETFILTER [TO] Saldo > 0

  Legacy:  ISAM SET RELATION ON 1 TO IdCliente INTO Ordini
  Moderna: ISAM.SETRELATION [TO] IdCliente INTO Ordini

10. JOIN
  Legacy:  ISAM JOIN WITH Ordini TO "joined.isdb" ON 1 FOR Clienti.Id = Ordini->CustomerId
  Moderna: ISAM.JOINWITH Ordini TO "joined.isdb" FOR Clienti.Id = Ordini->CustomerId

NOTA: Tutti i comandi del namespace ISAM supportano entrambe le sintassi. Le funzioni che
restituiscono un valore (EOF, RECNO, LASTREC, ...) restano con parentesi: ISAM.EOF(X).
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

// ReSharper disable UnusedParameter.Local
// ReSharper disable InvalidXmlDocComment
// ReSharper disable InconsistentNaming
// ReSharper disable UnusedParameter.Global
namespace Basic;

/// <summary>
/// Contiene i metodi di parsing dedicati per tutte le istruzioni del namespace ISAM.
/// Ogni metodo è "contestuale": sa di operare per il namespace ISAM e valida
/// la sintassi di conseguenza. La forma qualificata fusa (es. `ISAM.GO TOP`) usa le
/// STESSE clausole della forma legacy multi-parola, senza parentesi (AGENTS.md §2.2).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del sottosistema ISAM che iniziano con la keyword 'ISAM'.
    /// Gestisce esclusivamente la sintassi legacy non qualificata (es. `ISAM USE ...`, `ISAM DELETE ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' della chiamata legacy.</param>
    /// <returns>Il nodo AST per l'istruzione ISAM parsata.</returns>
    private static Stmt ParseIsamStatement(Parser parser, string expectedNamespace)
    {
        // Consuma la keyword 'ISAM' che ha attivato questo dispatcher.
        parser.Consume(TokenKind.Isam, "Internal parser error: Expected 'ISAM' keyword.");

        var commandToken = parser.Peek();

        // Disambigua i comandi composti basandosi sul token successivo.
        // ReSharper disable once SwitchStatementHandlesSomeKnownEnumValuesWithDefault
        return ParseIsamSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Nuovo helper che agisce come sub-dispatcher per i comandi `SET` del namespace ISAM.
    /// Viene chiamato solo dopo che il parser ha già consumato la sequenza `ISAM SET`.
    /// La sua unica responsabilità è leggere il sottocomando (es. ORDER, FILTER) e
    /// invocare il parser finale corretto.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' della chiamata legacy.</param>
    /// <returns>Il nodo Stmt corrispondente al comando SET di ISAM parsato.</returns>
    private static Stmt ParseIsamSetSubCommand(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Set, "Internal parser error: Expected 'SET'.");
        var subCommandToken = parser.Peek();
        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return subCommandToken.Kind switch
        {
            TokenKind.Order => ParseIsamSetOrderStatement(parser, expectedNamespace),
            TokenKind.Filter => ParseIsamSetFilterStatement(parser, expectedNamespace),
            TokenKind.Relation => ParseIsamSetRelationStatement(parser, expectedNamespace),
            TokenKind.Default => ParseDbfSetDefaultStatement(parser, expectedNamespace),
            TokenKind.Path => ParseDbfSetPathStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando '{subCommandToken.Lexeme}' non riconosciuto per ISAM SET.")
        };
    }

    /// <summary>
    /// Parsa la clausola `INDEXED BY`, comune ai motori ISAM e SPEED, che definisce
    /// la chiave primaria e gli indici secondari di una tabella.
    /// </summary>
    /// <remarks>
    /// Sintassi: `INDEXED BY field1 [UNSAFE], field2 [UNSAFE], ...`
    /// <br/>
    /// Il primo campo specificato viene sempre trattato come la chiave primaria. I campi successivi
    /// vengono considerati indici secondari. La keyword opzionale `UNSAFE` disabilita i controlli
    /// di consistenza per massimizzare le performance di scrittura, a rischio di inconsistenza dei dati.
    /// </remarks>
    /// <example>
    /// // Esempio per il motore ISAM
    /// USE "customers.isdb" OF TCustomer INDEXED BY CustomerID, LastName
    /// 
    /// // Esempio per il motore SPEED, con un indice primario UNSAFE
    /// SPEED USE "logs" FROM 1 OF TLog INDEXED BY LogID UNSAFE, Timestamp
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <returns>Un nodo AST <see cref="IsamIndexedByClause"/> che rappresenta la clausola.</returns>
    private static IsamIndexedByClause ParseIsamIndexedByClause(Parser parser)
    {
        var keyFields = ImmutableArray.CreateBuilder<IsamIndexField>();
        do
        {
            // La chiave ora può essere un'espressione complessa, non solo un identificatore.
            var keyExpression = parser.ParseExpression();

            // Il flag UNSAFE può essere specificato per ogni singola chiave.
            var isUnsafe = parser.Match(TokenKind.Unsafe);

            // La sintassi INDEXED BY non supporta ASC/DESC, che è specifico per SORT.
            // Pertanto, IsDescending è sempre false in questo contesto.
            keyFields.Add(new IsamIndexField(keyExpression, isUnsafe, IsDescending: false));
        } while (parser.Match(TokenKind.Comma));

        return new IsamIndexedByClause(keyFields.ToImmutable());
    }
    internal static Stmt ParseIsamSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Create => parser.At(1).Kind != TokenKind.Index
                ? throw CreateError(parser.At(1), "Expected INDEX after CREATE.")
                : ParseIsamCreateIndexStatement(parser, expectedNamespace),
            TokenKind.Drop => parser.At(1).Kind != TokenKind.Index
                ? throw CreateError(parser.At(1), "Expected INDEX after DROP.")
                : ParseIsamDropIndexStatement(parser, expectedNamespace),
            TokenKind.Set =>
                // Per la sintassi legacy, invochiamo il dispatcher di SET che userà il lookahead.
                ParseIsamSetSubCommand(parser, expectedNamespace),
            // Per i comandi a parola singola, invoca direttamente il parser.
            TokenKind.Open => ParseIsamOpenStatement(parser, expectedNamespace),
            TokenKind.Use => ParseIsamUseStatement(parser, expectedNamespace),
            TokenKind.Select => ParseIsamSelectStatement(parser, expectedNamespace),
            TokenKind.Close => ParseIsamCloseStatement(parser, expectedNamespace),
            TokenKind.Go or TokenKind.Goto => ParseIsamGoStatement(parser, expectedNamespace),
            TokenKind.Skip => ParseIsamSkipStatement(parser, expectedNamespace),
            TokenKind.Seek => ParseIsamSeekStatement(parser, expectedNamespace),
            TokenKind.Get => ParseIsamGetStatement(parser, expectedNamespace),
            TokenKind.Put => ParseIsamPutStatement(parser, expectedNamespace),
            TokenKind.Pack => ParseIsamPackStatement(parser, expectedNamespace),
            TokenKind.Reindex => ParseIsamReindexStatement(parser, expectedNamespace),
            TokenKind.Repair => ParseIsamRepairStatement(parser, expectedNamespace),
            TokenKind.Delete => ParseIsamDeleteStatement(parser, expectedNamespace),
            TokenKind.Recall => ParseIsamRecallStatement(parser, expectedNamespace),
            TokenKind.Validate => ParseIsamValidateStatement(parser, expectedNamespace,
                parser.Peek()) // Peek() è placeholder
            ,
            TokenKind.Begin => ParseBeginTransactionStatement(parser, expectedNamespace),
            TokenKind.Commit => ParseCommitStatement(parser),
            TokenKind.Rollback => ParseRollbackStatement(parser),
            TokenKind.Import => ParseIsamImportStatement(parser, expectedNamespace),
            TokenKind.Export => ParseIsamExportStatement(parser, expectedNamespace),
            TokenKind.ConvDoc => ParseIsamConvertStatement(parser, expectedNamespace),
            TokenKind.Backup => ParseIsamBackupStatement(parser, expectedNamespace),
            TokenKind.Lock => ParseIsamLockStatement(parser, expectedNamespace),
            TokenKind.Unlock => ParseIsamUnlockStatement(parser, expectedNamespace),
            TokenKind.Copy => ParseIsamCopyToStatement(parser, expectedNamespace),
            TokenKind.Join => ParseIsamJoinStatement(parser, expectedNamespace),
            TokenKind.Sort => ParseIsamSortToStatement(parser, expectedNamespace),
            TokenKind.Total => ParseIsamTotalOnStatement(parser, expectedNamespace),
            TokenKind.Append => ParseIsamAppendStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken,
                $"Comando '{commandToken.Lexeme}' non riconosciuto per il motore ISAM.")
        };
    }



    /// <summary>
    /// Parsa la variante ISAM dell'istruzione OPEN.
    /// Questa versione potenziata gestisce `SHARED`/`EXCLUSIVE`, `UNSAFE`, `ENCODING`,
    /// e la sintassi ambigua `AS handle` / `ALIAS alias`.
    /// Restituisce il nuovo nodo AST `IsamOpenStmt`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo <see cref="IsamOpenStmt"/> che rappresenta il comando.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM OPEN "clienti.isdb" EXCLUSIVE UNSAFE OF Cliente AS hnd ALIAS Clienti INDEXED BY ID
    /// 
    /// ' Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.OPEN "clienti.isdb" OF Cliente ALIAS Clienti INDEXED BY ID
    /// </code>
    /// </example>
    /// <summary>
    /// Parsa la variante ISAM dell'istruzione OPEN.
    /// Questa versione potenziata gestisce `SHARED`/`EXCLUSIVE`, `UNSAFE`, `ENCODING`,
    /// la sintassi qualificata e rende 'ALIAS' obbligatorio.
    /// L'handle numerico è stato rimosso.
    /// </summary>
    public static Stmt ParseIsamOpenStatement(Parser parser, string expectedNamespace)
    {
        var openToken = parser.Consume(TokenKind.Open, "Internal parser error: Expected 'OPEN'.");

        Expr fileName;
        Token udtNameToken;
        Token? aliasToken = null;
        IsamIndexedByClause? indexedBy = null;
        var dbLockingMode = DatabaseLockingMode.Default;
        Expr? encodingExpr = null;
        var isUnsafe = false;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): OPEN fileName [options] OF typeName [ALIAS alias] [INDEXED BY ...] ---
        fileName = parser.ParseExpression();

        // Loop per clausole opzionali prima di 'OF'
        while (!parser.Check(TokenKind.Of))
        {
            if (parser.IsAtEndOfStatement()) break;
            if (parser.Match(TokenKind.Shared)) dbLockingMode = DatabaseLockingMode.Shared;
            else if (parser.Match(TokenKind.Exclusive)) dbLockingMode = DatabaseLockingMode.Exclusive;
            else if (parser.Match(TokenKind.Unsafe)) isUnsafe = true;
            else if (parser.Match(TokenKind.Encoding)) encodingExpr = parser.ParseExpression();
            else break;
        }

        parser.Consume(TokenKind.Of, "Expected 'OF' to specify a user-defined TYPE.");
        udtNameToken = parser.Consume(TokenKind.Identifier, "Expected a TYPE name after 'OF'.");

        // Loop per le clausole dopo 'OF'
        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Alias))
            {
                if (aliasToken != null) throw CreateError(parser.Previous(), "'ALIAS alias' already specified.");
                aliasToken = parser.Consume(TokenKind.Identifier, "Expected alias name after ALIAS.");
            }
            else if (parser.Match(TokenKind.Indexed))
            {
                if (indexedBy != null) throw CreateError(parser.Previous(), "'INDEXED BY' already specified.");
                parser.Consume(TokenKind.By, "Expected 'BY' after 'INDEXED'.");
                indexedBy = ParseIsamIndexedByClause(parser);
            }
            else break;
        }

        if (aliasToken == null)
        {
            throw CreateError(openToken, "La clausola 'ALIAS' è obbligatoria per ISAM OPEN.");
        }

        return new IsamOpenStmt(
            FileName: fileName,
            OriginalTypeName: udtNameToken,
            OriginalAliasToken: aliasToken,
            Locking: dbLockingMode,
            IsUnsafe: isUnsafe,
            IndexedBy: indexedBy,
            EncodingExpr: encodingExpr,
            Token: openToken
        );
    }

    /// <summary>
    /// Parsa il comando `USE` per aprire un file ISAM in una work area con alias.
    /// Questo comando è specifico del namespace ISAM e gestisce sia la sintassi
    /// non qualificata (legacy) che quella qualificata fusa senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "ISAM") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="IsamUseStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM USE "clienti.isdb" OF Cliente SHARED UNSAFE ALIAS Clienti
    /// ISAM USE "ordini.isdb" OF Ordine EXCLUSIVE INDEXED BY IdCliente, DataOrdine, GetScore(ID) UNSAFE
    /// 
    /// ' Sintassi Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.USE "clienti.isdb" OF Cliente SHARED UNSAFE ALIAS Clienti
    /// </code>
    /// </example>
    public static Stmt ParseIsamUseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Use, "Expected 'USE'.");
        Expr fileName;
        Token typeNameToken;
        // Target? handle = null; // RIMOSSO
        IsamIndexedByClause? indexedBy = null;
        Token? aliasToken = null;
        var dbLockingMode = DatabaseLockingMode.Default;
        var isUnsafe = false;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): USE fileName OF typeName [options] ALIAS alias [options] ---
        fileName = parser.ParseExpression();

        parser.Consume(TokenKind.Of, "Expected 'OF' to specify a TYPE for ISAM file.");
        typeNameToken = parser.Consume(TokenKind.Identifier, "Expected a TYPE name after 'OF'.");

        // Loop per clausole opzionali in ordine flessibile
        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Shared)) dbLockingMode = DatabaseLockingMode.Shared;
            else if (parser.Match(TokenKind.Exclusive)) dbLockingMode = DatabaseLockingMode.Exclusive;
            else if (parser.Match(TokenKind.Unsafe)) isUnsafe = true;
            else if (parser.Match(TokenKind.Indexed))
            {
                parser.Consume(TokenKind.By, "Expected 'BY' after 'INDEXED'.");
                indexedBy = ParseIsamIndexedByClause(parser);
            }
            else if (parser.Match(TokenKind.Alias))
            {
                aliasToken = parser.Consume(TokenKind.Identifier, "Expected alias name after ALIAS.");
            }
            else
            {
                break;
            }
        }

        if (aliasToken == null)
        {
            throw CreateError(commandToken, "La clausola 'ALIAS' è obbligatoria per ISAM USE.");
        }

        return new IsamUseStmt(fileName, -1, typeNameToken, dbLockingMode, isUnsafe, indexedBy, -1,
            aliasToken, commandToken);
    }

    /// <summary>
    /// Parsa la variante ISAM del comando `SELECT` per cambiare la work area attiva.
    /// Questo comando è specifico del namespace ISAM.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "ISAM") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="IsamSelectWorkAreaStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM SELECT 1
    /// ISAM SELECT Clienti
    /// 
    /// ' Sintassi Qualificata fusa (senza parentesi)
    /// ISAM.SELECT 1
    /// ISAM.SELECT Clienti
    /// </code>
    /// </example>
    public static Stmt ParseIsamSelectStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Select, "Internal parser error.");

        Expr workArea;
        // Sintassi unificata (legacy e qualificata fusa): SELECT workArea
        // Un identificatore dopo SELECT è un alias (stringa), non una variabile.
        var next = parser.Peek();
        if (next.Kind == TokenKind.Identifier)
        {
            parser.Advance();
            workArea = new StringExpr(next.Lexeme, next);
        }
        else
        {
            workArea = parser.ParseExpression();
        }

        return new IsamSelectWorkAreaStmt(workArea, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione CLOSE per il namespace ISAM.
    /// Gestisce sia la sintassi legacy prefissata (`ISAM CLOSE ...`) che quella moderna qualificata (`ISAM.CLOSE ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST di tipo <see cref="CloseStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy Prefissata
    /// ISAM CLOSE 1, 2
    /// 
    /// ' Moderna Qualificata fusa
    /// ISAM.CLOSE 1, 2
    /// ISAM.CLOSE ' chiude l'area attiva
    /// </code>
    /// </example>
    public static CloseStmt ParseIsamCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Internal parser error: Expected 'CLOSE'.");
        var fileNumbers = ImmutableArray.CreateBuilder<Expr>();

        // Sintassi unificata (legacy e qualificata fusa): CLOSE [area1, area2, ...]
        if (parser.IsAtEndOfStatement())
            return new CloseStmt(fileNumbers.ToImmutable(), commandToken, IsIsam: true);

        do
        {
            fileNumbers.Add(parser.ParseExpression());
        } while (parser.Match(TokenKind.Comma));

        return new CloseStmt(fileNumbers.ToImmutable(), commandToken, IsIsam: true);
    }

    /// <summary>
    /// Parsa un'istruzione SKIP per il namespace ISAM, gestendo la doppia sintassi.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM SKIP 1
    /// ISAM SKIP 1, 10
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.SKIP 10
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamSkipStmt"/>.</returns>
    public static IsamSkipStmt ParseIsamSkipStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Skip, "Internal parser error: Expected 'SKIP'.");

        // Sintassi legacy: SKIP fileNumber [, recordCount]
        var fileNumber = parser.ParseExpression();
        Expr? recordCount = null;
        if (parser.Match(TokenKind.Comma))
        {
            recordCount = parser.ParseExpression();
        }

        return new IsamSkipStmt(fileNumber, recordCount, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione GO per il namespace ISAM per posizionare il cursore.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM GO 1, TOP
    /// ISAM GO 2, BOTTOM
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.GO TOP
    /// ISAM.GO 5
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamRecordStmt"/>, usato da ISAM per il posizionamento.</returns>
    public static IsamRecordStmt ParseIsamGoStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: GO fileNumber, TOP | BOTTOM
        var commandTokenRef = parser.Consume(TokenKind.Go, TokenKind.Goto);
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number in GO statement.");
        var location = parser.Consume(TokenKind.Top, TokenKind.Bottom);

        return new IsamRecordStmt(fileNumber, location, commandTokenRef);
    }

    /// <summary>
    /// Parsa un'istruzione SEEK specifica per il namespace ISAM, gestendo la doppia sintassi.
    /// QUESTA VERSIONE È STATA CORRETTA per gestire l'argomento `indexName` opzionale in
    /// entrambe le sintassi (qualificata fusa e legacy `USING`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamSeekStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM SEEK 1, "CHIAVE"
    /// ISAM SEEK 1, "Rossi" USING "NomeIdx"
    /// 
    /// ' Moderna Qualificata fusa (opera sull'area attiva)
    /// ISAM.SEEK "CHIAVE"
    /// ISAM.SEEK "Rossi" USING "NomeIdx"
    /// </code>
    /// </example>
    public static IsamSeekStmt ParseIsamSeekStatement(Parser parser, string expectedNamespace)
    {
        var seekToken = parser.Consume(TokenKind.Seek, "Internal parser error: Expected 'SEEK'.");

        // Sintassi legacy: SEEK fileNumber, position [USING indexName]
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");
        var position = parser.ParseExpression();
        Expr? indexName = null;
        if (parser.Match(TokenKind.Using))
        {
            indexName = parser.ParseExpression();
        }

        return new IsamSeekStmt(fileNumber, position, indexName, seekToken);
    }

    /// <summary>
    /// Parsa un'istruzione GET per il namespace ISAM, che legge il record corrente.
    /// Gestisce sia la sintassi legacy prefissata (`ISAM GET ...`) che quella moderna qualificata (`ISAM.GET ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST <see cref="GetFileStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi ISAM:</b>
    /// <code>
    /// ' Legacy Prefissata
    /// ISAM GET 1, mioRecord
    /// 
    /// ' Moderna Qualificata fusa (opera sull'area attiva)
    /// ISAM.GET mioRecord
    /// </code>
    /// </example>
    public static GetFileStmt ParseIsamGetStatement(Parser parser, string expectedNamespace)
    {
        var getToken = parser.Consume(TokenKind.Get, "Internal parser error: Expected 'GET'.");

        // Sintassi legacy: ISAM GET fileNumber, destination
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");
        var destination = parser.ParseTarget();

        // Per ISAM, recordNumber è sempre null.
        return new GetFileStmt(fileNumber, null, destination, getToken) { IsIsam = true };
    }

    /// <summary>
    /// Parsa un'istruzione PUT per il namespace ISAM, che scrive o aggiorna il record corrente.
    /// Gestisce sia la sintassi legacy prefissata (`ISAM PUT ...`) che quella moderna qualificata (`ISAM.PUT ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST <see cref="PutFileStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi ISAM:</b>
    /// <code>
    /// ' Legacy Prefissata
    /// ISAM PUT 1, mioRecord
    /// ISAM PUT 1, mioRecord UNSAFE
    /// 
    /// ' Moderna Qualificata fusa (opera sull'area attiva)
    /// ISAM.PUT mioRecord
    /// ISAM.PUT mioRecord UNSAFE
    /// </code>
    /// </example>
    public static IsamPutStmt ParseIsamPutStatement(Parser parser, string expectedNamespace)
    {
        var putToken = parser.Consume(TokenKind.Put, "Internal parser error: Expected 'PUT'.");

        // Sintassi unificata (legacy e qualificata fusa): PUT alias [, source]
        // Come SPEED PUT: `ISAM PUT Clienti, clienteRec` (alias esplicito) oppure
        // `ISAM PUT clienteRec` (source sull'area attiva, alias = -1).
        var firstExpr = parser.ParseExpression();
        Expr alias;
        Expr? source;
        if (parser.Match(TokenKind.Comma))
        {
            alias = firstExpr;
            source = parser.ParseExpression();
        }
        else
        {
            alias = new NumberExpr(-1, new Token(TokenKind.Number, "-1", -1.0, putToken.Position));
            source = firstExpr;
        }

        parser.Match(TokenKind.Unsafe); // accettato, ignorato

        return new IsamPutStmt(alias, source, putToken);
    }

    /// <summary>
    /// Parsa un'istruzione DELETE per il namespace ISAM, gestendo la sintassi qualificata e non,
    /// e una clausola di SCOPE opzionale.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM DELETE 1
    /// ISAM DELETE 1 FOR Id > 100
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.DELETE
    /// ISAM.DELETE FOR Id > 100
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamDeleteStmt"/>.</returns>
    public static IsamDeleteStmt ParseIsamDeleteStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Delete, "Internal parser error: Expected 'DELETE'.");

        // Sintassi legacy: DELETE fileNumber [SCOPE]
        var fileNumber = parser.ParseExpression();
        var scope = ParseScopeClause(parser);

        return new IsamDeleteStmt(fileNumber, scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione PACK per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM PACK 1
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.PACK
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamPackStmt"/>.</returns>
    public static IsamPackStmt ParseIsamPackStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Pack, "Internal parser error: Expected 'PACK'.");

        // Sintassi legacy: PACK fileNumber
        var fileNumber = parser.ParseExpression();

        return new IsamPackStmt(fileNumber, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione RECALL per il namespace ISAM, gestendo la sintassi qualificata e non,
    /// e una clausola di SCOPE opzionale.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM RECALL 1
    /// ISAM RECALL 1 FOR Id > 100
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.RECALL
    /// ISAM.RECALL FOR Id > 100
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamRecallStmt"/>.</returns>
    public static IsamRecallStmt ParseIsamRecallStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Recall, "Internal parser error: Expected 'RECALL'.");

        // Sintassi legacy: RECALL fileNumber [SCOPE]
        var fileNumber = parser.ParseExpression();
        var scope = ParseScopeClause(parser);

        return new IsamRecallStmt(fileNumber, scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione REINDEX per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM REINDEX 1
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva)
    /// ISAM.REINDEX
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamReindexStmt"/>.</returns>
    public static IsamReindexStmt ParseIsamReindexStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Reindex, "Internal parser error: Expected 'REINDEX'.");

        // Sintassi legacy: REINDEX fileNumber
        var fileNumber = parser.ParseExpression();

        return new IsamReindexStmt(fileNumber, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione REPAIR per il namespace ISAM.
    /// </summary>
    public static IsamRepairStmt ParseIsamRepairStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Repair, "Internal parser error: Expected 'REPAIR'.");

        // Sintassi legacy: REPAIR fileNumber
        var fileNumber = parser.ParseExpression();

        return new IsamRepairStmt(fileNumber, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione BACKUP per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM BACKUP 1 TO "backup.dat"
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva, TO opzionale)
    /// ISAM.BACKUP "backup.dat"
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamBackupStmt"/>.</returns>
    public static Stmt ParseIsamBackupStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: BACKUP fileNumber TO destinationFile
        var backupToken = parser.Consume(TokenKind.Backup, "Internal parser error: Expected 'BACKUP'.");
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after file number in BACKUP statement.");
        var destinationFile = parser.ParseExpression();

        return new IsamBackupStmt(fileNumber, destinationFile, backupToken);
    }

    /// <summary>
    /// Parsa un'istruzione `COPY TO` per il namespace ISAM.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamCopyToStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM COPY TO "backup.isdb" ON 1
    /// ISAM COPY TO "filtered.isdb" ON 1 FOR SALDO > 0
    /// 
    /// ' Moderna Qualificata fusa (opera sull'area attiva)
    /// ISAM.COPYTO "backup.isdb"
    /// ISAM.COPYTO "filtered.isdb" FOR SALDO > 0
    /// </code>
    /// </example>
    public static IsamCopyToStmt ParseIsamCopyToStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: COPY TO "file" ON fileNumber [FOR cond]
        var commandToken = parser.Consume(TokenKind.Copy, "Internal parser error: Expected 'COPY'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after COPY.");
        var destinationFile = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected 'ON' after destination file name.");
        var fileNumber = parser.ParseExpression();

        DbfForClause? scope = null;
        var scopeClause = ParseScopeClause(parser);
        if (scopeClause is DbfForClause forClause)
        {
            scope = forClause;
        }
        else if (scopeClause is not null)
        {
            throw CreateError(parser.Previous(), "Only a FOR clause is valid for the COPY TO command.");
        }

        return new IsamCopyToStmt(fileNumber, destinationFile, scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `JOIN WITH` per il namespace ISAM.
    /// </summary>
    public static IsamJoinWithStmt ParseIsamJoinStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: JOIN WITH alias TO "file" ON fileNumber FOR cond
        var commandToken = parser.Consume(TokenKind.Join, "Internal parser error: Expected 'JOIN'.");
        parser.Consume(TokenKind.With, "Expected 'WITH' after JOIN.");
        var secondaryAliasToken = parser.Consume(TokenKind.Identifier, "Expected alias of secondary work area.");
        parser.Consume(TokenKind.To, "Expected 'TO' after secondary alias.");
        var destinationFile = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected 'ON' after destination file name.");
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.For, "Expected 'FOR' clause to define join condition.");
        var condition = new DbfForClause(parser.ParseExpression());

        const int secondaryAliasSymbolId = -1;
        return new IsamJoinWithStmt(fileNumber, secondaryAliasSymbolId, secondaryAliasToken, destinationFile,
            condition, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `SORT TO` per il namespace ISAM.
    /// Accetta espressioni complesse, il flag UNSAFE per i campi, e la sintassi `ListLiteralExpr` `[...]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamSortToStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM SORT TO "sorted.isdb" ON 1 ON Cognome, Nome /D, GetLevel(ID) UNSAFE
    /// 
    /// ' Moderna
    /// ISAM.SORTTO(1, "sorted.isdb", ["Cognome", "Nome /D", "GetLevel(ID) UNSAFE"])
    /// </code>
    /// </example>
    public static IsamSortToStmt ParseIsamSortToStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: SORT TO destinationFile ON fileNumber ON fieldList [FOR condition]
        var commandToken = parser.Consume(TokenKind.Sort, "Internal parser error: Expected 'SORT'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after SORT.");
        var destinationFile = parser.ParseExpression();

        parser.Consume(TokenKind.On, "Expected 'ON' after destination file name.");
        var fileNumber = parser.ParseExpression();

        parser.Consume(TokenKind.On, "Expected 'ON' after file number.");

        var sortFields = ImmutableArray.CreateBuilder<IsamIndexField>();
        do
        {
            // In sintassi legacy, i campi sono espressioni dirette
            var keyExpression = parser.ParseExpression();
            var isUnsafe = parser.Match(TokenKind.Unsafe);

            // Gestione opzionale di /D per la discesa
            var isDescending = parser.Match(TokenKind.Divide) && parser.Match(TokenKind.Identifier) &&
                               parser.Previous().Lexeme.Equals("D", StringComparison.OrdinalIgnoreCase);

            sortFields.Add(new IsamIndexField(keyExpression, isUnsafe, isDescending));
        } while (parser.Match(TokenKind.Comma));

        DbfForClause? scope = null;
        var scopeClause = ParseScopeClause(parser);
        if (scopeClause is DbfForClause forClause)
        {
            scope = forClause;
        }
        else if (scopeClause is not null)
        {
            throw CreateError(parser.Previous(), "Only a FOR clause is valid for the SORT TO command.");
        }

        return new IsamSortToStmt(fileNumber, destinationFile, sortFields.ToImmutable(), scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione LOCK per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM LOCK 1
    /// ISAM LOCK 1, RECORD
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva, RECORD opzionale)
    /// ISAM.LOCK            ' File lock
    /// ISAM.LOCK RECORD     ' Record lock
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamLockStmt"/>.</returns>
    public static Stmt ParseIsamLockStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: LOCK fileNumber [, RECORD]
        var lockToken = parser.Consume(TokenKind.Lock, "Internal parser error: Expected 'LOCK'.");
        var fileNumber = parser.ParseExpression();
        var isRecordLock = parser.Match(TokenKind.Comma) && parser.Match(TokenKind.Record);

        return new IsamLockStmt(fileNumber, isRecordLock, lockToken);
    }

    /// <summary>
    /// Parsa un'istruzione UNLOCK per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT ISAM)
    /// ISAM UNLOCK 1
    /// ISAM UNLOCK 1, RECORD
    /// 
    /// ' Sintassi Qualificata fusa (opera sull'area attiva, RECORD opzionale)
    /// ISAM.UNLOCK          ' File lock
    /// ISAM.UNLOCK RECORD   ' Record lock
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamUnlockStmt"/>.</returns>
    public static Stmt ParseIsamUnlockStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: UNLOCK fileNumber [, RECORD]
        var unlockToken = parser.Consume(TokenKind.Unlock, "Internal parser error: Expected 'UNLOCK'.");
        var fileNumber = parser.ParseExpression();
        var isRecordLock = parser.Match(TokenKind.Comma) && parser.Match(TokenKind.Record);

        return new IsamUnlockStmt(fileNumber, isRecordLock, unlockToken);
    }

    /// <summary>
    /// Parsa un'istruzione APPEND per il namespace ISAM.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata
    /// ISAM APPEND Alias, udtVar   ' Append su area specificata da alias
    /// ISAM APPEND udtVar          ' Append su area attiva
    /// 
    /// ' Sintassi Qualificata
    /// ISAM.APPEND udtVar  ' Append su area attiva
    /// </code>
    /// </example>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamAppendStmt"/>.</returns>
    public static Stmt ParseIsamAppendStatement(Parser parser, string expectedNamespace)
    {
        var appendToken = parser.Consume(TokenKind.Append, "Internal parser error: Expected 'APPEND'.");

        // Sintassi legacy: APPEND [Alias,] sourceUdt (senza alias = area attiva via -1)
        var firstExpr = parser.ParseExpression();

        Expr fileNumber;
        Expr source;
        if (parser.Match(TokenKind.Comma))
        {
            // Caso: APPEND Alias, sourceUdt
            fileNumber = firstExpr;
            source = parser.ParseExpression();
        }
        else
        {
            // Caso: APPEND sourceUdt (su area attiva)
            fileNumber = new NumberExpr(-1, new Token(TokenKind.Number, "-1", -1.0, appendToken.Position));
            source = firstExpr;
        }

        return new IsamAppendStmt(fileNumber, source, appendToken);
    }

    /// <summary>
    /// Parsa l'istruzione SET ORDER specifica per il namespace ISAM.
    /// QUESTA VERSIONE È CORRETTA e analizza la sintassi `SET ORDER TO index ON handle`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token 'SET' o il token del namespace se la chiamata era `ISAM.SETORDER`.</param>
    /// <returns>Un nodo AST di tipo <see cref="IsamSetOrderStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// ISAM SET ORDER TO "CodiceCliente" ON 1
    /// ' Moderna Qualificata fusa (opera sull'area attiva, TO opzionale)
    /// ISAM.SETORDER "CodiceCliente"
    /// ISAM.SETORDER TO 1
    /// </code>
    /// </example>
    public static IsamSetOrderStmt ParseIsamSetOrderStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (ISAM) SET ORDER TO index ON handle — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Order, "Internal error: Expected 'ORDER'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'ORDER'.");
        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected 'ON' after index name for an ISAM command.");
        var fileNumber = parser.ParseExpression();

        return new IsamSetOrderStmt(fileNumber, indexName, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione CREATE INDEX per il namespace ISAM.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamCreateIndexStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM CREATE INDEX "idx_citta" ON 1 KEY ("UPPER(CITTA)") LEN 50
    /// 
    /// ' Moderna Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.CREATEINDEX "idx_citta" ON 1 KEY ("UPPER(CITTA)") LEN 50
    /// </code>
    /// </example>
    public static IsamCreateIndexStmt ParseIsamCreateIndexStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: CREATE INDEX indexName ON fileNumber KEY (expr) LEN n
        var createToken = parser.Consume(TokenKind.Create, "Internal parser error: Expected 'CREATE'.");
        parser.Consume(TokenKind.Index, "Expected 'INDEX' after CREATE.");

        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected 'ON' after index name.");
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Key, "Expected 'KEY' to specify the key expression.");
        parser.Consume(TokenKind.LeftParen, "Expected '(' before key expression.");
        var keyExpression = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after key expression.");
        parser.Consume(TokenKind.Len, "Expected 'LEN' to specify key length.");
        var keyLength = parser.ParseExpression();

        return new IsamCreateIndexStmt(fileNumber, indexName, keyExpression, keyLength, createToken);
    }

    /// <summary>
    /// Parsa un'istruzione DROP INDEX per il namespace ISAM, gestendo la doppia sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamDropIndexStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM DROP INDEX "idx_citta" ON Clienti
    /// 
    /// ' Moderna Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.DROPINDEX "idx_citta" ON Clienti
    /// </code>
    /// </example>
    public static IsamDropIndexStmt ParseIsamDropIndexStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: ISAM DROP INDEX indexName ON fileNumber
        var commandToken = parser.Consume(TokenKind.Drop, "Internal parser error: Expected 'DROP'.");
        parser.Consume(TokenKind.Index, "Expected 'INDEX' after DROP.");
        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected 'ON' after index name.");
        var fileNumber = parser.ParseExpression();

        return new IsamDropIndexStmt(fileNumber, indexName, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione EXPORT per il namespace ISAM.
    /// Gestisce sia la sintassi legacy prefissata (`ISAM EXPORT ...`) che quella moderna qualificata (`ISAM.EXPORT ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST di tipo <see cref="ExportToFileStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy Prefissata
    /// ISAM EXPORT 1 TO "clienti.json" AS JSON
    /// 
    /// ' Moderna Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.EXPORT 1 TO "clienti.json" AS JSON
    /// </code>
    /// </example>
    public static ExportToFileStmt ParseIsamExportStatement(Parser parser, string expectedNamespace)
    {
        var exportToken = parser.Consume(TokenKind.Export, "Internal parser error: Expected 'EXPORT'.");
        var engineType = new Token(TokenKind.Isam, "ISAM", null, exportToken.Position);
        Expr area;
        Expr filePath;
        Token format;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): EXPORT area TO "file" AS CSV | JSON ---
        area = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after file handle.");
        filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' to specify format.");
        format = parser.Consume(TokenKind.Csv, TokenKind.Json);

        return new ExportToFileStmt(engineType, area, filePath, format, exportToken);
    }

    /// <summary>
    /// Parsa un'istruzione IMPORT per il namespace ISAM.
    /// Gestisce sia la sintassi legacy prefissata (`ISAM IMPORT ...`) che quella moderna qualificata (`ISAM.IMPORT ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'ISAM' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST di tipo <see cref="ImportFromFileStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy Prefissata
    /// ISAM IMPORT 1 FROM "clienti.csv" AS CSV
    /// 
    /// ' Moderna Qualificata fusa (stesse clausole, senza parentesi)
    /// ISAM.IMPORT 1 FROM "clienti.csv" AS CSV
    /// </code>
    /// </example>
    public static ImportFromFileStmt ParseIsamImportStatement(Parser parser, string expectedNamespace)
    {
        var importToken = parser.Consume(TokenKind.Import, "Internal parser error: Expected 'IMPORT'.");
        var engineType = new Token(TokenKind.Isam, "ISAM", null, importToken.Position);
        Expr area, filePath;
        Token format;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): IMPORT area FROM "file" AS CSV | JSON ---
        area = parser.ParseExpression();
        parser.Consume(TokenKind.From, "Expected 'FROM' after the work area number in IMPORT statement.");
        filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' to specify format.");
        format = parser.Consume(TokenKind.Csv, TokenKind.Json);

        return new ImportFromFileStmt(engineType, area, filePath, format, importToken);
    }

    /// <summary>
    /// Parsa l'istruzione `CONVERT DBF "source" TO ISAM "dest" TYPE udt KEY "pk" LEN len`,
    /// gestendo la sintassi legacy e quella moderna qualificata, inclusa l'opzione `UNSAFE`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="ConvertDbfToIsamStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// ' Conversione base
    /// ISAM CONVERT DBF "source.dbf" TO ISAM "dest.isdb" TYPE Cliente KEY "ID" LEN 8
    /// 
    /// ' Conversione con valutazione chiave non sicura
    /// ISAM CONVERT DBF "source.dbf" TO ISAM "dest.isdb" TYPE Cliente KEY "ID" LEN 8 UNSAFE
    /// </code>
    /// </example>
    /// <example>
    /// Sintassi Moderna/Qualificata fusa (stesse clausole, senza parentesi):
    /// <code>
    /// ' Conversione base (safe)
    /// ISAM.CONVERT DBF "s.dbf" TO ISAM "d.isdb" TYPE Cliente KEY "ID" LEN 8
    /// 
    /// ' Conversione non sicura
    /// ISAM.CONVERT DBF "s.dbf" TO ISAM "d.isdb" TYPE Cliente KEY "ID" LEN 8 UNSAFE
    /// </code>
    /// </example>
    public static ConvertDbfToIsamStmt ParseIsamConvertStatement(Parser parser, string expectedNamespace)
    {
        Expr sourceFile, destFile, pkLen;
        Token typeNameToken, pkFieldToken;
        var isUnsafe = false;

        var convertToken = parser.Consume(TokenKind.ConvDoc, "Internal parser error: Expected 'CONVERT'.");

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): CONVERT DBF source TO ISAM dest TYPE udt KEY pk LEN n [UNSAFE] ---
        parser.Consume(TokenKind.Dbf, "Expected 'DBF' after CONVERT.");
        sourceFile = parser.ParseExpression();

        parser.Consume(TokenKind.To, "Expected 'TO' after source file.");
        parser.Consume(TokenKind.Isam, "Expected 'ISAM' after TO.");
        destFile = parser.ParseExpression();

        parser.Consume(TokenKind.Type, "Expected 'TYPE' to specify the UDT.");
        typeNameToken = parser.Consume(TokenKind.Identifier, "Expected a user-defined type name.");

        parser.Consume(TokenKind.Key, "Expected 'KEY' to specify the primary key.");

        if (parser.Check(TokenKind.StringLiteral))
        {
            var pkFieldExpr = parser.ParseExpression() as StringExpr;
            pkFieldToken = new Token(TokenKind.Identifier, pkFieldExpr!.Value, null, pkFieldExpr.Position);
        }
        else
        {
            pkFieldToken = parser.Consume(TokenKind.Identifier,
                "Expected a field name (as an identifier or a string) for the primary key.");
        }

        parser.Consume(TokenKind.Len, "Expected 'LEN' to specify the key length.");
        pkLen = parser.ParseExpression();

        isUnsafe = parser.Match(TokenKind.Unsafe);

        // In Fase 1, non risolviamo i simboli. Usiamo placeholder con ID fittizi (-1).
        // Il Linker in Fase 3 si occuperà della risoluzione e validazione.
        const int typeSymbolId = -1;
        const int pkFieldSymbolId = -1;

        return new ConvertDbfToIsamStmt(sourceFile, destFile, typeSymbolId, typeNameToken, pkFieldSymbolId,
            pkFieldToken, pkLen, isUnsafe, convertToken);
    }

    /// <summary>
    /// Parsa il comando `SET RELATION` specifico del namespace ISAM.
    /// VERSIONE CORRETTA: Gestisce correttamente la creazione e la rimozione di relazioni
    /// in entrambe le sintassi, legacy e qualificata.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del comando specifico (es. 'SET' o 'SETRELATION').</param>
    /// <returns>Un nodo AST <see cref="IsamSetRelationStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy (Creazione)
    /// ISAM SET RELATION ON 1 TO IdCliente INTO Ordini
    /// ' Legacy (Rimozione)
    /// ISAM SET RELATION ON 1 TO
    /// 
    /// ' Moderna Qualificata fusa (Creazione — opera sull'area attiva)
    /// ISAM.SETRELATION IdCliente INTO Ordini
    /// ' Moderna Qualificata fusa (Rimozione)
    /// ISAM.SETRELATION
    /// </code>
    /// </example>
    public static Stmt ParseIsamSetRelationStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (ISAM SET) RELATION ON fileNumber [TO key INTO alias]
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Relation, "Internal error: Expected 'RELATION'.");
        parser.Consume(TokenKind.On, "Expected 'ON' after 'RELATION'.");
        var fileNumber = parser.ParseExpression();

        if (!parser.Match(TokenKind.To) || parser.IsAtEndOfStatement())
            return new IsamSetRelationStmt(fileNumber, null, null, commandToken);

        var keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' after relation key expression.");
        var aliasToken = parser.Consume(TokenKind.Identifier, "Expected work area alias after 'INTO'.");
        return new IsamSetRelationStmt(fileNumber, keyExpr, aliasToken, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET FILTER TO [condizione] ON filenum` specifico del namespace ISAM.
    /// </summary>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IsamSetFilterStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM SET FILTER TO Prezzo > 100 ON 1
    /// ISAM SET FILTER TO ON 1  ' Rimuove il filtro
    ///
    /// ' Moderna Qualificata fusa (opera sull'area attiva, TO opzionale)
    /// ISAM.SETFILTER Prezzo > 100
    /// ISAM.SETFILTER ' Rimuove il filtro
    /// </code>
    /// </example>
    public static IsamSetFilterStmt ParseIsamSetFilterStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (ISAM) SET FILTER TO [condizione] ON filenum — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Filter, "Internal error: Expected 'FILTER'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'FILTER'.");
        Expr? condition = null;
        if (!parser.Check(TokenKind.On))
        {
            condition = parser.ParseExpression();
        }
        parser.Consume(TokenKind.On, "Expected 'ON' to specify the ISAM file number.");
        var fileNumber = parser.ParseExpression();

        return new IsamSetFilterStmt(fileNumber, condition, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `VALIDATE` per il namespace ISAM.
    /// </summary>
    /// <param name="expectedNamespace">Il namespace ("ISAM") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <param name="validateToken">Il token 'VALIDATE' originale, passato dal dispatcher polimorfico.</param>
    /// <returns>Un nodo AST <see cref="IsamValidateStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ISAM VALIDATE 1
    /// ISAM.VALIDATE(1)
    /// </code>
    /// </example>
    public static IsamValidateStmt ParseIsamValidateStatement(Parser parser, string expectedNamespace,
        Token validateToken)
    {
        Expr fileNumber;
        var commandTokenForAst = validateToken; // Usa il token passato dal dispatcher.

        // Sintassi unificata (legacy e qualificata fusa): VALIDATE fileNumber
        parser.Consume(TokenKind.Validate, "Expected 'VALIDATE'.");
        commandTokenForAst = parser.Previous();
        fileNumber = parser.ParseExpression();

        return new IsamValidateStmt(fileNumber, commandTokenForAst);
    }

    // ========================================================================
    // --- Parser per Comandi Comuni (Transazioni, I/O, Conversione) ---
    // ========================================================================

    /// <summary>
    /// Parsa un'istruzione `BEGIN TRANSACTION`.
    /// Gestisce correttamente sia la sintassi legacy (`BEGIN TRANSACTION...`)
    /// sia quella moderna qualificata (`DBF.BEGINTRANSACTION(...)`), inclusi tutti i livelli
    /// di isolamento in modo flessibile.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF" o "ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="BeginTransactionStmt"/>.</returns>
    public static Stmt ParseBeginTransactionStatement(Parser parser, string expectedNamespace)
    {
        var level = TransactionIsolationLevel.Default;

        // Sintassi legacy: BEGIN TRANSACTION [LEVEL ...]
        var commandToken = parser.Consume(TokenKind.Begin, "Internal parser error: Expected 'BEGIN'.");
        parser.Consume(TokenKind.Transaction, "Expected 'TRANSACTION' after 'BEGIN'.");

        if (!parser.Match(TokenKind.Level)) return new BeginTransactionStmt(level, commandToken);
        if (parser.Match(TokenKind.Read))
        {
            if (parser.Match(TokenKind.Committed))
            {
                level = TransactionIsolationLevel.ReadCommitted;
            }
            else if (parser.Match(TokenKind.Repeatable))
            {
                level = TransactionIsolationLevel.RepeatableRead;
            }
            else
            {
                throw CreateError(parser.Peek(), "Expected 'COMMITTED' or 'REPEATABLE' after 'READ'.");
            }
        }
        else if (parser.Match(TokenKind.Repeatable))
        {
            parser.Consume(TokenKind.Read, "Expected 'READ' after 'REPEATABLE'.");
            level = TransactionIsolationLevel.RepeatableRead;
        }
        else if (parser.Match(TokenKind.Serializable))
        {
            level = TransactionIsolationLevel.Serializable;
        }
        else
        {
            throw CreateError(parser.Peek(),
                "Expected an isolation level (e.g., READ COMMITTED, REPEATABLE READ, SERIALIZABLE) after 'LEVEL'.");
        }

        return new BeginTransactionStmt(level, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `COMMIT` per finalizzare una transazione.
    /// Gestisce sia la sintassi legacy non qualificata che quella moderna qualificata.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <returns>Un nodo AST <see cref="CommitStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM COMMIT
    /// DBF COMMIT
    /// 
    /// ' Moderna (ipotetica, per coerenza)
    /// DBF.COMMIT()
    /// ISAM.COMMIT()
    /// </code>
    /// </example>
    public static CommitStmt ParseCommitStatement(Parser parser)
    {
        var commitToken = parser.Consume(TokenKind.Commit, "Expected 'COMMIT'.");
        return new CommitStmt(commitToken);
    }

    /// <summary>
    /// Parsa un'istruzione `ROLLBACK` per annullare una transazione.
    /// Gestisce sia la sintassi legacy non qualificata che quella moderna qualificata.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <returns>Un nodo AST <see cref="RollbackStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// ISAM ROLLBACK
    /// DBF ROLLBACK
    /// 
    /// ' Moderna (ipotetica, per coerenza)
    /// DBF.ROLLBACK()
    /// ISAM.ROLLBACK()
    /// </code>
    /// </example>
    public static RollbackStmt ParseRollbackStatement(Parser parser)
    {
        var rollbackToken = parser.Consume(TokenKind.Rollback, "Expected 'ROLLBACK'.");
        return new RollbackStmt(rollbackToken);
    }

    /// <summary>ISAM TOTAL ON key TO file — delega a RDD CoreDb.TotalOn</summary>
    public static Stmt ParseIsamTotalOnStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Total, "Expected 'TOTAL'.");
        parser.Consume(TokenKind.On, "Expected 'ON' after TOTAL.");
        var keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after key expression.");
        var fileName = parser.ParseExpression();
        return new DbfTotalOnStmt(keyExpr, fileName, null, null, parser.Previous());
    }
}