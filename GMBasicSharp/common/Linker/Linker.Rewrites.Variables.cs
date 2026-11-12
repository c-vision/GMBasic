// FILE: Interpreter.Linker.Rewrites.Variables.cs
// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Riscrive un nodo VariableExpr. Questa è la funzione orchestratore critica del Rewriter.
    /// Gestisce la risoluzione dei simboli in un ordine di priorità rigoroso per garantire la
    /// correttezza semantica, specialmente in contesti ibridi (Codice + Database).
    ///
    /// ORDINE DI RISOLUZIONE:
    /// 1.  Lambda Scope: Priorità assoluta per i parametri delle lambda interne.
    /// 2.  Pre-Resolved: Gestione di simboli già risolti (Costanti, Closure, Ottimizzazioni).
    /// 3.  Contesto DBF/UDT: (Tentativo 1) Se un comando DB richiede un campo, questo ha la precedenza sulle variabili globali.
    /// 4.  Import: (Tentativo 2) Risoluzione tramite namespace importati (Selettivi e Wildcard).
    /// 5.  Fallback: (Tentativo 3) Ricerca globale/locale per nome esatto. CRUCIALE: Ignora Campi/Proprietà per evitare falsi positivi.
    /// 6.  Suffissi: Ricerca lasca per variabili con suffissi di tipo (es. Var$).
    /// 7.  Alias: Verifica se il nome corrisponde a un alias di Namespace o Database.
    /// 8.  Late Binding: (Clipper Mode) Se nulla è stato trovato, assume sia un campo di database dinamico.
    /// 9.  Errore: Se tutti i tentativi falliscono, lancia UndefinedVariable.
    /// </summary>
    private static Expr RewriteVariableExpr(VariableExpr ve, AstRewriteContext context, BasicProgram program,
        bool isMacroArgument = false)
    {
        const bool log = false; // Log forzato per debug
        Log($"[REWR-VAR] >>> Inizio riscrittura per VariableExpr ('{ve}') in Scope {ve.ScopeId}. Current ExpectedType: {context.ExpectedType?.ToString() ?? "N/A"}", log);

        // Se l'espressione è 'SELF' o 'BASE', la sua risoluzione dipende esclusivamente dal contesto della
        // classe corrente, non da una ricerca nella SymbolTable. Questo bypassa tutti gli altri meccanismi.
        if (ve.NameCleaned.Kind is TokenKind.Self or TokenKind.Base ||
            ve.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase) ||
            ve.NameCleaned.Lexeme.Equals("ME", StringComparison.OrdinalIgnoreCase) ||
            ve.NameCleaned.Lexeme.Equals("BASE", StringComparison.OrdinalIgnoreCase))
        {
            if (context.CurrentClass != null)
            {
                var selfSymbolId = FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program);
                if (selfSymbolId >= 0)
                {
                    return ve with { SymbolId = selfSymbolId };
                }
            }
            // UDT methods/constructors: risolvi SELF per SCOPE CHAIN (stesso meccanismo
            // del ramo CurrentClass sopra) — il simbolo SELF è registrato nello scope
            // della procedura da RegisterConstructorMember/RegisterProcedure, e lo
            // ScopeId del nodo punta a quello scope.
            if (context.CurrentUdtScopeId.HasValue)
            {
                var scopedUdtSelfId = FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program);
                if (scopedUdtSelfId >= 0)
                    return ve with { SymbolId = scopedUdtSelfId };

                // Fallback storico ("ultimo SELF registrato"): SBAGLIATO con più di un
                // tipo nel modulo (prendeva il SELF dell'ULTIMO tipo dichiarato, es. il
                // SELF del DELETE di un'altra classe dentro un metodo UDT — scoperto
                // testando la prova AOT del backend IL, che ha molti tipi nello stesso
                // modulo, mentre i test piccoli a tipo singolo passavano per coincidenza).
                // Mantenuto SOLO come ultima spiaggia quando la scope chain non trova
                // nulla, per non regredire eventuali casi che vi facevano affidamento.
                var selfCandidates = program.Symbols.FindSymbolsByName("SELF");
                var selfInUdtScope = selfCandidates.Count > 0 ? selfCandidates[selfCandidates.Count - 1] : null;
                if (selfInUdtScope != null)
                    return ve with { SymbolId = selfInUdtScope.Id };
            }
        }
        
        // 0b. Campo SCALARE bare in un metodo di classe → `SELF.<campo>` (LETTURA). Il binder
        // risolve già al simbolo del campo, ma un VariableExpr nudo viene letto come LOCAL dal
        // backend (simmetrico alla scrittura). Qui, per qualunque SymbolId già risolto.
        if (ve.SymbolId >= 0 && context.CurrentClass is { } ccReadPre)
        {
            var (fmReadAny, _) = FindFieldInHierarchy(ccReadPre, ve.NameCleaned.Lexeme);
            if (fmReadAny is FieldMember fmReadPre && ve.SymbolId == fmReadPre.Declaration.SymbolId)
            {
                var selfTokRP = new Token(TokenKind.Self, "SELF", null, ve.Position);
                var selfIdRP = FindSymbolIdInScopeChain("SELF", ve.ScopeId, ve.Position, program);
                var selfVarRP = new VariableExpr(selfIdRP, selfTokRP, selfTokRP) { ScopeId = ve.ScopeId };
                return new FieldAccessExpr(selfVarRP, ve.NameCleaned)
                {
                    FieldSymbolId = fmReadPre.Declaration.SymbolId,
                    ResolvedType = fmReadPre.Declaration.DataType,
                    ScopeId = ve.ScopeId
                };
            }
        }

        string varName = ve.NameCleaned.Lexeme;

        // 1. Lambda Scope (Priorità assoluta interna)
        if (TryResolveLambdaParameter(ve, context, program) is { } lambdaResult)
        {
            Log($"[REWR-VAR]   -> Risolto come parametro Lambda: '{varName}'.", log);
            return lambdaResult;
        }
        
        // 2. ID già valido (Ottimizzazione/Costanti/Closure o già risolto)
        if (ve.SymbolId >= 0)
        {
            // Il PARSER può aver già risolto il nome a un simbolo dichiarato in un BLOCCO che non
            // è più attivo (blocco chiuso): il nome non è in scope, quindi non va usato.
            // In OPTION STRICT è un errore; fuori da STRICT il nome vale come variabile implicita
            // (default 0/"") — stessa regola di TryResolveFallback, ma qui perché l'ID è già
            // presente e il fallback per nome non verrebbe mai raggiunto (2026-11-11).
            if (ve.ScopeId >= 0)
            {
                var preSym = program.Symbols.GetSymbol(ve.SymbolId);
                if (preSym is not null && !preSym.Flags.HasFlag(SymbolFlags.IsGlobal)
                    && program.Symbols.GetScope(preSym.ScopeId).Type == SymbolTable.ScopeType.Block
                    && FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program) < 0
                    && context.IsInStrictContext)
                {
                    // Solo in OPTION STRICT ON: il nome è dichiarato in un blocco che non è più
                    // attivo → errore. Fuori da STRICT si lascia il comportamento storico
                    // (lettura risolta: è codice errato per la spec §2.1/§6, valore non
                    // garantito). NON si sostituisce con un letterale: la stessa condizione può
                    // scattare per nomi che SONO in scope (i ScopeId del parser non seguono
                    // sempre i blocchi) e romperebbe la creazione implicita nelle chiamate BYREF
                    // (ScopesByRef). Vedi la nota Bug/Plan sullo scope di blocco.
                    throw CreateError(ve.NameSuffixed,
                        $"In OPTION STRICT mode, la variabile '{ve.NameSuffixed.Lexeme}' deve essere dichiarata esplicitamente.",
                        ErrorCode.UndefinedVariable);
                }
            }
            Log($"[REWR-VAR]   -> VariableExpr ha già SymbolId {ve.SymbolId}. Processo RewritePreResolvedSymbol...", log);
            return RewritePreResolvedSymbol(ve, context, program);
        }

        // 2b. In un metodo di classe, un nome NUDO che è un CAMPO SCALARE della classe
        // corrente (non ombreggiato da parametro/locale) va letto come `SELF.<campo>`:
        // altrimenti il backend lo tratta come LOCAL e legge il default (bug simmetrico a
        // `v = 5` che non persiste nel campo). Gli array hanno il loro percorso.
        if (context.CurrentClass is { } ccReadBare)
        {
            var (fmReadBare, _) = FindFieldInHierarchy(ccReadBare, varName);
            if (fmReadBare is FieldMember fieldMemReadBare)
            {
                var scopedReadId = ve.ScopeId >= 0
                    ? FindSymbolIdInScopeChain(varName, ve.ScopeId, ve.Position, program)
                    : -1;
                var shadowedRead = scopedReadId >= 0 && scopedReadId != fieldMemReadBare.Declaration.SymbolId;
                if (!shadowedRead)
                {
                    var selfTokRead = new Token(TokenKind.Self, "SELF", null, ve.Position);
                    var selfIdRead = FindSymbolIdInScopeChain("SELF", ve.ScopeId, ve.Position, program);
                    var selfVarRead = new VariableExpr(selfIdRead, selfTokRead, selfTokRead) { ScopeId = ve.ScopeId };
                    return new FieldAccessExpr(selfVarRead, ve.NameCleaned)
                    {
                        FieldSymbolId = fieldMemReadBare.Declaration.SymbolId,
                        ResolvedType = fieldMemReadBare.Declaration.DataType,
                        ScopeId = ve.ScopeId
                    };
                }
            }
        }

        // 3. Risoluzione tramite ScopeId (Ricerca lessicale standard)
        // Forza la ricerca partendo dallo scope impresso nel nodo, ignorando la posizione del cursore globale.
        // Questa chiamata ora risolverà correttamente anche 'SELF'.
        var scopedSymbolId = FindSymbolIdInScopeChain(varName, ve.ScopeId, ve.Position, program);
        if (scopedSymbolId >= 0)
        {
            Log($"[REWR-VAR]   -> '{varName}' risolto tramite FindSymbolIdInScopeChain (Scope ID {ve.ScopeId}) -> ID {scopedSymbolId}", log);
            // Item 9 (audit rewrite non uniforme): un ritorno diretto qui saltava la
            // sostituzione di closure, il rilevamento delle catture, la conversione
            // ProcedureDefinition->FunctionReferenceExpr e il constant folding — tutti
            // gestiti da RewritePreResolvedSymbol, usato correttamente dal ramo 2 sopra
            // per un SymbolId GIA' presente sul nodo.
            return RewritePreResolvedSymbol(ve with { SymbolId = scopedSymbolId }, context, program);
        }

        // 4. Contesto DBF/UDT Esplicito (Priorità semantica)
        if (TryResolveDbfContext(ve, context, program) is { } dbResult)
        {
            Log($"[REWR-VAR]   -> Risolto tramite contesto DBF/UDT: '{varName}'.", log);
            return dbResult;
        }

        // 5. Import (Wildcard/Selettivi)
        if (TryResolveImports(ve, context, program) is { } importResult)
        {
            Log($"[REWR-VAR]   -> Risolto tramite IMPORT: '{varName}'.", log);
            return importResult;
        }

        // 6. Fallback: Ricerca Globale/Locale per nome esatto (Recupero simboli persi)
        if (TryResolveFallback(ve, context, program) is { } fallbackResult)
        {
            Log($"[REWR-VAR]   -> Risolto tramite fallback globale/locale: '{varName}'.", log);
            return fallbackResult;
        }

        // 7. Ricerca per Suffisso
        if (TryResolveSuffix(ve, context, program) is { } suffixResult)
        {
            Log($"[REWR-VAR]   -> Risolto tramite suffisso: '{varName}'.", log);
            return suffixResult;
        }

        // 8. Alias
        if (TryResolveAlias(ve, context, program) is { } aliasResult)
        {
            Log($"[REWR-VAR]   -> Risolto tramite alias: '{varName}'.", log);
            return aliasResult;
        }

        // 9. Late Binding (Clipper Mode)
        if (context.IsInClipperMode && !isMacroArgument)
        {
            // Non applicare late-binding a variabili dichiarate con DIM.
            // Se il nome esiste nella SymbolTable come VariableDeclaration,
            // significa che e' una variabile BASIC normale, non un campo DBF.
            var existingVar = program.Symbols.FindSymbolsByName(varName)
                .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration);
            if (existingVar == null)
            {
                Log($"[REWR-VAR]   -> Fallback a Late Binding (Clipper Mode) per '{varName}'.", log);
                var dbToken = new Token(TokenKind.Identifier, "_dbRecord", null, ve.Position);
                var baseRecordExpr = new VariableExpr(SymbolTable.DbfRecordBufferSymbolId, dbToken, dbToken);
                return new FieldAccessExpr(baseRecordExpr, ve.NameCleaned)
                {
                    FieldSymbolId = SymbolTable.UnresolvedSymbolId,
                    ScopeId = ve.ScopeId
                };
            }
            Log($"[REWR-VAR]   -> Saltato Late Binding per '{varName}': variabile dichiarata con DIM.", log);
        }

        // 10. Fallback finale: Strict Mode o Dynamic Scope
        if (context.IsInStrictContext)
        {
            Log($"[REWR-VAR]   -> ERRORE: Simbolo non definito in OPTION STRICT ON: '{varName}'.", true);
            throw CreateError(ve.NameSuffixed,
                $"Il simbolo '{varName}' non è definito. Con OPTION STRICT ON, tutte le variabili " +
                $"devono essere dichiarate esplicitamente.", ErrorCode.UndefinedVariable);
        }
        
        Log($"[REWR-VAR]   -> Fallback a Dynamic Scope per '{varName}'.", log);
        return ve with { SymbolId = SymbolTable.DynamicScopeSymbolId };
    }
    
    /// <summary>
    /// Tenta di risolvere la variabile come un parametro definito nello scope di una lambda expression corrente.
    /// Questo ha la priorità assoluta per evitare che variabili esterne mascherino i parametri locali della lambda.
    /// </summary>
    private static Expr? TryResolveLambdaParameter(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        if (context.LambdaScope != null && context.LambdaScope.ContainsKey(ve.NameCleaned.Lexeme))
        {
            var resolvedId = FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program);
            if (resolvedId != SymbolTable.UnresolvedSymbolId) return ve with { SymbolId = resolvedId };
        }

        return null;
    }

    /// <summary>
    /// Gestisce i nodi VariableExpr che hanno già un SymbolId valido (>= 0).
    /// Responsabilità:
    /// - Sostituzione simboli per le Closure (Heap Promotion).
    /// - Rilevamento delle catture (IsCaptured) durante la fase di pre-analisi.
    /// - Trasformazione in FunctionReferenceExpr se il simbolo è una procedura.
    /// - Constant Folding: Sostituzione immediata delle costanti con il loro valore letterale.
    /// </summary>
    private static Expr RewritePreResolvedSymbol(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        var symbolId = ve.SymbolId;

        // Sostituzione Simboli (Closure)
        if (context.SymbolMap != null && context.SymbolMap.TryGetValue(symbolId, out var replacementExpr))
        {
            return replacementExpr;
        }

        var symbol = program.Symbols.GetSymbol(symbolId);

        // Rilevamento Catture
        if (context.SymbolMap == null && context.CurrentProcedure != null)
        {
            var defScope = program.Symbols.GetScope(symbol.ScopeId);
            var currentProcId = context.CurrentProcedure.SymbolId;

            if (defScope.Type == SymbolTable.ScopeType.Procedure && defScope.OwnerSymbolId != currentProcId)
            {
                if (!symbol.Has(SymbolFlags.IsCaptured))
                {
                    program.Symbols.UpdateSymbolFlags(symbolId, SymbolFlags.IsCaptured);
                    Log($"[REWRITER-CLOSURE] Rilevata cattura: '{symbol.OriginalName}' (ID {symbolId}).", RewriteLog);
                }
            }
        }

        if (symbol.DeclarationNode is ProcedureDefinition)
        {
            // Lettura del PROPRIO nome dall'interno del corpo della funzione stessa (return-
            // slot, es. `Somma = A + B: PRINT Somma`): NON è un riferimento a funzione di
            // prima classe (delegate) come lo sarebbe altrove — è il valore già assegnato al
            // return-slot. Il backend IL riserva già un local ordinario per
            // `_currentReturnSymbolId == currentProcedure.SymbolId` (vedi
            // EmitEndProcedureStatement/EmitLoadVariable in IlProgramCompiler) — qui va
            // lasciato un VariableExpr "normale" invece di un FunctionReferenceExpr, altrimenti
            // il backend costruirebbe un delegate (System.Func&lt;...&gt;) al posto del valore
            // (bug gemello del fix in ParseFunctionReturnAssignmentStatement, che produce
            // un'assegnazione bare risolta allo stesso simbolo per lo stesso motivo).
            if (context.CurrentProcedure != null && context.CurrentProcedure.SymbolId == symbolId)
            {
                return ve with { SymbolId = symbolId };
            }
            return BareProcedureReference(symbolId, ve, program);
        }

        // Constant Folding
        if (symbol.Flags.HasFlag(SymbolFlags.IsConst) &&
            symbol.DeclarationNode is ConstDeclaration constDecl)
        {
            return PerformConstantFolding(constDecl, ve, context, program);
        }

        // Inferenza tipo parametro da uso nel corpo (bug 2026-09-08, BackendIL-GPM
        // InvalidProgramException). Il PARSER assegna DoubleType (non InferredType) ai parametri SUB/FUNCTION senza
        // clausola AS né suffisso di tipo — stesso default "implicito" verificato da OPTION
        // STRICT più sotto in RewriteProcedure (HasAsClause:false + nessun suffisso). Se il
        // corpo lo passa come argomento a una chiamata con parametro di tipo NOTO diverso da
        // double (qui: `ExpectedType`, già propagato da RewriteSingleArgument per OGNI
        // chiamata, nativa o utente), quello è evidenza concreta del suo vero tipo — es.
        // `AI.AGENTRUN(NomeAgente, TaskText)` con NomeAgente/TaskText parametri STRING nativi.
        // Senza questo, il call-site (che passa stringhe reali) e la firma compilata del
        // parametro (double di default) disaccordano — sul backend IL lo scopre il
        // verificatore CLR (InvalidProgramException), sui transpiler C99 lo scopre `cc` (type
        // mismatch). Guardia stretta: solo parametri GENUINAMENTE impliciti (niente AS, niente
        // suffisso — stessa condizione di OPTION STRICT sotto), mai un `AS DOUBLE`/`x#`
        // esplicito, per non toccare un double voluto dal programmatore.
        if (symbol.DataType is DoubleType
            && symbol.Flags.HasFlag(SymbolFlags.IsParameter)
            && symbol.DeclarationNode is Parameter { HasAsClause: false } implicitParam
            && SplitNameAndSuffix(implicitParam.OriginalName.Lexeme).SuffixType == null
            && context.ExpectedType is not null and not InferredType and not DoubleType)
        {
            program.Symbols.UpdateSymbolType(symbolId, context.ExpectedType);
        }

        return ve;
    }

    /// <summary>
    /// Esegue il "Constant Folding" (piegatura delle costanti).
    /// Riscrive ricorsivamente il valore iniziale di una dichiarazione costante e restituisce un'espressione letterale.
    /// </summary>
    private static Expr PerformConstantFolding(ConstDeclaration constDecl, VariableExpr ve, AstRewriteContext context,
        BasicProgram program)
    {
        // ExpectedType azzerato: il valore della costante va piegato al suo tipo LETTERALE
        // proprio, mai al tipo del contesto in cui il RIFERIMENTO alla costante compare.
        // Propagando l'ExpectedType ambiente (es. INTEGER, ereditato dal target di
        // un'assegnazione "x AS INTEGER = FILE_CREATED"), RewriteExpression avvolgeva il
        // NumberExpr letterale in una conversione esplicita implicita (CONVERSIONS.CINT(1)) —
        // corretto per un letterale scritto DIRETTAMENTE in quella posizione, ma qui rompe il
        // folding: lo switch sotto accetta solo NumberExpr/StringExpr/BooleanExpr nudi, quindi
        // qualunque costante referenziata fuori da un contesto "neutro" (PRINT, dove
        // ExpectedType resta null) falliva con "Valore costante non letterale" — riproducibile
        // con "DIM x AS INTEGER: x = FILE_CREATED" mentre "PRINT FILE_CREATED" funzionava.
        var rewrittenValue = RewriteExpression(constDecl.InitialValue, context with { ExpectedType = null }, program);
        return rewrittenValue switch
        {
            NumberExpr n => new NumberExpr(n.Value, ve.NameCleaned),
            StringExpr s => new StringExpr(s.Value, ve.NameCleaned),
            BooleanExpr b => new BooleanExpr(b.Value, ve.NameCleaned),
            _ => throw new BasicException(constDecl, "Valore costante non letterale.")
        };
    }

    /// <summary>
    /// Riferimento NUDO (senza parentesi) a una procedura. Una NATIVA a 0 parametri che ritorna un valore
    /// (`PRINT TIME$`, `x = STRDATE`, `TI`) e' una CHIAMATA builtin — stessa semantica su tutti i backend
    /// (prima IL la valutava come chiamata, P-Code spingevano una closure/funptr NULL).
    /// Ogni altra procedura resta un <see cref="FunctionReferenceExpr"/> (funzione di prima classe).
    /// </summary>
    private static Expr BareProcedureReference(int symbolId, VariableExpr ve, BasicProgram program)
    {
        if (program.Symbols.GetSymbol(symbolId).DeclarationNode is NativeProcedureDefinition { Parameters.Length: 0, IsFunction: true } npd)
        {
            // Il namespace della chiamata DEVE essere quello vero della funzione: prima era
            // la stringa VUOTA ("." + nome), e PCode/VM-C dispatchano anche per namespace →
            // `PRINT STRTIME` (senza parentesi) stampava 0 mentre IL dava l'ora. Si risolve
            // dal simbolo; se il simbolo non è materializzato nella symbol table (definizione
            // del registro) si usa il namespace del registro per quel BuiltinKind.
            var nsName = program.Symbols.GetNamespaceNameForSymbol(symbolId);
            if (string.IsNullOrEmpty(nsName)
                && global::Basic.StdLib.Instance.BuiltinFunctionProvidersMap.TryGetValue(npd.BuiltinKind, out var nsProviders)
                && nsProviders.Count > 0)
                nsName = nsProviders[0];
            var nsToken = new Token(TokenKind.Identifier, nsName ?? "", null, ve.NameCleaned.Position);
            var nameToken = new Token(npd.BuiltinKind, npd.OriginalName.Lexeme, null, npd.OriginalName.Position);
            return new BuiltinFunctionCallExpr(nsToken, nameToken, System.Collections.Immutable.ImmutableArray<Expr>.Empty, npd.ReturnType);
        }
        return new FunctionReferenceExpr(symbolId, ve.NameCleaned);
    }

    /// <summary>
    /// TENTATIVO 1: Risoluzione Contestuale DBF/UDT.
    /// Verifica se il nome della variabile corrisponde a un campo dell'UDT associato alla work area attiva.
    /// Se trovato, trasforma il nodo in un `FieldAccessExpr` (es. `_dbRecord.Campo`).
    /// Questo ha priorità semantica sulle variabili globali in comandi come `REPLACE` o `INDEX ON`.
    /// </summary>
    private static Expr? TryResolveDbfContext(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        if (context.CurrentUdtScopeId.HasValue)
        {
            var udtSymbol = program.Symbols.GetSymbol(context.CurrentUdtScopeId.Value);
            if (udtSymbol.DeclarationNode is UserDefinedType udt)
            {
                var field = udt.Fields.FirstOrDefault(f =>
                    f.Name.Lexeme.Equals(ve.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (field != null)
                {
                    var dbToken = new Token(TokenKind.Identifier, "_dbRecord", null, ve.Position);
                    var baseRecordExpr = new VariableExpr(SymbolTable.DbfRecordBufferSymbolId, dbToken, dbToken);
                    return new FieldAccessExpr(baseRecordExpr, ve.NameCleaned)
                    {
                        FieldSymbolId = field.SymbolId,
                        ScopeId = ve.ScopeId
                    };
                }
            }
        }

        return null;
    }

    /// <summary>
    /// TENTATIVO 2: Risoluzione tramite Import.
    /// Cerca il simbolo nei namespace importati dal modulo corrente.
    /// Gestisce:
    /// - Import Selettivi (`IMPORT NS.{Membro}`): Priorità alta.
    /// - Import Wildcard (`IMPORT NS`) e Default (`SYSTEM`): Priorità bassa, con controllo di ambiguità.
    /// </summary>
    private static Expr? TryResolveImports(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        // Canone data/ora: TIME$/TI$ = STRTIME, DATE$ = STRDATE (il '$' non e' un semplice suffisso di tipo).
        var variableName = ve.NameSuffixed.Lexeme.ToUpperInvariant() switch
        {
            "TIME$" or "TI$" => "STRTIME",
            "DATE$" => "STRDATE",
            _ => ve.NameCleaned.Lexeme
        };
        if (!program.LoadedModules.TryGetValue(context.ModulePath, out var currentModule)) return null;

        // Import Selettivi
        foreach (var import in currentModule.Imports.Where(i => i.Mode == ImportMode.SpecificMembers))
        {
            var memberMatch = import.SpecificMembers.FirstOrDefault(m =>
                (m.Alias?.Lexeme ?? m.OriginalName.Lexeme).Equals(variableName, StringComparison.OrdinalIgnoreCase));

            if (memberMatch != null)
            {
                var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(import.NamespaceName.Lexeme);
                if (nsSymbolIds != null)
                {
                    var foundSymbol = nsSymbolIds
                        .Select(id => program.Symbols.GetSymbol(id))
                        .FirstOrDefault(s =>
                            s.OriginalName.Equals(memberMatch.OriginalName.Lexeme,
                                StringComparison.OrdinalIgnoreCase) && s.IsPublic);
                    if (foundSymbol != null)
                    {
                        // Item 8 (audit rewrite non uniforme): una costante raggiunta SOLO
                        // tramite IMPORT NS.{Membro} non veniva mai piegata al suo valore
                        // letterale — restava un VariableExpr risolto ma non foldato.
                        if (foundSymbol.Flags.HasFlag(SymbolFlags.IsConst) &&
                            foundSymbol.DeclarationNode is ConstDeclaration foundConstDecl)
                        {
                            return PerformConstantFolding(foundConstDecl, ve, context, program);
                        }
                        return ve with { SymbolId = foundSymbol.Id };
                    }
                }
            }
        }

        // Import Wildcard
        var visibleNamespaces = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var ns in program.Settings.DefaultNamespaces) visibleNamespaces.Add(ns);
        foreach (var import in currentModule.Imports.Where(i => i.Mode == ImportMode.AllMembers))
        {
            visibleNamespaces.Add(import.NamespaceName.Lexeme);
        }

        List<int> candidates = [];
        foreach (var nsName in visibleNamespaces)
        {
            var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(nsName);
            if (nsSymbolIds == null) continue;
            foreach (var id in nsSymbolIds)
            {
                var symbol = program.Symbols.GetSymbol(id);
                if (symbol.OriginalName.Equals(variableName, StringComparison.OrdinalIgnoreCase) && symbol.IsPublic)
                    candidates.Add(id);
            }
        }

        // Stessa nativa esposta da piu' namespace (es. TI/STRTI in SYSTEM e DATETIME): non e' ambiguita',
        // e' la stessa funzione (stesso DispatchToken e stesso tipo di ritorno) -> prima candidata.
        if (candidates.Count > 1
            && program.Symbols.GetSymbol(candidates[0]).DeclarationNode is NativeProcedureDefinition firstNative
            && candidates.Skip(1).All(id => program.Symbols.GetSymbol(id).DeclarationNode is NativeProcedureDefinition other
                && other.BuiltinKind == firstNative.BuiltinKind
                && other.ReturnType.Equals(firstNative.ReturnType)
                && other.Parameters.Length == firstNative.Parameters.Length))
            candidates = [candidates[0]];

        if (candidates.Count == 1)
        {
            // Item 8: stesso fix del ramo selettivo sopra, per l'import wildcard/default.
            var candidateSymbol = program.Symbols.GetSymbol(candidates[0]);
            if (candidateSymbol.Flags.HasFlag(SymbolFlags.IsConst) &&
                candidateSymbol.DeclarationNode is ConstDeclaration candidateConstDecl)
            {
                return PerformConstantFolding(candidateConstDecl, ve, context, program);
            }
            return ve with { SymbolId = candidates[0] };
        }
        if (candidates.Count > 1) throw CreateError(ve.NameSuffixed, $"Riferimento ambiguo a '{variableName}'.");

        return null;
    }

    /// <summary>
    /// FASE DI RECUPERO (FALLBACK 1): Ricerca Globale/Locale per Nome Esatto.
    /// Cerca simboli che potrebbero essere sfuggiti al Resolver posizionale (es. definiti dopo l'uso in codice legacy)
    /// o che sono globali.
    /// 
    /// FIX CRITICO:
    /// Questo metodo DEVE ignorare esplicitamente i simboli con flag `IsField` o `IsProperty`.
    /// Motivo: Un `TYPE` definisce simboli per i suoi campi. Se esiste un `TYPE Cliente` con campo `Saldo`,
    /// `FindSymbolsByName("Saldo")` lo troverà. Se non lo ignoriamo qui, restituiremmo un `VariableExpr` puntante
    /// alla definizione statica del campo, impedendo al `RewriteDbfUpdateStatement` di funzionare o bloccando
    /// il passaggio al Late Binding per i campi reali del database.
    /// </summary>
    private static Expr? TryResolveFallback(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        var variableName = ve.NameCleaned.Lexeme;
        var symbolsByName = program.Symbols.FindSymbolsByName(variableName);

        foreach (var symbol in symbolsByName)
        {
            // FIX CRITICO: Ignora Campi e Proprietà nel fallback.
            if (symbol.Flags.HasFlag(SymbolFlags.IsField) ||
                symbol.Flags.HasFlag(SymbolFlags.IsProperty))
                continue;

            // Module-level DIM/GLOBAL variables carry IsGlobal even when not in _global namespace.
            if (symbol.DeclarationNode is VariableDeclaration && symbol.Flags.HasFlag(SymbolFlags.IsGlobal))
            {
                if (symbol.Flags.HasFlag(SymbolFlags.IsConst) &&
                    symbol.DeclarationNode is ConstDeclaration constDeclGlobal)
                {
                    return PerformConstantFolding(constDeclGlobal, ve, context, program);
                }

                return ve with { SymbolId = symbol.Id };
            }

            var symbolFilePath = program.FindFilePathForSymbol(symbol.DeclarationNode);
            var isLocal = symbolFilePath != null &&
                          symbolFilePath.Equals(context.ModulePath, StringComparison.OrdinalIgnoreCase);
            var isGlobal = program.Symbols.GetNamespaceNameForSymbol(symbol.Id) == SymbolTable.GlobalScopeName;

            // Simbolo dichiarato dentro un BLOCCO che non è più attivo (blocco chiuso): con
            // OPTION STRICT ON è un errore (il nome non è in scope). Fuori da STRICT si lascia il
            // comportamento storico (il nome viene risolto dal fallback): leggerlo è un errore di
            // programmazione e il valore non è garantito (specifica §2.1/§6).
            if (!symbol.Flags.HasFlag(SymbolFlags.IsGlobal)
                && program.Symbols.GetScope(symbol.ScopeId).Type == SymbolTable.ScopeType.Block
                && FindSymbolIdInScopeChain(variableName, ve.ScopeId, ve.Position, program) < 0)
            {
                if (context.IsInStrictContext)
                {
                    throw CreateError(ve.NameSuffixed,
                        $"In OPTION STRICT mode, la variabile '{ve.NameSuffixed.Lexeme}' deve essere dichiarata esplicitamente.",
                        ErrorCode.UndefinedVariable);
                }
            }

            if (isLocal || isGlobal)
            {
                // Gestione ProcedureDefinition
                if (symbol.DeclarationNode is ProcedureDefinition)
                {
                    return BareProcedureReference(symbol.Id, ve, program);
                }

                // Constant Folding
                if (symbol.Flags.HasFlag(SymbolFlags.IsConst) &&
                    symbol.DeclarationNode is ConstDeclaration constDecl)
                {
                    return PerformConstantFolding(constDecl, ve, context, program);
                }

                return ve with { SymbolId = symbol.Id };
            }
        }

        return null;
    }

    /// <summary>
    /// FASE DI RECUPERO (FALLBACK 2): Ricerca per Suffissi.
    /// Tenta di risolvere variabili che sono state dichiarate con un suffisso di tipo (es. `Nome$`)
    /// ma usate senza suffisso (es. `Nome`).
    /// </summary>
    private static Expr? TryResolveSuffix(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        var suffixes = new[] { "$", "%", "&", "!", "#", "@" };
        foreach (var s in suffixes)
        {
            var candidateName = ve.NameCleaned.Lexeme + s;
            var globalIds = program.Symbols.FindSymbolInScope(candidateName);
            if (globalIds is { Count: > 0 }) return ve with { SymbolId = globalIds[0] };
        }

        return null;
    }

    /// <summary>
    /// TENTATIVO 4/5: Risoluzione Alias.
    /// Verifica se il nome corrisponde a un Alias di Namespace (definito con `IMPORT ... AS`)
    /// o a un Alias di Database attivo.
    /// </summary>
    private static Expr? TryResolveAlias(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        var variableName = ve.NameCleaned.Lexeme;

        if (program.LoadedModules.TryGetValue(context.ModulePath, out var currentModule))
        {
            foreach (var import in currentModule.Imports)
            {
                if (import.Alias != null &&
                    import.Alias.Lexeme.Equals(variableName, StringComparison.OrdinalIgnoreCase)) return ve;
                if (import.Mode == ImportMode.Namespace &&
                    import.NamespaceName.Lexeme.Equals(variableName, StringComparison.OrdinalIgnoreCase)) return ve;
            }
        }

        if (context.DbfState.AliasToUdtSymbolId.ContainsKey(variableName))
        {
            var symbolIds = program.Symbols.FindSymbolInScope(variableName);
            if (symbolIds is { Count: 1 }) return ve with { SymbolId = symbolIds[0] };
            throw CreateError(ve.NameSuffixed, $"Errore interno: alias '{variableName}' non trovato come simbolo.");
        }

        return null;
    }

    /// <summary>
    /// TENTATIVO 8: Late Binding (Modalità Clipper).
    /// Se siamo in un modulo con `IMPORT DBF` (Clipper Mode) e nessun simbolo statico è stato trovato,
    /// assumiamo che si tratti di un campo di database da risolvere a runtime.
    /// Restituisce un `FieldAccessExpr` speciale (`_dbRecord.Nome`) con `FieldSymbolId = SymbolTable.UnresolvedSymbolId`.
    /// </summary>
    private static Expr? TryResolveLateBinding(VariableExpr ve, AstRewriteContext context, BasicProgram program)
    {
        if (!context.IsInClipperMode) return null;

        // Non applicare late-binding a variabili dichiarate con DIM.
        // Se il nome esiste nella SymbolTable come VariableDeclaration,
        // significa che e' una variabile BASIC normale, non un campo DBF.
        var existingVar = program.Symbols.FindSymbolsByName(ve.NameCleaned.Lexeme)
            .FirstOrDefault(s => s.DeclarationNode is VariableDeclaration);
        if (existingVar != null) return null;

        var dbToken = new Token(TokenKind.Identifier, "_dbRecord", null, ve.Position);
        var baseRecordExpr = new VariableExpr(SymbolTable.DbfRecordBufferSymbolId, dbToken, dbToken);
        return new FieldAccessExpr(baseRecordExpr, ve.NameCleaned)
        {
            FieldSymbolId = SymbolTable.UnresolvedSymbolId,
            ScopeId = ve.ScopeId
        };
    }
}