// FILE: Interpreter.Linker.cs
// ReSharper disable UnusedParameter.Local

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene la logica di linking per il compilatore.
/// Questa classe è statica e stateless, e opera sull'oggetto BasicProgram
/// dopo che la fase di parsing è stata completata.
/// </summary>
public static partial class Linker
{
    /// <summary>
    /// Contesto di stato per la gestione delle work area DBF durante la riscrittura.
    /// Mappa gli alias ai SymbolId dei loro tipi UDT.
    /// </summary>
    public sealed record DbfContext(
        ImmutableDictionary<string, int> AliasToUdtSymbolId,
        string? CurrentAlias,
        bool CurrentIsUnsafe = false)
    {
        public DbfContext() : this(
            ImmutableDictionary<string, int>.Empty.WithComparers(StringComparer.OrdinalIgnoreCase), null)
        {
        }
    }

    /// <summary>
    /// Incapsula tutte le informazioni necessarie per la risoluzione dei nomi durante la fase di riscrittura dell'AST.
    /// </summary>
    /// <param name="ModulePath">Il percorso del modulo il cui AST è attualmente in fase di riscrittura.</param>
    /// <param name="CurrentProcedure">La procedura attualmente in fase di riscrittura (se siamo in un corpo di funzione).</param>
    /// <param name="DbfState">Lo stato corrente delle work area DBF/ISAM (alias attivo e tipi associati).</param>
    /// <param name="IsInClipperMode">Indica se la modalità di compatibilità Clipper (late-binding) è attiva per il modulo.</param>
    /// <param name="CurrentUdtScopeId">L'ID del simbolo del tipo UDT attualmente in contesto, usato per risolvere i nomi dei campi in comandi come INDEX ON.</param>
    /// <param name="OnTypeInferred">Callback per segnalare un cambiamento di tipo durante il loop di stabilizzazione.</param>
    /// <param name="LambdaScope">Scope locale per l'inferenza dei tipi all'interno di una lambda.</param>
    /// <param name="SymbolMap">Mappa opzionale per la riscrittura dei simboli (es. per le closure: SymbolId Variabile -> Espressione Accesso Campo).</param>
    public sealed record AstRewriteContext(
        string ModulePath,
        ProcedureDefinition? CurrentProcedure,
        DbfContext DbfState,
        bool IsInClipperMode,
        bool IsInStrictContext,
        int? CurrentScopeId, 
        int? CurrentUdtScopeId = null,
        Action? OnTypeInferred = null,
        IReadOnlyDictionary<string, DataType>? LambdaScope = null,
        IReadOnlyDictionary<int, Expr>? SymbolMap = null,
        DataType? ExpectedType = null,
        ClassDefinition? CurrentClass = null,
        IReadOnlyDictionary<string, DataType>? GenericTypeArgs = null);

    /// <summary>
    /// Controlla se un modulo ha un punto di ingresso valido per l'esecuzione.
    /// Un punto di ingresso è una SUB Main (senza parametri obbligatori) o almeno una linea di codice numerata.
    /// Questa versione include log dettagliati per il debug.
    /// </summary>
    public static bool HasEntryPoint(BasicModule module, BasicProgram program)
    {
        const bool log = false; // Abilita il logging per questo metodo
        Log($"[HasEntryPoint] Inizio verifica entry point per modulo: '{Path.GetFileName(module.FilePath)}'", log);

        // 1. Controllo per codice con numeri di riga
        if (module.Statements.Count > 0)
        {
            Log(
                $"[HasEntryPoint]  -> Trovate {module.Statements.Count} collezioni di statement numerati. Entry point valido.",
                log);
            return true;
        }

        Log("[HasEntryPoint]  -> Nessuno statement numerato trovato. Cerco SUB Main...", log);

        // 2. Controllo per SUB Main
        var mainCandidates = program.Symbols.GetProceduresForNamespace(SymbolTable.GlobalScopeName)
            .Where(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase))
            .ToList();

        if (mainCandidates.Count == 0)
        {
            Log("[HasEntryPoint]  -> ERRORE: Nessuna procedura con nome 'Main' trovata nello scope globale.", log);
            return false;
        }

        Log($"[HasEntryPoint]  -> Trovati {mainCandidates.Count} candidati con nome 'Main'. Analizzo ciascuno...", log);

        foreach (var proc in mainCandidates)
        {
            Log($"[HasEntryPoint]    - Candidato ID: {proc.SymbolId}, Nome: {proc.OriginalName.Lexeme}", log);

            // CONDIZIONE A: Verifica del file di origine
            var filePath = program.FindFilePathForSymbol(proc);
            var filePathMatches = filePath?.Equals(module.FilePath, StringComparison.OrdinalIgnoreCase) ?? false;
            Log($"[HasEntryPoint]      - File Path del simbolo: '{filePath ?? "NULL"}'", log);
            Log($"[HasEntryPoint]      - File Path del modulo target: '{module.FilePath}'", log);
            Log($"[HasEntryPoint]      - Corrispondenza percorso: {filePathMatches}", log);

            if (!filePathMatches)
            {
                Log($"[HasEntryPoint]      -> SCARTATO: Non appartiene al modulo principale.", log);
                continue;
            }

            // CONDIZIONE B: Verifica dei parametri
            var paramsAreOptional = proc.Parameters.All(p => p.Flags.HasFlag(SymbolFlags.IsOptional));
            Log($"[HasEntryPoint]      - Tutti i parametri sono opzionali: {paramsAreOptional}", log);

            if (paramsAreOptional)
            {
                Log($"[HasEntryPoint]  -> TROVATO: Trovato SUB Main valido nel modulo corretto. Entry point valido.",
                    log);
                return true;
            }

            Log($"[HasEntryPoint]      -> SCARTATO: Contiene parametri obbligatori.", log);
        }

        Log("[HasEntryPoint]  -> ERRORE: Nessun SUB Main valido trovato nel modulo principale.", log);
        return false;
    }

    /// <summary>
    /// Trova, tra le procedure globali chiamate "Main", quella riconosciuta come entry-point del
    /// modulo indicato — stesso criterio di <see cref="HasEntryPoint"/> (SUB Main dichiarata nel
    /// file del modulo, con tutti i parametri opzionali), MA senza lo short-circuit sulle "righe
    /// numerate": qui serve trovare Main anche quando il modulo contiene ANCHE statement top-level
    /// (incluso, nel caso del bug che questo helper serve a diagnosticare, una chiamata esplicita
    /// a Main() stessa — che finisce proprio in quegli statement top-level, non nel body di Main).
    /// </summary>
    private static ProcedureDefinition? FindMainEntryCandidate(BasicModule module, BasicProgram program)
    {
        var mainCandidates = program.Symbols.GetProceduresForNamespace(SymbolTable.GlobalScopeName)
            .Where(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase));

        foreach (var proc in mainCandidates)
        {
            var filePath = program.FindFilePathForSymbol(proc);
            if (!(filePath?.Equals(module.FilePath, StringComparison.OrdinalIgnoreCase) ?? false)) continue;
            if (proc.Parameters.All(p => p.Flags.HasFlag(SymbolFlags.IsOptional))) return proc;
        }

        return null;
    }

    /// <summary>
    /// Vieta l'invocazione esplicita dell'entry-point Main: l'entry è già auto-eseguito da tutti i
    /// backend (IL/PCode/VM-C via prologo del compilatore, C99/C99Native via IsMainProc), quindi
    /// una chiamata esplicita produce comportamento divergente tra backend — su VM/C, IL e PCode
    /// Main viene rieseguito una seconda volta (bug "Call-Main-Esplicita-Entry-Doppia-VMC-Parita").
    /// Cerca ricorsivamente, in tutti gli statement top-level di tutti i moduli e in tutti i corpi
    /// di procedura, una FunctionExpr che punta al SymbolId di Main e lancia un errore di
    /// compilazione chiaro al primo caso trovato.
    /// </summary>
    public static void ValidateNoExplicitMainCall(BasicProgram program, BasicModule mainModule)
    {
        var mainProc = FindMainEntryCandidate(mainModule, program);
        if (mainProc == null) return;

        foreach (var module in program.LoadedModules.Values)
        {
            foreach (var stmt in module.Statements.Values.SelectMany(s => s))
            {
                var offending = FindExplicitCallTo(stmt, mainProc.SymbolId);
                if (offending != null) throw BuildExplicitMainCallError(offending);
            }
        }

        foreach (var proc in program.Symbols.GetAllProcedures())
        {
            if (proc.Body.IsDefault) continue;
            foreach (var stmt in proc.Body)
            {
                var offending = FindExplicitCallTo(stmt, mainProc.SymbolId);
                if (offending != null) throw BuildExplicitMainCallError(offending);
            }
        }
    }

    private static BasicException BuildExplicitMainCallError(FunctionExpr call) =>
        CreateError(call.OriginalToken,
            "L'entry-point 'Main' viene eseguito automaticamente all'avvio del programma su tutti i backend; " +
            "non può essere chiamato esplicitamente (produrrebbe una doppia esecuzione divergente tra backend).");

    /// <summary>Walk ricorsivo generico per riflessione: trova la prima FunctionExpr che punta a
    /// <paramref name="mainSymbolId"/> in un sottoalbero. Ciclo-safe: usa
    /// <see cref="AstReflectiveWalk.ChildNodes"/> (che salta le back-reference tipo `Parent`, v.
    /// <see cref="AstReflectiveWalk"/>) e visita ogni nodo una sola volta.</summary>
    private static FunctionExpr? FindExplicitCallTo(AstNode? node, int mainSymbolId)
        => FindExplicitCallTo(node, mainSymbolId, new HashSet<AstNode>(ReferenceEqualityComparer.Instance));

    private static FunctionExpr? FindExplicitCallTo(AstNode? node, int mainSymbolId, HashSet<AstNode> visited)
    {
        if (node == null) return null;
        if (!visited.Add(node)) return null;
        if (node is FunctionExpr fn && fn.SymbolId == mainSymbolId) return fn;

        foreach (var child in AstReflectiveWalk.ChildNodes(node))
        {
            var found = FindExplicitCallTo(child, mainSymbolId, visited);
            if (found != null) return found;
        }

        return null;
    }

    /// <summary>
    /// FASE 1.1: Esegue l'analisi statica sui simboli, operando direttamente sulla SymbolTable del programma.
    /// Valida l'assenza di collisioni di nomi e di firme di procedure duplicate all'interno di ogni namespace.
    /// Esegue anche il "constant folding" pre-calcolando i valori di tutte le costanti dichiarate e
    /// memorizza il risultato in `program.ResolvedConstants`.
    /// </summary>
    /// <param name="program">L'istanza di BasicProgram che contiene la SymbolTable completa da validare.</param>
    /// <exception cref="BasicException">Lanciata se viene rilevata una definizione duplicata o una dipendenza circolare tra costanti.</exception>
    public static void PerformStaticAnalysisAndValidation(BasicProgram program)
    {
        var resolvedConstants = new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase);
        var resolving = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        // Valida la duplicazione dei simboli non procedurali e prepara la lista di tutte le costanti
        var allConstantsToResolve = new List<ConstDeclaration>();
        var allEnumMembersToResolve = new List<(EnumDefinition Owner, EnumMember Member)>();

        // Valida ANCHE lo scope GLOBALE: BasicProgram.Namespaces NON lo include (vedi
        // BasicProgram.cs "the global scope (which may not be in Namespaces)"), quindi le
        // procedure top-level non venivano controllate → un SUB/FUNCTION con la STESSA FIRMA
        // (nome+tipi param) definito due volte a livello globale NON dava errore sui backend
        // C# (IL/PCode/VM-C), mentre il C99Native lo rifiuta. Parità col C99Native.
        foreach (var nsName in program.Namespaces
                     .Append(SymbolTable.GlobalScopeName)
                     .Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var nonProcedureSymbols =
                new Dictionary<string, (string Type, Token Token)>(StringComparer.OrdinalIgnoreCase);
            var procedureSignatures =
                new Dictionary<string, HashSet<ProcedureSignature>>(StringComparer.OrdinalIgnoreCase);

            allConstantsToResolve.AddRange(program.Symbols.GetAllConstants()
                .Where(c => program.Symbols.GetNamespaceNameForSymbol(c.SymbolId) == nsName));

            foreach (var enumDef in program.Symbols.GetAllEnums()
                         .Where(e => program.Symbols.GetNamespaceNameForSymbol(e.SymbolId) == nsName))
            {
                if (!nonProcedureSymbols.TryAdd(enumDef.OriginalName.Lexeme, ("Enum", enumDef.OriginalName)))
                    throw new BasicException(ErrorCode.DuplicateDefinition, enumDef.OriginalName.Position,
                        $"Il simbolo '{enumDef.OriginalName.Lexeme}' è già definito in questo scope.");

                allEnumMembersToResolve.AddRange(enumDef.Members.Select(m => (enumDef, m)));
            }

            // Valida le variabili globali
            var globalsInNamespace = program.GetAllGlobalVariables()
                .Where(g => program.Symbols.GetNamespaceNameForSymbol(g.SymbolId) == nsName);
            foreach (var decl in globalsInNamespace)
            {
                if (nonProcedureSymbols.TryAdd(decl.NameCleaned.Lexeme, ("Variabile Globale", decl.NameSuffixed)))
                    continue;
                var existing = nonProcedureSymbols[decl.NameCleaned.Lexeme];
                throw new BasicException(ErrorCode.DuplicateDefinition, decl.NameSuffixed.Position,
                    $"Il simbolo globale '{decl.NameSuffixed.Lexeme}' è già stato definito come '{existing.Type}' nel namespace '{nsName}'.");
            }

            var typesInNamespace = program.Symbols.GetAllUserDefinedTypes()
                .Where(t => program.Symbols.GetNamespaceNameForSymbol(t.SymbolId) == nsName);
            foreach (var udt in typesInNamespace)
            {
                if (nonProcedureSymbols.TryAdd(udt.Name.Lexeme, ("Tipo (TYPE)", udt.Name))) continue;
                var existing = nonProcedureSymbols[udt.Name.Lexeme];
                throw new BasicException(ErrorCode.DuplicateDefinition, udt.Name.Position,
                    $"Il tipo '{udt.Name.Lexeme}' è già stato definito come '{existing.Type}' nel namespace '{nsName}'.");
            }

            var classesInNamespace = program.Symbols.GetAllClasses()
                .Where(c => program.Symbols.GetNamespaceNameForSymbol(c.SymbolId) == nsName);
            foreach (var classDef in classesInNamespace)
            {
                if (nonProcedureSymbols.TryAdd(classDef.Name.Lexeme, ("Classe (CLASS)", classDef.Name))) continue;
                var existing = nonProcedureSymbols[classDef.Name.Lexeme];
                throw new BasicException(ErrorCode.DuplicateDefinition, classDef.Name.Position,
                    $"La classe '{classDef.Name.Lexeme}' è già stata definita come '{existing.Type}' nel namespace '{nsName}'.");
            }

            foreach (var proc in program.GetProceduresForNamespace(nsName))
            {
                if (nonProcedureSymbols.TryGetValue(proc.Name.Lexeme, out var existingSymbol))
                    throw new BasicException(ErrorCode.DuplicateDefinition, proc.Name.Position,
                        $"Il nome della procedura '{proc.OriginalName.Lexeme}' nel namespace '{nsName}' è in conflitto. Già definito come '{existingSymbol.Type}' a riga {existingSymbol.Token.Line}.");

                if (!procedureSignatures.TryGetValue(proc.Name.Lexeme, out var signatures))
                    signatures = procedureSignatures[proc.Name.Lexeme] = [];

                if (!signatures.Add(proc.Signature))
                    throw new BasicException(ErrorCode.DuplicateDefinition, proc.Name.Position,
                        $"Una procedura '{proc.OriginalName.Lexeme}' con la stessa firma è già definita nel namespace '{nsName}'.");

                allConstantsToResolve.AddRange(proc.Body.OfType<ConstStmt>().SelectMany(cs => cs.Declarations));
            }
        }

        // Esegue il constant folding per tutte le costanti raccolte
        foreach (var constDecl in program.Symbols.GetAllConstants())
        {
            var symbol = program.Symbols.GetSymbol(constDecl.SymbolId);
            var scope = program.Symbols.GetScope(symbol.ScopeId);
            var qualifiedName = GetQualifiedSymbolName(scope, symbol.OriginalToken, program);

            if (!resolvedConstants.ContainsKey(qualifiedName))
            {
                // La valutazione ora avviene all'interno del ciclo che ha tutte le informazioni necessarie
                var value = EvaluateConstantExpression(constDecl.InitialValue, program, resolvedConstants, resolving,
                    qualifiedName);
                resolvedConstants[qualifiedName] = value;
            }
        }

        // Esegue il constant folding per tutti i membri di enum raccolti
        foreach (var enumDef in program.Symbols.GetAllEnums())
        {
            long currentEnumValue = 0;
            var enumName = enumDef.OriginalName.Lexeme;
            var updatedMembers = ImmutableArray.CreateBuilder<EnumMember>();

            foreach (var member in enumDef.Members)
            {
                var qualifiedName = $"{enumName}.{member.Name.Lexeme}";
                object memberValue;

                if (member.ValueExpr != null)
                {
                    memberValue = EvaluateConstantExpression(member.ValueExpr, program, resolvedConstants, resolving,
                        qualifiedName);
                    currentEnumValue = Convert.ToInt64(memberValue);
                }
                else
                {
                    memberValue = currentEnumValue;
                }

                resolvedConstants[qualifiedName] = memberValue;

                // Aggiorna il nodo AST del membro con il valore calcolato, se non era presente
                var updatedMember = member;
                if (member.ValueExpr == null)
                {
                    var valueToken = new Token(TokenKind.Number, currentEnumValue.ToString(), (double)currentEnumValue,
                        member.Name.Position);
                    updatedMember = member with { ValueExpr = new NumberExpr((double)currentEnumValue, valueToken) };

                    // Aggiorna la dichiarazione del simbolo per coerenza
                    program.Symbols.UpdateSymbolDeclaration(member.SymbolId, updatedMember);
                }

                updatedMembers.Add(updatedMember);

                currentEnumValue++;
            }

            // Aggiorna l'EnumDefinition nella SymbolTable con i membri aggiornati
            var updatedEnumDef = enumDef with { Members = updatedMembers.ToImmutable() };
            program.Symbols.UpdateSymbolDeclaration(enumDef.SymbolId, updatedEnumDef);
        }

        program.ResolvedConstants = resolvedConstants;
    }

    /// <summary>
    /// Helper ricorsivo per la valutazione di espressioni costanti (constant folding).
    /// Gestisce le dipendenze tra costanti e rileva i riferimenti circolari.
    /// </summary>
    /// <param name="expr">L'espressione da valutare.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <param name="resolved">Cache dei valori delle costanti già risolte.</param>
    /// <param name="resolving">Set per tracciare il percorso di risoluzione corrente e rilevare cicli.</param>
    /// <param name="currentConstName">Il nome qualificato della costante attualmente in fase di valutazione.</param>
    /// <returns>Il valore letterale (object) dell'espressione.</returns>
    /// <exception cref="BasicException">Lanciata se l'espressione non è una costante valida o se viene rilevata una dipendenza circolare.</exception>
    /// <exception cref="InvalidOperationException">Lanciata per errori interni di coerenza del compilatore.</exception>
    private static object EvaluateConstantExpression(Expr expr, BasicProgram program,
        Dictionary<string, object> resolved, HashSet<string> resolving, string currentConstName)
    {
        if (!resolving.Add(currentConstName))
            throw new BasicException(ErrorCode.SyntaxError, expr.Position,
                $"Rilevata dipendenza circolare nella definizione della costante '{currentConstName}'.");

        try
        {
            return expr switch
            {
                NumberExpr n => n.Value,
                StringExpr s => s.Value,
                BooleanExpr b => b.Value,
                VariableExpr ve => ResolveConstantVariable(ve, program, resolved, resolving),
                UnaryExpr ue => EvaluateUnary(ue, program, resolved, resolving, currentConstName),
                BinaryExpr be => EvaluateBinary(be, program, resolved, resolving, currentConstName),
                GroupingExpr ge => EvaluateConstantExpression(ge.Expression, program, resolved, resolving,
                    currentConstName),
                _ => throw new BasicException(ErrorCode.SyntaxError, expr.Position,
                    "L'espressione non è una costante calcolabile a tempo di compilazione.")
            };
        }
        finally
        {
            resolving.Remove(currentConstName);
        }

        object ResolveConstantVariable(VariableExpr ve, BasicProgram prog, Dictionary<string, object> res,
            HashSet<string> resl)
        {
            var symbol = prog.Symbols.GetSymbol(ve.SymbolId);
            if (!symbol.Flags.HasFlag(SymbolFlags.IsConst) &&
                !symbol.Flags.HasFlag(SymbolFlags.IsEnumMember))
                throw new BasicException(ErrorCode.SyntaxError, ve.Position,
                    $"Il simbolo '{ve.NameSuffixed.Lexeme}' non è una costante o un membro di un enum.");

            var scope = prog.Symbols.GetScope(symbol.ScopeId);
            var qualifiedName = GetQualifiedSymbolName(scope, symbol.OriginalToken, prog);

            if (res.TryGetValue(qualifiedName, out var value))
                return value;

            // Se è un membro di un Enum, il suo valore è già stato calcolato e messo in 'res'.
            // Se arriviamo qui, significa che c'è una dipendenza circolare o un errore.
            if (symbol.Flags.HasFlag(SymbolFlags.IsEnumMember))
            {
                throw new BasicException(ErrorCode.SyntaxError, ve.Position,
                    $"Il valore del membro enum '{ve.NameSuffixed.Lexeme}' non può essere risolto. Potrebbe esserci una dipendenza circolare.");
            }

            if (symbol.DeclarationNode is not ConstDeclaration constDecl)
                throw new InvalidOperationException("Il nodo di dichiarazione della costante non è valido.");

            return EvaluateConstantExpression(constDecl.InitialValue, prog, res, resl, qualifiedName);
        }

        object EvaluateUnary(UnaryExpr ue, BasicProgram prog, Dictionary<string, object> res, HashSet<string> resl,
            string ccn)
        {
            var right = EvaluateConstantExpression(ue.Right, prog, res, resl, ccn);
            return ue.Op.Kind switch
            {
                TokenKind.Minus => -(dynamic)Convert.ToDouble(right),
                TokenKind.Not => !Convert.ToBoolean(right),
                _ => throw new BasicException(ErrorCode.SyntaxError, ue.Op.Position,
                    $"Operatore unario '{ue.Op.Lexeme}' non valido in un'espressione costante.")
            };
        }

        object EvaluateBinary(BinaryExpr be, BasicProgram prog, Dictionary<string, object> res, HashSet<string> resl,
            string ccn)
        {
            var left = EvaluateConstantExpression(be.Left, prog, res, resl, ccn);
            var right = EvaluateConstantExpression(be.Right, prog, res, resl, ccn);

            if (be.Op.Kind == TokenKind.Plus && (left is string || right is string))
                return Convert.ToString(left) + Convert.ToString(right);

            var leftNum = Convert.ToDouble(left);
            var rightNum = Convert.ToDouble(right);

            // Divisione/MOD per zero in un'espressione COSTANTE (CONST/DATA): errore a compile
            // time con ERR 11 — stessa semantica del runtime (prima `\` lanciava
            // DivideByZeroException non gestita, che diventava un crash del compilatore).
            if (rightNum == 0 && be.Op.Kind is TokenKind.Divide or TokenKind.IntegerDivide or TokenKind.Mod)
                throw new BasicException(ErrorCode.DivisionByZero, be.Op.Position, "Division by zero");

            return be.Op.Kind switch
            {
                TokenKind.Plus => leftNum + rightNum,
                TokenKind.Minus => leftNum - rightNum,
                TokenKind.Multiply => leftNum * rightNum,
                TokenKind.Divide => leftNum / rightNum,
                TokenKind.IntegerDivide => Math.Truncate(leftNum / rightNum),
                TokenKind.Mod => leftNum % rightNum,
                TokenKind.Power => Math.Pow(leftNum, rightNum),
                TokenKind.And => Convert.ToInt64(left) & Convert.ToInt64(right),
                TokenKind.Or => Convert.ToInt64(left) | Convert.ToInt64(right),
                _ => throw new BasicException(ErrorCode.SyntaxError, be.Op.Position,
                    $"Operatore '{be.Op.Lexeme}' non valido in un'espressione costante.")
            };
        }
    }

    /// <summary>
    /// Costruisce il nome qualificato completo per un simbolo, risalendo la gerarchia degli scope.
    /// È essenziale per creare chiavi univoche per la cache delle costanti risolte.
    /// </summary>
    /// <param name="currentScope">Lo scope in cui il simbolo è stato definito.</param>
    /// <param name="nameToken">Il token originale del nome del simbolo.</param>
    /// <param name="program">L'istanza del programma per accedere alla SymbolTable.</param>
    /// <returns>Una stringa che rappresenta il nome qualificato, es. "MioNamespace.MiaClasse.MiaCostante".</returns>
    private static string GetQualifiedSymbolName(SymbolTable.Scope currentScope, Token nameToken, BasicProgram program)
    {
        var name = nameToken.Lexeme;
        if (currentScope.Type == SymbolTable.ScopeType.Procedure)
        {
            var procSymbol = program.Symbols.GetSymbol(currentScope.OwnerSymbolId);
            var parentScope = program.Symbols.GetScope(procSymbol.ScopeId);
            return $"{GetQualifiedSymbolName(parentScope, procSymbol.OriginalToken, program)}.{name}";
        }

        if (currentScope.Type == SymbolTable.ScopeType.Namespace && currentScope.Name != SymbolTable.GlobalScopeName)
        {
            return $"{currentScope.Name}.{name}";
        }

        return name;
    }

    /// <summary>
    /// FASE 1.5: Applica passate di ottimizzazione (come il `LoopFlatteningOptimizer`)
    /// all'AST di ogni modulo e procedura direttamente all'interno del BasicProgram.
    /// Questo metodo modifica lo stato, non restituisce nuovi moduli.
    /// </summary>
    /// <param name="program">L'istanza di BasicProgram che contiene i moduli e la SymbolTable da ottimizzare.</param>
    public static void OptimizeAsts(BasicProgram program)
    {
        const bool log = false;
        // NOTA: Qui il costruttore del LoopFlatteningOptimizer non ha parametri nel codice fornito, 
        // quindi assumiamo la versione senza parametri.
        var optimizer = new LoopFlatteningOptimizer();

        // --- DIAGNOSTICA: DUMP PRIMA DEL LOOP ---
        Log("\n[DIAGNOSTICA OPTIMIZEASTS] --- INIZIO ITERAZIONE (Proc Candidates) ---", log);
        var mutableProcedures = program.GetAllProcedures()
            .Where(p => p is FunctionDefinition or GeneratorDefinition)
            .ToList();

        Log($"[DIAGNOSTICA OPTIMIZEASTS] Trovate {mutableProcedures.Count} procedure mutabili (UDF/GEN).", log);
        foreach (var proc in mutableProcedures)
        {
            Log(
                $"[DIAGNOSTICA OPTIMIZEASTS] ID: {proc.SymbolId}, Nome: {proc.OriginalName.Lexeme}, Tipo: {proc.GetType().Name}",
                log);
        }

        Log("----------------------------------------------------------------", log);

        // 1. Fase 1 (Haxe-CrossPlatform-Proposals): IR Lowering → Constant Folding → DCE.
        // Lowering: normalizza i costrutti complessi (FOR senza STEP → STEP 1, IF annidati)
        // in forma canonica PRIMA di folding/flatten — un input unificato e pre-ottimizzato
        // per il loop flatten, il P-Code VM e (in futuro) il backend C99.
        foreach (var proc in mutableProcedures)
        {
            var loweredBody = proc.Body.Select(AstLowerer.LowerStatement).ToImmutableArray();
            UpdateProcedureBodyInSymbolTable(program.Symbols, proc, loweredBody);
        }

        // Constant Folding: valuta a compile-time le espressioni con operandi costanti
        // letterali (1 + 2 → 3, "A" + "B" → "AB", TRUE AND TRUE → TRUE).
        foreach (var proc in mutableProcedures)
        {
            var foldedBody = proc.Body.Select(ConstantFoldingOptimizer.FoldStatement).ToImmutableArray();
            UpdateProcedureBodyInSymbolTable(program.Symbols, proc, foldedBody);
        }

        // Dead Code Elimination: elimina SUB/FUNCTION/TYPE mai raggiungibili dagli entry
        // point (Main + top-level + Public) — sorgenti più compatte per qualunque target.
        DeadCodeEliminator.EliminateDeadCode(program);

        // 2. Ottimizza il corpo di tutte le procedure del programma.
        // Itera sulla lista filtrata:
        foreach (var proc in mutableProcedures)
        {
            var optimizedBody = optimizer.FlattenLoopPass(proc.Body).ToImmutableArray();
            UpdateProcedureBodyInSymbolTable(program.Symbols, proc, optimizedBody);
        }

        // // --- DIAGNOSTICA: DUMP DOPO IL LOOP ---
        // Log("\n[DIAGNOSTICA OPTIMIZEASTS] --- DOPO APPIATTIMENTO (Verifica Coerenza) ---", true);
        //
        // Log($"[DIAGNOSTICA OPTIMIZEASTS] Conteggio finale simboli: {program.Symbols.Count}", true);
        //
        // // Stampa tutti i simboli (nome e ID)
        // var allSymbols = program.Symbols.GetAllSymbols().OrderBy(s => s.Id);
        // foreach (var symbol in allSymbols)
        // {
        // //    Log(
        // //        $"[DIAGNOSTICA OPTIMIZEASTS] ID: {symbol.Id}, Nome: {symbol.OriginalName}, Tipo: {symbol.DeclarationNode.GetType().Name}",
        // //        true);
        // }
        //
        // Log("----------------------------------------------------------------", true);

        // 2. Itera su ogni modulo per ottimizzare gli statements e ricalcolare le label.
        foreach (var (modulePath, originalModule) in
                 program.LoadedModules.ToList()) // Usa ToList per modificare la collezione durante l'iterazione
        {
            // Ottimizza le istruzioni a livello di modulo.

            // Chiama il nuovo metodo ottimizzato che opera sul Dictionary
            var optimizedStatements = optimizer.FlattenStatementsByLine(originalModule.Statements);

            // Recupera le procedure (ora ottimizzate) che appartengono a questo modulo.
            var moduleProcedures = program.GetProceduresForModule(originalModule).ToList();

            // Passa il 'modulePath' a RebuildLabelMap.
            RebuildLabelMap(modulePath, optimizedStatements, moduleProcedures, program);

            // Sostituisce il vecchio modulo con la sua versione completamente ottimizzata.
            var optimizedModule = new BasicModule(
                originalModule.FilePath,
                originalModule.IsStrict,
                originalModule.SourceMap,
                optimizedStatements,
                originalModule.Imports,
                originalModule.Declarations,
                originalModule.LibraryName
            );
            // FlattenStatementsByLine può eliminare/fondere righe (es. fusione di loop
            // annidati) — riusare ExecutionOrder invariato lascerebbe numeri di riga non
            // più presenti come chiave in optimizedStatements, causando un
            // KeyNotFoundException nel backend IL (EmitInitializeDataPool e il
            // dispatcher ON ERROR indicizzano entrambi Statements per ogni riga in
            // ExecutionOrder — vedi IL-OnError-Loop-Bug.md). Filtra invece di
            // riordinare: preserva l'ordine di programma originale (che per righe
            // sintetiche NON è un semplice sort numerico, vedi il commento su
            // BasicModule.ExecutionOrder), rimuove solo le chiavi che non esistono più.
            optimizedModule.ExecutionOrder = [.. originalModule.ExecutionOrder.Where(optimizedStatements.ContainsKey)];
            program.LoadedModules[modulePath] = optimizedModule;
        }
    }

    /// <summary>
    /// Metodo helper per aggiornare il corpo di una procedura all'interno della SymbolTable.
    /// </summary>
    public static void UpdateProcedureBodyInSymbolTable(SymbolTable symbols, ProcedureDefinition originalProc,
        ImmutableArray<Stmt> newBody)
    {
        var symbolId = originalProc.SymbolId;
        if (symbolId < 0 || symbolId >= symbols.Count) return;

        // Crea una nuova istanza della definizione della procedura con il corpo aggiornato.
        ProcedureDefinition updatedProc = originalProc switch
        {
            FunctionDefinition fd => fd with { Body = newBody },
            GeneratorDefinition gd => gd with { Body = newBody },
            // AGGIUNTO: Se è un altro tipo (Native, External) non deve aggiornare il corpo.
            NativeProcedureDefinition npd => npd,
            ExternalProcedureDefinition epd => epd,
            _ => throw new InvalidOperationException(
                "Solo il corpo di FunctionDefinition e GeneratorDefinition può essere aggiornato.")
        };
        // Aggiorna il nodo di dichiarazione del simbolo.
        symbols.UpdateSymbolDeclaration(symbolId, updatedProc);
    }

    /// <summary>
    /// Ricostruisce le posizioni delle etichette (sia testuali che numeriche) direttamente nella SymbolTable
    /// dopo che un AST è stato modificato dalle passate di ottimizzazione.
    /// Questo metodo non restituisce più una mappa, ma aggiorna la fonte di verità.
    /// </summary>
    private static void RebuildLabelMap(
        string modulePath,
        Dictionary<int, List<Stmt>> statementsByLine,
        List<ProcedureDefinition> procedures,
        BasicProgram program)
    {
        const bool log = false;
        Log("[RebuildLabelMap] Avvio aggiornamento posizioni etichette nella SymbolTable...", log);

        // 1. Cerca, aggiorna e REGISTRA le etichette nel corpo principale (codice numerato).
        foreach (var (lineNumber, statements) in statementsByLine)
        {
            // --- MODIFICA CRITICA: Registra il numero di riga come etichetta ---
            if (lineNumber > 0)
            {
                var key = SymbolTable.GetQualifiedLabelName(modulePath, null, lineNumber.ToString());
                // La posizione di un'etichetta numerica è sempre l'inizio della sua riga logica (indice 0).
                var newLocation = new LabelLocation(null, lineNumber, 0);
                program.Symbols.AddOrUpdateLabel(key, newLocation);
                Log($"[RebuildLabelMap]   - Registrata etichetta numerica '{key}'.", log);
            }
            // --- FINE MODIFICA ---

            FindAndUpdateLabelsRecursively(statements, null, lineNumber, null);
        }

        // 2. Cerca e aggiorna le etichette all'interno di ogni procedura.
        foreach (var procedure in procedures)
        {
            FindAndUpdateLabelsRecursively(procedure.Body, procedure, -1, null); // lineNumber è -1 per le procedure
        }

        Log("[RebuildLabelMap] Aggiornamento posizioni etichette completato.", log);
        return;

        // Funzione helper ricorsiva per visitare i nodi Stmt e aggiornare la SymbolTable.
        // 'blockOwner' e' l'array di statement del blocco annidato (IfClause/CaseClause/...) a cui
        // 'statements' appartiene, se presente: usato per etichettare le label trovate a QUESTO
        // livello come "block-scoped" (vedi LabelLocation.BlockStatements / CallFrame.LocalBlockBody),
        // cosi' i salti (GOTO/loop) verso di esse possono essere risolti correttamente anche quando
        // il blocco e' eseguito con un frame locale invece del Body/IP globale della procedura.
        void FindAndUpdateLabelsRecursively(IEnumerable<Stmt> statements, ProcedureDefinition? proc, int lineNumber,
            ImmutableArray<Stmt>? blockOwner)
        {
            var currentIndex = 0;
            foreach (var stmt in statements)
            {
                if (stmt is LabelStmt labelStmt)
                {
                    // Crea la chiave qualificata e la nuova, corretta LabelLocation.
                    var key = SymbolTable.GetQualifiedLabelName(modulePath, proc, labelStmt.Label.Lexeme);
                    var newLocation = new LabelLocation(proc, lineNumber, currentIndex, blockOwner);

                    // Aggiorna o aggiunge l'etichetta nella mappa piatta.
                    program.Symbols.AddOrUpdateLabel(key, newLocation);
                    Log(
                        $"[RebuildLabelMap]   - Registrata/Aggiornata etichetta testuale '{key}' alla nuova posizione (Index: {currentIndex}).",
                        log);
                }

                // Scendi ricorsivamente nei blocchi.
                switch (stmt)
                {
                    case IfStmt ifStmt:
                        foreach (var clause in ifStmt.IfClauses)
                            FindAndUpdateLabelsRecursively(clause.Statements, proc, lineNumber, clause.Statements);
                        if (ifStmt.ElseClause != null)
                            FindAndUpdateLabelsRecursively(ifStmt.ElseClause.Statements, proc, lineNumber,
                                ifStmt.ElseClause.Statements);
                        break;
                    case SelectStmt selectStmt:
                        foreach (var clause in selectStmt.CaseClauses)
                            FindAndUpdateLabelsRecursively(clause.Statements, proc, lineNumber, clause.Statements);
                        if (selectStmt.ElseClause != null)
                            FindAndUpdateLabelsRecursively(selectStmt.ElseClause.Statements, proc, lineNumber,
                                selectStmt.ElseClause.Statements);
                        break;
                    case TryCatchStmt tryStmt:
                        FindAndUpdateLabelsRecursively(tryStmt.TryBody, proc, lineNumber, tryStmt.TryBody);
                        foreach (var clause in tryStmt.CatchClauses)
                            FindAndUpdateLabelsRecursively(clause.Body, proc, lineNumber, clause.Body);
                        if (tryStmt.Finally != null)
                            FindAndUpdateLabelsRecursively(tryStmt.Finally.Body, proc, lineNumber,
                                tryStmt.Finally.Body);
                        break;
                    case UsingStmt usingStmt:
                        FindAndUpdateLabelsRecursively(usingStmt.Body, proc, lineNumber, usingStmt.Body);
                        break;
                    case WithStmt withStmt:
                        FindAndUpdateLabelsRecursively(withStmt.Body, proc, lineNumber, withStmt.Body);
                        break;
                }

                currentIndex++;
            }
        }
    }

    /// <summary>
    /// FASE 3.4: Esegue la riscrittura dell'AST per tutti i moduli del programma.
    /// VERSIONE DI DEBUG ESTREMO: Logga ogni passaggio per tracciare il flusso di dati
    /// tra l'AST di FASE 1 e la SymbolTable di FASE 2.
    /// CORRETTA: Utilizza i SymbolId esistenti per recuperare i simboli, gestendo i tipi concreti.
    /// </summary>
    private static bool _semFoldSecondPass;

    public static void ResolveAndRewriteModuleAsts(BasicProgram program)
    {
        // §4.10(f): il folder semantico (AI.SYMBOL/IMPACT/CALLMAP) deve vedere il programma LINKATO
        // completo (es. il corpo di Main). Eseguiamo il rewrite UNA volta senza folder, poi una
        // seconda volta col folder: solo allora le call graph e' completa.
        if (SemanticQueryFolder is not null && !_semFoldSecondPass)
        {
            var folder = SemanticQueryFolder;
            SemanticQueryFolder = null;
            ResolveAndRewriteModuleAsts(program);        // 1a passata: link completo, nessun fold
            SemanticQueryFolder = folder;
            _semFoldSecondPass = true;
            try { ResolveAndRewriteModuleAsts(program); }  // 2a passata: fold col programma completo
            finally { _semFoldSecondPass = false; }
            return;
        }

        const bool log = false; // Log forzato per il debug
        Log("\n[REWRITER] === INIZIO FASE 3.4: RISCRITTURA AST GLOBALE ===", log);

        foreach (var modulePath in program.LoadedModules.Keys.ToList())
        {
            var originalModule = program.LoadedModules[modulePath];
            Log($"[REWRITER] --- Processo il modulo: {Path.GetFileName(modulePath)} ---", log);

            var isInClipperMode = originalModule.Imports.Any(i =>
                i.NamespaceName.Lexeme.Equals("DBF", StringComparison.OrdinalIgnoreCase));

            if (isInClipperMode)
            {
                Log($"[REWRITER]   -> Rilevato 'IMPORT DBF'. Attivazione Modalità Clipper per questo modulo.",
                    log);
            }

            var rewrittenDeclarations = new List<AstNode>();
            program.Symbols.ResetScope();

            var currentNamespaceName = SymbolTable.GlobalScopeName;

            Log($"[REWRITER]   Itero sui simboli di alto livello del modulo...", log);

            var fileId = program.GetOrAddFileId(modulePath);
            var topLevelSymbolsInModule = program.Symbols.GetSymbolsByFileId(fileId)
                .Where(s =>
                {
                    var scope = program.Symbols.GetScope(s.ScopeId);
                    // Filtra per simboli definiti direttamente in un namespace o nello scope globale.
                    // AGGIUNTA CRUCIALE: Escludi i membri degli enum da questa lista di alto livello.
                    return !s.Flags.HasFlag(SymbolFlags.IsEnumMember) &&
                           (scope.Type is SymbolTable.ScopeType.Namespace or SymbolTable.ScopeType.Global);
                })
                .ToList();

            Log($"[REWRITER]   Trovati {topLevelSymbolsInModule.Count} simboli di alto livello per il modulo.", log);

            // Il contesto a livello di modulo viene creato una sola volta
            // e mantenuto per l'intera durata della riscrittura del modulo, propagato sia
            // alle procedure che agli statement a livello di modulo.
            var moduleContext = new AstRewriteContext(modulePath, null, new DbfContext(), isInClipperMode,
                originalModule.IsStrict, 0);

            foreach (var symbol in topLevelSymbolsInModule)
            {
                Log($"[REWRITER]     - Processo simbolo: {symbol}", log);
                var authoritativeDeclNode = symbol.DeclarationNode;
                Log($"[REWRITER]       -> Nodo AST autorevole recuperato dalla SymbolTable: {authoritativeDeclNode}",
                    log);

                var symbolNamespace = program.Symbols.GetNamespaceNameForSymbol(symbol.Id);
                if (symbolNamespace != currentNamespaceName)
                {
                    Log($"[REWRITER]         -> CAMBIO NAMESPACE: da '{currentNamespaceName}' a '{symbolNamespace}'",
                        log);
                    if (currentNamespaceName != SymbolTable.GlobalScopeName) program.Symbols.ExitScope();
                    if (symbolNamespace != SymbolTable.GlobalScopeName)
                        program.Symbols.EnterScope(symbolNamespace, SymbolTable.ScopeType.Namespace);
                    currentNamespaceName = symbolNamespace;
                }

                var rewrittenNode = authoritativeDeclNode;

                if (authoritativeDeclNode is ProcedureDefinition authoritativeProc)
                {
                    // Imposta la classe proprietaria nel contesto (se esiste)
                    var parentClass = authoritativeProc.Parent as ClassDefinition;
                    var procContext = moduleContext with { CurrentClass = parentClass };
                    
                    program.Symbols.EnterScope(authoritativeProc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure, authoritativeProc.SymbolId);
                    try {
                        rewrittenNode = RewriteProcedure(authoritativeProc, procContext, program);
                        // Aggiorna la SymbolTable con la procedura riscritta.
                        // Senza questo, GetProceduresForNamespace() restituisce la procedura
                        // ORIGINALE (pre-rewrite) e l'Executor esegue l'AST sbagliato.
                        program.Symbols.UpdateSymbolDeclaration(authoritativeProc.SymbolId, rewrittenNode);
                    } finally {
                        program.Symbols.ExitScope();
                    }
                }
                else if (authoritativeDeclNode is ClassDefinition authoritativeClass)
                {
                    // Passa la classe stessa come contesto per i suoi membri
                    var classContext = moduleContext with { CurrentClass = authoritativeClass };
                    rewrittenNode = RewriteClassDefinition(authoritativeClass, classContext, program);
                }
                else if (authoritativeDeclNode is EnumDefinition authoritativeEnum)
                {
                    Log($"[REWRITER]         -> Riscrivo Enum '{authoritativeEnum.OriginalName.Lexeme}'...", log);
                    rewrittenNode = RewriteEnumDefinition(authoritativeEnum, moduleContext, program);
                    Log($"[REWRITER]         -> Enum riscritto. Nodo finale: {rewrittenNode}", log);
                }
                else if (authoritativeDeclNode is UserDefinedType authoritativeUdt)
                {
                    // BUG: i TYPE dichiarati a livello di modulo non passano MAI da qui per
                    // Fields/Methods/Constructors/Properties (funzionano comunque: Methods vengono
                    // interamente risolti in ResolveDeclaration/case ProcedureDefinition, Constructors
                    // via ResolveStatementList dedicato — vedi Linker.Resolver.cs case UserDefinedType).
                    // Gli OPERATORS pero' hanno BISOGNO anche del pass di REWRITE (non solo Resolve):
                    // RewriteNewExpr e' cio' che aggiorna NewExpr.ParsedType dal riferimento di tipo
                    // "grezzo" (pre-binding) al UserDefinedType canonico con Constructors[].Parameters[]
                    // gia' risolti dal Binder. Senza questo, `NEW Vector2D(...)` dentro un OPERATOR usa
                    // un ParsedType stantio i cui parametri costruttore hanno ancora SymbolId=-1,
                    // causando BindArgumentsFromValues a scrivere sotto chiavi sintetiche
                    // (SELF_SYMBOL_ID - i - 1) mai lette dal corpo del costruttore (che referenzia
                    // ancora il SymbolId reale assegnato dal Binder) — il valore letto ricade quindi
                    // sullo scope padre, tipicamente un parametro dell'operatore (es. l'istanza UdtInstance
                    // 'a'), producendo errori come "Cannot convert value 'UdtInstance {...}' to type 'DOUBLE'".
                    // Fix MINIMO e mirato: riscrivi SOLO gli Operators (non Fields/Methods/Constructors/
                    // Properties, che già funzionano — ririscriverli qui li ROMPE, verificato empiricamente).
                    if (authoritativeUdt.Operators.Length > 0 || authoritativeUdt.Constructors.Length > 0 || authoritativeUdt.Methods.Length > 0)
                    {
                        var udtCtx = moduleContext with
                        {
                            CurrentUdtScopeId = authoritativeUdt.SymbolId >= 0
                                ? authoritativeUdt.SymbolId
                                : moduleContext.CurrentUdtScopeId
                        };
                        var rewrittenOperators = authoritativeUdt.Operators.Select(op =>
                        {
                            var (rewrittenBody, _) = RewriteStatementList(op.Body, udtCtx, null, program);
                            return op with { Body = rewrittenBody };
                        }).ToImmutableArray();
                        // ESPERIMENTO (2026-07-20, su decisione dell'utente): estendi il
                        // rewrite anche a Costruttori e Metodi, per risolvere SELF (mai
                        // risolto altrimenti — serve al backend IL). Il commento sopra
                        // avvisa che "ririscriverli ROMPE, verificato empiricamente" —
                        // questo esperimento serve a scoprire COSA esattamente si rompe.
                        var rewrittenUdtConstructors = authoritativeUdt.Constructors.Select(ctor =>
                        {
                            var (rewrittenBody, _) = RewriteStatementList(ctor.Body, udtCtx, null, program);
                            return ctor with { Body = rewrittenBody };
                        }).ToImmutableArray();
                        var rewrittenUdtMethods = authoritativeUdt.Methods.Select(m =>
                        {
                            if (m is not FunctionDefinition fd) return m;
                            var (rewrittenBody, _) = RewriteStatementList(fd.Body, udtCtx, null, program);
                            return (ProcedureDefinition)(fd with { Body = rewrittenBody });
                        }).ToImmutableArray();
                        var rewrittenUdt = authoritativeUdt with
                        {
                            Operators = rewrittenOperators,
                            Constructors = rewrittenUdtConstructors,
                            Methods = rewrittenUdtMethods
                        };
                        if (authoritativeUdt.SymbolId >= 0)
                            program.Symbols.UpdateSymbolDeclaration(authoritativeUdt.SymbolId, rewrittenUdt);
                        rewrittenNode = rewrittenUdt;
                    }
                }
                else if (authoritativeDeclNode is VariableDeclaration varDecl)
                {
                    Log($"[REWRITER]         -> Trovato VariableDeclaration nudo. Lo avvolgo in un DeclarationStmt per la riscrittura.", log);
                    // Crea un DeclarationStmt temporaneo per la coerenza.
                    var tempDeclStmt = new DeclarationStmt(
                        new Token(TokenKind.Dim, "DIM", null, varDecl.Position),
                        [varDecl]
                    ) { ScopeId = varDecl.ScopeId }; // Propaga lo ScopeId dal nodo figlio

                    // Riscrivi lo statement completo.
                    (rewrittenNode, _) = RewriteStatement(tempDeclStmt, moduleContext, null, program);
                    Log($"[REWR-DEBUG]         -> DeclarationStmt (da VarDecl nudo) riscritto. Nodo finale: {rewrittenNode}", log);
                }

                rewrittenDeclarations.Add(rewrittenNode);
            }

            if (currentNamespaceName != SymbolTable.GlobalScopeName)
            {
                program.Symbols.ExitScope();
            }

            moduleContext = moduleContext with { 
                CurrentProcedure = null, 
                CurrentClass = null, 
                CurrentUdtScopeId = null, 
                SymbolMap = null 
            };
            
            var rewrittenStatements = new Dictionary<int, List<Stmt>>();

            Log($"[REWRITER-CTX] Inizio riscrittura modulo. Alias: '{moduleContext.DbfState.CurrentAlias}'", log);

            foreach (var kvp in originalModule.Statements.OrderBy(kvp => kvp.Key))
            {
                var lineStatements = new List<Stmt>();
                foreach (var stmt in kvp.Value)
                {
                    // Propaga il contesto (DbfState/Alias) tra gli statement
                    (var rewrittenStmt, moduleContext) = RewriteStatement(stmt, moduleContext, null, program);
                    lineStatements.Add(rewrittenStmt);
                }
                rewrittenStatements[kvp.Key] = lineStatements;
            }

            var rewrittenModule = new BasicModule(
                originalModule.FilePath,
                originalModule.IsStrict,
                originalModule.SourceMap,
                rewrittenStatements,
                originalModule.Imports,
                [..rewrittenDeclarations],
                originalModule.LibraryName
            );
            rewrittenModule.ExecutionOrder = originalModule.ExecutionOrder;

            program.LoadedModules[modulePath] = rewrittenModule;
            Log($"[REWRITER] --- Modulo '{Path.GetFileName(modulePath)}' completamente riscritto. ---", log);
        }

        Log("[REWRITER] === FINE FASE 3.4 ===", log);
    }

    /// <summary>
    /// Passata 3.4b: ricostruisce nuovamente la mappa delle etichette dopo la riscrittura finale
    /// dell'AST (<see cref="ResolveAndRewriteModuleAsts"/>).
    /// </summary>
    /// <remarks>
    /// Necessaria perché <see cref="RebuildLabelMap"/> viene già invocata in FASE 3.2
    /// (<see cref="OptimizeAsts"/>), ma su un AST che FASE 3.4 sostituisce con nuove istanze
    /// (ogni nodo riscritto è una nuova istanza immutabile, anche quando il suo contenuto non
    /// cambia). Gli indici posizionali (<c>StatementIndex</c>) restano validi attraverso la
    /// riscrittura, perché questa preserva l'ordine/conteggio delle statement — ma i riferimenti
    /// diretti come <see cref="LabelLocation.BlockStatements"/> (confrontati per identità di
    /// riferimento a runtime, non per struttura) diventerebbero stantii senza questa ripetizione,
    /// puntando ad array di statement ormai scartati invece che a quelli effettivamente eseguiti.
    /// </remarks>
    public static void RebuildBlockScopedLabelReferences(BasicProgram program)
    {
        foreach (var modulePath in program.LoadedModules.Keys.ToList())
        {
            var module = program.LoadedModules[modulePath];
            var moduleProcedures = program.GetProceduresForModule(module).ToList();
            RebuildLabelMap(modulePath, module.Statements, moduleProcedures, program);
        }
    }

    /// <summary>
    /// Crea e lancia un'eccezione di parsing formattata (`BasicException`), arricchita con
    /// informazioni contestuali come il token, la riga, la colonna e il testo della riga di codice sorgente.
    /// </summary>
    private static BasicException CreateError(Token token, string message,
        ErrorCode code = ErrorCode.SyntaxError) => new(token, message, code);

    /// <summary>
    /// Crea e lancia un'eccezione di parsing formattata (`BasicException`), arricchita con
    /// informazioni contestuali come l'espressione, la riga, la colonna e il testo della riga di codice sorgente.
    /// </summary>
    private static BasicException CreateError(Expr expression, string message,
        ErrorCode code = ErrorCode.SyntaxError) => new(expression, message, code);

    /// <summary>
    /// Metodo helper per il logging condizionale durante il processo di linking.
    /// </summary>
    /// <summary>
    /// Flag per logging verboso del linker, impostato dall'Executor durante l'avvio.
    /// </summary>
    internal static bool VerboseLinker { get; set; }

    /// <summary>
    /// Metodo helper per il logging condizionale durante il processo di linking.
    /// </summary>
    private static void Log(string message, bool force = false)
    {
        if (VerboseLinker || force)
            Console.WriteLine(message);
    }
    
}