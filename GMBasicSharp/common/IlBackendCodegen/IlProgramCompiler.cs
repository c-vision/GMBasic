// FILE: Basic.IlBackend.IlProgramCompiler.cs

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>
/// Backend che attraversa l'AST già risolto dal Linker (lo stesso input che consuma
/// oggi il Compiler p-code) e genera IL .NET reale tramite
/// <see cref="System.Reflection.Emit"/>, invece di bytecode.
///
/// Copertura attuale (fetta verticale MVP, vedi TODO Obsidian Filone B fase B6):
/// solo statement top-level a numero di riga (non ancora SUB Main come entry
/// point), PRINT, LET su variabili scalari, IF/ELSEIF/ELSE, FOR/NEXT, GOTO,
/// aritmetica e confronti di base, letterali numerici e stringa.
/// GOSUB/RETURN non ancora supportati (richiedono un meccanismo di indirizzo
/// di ritorno non ancora progettato). Qualunque costrutto non ancora coperto
/// lancia un errore esplicito a compile-time
/// (<see cref="NotSupportedIlStatementException"/> /
/// <see cref="NotSupportedIlExpressionException"/>) — non fallback silenzioso,
/// non crash a runtime.
/// </summary>
public sealed partial class IlProgramCompiler
{
    private readonly BasicProgram _program;
    private readonly ILGenerator _il;
    private readonly IlTypeContext _types;
    private readonly Dictionary<int, LocalBuilder> _locals = new();
    // Fallback: i local per NOME della procedura corrente. Il linker a volte risolve una
    // variabile IMPLICITA (non-DIM) assegnata e poi usata DENTRO un FOR con due SymbolId
    // diversi (dump: stloc.s 5 / ldloc.s 6 → il load leggeva un local mai scritto = 0).
    // Azzerato a ogni metodo; il fallback per nome riunisce store/load della stessa
    // variabile. Mai global: procedure diverse con lo stesso nome non si mescolano.
    private readonly Dictionary<string, LocalBuilder> _localByName = new();
    // Symbol la cui DIM dichiara tipi DIVERSI nello stesso scope (es. `DIM S AS SINGLE` +
    // `DIM S$ AS STRING` — S e S$ sono lo STESSO slot, il suffisso $ è un'annotazione di
    // tipo). Il P-Code VM tollera (stack dinamico); l'IL deve usare un local object boxed,
    // altrimenti il local creato col primo tipo (SINGLE) viene poi scritto con un altro
    // tipo (STRING) → InvalidProgram (TypesConversions.bas/ScopesByRef.bas).
    private readonly HashSet<int> _redefinedTypeSymbols = new();
    // Cached IlRuntime.Current → IRuntimeHost per evitare letture ThreadStatic ripetute
    private LocalBuilder? _runtimeLocal;
    private LocalBuilder? _consoleLocal;
    private LocalBuilder? _graphicsLocal;    private LocalBuilder? _fileLocal;
    private LocalBuilder? _audioLocal;
    private LocalBuilder? _networkLocal;
    private LocalBuilder? _dbLocal;
    // Contesto lessicale WITH: ogni blocco cattura il receiver una sola volta nel
    // suo tipo CLR reale; i membri con leading dot usano il top dello stack.
    private readonly Stack<(LocalBuilder Local, DataType Type)> _withTargets = new();

    /// <summary>Etichette con nome (generate dal Parser per l'appiattimento di loop/IF), per JumpStmt/ConditionalJumpStmt/LabelStmt.</summary>
    private readonly Dictionary<string, Label> _namedLabels = new();

    /// <summary>Un'etichetta IL per ogni numero di riga del programma, per GOTO/GOSUB numerici.</summary>
    private readonly Dictionary<int, Label> _lineLabels = new();
    /// <summary>Label di uscita del metodo corrente: target dei GOTO verso righe inesistenti
    /// (idioma `GOTO 999` come "uscita dal programma" — il C99 emette un label di fallback
    /// che termina il main). Definita a inizio metodo, marcata prima del `ret` finale.</summary>
    private Label? _missingLineExitLabel;
    /// <summary>
    /// A quale numero di riga appartiene ogni etichetta con nome — popolato SOLO da
    /// EmitEntryPointWithErrorHandling (ON ERROR GOTO/RESUME attivo, vedi quel metodo),
    /// mai da EmitStatementBody (procedure) dato che il codice moderno non ha righe
    /// numerate. Serve a EmitBranch/EmitConditionalBranch per sapere a quale "chunk"
    /// (numero di riga) redirigere un salto con nome quando attraversa il confine di un
    /// blocco try per-riga.
    /// </summary>
    private readonly Dictionary<string, int> _namedLabelToLine = new();

    /// <summary>
    /// Non null SOLO mentre si compila l'entry point con ON ERROR GOTO/RESUME attivo
    /// (EmitEntryPointWithErrorHandling): il locale che tiene la "prossima riga da
    /// eseguire" e l'etichetta a cui saltare (Leave, mai Br) per uscire dal chunk try
    /// CORRENTE e tornare al dispatcher. Ogni emettitore di salto (GOTO/etichette con
    /// nome/uscita FOR/uscita FOR EACH/END) lo controlla: se non null, un salto verso una
    /// riga DIVERSA da quella corrente non può essere un Br diretto (attraverserebbe il
    /// confine di due blocchi try per-riga separati, IL non valido — solo Leave può
    /// uscire da una regione protetta) — va invece tradotto in "imposta lo stato, poi
    /// Leave verso il dispatcher".
    /// </summary>
    private (LocalBuilder State, LocalBuilder ErrorHandlerTarget, LocalBuilder ErrorLine, LocalBuilder IsGosub, Label LeaveTarget, ImmutableArray<int> ExecutionOrder)? _onErrorDispatch;

    /// <summary>
    /// Punto di uscita di un `RETURN valore` emesso DENTRO il dispatch a chunk di una
    /// PROCEDURA con ON ERROR attivo (EmitProcedureBodyWithErrorHandling): un `ret` grezzo
    /// dentro il try per-chunk è IL invalido, quindi si salva il valore in
    /// <see cref="_onErrorReturnValue"/> e si fa `leave` a questa etichetta (fuori da ogni
    /// regione protetta), che emette il vero `ret`. Nullo nell'entry point (dove RETURN
    /// non è un ritorno di metodo) e fuori dal dispatch.
    /// </summary>
    private Label? _onErrorReturnExitLabel;

    /// <summary>Local tipizzato (tipo di ritorno CLR) che trasporta il valore di un
    /// `RETURN valore` dal try del chunk al punto di uscita
    /// <see cref="_onErrorReturnExitLabel"/>. Nullo per SUB/Main (nessun valore).</summary>
    private LocalBuilder? _onErrorReturnValue;

    /// <summary>SUB Main da chiamare DOPO il corpo top-level (entry point che esegue il
    /// codice di livello modulo e poi Main). Impostato in Host.Compile.cs quando esistono SIA
    /// codice top-level eseguibile SIA un SUB Main — senza, il top-level sarebbe ignorato.</summary>
    private (FunctionDefinition Def, MethodBuilder Method)? _trailingMainCall;

    /// <summary>
    /// Lo scope TRY attualmente più interno in compilazione (fuori scope non appena
    /// stmt.TryBody finisce di compilare) — un GOTO/etichetta con nome/numero di riga il
    /// cui target NON è nell'insieme (Labels/Lines) di QUESTO scope attraversa il
    /// confine del try corrente: IL vieta un Br diretto fuori da una regione protetta
    /// (solo Leave può farlo, e lo fa correttamente anche attraverso più livelli
    /// annidati in una sola istruzione — non serve tracciare l'intera pila, solo lo
    /// scope più interno, per decidere Br/Leave), altrimenti InvalidProgramException al
    /// caricamento dell'assembly (vedi AiWiki IL-OnError-Loop-Bug.md — bug trovato
    /// tramite ExceptionsTryIfSelect.bas: un `GOTO` dal corpo del TRY a un'etichetta dopo
    /// `END TRY`, es. `IF N > 10 GOTO fine`).
    /// </summary>
    private readonly Stack<(HashSet<string> Labels, HashSet<int> Lines)> _activeTryScopes = new();

    /// <summary>Punto di throw esterno alle regioni protette (per l'ERROR statement dentro
    /// un TRY: `throw` grezzo in una protected region è IL invalido → Leave qui + throw).
    /// Per-metodo, marcato a fine corpo (EmitStatementBody), reset a ogni metodo.</summary>
    private Label? _errorThrowExitLabel;
    private LocalBuilder? _errorThrowTemp;
    private bool _errorThrowExitPending;
    /// <summary>Punto di RETURN esterno alle regioni protette (RETURN con valore dentro un
    /// TRY/FINALLY: `ret` grezzo in una protected region è IL invalido — Leave qui + ret).
    /// Il valore viene salvato in un local tipizzato prima del leave.</summary>
    private Label? _returnExitLabel;
    private LocalBuilder? _returnExitValue;
    private bool _returnExitPending;

    /// <summary>DEFER sul path lineare (parità Go/PCode/VM-C/C99Native): body
    /// registrati durante la compilazione del corpo della procedura corrente, eseguiti LIFO
    /// PRIMA del ret reale. Un'istanza di IlProgramCompiler è per-procedura (vedi i
    /// costruttori privati), quindi la lista parte vuota per costruzione — nessun clear
    /// esplicito necessario fra una procedura e l'altra.</summary>
    private readonly List<Stmt> _currentDefers = new();

    /// <summary>Valore sentinella per lo stato "fine procedura" in EmitEntryPointWithErrorHandling — mai un numero di riga BASIC valido (sempre positivi).</summary>
    private const int OnErrorDoneState = -1;

    /// <summary>Valore sentinella per _errorHandlerTargetLocal: nessun gestore ON ERROR installato — un errore propaga come eccezione CLR non gestita.</summary>
    private const int OnErrorNoHandler = -1;

    /// <summary>Valore per _errorHandlerTargetLocal impostato da ON ERROR RESUME NEXT: ignora l'errore, continua dalla riga successiva a quella fallita, nessun salto a un gestore.</summary>
    private const int OnErrorResumeNextMode = 0;

    /// <summary>Base per i target ON ERROR che puntano a una PROCEDURA (non a una riga):
    /// _errorHandlerTargetLocal assume valore negativo sotto questa base e il dispatcher
    /// invoca il metodo registrato in <see cref="_onErrorProcedureTargets"/> invece di
    /// saltare a una riga (comportamento P-Code: il salto all'indirizzo della procedura).</summary>
    private const int OnErrorProcedureTargetBase = -1000;

    /// <summary>Target ON ERROR → procedura (ON ERROR GOTO NomeSub): la chiave è il valore
    /// sentinella (OnErrorProcedureTargetBase - indice), il valore è il MethodBuilder della
    /// procedura da invocare quando si verifica un errore.</summary>
    private readonly Dictionary<int, MethodBuilder> _onErrorProcedureTargets = new();

    /// <summary>
    /// Stato dei loop FOR attivi, keyed per lexeme della variabile contatore
    /// (ForCheckAndJumpStmt/NextStmt non portano il SymbolId risolto, solo un
    /// Token grezzo — vedi nota in GetOrDeclareLocal). Non rientrante per loop
    /// annidati sulla STESSA variabile (limite noto, non il caso comune).
    /// </summary>
    private readonly Dictionary<string, (int SymbolId, LocalBuilder EndLocal, LocalBuilder StepLocal)> _forLoopState = new();
    /// <summary>Exit label per ogni FOR/DO attivo, keyed per nome variabile. Per EXIT FOR.</summary>
    private readonly Dictionary<string, Label> _loopExitLabels = new();
    /// <summary>
    /// Stack runtime degli identificatori dei siti GOSUB dell'entry point. Le etichette
    /// non possono essere impilate durante la compilazione: l'ordine lessicale dei
    /// GOSUB non coincide necessariamente con l'ordine dinamico dei RETURN.
    /// </summary>
    private LocalBuilder? _gosubReturnStackLocal;
    private readonly Dictionary<GosubStmt, (int Id, Label ReturnLabel)> _gosubSites =
        new(ReferenceEqualityComparer.Instance);
    private sealed record DispatchedGosubSite(int Id, int OwnerState, Label ReturnLabel, TryCatchStmt? ProtectedTry);
    private readonly Dictionary<GosubStmt, DispatchedGosubSite> _dispatchedGosubSites =
        new(ReferenceEqualityComparer.Instance);
    private readonly Dictionary<TryCatchStmt, Label> _dispatchedTryEntryLabels =
        new(ReferenceEqualityComparer.Instance);
    private LocalBuilder? _gosubResumeSiteLocal;
    private Label[] _gosubReturnLabels = [];
    /// <summary>Stack per loop annidati — tiene traccia delle label di exit/continue per il loop corrente.</summary>
    private readonly Stack<(Label ContinueLabel, Label ExitLabel)> _activeLoopStack = new();
    /// <summary>Tipo (For/Do/While/ForEach) del loop corrente, parallelo a _activeLoopStack: serve a
    /// sapere quando un ExitLoopStmt chiude un FOR (per rimuovere il contatore da _activeForCounters).</summary>
    private readonly Stack<TokenKind> _activeLoopKinds = new();
    /// <summary>Contatori FOR ATTIVI (push in EmitForStatement, pop all'ExitLoopStmt del FOR).
    /// La lettura di una variabile il cui NOME coincide con un contatore FOR va reindirizzata al
    /// local del contatore SOLO se quel FOR è ancora aperto — _forLoopState da solo non basta:
    /// resta popolato per tutta la procedura e reindirizzava anche le variabili DOPO il loop
    /// (es. J di un DO successivo letto dal local del vecchio contatore FOR J → loop infinito).</summary>
    private readonly Stack<int> _activeForCounters = new();

    /// <summary>
    /// Stato dei loop FOR EACH attivi, keyed per lexeme della (unica, per ora — vedi
    /// EmitForEachInitStatement) variabile di iterazione, stesso schema di
    /// <see cref="_forLoopState"/> per FOR (ForEachAdvanceAndAssignStmt, più avanti nello
    /// stream, non porta il SymbolId dell'array né il proprio contatore, solo i Token
    /// grezzi delle variabili). <c>IteratorSymbolId</c> è risolto UNA volta in
    /// EmitForEachInitStatement (per nome, nello scope del loop — vedi Linker.Binding.cs,
    /// case ForEachInitStmt, che registra li' le variabili di iterazione) ed è cachato qui
    /// per evitare di rifare la ricerca a ogni iterazione.
    /// </summary>
    private readonly Dictionary<string, (ArrayInfo Array, LocalBuilder CounterLocal, int IteratorSymbolId)> _forEachState = new();

    /// <summary>
    /// KeyOrElementClrType/ValueClrType NON vengono mai letti dal DataType simbolico delle
    /// variabili di iterazione — il Linker (Linker.Binding.cs, case ForEachInitStmt) registra
    /// OGNI variabile di un FOR EACH con InferredType (→ Double di default), identico per
    /// entrambe le variabili di un ciclo a 2, senza alcuna nozione di "questa è la chiave,
    /// questa il valore" o del tipo reale della collezione — usarlo qui produrrebbe un
    /// Unbox_Any(Double) su una chiave STRING (bug scoperto con `FOR EACH item, cost IN
    /// menu` su un DICT OF STRING,DOUBLE: cast exception "String → Double"). Calcolati invece
    /// UNA volta in EmitForEachInitStatement da InferForEachElementTypes, che guarda il tipo
    /// REALE della collezione (ListType.ElementType, DictType.Key/ValueType, ecc.) — lo stesso
    /// principio già usato dal fast path array (ArrayInfo.ElementDataType, mai il DataType
    /// della variabile). Il DataType risolto viene anche scritto nel symbol table stesso
    /// (UpdateSymbolType, in EmitForEachInitStatement) — non solo cachato qui — perché
    /// InferType(VariableExpr) per ogni USO successivo della variabile (es. l'argomento di
    /// PRINT) legge symbol.DataType, mai questa cache locale.
    /// </summary>
    private readonly Dictionary<string, (LocalBuilder ListLocal, LocalBuilder CounterLocal, int IteratorSymbolId, int? ValueIteratorSymbolId, Type KeyOrElementClrType, Type? ValueClrType, int VarCount, Type? ElementClrType)> _forEachListState = new();

    private static readonly MethodInfo IlForEachValuesMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ForEachValues))!;
    private static readonly MethodInfo IlForEachEntriesMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ForEachEntries))!;
    private static readonly ConstructorInfo GosubStackConstructor = typeof(Stack<int>).GetConstructor(Type.EmptyTypes)!;
    private static readonly MethodInfo GosubStackPushMethod = typeof(Stack<int>).GetMethod(nameof(Stack<int>.Push))!;
    private static readonly MethodInfo GosubStackPopMethod = typeof(Stack<int>).GetMethod(nameof(Stack<int>.Pop))!;
    private static readonly MethodInfo GosubStackCountGetter = typeof(Stack<int>).GetProperty(nameof(Stack<int>.Count))!.GetMethod!;
    private static readonly MethodInfo IlRangeBuiltinMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RangeBuiltin))!;
    private static readonly MethodInfo IlReverseGenericMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReverseGeneric))!;
    private static readonly MethodInfo IlJsonParseMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonParse))!;
    private static readonly MethodInfo IlJsonLoadFileMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonLoadFile))!;
    private static readonly MethodInfo IlJsonFieldGetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonFieldGet))!;
    private static readonly MethodInfo IlRuntimeTabMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Tab))!;
    private static readonly MethodInfo IlRuntimeSecondsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Seconds))!;
    private static readonly MethodInfo IlRuntimeCdowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Cdow))!;
    private static readonly MethodInfo IlRuntimeDowMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Dow))!;
    private static readonly MethodInfo PoolGetMethod = typeof(PCodePool).GetMethod(nameof(PCodePool.Get))!;
    private static readonly MethodInfo PoolPutMethod = typeof(PCodePool).GetMethod(nameof(PCodePool.Put))!;
    private static readonly MethodInfo ConcurrentMapStoreMethod = typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Store))!;
    private static readonly MethodInfo ConcurrentMapLoadMethod = typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Load))!;
    private static readonly MethodInfo ConcurrentMapDeleteMethod = typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Delete))!;
    private static readonly MethodInfo ConcurrentMapLoadOrStoreMethod = typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.LoadOrStore))!;
    private static readonly MethodInfo AtomicSwapMethod = typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Swap))!;
    private static readonly MethodInfo AtomicStoreMethod = typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Store))!;
    private static readonly MethodInfo AtomicLoadMethod = typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Load))!;
    private static readonly MethodInfo AtomicAddMethod = typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Add))!;
    private static readonly MethodInfo AtomicCompareSwapMethod = typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.CompareSwap))!;
    private static readonly MethodInfo IlRuntimeDtocMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Dtoc))!;
    private static readonly MethodInfo IlRuntimeCmonthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Cmonth))!;
    private static readonly MethodInfo IlRuntimeDateDiffMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.DateDiff))!;
    private static readonly MethodInfo IlCompressionCompressMethod = typeof(CommonLib.CompressionUtils).GetMethod(nameof(CommonLib.CompressionUtils.CompressString))!;
    private static readonly MethodInfo IlCompressionDecompressMethod = typeof(CommonLib.CompressionUtils).GetMethod(nameof(CommonLib.CompressionUtils.DecompressString))!;
    private static readonly MethodInfo IlGrepMatchesMethod = typeof(NativeGrep).GetMethod(nameof(NativeGrep.MatchesArray))!;
    private static readonly MethodInfo IlGrepFilesMethod = typeof(NativeGrep).GetMethod(nameof(NativeGrep.FilesArray))!;
    private static readonly MethodInfo IlGrepCountMethod = typeof(NativeGrep).GetMethod(nameof(NativeGrep.Count))!;
    private static readonly MethodInfo IlRuntimeZipMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Zip))!;
    private static readonly MethodInfo IlRuntimeUnzipMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Unzip))!;
    private static readonly MethodInfo IlRuntimeAlgorithmsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Algorithms))!;
    private static readonly MethodInfo IlRuntimeHostMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Host))!;
    private static readonly MethodInfo IlRuntimeTempDirMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TempDir))!;
    private static readonly MethodInfo IlRuntimeGetHostIpMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.GetHostIp))!;
    private static readonly MethodInfo IlRuntimePeekMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Peek))!;
    private static readonly MethodInfo IlRuntimePetsciiMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Petscii))!;
    private static readonly MethodInfo IlRuntimeScreensMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Screens))!;
    private static readonly MethodInfo IlRuntimeScreenInfoMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ScreenInfo))!;
    private static readonly MethodInfo IlRuntimeRgbMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Rgb))!;
    private static readonly MethodInfo IlRuntimeRgbaToColorIntMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RgbaToColorInt), [typeof(object)])!;
    private static readonly MethodInfo IlRuntimeCellMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.Cell))!;
    private static readonly MethodInfo IlRuntimeCellValueMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.CellValue), [typeof(double), typeof(double), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeVarPtrMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.VarPtr))!;
    private static readonly MethodInfo IlRuntimeTypeInfoMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TypeInfo))!;
    private static readonly MethodInfo IlRuntimeTypeInfoHasMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TypeInfoHas))!;
    private static readonly MethodInfo IlRuntimeMethodInfoMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MethodInfo))!;
    private static readonly MethodInfo IlRuntimePropertyInfoMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.PropertyInfo))!;
    private static readonly MethodInfo IlRuntimeUdtPackedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UdtPacked))!;
    private static readonly MethodInfo IlRuntimeUdtAlignedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UdtAligned))!;
    private static readonly MethodInfo IlRuntimeUniqueCountMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UniqueCount))!;
    private static readonly MethodInfo IlRuntimeStringTrimMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StringTrim))!;
    private static readonly MethodInfo IlRuntimeJsonValueToStringMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonValueToString))!;
    private static readonly MethodInfo IlRuntimeStringTrimStartMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StringTrimStart))!;
    private static readonly MethodInfo IlRuntimeStringTrimEndMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.StringTrimEnd))!;
    private static readonly MethodInfo IlRuntimeSafeCastMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.SafeCastValue))!;
    private static readonly MethodInfo IlRuntimeTextHeightMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TextHeight))!;
    private static readonly MethodInfo IlRuntimeC2PXMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.C2PX), [typeof(double), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeC2PYMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.C2PY), [typeof(double), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeP2CXMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.P2CX), [typeof(double), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeP2CYMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.P2CY), [typeof(double), typeof(double)])!;
    private static readonly MethodInfo IlRuntimeFormAddControlMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormAddControl))!;
    private static readonly MethodInfo IlRuntimeFormSetLastStyleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormSetLastStyle))!;
    private static readonly MethodInfo IlRuntimeFormFrameMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormFrame))!;
    private static readonly MethodInfo IlRuntimeFormClosedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormClosed))!;
    private static readonly MethodInfo IlRuntimeFormCloseMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormClose))!;
    private static readonly MethodInfo IlRuntimeFormControlGetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlGet))!;
    private static readonly MethodInfo IlRuntimeChartConfigSetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ChartConfigSet))!;
    private static readonly MethodInfo IlRuntimeChartConfigGetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ChartConfigGet))!;
    private static readonly MethodInfo IlRuntimeChartConfigGetStrMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ChartConfigGetStr))!;
    private static readonly MethodInfo IlRuntimeChartDrawMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ChartDraw))!;
    private static readonly MethodInfo IlRuntimeFormControlGetStrMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlGetStr))!;
    private static readonly MethodInfo IlRuntimeFormControlSetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlSet))!;
    private static readonly MethodInfo IlRuntimeFormControlMoveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlMove))!;
    private static readonly MethodInfo IlRuntimeFormControlAddMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlAdd))!;
    private static readonly MethodInfo IlRuntimeFormControlRemoveMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlRemove))!;
    private static readonly MethodInfo IlRuntimeFormControlDrawMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlDraw))!;
    private static readonly MethodInfo IlRuntimeFormControlFocusMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlFocus))!;
    private static readonly MethodInfo IlRuntimeFormControlZorderMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlZorder))!;
    private static readonly MethodInfo IlRuntimeFormControlInvalidateMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlInvalidate))!;
    private static readonly MethodInfo IlRuntimeFormControlRefreshMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormControlRefresh))!;
    private static readonly MethodInfo IlRuntimeFormBindUserControlEventMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormBindUserControlEvent))!;
    private static readonly MethodInfo IlRuntimeFormLastHandleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormLastHandle))!;
    private static readonly MethodInfo IlRuntimeFormClickedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormClicked))!;
    private static readonly MethodInfo IlRuntimeFormDblClickedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormDblClicked))!;
    private static readonly MethodInfo IlRuntimeFormChangedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormChanged))!;
    private static readonly MethodInfo IlRuntimeFormGotFocusedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormGotFocused))!;
    private static readonly MethodInfo IlRuntimeFormLostFocusedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormLostFocused))!;
    private static readonly MethodInfo IlRuntimeMouseClickedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseClicked))!;
    private static readonly MethodInfo IlRuntimeMouseDblClickedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MouseDblClicked))!;
    private static readonly MethodInfo IlRuntimeMousePressedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.MousePressed))!;
    private static readonly MethodInfo IlRuntimeKeyPressedMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.KeyPressed))!;
    private static readonly MethodInfo IlRuntimeUiFrameMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UiFrame))!;
    private void EmitTuiIdIL(Target? id) { if (id == null) return; _il.Emit(OpCodes.Call, IlRuntimeUiLastHandleMethod); EmitAssignTargetFromTop(id, new DoubleType()); }
    private void EmitTuiPropsIL(ControlProps? p, Token tok) { if (p == null) return; EmitTuiCall(IlRuntimeControlApplyPropsMethod, [p.Name, p.Tag, p.Tooltip, p.TabIndex, p.Visible, p.Enabled, p.TabStop, p.Focus ? new NumberExpr(1.0, tok) : null, p.Style, p.Color, p.ColorFocus, p.ColorSelect, p.ColorError], null); }
    private static readonly MethodInfo IlRuntimeUiLastHandleMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.UiLastHandle))!;
    private static readonly MethodInfo IlRuntimeControlApplyPropsMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.ControlApplyProps))!;
    private static readonly MethodInfo IlRuntimeFormSetEventIdMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormSetEventId))!;
    private static readonly MethodInfo IlRuntimeRegisterControlEventMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RegisterControlEventHandler))!;
    private static readonly MethodInfo IlRuntimeFormFireEventMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.FormFireEvent))!;
    private static readonly MethodInfo IlRuntimeTextWidthMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TextWidth))!;
    private static readonly MethodInfo IlJsonFieldSetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonFieldSet))!;
    private static readonly MethodInfo IlJsonIndexGetMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonIndexGet))!;
    private static readonly MethodInfo ForEachValuesListCountGetter = typeof(List<object?>).GetProperty(nameof(List<object?>.Count))!.GetGetMethod()!;
    private static readonly MethodInfo ForEachValuesListItemGetter = typeof(List<object?>).GetProperty("Item")!.GetGetMethod()!;
    private static readonly MethodInfo ForEachEntriesListCountGetter = typeof(List<Tuple<object?, object?>>).GetProperty(nameof(List<Tuple<object?, object?>>.Count))!.GetGetMethod()!;
    private static readonly MethodInfo ForEachEntriesListItemGetter = typeof(List<Tuple<object?, object?>>).GetProperty("Item")!.GetGetMethod()!;
    private static readonly MethodInfo TupleItem1Getter = typeof(Tuple<object?, object?>).GetProperty(nameof(Tuple<object?, object?>.Item1))!.GetGetMethod()!;
    private static readonly MethodInfo TupleItem2Getter = typeof(Tuple<object?, object?>).GetProperty(nameof(Tuple<object?, object?>.Item2))!.GetGetMethod()!;
    private static readonly MethodInfo ArrayGetValueMethod = typeof(Array).GetMethod(nameof(Array.GetValue), [typeof(int)])!;

    /// <summary>
    /// Metadati per ogni array DIMensionato, keyed per SymbolId — popolato da
    /// EmitArrayDeclaration, consumato da EmitArrayAccess/EmitArrayElementAssignment/
    /// EmitRedimStatement. Non riusa <see cref="_locals"/> perché <see cref="ArrayType"/>
    /// non porta il rank (solo l'ElementType — vedi DataTypeMapper), quindi il rank e i
    /// limiti inferiori vanno tracciati qui, non ricavabili da DataTypeMapper.Map da solo.
    /// </summary>
    private readonly Dictionary<int, ArrayInfo> _arrays = new();

    /// <summary>
    /// Un array CLR reale (rank 1 = <c>T[]</c>/SZArray, rank N = <c>T[,...]</c>
    /// rettangolare) con elementi UNBOXED — mai un wrapper dinamico/generico, per la
    /// velocità del programma compilato: rank 1 usa ldelem/stelem diretti, rank N usa
    /// i metodi pseudo-intrinsechi Get/Set/Address dell'array rettangolare (anch'essi
    /// senza boxing per il tipo elemento reale, a differenza di Array.GetValue/
    /// SetValue). Entrambe le forme sono memoria CONTIGUA (mai jagged/array-di-array).
    /// </summary>
    private sealed record ArrayInfo(LocalBuilder Local, Type ClrArrayType, DataType ElementDataType, int Rank, int[] LowerBounds, Action? LoadRef = null);

    /// <summary>
    /// Come <see cref="ArrayInfo"/>, ma per un campo array di CLASSE (un array CLR reale
    /// per-ISTANZA, mai per-tipo): non esiste un LocalBuilder da caricare con Ldloc, la
    /// referenza va caricata leggendo il campo dall'istanza (Ldfld) — vedi
    /// EmitLoadArrayRefAndIndices, che accetta ora un delegato invece del solo LocalBuilder
    /// per coprire entrambi i casi. Allocato una volta per istanza in ogni costruttore
    /// della classe (vedi EmitConstructorBody), non in DIM (i campi non passano da lì).
    /// </summary>
    /// <summary>
    /// <paramref name="ClassSymbolId"/>/<paramref name="FieldName"/> permettono di
    /// risolvere questo campo per nome partendo dal SOLO SymbolId della classe
    /// proprietaria (vedi TryResolveIndexedFieldArray) invece che percorrendo
    /// <c>ClassDefinition.Members</c>: quest'ultima lista può essere STALE quando
    /// raggiunta tramite SELF (bug del Linker scoperto testando questa feature — il
    /// simbolo sintetico SELF porta una ClassType/ClassDefinition "snapshot" catturata
    /// PRIMA che RegisterFieldMember assegni il SymbolId definitivo al campo, quindi
    /// `fm.Declaration.SymbolId` risulta ancora -1 in quello snapshot). Il SymbolId
    /// della CLASSE stessa resta invece stabile in ogni snapshot, quindi è l'unica
    /// chiave affidabile per risalire a questo dizionario (popolato una volta sola in
    /// Pass 1, dalla lista `classDefinitions` autorevole).
    /// </summary>
    internal sealed record FieldArrayInfo(FieldBuilder Field, Type ClrArrayType, DataType ElementDataType, int Rank, int[] LowerBounds, int ClassSymbolId, string FieldName);

    /// <summary>
    /// Tutte le procedure utente (SUB/FUNCTION) definite nel modulo principale, keyed per
    /// SymbolId — condiviso da TUTTE le istanze di IlProgramCompiler (Main + una per
    /// procedura), popolato in un'unica pre-passata PRIMA di compilare qualunque corpo
    /// (stesso pattern a due passate già usato per le label — vedi EmitEntryPoint), cosi'
    /// riferimenti in avanti e ricorsione (diretta o mutua) funzionano senza problemi.
    /// </summary>
    private readonly Dictionary<int, MethodBuilder> _procedureMethods;

    /// <summary>
    /// La FunctionDefinition originale di ogni procedura, keyed per SymbolId — condivisa
    /// come <see cref="_procedureMethods"/>. Serve ai call site (EmitFunctionCall) per
    /// sapere QUALI parametri del CALLEE sono BYREF (il MethodBuilder da solo lo sa già
    /// via il tipo `T&`, ma è più semplice ririleggerlo dai Parameters originali che
    /// ispezionare i ParameterInfo del MethodBuilder).
    /// </summary>
    private readonly Dictionary<int, FunctionDefinition> _procedureDefinitions;

    /// <summary>
    /// Info su un parametro della procedura correntemente compilata: indice argomento
    /// CIL, se è BYREF (l'argomento CIL è un vero `T&`, non un `T` — leggerlo richiede
    /// Ldarg+Ldind, scriverlo Ldarg+valore+Stind, non il semplice Ldarg/Starg di un
    /// parametro BYVAL) e il suo DataType (serve a scegliere l'opcode Ldind/Stind giusto).
    /// </summary>
    /// <param name="IsBox">Fase 4 (piano Closure-ByRef-Box-Linker-Definitivo): true quando
    /// questo parametro, LOGICAMENTE BYREF, è rappresentato a livello CLR come
    /// <c>ByRefBox&lt;T&gt;</c> (BYVAL) invece che <c>T&amp;</c> — perché catturato da una
    /// closure locale che sfugge (vedi IlProgramCompiler.ByRefEscapeBox.cs). Quando è true,
    /// <see cref="IsByRef"/> è SEMPRE false (il parametro è BYVAL a livello CLR: Ldarg diretto,
    /// mai Ldarga/Ldind) — l'accesso "logicamente BYREF" passa da Value, non da un indirizzo.</param>
    private sealed record ParameterInfo(int ArgIndex, bool IsByRef, DataType DataType, bool IsBox = false);

    /// <summary>
    /// Mappa SymbolId parametro → info parametro, popolata dal costruttore SOLO per
    /// un'istanza che compila il corpo di una procedura (vuota per Main, che non ha
    /// parametri). Un parametro è un vero argomento CIL (Ldarg/Starg, o Ldarg+Ldind/Stind
    /// se BYREF), non un LocalBuilder — vedi EmitLoadVariable/EmitStoreVariable.
    /// </summary>
    private readonly Dictionary<int, ParameterInfo> _parameters = new();

    /// <summary>
    /// Mappa SymbolId di un parametro-array BYREF → il suo ArgIndex (stesso indice già in
    /// ParameterInfo, duplicato qui per un lookup diretto) — vedi RegisterParameters. Un
    /// parametro-array viene SEMPRE materializzato in un locale reale in _arrays (Ldarg +
    /// Ldind_Ref + Stloc, all'ingresso della procedura) cosi' che tutta la macchina array
    /// esistente (indicizzazione, FOR EACH, UBOUND/LBOUND) funzioni invariata leggendo quel
    /// locale — ma REDIM, che REASSEGNA il locale a un array nuovo/ridimensionato, deve
    /// anche scrivere quella nuova referenza indietro attraverso il parametro BYREF
    /// originale (Ldarg + Ldloc + Stind_Ref) perché sia visibile al chiamante — questa
    /// mappa dice a EmitRedimStatement quando farlo (assente per un array locale DIM'd
    /// puro, che non ha nulla "a monte" a cui scrivere indietro).
    /// </summary>
    private readonly Dictionary<int, int> _arrayParamWriteback = new();

    /// <summary>
    /// Tipo di ritorno della procedura correntemente compilata da questa istanza (null per
    /// Main). Serve a EmitEndProcedureStatement per sapere se "END FUNCTION" raggiunto
    /// senza una RETURN esplicita deve spingere un valore di default prima di `ret` (una
    /// FUNCTION non può fare `ret` senza valore) o restituire senza valore (SUB/Main).
    /// </summary>
    private readonly DataType? _currentReturnType;

    /// <summary>SymbolId della variabile di ritorno (il nome della FUNCTION stessa). Usato da
    /// EmitEndProcedureStatement per caricare il valore calcolato dalla procedura invece del
    /// valore di default (0/0.0/""). Null per SUB/Main/costruttori (nessuna variabile di ritorno).
    /// </summary>
    private readonly int _currentReturnSymbolId;

    /// <summary>
    /// TypeBuilder di ogni CLASS utente dichiarata nel modulo principale, keyed per
    /// SymbolId — condiviso da TUTTE le istanze di IlProgramCompiler, popolato in
    /// pre-passata PRIMA di dichiarare campi/metodi/costruttori (stesso pattern a due
    /// passate delle procedure/label — riferimenti in avanti tra classi funzionano).
    /// Un'istanza di CLASS è un vero reference type CLR (TypeBuilder che eredita da
    /// System.Object) — mai un dizionario dinamico di campi.
    /// </summary>
    private readonly Dictionary<int, TypeBuilder> _classTypes;

    /// <summary>FieldBuilder di ogni campo di classe, keyed per SymbolId del campo — condiviso.</summary>
    private readonly Dictionary<int, FieldBuilder> _classFields;

    /// <summary>Metadati di ogni campo ARRAY di classe (rank, tipo elemento, limiti inferiori), keyed per SymbolId del campo — condiviso, vedi <see cref="FieldArrayInfo"/>.</summary>
    private readonly Dictionary<int, FieldArrayInfo> _classFieldArrays;

    /// <summary>
    /// Tutti i costruttori (overload inclusi) di ogni classe, keyed per SymbolId DELLA
    /// CLASSE (non del costruttore — ConstructorDefinition non ha un SymbolId proprio).
    /// Overload risolti per NUMERO di argomenti (stessa strategia usata dal p-code
    /// Compiler/VM per OP_NEW_CLASS_INSTANCE, ma decisa a COMPILE-TIME qui invece che a
    /// runtime — IL è tipizzato staticamente, `newobj` deve riferirsi a UN costruttore
    /// preciso) — non per tipi completi: due costruttori con lo stesso numero di
    /// argomenti ma tipi diversi non sono ancora distinguibili, lancia esplicito.
    /// </summary>
    private readonly Dictionary<int, List<ClassConstructorInfo>> _classConstructors;

    /// <summary>Un costruttore di classe dichiarato, con i suoi parametri originali (per la risoluzione degli overload per numero di argomenti).</summary>
    internal sealed record ClassConstructorInfo(ConstructorBuilder Builder, ImmutableArray<Parameter> Parameters);

    /// <summary>
    /// Accessor get_X/set_X di ogni PROPERTY di classe, keyed per SymbolId DELLA
    /// PROPERTY — condiviso. Get/Set sono veri MethodBuilder d'istanza (esattamente come
    /// un metodo normale — vedi EmitPropertyAccess/EmitFieldAssignment), chiamati con
    /// `call` diretta (nessun supporto a proprietà VIRTUAL in questo MVP).
    /// </summary>
    private readonly Dictionary<int, ClassPropertyInfo> _classProperties;

    internal sealed record ClassPropertyInfo(MethodBuilder? Getter, MethodBuilder? Setter);

    /// <summary>
    /// MethodBuilder di ogni OPERATOR sovraccaricato, keyed per (SymbolId della classe
    /// dichiarante, TokenKind dell'operatore) — <see cref="OperatorDefinition"/> non porta
    /// un proprio SymbolId (a differenza di FunctionDefinition/costruttori), quindi non può
    /// condividere <see cref="_procedureMethods"/>. Un OPERATOR è compilato come un metodo
    /// STATIC con ENTRAMBI gli operandi come parametri espliciti (indice 0-based, nessun
    /// SELF — confermato con --dump-ast: il corpo di un OPERATOR non referenzia mai SELF,
    /// solo i due parametri dichiarati, es. "OPERATOR +(a AS V, b AS V)"), mai un metodo
    /// d'istanza. Un `BinaryExpr` con operandi ClassType NON viene mai riscritto dal Linker
    /// in una chiamata esplicita (resta un BinaryExpr generico, dispatch dinamico a runtime
    /// nel p-code Compiler/Executor) — questo backend risolve invece staticamente in
    /// EmitBinaryExpression, stesso identico algoritmo di ricerca (per TokenKind
    /// dell'operatore, risalendo la gerarchia base) usato da
    /// Linker.InferTypes.LinkerInferExpressionType per l'inferenza del tipo.
    /// </summary>
    private readonly Dictionary<(int OwnerSymbolId, TokenKind OpKind, int Arity, string ReturnType), MethodBuilder> _classOperators;

    /// <summary>
    /// MethodBuilder del FINALIZER CLR (nome "Finalize") di ogni classe con DELETE/DESTRUCTOR,
    /// keyed per SymbolId DELLA CLASSE — condiviso. Usato per la **finalizzazione
    /// DETERMINISTICA** a fine scope/procedura (parità PCode `OP_FINALIZE_SCOPE/GLOBAL`):
    /// il codegen chiama esplicitamente il finalizer + `GC.SuppressFinalize` per evitare la
    /// doppia esecuzione via GC. Nullable: gli usi che non lo passano restano GC-only.
    /// </summary>
    private readonly Dictionary<int, MethodBuilder>? _classFinalizers;

    /// <summary>Metodo `__Destructor` delle UDT (value type) con SUB DESTRUCTOR, keyed per
    /// SymbolId del TYPE. Un value type NON ha finalizer CLR → la finalizzazione è SOLO
    /// quella esplicita (Ldloca + call) a fine procedura. Nullable: GC-only dove non passato.</summary>
    private readonly Dictionary<int, MethodBuilder>? _udtFinalizers;

    /// <summary>
    /// Info di compilazione per ogni <see cref="LambdaExpr"/> del programma, popolato da
    /// <see cref="DiscoverAndCompileLambdas"/> PRIMA di Pass 2 (i corpi che le usano — una
    /// lambda può comparire in qualunque punto in cui compila un'espressione). Keyed per
    /// riferimento (due lambda testualmente identiche in punti diversi sono nodi AST
    /// distinti — la stessa istanza .NET non ricorre mai).
    /// </summary>
    private readonly Dictionary<LambdaExpr, LambdaClosureInfo> _lambdaInfos;

    /// <summary>
    /// Info di compilazione di una singola lambda: il tipo delegate CLR risultante
    /// dall'espressione (Func&lt;...&gt;/Action&lt;...&gt;, vedi DataTypeMapper), il metodo
    /// "Invoke" (STATIC se <see cref="Captures"/> è vuoto — nessuna classe di chiusura
    /// necessaria — altrimenti d'ISTANZA su <see cref="ClosureType"/>), e l'elenco delle
    /// variabili catturate con il campo di chiusura corrispondente.
    /// </summary>
    internal sealed record LambdaClosureInfo(
        Type DelegateType,
        TypeBuilder? ClosureType,
        ConstructorBuilder? ClosureCtor,
        MethodBuilder InvokeMethod,
        ImmutableArray<(int SymbolId, DataType DataType, FieldBuilder Field)> Captures);

    /// <summary>
    /// Quando questa istanza compila il corpo "Invoke" di una lambda CON catture (vedi
    /// <see cref="LambdaClosureInfo"/>): mappa il SymbolId ORIGINALE di ogni variabile
    /// catturata (quello con cui il corpo, mai riscritto per la chiusura, la referenzia
    /// ancora) al campo di chiusura che ne contiene lo snapshot — stesso ruolo di
    /// <see cref="_classFields"/> per SELF, ma locale a QUESTA istanza (catture diverse
    /// per lambda diverse, mai un dizionario globale come le altre). Vuoto per ogni corpo
    /// che non è l'Invoke di una lambda con catture.
    /// </summary>
    private readonly Dictionary<int, FieldBuilder> _capturedFields = new();

    /// <summary>
    /// True quando questa istanza compila il corpo di un METODO o COSTRUTTORE di classe
    /// (mai per Main/procedure top-level): l'argomento CIL 0 è l'istanza implicita
    /// (`this`, mai esplicito nella firma — il CLR lo gestisce da sé per i metodi
    /// d'istanza). SELF/ME vi si risolvono per NOME (vedi EmitLoadVariable), non per
    /// SymbolId: il Linker registra "SELF"/"ME" come simboli sintetici nello scope del
    /// metodo (Linker.Binding.cs, RegisterProcedure) ma non espone il loro SymbolId
    /// all'esterno in un modo comodo da recuperare qui — lo stesso identico problema che
    /// il p-code Compiler risolve risolvendo SELF per nome nella propria tabella dei
    /// locali (Compiler.UserTypes.cs), invece che per ID Linker.
    /// </summary>
    private readonly bool _isInstanceMember;

    /// <summary>
    /// SymbolId della classe proprietaria del METODO/COSTRUTTORE/ACCESSOR correntemente
    /// compilato da questa istanza (null per Main/procedure top-level, mai usato quando
    /// <see cref="_isInstanceMember"/> è false). Popolato dal PASS 2 (che itera
    /// <c>classDefinitions</c>, la lista AUTOREVOLE) invece che risolto dinamicamente da
    /// SELF/ME: la ClassType/ClassDefinition "vista" dal simbolo sintetico SELF è uno
    /// snapshot catturato PRIMA che RegisterFieldMember completi la risoluzione dei
    /// SymbolId dei campi (bug del Linker scoperto testando i campi ARRAY di classe — sia
    /// il SymbolId del campo SIA quello della classe stessa risultano ancora -1 in quello
    /// snapshot), quindi va sempre evitato come fonte per SELF — vedi
    /// TryResolveIndexedFieldArray.
    /// </summary>
    private readonly int? _currentClassSymbolId;

    /// <summary>Costruisce il compilatore per il corpo di Main (nessun parametro, nessun tipo di ritorno).</summary>
    internal IlProgramCompiler(
        BasicProgram program, ILGenerator il, IlTypeContext types,
        Dictionary<int, MethodBuilder> procedureMethods, Dictionary<int, FunctionDefinition> procedureDefinitions,
        Dictionary<int, TypeBuilder> classTypes, Dictionary<int, FieldBuilder> classFields, Dictionary<int, FieldArrayInfo> classFieldArrays, Dictionary<int, List<ClassConstructorInfo>> classConstructors,
        Dictionary<int, ClassPropertyInfo> classProperties, Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        Dictionary<LambdaExpr, LambdaClosureInfo> lambdaInfos,
        Dictionary<int, MethodBuilder>? classFinalizers = null,
        Dictionary<int, MethodBuilder>? udtFinalizers = null)
    {
        _program = program;
        _il = il;
        _types = types;
        _procedureMethods = procedureMethods;
        _procedureDefinitions = procedureDefinitions;
        _classTypes = classTypes;
        _classFields = classFields;
        _classFieldArrays = classFieldArrays;
        _classConstructors = classConstructors;
        _classProperties = classProperties;
        _classOperators = classOperators;
        _lambdaInfos = lambdaInfos;
        _classFinalizers = classFinalizers;
        _udtFinalizers = udtFinalizers;
        _currentReturnSymbolId = -1;
    }

    private IlProgramCompiler(
        BasicProgram program, ILGenerator il, IlTypeContext types,
        Dictionary<int, MethodBuilder> procedureMethods, Dictionary<int, FunctionDefinition> procedureDefinitions,
        Dictionary<int, TypeBuilder> classTypes, Dictionary<int, FieldBuilder> classFields, Dictionary<int, FieldArrayInfo> classFieldArrays, Dictionary<int, List<ClassConstructorInfo>> classConstructors,
        Dictionary<int, ClassPropertyInfo> classProperties, Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        Dictionary<LambdaExpr, LambdaClosureInfo> lambdaInfos,
        FunctionDefinition currentProcedure,
        Dictionary<int, MethodBuilder>? classFinalizers = null,
        Dictionary<int, MethodBuilder>? udtFinalizers = null)
        : this(program, il, types, procedureMethods, procedureDefinitions, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, classFinalizers, udtFinalizers)
    {
        _currentReturnType = currentProcedure.ReturnType;
        _currentReturnSymbolId = currentProcedure.SymbolId;
        RegisterParameters(currentProcedure.Parameters, 0);
        InitializeNumericOrStringReturnSlot();
    }

    /// <summary>Return-slot STRING non assegnato: default "" (non null). Senza, una FUNCTION
    /// AS STRING che esce con EXIT FUNCTION/END FUNCTION senza assegnare il nome lasciava null
    /// e LEN/PRINT andavano in NullReferenceException su IL (PCode/C/VM/C restituivano "").</summary>
    private void InitializeNumericOrStringReturnSlot()
    {
        if (_currentReturnSymbolId < 0 || _currentReturnType is not StringType) return;
        var local = GetOrDeclareLocal(_currentReturnSymbolId);
        _il.Emit(OpCodes.Ldstr, string.Empty);
        _il.Emit(OpCodes.Stloc, local);
    }

    /// <summary>
    /// Costruisce il compilatore per il corpo di un METODO, COSTRUTTORE o ACCESSOR
    /// get/set di una PROPERTY di classe. I parametri partono da indice argomento 1 (0 è
    /// l'istanza implicita) — <paramref name="returnType"/> è null per un costruttore o
    /// un setter (sempre void a livello CIL).
    /// </summary>
    /// <param name="returnSymbolId">SymbolId della FUNCTION il cui return-slot la
    /// bare-assignment `NomeMetodo = espr` popola e che <c>EmitEndProcedureStatement</c>
    /// deve caricare prima del <c>ret</c>. -1 per costruttori/setter/getter (nessun
    /// return-slot condiviso con il nome del metodo). Z1: senza questo, un FUNCTION method
    /// terminava con <c>EmitDefaultValue</c> (0/""/null) invece del valore assegnato —
    /// il backend IL NON propagava il return dei metodi di classe (PRINT r.GetStatusCode()
    /// stampava 0 invece di 200).</param>
    private IlProgramCompiler(
        BasicProgram program, ILGenerator il, IlTypeContext types,
        Dictionary<int, MethodBuilder> procedureMethods, Dictionary<int, FunctionDefinition> procedureDefinitions,
        Dictionary<int, TypeBuilder> classTypes, Dictionary<int, FieldBuilder> classFields, Dictionary<int, FieldArrayInfo> classFieldArrays, Dictionary<int, List<ClassConstructorInfo>> classConstructors,
        Dictionary<int, ClassPropertyInfo> classProperties, Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        Dictionary<LambdaExpr, LambdaClosureInfo> lambdaInfos,
        ImmutableArray<Parameter> parameters, DataType? returnType, int currentClassSymbolId, int returnSymbolId = -1)
        : this(program, il, types, procedureMethods, procedureDefinitions, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos)
    {
        _currentReturnType = returnType;
        _currentReturnSymbolId = returnSymbolId;
        _isInstanceMember = true;
        _currentClassSymbolId = currentClassSymbolId;
        RegisterParameters(parameters, 1);
        InitializeNumericOrStringReturnSlot();
    }

    /// <summary>
    /// Costruisce il compilatore per il corpo "Invoke" di una lambda CON catture (vedi
    /// <see cref="LambdaClosureInfo"/>) — un metodo d'ISTANZA sul closure type sintetico
    /// (arg 0 = istanza di chiusura), come un metodo di classe, ma senza SELF/ME BASIC (una
    /// lambda non può referenziarli) e con <paramref name="capturedFields"/> popolato per
    /// reindirizzare l'accesso alle variabili catturate su campo invece che su locale/
    /// parametro (vedi EmitLoadVariable/EmitStoreVariable).
    /// </summary>
    private IlProgramCompiler(
        BasicProgram program, ILGenerator il, IlTypeContext types,
        Dictionary<int, MethodBuilder> procedureMethods, Dictionary<int, FunctionDefinition> procedureDefinitions,
        Dictionary<int, TypeBuilder> classTypes, Dictionary<int, FieldBuilder> classFields, Dictionary<int, FieldArrayInfo> classFieldArrays, Dictionary<int, List<ClassConstructorInfo>> classConstructors,
        Dictionary<int, ClassPropertyInfo> classProperties, Dictionary<(int, TokenKind, int, string), MethodBuilder> classOperators,
        Dictionary<LambdaExpr, LambdaClosureInfo> lambdaInfos,
        ImmutableArray<Parameter> lambdaParameters, DataType lambdaReturnType,
        Dictionary<int, FieldBuilder> capturedFields)
        : this(program, il, types, procedureMethods, procedureDefinitions, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos)
    {
        _currentReturnType = lambdaReturnType;
        _capturedFields = capturedFields;
        // I parametri-array della lambda sono BYVAL nel metodo Invoke (il delegate
        // Func<T[],...> non supporta ref) — rimuovi IsByRef (il parser forza byref sugli
        // array), altrimenti il corpo fa Ldarg+Ldind_Ref su un T[] → "Expected native int
        // or pointer, but got O" (InvalidProgram).
        RegisterParameters(lambdaParameters.Select(p => p with { Flags = p.Flags & ~SymbolFlags.IsByRef }).ToImmutableArray(), 1);
    }

    /// <summary>
    /// Registra ogni parametro in _parameters (ArgIndex/IsByRef/DataType, come prima —
    /// vedi ParameterInfo) e, per un parametro-array, lo materializza ANCHE in _arrays:
    /// un array-parametro è SEMPRE BYREF e SEMPRE rank 1 (la sintassi "Nome()" non permette
    /// altro, vedi MapProcedureType), quindi Ldarg+Ldind_Ref da' il riferimento all'array
    /// del chiamante — copiato in un locale reale (Stloc) cosi' che indicizzazione/FOR
    /// EACH/UBOUND/LBOUND/REDIM (che leggono tutti _arrays direttamente per SymbolId, MAI
    /// EmitLoadVariable/_parameters) funzionino esattamente come per un array DIM'd
    /// localmente, senza bisogno di percorsi separati. _arrayParamWriteback registra quali
    /// SymbolId necessitano di scrivere indietro un nuovo riferimento (dopo un REDIM) nel
    /// parametro BYREF originale — vedi EmitRedimStatement.
    /// </summary>
    private void RegisterParameters(ImmutableArray<Parameter> parameters, int argOffset)
    {
        // Fase 4 (Closure-ByRef-Box-Linker-Definitivo): se QUESTA procedura ha parametri BYREF
        // catturati da una propria closure locale che sfugge, quelle posizioni sono box-typed
        // a livello CLR (vedi MapParamClrTypeConsideringBox, usato in Host.Compile.cs per la
        // FIRMA del metodo) — _currentReturnSymbolId è già impostato dal costruttore derivato
        // PRIMA di chiamare RegisterParameters (per le procedure "normali"; per metodi/
        // costruttori di classe e lambda resta -1: limite noto, quei parametri restano T&
        // anche se in teoria catturati — non esercitato da FunctionsAdvanced.bas).
        var boxPositions = _currentReturnSymbolId >= 0
            ? GetByRefCapturedParamPositions(_program).GetValueOrDefault(_currentReturnSymbolId)
            : null;
        for (var i = 0; i < parameters.Length; i++)
        {
            var parameter = parameters[i];
            var argIndex = i + argOffset;
            var isBox = boxPositions != null && boxPositions.Contains(i) && parameter.Flags.HasFlag(SymbolFlags.IsByRef);
            _parameters[parameter.SymbolId] = new ParameterInfo(argIndex, !isBox && parameter.Flags.HasFlag(SymbolFlags.IsByRef), parameter.DataType, isBox);

            if (parameter.DataType is ArrayType arrayParamType)
            {
                var elementClrType = DataTypeMapper.Map(arrayParamType.ElementType, _types, _classTypes);
                var arrayLocal = _il.DeclareLocal(elementClrType.MakeArrayType());
                _il.Emit(OpCodes.Ldarg, argIndex);
                if (parameter.Flags.HasFlag(SymbolFlags.IsByRef))
                    _il.Emit(OpCodes.Ldind_Ref);
                _il.Emit(OpCodes.Stloc, arrayLocal);
                _arrays[parameter.SymbolId] = new ArrayInfo(arrayLocal, elementClrType.MakeArrayType(), arrayParamType.ElementType, 1, [0]);
                _arrayParamWriteback[parameter.SymbolId] = argIndex;
            }
        }
    }

    /// <summary>
    /// Compila <paramref name="program"/> in un assembly .NET persistibile su disco,
    /// pronto per <c>dotnet publish -p:PublishAot=true</c> (o per essere caricato ed
    /// eseguito in-process a scopo di test, senza pubblicazione AOT).
    /// </summary>
    /// <param name="types">
    /// Il "mondo" di tipi scalari da usare per tutta l'emissione (locali, parametri,
    /// classe base, ecc. — vedi <see cref="IlTypeContext"/> per il perché conta).
    /// Default <see cref="IlTypeContext.ForRuntime"/> — corretto per il caricamento
    /// in-process (<c>Assembly.Load</c>) nello stesso processo che genera l'assembly.
    /// Se il .dll salvato deve essere referenziato da un ALTRO progetto in
    /// compilazione (es. <c>dotnet publish -p:PublishAot=true</c> su un progetto host
    /// separato), va invece passato <see cref="IlTypeContext.ForReferenceAssemblies"/>
    /// — altrimenti il progetto consumer fallisce con "CS0012: tipo definito in un
    /// assembly di cui manca il riferimento" (System.Private.CoreLib, l'assembly di
    /// implementazione runtime usato di default, non è quello che un progetto normale
    /// referenzia implicitamente in compilazione — quello è System.Runtime.dll, un
    /// assembly di riferimento diverso pur definendo gli stessi tipi). Vedi TODO
    /// Obsidian, Filone B, B6, per lo storico completo di questo problema.
    /// </param>

    private void EmitZipStatement(ZipStmt stmt)
    {
        // ZIP source TO dest → COMPRESSIONZIP(source, dest) POP (come la VM).
        EmitExpression(stmt.Source);
        EmitExpression(stmt.Destination);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ZipLegacy), typeof(object), typeof(object)));
        _il.Emit(OpCodes.Pop);
    }

    private void EmitUnzipStatement(UnzipStmt stmt)
    {
        // UNZIP archive TO dest → COMPRESSIONUNZIP(archive, dest) POP (come la VM).
        EmitExpression(stmt.Archive);
        EmitExpression(stmt.Destination);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.UnzipLegacy), typeof(object), typeof(object)));
        _il.Emit(OpCodes.Pop);
    }

    /// <summary>Local del metodo corrente che tiene il numero di riga BASIC dello statement in
    /// esecuzione (aggiornato da EmitStatement). Serve a riportare `ERR.LineNumber` (e la
    /// posizione delle eccezioni CLR avvolte) con la riga ESATTA del programma: il chunk del
    /// dispatch ON ERROR copre più statement e per le procedure OwnerLine non è una riga vera
    /// (0/pseudo-riga). Nullo nei metodi che non lo dichiarano (lambda/handler sintetici): in
    /// quel caso si ricade sulla riga statica dello statement.</summary>
    private LocalBuilder? _currentLineLocal;

    internal void EmitStatement(Stmt stmt)
    {
        IlConversion.CurrentStatement = $"{stmt.Position.Line}:{stmt.Position.Column} {stmt.GetType().Name}";
        // Traccia la riga dello statement in esecuzione (2 istruzioni per statement, solo se la
        // riga cambia): è la fonte di ERR.LineNumber nel catch ON ERROR/TRY.
        if (_currentLineLocal is { } lineLocal && stmt.Position.Line > 0)
        {
            _il.Emit(OpCodes.Ldc_I4, stmt.Position.Line);
            _il.Emit(OpCodes.Stloc, lineLocal);
        }
        switch (stmt)
        {
            case PrintStmt p: EmitPrintStatement(p); break;
            case LetStmt l: EmitLetStatement(l); break;
            case SwapStmt sw: EmitSwapStatement(sw); break;
            case ConstStmt c: EmitConstStatement(c); break;
            case DataStmt: break; // materializzato una sola volta in EmitInitializeDataPool
            case ReadStmt read: EmitReadStatement(read); break;
            case RestoreStmt restore: EmitRestoreStatement(restore); break;
            case RandomizeStmt randomize: EmitRandomizeStatement(randomize); break;
            case SystemShellStmt shell: EmitSystemShellStatement(shell); break;
            case KeyGenStmt keyGen: EmitKeyGenStatement(keyGen); break;
            case KeyCloseStmt keyClose: EmitKeyCloseStatement(keyClose); break;
            case DeclarationStmt d: EmitDeclarationStatement(d); break;
            case RedimStmt r: EmitRedimStatement(r); break;
            case AAddStmt aadd: EmitAAddStatement(aadd); break;
            case AInsStmt ains: EmitAInsStatement(ains); break;
            case ADelStmt adel: EmitADelStatement(adel); break;
            case ASortStmt asort: EmitASortStatement(asort); break;
            case AEvalStmt aeval: EmitAEvalStatement(aeval); break;
            case AShuffleStmt ashuffle: EmitAShuffleStatement(ashuffle); break;
            case AFillStmt afill: EmitAFillStatement(afill); break;
            case ACopyStmt acopy: EmitACopyStatement(acopy); break;
            case WithStmt withStmt: EmitWithStatement(withStmt); break;
            case ClsStmt cls: EmitClsStatement(cls); break;
            case ColorStmt color: EmitColorStatement(color); break;
            case LocateStmt locate: EmitLocateStatement(locate); break;
            case ConsoleCaptionStmt caption: EmitConsoleCaptionStatement(caption); break;
            case PaletteStmt palette: EmitPaletteStatement(palette); break;
            case ViewPrintStmt view: EmitViewPrintStatement(view); break;
            case PSetStmt pset: EmitPSetStatement(pset); break;
            case PresetStmt preset: EmitPresetStatement(preset); break;
            case FontStmt font: EmitFontStatement(font); break;
            case PCopyStmt pcopy: EmitPCopyStatement(pcopy); break;
            case CircleStmt circle: EmitCircleStatement(circle); break;
            case LineStmt line: EmitLineStatement(line); break;
            case DrawStmt draw: EmitDrawStatement(draw); break;
            case PaintStmt paint: EmitPaintStatement(paint); break;
            case ImageGetStmt ig: EmitImageGetStatement(ig); break;
            case ImagePutStmt ip: EmitImagePutStatement(ip); break;
            case DrawTextStmt dt: EmitDrawTextStatement(dt); break;
            case PolygonStmt polygon: EmitPolygonStatement(polygon); break;
            case CloseStmt close: EmitCloseStatement(close); break;
            case OpenStmt open: EmitOpenStatement(open); break;
            case ZipStmt zip: EmitZipStatement(zip); break;
            // LocalTypeStmt (TYPE dichiarato nel corpo): dichiarazione di tipo, nessun codice
            // a runtime (il TypeBuilder è già registrato nel pass 0) — come la VM
            // (EmitExprAndPop → no-op).
            case LocalTypeStmt: break;
            case UnzipStmt unzip: EmitUnzipStatement(unzip); break;
            case WriteFileStmt wf: EmitWriteFileStatement(wf); break;
            case PrintFileStmt pf: EmitPrintFileStatement(pf); break;
            case InputFileStmt inf: EmitInputFileStatement(inf); break;
            case LineInputFileStmt linf: EmitLineInputFileStatement(linf); break;
            case SeekStmt seek: EmitSeekStatement(seek); break;
            case KillStmt kill: EmitKillStatement(kill); break;
            case LockStmt lk: EmitLockStatement(lk); break;
            case UnlockStmt ul: EmitUnlockStatement(ul); break;
            case TcpConnectStmt tc: EmitTcpConnectStatement(tc); break;
            case MessageConnectStmt mc: EmitMessageConnectStatement(mc); break;
            case MessageSubscribeStmt ms: EmitMessageSubscribeStatement(ms); break;
            case MessageUnsubscribeStmt mu: EmitMessageUnsubscribeStatement(mu); break;
            case MessagePublishStmt mp: EmitMessagePublishStatement(mp); break;
            case MessagePingStmt mpi: EmitMessagePingStatement(mpi); break;
            case MessageCloseStmt mcl: EmitMessageCloseStatement(mcl); break;
            case OnMessageStmt mon: EmitOnMessageStatement(mon); break;
            case TcpSendStmt ts: EmitTcpSendStatement(ts); break;
            case TcpCloseStmt tcl: EmitTcpCloseStatement(tcl); break;
            case UdpSendStmt us: EmitUdpSendStatement(us); break;
            case UdpCloseStmt uc: EmitUdpCloseStatement(uc); break;
            case UdpOpenStmt uo: EmitUdpOpenStatement(uo); break;
            case UdpRecvStmt ur: EmitUdpRecvStatement(ur); break;
            case TcpOpenStmt to: EmitTcpOpenStatement(to); break;
            case TcpAcceptStmt ta: EmitTcpAcceptStatement(ta); break;
            case TcpRecvStmt tr: EmitTcpRecvStatement(tr); break;
            case WebSocketConnectStmt wsc: EmitWebSocketStatement(wsc); break;
            case RenameStmt rename: EmitRenameStatement(rename); break;
            case MkDirStmt mkdir: EmitMkDirStatement(mkdir); break;
            case ChDirStmt chdir: EmitChDirStatement(chdir); break;
            case PixelBufferStmt pb: EmitPixelBufferStatement(pb); break;
            case CopyStmt copy: EmitCopyStatement(copy); break;
            case MoveStmt move: EmitMoveStatement(move); break;
            case RmDirStmt rmdir: EmitRmDirStatement(rmdir); break;
            case XCopyStmt xcopy: EmitXCopyStatement(xcopy); break;
            case AtSayStmt atSay: EmitAtSayStatement(atSay); break;
            case SleepStmt sleep: EmitSleepStatement(sleep); break;
            case SendStmt send: EmitSendStatement(send); break;
            case ReceiveStmt recv: EmitReceiveStatement(recv); break;
            case CloseChannelStmt cc: EmitCloseChannelStatement(cc); break;
            case MutexLockStmt ml: EmitMutexLockStatement(ml); break;
            case CondWaitStmt cw: EmitCondWaitStatement(cw); break;
            case CondSignalStmt cs: EmitCondSignalStatement(cs); break;
            case CondBroadcastStmt cb: EmitCondBroadcastStatement(cb); break;
            case MutexUnlockStmt mu: EmitMutexUnlockStatement(mu); break;
            case MutexRLockStmt mrl: EmitMutexRLockStatement(mrl); break;
            case MutexRUnlockStmt mru: EmitMutexRUnlockStatement(mru); break;
            case OnceStmt onceS: EmitOnceStatement(onceS); break;
            case DeferStmt deferS: EmitDeferStatement(deferS); break;
            case GoSelectStmt gsel: EmitGoSelectStatement(gsel); break;
            case WaitStmt wait: EmitWaitStatement(wait); break;
            case DoneStmt done: EmitDoneStatement(done); break;
            case HttpFetchStmt fetch: EmitFetchStatement(fetch); break;
            case TupleAssignmentStmt tupleAssignment: EmitTupleAssignmentStatement(tupleAssignment.VariableNames, tupleAssignment.TupleExpression, tupleAssignment); break;
            case TupleDestructuringStmt tupleDestructuring: EmitTupleAssignmentStatement(tupleDestructuring.VariableNames, tupleDestructuring.TupleExpression, tupleDestructuring); break;
            case MatrixCommandStmt matrixCommand: EmitMatrixCommandStatement(matrixCommand); break;
            case VectorCommandStmt vectorCommand: EmitVectorCommandStatement(vectorCommand); break;
            case ExpressionStmt e: EmitExpressionStatement(e); break;
            case SemanticPatchStmt: break;   // §4.10 write-side: applicato nel linker (no-op)
            case IfStmt i: EmitIfStatement(i); break;
            case ForStmt f: EmitForStatement(f); break;
            case ForCheckAndJumpStmt fc: EmitForCheckAndJumpStatement(fc); break;
            case NextStmt n: EmitNextStatement(n); break;
            case ForEachInitStmt fei: EmitForEachInitStatement(fei); break;
            case ForEachAdvanceAndAssignStmt fea: EmitForEachAdvanceAndAssignStatement(fea); break;
            case SelectStmt sel: EmitSelectStatement(sel); break;
            case MatchStmt mat: EmitMatchStatement(mat); break;
            case TryCatchStmt tc: EmitTryCatchStatement(tc); break;
            case ThrowStmt th: EmitThrowStatement(th); break;
            case RethrowStmt: _il.Emit(OpCodes.Rethrow); break;
            case ErrorStmt er: EmitErrorStatement(er); break;
            case OnErrorStmt onErr: EmitOnErrorStatement(onErr); break;
            case OnErrorResumeStmt onErrResume: EmitOnErrorResumeStatement(onErrResume); break;
            case ResumeStmt resume: EmitResumeStatement(resume); break;
            case EnterLoopStmt enterLoop: {
                var exitLabel = _il.DefineLabel();
                var continueLabel = _il.DefineLabel();
                _activeLoopStack.Push((continueLabel, exitLabel));
                _activeLoopKinds.Push(enterLoop.StartToken.Kind);
                if (!string.IsNullOrEmpty(enterLoop.ExitLabel.Lexeme))
                    _loopExitLabels[enterLoop.ExitLabel.Lexeme] = exitLabel;
                if (!string.IsNullOrEmpty(enterLoop.ContinueLabel.Lexeme))
                    _loopExitLabels[enterLoop.ContinueLabel.Lexeme] = continueLabel;
                break;
            }
            case ExitLoopStmt exitLoop: {
                var (continueLabel, exitLabel) = _activeLoopStack.Pop();
                // Il contatore FOR è attivo solo DENTRO il suo ciclo: alla chiusura del FOR
                // rimuovilo da _activeForCounters (in _forLoopState resta, ma non è più
                // "attivo" — vedi _activeForCounters).
                if (_activeLoopKinds.Count > 0 && _activeLoopKinds.Pop() == TokenKind.For && _activeForCounters.Count > 0)
                    _activeForCounters.Pop();
                _il.MarkLabel(continueLabel);
                _il.MarkLabel(exitLabel);
                break;
            }
            case LabelStmt ls: _il.MarkLabel(ResolveNamedLabel(ls.Label)); break;
            case JumpStmt j: EmitBranch(j.TargetLabel); break;
            case ConditionalJumpStmt cj: EmitConditionalJumpStatement(cj); break;
            case GotoStmt g: EmitBranch(g.Target); break;
            case EndStmt or StopStmt:
                if (_onErrorDispatch is { } endDispatch) EmitSetStateAndLeave(endDispatch.State, endDispatch.LeaveTarget, OnErrorDoneState);
                else _il.Emit(OpCodes.Ret);
                break;
            case RemStmt: break; // i commenti non generano IL
            case ReturnStmt r: EmitReturnStatement(r); break;
            case EndProcedureStmt: EmitEndProcedureStatement(); break;
            case ExitStmt exit: EmitExitStatement(exit); break;
            case ContinueStmt cont: EmitContinueStatement(cont); break;
            case GosubStmt gosub: EmitGosubStatement(gosub); break;
            case OnJumpStmt onJump: EmitOnJumpStatement(onJump); break;
            case InputStmt input: EmitInputStatement(input); break;
            case LineInputStmt lineInput: EmitLineInputStatement(lineInput); break;
            case GoProcedureStmt gp: EmitGoProcedureStatement(gp); break;
            case GoExitStmt ge: EmitGoExitStatement(ge); break;
            case YieldStmt yld: EmitYieldStatement(yld); break;
            case BeginTransactionStmt bt: EmitBeginTransactionStatement(bt); break;
            case CommitStmt cm: EmitCommitStatement(cm); break;
            case RollbackStmt rb: EmitRollbackStatement(rb); break;
            case InkeyStmt ink: EmitInkeyStatement(ink); break;
            case WaitKeyStmt wk: EmitWaitKeyStatement(wk); break;
            case GetKeyboardStmt gk: EmitGetKeyboardStatement(gk); break;
            case MouseConfigStmt mc: EmitMouseConfigStatement(mc); break;
            case MouseSetStmt ms: EmitMouseSetStatement(ms); break;
            case MouseVisibilityStmt mv: EmitMouseVisibilityStatement(mv); break;
            case MouseCursorStmt mcur: EmitMouseCursorStatement(mcur); break;
            case MouseLimitStmt ml: EmitMouseLimitStatement(ml); break;
            case MouseWaitStmt mw: EmitMouseWaitStatement(mw); break;
            case IoWatchStmt iw: EmitIoWatchStatement(iw); break;
            case IoUnwatchStmt iu: EmitIoUnwatchStatement(iu); break;
            case SetDateFormatStmt sdf: EmitSetDateFormatStatement(sdf); break;
            case SetThemeStmt sth: EmitSetThemeStatement(sth); break;
            case PaletteDefineStmt pd: EmitPaletteDefineStatement(pd); break;
            case PaletteUseStmt pu: EmitPaletteUseStatement(pu); break;
            case PaletteLoadStmt pl: EmitPaletteLoadStatement(pl); break;
            case PaletteSaveStmt ps: EmitPaletteSaveStatement(ps); break;
            case PaletteReplaceStmt pr: EmitPaletteReplaceStatement(pr); break;
            case JsonSaveStmt js: EmitJsonSaveStatement(js); break;
            case JsonWriteValueStmt jwv: EmitJsonWriteValueStatement(jwv); break;
            case JsonWriteSectionStmt jws: EmitJsonWriteSectionStatement(jws); break;
            case SqlBackupStmt sback: EmitSqlBackupStatement(sback); break;
            case SqlCloseStmt scl: EmitSqlCloseStatement(scl); break;
            case SqlCloseResultSetStmt scrs: EmitSqlCloseResultSetStatement(scrs); break;
            case SqlDetachStmt sd: EmitSqlDetachStatement(sd); break;
            case SqlBeginTransactionStmt sbt:
                EmitSqlAliasObject(sbt.DbAlias);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlTransactionBegin), typeof(object)));
                break;
            case SqlCommitStmt scm:
                EmitSqlAliasObject(scm.DbAlias);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlTransactionCommit), typeof(object)));
                break;
            case SqlRollbackStmt srb:
                EmitSqlAliasObject(srb.DbAlias);
                if (srb.SavepointName is not null)
                {
                    EmitStringArg(srb.SavepointName);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlRollbackToSavepoint), typeof(object), typeof(string)));
                }
                else _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlTransactionRollback), typeof(object)));
                break;
            case SqlSavepointStmt ssp:
                EmitSqlAliasObject(ssp.DbAlias);
                EmitStringArg(ssp.Name);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlSavepoint), typeof(object), typeof(string)));
                break;
            case SqlReleaseStmt srl:
                EmitSqlAliasObject(srl.DbAlias);
                EmitStringArg(srl.Name);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlReleaseSavepoint), typeof(object), typeof(string)));
                break;
            case SqlOpenStmt so: EmitSqlOpenStatement(so); break;
            case SqlExecStmt se: EmitSqlExecStatement(se); break;
            case SqlQueryStmt sq: EmitSqlQueryStatement(sq); break;
            case SqlAttachStmt sa: EmitSqlAttachStatement(sa); break;
            // --- DBF statement (porting da GM-Basic/GMBasicAI IlBackend): delega a
            //     IDatabaseService tramite i metodi runtime IlRuntime.Dbf* (area corrente).
            case DbfUseStmt duse: EmitDbfUseStatement(duse); break;
            case DbfSelectWorkAreaStmt dsw: EmitObjectBoxedArgument(dsw.WorkArea); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSelectWorkArea), typeof(object))); break;
            // DBF CLOSE DATABASES (WorkAreas vuoto) = chiudi TUTTO; DBF CLOSE <alias>[, ...] =
            // chiudi SOLO le aree indicate. Prima QUALSIASI DBF CLOSE compilava a DbfCloseAll →
            // un `DBF CLOSE Temp` dopo un'area principale chiudeva anche quella, e il comando
            // successivo sull'area corrente (es. DBF AGGREGATE) non trovava più l'area.
            case DbfCloseStmt dcl:
                if (dcl.WorkAreas.IsEmpty)
                {
                    // CLOSE DATABASES/ALL → tutte le aree; CLOSE nudo → SOLO la corrente
                    // (prima chiudeva tutto anche per il CLOSE nudo, rompendo le altre aree).
                    if (dcl.All)
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCloseAll)));
                    else
                    {
                        _il.Emit(OpCodes.Ldnull);
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfClose), typeof(string)));
                    }
                }
                else
                    foreach (var wa in dcl.WorkAreas)
                    {
                        if (wa is StringExpr dce) _il.Emit(OpCodes.Ldstr, dce.Value);
                        else if (wa is VariableExpr dcv) _il.Emit(OpCodes.Ldstr, dcv.NameCleaned.Lexeme);
                        else EmitStringArg(wa);
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfClose), typeof(string)));
                    }
                break;
            case DbfGoStmt dgo: EmitExpression(dgo.Destination); var goClr = DataTypeMapper.Map(NormalizeInferredType(InferType(dgo.Destination)), _types, _classTypes); if (goClr.IsValueType) _il.Emit(OpCodes.Box, goClr); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfGo), typeof(object), typeof(string))); break;
            case DbfSkipStmt dsk: if (dsk.RecordCount is null) _il.Emit(OpCodes.Ldc_R8, 1.0); else EmitExpression(dsk.RecordCount); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSkip), typeof(double), typeof(string))); break;
            case DbfSeekStmt dse2: EmitExpression(dse2.Value); var skClr = DataTypeMapper.Map(NormalizeInferredType(InferType(dse2.Value)), _types, _classTypes); if (skClr.IsValueType) _il.Emit(OpCodes.Box, skClr); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSeek), typeof(object), typeof(string))); _il.Emit(OpCodes.Pop); break;
            case DbfSetOrderStmt dso: EmitExpression(dso.Value); var ordClr = DataTypeMapper.Map(NormalizeInferredType(InferType(dso.Value)), _types, _classTypes); if (ordClr.IsValueType) _il.Emit(OpCodes.Box, ordClr); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetOrder), typeof(object), typeof(string))); break;
            case DbfSetFilterStmt dsf: if (dsf.Condition is null) _il.Emit(OpCodes.Ldnull); else if (dsf.Condition is LambdaExpr sfl && sfl.ExpressionBody is not null) _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(sfl.ExpressionBody)); else if (dsf.Condition is StringExpr or VariableExpr) EmitStringArg(dsf.Condition); else _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(dsf.Condition)); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetFilter), typeof(string), typeof(string))); break;
            case DbfSetDeletedStmt dsd: _il.Emit(dsd.State.Lexeme.Equals("ON", StringComparison.OrdinalIgnoreCase) ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetDeleted), typeof(bool), typeof(string))); break;
            case DbfSetExactStmt dsx: _il.Emit(dsx.State.Lexeme.Equals("ON", StringComparison.OrdinalIgnoreCase) ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetExact), typeof(bool), typeof(string))); break;
            case DbfSetDefaultStmt dsd2: EmitStringArg(dsd2.Path); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetDefaultPath), typeof(string))); break;
            case DbfSetPathStmt dsp: EmitStringArg(dsp.PathList); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetSearchPath), typeof(string))); break;
            case DbfAppendBlankStmt dab: _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAppendBlank), typeof(string))); break;
            // DBF DELETE/RECALL [scope]: ddl.Scope/drc.Scope erano del tutto ignorati (sempre
            // solo il record corrente, indipendentemente da ALL/FOR/NEXT/REST — stesso bug/
            // causa già risolto per DBF REPLACE). EmitDbfScopedStatement usa lo stesso motore
            // scope-aware di RDD DELETE/RECALL (CoreDb.ScopedDelete/ScopedRecall).
            case DbfDeleteStmt ddl: EmitDbfScopedStatement(ddl.Scope, nameof(IlRuntime.DbfDeleteScoped)); break;
            case DbfRecallStmt drc: EmitDbfScopedStatement(drc.Scope, nameof(IlRuntime.DbfRecallScoped)); break;
            case DbfPackStmt dpk: _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfPack), typeof(string))); break;
            case DbfZapStmt dzp: _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfZap), typeof(string))); break;
            case DbfReindexStmt drx: _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfReindex), typeof(string))); break;
             // Dbf complessi/legacy: dispatch ai runtime method reali (IlRuntime.Dbf*).
             case DbfCreateStmt dcr:
             {
                 // CREATE <path> FROM TYPE <t>: definisce il file .dbf con i campi del TYPE.
                 EmitStringArg(dcr.FileName);
                 var udt = _program.Symbols.FindSymbolsByName(dcr.TypeNameToken.Lexeme)
                     .Select(s => s.DataType).OfType<UserDefinedType>().FirstOrDefault();
                 var defs = udt is null ? "" : string.Join(",",
                     udt.Fields.Select(f => $"{f.Name.Lexeme}={DbfCreateTypeName(f.DataType)}={DbfCreateFieldLen(f.DataType)}"));
                 _il.Emit(OpCodes.Ldstr, defs);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCreate), typeof(string), typeof(string)));
                 break;
             }
             case DbfAppendRecordStmt dar2:
             {
                 // `DBF APPEND RECORD {Campo: valore, ...}` (literal UDT, es.
                 // DbfRelationJoin.bas "DBF APPEND RECORD {ID: "A", NAME: "ALICE"}"): un
                 // DictionaryLiteralExpr non è né un ArrayLiteralExpr né una VariableExpr UDT —
                 // EmitObjectArrayArg cadeva nel fallback finale (object[0] sempre vuoto),
                 // stesso bug/causa già risolto per SPEED PUT (vedi SpeedPutStmt): ogni record
                 // veniva scritto senza campi, spiegando letture "0"/vuote a valle (bare field
                 // ITEM dopo SELECT ORD, SET RELATION/JOIN che non trovano corrispondenze).
                 if (dar2.Source is DictionaryLiteralExpr dar2Dict)
                 {
                     EmitDictionaryLiteral(dar2Dict, new DictType(new StringType(), new AnyType()));
                     _il.Emit(OpCodes.Ldnull);
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAppendRecordByName), typeof(Dictionary<string, object>), typeof(string)));
                 }
                 else
                 {
                     // APPEND RECORD <values> (array/UDT): aggiunge una riga.
                     EmitObjectArrayArg(dar2.Source);
                     _il.Emit(OpCodes.Ldnull);
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAppendRecord), typeof(object[]), typeof(string)));
                     _il.Emit(OpCodes.Pop);
                 }
                 break;
             }
             case DbfAppendFromStmt daf:
             {
                 // DBF APPEND FROM "file" [FOR cond] — era un no-op (EmitDatabaseNoop):
                 // i record del file sorgente NON venivano copiati, in silenzio. Ora delega
                 // a IlRuntime.DbfAppendFrom (stesso pattern di DbfCopyTo). Parità PCodeVM
                 // (SysCall.DBF_APPEND_FROM → engine.AppendFrom).
                 EmitStringArg(daf.FileName);
                 if (daf.Scope?.Condition is not null) _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(daf.Scope.Condition));
                 else _il.Emit(OpCodes.Ldnull);
                 _il.Emit(OpCodes.Ldnull);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAppendFrom), typeof(string), typeof(object), typeof(string)));
                 break;
             }
              case DbfGetRecordStmt dgr:
              {
                  // GET RECORD <dest>: destinazione UDT (record tipizzato) → object[] copiato
                  // campo per campo; destinazione INFERITA (`DBF GET clienteRec`, late-binding)
                  // → dict nome→valore letto da DynamicFieldGet. Il valore va prodotto DENTRO
                  // emitValue: per global/static lo store pusha il symbolId PRIMA del valore.
                  var dstIsUdtGet = dgr.Destination is VariableTarget dvGet
                      && NormalizeInferredType(_program.Symbols.GetSymbol(dvGet.SymbolId).DataType) is UserDefinedType;
                  if (dgr.Destination is VariableTarget dvtGet)
                  {
                      if (dstIsUdtGet)
                      {
                          _il.Emit(OpCodes.Ldnull);
                          _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfGetRecord)));
                          EmitCopyObjectArrayToTarget(dgr.Destination);
                      }
                      else
                      {
                          EmitStoreVariable(dvtGet.SymbolId, () =>
                          {
                              _il.Emit(OpCodes.Ldnull);
                              _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfGetRecordDict)));
                          });
                      }
                  }
                  else
                  {
                      _il.Emit(OpCodes.Ldnull);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfGetRecordDict)));
                      _il.Emit(OpCodes.Pop);
                  }
                 break;
             }
             case DbfReplaceStmt drp:
             {
                 // REPLACE <field> WITH <value> [...] [scope]: aggiorna i campi dei record in
                 // scope (ALL/NEXT/RECORD/REST/FOR/WHILE, default: solo il record corrente).
                 // FieldTarget.FieldSymbolId qui è l'ID GLOBALE del campo nel Linker (assegnato
                 // da RewriteDbfReplaceStatement via CurrentUdtScopeId), NON l'indice posizionale
                 // 0-based del driver — passarlo come indice era un no-op silenzioso. Si usa
                 // invece il NOME del campo (OriginalToken), stesso pattern late-binding già
                 // usato in lettura da EmitFieldAccess/DbfLateBindField e in scrittura da
                 // DbfAppendRecordByName.
                 //
                 // Senza `AS Type` su DBF USE (puro Clipper late-binding, es. `DBF USE f ALIAS X`
                 // senza tipo), CurrentUdtScopeId non risolve mai un UDT: RewriteTarget non
                 // costruisce un FieldTarget e il target resta un VariableTarget nudo non
                 // risolto (stesso ramo late-binding di ITEM/NAME in lettura). Il case sotto va
                 // gestito ANCHE per questo target, altrimenti REPLACE resta un no-op silenzioso
                 // ogni volta che DBF USE non specifica AS Type.
                 //
                 // La clausola di scope (drp.Scope) era del tutto ignorata: "REPLACE Nome WITH
                 // X ALL" applicava SEMPRE solo al record su cui il cursore si trovava per
                 // caso (scoperto verificando 3 record: solo l'ultimo APPESO veniva
                 // modificato). Ora usa lo stesso motore scope-aware di RDD REPLACE
                 // (CoreDb.ScopedReplace via DbfReplaceScoped), dizionario per nome campo.
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddNewReplaceDict)));
                 // Valori valutati PRIMA di costruire il dict (e di qualunque effetto): la
                 // lettura late-binding del valore (`REPLACE Saldo WITH Saldo + 50`) deve
                 // riflettere lo stato PRE-REPLACE. Valutata dentro il loop con Dup/Ldstr sullo
                 // stack, il bind restituiva un valore già alterato/0 (verificato: 50 invece di
                 // 150 su IL e VM/C; c99n la valutano prima → 150).
                 var replValueLocs = new LocalBuilder[drp.Replacements.Length];
                 for (var rvi = 0; rvi < drp.Replacements.Length; rvi++)
                 {
                     var rvv = drp.Replacements[rvi];
                     if (rvv.Target is MacroTarget) continue;
                     EmitExpression(rvv.Value);
                     var rvClrHoist = DataTypeMapper.Map(NormalizeInferredType(InferType(rvv.Value)), _types, _classTypes);
                     if (rvClrHoist.IsValueType) _il.Emit(OpCodes.Box, rvClrHoist);
                     var rvLoc = _il.DeclareLocal(typeof(object));
                     _il.Emit(OpCodes.Stloc, rvLoc);
                     replValueLocs[rvi] = rvLoc;
                 }
                 var replIdx = -1;
                 foreach (var repl in drp.Replacements)
                 {
                     replIdx++;
                     string fieldName;
                     if (repl.Target is FieldTarget replFt) fieldName = replFt.OriginalToken.Lexeme;
                     else if (repl.Target is VariableTarget replVt) fieldName = replVt.NameCleaned.Lexeme;
                     else if (repl.Target is MacroTarget replMt && replMt.Expression is MacroExpr replMm)
                     {
                         // `REPLACE &cf WITH v` (parità PCode): il NOME campo è il VALORE runtime
                         // dell'espressione INTERNA della macro (cf="BALANCE" → campo BALANCE).
                         // MAI MacroEvalExpr (eseguirebbe il testo come codice) — EmitExpression
                         // sull'interno valuta la variabile che produce la stringa.
                         _il.Emit(OpCodes.Dup); // dict
                         EmitExpression(replMm.Expression);
                         EmitConvertToString(InferType(replMm.Expression));
                         EmitExpression(repl.Value);
                         var rvClrMacro = DataTypeMapper.Map(NormalizeInferredType(InferType(repl.Value)), _types, _classTypes);
                         if (rvClrMacro.IsValueType) _il.Emit(OpCodes.Box, rvClrMacro);
                         _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddReplaceDictSet),
                             typeof(System.Collections.Generic.Dictionary<string, object?>), typeof(string), typeof(object)));
                         continue;
                     }
                     else continue;
                     _il.Emit(OpCodes.Dup); // dict
                     _il.Emit(OpCodes.Ldstr, fieldName);
                     if (replValueLocs[replIdx] is not null)
                         _il.Emit(OpCodes.Ldloc, replValueLocs[replIdx]);
                     else
                     {
                         EmitExpression(repl.Value);
                         var rvClr = DataTypeMapper.Map(NormalizeInferredType(InferType(repl.Value)), _types, _classTypes);
                         if (rvClr.IsValueType) _il.Emit(OpCodes.Box, rvClr);
                     }
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddReplaceDictSet),
                         typeof(System.Collections.Generic.Dictionary<string, object?>), typeof(string), typeof(object)));
                 }
                 EmitScopeClauseArgs(drp.Scope);
                 _il.Emit(OpCodes.Ldnull); // alias (usa l'alias DBF corrente)
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfReplaceScoped),
                     typeof(object), typeof(int), typeof(object), typeof(object), typeof(string)));
                 break;
             }
             case DbfIndexOnStmt dind:
             {
                 // INDEX ON <key> TO <file>: crea un indice NTX/NDX. Il motore (DbfEngine)
                 // re-imposta il testo della chiave sul proprio Evaluator per RI-valutarla a
                 // ogni Seek contro il record trovato (stesso motivo di SET RELATION/SET
                 // FILTER/JOIN FOR, vedi ExprPlain.cs) — serve il NOME campo ("CID"), non il
                 // suo VALORE. `EmitStringArg` valutava l'espressione a runtime: con un campo
                 // nudo late-bound (Clipper) questo significava eseguire una lettura DBF nel
                 // punto in cui INDEX ON viene compilato, il cui risultato (es. "0" per un
                 // record non ancora posizionato) diventava la chiave stessa — l'indice veniva
                 // costruito e riletto correttamente, ma OGNI successivo Seek rivalutava
                 // l'espressione "0" (letterale, non più un nome campo) invece del vero valore
                 // del campo nel record trovato, quindi non trovava mai un match (scoperto dal
                 // vivo: DBF SET RELATION + GO TOP su DbfRelationJoin.bas lasciava il figlio a
                 // EOF nonostante SyncRelation calcolasse correttamente il valore da cercare).
                 _il.Emit(OpCodes.Ldstr, ExprPlain.From(dind.KeyExpression));
                 EmitStringArg(dind.FileName);
                 _il.Emit(OpCodes.Ldc_I4, 0);
                 _il.Emit(OpCodes.Ldnull);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfIndexOn), typeof(string), typeof(string), typeof(int), typeof(string)));
                 break;
             }
              case DbfCopyStructureStmt dcs:
              {
                  EmitStringArg(dcs.FileName);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCopyStructure), typeof(string), typeof(string)));
                  break;
              }
              case DbfCopyToStmt dct:
              {
                  EmitStringArg(dct.FileName);
                  if (dct.Scope?.Condition is not null) _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(dct.Scope.Condition));
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCopyTo), typeof(string), typeof(object), typeof(string)));
                  break;
              }
              case DbfJoinWithStmt djw:
              {
                  _il.Emit(OpCodes.Ldstr, djw.OriginalAliasToken.Lexeme);
                  EmitStringArg(djw.FileName);
                  var jCond = djw.Condition.Condition is LambdaExpr jl && jl.ExpressionBody is not null
                      ? jl.ExpressionBody : djw.Condition.Condition;
                  _il.Emit(OpCodes.Ldstr, ExprPlain.FromQualified(jCond));
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfJoin), typeof(string), typeof(string), typeof(object), typeof(string)));
                  break;
              }
              case DbfSortStmt dsrt:
              {
                  EmitStringArg(dsrt.FileName);
                  var sFields = dsrt.SortFields.Select(f => f.OriginalToken.Lexeme).ToArray();
                  var sDesc = dsrt.SortFields.Select(f => f.Descending).ToArray();
                  _il.Emit(OpCodes.Ldc_I4, sFields.Length);
                  _il.Emit(OpCodes.Newarr, _types.String);
                  for (var i = 0; i < sFields.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(OpCodes.Ldstr, sFields[i]); _il.Emit(OpCodes.Stelem_Ref);
                  }
                  _il.Emit(OpCodes.Ldc_I4, sDesc.Length);
                  _il.Emit(OpCodes.Newarr, typeof(bool));
                  for (var i = 0; i < sDesc.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(sDesc[i] ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Stelem_I1);
                  }
                  var sCond = dsrt.Scope is DbfForClause sf ? SerializeFilterCondition(sf.Condition) : null;
                  _il.Emit(OpCodes.Ldnull); // alias
                  if (sCond is null) _il.Emit(OpCodes.Ldnull); else _il.Emit(OpCodes.Ldstr, sCond);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSortTo), typeof(string), typeof(string[]), typeof(bool[]), typeof(string), typeof(string)));
                  break;
              }
              case DbfUpdateStmt dud:
              {
                  // Chiave come NOME CAMPO (non valore): serve al confronto destinazione/sorgente.
                  var ukeyName = dud.KeyExpression is FieldAccessExpr kfa ? kfa.Field.Lexeme
                               : dud.KeyExpression is VariableExpr kve ? kve.NameCleaned.Lexeme
                               : ExprPlain.From(dud.KeyExpression);
                  _il.Emit(OpCodes.Ldstr, ukeyName);
                  var uNames = dud.Replacements.Select(r => r.NameCleaned.Lexeme).ToArray();
                  var uExprs = dud.Replacements.Select(r => ExprPlain.FromQualified(r.Value)).ToArray();
                  _il.Emit(OpCodes.Ldc_I4, uNames.Length);
                  _il.Emit(OpCodes.Newarr, _types.String);
                  for (var i = 0; i < uNames.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(OpCodes.Ldstr, uNames[i]); _il.Emit(OpCodes.Stelem_Ref);
                  }
                  _il.Emit(OpCodes.Ldc_I4, uExprs.Length);
                  _il.Emit(OpCodes.Newarr, _types.String);
                  for (var i = 0; i < uExprs.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(OpCodes.Ldstr, uExprs[i]); _il.Emit(OpCodes.Stelem_Ref);
                  }
                  // Alias SORGENTE di UPDATE ON <key> FROM <alias> (il destino è l'area corrente).
                  _il.Emit(OpCodes.Ldstr, dud.OriginalAliasToken.Lexeme);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfUpdate), typeof(string), typeof(string[]), typeof(string[]), typeof(string)));
                  break;
              }
              case DbfTotalOnStmt dtot:
              {
                  EmitStringArg(dtot.FileName);
                  _il.Emit(OpCodes.Ldstr, dtot.KeyExpression is FieldAccessExpr tkf ? tkf.Field.Lexeme
                      : dtot.KeyExpression is VariableExpr tkv ? tkv.NameCleaned.Lexeme
                      : ExprPlain.From(dtot.KeyExpression));
                  var tFields = dtot.Fields?.FieldExpressions.Select(f => ExprPlain.From(f)).ToArray() ?? [];
                  _il.Emit(OpCodes.Ldc_I4, tFields.Length);
                  _il.Emit(OpCodes.Newarr, _types.String);
                  for (var i = 0; i < tFields.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(OpCodes.Ldstr, tFields[i]); _il.Emit(OpCodes.Stelem_Ref);
                  }
                  if (dtot.Scope?.Condition is not null) _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(dtot.Scope.Condition));
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfTotalOn), typeof(string), typeof(string), typeof(string[]), typeof(object), typeof(string)));
                  break;
              }
              case DbfValidateStmt dval:
              {
                  if (dval.Area is VariableExpr dvArea) _il.Emit(OpCodes.Ldstr, dvArea.NameCleaned.Lexeme);
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfValidateStmt), typeof(string)));
                  break;
              }
              case DbfListStmt dl:
              {
                  var lf = dl.Fields is null ? "" : string.Join(",", dl.Fields.FieldExpressions.Select(f => ExprPlain.From(f)));
                  _il.Emit(OpCodes.Ldstr, lf);
                  if (dl.Destination is StringExpr dlse) _il.Emit(OpCodes.Ldstr, dlse.Value);
                  else if (dl.Destination is not null) EmitStringArg(dl.Destination);
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(dl.ToPrint ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  var listCond = dl.Scope switch
                  {
                      DbfForClause lfc => lfc.Condition,
                      DbfWhileClause lwc => lwc.Condition,
                      _ => null
                  };
                  if (listCond is not null)
                  {
                      // LIST ... FOR/WHILE: condizione serializzata, valutata per record.
                      _il.Emit(OpCodes.Ldstr, SerializeFilterCondition(listCond));
                      _il.Emit(OpCodes.Ldnull);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfListCond), typeof(string), typeof(string), typeof(bool), typeof(string), typeof(string)));
                  }
                  else
                  {
                      _il.Emit(OpCodes.Ldnull);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfList), typeof(string), typeof(string), typeof(bool), typeof(string)));
                  }
                  break;
              }
              case DbfReportStmt dr2:
              {
                  _il.Emit(OpCodes.Ldstr, ExprPlain.From(dr2.FormFile));
                  if (dr2.Destination is StringExpr drse) _il.Emit(OpCodes.Ldstr, drse.Value);
                  else if (dr2.Destination is not null) EmitStringArg(dr2.Destination);
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(dr2.ToPrint ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfReport), typeof(string), typeof(string), typeof(bool), typeof(string)));
                  break;
              }
              case DbfAggregateStmt dag:
              {
                  _il.Emit(OpCodes.Ldstr, dag.KeyExpression is FieldAccessExpr dkf ? dkf.Field.Lexeme : ExprPlain.From(dag.KeyExpression));
                  _il.Emit(OpCodes.Ldstr, dag.FileName is StringExpr dafse ? dafse.Value : ExprPlain.From(dag.FileName));
                  var aggs = dag.AggregationsExpr is ArrayLiteralExpr dgar
                      ? string.Join(",", dgar.Elements.OfType<StringExpr>().Select(s => s.Value))
                      : (dag.AggregationsExpr is null ? "" : ExprPlain.From(dag.AggregationsExpr));
                  _il.Emit(OpCodes.Ldstr, aggs);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAggregate), typeof(string), typeof(string), typeof(string), typeof(string)));
                  break;
              }
              case DbfCreateViewStmt dcv:
              {
                  _il.Emit(OpCodes.Ldstr, dcv.ViewNameToken.Lexeme);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfCreateView), typeof(string)));
                  break;
              }
              case DbfSetRelationStmt dsrel:
              {
                  // ExprPlain.From ricostruisce testo BASIC pulito e RI-PARSABILE (niente
                  // suffissi #SymbolId) — a differenza del vecchio EmitStringArg (che VALUTAVA
                  // l'espressione UNA volta al momento di SET RELATION, congelando un valore
                  // stantio invece del testo), il runtime ora la ricompila e rivaluta a ogni
                  // navigazione del padre tramite Basic.Databases.KeyExpressionPCodeHook (vedi
                  // DbfDriver.SyncRelation). Il caso semplice (nome di campo) funzionava già;
                  // questo fix riguarda le espressioni composte (es. `TO STR(ID)+X`).
                  if (dsrel.KeyExpression is not null) _il.Emit(OpCodes.Ldstr, ExprPlain.From(dsrel.KeyExpression));
                  else _il.Emit(OpCodes.Ldnull);
                  if (dsrel.ChildAlias is not null) _il.Emit(OpCodes.Ldstr, dsrel.ChildAlias.Lexeme);
                  else _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetRelation), typeof(string), typeof(string), typeof(string)));
                  break;
              }
              case DbfSetExclusiveStmt dsex:
              {
                  _il.Emit(dsex.State.Kind == TokenKind.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetExclusive), typeof(bool), typeof(string)));
                  break;
              }
              case DbfSetSchemaStmt dsch:
              {
                  _il.Emit(dsch.State.Kind == TokenKind.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  _il.Emit(OpCodes.Ldnull);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetSchemaStrict), typeof(bool), typeof(string)));
                  break;
              }
             // ============================================================
             // ISAM — delega a IDatabaseService via IlRuntime.Isam*
             // ============================================================
             case IsamOpenStmt iopen:
             {
                 // Passa lo schema (typeName, campi UDT, PK, indici) per costruire l'engine.
                 EmitStringArg(iopen.FileName);
                 var iopenAlias = iopen.OriginalAliasToken.Lexeme;
                 _il.Emit(OpCodes.Ldstr, iopenAlias);
                 var isamOpenSchema = BuildIsamSchemaJson(iopen.OriginalTypeName.Lexeme, iopen.IndexedBy);
                 _il.Emit(OpCodes.Ldstr, isamOpenSchema);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamOpenSchema), typeof(string), typeof(string), typeof(string)));
                 _il.Emit(OpCodes.Pop);
                 break;
             }
             case IsamUseStmt iuse:
             {
                 // Passa lo schema (typeName, campi UDT, chiave primaria, indici) al servizio
                 // per costruire l'engine ISAM — pattern PCodeVM ISAM_USE.
                 EmitStringArg(iuse.FileName);
                 var iuseAlias = iuse.OriginalAliasToken.Lexeme;
                 _il.Emit(OpCodes.Ldstr, iuseAlias);
                 var isamSchemaJson = BuildIsamSchemaJson(iuse.OriginalTypeName.Lexeme, iuse.IndexedBy);
                 _il.Emit(OpCodes.Ldstr, isamSchemaJson);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamOpenSchema), typeof(string), typeof(string), typeof(string)));
                 _il.Emit(OpCodes.Pop);
                 break;
             }
             case IsamAppendStmt iapp:
             {
                 EmitObjectArrayArg(iapp.Source);
                 EmitIsamAliasArg(iapp.FileNumber);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamInsert), typeof(object[]), typeof(string)));
                 break;
             }
             case IsamPutStmt iput:
             {
                 // ISAM PUT alias [, udtVar] — modifica il record CORRENTE. Prima il parser
                 // produceva PutFileStmt (I/O file) → PutFile su handle invalido. Ora delega
                 // a IlRuntime.IsamPut (pattern PCodeVM ISAM_PUT).
                 EmitObjectArrayArg(iput.Source ?? new ArrayLiteralExpr([], null!));
                 EmitIsamAliasArg(iput.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamPut), typeof(object[]), typeof(string)));
                 break;
             }
             case ConvertDbfToIsamStmt cdis:
             {
                 // CONVERT DBF "src" TO ISAM "dst" TYPE <UDT> KEY <pk> LEN <n> — pattern PCode.
                 EmitStringArg(cdis.SourceFile);
                 EmitStringArg(cdis.DestinationFile);
                 _il.Emit(OpCodes.Ldstr, cdis.OriginalTypeName.Lexeme);
                 _il.Emit(OpCodes.Ldstr, cdis.OriginalPkFieldName.Lexeme);
                 EmitDoubleArgument(cdis.KeyLength);
                 _il.Emit(OpCodes.Ldc_R8, cdis.IsUnsafe ? 1.0 : 0.0);
                 // Schema JSON (nome+tipo REALE di ogni campo del TYPE, stesso formato/builder
                 // di ISAM OPEN/SPEED OPEN): IlRuntime non ha accesso alla SymbolTable, solo il
                 // compilatore può risolvere i tipi effettivi del TYPE — senza, IsamConvertDbf
                 // scriveva lo schema ISAM coi tipi semplificati da TranslateDbfSchemaToIsam
                 // (ogni Numeric DBF diventa DoubleType), mai compatibile col vero TYPE al
                 // successivo ISAM OPEN ... OF <UDT>.
                 _il.Emit(OpCodes.Ldstr, BuildIsamSchemaJson(cdis.OriginalTypeName.Lexeme, null));
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamConvertDbf), typeof(string), typeof(string), typeof(string), typeof(string), typeof(double), typeof(double), typeof(string)));
                 _il.Emit(OpCodes.Pop);
                 break;
             }
             case IsamSkipStmt isk:
             {
                 if (isk.RecordCount is null) EmitDouble(1.0); else EmitExpression(isk.RecordCount);
                 EmitIsamAliasArg(isk.FileNumber);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSkip), typeof(double), typeof(string)));
                 break;
             }
             case IsamSeekStmt iseek:
             {
                 EmitExpression(iseek.PositionExpr);
                 var iskClr = DataTypeMapper.Map(NormalizeInferredType(InferType(iseek.PositionExpr)), _types, _classTypes);
                 if (iskClr.IsValueType) _il.Emit(OpCodes.Box, iskClr);
                 EmitIsamAliasArg(iseek.FileNumber);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSeek), typeof(object), typeof(string)));
                 break;
             }
             case IsamSetOrderStmt isord:
             {
                 EmitStringArg(isord.IndexName);
                 EmitIsamAliasArg(isord.FileNumber);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSetOrder), typeof(string), typeof(string)));
                 break;
             }
             case IsamRecordStmt irecGo:
             {
                 // GO #file, TOP/BOTTOM/<recno> → IsamGo. TOP → 0, BOTTOM → -2 (sentinella
                 // come SpeedGo), altrimenti il recno. Prima emetteva SEMPRE 0.0 → ogni GO
                 // diventava GoTop e BOTTOM era ignorato.
                 if (irecGo.Location is { } loc && loc.Kind == TokenKind.Bottom)
                     EmitDouble(-2.0);
                 else
                     EmitDouble(0.0);
                 EmitIsamAliasArg(irecGo.FileNumber);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamGo), typeof(double), typeof(string)));
                 break;
             }
             case IsamPackStmt ipack: EmitIsamAliasArg(ipack.FileNumber); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamPack), typeof(string))); break;
             case IsamReindexStmt ireidx: EmitIsamAliasArg(ireidx.FileNumber); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamReindex), typeof(string))); break;
              case IsamRecallStmt irec:
              {
                  _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(irec.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamRecall), typeof(object), typeof(string)));
                  break;
              }
              case IsamDeleteStmt idel:
              {
                  _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(idel.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamDelete), typeof(object), typeof(string)));
                  break;
              }
              case IsamSetFilterStmt isf:
              {
                  // Il runtime IlRuntime.IsamSetFilter riceve la condizione come STRINGA e la
                  // ri-parsa con RuntimeFilterParser (CreateEvaluator(condition?.ToString())),
                  // come la PCodeVM (stringa per macro &(expr), stringa runtime altrimenti).
                  // NON valutare la condizione a compile-time: `Citta = "Roma"` ha a sinistra
                  // un campo bare ISAM (tipo Inferred → emette un double 0.0 di default) e a
                  // destra una STRING → isString=true → String.Equals(double, string) = IL
                  // invalido. Passando il TESTO della condizione si delega il parse al runtime.
                  // Eccezione: macro `&(f)` -> il testo del filtro e' il VALORE (runtime) di `f`, non il suo nome:
                  // ExprPlain.From(MacroExpr) ritorna il testo dell'interno ("f"), che l'evaluator trovava sempre falso
                  // (IL: 0 righe). Come DBF/SPEED/PCode: si emette l'espressione interna come stringa a runtime.
                  if (isf.Condition is MacroExpr isfMacro)
                      EmitStringArg(isfMacro.Expression);
                  else if (isf.Condition is not null)
                      _il.Emit(OpCodes.Ldstr, ExprPlain.FilterText(isf.Condition));
                  else _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(isf.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSetFilter), typeof(object), typeof(string)));
                  break;
              }
             case IsamCreateIndexStmt ici:
             {
                 EmitStringArg(ici.IndexName);
                 // La KeyExpression è il TESTO della chiave (nome campo o espressione), NON un
                 // valore da valutare: EmitStringArg(VariableExpr "Saldo") caricava la variabile
                 // (mai dichiarata) → "0" → indice con chiavi tutte 0 → restituiva l'ordine di
                 // inserimento invece del proprio (CREATE INDEX su campo non-PK, §4 Saldo).
                 // Stesso testo del PCode (ExprPlain.FilterText: VariableExpr→nome,
                 // StringExpr→valore, altro→testo senza suffissi #SymbolId).
                 _il.Emit(OpCodes.Ldstr, ExprPlain.FilterText(ici.KeyExpression));
                 EmitExpression(ici.KeyLength);
                  EmitIsamAliasArg(ici.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamCreateIndex), typeof(string), typeof(string), typeof(double), typeof(string)));
                  break;
              }
              case IsamDropIndexStmt idi:
              {
                  EmitStringArg(idi.IndexName);
                  EmitIsamAliasArg(idi.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamDropIndex), typeof(string), typeof(string)));
                  break;
              }
              case IsamRepairStmt irp:
              {
                  EmitIsamAliasArg(irp.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamRepair), typeof(string)));
                  break;
              }
              case IsamBackupStmt ibk:
              {
                  EmitStringArg(ibk.DestinationFile);
                  EmitIsamAliasArg(ibk.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamBackup), typeof(string), typeof(string)));
                  break;
              }
              case IsamValidateStmt iv:
              {
                  EmitIsamAliasArg(iv.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamValidate), typeof(string)));
                  _il.Emit(OpCodes.Pop);
                  break;
              }
              case IsamCopyToStmt ict:
              {
                  EmitStringArg(ict.DestinationFile);
                  // Il runtime IlRuntime.IsamCopyTo riceve la condizione come STRINGA e la
                  // ri-parsa con RuntimeFilterParser (CreateEvaluator(condition?.ToString())).
                  // NON valutare la condizione a compile-time: `FOR Citta = "Roma"` ha un
                  // campo bare ISAM (Inferred → double 0.0) a sinistra e una STRING a destra
                  // → String::Equals(double, string) = IL invalido (DbIsamFullTests Proc_601).
                  if (ict.Scope?.Condition is not null)
                      _il.Emit(OpCodes.Ldstr, ExprPlain.From(ict.Scope.Condition));
                  else _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(ict.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamCopyTo), typeof(string), typeof(object), typeof(string)));
                  break;
              }
              case IsamSortToStmt ist:
              {
                  EmitStringArg(ist.DestinationFile);
                  var sortFields = ist.SortFields.Select(f => f.KeyExpression is StringExpr se ? se.Value : f.KeyExpression.ToString()).ToArray();
                  var descFields = ist.SortFields.Select(f => f.IsDescending).ToArray();
                  _il.Emit(OpCodes.Ldc_I4, sortFields.Length);
                  _il.Emit(OpCodes.Newarr, _types.String);
                  for (var i = 0; i < sortFields.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup);
                      _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(OpCodes.Ldstr, sortFields[i]);
                      _il.Emit(OpCodes.Stelem_Ref);
                  }
                  _il.Emit(OpCodes.Ldc_I4, descFields.Length);
                  _il.Emit(OpCodes.Newarr, typeof(bool));
                  for (var i = 0; i < descFields.Length; i++)
                  {
                      _il.Emit(OpCodes.Dup);
                      _il.Emit(OpCodes.Ldc_I4, i);
                      _il.Emit(descFields[i] ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                      _il.Emit(OpCodes.Stelem_I1);
                  }
                  if (ist.Scope?.Condition is not null)
                      _il.Emit(OpCodes.Ldstr, ExprPlain.From(ist.Scope.Condition));
                  else _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(ist.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSortTo), typeof(string), typeof(string[]), typeof(bool[]), typeof(object), typeof(string)));
                  break;
              }
              case IsamJoinWithStmt ijw:
              {
                  _il.Emit(OpCodes.Ldstr, ijw.SecondaryAliasToken.Lexeme);
                  EmitStringArg(ijw.DestinationFile);
                  // IsamJoin riceve la condizione come STRINGA (ri-parsata da
                  // RuntimeFilterParser) — NON valutarla a compile-time (campi bare ISAM →
                  // double 0.0 → String::Equals invalido, stesso pattern di SET FILTER).
                  _il.Emit(OpCodes.Ldstr, ExprPlain.From(ijw.Condition.Condition));
                  EmitIsamAliasArg(ijw.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamJoin), typeof(string), typeof(string), typeof(object), typeof(string)));
                  break;
              }
              case IsamSetRelationStmt isrel:
              {
                  // ExprPlain.From (non EmitStringArg): il vecchio codice VALUTAVA
                  // isrel.KeyExpression a compile-time-callsite (un identificatore bare come
                  // "ID" non è una variabile reale in questo contesto → risolve a NIL/0
                  // silenzioso, "SET RELATION TO ID" non trovava mai nulla nemmeno nel caso
                  // semplice). Il testo pulito viene ora rivalutato per-record dal driver
                  // tramite Basic.Databases.KeyExpressionPCodeHook (vedi IsamDriver.SyncRelation).
                  if (isrel.KeyExpression is not null) _il.Emit(OpCodes.Ldstr, ExprPlain.From(isrel.KeyExpression));
                  else _il.Emit(OpCodes.Ldnull);
                  if (isrel.ChildAlias is not null) _il.Emit(OpCodes.Ldstr, isrel.ChildAlias.Lexeme);
                  else _il.Emit(OpCodes.Ldnull);
                  EmitIsamAliasArg(isrel.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSetRelation), typeof(string), typeof(string), typeof(string)));
                  break;
              }
              case IsamSelectWorkAreaStmt isw:
              {
                  // Il parser produce un StringExpr per un alias (SELECT Clienti), NON un
                  // VariableExpr (vedi ParseIsamSelectStatement) — Ldnull avrebbe chiamato
                  // IsamSelectWorkArea(null) che NON cambia l'area corrente. Passa sempre la
                  // stringa dell'alias.
                  if (isw.WorkArea is StringExpr iswS) _il.Emit(OpCodes.Ldstr, iswS.Value);
                  else if (isw.WorkArea is VariableExpr iswV) _il.Emit(OpCodes.Ldstr, iswV.NameCleaned.Lexeme);
                  else EmitStringArg(isw.WorkArea);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSelectWorkArea), typeof(string)));
                  break;
              }
              case IsamLockStmt ilk:
              {
                  _il.Emit(ilk.IsRecordLock ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  EmitIsamAliasArg(ilk.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamLock), typeof(bool), typeof(string)));
                  break;
              }
              case IsamUnlockStmt iul:
              {
                  _il.Emit(iul.IsRecordLock ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                  EmitIsamAliasArg(iul.FileNumber);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamUnlock), typeof(bool), typeof(string)));
                  break;
              }
             // ============================================================
             // SPEED — delega a IDatabaseService via IlRuntime.Speed*
             // ============================================================
             case SpeedOpenStmt spOpen:
             {
                 EmitStringArg(spOpen.DatabasePath);
                 var spOpenAlias = spOpen.OriginalAliasToken.Lexeme;
                 _il.Emit(OpCodes.Ldstr, spOpenAlias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedOpen), typeof(string), typeof(string)));
                 _il.Emit(OpCodes.Pop);
                 break;
             }
             case SpeedCloseStmt spClose:
             {
                 // SPEED CLOSE [alias | DATABASES] → IlRuntime.SpeedClose(alias) / SpeedCloseAll().
                 if (spClose.AllDatabases || spClose.AliasToken is null)
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedCloseAll)));
                 else
                 {
                     _il.Emit(OpCodes.Ldstr, spClose.AliasToken.Lexeme);
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedClose), typeof(string)));
                 }
                 break;
             }
             case SpeedUseStmt spUse:
             {
                 // Prima chiamava solo SpeedOpen(dbAlias, tableAlias) — ignorava COMPLETAMENTE
                 // TableName/lo schema UDT (OF Type)/INDEXED BY, riaprendo un secondo Database
                 // sullo stesso path invece di legarsi alla tabella corretta nel database già
                 // aperto (vedi IlRuntime.SpeedUseSchema/DatabaseService.Commercial.SpeedOpenSchema
                 // per il dettaglio — bug distinto e più a monte del crash SIGSEGV isolato in
                 // SPEED GET, stesso schema JSON di IsamOpenSchema via BuildIsamSchemaJson,
                 // riusato as-is: stessa forma Fields/PrimaryKey/Indexes).
                 EmitSpeedAliasArg(spUse.DatabaseReference);
                 EmitStringArg(spUse.TableName);
                 var spUseAlias = spUse.OriginalAliasToken.Lexeme;
                 _il.Emit(OpCodes.Ldstr, spUseAlias);
                 var spUseSchemaJson = BuildIsamSchemaJson(spUse.OriginalTypeName.Lexeme, spUse.IndexedBy);
                 _il.Emit(OpCodes.Ldstr, spUseSchemaJson);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedUseSchema), typeof(string), typeof(string), typeof(string), typeof(string)));
                 _il.Emit(OpCodes.Pop);
                 break;
             }
             case SpeedPutStmt spPut:
             {
                 // `SPEED PUT alias, {Campo: valore, ...}` (literal UDT, es. DbSpeedFullTests.bas
                 // "SPEED PUT Clienti, {ID: 101, Nome: ..., ...}"): un DictionaryLiteralExpr non è
                 // un ArrayLiteralExpr né una VariableExpr UDT — EmitObjectArrayArg cadeva nel
                 // fallback finale (Ldc_I4_0; Newarr Object), un object[0] SEMPRE vuoto. Ogni
                 // record veniva scritto con TUTTI i campi assenti/default (bug scoperto dal vivo
                 // isolando "SPEED SELECT ritorna 0 righe": la tabella Clienti aveva un solo
                 // record fantasma, tutti campi a default, invece dei 3 record reali). L'ordine
                 // dei campi nel dizionario può non coincidere con l'ordine di schema SPEED
                 // (SymbolId posizionale, vedi SpeedDriver.LoadSchema): si passa quindi un
                 // Dictionary<string,object> per NOME e si lascia a IlRuntime.SpeedPutByName/
                 // SpeedInsertByName il riordino per nome contro lo schema REALE della tabella,
                 // noto solo a runtime.
                 if (spPut.Source is DictionaryLiteralExpr spPutDict)
                 {
                     EmitDictionaryLiteral(spPutDict, new DictType(new StringType(), new AnyType()));
                     EmitSpeedAliasArg(spPut.Alias);
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedPutByName), typeof(Dictionary<string, object>), typeof(string)));
                 }
                 else
                 {
                     EmitSpeedKeyArg(spPut);
                     EmitObjectArrayArg(spPut.Source ?? new ArrayLiteralExpr([], null!));
                     EmitSpeedAliasArg(spPut.Alias);
                     _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedPut), typeof(string), typeof(object[]), typeof(string)));
                 }
                 break;
             }
             case SpeedGetStmt spGet:
             {
                 // Prima passava l'ALIAS come se fosse la CHIAVE da cercare (EmitSpeedKeyArg
                 // sotto emetteva stmt.Alias.NameCleaned.Lexeme, es. "Clienti") — SpeedGet
                 // cercava sempre un record con quella stringa come chiave primaria, mai
                 // trovato: array vuoto, poi IndexOutOfRangeException nel loop di copia campi
                 // (vedi EmitCopyObjectArrayToTarget). `SPEED GET alias, record` legge il
                 // record CORRENTE (posizionato da GO/SEEK/SKIP) — nessuna chiave esplicita.
                 EmitSpeedAliasArg(spGet.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedGetCurrent), typeof(string)));
                 EmitCopyObjectArrayToTarget(spGet.Destination);
                 break;
             }
             case SpeedDeleteStmt spDel:
             {
                 EmitSpeedKeyArg(spDel);
                 EmitSpeedAliasArg(spDel.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedDelete), typeof(string), typeof(string)));
                 break;
             }
             case SpeedSeekStmt spSeek:
             {
                 EmitStringArg(spSeek.KeyValue);
                 EmitSpeedAliasArg(spSeek.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSeek), typeof(string), typeof(string)));
                 break;
             }
             // Prima TUTTI no-op incondizionati (EmitDatabaseNoop) — dopo SPEED PUT il cursore
             // non veniva mai spostato/filtrato, quindi SPEED GET (record CORRENTE) trovava
             // sempre nulla. Bug scoperto verificando dal vivo il fix di SPEED USE (schema
             // OF/INDEXED BY, vedi IlRuntime.SpeedUseSchema).
             case SpeedGoStmt spGo:
             {
                 _il.Emit(spGo.Location.Kind == TokenKind.Bottom ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                 EmitSpeedAliasArg(spGo.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedGo), typeof(bool), typeof(string)));
                 break;
             }
             case SpeedSkipStmt spSkip:
             {
                 if (spSkip.Count is not null) EmitDoubleArgument(spSkip.Count); else EmitDouble(1.0);
                 EmitSpeedAliasArg(spSkip.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSkip), typeof(double), typeof(string)));
                 break;
             }
             case SpeedSetOrderStmt spSetOrder:
             {
                 EmitStringArg(spSetOrder.IndexName);
                 EmitSpeedAliasArg(spSetOrder.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSetOrder), typeof(string), typeof(string)));
                 break;
             }
case SpeedSetFilterStmt spSetFilter:
              {
                  if (spSetFilter.Condition is not null) _il.Emit(OpCodes.Ldstr, ExprPlain.FilterText(spSetFilter.Condition)); else _il.Emit(OpCodes.Ldnull);
                  EmitSpeedAliasArg(spSetFilter.Alias);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSetFilter), typeof(string), typeof(string)));
                  break;
              }
             // `SET RELATION ON parentAlias [TO keyExpr INTO childAlias]`: a differenza di
             // DBF/ISAM (area corrente implicita come padre), SPEED nomina sempre entrambi gli
             // alias esplicitamente — nessuna ambiguità sul padre. ExprPlain.From (non
             // EmitStringArg): keyExpr è rivalutata per-record dal driver figlio a ogni
             // navigazione del padre (vedi SpeedDriver.SyncRelation), non un valore congelato
             // al momento della SET RELATION.
             case SpeedSetRelationStmt spSetRel:
             {
                 if (spSetRel.KeyExpression is not null) _il.Emit(OpCodes.Ldstr, ExprPlain.From(spSetRel.KeyExpression));
                 else _il.Emit(OpCodes.Ldnull);
                 EmitSpeedAliasArg(spSetRel.ParentAlias);
                 if (spSetRel.ChildAlias is not null) EmitSpeedAliasArg(spSetRel.ChildAlias);
                 else _il.Emit(OpCodes.Ldnull);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSetRelation), typeof(string), typeof(string), typeof(string)));
                 break;
             }
             case SpeedCreateIndexStmt spCIdx:
             {
                 EmitStringArg(spCIdx.IndexName);
                 _il.Emit(OpCodes.Ldstr, ExprPlain.From(spCIdx.KeyExpression));
                 EmitSpeedAliasArg(spCIdx.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedCreateIndex), typeof(string), typeof(string), typeof(string)));
                 break;
             }
             case SpeedDropIndexStmt spDIdx:
             {
                 EmitStringArg(spDIdx.IndexName);
                 EmitSpeedAliasArg(spDIdx.Alias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedDropIndex), typeof(string), typeof(string)));
                 break;
             }
             // CREATE/REFRESH/DROP VIEW e SELECT: il motore (Database.ExecuteQuery/RefreshView/
             // DropView) è già completo — mancava solo il ponte compilatore→IlRuntime. Il piano
             // di query viene serializzato in JSON a compile-time (BuildSpeedQueryPlanJson,
             // stesso approccio di BuildIsamSchemaJson) perché IlRuntime non ha accesso alla
             // SymbolTable/AST per risolvere "SELECT *" o l'ordine dei campi del TYPE OF.
             case SpeedCreateViewStmt spCView:
             {
                 if (spCView.SelectStatement is null || spCView.SelectStatement.From is null)
                     throw new NotSupportedException("Backend IL: SPEED CREATE VIEW richiede AS SELECT ... FROM ... (la forma con espressione query dinamica non è supportata).");
                 var viewPlanJson = BuildSpeedQueryPlanJson(spCView.SelectStatement);
                 var viewSchemaJson = BuildIsamSchemaJson(spCView.OriginalTypeName.Lexeme, null);
                 _il.Emit(OpCodes.Ldstr, spCView.SelectStatement.From.TableAlias.Lexeme);
                 _il.Emit(OpCodes.Ldstr, spCView.OriginalViewName.Lexeme);
                 _il.Emit(OpCodes.Ldstr, viewSchemaJson);
                 _il.Emit(OpCodes.Ldstr, viewPlanJson);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedCreateView), typeof(string), typeof(string), typeof(string), typeof(string)));
                 break;
             }
             case SpeedRefreshViewStmt spRView:
             {
                 _il.Emit(OpCodes.Ldstr, spRView.ViewName.Lexeme);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedRefreshView), typeof(string)));
                 break;
             }
             case SpeedDropViewStmt spDView:
             {
                 _il.Emit(OpCodes.Ldstr, spDView.ViewName.Lexeme);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedDropView), typeof(string)));
                 break;
             }
             case SpeedSelectStmt spSel:
             {
                 if (spSel.QueryExpression is not null || spSel.From is null)
                     throw new NotSupportedException("Backend IL: SPEED SELECT dinamico (stringa) non è supportato, solo la forma statica con clausole.");
                 var destUdt = FindUdtByName(spSel.DestinationUdtToken?.Lexeme ?? "")
                     ?? throw new NotSupportedException($"Backend IL: TYPE '{spSel.DestinationUdtToken?.Lexeme}' non trovato per SPEED SELECT.");
                 var selectPlanJson = BuildSpeedQueryPlanJson(spSel);
                 _il.Emit(OpCodes.Ldstr, selectPlanJson);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSelect), typeof(string)));
                 EmitCopyObjectArrayArrayToUdtArrayTarget(spSel.IntoTarget!, destUdt);
                 break;
             }
             // ============================================================
             // LEDGER — delega a IDatabaseService via IlRuntime.Ledger*
             // ============================================================
             case LedgerOpenStmt lgOpen:
             {
                 EmitStringArg(lgOpen.FilePath);
                 var lgAlias = lgOpen.Handle is VariableTarget lgH ? lgH.NameCleaned.Lexeme : "ledger";
                 _il.Emit(OpCodes.Ldstr, lgAlias);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerOpen), typeof(string), typeof(string)));
                 EmitAssignTargetFromTop(lgOpen.Handle, new LongType());
                 break;
             }
             case LedgerCloseStmt lgClose:
             {
                 EmitLedgerAliasArg(lgClose.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerClose), typeof(string)));
                 break;
             }
             case LedgerAddStmt lgAdd:
             {
                 // LEDGER ADD <handle> WITH date, desc {postings} INTO <newId>:
                 // values = [date, desc, accountsCsv, amountsCsv] (schema nativa LedgerAdd).
                 EmitLedgerAliasArg(lgAdd.Handle);
                 _il.Emit(OpCodes.Ldc_I4, 4);
                 _il.Emit(OpCodes.Newarr, _types.Object);
                 // [0] = date
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_0);
                 EmitExpression(lgAdd.Date);
                 var dClr = DataTypeMapper.Map(NormalizeInferredType(InferType(lgAdd.Date)), _types, _classTypes);
                 if (dClr.IsValueType) _il.Emit(OpCodes.Box, dClr);
                 _il.Emit(OpCodes.Stelem_Ref);
                 // [1] = description
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_1);
                 EmitExpression(lgAdd.Description);
                 var descClr = DataTypeMapper.Map(NormalizeInferredType(InferType(lgAdd.Description)), _types, _classTypes);
                 if (descClr.IsValueType) _il.Emit(OpCodes.Box, descClr);
                 _il.Emit(OpCodes.Stelem_Ref);
                 // [2] = accounts (join con ";")
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_2);
                 if (lgAdd.Postings.Length > 0)
                 {
                     _il.Emit(OpCodes.Ldstr, string.Join(";", lgAdd.Postings.Select(p => p.Account is StringExpr sa ? sa.Value : p.Account.ToString())));
                 }
                 else _il.Emit(OpCodes.Ldstr, "");
                 _il.Emit(OpCodes.Stelem_Ref);
                 // [3] = amounts (join con ";")
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_3);
                 if (lgAdd.Postings.Length > 0)
                 {
                     _il.Emit(OpCodes.Ldstr, string.Join(";", lgAdd.Postings.Select(p => LedgerAmountValue(p.Amount!))));
                 }
                 else _il.Emit(OpCodes.Ldstr, "");
                 _il.Emit(OpCodes.Stelem_Ref);
                 _il.Emit(OpCodes.Ldnull);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerAdd), typeof(string), typeof(object[]), typeof(string)));
                 EmitAssignTargetFromTop(lgAdd.NewIdVariable, new DoubleType());
                 break;
             }
             case LedgerRegisterStmt lgReg:
             {
                 if (lgReg.Query is not null) EmitStringArg(lgReg.Query); else _il.Emit(OpCodes.Ldstr, "");
                 EmitLedgerAliasArg(lgReg.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerRegister), typeof(string), typeof(string)));
                 EmitAssignTargetFromTop(lgReg.DestinationArray);
                 break;
             }
             case LedgerBalanceStmt lgBal:
             {
                 if (lgBal.Query is not null) EmitStringArg(lgBal.Query); else _il.Emit(OpCodes.Ldstr, "");
                 EmitLedgerAliasArg(lgBal.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerBalance), typeof(string), typeof(string)));
                 EmitAssignTargetFromTop(lgBal.DestinationArray);
                 break;
             }
             case LedgerEditStmt lgEd:
             {
                 // LEDGER EDIT <h> ID <id> WITH date, desc {postings} INTO <newId>:
                 // delete + append (nuova transazione), il runtime ritorna il nuovo count.
                 EmitExpression(lgEd.TransactionId);
                 EmitCoerceTypes(NormalizeInferredType(InferType(lgEd.TransactionId)), new DoubleType());
                 _il.Emit(OpCodes.Ldc_I4, 4);
                 _il.Emit(OpCodes.Newarr, _types.Object);
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_0);
                 EmitExpression(lgEd.Date);
                 var eDClr = DataTypeMapper.Map(NormalizeInferredType(InferType(lgEd.Date)), _types, _classTypes);
                 if (eDClr.IsValueType) _il.Emit(OpCodes.Box, eDClr);
                 _il.Emit(OpCodes.Stelem_Ref);
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_1);
                 EmitExpression(lgEd.Description);
                 var eDescClr = DataTypeMapper.Map(NormalizeInferredType(InferType(lgEd.Description)), _types, _classTypes);
                 if (eDescClr.IsValueType) _il.Emit(OpCodes.Box, eDescClr);
                 _il.Emit(OpCodes.Stelem_Ref);
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_2);
                 _il.Emit(OpCodes.Ldstr, lgEd.Postings.Length > 0 ? string.Join(";", lgEd.Postings.Select(p => p.Account is StringExpr sa ? sa.Value : p.Account.ToString())) : "");
                 _il.Emit(OpCodes.Stelem_Ref);
                 _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4_3);
                 _il.Emit(OpCodes.Ldstr, lgEd.Postings.Length > 0 ? string.Join(";", lgEd.Postings.Select(p => LedgerAmountValue(p.Amount!))) : "");
                 _il.Emit(OpCodes.Stelem_Ref);
                 EmitLedgerAliasArg(lgEd.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerEdit), typeof(double), typeof(object[]), typeof(string)));
                 EmitAssignTargetFromTop(lgEd.NewIdVariable, new DoubleType());
                 break;
             }
              case LedgerDeleteStmt lgDel:
              {
                  EmitExpression(lgDel.TransactionId);
                  EmitCoerceTypes(NormalizeInferredType(InferType(lgDel.TransactionId)), new DoubleType());
                  EmitLedgerAliasArg(lgDel.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerDelete), typeof(double), typeof(string)));
                  break;
              }
              case LedgerGoStmt lgGo:
              {
                  // LEDGER GO <h> TOP|BOTTOM|<n> — posiziona il cursore (pattern DBF/ISAM).
                  if (lgGo.Destination is StringExpr sTop && sTop.Value.Equals("TOP", StringComparison.OrdinalIgnoreCase))
                  {
                      EmitLedgerAliasArg(lgGo.Handle);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGoTop), typeof(string)));
                  }
                  else if (lgGo.Destination is StringExpr sBot && sBot.Value.Equals("BOTTOM", StringComparison.OrdinalIgnoreCase))
                  {
                      EmitLedgerAliasArg(lgGo.Handle);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGoBottom), typeof(string)));
                  }
                  else
                  {
                      EmitExpression(lgGo.Destination);
                      EmitCoerceTypes(NormalizeInferredType(InferType(lgGo.Destination)), new DoubleType());
                      EmitLedgerAliasArg(lgGo.Handle);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGo), typeof(double), typeof(string)));
                  }
                  break;
              }
              case LedgerSkipStmt lgSk:
              {
                  // LEDGER SKIP <h> [count] — muove il cursore (default 1, negativo indietro).
                  if (lgSk.Count is not null) EmitExpression(lgSk.Count);
                  else _il.Emit(OpCodes.Ldc_R8, 1.0);
                  EmitCoerceTypes(lgSk.Count is not null ? NormalizeInferredType(InferType(lgSk.Count)) : new DoubleType(), new DoubleType());
                  EmitLedgerAliasArg(lgSk.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerSkip), typeof(double), typeof(string)));
                  break;
              }
              case LedgerSaveStmt lgSv:
              {
                  // LEDGER SAVE <h> — persiste indice e budget (era EmitDatabaseNoop).
                  EmitLedgerAliasArg(lgSv.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerSave), typeof(string)));
                  break;
              }
             case LedgerPackStmt lgPk:
             {
                 EmitLedgerAliasArg(lgPk.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerPack), typeof(string)));
                 break;
             }
             case LedgerAccountsStmt lgAc:
             {
                 if (lgAc.RootAccount is not null) EmitStringArg(lgAc.RootAccount); else _il.Emit(OpCodes.Ldnull);
                 EmitLedgerAliasArg(lgAc.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerAccounts), typeof(string), typeof(string)));
                 EmitAssignTargetFromTop(lgAc.DestinationArray);
                 break;
             }
             case LedgerValidateStmt lgVa:
             {
                 EmitLedgerAliasArg(lgVa.Handle);
                 _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerValidate), typeof(string)));
                 EmitAssignTargetFromTop(lgVa.DestinationVariable, new DoubleType());
                 break;
             }
              case LedgerGetStmt lgGt:
              {
                  if (lgGt.TransactionId is not null)
                  {
                      EmitExpression(lgGt.TransactionId);
                      EmitCoerceTypes(NormalizeInferredType(InferType(lgGt.TransactionId)), new DoubleType());
                      EmitLedgerAliasArg(lgGt.Handle);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGet), typeof(double), typeof(string)));
                  }
                  else
                  {
                      // GET senza ID: legge la transazione corrente del cursore.
                      EmitLedgerAliasArg(lgGt.Handle);
                      _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGetCurrent), typeof(string)));
                  }
                  EmitAssignTargetFromTop(lgGt.DestinationArray);
                  break;
              }
              case LedgerBudgetStmt lgBg:
              {
                  // LEDGER BUDGET <h> <account> <amount> <period> — era EmitDatabaseNoop.
                  EmitStringArg(lgBg.Account);
                  EmitExpression(lgBg.Amount);
                  EmitCoerceTypes(NormalizeInferredType(InferType(lgBg.Amount)), new DoubleType());
                  EmitStringArg(lgBg.Period);
                  EmitLedgerAliasArg(lgBg.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerBudget), typeof(string), typeof(double), typeof(string), typeof(string)));
                  break;
              }
              case LedgerVarianceStmt lgVr:
              {
                  // LEDGER VARIANCE <h> <account> <period> INTO dest — era EmitDatabaseNoop.
                  EmitStringArg(lgVr.Account);
                  EmitStringArg(lgVr.Period);
                  EmitLedgerAliasArg(lgVr.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerVariance), typeof(string), typeof(string), typeof(string)));
                  EmitAssignTargetFromTop(lgVr.DestinationVariable, new DoubleType());
                  break;
              }
              case LedgerPlotStmt lgPl:
              {
                  // LEDGER PLOT <h> <query> <chartType> INTO dest — era EmitDatabaseNoop.
                  EmitStringArg(lgPl.Query);
                  EmitStringArg(lgPl.ChartType);
                  EmitLedgerAliasArg(lgPl.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerPlot), typeof(string), typeof(string), typeof(string)));
                  EmitAssignTargetFromTop(lgPl.DestinationVariable);
                  break;
              }
              case LedgerImportStmt lgIm:
              {
                  // LEDGER IMPORT <h> <csvPath> <mapping> [INTO dest] — era EmitDatabaseNoop.
                  EmitStringArg(lgIm.CsvPath);
                  EmitStringArg(lgIm.MappingRules);
                  EmitLedgerAliasArg(lgIm.Handle);
                  _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerImport), typeof(string), typeof(string), typeof(string)));
                  if (lgIm.DestinationVariable is not null) EmitAssignTargetFromTop(lgIm.DestinationVariable, new DoubleType());
                  else _il.Emit(OpCodes.Pop);
                  break;
              }
             // Rdd (nuovo naming): delega agli stessi runtime DBF.
             case RddUseStmt rduse: EmitRddUseStatement(rduse); break;
            case RddLockStmt rl:
            {
                _il.Emit(OpCodes.Ldc_R8, rl.Kind switch { RddLockKind.RLock => 0.0, RddLockKind.FLock => 1.0, _ => 2.0 });
                if (rl.AreaToken is not null) _il.Emit(OpCodes.Ldstr, rl.AreaToken.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddLock), typeof(double), typeof(string)));
                break;
            }
            case RddSetDefaultStmt rsd:
            {
                EmitStringArg(rsd.DriverName);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetDefault), typeof(string)));
                break;
            }
            case RddCopyToStmt rcp: EmitBoxedArg(rcp.Area); EmitBoxedArg(rcp.FileName); if (rcp.OutDriver is not null) EmitBoxedArg(rcp.OutDriver); else _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddCopyTo), typeof(object), typeof(object), typeof(object))); break;
            case RddSortToStmt rsrt: EmitBoxedArg(rsrt.Area); EmitBoxedArg(rsrt.FileName); if (rsrt.Fields is not null) EmitBoxedArg(rsrt.Fields); else _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSortTo), typeof(object), typeof(object), typeof(object))); break;
            case RddTotalOnStmt rtot: EmitBoxedArg(rtot.Area); EmitBoxedArg(rtot.Key); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddTotalOn), typeof(object), typeof(object))); break;
            case RddDeleteStmt rdd: EmitRddScopedStatement(rdd.Scope, nameof(IlRuntime.RddDelete)); break;
            case RddRecallStmt rrc: EmitRddScopedStatement(rrc.Scope, nameof(IlRuntime.RddRecall)); break;
            case RddReplaceStmt rrp: EmitRddReplaceStatement(rrp); break;
            case RddSetDeletedStmt rsdel: _il.Emit(rsdel.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetDeleted), typeof(bool), typeof(string))); break;
            case RddSetLockTimeoutStmt rlt:
            {
                EmitExpression(rlt.Ms);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetLockTimeout), typeof(double)));
                break;
            }
            case RddSetReprocessStmt rrp:
            {
                // SET REPROCESS ms (Clipper): retry timeout dei lock → RddSetLockTimeout.
                EmitExpression(rrp.Ms);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetLockTimeout), typeof(double)));
                break;
            }
            case RddSetOrderStmt rso:
            {
                // SET ORDER area, ordine — ordine come NOME indice (SQL: ORDER BY colonna).
                EmitObjectBoxedArgument(rso.Area);
                EmitObjectBoxedArgument(rso.Order);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetOrder), typeof(object), typeof(object)));
                break;
            }
            case RddCreateIndexStmt rci:
            {
                EmitObjectBoxedArgument(rci.Area);
                EmitObjectBoxedArgument(rci.Name);
                EmitObjectBoxedArgument(rci.Key);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddCreateIndex), typeof(object), typeof(object), typeof(object)));
                break;
            }
            case RddCloseIndexStmt rcI:
            {
                EmitObjectBoxedArgument(rcI.Area);
                EmitObjectBoxedArgument(rcI.Name);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddCloseIndex), typeof(object), typeof(object)));
                break;
            }
            case RddSetFieldStmt rsF:
            {
                EmitBoxedArg(rsF.Area);
                EmitBoxedArg(rsF.Field);
                EmitBoxedArg(rsF.Value);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetField), typeof(object), typeof(object), typeof(object)));
                break;
            }
            case RddOnEventStmt rsOn:
            {
                EmitStringArg(rsOn.EventName);
                if (rsOn.Callback is not null) EmitStringArg(rsOn.Callback);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddOnEvent), typeof(string), typeof(string)));
                break;
            }
            case RddOffEventStmt rsOff:
            {
                EmitStringArg(rsOff.EventName);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddOffEvent), typeof(string)));
                break;
            }
            case VectorDbEnsureCollectionStmt vec: EmitVectorDbStatement(vec); break;
            case VectorDbIndexTextStmt vi: EmitVectorDbStatement(vi); break;
            case VectorDbIndexMetaStmt vm: EmitVectorDbStatement(vm); break;
            case VectorDbSearchStmt vs: EmitVectorDbStatement(vs); break;
            case VectorDbCountStmt vc: EmitVectorDbStatement(vc); break;
            case VectorDbListCollectionsStmt vl: EmitVectorDbStatement(vl); break;
            case VectorDbRemoveSourceStmt vr: EmitVectorDbStatement(vr); break;
            case VectorDbLoadDocumentsStmt vld: EmitVectorDbStatement(vld); break;
            case VectorDbPurgeDeprecatedStmt vp: EmitVectorDbStatement(vp); break;
            // VectorDB legacy statements
            // TUI / Clipper / UI controls
            case AtGetStmt atget: EmitAtGetStatement(atget); break;
            case BoxStmt box: EmitBoxStatement(box); break;
            case PixelControlStmt pcx: EmitPixelControl(pcx); break;
            case ControlEventStmt cev: EmitControlEvent(cev); break;
            case FireEventStmt fest: EmitFireEvent(fest); break;
            case ControlMethodStmt cmx: EmitControlMethod(cmx); break;
            case CheckBoxStmt chk: EmitTuiCall(IlRuntimeTuiCheckBoxMethod, [chk.Row, chk.Col, chk.Caption], chk.Variable); EmitTuiIdIL(chk.Id); EmitTuiPropsIL(chk.Props, chk.Token); if (chk.Events != null) foreach (var _e in chk.Events) EmitControlEvent(_e); break;
            case ClipperReadStmt cr: EmitPendingReadAssignments(); break;
            case ComboBoxStmt cbo: EmitTuiCall(IlRuntimeTuiComboBoxMethod, [cbo.Row, cbo.Col, cbo.Width, cbo.Items], cbo.Variable); EmitTuiIdIL(cbo.Id); EmitTuiPropsIL(cbo.Props, cbo.Token); if (cbo.Events != null) foreach (var _e in cbo.Events) EmitControlEvent(_e); break;
            case ColorPickerStmt cp: EmitTuiCall(IlRuntimeTuiColorPickerMethod, [cp.Row, cp.Col, cp.Columns], cp.Variable); EmitTuiIdIL(cp.Id); EmitTuiPropsIL(cp.Props, cp.Token); if (cp.Events != null) foreach (var _e in cp.Events) EmitControlEvent(_e); break;
            case DatePickerStmt dp: EmitTuiCall(IlRuntimeTuiDatePickerMethod, [dp.Row, dp.Col], dp.Variable); EmitTuiIdIL(dp.Id); EmitTuiPropsIL(dp.Props, dp.Token); if (dp.Events != null) foreach (var _e in dp.Events) EmitControlEvent(_e); break;
            case FileDialogStmt fd: EmitTuiCall(IlRuntimeTuiFileDialogMethod, [fd.Mask, fd.Title], fd.Variable); EmitTuiIdIL(fd.Id); EmitTuiPropsIL(fd.Props, fd.Token); if (fd.Events != null) foreach (var _e in fd.Events) EmitControlEvent(_e); break;
            case GridStmt grid: EmitTuiCall(IlRuntimeTuiGridMethod, [grid.Row, grid.Col, grid.Width, grid.Height, grid.Headers, grid.Data], grid.Variable); EmitTuiIdIL(grid.Id); EmitTuiPropsIL(grid.Props, grid.Token); if (grid.Events != null) foreach (var _e in grid.Events) EmitControlEvent(_e); break;
            case ListBoxStmt lb: EmitTuiCall(IlRuntimeTuiListBoxMethod, [lb.Row, lb.Col, lb.Width, lb.Height, lb.Items], lb.Variable); EmitTuiIdIL(lb.Id); EmitTuiPropsIL(lb.Props, lb.Token); if (lb.Events != null) foreach (var _e in lb.Events) EmitControlEvent(_e); break;
            case MenuStmt menu: EmitTuiCall(IlRuntimeTuiMenuMethod, [menu.Row, menu.Col, menu.Items], menu.Variable); EmitTuiIdIL(menu.Id); EmitTuiPropsIL(menu.Props, menu.Token); if (menu.Events != null) foreach (var _e in menu.Events) EmitControlEvent(_e); break;
            case ToggleStmt tg: EmitTuiCall(IlRuntimeTuiToggleMethod, [tg.Row, tg.Col, tg.Caption], tg.Variable); EmitTuiIdIL(tg.Id); EmitTuiPropsIL(tg.Props, tg.Token); if (tg.Events != null) foreach (var _e in tg.Events) EmitControlEvent(_e); break;
            case SpinnerStmt sp: EmitSpinnerStatement(sp); EmitTuiIdIL(sp.Id); EmitTuiPropsIL(sp.Props, sp.Token); if (sp.Events != null) foreach (var _e in sp.Events) EmitControlEvent(_e); break;
            case SliderStmt sl: EmitTuiCall(IlRuntimeTuiSliderMethod, [sl.Row, sl.Col, sl.Length, sl.Min, sl.Max, sl.Step], sl.Variable); EmitTuiIdIL(sl.Id); EmitTuiPropsIL(sl.Props, sl.Token); if (sl.Events != null) foreach (var _e in sl.Events) EmitControlEvent(_e); break;
            case NumericUpDownStmt nu: EmitTuiCall(IlRuntimeTuiNumericUpDownMethod, [nu.Row, nu.Col, nu.Length, nu.Min, nu.Max, nu.Step], nu.Variable); EmitTuiIdIL(nu.Id); EmitTuiPropsIL(nu.Props, nu.Token); if (nu.Events != null) foreach (var _e in nu.Events) EmitControlEvent(_e); break;
            case StarRatingStmt sr: EmitTuiCall(IlRuntimeTuiStarRatingMethod, [sr.Row, sr.Col, sr.Length, sr.Max, sr.Value], sr.Variable); EmitTuiIdIL(sr.Id); EmitTuiPropsIL(sr.Props, sr.Token); if (sr.Events != null) foreach (var _e in sr.Events) EmitControlEvent(_e); break;
            case ScrollBarStmt sb: EmitTuiCall(IlRuntimeTuiScrollBarMethod, [sb.Row, sb.Col, sb.Length, sb.Min, sb.Max, sb.Value], sb.Variable); EmitTuiIdIL(sb.Id); EmitTuiPropsIL(sb.Props, sb.Token); if (sb.Events != null) foreach (var _e in sb.Events) EmitControlEvent(_e); break;
            case ScrollViewStmt sv: EmitTuiCall(IlRuntimeTuiScrollViewMethod, [sv.Row, sv.Col, sv.Width, sv.Height, sv.Content], sv.Variable); EmitTuiIdIL(sv.Id); EmitTuiPropsIL(sv.Props, sv.Token); if (sv.Events != null) foreach (var _e in sv.Events) EmitControlEvent(_e); break;
            case LabelControlStmt lb: EmitTuiCall(IlRuntimeTuiLabelMethod, [lb.Row, lb.Col, lb.Width, lb.Text], lb.Variable); EmitTuiIdIL(lb.Id); EmitTuiPropsIL(lb.Props, lb.Token); if (lb.Events != null) foreach (var _e in lb.Events) EmitControlEvent(_e); break;
            case DataListStmt dl: EmitTuiCall(IlRuntimeTuiDataListMethod, [dl.Row, dl.Col, dl.Width, dl.Height, dl.Items], dl.Variable); EmitTuiIdIL(dl.Id); EmitTuiPropsIL(dl.Props, dl.Token); if (dl.Events != null) foreach (var _e in dl.Events) EmitControlEvent(_e); break;
            case PropertyGridStmt pg: EmitTuiCall(IlRuntimeTuiPropertyGridMethod, [pg.Row, pg.Col, pg.Width, pg.Height, pg.Items], pg.Variable); EmitTuiIdIL(pg.Id); EmitTuiPropsIL(pg.Props, pg.Token); if (pg.Events != null) foreach (var _e in pg.Events) EmitControlEvent(_e); break;
            case LogViewStmt lv: EmitTuiCall(IlRuntimeTuiLogViewMethod, [lv.Row, lv.Col, lv.Width, lv.Height, lv.Records], lv.Variable); EmitTuiIdIL(lv.Id); EmitTuiPropsIL(lv.Props, lv.Token); if (lv.Events != null) foreach (var _e in lv.Events) EmitControlEvent(_e); break;
            case HexViewStmt hx: EmitTuiCall(IlRuntimeTuiHexViewMethod, [hx.Row, hx.Col, hx.Width, hx.Height, hx.Data], hx.Variable); EmitTuiIdIL(hx.Id); EmitTuiPropsIL(hx.Props, hx.Token); if (hx.Events != null) foreach (var _e in hx.Events) EmitControlEvent(_e); break;
            case BigTextStmt bt: EmitTuiCall(IlRuntimeTuiBigTextMethod, [bt.Row, bt.Col, bt.Text], bt.Variable); EmitTuiIdIL(bt.Id); EmitTuiPropsIL(bt.Props, bt.Token); if (bt.Events != null) foreach (var _e in bt.Events) EmitControlEvent(_e); break;
            case CodeAreaStmt ca: EmitTuiCall(IlRuntimeTuiCodeAreaMethod, [ca.Row, ca.Col, ca.Width, ca.Height, ca.Lines], ca.Variable); EmitTuiIdIL(ca.Id); EmitTuiPropsIL(ca.Props, ca.Token); if (ca.Events != null) foreach (var _e in ca.Events) EmitControlEvent(_e); break;
            case ReorderListBoxStmt rl: EmitTuiCall(IlRuntimeTuiReorderListBoxMethod, [rl.Row, rl.Col, rl.Width, rl.Height, rl.Items], rl.Variable); EmitTuiIdIL(rl.Id); EmitTuiPropsIL(rl.Props, rl.Token); if (rl.Events != null) foreach (var _e in rl.Events) EmitControlEvent(_e); break;
            case AlertBoxStmt ab: EmitTuiCall(IlRuntimeTuiAlertBoxMethod, [ab.Row, ab.Col, ab.Width, ab.Height, ab.Text], ab.Variable); EmitTuiIdIL(ab.Id); EmitTuiPropsIL(ab.Props, ab.Token); if (ab.Events != null) foreach (var _e in ab.Events) EmitControlEvent(_e); break;
            case ConfirmationBoxStmt cb: EmitTuiCall(IlRuntimeTuiConfirmationBoxMethod, [cb.Row, cb.Col, cb.Width, cb.Height, cb.Text], cb.Variable); EmitTuiIdIL(cb.Id); EmitTuiPropsIL(cb.Props, cb.Token); if (cb.Events != null) foreach (var _e in cb.Events) EmitControlEvent(_e); break;
            case FileExplorerStmt fe: EmitTuiCall(IlRuntimeTuiFileExplorerMethod, [fe.Row, fe.Col, fe.Width, fe.Height, fe.Path], fe.Variable); EmitTuiIdIL(fe.Id); EmitTuiPropsIL(fe.Props, fe.Token); if (fe.Events != null) foreach (var _e in fe.Events) EmitControlEvent(_e); break;
            case DashboardStmt db: EmitTuiCall(IlRuntimeTuiDashboardMethod, [db.Row, db.Col, db.Width, db.Height, db.Metrics], db.Variable); EmitTuiIdIL(db.Id); EmitTuiPropsIL(db.Props, db.Token); if (db.Events != null) foreach (var _e in db.Events) EmitControlEvent(_e); break;
            case SplitterStmt sp: EmitTuiCall(IlRuntimeTuiSplitterMethod, [sp.Row, sp.Col, sp.Width, sp.Height, sp.Orient], sp.Variable); EmitTuiIdIL(sp.Id); EmitTuiPropsIL(sp.Props, sp.Token); if (sp.Events != null) foreach (var _e in sp.Events) EmitControlEvent(_e); break;
            case ContextMenuStmt cm: EmitTuiCall(IlRuntimeTuiContextMenuMethod, [cm.Row, cm.Col, cm.Width, cm.Height, cm.Items], cm.Variable); EmitTuiIdIL(cm.Id); EmitTuiPropsIL(cm.Props, cm.Token); if (cm.Events != null) foreach (var _e in cm.Events) EmitControlEvent(_e); break;            case PrinterDialogStmt prdlg: EmitTuiPrinterDialog(prdlg); EmitTuiIdIL(prdlg.Id); EmitTuiPropsIL(prdlg.Props, prdlg.Token); if (prdlg.Events != null) foreach (var _e in prdlg.Events) EmitControlEvent(_e); break;
            case RadioGroupStmt rg: EmitTuiCall(IlRuntimeTuiRadioGroupMethod, [rg.Row, rg.Col, rg.Items], rg.Variable); EmitTuiIdIL(rg.Id); EmitTuiPropsIL(rg.Props, rg.Token); if (rg.Events != null) foreach (var _e in rg.Events) EmitControlEvent(_e); break;
            case ScrollStmt scroll: EmitScrollStatement(scroll); break;
            case StatusBarStmt stbar: EmitStatusBarStatement(stbar); EmitTuiIdIL(stbar.Id); EmitTuiPropsIL(stbar.Props, stbar.Token); if (stbar.Events != null) foreach (var _e in stbar.Events) EmitControlEvent(_e); break;
            case TabStmt tab: EmitTuiCall(IlRuntimeTuiTabMethod, [tab.Row, tab.Col, tab.Width, tab.Height, tab.Tabs], tab.Variable); EmitTuiIdIL(tab.Id); EmitTuiPropsIL(tab.Props, tab.Token); if (tab.Events != null) foreach (var _e in tab.Events) EmitControlEvent(_e); break;
            case TBrowseStmt tb: EmitTuiCall(IlRuntimeTuiTBrowseMethod, [tb.Row, tb.Col, tb.Width, tb.Height, tb.Columns, tb.Data, tb.Options], tb.Variable); EmitTuiIdIL(tb.Id); EmitTuiPropsIL(tb.Props, tb.Token); if (tb.Events != null) foreach (var _e in tb.Events) EmitControlEvent(_e); break;
            case TreeViewStmt tv: EmitTuiCall(IlRuntimeTuiTreeViewMethod, [tv.Row, tv.Col, tv.Width, tv.Height, tv.Tree], tv.Variable); EmitTuiIdIL(tv.Id); EmitTuiPropsIL(tv.Props, tv.Token); if (tv.Events != null) foreach (var _e in tv.Events) EmitControlEvent(_e); break;
            case WindowActionStmt wa: EmitWindowActionStatement(wa); break;
            case WindowPumpStmt wpu: EmitLdI4(wpu.Ms); _il.Emit(OpCodes.Call, IlRuntimeUiFrameMethod); break;
            case WindowRequestCloseStmt: _il.Emit(OpCodes.Call, IlRuntimeFormCloseMethod); _il.Emit(OpCodes.Ldc_R8, 0.0); _il.Emit(OpCodes.Pop); break;
            case WindowVisibilityStmt wv: EmitWindowVisibilityStatement(wv); break;
            case WindowSetTitleStmt wst: EmitWindowSetTitleStatement(wst); break;
            case WindowAppendLineStmt wall: EmitWindowAppendLineStatement(wall); break;
            case WindowSetContentStmt wsc: EmitWindowSetContentStatement(wsc); break;
            case WindowClearStmt wcl: EmitWindowClearStatement(wcl); break;
            case WindowScrollStmt wscr: EmitWindowScrollStatement(wscr); break;
            case WindowEditStmt wed: EmitWindowEditStatement(wed); break;
            case WindowSelectStmt wsel: EmitWindowSelectStatement(wsel); break;
            case WindowThemeStmt wth: EmitWindowThemeStatement(wth); break;
            case WindowDesktopStmt wds: EmitWindowDesktopStatement(wds); break;
            case WindowMoveDesktopStmt wmd: EmitWindowMoveDesktopStatement(wmd); break;
            case WindowFrameStmt wfr: EmitWindowFrameStatement(wfr); break;
            case WindowShadowStmt wsh: EmitWindowShadowStatement(wsh); break;
            // Workflow/Team/Agent
            case WorkflowDefStmt wd: EmitWorkflowDefStatement(wd); break;
            case WorkflowStepStmt wstep: EmitWorkflowStepStatement(wstep); break;
            case WorkflowExecuteStmt wex: EmitWorkflowExecuteStatement(wex); break;
            case WorkflowEndStmt wend: _il.Emit(OpCodes.Call, IlRuntimeWorkflowEndMethod); break;
            case TeamCreateStmt tc: EmitAgentNameStatement(IlRuntimeTeamCreateMethod, tc.TeamName); break;
            case TeamAddAgentStmt taa: EmitAgentTwoNameStatement(IlRuntimeTeamAddAgentMethod, taa.TeamName, taa.AgentName); break;
            case TeamRemoveAgentStmt tra: EmitAgentTwoNameStatement(IlRuntimeTeamRemoveAgentMethod, tra.TeamName, tra.AgentName); break;
            case TeamMessageStmt tm: EmitTeamMessageStatement(tm); break;
            case TeamSetRoleStmt tsr: EmitTeamSetRoleStatement(tsr); break;
            case TeamSetStrategyStmt tss: EmitTeamSetStrategyStatement(tss); break;
            case TeamSetCoordinatorStmt tsc: EmitAgentTwoNameStatement(IlRuntimeTeamSetCoordinatorMethod, tsc.TeamName, tsc.AgentName); break;
            case TeamSetRecursionStmt tsrec: EmitTeamSetRecursionStatement(tsrec); break;
            case TeamSetLatentStmt tsl: EmitTeamSetLatentStatement(tsl); break;
            case TeamSetLinkStmt tslink: EmitTeamSetLinkStatement(tslink); break;
            case TeamSetThoughtsStmt tsth: EmitTeamSetThoughtsStatement(tsth); break;
            case TeamTrainInnerStmt ttin: EmitTeamTrainInnerStatement(ttin); break;
            case TeamTrainOuterStmt ttout: EmitTeamTrainOuterStatement(ttout); break;
            case TeamSetDagStmt tsd: EmitTeamSetDagStatement(tsd); break;
            case TeamSetComplexityStmt tscx: EmitTeamSetComplexityStatement(tscx); break;
            case TeamSetReflectionStmt tsrf: EmitTeamSetReflectionStatement(tsrf); break;
            case TeamSetCriticStmt tscr: EmitTeamSetCriticStatement(tscr); break;
            case TeamSetPruneStmt tspr: EmitTeamSetPruneStatement(tspr); break;
            case TeamTrainThinkingStmt tth: EmitTeamTrainThinkingStatement(tth); break;
            case TeamTrainActorCriticStmt tac: EmitTeamTrainActorCriticStatement(tac); break;
            case TeamDistillThinkingStmt tdt: EmitTeamDistillThinkingStatement(tdt); break;
            case TeamSaveStmt tsv: EmitAgentNameStatement(IlRuntimeTeamSaveMethod, tsv.TeamName); break;
            case TeamLoadStmt tld: EmitAgentNameStatement(IlRuntimeTeamLoadMethod, tld.TeamName); break;
            case TeamRunStmt tr: EmitTeamRunStatement(tr); break;
            case TeamBroadcastStmt tbs: EmitTeamBroadcastStatement(tbs); break;
            case TeamRedefineStmt trd: EmitTeamRedefineStatement(trd); break;
            case CreateAgentStmt ca: EmitAgentNameStatement(IlRuntimeAgentCreateMethod, ca.AgentName); break;
            case DeleteAgentStmt da: EmitAgentNameStatement(IlRuntimeAgentDeleteMethod, da.AgentName); break;
            case SaveAgentStmt sa: EmitAgentNameStatement(IlRuntimeAgentSaveMethod, sa.AgentName); break;
            case LoadAgentStmt la: EmitAgentNameStatement(IlRuntimeAgentLoadMethod, la.AgentName); break;
            // HTTP/Network extras
            case HttpServerStmt hs: EmitHttpServerStatement(hs); break;
            case HttpConfigStmt hc: EmitHttpConfigStatement(hc); break;
            case HttpRouteStmt hr: EmitHttpRouteStatement(hr); break;
            case HttpStaticStmt hs: EmitHttpStaticStatement(hs); break;
            case HttpUseStmt hu: EmitHttpUseStatement(hu); break;
            case HttpUseNextStmt hun: EmitHttpUseNextStatement(hun); break;
            case HttpListenerLoopStmt: break; // VM-internal no-op
            case HttpConnectionHandlerStmt: break; // VM-internal no-op
            // Misc
            case MacroStmt macro: EmitMacroStatement(macro); break;
            case RunStmt run: EmitRunStatement(run); break;
            // SSL
            case SslOpenStmt ssl: EmitSslOpenStatement(ssl); break;
            case SslConnectStmt sslc: EmitSslConnectStatement(sslc); break;
            // Events
            case AddHandlerStmt ah: EmitEventStatement(ah); break;
            case RemoveHandlerStmt rh: EmitEventStatement(rh); break;
            case RaiseEventStmt re: EmitRaiseEventStatement(re); break;
            // File I/O advanced
            case GetFileStmt gf: EmitGetFileStatement(gf); break;
            case PutFileStmt pf: EmitPutFileStatement(pf); break;
            case ExportToFileStmt ef:
            {
                // ef.Area è tipicamente un ALIAS (es. `ISAM EXPORT Clienti TO ...`, vedi
                // DbIsamFullTests.bas), non un numero di area — EmitIsamAliasArg (VariableExpr
                // → lessema, NumberExpr → stringa numerica) evita di valutarlo come una vera
                // espressione (un alias bare non è una variabile dichiarata).
                _il.Emit(OpCodes.Ldstr, ef.EngineType.Lexeme);
                EmitIsamAliasArg(ef.Area);
                EmitStringArg(ef.FilePath);
                _il.Emit(OpCodes.Ldstr, ef.Format.Lexeme);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ExportToFile), typeof(string), typeof(string), typeof(string), typeof(string)));
                break;
            }
            case ImportFromFileStmt imf:
            {
                _il.Emit(OpCodes.Ldstr, imf.EngineType.Lexeme);
                EmitIsamAliasArg(imf.Area);
                EmitStringArg(imf.FilePath);
                _il.Emit(OpCodes.Ldstr, imf.Format.Lexeme);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ImportFromFile), typeof(string), typeof(string), typeof(string), typeof(string)));
                break;
            }
            // Memory mapping
            case MemoryMapStmt mm: EmitMemoryMapStatement(mm); break;
            case MemoryMapPokeStmt mp: EmitMemoryPokeStatement(mp); break;
            case MemoryMapWriteBytesStmt mw: EmitMemoryWriteBytesStatement(mw); break;
            // Network config
            case SetOptionStmt so: EmitSetOptionStatement(so); break;
            case ShutdownStmt sd: EmitShutdownStatement(sd); break;
            // Using block
            case UsingStmt us: EmitUsingStatement(us); break;
            // Module system
            case ImportStmt imp: EmitImportStatement(imp); break;
            case RequireStmt req: EmitRequireStatement(req); break;
            // Clipboard
            case ClipStmt clip: EmitClipStatement(clip); break;
            // Audio control
            // Console TUI extras
            case ConsoleScrollModeStmt sms: EmitConsoleScrollModeStatement(sms); break;
            case ConsoleSendKeyStmt sk: EmitConsoleSendKeyStatement(sk); break;
            case CursorShapeStmt csh: EmitCursorShapeStatement(csh); break;
            case CursorStateStmt css: EmitCursorStateStatement(css); break;
            case ClearRectStmt: EmitClearRectStatement(); break;
            case ScreenStmt screen: EmitScreenStatement(screen); break;
            case TextModeStmt textmode: EmitTextModeStatement(textmode); break;
            case ScreenSetStmt screenset: EmitScreenSetStatement(screenset); break;
            case CCLSStmt ccls: EmitCClsStatement(ccls); break;
            case CPrintStmt cprint: EmitCPrintStatement(cprint); break;
            case CLocateStmt cloc: EmitCLocateStatement(cloc); break;
            case PointStmt pt: EmitPointStatement(pt); break;
            case SyncStmt sync: EmitSyncStatement(sync); break;
            case LoadTextureStmt lt: EmitLoadTextureStatement(lt); break;
            case DrawTextureStmt dt2: EmitDrawTextureStatement(dt2); break;
            case DrawTexturesStmt dts2: EmitDrawTexturesStatement(dts2); break;
            case ImagePutsStmt ips: EmitImagePutsStatement(ips); break;
            case ViewStmt view2: EmitViewStatement(view2); break;
            case BSaveStmt bsave: EmitBSaveStatement(bsave); break;
            case BLoadStmt bload: EmitBLoadStatement(bload); break;
            case GraphicsPrintExprStmt gpe: EmitGraphicsPrintExprStatement(gpe); break;
            case GraphicsNewlineStmt gnl: EmitGraphicsNewlineStatement(gnl); break;
            case GraphicsTabStmt gtab: EmitGraphicsTabStatement(gtab); break;
            case GraphicsTabZoneStmt gtz: EmitGraphicsTabZoneStatement(gtz); break;
            case CSaveScreenStmt css: EmitCSaveScreenStatement(css); break;
            case CRestoreScreenStmt crs: EmitCRestoreScreenStatement(crs); break;
            case SpritedefStmt sdef: EmitSpriteDefStatement(sdef); break;
            case SpriteCommandStmt scmd: EmitSpriteCommandStatement(scmd); break;
            case TilemapCommandStmt tm: EmitTilemapCommandStatement(tm); break;
            case TickStmt tick: EmitTickStatement(tick); break;
            case TickEndStmt ticke: EmitTickEndStatement(ticke); break;
            case UnloadResourceStmt ur: EmitUnloadResourceStatement(ur); break;
            // ── Audio (IAudioService; headless → NullAudioService, SDL → SdlAudioService) ──
            case SoundStmt snd: EmitSoundStatement(snd); break;
            case SetFilterStmt sf: EmitSetFilterStatement(sf); break;
            case SoundLoadStmt sl: EmitSoundLoadStatement(sl); break;
            case SoundPlayStmt sp: EmitSoundPlayStatement(sp); break;
            case SoundVolumeStmt sv: EmitSoundVolumeStatement(sv); break;
            case PlayStmt ply: EmitPlayStatement(ply); break;
            case AudioControlStmt ac: EmitAudioControlStatement(ac); break;
            case SaveScreenStmt sss: EmitSaveScreenStatement(sss); break;
            case RestoreScreenStmt rss: EmitRestoreScreenStatement(rss); break;
            case ViewPortStmt vps: EmitWindowRegionStatement(vps); break;
            case SetClipperStmt scs: EmitSetClipperStatement(scs); break;
            case SetAttributeStmt sas: EmitSetAttributeStatement(sas); break;
            // Window/Graphics events
            case OnWindowStmt ows: EmitOnWindowStatement(ows); break;
            case DefineWindowStmt dws: EmitDefineWindowStatement(dws); break;
            // Input events
            case OnKeyStmt oks: EmitOnKeyStatement(oks); break;
            case OnMouseStmt oms2: EmitOnMouseStatement(oms2); break;
            case OnSpriteCollisionStmt oscs: EmitOnSpriteCollisionStatement(oscs); break;
            // Charting
            // CHART.PLOT/FINANCIAL/SHOW/SAVE/CLEAR/CLOSE → IGraphicsService (schema PCodeVM
            // CHART_* → CommonGraphics.ChartPlotManager). In headless NullGraphicsService
            // sono no-op; con SDL (SdlGraphicsService) creano/plot/scrivono il grafico reale.
            case ChartPlotStmt cps: EmitChartPlotStatement(cps); break;
            case ChartFinancialStmt cfs: EmitChartFinancialStatement(cfs); break;
            case ChartConfigStmt ccs: EmitChartConfig(ccs); break;
            case ChartDrawStmt cds: EmitChartDraw(cds); break;
            case ChartRenderStmt crs: EmitChartRenderStatement(crs); break;
            case ChartClearStmt ccs: EmitChartClearStatement(ccs); break;
            case ChartCloseStmt ccs2: EmitChartCloseStatement(ccs2); break;
            // Internal/OOP (no-op)
            case AmbiguousMethodStmt: break;
            case BaseConstructorCallStmt: break;
            case EvaluateIndicesStmt: break;
            // Procedura LOCALE: compilata come metodo dal loop delle procedure (raccolta da
            // LocalProcedureStmt) — qui nel corpo è solo la dichiarazione, no-op.
            case LocalProcedureStmt: break;
            default:
                throw new NotSupportedIlStatementException(stmt);
        }
    }

    /// <summary>
    /// Materializza una costante BASIC nel locale CLR associato al suo SymbolId. Il
    /// linker preserva l'uso del simbolo nel resto dell'AST, quindi l'emettitore non deve
    /// introdurre un secondo meccanismo di literal replacement; i normali load di variabile
    /// restano validi anche per espressioni costanti non banali gia' risolte dal linker.
    /// La protezione contro una riassegnazione resta responsabilita' del Linker.
    /// </summary>

    /// <summary>Funzioni LEDGER come espressioni (pattern DBF/ISAM): EOF/BOF/RECNO/FOUND/COUNT/
    /// LASTREC/GET/VALIDATE/REGISTER/BALANCE/ACCOUNTS/VARIANCE/PLOT/IMPORT/OPEN. Il primo
    /// argomento è l'handle → passa il NOME della variabile come alias (LedgerHandleFor),
    /// come fanno le forme statement. ADD/EDIT hanno la forma funzione con postings dict:
    /// usa la forma statement (LEDGER ADD/EDIT ... INTO).</summary>
}

/// <summary>
/// Lanciata quando il backend IL incontra uno Stmt non ancora coperto.
/// Segnala una lacuna di copertura nota (vedi TODO Obsidian, Filone B, fase
/// B7 — espansione sistematica), non un bug.
/// </summary>
public sealed class NotSupportedIlStatementException(Stmt stmt)
    : NotSupportedException($"Backend IL: statement '{stmt.GetType().Name}' non ancora supportato.")
{
    public Stmt Statement { get; } = stmt;
}

/// <summary>
/// Lanciata quando il backend IL incontra un Expr non ancora coperto.
/// Segnala una lacuna di copertura nota, non un bug.
/// </summary>
public sealed class NotSupportedIlExpressionException(Expr expr, string? callName = null)
    : NotSupportedException($"Backend IL: espressione '{expr.GetType().Name}'{(callName is null ? "" : $" '{callName}'")} non ancora supportata.")
{
    public Expr Expression { get; } = expr;
}
