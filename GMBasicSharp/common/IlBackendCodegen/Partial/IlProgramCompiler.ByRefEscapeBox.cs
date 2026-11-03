// FILE: IlBackendCodegen/Partial/IlProgramCompiler.ByRefEscapeBox.cs
// PARTIAL — Fase 4 del piano Closure-ByRef-Box-Linker-Definitivo (vault:
// GMBasic/Todo/Closure-ByRef-Box-Linker-Definitivo-TODO.md §FASE 4). Un parametro BYREF
// catturato da una closure locale che sfugge (RETURN di una FUNCTION/delegate) richiede che
// la variabile del CHIAMANTE sia rappresentata come ByRefBox<T> (reference type, sopravvive
// al ritorno del metodo) invece che come T&/local CLR normale — un T& non può MAI essere
// salvato in un campo (vincolo del CLR), e un local CLR normale non sopravvive al ritorno del
// metodo che lo dichiara. Questo file contiene: (1) il pre-scan whole-program che determina
// quali parametri BYREF di quali procedure sono catturati da una loro closure annidata, (2) il
// pre-scan LOCALE (per-istanza) che determina quali variabili del chiamante corrente vanno
// rappresentate come box perché passate BYREF in una di quelle posizioni.

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    /// <summary>
    /// Cache per-programma (weak: non impedisce la GC di BasicProgram tra compilazioni
    /// diverse in un processo long-lived come REPL/BSP) della mappa "per la procedura X, quali
    /// POSIZIONI di parametro BYREF sono catturate da una sua closure locale annidata".
    /// Calcolata UNA VOLTA per programma, non per singola istanza di IlProgramCompiler (che
    /// viene creata una per corpo/metodo/costruttore compilato).
    /// </summary>
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<BasicProgram, Dictionary<int, HashSet<int>>>
        _byRefCaptureMapCache = new();

    /// <summary>
    /// Tipo CLR di un parametro, box-aware: per un parametro BYREF in una posizione catturata
    /// da una closure locale DELLA STESSA procedura (SymbolId <paramref name="procSymbolId"/>),
    /// <c>ByRefBox&lt;T&gt;</c> (BYVAL — il box stesso è il meccanismo di condivisione, mai più
    /// un vero <c>T&amp;</c> CLR, che non potrebbe comunque sopravvivere nel campo della
    /// closure). Altrimenti il comportamento di sempre (<c>T&amp;</c> per BYREF, <c>T</c> per
    /// BYVAL) — no-op per la stragrande maggioranza dei parametri.
    /// </summary>
    private static Type MapParamClrTypeConsideringBox(Parameter p, int position, int procSymbolId,
        BasicProgram program, IlTypeContext types, IReadOnlyDictionary<int, TypeBuilder> classTypes)
    {
        var mapped = MapProcedureType(p.DataType, types, classTypes);
        if (!p.Flags.HasFlag(SymbolFlags.IsByRef)) return mapped;
        if (GetByRefCapturedParamPositions(program).TryGetValue(procSymbolId, out var positions) && positions.Contains(position))
            return typeof(ByRefBox<>).MakeGenericType(mapped);
        return mapped.MakeByRefType();
    }

    /// <summary>
    /// Determina quali posizioni di parametro BYREF di quali procedure sono "catturate da una
    /// closure locale che sfugge". IMPORTANTE (scoperto empiricamente, 2026-08-31): NON si può
    /// rilevare cercando <c>LocalProcedureStmt</c> nel corpo via
    /// <c>program.Symbols.GetAllProcedures()</c> — quel corpo è la versione POST-REWRITE del
    /// Linker, che ha GIÀ rimosso ogni <c>LocalProcedureStmt</c> promossa (sostituita da un
    /// <c>MethodMember</c> sulla <c>__ClosureCtx</c> — vedi <c>PerformHeapPromotion</c>): un
    /// pre-scan basato su quel marker trova SEMPRE zero risultati (bug reale, verificato dal
    /// vivo — vedi vault, Fase 4.1). Il segnale AFFIDABILE post-rewrite è invece l'ESISTENZA
    /// della classe <c>__ClosureCtx_&lt;nome&gt;_&lt;SymbolId&gt;</c> stessa (il Linker la crea
    /// SEMPRE quando promuove qualcosa) con un CAMPO il cui nome coincide con un parametro
    /// BYREF della procedura proprietaria (estratta dal SUFFISSO numerico del nome classe,
    /// stessa convenzione con cui il Linker la genera).
    /// </summary>
    private static Dictionary<int, HashSet<int>> GetByRefCapturedParamPositions(BasicProgram program)
    {
        if (_byRefCaptureMapCache.TryGetValue(program, out var cached)) return cached;
        var map = new Dictionary<int, HashSet<int>>();
        var allProcsById = program.Symbols.GetAllProcedures().OfType<FunctionDefinition>()
            .GroupBy(fd => fd.SymbolId).ToDictionary(g => g.Key, g => g.First());
        // Le classi __ClosureCtx_<nome>_<id> (heap promotion del Linker) sono registrate SOLO
        // nella SymbolTable, MAI in LoadedModules[x].Declarations (stessa scoperta già fatta da
        // Host.Compile.cs per PreScanRedefinedTypes — vedi il commento gemello lì): vanno
        // trovate camminando i simboli per indice, non con AllModuleDeclarations.
        var closureCtxClasses = new List<ClassDefinition>();
        for (var ci = 0; ci < program.Symbols.Count; ci++)
        {
            var sym = program.Symbols.GetSymbol(ci);
            if (sym.DeclarationNode is ClassDefinition cd && sym.OriginalName.StartsWith("__ClosureCtx", StringComparison.Ordinal))
                closureCtxClasses.Add(cd);
        }
        foreach (var ctxClass in closureCtxClasses)
        {
            var name = ctxClass.OriginalName.Lexeme;
            var lastUnderscore = name.LastIndexOf('_');
            int ownerProcSymbolId = -1;
            var parsedOk = lastUnderscore >= 0 && int.TryParse(name[(lastUnderscore + 1)..], out ownerProcSymbolId);
            if (!parsedOk) continue;
            if (!allProcsById.TryGetValue(ownerProcSymbolId, out var ownerProc)) continue;
            foreach (var field in ctxClass.Members.OfType<FieldMember>())
            {
                var fieldName = field.Declaration.NameCleaned.Lexeme;
                for (var i = 0; i < ownerProc.Parameters.Length; i++)
                {
                    var p = ownerProc.Parameters[i];
                    if (!p.Flags.HasFlag(SymbolFlags.IsByRef)
                        || !p.OriginalName.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase)) continue;
                    if (!map.TryGetValue(ownerProcSymbolId, out var positions))
                        map[ownerProcSymbolId] = positions = new HashSet<int>();
                    positions.Add(i);
                }
            }
        }
        _byRefCaptureMapCache.Add(program, map);
        return map;
    }

    /// <summary>
    /// Cache dei NOMI (case-insensitive) di parametri BYREF catturati da una closure locale
    /// annidata, su TUTTO il programma — usata SOLO per decidere il tipo CLR di un campo
    /// __ClosureCtx (Host.Compile.cs, costruzione dei FieldMember): a quel punto il Linker ha
    /// già creato un campo con lo STESSO nome del simbolo originale ma un SymbolId NUOVO (mai
    /// tracciato all'indietro fino al parametro/posizione originale) — usare il nome è una
    /// semplificazione deliberata (un'omonimia fra un campo BYVAL di una procedura ignota e un
    /// parametro BYREF catturato di un'altra renderebbe quel campo box-typed inutilmente, MAI
    /// scorrettamente: la lettura/scrittura è instradata dal TIPO CLR effettivo del campo,
    /// mai da un flag a parte — vedi IsByRefBoxFieldType). Accettato per contenere lo sforzo
    /// della Fase 4 al caso base (FunctionsAdvanced.bas), non un'implementazione a prova di
    /// ogni collisione di nomi immaginabile.
    /// </summary>
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<BasicProgram, HashSet<string>>
        _byRefCaptureNameCache = new();

    private static HashSet<string> GetByRefCapturedParamNames(BasicProgram program)
    {
        if (_byRefCaptureNameCache.TryGetValue(program, out var cached)) return cached;
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var proc in program.Symbols.GetAllProcedures())
        {
            if (proc is not FunctionDefinition fd || !GetByRefCapturedParamPositions(program).TryGetValue(fd.SymbolId, out var positions))
                continue;
            foreach (var pos in positions)
                if (pos < fd.Parameters.Length) names.Add(fd.Parameters[pos].OriginalName.Lexeme);
        }
        _byRefCaptureNameCache.Add(program, names);
        return names;
    }


    /// <summary>
    /// Spinge sullo stack di attraversamento ogni AstNode di una collezione — a parte, perché
    /// una <c>ImmutableArray&lt;T&gt;</c> DEFAULT (mai inizializzata, es. <c>Body</c> di una
    /// dichiarazione solo-firma, o <c>ElseClause</c>/liste opzionali mai popolate) implementa
    /// IEnumerable ma lancia InvalidOperationException al primo GetEnumerator() — controllo
    /// per riflessione su IsDefault (se la proprietà esiste, è una ImmutableArray) prima di
    /// iterare, altrimenti l'attraversamento generico crasherebbe su QUALUNQUE albero reale
    /// (quasi ogni Stmt/Expr ha almeno una collezione opzionale mai popolata).
    /// </summary>
    private static void PushEnumerableChildren(System.Collections.IEnumerable seq, Stack<AstNode> stack)
    {
        var isDefaultProp = seq.GetType().GetProperty("IsDefault");
        if (isDefaultProp != null && isDefaultProp.GetValue(seq) is true) return;
        foreach (var item in seq)
        {
            switch (item)
            {
                case AstNode itemNode:
                    stack.Push(itemNode);
                    break;
                // Assignment (record non-AstNode, es. LetStmt.Assignments): il VALORE è
                // un Expr (AstNode) e va attraversato — senza questo, una call dentro
                // un'assegnazione (`MioIncrementatore = CreaIncrementatore(contatoreGlobale)`)
                // non veniva MAI vista dal pre-scan di BoxedSymbols (riga del bug: la call a
                // CreaIncrementatore in un LetStmt non era riconosciuta come FunctionExpr,
                // quindi contatoreGlobale non veniva marcato box-typed nel chiamante).
                case Assignment assignment:
                    if (assignment.Value is AstNode valNode) stack.Push(valNode);
                    break;
            }
        }
    }

    /// <summary>
    /// Insieme (lazy, calcolato una sola volta per istanza) dei SymbolId di parametro/locale
    /// DI QUESTA PROCEDURA che devono essere rappresentati come ByRefBox&lt;T&gt; invece che
    /// col tipo nudo — perché passati BYREF in una CALL verso una procedura che li cattura in
    /// una propria closure locale (vedi <see cref="GetByRefCapturedParamPositions"/>). Vuoto
    /// (mai null) per la stragrande maggioranza delle procedure, che non passano mai un
    /// argomento in una posizione "catturata" — in quel caso questo intero meccanismo è un
    /// no-op completo, stesso codegen di oggi.
    /// </summary>
    private HashSet<int>? _boxedSymbolsCache;
    private HashSet<int> BoxedSymbols
    {
        get
        {
            if (_boxedSymbolsCache != null) return _boxedSymbolsCache;
            var result = new HashSet<int>();
            var body = GetCurrentProcedureBodyForBoxScan();
            if (body != null && !body.Value.IsDefault)
            {
            var map = GetByRefCapturedParamPositions(_program);
            foreach (var callExpr in FindAllCallExpressions(body.Value))
            {
                if (callExpr.SymbolId < 0 || !map.TryGetValue(callExpr.SymbolId, out var positions)) continue;
                for (var i = 0; i < callExpr.Args.Length && i < 64; i++)
                {
                    if (!positions.Contains(i)) continue;
                    if (callExpr.Args[i] is VariableExpr { SymbolId: >= 0 } argVe) result.Add(argVe.SymbolId);
                }
            }
            // Catture MUTATE da closure inline (bug closure_byref): una variabile LOCALE
            // scritta dentro una lambda/SUB del corpo va rappresentata come ByRefBox<T>
            // (reference condiviso), altrimenti la mutazione resta nel campo-snapshot della
            // closure e non si propaga al chiamante. Coerente con DiscoverAndCompileLambdas
            // (campo closure ByRefBox<T>) e EmitLambdaCreation (passa il box). Escluse le
            // variabili globali/statiche (già condivise via bag) e i parametri (che hanno il
            // loro percorso box dedicato Fase 4).
            foreach (var lambdaExpr in FindAllNodesGeneric<LambdaExpr>(body.Value.Cast<AstNode>()))
                foreach (var mutSym in CollectMutatedSymbols(lambdaExpr))
                    if (!IsGlobalSymbol(mutSym) && !IsStaticSymbol(mutSym) && !_parameters.ContainsKey(mutSym))
                        result.Add(mutSym);
            }
            _boxedSymbolsCache = result;
            return result;
        }
    }

    /// <summary>
    /// Il corpo della procedura che QUESTA istanza sta compilando, per il pre-scan locale di
    /// <see cref="BoxedSymbols"/>. Per un metodo/costruttore/procedura "normale" è
    /// <c>_procedureDefinitions[_currentReturnSymbolId]</c> (già disponibile). Per Main (e i
    /// costruttori statici), <c>_currentReturnSymbolId</c> è -1 per costruzione — cerca "Main"
    /// per nome tra le procedure del programma (unico caso in cui non c'è un SymbolId diretto
    /// da questa istanza verso il proprio corpo).
    /// </summary>
    private ImmutableArray<Stmt>? GetCurrentProcedureBodyForBoxScan()
    {
        if (_currentReturnSymbolId >= 0 && _procedureDefinitions.TryGetValue(_currentReturnSymbolId, out var def))
            return def.Body;
        var main = _program.Symbols.GetAllProcedures()
            .OfType<FunctionDefinition>()
            .FirstOrDefault(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase));
        return main?.Body;
    }

    private static IEnumerable<FunctionExpr> FindAllCallExpressions(ImmutableArray<Stmt> body) =>
        FindAllNodesGeneric<FunctionExpr>(body.Cast<AstNode>());

    /// <summary>Attraversamento generico dell'AST per RIFLESSIONE (non uno switch esaustivo per
    /// tipo di nodo, come in Linker.Rewrites.cs::FindAllNodesOfType — quella logica è privata
    /// al Linker, non condivisibile da un assembly diverso senza esporla). Cammina OGNI
    /// proprietà pubblica che è un AstNode o una collezione di AstNode — più lento di uno
    /// switch scritto a mano, ma sempre COMPLETO per costruzione: usato solo in un pre-scan
    /// per-procedura eseguito una volta per corpo compilato, mai in un hot path. Raccoglie
    /// nodi di un tipo T qualunque (qui: FunctionExpr, per trovare le CALL EXPRESSION).</summary>
    private static IEnumerable<T> FindAllNodesGeneric<T>(IEnumerable<AstNode> roots) where T : AstNode
    {
        var stack = new Stack<AstNode>(roots);
        var visited = new HashSet<AstNode>();
        var results = new List<T>();
        while (stack.Count > 0)
        {
            var node = stack.Pop();
            if (!visited.Add(node)) continue;
            if (node is T match) results.Add(match);
            foreach (var prop in node.GetType().GetProperties(BindingFlags.Public | BindingFlags.Instance))
            {
                if (prop.GetIndexParameters().Length > 0) continue;
                object? val;
                try { val = prop.GetValue(node); } catch { continue; }
                switch (val)
                {
                    case AstNode child:
                        stack.Push(child);
                        break;
                    case string:
                        break;
                    case System.Collections.IEnumerable seq:
                        PushEnumerableChildren(seq, stack);
                        break;
                }
            }
        }
        return results;
    }

    /// <summary>
    /// Call-site (Fase 4.5): l'argomento per una posizione box-typed del callee. Se
    /// l'espressione è un riferimento diretto a un simbolo DI QUESTA PROCEDURA già
    /// rappresentato come box (un local in <see cref="BoxedSymbols"/>, o un parametro con
    /// <c>ParameterInfo.IsBox</c>), passa il BOX STESSO (identità condivisa — le mutazioni
    /// della closure del callee restano visibili qui). Altrimenti (letterale, espressione,
    /// variabile non condivisibile a lungo termine) costruisce un box "usa e getta": la
    /// chiamata e la closure restituita restano corrette, ma senza propagazione delle
    /// mutazioni future al chiamante — limite noto (vedi TODO §4.5), mai un crash.
    /// </summary>
    private void EmitCallArgumentAsBox(Expr argExpr, DataType paramDataType)
    {
        var boxClrType = typeof(ByRefBox<>).MakeGenericType(MapProcedureType(paramDataType, _types, _classTypes));
        if (argExpr is VariableExpr { SymbolId: >= 0 } ve && TryEmitBoxReference(ve.SymbolId))
            return;
        EmitExpression(argExpr);
        _il.Emit(OpCodes.Newobj, boxClrType.GetConstructor([boxClrType.GetGenericArguments()[0]])!);
    }
}
