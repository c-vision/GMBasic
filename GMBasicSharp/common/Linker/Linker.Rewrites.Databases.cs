// FILE: Interpreter.Linker.Rewrites.Databases.cs

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Riscrive una clausola di scope DBF (FOR/WHILE), creando un contesto specializzato
    /// per risolvere correttamente i campi di database all'interno dell'espressione della clausola.
    /// È il punto di ingresso per la gestione del late-binding in comandi come DELETE, REPLACE, ecc.
    /// </summary>
    /// <param name="scope">La clausola di scope da riscrivere.</param>
    /// <param name="context">Il contesto di riscrittura generale.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>La clausola di scope riscritta con le espressioni risolte.</returns>
    private static DbfScopeClause? RewriteDbfScope(DbfScopeClause? scope, AstRewriteContext context,
        BasicProgram program)
    {
        if (scope == null) return null;

        // 1. Determina il contesto UDT per la clausola di scope basandosi sull'alias attivo.
        // Se non c'è un alias attivo o un tipo associato, udtSymbolId sarà il valore di default (0, che non è un ID valido) o non sarà trovata.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // 2. Crea un contesto di riscrittura specializzato per questa clausola.
        // Se udtSymbolId > 0, il contesto abilita la risoluzione statica dei campi.
        // Se udtSymbolId <= 0 ma IsInClipperMode è true, il contesto abilita il late-binding.
        var scopeContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // 3. Usa il nuovo 'scopeContext' per riscrivere le espressioni interne.
        return scope switch
        {
            DbfForClause c => c with { Condition = RewriteExpression(c.Condition, scopeContext, program) },
            DbfWhileClause c => c with { Condition = RewriteExpression(c.Condition, scopeContext, program) },
            // La clausola NEXT non contiene campi di database, usa il contesto originale.
            DbfNextClause c => c with { Count = RewriteExpression(c.Count, context, program) },
            // La clausola RECORD contiene solo un numero di record, usa il contesto originale.
            DbfRecordClause c => c with { RecordNumber = RewriteExpression(c.RecordNumber, context, program) },
            DbfAllClause => scope,
            DbfRestClause => scope,
            _ => throw new NotImplementedException($"RewriteDbfScope: Tipo sconosciuto {scope.GetType().Name}")
        };
    }

    // #################################################################################################################
    // DBF
    // #################################################################################################################
    
    /// <summary>
    /// Riscrive un'istruzione DbfUseStmt. Le sue responsabilità principali sono:
    /// 1. Registrare l'alias della work area come un simbolo `IsDynamicScope`.
    /// 2. Risolvere la clausola `AS Tipo` (se presente) per ottenere un `UdtSymbolId`.
    /// 3. Aggiornare il contesto di riscrittura (`AstRewriteContext`) con la nuova associazione `alias -> UdtSymbolId`.
    /// Se `AS Tipo` è assente, l'alias viene mappato a `-1` per segnalare una struttura sconosciuta a tempo di compilazione.
    /// Questo è il primo passo fondamentale per abilitare sia la risoluzione statica che il late-binding.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteDbfUseStatement(DbfUseStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Riscrive le espressioni interne (es. percorsi di file e indici).
        var rewrittenStmtPartial = stmt with
        {
            FileName = RewriteExpression(stmt.FileName, context, program),
            IndexPaths = [..stmt.IndexPaths.Select(arg => RewriteExpression(arg, context, program))]
        };

        var newContext = context;
        var aliasName = rewrittenStmtPartial.OriginalAliasToken?.Lexeme;

        // Logica di fallback: se ALIAS è omesso, usa il nome del file come alias (compatibilità Clipper).
        if (string.IsNullOrEmpty(aliasName))
        {
            if (rewrittenStmtPartial.FileName is StringExpr fileNameExpr)
            {
                aliasName = Path.GetFileNameWithoutExtension(fileNameExpr.Value);
            }
            else if (!rewrittenStmtPartial.IsNew)
            {
                throw CreateError(rewrittenStmtPartial.Token,
                    "La clausola ALIAS è obbligatoria quando il nome del file è un'espressione e non si usa NEW.");
            }
        }

        int? finalAliasSymbolId = null;
        Token? finalAliasToken = rewrittenStmtPartial.OriginalAliasToken;

        // Se abbiamo un nome di alias, ci assicuriamo che esista come simbolo dinamico.
        if (!string.IsNullOrEmpty(aliasName))
        {
            // Se l'alias è stato derivato (es. dal nome file), creiamo un token fittizio per esso.
            if (finalAliasToken == null)
            {
                finalAliasToken = new Token(TokenKind.Identifier, aliasName, aliasName, stmt.Position);
            }

            // Cerca se un simbolo per l'alias esiste già.
            var symbolIds = program.Symbols.FindSymbolInScope(aliasName);
            if (symbolIds == null || !symbolIds.Any())
            {
                // Se non esiste, lo registriamo come simbolo a scope dinamico.
                finalAliasSymbolId = program.Symbols.RegisterSymbol(
                    finalAliasToken,
                    SymbolFlags.IsDynamicScope,
                    new InferredType(), // Il tipo dell'alias in sé non è rilevante.
                    finalAliasToken
                );
            }
            else
            {
                // Se esiste già, usiamo il suo ID.
                finalAliasSymbolId = symbolIds[0];
            }
        }

        // Risolve la clausola `AS Tipo` per ottenere il SymbolId dell'UDT.
        var udtSymbolId = SymbolTable.UnresolvedSymbolId; // Default a -1 (sconosciuto)
        if (rewrittenStmtPartial.OriginalTypeName != null)
        {
            var modulePath = program.FileIdToPathMap.GetValueOrDefault(stmt.Token.Position.FileId, string.Empty);
            var resolvedType = ResolveDataType(
                new UnresolvedQualifiedType(new QualifiedNameExpr([rewrittenStmtPartial.OriginalTypeName])),
                program,
                modulePath
            );

            if (resolvedType is UserDefinedType udt)
            {
                if (!udt.IsFixedSize)
                    throw new BasicException(rewrittenStmtPartial.OriginalTypeName,
                        $"Il tipo '{udt.Name.Lexeme}' ha campi a lunghezza variabile (STRING/LIST/DICT/ANY) e non può essere usato come record a lunghezza fissa (DBF USE).",
                        ErrorCode.TypeMismatch);
                udtSymbolId = udt.SymbolId;
            }
            else
            {
                throw new BasicException(rewrittenStmtPartial.OriginalTypeName,
                    $"Il tipo '{rewrittenStmtPartial.OriginalTypeName.Lexeme}' specificato per DBF USE non è un tipo definito dall'utente (UDT) valido.",
                    ErrorCode.TypeMismatch);
            }
        }

        if (rewrittenStmtPartial.FieldMappings.Count > 0 && udtSymbolId <= 0)
        {
            throw new BasicException(rewrittenStmtPartial.Token,
                "La clausola MAP richiede AS Tipo per definire i campi del TYPE da mappare.",
                ErrorCode.TypeMismatch);
        }

        // Aggiorna il contesto di riscrittura con le nuove informazioni sulla work area.
        if (!string.IsNullOrEmpty(aliasName))
        {
            var newDbfState = new DbfContext(
                // Mappa l'alias al tipo risolto (o -1 se non c'era 'AS Tipo').
                AliasToUdtSymbolId: context.DbfState.AliasToUdtSymbolId.SetItem(aliasName, udtSymbolId),
                // Imposta questo alias come quello attualmente attivo.
                CurrentAlias: aliasName,
                CurrentIsUnsafe: rewrittenStmtPartial.IsUnsafe
            );
            newContext = context with { DbfState = newDbfState };
        }

        // Costruisce lo statement finale riscritto, ora arricchito con gli ID corretti.
        var rewrittenStmt = rewrittenStmtPartial with
        {
            AliasSymbolId = finalAliasSymbolId,
            OriginalAliasToken = finalAliasToken,
            TypeSymbolId = udtSymbolId
        };

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfSelectWorkAreaStmt, aggiornando il `CurrentAlias` nel contesto di riscrittura.
    /// L'analisi dell'espressione `WorkArea` (che può essere un nome, una stringa o un numero) determina
    /// quale alias di database diventa attivo per le istruzioni successive. Questo stato è cruciale
    /// per permettere al Rewriter di risolvere correttamente i nomi dei campi che appaiono in espressioni
    /// successive (es. `FOR ID = 5`), collegandoli all'UDT della work area attiva.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteDbfSelectStatement(DbfSelectWorkAreaStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        var rewrittenStmt = stmt with { WorkArea = RewriteExpression(stmt.WorkArea, context, program) };
        var newContext = context;

        string? newAlias = null;
        if (rewrittenStmt.WorkArea is VariableExpr { SymbolId: >= 0 } ve)
        {
            var symbol = program.Symbols.GetSymbol(ve.SymbolId);
            if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
            {
                newAlias = symbol.OriginalName;
            }
        }
        else if (rewrittenStmt.WorkArea is StringExpr se) // SELECT "MioAlias"
        {
            newAlias = se.Value;
        }

        if (newAlias != null)
        {
            newContext = context with { DbfState = context.DbfState with { CurrentAlias = newAlias } };
        }

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfReplaceStmt, creando un contesto specializzato per risolvere
    /// i campi di database all'interno delle espressioni di valore e della clausola di scope.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfReplaceStatement(DbfReplaceStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è un alias attivo E NON siamo in modalità Clipper, allora è un errore.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && !context.IsInClipperMode)
        {
            throw CreateError(stmt.Token, "Nessuna work area DBF attiva per il comando REPLACE.",
                ErrorCode.NoDatabaseInUse);
        }

        // Inizializza l'ID del tipo a "sconosciuto" (0 o -1).
        int udtSymbolId = 0;
        
        // Tenta di ottenere il tipo associato all'alias SOLO SE un alias è effettivamente attivo nel contesto.
        if (!string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out udtSymbolId);
        }
        // Fase 2: Crea un contesto di riscrittura specializzato per questa operazione.
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // Fase 3: Riscrive le assegnazioni (sia il Target che il Value).
        var rewrittenReplacements = stmt.Replacements.Select(r =>
        {
            var rewrittenTarget = RewriteTarget(r.Target, dbContext, program);
            var rewrittenValue = RewriteExpression(r.Value, dbContext, program);
            return new Assignment(rewrittenTarget, rewrittenValue);
        }).ToImmutableArray();

        // Fase 4: Riscrive la clausola di scope.
        var rewrittenScope = RewriteDbfScope(stmt.Scope, dbContext, program);

        var finalStmt = new DbfReplaceStmt(rewrittenReplacements, rewrittenScope, stmt.Token);

        // Questo comando non modifica il contesto (non cambia alias), quindi restituisce il contesto originale.
        return (finalStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfGetRecordStmt, gestendo la creazione implicita di variabili
    /// in modalità late-binding (Clipper) e aggiornando il contesto di riscrittura.
    /// </summary>
    private static (DbfGetRecordStmt, AstRewriteContext) RewriteDbfGetStatement(DbfGetRecordStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        // CASO 1: GET (senza argomenti)
        // Questo comando popola il buffer interno dell'Executor. Non c'è nulla da riscrivere
        // a livello di AST, e non modifica il contesto del Rewriter.
        if (stmt.Destination == null)
        {
            return (stmt, context);
        }

        // CASO 2: GET <variabile>
        // Questo comando ha la semantica speciale di creare la variabile se non esiste.

        var target = stmt.Destination;
        Target rewrittenTarget;

        // Se il target è una variabile semplice non ancora risolta (tipico del late-binding).
        if (target is VariableTarget { SymbolId: < 0 } vt)
        {
            var symbolName = vt.NameCleaned.Lexeme;
            var existingSymbolIds = program.Symbols.FindSymbolInScope(symbolName);

            if (existingSymbolIds == null || !existingSymbolIds.Any())
            {
                // La variabile non esiste. Dobbiamo crearla implicitamente.
                // Registra un nuovo simbolo per questa variabile nello scope corrente (quello della procedura).
                // Il tipo è "Inferred" perché verrà determinato a runtime dall'Executor
                // in base alla struttura del file DBF.
                var newSymbolId = program.Symbols.RegisterSymbol(
                    vt.NameCleaned,
                    SymbolFlags.IsLocal,
                    new InferredType(),
                    vt
                );

                Log($"[REWRITER-DBF] Variabile implicita '{symbolName}' creata da GET con ID: {newSymbolId}",
                    RewriteLog);

                // Aggiorna il nodo Target con il nuovo SymbolId.
                rewrittenTarget = vt with { SymbolId = newSymbolId };

                // Aggiorniamo anche la dichiarazione nella symbol table per coerenza
                program.Symbols.UpdateSymbolDeclaration(newSymbolId, rewrittenTarget);
            }
            else
            {
                // La variabile esiste già. Basta riscriverla per assicurarsi che l'ID sia corretto.
                rewrittenTarget = RewriteTarget(target, context, program);
            }
        }
        else
        {
            // Se il target è più complesso (es. un campo di un altro UDT),
            // lo riscriviamo normalmente.
            rewrittenTarget = RewriteTarget(target, context, program);
        }

        // Lo statement riscritto. Il contesto non viene modificato da un GET.
        var rewrittenStmt = stmt with { Destination = rewrittenTarget };

        // Restituisce lo statement aggiornato e il contesto originale,
        // poiché GET non cambia l'alias attivo o altre impostazioni di stato del DB.
        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfUpdateStmt, gestendo la risoluzione dei campi
    /// da due work area diverse (quella corrente e quella specificata in FROM).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfUpdateStatement(DbfUpdateStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Contesto per la work area corrente (target dell'UPDATE)
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            throw CreateError(stmt.Token, "Nessuna work area DBF attiva per il comando UPDATE.",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera l'UDT per la work area corrente, ma non lancia un errore se non c'è (per late-binding).
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out var currentUdtId);
        var currentDbContext = context with { CurrentUdtScopeId = currentUdtId > 0 ? currentUdtId : null };

        // Verifica l'esistenza della work area sorgente (clausola FROM)
        var sourceAlias = stmt.OriginalAliasToken.Lexeme;
        if (!context.DbfState.AliasToUdtSymbolId.ContainsKey(sourceAlias))
        {
            // Se siamo in Modalità Clipper, l'alias potrebbe non essere ancora noto.
            // Tuttavia, UPDATE ON richiede che la tabella sorgente sia aperta e indicizzata.
            // Quindi, questo controllo rimane valido.
            throw CreateError(stmt.OriginalAliasToken,
                $"L'alias '{sourceAlias}' specificato in FROM non è una work area DBF aperta o valida.",
                ErrorCode.DbfAliasNotFound);
        }

        // Riscrive l'espressione chiave (appartiene alla work area corrente).
        // Usa il contesto specializzato per risolvere i campi.
        var rewrittenKeyExpr = RewriteExpression(stmt.KeyExpression, currentDbContext, program);

        // Riscrive le espressioni di sostituzione.
        var rewrittenReplacements = stmt.Replacements.Select(r =>
        {
            // Il campo target (a sinistra di WITH) appartiene sempre alla work area corrente.
            // Lo riscriviamo come espressione per trasformare il VariableExpr in FieldAccessExpr.
            // CORREZIONE: Aggiornato costruttore VariableExpr
            var rewrittenTargetAsExpr =
                RewriteExpression(new VariableExpr(-1, r.NameSuffixed, r.NameCleaned), currentDbContext, program);

            if (rewrittenTargetAsExpr is not FieldAccessExpr rewrittenTarget)
            {
                throw CreateError(r.NameSuffixed,
                    $"'{r.NameSuffixed.Lexeme}' non è un campo valido nella work area corrente '{context.DbfState.CurrentAlias}'.");
            }

            // Per il valore (a destra di WITH), usiamo il contesto originale.
            // RewriteExpression e i suoi sotto-metodi (come RewriteFieldAccess) sono già in grado
            // di gestire nomi qualificati (es. Movimenti.Saldo) e nomi non qualificati (che
            // verranno risolti nel contesto della work area attiva, come definito in currentDbContext).
            // Non è necessario creare un nuovo contesto.
            // Il VALORE resta NON riscritto: il runtime di UPDATE ON deve distinguere i campi
            // della DESTINAZIONE (nomi nudi) da quelli del SORGENTE (`<alias>.<campo>`). La
            // riscrittura risolveva `<alias>.campo` sull'area CORRENTE e perdeva l'alias
            // (`ExprPlain.From` → "Nome"), rendendo impossibile la risoluzione a runtime.
            var rewrittenValue = r.Value;

            return r with { FieldSymbolId = rewrittenTarget.FieldSymbolId, Value = rewrittenValue };
        }).ToImmutableArray();

        var rewrittenStmt = new DbfUpdateStmt(rewrittenKeyExpr, stmt.AliasSymbolId, stmt.OriginalAliasToken,
            rewrittenReplacements,
            stmt.Token);

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfJoinWithStmt, gestendo la risoluzione dei campi
    /// dalle due work area coinvolte nella condizione FOR e nella clausola FIELDS.
    /// I campi non qualificati nella condizione FOR si riferiscono alla work area primaria (attiva).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfJoinStatement(DbfJoinWithStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // Contesto per la work area corrente (primaria)
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            throw CreateError(stmt.Token, "Nessuna work area DBF primaria attiva per il comando JOIN.",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera l'UDT per la work area primaria, ma non lancia un errore se non c'è (per late-binding).
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out var primaryUdtId);

        // Contesto per la work area secondaria (clausola WITH)
        var secondaryAlias = stmt.OriginalAliasToken.Lexeme;
        if (!context.DbfState.AliasToUdtSymbolId.ContainsKey(secondaryAlias))
        {
            throw CreateError(stmt.OriginalAliasToken,
                $"L'alias '{secondaryAlias}' specificato in WITH non è una work area DBF aperta o valida.",
                ErrorCode.DbfAliasNotFound);
        }

        // Crea un contesto specializzato che imposta la work area primaria come default per i campi non qualificati.
        var joinConditionContext = context with { CurrentUdtScopeId = primaryUdtId > 0 ? primaryUdtId : null };
        var rewrittenCondition = (DbfForClause)RewriteDbfScope(stmt.Condition, joinConditionContext, program)!;

        // Logica per riscrivere la clausola FIELDS (se presente)
        DbfFieldsClause? rewrittenFields = null;
        if (stmt.Fields != null)
        {
            // Per i campi in FIELDS, la risoluzione deve considerare entrambe le tabelle.
            // La logica di RewriteExpression è già in grado di gestire i FieldAccessExpr qualificati
            // (es. Clienti.Nome), che verranno risolti cercando l'alias nel DbfState.
            // Per i campi non qualificati, userà il contesto della tabella primaria.
            var rewrittenFieldExpressions = stmt.Fields.FieldExpressions
                .Select(expr =>
                    RewriteExpression(expr, joinConditionContext,
                        program))
                .ToImmutableArray();

            rewrittenFields = new DbfFieldsClause(FieldExpressions: rewrittenFieldExpressions);
        }

        var rewrittenStmt = new DbfJoinWithStmt(stmt.AliasSymbolId, stmt.OriginalAliasToken, rewrittenFileName,
            rewrittenCondition,
            rewrittenFields, stmt.Token);

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive CREATE VIEW: registra l'alias vista e risolve WHERE nel contesto UDT della tabella sorgente.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfCreateViewStatement(DbfCreateViewStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        var viewName = stmt.ViewNameToken.Lexeme;
        var symbolIds = program.Symbols.FindSymbolInScope(viewName);
        int viewSymbolId;
        if (symbolIds is not { Count: > 0 })
        {
            viewSymbolId = program.Symbols.RegisterSymbol(
                stmt.ViewNameToken,
                SymbolFlags.IsDynamicScope,
                new InferredType(),
                stmt.ViewNameToken);
        }
        else
        {
            viewSymbolId = symbolIds[0];
        }

        DbfSelectQuery? rewrittenQuery = null;
        Expr? rewrittenQueryExpr = null;
        var newContext = context;

        if (stmt.SelectQuery != null)
        {
            var fromAlias = stmt.SelectQuery.From.TableAlias.Lexeme;
            if (!context.DbfState.AliasToUdtSymbolId.TryGetValue(fromAlias, out var sourceUdtId))
            {
                throw CreateError(stmt.SelectQuery.From.TableAlias,
                    $"Work area sorgente '{fromAlias}' non aperta o senza TYPE.", ErrorCode.DbfAliasNotFound);
            }

            var queryContext = context with { CurrentUdtScopeId = sourceUdtId > 0 ? sourceUdtId : null };
            rewrittenQuery = RewriteDbfSelectQuery(stmt.SelectQuery, queryContext, program);

            newContext = context with
            {
                DbfState = context.DbfState with
                {
                    AliasToUdtSymbolId = context.DbfState.AliasToUdtSymbolId.SetItem(viewName, sourceUdtId)
                }
            };
        }
        else if (stmt.QueryExpression != null)
        {
            rewrittenQueryExpr = RewriteExpression(stmt.QueryExpression, context, program);
        }

        var rewrittenStmt = new DbfCreateViewStmt(viewSymbolId, stmt.ViewNameToken, rewrittenQuery,
            rewrittenQueryExpr, stmt.Token);
        return (rewrittenStmt, newContext);
    }

    private static DbfSelectQuery RewriteDbfSelectQuery(DbfSelectQuery query, AstRewriteContext context,
        BasicProgram program)
    {
        return query with
        {
            SelectFields = query.SelectFields
                ?.Select(f => f with { Expression = RewriteExpression(f.Expression, context, program) })
                .ToImmutableArray(),
            Join = query.Join != null
                ? query.Join with { Condition = RewriteExpression(query.Join.Condition, context, program) }
                : null,
            Where = query.Where != null ? RewriteExpression(query.Where, context, program) : null
        };
    }

    /// <summary>
    /// Riscrive un'istruzione DbfDeleteStmt, creando un contesto specializzato per risolvere
    /// i campi di database all'interno della clausola di scope (es. FOR).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfDeleteStatement(DbfDeleteStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è una clausola di scope, non c'è nulla da risolvere.
        if (stmt.Scope == null)
        {
            return (stmt, context);
        }
        
        // Se c'è uno scope ma non c'è un alias attivo E NON siamo in modalità Clipper, è un errore.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && !context.IsInClipperMode)
        {
            throw CreateError(stmt.Token, "Nessuna work area DBF attiva per la clausola FOR del comando DELETE.",
                ErrorCode.NoDatabaseInUse);
        }

        // Inizializza l'ID del tipo a "sconosciuto".
        int udtSymbolId = 0;
        
        // Tenta di ottenere il tipo associato all'alias SOLO SE un alias è effettivamente attivo nel contesto.
        if (!string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out udtSymbolId);
        }
        
        // Crea un contesto di riscrittura specializzato per la clausola di scope.
        // Se udtSymbolId non è valido, CurrentUdtScopeId sarà null, attivando il late-binding.
        var scopeContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenScope = RewriteDbfScope(stmt.Scope, scopeContext, program);

        var finalStmt = stmt with { Scope = rewrittenScope };
        
        // DELETE non cambia il contesto, quindi restituisce quello originale.
        return (finalStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfRecallStmt, creando un contesto specializzato per risolvere
    /// i campi di database all'interno della clausola di scope (es. FOR).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfRecallStatement(DbfRecallStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è una clausola di scope, non c'è nulla da risolvere.
        if (stmt.Scope == null)
        {
            return (stmt, context);
        }

        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) ||
            !context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out var udtSymbolId))
        {
            var rewrittenStmt = stmt with { Scope = RewriteDbfScope(stmt.Scope, context, program) };
            return (rewrittenStmt, context);
        }

        // Crea un contesto di riscrittura specializzato per la clausola di scope.
        var scopeContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenScope = RewriteDbfScope(stmt.Scope, scopeContext, program);

        return (stmt with { Scope = rewrittenScope }, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfAppendFromStmt, applicando la logica di late-binding alla clausola FOR.
    /// La condizione FOR viene valutata nel contesto della work area di destinazione (quella attiva).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfAppendFromStatement(DbfAppendFromStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        // L'espressione del nome del file sorgente viene riscritta con il contesto standard.
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // La clausola di scope viene riscritta utilizzando l'helper specializzato,
        // che creerà il sotto-contesto corretto (statico o late-binding) per la condizione,
        // basandosi sulla work area attualmente attiva.
        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, context, program);

        var rewrittenStmt = stmt with
        {
            FileName = rewrittenFileName,
            Scope = rewrittenScope
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfCopyToStmt, applicando la logica di late-binding alla clausola FOR.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfCopyToStatement(DbfCopyToStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // L'espressione del nome del file viene riscritta con il contesto standard,
        // poiché non può contenere campi di database.
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // La clausola di scope viene riscritta utilizzando l'helper specializzato,
        // che creerà il sotto-contesto corretto (statico o late-binding) per la condizione.
        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, context, program);

        var rewrittenStmt = stmt with
        {
            FileName = rewrittenFileName,
            Scope = rewrittenScope
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfSortStmt, risolvendo i campi di ordinamento e la clausola
    /// di scope nel contesto della work area DBF attiva.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfSortStatement(DbfSortStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // Se non c'è una work area attiva, non possiamo risolvere i campi statici.
        // Tuttavia, se siamo in modalità Clipper e usiamo macro/espressioni, potremmo voler procedere
        // affidandoci al late binding. Ma per coerenza e sicurezza, richiediamo un contesto DB.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && !context.IsInClipperMode)
        {
            throw CreateError(stmt.Token, "Nessuna work area DBF attiva per il comando SORT.",
                ErrorCode.NoDatabaseInUse);
        }

        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // Contesto DB per risolvere i campi (se presenti in espressioni).
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // Riscrivi i campi di ordinamento
        var rewrittenSortFields = stmt.SortFields.Select(f =>
        {
            // CASO A: Campo definito da un'Espressione (es. Macro o UDF)
            if (f.FieldExpression != null)
            {
                // Riscrivi l'espressione (es. risolvi le variabili nella macro)
                // Usiamo il contesto standard per le variabili della macro, NON il contesto DB,
                // perché la macro stessa (es. cSortKey) è una variabile locale.
                // Tuttavia, se l'espressione contiene campi, il dbContext aiuterebbe.
                // Per le macro semplici (&Var), Var è locale.
                // Per sicurezza, usiamo context (standard) per la macro.
                var rewrittenExpr = RewriteExpression(f.FieldExpression, context, program);

                return f with { FieldExpression = rewrittenExpr };
            }

            // CASO B: Campo definito da un Token statico (Identificatore)

            // Se l'ID è già risolto, non facciamo nulla.
            if (f.FieldSymbolId >= 0) return f;

            var fieldName = f.OriginalToken.Lexeme;

            // Se abbiamo un tipo statico (AS Tipo), cerchiamo il campo al suo interno.
            if (udtSymbolId > 0 && program.Symbols.GetSymbol(udtSymbolId).DeclarationNode is UserDefinedType udt)
            {
                var field = udt.Fields.FirstOrDefault(udtField =>
                    udtField.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));

                if (field != null)
                {
                    return f with { FieldSymbolId = field.SymbolId };
                }

                // Se non è un campo definito staticamente, è sempre un errore, indipendentemente dalla modalità.
                throw CreateError(f.OriginalToken,
                    $"Il campo '{fieldName}' non è stato trovato nel tipo '{udt.Name.Lexeme}' associato " +
                    $"staticamente alla work area '{context.DbfState.CurrentAlias}'.");
            }

            // Se non abbiamo un tipo statico (late-binding), il comportamento dipende dalla modalità Clipper.
            // Se NON siamo in modalità Clipper, un campo non risolvibile è un errore.
            if (!context.IsInClipperMode)
            {
                throw CreateError(f.OriginalToken,
                    $"Il campo '{fieldName}' non è definito. Per utilizzare campi di database non dichiarati, " +
                    $"assicurarsi che il modulo contenga 'IMPORT DBF' e che non ci sia una clausola 'AS Tipo' nel comando 'USE'.");
            }

            // Se siamo in modalità Clipper e senza 'AS Tipo', lasciamo l'ID a -1.
            // L'Executor risolverà il nome del campo a runtime (late binding).
            return f;
        }).ToImmutableArray();

        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, dbContext, program);

        var rewrittenStmt = new DbfSortStmt(rewrittenFileName, rewrittenSortFields, rewrittenScope, stmt.Token);
        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione `INDEX ON ... TO ...` per il namespace DBF.
    /// Determina la work area attiva per creare un contesto specializzato che permette
    /// di risolvere i nomi dei campi all'interno della KeyExpression.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfIndexOnStatement(DbfIndexOnStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Risolve l'espressione del nome del file di indice (contesto standard).
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // Se non c'è un alias attivo E NON siamo in modalità Clipper, allora è un errore.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && !context.IsInClipperMode)
        {
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per INDEX ON. Nessuna work area DBF è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Inizializza l'ID del tipo a "sconosciuto".
        int udtSymbolId = 0;
        
        // Tenta di ottenere il tipo associato all'alias SOLO SE un alias è effettivamente attivo nel contesto.
        if (!string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out udtSymbolId);
        }

        // Crea un contesto di riscrittura specializzato per l'espressione chiave.
        var keyContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // Riscrive l'espressione chiave usando il nuovo contesto.
        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, keyContext, program);
        ValidateRewrittenIndexKey(rewrittenKeyExpression, fieldIsUnsafe: false, keyContext, program, stmt.Token);

        var rewrittenStmt = stmt with
        {
            FileName = rewrittenFileName,
            KeyExpression = rewrittenKeyExpression
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfSetRelationStmt, applicando la logica di late-binding
    /// all'espressione chiave (KeyExpression).
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfSetRelationStatement(DbfSetRelationStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è un'espressione chiave (es. SET RELATION TO per rompere la relazione), non c'è nulla da fare.
        if (stmt.KeyExpression == null)
        {
            return (stmt, context);
        }

        // Se non c'è una work area attiva (la tabella padre), non possiamo risolvere i campi.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && context.IsInClipperMode)
        {
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per SET RELATION. Nessuna work area DBF padre è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera l'ID del tipo UDT associato, ma non lancia un errore se non lo trova.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // Crea il contesto di database per l'espressione chiave.
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, dbContext, program);

        var rewrittenStmt = stmt with
        {
            KeyExpression = rewrittenKeyExpression
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfSetFilterStmt, applicando la logica di late-binding
    /// all'espressione di condizione.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfSetFilterStatement(DbfSetFilterStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è una condizione di filtro (es. SET FILTER TO per pulire), non c'è nulla da fare.
        if (stmt.Condition == null)
        {
            return (stmt, context);
        }

        // Se non c'è una work area attiva, non possiamo risolvere i campi.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && context.IsInClipperMode)
        {
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per SET FILTER. Nessuna work area DBF è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera l'ID del tipo UDT associato, ma non lancia un errore se non lo trova.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // Crea il contesto di database. Se udtSymbolId non è valido, ma IsInClipperMode è true,
        // questo contesto abiliterà il late-binding.
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        var rewrittenCondition = RewriteExpression(stmt.Condition, dbContext, program);

        var rewrittenStmt = stmt with
        {
            Condition = rewrittenCondition
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfListStmt, applicando la logica di late-binding
    /// alle clausole FIELDS e FOR/WHILE.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfListStatement(DbfListStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Se non c'è una work area attiva, non possiamo risolvere i campi, a meno che non siamo in modalità Clipper
        // e il comando non usi campi (es. LIST TO PRINT). In generale, è più sicuro richiedere una work area attiva.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && context.IsInClipperMode &&
            (stmt.Fields != null || stmt.Scope != null))
        {
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per LIST. Nessuna work area DBF è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera l'ID del tipo UDT associato, ma non lancia un errore se non lo trova.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // Crea il contesto di database per le clausole.
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // Riscrive le clausole usando il contesto specializzato.
        var rewrittenScope = RewriteDbfScope(stmt.Scope, dbContext, program);

        DbfFieldsClause? rewrittenFields = null;
        if (stmt.Fields != null)
        {
            var rewrittenFieldExpressions = stmt.Fields.FieldExpressions
                .Select(expr => RewriteExpression(expr, dbContext, program))
                .ToImmutableArray();
            rewrittenFields = new DbfFieldsClause(rewrittenFieldExpressions);
        }

        // La destinazione (TO FILE) non contiene campi, usa il contesto standard.
        var rewrittenDestination = stmt.Destination != null
            ? RewriteExpression(stmt.Destination, context, program)
            : null;

        var rewrittenStmt = stmt with
        {
            Scope = rewrittenScope,
            Fields = rewrittenFields,
            Destination = rewrittenDestination
        };

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfTotalOnStmt, creando un contesto specializzato per risolvere
    /// i campi di database all'interno di KeyExpression, Fields e della clausola di Scope.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfTotalOnStatement(DbfTotalOnStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // Se non c'è una work area attiva, non possiamo procedere se siamo in modalità Clipper
        // perché non avremmo alcun riferimento per il late binding.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias) && context.IsInClipperMode)
        {
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per TOTAL ON. Nessuna work area DBF è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Cerca l'ID del tipo UDT associato, ma non lancia un errore se non lo trova.
        // Se non viene trovato, udtSymbolId sarà 0 (default), che è gestito correttamente sotto.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias ?? "", out var udtSymbolId);

        // Crea il contesto di database. Se udtSymbolId è 0 o non valido, CurrentUdtScopeId sarà null.
        // Se IsInClipperMode è true, questo contesto abiliterà il late binding.
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, dbContext, program);

        DbfFieldsClause? rewrittenFields = null;
        if (stmt.Fields != null)
        {
            var rewrittenFieldExpressions = stmt.Fields.FieldExpressions
                .Select(expr => RewriteExpression(expr, dbContext, program))
                .ToImmutableArray();

            rewrittenFields = new DbfFieldsClause(FieldExpressions: rewrittenFieldExpressions);
        }

        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, dbContext, program);

        var rewrittenStmt = new DbfTotalOnStmt(rewrittenKeyExpression, rewrittenFileName, rewrittenFields,
            rewrittenScope,
            stmt.Token);

        return (rewrittenStmt, context);
    }

    /// <summary>
    /// Riscrive un'istruzione DbfAggregateStmt.
    /// Risolve le espressioni interne (Key, Scope e le espressioni dentro le aggregazioni) 
    /// nel contesto DBF corretto.
    /// </summary>
    private static (Stmt, AstRewriteContext) RewriteDbfAggregateStatement(DbfAggregateStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);

        // Verifica che ci sia una work area attiva, altrimenti la risoluzione dei campi fallirebbe.
        if (string.IsNullOrEmpty(context.DbfState.CurrentAlias))
        {
            // In modalità Clipper, si potrebbe procedere se le espressioni non usano campi, ma è più sicuro
            // richiedere sempre un'area attiva per questo comando, data la sua natura data-centrica.
            throw CreateError(stmt.Token,
                "Impossibile risolvere i campi per AGGREGATE. Nessuna work area DBF è attiva (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        // Recupera il tipo UDT associato all'alias corrente per creare il contesto di risoluzione dei campi.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(context.DbfState.CurrentAlias, out var udtSymbolId);
        var dbContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        // Riscrive la chiave di raggruppamento e la clausola di scope nel contesto del database.
        var rewrittenKey = RewriteExpression(stmt.KeyExpression, dbContext, program);
        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, dbContext, program);

        // Riscrive l'espressione che contiene le definizioni di aggregazione.
        // Se si tratta di un ArrayLiteralExpr (il caso comune per entrambe le sintassi),
        // il Rewriter scenderà ricorsivamente e riscriverà ogni StringExpr al suo interno,
        // anche se in questo caso non c'è molto da fare su un letterale stringa.
        // Il lavoro importante (il parsing della stringa) avverrà nell'Executor.
        var rewrittenAggregationsExpr = RewriteExpression(stmt.AggregationsExpr, context, program);

        // Validazione semantica: assicuriamoci che l'espressione delle aggregazioni si risolva in un ARRAY OF STRING
        var aggExprType = LinkerInferExpressionType(rewrittenAggregationsExpr, context, program);
        if (aggExprType is not (ArrayType { ElementType: StringType } or ListType { ElementType: StringType }) &&
            aggExprType is not InferredType)
        {
            throw CreateError(rewrittenAggregationsExpr,
                "Le definizioni per DBF AGGREGATE devono essere fornite come un array di stringhe.",
                ErrorCode.TypeMismatch);
        }

        var rewrittenStmt = new DbfAggregateStmt(rewrittenKey, rewrittenFileName, rewrittenAggregationsExpr,
            rewrittenScope, stmt.Token);

        // AGGREGATE non cambia lo stato del contesto (es. l'alias attivo), quindi restituiamo il contesto originale.
        return (rewrittenStmt, context);
    }

    // #################################################################################################################
    // ISAM
    // #################################################################################################################

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamOpenStmt`.
    /// Questo metodo risolve il tipo UDT e aggiorna il contesto DBF/ISAM con il nuovo alias.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteIsamOpenStatement(IsamOpenStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Risolve il tipo UDT specificato nella clausola OF.
        var resolvedDataType =
            ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program,
                context.ModulePath);
        if (resolvedDataType is not UserDefinedType udt)
        {
            throw CreateError(stmt.OriginalTypeName,
                $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per ISAM OPEN non è un UDT valido.",
                ErrorCode.TypeMismatch);
        }
        if (!udt.IsFixedSize)
        {
            throw CreateError(stmt.OriginalTypeName,
                $"Il tipo '{udt.Name.Lexeme}' ha campi a lunghezza variabile (STRING/LIST/DICT/ANY) e non può essere usato come record a lunghezza fissa (ISAM OPEN).",
                ErrorCode.TypeMismatch);
        }

        var udtSymbolId = udt.SymbolId;

        // Riscrivi le altre espressioni nello statement.
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);
        var rewrittenEncoding =
            stmt.EncodingExpr != null ? RewriteExpression(stmt.EncodingExpr, context, program) : null;

        // Per INDEXED BY, crea un contesto DB per risolvere i nomi dei campi.
        var dbFieldContext = context with { CurrentUdtScopeId = udtSymbolId };
        var openDbContext = dbFieldContext with
        {
            DbfState = dbFieldContext.DbfState with { CurrentIsUnsafe = stmt.IsUnsafe }
        };
        var rewrittenIndexedBy = RewriteIsamIndexedBy(stmt.IndexedBy, openDbContext, program);

        var rewrittenStmt = stmt with
        {
            FileName = rewrittenFileName,
            IndexedBy = rewrittenIndexedBy,
            EncodingExpr = rewrittenEncoding
        };

        // Aggiorna il contesto del Rewriter (DbfState) con il nuovo alias.
        var newDbfState = new DbfContext(
            AliasToUdtSymbolId: context.DbfState.AliasToUdtSymbolId.SetItem(
                rewrittenStmt.OriginalAliasToken.Lexeme,
                udtSymbolId),
            CurrentAlias: rewrittenStmt.OriginalAliasToken.Lexeme,
            CurrentIsUnsafe: stmt.IsUnsafe
        );
        var newContext = context with { DbfState = newDbfState };

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Helper specifico per riscrivere le espressioni all'interno di una clausola `INDEXED BY`.
    /// Itera su ogni `IsamIndexField` e invoca `RewriteExpression` sulla `KeyExpression`,
    /// assicurando che qualsiasi variabile o funzione utilizzata per definire una chiave di indice
    /// sia completamente risolta e semanticamente validata.
    /// </summary>
    private static IsamIndexedByClause? RewriteIsamIndexedBy(IsamIndexedByClause? clause, AstRewriteContext context,
        BasicProgram program)
    {
        if (clause == null) return null;
        return new IsamIndexedByClause(KeyFields:
        [
            ..clause.KeyFields.Select(f =>
            {
                var rewritten = RewriteExpression(f.KeyExpression, context, program);
                ValidateRewrittenIndexKey(rewritten, f.IsUnsafe, context, program, IndexKeyErrorToken(rewritten));
                return f with { KeyExpression = rewritten };
            })
        ]);
    }

    /// <summary>
    /// Riscrive un'istruzione IsamUseStmt, eseguendo la stessa logica di `RewriteDbfUseStatement`.
    /// Aggiorna il contesto di riscrittura (`AstRewriteContext`) per registrare l'associazione tra l'alias
    /// e il `SymbolId` del tipo UDT associato, e imposta questo alias come `CurrentAlias`.
    /// Questo permette al sistema di tracciare quale tipo di record è attivo per la risoluzione dei campi.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteIsamUseStatement(IsamUseStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Risolve il tipo UDT specificato nella clausola OF.
        var resolvedDataType =
            ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program,
                context.ModulePath);
        if (resolvedDataType is not UserDefinedType udt)
        {
            throw CreateError(stmt.OriginalTypeName,
                $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per ISAM USE non è un UDT valido.",
                ErrorCode.TypeMismatch);
        }

        var udtSymbolId = udt.SymbolId;

        // Riscrivi le altre espressioni nello statement.
        var rewrittenFileName = RewriteExpression(stmt.FileName, context, program);
        var useDbContext = context with
        {
            CurrentUdtScopeId = udtSymbolId,
            DbfState = context.DbfState with { CurrentIsUnsafe = stmt.IsUnsafe }
        };
        var rewrittenIndexedBy = RewriteIsamIndexedBy(stmt.IndexedBy, useDbContext, program);

        // Crea lo statement riscritto, aggiornando il TypeSymbolId.
        var rewrittenStmt = stmt with
        {
            FileName = rewrittenFileName,
            TypeSymbolId = udtSymbolId, // Aggiorna con l'ID risolto
            IndexedBy = rewrittenIndexedBy,
            AliasSymbolId = stmt.AliasSymbolId // L'ID dell'alias è già stato risolto dal Binder
        };

        // Aggiorna il contesto del Rewriter (DbfState) per i comandi successivi.
        var newDbfState = new DbfContext(
            AliasToUdtSymbolId: context.DbfState.AliasToUdtSymbolId.SetItem(
                rewrittenStmt.OriginalAliasToken.Lexeme,
                udtSymbolId),
            CurrentAlias: rewrittenStmt.OriginalAliasToken.Lexeme,
            CurrentIsUnsafe: stmt.IsUnsafe
        );
        var newContext = context with { DbfState = newDbfState };

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione IsamSelectWorkAreaStmt, aggiornando il `CurrentAlias` nel contesto di riscrittura.
    /// La sua logica è identica a quella di `RewriteDbfSelectStatement`, garantendo un comportamento
    /// coerente tra i diversi motori di database per la gestione delle work area attive.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteIsamSelectStatement(
        IsamSelectWorkAreaStmt stmt, AstRewriteContext context, BasicProgram program)
    {
        // La logica è identica a quella di DBF.
        var rewrittenStmt = stmt with { WorkArea = RewriteExpression(stmt.WorkArea, context, program) };
        var newContext = context;

        string? newAlias = null;
        if (rewrittenStmt.WorkArea is VariableExpr { SymbolId: >= 0 } ve)
        {
            var symbol = program.Symbols.GetSymbol(ve.SymbolId);
            if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
            {
                newAlias = symbol.OriginalName;
            }
        }
        else if (rewrittenStmt.WorkArea is StringExpr se)
        {
            newAlias = se.Value;
        }

        if (newAlias != null)
        {
            newContext = context with { DbfState = context.DbfState with { CurrentAlias = newAlias } };
        }

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamCopyToStmt`.
    /// Determina il tipo UDT associato all'handle del database per risolvere
    /// correttamente i campi nell'espressione della clausola FOR.
    /// </summary>
    private static IsamCopyToStmt RewriteIsamCopyToStatement(IsamCopyToStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);
        var rewrittenDestinationFile = RewriteExpression(stmt.DestinationFile, context, program);

        if (stmt.Scope == null)
        {
            return stmt with
            {
                FileNumber = rewrittenFileNumber,
                DestinationFile = rewrittenDestinationFile
            };
        }

        // Determina l'alias target.
        string? targetAlias = null;

        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        targetAlias ??= context.DbfState.CurrentAlias;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var udtSymbolId);

        var scopeContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, scopeContext, program);

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            DestinationFile = rewrittenDestinationFile,
            Scope = rewrittenScope
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione <see cref="IsamJoinWithStmt"/>.
    /// </summary>
    private static IsamJoinWithStmt RewriteIsamJoinWithStatement(IsamJoinWithStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);
        var rewrittenDestinationFile = RewriteExpression(stmt.DestinationFile, context, program);

        string? targetAlias = null;
        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        targetAlias ??= context.DbfState.CurrentAlias;
        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var primaryUdtSymbolId);

        var primaryContext = context with { CurrentUdtScopeId = primaryUdtSymbolId > 0 ? primaryUdtSymbolId : null };
        var rewrittenCondition = (DbfForClause)RewriteDbfScope(stmt.Condition, primaryContext, program)!;

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            DestinationFile = rewrittenDestinationFile,
            Condition = rewrittenCondition
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamCreateIndexStmt`.
    /// Questo metodo determina il tipo UDT associato all'handle del database usando la mappa
    /// pre-calcolata in BasicProgram e crea un contesto di riscrittura specifico per la `KeyExpression`.
    /// VERSIONE CORRETTA: Utilizza l'helper centralizzato FindUdtSymbolIdForDbHandle.
    /// </summary>
    private static IsamCreateIndexStmt RewriteIsamCreateIndexStatement(IsamCreateIndexStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);
        var rewrittenIndexName = RewriteExpression(stmt.IndexName, context, program);
        var rewrittenKeyLength = RewriteExpression(stmt.KeyLength, context, program);

        // Determina l'alias target.
        string? targetAlias = null;

        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        targetAlias ??= context.DbfState.CurrentAlias;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var udtSymbolId);

        var keyContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, keyContext, program);
        ValidateRewrittenIndexKey(rewrittenKeyExpression, fieldIsUnsafe: false, keyContext, program, stmt.Token);

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            IndexName = rewrittenIndexName,
            KeyLength = rewrittenKeyLength,
            KeyExpression = rewrittenKeyExpression
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamSetFilterStmt`.
    /// Determina il tipo UDT associato all'handle e crea un contesto specifico
    /// per la riscrittura dell'espressione di condizione, permettendo di risolvere i nomi dei campi.
    /// </summary>
    private static IsamSetFilterStmt RewriteIsamSetFilterStatement(IsamSetFilterStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);

        if (stmt.Condition == null)
        {
            return stmt with { FileNumber = rewrittenFileNumber };
        }

        // Determina l'alias target.
        string? targetAlias = null;

        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        targetAlias ??= context.DbfState.CurrentAlias;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var udtSymbolId);

        var filterContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenCondition = RewriteExpression(stmt.Condition, filterContext, program);

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            Condition = rewrittenCondition
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamSetRelationStmt`.
    /// Determina il tipo UDT della work area padre per risolvere correttamente
    /// l'espressione chiave (KeyExpression).
    /// </summary>
    private static IsamSetRelationStmt RewriteIsamSetRelationStatement(IsamSetRelationStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Risolve l'handle/alias della work area padre.
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);

        // Se non c'è espressione chiave (es. SET RELATION TO per rompere la relazione), non c'è nulla da fare.
        if (stmt.KeyExpression == null)
        {
            return stmt with { FileNumber = rewrittenFileNumber };
        }

        // Determina l'alias target.
        string? targetAlias = null;

        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        // Se non abbiamo trovato un alias esplicito, usiamo quello corrente.
        targetAlias ??= context.DbfState.CurrentAlias;

        // Determina il tipo UDT.
        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var udtSymbolId);

        // Risolve la key expression: per ISAM SET RELATION, il campo chiave (es. 'ID')
        // viene risolto manualmente dal UDT per evitare il late-binding Clipper che creerebbe
        // FieldAccessExpr(_dbRecord, campo) con FieldSymbolId = -1 (non risolvibile).
        var rewrittenKeyExpression = stmt.KeyExpression;
        if (udtSymbolId > 0 && rewrittenKeyExpression is VariableExpr veKey && veKey.SymbolId < 0)
        {
            var udtSymbol = program.Symbols.GetSymbol(udtSymbolId);
            if (udtSymbol.DeclarationNode is UserDefinedType udt)
            {
                var field = udt.Fields.FirstOrDefault(f =>
                    f.Name.Lexeme.Equals(veKey.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (field != null)
                {
                    // Crea VariableExpr con il SymbolId del campo UDT (risolto a runtime via keyScope)
                    rewrittenKeyExpression = veKey with { SymbolId = field.SymbolId };
                }
            }
        }
        else if (rewrittenKeyExpression != null)
        {
            // Per espressioni chiave complesse, usa il rewriter standard
            rewrittenKeyExpression = RewriteExpression(rewrittenKeyExpression, context, program);
        }

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            KeyExpression = rewrittenKeyExpression
            // ChildAlias è un Token, non un'espressione, quindi non richiede riscrittura.
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IsamSortToStmt`.
    /// Determina il tipo UDT associato all'handle per risolvere correttamente
    /// le espressioni nei campi di ordinamento (SortFields) e nella clausola FOR.
    /// </summary>
    private static IsamSortToStmt RewriteIsamSortToStatement(IsamSortToStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenFileNumber = RewriteExpression(stmt.FileNumber, context, program);
        var rewrittenDestinationFile = RewriteExpression(stmt.DestinationFile, context, program);

        // Determina l'alias target.
        string? targetAlias = null;

        if (rewrittenFileNumber is StringExpr se)
        {
            targetAlias = se.Value;
        }
        else if (rewrittenFileNumber is VariableExpr ve)
        {
            // Se è una variabile, potrebbe essere un alias registrato.
            // Se ha un ID valido e il simbolo è IsDatabaseAlias, usiamo il nome originale.
            if (ve.SymbolId >= 0)
            {
                var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsDynamicScope))
                {
                    targetAlias = symbol.OriginalName;
                }
            }
            // Fallback: se l'ID è -1 (alias non risolto come variabile ma passato dal resolver), usiamo il lexema.
            else
            {
                targetAlias = ve.NameCleaned.Lexeme;
            }
        }

        // Se non abbiamo trovato un alias esplicito, usiamo quello corrente.
        targetAlias ??= context.DbfState.CurrentAlias;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(targetAlias ?? "", out var udtSymbolId);

        var fieldContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };

        var rewrittenSortFields = stmt.SortFields
            .Select(f =>
            {
                var rewritten = RewriteExpression(f.KeyExpression, fieldContext, program);
                ValidateRewrittenIndexKey(rewritten, f.IsUnsafe, fieldContext, program, IndexKeyErrorToken(rewritten));
                return f with { KeyExpression = rewritten };
            })
            .ToImmutableArray();

        var rewrittenScope = (DbfForClause?)RewriteDbfScope(stmt.Scope, fieldContext, program);

        return stmt with
        {
            FileNumber = rewrittenFileNumber,
            DestinationFile = rewrittenDestinationFile,
            SortFields = rewrittenSortFields,
            Scope = rewrittenScope
        };
    }

    // #################################################################################################################
    // SPEED
    // #################################################################################################################

    /// <summary>
    /// Riscrive un'istruzione SpeedOpenStmt, aggiornando il contesto di riscrittura
    /// per registrare il nuovo alias del database. Non risolve un tipo UDT poiché
    /// SPEED OPEN non è legato a un tipo specifico.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteSpeedOpenStatement(SpeedOpenStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Riscrive l'espressione del percorso del database.
        var rewrittenDatabasePath = RewriteExpression(stmt.DatabasePath, context, program);

        var rewrittenStmt = stmt with
        {
            DatabasePath = rewrittenDatabasePath
        };

        // Aggiorna il contesto del Rewriter (DbfState).
        // Per SPEED OPEN, non c'è un UDT da associare, quindi usiamo un ID placeholder (-1).
        // Impostiamo l'alias come alias "corrente" per un eventuale USE implicito successivo,
        // anche se la sintassi non lo supporta formalmente.
        var newDbfState = new DbfContext(
            AliasToUdtSymbolId: context.DbfState.AliasToUdtSymbolId.SetItem(
                rewrittenStmt.OriginalAliasToken.Lexeme,
                -1), // Nessun UDT associato a un database, solo a una tabella
            CurrentAlias: rewrittenStmt.OriginalAliasToken.Lexeme
        );
        var newContext = context with { DbfState = newDbfState };

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione SpeedUseStmt.
    /// 1. Risolve il tipo UDT.
    /// 2. Crea un contesto specializzato con l'ID del tipo.
    /// 3. Usa questo contesto per riscrivere la clausola `INDEXED BY`, permettendo la risoluzione dei campi.
    /// 4. Aggiorna il contesto di riscrittura globale con il nuovo alias.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteSpeedUseStatement(SpeedUseStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Fase 1: Risolvi il tipo UDT specificato nella clausola OF.
        var resolvedDataType =
            ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program,
                context.ModulePath);
        if (resolvedDataType is not UserDefinedType udt)
        {
            throw CreateError(stmt.OriginalTypeName,
                $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per SPEED USE non è un UDT valido.",
                ErrorCode.TypeMismatch);
        }

        var udtSymbolId = udt.SymbolId;

        // Fase 2: Riscrivi le espressioni standard (quelle che non dipendono dal contesto del DB).
        var rewrittenTableName = RewriteExpression(stmt.TableName, context, program);
        var rewrittenDatabaseReference = RewriteExpression(stmt.DatabaseReference, context, program);

        // Fase 3: Crea un contesto specifico per il DB per risolvere i campi in INDEXED BY.
        var dbFieldContext = context with
        {
            CurrentUdtScopeId = udtSymbolId,
            DbfState = context.DbfState with { CurrentIsUnsafe = stmt.IsUnsafe }
        };
        var rewrittenIndexedBy = RewriteIsamIndexedBy(stmt.IndexedBy, dbFieldContext, program);

        // Fase 4: Costruisci lo statement riscritto.
        var rewrittenStmt = stmt with
        {
            TableName = rewrittenTableName,
            DatabaseReference = rewrittenDatabaseReference,
            TypeSymbolId = udtSymbolId,
            IndexedBy = rewrittenIndexedBy
        };

        // Fase 5: Aggiorna il contesto di riscrittura per le istruzioni successive.
        var newDbfState = new DbfContext(
            AliasToUdtSymbolId: context.DbfState.AliasToUdtSymbolId.SetItem(
                rewrittenStmt.OriginalAliasToken.Lexeme,
                udtSymbolId),
            CurrentAlias: rewrittenStmt.OriginalAliasToken.Lexeme,
            CurrentIsUnsafe: stmt.IsUnsafe
        );
        var newContext = context with { DbfState = newDbfState };

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione SpeedCreateViewStmt. Il Parser lascia OfTypeSymbolId a -1
    /// (placeholder): senza questo passo, ExecuteSpeedCreateViewStep (Gpm.Speed.cs) legge
    /// stmt.OfTypeSymbolId, chiama GetSymbol(-1) (che restituisce un simbolo fittizio, non
    /// UserDefinedType) e lancia "Il simbolo 'X' (ID: -1) non è un TYPE definito." — a
    /// differenza di SpeedUseStmt, il cui TypeSymbolId viene risolto correttamente da
    /// RewriteSpeedUseStatement, non esisteva l'equivalente per la clausola OF di CREATE VIEW.
    /// </summary>
    private static SpeedCreateViewStmt RewriteSpeedCreateViewStatement(SpeedCreateViewStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        var resolvedDataType =
            ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([stmt.OriginalTypeName])), program,
                context.ModulePath);
        if (resolvedDataType is not UserDefinedType udt)
        {
            throw CreateError(stmt.OriginalTypeName,
                $"Il tipo '{stmt.OriginalTypeName.Lexeme}' specificato per SPEED CREATE VIEW non è un UDT valido.",
                ErrorCode.TypeMismatch);
        }

        return stmt with
        {
            OfTypeSymbolId = udt.SymbolId,
            SelectStatement = stmt.SelectStatement != null
                ? RewriteSpeedSelectQuery(stmt.SelectStatement, context, program)
                : null,
            QueryExpression = stmt.QueryExpression != null
                ? RewriteExpression(stmt.QueryExpression, context, program)
                : null
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere tutte le sotto-espressioni di una query `SPEED SELECT`.
    /// Attraversa in modo esaustivo tutte le clausole (`SelectFields`, `Join`, `Where`, `GroupBy`, `Having`, `OrderBy`)
    /// e invoca `RewriteExpression` o `RewriteTarget` sui loro componenti, garantendo che l'intera query
    /// sia semanticamente risolta prima dell'esecuzione.
    /// </summary>
    private static SpeedSelectStmt RewriteSpeedSelectQuery(SpeedSelectStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        return stmt with
        {
            SelectFields = stmt.SelectFields
                ?.Select(f => f with { Expression = RewriteExpression(f.Expression, context, program) })
                // ReSharper disable once UseCollectionExpression
                .ToImmutableArray(),
            IntoTarget = stmt.IntoTarget != null ? RewriteTarget(stmt.IntoTarget, context, program) : null,
            Join = stmt.Join != null
                ? stmt.Join with { Condition = RewriteExpression(stmt.Join.Condition, context, program) }
                : null,
            Where = stmt.Where != null ? RewriteExpression(stmt.Where, context, program) : null,
            // ReSharper disable once UseCollectionExpression
            GroupBy = stmt.GroupBy?.Select(e => RewriteExpression(e, context, program)).ToImmutableArray(),
            Having = stmt.Having != null ? RewriteExpression(stmt.Having, context, program) : null,
            OrderBy = stmt.OrderBy
                ?.Select(o => o with { Expression = RewriteExpression(o.Expression, context, program) })
                // ReSharper disable once UseCollectionExpression
                .ToImmutableArray(),
            QueryExpression = stmt.QueryExpression != null
                ? RewriteExpression(stmt.QueryExpression, context, program)
                : null
        };
    }

    /// <summary>
    /// Riscrive un'istruzione SpeedSelectStmt, che è una query complessa.
    /// Delega la riscrittura delle numerose sotto-espressioni (SELECT, WHERE, GROUP BY, etc.)
    /// a un metodo helper specializzato (`RewriteSpeedSelectQuery`). Per quanto riguarda il contesto,
    /// si assume che una query `SELECT` non modifichi l'alias attivo globale, quindi il `DbfContext`
    /// viene propagato senza modifiche. Una logica futura potrebbe analizzare la clausola `INTO`
    /// per dedurre cambiamenti di stato.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteSpeedSelectStatement(SpeedSelectStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // La riscrittura di una query SPEED SELECT è complessa. Per ora, ci concentriamo sull'aggiornamento del contesto
        // se la query stessa cambia l'alias attivo, anche se questo è meno comune.
        // La logica di riscrittura interna alla query viene gestita da un helper.
        var rewrittenStmt = RewriteSpeedSelectQuery(stmt, context, program);

        // Per semplicità, assumiamo che SpeedSelect non cambi l'alias attivo globale.
        // Una logica più avanzata potrebbe analizzare la clausola INTO.
        var newContext = context;

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `SpeedCreateIndexStmt`.
    /// </summary>
    private static SpeedCreateIndexStmt RewriteSpeedCreateIndexStatement(SpeedCreateIndexStmt stmt,
        AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenIndexName = RewriteExpression(stmt.IndexName, context, program);
        var rewrittenAlias = RewriteExpression(stmt.Alias, context, program);

        var aliasName = (rewrittenAlias as VariableExpr)?.NameCleaned.Lexeme ??
                        (rewrittenAlias as StringExpr)?.Value;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(aliasName ?? "", out var udtSymbolId);

        var keyContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, keyContext, program);
        ValidateRewrittenIndexKey(rewrittenKeyExpression, fieldIsUnsafe: false, keyContext, program, stmt.Token);

        return stmt with
        {
            IndexName = rewrittenIndexName,
            Alias = rewrittenAlias,
            KeyExpression = rewrittenKeyExpression
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `SpeedSetFilterStmt`.
    /// </summary>
    private static SpeedSetFilterStmt RewriteSpeedSetFilterStatement(SpeedSetFilterStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenAlias = RewriteExpression(stmt.Alias, context, program);

        if (stmt.Condition == null) return stmt with { Alias = rewrittenAlias };

        var aliasName = (rewrittenAlias as VariableExpr)?.NameCleaned.Lexeme ??
                        (rewrittenAlias as StringExpr)?.Value;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(aliasName ?? "", out var udtSymbolId);

        var filterContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenCondition = RewriteExpression(stmt.Condition, filterContext, program);

        return stmt with
        {
            Alias = rewrittenAlias,
            Condition = rewrittenCondition
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `SpeedSetRelationStmt`.
    /// </summary>
    private static SpeedSetRelationStmt RewriteSpeedSetRelationStatement(SpeedSetRelationStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        var rewrittenParentAlias = RewriteExpression(stmt.ParentAlias, context, program);
        var rewrittenChildAlias = stmt.ChildAlias != null ? RewriteExpression(stmt.ChildAlias, context, program) : null;

        if (stmt.KeyExpression == null)
            return stmt with { ParentAlias = rewrittenParentAlias, ChildAlias = rewrittenChildAlias };

        var parentAliasName = (rewrittenParentAlias as VariableExpr)?.NameCleaned.Lexeme ??
                              (rewrittenParentAlias as StringExpr)?.Value;

        context.DbfState.AliasToUdtSymbolId.TryGetValue(parentAliasName ?? "", out var udtSymbolId);

        var keyContext = context with { CurrentUdtScopeId = udtSymbolId > 0 ? udtSymbolId : null };
        var rewrittenKeyExpression = RewriteExpression(stmt.KeyExpression, keyContext, program);

        return stmt with
        {
            ParentAlias = rewrittenParentAlias,
            ChildAlias = rewrittenChildAlias,
            KeyExpression = rewrittenKeyExpression
        };
    }

    /// <summary>
    /// Riscrive un'istruzione SpeedSetOrderStmt.
    /// </summary>
    /// <remarks>
    /// Storia di questa funzione (per evitare di reintrodurre il bug una terza volta):
    /// una versione precedente tentava di parsare il nome indice tra virgolette come
    /// espressione di campo ("TENTATIVO 1"), ripiegando su una stringa letterale solo se
    /// il parsing/binding lanciava un'eccezione ("TENTATIVO 2"). Il problema: per un
    /// identificatore sintatticamente valido (es. "CittaIdx", "ID_Ordine"),
    /// RewriteVariableExpr (Linker.Rewrites.Variables.cs) non lancia MAI fuori da OPTION
    /// STRICT ON — degrada silenziosamente a SymbolTable.DynamicScopeSymbolId come
    /// "ultima spiaggia" (pensato per i campi DBF/ISAM dinamici legittimi). Il TENTATIVO 1
    /// "riusciva" quindi sempre per qualunque nome indice dall'aspetto di un identificatore
    /// valido — che è la stragrande maggioranza dei nomi indice in pratica — anche quando
    /// NON era affatto un campo reale. A runtime, la VariableExpr con DynamicScopeSymbolId
    /// falliva la ricerca nel DynamicScopeStack e ripiegava sull'ultima-spiaggia "qualunque
    /// variabile nello scope", restituendo il valore di una variabile completamente
    /// estranea come nome indice (es. "CittaIdx" valutato al valore di una variabile locale
    /// cPath="speed_test_data" dichiarata altrove nella stessa procedura). Anche quando
    /// l'identificatore corrispondeva a un CAMPO REALE (es. "ID_Ordine", un campo autentico
    /// del TYPE Ordine), TENTATIVO 1 produceva comunque un riferimento a campo non valutabile
    /// in questo contesto (nessun record corrente attivo), fallendo con "Accesso a campi non
    /// supportato per il tipo 'String'".
    ///
    /// Un nome indice tra virgolette (`SET ORDER TO "CittaIdx" ON alias`) è per convenzione
    /// SEMPRE un nome letterale in questo linguaggio — se l'utente intendesse un'espressione
    /// di campo dinamica, la scriverebbe senza virgolette (`SET ORDER TO Citta ON alias`,
    /// gestito dal ramo `stmt.IndexName is not StringExpr` sopra). Non c'è quindi bisogno
    /// (né un modo affidabile, vista la degradazione silenziosa di RewriteVariableExpr) di
    /// tentare la risoluzione come campo per una stringa letterale: va sempre lasciata
    /// invariata come nome indice.
    /// </remarks>
    private static Stmt RewriteSpeedSetOrderStatement(SpeedSetOrderStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenAlias = RewriteExpression(stmt.Alias, context, program);

        // Se l'espressione dell'indice non è una stringa letterale (es. una variabile o
        // un riferimento a campo esplicito senza virgolette), la risolviamo normalmente.
        if (stmt.IndexName is not StringExpr)
        {
            var rewrittenIndexName = RewriteExpression(stmt.IndexName, context, program);
            return stmt with { Alias = rewrittenAlias, IndexName = rewrittenIndexName };
        }

        // Stringa letterale tra virgolette: sempre un nome di indice, mai un'espressione
        // di campo da risolvere (vedi remarks). Lasciata invariata.
        return stmt with { Alias = rewrittenAlias };
    }

    /// <summary>
    /// Espande CHECKED "expr" in un AST parsato e validato nel contesto work area corrente.
    /// ponytail: valida solo contro CurrentUdtScopeId (alias singolo); JOIN multi-tabella non coperto.
    /// </summary>
    private static Expr RewriteCheckedStringExpr(CheckedStringExpr cse, AstRewriteContext context,
        BasicProgram program)
    {
        if (!context.CurrentUdtScopeId.HasValue && !context.IsInClipperMode)
        {
            throw CreateError(cse.CheckedToken,
                "CHECKED richiede una work area DBF attiva per validare i riferimenti ai campi (usare SELECT).",
                ErrorCode.NoDatabaseInUse);
        }

        var originalSourceLine = program.SourceMap.FirstOrDefault(l =>
                                     l.OriginalLineNumber == cse.Position.Line && l.FileId == cse.Position.FileId)
                                 ?? new SourceLine(cse.Position.FileId, "", cse.Position.Line, cse.Value);
        var baseColumn = cse.StringToken.Column + 1;

        Expr parsed;
        try
        {
            parsed = Parser.ParseSingleExpression(cse.Value, program, originalSourceLine, baseColumn, null);
        }
        catch (BasicException ex)
        {
            throw CreateError(cse.CheckedToken, $"Espressione CHECKED non valida: {ex.Message}", ex.Code);
        }

        return RewriteExpression(parsed, context, program);
    }
}