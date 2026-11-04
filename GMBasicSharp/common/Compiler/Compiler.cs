// FILE: Interpreter.Compiler.Compiler.cs

using System.Collections.Immutable;


namespace Compiler;

/// <summary>
/// Voce della tabella host-procs per macro/EVAL annidate (S_eval §ii): una procedura
/// del programma OSPITE chiamabile dal codice macro via <see cref="OpCode.OP_CALL_HOST"/>.
/// Addr = indirizzo Ip nel chunk ospite (PCodeVM), oppure -1 (host CLR su IL: la
/// risoluzione avviene per nome nel registro IlRuntime). Mai serializzata in A1/GMBC.
/// </summary>
public sealed record MacroHostProc(string Name, int Addr, int Arity);

/// <summary>
/// Compila un Abstract Syntax Tree (AST) in un <see cref="CompiledChunk"/> di P-Code.
/// La classe è suddivisa in file parziali per organizzare la logica di compilazione.
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Il <see cref="CompiledChunk"/> in costruzione. Contiene il P-Code, la constant pool e le linee sorgente.
    /// </summary>
    private readonly CompiledChunk _chunk = new();

    /// <summary>
    /// Una lista piatta di tutte le variabili locali attualmente in scope durante la compilazione.
    /// L'ordine nella lista determina il loro indice (slot) sullo stack.
    /// </summary>
    private readonly List<Local> _locals = [];
    private int _maximumLocalCount;

    /// <summary>
    /// Indice (in <see cref="_locals"/>) del primo local dichiarato nel BODY della procedura
    /// standalone correntemente compilata da <see cref="CompileProcedures"/> (dopo return-slot
    /// e parametri) — usato da <see cref="VisitEndProcedureStatement"/> per finalizzare
    /// (DESTRUCTOR) le istanze locali create con NEW dentro la SUB/FUNCTION quando questa
    /// termina per fine corpo (BUG-6). -1 = "non applicabile" (metodi di classe, costruttori,
    /// lambda: gestiti altrove o non ancora coperti, non toccare i loro locali).
    /// </summary>
    private int _bodyLocalsBoundary = -1;

    /// <summary>
    /// TRUE se la procedura standalone correntemente compilata contiene almeno un statement
    /// GOSUB nel proprio corpo. Quando è TRUE, un RETURN/EXIT SUB esplicito è AMBIGUO a
    /// compile-time (può terminare la procedura OPPURE tornare da una subroutine GOSUB attiva,
    /// deciso a runtime da <c>task.GosubContinuations</c> — vedi OP_RETURN in PCodeProcessor):
    /// la finalizzazione dei locali del body viene quindi SKIPPATA per non distruggere i local
    /// della procedura a un RETURN di una subroutine GOSUB (residuo BUG-6, documentato). Quando
    /// è FALSE, ogni RETURN/EXIT SUB nel body termina SEMPRE la procedura → finalizziamo i
    /// locali del body PRIMA di OP_RETURN (fix residuo BUG-6, 2026-09-03).
    /// </summary>
    private bool _currentProcHasGosub;

    /// <summary>
    /// Contatore utilizzato per generare etichette di salto univoche per il P-Code interno
    /// (es. salti per i cicli appiattiti, GO SELECT, blocchi di codice).
    /// </summary>
    private int _compilerLabelCounter;

    /// <summary>
    /// Tabella dei simboli per le variabili globali. Mappa il nome della variabile (stringa)
    /// al suo indice (slot) nella tabella globale della VM.
    /// </summary>
    private readonly Dictionary<string, int> _globals = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Lista che contiene le informazioni dettagliate (incluso il tipo) per ogni variabile globale,
    /// indicizzata in modo coerente con `_globals`.
    /// </summary>
    private readonly List<GlobalInfo> _globalInfos = [];

    /// <summary>
    /// Hook opzionale per il percorso "codice dinamico" (macro/EVAL dell'IL Backend, vedi
    /// IlRuntime.MacroEvalPCode): questo Compiler compila un singolo statement/espressione in
    /// ISOLAMENTO (nessun PreCompilationSetup/RegisterModuleLevelConstants sul programma
    /// ospite), quindi <see cref="_globals"/> parte VUOTO — senza questo hook, qualunque
    /// riferimento a una variabile GLOBALE già esistente nel programma ospite cadrebbe nel
    /// fallback silenzioso "non trovata -> NIL" (vedi LoadVariable/AssignVariable). Se impostato,
    /// viene interrogato PRIMA di quel fallback: se ritorna true per un nome, quel nome viene
    /// registrato come globale in questa compilazione isolata (stesso trattamento di un globale
    /// "vero"), permettendo al chiamante di leggere/scrivere il valore reale corrispondente nel
    /// programma ospite (via <see cref="GlobalSlots"/> dopo la compilazione). Null (default) per
    /// qualunque altro uso di Compiler: comportamento identico a prima, invariato.
    /// </summary>
    public Func<string, bool>? ExternalGlobalProbe { get; set; }

    /// <summary>
    /// True quando questo Compiler compila codice macro/EVAL (impostato dai percorsi
    /// MacroEval* di IlRuntime). Semantica S_eval: il codice gira in un nuovo scope figlio —
    /// <c>STATIC</c> e definizioni di procedura non hanno senso qui e generano errore esplicito
    /// (mai no-op silenzioso). False (default) per qualunque compilazione normale.
    /// </summary>
    public bool IsMacroScope { get; set; }

    /// <summary>
    /// Vista di sola lettura di <see cref="_globals"/> — nome variabile normalizzato -> slot
    /// P-Code. Usata dal chiamante (vedi <see cref="ExternalGlobalProbe"/>) per sapere, dopo la
    /// compilazione, quali nomi sono stati effettivamente registrati come globali e a quale slot,
    /// cosi' da poter seminare/rileggere il valore reale in quello slot prima/dopo l'esecuzione.
    /// </summary>
    public IReadOnlyDictionary<string, int> GlobalSlots => _globals;

    /// <summary>
    /// Risolve <paramref name="name"/> come globale già noto a questa compilazione, oppure —
    /// se non trovato e <see cref="ExternalGlobalProbe"/> conferma che il programma ospite lo
    /// conosce come globale — lo registra al volo con un nuovo slot. Vedi
    /// <see cref="ExternalGlobalProbe"/> per il perché. A probe nullo si comporta esattamente
    /// come il vecchio `_globals.TryGetValue` diretto (nessuna regressione per gli altri usi).
    /// </summary>
    private bool TryResolveOrRegisterGlobal(string name, out int index)
    {
        if (_globals.TryGetValue(name, out index)) return true;
        if (ExternalGlobalProbe != null && ExternalGlobalProbe(name))
        {
            index = _globals.Count;
            _globals[name] = index;
            return true;
        }
        index = -1;
        return false;
    }

    private DataType? _currentFunctionReturnType;
    private string? _currentProcedureName;

    // Impostato da VisitMethodMember mentre compila il corpo di un metodo — permette a
    // CompilerInferExpressionType di risolvere "__context"/"__ClosureCtx_*" (il riferimento
    // al contesto di closure, riscritto dal linker come VariableExpr con quel nome) al suo
    // vero ClassType anche dentro un metodo, dove _locals[0] si chiama "SELF" e non
    // "__context" — senza questo, InferExpressionTypeFromTarget non trova mai un match per
    // nome in _locals e ripiega su DeduceTypeFromSuffix, perdendo il ClassType.
    private ClassType? _currentMethodClassType;

    // Un costruttore (SUB NEW) restituisce sempre l'istanza appena creata
    // (task.Frame.ConstructorInstance), mai NIL: quando true, VisitEndProcedureStatement
    // (Compiler.Gosub.cs) non emette l'OP_PUSH_NIL di default per l'EndProcedureStmt
    // finale del corpo del costruttore, cosi' l'OP_RETURN che segue (emesso da
    // VisitConstructorMember) trova lo stack pulito e ricade sul fallback
    // ConstructorInstance in PCodeProcessor.OP_RETURN invece di restituire NIL.
    private bool _isCompilingConstructor;

    /// <summary>
    /// Una lista di salti che devono essere patchati con indirizzi nella constant pool.
    /// Utilizzata principalmente per le istruzioni <c>OP_ON_KEY</c> o HTTP CALLBACKS
    /// che referenziano indirizzi di codice compilato.
    /// La tupla contiene l'offset nel P-Code dove si trova il riferimento alla costante
    /// e il Token che rappresenta la label o procedura target.
    /// </summary>
    private readonly List<(int, Token)> _constantsToPatch = [];
    // Operandi assoluti non immediatamente preceduti dal proprio opcode (oggi i target
    // multipli di OP_ON_JUMP). Non possono usare _jumpsToPatch, il cui decoder guarda
    // patchOffset-1 e li scambierebbe per salti relativi leggendo type/count come opcode.
    private readonly List<(int PatchOffset, Token Target)> _absoluteAddressPatches = [];

    // Trasferimenti di controllo BASIC espliciti. Dopo il patching degli indirizzi vengono
    // validati contro le regioni TRY: entrare dall'esterno in TRY/CATCH/FINALLY salterebbe
    // OP_PUSH_TRY e costruirebbe uno stato di unwind impossibile. I salti strutturali emessi
    // internamente dal compilatore non appartengono a questa lista.
    private sealed record ExplicitControlTransfer(
        int InstructionIp,
        int TargetOperandIp,
        string Operation,
        bool RequireUnprotectedTarget = false);

    private readonly List<ExplicitControlTransfer> _explicitControlTransfers = [];

    /// <summary>
    /// La profondità dello scope corrente durante la compilazione (0 per lo scope globale,
    /// incrementata per ogni blocco o procedura annidata).
    /// </summary>
    private int _scopeDepth;

    /// <summary>
    /// Flag di stato per istruire `VisitJumpStatement` a ignorare il prossimo
    /// salto. Usato per sopprimere il JUMP ridondante che segue un NEXT
    /// in un ciclo FOR ottimizzato.
    /// </summary>
    private bool _ignoreNextJump;
    /// <summary>Dentro la clausola WITH di DBF REPLACE: un identificatore non dichiarato e0027 un
    /// CAMPO late-binding (il Linker non produce il nodo bare-field dentro il valore).</summary>
    private bool _dbfReplaceValueMode;
    /// <summary>Dentro una clausola FOR/WHILE di un comando DBF: i nomi nudi sono campi.</summary>
    private bool _dbfScopeCondMode;

    /// <summary>
    /// Stack che tiene traccia del contesto dei cicli FOR aperti.
    /// Ogni elemento è una tupla contenente l'indirizzo di inizio del corpo del loop
    /// e il token dell'etichetta di uscita.
    /// </summary>
    private readonly Stack<(int loopStartOffset, Token exitLabel, int counterSlot)> _forLoopContexts = new();
    // true se il contatore FOR corrente ombreggiava un binding esterno (DIM/globale): a NEXT
    // l'ombra va rimossa (l'esterno riemerge). false → il contatore va promosso a scope
    // esterno a fine ciclo (visibile dopo il NEXT). Vedi Compiler.Loops (canone 2026-09-30).
    private readonly Stack<bool> _forCounterHadOuter = new();

    // Predicati VALID pendenti: (indice costante placeholder, nome procedura, espressione).
    // Le subroutine vengono emesse a fine procedura (dopo OP_RETURN del body — mai raggiungibili
    // dal flusso) e la costante placeholder viene patchata con l'indirizzo reale.
    private readonly List<(int constIndex, string? procName, Expr expr, List<object?> pool)> _pendingValidPredicates = new();
    private readonly Stack<(int arrSlot, int ctrSlot, bool isDictLike, bool isMatrix, bool isChannel, int varCount)> _feSlots = new();

    /// <summary>
    /// Flag che indica se il punto di compilazione corrente è raggiungibile.
    /// Usato per l'eliminazione del codice morto (dead code elimination).
    /// </summary>
    private bool _isReachable = true;

    /// <summary>
    /// L'indice della riga di codice sorgente corrente, utilizzato per mappare
    /// ogni istruzione P-Code alla sua origine nel sorgente BASIC.
    /// </summary>
    private int _currentSourceLineIndex = -1;

    /// <summary>
    /// Mappa gli indirizzi delle procedure (SUB/FUNCTION/GENERATOR) dal loro nome
    /// all'offset di inizio nel <see cref="CompiledChunk.Code"/>.
    /// Utilizzata per risolvere le chiamate <c>OP_CALL</c> e <c>OP_GOSUB</c>.
    /// </summary>
    private readonly Dictionary<string, int> _procedureAddresses = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Tabella ordinata delle procedure host visibili a una compilazione macro (impostata
    /// dal chiamante: EvalMacroInPCode dal chunk ospite, MacroEval* dal programma).
    /// L'indice nella lista è l'operando di OP_CALL_HOST. Null = nessuna (comportamento
    /// preesistente: chiamata ignota = errore esplicito).
    /// </summary>
    public List<MacroHostProc>? ExternalHostProcs { get; set; }

    /// <summary>
    /// Indirizzo Ip -> numero di parametri dichiarati dalla SUB/FUNCTION a quell'indirizzo.
    /// Serve a runtime a chiamanti che costruiscono un elenco di argomenti SENZA passare dal
    /// normale sito di chiamata compilato (che allinea gli argomenti al type-checking) — oggi
    /// solo PCodeProcessor.SpawnHttpHandlerTask (dispatch HTTP/WebSocket): senza questo, un
    /// handler con MENO parametri di quelli forniti (es. 2 invece di 3 per req/res/next) riceve
    /// comunque tutti gli argomenti, disallineando silenziosamente il binding (l'ultimo
    /// parametro dichiarato viene legato al PRIMO valore push-ato in eccesso, non a quello
    /// giusto — vedi TODO-PCODE-BUGS.md).
    /// </summary>
    private readonly Dictionary<int, int> _procedureArities = new();

    /// <summary>
    /// Mappa i numeri di riga del codice sorgente BASIC al loro corrispondente
    /// offset nel P-Code generato. Utilizzata per i salti <c>GOTO &lt;numero_riga&gt;</c>.
    /// </summary>
    private readonly Dictionary<int, int> _lineNumberAddresses = [];

    /// <summary>
    /// Contesto di compilazione (BasicProgram) per accesso a SymbolTable.
    /// </summary>
    private ICompilationContext? _ctx;

    /// <summary>
    /// Una lista di salti che puntano a numeri di riga, che devono essere
    /// patchati dopo che tutti i numeri di riga sono stati compilati.
    /// La tupla contiene l'offset nel P-Code dove si trova l'operando del salto
    /// e il numero di riga target.
    /// </summary>
    private readonly List<(int, int)> _lineNumberJumpsToPatch = [];

    /// <summary>
    /// Mappa ottimizzata per la ricerca delle definizioni di procedura per nome e arità (numero di parametri).
    /// Utilizzata per la risoluzione degli overload delle chiamate a funzioni definite dall'utente.
    /// </summary>
    private readonly Dictionary<(string, int), ProcedureDefinition> _procedureMap = new(new ProcedureKeyComparer());

    /// <summary>
    /// Mappa gli offset dei campi all'interno di una struttura (UDT) o di un'istanza di classe.
    /// La chiave è una stringa nel formato "NomeTipo.NomeCampo" o "NomeClasse.NomeCampo",
    /// e il valore è l'offset in byte all'interno dell'istanza.
    /// Utilizzata per gli opcode <c>OP_LOAD_FIELD</c> e <c>OP_STORE_FIELD</c>.
    /// </summary>
    private readonly Dictionary<string, ushort> _fieldOffsets = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// SymbolId di tutti i campi STATIC di CLASS scoperti durante la compilazione (popolato in
    /// VisitClassDefinition). Il Compiler non ha accesso alla SymbolTable/BasicProgram (lavora solo
    /// sull'AST già risolto dal Linker), quindi questo set locale è l'unico modo per
    /// VisitFieldAccessExpression/AssignField di sapere se un dato FieldSymbolId va indirizzato allo
    /// storage condiviso (OP_LOAD_STATIC_FIELD/OP_STORE_STATIC_FIELD) invece che a quello per-istanza
    /// (OP_LOAD_MEMBER/OP_STORE_MEMBER).
    /// </summary>
    private readonly HashSet<int> _staticFieldSymbolIds = new();

    /// <summary>
    /// Ottiene o imposta un valore che indica se l'output dettagliato del compilatore è attivo.
    /// </summary>
    public static bool VerboseCompiler { get; set; }

    // Lista di riferimenti ai descrittori di classe che necessitano di essere patchati
    // con le V-table e gli indirizzi dei costruttori dopo che tutti i metodi sono stati compilati.
    private readonly List<(int ConstIndex, object ClassDef)> _classDescriptorsToPatch = [];

    // Mappa per memorizzare i descrittori delle classi già compilati, indicizzati per nome della classe.
    // La chiave è il nome della classe, il valore è il Dictionary<string, object?> che rappresenta il descrittore.
    private readonly Dictionary<string, Dictionary<string, object?>> _compiledClassDescriptors =
        new(StringComparer.OrdinalIgnoreCase);

    private readonly Dictionary<(string, int), ExternalProcedureDefinition> _ffiFunctions =
        new(new ProcedureKeyComparer());

    /// <summary>
    /// Mappa per le costanti definite dall'utente con CONST.
    /// Chiave: nome della costante. Valore: il suo valore letterale.
    /// </summary>
    private readonly Dictionary<string, object> _constants = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Rappresenta una variabile catturata da uno scope esterno (una "upvalue").
    /// </summary>
    public sealed class Upvalue
    {
        public byte Index;
        public bool IsLocal;
        /// <summary>1 = locale del frame chiamante; 2 = UPVALUE del frame corrente
        /// (cattura TRANSITIVA: lambda dentro lambda). Emesso in OP_CLOSURE al posto del
        /// vecchio booleano (0/1) — il valore 0 resta il "frame esterno sullo stack" usato
        /// dai riferimenti di funzione. Prima una cattura transitiva viaggiava come 0 e la
        /// VM la interpretava come globale → la variabile del livello esterno valeva 0
        /// (PCode/VM-C: `{|a| RETURN {|b| RETURN a+b+k}}` perdeva `k`).</summary>
        public byte Kind;
    }

    private readonly Compiler? _enclosing;
    private readonly List<Upvalue> _upvalues = [];

    /// <summary>Indirizzi-closure emessi da questo compilatore FIGLIO come costanti
    /// (indice nel pool ROOT, indirizzo relativo al PROPRIO chunk): il genitore li
    /// riloca al merge. Bug closure annidate (2026-10-02): l'indirizzo della closure
    /// interna non veniva shiftato all'inserimento nel chunk del genitore → la VM
    /// saltava a codice sbagliato (hang su PCode/VM-C per EVAL annidato).</summary>
    private readonly List<(int ConstIndex, int ChildAddress)> _pendingClosureFixes = [];
    public IReadOnlyList<(int ConstIndex, int ChildAddress)> GetPendingClosureFixes() => _pendingClosureFixes;

    /// <summary>
    /// Costruttore per il compilatore principale.
    /// </summary>
    public Compiler()
    {
        _enclosing = null;
    }

    /// <summary>
    /// Costruttore per un compilatore "figlio", usato per compilare procedure locali.
    /// Eredita le mappe di simboli globali dal compilatore "padre" per garantire
    /// la corretta risoluzione di procedure e variabili globali.
    /// </summary>
    private Compiler(Compiler enclosing)
    {
        _enclosing = enclosing;

        // Eredita le mappe di simboli e le tabelle di definizione condivise.
        // Queste non vengono modificate durante la compilazione di una closure,
        // ma solo lette.
        _procedureMap = enclosing._procedureMap;
        _globals = enclosing._globals;
        _globalInfos = enclosing._globalInfos;
        _fieldOffsets = enclosing._fieldOffsets;
        _compiledClassDescriptors = enclosing._compiledClassDescriptors;
        _ffiFunctions = enclosing._ffiFunctions;
        _constants = enclosing._constants;

        // Inizializza le proprie strutture dati di stato locale.
        _chunk = new CompiledChunk();
        _locals = [];
        _upvalues = [];
        _scopeDepth = 0;
        _isReachable = true;
    }

    /// <summary>
    /// Restituisce la lista di upvalues catturate da questo compilatore.
    /// </summary>
    public IReadOnlyList<Upvalue> GetUpvalues() => _upvalues;

    /// <summary>
    /// Compila l'intero programma BASIC AST in un <see cref="CompiledChunk"/> di P-Code.
    /// Questa versione è stata refattorizzata e corretta per gestire l'inizializzazione
    /// delle variabili globali.
    /// </summary>
    public CompiledChunk Compile(ICompilationContext ctx)
    {
        _ctx = ctx;
        PreCompilationSetup(ctx);
        RegisterModuleLevelConstants(ctx);
        CompileDefinitions(ctx);

        // --- NUOVA FASE: Inizializzazione delle Variabili Globali ---
        var globalInitChunkStart = _chunk.Code.Count;
        CompileGlobalInitializers(ctx);

        if (_chunk.Code.Count > globalInitChunkStart)
        {
            _procedureAddresses["__global_init"] = globalInitChunkStart;
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_RETURN);
        }

        // Inizializzazione dei campi STATIC di CLASS (PUBLIC STATIC DIM x = ...): a differenza dei
        // campi non-STATIC (i cui inizializzatori vengono emessi per OGNI istanza dal costruttore,
        // vedi EmitFieldInitializers in Compiler.UserTypes.cs), un campo STATIC e' condiviso da
        // TUTTE le istanze e va inizializzato UNA SOLA VOLTA, prima che qualunque codice utente giri.
        var staticInitChunkStart = _chunk.Code.Count;
        CompileStaticFieldInitializers(ctx);
        if (_chunk.Code.Count > staticInitChunkStart)
        {
            _procedureAddresses["__static_init"] = staticInitChunkStart;
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_RETURN);
        }

        CompileMainBodyAndSubroutines(ctx);

        // Set entry point to SUB Main if it exists
        if (_procedureAddresses.TryGetValue("Main", out var mainAddr) ||
            _procedureAddresses.TryGetValue("__main", out mainAddr))
        {
            // Se esiste SUB Main, il corpo top-level (__main) va ESEGUITO PRIMA di Main:
            // altrimenti il codice di livello modulo (DIM + assegnazioni, chiamate) è compilato
            // in __main ma MAI invocato → fallimento SILENZIOSO (variabili vuote/0). Bug reale
            // riprodotto con `DIM MSG AS STRING : MSG="x" : SUB Main : PRINT MSG` → vuoto.
            var hasMain = _procedureAddresses.ContainsKey("Main");
            var hasMainBody = hasMain && _procedureAddresses.TryGetValue("__main", out _);
            var hasGlobalInit = _procedureAddresses.TryGetValue("__global_init", out var globalInitAddr);
            var hasStaticInit = _procedureAddresses.TryGetValue("__static_init", out var staticInitAddr);
            var hasDestructors = _procedureAddresses.Keys.Any(k => k.EndsWith(".__delete"));

            if (hasGlobalInit || hasStaticInit || hasDestructors || hasMainBody)
            {
                // __main viene chiamato dal prologo e deve quindi terminare con OP_RETURN.
                // CompileMainBodyAndSubroutines applica direttamente questo contratto anche
                // nel caso destructor-only. Non cercare mai OP_HALT scandendo i byte grezzi:
                // opcode e operandi condividono lo stesso stream e 0x3f può essere, per
                // esempio, il byte basso di OP_PUSH_CONST 63. La vecchia scansione trasformava
                // quell'operando in 0x3e (OP_RETURN), corrompendo silenziosamente il chunk.

                var prologueStart = _chunk.Code.Count;
                // __static_init PRIMA di __global_init: i campi STATIC vanno inizializzati
                // "prima che qualunque codice utente giri" (vedi commento su
                // CompileStaticFieldInitializers) — ma __global_init ESEGUE codice utente (i
                // costruttori dei DIM globali, es. `DIM gLog = NEW Risorsa(...)`), che può già
                // leggere/scrivere campi STATIC (es. un contatore istanze incrementato dal
                // costruttore). Con l'ordine precedente (global poi static), l'incremento fatto
                // dal costruttore di un DIM globale veniva SILENZIOSAMENTE cancellato subito
                // dopo dal `= 0` di __static_init — bug scoperto durante il fix di BUG-7
                // (riassegnazione + STATIC counter in ClassesDestructor.bas).
                if (hasStaticInit)
                {
                    EmitByte((byte)OpCode.OP_CALL);
                    EmitShort(staticInitAddr);
                }
                if (hasGlobalInit)
                {
                    EmitByte((byte)OpCode.OP_CALL);
                    EmitShort(globalInitAddr);
                }
                // Corpo top-level (__main) PRIMA di SUB Main: esegue le istruzioni di livello
                // modulo che altrimenti sarebbero ignorate (bug DIM/assegnazioni top-level).
                if (hasMainBody && _procedureAddresses.TryGetValue("__main", out var mainBodyAddr))
                {
                    EmitByte((byte)OpCode.OP_CALL);
                    EmitShort(mainBodyAddr);
                }
                EmitByte((byte)OpCode.OP_CALL);
                EmitShort(mainAddr);
                /* FUNCTION Main() AS INTEGER: il valore di ritorno (sullo stack) e' l'EXIT CODE.
                 * SUB Main ritorna NIL → 0. Stessa convenzione di IL/C99Native. */
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PROGRAM_EXIT); EmitByte((byte)1);
                EmitByte((byte)OpCode.OP_POP);
                EmitGlobalDestructorFinalizers();
                EmitByte((byte)OpCode.OP_HALT);
                _chunk.EntryPoint = prologueStart;
            }
            else
            {
                _chunk.EntryPoint = mainAddr;
            }
        }

        PostCompilationPatching();

        _isReachable = true;
        EmitByte((byte)OpCode.OP_HALT, -1);
        //LogDebug("[COMPILER] HALT finale emesso.");

        // _globalInfos è ormai finale (vedi GlobalCount su CompiledChunk per il perché).
        _chunk.GlobalCount = _globalInfos.Count;
        _chunk.LocalCount = _maximumLocalCount;
        _chunk.GlobalNames = new string[_globalInfos.Count];
        foreach (var kv in _globals)
        {
            if ((uint)kv.Value < (uint)_chunk.GlobalNames.Length)
                _chunk.GlobalNames[kv.Value] = kv.Key;
        }

        // _procedureAddresses è ormai finale — copiato per intero cosi' la VM puo' risolvere a
        // runtime un nome di procedura a indirizzo Ip (vedi CompiledChunk.ProcedureAddresses).
        foreach (var kv in _procedureAddresses) _chunk.ProcedureAddresses[kv.Key] = kv.Value;

        // _procedureArities è ormai finale — copiato per intero (vedi CompiledChunk.ProcedureArities).
        foreach (var kv in _procedureArities) _chunk.ProcedureArities[kv.Key] = kv.Value;

        return _chunk;
    }

    /// <summary>
    /// Emette il P-Code per applicare, UNA SOLA VOLTA, gli inizializzatori inline di tutti i campi
    /// STATIC di tutte le CLASS del programma — prima che qualunque codice utente (incluso il primo
    /// costruttore) possa leggerli. Simmetrico a EmitFieldInitializers (Compiler.UserTypes.cs), che
    /// gestisce i campi NON-static, ripetuto per ogni istanza.
    /// </summary>
    private void CompileStaticFieldInitializers(ICompilationContext ctx)
    {
        foreach (var classDef in ctx.GetAllClasses())
        {
            foreach (var fieldMember in classDef.Members.OfType<FieldMember>())
            {
                var decl = fieldMember.Declaration;
                if (decl.InitialValue is null) continue;
                if (!decl.Flags.HasFlag(SymbolFlags.IsStatic)) continue;

                VisitExpression(decl.InitialValue);
                EmitByte((byte)OpCode.OP_STORE_STATIC_FIELD);
                EmitShort((ushort)decl.SymbolId);
            }
        }
    }

    /// <summary>
    /// Esegue tutti i passaggi di setup prima della generazione del P-Code (Passata 0 e 1).
    /// </summary>
    private void PreCompilationSetup(ICompilationContext ctx)
    {
        // Built-in namespace constants
        _constants["PI"] = Math.PI;
        _constants["MATH.PI"] = Math.PI;
        PopulateProcedureAndFfiMaps(ctx);
        RegisterSystemTypeOffsets();
        PopulateDataPool(ctx);
        DefineGlobalVariables(ctx);
        // Nomi DYNAMIC (scope dinamico per NOME): pre-scan una volta, prima di compilare le
        // procedure — una lettura a cascata può precedere la dichiarazione. Vedi
        // Compiler.Scope.Dynamic.cs.
        CollectDynamicNames(ctx);
    }

    /// <summary>
    /// Pre-pass (twin-port GMBasicAI): registra le CONST a livello di MODULO in _constants
    /// PRIMA di CompileDefinitions. Senza, le costanti top-level arrivavano al compilatore
    /// DOPO le procedure che le usavano (l'ordine di compilazione è PreCompilationSetup ->
    /// CompileDefinitions [che include CompileProcedures] -> CompileGlobalInitializers ->
    /// CompileMainBodyAndSubroutines) e il fallback _constants.TryGetValue in VisitExpression
    /// restava morto per le costanti utente — %NOME cascava a NIL in ogni posizione NON
    /// riscritta dal Linker (es. MATCH %K2). Difesa in profondità oltre al folding Rewrites.
    /// </summary>
    private void RegisterModuleLevelConstants(ICompilationContext ctx)
    {
        var modules = ctx.LoadedModules?.Values.ToList() ?? new List<BasicModule>();
        if (ctx.MainModule != null && !modules.Contains(ctx.MainModule))
            modules.Add(ctx.MainModule);
        foreach (var module in modules)
        {
            foreach (var stmts in module.Statements.Values)
            {
                foreach (var stmt in stmts)
                {
                    if (stmt is ConstStmt cs) VisitConstStatement(cs);
                }
            }
        }
    }

    /// <summary>
    /// Compila tutte le definizioni a livello di modulo (UDT, Classi, Procedure).
    /// </summary>
    private void CompileDefinitions(ICompilationContext ctx)
    {
        CompileUserDefinedTypes(ctx);
        CompileClasses(ctx);
        CompileProcedures(ctx);
    }

    /// <summary>
    /// Itera su tutte le dichiarazioni globali e genera il P-Code per le loro
    /// espressioni di inizializzazione.
    /// </summary>
    /// <summary>Dichiarazioni globali VERAMENTE top-level (compaiono DIRETTAMENTE nelle righe
    /// del modulo, non annidate dentro un blocco FOR/IF/WHILE/DO) il cui inizializzatore viene
    /// "hoistato" in __global_init (vedi CompileGlobalInitializers) — memoizzato perché serve
    /// anche a CompileMainBodyAndSubroutines per NON ricompilarle una seconda volta in linea.
    /// GMBasic non ha scoping a blocco: una DIM dentro un FOR/FOR EACH è ANCH'ESSA un simbolo
    /// GLOBALE (stesso bag, vedi GetAllGlobalVariables — filtra solo per SymbolFlags.IsGlobal,
    /// indipendente dall'annidamento sintattico), ma il suo inizializzatore deve girare IN
    /// LINEA a ogni iterazione (può dipendere da variabili di ciclo come `i` o da dati
    /// popolati da statement precedenti nello stesso blocco), MAI essere hoistato ed eseguito
    /// UNA SOLA VOLTA prima ancora che il blocco/loop parta. Bug reale (pagina BSP resa
    /// vuota): `FOR i = 0 TO pkgs.Count()-1: DIM nm = pkgs[i]["Name"]: PRINT ...: NEXT`
    /// eseguiva l'inizializzatore di `nm` in __global_init (chiamato dal prologo PRIMA di
    /// __main), con `i` non ancora inizializzato e `pkgs` ancora vuoto.</summary>
    private HashSet<VariableDeclaration>? _hoistedGlobalInitDecls;

    private bool IsDynamicVariableName(string name)
        => _dynamicNames.ContainsKey(NormalizeIdentifierName(name));

    private HashSet<VariableDeclaration> GetHoistedGlobalInitDecls(ICompilationContext ctx)
    {
        if (_hoistedGlobalInitDecls != null) return _hoistedGlobalInitDecls;
        var set = new HashSet<VariableDeclaration>(ReferenceEqualityComparer.Instance);
        if (ctx.MainModule != null)
        {
            // BUG REALE (pagina BSP: "DIM x = ..." dentro FOR sempre vuoto). La versione
            // precedente iterava ctx.MainModule.Statements.Values (un Dictionary: ordine di
            // enumerazione NON garantito essere l'ordine di riga) e includeva OGNI
            // DeclarationStmt del modulo, senza distinguere quelle DENTRO un blocco
            // FOR/IF/WHILE/DO da quelle davvero in testa al modulo — nonostante il commento
            // originale dicesse il contrario, non c'era nessun controllo di annidamento.
            // GMBasic non ha un AST a blocchi per questi costrutti: dopo l'ottimizzatore, FOR/
            // IF/WHILE/DO sono già appiattiti in LabelStmt/JumpStmt/ConditionalJumpStmt (+
            // GotoStmt/GosubStmt/OnJumpStmt per i salti espliciti) intermezzati nella sequenza
            // lineare per riga. Quindi l'unico modo corretto per riconoscere "sono ancora nel
            // prefisso strettamente lineare in testa al modulo" è camminare le righe IN ORDINE
            // DI ESECUZIONE (ExecutionOrder, stesso fallback di CompileMainBodyAndSubroutines)
            // e fermarsi al primo statement che introduce un salto o un'etichetta: da quel
            // punto in poi non possiamo più garantire che una riga giri esattamente una volta,
            // incondizionatamente, prima di __main.
            var executionOrder = ctx.MainModule.ExecutionOrder;
            if (executionOrder.IsDefaultOrEmpty)
                executionOrder = ctx.MainModule.Statements.Keys.OrderBy(k => k).ToImmutableArray();

            foreach (var lineNumber in executionOrder)
            {
                if (!ctx.MainModule.Statements.TryGetValue(lineNumber, out var stmts)) continue;
                var stoppedThisLine = false;
                foreach (var stmt in stmts)
                {
                    if (stmt is LabelStmt or JumpStmt or ConditionalJumpStmt or GotoStmt or GosubStmt or OnJumpStmt)
                    {
                        stoppedThisLine = true;
                        break;
                    }
                    if (stmt is DeclarationStmt ds && ds.Keyword.Kind != TokenKind.Dynamic)
                    {
                        /* DYNAMIC: NON hoistato. Il valore iniziale deve passare da
                         * OP_DYN_DECLARE (frame/BaseDynamicVars); hoistarlo in uno slot
                         * globale lasciava il nome assente dallo scope dinamico a runtime
                         * (P-Code: @GET prefill vuoto e PRINT via OP_DYN_LOAD vuoto). */
                        foreach (var d in ds.Declarations)
                            if (d.InitialValue != null)
                                set.Add(d);
                    }
                    else if (stmt is not ConstStmt and not DataStmt)
                    {
                        /* Ordine di esecuzione (parità IL): una DICHIARAZIONE con inizializzatore
                         * che segue un'ISTRUZIONE non va più hoistata in __global_init — altrimenti
                         * il suo inizializzatore gira PRIMA delle istruzioni che lo precedono nel
                         * sorgente (bug: `AI AGENT CREATE` prima di `DIM r$ = AI.AGENTASKSYNC(...)`
                         * → «agent not registered» su PCode/VM-C, mentre IL emette in ordine
                         * sorgente). Si hoistano solo le DICHIARAZIONI del prefisso iniziale. */
                        stoppedThisLine = true;
                        break;
                    }
                }
                if (stoppedThisLine) break;
            }
        }
        return _hoistedGlobalInitDecls = set;
    }

    private void CompileGlobalInitializers(ICompilationContext ctx)
    {
        //LogDebug("[COMPILER] Compilazione degli inizializzatori globali...");

        // FIX (2026-09-02, bug "token$ risolta come LOCAL(0) vuoto su VM/C"): questo metodo
        // compila le espressioni inizializzatrici di OGNI `DIM x = expr` a livello modulo
        // (incluse le chiamate a funzioni STDLIB, es. AUTH.JWTPAYLOAD) SUBITO DOPO
        // CompileDefinitions/CompileProcedures, ma PRIMA di CompileMainBodyAndSubroutines —
        // che e' l'UNICO altro punto a resettare _locals/_scopeDepth prima di compilare
        // codice a livello Main. CompileProcedures (Compiler.cs) itera OGNI procedura
        // registrata, incluse le NativeProcedureDefinition di libreria (es. AUTH.JWTVERIFY/
        // JWTPAYLOAD, entrambe con un parametro chiamato letteralmente "token"), e per
        // ciascuna fa AddLocal(param) SENZA alcun reset _locals.Clear() dopo l'ULTIMA
        // procedura processata — quel local fantasma (es. "token", lasciato da
        // AUTH.JWTPAYLOAD, l'ultima nativa iterata) restava quindi agganciato a _locals
        // fino a qui. Se un initializer di DIM (es. `DIM p$ = AUTH.JWTPAYLOAD(token$)`)
        // referenzia per NOME una variabile che collide col parametro fantasma, LoadVariable
        // trova il local fantasma PRIMA di controllare i globals (ResolveLocal ha priorita'
        // su TryResolveOrRegisterGlobal) ed emette OP_LOAD_LOCAL(slot sbagliato) invece di
        // OP_LOAD_GLOBAL — riproducibile SOLO quando la chiamata funge da INITIALIZER di un
        // DIM (compilato qui), non quando e' un'istruzione PRINT/altro (compilata dopo il
        // reset di CompileMainBodyAndSubroutines, quindi sempre pulita). Stesso reset
        // esplicito gia' presente li' (riga ~637), applicato qui per parita'.
        _locals.Clear();
        _scopeDepth = 0;

        // Utilizza l'helper per ottenere tutte le dichiarazioni globali con un valore iniziale,
        // filtrate a quelle DIRETTAMENTE a livello modulo (vedi GetHoistedGlobalInitDecls).
        var topLevelDeclNodes = GetHoistedGlobalInitDecls(ctx);
        var allGlobalDeclarations = ctx.GetAllGlobalVariables()
            .Where(decl => decl.InitialValue != null
                && !IsDynamicVariableName(decl.NameCleaned.Lexeme)
                && (ctx.MainModule == null || topLevelDeclNodes.Contains(decl)));

        foreach (var declaration in allGlobalDeclarations)
        {
            // Array globale MULTI-DIMENSIONALE con inizializzatore (es. `DIM A(2,2) = {…}`):
            // il percorso semplificato sotto (VisitExpression del letterale + STORE_GLOBAL)
            // creava un array dinamico 1-D con i soli valori, ignorando le dimensioni
            // dichiarate — `A(0,0)` su un array 1-D restituiva null (stampa vuota). Qui si
            // replica il percorso di VisitDeclarationStatement: CREATE_ARRAY (dimCount +
            // dimensioni) + elementi + OP_INIT_ARRAY, poi lo store nel global.
            if (declaration.Dimensions.Length >= 2 && declaration.InitialValue is ArrayLiteralExpr arrLit)
            {
                var dims = declaration.Dimensions;
                var lowerBounds = declaration.LowerBounds;
                var toDims = lowerBounds.Length;
                EmitConstant((double)dims.Length);
                for (int di = 0; di < dims.Length; di++)
                {
                    if (di < lowerBounds.Length) VisitExpression(lowerBounds[di]);
                    VisitExpression(dims[di]);
                }
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.CREATE_ARRAY);
                EmitByte((byte)(dims.Length + toDims + 1));

                foreach (var element in arrLit.Elements) VisitExpression(element);
                EmitByte((byte)OpCode.OP_INIT_ARRAY);
                EmitShort((ushort)arrLit.Elements.Length);
            }
            else
            {
                // 1. Compila l'espressione iniziale. Il risultato viene lasciato sulla stack.
                // EmitCollectionAwareInitializer (non un VisitExpression diretto): un GLOBALE
                // con inizializzatore letterale e tipo COLLEZIONE dichiarato esplicitamente
                // (es. `DIM hs AS HASHSET<INTEGER> = [3,1,4,1,5]`, hoisted qui PRIMA del corpo
                // — VisitDeclarationStatement non viene MAI raggiunto per questi globali)
                // altrimenti VisitExpression del letterale chiamava SEMPRE
                // VisitListLiteralExpression (OP_MK_LIST, una LIST generica), ignorando il tipo
                // dichiarato: HASHSET restava senza dedup, STACK restava in ordine di
                // inserimento invece di LIFO — bug reale, ArraysPrintFormat.bas (PCodeVM).
                var declType = declaration.DataType ?? DeduceTypeFromSuffix(declaration.NameSuffixed.Lexeme);
                EmitCollectionAwareInitializer(declType, declaration.InitialValue!);
            }

            // 2. Risolve l'indice della variabile globale.
            if (!_globals.TryGetValue(declaration.NameCleaned.Lexeme, out var globalIndex))
            {
                throw new Exception(
                    $"Internal Compiler Error: Global variable '{declaration.NameSuffixed.Lexeme}' was not defined in PreCompilationSetup.");
            }

            // 3. Emette l'istruzione per salvare il valore nella variabile globale.
            EmitByte((byte)OpCode.OP_STORE_GLOBAL);
            EmitShort(globalIndex);
        }

        // 2026-09-21 (parità IL, bug GLOBAL-init): i GLOBAL dichiarati SENZA inizializzatore
        // restavano NIL (slot null) mentre IL li inizializza al default di tipo (LONG→0).
        // Causa: il main-body salta TUTTI i DeclarationStmt GLOBAL (riga ~810: sono registrati
        // qui solo se hanno init), e GetHoistedGlobalInitDecls raccoglie solo quelli CON init.
        // Qui si emette il default per gli scalari GLOBAL senza init (stringhe→"",
        // numerici/bit/date→0.0); array/UDT/classi restano null (fuori perimetro).
        // I default sono costanti senza effetti collaterali: vale per ogni riga di modulo.
        // Vale per PCode E VM/C (stesso Compiler, stessi chunk).
        if (ctx.MainModule != null)
        {
            var initedNames = new HashSet<string>(
                allGlobalDeclarations.Select(d => d.NameCleaned.Lexeme),
                StringComparer.OrdinalIgnoreCase);
            foreach (var stmts in ctx.MainModule.Statements.Values)
            {
                foreach (var stmt in stmts)
                {
                    if (stmt is not DeclarationStmt dsGlobal
                        || dsGlobal.Keyword.Kind != TokenKind.Global)
                        continue;
                    foreach (var declaration in dsGlobal.Declarations)
                    {
                        if (declaration.InitialValue != null
                            || declaration.Dimensions.Length != 0
                            || initedNames.Contains(declaration.NameCleaned.Lexeme))
                            continue;
                        var declType = declaration.DataType
                            ?? DeduceTypeFromSuffix(declaration.NameSuffixed.Lexeme);
                        if (declType is StringType) EmitConstant("");
                        else if (declType is SingleType or DoubleType or CurrencyType or BitType
                            or ByteType or SByteType or IntegerType or UIntegerType or LongType
                            or ULongType or QuadType or UQuadType or DateType or TimeType
                            or DateTimeType or TimeSpanType)
                            EmitConstant(0.0);
                        else continue;
                        if (!_globals.TryGetValue(declaration.NameCleaned.Lexeme, out var globalIndex))
                            continue;
                        EmitByte((byte)OpCode.OP_STORE_GLOBAL);
                        EmitShort(globalIndex);
                    }
                }
            }
        }
    }

    /// <summary>
    /// Compila il corpo principale del programma e tutte le subroutine numerate in modo sequenziale.
    /// </summary>
    private void CompileMainBodyAndSubroutines(ICompilationContext ctx)
    {
        // --- CORREZIONE: Aggiunto controllo di nullità ---
        if (ctx.MainModule == null)
        {
            // Se non c'è un modulo principale, non c'è corpo da compilare.
            // Imposta un indirizzo di default per __main per evitare crash successivi.
            _procedureAddresses["__main"] = 0;
            return;
        }

        _procedureAddresses["__main"] = _chunk.Code.Count;
        _locals.Clear(); // Pulisce i locali da compilazioni di procedure precedenti.
        _scopeDepth = 0; // Assicura che siamo nello scope globale.
        _isReachable = true;
        // NON chiamare BeginScope() qui.

        var executionOrder = ctx.MainModule.ExecutionOrder;
        // Se ExecutionOrder è vuoto, usa Statements ordinati come fallback
        if (executionOrder.IsDefaultOrEmpty)
        {
            executionOrder = ctx.MainModule.Statements.Keys.OrderBy(k => k).ToImmutableArray();
        }

        // Fusioni native "tutto il ciclo in un opcode": ammissibilità pre-scansionata sulla
        // lista PIATTA dei soli statement di codice (il lowering di un FOR può essere
        // distribuito su più line-number, quindi la vista per-lista dell'emissione sotto non
        // basta). Vedi Compiler.NativeLoopFusion.cs.
        var flatModuleStatements = new List<Stmt>();
        foreach (var lineNumberScan in executionOrder)
            if (ctx.MainModule.Statements.TryGetValue(lineNumberScan, out var lineStatementsScan))
                flatModuleStatements.AddRange(lineStatementsScan);
        PreScanWholeBodyNativeFusions(flatModuleStatements);

        // Etichette a livello modulo usate SOLO come target di ON ERROR GOTO/GOSUB (da
        // qualunque procedura del programma, non necessariamente da __main stesso): un blocco
        // così etichettato (es. `CallerHandler:` seguito da PRINT/RESUME) NON è codice di
        // avvio sequenziale — è un gestore d'errore raggiungibile solo per salto esplicito a
        // runtime. Senza questa esclusione, il ciclo sotto lo compilava (correttamente) come
        // parte del corpo sequenziale di __main, che il prologo dell'entry-point chiama SEMPRE
        // incondizionatamente all'avvio: il gestore veniva quindi eseguito una volta in più,
        // PRIMA che qualunque errore reale lo attivasse (bug: PCodeGpmOnErrorFramePropagation.bas
        // stampava "CALLER-HANDLER" 3 volte invece di 2, una in più a runtime zero).
        var onErrorTargetLabels = CollectOnErrorTargetLabelNames(ctx);
        var guardedBlockJumpOffset = -1;

        /* SUB Main ESISTE → il codice NUMERATO di modulo (righe 10/20/… legacy) NON viene
         * eseguito all'avvio: resta raggiungibile via GOTO/GOSUB (è il contratto dei
         * programmi "ibridi", vedi FunctionsMain.bas). Prima __main lo eseguiva sempre come
         * preambolo, quindi la riga 20 stampava "ERRORE: la riga 20 non doveva essere
         * eseguita" su PCode/VM-C (C99Native era già corretto). Le righe NON numerate
         * (chiavi sintetiche >= 1_000_000) restano preambolo ed eseguono PRIMA di Main
         * (DIM/assegnazioni top-level). Il blocco numerato viene emesso DOPO un RETURN,
         * così le etichette/GOSUB restano validi ma il flusso non ci cade dentro. */
        var hasMainProc = _procedureAddresses.ContainsKey("Main");
        var emitOrder = executionOrder;
        if (hasMainProc)
        {
            var synthetic = executionOrder.Where(k => k >= 1_000_000).ToImmutableArray();
            var legacy = executionOrder.Where(k => k < 1_000_000).ToImmutableArray();
            if (legacy.Length > 0)
                emitOrder = [..synthetic, ..legacy];
        }
        /* Il RETURN separatore va emesso appena inizia il codice numerato, ANCHE quando il
         * modulo non ha alcuna riga sintetica (programma "solo numerato" + SUB Main: __main
         * sarebbe tutto legacy e partirebbe come preambolo). */
        bool legacySeparatorPending = hasMainProc && emitOrder.Any(k => k < 1_000_000);

        foreach (var lineNumber in emitOrder)
        {
            if (legacySeparatorPending && lineNumber < 1_000_000)
            {
                /* Fine del codice di AVVIO di __main: il codice numerato legacy non deve
                 * proseguire (raggiungibile solo per salto esplicito). */
                EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_RETURN);
                legacySeparatorPending = false;
            }
            if (!ctx.MainModule.Statements.TryGetValue(lineNumber, out var statements))
                continue;

            if (statements.Count > 0 && statements[0] is LabelStmt lbl)
            {
                // Chiude l'eventuale blocco guardato precedente: il salto atterra qui, subito
                // prima di questa nuova etichetta (guardata o meno).
                if (guardedBlockJumpOffset >= 0)
                {
                    PatchJump(guardedBlockJumpOffset);
                    guardedBlockJumpOffset = -1;
                }
                if (onErrorTargetLabels.Contains(lbl.Label.Lexeme))
                    guardedBlockJumpOffset = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
            }

            _lineNumberAddresses[lineNumber] = _chunk.Code.Count;
            _isReachable = true;

            var hoistedDecls = GetHoistedGlobalInitDecls(ctx);
            foreach (var stmt in statements)
            {
                // ConstStmt NON è più skippato: scorre a VisitStatement -> case ConstStmt
                // (registrazione in _constants, fix %NOME twin-port GMBasicAI).
                if (stmt is DataStmt) continue;
                // GLOBAL: skippare INCONDIZIONATAMENTE lo statement (come faceva questo ramo
                // prima) perde l'inizializzatore quando la dichiarazione NON è stata hoistata
                // in __global_init — GetHoistedGlobalInitDecls ferma la scansione del prefisso
                // al primo statement non-dichiarazione (IMPORT, PRINT, ...), quindi
                // "IMPORT HTTP" seguito da "GLOBAL NextId AS LONG = 1" non hoista nulla e
                // l'istruzione sotto (850 old) saltava comunque il GLOBAL: l'init non girava
                // NEANCHE in __main → global RESTA NIL per tutto il programma (bug reale,
                // HttpRoutes.bas su PCode: CreateUser crasha con "Value cannot be null
                // (Parameter 'key')" su Users[NextId], e POST → 500). Ora lo statement GLOBAL
                // gira in linea SOLO se contiene almeno una dichiarazione con inizializzatore
                // non hoistata; i GLOBAL senza inizializzatore (solo registrazione, già fatta
                // da CompileDefinitions) restano skippati per non azzerare global già
                // assegnati da righe precedenti.
                if (stmt is DeclarationStmt { Keyword.Kind: TokenKind.Global } gGlobal
                    && gGlobal.Declarations.All(d => d.InitialValue is null || hoistedDecls.Contains(d)))
                    continue;
                // DIM top-level CON inizializzatore: già compilata in __global_init (vedi
                // CompileGlobalInitializers/GetHoistedGlobalInitDecls) — ricompilarla ANCHE
                // qui in linea la eseguirebbe DUE VOLTE (una in __global_init, prima di
                // __main; una qui, dentro __main). Per un letterale semplice (`DIM x = 5`)
                // è innocuo (idempotente), ma per un'espressione con EFFETTI COLLATERALI
                // (es. `DIM r = FETCH(url, opts)`, una vera chiamata di rete) la seconda
                // esecuzione può comportarsi diversamente dalla prima — bug reale: una
                // pagina BSP con `DIM r = FETCH(...)` a livello modulo eseguiva il FETCH due
                // volte, e la seconda sovrascriveva `pkgs` con un risultato vuoto/diverso,
                // pur avendo già stampato correttamente l'HTML della pagina. Salta l'intero
                // statement SOLO se OGNI sua dichiarazione con inizializzatore è già hoistata
                // (nessun rischio di perdere la registrazione di una dichiarazione NON
                // coperta, es. un'altra variabile senza inizializzatore nello stesso DIM).
                if (stmt is DeclarationStmt dsInit
                    && dsInit.Declarations.All(d => d.InitialValue != null && hoistedDecls.Contains(d)))
                    continue;
                VisitStatement(stmt);
            }
        }

        if (guardedBlockJumpOffset >= 0)
            PatchJump(guardedBlockJumpOffset);

        // NON chiamare EndScope() qui.

        // S_eval (EVAL/macro): trattiene i nomi delle locali top-level (__main) — DIM senza
        // inizializzatore e assegnazioni implicite vivono nel frame di __main come locali.
        if (_procedureAddresses.TryGetValue("__main", out var mainAddr))
            SnapshotProcedureLocalNames(mainAddr);

        // CRITICO: quando il prologo sintetico (CALL __global_init/CALL __static_init/CALL __main)
        // viene generato (righe ~283-330), __main è CHIAMATO con OP_CALL e deve tornare al prologo
        // con OP_RETURN. Senza questo RETURN il corpo top-level "cade" nel prologo (che chiama di
        // nuovo __global_init e __main) -> loop infinito -> il task non termina mai (TIMEOUT nei
        // test). Bug reale: QUALUNQUE programma top-level con `DIM x = 5` (variabile globale con
        // inizializzatore -> __global_init presente) restava in esecuzione per sempre.
        // (Quando l'entry point è __main direttamente — nessun __global_init/__static_init — il
        // RETURN è innocuo: il frame del task principale termina e l'OP_HALT finale non viene
        // raggiunto, ma il task è già Finished; il caso con prologo è l'unico dove conta.)
        if (_procedureAddresses.TryGetValue("__global_init", out _) ||
            _procedureAddresses.TryGetValue("__static_init", out _) ||
            _procedureAddresses.ContainsKey("Main") ||
            _procedureAddresses.Keys.Any(k => k.EndsWith(".__delete")))
        {
            EmitByte((byte)OpCode.OP_RETURN);
        }
        /* Subroutine dei predicati VALID/WHEN dichiarati a LIVELLO MODULO: senza questa
         * chiamata la constant dell'indirizzo restava 0 e il predicato non veniva mai
         * eseguito (P-Code: @GET WHEN al modulo non saltava il campo e restava in attesa). */
        EmitValidPredicateSubroutines();
        //LogDebug("[COMPILER] Corpo principale e subroutine numerate compilati.");
    }

    /// <summary>
    /// Raccoglie i nomi di TUTTE le etichette (non numeriche) referenziate da un'istruzione
    /// ON ERROR GOTO/GOSUB in QUALUNQUE punto del programma (corpi di procedura inclusi — il
    /// riferimento vive tipicamente dentro una SUB, l'etichetta target invece a livello
    /// modulo). Usato da <see cref="CompileMainBodyAndSubroutines"/> per riconoscere ed
    /// escludere dal flusso sequenziale di __main le etichette che sono puri gestori
    /// d'errore (raggiungibili solo per salto esplicito a runtime, mai da eseguire
    /// incondizionatamente all'avvio del programma).
    /// </summary>
    private static HashSet<string> CollectOnErrorTargetLabelNames(ICompilationContext ctx)
    {
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        // Ciclo-safe (bug 2026-09-16 FindExplicitCallTo/stack overflow): i nodi UDT hanno una
        // back-reference al genitore che NON è un figlio strutturale. AstReflectiveWalk.ChildNodes
        // la salta; il set dei visitati protegge da qualunque altro sottografo condiviso o ciclo.
        var visited = new HashSet<AstNode>(ReferenceEqualityComparer.Instance);

        void Walk(AstNode? node)
        {
            if (node == null) return;
            if (!visited.Add(node)) return;
            if (node is OnErrorStmt { Target: { } target } && target.Kind != TokenKind.Number)
                names.Add(target.Lexeme);

            foreach (var child in AstReflectiveWalk.ChildNodes(node))
                Walk(child);
        }

        if (ctx.MainModule != null)
            foreach (var stmts in ctx.MainModule.Statements.Values)
                foreach (var stmt in stmts)
                    Walk(stmt);

        foreach (var proc in ctx.GetAllProcedures())
        {
            if (proc.Body.IsDefault) continue;
            foreach (var stmt in proc.Body)
                Walk(stmt);
        }

        return names;
    }

    /// <summary>
    /// Esegue tutti i passaggi di patching finale (salti, costanti, etc.).
    /// </summary>
    private void PostCompilationPatching()
    {
        PatchAllJumps();
        //LogDebug("[COMPILER] Salti a etichette e procedure risolti.");

        PatchAbsoluteAddressTargets();

        PatchAllLineNumberJumps();
        //LogDebug("[COMPILER] Salti a numeri di riga risolti.");

        ValidateExplicitControlTransfers();

        PatchAllConstants();
        //LogDebug("[COMPILER] Riferimenti a codice nella constant pool risolti.");
    }

    /// <summary>
    /// Popola le mappe interne del compilatore per le procedure definite dall'utente
    /// e per le funzioni di libreria esterna (FFI).
    /// </summary>
    /// <param name="program">Il programma compilato contenente tutti i simboli.</param>
    private void PopulateProcedureAndFfiMaps(ICompilationContext ctx)
    {
        foreach (var proc in ctx.GetAllProcedures())
        {
            if (proc is ExternalProcedureDefinition epd)
            {
                var key = (epd.OriginalName.Lexeme, epd.Parameters.Length);
                if (!_ffiFunctions.TryAdd(key, epd))
                {
                    continue;
                }
                AddConstant(epd.LibraryName.Literal as string);
                if (epd.AliasName is not null) AddConstant(epd.AliasName.Literal as string);
            }
            else
            {
                var procName = proc.Name.Lexeme;
                var regularKey = (procName, proc.Parameters.Length);
                // Un utente può definire una propria procedura con lo stesso nome (e stessa
                // arità) di un builtin (es. `GENERATOR Count(n) ... END GENERATOR` collide col
                // builtin COUNT, TokenKind.Count → BuiltInFunction.COUNT). L'ordine di iterazione
                // di ctx.GetAllProcedures() non è garantito rispetto a user-vs-native, quindi
                // un'assegnazione incondizionata qui può lasciare che il NativeProcedureDefinition
                // sovrascriva quello dell'utente (osservato con FunctionsYield.bas: NEXT(Count(3))
                // riceveva il risultato del builtin COUNT invece di un generatore) — il codice
                // utente deve sempre avere la precedenza (shadowing), mai il builtin.
                if (!_procedureMap.TryGetValue(regularKey, out var existingProcForKey) ||
                    proc is not NativeProcedureDefinition || existingProcForKey is NativeProcedureDefinition)
                {
                    _procedureMap[regularKey] = proc;
                }
                if (proc is FunctionDefinition fd) { var ok = (fd.OriginalName.Lexeme, proc.Parameters.Length); if (!_procedureMap.ContainsKey(ok)) _procedureMap[ok] = proc; }
                if (proc.Parent is UserDefinedType udtP) { var fk = (udtP.Name.Lexeme + "." + procName, proc.Parameters.Length); if (!_procedureMap.ContainsKey(fk)) _procedureMap[fk] = proc; }
                else if (proc.Parent is ClassDefinition classP) { var fk = (classP.Name.Lexeme + "." + procName, proc.Parameters.Length); if (!_procedureMap.ContainsKey(fk)) _procedureMap[fk] = proc; }
                // Also register with full name for UDT/CLASS methods/operators
                if (proc.Parent is UserDefinedType udtParent)
                {
                    var fullKey = (udtParent.Name.Lexeme + "." + procName, proc.Parameters.Length);
                    if (!_procedureMap.ContainsKey(fullKey))
                        _procedureMap[fullKey] = proc;
                }
                else if (proc.Parent is ClassDefinition classParent)
                {
                    var fullKey = (classParent.Name.Lexeme + "." + procName, proc.Parameters.Length);
                    if (!_procedureMap.ContainsKey(fullKey))
                        _procedureMap[fullKey] = proc;
                }
            }
        }

        // Aggiunge anche le specializzazioni di funzioni generiche
        foreach (var spec in ctx.ResolvedSpecializations.Values)
        {
            var specKey = (spec.Name.Lexeme, spec.Parameters.Length);
            if (!_procedureMap.ContainsKey(specKey))
            {
                _procedureMap[specKey] = spec;
            }
        }
    }

    private void RegisterSystemTypeOffsets()
    {
        //LogDebug("[COMPILER] Registrazione offset per i tipi di sistema...");
        _fieldOffsets["DISPLAYMODE.Active"] = 0;
        _fieldOffsets["DISPLAYMODE.ModeID"] = 1;
        _fieldOffsets["DISPLAYMODE.Width"] = 2;
        _fieldOffsets["DISPLAYMODE.Height"] = 3;
        _fieldOffsets["DISPLAYMODE.BPP"] = 4;
        _fieldOffsets["DISPLAYMODE.Colors"] = 5;
        _fieldOffsets["DISPLAYMODE.Description"] = 6;
    }

    /// <summary>
    /// Popola il "data pool" del chunk P-Code con tutti i valori delle istruzioni DATA
    /// aggregati da tutti i moduli del programma.
    /// </summary>
    /// <param name="ctx">Il contesto di compilazione con i moduli caricati.</param>
    private void PopulateDataPool(ICompilationContext ctx)
    {
        // Raccoglie i valori DATA direttamente dai DataStmt dei moduli (procedure E top-level),
        // nell'ordine dei numeri di riga (coerenza di RESTORE). Il dizionario
        // DataStoresByModule di BasicProgram non viene popolato da nessun passaggio: raccogliere
        // qui è il percorso reale che rende OP_READ/RESTORE funzionanti.
        foreach (var module in ctx.LoadedModules.Values)
        {
            var dataStmts = new List<DataStmt>();

            // Top-level: module.Statements è {riga → List<Stmt>}, include i DataStmt di livello 0.
            foreach (var lineNum in module.ExecutionOrder)
            {
                if (module.Statements.TryGetValue(lineNum, out var stmts))
                    dataStmts.AddRange(stmts.OfType<DataStmt>());
            }

            // Procedure (dati DATA dentro le procedure: l'ordine di definizione è conservato
            // da GetAllProcedures; i DATA di procedura seguono quelli top-level).
            dataStmts.AddRange(ctx.GetAllProcedures().SelectMany(p => p.Body.OfType<DataStmt>()));

            foreach (var dataStmt in dataStmts)
            {
                // Riga del DataStmt → indice del primo valore nel pool (per RESTORE <riga|etichetta>).
                _dataLineToIndex[dataStmt.Token.Position.Line] = _chunk.Data.Count;
                foreach (var valueExpr in dataStmt.Values)
                {
                    object? dataValue = null;
                    if (valueExpr is NumberExpr n) dataValue = n.Value;
                    else if (valueExpr is StringExpr s) dataValue = s.Value;
                    else if (valueExpr is BooleanExpr b) dataValue = b.Value ? -1.0 : 0.0;
                    // NilExpr / valore non costante → NIL (gap documentato: DATA deve essere costante).
                    _chunk.Data.Add(dataValue);
                }
            }
        }

        //LogDebug("[COMPILER] Data pool popolata con i valori DATA.");
    }

    /// <summary>
    /// Alloca slot per tutte le variabili globali esplicite, registrando
    /// correttamente il loro tipo di dato.
    /// </summary>
    private void DefineGlobalVariables(ICompilationContext ctx)
    {
        foreach (var decl in ctx.GetAllGlobalVariables())
        {
            if (!_globals.ContainsKey(decl.NameCleaned.Lexeme))
            {
                var type = decl.DataType ?? DeduceTypeFromSuffix(decl.NameSuffixed.Lexeme);
                DefineGlobal(decl.NameCleaned.Lexeme, type);
            }
        }
    }

    /// <summary>
    /// Itera su tutti i tipi definiti dall'utente (UDT) nel programma e
    /// li compila in un formato intermedio per il runtime.
    /// </summary>
    /// <param name="program">Il programma compilato che contiene tutti i tipi definiti.</param>
    private void CompileUserDefinedTypes(ICompilationContext ctx)
    {
        var allUserDefinedTypes = ctx.GetAllUserDefinedTypes();
        foreach (var udt in allUserDefinedTypes)
        {
            VisitUserDefinedType(udt);
        }
        //LogDebug("[COMPILER] Descrittori UDT calcolati.");
    }

    /// <summary>
    /// Itera su tutte le classi definite nel programma e le compila
    /// in un formato intermedio per il runtime.
    /// </summary>
    /// <param name="program">Il programma compilato che contiene tutte le definizioni di classe.</param>
    private void CompileClasses(ICompilationContext ctx)
    {
        var allClassDefinitions = ctx.GetAllClasses();
        foreach (var classDef in allClassDefinitions)
        {
            VisitClassDefinition(classDef);
        }
        //LogDebug("[COMPILER] Descrittori di Classe e membri compilati.");
    }

    /// <summary>
    /// Compila tutte le procedure definite dall'utente (SUB, FUNCTION, etc.).
    /// </summary>
    /// <param name="program">Il programma compilato che contiene tutte le definizioni di procedura.</param>
    private void CompileProcedures(ICompilationContext ctx)
    {
        // ctx.GetAllProcedures() e la rilettura manuale di ctx.MainModule.Declarations sotto
        // restituiscono, per un programma a file singolo, LE STESSE istanze ProcedureDefinition
        // per ogni SUB/FUNCTION dichiarata nel modulo principale — la seconda rilettura è quindi
        // ridondante e finora ogni SUB/FUNCTION del genere veniva compilata due volte (bytecode
        // duplicato). Per `SUB Main` questo significava che l'intero programma veniva anche
        // ESEGUITO due volte come task indipendenti (la VM avvia solo l'entry point ufficiale, ma
        // essendo comunque presente nel bytecode finale un secondo `HTTP SERVER porta` reale
        // veniva eseguito e falliva con "Address already in use"), e un riferimento a procedura
        // usato come callback (`HTTP USE AuthMiddleware()`) compilato durante la copia SBAGLIATA
        // di `Main` poteva risolvere l'indirizzo del callback alla copia SBAGLIATA — causa di un
        // auth-bypass osservato in un middleware che non chiamava mai `HTTP USE NEXT`.
        //
        // La chiave di deduplicazione corretta NON è proc.Name.Lexeme: per una SUB (rappresentata
        // internamente come FunctionDefinition), Name.Lexeme è la stringa letterale "SUB" (non il
        // nome dichiarato — perché, non chiaro, ma verificato empiricamente con diagnostica
        // dedicata), IDENTICA per ogni SUB del programma — deduplicare su questa chiave confonde
        // quindi Main/ClientTest/ShowToken come se fossero la stessa procedura, lasciandone
        // compilate solo una (un primo tentativo di fix con questa chiave sbagliata ha causato una
        // regressione: hang totale, corpo reale mai compilato per le SUB "perdenti"). Il nome
        // VERO, sia per le SUB (dove differisce da Name) sia per le FUNCTION/builtin (dove
        // coincide comunque con Name, quindi la stessa chiave funziona per entrambi i casi) è
        // OriginalName.Lexeme — la stessa chiave che il codice sotto usa già per popolare
        // _procedureAddresses nel caso FunctionDefinition (motivo per cui i lookup a runtime per
        // nome hanno sempre funzionato anche con il bytecode duplicato: la seconda scrittura in
        // quel dizionario, sotto OriginalName, sovrascrive sempre la prima). Porting da GMBasicAI
        // (commit 7736047), verificato lì con HttpRoutes.bas prima di riportare qui.
        var allProcedures = ctx.GetAllProcedures();
        var userProcs = new List<ProcedureDefinition>(allProcedures);
        if (ctx.MainModule != null)
        {
            foreach (var decl in ctx.MainModule.Declarations)
                if (decl is ProcedureDefinition pd)
                    userProcs.Add(pd);
        }
        var lastIndexByName = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < userProcs.Count; i++)
            lastIndexByName[userProcs[i].OriginalName.Lexeme] = i;
        // 2026-09-24 (overload): dedup per SymbolId (lo stesso nodo può comparire due
        // volte via MainModule) — MAI per nome: gli overload omonimi si compilano TUTTI,
        // prima solo l'ultimo veniva emesso e ogni chiamata lo eseguiva.
        var seenProcIds = new HashSet<int>();

        // Accumula le etichette di tutte le procedure per il patching finale
        var allLabels = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

        for (var procIndex = 0; procIndex < userProcs.Count; procIndex++)
        {
            if (userProcs[procIndex].SymbolId >= 0)
            {
                if (!seenProcIds.Add(userProcs[procIndex].SymbolId)) continue;
            }
            else if (lastIndexByName[userProcs[procIndex].OriginalName.Lexeme] != procIndex) continue;
            var proc = userProcs[procIndex];
            _currentFunctionReturnType = (proc as FunctionDefinition)?.ReturnType;
            _currentProcedureName = proc.OriginalName.Lexeme;
            var procStartAddr = _chunk.Code.Count;
            // proc.Name.Lexeme per una SUB (rappresentata internamente come FunctionDefinition)
            // è la stringa letterale "SUB", NON il nome dichiarato (vedi commento sopra) — usare
            // sempre OriginalName.Lexeme (per le FUNCTION coincide comunque con Name, nessuna
            // differenza di comportamento lì). Prima questa riga scriveva la chiave letterale
            // "SUB" in _procedureAddresses, sovrascritta a ogni SUB compilata: l'ultima SUB del
            // chunk restava agganciata sotto quella chiave spuria, che ChunkMerger.Merge (nomi
            // in comune tra chunk indipendenti) poteva far collidere con la stessa chiave
            // spuria di un chunk fuso, rediretto verso l'indirizzo sbagliato (bug: chunk fuso
            // eseguiva una procedura diversa da Main pur partendo dal proprio EntryPoint).
            _procedureAddresses[proc.OriginalName.Lexeme] = procStartAddr;
            // 2026-09-24 (overload): chiave con arità della definizione ("Nome#k") per il
            // patching dei call-site — la chiave nuda resta (ultimo vince) per compatibilità
            // con tutti i siti non convertiti (GOSUB, metodi, GO FUNCTION...).
            _procedureAddresses[$"{proc.OriginalName.Lexeme}#{proc.Parameters.Length}"] = procStartAddr;
            // 2026-09-24 (namespace): chiave per-Simbolo — il chunk non conserva il
            // namespace, quindi due omonime in namespace diversi collidono sul nome:
            // l'indirizzo per-Simbolo le disambigua (CallPatchToken porta il SymbolId).
            if (proc.SymbolId >= 0) _procedureAddresses[$"@sym{proc.SymbolId}"] = procStartAddr;
            _procedureArities[_chunk.Code.Count] = proc.Parameters.Length;
            _locals.Clear();
            _scopeDepth = 0;
            _isReachable = true;
            _bodyLocalsBoundary = -1;
            _currentProcHasGosub = false;

            // Le etichette sono scope locali a ciascuna procedura.
            // Resetta per evitare conflitti tra procedure diverse.
            _labels.Clear();

            // Return-slot: riserva un local col nome della FUNCTION stessa (stesso schema di
            // CompileProcedureBody(ProcedureDefinition) sotto, e del nativo C — vedi
            // NativeCompiler-Recursion-SelfCallShadowedByReturnSlot.md — cc_local_add(c,
            // s->func_name)). Senza, un'assegnazione bare `NomeFunzione = espr` (dopo il fix
            // del parser che non la tratta più come ReturnStmt esplicito, vedi
            // ParseFunctionReturnAssignmentStatement) non trova nessun local con quel nome
            // (ResolveLocal fallisce) e cade nel ramo TryResolveOrRegisterGlobal:
            // l'assegnazione scrive in una variabile GLOBALE fantasma invece di aggiornare
            // ReturnValue via OP_STORE_LOCAL — la funzione, arrivata a fine corpo senza un
            // RETURN esplicito, restituiva quindi un valore stantio invece di quello assegnato
            // (bug: `FUNCTION Quadrato(N): Quadrato = N * N: END FUNCTION` restituiva N invece
            // di N*N sul motore PCode-managed).
            if (proc is FunctionDefinition fdSelf)
                AddLocal(fdSelf.OriginalName, _currentFunctionReturnType ?? new InferredType());

            BeginScope();
            foreach (var param in proc.Parameters)
            {
                AddLocal(param.Name, param.DataType);
            }
            
            // Pop degli argomenti dalla stack nei locali (in ordine inverso)
            for (int i = proc.Parameters.Length - 1; i >= 0; i--)
            {
                var localIdx = ResolveLocal(proc.Parameters[i].Name.Lexeme);
                if (localIdx != -1)
                    EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx);
            }

            // Clear ReturnValue after parameter stores — parameter STORE_LOCAL
            // would have set it, which would override the actual function return.
            EmitByte((byte)OpCode.OP_CLEAR_RETURN);
            // Default del return-slot numerico: una FUNCTION AS INTEGER che esce con
            // EXIT FUNCTION (o a fine corpo) senza assegnare il nome deve restituire 0,
            // non NIL. Il local nasce non inizializzato; senza questo il chiamante riceve
            // NIL e `r = F()` lascia r non numerico (PRINT stampa vuoto).
            if (proc is FunctionDefinition fdRet && IsNumericScalarElement(fdRet.ReturnType))
            {
                var retSlotInit = ResolveLocal(fdRet.OriginalName.Lexeme);
                if (retSlotInit != -1)
                {
                    EmitConstant(0.0);
                    EmitByte((byte)OpCode.OP_STORE_LOCAL);
                    EmitShort((ushort)retSlotInit);
                }
            }

            // Da qui in poi ogni AddLocal è un local dichiarato nel BODY (non return-slot, non
            // parametro) — vedi EmitFinalizeBodyLocals/_bodyLocalsBoundary (BUG-6).
            _bodyLocalsBoundary = _locals.Count;

            // Fusioni native "tutto il ciclo in un opcode": pre-scan del corpo della procedura
            // (vedi Compiler.NativeLoopFusion.cs).
            PreScanWholeBodyNativeFusions(proc.Body);

            foreach (var stmt in proc.Body)
            {
                // ConstStmt NON è più skippato: scorre a VisitStatement -> case ConstStmt
                // (registrazione in _constants, fix %NOME twin-port GMBasicAI).
                if (stmt is DataStmt) continue;
                if (stmt is DeclarationStmt ds && ds.Keyword.Kind != TokenKind.Global)
                {
                    bool needsInit = false;
                    foreach (var decl in ds.Declarations)
                    {
                        // UserDefinedType manca qui era il bug: senza inizializzatore esplicito,
                        // un locale UDT veniva registrato (AddLocal) ma MAI visitato
                        // (VisitDeclarationStatement, che spinge OP_NEW_UDT_INSTANCE e lo
                        // salva nello slot, non veniva mai chiamato) — lo slot restava a
                        // null per tutta la vita della SUB/FUNCTION. `P.X = 42` (OP_STORE_MEMBER
                        // su un obj null) e la lettura successiva fallivano entrambe in
                        // silenzio (vedi 02-Knowledge/GMBasic-PCODE-UDT-Field-Access-Bug.md).
                        // ListType/DictType/ecc. mancavano qui per lo stesso identico motivo di
                        // UserDefinedType sopra: `DIM x AS LIST OF T` (nessun inizializzatore)
                        // prendeva la scorciatoia !needsInit sotto — OP_PUSH_NIL invece di
                        // OP_NEW (List<object?> vuota, vedi VisitDeclarationStatement) — x
                        // restava null per tutta la vita della SUB. Un x.Add(...) su null non
                        // lanciava (dispatch silenzioso), ma se lo stesso local slot era stato
                        // in precedenza occupato da un altro valore reale (es. il temp __arr di
                        // un FOR EACH su array eseguito prima nello stesso scope), x.Add finiva
                        // per operare su QUELL'oggetto — un vero array a dimensione fissa,
                        // "Collection was of a fixed size" (bug scoperto con
                        // ForEachComprehensive.bas: LIST OF Person dopo un FOR EACH su array UDT).
                        // TUTTE le collezioni: mancavano TreeType/AVL/RedBlack/BIT/Trie/Heap/
                        // Bag/Cache/Graph/Timeline/HashRing/RingBuffer/SparseSet/BPlusTree →
                        // `DIM T AS TREE<...>` in una SUB prendeva la scorciatoia !needsInit
                        // (AddLocal + continue) → VisitStatement MAI chiamato → nessun OP_NEW →
                        // T restava undefined. Stesso bug già fixato per List/Dict, incompleto.
                        if (decl.DataType is Basic.WaitGroupType or Basic.ChannelType or Basic.MutexType or Basic.RwMutexType or Basic.OnceType or Basic.AtomicType or Basic.PoolType or Basic.CondType or Basic.ConcurrentMapType or ArrayType or UserDefinedType
                            or ListType or DictType or HashSetType or QueueType or StackType or LinkedListType
                            or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType or BinaryIndexedTreeType
                            or TrieType or CircularLinkedListType or BinaryHeapType or BagType or CacheType or
                            GraphType or TimelineType or HashRingType or RingBufferType or SparseSetType or BPlusTreeType
                            or StringBuilderType or RegexType or HyperLogLogType)
                        { needsInit = true; break; }
                        if (decl.InitialValue != null)
                        { needsInit = true; break; }
                    }
                    if (!needsInit)
                    {
                        // Register variables in locals even without initialization
                        foreach (var decl in ds.Declarations)
                        {
                            var dt = decl.DataType;
                            if (dt is not null and not InferredType)
                            {
                                AddLocal(decl.NameCleaned, dt);
                                // Push default value for numeric/date types
                                if (dt is SingleType or DoubleType or CurrencyType or IntegerType or LongType or ByteType or BitType or QuadType or DateType or TimeType or DateTimeType)
                                    EmitConstant(0.0);
                                else
                                    EmitByte((byte)OpCode.OP_PUSH_NIL);
                                var li = ResolveLocal(decl.NameCleaned.Lexeme);
                                if (li != -1) EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)li);
                            }
                            else
                            {
                                AddLocal(decl.NameCleaned, dt ?? new InferredType());
                            }
                        }
                        continue;
                    }
                }
                VisitStatement(stmt);
            }

            // Subroutine dei predicati VALID di QUESTA procedura: emesse dopo il body
            // (il flusso non ci arriva mai — il body termina con OP_RETURN).
            EmitValidPredicateSubroutines();

            // S_eval (EVAL/macro): trattiene i nomi delle locali di procedura (sopravvissute:
            // blocco-locals già rimosse da EndScope) sotto l'indirizzo di inizio procedura.
            SnapshotProcedureLocalNames(procStartAddr);

            // Accumula le etichette di questa procedura per il patching finale
            foreach (var (k, v) in _labels)
            {
                allLabels[k] = v;
            }
        }

        // Ripristina tutte le etichette accumulate per il patching finale
        foreach (var (k, v) in allLabels)
        {
            _labels[k] = v;
        }
    }

    /// <summary>
    /// Compila il corpo di una procedura (SUB, FUNCTION, etc.) definito da una lista di statement.
    /// Questo è l'overload principale usato dalle lambda a blocco.
    /// </summary>
    public CompiledChunk CompileProcedureBody(ImmutableArray<Parameter> parameters, ImmutableArray<Stmt> body)
    {
        BeginScope();
        foreach (var param in parameters)
        {
            AddLocal(param.Name, param.DataType);
        }

        // Fusioni native "tutto il ciclo in un opcode" (lambda/LOCAL SUB/FUNCTION a blocco):
        // pre-scan del corpo. Vedi Compiler.NativeLoopFusion.cs.
        PreScanWholeBodyNativeFusions(body);

        foreach (var stmt in body)
        {
            VisitStatement(stmt);
        }

        // Se il flusso di codice è ancora raggiungibile, aggiungi un RETURN implicito.
        if (_isReachable)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_RETURN);
        }

        EndScope();
        _chunk.LocalCount = _maximumLocalCount;
        // Chunk figlio (lambda, LOCAL SUB/FUNCTION, macro): snapshot relativo a 0, il genitore
        // trasla all'indirizzo reale in fase di merge (MergeChildLocalNames). Per la macro è
        // metadata inutilizzato (la VM annidata usa i propri slot via opcode) ma innocuo.
        SnapshotProcedureLocalNames(0);
        return _chunk;
    }

    /// <summary>
    /// Overload per compilare una ProcedureDefinition completa.
    /// </summary>
    public CompiledChunk CompileProcedureBody(ProcedureDefinition proc)
    {
        _currentFunctionReturnType = (proc as FunctionDefinition)?.ReturnType;
        _currentProcedureName = proc.OriginalName.Lexeme;
        if (proc is FunctionDefinition)
            AddLocal(proc.OriginalName, _currentFunctionReturnType ?? new InferredType());
        return CompileProcedureBody(proc.Parameters, proc.Body);
    }

    /// <summary>
    /// Compila un corpo di funzione definito da una singola espressione (es. lambda `=>`).
    /// </summary>
    public CompiledChunk CompileExpressionBody(ImmutableArray<Parameter> parameters, Expr body)
    {
        BeginScope();
        foreach (var param in parameters)
        {
            AddLocal(param.Name, param.DataType);
        }

        // Pop degli argomenti dalla stack nei locali (in ordine inverso)
        for (int i = parameters.Length - 1; i >= 0; i--)
        {
            var localIdx = ResolveLocal(parameters[i].Name.Lexeme);
            if (localIdx != -1)
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx);
        }
        // OP_CLEAR_RETURN dopo i parametri per evitare ReturnValue stantio
        EmitByte((byte)OpCode.OP_CLEAR_RETURN);
        EmitByte((byte)OpCode.OP_CLEAR_RETURN);

        // Compila l'espressione...
        VisitExpression(body);
        // ...e usa il suo risultato per l'istruzione di ritorno.
        EmitByte((byte)OpCode.OP_RETURN);

        EndScope();
        _chunk.LocalCount = _maximumLocalCount;
        // Come sopra (corpo lambda `=>` a singola espressione): snapshot relativo a 0.
        SnapshotProcedureLocalNames(0);
        return _chunk;
    }

    /// <summary>
    /// Comparer personalizzato per la chiave della mappa delle procedure,
    /// per garantire una ricerca case-insensitive solo sul nome della procedura.
    /// </summary>
    private class ProcedureKeyComparer : IEqualityComparer<(string, int)>
    {
        public bool Equals((string, int) x, (string, int) y)
        {
            return x.Item1.Equals(y.Item1, StringComparison.OrdinalIgnoreCase) && x.Item2 == y.Item2;
        }

        public int GetHashCode((string, int) obj)
        {
            return StringComparer.OrdinalIgnoreCase.GetHashCode(obj.Item1) ^ obj.Item2.GetHashCode();
        }
    }

    /// <summary>
    /// Scatta i nomi delle locali di procedura sopravvissute a questo punto della compilazione
    /// (indice array = slot; null dove lo slot non ha nome noto) in
    /// <see cref="CompiledChunk.ProcedureLocalNames"/> sotto <paramref name="procStartAddr"/>.
    /// Chiamato a fine corpo procedura: le locali di BLOCCO sono già state rimosse da EndScope
    /// (slot riusati), quindi lo snapshot contiene esattamente i nomi a livello procedura
    /// (return-slot, parametri, DIM di procedura) con gli slot definitivi — le uniche risolvibili
    /// staticamente per EVAL (semantica S_eval; le locali di blocco restano fase 2).
    /// I nomi sono normalizzati (senza suffisso tipo), confronto a runtime OrdinalIgnoreCase.
    /// </summary>
    private void SnapshotProcedureLocalNames(int procStartAddr)
    {
        var names = new string?[_locals.Count];
        var numeric = new bool[_locals.Count];
        for (var i = 0; i < _locals.Count; i++)
        {
            var lexeme = _locals[i].Name.Lexeme;
            names[i] = string.IsNullOrEmpty(lexeme) ? null : CompiledChunk.NormalizeLocalName(lexeme);
            numeric[i] = _locals[i].NumericDefault ?? IsNumericDefaultType(_locals[i].Type);
        }
        _chunk.ProcedureLocalNames[procStartAddr] = names;
        _chunk.ProcedureLocalNumeric[procStartAddr] = numeric;
    }

    /// <summary>Il default di una variabile di questo tipo è 0 (numerico)? Per STRING/CLASS/
    /// UDT/ANY/collezioni il default resta null. Vedi ProcedureLocalNumeric.</summary>
    internal static bool IsNumericDefaultType(DataType? type) => type switch
    {
        SingleType or DoubleType or CurrencyType or BitType or ByteType or SByteType
            or IntegerType or UIntegerType or LongType or ULongType or QuadType or UQuadType => true,
        _ => false
    };

    /// <summary>
    /// Cattura le locali VIVE al SITO macro (EVAL/&) sotto l'indirizzo corrente del chunk —
    /// incluse quelle di BLOCCO IF/FOR (che a fine procedura non esistono più: rimosse da
    /// EndScope). Chiamato subito DOPO l'emissione dell'opcode di eval: il runtime esegue la
    /// macro con task.Ip che punta al byte successivo, quindi la chiave è l'ip corrente.
    /// Il provider runtime sceglie per binary search lo snapshot più vicino ≤ Ip: per i siti
    /// macro vince questo (più preciso), per il resto resta lo snapshot di fine procedura.
    /// </summary>
    private void RegisterMacroSiteLocalNames()
    {
        var names = new string?[_locals.Count];
        for (var i = 0; i < _locals.Count; i++)
        {
            var lexeme = _locals[i].Name.Lexeme;
            names[i] = string.IsNullOrEmpty(lexeme) ? null : CompiledChunk.NormalizeLocalName(lexeme);
        }
        _chunk.ProcedureLocalNames[_chunk.Code.Count] = names;
    }

    /// <summary>
    /// Inizia un nuovo blocco di scope (es. entrando in una procedura o un blocco IF).
    /// </summary>
    private void BeginScope() => _scopeDepth++;

    /// <summary>
    /// Termina il blocco di scope corrente, emettendo istruzioni OP_POP per
    /// rimuovere le variabili locali create in quello scope, ma solo se
    /// il codice è raggiungibile.
    /// </summary>
    private void EndScope()
    {
        _scopeDepth--;

        // Chiude le locali dello scope che termina. Le locali IMPLICITE "promosse" (Promoted: create in questo blocco ma di
        // competenza della procedura) sopravvivono: restano al loro posto (gli indici emessi non cambiano) e si continua a
        // scendere per chiudere le locali di blocco che sono rimaste SOTTO di loro — se non in coda, vengono finalizzate e rese
        // irrisolvibili (sentinella) invece di rimosse. Si ferma alla prima locale di uno scope esterno.
        for (var idx = _locals.Count - 1; idx >= 0; idx--)
        {
            var l = _locals[idx];
            if (l.Depth <= _scopeDepth)
            {
                if (l.Promoted) continue;
                break;
            }

            if (idx < 65536 && !l.AlreadyFinalized)
            {
                EmitByte((byte)OpCode.OP_FINALIZE_SCOPE);
                EmitShort((ushort)idx);
            }

            if (idx == _locals.Count - 1)
                _locals.RemoveAt(idx);
            else
            {
                l.Name = new Token(TokenKind.Identifier, "__scope_removed_" + idx, null, SourcePosition.Unknown);
                l.Depth = 0;
                l.AlreadyFinalized = true;
            }
        }

        // Non emettiamo OP_POP: i frameLocals sono gestiti dal frame,
        // lo stack globale e' gestito da OP_CALL_MEMBER/OP_NEW_CLASS_INSTANCE/OP_RETURN
    }

    /// <summary>
    /// Emette OP_FINALIZE_SCOPE per ogni local dichiarato nel BODY della procedura standalone
    /// corrente (indice &gt;= <see cref="_bodyLocalsBoundary"/>), in ordine inverso di
    /// dichiarazione — SENZA toccare <see cref="_locals"/>/<see cref="_scopeDepth"/> (a
    /// differenza di <see cref="EndScope"/>): qui la procedura sta terminando per davvero
    /// (END SUB/FUNCTION raggiunto), quindi non serve più tracciare lo scope per statement
    /// successivi. Esclude deliberatamente il return-slot e i PARAMETRI (indici &lt;
    /// _bodyLocalsBoundary): i parametri sono riferimenti dell'istanza del CHIAMANTE (passaggio
    /// per riferimento per gli oggetti), finalizzarli distruggerebbe un'istanza che il chiamante
    /// potrebbe ancora usare — BUG-6 riguarda solo le istanze CREATE dentro la SUB/FUNCTION.
    /// </summary>
    private void EmitFinalizeBodyLocals()
    {
        for (var i = _locals.Count - 1; i >= _bodyLocalsBoundary; i--)
        {
            if (i < 65536 && !_locals[i].AlreadyFinalized)
            {
                EmitByte((byte)OpCode.OP_FINALIZE_SCOPE);
                EmitShort((ushort)i);
            }
        }
    }

    private void EmitGlobalDestructorFinalizers()
    {
        for (int i = _globalInfos.Count - 1; i >= 0; i--)
        {
            EmitByte((byte)OpCode.OP_FINALIZE_GLOBAL);
            EmitShort(i);
        }
    }

    /// <summary>
    /// Metodo da chiamare alla fine della compilazione per risolvere gli indirizzi
    /// lasciati come placeholder nella tabella delle costanti (usato da ON KEY).
    /// </summary>
    private void PatchAllConstants()
    {
        foreach (var (constIndex, targetToken) in _constantsToPatch)
        {
            int targetAddress;

            if (_labels.TryGetValue(targetToken.Lexeme, out var labelAddress))
            {
                targetAddress = labelAddress;
            }
            else if (_procedureAddresses.TryGetValue(targetToken.Lexeme, out var procAddress))
            {
                targetAddress = procAddress;
            }
            else
            {
                targetAddress = 0;
            }

            _chunk.Constants[constIndex] = (double)targetAddress;
        }
        
        // Patch VTable indirizzi nei class descriptor
        foreach (var (constIndex, _) in _classDescriptorsToPatch)
        {
            if (_chunk.Constants[constIndex] is not Dictionary<string, object?> descriptor) continue;
            
            // Patch VTable (metodi)
            if (descriptor.TryGetValue("VTable", out var vtObj) &&
                vtObj is Dictionary<string, string> vTable)
            {
                var patched = new Dictionary<string, object?>();
                foreach (var entry in vTable)
                {
                    if (_procedureAddresses.TryGetValue(entry.Value, out var addr))
                        patched[entry.Key] = addr;
                }
                descriptor["VTable"] = patched;
            }
            
            // Patch Constructors (lista nomi → indirizzi)
            if (descriptor.TryGetValue("Constructors", out var ctObj) &&
                ctObj is System.Collections.IList ctorList)
            {
                var patched = new List<int>();
                foreach (var item in ctorList)
                {
    if (item is string ctorName && _procedureAddresses.TryGetValue(ctorName, out var addr))
                        patched.Add(addr);
                    else if (item is int intAddr)
                        patched.Add(intAddr);
                    else if (item is double dAddr)
                        patched.Add((int)dAddr);
                }
                descriptor["Constructors"] = patched;
            }
            
            // Patch Destructor (nome → indirizzo)
            if (descriptor.TryGetValue("Destructor", out var dtObj) && dtObj is string dtorName &&
                _procedureAddresses.TryGetValue(dtorName, out var dtorAddr))
            {
                descriptor["Destructor"] = dtorAddr;
            }
            if (descriptor.TryGetValue("PropertyGetters", out var pgObj) && pgObj is Dictionary<string, string> propGetters)
            {
                var patched = new Dictionary<string, object?>();
                foreach (var entry in propGetters)
                    if (_procedureAddresses.TryGetValue(entry.Value, out var addr))
                        patched[entry.Key] = addr;
                descriptor["PropertyGetters"] = patched;
            }
            if (descriptor.TryGetValue("PropertySetters", out var psObj) && psObj is Dictionary<string, string> propSetters)
            {
                var patched = new Dictionary<string, object?>();
                foreach (var entry in propSetters)
                    if (_procedureAddresses.TryGetValue(entry.Value, out var addr))
                        patched[entry.Key] = addr;
                descriptor["PropertySetters"] = patched;
            }
        }
    }

    /// <summary>
    /// Metodo helper per il logging condizionale durante la compilazione.
    /// </summary>
    /// <param name="message">Il messaggio da loggare.</param>
    private static void LogDebug(string message)
    {
        if (VerboseCompiler)
            Console.WriteLine(message);
    }
}
