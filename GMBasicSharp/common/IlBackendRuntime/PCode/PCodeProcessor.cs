// FILE: Basic.IlBackend.PCode.PCodeProcessor.cs
// PCodeProcessor — dispatch opcode e logica P-CODE.
// Usa PCodeTask, PCodeCallFrame, PCodeScheduler da BRT.Gpm.

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.Ledger;
using Compiler;
using System.Globalization;
using System.Linq;
using System.Net;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

/// <summary>Esito completo di una slice di esecuzione P-Code.</summary>
internal enum PCodeTickResult
{
    Completed,
    Faulted,
    Parked,
    Yielded,
    Preempted
}

/// <summary>
/// Processore P-CODE: esegue il dispatch degli opcode su un task.
/// </summary>
internal sealed partial class PCodeProcessor
{
    /// <summary>Massimo numero di argomenti/slot FFI (2026-11: era 8). Specchio di
    /// GMB_FFI_MAX_ARGS in gmb_bytecode/include/gmb_compiler.h.</summary>
    internal const int PCodeFfiMaxArgs = 64;

    private readonly IRuntimeHost _runtime;

    /// <summary>
    /// Registry handle numerico → oggetto NetworkHandle, popolato da ExecuteNetCmd quando
    /// TCP OPEN/CONNECT/ACCEPT creano gli handle. Serve a REMOTE/REMOTEPORT/LOCALPORT/RECV:
    /// il compilatore passa l'ID numerico (long), non l'oggetto — prima questi case ricevevano
    /// Int64 e il pattern 'is NetworkHandle' falliva sempre (IP/porta vuoti, RECV=0).
    /// </summary>
    private readonly Dictionary<long, CommonLib.Network.NetworkHandle> _netHandles = new();

    private readonly byte[] _code;
    private readonly List<object?> _constants;
    // Data pool DATA/RESTORE: i valori raccolti dal compilatore (PopulateDataPool). Prima
    // OP_READ leggeva da task.ReadNextData() — un placeholder che restituiva 0,1,2,... (bug:
    // READ dava valori di indice, non i DATA). Il puntatore è PER-VM (condiviso tra i task,
    // come la tabella DATA del modulo), non per-task.
    private readonly object[] _dataPool;
    private int _dataPointer;
    private readonly PCodeScheduler? _scheduler;
    private readonly object _consoleLock;
    private static readonly System.Random _random = new(42);
    private int _printColumn;
    private readonly PCodeVm? _vm;
    private readonly int _globalCount;
    private readonly string[] _hostGlobalNames;
    /// <summary>S_eval annidato: provider dello scope genitore (il chiamante che ha creato
    /// questa VM per una macro). Se impostati, il lookup di EVAL li consulta DOPO i nomi
    /// propri del chunk/frame — così `EVAL("EVAL(\"g\")")` vede i globali/locali dell'host
    /// anche se il livello intermedio non li nomina come identificatori. Catena induttiva:
    /// MacroEvalWithProvider installa qui il proprio provider concatenato. Null = radice.</summary>
    internal Func<string, bool>? MacroParentKnown;
    internal Func<string, object?>? MacroParentGet;
    internal Action<string, object?>? MacroParentSet;
    /// <summary>Sink dei PRINT di questa VM quando gira come macro annidata: invece di
    /// scrivere subito sulla console (flush di fine-tick del task figlio, che precede il
    /// flush del task ospite → output macro prima delle etichette PRINT del chiamante),
    /// il testo viene accodato ai PendingPrints del task ospite, preservando l'ordine.
    /// kind: 0 = PRINT senza newline finale, 1 = riga, 2 = newline puro (vedi PendingPrints).</summary>
    internal Action<int, string?>? PrintSink;
    /// <summary>S_eval: snapshot nomi-locali per procedura (chiave = Ip inizio), dal chunk ospite.
    /// Solo in-memory, mai serializzato (vedi CompiledChunk.ProcedureLocalNames).</summary>
    private readonly Dictionary<int, string?[]> _hostLocalNames;
    /// <summary>Indirizzi di inizio procedura ordinati — per risolvere la procedura corrente da task.Ip.</summary>
    private readonly int[] _procStartAddrs;
    private readonly int _localCount;

    /// <summary>
    /// Storage condiviso per i campi STATIC di CLASS, chiavato per SymbolId — condiviso da tutti i
    /// task/goroutine dell'intero programma (un solo PCodeProcessor per Chunk), a differenza dei
    /// campi di istanza (offset nel dizionario di ciascun oggetto). Popolato una volta sola dal
    /// prologo __static_init emesso dal Compiler (vedi Compiler.cs::CompileStaticFieldInitializers),
    /// letto/scritto da OP_LOAD_STATIC_FIELD/OP_STORE_STATIC_FIELD.
    /// </summary>
    private readonly Dictionary<int, object?> _staticFieldValues = new();

    /// <summary>
    /// "Dynamic scope" per NOME — per identificatori riservati non dichiarabili con DIM (keyword
    /// del linguaggio, es. FORM/QUERY nelle pagine BSP: TokenKind.Form/SqlQuery). Il Linker li marca
    /// con SymbolId = SymbolTable.DynamicScopeSymbolId invece di un vero slot globale (nessun DIM,
    /// nessuna riga in cui allocare un indice), quindi vanno risolti per NOME a runtime — parità con
    /// IlRuntime.DynamicScopeGet/Set nel backend IL. Un solo PCodeProcessor per Chunk (come
    /// _staticFieldValues sopra): l'host (es. TryServeBspPage) semina qui PRIMA di eseguire il
    /// chunk, OP_LOAD_DYNAMIC_SCOPE legge per nome.
    /// </summary>
    private readonly Dictionary<string, object?> _dynamicScope = new(StringComparer.OrdinalIgnoreCase);

    public void SetDynamicScope(string name, object? value) => _dynamicScope[name] = value;
    public object? GetDynamicScope(string name) => _dynamicScope.TryGetValue(name, out var v) ? v : null;


    /// <summary>
    /// True dopo la prima sottoscrizione a GraphicsManager.Instance.WindowResized/
    /// WindowCloseRequested — GraphicsManager e' un singleton di processo, quindi la
    /// sottoscrizione va fatta una sola volta per evitare handler duplicati se ON WINDOW
    /// viene eseguito più volte nello stesso programma.
    /// </summary>
    private bool _windowEventsSubscribed;

    // Database engines
    private sealed class IsamWorkArea
    {
        public Basic.ISAM.Engine? Engine;
        public Basic.UserDefinedType? Udt;
        public Basic.ISAM.Config.IsamSchema? Schema;
        public List<string>? LastAppendKeys = null;
        public List<int>? FieldSymbolIds;
        public List<string>? FieldNames;
        // Bug ISAM-Comandi-Parita-5-Backend: APPEND/DELETE/RECALL/PACK/REINDEX operano
        // direttamente su Engine (bypassando IsamDriver.Append/Delete/Recall/Pack), quindi
        // IsamDriver._recCount (letto da ISAM.LASTREC) resta la' fermo al valore dell'ultima
        // GO/SKIP/SEEK — mai aggiornato dopo una mutazione se nessuna navigazione e' seguita.
        // Esposto qui cosi' i chiamanti in PCodeProcessor.Syscalls.cs possono risincronizzarlo
        // subito dopo la mutazione, senza ri-risolvere l'area da zero.
        public Basic.Databases.Rdd.IsamDriver? Driver;
    }

    private string? _dbfDefaultPath = null;
    private readonly Dictionary<string, Basic.Databases.Speed.Database> _speedDbs = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<int, LedgerContext> _ledgerContexts = new();

    /// <summary>Rilascia TUTTE le risorse DB aperte da questo processore: DataSession
    /// (ISAM/DBF/SQL), database SPEED (WAL con FileShare.None) e contesti LEDGER.
    /// Chiamato dal syscall DBF_CLOSE_DATABASES e da <see cref="PCodeVm.Dispose"/> a fine
    /// run: senza il dispose a fine run il WAL SPEED restava aperto e il backend successivo
    /// nello STESSO processo (harness di parità PCode→IL) falliva con "file in uso"
    /// (vedi Bugs/(open) - SPEED-Parity-Runner-WAL-Lock).</summary>
    internal void CloseAllDatabases()
    {
        try { _dataSession.CloseAll(); _dataSession.Locks.ClearAll(); } catch { }
        foreach (var kv in _speedDbs) { try { kv.Value.Dispose(); } catch { } }
        _speedDbs.Clear();
        foreach (var kv in _ledgerContexts) { try { kv.Value.Dispose(); } catch { } }
        _ledgerContexts.Clear();
    }
    
    // RDD: DataSession unificato (condiviso con BasicRuntime.SqlDataSession)
    private Basic.Databases.Rdd.DataSession _dataSession => _runtime.SqlDataSession;

    private Basic.Databases.Rdd.SqlDriver ResolveSqlDriver(string alias) =>
        ((Basic.Databases.Rdd.SqlDriver)_dataSession.GetWorkArea(_dataSession.FindAreaByAlias(alias))?.Driver!)
        ?? FindAnySqlDriver()
        ?? throw new InvalidOperationException($"SQL database '{alias}' is not open.");

    private Basic.Databases.Rdd.SqlDriver? FindAnySqlDriver()
    {
        for (int i = 1; i <= Basic.Databases.Rdd.DataSession.MaxWorkareas; i++)
        {
            var wa = _dataSession.GetWorkArea(i);
            if (wa?.Driver is Basic.Databases.Rdd.SqlDriver sd) return sd;
        }
        return null;
    }

    // Lock per mutazione concorrente dei dizionari condivisi (_speedDbs, _ledgerContexts, _dataSession)
    private readonly object _isamLock = new();
    private readonly object _dbfLock = new();
    private readonly object _speedLock = new();
    private readonly object _ledgerLock = new();
    // BUG 23 (TODO-PCODE-BUGS.md): ResolveDbLock non aveva alcun lock per i syscall SQL_* e
    // mappava per errore tutti i syscall RDD_* a _isamLock (un driver SQL non dovrebbe contendere
    // con ISAM) — con goroutine concorrenti, accessi simultanei a _dataSession (RegisterDirect/
    // Unregister/FindAreaByAlias/AllocateArea/CloseAll) corrompevano _areas/_aliases.
    private readonly object _sqlLock = new();
    private readonly object _randomLock = new();
    private readonly object _staticFieldLock = new();

    // HTTP server
    /// <summary>Lock per l'avvio/arresto/riconfigurazione del server HTTP (non per la pipeline di
    /// dispatch delle richieste — quella gira in parallelo su piu' connessioni per design, vedi
    /// piano Obsidian PCode-HTTP-Server-Plan.md, filosofia "lock per sottosistema, non per
    /// richiesta").</summary>
    private readonly object _httpLock = new();
    private PCodeHttpServerInstance? _httpServer;
    private PCodeHttpRouter? _httpRouter;
    private HttpConfig? _httpConfig;
    private object?[]? _httpGlobalsTemplate;
    private Basic.IlBackend.PCode.Http.PCodeHttpRateLimiter? _httpRateLimiter;
    private string? _httpBspDir;
    private string? _httpStaticBaseDir;
    private CommonLib.Http.BspSessionManager _bspSessionManager => CommonLib.Http.SessionStores.Default;
    /// <summary>Handle di continuazione della catena middleware→handler registrato per PCodeTask al
    /// momento dello spawn (SpawnHttpHandlerTask) — indipendentemente da quanti parametri dichiara
    /// la SUB di destinazione. Fallback per ExecuteHttpUseNext quando la sintassi implicita
    /// "HTTP USE NEXT" (senza handle esplicito) compare in un middleware che non dichiara un
    /// parametro "next" (es. HttpRoutes.bas, LoggerMiddleware a 2 parametri): in quel caso
    /// l'argomento BASIC risolto a runtime è NIL (variabile "next" mai dichiarata in quello scope,
    /// vedi Compiler.Variables.cs LoadVariable — push NIL silenzioso, comportamento generale per
    /// qualunque nome non dichiarato, non specifico qui), quindi va ripescato da questa tabella.
    /// ConditionalWeakTable: nessuna pulizia esplicita necessaria, la entry sparisce da sola quando
    /// il PCodeTask non è più referenziato (fine richiesta).</summary>
    private readonly System.Runtime.CompilerServices.ConditionalWeakTable<PCodeTask, Basic.IlBackend.PCode.Http.PCodeHttpNextHandle> _httpTaskNextHandle = new();
    private readonly Dictionary<string, int> _procedureAddresses;
    private readonly Dictionary<int, int> _procedureArities;
    private readonly IReadOnlyDictionary<int, TryRegionMetadata> _tryRegions;
    private System.Collections.Generic.Stack<object?>? _vmStack;
    // Island bridge
    private ClipperLib.Controls.WindowManager? _tuiWindows;
    private bool _tuiWindowEventsSubscribed;
    // Finestre PIXEL (Fase W-Pixel) — solo con schermo grafico attivo
    private CommonGraphics.Graphics.GfxWindowManager? _gfxWindows;
    // Memory Mapped Files (IO.MEMORYMAP suite): UNICA implementazione in
    // IlRuntime.MemoryMap/MemoryMapClose (registry condiviso con la gamba IL) —
    // qui nessun dizionario locale (prima ce n'era uno mai popolato: i comandi
    // MMAPPOKE/MMAPWRITEBYTES no-op e i CLOSE senza dispose).
    // Audio (SOUND/PLAY/FILTER) — AudioManager per-run (SDL2_mixer via CommonGraphics)
    private CommonGraphics.AudioManager? _audioManager;
    // Eventi generici (ADD/REMOVE HANDLER + RAISE EVENT): nomeEvento → procedure handler
    private System.Collections.Generic.Dictionary<string, System.Collections.Generic.List<string>>? _eventHandlers;
    /// <summary>Collisioni del SYNC precedente (salvate dalla callback del game loop, main
    /// thread) — il DISPATCH avviene nel WORKER (nel case SYNC), mai dal main thread:
    /// DispatchEventHandler dal main su un task bloccato congelava il loop di animazione.</summary>
    private System.Collections.Generic.List<(int id1, int id2, float x, float y)>? _pendingSpriteCollisions;
    /// <summary>Alterna il dispatch tra SPRITE_COLLISION (x,y) e SPRITE_COLLISIONS (array id):
    /// UN SOLO handler per SYNC — due dispatch nello stesso SYNC lasciavano il frame della
    /// prima SUB orfano (il main perdeva le sue variabili → Alive=null → EXIT DO immediato
    /// = "freeze" dopo ~1s) e lo stack cresceva (overflow OP_PUSH_NIL).</summary>
    private bool _dispatchNextPlural;
    private (string Handler, object?[] Args)[]? _pendingEventHandlers;
    private int _pendingEventIndex;
    // Buffer console per SAVE/RESTORE SCREEN in modalità testo (nome → griglia)
    private System.Collections.Generic.Dictionary<string, CommonLib.Screens.ScreenCell[,]>? _consoleScreens;
    // IO WATCH (FileSystemWatcher per-run): path → watcher
    private System.Collections.Generic.Dictionary<string, System.IO.FileSystemWatcher>? _fileWatchers;
    private System.Collections.Generic.Dictionary<string, string>? _fileWatchCallbacks;

    /// <summary>
    /// Crea il WindowManager TUI (per-run) e sottoscrive gli eventi delle finestre al
    /// EventDispatcher UNIFICATO: ogni evento finestra (open/close/move/focus/minimize/
    /// restore) diventa un DispatchWindow(kind). WindowEventKind (Ast/Graphics.cs):
    /// Close=0, Resize=1, Quit=2, Move=3, Focus=4, Minimize=5, Restore=6, Open=7, Paste=8.
    /// </summary>
    private void EnsureTuiWindows()
    {
        if (_tuiWindows != null) return;
        _tuiWindows = new ClipperLib.Controls.WindowManager(new ClipperLib.Clipper.ScreenRect(0, 0, 80, 24));
        try { _tuiWindows.ClipboardService = _runtime.Clipboard; } catch { }
        if (_tuiWindowEventsSubscribed) return;
        _tuiWindowEventsSubscribed = true;
        try
        {
            _tuiWindows.WindowOpened += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(7, e.Id);   // Open
            _tuiWindows.WindowClosed += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(0, e.Id);   // Close
            _tuiWindows.WindowMoved += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(3, e.Id);    // Move
            _tuiWindows.WindowResized += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(1, e.Id);  // Resize
            _tuiWindows.WindowFocused += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(4, e.Id);  // Focus
            _tuiWindows.WindowMinimized += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(5, e.Id);// Minimize
            _tuiWindows.WindowRestored += (_ , e) => CommonLib.App.EventDispatcher.Instance.DispatchWindow(6, e.Id); // Restore
        }
        catch { }
    }

    private static bool _mouseSubscribed;
    private static bool _breakSubscribed;
    private static bool _pcodeMouseDown;

    /// <summary>Instrada un evento mouse alle finestre gestite (TUI cell-based e PIXEL).
    /// kind: 0=down, 1=up, 2=drag. x,y = coordinate PIXEL dello schermo grafico. Per le
    /// finestre TUI converte in celle con la cella font corrente; per le PIXEL usa i pixel.</summary>
    private void RouteGuiMousePCode(int kind, int x, int y)
    {
        try
        {
            if (kind == 0) _pcodeMouseDown = true;
            else if (kind == 1) _pcodeMouseDown = false;
            // Finestre PIXEL (schermo grafico attivo e finestre presenti).
            if (_gfxWindows is { } gp && gp.Windows.Count > 0)
            {
                var gm = PCodeGraphics.Instance;
                if (gm.IsScreenOn)
                {
                    gp.HandleMouse(kind, x, y);
                    RenderPixelWindowsForPCode();
                }
            }
        }
        catch { }
        try
        {
            // Finestre TUI (cell-based): converte pixel → cella.
            if (_tuiWindows is not { } wm || wm.WindowCount == 0) return;
            var g = PCodeGraphics.Instance;
            int cellW, cellH;
            try
            {
                cellW = Math.Max(1, g.CharCellWidth);
                cellH = Math.Max(1, g.CharCellHeight);
            }
            catch { cellW = 8; cellH = 16; }
            var cx = x / cellW;
            var cy = y / cellH;
            ClipperLib.Clipper.MouseAction ma = kind switch
            {
                0 => ClipperLib.Clipper.MouseAction.Click(cx, cy),
                1 => ClipperLib.Clipper.MouseAction.Release(cx, cy),
                _ => ClipperLib.Clipper.MouseAction.Drag(cx, cy),
            };
            wm.HandleEvent(new ClipperLib.Clipper.UiMouse(ma));
            RenderTuiWindowsForPCode();
        }
        catch { }
    }
    public static DebugServer? DebugServer;

    /// <summary>
    /// Sottoscrive una sola volta gli eventi mouse SDL (SdlManager — mouse GUI) e li inoltra
    /// al EventDispatcher unificato (ON MOUSE). Gli eventi arrivano da un thread SDL:
    /// DispatchMouse spawna il task dell'handler (thread-safe, come FireWindowEvent).
    /// </summary>
    private void SubscribeGuiMouseEvents()
    {
        if (_mouseSubscribed) return;
        _mouseSubscribed = true;
        try
        {
            var mgr = CommonGraphics.Consoles.SdlManager.Instance;
            mgr.MouseButtonEvent += e =>
            {
                int kind = e.type == SDL2.SDL.SDL_EventType.SDL_MOUSEBUTTONDOWN ? 2 : 3; // Down/Up
                int x = e.button.x, y = e.button.y;
                int b = e.button.button switch { 2 => 1, 3 => 2, _ => 0 };
                // Stato mouse → runtime C (`gmb_clipper_mouse_update_simple`): kind GMB_MA_*
                // (0=CLICK, 2=RIGHTCLICK, 3=MIDDLECLICK, 9=RELEASE). Le letture MOUSE.* passano
                // da IlRuntime.MouseX() → bridge C, quindi lo stato C# non è più la sorgente.
                try { Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseUpdateEvent(kind == 2 ? (b == 1 ? 2 : b == 2 ? 3 : 0) : 9, x, y); } catch { }
                CommonLib.App.EventDispatcher.Instance.DispatchMouse(kind, x, y);
                // Instrada il click alle finestre gestite (TUI e PIXEL).
                RouteGuiMousePCode(kind == 2 ? 0 : 1, x, y);
            };
            mgr.MouseMotionEvent += e =>
            {
                // Movimento → runtime C (kind 11 = GMB_MA_MOVE; coordinate pixel GUI).
                try { Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseUpdateEvent(11, e.motion.x, e.motion.y); } catch { }
                CommonLib.App.EventDispatcher.Instance.DispatchMouse(4, e.motion.x, e.motion.y); // Move
                if (_pcodeMouseDown) RouteGuiMousePCode(2, e.motion.x, e.motion.y);
            };
            mgr.MouseWheelEvent += e =>
            {
                try
                {
                    // Rotella → runtime C (5=GMB_MA_SCROLLUP, 6=GMB_MA_SCROLLDOWN).
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseUpdateEvent(e.wheel.y > 0 ? 5 : 6, Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseX(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseY());
                    CommonLib.App.EventDispatcher.Instance.DispatchMouse(e.wheel.y > 0 ? 5 : 6, Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseX(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperMouseY());
                }
                catch { }
            };
        }
        catch { }
    }

    /// <summary>
    /// CTRL+C (BreakRequested del keyboard service) — un evento come gli altri nel
    /// EventDispatcher UNIFICATO: DispatchKey(KeyCombination(C, Control)); se un handler
    /// ON KEY(Ctrl+C) è registrato lo consuma, altrimenti (default) il programma viene
    /// interrotto (SignalShutdown sullo scheduler).
    /// </summary>
    private void SubscribeBreakEvent()
    {
        if (_breakSubscribed) return;
        _breakSubscribed = true;
        try
        {
            _runtime.Keyboard.BreakRequested += () =>
            {
                var consumed = CommonLib.App.EventDispatcher.Instance.DispatchKey(new CommonLib.Keyboard.KeyCombination(ConsoleKey.C, ConsoleModifiers.Control));
                if (!consumed)
                {
                    try { _scheduler?.SignalShutdown(); } catch { }
                }
            };
        }
        catch { }
    }

    private readonly int _chunkFormatVersion = 2;
    private readonly Dictionary<int, string> _addrToProcedureName;
    private Dictionary<int, Basic.Runtime.Gpm.IslandRun> _islandByAddr = new();
    private int _islandCacheVersion = -1;
    private int[]? _debugSourceLines;
    /// <summary>Testo delle righe sorgente (indice = riga 0-based) per ERR.SourceLineText;
    /// arriva dalla coda opzionale del chunk (null = chunk vecchio → "").</summary>
    private string[]? _debugSourceTexts;
    /// <summary>Catena del corpo di un generatore il cui errore sta per essere ri-lanciato nel
    /// chiamante: <see cref="BuildBasicTrace"/> la antepone e la consuma.</summary>
    private string? _pendingTracePrefix;
    /// <summary>Task e ip dell'istruzione in esecuzione quando si costruisce la catena per un
    /// errore nato nel CORPO di un generatore (vedi DriveGeneratorTask): il corpo non aggiorna
    /// <see cref="PCodeTask.CurrentInstructionIp"/> — farlo cambierebbe la semantica d'errore —
    /// quindi l'ip giusto si fornisce qui, per la sola lettura della riga.</summary>
    private PCodeTask? _generatorTraceTask;
    private int _generatorTraceIp = -1;
    /// <summary>Riga da usare per il PROSSIMO RaiseBasicError (re-raise di un errore di
    /// GENERATORE: la riga del corpo, non quella del chiamante dove sta la NEXT).</summary>
    private int _pendingLineOverride;

    public PCodeProcessor(IRuntimeHost runtime, CompiledChunk chunk, PCodeScheduler? scheduler = null, object? consoleLock = null, PCodeVm? vm = null)
    {
        SubscribeGuiMouseEvents();
        SubscribeBreakEvent();
        _runtime = runtime;
        _chunkFormatVersion = chunk.FormatVersion;
        _code = chunk.Code.ToArray();
        _debugSourceLines = chunk.Lines.Count == chunk.Code.Count ? chunk.Lines.ToArray() : null;
        _debugSourceTexts = chunk.SourceTexts.Length > 0 ? chunk.SourceTexts : null;
        _debugStatementRanges = chunk.StatementRanges.Count > 0 ? chunk.StatementRanges.ToArray() : null;
        _localNumericByAddr = chunk.ProcedureLocalNumeric.Count > 0 ? chunk.ProcedureLocalNumeric : null;
        _constants = chunk.Constants.ToList();
        _dataPool = chunk.Data.ToArray();
        _dataPointer = 0;
        _scheduler = scheduler;
        _consoleLock = consoleLock ?? new object();
        _vm = vm;
        _globalCount = chunk.GlobalCount;
        _hostGlobalNames = chunk.GlobalNames ?? [];
        _hostLocalNames = chunk.ProcedureLocalNames;
        _procStartAddrs = chunk.ProcedureLocalNames.Keys.OrderBy(a => a).ToArray();
        _localCount = chunk.LocalCount > 0 ? chunk.LocalCount : 256;
        _procedureAddresses = chunk.ProcedureAddresses;
        _procedureArities = chunk.ProcedureArities;
        _tryRegions = chunk.TryRegions;
        _addrToProcedureName = new Dictionary<int, string>(chunk.ProcedureAddresses.Count);
        foreach (var kv in chunk.ProcedureAddresses)
            _addrToProcedureName[kv.Value] = kv.Key;
        // Invoke-by-name per le procedure di QUESTO chunk: le registra (marker -(ip+1) → path
        // PCodeVM in IlRuntime.InvokeBasicProcedureGpm) + installa l'hook che spawna il task PCode
        // sul QUESTO scheduler col FutureToResolve — la GPM PCode è l'unico esecutore, MAI il
        // thread del chiamante esterno (PLUGINCALL/agent-tool/MCP server).
        foreach (var kv in chunk.ProcedureAddresses)
            Basic.IlBackend.IlRuntime.RegisterPCodeProcedure(kv.Key, kv.Value,
                _procedureArities.TryGetValue(kv.Value, out var pcArity) ? pcArity : 0);
        // Dispatcher MCP → procedure BASIC (per gli agenti con tool anche senza MCP SERVE) —
        // il Main IL lo chiama nel prologo (EnsureMcpDispatcher), il PCodeVM lo cabla qui.
        Basic.IlBackend.IlRuntime.EnsureMcpDispatcher();
        Basic.IlBackend.IlRuntime.PCodeInvokeHook = (ip, args, globals) =>
        {
            var task = _scheduler!.CreateTask(ip);
            task.Scheduler = _scheduler;
            if (globals is { Length: > 0 } g) task.Globals = g;
            else if (_vm is { } vmV) task.Globals = vmV.MainTask.Globals;
            var future = new Basic.Runtime.Gpm.PCodeFutureValue();
            task.FutureToResolve = future;
            if (_procedureArities.TryGetValue(ip, out var arity) && arity < args.Length)
                args = args[..arity];
            foreach (var a in args) task.PushRef(a);
            _scheduler.Schedule(task);
            return future;
        };
        Basic.Runtime.Gpm.IslandRuntime.SetFileHost(runtime);
        // AiVectorRuntime usa un DataSession condiviso: l'Interpreter lo inizializzava
        // all'avvio (Gpm all'avvio); nella PCodeVM lo si aggancia al runtime host.
        Basic.AI.Rag.AiVectorRuntime.SqlDataSession = _runtime.SqlDataSession;
    }

    private object?[] CreateLocals() => CreateLocals(-1);

    /// <summary>Crea l'array dei locali e inizializza a 0.0 gli slot NUMERICI della
    /// procedura <paramref name="procAddr"/> (metadata ProcedureLocalNumeric). Senza, una
    /// variabile la cui unica scrittura non viene mai eseguita restava null (stampa vuota,
    /// TYPENAME NIL) mentre IL/C99Native danno 0/DOUBLE.</summary>
    private object?[] CreateLocals(int procAddr)
    {
        var locals = new object?[_localCount];
        InitNumericLocals(locals, procAddr);
        return locals;
    }

    private void InitNumericLocals(object?[] locals, int procAddr)
    {
        if (_localNumericByAddr is null || procAddr < 0) return;
        if (!_localNumericByAddr.TryGetValue(procAddr, out var flags)) return;
        for (int i = 0; i < flags.Length && i < locals.Length; i++)
            if (flags[i] && locals[i] is null) locals[i] = 0.0;
    }

    /// <summary>Inizializza gli slot numerici del task radice (top-level/Main).</summary>
    internal void InitRootNumericLocals(PCodeTask task, int procAddr)
        => InitNumericLocals(task.GetLocals(), procAddr);

    /// <summary>Valuta una macro (&amp;/EVAL) contro lo scope della PCodeVM corrente, semantica
    /// S_eval (scope figlio transiente, vincolante 2026-09-09): lookup nome → LOCALI della procedura
    /// corrente (snapshot <see cref="_hostLocalNames"/>) PRIMA dei globali per nome via
    /// <see cref="_hostGlobalNames"/> → <c>task.Globals</c>. Le DIM/LET/implicite-su-ignote del
    /// codice macro vivono nei locals del chunk annidato e muoiono con esso (contenimento del
    /// compilatore, mai leak nell'host); le scritture su nomi esterni fanno write-back in place
    /// (slot frame, con unwrapping ByRefBox/RefToVar come OP_STORE_LOCAL, o globale). Delega a
    /// IlRuntime.MacroEvalWithProvider (parse+compile+RunSync single-shot) — trasposizione del
    /// modello VM/C (gmb_expr: env {task,chunk} + get/set per nome).</summary>
    private object? EvalMacroInPCode(PCodeTask task, string code, bool asExpr)
    {
        if (string.IsNullOrWhiteSpace(code)) return asExpr ? code : null;
        var hostGlobals = task.Globals;
        // Binda i parent in locali: se un altro thread li azzerasse dopo l'install,
        // la catena resterebbe valida per tutta questa eval.
        var pKnown = MacroParentKnown;
        var pGet = MacroParentGet;
        var pSet = MacroParentSet;
        // Frame locali correnti — stesso pattern di OP_LOAD_LOCAL/OP_STORE_LOCAL (incluso il
        // fallback task.GetLocals() quando il frame non ha array proprio).
        var hostLocals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
        var localNames = ResolveProcLocalNames(task.Ip);
        int LocalSlot(string name)
        {
            if (localNames == null) return -1;
            var norm = CompiledChunk.NormalizeLocalName(name);
            // Ricerca dal PIÙ PROFONDO (indice più alto): come ResolveLocal del compilatore,
            // vince la dichiarazione più recente — un contatore FOR o una DIM di blocco che
            // MASCHERANO una locale di procedura stanno su slot più alti (first-match le
            // nasconderebbe: EVAL("j") dentro il loop leggeva lo slot del proc-j mai aggiornato).
            for (var i = localNames.Length - 1; i >= 0; i--)
            {
                if (i >= hostLocals.Length) continue;
                var n = localNames[i];
                if (!string.IsNullOrEmpty(n) && n.Equals(norm, StringComparison.OrdinalIgnoreCase)) return i;
            }
            return -1;
        }
        bool Known(string name)
        {
            if (LocalSlot(name) >= 0) return true;
            if (_hostGlobalNames.Length != 0 && hostGlobals != null
                && Array.IndexOf(_hostGlobalNames, name) >= 0) return true;
            // S_eval annidato: cade nello scope genitore (catena verso l'host).
            if (pKnown?.Invoke(name) ?? false) return true;
            // S_eval: livello record DBF per ultimo (dopo globali e scope genitore: nessun
            // cambio per i nomi esistenti, nuova capacità solo per nomi altrimenti ignoti;
            // i transienti vincono sul persistente — mai mutazioni DB accidentali).
            return TryReadDbfRecordField(task, name, out _);
        }
        object? Get(string name)
        {
            var ls = LocalSlot(name);
            if (ls >= 0)
            {
                // Specchio di OP_LOAD_LOCAL: ByRefBox unwrappato in lettura.
                var lv = hostLocals[ls];
                return lv is ByRefBox box ? box.Value : lv;
            }
            if (_hostGlobalNames.Length != 0 && hostGlobals != null)
            {
                var i = Array.IndexOf(_hostGlobalNames, name);
                if (i >= 0 && i < hostGlobals.Length) return hostGlobals[i];
            }
            // Scope genitore prima del record (il nil di un temp esistente non deve
            // cadere nel record: Known distingue "ignoto" da "nil").
            if (pKnown?.Invoke(name) ?? false) return pGet?.Invoke(name);
            if (TryReadDbfRecordField(task, name, out var recVal)) return recVal;
            return null;
        }
        void Set(string name, object? value)
        {
            var ls = LocalSlot(name);
            if (ls >= 0)
            {
                // Specchio di OP_STORE_LOCAL (ByRefBox/RefToVar scritti in place) + ReturnValue,
                // come se l'assegnazione fosse eseguita inline dal chiamante.
                if (hostLocals[ls] is ByRefBox box) box.Value = value;
                else if (hostLocals[ls] is RefToVar rvSL) rvSL.Array[rvSL.Index] = value;
                else hostLocals[ls] = value;
                if (task.Frame != null) task.Frame.ReturnValue = value;
                return;
            }
            if (_hostGlobalNames.Length != 0 && hostGlobals != null)
            {
                var i = Array.IndexOf(_hostGlobalNames, name);
                if (i >= 0 && i < hostGlobals.Length) { hostGlobals[i] = value; return; }
            }
            // Scope genitore prima del record (un temp esistente ombreggia il campo).
            if (pKnown?.Invoke(name) ?? false) { pSet?.Invoke(name, value); return; }
            // S_eval: assegnazione a nome campo scrive nel record (parità Clipper).
            if (TryWriteDbfRecordField(task, name, value)) return;
            pSet?.Invoke(name, value);
        }
        // S_eval §ii: contesto host per OP_CALL_HOST — inoltra quello ricevuto (livelli oltre
        // il primo) oppure costruiscilo dal chunk ospite (radice). Stesso task ospite: il pump
        // sincrono lo prende in prestito con save/restore (stesso thread, mai concorrente).
        var hostTable = _vm?.MacroHostTable ?? BuildHostProcTable();
        Func<int, object?[], object?>? hostInvoker = _vm?.MacroHostInvoker
            ?? ((idx, args) => hostTable == null || idx < 0 || idx >= hostTable.Count
                ? throw new InvalidOperationException($"OP_CALL_HOST idx={idx}: tabella host assente.")
                /* Parametri OPTIONAL omessi: il percorso PCode invoca per INDIRIZZO (niente AST
                 * nel compilatore del frammento) → i default letterali si applicano qui, così
                 * `EVAL("Opt(5)")` dà lo stesso valore della chiamata diretta. */
                : CallHostProcedureSync(task, hostTable[idx].Addr,
                    Basic.IlBackend.IlRuntime.ExpandOmittedOptionals(hostTable[idx].Name, hostTable[idx].Arity, args)));
        return Basic.IlBackend.IlRuntime.MacroEvalWithProvider(code, asExpr, _runtime, Known, Get, Set,
            MacroParentKnown, MacroParentGet, MacroParentSet, hostTable, hostInvoker,
            // Macro-comando: i PRINT della VM figlia si accodano ai PendingPrints di QUESTO
            // task (flush unico a fine tick del chiamante) → ordine interleaving preservato
            // (bug ClipperMacroCanary: l'output macro precedeva le etichette PRINT).
            // Prima di accodare, l'eventuale riga IN STAGING del chiamante (`PRINT "A";`)
            // viene spostata nei pending come kind 0: lo staging sopravvive ai tick e
            // verrebbe flushato solo a fine riga, scavalcando l'output macro.
            (kind, text) =>
            {
                if (task.PrintLineStaging is { Length: > 0 } staged)
                {
                    (task.PendingPrints ??= new()).Add((0, staged.ToString(),
                        Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(),
                        Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                    task.PrintLineStaging = null;
                }
                (task.PendingPrints ??= new()).Add(((byte)kind, text,
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(),
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
            });
    }

    /// <summary>S_eval §ii: tabella host-procs dal chunk ospite (nome → addr+arity).
    /// Solo voci con arità nota; null se vuota (macro senza host chiamabile).</summary>
    private List<Compiler.MacroHostProc>? BuildHostProcTable()
    {
        if (_procedureAddresses.Count == 0) return null;
        var table = new List<Compiler.MacroHostProc>();
        foreach (var (name, addr) in _procedureAddresses)
        {
            if (!_procedureArities.TryGetValue(addr, out var ar)) continue;
            /* Default letterali dei parametri OPTIONAL: il compilatore del frammento li
             * pusha quando la macro li omette (altrimenti la procedura riceveva 0). */
            table.Add(new Compiler.MacroHostProc(name, addr, ar,
                Basic.IlBackend.IlRuntime.HostProcDefaults(name, ar)));
        }
        return table.Count > 0 ? table : null;
    }

    /// <summary>S_eval §ii: esegue una procedura HOST in modo sincrono sullo stesso task
    /// (pattern CallClosureSync ma per procedure nominate del chunk ospite, non closure).
    /// Usato da OP_CALL_HOST nelle macro: salva sp/frame/ip/eccezione, spinge un frame
    /// con gli argomenti, ticka finché il frame non torna, preleva il risultato e
    /// ripristina tutto. Park/yield dentro la chiamata = errore esplicito (un'attesa
    /// sincrona non può completarsi: deadlock garantito). Guard 1M istruzioni come VM/C.</summary>
    internal object? CallHostProcedureSync(PCodeTask hostTask, int addr, object?[] args)
    {
        if (addr < 0 || addr >= _code.Length)
            throw new InvalidOperationException($"chiamata host a indirizzo {addr} fuori range.");
        foreach (var a in args) hostTask.PushRef(a);
        var savedSp = hostTask.Sp;
        var savedFrame = hostTask.Frame;
        var savedIp = hostTask.Ip;
        var savedExc = hostTask.PendingException;
        var frame = new PCodeCallFrame
        {
            SavedStackPointer = savedSp,
            ReturnIp = savedIp,
            Previous = savedFrame,
            LocalVariables = CreateLocals(addr),
            ReturnValue = null
        };
        EnterProcedureErrorScope(hostTask, frame);
        hostTask.Frame = frame;
        hostTask.Ip = addr;
        object? result = null;
        try
        {
            var guard = 0;
            while (hostTask.Frame != null && hostTask.Frame != savedFrame && hostTask.State == PCodeTaskState.Running)
            {
                if (++guard > 1000000)
                    throw new InvalidOperationException("EVAL: chiamata host supera 1M istruzioni (loop infinito?).");
                var ip = hostTask.Ip;
                if (ip < 0 || ip >= _code.Length) break;
                hostTask.CurrentInstructionIp = ip;
                hostTask.Ip++;
                if (!DispatchOpcode(hostTask, (OpCode)_code[ip])) break;
            }
            if (hostTask.Frame != null && hostTask.Frame != savedFrame)
                throw new InvalidOperationException("EVAL: la procedura chiamata non può parcheggiare/yieldare (I/O o GPM dentro EVAL sincrono).");
            if (hostTask.PendingException != null)
                throw new InvalidOperationException($"EVAL: errore nella procedura chiamata: {hostTask.PendingException.Message}", hostTask.PendingException);
            if (hostTask.Sp > savedSp - args.Length) result = hostTask.PopRef();
        }
        finally
        {
            if (hostTask.Frame == null || hostTask.Frame != savedFrame)
                hostTask.Frame = savedFrame;
            hostTask.Sp = savedSp - args.Length;
            hostTask.Ip = savedIp;
            hostTask.PendingException = savedExc;
        }
        return result;
    }

    /// <summary>S_eval + OP_LOAD_FIELD: legge un campo dal record corrente dell'area DBF
    /// attiva (parità IL DbfLateBindField). Solo driver reali con campi; mai eccezioni.
    /// Usato sia dal LOAD_FIELD con receiver NIL (bare `NAME`) sia dal livello record
    /// del provider EVAL/macro (`&cf` con cf="NAME").</summary>
    private bool TryReadDbfRecordField(PCodeTask task, string? name, out object? value)
    {
        value = null;
        if (string.IsNullOrEmpty(name)) return false;
        try
        {
            var drv = ResolveDbfDriver(task, null);
            if (drv == null) return false;
            var fi = drv.GetFieldIndex(name);
            if (fi < 0) return false;
            value = drv.GetFieldValue(fi);
            return true;
        }
        catch { return false; }
    }

    /// <summary>S_eval: `EVAL("CAMPO = v")` scrive nel record corrente (parità Clipper:
    /// l'assegnazione a nome campo aggiorna il record, non crea una variabile).</summary>
    private bool TryWriteDbfRecordField(PCodeTask task, string? name, object? value)
    {
        if (string.IsNullOrEmpty(name)) return false;
        try
        {
            var drv = ResolveDbfDriver(task, null);
            if (drv == null) return false;
            var fi = drv.GetFieldIndex(name);
            if (fi < 0) return false;
            drv.SetFieldValue(fi, value);
            return true;
        }
        catch { return false; }
    }

    /// <summary>Risolve i nomi-locali snapshot della procedura che contiene <paramref name="ip"/>
    /// (indirizzo di inizio massimo &lt;= ip), o null se l'Ip è fuori da qualunque procedura nota
    /// (chunk legacy senza snapshot, o codice sintetico). Le locali di BLOCCO non sono incluse
    /// nello snapshot: un EVAL dentro un blocco che nomina una block-local non la vede
    /// (NIL o variabile annidata, come prima); limite noto: se uno slot riusato da un DIM di
    /// procedura omonimo dichiarato DOPO il blocco, l'EVAL dentro il blocco legge quello slot —
    /// il pattern standard (DIM in testa alla procedura) è esatto.</summary>
    private string?[]? ResolveProcLocalNames(int ip)
    {
        var addrs = _procStartAddrs;
        if (addrs.Length == 0) return null;
        var lo = 0; var hi = addrs.Length - 1; var best = -1;
        while (lo <= hi)
        {
            var mid = (lo + hi) >> 1;
            if (addrs[mid] <= ip) { best = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        if (best < 0) return null;
        return _hostLocalNames.TryGetValue(addrs[best], out var names) ? names : null;
    }


    private bool? DispatchIsland(PCodeTask task, int addr, bool isCallN, int savedSp)
    {
        var version = Basic.Runtime.Gpm.IslandRegistry.Version;
        if (version != _islandCacheVersion)
        {
            var rebuild = new Dictionary<int, Basic.Runtime.Gpm.IslandRun>();
            foreach (var (procName, procAddr) in _procedureAddresses)
                if (Basic.Runtime.Gpm.IslandRegistry.TryGet(procName, out var island))
                    rebuild[procAddr] = island;
            _islandByAddr = rebuild;
            _islandCacheVersion = version;
        }
        if (!_islandByAddr.TryGetValue(addr, out var islandRun)) return null;
        var prevSleepDeadline = task.Frame?.IslandSleepDeadlineMs ?? 0;
        var prevSendRetry = task.Frame?.IslandSendRetry ?? false;
        var prevRecvRetry = task.Frame?.IslandRecvRetry ?? false;
        var newFrame = isCallN
            ? new PCodeCallFrame { SavedStackPointer = savedSp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = CreateLocals(addr), ReturnValue = null, IslandSleepDeadlineMs = prevSleepDeadline, IslandSendRetry = prevSendRetry, IslandRecvRetry = prevRecvRetry }
            : new PCodeCallFrame { SavedStackPointer = savedSp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = CreateLocals(addr), IslandSleepDeadlineMs = prevSleepDeadline, IslandSendRetry = prevSendRetry, IslandRecvRetry = prevRecvRetry };
        EnterProcedureErrorScope(task, newFrame);
        task.Frame = newFrame;
        Basic.Runtime.Gpm.IslandRuntime.SetCurrentTask(task);
        Basic.Runtime.Gpm.IslandRuntime.SetFileHost(_runtime);
        try { var result = islandRun(task); return result == Basic.Runtime.Gpm.IslandResult.Continue; }
        finally { Basic.Runtime.Gpm.IslandRuntime.SetCurrentTask(null); }
    }

    /// <summary>Esegue un tick e restituisce true se è terminato con OP_YIELD (il chiamante deve
    /// riaccodare il task), false se bloccato/finito/in eccezione (nessun riaccodamento, già
    /// gestito altrove o non necessario). Il valore di ritorno è LOCALE sullo stack del
    /// chiamante — MAI un campo sul task condiviso (vedi PCodeVm.HandlePostTick per il perché:
    /// un campo come il precedente PCodeTask.LastTickWasYield resta scritturabile da un ALTRO
    /// thread che esegua un tick SUCCESSIVO dello stesso task prima che questo torni al
    /// chiamante, portando a una doppia esecuzione della stessa PCodeTask sotto vero
    /// parallelismo — bug reale, causa di corruzione/hang intermittenti in
    /// GoRoutinesWorkers.bas).</summary>
    public bool ExecuteTaskTick(PCodeTask task) => ExecuteTaskSlice(task, int.MaxValue) == PCodeTickResult.Yielded;

    /// <summary>Esegue al massimo <paramref name="instructionBudget"/> opcode e restituisce
    /// una transizione tipizzata, così lo scheduler distingue yield, parking, preemption e fine.</summary>
    public PCodeTickResult ExecuteTaskSlice(PCodeTask task, int instructionBudget)
    {
        if (instructionBudget <= 0)
            throw new ArgumentOutOfRangeException(nameof(instructionBudget));

        var executedInstructions = 0;
        try
        {
            while (!task.IsFinished && executedInstructions < instructionBudget)
            {
                try
                {
                    if (task.CancellationRequested && !task.CancellationUnwindStarted)
                    {
                        if (BeginCancellationUnwind(task))
                            continue;
                        return PCodeTickResult.Faulted;
                    }
                    if (CompleteFinallyIfNeeded(task))
                        continue;
                    // Un builtin "parked" (ParkBlocking: LLM FINETUNE/LORA APPLY, AIAGENTASK,
                    // AIDISTILL*, AIVECTORASK, ...) fallito sul thread pool arriva qui con
                    // task.PendingException già valorizzata da PCodeWaitRegistration.TryComplete
                    // (GpmCore.cs) — il task è stato risvegliato ma l'eccezione non è mai stata
                    // rilanciata nel suo contesto, quindi il tick riprendeva silenziosamente
                    // dall'istruzione successiva (ON ERROR non la vedeva mai; l'errore restava
                    // "appeso" fino a fine run, dove usciva come crash non gestito — bug 
                    // GPM-Park-Error-Non-Catturabile-PCode-VMC). La cancellazione cooperativa
                    // (CancellationUnwindStarted + OperationCanceledException) resta esclusa: non
                    // è un errore BASIC catturabile, deve continuare a propagarsi come prima.
                    if (task.PendingException != null &&
                        !(task.CancellationUnwindStarted && task.PendingException is OperationCanceledException))
                    {
                        var pending = task.PendingException;
                        task.PendingException = null;
                        try
                        {
                            RaiseBasicError(task, 200.0, pending.Message);
                        }
                        catch (Exception unhandled)
                        {
                            var crashOpcode = task.Ip >= 0 && task.Ip < _code.Length ? ((OpCode)_code[task.Ip]).ToString() : "???";
                            task.PendingException = new Exception($"IP={task.Ip} op={crashOpcode}: {unhandled.Message}", unhandled);
                            task.State = PCodeTaskState.Finished;
                        }
                        continue;
                    }
                    var ip = task.Ip;
                    if (ip < 0 || ip >= _code.Length) { task.State = PCodeTaskState.Finished; return PCodeTickResult.Completed; }
                    var opcode = (OpCode)_code[ip];
                    task.CurrentInstructionIp = ip;
                    task.Ip++;
                    executedInstructions++;
                    if (DebugServer != null && _debugSourceLines != null && ip < _debugSourceLines.Length)
                        DebugServer.MaybePauseAt(ip, task);
                    if (!DispatchOpcode(task, opcode))
                    {
                        if (opcode == OpCode.OP_YIELD || opcode == OpCode.OP_NEXT_GENERATOR)
                            return PCodeTickResult.Yielded;
                        if (task.IsFinished)
                            return task.PendingException == null ? PCodeTickResult.Completed : PCodeTickResult.Faulted;
                        return PCodeTickResult.Parked;
                    }
                    // TaskYield (Gosched esplicito): il builtin YIELD a 0 argomenti richiede lo
                    // yield a metà tick. Il flag è letto e azzerato QUI, dentro la stessa tick
                    // (nessuna race cross-tick — vedi commento su PCodeTask.YieldRequested);
                    // ritornare true fa sì che HandlePostTick riaccodi il task, come OP_YIELD.
                    if (task.YieldRequested)
                    {
                        task.YieldRequested = false;
                        return PCodeTickResult.Yielded;
                    }
                    // I syscall grafici (SCREEN/COLOR/CLS/LOCATE/PRINT grafico, ecc.) accodano
                    // l'operazione sulla coda del game loop e marcano il task Blocked, ma
                    // ritornano true da DispatchOpcode (a differenza di canali/sleep/future, che
                    // ritornano false) — senza questo controllo il ciclo continuava a eseguire
                    // TUTTI gli opcode del chunk in un colpo solo, quindi es. un PRINT dopo SCREEN
                    // veniva eseguito PRIMA che il game loop avesse processato SCREEN
                    // (IsScreenOn ancora false) e finiva sulla console invece che sulla finestra
                    // grafica (race condition, vedi IL-Graphics-Test-Bugs-Prompt-Claude.md).
                    if (task.State == PCodeTaskState.Blocked)
                        return PCodeTickResult.Parked;
                }
                catch (PCodeStackOverflowException ex)
                {
                    try
                    {
                        RaiseBasicError(task, 200.0, ex.Message);
                    }
                    catch (Exception unhandled)
                    {
                        var crashOpcode = task.CurrentInstructionIp >= 0 && task.CurrentInstructionIp < _code.Length
                            ? ((OpCode)_code[task.CurrentInstructionIp]).ToString()
                            : "???";
                        task.PendingException = new Exception(
                            $"IP={task.CurrentInstructionIp} op={crashOpcode}: {unhandled.Message}",
                            unhandled);
                        task.State = PCodeTaskState.Finished;
                    }
                }
                catch (Exception ex)
                {
                    var crashOpcode = task.Ip >= 0 && task.Ip < _code.Length ? ((OpCode)_code[task.Ip]).ToString() : "???";
                    task.PendingException = new Exception($"IP={task.Ip} op={crashOpcode}: {ex.Message}", ex);
                    task.State = PCodeTaskState.Finished;
                }
            }
            if (task.IsFinished)
                return task.PendingException == null ? PCodeTickResult.Completed : PCodeTickResult.Faulted;
            return PCodeTickResult.Preempted;
        }
        finally
        {
            // Flush unico, sotto un solo lock, di tutti i PRINT accumulati in questo tick — vedi
            // PCodeTask.PendingPrints. try/finally garantisce il flush anche sui percorsi di
            // uscita anticipata (yield, blocco, fine task, eccezione), mai un buffer che resta
            // bloccato in attesa del prossimo tick.
            FlushPrintBuffer(task);
            // Chiude la finestra "Executing" aperta da FindRunnableTask SOLO ORA, dopo il flush:
            // se un canale/waitgroup ha marcato il task Blocked-e-visibile mentre eravamo ancora
            // dentro DispatchOpcode (prima di arrivare qui), un wake concorrente è stato rimandato
            // (WakePending) invece di far ripescare il task a un secondo worker in parallelo con
            // noi — vedi PCodeTask.Executing. Va riaccodato ORA se necessario.
            task.Scheduler?.ReleaseExecuting(task);
        }
    }

    /// <summary>Scrive in un colpo solo (un solo lock) tutti i PRINT bufferati durante il tick —
    /// vedi PCodeTask.PendingPrints per il motivo (atomicità dell'intera istruzione PRINT sotto
    /// vero parallelismo, non della singola scrittura). Se il task è Finished e ha ancora
    /// contenuto in PrintLineStaging (es. lo script termina su una PRINT con `;` finale, che non
    /// emette mai OP_PRINT_LINE/NEWLINE — vedi VisitPrintStatement), lo scrive comunque: altrimenti
    /// resterebbe bufferato per sempre, perso silenziosamente, dato che il task non farà più tick.</summary>
    private void FlushPrintBuffer(PCodeTask task)
    {
        if (task.State == PCodeTaskState.Finished && task.PrintLineStaging is { Length: > 0 } leftover)
        {
            (task.PendingPrints ??= new()).Add((0, leftover.ToString(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
            task.PrintLineStaging = null;
        }

        var pending = task.PendingPrints;
        if (pending == null || pending.Count == 0) return;
        if (PrintSink != null)
        {
            // Macro annidata: riversa l'output nel sink (PendingPrints del task ospite)
            // invece di scrivere subito — l'ordine con i PRINT del chiamante è preservato.
            foreach (var (kind, text, _, _) in pending) PrintSink(kind, text);
            pending.Clear();
            return;
        }
        lock (_consoleLock)
        {
            foreach (var (kind, text, printerOnly, printerRouting) in pending)
            {
                // SET PRINTER / SET DEVICE TO PRINTER (Clipper): lo stato stampante è CATTURATO
                // al momento dell'istruzione PRINT (il flush è ritardato a fine tick — valutarlo
                // qui darebbe lo stato FINALE del tick a tutti i print del tick).
                if (printerRouting)
                    Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterWrite(text ?? "", kind == 1);
                if (printerOnly) continue; // DEVICE=PRINTER: nessun video

                // Finestra attiva (parità backend IL, IlRuntime.PrintDispatch/
                // PrintNewLineDispatch): il testo va al buffer della finestra invece che
                // alla console/pagina base. kind 0 (PRINT senza newline finale, es. "a";)
                // continua sull'ultima riga; kind 1 la chiude aprendone una nuova; kind 2 è
                // un a-capo puro. Prima PRINT dentro una finestra WINDOW SELECT scriveva
                // comunque sulla console base, mai nel buffer della finestra.
                if (ActivePixelWindowPCode() is { } printPw)
                {
                    AppendPrintToLines(printPw.Lines, kind, text);
                    RenderPixelWindowsForPCode();
                    continue;
                }
                if (ActiveWindowObjPCode() is { } printAw)
                {
                    AppendPrintToLines(printAw.Lines, kind, text);
                    RenderTuiWindowsForPCode();
                    continue;
                }

                // PRINT GRAFICO: con la finestra attiva il testo va alla pagina (PrintInternal
                // via coda GPM, main thread) — NON alla console. Senza questo, PRINT dopo SCREEN
                // finiva sempre in console (bug 1 di IL-Graphics-Test-Bugs-Prompt-Claude.md:
                // lo screenshot era tutto nero e il testo compariva solo nel PCOUT).
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    if (kind == 2)
                    {
                        // Newline puro (PRINT vuoto): va a capo nella pagina grafica.
                        PCodeGraphics.Instance.PRINT_GPM("\n", (_r, _e) => { });
                    }
                    else if (!string.IsNullOrEmpty(text))
                        PCodeGraphics.Instance.PRINT_GPM(text + "\n", (_r, _e) => { });
                    continue;
                }

                switch (kind)
                {
                    case 0: _runtime.Console.Print(text); break;
                    case 1: _runtime.Console.PrintLine(text); break;
                    case 2: _runtime.Console.PrintNewLine(); break;
                }
            }
        }
        pending.Clear();
    }

    /// <summary>Applica la semantica di un elemento PendingPrints (kind 0/1/2) a un buffer
    /// di finestra riga per riga, replicando Console.Print/PrintLine/PrintNewLine: kind 0
    /// continua sull'ultima riga (PRINT senza newline finale, es. "a";), kind 1 la chiude e
    /// ne apre una nuova, kind 2 è un a-capo puro.</summary>
    private static void AppendPrintToLines(IList<string> lines, int kind, string? text)
    {
        if (lines.Count == 0) lines.Add("");
        switch (kind)
        {
            case 0:
                lines[^1] += text ?? "";
                break;
            case 1:
                lines[^1] += text ?? "";
                lines.Add("");
                break;
            case 2:
                lines.Add("");
                break;
        }
    }

    [Obsolete("Usa ExecuteTaskTick")]
    public void ExecuteTask(PCodeTask task, int maxInstructions = 100000)
    {
        var instrCount = 0;
        while (!task.IsFinished && instrCount < maxInstructions)
        {
            try
            {
                var ip = task.Ip;
                if (ip < 0 || ip >= _code.Length) break;
                var opcode = (OpCode)_code[ip];
                task.Ip++; // Consuma opcode
                if (!DispatchOpcode(task, opcode)) break;
                instrCount++;
            }
            catch (Exception ex)
            {
                var crashOpcode = task.Ip >= 0 && task.Ip < _code.Length ? ((OpCode)_code[task.Ip]).ToString() : "???";
                task.PendingException = new Exception($"IP={task.Ip} op={crashOpcode}: {ex.Message}", ex);
                task.State = PCodeTaskState.Finished;
            }
        }
        if (instrCount >= maxInstructions)
        {
            // Timeslice scaduto — il task verrà ri-accodato dal scheduler
            // Non impostare Finished, lascia che continui nel prossimo ciclo
        }
    }

    /// <summary>
    /// Chiama la subroutine del predicato VALID (indirizzo P-Code) col frame corrente:
    /// la variabile del campo è GIÀ stata applicata (ApplyAll al READ). Salva/ripristina
    /// Ip e Frame; il bool del predicato resta in cima allo stack e viene consumato qui.
    /// </summary>
    private bool EvaluateValidPredicate(PCodeTask task, int addr)
    {
        var savedIp = task.Ip;
        var savedFrame = task.Frame;
        task.Ip = addr;
        var guard = 0;
        try
        {
            while (guard++ < 4096)
            {
                var ip = task.Ip;
                if (ip < 0 || ip >= _code.Length) return false;
                var opcode = (OpCode)_code[ip];
                task.Ip++;
                if (opcode == OpCode.OP_END_VALID) break;
                if (!DispatchOpcode(task, opcode)) return false;
                if (task.State == PCodeTaskState.Blocked) return false;
            }
            var result = task.Sp > 0 ? task.PopNum() : 0;
            return result != 0.0;
        }
        finally
        {
            task.Ip = savedIp;
            task.Frame = savedFrame;
        }
    }

    /// <summary>Dispatch di un opcode: prima il percorso CALDO (`DispatchHot`, file
    /// PCodeProcessor.Hot.cs, compilato ottimizzato dal JIT), poi il freddo. Vedi la nota in Hot.cs.</summary>
    private bool DispatchOpcode(PCodeTask task, OpCode opcode)
    {
        var r = DispatchHot(task, opcode, out var handled);
        return handled ? r : DispatchOpcodeCold(task, opcode);
    }

    /// <summary>Opcode NON caldi (rari o pesanti). Resta un metodo molto grande: il JIT lo compila
    /// in MinOpts, ed e' accettabile solo perche' qui non passano gli opcode dei cicli.</summary>
    private bool DispatchOpcodeCold(PCodeTask task, OpCode opcode)
    {
        switch (opcode)
        {
            // === Stack Operations ===
            case OpCode.OP_HALT:
                task.State = PCodeTaskState.Finished;
                return false;
            case OpCode.OP_GO_EXIT:
                if (BeginGoExitUnwind(task))
                    return true;
                return false;

            // === GOTO ===
            case OpCode.OP_GOTO:
            {
                var addr = ReadU16(task);
                if (BeginGotoUnwind(task, addr))
                    return true;
                UnwindTryRegionsForJump(task, addr);
                task.Ip = addr;
                return true;
            }
            case OpCode.OP_GOSUB:
            {
                var addr = ReadU16(task);
                task.GosubContinuations.Push(new PCodeGosubContinuation(
                    task.Ip, task.Sp, task.TryStack.Count, task.CleanupStack.Count, task.Frame));
                task.Ip = addr;
                return true;
            }
            case OpCode.OP_CALL_INDIRECT:
            {
                var argCount = ReadU8(task);
                var fnPtr = task.Sp > 0 ? task.PopRef() : null;
                int? addr = null;
                ClosureData? closureData = null;
                if (fnPtr is int ia) addr = ia;
                else if (fnPtr is double da) addr = (int)da;
                // Una ClosureData GREZZA (una lambda semplice assegnata a una variabile,
                // es. `LET Triplica = FN(x) => x*3`, MAI passata da un RETURN — quindi mai
                // "spacchettata" nel suo Dictionary da OP_RETURN come le closure promosse
                // da heap-promotion) non veniva riconosciuta da NESSUN ramo qui: solo i casi
                // "ClosureData dentro un IDictionary" (VTable/__closure__) erano gestiti.
                // addr restava null, si cadeva nel percorso di fallimento in fondo
                // (pop degli argomenti + push di NULL) — la chiamata restituiva sempre NULL.
                else if (fnPtr is ClosureData cdDirect) {
                    addr = cdDirect.ConstIndex >= 0 && cdDirect.ConstIndex < _constants.Count
                        ? (int?)(_constants[cdDirect.ConstIndex] is double d ? (int)d : _constants[cdDirect.ConstIndex] is int i ? i : -1)
                        : null;
                    closureData = cdDirect;
                }
                else if (fnPtr is System.Collections.IDictionary dict && dict.Contains("__class__") && dict["__class__"] is System.Collections.IDictionary cls && cls.Contains("VTable") && cls["VTable"] is System.Collections.IDictionary vt) {
                    foreach (var vk in vt.Keys) {
                        var vv = vt[vk!];
                        if (vv is int iv) { addr = iv; break; }
                        if (vv is double dv) { addr = (int)dv; break; }
                        if (vv is ClosureData cd2) { addr = cd2.ConstIndex >= 0 && cd2.ConstIndex < _constants.Count ? (int?)(_constants[cd2.ConstIndex] is double d ? (int)d : _constants[cd2.ConstIndex] is int i ? i : -1) : null; closureData = cd2; break; }
                    }
                }
                // Check __closure__ in Dict
                if (addr == null && fnPtr is System.Collections.IDictionary dict2 && dict2.Contains("__closure__") && dict2["__closure__"] is ClosureData cdDict) {
                    addr = cdDict.ConstIndex >= 0 && cdDict.ConstIndex < _constants.Count ? (int?)(_constants[cdDict.ConstIndex] is double d ? (int)d : _constants[cdDict.ConstIndex] is int i ? i : -1) : null;
                    closureData = cdDict;
                }
                if (addr.HasValue && addr.Value >= 0 && addr.Value < _code.Length)
                {
                    var locals = CreateLocals();
                    // For ClosureData closures: if first upvalue is a context Dict, set as SELF (slot 0)
                    if (closureData != null) {
                        var upval0 = closureData.GetUpvalue(0);
                        if (upval0 is System.Collections.IDictionary upDict)
                            locals[0] = upDict;
                    }
                    // For Dictionary-wrapped closures: pass the Dict as SELF (slot 0)
                    if (fnPtr is System.Collections.IDictionary dict)
                        locals[0] = dict;
                    // SavedStackPointer deve escludere gli argCount argomenti ancora sullo
                    // stack (li ha pushati il chiamante, non ancora consumati da fnPtr —
                    // solo popato sopra): esattamente come fa OP_CALL_N (savedSp = task.Sp -
                    // argCount). Senza sottrarre argCount, OP_RETURN (che fa
                    // `task.Sp = task.Frame.SavedStackPointer` per ripristinare lo stack)
                    // lascia un valore "fantasma" per ogni argomento — la funzione chiamata
                    // consuma il suo argomento con STORE_LOCAL durante l'esecuzione, ma
                    // OP_RETURN poi RIPRISTINA lo stack a un livello che include ancora quello
                    // spazio, sommandoci sopra il valore di ritorno. Ogni chiamata indiretta
                    // (funzione passata come variabile, lambda) lasciava così una cella
                    // residua sullo stack — invisibile quando il chiamante scarta subito il
                    // risultato (OP_POP consuma per caso la cella fantasma), ma corrompeva la
                    // stack per PRINT successivi nella stessa lista (es.
                    // `PRINT "x -> "; Triplica(10)`, dove il PRINT_LINE finale leggeva la
                    // cella fantasma di una chiamata PRECEDENTE invece del vero risultato).
                    var savedSp = task.Sp - argCount;
                    var newFrame = new PCodeCallFrame { SavedStackPointer = savedSp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = locals, Closure = closureData };
                    EnterProcedureErrorScope(task, newFrame);
                    task.Frame = newFrame;
                    task.Ip = addr.Value;
                    return true;
                }
                for (int i = 0; i < argCount; i++) task.PopRef();
                task.PushRef(null);
                return true;
            }
            case OpCode.OP_CALL_HOST:
            {
                // S_eval §ii: chiama una procedura HOST da macro/EVAL. La tabella + invocatore
                // viaggiano sulla VM annidata (PCodeVm.MacroHostTable/Invoker, in-memory).
                var hostIdx = ReadU16(task);
                var hostArgc = ReadU8(task);
                var hostVm = _vm;
                var hostTable = hostVm?.MacroHostTable;
                var hostInvoker = hostVm?.MacroHostInvoker;
                if (hostTable == null || hostInvoker == null || hostIdx < 0 || hostIdx >= hostTable.Count)
                {
                    task.PendingException = new Exception($"OP_CALL_HOST idx={hostIdx}: nessuna tabella host (macro senza contesto chiamante).");
                    task.State = PCodeTaskState.Finished;
                    return false;
                }
                var entry = hostTable[hostIdx];
                if (task.Sp < hostArgc)
                {
                    task.PendingException = new Exception($"OP_CALL_HOST '{entry.Name}': stack underflow (attesi {hostArgc} argomenti).");
                    task.State = PCodeTaskState.Finished;
                    return false;
                }
                var callArgs = new object?[hostArgc];
                for (var a = hostArgc - 1; a >= 0; a--) callArgs[a] = task.PopRef();
                object? callResult;
                try { callResult = hostInvoker(hostIdx, callArgs); }
                catch (Exception ex)
                {
                    task.PendingException = new Exception($"OP_CALL_HOST '{entry.Name}': {ex.Message}", ex);
                    task.State = PCodeTaskState.Finished;
                    return false;
                }
                task.PushRef(callResult);
                return true;
            }
            case OpCode.OP_CALL_CTOR:
            {
                var addr = ReadU16(task);
                var numArgs = task.Sp > 0 ? task.PopRef() : null;
                var argCount = numArgs is double d ? (int)d : numArgs is int i ? i : 0;
                var fl = CreateLocals();
                if (argCount > 0)
                {
                    var instance = task.Sp > 0 ? task.PopRef() : null;
                    var args = new object?[argCount - 1];
                    for (var a = argCount - 2; a >= 0; a--)
                        args[a] = task.Sp > 0 ? task.PopRef() : null;
                    fl[0] = instance;
                    for (var a = 0; a < args.Length; a++)
                        fl[a + 1] = args[a];
                }
                var ctorFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null, IsDirectCall = true };
                EnterProcedureErrorScope(task, ctorFrame);
                task.Frame = ctorFrame;
                task.Ip = addr;
                return true;
            }
            case OpCode.OP_ON_JUMP:
            {
                var jumpType = ReadU8(task);
                var numTargets = ReadU8(task);
                var targets = new int[numTargets];
                for (var i = 0; i < numTargets; i++)
                    targets[i] = ReadU16(task);
                var selector = PopInt(task);
                if (selector >= 1 && selector <= numTargets)
                {
                    var targetIp = targets[selector - 1];
                    if (jumpType == (byte)JumpType.Gosub)
                    {
                        // ON ... GOSUB è un GOSUB BASIC nella stessa activation frame, non una
                        // CALL di procedura: conserva locals/EH e usa la continuation dedicata.
                        task.GosubContinuations.Push(new PCodeGosubContinuation(
                            task.Ip, task.Sp, task.TryStack.Count, task.CleanupStack.Count, task.Frame));
                        task.Ip = targetIp;
                        return true;
                    }
                    // JumpType.Goto = 0: stessa semantica/unwind di OP_GOTO, inclusi FINALLY
                    // sospendibili. Il vecchio codice invertiva il discriminante e costruiva
                    // erroneamente una call frame per ON GOTO.
                    if (BeginGotoUnwind(task, targetIp))
                        return true;
                    UnwindTryRegionsForJump(task, targetIp);
                    task.Ip = targetIp;
                }
                return true;
            }

            // === PRINT ===
            // I pezzi intermedi di un'istruzione PRINT multi-argomento si accumulano in
            // task.PrintLineStaging (MAI direttamente in PendingPrints) — vedi il commento su
            // PrintLineStaging (GpmCore.cs) per il motivo: valutare un argomento può risospendere
            // il task a meta' istruzione, e PendingPrints e' cio' che il flush di fine-tick rende
            // visibile alle altre goroutine. Solo l'ultimo pezzo (OP_PRINT_LINE) o un
            // OP_PRINT_NEWLINE che trova dello staging pendente committano tutto insieme.
            case OpCode.OP_PRINT_VALUE:
            {
                var val = task.Sp > 0 ? task.PopRef() : null;
                var formatted = FormatPrintValue(val);
                PrintStageEnter(task);   /* PRINT annidato: sospende lo staging esterno */
                (task.PrintLineStaging ??= new()).Append(formatted);
                lock (_consoleLock) _printColumn += formatted.Length;
                return true;
            }
            case OpCode.OP_PRINT_FLUSH:
            {
                /* Fine di una PRINT SENZA newline finale (`PRINT x;` / `PRINT x,`): il
                 * parziale accumulato si COMMETTE come testo (kind 0 = senza a capo), come fa
                 * C99Native chiudendo sempre lo staging a fine istruzione
                 * (gmb_print_stage_end). Senza questo marcatore lo staging restava pendente e
                 * il PRINT di una procedura chiamata DOPO (frame diverso) lo sospendeva →
                 * l'ordine di stampa si invertiva (opt/opt_forward_array: "outer=" in fondo). */
                PrintStageEnter(task);
                var partial = task.PrintLineStaging?.ToString() ?? string.Empty;
                task.PrintLineStaging = null;
                PrintStageLeave(task);   /* riprende un eventuale PRINT esterno sospeso */
                if (partial.Length > 0)
                {
                    if (PCodeGraphics.Instance.IsScreenOn)
                    {
                        PCodeGraphics.Instance.PRINT_GPM(partial, (_r, _e) => { });
                    }
                    else
                    {
                        (task.PendingPrints ??= new()).Add((0, partial,
                            Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(),
                            Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                        FlushPrintBuffer(task);
                    }
                }
                return true;
            }
            case OpCode.OP_PRINT_LINE:
            {
                var val = task.Sp > 0 ? task.PopRef() : null;
                var formatted = FormatPrintValue(val);
                PrintStageEnter(task);   /* PRINT annidato: sospende lo staging esterno */
                string fullLine;
                if (task.PrintLineStaging is { Length: > 0 } staging)
                {
                    staging.Append(formatted);
                    fullLine = staging.ToString();
                    task.PrintLineStaging = null;
                }
                else
                {
                    fullLine = formatted;
                }
                PrintStageLeave(task);   /* riprende il PRINT esterno sospeso */
                // PRINT GRAFICO: accoda SUBITO (al completamento della riga), NON a fine tick:
                // il flush di fine tick arriverebbe DOPO i comandi successivi già accodati nel
                // game loop (es. un COLOR 10 dopo il PRINT rendeva il testo verde — bug
                // verificato con SCREEN+FONT+COLOR 15+PRINT+COLOR 10+CIRCLE).
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    // PRINT in grafica va a capo dopo ogni riga: PrintInternal gestisce i \n
                    // (splitta e avanza _cursorPosition.y di TextHeight). Prima il newline
                    // mancava → i PRINT sequenziali si sovrapponevano sulla stessa riga.
                    if (!string.IsNullOrEmpty(fullLine))
                        PCodeGraphics.Instance.PRINT_GPM(fullLine + "\n", (_r, _e) => { });
                    else
                        PCodeGraphics.Instance.PRINT_GPM("\n", (_r, _e) => { });
                    lock (_consoleLock) _printColumn = 0;
                    return true;
                }
                (task.PendingPrints ??= new()).Add((1, fullLine, Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                // Flush IMMEDIATO a istruzione PRINT completa (non a fine tick): con piu' worker un task
                // svegliato da questo (SEND/DONE/UNLOCK...) puo' stampare PRIMA che il tick del
                // produttore termini, invertendo l'ordine causale delle righe (GoRoutinesMinimal).
                // L'atomicita' della riga resta garantita (commit solo a istruzione completa).
                FlushPrintBuffer(task);
                lock (_consoleLock) _printColumn = 0;
                return true;
            }
            case OpCode.OP_PRINT_NEWLINE:
                if (task.PrintLineStaging is { Length: > 0 } stagedAtNewline)
                {
                    (task.PendingPrints ??= new()).Add((1, stagedAtNewline.ToString(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                    task.PrintLineStaging = null;
                    PrintStageLeave(task);   /* riprende il PRINT esterno sospeso */
                }
                else
                {
                    (task.PendingPrints ??= new()).Add((2, null, Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly(), Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting()));
                }
                FlushPrintBuffer(task);   // vedi OP_PRINT_LINE: ordine causale fra task
                lock (_consoleLock) _printColumn = 0;
                return true;
            case OpCode.OP_PRINT_TAB_ZONE:
                // In GRAFICA il tab-zone (la virgola di PRINT) NON si traduce in spazi:
                // il font TTF è mono ma gli spazi non allineano alle colonne della
                // pagina. Come il Deprecated: si chiude il testo corrente (senza
                // newline) e si sposta il cursore grafico all'inizio della zona di 14
                // caratteri successiva (TAB_ZONE_GPM).
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    if (task.PrintLineStaging is { Length: > 0 } gzStaging)
                    {
                        var gzText = gzStaging.ToString();
                        task.PrintLineStaging = null;
                        PCodeGraphics.Instance.PRINT_GPM(gzText, (_r, _e) => { });
                    }
                    PCodeGraphics.Instance.TAB_ZONE_GPM((_r, _e) => { });
                    lock (_consoleLock) _printColumn = 0;
                    return true;
                }
                lock (_consoleLock)
                {
                    // Colonna PER TASK (lunghezza dello staging di riga del task corrente), mai
                    // il contatore globale del processore: con i dispatch HTTP il task che
                    // stampa "[LOG] ,NOW()" e il task client che stampa "[CLIENT] ..." girano
                    // in parallelo e il contatore condiviso dava padding diverso a ogni run
                    // (righe [LOG] disallineate tra IL e PCode nel sample HttpRoutes — la parità
                    // §4.1 vuole output identico a parità di input: è deterministico solo il
                    // tracciamento per-riga). Il backend IL allinea alla stessa zona di 14
                    // calcolata sulla riga in corso.
                    var col = task.PrintLineStaging?.Length ?? 0;
                    var padding = 14 - (col % 14);
                    if (padding <= 0) padding = 14;
                    (task.PrintLineStaging ??= new()).Append(' ', padding);
                }
                return true;
            case OpCode.OP_YIELD:
                return false; // Cede il controllo allo scheduler

            // === VARIABLES (globals) ===
            case OpCode.OP_DEFINE_GLOBAL:
            {
                var idx = ReadU16(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                task.EnsureGlobals(idx + 1);
                task.Globals![idx] = val;
                return true;
            }
            case OpCode.OP_ADJUST_STRING:
            {
                var len = ReadU16(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                var s = val?.ToString() ?? "";
                if (s.Length > len) s = s.Substring(0, len);
                else if (s.Length < len) s = s.PadRight(len);
                task.PushRef(s);
                return true;
            }
            case OpCode.OP_MAKE_BYREF:
            {
                var slot = ReadU16(task);
                var locals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
                /* Idempotente (specchio di OP_MAKE_BYREF in VM/C): se lo slot contiene già una
                 * ByRefBox (parametro BYREF inoltrato a un'altra procedura, ora anche gli ARRAY
                 * vengono passati per riferimento) si RIUSA quella cella — annidare un box in un
                 * box faceva leggere al callee un box invece dell'array (UBOUND=0, bug 2026-11-03). */
                if (slot < locals.Length && locals[slot] is ByRefBox existingBox)
                    task.PushRef(existingBox);
                else
                    task.PushRef(new ByRefBox(locals, slot));
                return true;
            }
            case OpCode.OP_MAKE_BYREF_GLOBAL:
            {
                var gSlot = ReadU16(task);
                task.EnsureGlobals(gSlot + 1);
                task.PushRef(new ByRefBox(task.Globals!, gSlot));
                return true;
            }
            case OpCode.OP_MAKE_BYREF_ELEMENT:
            {
                // Vedi OP_MAKE_BYREF_ELEMENT in OpCode.cs: `Arr(i)` come argomento BYREF
                // (mono-dimensionale). Stack: [array][indice] (stesso ordine di
                // OP_LOAD_ELEMENT/numIndices=1).
                var idxVal = task.Sp > 0 ? task.PopRef() : null;
                var arr = task.Sp > 0 ? task.PopRef() : null;
                if (arr is RefToVar rvE) arr = rvE.Array[rvE.Index];
                var idx = Convert.ToInt32(idxVal ?? 0.0);
                if (arr is object?[] objArr && idx >= 0 && idx < objArr.Length)
                {
                    task.PushRef(new ByRefBox(objArr, idx));
                    return true;
                }
                // Fallback (array multi-dim rappresentato come Array generico non-object?[],
                // o indice fuori range): comportamento BYVAL come prima di questo fix — push
                // del solo valore, non un riferimento scrivibile.
                if (arr is Array arrGen && idx >= arrGen.GetLowerBound(0) && idx <= arrGen.GetUpperBound(0))
                {
                    task.PushRef(arrGen.GetValue(idx));
                    return true;
                }
                task.PushRef(null);
                return true;
            }
            case OpCode.OP_IS_TYPE:
            {
                var typeCode = ReadU8(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                bool result = typeCode switch
                {
                    0 => val is string,
                    1 => val is int or double or float,
                    2 => val is double or float,
                    3 => val == null,
                    4 => val is System.Collections.IDictionary,
                    5 => val is System.Collections.IList,
                    _ => false
                };
                task.PushRef((double)(result ? -1 : 0));
                return true;
            }

            // === FOR LOOP ===
            case OpCode.OP_NATIVE_STRING_APPEND_LOOP:
            {
                // Fusion nativo: self-append `s$ = s$ + term` in un FOR — replica la semantica
                // della VM/C (gmb_vm_core.c, case OP_NATIVE_STRING_APPEND_LOOP): l'intero corpo
                // gira qui su un StringBuilder, niente dispatch per iterazione. Il risultato è
                // scritto nello slot SOLO alla fine. Formato (8 byte):
                //   [op] [acc_slot u8] [ctr_slot u8] [term_mode u8] [f_code u16] [const_idx u16]
                // term_mode: 0=letterale (const_idx nel pool), 1=builtin num→str sul contatore
                // (f_code: STR=31, CSTR=8). Range [start,end,step] già in ForState (OP_FOR_PREPARE).
                const int nativeStringLoopSliceIterations = 1_024;
                var resumeIp = task.Ip - 1;
                var accSlot = ReadU8(task);
                var ctrSlot = ReadU8(task);
                var termMode = ReadU8(task);
                var fCode = ReadU16(task);
                var constIdx = ReadU16(task);
                if (ForStateOf(task).TryGetValue(ctrSlot, out var nativeStrState))
                {
                    var locals = task.Frame?.LocalVariables;
                    if (locals == null || locals.Length == 0) locals = task.GetLocals();
                    // Seed: valore CORRENTE dello slot (sola lettura).
                    var sb = new System.Text.StringBuilder();
                    if (accSlot < locals.Length && locals[accSlot] is string seed) sb.Append(seed);
                    // Termine letterale pre-risolto (term_mode 0).
                    string? lit = null;
                    if (termMode == 0 && constIdx < _constants.Count && _constants[constIdx] is string cs) lit = cs;
                    var step = nativeStrState.Step;
                    var end = nativeStrState.End;
                    var i = ctrSlot < locals.Length && locals[ctrSlot] is double ctrVal ? ctrVal : nativeStrState.Counter;
                    var iterations = 0;
                    for (; ; )
                    {
                        if (termMode == 1)
                        {
                            // STR (31) / CSTR (8): stesso formato dei rispettivi builtin (non il
                            // generico ToString) — parità col normale esecuzione non fusa.
                            sb.Append(fCode == (int)BuiltInFunction.STR
                                ? StringHelpers.Str(i)
                                : CstrInvariant(i));
                        }
                        else if (lit != null)
                        {
                            sb.Append(lit);
                        }
                        var nxt = i + step;
                        var done = step >= 0 ? nxt > end : nxt < end;
                        i = nxt;
                        iterations++;
                        if (done) break;
                        if (iterations >= nativeStringLoopSliceIterations)
                        {
                            // Quantum esterno non può preemptare un singolo dispatch: salva lo
                            // stato e riesegue lo stesso opcode nella slice successiva.
                            ForStateOf(task)[ctrSlot] = (i, end, step);
                            if (accSlot < locals.Length) locals[accSlot] = sb.ToString();
                            if (ctrSlot < locals.Length) locals[ctrSlot] = i;
                            task.Ip = resumeIp;
                            task.YieldRequested = true;
                            return true;
                        }
                    }
                    ForStateOf(task).Remove(ctrSlot);
                    if (accSlot < locals.Length) locals[accSlot] = sb.ToString();
                    if (ctrSlot < locals.Length) locals[ctrSlot] = i;
                }
                return true;
            }

            // === SYSCALL ===
            case OpCode.OP_SYSCALL:
            {
                // GMBasic: il compilatore EmitSysCall scrive il syscall come U16 big-endian
                // (2 byte: hi, lo) — formato [OP_SYSCALL] [hi] [lo] [argCount] (4 byte).
                // (GMBasicAI usa invece 1 byte — formato diverso, vedi il suo Compiler.Console.)
                var syscallCode = ReadU16(task);
                var argCount = ReadU8(task);
                // Stesso flush di OP_CALL_BUILTIN (ordine output vs scritture dirette).
                FlushPrintBuffer(task);
                ExecuteSyscall(task, syscallCode, argCount);
                return true;
            }

            // === FILE_CMD ===
            case OpCode.OP_FILE_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                ExecuteFileCmd(task, subCode, argCount);
                return true;
            }

            // === ARRAY ELEMENT ACCESS ===
            case OpCode.OP_LOAD_ELEMENT_POSITIONAL:
            {
                // Identico a OP_LOAD_ELEMENT su Array/IList/string (per quei tipi l'indice È già
                // posizionale). Su IDictionary l'indice è invece un CONTATORE di iterazione (0-based,
                // coerente con LBOUND(arr)==0 per i dict — vedi VisitForEachInitStatement), non una
                // chiave: legge la N-esima entry nell'ordine di enumerazione. Unico modo legale per
                // ottenere questo comportamento — mai raggiungibile da un vero indicizzatore `d[k]`
                // dopo il fix di OP_LOAD_ELEMENT sopra. Porting da GMBasicAI (commit 701da5e).
                var numIndices = ReadU8(task);
                // implicit-await inline: stessa logica di OP_LOAD_ELEMENT (base sotto gli indici)
                if (AwaitInlineFutureAt(task, 2, task.Sp - numIndices)) return false;
                var keys = new object?[numIndices];
                for (var i = numIndices - 1; i >= 0; i--)
                    keys[i] = task.Sp > 0 ? task.PopRef() : null;
                var arr = task.Sp > 0 ? task.PopRef() : null;
                if (arr is RefToVar rvLp) arr = rvLp.Array[rvLp.Index];
                // Stage C: matrice a descrittore — due indici → elemento; un indice → riga.
                if (arr is CommonLib.MatrixValue mvP)
                {
                    if (numIndices >= 2)
                        task.PushRef(mvP.Get(Convert.ToInt32(keys[0] ?? 0), Convert.ToInt32(keys[1] ?? 0)));
                    else if (numIndices == 1)
                    {
                        var rowIdxP = Convert.ToInt32(keys[0] ?? 0);
                        var rowP = new double[mvP.Cols];
                        for (var cP = 0; cP < mvP.Cols; cP++) rowP[cP] = mvP.Get(rowIdxP, cP);
                        task.PushRef(rowP);
                    }
                    else task.PushRef(null);
                    return true;
                }
                if (arr is Array arrayP && numIndices > 0)
                {
                    var idx = keys.Select(k => Convert.ToInt32(k ?? 0)).ToArray();
                    try
                    {
                        // Matrice multi-rank con UN solo indice (FOR EACH su matrice): il valore
                        // di iterazione è la RIGA come array 1D (es. FOR EACH row IN m(2,2) →
                        // row = [m(i,0), m(i,1)]). GetValue([i]) lancerebbe su array Rank>1
                        // (richiede un indice per dimensione) — lo slicing esplicito restituisce
                        // la riga. Con N variabili di iterazione il Compiler la destruttura poi
                        // in elementi (FOR EACH a, b IN m → a=m(i,0), b=m(i,1)).
                        if (arrayP.Rank > 1 && numIndices == 1)
                        {
                            var rowIdx = idx[0];
                            var lb = arrayP.GetLowerBound(1);
                            var len2 = arrayP.GetLength(1);
                            var row = Array.CreateInstance(arrayP.GetType().GetElementType()!, len2);
                            for (var c = 0; c < len2; c++)
                                row.SetValue(arrayP.GetValue(rowIdx, c + lb), c);
                            task.PushRef(row);
                        }
                        else
                        {
                            var elem = arrayP.GetValue(idx);
                            task.PushRef(elem);
                        }
                    }
                    catch { task.PushRef(null); }
                }
                else if (arr is System.Collections.IDictionary dictP && numIndices == 1)
                {
                    int idx = Convert.ToInt32(keys[0] ?? 0);
                    if (idx >= 0 && idx < dictP.Count)
                    {
                        var en = dictP.GetEnumerator();
                        for (int e = 0; e <= idx; e++) en.MoveNext();
                        // Convert KeyValuePair<K,V> to DictionaryEntry for LOAD_FIELD compatibility
                        var current = en.Current;
                        if (current is System.Collections.Generic.KeyValuePair<string, object?> kvp)
                            task.PushRef(new System.Collections.DictionaryEntry(kvp.Key, kvp.Value));
                        else if (current is System.Collections.Generic.KeyValuePair<object, object?> kvp2)
                            task.PushRef(new System.Collections.DictionaryEntry(kvp2.Key, kvp2.Value));
                        else
                            task.PushRef(current);
                    }
                    else task.PushRef(null);
                }
                else if (arr is System.Collections.IList listP && numIndices > 0)
                {
                    try { task.PushRef(listP[Convert.ToInt32(keys[0] ?? 0)]); }
                    catch { task.PushRef(null); }
                }
                else if (arr is string strP && numIndices > 0)
                {
                    var idx = Convert.ToInt32(keys[0] ?? 0);
                    try { task.PushRef(idx >= 0 && idx < strP.Length ? strP[idx].ToString() : null); }
                    catch { task.PushRef(null); }
                }
                /* TRIE: la classe CommonLib NON implementa IEnumerable (le parole si leggono
                 * con GetAllWords()): senza questo ramo il FOR EACH stampava vuoto
                 * (CollectionsModern). */
                else if (arr is CommonLib.Collections.Trie trieP && numIndices == 1)
                {
                    var tidxP = Convert.ToInt32(keys[0] ?? 0);
                    var twP = trieP.Words;
                    task.PushRef(tidxP >= 0 && tidxP < twP.Count ? twP[tidxP] : null);
                }
                /* Collezioni enumerabili NON IList/IDictionary (CommonLib e affini):
                 * N-esimo elemento nell'ordine di enumerazione (FOR EACH/comprehension). */
                /* Collezioni chiave→valore CommonLib (RedBlackTree/Cache/BPlusTree) che NON
                 * implementano IEnumerable non-generica: N-esima entry nell'ordine di
                 * Entries() (per RedBlack/BPlus è l'ordine ordinato). Prima il FOR EACH a due
                 * variabili leggeva nil ("  -  ->" su CollectionsModern §3). */
                else if (arr is CommonLib.Collections.IKeyValueIndexable kviPos && numIndices == 1)
                {
                    var kidxP = Convert.ToInt32(keys[0] ?? 0);
                    object? kitemP = null;
                    var kk = 0;
                    foreach (var it in kviPos.Entries()) { if (kk++ == kidxP) { kitemP = it; break; } }
                    task.PushRef(kitemP);
                }
                else if (arr is System.Collections.IEnumerable genEnumP && numIndices == 1)
                {
                    var gidxP = Convert.ToInt32(keys[0] ?? 0);
                    object? gitemP = null;
                    var gkP = 0;
                    foreach (var it in genEnumP) { if (gkP++ == gidxP) { gitemP = it; break; } }
                    task.PushRef(gitemP);
                }
                else
                    task.PushRef(null);
                return true;
            }

            case OpCode.OP_FE_DESTRUCT:
            {
                // Destruttura il valore di iterazione di FOR EACH su MATRICE in N elementi.
                // Stack: [..., valore]. Se valore è un Array (riga di matrice) e N>0, pusha
                // gli elementi (il primo elemento finisce in cima all'ultima push — l'ordine
                // di pop del compiler li legge dal primo al N-esimo). Se NON è un Array
                // (array 1D normale), è l'elemento di iterazione: lo lascia invariato.
                var feN = ReadU8(task);
                var feVal = task.Sp > 0 ? task.PopRef() : null;
                if (feVal is Array feRow && feN > 0)
                {
                    var len2 = Math.Min(feRow.Length, feN);
                    // Ordine: il primo POP = variabile[0] = elemento 0 della riga. Pusha quindi
                    // in ordine INVERSO: prima i mancanti (null, se la riga è più corta di N),
                    // poi gli elementi dal N-1-esimo al 0-esimo (elemento 0 in cima allo stack).
                    // Con N>colonne i mancanti restano null (destructuring parziale, come le
                    // tuple parziali). Con N<colonne si prendono i primi N (utile: riga larga,
                    // uso solo le prime colonne).
                    for (var fi = len2; fi < feN; fi++) task.PushRef(null);
                    for (var fi = len2 - 1; fi >= 0; fi--) task.PushRef(feRow.GetValue(fi));
                }
                else
                {
                    task.PushRef(feVal); // non-matrice: valore singolo invariato
                }
                return true;
            }

            case OpCode.OP_END_VALID:
                // Mai eseguito nel flusso normale (le subroutine VALID sono saltate via indirizzo).
                // Se arriva qui: anomalia — ferma il task.
                task.State = PCodeTaskState.Finished;
                return false;

            // === ERROR HANDLING ===
            case OpCode.OP_SET_ERR_HANDLER_GOTO:
            {
                var addr = ReadU16(task);
                task.ErrorHandlerIp = addr == 0 ? null : addr;
                task.ErrorHandlerIsGosub = false;
                return true;
            }
            case OpCode.OP_SET_ERR_HANDLER_GOSUB:
            {
                var addr = ReadU16(task);
                task.ErrorHandlerIp = addr == 0 ? null : addr;
                task.ErrorHandlerIsGosub = addr != 0;
                return true;
            }
            case OpCode.OP_SET_ERR_HANDLER_RESUME:
            {
                // ON ERROR RESUME NEXT: imposta la modalità resume-next, NESSUN salto.
                task.ErrorMode = PCodeErrorMode.ResumeNext;
                task.ErrorHandlerIp = null;
                task.ErrorHandlerIsGosub = false;
                return true;
            }
            case OpCode.OP_RESUME:
            {
                var mode = ReadU16(task); // 0 = RESUME NEXT, 1 = RESUME, else = label/line
                if (task.ErrorHandlerIp.HasValue)
                {
                    if (mode == 0) // RESUME NEXT
                    {
                        if (task.ErrorHandlerIsGosub && task.IpStack != null && task.IpStack.Count > 0)
                        {
                            task.Ip = task.IpStack.Pop();
                        }
                        else if (task.ErrorResumeNextIp >= 0)
                            task.Ip = task.ErrorResumeNextIp;
                    }
                    else if (mode == 1) // RESUME (retry)
                    {
                        if (task.ErrorHandlerIsGosub && task.IpStack is { Count: > 0 })
                            task.IpStack.Pop();
                        if (task.ErrorLineIp >= 0)
                            task.Ip = task.ErrorLineIp;
                    }
                    else // RESUME <label/linea>
                    {
                        if (task.ErrorHandlerIsGosub && task.IpStack is { Count: > 0 })
                            task.IpStack.Pop();
                        task.Ip = mode;
                    }
                }
                else if (mode == 0)
                {
                    task.ErrorMode = PCodeErrorMode.ResumeNext;
                }
                return true;
            }

            // === CALL_BUILTIN ===
            // === COLLECTIONS ===
            case OpCode.OP_MK_ARRAY:
            {
                var count = ReadU16(task);
                var elements = new object?[count];
                for (var i = count - 1; i >= 0; i--)
                    elements[i] = task.Sp > 0 ? task.PopRef() : null;
                task.PushRef(elements);
                return true;
            }
            case OpCode.OP_MK_DICT:
            {
                var count = ReadU16(task);
                var actualCount = Math.Min(count, task.Sp / 2);
                // Collect all key-value pairs from stack (LIFO) then reverse to preserve order
                var pairs = new (object? key, object? val)[actualCount];
                for (var i = actualCount - 1; i >= 0; i--)
                {
                    var val = task.Sp > 0 ? task.PopRef() : null;
                    var key = task.Sp > 0 ? task.PopRef() : null;
                    pairs[i] = (key, val);
                }
                var dict = new Dictionary<object, object?>();
                foreach (var (key, val) in pairs)
                    dict[key!] = val;
                // DIM x AS REDBLACK/TREE/AVL = { k:v, ... }: sotto le coppie c'è l'istanza
                // pushato da OP_NEW — popola QUELLA invece di creare un dict generico
                // (altrimenti RB = dict, non RedBlackTree → ToArray/Add falliscono).
                if (task.Sp > 0 && task.Stack[task.Sp - 1].Type == 2)
                {
                    var existingRef = task.Stack[task.Sp - 1].Ref;
                    // Solo un'istanza VUOTA (appena creata da OP_NEW per `DIM x AS DICT = {…}`) viene popolata.
                    // Un dict GIÀ popolato in cima allo stack è l'elemento precedente di un letterale
                    // (`[{a:1}, {b:2}]`): fonderci dentro il successivo faceva collassare la lista in
                    // UN solo dict (LEN 0/1, APPENDMANY inseriva un solo record).
                    if (existingRef is System.Collections.IDictionary existing && existing.Count == 0)
                    {
                        foreach (var (key, val) in pairs)
                            existing[key!] = val;
                        return true; // l'istanza è già in cima allo stack
                    }
                    // Collection chiave→valore (RedBlackTree, Cache, BPlusTree): popola via Set.
                    if (existingRef is CommonLib.Collections.IKeyValueIndexable kvi)
                    {
                        foreach (var (key, val) in pairs)
                            kvi.Set(key, val);
                        return true;
                    }
                }
                task.PushRef(dict);
                return true;
            }
            case OpCode.OP_MK_LIST:
            {
                var count = ReadU16(task);
                var list = new List<object?>();
                for (var i = 0; i < count; i++)
                    list.Add(task.Sp > 0 ? task.PopRef() : null);
                list.Reverse();
                task.PushRef(list);
                return true;
            }
            case OpCode.OP_INIT_ARRAY:
            {
                var count = ReadU16(task);
                var elements = new object?[count];
                for (var i = count - 1; i >= 0; i--)
                    elements[i] = task.Sp > 0 ? task.PopRef() : null;
                var arr = task.Sp > 0 ? task.PopRef() : null;
                if (arr is Array a)
                {
                    if (a.Rank > 1)
                    {
                        // Multi-dimensional array: map flat index to indices
                        var dims = new int[a.Rank];
                        var lob = new int[a.Rank];
                        for (int di = 0; di < a.Rank; di++) { dims[di] = a.GetLength(di); lob[di] = a.GetLowerBound(di); }
                        for (var j = 0; j < elements.Length && j < a.Length; j++)
                        {
                            var idx = new int[a.Rank];
                            var rem = j;
                            for (int di = a.Rank - 1; di >= 0; di--) {
                                idx[di] = lob[di] + rem % dims[di];
                                rem /= dims[di];
                            }
                            a.SetValue(elements[j], idx);
                        }
                        for (var j = elements.Length; j < a.Length; j++)
                        {
                            var idx = new int[a.Rank];
                            var rem = j;
                            for (int di = a.Rank - 1; di >= 0; di--) {
                                idx[di] = lob[di] + rem % dims[di];
                                rem /= dims[di];
                            }
                            a.SetValue(0.0, idx);
                        }
                    }
                    else
                    {
                        var lob0 = a.GetLowerBound(0);
                        for (var j = 0; j < elements.Length && j < a.Length; j++)
                            a.SetValue(elements[j], lob0 + j);
                        for (var j = elements.Length; j < a.Length; j++)
                            a.SetValue(0.0, lob0 + j);
                    }
                }
                else if (arr is System.Collections.Generic.HashSet<object?> hs)
                {
                    foreach (var elem in elements)
                        if (elem != null) hs.Add(elem);
                }
                else if (arr is System.Collections.IList il2 && !(arr is Array))
                {
                    foreach (var elem in elements)
                        il2.Add(elem);
                }
                else if (arr is System.Collections.Generic.Stack<object?> stk)
                {
                    foreach (var elem in elements)
                        if (elem != null) stk.Push(elem);
                }
                else if (arr is System.Collections.Generic.Queue<object?> qu)
                {
                    foreach (var elem in elements)
                        if (elem != null) qu.Enqueue(elem);
                }
                /* LINKEDLIST/DOUBLYLINKEDLIST (OP_NEW typeCode 12 → LinkedList<object?>):
                 * nessun ramo sopra la copriva (non è IList) → il letterale
                 * `DIM L AS LINKEDLIST<STRING> = ["B"]` restava VUOTO (CollectionsLinked:
                 * "SLL.Count= 0" finché non si aggiungeva a mano). */
                else if (arr is System.Collections.Generic.LinkedList<object?> llInit)
                {
                    foreach (var elem in elements)
                        llInit.AddLast(elem);
                }
                // BinaryIndexedTree: il costruttore prende il source ARRAY (non c'è Add) —
                // ricreata con i dati dell'inizializzatore ([1,2,3,4,5] → source).
                else if (arr is CommonLib.Collections.BinaryIndexedTree<double>)
                {
                    // Ricreata con il source (il costruttore prende l'array — niente Add).
                    var srcArr = elements.Select(e => SafeDouble(e)).ToArray();
                    arr = new CommonLib.Collections.BinaryIndexedTree<double>(srcArr);
                }
                else if (arr is CommonLib.Collections.BinaryHeap<object> bh)
                {
                    foreach (var elem in elements)
                        if (elem != null) bh.Add(elem);
                }
                // TRIE: Add ogni parola (il costruttore non prende dati).
                else if (arr is CommonLib.Collections.Trie trieInit)
                {
                    foreach (var elem in elements)
                        if (elem != null) trieInit.Add(elem.ToString() ?? "");
                }
                // BAG (Dictionary<object,long>): ogni elemento incrementa il conteggio.
                else if (arr is System.Collections.IDictionary bagInit)
                {
                    foreach (var elem in elements)
                    {
                        if (elem == null) continue;
                        var cur = bagInit.Contains(elem) && bagInit[elem] is long bl ? bl : 0L;
                        bagInit[elem] = cur + 1;
                    }
                }
                task.PushRef(arr);
                return true;
            }


            case OpCode.OP_LOAD_DYNAMIC_SCOPE:
            {
                var dsNameIdx = ReadU16(task);
                var dsName = dsNameIdx >= 0 && dsNameIdx < _constants.Count ? _constants[dsNameIdx]?.ToString() : null;
                task.PushRef(dsName != null ? GetDynamicScope(dsName) : null);
                return true;
            }

            // === SCOPE DINAMICO (DYNAMIC) — la catena dei frame di chiamata È la catena dello
            // scope dinamico (spec Variables-Scope §2.5). Vedi OpCode.OP_DYN_*. Una procedura
            // che non dichiara DYNAMIC ha un frame vuoto: la lettura risale la catena, la
            // scrittura colpisce il frame più interno che definisce il nome. Alla pop del frame
            // le dichiarazioni spariscono (ripristino automatico), quindi nessun push/pop.
            case OpCode.OP_DYN_DECLARE:
            {
                var dnIdx = ReadU16(task);
                var dnName = dnIdx >= 0 && dnIdx < _constants.Count ? _constants[dnIdx]?.ToString() : null;
                var dnVal = task.Sp > 0 ? task.PopRef() : null;
                if (!string.IsNullOrEmpty(dnName))
                {
                    System.Collections.Generic.Dictionary<string, object?> bag;
                    if (task.Frame is null) bag = task.BaseDynamicVars;
                    else
                    {
                        task.Frame.DynamicVars ??= new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                        bag = task.Frame.DynamicVars;
                    }
                    bag[dnName!] = dnVal;
                }
                return true;
            }
            case OpCode.OP_DYN_LOAD:
            {
                var dnIdx = ReadU16(task);
                var dnName = dnIdx >= 0 && dnIdx < _constants.Count ? _constants[dnIdx]?.ToString() : null;
                var dnKind = ReadU8(task);
                object? dnFound = null;
                var dnHas = false;
                if (!string.IsNullOrEmpty(dnName))
                {
                    for (var fr = task.Frame; fr is not null && !dnHas; fr = fr.Previous)
                        if (fr.DynamicVars is { } dv && dv.TryGetValue(dnName!, out dnFound)) dnHas = true;
                    if (!dnHas && task.BaseDynamicVars.TryGetValue(dnName!, out dnFound)) dnHas = true;
                }
                // Assente → default paritetico a gmb_dscope_get_string/get_double ("" / 0.0).
                if (!dnHas) dnFound = dnKind == 1 ? string.Empty : (object?)0.0;
                task.PushRef(dnFound);
                return true;
            }
            case OpCode.OP_DYN_STORE:
            {
                var dnIdx = ReadU16(task);
                var dnName = dnIdx >= 0 && dnIdx < _constants.Count ? _constants[dnIdx]?.ToString() : null;
                var dnVal = task.Sp > 0 ? task.PopRef() : null;
                if (!string.IsNullOrEmpty(dnName))
                {
                    System.Collections.Generic.Dictionary<string, object?>? target = null;
                    for (var fr = task.Frame; fr is not null; fr = fr.Previous)
                        if (fr.DynamicVars is { } dv && dv.ContainsKey(dnName!)) { target = dv; break; }
                    if (target is null && task.BaseDynamicVars.ContainsKey(dnName!)) target = task.BaseDynamicVars;
                    if (target is null)
                    {
                        if (task.Frame is null) target = task.BaseDynamicVars;
                        else
                        {
                            task.Frame.DynamicVars ??= new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                            target = task.Frame.DynamicVars;
                        }
                    }
                    target[dnName!] = dnVal;
                }
                return true;
            }

            // === UDT FIELDS ===
            case OpCode.OP_LOAD_FIELD:
            {
                var fieldNameIdx = ReadU16(task);
                var fieldName = fieldNameIdx >= 0 && fieldNameIdx < _constants.Count ? _constants[fieldNameIdx]?.ToString() : null;
                // implicit-await inline (FETCH(url).Campo): l'istanza in cima può essere un future
                if (AwaitInlineFuture(task, 3)) return false;
                var obj = task.Sp > 0 ? task.PopRef() : null;
                if (obj is Basic.RegexNative.RegexWrapper _reGet)
                {
                    task.PushRef(fieldName == "Pattern" ? _reGet.Pattern : (object?)null);
                    return true;
                }
                if (obj is Basic.RegexNative.RegexMatchValue _mGet)
                {
                    task.PushRef(fieldName switch
                    {
                        "Value" => _mGet.Value,
                        "Index" => (double)_mGet.Index,
                        "Length" => (double)_mGet.Length,
                        "Success" => _mGet.Success,
                        _ => (object?)null
                    });
                    return true;
                }
                if (obj is System.Collections.IList _listGet && !(obj is System.Text.StringBuilder))
                {
                    if (fieldName == "Count") { task.PushRef((double)_listGet.Count); return true; }
                }
                /* Count come PROPRIETÀ su collezioni generiche (HASHSET<T>, QUEUE<T>,
                 * STACK<T>): implementano ICollection<T> ma NON la ICollection non-generica,
                 * quindi i rami sopra (IList/ICollection) non scattano → `hs.Count` dava nil
                 * mentre `hs.Count()` funzionava (Collections/CollectionsFluent). */
                if (obj is CommonLib.Collections.HyperLogLog hllGet
                    && (fieldName == "Count" || fieldName == "Length"))
                {
                    task.PushRef(hllGet.Count());
                    return true;
                }
                if (fieldName == "Count" && obj is not null && obj is not string)
                {
                    var cp = obj.GetType().GetProperty("Count",
                        System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                    if (cp is not null && cp.PropertyType == typeof(int) && cp.GetIndexParameters().Length == 0)
                    {
                        task.PushRef((double)(int)cp.GetValue(obj)!); return true;
                    }
                }
                if (obj is System.Text.StringBuilder sbField)
                {
                    task.PushRef(fieldName switch
                    {
                        "Length" => (double)sbField.Length,
                        "Capacity" => (double)sbField.Capacity,
                        _ => null
                    });
                    return true;
                }
                if (obj is Basic.IlBackend.PCode.Http.PCodeHttpResponse httpResGet)
                {
                    if (fieldName == "StatusCode") task.PushRef((double)httpResGet.StatusCode);
                    else if (fieldName == "ContentType") task.PushRef(httpResGet.Headers.TryGetValue("Content-Type", out var ctGet) ? ctGet : "");
                    else task.PushRef(null);
                    return true;
                }
                if (obj is Basic.IlBackend.PCode.Http.PCodeWebSocketHandle)
                {
                    task.PushRef(null); // sola scrittura (nomi SUB), nessun sample legge queste proprieta'
                    return true;
                }
                if (obj is Basic.IlBackend.PCode.Http.PCodeFetchResult fetchResGet)
                {
                    task.PushRef(fieldName switch
                    {
                        "StatusCode" => (double)fetchResGet.StatusCode,
                        "Body" => fetchResGet.Body,
                        "Headers" => fetchResGet.HeadersAsDict(),
                        "ContentLength" => (double)fetchResGet.BodyBytes.Length,
                        "Url" => fetchResGet.Url,
                        "IsSuccess" => fetchResGet.IsSuccess,
                        // Mancava — PCodeFetchResult.ErrorMessage esiste da sempre ma non era mai
                        // esposto qui, quindi res.ErrorMessage su un FETCHRESULT restituiva
                        // sempre null anche a richiesta fallita (bug scoperto verificando FETCH
                        // avanzato Fase 3, certificate pinning con impronta errata).
                        "ErrorMessage" => fetchResGet.ErrorMessage,
                        _ => null
                    });
                    return true;
                }
                if (obj is System.Collections.IDictionary dict)
                {
                    // Special fields: Count, Keys, Values
                    if (fieldName == "Count") { task.PushRef((double)dict.Count); return true; }
                    /* Keys/Values: LISTA (parità IL/VM/C/C99Native). Prima si pushava la
                     * KeyCollection/ValueCollection raw → PRINT stampava il NOME DEL TIPO CLR
                     * ("System.Collections.Generic.Dictionary`2+KeyCollection[...]") invece di
                     * "[uno, due]" (sample Collections/CollectionsFluent e Minitest). */
                    if (fieldName == "Keys")
                    {
                        var kl = new System.Collections.Generic.List<object?>();
                        foreach (var kk in dict.Keys) kl.Add(kk);
                        task.PushRef(kl); return true;
                    }
                    if (fieldName == "Values")
                    {
                        var vl = new System.Collections.Generic.List<object?>();
                        foreach (var vv in dict.Values) vl.Add(vv);
                        task.PushRef(vl); return true;
                    }
                    var val = dict.Contains(fieldName ?? "") ? dict[fieldName ?? ""] : null;
                    if (val == null && dict.Contains("__class__") && dict["__class__"] is System.Collections.IDictionary cd &&
                        cd.Contains("PropertyGetters") && cd["PropertyGetters"] is System.Collections.IDictionary pgs)
                    {
                        var ga = pgs[fieldName ?? ""];
                        if (ga is int getterAddr)
                        {
                            var fl = CreateLocals();
                            fl[0] = obj;
                            var getterFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null };
                            EnterProcedureErrorScope(task, getterFrame);
                            task.Frame = getterFrame;
                            task.Ip = getterAddr;
                            return true;
                        }
                    }
                    task.PushRef(val);
                }
                else if (obj is System.Collections.IList list && fieldName == "Count")
                    task.PushRef((double)list.Count);
                else if (obj is System.Collections.ICollection col && fieldName == "Count")
                    task.PushRef((double)col.Count);
                // Collection methods accessed as properties: Peek, Dequeue, Pop
                else if (obj is System.Collections.Generic.Queue<object?> qq)
                {
                    if (fieldName == "Count") task.PushRef((double)qq.Count);
                    else if (fieldName == "Peek") { try { task.PushRef(qq.Peek()); } catch { task.PushRef(null); } }
                    else if (fieldName == "Dequeue") { try { task.PushRef(qq.Dequeue()); } catch { task.PushRef(null); } }
                    else task.PushRef(null);
                    return true;
                }
                else if (obj is System.Collections.Generic.Stack<object?> st)
                {
                    if (fieldName == "Count") task.PushRef((double)st.Count);
                    else if (fieldName == "Peek") { try { task.PushRef(st.Peek()); } catch { task.PushRef(null); } }
                    else if (fieldName == "Pop") { try { task.PushRef(st.Pop()); } catch { task.PushRef(null); } }
                    else task.PushRef(null);
                    return true;
                }
                else if (obj is System.Collections.DictionaryEntry de)
                {
                    if (fieldName == "Key") task.PushRef(de.Key);
                    else if (fieldName == "Value") task.PushRef(de.Value);
                    else task.PushRef(null);
                }
                /* Elemento di FOR EACH su RedBlackTree/BPlusTree/... = KeyValuePair<,> boxed:
                 * senza questo ramo `FOR EACH k, v IN rb` assegnava nil a entrambe le
                 * variabili ("  -  ->" su CollectionsModern §3). */
                else if (obj is not null && (fieldName == "Key" || fieldName == "Value")
                         && obj.GetType().IsGenericType
                         && obj.GetType().GetGenericTypeDefinition() == typeof(System.Collections.Generic.KeyValuePair<,>))
                {
                    task.PushRef(obj.GetType().GetProperty(fieldName!)?.GetValue(obj));
                }
                // Accesso campo su una STRINGA JSON (es. risultato di JSON.ToJson/ToObject su un
                // UDT, che il PCode serializza in testo): parsa e leggi il campo — parità del
                // backend IL (IlRuntime.JsonFieldGet gestisce il caso string).
                else if (obj is string jsonStrField && !string.IsNullOrEmpty(fieldName))
                {
                    task.PushRef(PCodeJsonStringField(jsonStrField, fieldName));
                }
                else if (obj == null && TryReadDbfRecordField(task, fieldName, out var dbfVal))
                {
                    // Campo DBF bare (`PRINT NAME` con area aperta — parità IL DbfLateBindField):
                    // il compilatore emette NIL + LOAD_FIELD. Priorità campo stile Clipper;
                    // un vero nil-deref con campo omonimo aperto legge il record (documentato).
                    task.PushRef(dbfVal);
                }
                else task.PushRef(null);
                return true;
            }
            case OpCode.OP_STORE_FIELD:
            {
                var fieldNameIdx = ReadU16(task);
                var fieldName = fieldNameIdx < _constants.Count ? _constants[fieldNameIdx]?.ToString() : null;
                var obj = task.Sp > 0 ? task.PopRef() : null;
                var val = task.Sp > 0 ? task.PopRef() : null;
                if (obj is Basic.RegexNative.RegexWrapper _reSet && fieldName == "Pattern")
                {
                    _reSet.Pattern = val?.ToString() ?? "";
                    return true;
                }
                if (obj is Basic.IlBackend.PCode.Http.PCodeHttpResponse httpResSet)
                {
                    if (fieldName == "StatusCode") httpResSet.SetStatus(Convert.ToInt32(val ?? 200.0));
                    else if (fieldName == "ContentType") httpResSet.SetHeader("Content-Type", val?.ToString() ?? "");
                    return true;
                }
                if (obj is Basic.IlBackend.PCode.Http.PCodeWebSocketHandle wsSet)
                {
                    // Sintassi legacy: il nome della SUB come stringa, risolto a indirizzo Ip
                    // tramite la tabella copiata dal compilatore (vedi CompiledChunk.ProcedureAddresses).
                    var name = val?.ToString();
                    var addr = name != null && _procedureAddresses.TryGetValue(name, out var a) ? (int?)a : null;
                    if (fieldName == "OnMessage") wsSet.OnMessageIp = addr;
                    else if (fieldName == "OnClose") wsSet.OnCloseIp = addr;
                    else if (fieldName == "OnError") wsSet.OnErrorIp = addr;
                    return true;
                }
                if (obj is System.Collections.IDictionary dict)
                {
                    // Try calling property setter if available
                    try
                    {
                        if (dict["__class__"] is System.Collections.IDictionary cd &&
                            cd["PropertySetters"] is System.Collections.IDictionary pss &&
                            pss[fieldName ?? ""] is int setterAddr)
                        {
                            var fl = CreateLocals();
                            fl[0] = obj; fl[1] = val;
                            var setterFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null };
                            EnterProcedureErrorScope(task, setterFrame);
                            task.Frame = setterFrame;
                            task.Ip = setterAddr;
                            return true;
                        }
                    }
                    catch { /* fall through to direct store */ }
                    dict[fieldName ?? ""] = val;
                }
                return true;
            }

            static int ToInt(object? o, int fallback)
            {
                if (o is double d) return (int)d;
                if (o is int i) return i;
                if (o is long l) return (int)l;
                if (o is System.IConvertible cv) { try { return cv.ToInt32(System.Globalization.CultureInfo.InvariantCulture); } catch { } }
                return fallback;
            }

            case OpCode.OP_NEW:
            {
                var typeCode = ReadU16(task);
                // Gli argomenti del costruttore (se presenti) sono sullo stack come
                // [args...][count] PRIMA di OP_NEW (pattern identico alle classi,
                // vedi Compiler.Expressions.cs VisitNewExpression). Se count>0 li
                // applichiamo ai costruttori dei tipi built-in (RingBuffer/SparseSet/
                // HashRing/BPlusTree) — prima venivano SILENZIOSAMENTE SCARTATi: il
                // bytecode conteneva solo OP_NEW typeCode e la struttura nasceva coi
                // default (es. CircularQueue(64) invece di capacity=3).
                var ctorCount = 0;
                var ctorArgs = Array.Empty<object?>();
                if (task.Sp > 0)
                {
                    var cntObj = task.PopRef();
                    ctorCount = cntObj is double cd ? (int)cd : cntObj is int ci ? ci : 0;
                    if (ctorCount < 0) ctorCount = 0;
                    ctorArgs = new object?[ctorCount];
                    for (var a = ctorCount - 1; a >= 0; a--)
                        ctorArgs[a] = task.Sp > 0 ? task.PopRef() : null;
                }
                // typeCode: 1=List, 2=Dict, 3=HashSet, 4=Queue, 5=Stack, etc.
                object? newObj = typeCode switch
                {
                    1 => new System.Collections.Generic.List<object?>(),
                    // DICT<K,V> con K/V espliciti (es. `NEW DICT<LONG, JSON>`) finiva SEMPRE qui
                    // (DictType => 2, vedi Compiler.Expressions.cs VisitNewExpression) con un
                    // Dictionary<string,object?> — scartando SILENZIOSAMENTE il tipo di chiave
                    // dichiarato: una chiave non-stringa (es. un LONG) faceva esplodere l'indicizzatore
                    // con ArgumentException "not of type System.String" al primo uso reale. OP_NEW_DICT
                    // (il costrutto NEW DICT non tipizzato, poco sotto in questo stesso file) usa
                    // GIA' correttamente Dictionary<object,object?> — allineato qui per coerenza:
                    // stessa struttura dati generica, chiave di QUALUNQUE tipo, dato che ogni accesso
                    // nel resto della VM passa comunque dall'interfaccia non generica IDictionary.
                    // Porting da GMBasicAI (commit 7736047).
                    2 => new Dictionary<object, object?>(),
                    3 => new System.Collections.Generic.HashSet<object?>(),
                    4 => new System.Collections.Generic.Queue<object?>(),
                    5 => new System.Collections.Generic.Stack<object?>(),
                    12 => new System.Collections.Generic.LinkedList<object?>(),
                    // 28 = STRINGBUILDER (DIM AS STRINGBUILDER / NEW STRINGBUILDER([cap])).
                    // Rappresentato con System.Text.StringBuilder (parità IL); i metodi
                    // sono dispatchati in OP_CALL_MEMBER.
                    28 => new System.Text.StringBuilder(ctorCount > 0 && ctorArgs[0] is not null
                              ? (int)Convert.ToDouble(ctorArgs[0]) : 16),
                    // 29 = REGEX (DIM AS REGEX): wrapper runtime (parità IL IlRuntime/RegexWrapper).
                    // Sensa, la variabile restava null e TYPENAME(direcast(reg,REGEX)) dava NIL
                    // invece di REGEXWRAPPER (divergenza su TypesDirectCast.bas §3).
                    29 => new Basic.RegexNative.RegexWrapper(),
                    // 30 = HLL (HyperLogLog): stima di cardinalità (CommonLib, mirror del
                    // runtime C gmb_hll.c). Member dispatch: fallback reflection + CollectionInvoke.
                    30 => new CommonLib.Collections.HyperLogLog(),
                    // NEW HTTPOPTIONS: rappresentato come Dictionary<string,object?> (stesso
                    // modello "duck-typed" gia' usato per UDT/istanze di classe — vedi
                    // OP_NEW_UDT_INSTANCE), popolato con gli stessi default di
                    // CommonLib.Http.HttpOptions cosi' OP_LOAD_FIELD/OP_STORE_FIELD (gia' generici
                    // su IDictionary) leggono/scrivono opts.Method/.Body/.Timeout/.UseCookies senza
                    // bisogno di altro codice; Headers e' un dict annidato con le stesse chiavi
                    // tipizzate esposte da CommonLib.Http.HttpHeaders (letto da OP_FETCH, vedi
                    // ConvertPCodeHttpOptions).
                    22 => new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
                    {
                        ["Method"] = "GET",
                        ["Headers"] = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase),
                        ["Body"] = null,
                        ["Timeout"] = 30000.0,
                        ["UseCookies"] = true,
                        ["Username"] = null,
                        ["Password"] = null,
                        ["BearerToken"] = null,
                        ["ProxyUrl"] = null,
                        ["ProxyUsername"] = null,
                        ["ProxyPassword"] = null,
                        ["ClientCertificatePath"] = null,
                        ["ClientCertificatePassword"] = null,
                        ["AllowRedirects"] = true,
                        ["MaxRedirects"] = 10.0,
                        ["AllowInsecureSSL"] = false,
                        ["BrowserPreset"] = null,
                        ["PinnedCertificateSha256"] = null,
                    },
                    // NEW HTTPCONFIG: default seminati dall'UNICA sorgente managed
                    // (NewHttpConfigDefaults) condivisa con la gamba IL. ExecuteHttpConfig
                    // (che legge cfgDict con GetNum/GetStr generici) vede i valori scritti da
                    // BASIC (es. cfg.Port = 8093) invece di ricadere sempre sui default.
                    23 => NewHttpConfigDefaults(),
                    // NEW JSON: oggetto vuoto duck-typed, stesso pattern di typeCode 22/23 — i
                    // campi assegnati dinamicamente (dati.server = "x") passano gia' per
                    // OP_STORE_FIELD generico su IDictionary, nessun altro codice necessario.
                    24 => new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase),
                    // NEW COOKIEJAR: oggetto C# reale (non duck-typed — nessun campo, solo il
                    // metodo Clear(), vedi OP_CALL_MEMBER sotto), cosi' l'istanza puo' essere
                    // passata direttamente a ConvertPCodeHttpOptions -> HttpOptions.CookieJar.
                    25 => new CommonLib.Http.CookieJar(),
                    // Collezioni avanzate (CommonLib.Collections) — prima mancavano in
                    // OP_NEW: DIM x AS GRAPH<...> = NEW GRAPH<...> creava null → tutti i
                    // metodi erano no-op silenziosi (NodeCount=0, HasNode vuoto, ...).
                    // BUG sistemico: Tree/AVL/RedBlack/BIT/Trie/Heap/Bag/Cache/Graph/
                    // Timeline/HashRing/RingBuffer/SparseSet/BPlusTree.
                    // Comparer NUMERICO-robusto: il comparer di default per object esplode
                    // quando le chiavi boxed hanno tipi diversi (es. 10.0 double vs 10 int —
                    // "Object must be of type Double"). Le chiavi BASIC INTEGER/LONG arrivano
                    // incoerentemente come int o double; questo comparer normalizza i numeri
                    // e confronta il resto come stringa. Senza, AVLTREE<INTEGER,...>.Add al
                    // secondo inserimento lanciava (comparer tipi misti).
                    6 => new System.Collections.Generic.SortedDictionary<object, object?>(CompareMixedKeys()),
                    7 => new System.Collections.Generic.SortedDictionary<object, object?>(CompareMixedKeys()),
                    8 => new System.Collections.Generic.SortedDictionary<object, object?>(CompareMixedKeys()),
                    9 => new CommonLib.Collections.RedBlackTree<string, object?>(),
                    10 => new CommonLib.Collections.BinaryIndexedTree<double>(Array.Empty<double>()),
                    11 => new CommonLib.Collections.Trie(),
                    // CIRCULAR (CircularLinkedList, type code 13): il codegen IL lo materializza
                    // come List<> (DataTypeMapper: CircularLinkedListType → List<T>) e fornisce
                    // Current/Rotate come helper sul List. Prima mancava → il var restava null
                    // (TYPENAME "NIL", VALTYPE "U").
                    13 => new System.Collections.Generic.List<object?>(),
                    14 => new CommonLib.Collections.BinaryHeap<object>(Comparer<object>.Create((a, b) =>
                        Comparer<double>.Default.Compare(SafeDouble(a), SafeDouble(b)))),
                    27 => new CommonLib.Collections.BinaryHeap<object>(Comparer<object>.Create((a, b) =>
                        Comparer<double>.Default.Compare(SafeDouble(b), SafeDouble(a)))),
                    15 => new System.Collections.Generic.Dictionary<object, long>(),
                    16 => new CommonLib.Collections.Cache<object, object?>(),
                    17 => new CommonLib.Collections.Graph<object>(),
                    18 => new CommonLib.Collections.Timeline<object>(),
                    // 100 repliche = default di CommonLib (e valore usato da IL/VM-C):
                    // con 1 il GetNode dava un nodo diverso (CollectionsModern §11).
                    19 => ctorCount >= 1 && ctorArgs[0] is System.Collections.IEnumerable nodeList
                        ? new CommonLib.Collections.ConsistentHash<object>(nodeList.Cast<object>(), 100)
                        : new CommonLib.Collections.ConsistentHash<object>(Array.Empty<object>(), 100),
                    20 => ctorCount >= 1 ? new CommonLib.Collections.CircularQueue<object>(ToInt(ctorArgs[0], 64))
                        : new CommonLib.Collections.CircularQueue<object>(64),
                    21 => ctorCount >= 2
                        ? new CommonLib.Collections.SparseSet(ToInt(ctorArgs[0], 1_000_000), ToInt(ctorArgs[1], 64))
                        : new CommonLib.Collections.SparseSet(1_000_000, 64),
                    26 => ctorCount >= 1 ? new CommonLib.Collections.BPlusTree<string, object?>(ToInt(ctorArgs[0], 3))
                        : new CommonLib.Collections.BPlusTree<string, object?>(3),
                    _ => null
                };
                task.PushRef(newObj);
                return true;
            }
            case OpCode.OP_ATTACH_UDT_DESC:
            {
                // Allega il descrittore UDT del tipo TARGET all'istanza dict in cima
                // (JSON.TOOBJECT con target UDT): i FieldNames del descrittore della
                // dichiarazione usano gli symbol id del sito d'uso (il descrittore
                // serializzato nel JSON può averne altri).
                var attachIdx = ReadU16(task);
                var attachDesc = attachIdx < _constants.Count ? _constants[attachIdx] as System.Collections.IDictionary : null;
                var attachVal = task.Sp > 0 ? task.PopRef() : null;
                if (attachDesc != null && attachVal is System.Collections.IDictionary attachDict)
                {
                    attachDict["__class__"] = attachDesc;
                    // Campi LIST OF <ElemUDT> popolati da JSON.TOOBJECT: gli elementi sono dict
                    // SENZA __class__ → aggancia il descriptor dell'elemento (FieldArrayElemDesc)
                    // così `u.legs(i).campo` risolve per nome (parità VM-C/C99).
                    if (attachDesc["FieldArrayElemDesc"] is System.Collections.IDictionary faed
                        && attachDesc["FieldNames"] is System.Collections.IDictionary fnames)
                    {
                        foreach (System.Collections.DictionaryEntry fe in faed)
                        {
                            if (fe.Key == null || fe.Value == null) continue;
                            // FieldArrayElemDesc/FieldNames sono chiavati per SymbolId (int).
                            var fname = fnames.Contains(fe.Key) ? fnames[fe.Key]?.ToString() : null;
                            if (fname == null || !attachDict.Contains(fname)) continue;
                            if (attachDict[fname] is System.Collections.IList lst)
                                foreach (var el in lst)
                                    if (el is System.Collections.IDictionary eld) eld["__class__"] = fe.Value;
                        }
                    }
                }
                task.PushRef(attachVal);
                return true;
            }
            case OpCode.OP_NEW_UDT_INSTANCE:
            case OpCode.OP_NEW_CLASS_INSTANCE:
            {
                var typeIdx = ReadU16(task);
                var descriptor = typeIdx < _constants.Count ? _constants[typeIdx] as System.Collections.IDictionary : null;
                var instance = new Dictionary<string, object?>();
                if (descriptor != null)
                {
                    instance["__class__"] = descriptor;
                    // Init UDT fields with defaults — tipizzato (0.0 per numerici/date, ""
                    // per stringhe) via FieldTypes quando disponibile, altrimenti "" come
                    // prima (fallback per un tipo di campo non ancora coperto qui, es. UDT
                    // annidato/array — vedi DefaultValueForFieldType).
                    var fieldTypes = descriptor.Contains("FieldTypes") ? descriptor["FieldTypes"] as System.Collections.IDictionary : null;
                    var fieldArrDims = descriptor.Contains("FieldArrayDimensions") ? descriptor["FieldArrayDimensions"] as System.Collections.IDictionary : null;
                    if (descriptor["FieldNames"] is System.Collections.IDictionary fnDict)
                    {
                        foreach (var k in fnDict.Keys)
                            if (fnDict[k] is string fn)
                            {
                                var fDt = fieldTypes != null && fieldTypes.Contains(k) ? fieldTypes[k] : null;
                                // Campo-array (PUBLIC DIM arr(N)): alloca l'array PER-ISTANZA con le
                                // dimensioni registrate dal Compiler (prima era un default scalare →
                                // `arr(i)`/`SELF.arr(i)` falliva con "indexing a numeric scalar").
                                instance[fn] = fieldArrDims != null && fieldArrDims.Contains(k)
                                    ? MakeFieldArray(fieldArrDims[k], fDt)
                                    : DefaultValueForFieldType(fDt);
                            }
                    }
                    var numObj = task.Sp > 0 ? task.PopRef() : null;
                    var argCount = numObj is double d ? (int)d : numObj is int i ? i : 0;
                    var args = new object?[argCount];
                    for (var a = argCount - 1; a >= 0; a--)
                        args[a] = task.Sp > 0 ? task.PopRef() : null;
                    // Store ClosureData in __closure__ if present in args
                    foreach (var a in args)
                        if (a is ClosureData cd && !instance.ContainsKey("__closure__"))
                            instance["__closure__"] = cd;
                    // Always try to call constructor (even for 0 args)
                    int? ctorAddr = null;
                    if (descriptor["Constructors"] is System.Collections.IList ctors && ctors.Count > 0)
                    {
                        var fc = ctors[0];
                        if (fc is int iv) ctorAddr = iv;
                        else if (fc is double dv) ctorAddr = (int)dv;
                    }
                    if (ctorAddr.HasValue)
                    {
                        var frameLocals = CreateLocals();
                        frameLocals[0] = instance;
                        for (var a = 0; a < argCount; a++)
                            frameLocals[a + 1] = args[a];
                        // SavedStackPointer: posizione prima degli args (ripristina stack del chiamante).
                        // task.Sp e' GIA' al livello pre-chiamata qui: numObj e tutti gli args sono
                        // gia' stati fatti POP sopra (righe precedenti), quindi nessuna ulteriore
                        // somma va fatta — sommare di nuovo (+1+argCount, come faceva prima)
                        // gonfiava savedSp lasciando entries stale nell'array sottostante dello
                        // stack che potevano essere ripescate erroneamente da un OP_THROW/store
                        // immediatamente successivo (bug scoperto con THROW NEW <Classe>(arg1, arg2)
                        // a 2+ argomenti, dove OP_THROW leggeva un valore stale invece dell'istanza).
                        var savedSp = task.Sp;
                        var classCtorFrame = new PCodeCallFrame { SavedStackPointer = savedSp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = frameLocals, ConstructorInstance = instance };
                        EnterProcedureErrorScope(task, classCtorFrame);
                        task.Frame = classCtorFrame;
                        task.Ip = ctorAddr.Value;
                        return true;
                    }
                }
                task.PushRef(instance); return true;
            }

            case OpCode.OP_CLONE_UDT:
            {
                var top = task.Sp > 0 ? task.PopRef() : null;
                if (top is System.Collections.IDictionary srcDict)
                {
                    var clone = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                    foreach (System.Collections.DictionaryEntry entry in srcDict)
                        clone[(string)entry.Key] = entry.Value;
                    // I buffer dei gruppi (UNION/bitfield) sono storage PROPRIO dell'istanza:
                    // copia profonda, altrimenti scrivere nella copia altererebbe l'originale.
                    foreach (var key in clone.Keys.ToList())
                        if (key.StartsWith("__grp", StringComparison.Ordinal) && clone[key] is byte[] buf)
                            clone[key] = (byte[])buf.Clone();
                    task.PushRef(clone);
                }
                else
                {
                    task.PushRef(top);
                }
                return true;
            }

            // === EXCEPTIONS ===
            case OpCode.OP_PUSH_TRY:
            {
                var handlerOffset = ReadU16(task);
                var tryInfo = new TryInfo
                {
                    ProtectedStartIp = task.Ip,
                    HandlerIp = task.Ip + handlerOffset,
                    Frame = task.Frame,
                    StackPointer = task.Sp
                };
                if (_tryRegions.TryGetValue(task.CurrentInstructionIp, out var region))
                {
                    tryInfo.FinallyIp = region.FinallyIp;
                    tryInfo.EndIp = region.EndIp;
                    tryInfo.HasCatch = region.HasCatch;
                    task.CleanupStack.Push(tryInfo);
                }
                task.TryStack.Push(tryInfo);
                return true;
            }
            case OpCode.OP_POP_TRY:
            {
                if (task.TryStack.Count > 0)
                    task.TryStack.Pop();
                return true;
            }
            case OpCode.OP_THROW:
            {
                var val = task.Sp > 0 ? task.PopRef() : null;
                /* Punti di ripresa per ON ERROR: un THROW non numerico non passava da
                 * RaiseBasicError e lasciava ErrorResumeNextIp STANTIO (ripresa in un punto
                 * arbitrario o loop). Granularità di ISTRUZIONE come per gli altri errori. */
                task.ErrorLineIp = task.CurrentInstructionIp;
                task.ErrorResumeNextIp = StatementResumeIp(task.CurrentInstructionIp, task.Ip);
                // THROW di un CODICE NUMERICO = errore BASIC: stesso schema a 3 livelli di
                // `ERROR n` (TRY/CATCH -> ON ERROR GOTO -> fatale), così ERR.Number viene
                // popolata e `ON ERROR` lo cattura. Prima, senza TRY, il THROW terminava il
                // programma anche con un handler installato (divergenza con IL/C99).
                if (val is double or int or long or float or decimal)
                {
                    var code = Convert.ToDouble(val, System.Globalization.CultureInfo.InvariantCulture);
                    RaiseBasicError(task, code, BasicException.GetDefaultMessage((ErrorCode)(int)code));
                    return true;
                }
                // THROW di un oggetto/stringa: il valore lanciato resta il valore (per
                // `CATCH e AS <tipo>`), mentre ERR porta il dettaglio in forma di dizionario.
                var thrownCode = val switch
                {
                    System.Collections.IDictionary dct when dct["Number"] is not null => Convert.ToDouble(dct["Number"], System.Globalization.CultureInfo.InvariantCulture),
                    _ => (double)ErrorCode.UserDefined
                };
                var thrownDescription = val switch
                {
                    System.Collections.IDictionary dct when dct["Description"] is string ds => ds,
                    string str => str,
                    _ => "Unspecified runtime error"
                };
                SetErrGlobal(task, new System.Collections.Generic.Dictionary<string, object?>
                {
                    ["Number"] = thrownCode,
                    ["Description"] = thrownDescription,
                    ["LineNumber"] = (double)SourceLineForIp(task.CurrentInstructionIp),
                    ["SourceLineText"] = SourceLineTextForIp(ReferenceEquals(task, _generatorTraceTask) && _generatorTraceIp >= 0 ? _generatorTraceIp : task.CurrentInstructionIp),
                    ["StackTrace"] = BuildBasicTrace(task)
                });
                if (TryEnterExceptionHandler(task, val))
                    return true;
                // Nessun TRY: prova l'handler ON ERROR (parità con RaiseBasicError).
                if (task.ErrorMode == PCodeErrorMode.ResumeNext)
                {
                    task.Ip = task.ErrorResumeNextIp;
                    return true;
                }
                UnwindProcedureFramesToErrorHandler(task);
                if (task.ErrorHandlerIp > 0)
                {
                    if (task.ErrorHandlerIsGosub)
                    {
                        task.IpStack ??= new Stack<int>();
                        task.IpStack.Push(task.Ip);
                    }
                    task.Ip = task.ErrorHandlerIp.Value;
                    return true;
                }
                // No handler found — terminate
                task.State = PCodeTaskState.Finished;
                return false;
            }

            // === DATA STATEMENTS ===
            case OpCode.OP_READ:
            {
                // Legge il prossimo valore dal data pool (chunk.Data). Prima leggeva
                // task.ReadNextData() — placeholder che restituiva 0,1,2,... (bug: READ non
                // leggeva mai i valori DATA). Fuori pool → OUT OF DATA (TRY/CATCH → ON ERROR → throw).
                if (_dataPointer >= _dataPool.Length)
                {
                    RaiseBasicError(task, 200.0, "OUT OF DATA");
                    return true;
                }
                var val = _dataPool[_dataPointer++];
                task.PushRef(val);
                return true;
            }
            case OpCode.OP_RESTORE:
            {
                // Operando U16 = posizione nel data pool (0 = inizio; RESTORE <riga|etichetta>
                // risolta a compile-time dal compilatore). Prima azzerava il puntatore placeholder
                // (task.ResetDataPointer) e ignorava l'operando.
                _dataPointer = ReadU16(task);
                return true;
            }

            // === REFERENCES ===
            case OpCode.OP_PUSH_REF_LOCAL:
            {
                var slot = ReadU16(task);
                // Stesso array usato da OP_STORE_LOCAL/OP_LOAD_LOCAL: il frame del task
                // (quando esiste, es. __main chiamato dal prologo sintetico) o _locals del
                // task (entry point diretto). Prima usava SEMPRE task.GetLocals(): in un
                // __main top-level con prologo, STORE_LOCAL scriveva nel frame mentre
                // PUSH_REF_LOCAL leggeva da _locals -> RefToVar su null -> AADD/ACOPY/etc.
                // non modificavano la lista risultato (liste vuote, bug su TUTTE le
                // comprehension top-level e su qualunque AADD in una procedura chiamata
                // con OP_CALL dal prologo).
                var locals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
                task.PushRef(new RefToVar(locals, slot));
                return true;
            }
            case OpCode.OP_PUSH_REF_GLOBAL:
            {
                var idx = ReadU16(task);
                task.EnsureGlobals(idx + 1);
                task.PushRef(new RefToVar(task.Globals!, idx));
                return true;
            }

            case OpCode.OP_LOAD_MEMBER:
            {
                var offset = ReadU16(task);
                // implicit-await inline (FETCH(url).Campo su istanza di classe/tipo): future in cima
                if (AwaitInlineFuture(task, 3)) return false;
                var obj = task.Sp > 0 ? task.PopRef() : null;
                if (obj is System.Collections.IDictionary dict)
                {
                    if (PCodeFieldNameFromOffset(dict, offset) is string fieldName)
                    {
                        // Check if this field has a property getter (higher priority than field access)
                        if (dict["__class__"] is System.Collections.IDictionary classDescG
                            && classDescG.Contains("PropertyGetters") && classDescG["PropertyGetters"] is System.Collections.IDictionary pgs
                            && pgs[fieldName] is int getterAddr)
                        {
                            var fl = CreateLocals();
                            fl[0] = obj;
                            var memberGetterFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null };
                            EnterProcedureErrorScope(task, memberGetterFrame);
                            task.Frame = memberGetterFrame;
                            task.Ip = getterAddr;
                            return true;
                        }
                        task.PushRef(UnwrapByRefChain(dict[fieldName]));
                    }
                    else
                    { task.PushRef(UnwrapByRefChain(dict["__field_" + offset])); }
                }
                else if (obj is System.Collections.IList list) { task.PushRef((double)list.Count); }
                else if (obj is System.Collections.ICollection col2) { task.PushRef((double)col2.Count); }
                else
                    task.PushRef(null);
                return true;
            }
            case OpCode.OP_STORE_MEMBER:
            {
                var offset = ReadU16(task);
                var obj = task.Sp > 0 ? task.PopRef() : null;
                var val = task.Sp > 0 ? task.PopRef() : null;
                if (obj is System.Collections.IDictionary dict)
                {
                    if (dict["__class__"] is System.Collections.IDictionary smClassDesc
                        && PCodeFieldNameFromOffset(dict, offset) is string fieldName)
                    {
                        // Check PropertySetters first
                        try
                        {
                            if (smClassDesc["PropertySetters"] is System.Collections.IDictionary pss &&
                                pss[fieldName] is int setterAddr)
                            {
                                var fl = CreateLocals();
                                fl[0] = obj; fl[1] = val;
                                var memberSetterFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null };
                                EnterProcedureErrorScope(task, memberSetterFrame);
                                task.Frame = memberSetterFrame;
                                task.Ip = setterAddr;
                                return true;
                            }
                        }
                        catch { }
                        if (!WriteThroughByRefChain(dict, fieldName, val)) dict[fieldName] = val;
                        ApplyUdtGroupWrite(dict, smClassDesc, offset, val);
                    }
                    else
                    {
                        var fk = "__field_" + offset;
                        if (!WriteThroughByRefChain(dict, fk, val)) dict[fk] = val;
                    }
                }
                return true;
            }

            // Box BYREF su campo __ClosureCtx (piano Closure-ByRef-Box-Linker-Definitivo,
            // Fase 2/3): un campo che contiene un ByRefBox (posto lì da MakeByRefExpr, mai
            // dal codice utente) va SEMPRE srotolato in lettura e scritto ATTRAVERSO in
            // scrittura — mai trattato come un valore/campo normale. Srotola una CATENA di
            // box (OP_MAKE_BYREF su uno slot che contiene già un box ne crea uno che
            // indirizza a quello stesso slot — un livello di indirezione in più, innocuo se
            // srotolato completamente qui) invece di un solo livello.
            static object? UnwrapByRefChain(object? v)
            {
                var guard = 0;
                while (v is ByRefBox b && guard++ < 64) v = b.Value;
                return v;
            }

            // Se il campo contiene (attraverso una catena) un ByRefBox, scrive nell'INNERMOST
            // box invece di sovrascrivere il campo — preserva l'identità del box condivisa col
            // chiamante. Ritorna true se ha scritto attraverso un box (il chiamante non deve
            // più sovrascrivere il campo), false se il campo va scritto normalmente.
            static bool WriteThroughByRefChain(System.Collections.IDictionary dict, object key, object? val)
            {
                if (dict[key] is not ByRefBox box) return false;
                var guard = 0;
                while (box.Value is ByRefBox inner && guard++ < 64) box = inner;
                box.Value = val;
                return true;
            }

            case OpCode.OP_LOAD_STATIC_FIELD:
            {
                var symbolId = ReadU16(task);
                object? staticVal;
                lock (_staticFieldLock) _staticFieldValues.TryGetValue(symbolId, out staticVal);
                task.PushRef(staticVal);
                return true;
            }
            case OpCode.OP_STORE_STATIC_FIELD:
            {
                var symbolId = ReadU16(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                lock (_staticFieldLock) _staticFieldValues[symbolId] = val;
                return true;
            }
            case OpCode.OP_NEW_GENERATOR_EXPR:
            {
                var outputClosure = (task.Sp > 0 ? task.PopRef() : null) as ClosureData;
                var conditionClosure = (task.Sp > 0 ? task.PopRef() : null) as ClosureData;
                var sourceClosure = (task.Sp > 0 ? task.PopRef() : null) as ClosureData;
                if (sourceClosure == null || outputClosure == null)
                {
                    task.PushRef(null);
                    return true;
                }
                var genState = new PCodeGeneratorExprState();
                var sourceValue = CallClosureSync(task, sourceClosure);
                var innerEnumerator = GetRuntimeEnumeratorForGenerator(sourceValue);
                while (innerEnumerator.MoveNext())
                {
                    var current = innerEnumerator.Current;
                    if (conditionClosure != null)
                    {
                        var condResult = CallClosureSync(task, conditionClosure, current);
                        var isTrue = condResult switch
                        {
                            double d => d != 0.0,
                            bool b => b,
                            null => false,
                            _ => Convert.ToDouble(condResult) != 0.0
                        };
                        if (!isTrue) continue;
                    }
                    genState.Add(CallClosureSync(task, outputClosure, current));
                }
                task.PushRef(genState);
                return true;
            }
            case OpCode.OP_NEW_GENERATOR_CALL:
            {
                var addr = ReadU16(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                // Task dedicato SENZA Frame iniziale (mai pushato, a differenza di OP_CALL_N) —
                // il corpo del generatore gira "a livello top-level" su questo task, esattamente
                // come il codice principale del programma: OP_LOAD_LOCAL/OP_STORE_LOCAL cadono
                // già sul fallback task.GetLocals() quando Frame è null (stesso pattern usato
                // ovunque nella VM, es. OP_FOR_EACH_ADVANCE). Nessun thread reale lanciato (a
                // differenza di OP_GO, che accoda su PCodeScheduler): il task viene guidato in modo
                // sincrono da NEXT(), mai schedulato né eseguito in parallelo al chiamante.
                //
                // Gli argomenti vanno PUSHATI sullo stack del NUOVO task, non scritti direttamente
                // nei suoi locals: il PROLOGO già compilato del corpo del generatore (CompileProcedures,
                // Compiler.cs) inizia con OP_STORE_LOCAL per ogni parametro, che si aspetta di
                // POPPARE gli argomenti dallo stack — esattamente come per una chiamata normale
                // (OP_CALL_N). Scriverli direttamente nei locals veniva subito sovrascritto da
                // quel prologo con null (stack vuoto → OP_STORE_LOCAL popa "niente"), lasciando il
                // parametro sempre a 0/null (bug osservato: FOR i=1 TO n non eseguiva mai il corpo,
                // n restava 0).
                var genTask = new PCodeTask { Ip = addr, Globals = task.Globals };
                for (var i = 0; i < args.Length; i++)
                    genTask.PushRef(args[i]);
                task.PushRef(new PCodeGeneratorInstance(genTask));
                return true;
            }
            case OpCode.OP_CALL_MEMBER:
            {
                var nameIdx = ReadU16(task);
                var methodName = nameIdx >= 0 && nameIdx < _constants.Count ? _constants[nameIdx] as string : null;
                if (methodName == null) { task.PushRef(null); return true; }
                
                var numArgs = task.Sp > 0 ? task.PopRef() : null;
                var argCount = Convert.ToInt32(numArgs ?? 0.0);
                // implicit-await inline: l'istanza è sotto numArgs+args (es. FETCH(url).SaveToFile(...))
                if (AwaitInlineFutureAt(task, 3, task.Sp - argCount - 1)) return false;
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                
                // Peek instance: if null after pop, search stack for matching collection
                var instance = task.Sp > 0 ? task.PopRef() : null;
                if (instance == null)
                {
                    // Try to find instance on the remaining stack (accumulated from prior statements)
                    for (var si = task.Sp - 1; si >= 0; si--)
                    {
                        var tv = task.Stack[si].Type == 2 ? task.Stack[si].Ref : null;
                        if (tv is System.Collections.IDictionary || tv is System.Collections.IList)
                        { instance = tv; break; }
                    }
                }
                if (instance == null)
                {
                    // Dispatch su riferimento nil (`DIM c AS Cls` senza NEW): errore BASIC
                    // catturabile (parità IL/ON ERROR), mai un nil silenzioso (bug
                    // CLASS-MethodCall-Su-Nil-Dispatch-Divergente).
                    RaiseBasicError(task, 91.0, "Object reference not set to an instance of an object");
                    return true;
                }

                // STRINGBUILDER (System.Text.StringBuilder): i metodi Append/AppendLine/
                // AppendFormat/Insert/Replace/Remove/Clear/ToString/Length/Capacity.
                if (instance is System.Text.StringBuilder sbInst)
                {
                    string SArg(int i) => i < args.Length && args[i] is not null ? args[i]!.ToString()! : "";
                    switch (methodName.ToUpperInvariant())
                    {
                        case "APPEND": sbInst.Append(args.Length > 0 ? args[0] : null); task.PushRef(sbInst); return true;
                        case "APPENDLINE":
                            if (args.Length > 0) sbInst.AppendLine(SArg(0)); else sbInst.AppendLine();
                            task.PushRef(sbInst); return true;
                        case "APPENDFORMAT":
                            if (args.Length >= 1) sbInst.AppendFormat(SArg(0), args.Skip(1).Cast<object?>().ToArray());
                            task.PushRef(sbInst); return true;
                        case "INSERT":
                            if (args.Length >= 2) sbInst.Insert((int)Convert.ToDouble(args[0] ?? 0.0), SArg(1));
                            task.PushRef(sbInst); return true;
                        case "REPLACE":
                            if (args.Length >= 2) sbInst.Replace(SArg(0), SArg(1));
                            task.PushRef(sbInst); return true;
                        case "REMOVE":
                            if (args.Length >= 2) sbInst.Remove((int)Convert.ToDouble(args[0] ?? 0.0), (int)Convert.ToDouble(args[1] ?? 0.0));
                            task.PushRef(sbInst); return true;
                        case "CLEAR": sbInst.Clear(); task.PushRef(sbInst); return true;
                        case "TOSTRING": task.PushRef(sbInst.ToString()); return true;
                        case "LENGTH": task.PushRef((double)sbInst.Length); return true;
                        case "CAPACITY": task.PushRef((double)sbInst.Capacity); return true;
                    }
                }

                // REGEX (Basic.RegexNative.RegexWrapper): IsMatch/Match/Matches/Replace/Split.
                if (instance is Basic.RegexNative.RegexWrapper _reCall)
                {
                    string RArg(int i) => i < args.Length && args[i] is not null ? args[i]!.ToString()! : "";
                    switch (methodName.ToUpperInvariant())
                    {
                        case "ISMATCH": task.PushRef(_reCall.IsMatch(RArg(0))); return true;
                        case "MATCH": task.PushRef(_reCall.Match(RArg(0))); return true;
                        case "MATCHES": task.PushRef(_reCall.Matches(RArg(0))); return true;
                        case "REPLACE": task.PushRef(_reCall.Replace(RArg(0), RArg(1))); return true;
                        case "SPLIT": task.PushRef(_reCall.Split(RArg(0))); return true;
                    }
                }

                if (instance is Basic.IlBackend.PCode.Http.PCodeHttpResponse httpRes)
                {
                    // Nomi metodo allineati alla convenzione REALE usata dai sample .bas
                    // esistenti (Executor/Samples/Http/*.bas, scritti per l'AST executor):
                    // SendJSON (non SendJson), StatusCode/ContentType come PROPRIETA' (gestite
                    // in OP_LOAD_FIELD/OP_STORE_FIELD, non qui).
                    switch (methodName)
                    {
                        case "Send" when argCount >= 1: httpRes.Send(args[0]?.ToString() ?? ""); break;
                        case "SendBytes" when argCount >= 1 && args[0] is byte[] bytes: httpRes.SendBytes(bytes, argCount >= 2 ? args[1]?.ToString() ?? "application/octet-stream" : "application/octet-stream"); break;
                        case "SendJSON" or "SendJson" when argCount >= 1: httpRes.SendJson(args[0]); break;
                        case "SendFile" when argCount >= 1: httpRes.SendFile(args[0]?.ToString() ?? "", argCount >= 2 ? args[1]?.ToString() : null); break;
                        case "SetStatus" or "Status" when argCount >= 1: httpRes.SetStatus(Convert.ToInt32(args[0] ?? 200.0)); break;
                        case "SetHeader" when argCount >= 2: httpRes.SetHeader(args[0]?.ToString() ?? "", args[1]?.ToString() ?? ""); break;
                        case "SetCookie" when argCount >= 2: httpRes.SetCookie(args[0]?.ToString() ?? "", args[1]?.ToString() ?? "",
                            argCount >= 3 ? args[2]?.ToString() : null, argCount < 4 || (args[3] is double hb && hb != 0), argCount >= 5 && args[4] is double sb && sb != 0); break;
                        case "Redirect" when argCount >= 1: httpRes.Redirect(args[0]?.ToString() ?? "/", argCount >= 2 ? Convert.ToInt32(args[1] ?? 302.0) : 302); break;
                        // ── Streaming (chunked): RES.STARTSTREAM/WRITE/FLUSH/ENDSTREAM ──────
                        // Logica UNICA condivisa con il backend IL (PCodeHttpResponseStreamInvoke).
                        case var _ when PCodeHttpResponseStreamInvoke(httpRes, methodName, args, argCount): break;
                    }
                    task.PushRef(null);
                    return true;
                }

                if (instance is Basic.IlBackend.PCode.Http.PCodeWebSocketHandle wsInst)
                {
                    switch (methodName)
                    {
                        case "Send" when argCount >= 1:
                            ParkAsync(task, PCodeWaitKind.Network,
                                ct => wsInst.SendAsync(args[0]?.ToString() ?? "", ct),
                                _ => task.PushRef(null));
                            return true;
                        case "SendBytes" when argCount >= 1 && args[0] is byte[] wsBytes:
                            ParkAsync(task, PCodeWaitKind.Network,
                                ct => wsInst.SendBytesAsync(wsBytes, ct),
                                _ => task.PushRef(null));
                            return true;
                        case "Close":
                            ParkAsync(task, PCodeWaitKind.Network,
                                ct => wsInst.CloseAsync(argCount >= 1 ? args[0]?.ToString() : null, ct),
                                _ => task.PushRef(null));
                            return true;
                    }
                    task.PushRef(null);
                    return true;
                }

                if (instance is Basic.IlBackend.PCode.Http.PCodeFetchResult fetchResInst)
                {
                    switch (methodName)
                    {
                        case "SaveToFile" when argCount >= 1: fetchResInst.SaveToFile(args[0]?.ToString() ?? ""); task.PushRef(null); return true;
                        case "GetBodyBytes": task.PushRef(fetchResInst.BodyBytes); return true;
                    }
                    task.PushRef(null);
                    return true;
                }

                if (instance is CommonLib.Http.CookieJar cookieJarInst)
                {
                    if (methodName == "Clear") cookieJarInst.Clear();
                    task.PushRef(null);
                    return true;
                }

                // HTTPREQUEST seminata come Dictionary (req): il dict contiene l'istanza reale
                // in "__httpRequest" (vedi BuildRequestDict) → dispatcha i metodi BASIC della
                // HTTPREQUEST per nome (req.ParseForm()/req.ParseMultipartForm()). UNICA logica
                // shared con backend IL (IlRuntime.CollectionInvoke → TryHttpRequestDictInvoke).
                if (instance is System.Collections.IDictionary httpReqDict
                    && PCodeHttpRequestDictInvoke(httpReqDict, methodName, argCount, out object? hrRes))
                {
                    task.PushRef(hrRes);
                    return true;
                }

                // F4 (Incongruenze): BRIDGE managed — un solo dispatcher per le collection.
                // I casi non-collection (StringBuilder/HTTP/class) sono già stati gestiti sopra;
                // per IList/IDictionary si prova il dispatcher condiviso IL e si ricade sul
                // vecchio switch PCode SOLO se il metodo non è noto (NotSupportedException).
                // Le due gambe condividono così la stessa semantica (Add/Remove/Keys/Min/Max/
                // Trie/Bag/Graph/...), senza duplicare 76 casi.
                /* PRECEDENZA DEI METODI DI CLASSE (2026-11-12). L'istanza di una classe BASIC è
                 * un Dictionary con "__class__" (descrittore con VTable/PropertyGetters): senza
                 * questo controllo il bridge sotto RIVENDICAVA i nomi di collection e il metodo
                 * utente non veniva mai chiamato — `f.Count()` (0 argomenti) tornava il numero
                 * di CHIAVI dell'istanza invece del valore del metodo (controls/segmented_uc:
                 * `n=9` invece di `4` su PCode, IL/C99 corretti). Se il membro è nella
                 * VTable/PropertyGetters della classe, la risoluzione della classe ha la
                 * precedenza; altrimenti (istanza "nuda", es. dict/JSON) il bridge resta. */
                int? classMethodAddr = null;
                if (instance is System.Collections.IDictionary cdInst && cdInst.Contains("__class__")
                    && cdInst["__class__"] is System.Collections.IDictionary cdDesc)
                {
                    if (cdDesc["VTable"] is System.Collections.IDictionary cdVt)
                    {
                        var cdMa = cdVt[methodName ?? ""];
                        if (cdMa is int cdIa) classMethodAddr = cdIa;
                        else if (cdMa is double cdDa) classMethodAddr = (int)cdDa;
                    }
                    if (!classMethodAddr.HasValue && cdDesc["PropertyGetters"] is System.Collections.IDictionary cdPg)
                    {
                        var cdGa = cdPg[methodName ?? ""];
                        if (cdGa is int cdGia) classMethodAddr = cdGia;
                        else if (cdGa is double cdGda) classMethodAddr = (int)cdGda;
                    }
                }
                if (!classMethodAddr.HasValue && (instance is System.Collections.IList || instance is System.Collections.IDictionary
                    // HLL: NON è IList/IDictionary (non conserva gli elementi) ma va dispatchato
                    // dal dispatcher condiviso (prima finiva nella reflection generica, che
                    // ingoiava le eccezioni: `Load("garbage")` non segnalava nulla).
                    || instance is CommonLib.Collections.HyperLogLog))
                {
                    try
                    {
                        task.PushRef(IlRuntime.CollectionInvoke(instance, methodName, args));
                        return true;
                    }
                    catch (NotSupportedException)
                    {
                        /* metodo non gestito dal dispatcher condiviso: fallback PCode sotto */
                    }
                }

                /* CIRCULAR (typeCode 13 → List<object?>): Current()/Rotate(n) sono helper
                 * BASIC, non metodi CLR — senza questo ramo erano no-op silenziosi
                 * (CollectionsLinked: "Current:" vuoto, Rotate inerte). Stessi helper
                 * condivisi con IL (stato per istanza + rotazione FISICA della lista:
                 * stesso ordine di iterazione di VM-C/C99). */
                if (instance is System.Collections.IList circInst && methodName != null)
                {
                    if (methodName.Equals("Current", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef(IlRuntime.CircularCurrent(instance));
                        return true;
                    }
                    if (methodName.Equals("Rotate", StringComparison.OrdinalIgnoreCase) && argCount >= 1)
                    {
                        IlRuntime.CircularRotate(instance, Convert.ToDouble(args[0] ?? 0.0));
                        task.PushRef(null);
                        return true;
                    }
                }
                int? methodAddr = classMethodAddr;
                // Check IList FIRST (collections like List<T>)
                if (instance is System.Collections.IList iList)
                {
                    if (methodName == "Count" && argCount == 0) { task.PushRef((double)iList.Count); return true; }
                    if (methodName == "Add" && argCount == 1) { iList.Add(args[0]); task.PushRef(null); return true; }
                    if (methodName == "AddRange" && argCount == 1 && args[0] is System.Collections.IList src) { foreach (var item in src) iList.Add(item); task.PushRef(null); return true; }
                    if (methodName == "RemoveAt" && argCount == 1) { iList.RemoveAt(Convert.ToInt32(args[0] ?? 0)); task.PushRef(null); return true; }
                    if (methodName == "IndexOf" && argCount == 1) { task.PushRef((double)iList.IndexOf(args[0])); return true; }
                    if (methodName == "Insert" && argCount == 2) { iList.Insert(Convert.ToInt32(args[0] ?? 0), args[1]); task.PushRef(null); return true; }
                    if (methodName == "Clear" && argCount == 0) { iList.Clear(); task.PushRef(null); return true; }
                    if (methodName == "Contains" && argCount == 1) { task.PushRef(iList.Contains(args[0]) ? 1.0 : 0.0); return true; }
                    if (methodName == "Join" && argCount >= 1) { var jsep0 = args[0]?.ToString() ?? ""; task.PushRef(string.Join(jsep0, iList.Cast<object?>().Select(x => x?.ToString() ?? ""))); return true; }
                    if (methodName == "Max" && argCount == 0) { task.PushRef(iList.Cast<object?>().Select(SafeDouble).DefaultIfEmpty(0).Max()); return true; }
                    if (methodName == "Min" && argCount == 0) { task.PushRef(iList.Cast<object?>().Select(SafeDouble).DefaultIfEmpty(0).Min()); return true; }
                    if (methodName == "Sum" && argCount == 0) { task.PushRef(iList.Cast<object?>().Select(SafeDouble).Sum()); return true; }
                }
                if (instance is System.Collections.IDictionary instDict)
                {
                    var cd = instDict.Contains("__class__") ? instDict["__class__"] as System.Collections.IDictionary : null;
                    var vt = cd?["VTable"] as System.Collections.IDictionary;
                    var ma = vt?[methodName ?? ""];
                    if (ma is int ia) methodAddr = ia;
                    else if (ma is double da) methodAddr = (int)da;
                    // Property getters
                    if (!methodAddr.HasValue && cd != null && cd.Contains("PropertyGetters") && cd["PropertyGetters"] is System.Collections.IDictionary pgs)
                    {
                        var ga = pgs[methodName ?? ""];
                        if (ga is int gia) methodAddr = gia;
                        else if (ga is double gda) methodAddr = (int)gda;
                    }
                    // Plain dictionary operations (when NOT a class instance)
                    if (cd == null)
                    {
                        if (methodName == "Count" && argCount == 0)
                        {
                            // BAG (Dictionary<object,long>): Count = somma dei conteggi
                            // (elementi totali), non delle chiavi — come il vecchio wrapper
                            // Bag.Count() (_totalCount). Un Dict normale usa
                            // Dictionary<object,object?> → Count = chiavi.
                            if (instDict.GetType().IsGenericType &&
                                instDict.GetType().GetGenericArguments().Length == 2 &&
                                instDict.GetType().GetGenericArguments()[1] == typeof(long))
                            {
                                long sum = 0;
                                foreach (var v in instDict.Values) { if (v is long lv) sum += lv; }
                                task.PushRef((double)sum); return true;
                            }
                            task.PushRef((double)instDict.Count); return true;
                        }
                        // Convention BASIC: bool = -1/0 (parity IL ContainsKey CLR bool +
                        // VMC); qui era 1.0/0.0 → mismatch su PRINT in mini di parità.
                        if (methodName == "ContainsKey" && argCount == 1) { task.PushRef(instDict.Contains(args[0]!) ? -1.0 : 0.0); return true; }
                        if (methodName == "Keys" && argCount == 0) { task.PushRef(instDict.Keys); return true; }
                        if (methodName == "Values" && argCount == 0) { task.PushRef(instDict.Values); return true; }
                        if (methodName == "Remove" && argCount == 1)
                        {
                            // BAG (Dictionary<object,long>): Remove(item) rimuove UNA singola
                            // istanza — decrementa la molteplicità; se arriva a 0 rimuove la chiave.
                            // (semantica CommonLib, campione: "Remove rimuove una singola istanza")
                            if (instDict.GetType().IsGenericType &&
                                instDict.GetType().GetGenericArguments().Length == 2 &&
                                instDict.GetType().GetGenericArguments()[1] == typeof(long) &&
                                instDict.Contains(args[0]!))
                            {
                                var cv = instDict[args[0]!];
                                long mult = cv is long cl ? cl : (long)System.Convert.ToDouble(cv);
                                if (mult <= 1) { instDict.Remove(args[0]!); }
                                else { instDict[args[0]!] = mult - 1; }
                                task.PushRef(-1.0); return true;
                            }
                            var had = instDict.Contains(args[0]!); instDict.Remove(args[0]!); task.PushRef(had ? -1.0 : 0.0); return true;
                        }
                        if (methodName == "Clear" && argCount == 0) { instDict.Clear(); task.PushRef(null); return true; }
                        if (methodName == "TryGetValue" && argCount == 1) { task.PushRef(instDict[args[0]!]); return true; }
                        // Add(key, value) mancava qui — con solo IList.Add (1 arg, sopra) gestito,
                        // qualunque "dict.Add(k, v)" su un dizionario semplice (es. opts.Headers,
                        // che la VM rappresenta come Dictionary<string,object?> — vedi typeCode 22)
                        // cadeva silenziosamente su nessun case, un no-op mai notato prima (bug
                        // scoperto verificando FETCH avanzato Fase 3 — opts.Headers.Add(...) non
                        // impostava mai davvero l'header). Semantica "set" (sovrascrive se la
                        // chiave esiste gia'), non "throw on duplicate" come il vero
                        // IDictionary.Add di .NET — coerente con come un utente BASIC si aspetta
                        // che si comporti, e con HttpHeaders.SetHeader lato AST.
                        if (methodName == "Add" && argCount == 2) { instDict[args[0]!] = args[1]; task.PushRef(null); return true; }
                        // Set/SetKeyValue(key, value) — stessa semantica di Add(k,v) sopra:
                        // il linker (F1 Incongruenze) conserva il MethodCallExpr per SET sul
                        // dict e il membro canonico è "Set" (CollMembers) — senza
                        // questo case il set era un no-op silenzioso (keys mancavano).
                        if (methodName is "Set" or "SetKeyValue" && argCount == 2) { instDict[args[0]!] = args[1]; task.PushRef(null); return true; }
                        // Get(key) mancava qui — stesso identico buco di Add(k,v) sopra (BUG 22):
                        // req.Headers.Get("User-Agent") (l'unico modo documentato per leggere un
                        // header con un nome che contiene un trattino, non un identificatore BASIC
                        // valido — vedi BuildRequestDict, Headers e' un Dictionary<string,object?>
                        // case-insensitive) cadeva silenziosamente su nessun case, restituendo
                        // sempre null senza errore. Trovato verificando HttpFetchAdvanced.bas sulla
                        // P-CODE VM (mismatch "userAgent":null contro l'AST, che invece lo risolve
                        // correttamente tramite lo stesso metodo).
                        if (methodName == "Get" && argCount == 1) { task.PushRef(instDict.Contains(args[0]!) ? instDict[args[0]!] : null); return true; }
                        // F1 (parity IL DictMaxMinPair + VMC): MaxPair/MinPair — chiave con
                        // valore max/min (numeric mode wins su misto, stringa ordinale).
                        if (methodName is "MaxPair" or "MinPair" && argCount == 0 && instDict.Count > 0)
                        {
                            var isMax = methodName == "MaxPair";
                            object? bestKey = null;
                            double? bestNum = null;
                            string? bestStr = null;
                            bool numericMode = false;
                            foreach (var k in instDict.Keys.Cast<object?>())
                            {
                                var v = instDict[k!];
                                var s = Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture) ?? "";
                                if (double.TryParse(s, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var num))
                                {
                                    if (!numericMode) { numericMode = true; bestKey = k; bestNum = num; bestStr = null; }
                                    else if ((isMax && num > bestNum) || (!isMax && num < bestNum)) { bestKey = k; bestNum = num; }
                                }
                                else if (!numericMode)
                                {
                                    if (bestKey == null) { bestKey = k; bestStr = s; }
                                    else if ((isMax && string.CompareOrdinal(s, bestStr) > 0) || (!isMax && string.CompareOrdinal(s, bestStr) < 0)) { bestKey = k; bestStr = s; }
                                }
                            }
                            task.PushRef(bestKey);
                            return true;
                        }
                    }
                }
                else if (instance is System.Collections.IList list)
                {
                    if (methodName != null && methodName.Equals("Count", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef((double)list.Count);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Add", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                    {
                        list.Add(args[0]);
                        task.PushRef(null);
                        return true;
                    }
                    // F1 (parity VM/C): AddUnique — HashSet-like, duplicato ignorato.
                    if (methodName != null && methodName.Equals("AddUnique", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                    {
                        if (!list.Contains(args[0])) list.Add(args[0]);
                        task.PushRef(null);
                        return true;
                    }
                    // F1 (parity VM/C gmb_vm_core OP_CALL_MEMBER list): AddAfter(i,v) inserisce
                    // DOPO la posizione i (0-based), AddBefore(i,v) prima; pad nil se oltre.
                    if (methodName != null && (methodName.Equals("AddAfter", StringComparison.OrdinalIgnoreCase) || methodName.Equals("AddBefore", StringComparison.OrdinalIgnoreCase)) && argCount == 2)
                    {
                        var insIdx = Convert.ToInt32(Convert.ToDouble(args[0] ?? 0));
                        if (methodName.Equals("AddAfter", StringComparison.OrdinalIgnoreCase)) insIdx += 1;
                        if (insIdx < 0) insIdx = 0;
                        while (insIdx > list.Count) list.Add(null);
                        list.Insert(Math.Min(insIdx, list.Count), args[1]);
                        task.PushRef(null);
                        return true;
                    }
                    // F1 (parity VM/C): IsEmpty → count==0 → 1/0.
                    if (methodName != null && methodName.Equals("IsEmpty", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef(list.Count == 0 ? -1.0 : 0.0);
                        return true;
                    }
                    // F1 (parity VM/C/IL): Front = testa, Back = coda.
                    if (methodName != null && (methodName.Equals("Front", StringComparison.OrdinalIgnoreCase) || methodName.Equals("Back", StringComparison.OrdinalIgnoreCase)) && argCount == 0 && list.Count > 0)
                    {
                        task.PushRef(methodName.Equals("Front", StringComparison.OrdinalIgnoreCase) ? list[0] : list[list.Count - 1]);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("AddRange", StringComparison.OrdinalIgnoreCase) && argCount == 1 && args[0] is System.Collections.IList srcList)
                    {
                        foreach (var item in srcList) list.Add(item);
                        task.PushRef(null);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("RemoveAt", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                    {
                        list.RemoveAt(Convert.ToInt32(args[0] ?? 0));
                        task.PushRef(null);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("IndexOf", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                    {
                        task.PushRef((double)list.IndexOf(args[0]));
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Insert", StringComparison.OrdinalIgnoreCase) && argCount == 2)
                    {
                        list.Insert(Convert.ToInt32(args[0] ?? 0), args[1]);
                        task.PushRef(null);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Clear", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        list.Clear();
                        task.PushRef(null);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Contains", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                    {
                        task.PushRef(list.Contains(args[0]) ? 1.0 : 0.0);
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Join", StringComparison.OrdinalIgnoreCase) && argCount >= 1)
                    {
                        var jsep1 = args[0]?.ToString() ?? "";
                        task.PushRef(string.Join(jsep1, list.Cast<object?>().Select(x => x?.ToString() ?? "")));
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Max", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef(list.Cast<object?>().Select(SafeDouble).DefaultIfEmpty(0).Max());
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Min", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef(list.Cast<object?>().Select(SafeDouble).DefaultIfEmpty(0).Min());
                        return true;
                    }
                    if (methodName != null && methodName.Equals("Sum", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        task.PushRef(list.Cast<object?>().Select(SafeDouble).Sum());
                        return true;
                    }
                    // Fluent API: Sort, Reverse, Filter, Map, Unique
                    if (methodName != null && methodName.Equals("Sort", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        var arr = list.Cast<object?>().ToArray(); Array.Sort(arr); list.Clear(); foreach (var it in arr) list.Add(it);
                        task.PushRef(list); return true;
                    }
                    if (methodName != null && methodName.Equals("Reverse", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        var arr = list.Cast<object?>().ToArray(); Array.Reverse(arr); list.Clear(); foreach (var it in arr) list.Add(it);
                        task.PushRef(list); return true;
                    }
                    if (methodName != null && methodName.Equals("Unique", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        var uniq = list.Cast<object?>().Distinct().ToList(); list.Clear(); foreach (var it in uniq) list.Add(it);
                        task.PushRef(list); return true;
                    }
                    if (methodName != null && methodName.Equals("Flatten", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                    {
                        var flat = new System.Collections.Generic.List<object?>();
                        void FlattenRec(System.Collections.IEnumerable enu) { foreach (var item in enu) { if (item is System.Collections.IEnumerable sub) FlattenRec(sub); else flat.Add(item); } }
                        FlattenRec(list); list.Clear(); foreach (var it in flat) list.Add(it);
                        task.PushRef(list); return true;
                    }
                    if (methodName != null && methodName.Equals("Filter", StringComparison.OrdinalIgnoreCase) && argCount == 1 && args[0] is ClosureData filterMemFn)
                    {
                        var filtered = new System.Collections.Generic.List<object?>();
                        foreach (var item in list) { var res = CallClosureSync(task, filterMemFn, item); if (SafeDouble(res) != 0.0) filtered.Add(item); }
                        list.Clear(); foreach (var it in filtered) list.Add(it);
                        task.PushRef(list); return true;
                    }
                    if (methodName != null && methodName.Equals("Map", StringComparison.OrdinalIgnoreCase) && argCount == 1 && args[0] is ClosureData mapMemFn)
                    {
                        var mapped = new System.Collections.Generic.List<object?>();
                        foreach (var item in list) mapped.Add(CallClosureSync(task, mapMemFn, item));
                        list.Clear(); foreach (var it in mapped) list.Add(it);
                        task.PushRef(list); return true;
                    }
                }
                
                // Handle Queue<object?> method calls
                if (instance is System.Collections.Generic.Queue<object?> qInst)
                {
                    if (methodName != null && methodName.Equals("Enqueue", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                        { qInst.Enqueue(args[0]); task.PushRef(null); return true; }
                    if (methodName != null && methodName.Equals("Dequeue", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(qInst.Dequeue()); return true; }
                    if (methodName != null && methodName.Equals("Peek", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(qInst.Peek()); return true; }
                    if (methodName != null && methodName.Equals("Count", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef((double)qInst.Count); return true; }
                    if (methodName != null && methodName.Equals("Clear", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { qInst.Clear(); task.PushRef(null); return true; }
                }
                // Handle Stack<object?> method calls
                if (instance is System.Collections.Generic.Stack<object?> stInst)
                {
                    if (methodName != null && methodName.Equals("Push", StringComparison.OrdinalIgnoreCase) && argCount == 1)
                        { stInst.Push(args[0]); task.PushRef(null); return true; }
                    if (methodName != null && methodName.Equals("Pop", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(stInst.Pop()); return true; }
                    if (methodName != null && methodName.Equals("Peek", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(stInst.Peek()); return true; }
                    if (methodName != null && methodName.Equals("Count", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef((double)stInst.Count); return true; }
                    if (methodName != null && methodName.Equals("Clear", StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { stInst.Clear(); task.PushRef(null); return true; }
                }
                
                // Handle DictionaryEntry property access (Key/Value)
                if (instance is System.Collections.DictionaryEntry de)
                {
                    if ("Value".Equals(methodName, StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(de.Value); return true; }
                    if ("Key".Equals(methodName, StringComparison.OrdinalIgnoreCase) && argCount == 0)
                        { task.PushRef(de.Key); return true; }
                }
                
                // ── Metodi BASIC specializzati (non 1:1 con i metodi C#) ──
                // TREE/AVL/REDBLACK (SortedDictionary): Min/Max/ExtractMin/ExtractMax/
                // First/Last non sono metodi C# — Min=prima chiave, Max=ultima.
                var upName = methodName?.ToUpperInvariant();
                // TREE/AVL/REDBLACK (SortedDictionary): Min/Max/ExtractMin/ExtractMax/
                // First/Last non sono metodi C# — Min=prima chiave, Max=ultima.
                if (instance is System.Collections.IDictionary kvDict &&
                    upName is "MIN" or "MAX" or "EXTRACTMIN" or "EXTRACTMAX" or "FIRST" or "LAST")
                {
                    if (kvDict.Count == 0) { task.PushRef(null); return true; }
                    if (upName is "MIN" or "EXTRACTMIN" or "FIRST")
                    {
                        var e = kvDict.GetEnumerator(); e.MoveNext();
                        var kv = new System.Collections.Generic.List<object?> { e.Key, e.Value };
                        if (upName == "EXTRACTMIN") kvDict.Remove(e.Key!);
                        task.PushRef(kv); return true;
                    }
                    var em = kvDict.GetEnumerator(); object? lastK = null, lastV = null;
                    while (em.MoveNext()) { lastK = em.Key; lastV = em.Value; }
                    var kv2 = new System.Collections.Generic.List<object?> { lastK, lastV };
                    if (upName == "EXTRACTMAX") kvDict.Remove(lastK!);
                    task.PushRef(kv2); return true;
                }
                 // CACHE/collection chiave→valore: Get/Keys/ContainsKey via interfaccia non
                 // generica (niente reflection per nome).
                 if (instance is CommonLib.Collections.IKeyValueIndexable kvi &&
                     upName is "GET" or "KEYS" or "CONTAINSKEY")
                 {
                     if (upName == "GET" && argCount == 1)
                     {
                         if (kvi.TryGetValue(args[0], out var gv))
                         { task.PushRef(gv); return true; }
                         task.PushRef(null); return true;
                     }
                     if (upName == "KEYS" && argCount == 0)
                     {
                         /* Ordine CANONICO per nome chiave (C99/VM/C usano le chiavi
                          * ordinate; ConcurrentDictionary di Cache dà un ordine di hash
                          * instabile: "Keys" dava key2,key1 solo sul PCode). */
                         var keys = new System.Collections.Generic.List<object?>();
                         foreach (var kv in kvi.Entries())
                             keys.Add(kv.Key);
                         keys.Sort((a, b) => string.CompareOrdinal(
                             Convert.ToString(a, System.Globalization.CultureInfo.InvariantCulture),
                             Convert.ToString(b, System.Globalization.CultureInfo.InvariantCulture)));
                         task.PushRef(keys); return true;
                     }
                     if (upName == "CONTAINSKEY" && argCount == 1)
                     {
                         task.PushRef(kvi.TryGetValue(args[0], out _) ? -1.0 : 0.0);
                         return true;
                     }
                     task.PushRef(null); return true;
                 }

                 // BPLUSTREE/RedBlackTree: Min/Max/Extract/Get via interfaccia non generica.
                 if (instance is CommonLib.Collections.IKeyValueIndexable kvi2 &&
                     upName is "MIN" or "MAX" or "EXTRACTMAX" or "EXTRACTMIN" or "GET")
                 {
                     if (upName is "MIN" or "MAX" or "EXTRACTMIN" or "EXTRACTMAX")
                     {
                         object? mink = null, minv = null, maxk = null, maxv = null;
                         var ok = upName is "MIN" or "EXTRACTMIN"
                             ? kvi2.TryGetMin(out mink, out minv)
                             : kvi2.TryGetMax(out maxk, out maxv);
                         if (ok)
                         {
                             /* EXTRACT* rimuove la voce (DictEdgePair con extract=true nel
                              * dispatcher condiviso, come IL/VM/C/C99): prima restituiva la
                              * coppia senza rimuovere ("Nuovo Max" restava zulu). */
                             if (upName is "EXTRACTMIN" or "EXTRACTMAX")
                             {
                                 /* Rimuove davvero la voce (BPlusTree.Delete): prima
                                  * restituiva la coppia lasciandola nell'albero
                                  * ("Nuovo Max" restava [zulu,30]). */
                                 var del = instance!.GetType().GetMethod("Delete",
                                     System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                                 if (del != null)
                                     del.Invoke(instance, new[] { upName == "EXTRACTMIN" ? mink : maxk });
                                 var kvx = new System.Collections.Generic.List<object?> { upName == "EXTRACTMIN" ? mink : maxk, upName == "EXTRACTMIN" ? minv : maxv };
                                 task.PushRef(kvx); return true;
                             }
                             var kv = new System.Collections.Generic.List<object?> { upName is "MIN" or "EXTRACTMIN" ? mink : maxk, upName is "MIN" or "EXTRACTMIN" ? minv : maxv };
                             task.PushRef(kv); return true;
                         }
                         task.PushRef(null); return true;
                     }
                     if (upName == "GET" && argCount == 1)
                     {
                         if (kvi2.TryGetValue(args[0], out var gv2)) { task.PushRef(gv2); return true; }
                         task.PushRef(null); return true;
                     }
                     task.PushRef(null); return true;
                 }

                // Collezioni avanzate CommonLib.Collections (Graph/Timeline/Heap/...):
                // dispatch per RIFLESSIONE sul nome del metodo. I metodi C# reali hanno
                // nomi tipo AddNode/Bfs/Dfs/GetNeighbors/RemoveEdge/HasNode/... — coerenti
                // con i nomi BASIC (TypeSchemaRegistry li dichiara con lo stesso casing).
                // F1: GETEDGEWEIGHT — la classe espone TryGetEdgeWeight(from, to, out w):
                // niente alias generico, il VALORE è il peso (0 se assente), parity VMC/IL.
                if (instance != null && upName == "GETEDGEWEIGHT" && argCount == 2
                    && instance.GetType().Name.StartsWith("Graph", StringComparison.Ordinal)
                    && instance.GetType().GetMethod("TryGetEdgeWeight", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance) is { } tgw)
                {
                    var tgwArgs = new object?[3] { args[0]!, args[1]!, 0.0 };
                    var ok = (bool)tgw.Invoke(instance, tgwArgs)!;
                    task.PushRef(ok ? Convert.ToDouble(tgwArgs[2]) : 0.0);
                    return true;
                }
                // F1 (parity VM/C/IL): AddNodeConsistent — idempotente, -1 se il nodo esiste
                // dopo la chiamata, 0 su errore.
                if (instance != null && upName == "ADDNODECONSISTENT" && argCount == 1
                    && instance.GetType().Name.StartsWith("Graph", StringComparison.Ordinal)
                    && instance.GetType().GetMethod("AddNode", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance) is { } anp
                    && instance.GetType().GetMethod("HasNode", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance) is { } hnp)
                {
                    anp.Invoke(instance, new[] { args[0] });
                    task.PushRef((bool)hnp.Invoke(instance, new[] { args[0] })! ? -1.0 : 0.0);
                    return true;
                }
                // Prima queste istanze erano null (OP_NEW non le creava) oppure il metodo
                // cadeva nel fallback PushRef(null) → no-op silenziosi (NodeCount=0, ...).
                // QUEUE: Enqueue/Dequeue/Peek/Contains/Front/Back — Queue<object?> non è IList/IDict.
                if (instance is System.Collections.Generic.Queue<object?> qInstQ)
                {
                    if (upName == "ENQUEUE" && argCount == 1) { qInstQ.Enqueue(args[0]!); task.PushRef(null); return true; }
                    if (upName == "DEQUEUE" && argCount == 0) { task.PushRef(qInstQ.Count > 0 ? qInstQ.Dequeue() : null); return true; }
                    if (upName == "PEEK" && argCount == 0) { task.PushRef(qInstQ.Count > 0 ? qInstQ.Peek() : null); return true; }
                    if (upName == "CONTAINS" && argCount == 1) { task.PushRef(qInstQ.Contains(args[0]!) ? -1.0 : 0.0); return true; }
                    // F1 (parity VM/C/IL): Front = testa (prossimo da Dequeue), Back = coda.
                    if (upName is "FRONT" or "BACK" && argCount == 0)
                    {
                        if (qInstQ.Count == 0) { task.PushRef(null); return true; }
                        if (upName == "FRONT") { task.PushRef(qInstQ.Peek()); return true; }
                        object? last = null;
                        foreach (var it in qInstQ) last = it;
                        task.PushRef(last);
                        return true;
                    }
                    if (upName == "ISEMPTY" && argCount == 0) { task.PushRef(qInstQ.Count == 0 ? -1.0 : 0.0); return true; }
                }
                // STACK: Push/Pop/Peek/Contains.
                if (instance is System.Collections.Generic.Stack<object?> stkInstS)
                {
                    if (upName == "PUSH" && argCount == 1) { stkInstS.Push(args[0]!); task.PushRef(null); return true; }
                    if (upName == "POP" && argCount == 0) { task.PushRef(stkInstS.Count > 0 ? stkInstS.Pop() : null); return true; }
                    if (upName == "PEEK" && argCount == 0) { task.PushRef(stkInstS.Count > 0 ? stkInstS.Peek() : null); return true; }
                    if (upName == "CONTAINS" && argCount == 1) { task.PushRef(stkInstS.Contains(args[0]!) ? -1.0 : 0.0); return true; }
                    if (upName == "ISEMPTY" && argCount == 0) { task.PushRef(stkInstS.Count == 0 ? -1.0 : 0.0); return true; }
                }
                // HASHSET: Add/Contains/Remove/Union/Intersect/Except — HashSet<object?>
                // non è IList né IDictionary → senza questi case era no-op silenzioso.
                if (instance is System.Collections.Generic.HashSet<object?> hsInst)
                {
                    if (upName == "ADD" && argCount == 1) { hsInst.Add(args[0]!); task.PushRef(null); return true; }
                    if (upName == "CONTAINS" && argCount == 1) { task.PushRef(hsInst.Contains(args[0]!) ? -1.0 : 0.0); return true; }
                    if (upName == "REMOVE" && argCount == 1) { task.PushRef(hsInst.Remove(args[0]!) ? -1.0 : 0.0); return true; }
                    if (upName == "UNION" && argCount == 1 && args[0] is System.Collections.Generic.HashSet<object?> u2)
                    { var r = new System.Collections.Generic.HashSet<object?>(hsInst); r.UnionWith(u2); task.PushRef(r); return true; }
                    if (upName == "INTERSECT" && argCount == 1 && args[0] is System.Collections.Generic.HashSet<object?> i2)
                    { var r = new System.Collections.Generic.HashSet<object?>(hsInst); r.IntersectWith(i2); task.PushRef(r); return true; }
                    if (upName == "EXCEPT" && argCount == 1 && args[0] is System.Collections.Generic.HashSet<object?> e2)
                    { var r = new System.Collections.Generic.HashSet<object?>(hsInst); r.ExceptWith(e2); task.PushRef(r); return true; }
                    if (upName == "SYMMETRICEXCEPT" && argCount == 1 && args[0] is System.Collections.Generic.HashSet<object?> x2)
                    { var r = new System.Collections.Generic.HashSet<object?>(hsInst); r.SymmetricExceptWith(x2); task.PushRef(r); return true; }
                }
                // BAG.Add: il Bag BASIC è un Dictionary<object,long> — Add con 1 argomento
                // incrementa il conteggio (prima: il case IDictionary.Add a 2 argomenti non
                // matchava → Add no-op → Count/UniqueItems vuoti).
                if (methodName != null && methodName.Equals("Add", StringComparison.OrdinalIgnoreCase)
                    && argCount == 1 && instance is System.Collections.IDictionary bagAdd)
                {
                    var key = args[0]!;
                    var cur = bagAdd.Contains(key) && bagAdd[key] is long lv ? lv : 0L;
                    bagAdd[key] = cur + 1;
                    task.PushRef(null);
                    return true;
                }
                // BAG.UniqueItems: il Bag BASIC è un Dictionary<object,long> (conteggi);
                // UniqueItems restituisce le CHIAVI uniche (il metodo C# non esiste —
                // è la semantica BASIC, come UniqueCount = Count del dict).
                if (methodName != null && methodName.Equals("UniqueItems", StringComparison.OrdinalIgnoreCase)
                    && instance is System.Collections.IDictionary bagDict)
                {
                    task.PushRef(bagDict.Keys.Cast<object?>().ToList());
                    return true;
                }
                // RINGBUFFER.Add → CircularQueue.Enqueue (il BASIC usa Add, la classe C#
                // Enqueue — prima Add cadeva nella reflection che non trova Enqueue sotto
                // quel nome → no-op silenzioso, Count restava 0).
                if (methodName != null && methodName.Equals("Add", StringComparison.OrdinalIgnoreCase)
                    && argCount == 1 && instance is CommonLib.Collections.CircularQueue<object> cqAdd)
                {
                    cqAdd.Enqueue(args[0]!);
                    task.PushRef(null);
                    return true;
                }
                // RINGBUFFER.Capacity: CircularQueue espone la proprietà Capacity (non un
                // metodo) — la reflection sotto cerca solo metodi.
                if (methodName != null && methodName.Equals("Capacity", StringComparison.OrdinalIgnoreCase)
                    && instance is CommonLib.Collections.CircularQueue<object> cq)
                {
                    task.PushRef((double)cq.Capacity);
                    return true;
                }
                // RINGBUFFER.Capacity su istanza non tipizzata (generic è object)
                if (methodName != null && methodName.Equals("Capacity", StringComparison.OrdinalIgnoreCase)
                    && instance is not null && instance.GetType().Name.Contains("CircularQueue"))
                {
                    var capProp = instance.GetType().GetProperty("Capacity");
                    if (capProp != null && capProp.GetValue(instance) is int capv) { task.PushRef((double)capv); return true; }
                }

                if (instance is not (System.Collections.IList or System.Collections.IDictionary)
                    && instance is not Basic.IlBackend.PCode.Http.PCodeHttpResponse
                    && instance is not Basic.IlBackend.PCode.Http.PCodeWebSocketHandle
                    && instance is not Basic.IlBackend.PCode.Http.PCodeFetchResult
                    && methodName != null)
                {
                    var instType = instance!.GetType();
                    /* LINKEDLIST/DOUBLYLINKEDLIST: nel P-Code sono
                     * System.Collections.Generic.LinkedList<T> — NON IList/IDictionary, quindi
                     * non entravano né nel dispatcher condiviso né nel blocco CommonLib (il
                     * namespace è System.Collections.Generic): AddFirst/AddLast/Count/First/
                     * Last/Remove finivano nel PushRef(null) = no-op silenziosi (sample
                     * CollectionsLinked: "SLL.Count= 0", liste vuote). Delega al dispatcher
                     * condiviso di IlRuntime (stessa semantica di IL/VM-C/C99). */
                    if (instType.IsGenericType
                        && instType.GetGenericTypeDefinition() == typeof(System.Collections.Generic.LinkedList<>))
                    {
                        task.PushRef(IlRuntime.CollectionInvoke(instance, methodName!, args));
                        return true;
                    }
                    if (instType == null) { task.PushRef(null); return true; }
                    if (instType.Namespace?.StartsWith("CommonLib.Collections") == true ||
                        instType.Name.StartsWith("ConsistentHash") ||
                        instType.Name.StartsWith("Graph") ||
                        instType.Name.StartsWith("Timeline") ||
                        instType.Name.StartsWith("BinaryHeap") ||
                        instType.Name.StartsWith("Cache") ||
                        instType.Name.StartsWith("RedBlackTree") ||
                        instType.Name.StartsWith("BinaryIndexedTree") ||
                        instType.Name.StartsWith("CircularQueue") ||
                        instType.Name.StartsWith("SparseSet") ||
                        instType.Name.StartsWith("Trie") ||
                        instType.Name.StartsWith("BPlusTree"))
                    {
                        /* Timeline/Graph: forme canoniche nel dispatcher condiviso
                         * ({"Key":seriale,"Value":...} e lista {"node","dist"} ordinata). */
                        if (((upName is "FIRST" or "LAST" or "GETRANGE")
                             && instType.Name.StartsWith("Timeline", StringComparison.Ordinal))
                            || (upName == "DIJKSTRA"
                                && instType.Name.StartsWith("Graph", StringComparison.Ordinal)))
                        {
                            task.PushRef(IlRuntime.CollectionInvoke(instance, methodName!, args));
                            return true;
                        }
                        // GRAPH.GetNeighbors(node) → lista di record {node, weight} — parità
                        // col runtime C (gmb_graph_neighbors_val) e con IL/C99/VM-C. La
                        // riflessione generica rendeva l'IEnumerable<KeyValuePair> come lista
                        // di dict VUOTI ([{},{}]).
                        if (upName == "GETNEIGHBORS" && argCount == 1 && instType.Name.StartsWith("Graph", StringComparison.Ordinal)
                            && instType.GetMethod("GetNeighbors", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance) is { } gnef)
                        {
                            var lresN = new System.Collections.Generic.List<object?>();
                            if (gnef.Invoke(instance, new[] { args[0] }) is System.Collections.IEnumerable gneEnum)
                                foreach (var kv in gneEnum)
                                {
                                    var ktN = kv!.GetType();
                                    lresN.Add(new Dictionary<string, object?>
                                    {
                                        ["node"] = ktN.GetProperty("Key")!.GetValue(kv),
                                        ["weight"] = ktN.GetProperty("Value")!.GetValue(kv),
                                    });
                                }
                            task.PushRef(lresN.ToArray());
                            return true;
                        }
                        var meth = instType.GetMethods()
                            .FirstOrDefault(m => m.Name.Equals(methodName, StringComparison.OrdinalIgnoreCase)
                                && !m.IsSpecialName
                                && m.GetParameters().Length >= argCount
                                && (m.GetParameters().Length == argCount ||
                                    m.GetParameters().Skip(argCount).All(p => p.IsOptional)));
                        // Property C# come "metodo" BASIC (es. Graph.NodeCount, Graph.Count):
                        // `g.NodeCount()` e `g.NodeCount` leggono la property via getter.
                        if (meth == null && argCount == 0 && methodName != null)
                        {
                            var prop = instType.GetProperty(methodName,
                                System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                            if (prop != null && prop.CanRead)
                            {
                                var getter = prop.GetGetMethod();
                                if (getter != null)
                                    meth = getter;
                            }
                        }
                        // Alias BASIC → metodo C# (nomi non 1:1 tra le classi CommonLib e il BASIC)
                        if (meth == null && methodName != null)
                        {
                            var aliasTarget = methodName.ToUpperInvariant() switch
                            {
                                "FINDALL" when instType.Name == "Trie" => "FindAllWordsWithPrefix",
                                "STARTSWITH" when instType.Name == "Trie" => "StartsWith",
                                "CONTAINS" when instType.Name == "Trie" => "Contains",
                                "GET" when instType.Name.StartsWith("Cache") => null, // gestito sopra
                                "MIN" or "MAX" or "EXTRACTMAX" or "EXTRACTMIN" or "GET" when instType.Name.StartsWith("BPlusTree") => null, // gestito sopra
                                // F1 (parity IL/VMC): il BASIC chiama GetEdgeWeight, la classe
                                // CommonLib espone TryGetEdgeWeight(from, to, out weight).
                                "GETEDGEWEIGHT" when instType.Name.StartsWith("Graph") => "TryGetEdgeWeight",
                                _ => null
                            };
                            if (aliasTarget != null)
                                meth = instType.GetMethod(aliasTarget,
                                    System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                        }
                        if (meth != null)
                        {
                            try
                            {
                                // Coercizione argomenti: il BASIC passa double/string; i metodi
                                // C# vogliono int/long/etc. Reflection.Invoke con tipo sbagliato
                                // lancia ArgumentException → catch → no-op silenzioso (es.
                                // SparseSet.Add(double) su Add(int)). Si converte ogni argomento
                                // al tipo del parametro.
                                var ps = meth.GetParameters();
                                for (var ai = 0; ai < args.Length && ai < ps.Length; ai++)
                                {
                                    if (args[ai] == null || ps[ai].ParameterType.IsInstanceOfType(args[ai])) continue;
                                    var pt = ps[ai].ParameterType;
                                    if (pt == typeof(int)) args[ai] = (int)SafeDouble(args[ai]);
                                    else if (pt == typeof(long)) args[ai] = (long)SafeDouble(args[ai]);
                                    else if (pt == typeof(short)) args[ai] = (short)SafeDouble(args[ai]);
                                    else if (pt == typeof(byte)) args[ai] = (byte)SafeDouble(args[ai]);
                                    else if (pt == typeof(float)) args[ai] = (float)SafeDouble(args[ai]);
                                    else if (pt == typeof(double)) args[ai] = SafeDouble(args[ai]);
                                    else if (pt == typeof(bool)) args[ai] = SafeDouble(args[ai]) != 0;
                                    else if (pt == typeof(string)) args[ai] = args[ai]?.ToString() ?? "";
                                    else if (pt == typeof(DateTime)) args[ai] = DateTime.FromOADate(SafeDouble(args[ai]));
                                    else if (pt == typeof(DateTimeOffset))
                                        args[ai] = new DateTimeOffset(DateTime.FromOADate(SafeDouble(args[ai])));
                                    else if (pt == typeof(TimeSpan))
                                        args[ai] = System.TimeSpan.FromSeconds(SafeDouble(args[ai]));
                                    else if (pt == typeof(TimeSpan?))
                                        args[ai] = System.TimeSpan.FromSeconds(SafeDouble(args[ai]));
                                }
                                // Se ci sono parametri opzionali non passati dal BASIC, riempili
                                // con null (Reflection.Invoke richiede l'array COMPLETO — i default
                                // dei parametri opzionali non vengono applicati automaticamente).
                                if (args.Length < ps.Length)
                                {
                                    var padded = new object?[ps.Length];
                                    Array.Copy(args, padded, args.Length);
                                    for (var pi = args.Length; pi < ps.Length; pi++)
                                        padded[pi] = ps[pi].IsOptional ? Type.Missing : null;
                                    args = padded;
                                }
                                var res = meth.Invoke(instance, args);
                                // Ritorni comuni → tipi BASIC
                                if (res is bool b) { task.PushRef(b ? -1.0 : 0.0); return true; }
                                if (res is int ii) { task.PushRef((double)ii); return true; }
                                if (res is long ll) { task.PushRef((double)ll); return true; }
                                if (res is double dd) { task.PushRef(dd); return true; }
                                if (res is float ff) { task.PushRef((double)ff); return true; }
                                if (res is string str) { task.PushRef(str); return true; }
                                if (res is System.Collections.IEnumerable en && res is not string)
                                {
                                    var list = new System.Collections.Generic.List<object?>();
                                    foreach (var item in en) list.Add(item);
                                    task.PushRef(list);
                                    return true;
                                }
                                if (res is KeyValuePair<DateTimeOffset, object> kv)
                                {
                                    var pair = new System.Collections.Generic.List<object?> { kv.Key, kv.Value };
                                    task.PushRef(pair);
                                    return true;
                                }
                                task.PushRef(null);
                                return true;
                            }
                            catch
                            {
                                task.PushRef(null);
                                return true;
                            }
                        }
                    }
                }

                // Campo di un dict non risolto dal linker (es. percorso SourcePipeline, che non
                // arricchisce l'AST come Orchestrator): `result.Centroids(0)` con result=dict e
                // Centroids=chiave-array. Parità col percorso Orchestrator (LOAD_MEMBER per ID +
                // LOAD_ELEMENT). Senza questo il membro cadeva nel fallback null qui sotto
                // (PCodeGpmAnalysisSafepoint: FAIL: KMEANS result nel percorso SourcePipeline).
                if (methodName != null && !methodName.StartsWith("__") &&
                    instance is System.Collections.IDictionary memberDict && memberDict.Contains(methodName))
                {
                    var memberValue = memberDict[methodName];
                    if (argCount >= 1 && memberValue is System.Collections.IList memberList)
                    {
                        var memberIdx = Convert.ToInt32(SafeDouble(args[0]));
                        if (memberIdx >= 0 && memberIdx < memberList.Count)
                        {
                            task.PushRef(memberList[memberIdx]);
                            return true;
                        }
                        task.PushRef(null);
                        return true;
                    }
                    task.PushRef(memberValue);
                    return true;
                }

                if (methodAddr.HasValue)
                {
                    var fl = CreateLocals();
                    var isOp = methodName != null && methodName.StartsWith("__");
                    fl[0] = instance;
                    if (isOp) fl[1] = instance;
                    var ao = isOp ? 2 : 1;
                    for (var a = 0; a < argCount; a++)
                        fl[a + ao] = args[a];
                    var methodFrame = new PCodeCallFrame { SavedStackPointer = task.Sp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = fl, ConstructorInstance = null };
                    EnterProcedureErrorScope(task, methodFrame);
                    task.Frame = methodFrame;
                    task.Ip = methodAddr.Value;
                }
                else { task.PushRef(null); }
                return true;
            }
            case OpCode.OP_PUSH_CODE_REF:
            {
                var constIdx = ReadU16(task);
                task.PushRef(constIdx >= 0 && constIdx < _constants.Count ? _constants[constIdx] : null);
                return true;
            }

            // === CONCURRENCY (GPM — task switching) ===
            case OpCode.OP_GO:
            {
                var addr = ReadU16(task);
                if (_scheduler != null)
                    LaunchGoTask(task, addr);
                return true;
            }
            case OpCode.OP_GO_FUNCTION:
            {
                var addr = ReadU16(task);
                if (_scheduler != null)
                {
                    var goTask = LaunchGoTask(task, addr);
                    var future = new PCodeFutureValue();
                    goTask.FutureToResolve = future;
                    task.PushRef(future);
                }
                else task.PushRef(null);
                return true;
            }
            case OpCode.OP_SEND:
            {
                // WaitingReceivers/WaitingSenders sono liste "chi svegliare", NON un canale di
                // trasferimento diretto: il trasferimento passa SEMPRE dal buffer. Un task
                // risvegliato ri-esegue l'opcode da capo (Ip riavvolto) e ritenta l'operazione.
                // TrySendOrPark fa "prova a inviare, altrimenti registrati come in attesa" in UN
                // SOLO lock hold — necessario sotto vero parallelismo: separare le due chiamate
                // (TrySend poi, se fallito, EnqueueSender) lascerebbe una finestra in cui un
                // RECEIVE concorrente libera spazio proprio in mezzo, non trova ancora nessun
                // mittente registrato e non sveglia nessuno — questo mittente si bloccherebbe
                // per sempre (hang intermittente osservato in GoRoutinesWorkers.bas).
                if (task.Sp < 2) return true;
                var ch = task.PopRef() as ChannelInfo;
                var val = task.PopRef();
                if (ch != null)
                {
                    // Prepara lo stato di retry (stack + Ip) PRIMA della chiamata atomica: se
                    // TrySendOrPark decide di parcheggiarci, il task diventa visibile (registrato
                    // in WaitingSenders + State=Blocked, atomicamente) SOLO dopo che Ip/stack sono
                    // già nello stato corretto per una ri-esecuzione — altrimenti un wake
                    // concorrente potrebbe far ripartire il retry su un altro thread PRIMA che
                    // questo stesso thread abbia finito di prepararlo.
                    task.PushRef(val);
                    task.PushRef(ch);
                    task.Ip--; // ri-esegue OP_SEND al risveglio (se effettivamente parcheggiato)
                    if (ch.TrySendOrPark(val, new ChannelSenderWaiter(task, null, null, 0), out var wokenReceiver))
                    {
                        // Inviato con successo: annulla la preparazione di retry appena fatta.
                        task.Sp -= 2;
                        task.Ip++;
                        if (wokenReceiver is { } rcv) rcv.TryWake(_scheduler);
                        return true;
                    }
                    // Parcheggiato: State già impostato a Blocked dentro TrySendOrPark, atomicamente
                    // con la registrazione in WaitingSenders.
                    return false;
                }
                return true;
            }
            case OpCode.OP_RECV:
            {
                // Stesso motivo di OP_SEND: TryReceiveOrPark fa "prova a ricevere, altrimenti
                // registrati come in attesa" in UN SOLO lock hold, per non lasciare la finestra
                // in cui un SEND concorrente riempie il buffer proprio fra un TryReceive fallito
                // e l'EnqueueReceiver separato.
                if (task.Sp <= 0) return true;
                var ch = task.PopRef() as ChannelInfo;
                if (ch != null)
                {
                    // Stessa preparazione-prima-della-visibilità di OP_SEND.
                    task.PushRef(ch);
                    task.Ip--; // ri-esegue OP_RECV al risveglio (se effettivamente parcheggiato)
                    if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rv, out var wokenSender))
                    {
                        task.Sp -= 1;
                        task.Ip++;
                        if (wokenSender is { } snd) snd.TryWake(_scheduler);
                        task.PushRef(rv);
                        return true;
                    }
                    // Parcheggiato: State già impostato a Blocked dentro TryReceiveOrPark.
                    return false;
                }
                return true;
            }
            case OpCode.OP_RECV_OK:
            {
                // RECEIVE v FROM ch OK ok (parità Go v, ok := <-ch): pusha (valore, flag).
                // Flag = 1 se il valore è reale, 0 se il canale è chiuso e svuotato (il runtime
                // pusha null in quel caso). Identico protocollo di OP_RECV, ma al termine pusha
                // due valori: il valore ricevuto e il flag.
                if (task.Sp <= 0) return true;
                var chOk = task.PopRef() as ChannelInfo;
                if (chOk != null)
                {
                    task.PushRef(chOk);
                    task.Ip--; // ri-esegue OP_RECV_OK al risveglio
                    if (chOk.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rvOk, out var wokenSenderOk))
                    {
                        task.Sp -= 1;
                        task.Ip++;
                        if (wokenSenderOk is { } sndOk) sndOk.TryWake(_scheduler);
                        task.PushRef(rvOk);
                        task.PushRef(rvOk != null ? 1.0 : 0.0);
                        return true;
                    }
                    return false;
                }
                return true;
            }
            case OpCode.OP_WG_WAIT:
            {
                // VisitWaitStatement non consuma alcun risultato dopo OP_WG_WAIT — non va
                // ripushato il WaitGroup sullo stack (bug precedente: veniva ripushato
                // incondizionatamente e mai più poppato da nessuno, un vero leak che cresce
                // a ogni WAIT in un loop). Niente rewind di Ip: a differenza di RECV/SEND, chi
                // sblocca (DoneAndWake) sa già che deve svegliare — nessun retry necessario.
                var wgVal = task.Sp > 0 ? task.PopRef() : null;
                // ShouldWait imposta anche State=Blocked, atomicamente con la registrazione in
                // _waiters (vedi PCodeWaitGroup) — stesso motivo di TrySendOrPark/TryReceiveOrPark.
                if (wgVal is PCodeWaitGroup pwg && pwg.ShouldWait(task))
                {
                    return false;
                }
                if (wgVal is not PCodeWaitGroup)   // nil O numero (il compilatore spinge un valore segnaposto per la WAIT nuda)
                {
                    // WAIT nuda (senza gruppo): attende i figli lanciati con GO SUB senza
                    // IN (gruppo implicito del task); senza figli mai lanciati resta no-op
                    // come prima (bug 2026-10-01). I figli con IN restano di competenza
                    // del loro gruppo esplicito.
                    var imp = Basic.Runtime.Gpm.GpmImplicitChildren.GetGroup(task);
                    if (imp is not null && imp.ShouldWait(task))
                    {
                        return false;
                    }
                }
                return true;
            }
            case OpCode.OP_WG_DONE:
            {
                var wg = task.Sp > 0 ? task.PopRef() : null;
                if (wg is PCodeWaitGroup pwg)
                {
                    pwg.DoneAndWake(_scheduler);
                    // Se `wg` è lo stesso WaitGroup registrato su task.WaitGroup (GO SUB ... IN
                    // wg), lo azzera per disattivare il Done IMPLICITO di fine-task
                    // (HandlePostTick) — senza questo, un DONE esplicito + quello implicito
                    // decrementano due volte per una singola goroutine (stesso bug del backend
                    // IL, stessa causa: entrambi condividono PCodeWaitGroup/task.WaitGroup —
                    // Bugs/(open) - WaitGroup-Wait-Non-Attende-Tutte-Goroutine-Multiple.md).
                    if (ReferenceEquals(task.WaitGroup, pwg)) task.WaitGroup = null;
                }
                return true;
            }
            case OpCode.OP_CHANNEL_CLOSE:
            {
                // Parità Go close(ch): chiude il canale atomicamente. I receiver in attesa
                // vengono svegliati con flag ok=0; i sender in attesa ricevono errore. Un
                // doppio close (Close() ritorna false) produce un errore BASIC catturabile.
                if (task.Sp <= 0) return true;
                var chClose = task.PopRef() as ChannelInfo;
                if (chClose != null)
                {
                    if (!chClose.Close())
                        task.PendingException =
                            new InvalidOperationException("close su canale già chiuso");
                }
                return true;
            }
            case OpCode.OP_NEW_MUTEX:
            {
                // Parità Go sync.Mutex: pusha un nuovo mutex non detenuto.
                task.PushRef(new Basic.Runtime.Gpm.PCodeMutex());
                return true;
            }
            case OpCode.OP_NEW_RWMUTEX:
            {
                // Parità Go sync.RWMutex: pusha un nuovo RWMUTEX non detenuto.
                task.PushRef(new Basic.Runtime.Gpm.PCodeRwMutex());
                return true;
            }
            case OpCode.OP_LOCK:
            {
                // Parità Go mu.Lock() (sync.Mutex) / rw.Lock() (sync.RWMutex, scrittore):
                // acquisisce il lock. Se già detenuto, parcheggia il task (State=Blocked,
                // registrazione atomica) finché UNLOCK lo sveglia.
                if (task.Sp <= 0) return true;
                var lockedObj = task.PopRef();
                if (lockedObj is not (Basic.Runtime.Gpm.PCodeMutex or Basic.Runtime.Gpm.PCodeRwMutex))
                    return true;
                task.PushRef(lockedObj);
                task.Ip--; // ri-esegue OP_LOCK al risveglio
                bool acquired = lockedObj switch
                {
                    Basic.Runtime.Gpm.PCodeMutex mu => mu.TryLock(task),
                    Basic.Runtime.Gpm.PCodeRwMutex rw => rw.TryLock(task),
                    _ => true
                };
                if (acquired)
                {
                    task.Sp -= 1;
                    task.Ip++;
                    return true;
                }
                return false; // parcheggiato: State già Blocked dentro TryLock
            }
            case OpCode.OP_UNLOCK:
            {
                // Parità Go mu.Unlock(): rilascia il mutex e sveglia al più un waiter.
                if (task.Sp <= 0) return true;
                var muUnlock = task.PopRef();
                switch (muUnlock)
                {
                    case Basic.Runtime.Gpm.PCodeMutex mu:
                        mu.Unlock(_scheduler);
                        break;
                    case Basic.Runtime.Gpm.PCodeRwMutex rw:
                        rw.Unlock(_scheduler);
                        break;
                }
                return true;
            }
            case OpCode.OP_COND_WAIT:
            {
                // Parità Go c.Wait(): lo stack è [cond, mutex] (cond pushato per primo dal compiler).
                // Rilascia il mutex e parcheggia il task finché un SIGNAL/BROADCAST. NON riavvolge
                // Ip: il compiler emette un OP_LOCK subito dopo, che al wake riacquisisce il mutex.
                if (task.Sp < 2) return true;
                var condMu = task.PopRef() as Basic.Runtime.Gpm.PCodeMutex;
                var condObj = task.PopRef() as Basic.Runtime.Gpm.PCodeCond;
                if (condObj == null || condMu == null) return true;
                condMu.Unlock(_scheduler);
                condObj.TryWait(task);
                return false; // parcheggiato
            }
            case OpCode.OP_COND_SIGNAL:
            {
                if (task.Sp <= 0) return true;
                var signalCond = task.PopRef() as Basic.Runtime.Gpm.PCodeCond;
                if (signalCond != null) signalCond.Signal(_scheduler);
                return true;
            }
            case OpCode.OP_COND_BROADCAST:
            {
                if (task.Sp <= 0) return true;
                var bcCond = task.PopRef() as Basic.Runtime.Gpm.PCodeCond;
                if (bcCond != null) bcCond.Broadcast(_scheduler);
                return true;
            }
            case OpCode.OP_RLOCK:
            {
                // Parità Go mu.RLock() (sync.RWMutex): lock in LETTURA condiviso tra lettori;
                // parcheggia se un scrittore è attivo o in attesa (writer-preference).
                if (task.Sp <= 0) return true;
                var rwObj = task.PopRef() as Basic.Runtime.Gpm.PCodeRwMutex;
                if (rwObj == null) return true;
                task.PushRef(rwObj);
                task.Ip--; // ri-esegue OP_RLOCK al risveglio
                if (rwObj.TryRLock(task))
                {
                    task.Sp -= 1;
                    task.Ip++;
                    return true;
                }
                return false; // parcheggiato
            }
            case OpCode.OP_RUNLOCK:
            {
                // Parità Go mu.RUnlock() (sync.RWMutex): rilascia il lock in lettura.
                if (task.Sp <= 0) return true;
                var rwUnlock = task.PopRef() as Basic.Runtime.Gpm.PCodeRwMutex;
                if (rwUnlock != null)
                    rwUnlock.RUnlock(_scheduler);
                return true;
            }
            case OpCode.OP_NEW_ONCE:
            {
                // Parità Go sync.Once: pusha un nuovo Once non eseguito.
                task.PushRef(new Basic.Runtime.Gpm.PCodeOnce());
                return true;
            }
            case OpCode.OP_ONCE_BEGIN:
            {
                // Parità Go once.Do(): il primo chiamante esegue la procedura; i chiamanti
                // concorrenti parcheggiano finché il primo completa; i successivi saltano.
                if (task.Sp <= 0) return true;
                var skipAddr = ReadU16(task);
                var onceObj = task.PopRef() as Basic.Runtime.Gpm.PCodeOnce;
                if (onceObj == null) return true;
                switch (onceObj.TryBegin(task))
                {
                    case Basic.Runtime.Gpm.PCodeOnce.OnceBeginResult.Started:
                        task.PendingOnce = onceObj;   // OP_ONCE_END lo completa
                        return true;
                    case Basic.Runtime.Gpm.PCodeOnce.OnceBeginResult.Done:
                        task.Ip = skipAddr;           // già eseguito: salta la procedura
                        return true;
                    default: // Parked
                        // ri-esegue OP_ONCE_BEGIN al wake (stato ora Done → skip).
                        task.PushRef(onceObj);
                        task.Ip -= 3;
                        return false;
                }
            }
            case OpCode.OP_ONCE_END:
            {
                // Completa il Once (segna Done) e sveglia tutti i waiter parcheggiati.
                if (task.PendingOnce is { } po)
                {
                    task.PendingOnce = null;
                    po.Complete(_scheduler);
                }
                return true;
            }
            case OpCode.OP_NEW_ATOMIC:
            {
                // Parità Go sync/atomic: pusha un nuovo box ATOMIC a 0.
                task.PushRef(new Basic.Runtime.Gpm.PCodeAtomic());
                return true;
            }
            case OpCode.OP_NEW_POOL:
            {
                // Parità Go sync.Pool: pusha un nuovo POOL vuoto.
                task.PushRef(new Basic.Runtime.Gpm.PCodePool());
                return true;
            }
            case OpCode.OP_NEW_COND:
            {
                // Parità Go sync.Cond: pusha una nuova variabile di condizione.
                task.PushRef(new Basic.Runtime.Gpm.PCodeCond());
                return true;
            }
            case OpCode.OP_NEW_CONCURRENTMAP:
            {
                // Parità Go sync.Map: pusha una mappa concorrente vuota.
                task.PushRef(new Basic.Runtime.Gpm.PCodeConcurrentMap());
                return true;
            }
            case OpCode.OP_DEFER:
            {
                // Parità Go defer: registra l'indirizzo del body nel frame corrente (LIFO).
                // A fine procedura (OP_RETURN) i body vengono eseguiti in ordine inverso.
                // Goroutine radice (frame null): storage task-level (task.DeferredIps).
                var deferAddr = ReadU16(task);
                if (task.Frame is { } deferFrame)
                {
                    deferFrame.DeferredIps ??= new System.Collections.Generic.Stack<int>();
                    deferFrame.DeferredIps.Push(deferAddr);
                }
                else
                {
                    task.DeferredIps ??= new System.Collections.Generic.Stack<int>();
                    task.DeferredIps.Push(deferAddr);
                }
                return true;
            }
            case OpCode.OP_DEFER_RET:
            {
                // Torna all'uscita della procedura: continua a svuotare i DEFER LIFO o completa
                // il RETURN. Vedi l'iniezione in OP_RETURN (task.PendingDeferReturn).
                return ExecuteNextDeferredOrCompleteReturn(task);
            }
            case OpCode.OP_NEW_WAITGROUP:
            {
                task.PushRef(new PCodeWaitGroup());
                return true;
            }
            case OpCode.OP_NEW_CHANNEL:
            {
                // Parità Go make(chan T, n): operando u8 = capacity. 0 = unbuffered (handshake Go,
// implementato come slot di handshake: il buffer fisico accetta 1 valore in transito
// finché il receiver non lo preleva — il sender del valore successivo parcheggia).
                var capacity = ReadU8(task);
                task.PushRef(new ChannelInfo(capacity));
                return true;
            }
            case OpCode.OP_SLEEP:
            {
                if (task.Sp <= 0) return true;
                var ms = PopInt(task);
                if (ms == 0)
                {
                    task.YieldRequested = true;
                    return true;
                }
                if (ms < 0) return true;
                // La deadline compete atomicamente con cancel tramite la stessa registration
                // generazionale di rete/grafica: un solo winner puo' risvegliare il PCB.
                var wait = task.BeginWait(PCodeWaitKind.Timer, () => task.SleepUntilMs = 0);
                task.ParkUntil(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + (long)ms, wait);
                return false;
            }
            // === GO SELECT ===
            case OpCode.OP_GO_SELECT_SETUP:
            {
                var startIp = task.Ip - 1; // posizione dell'opcode
                var numCases = ReadU16(task);
                var numSends = ReadU16(task);
                var defaultOffset = ReadU16(task);

                // Leggi indirizzi di ogni CASE
                var caseAddrs = new int[numCases];
                for (var i = 0; i < numCases; i++)
                    caseAddrs[i] = ReadU16(task);

                var numRecvs = numCases - numSends;

                // Stack: [send_val_0 ... send_val_S-1] [channel_0 ... channel_C-1]
                var baseSp = task.Sp - numCases - numSends;

                if (baseSp >= 0)
                {
                    // Un producer/consumer ha già vinto il CAS del SELECT parcheggiato:
                    // rispetta quella decisione prima della scansione ordinaria. Il valore/slot
                    // resta nel canale secondo il protocollo retry corrente; se una terza parte
                    // lo ha consumato nel frattempo, abbandona la preferenza e riprova tutti i case.
                    if (task.PendingSelect is { WinningCaseIndex: >= 0 } resumedSelect)
                    {
                        var winningCase = resumedSelect.WinningCaseIndex;
                        task.PendingSelect = null;
                        if (winningCase < numCases)
                        {
                            var winningChannelIndex = baseSp + numSends + winningCase;
                            var winningChannel = winningChannelIndex >= 0 && winningChannelIndex < task.Sp &&
                                                 task.Stack[winningChannelIndex].Type == 2
                                ? task.Stack[winningChannelIndex].Ref as ChannelInfo
                                : null;
                            if (winningChannel != null)
                            {
                                if (winningCase >= numSends && winningChannel.TryReceiveWake(out var received, out var senderToWake))
                                {
                                    if (senderToWake != null) _scheduler?.WakeTask(senderToWake);
                                    task.Sp -= numCases + numSends;
                                    task.PushRef(received);
                                    task.Ip = caseAddrs[winningCase];
                                    return true;
                                }
                                if (winningCase < numSends)
                                {
                                    var winningValueSlot = task.Stack[baseSp + winningCase];
                                    var winningValue = winningValueSlot.Type == 2
                                        ? winningValueSlot.Ref
                                        : (object)winningValueSlot.Num;
                                    if (winningChannel.TrySendWake(winningValue, out var receiverToWake))
                                    {
                                        if (receiverToWake != null) _scheduler?.WakeTask(receiverToWake);
                                        task.Sp -= numCases + numSends;
                                        task.Ip = caseAddrs[winningCase];
                                        return true;
                                    }
                                }
                            }
                        }
                    }

                    // Prova RECV su tutti i canali
                    // I canali sono in posizione baseSp + numSends + j
                    // I primi numSends canali sono SEND, i successivi RECV
                    for (var j = 0; j < numCases; j++)
                    {
                        var chIdx = baseSp + numSends + j;
                        if (chIdx >= task.Sp) continue;
                        var ch = task.Stack[chIdx].Type == 2
                            ? task.Stack[chIdx].Ref as ChannelInfo
                            : null;
                        if (ch == null)
                        {
                            // Due casi distinti in uno slot non-canale:
                            //  - canale NIL (Type==2, Ref==null): parità Go select su canale
                            //    nil → case DISABILITATO per sempre (mai pronto, non blocca,
                            //    mai scelto) — solo gli altri case possono vincere.
                            //  - case TIMEOUT (slot numerico o marcato StackBasicType.Date):
                            //    la DEADLINE assoluta (ms epoch, sovrascritta dal park e marcata
                            //    Date). Solo in quel caso è pronto se now >= deadline. Al primo
                            //    passaggio lo slot è la durata RELATIVA (es. 200 ms): non ancora
                            //    scaduta — la conversione a deadline avviene al park. Senza il
                            //    marcatore, `now >= 200` sarebbe SEMPRE vero (epoch ~1.7e12) e
                            //    il TIMEOUT scattava subito.
                            if (task.Stack[chIdx].Type == 2 && task.Stack[chIdx].Ref == null)
                                continue; // canale nil: case disabilitato
                            var timeoutDeadline = SlotNumericValue(task.Stack[chIdx]);
                            if (task.Stack[chIdx].BasicType == (byte)StackBasicType.Date &&
                                timeoutDeadline > 0 && DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() >= timeoutDeadline)
                            {
                                task.Sp -= numCases + numSends;
                                task.PushRef(null);
                                task.Ip = caseAddrs[j];
                                task.PendingSelect = null;
                                return true;
                            }
                            continue;
                        }
                        var isRecv = j >= numSends;
                        if (isRecv && ch.TryReceive(out var rv))
                        {
                            // Il buffer si è liberato di uno slot: sveglia un mittente in
                            // attesa, se c'è (stesso wake-only pattern di OP_RECV).
                            if (ch.TryDequeueSender(out var freedSnd))
                                freedSnd!.TryWake(_scheduler);
                            task.Sp -= numCases + numSends;
                            task.PushRef(rv);
                            task.Ip = caseAddrs[j];
                            task.PendingSelect = null;
                            // Esegue il corpo del case NELLO STESSO tick (come il ramo SEND
                            // sotto, e come OP_RECV normale) — niente yield extra qui: nel
                            // modello cooperativo un yield in più non "da' tempo" a nessuno,
                            // regala solo un turno di scheduling non necessario al produttore
                            // fra "trovato il dato" e "consumato", che può fargli inviare un
                            // secondo valore prima che questo SELECT abbia finito il suo loop.
                            return true;
                        }
                    }

                    // Prova SEND
                    for (var j = 0; j < numSends && j < numCases; j++)
                    {
                        var chIdx = baseSp + numSends + j;
                        if (chIdx >= task.Sp) continue;
                        var sendCh = task.Stack[chIdx].Ref as ChannelInfo;
                        if (sendCh == null) continue;
                        var sendValIdx = baseSp + j;
                        var sendVal = sendValIdx >= 0 && sendValIdx < task.Sp
                            ? (task.Stack[sendValIdx].Type == 2
                                ? task.Stack[sendValIdx].Ref
                                : (object)task.Stack[sendValIdx].Num)
                            : null;
                        if (sendCh.TrySend(sendVal))
                        {
                            if (sendCh.TryDequeueReceiver(out var rcv))
                                rcv!.TryWake(_scheduler);
                            task.Sp -= numCases + numSends;
                            task.Ip = caseAddrs[j];
                            task.PendingSelect = null;
                            return true;
                        }
                    }

                    // DEFAULT: esegui immediatamente. Sentinel 0xFFFF = "nessun DEFAULT nel
                    // sorgente" (Compiler.GoRoutines.cs) — endAddr, usato prima come sentinel
                    // implicito, è sempre > 0 e faceva sì che un SELECT senza DEFAULT non
                    // bloccasse mai (bug: eseguiva sempre il ramo "default" verso endAddr).
                    if (defaultOffset != 0xFFFF)
                    {
                        task.Sp -= numCases + numSends;
                        task.Ip = defaultOffset;
                        task.PendingSelect = null;
                        return true;
                    }
                }

                // Nulla pronto e no DEFAULT: blocca su tutti i canali (RECV e SEND).
                var selectContext = new SelectContext();
                task.PendingSelect = selectContext;
                // Una sola registration generazionale possiede l'intera SELECT: tutti i case
                // condividono lo stesso winner e cancel stacca ogni iscrizione tramite il
                // SelectContext. Deve esistere PRIMA di rendere il primo waiter visibile, perché
                // un producer/consumer concorrente può vincere già dentro Try*OrPark.
                var selectWait = task.BeginWait(PCodeWaitKind.Select, selectContext.Cancel);
                // CRITICO: NON rimuovere gli operandi dallo stack qui. Al risveglio il task
                // ri-esegue OP_GO_SELECT_SETUP (task.Ip = startIp) e l'handler ricalcola
                // baseSp = task.Sp - numCases - numSends: se gli operandi fossero già stati
                // rimossi, baseSp diventerebbe negativo, il blocco `if (baseSp >= 0)` verrebbe
                // saltato e il task si ri-parcheggerebbe per sempre SENZA MAI controllare il
                // buffer — deadlock deterministico quando un SELECT si blocca prima che il
                // mittente invii (riprodotto con WAIT nel producer: il valore resta nel buffer
                // e nessuno lo consuma). Stesso pattern di OP_SEND/OP_RECV, che ripushano gli
                // operandi e riavvolgono l'Ip PRIMA di parcheggiarsi (TrySendOrPark/TryReceiveOrPark).
                // PARK ATOMICO per canale: il vecchio pattern "EnqueueReceiver/EnqueueSender"
                // separato dal check del buffer lasciava la finestra in cui un SEND concorrente
                // riempiva il buffer proprio fra la scansione fallita e la registrazione — il
                // valore restava nel buffer e il task restava bloccato per sempre (nessun wake:
                // il SEND era già passato). TryReceiveOrPark/TrySendOrPark chiudono la finestra:
                // se nel frattempo un valore/slot è arrivato, completano SUBITO il case invece
                // di parcheggiarsi. (Fix congiunto: senza il ripristino degli operandi qui sopra,
                // anche il wake corretto ri-eseguiva il SETUP su uno stack senza operandi e si
                // ri-parcheggiava — deadlock doppio, coperto dal canarino GoSelectParkWake.)
                for (int j = 0; j < numCases; j++)
                {
                    var chIdx = baseSp + numSends + j;
                    if (chIdx < 0 || chIdx >= task.Sp) continue;
                    var pch = task.Stack[chIdx].Type == 2 ? task.Stack[chIdx].Ref as ChannelInfo : null;
                    if (pch == null)
                    {
                        // Canale NIL (Type==2, Ref==null): case DISABILITATO (parità Go select
                        // su canale nil) — non parcheggia mai, solo gli altri case possono
                        // vincere. Distinto dal TIMEOUT (slot numerico) qui sotto.
                        if (task.Stack[chIdx].Type == 2 && task.Stack[chIdx].Ref == null)
                            continue;
                        // Case TIMEOUT (parità Go case <-time.After(d)): lo slot contiene la
                        // durata in ms. Park del task con lo STESSO contratto timer di SLEEP
                        // (ParkUntil imposta SleepUntilMs + registra il timer): senza SleepUntilMs,
                        // EarliestSleepDeadline scarta il timer e il watcher dorme per sempre
                        // (deadlock nel case solo-TIMEOUT). Alla scadenza il task ri-esegue il
                        // SETUP e trova il case pronto. Lo slot viene sovrascritto con la
                        // DEADLINE ASSOLUTA (ms epoch) per il retry.
                        var dur = SlotNumericValue(task.Stack[chIdx]);
                        if (dur > 0)
                        {
                            var deadline = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + (long)dur;
                            task.Stack[chIdx] = new StackEntry { Num = deadline, Type = 1, BasicType = (byte)StackBasicType.Date };
                            task.ParkUntil(deadline, selectWait);
                        }
                        continue;
                    }
                    if (!selectContext.IsPending) break;
                    selectContext.RegisterChannel(pch);
                    if (j >= numSends)
                    {
                        if (pch.TryReceiveOrPark(
                                new ChannelReceiverWaiter(task, selectContext, j, selectWait),
                                out var rv, out var wokenSender))
                        {
                            // Razza colpita: valore arrivato fra scan e park — completa SUBITO.
                            if (wokenSender is { } ws) ws.TryWake(_scheduler);
                            task.Sp -= numCases + numSends;
                            task.PushRef(rv);
                            task.Ip = caseAddrs[j];
                            task.State = PCodeTaskState.Running; // un park su canali precedenti l'aveva messo Blocked
                            task.PendingSelect = null;
                            // TryReceiveOrPark ha già rivendicato atomicamente il case PRIMA
                            // di consumare il valore.
                            selectContext.DetachLosers();
                            // Il case è diventato pronto mentre QUESTO worker esegue ancora il
                            // setup: non serve un wake. Chiudi la registration e prosegui nello
                            // stesso tick, evitando un WakePending che rieseguirebbe il PCB.
                            selectWait.Cancel();
                            task.ClearPendingWaitCancellation();
                            return true;
                        }
                        // Parcheggiato atomicamente su questo canale: continua sugli altri.
                    }
                    else
                    {
                        // j < numSends → canale SEND; il valore da inviare è a baseSp + j
                        var sendSlot = task.Stack[baseSp + j];
                        var sendVal = sendSlot.Type == 2 ? sendSlot.Ref : (object)sendSlot.Num;
                        if (pch.TrySendOrPark(sendVal,
                                new ChannelSenderWaiter(task, sendVal, selectContext, j, selectWait),
                                out var wokenReceiver))
                        {
                            // Razza colpita: slot liberato fra scan e park — completa SUBITO.
                            if (wokenReceiver is { } rcv) rcv.TryWake(_scheduler);
                            task.Sp -= numCases + numSends;
                            task.Ip = caseAddrs[j];
                            task.State = PCodeTaskState.Running;
                            task.PendingSelect = null;
                            // TrySendOrPark ha già rivendicato atomicamente il case PRIMA
                            // di pubblicare il valore.
                            selectContext.DetachLosers();
                            selectWait.Cancel();
                            task.ClearPendingWaitCancellation();
                            return true;
                        }
                        // Parcheggiato atomicamente su questo canale: continua sugli altri.
                    }
                }
                task.Ip = startIp;
                task.State = PCodeTaskState.Blocked;
                return false;
            }
            case OpCode.OP_NEXT_GENERATOR:
            {
                // Punto di sospensione per YIELD (VisitYieldStatement, Compiler.Others.cs). Il
                // valore da restituire e' gia' sullo stack (VisitExpression(stmt.ValueToYield)
                // compilato subito prima). Salva il valore in PendingResult e restituisce false
                // per interrompere IMMEDIATAMENTE il ciclo di dispatch — sia che sia
                // DriveGeneratorTask (NEXT esplicito) sia il ciclo normale di ExecuteTaskTick (se
                // mai eseguito per errore fuori da un generatore, vedi guardia in
                // ExecuteYieldStep-equivalente lato GPM). task.Ip punta già alla PROSSIMA
                // istruzione (incrementato da ExecuteTaskTick/DriveGeneratorTask prima del
                // dispatch): riprendere da lì al prossimo NEXT() continua correttamente subito
                // DOPO lo YIELD, preservando locals/ForState (nessun frame da ripristinare, il
                // generator task non ne ha mai avuto uno — vedi OP_NEW_GENERATOR_CALL).
                var yieldedVal = task.Sp > 0 ? task.PopRef() : null;
                task.PendingResult = yieldedVal;
                return false;
            }
            case OpCode.OP_RESOLVE_FUTURE:
            {
                // Il future è già sullo stack (VisitExpression(expr.FutureToResolve), emesso
                // subito prima da VisitResolveExpression) — PeekTop, non Pop: se non è ancora
                // risolto dobbiamo rewindare l'Ip per ri-eseguire QUESTO opcode al risveglio, e
                // il valore deve essere ancora lì quando riparte (stesso motivo del rewind
                // PRIMA di RegisterWaiter in OP_LOAD_GLOBAL/OP_LOAD_LOCAL — RegisterWaiter può
                // svegliare il task concorrentemente).
                var top = task.PeekTop();
                if (top is PCodeFutureValue fut)
                {
                    task.Ip -= 1; // opcode(1), nessun operando: ri-esegue OP_RESOLVE_FUTURE
                    if (fut.RegisterWaiter(task))
                    {
                        return false;
                    }
                    task.Ip += 1; // già risolto: annulla il rewind
                    task.PopRef(); // ora consuma il future dallo stack
                    if (fut.FailureException != null) { PropagateFutureFailure(task, fut); return true; }
                    task.PushRef(fut.Result);
                    return true;
                }
                // Non è un future (già un valore semplice): RESOLVE è l'identità, lascialo sullo stack.
                return true;
            }

            // === MATRIX COMMANDS ===
            case OpCode.OP_MATRIX_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                var mtx = ExecuteMatrixOp(task, subCode, args);
                if (ReferenceEquals(mtx, ParkedMatrixResult))
                    return true;
                if (mtx != null) task.PushRef(mtx);
                return true;
            }
            case OpCode.OP_VECTOR_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                var vec = ExecuteVectorOp(subCode, task, argCount);
                if (vec != null) task.PushRef(vec);
                return true;
            }

            // === NETWORK COMMANDS (TCP/UDP/SSL) ===
            case OpCode.OP_NET_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                var result = ExecuteNetCmd(task, subCode, args);
                if (ReferenceEquals(result, ParkedNetResult))
                    return true;
                if (result != null) task.PushRef(result);
                return true;
            }

            // === CLIPBOARD COMMANDS ===
            case OpCode.OP_CLIP_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                var clipResult = ExecuteClipCmd(subCode, args);
                if (clipResult != null) task.PushRef(clipResult);
                return true;
            }

            // === AUDIO COMMANDS (SOUND/PLAY/FILTER → AudioManager) ===
            case OpCode.OP_AUDIO_CMD:
            {
                var subCode = ReadU8(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                ExecuteAudioCmd(subCode, args);
                return true;
            }

            // === FFI CALL ===
            case OpCode.OP_CALL_FFI:
            {
                var libIdx = ReadU16(task);
                var nameIdx = ReadU16(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                // Stage C: le matrici a descrittore vanno marshallate all'ABI nativa come array
                // rettangolare (FfiService pinna System.Array). I parametri OUT/INOUT matrice
                // non sono usati dall'ABI reale (il contratto F3 li ammette solo in firma).
                for (var i = 0; i < args.Length; i++)
                    if (args[i] is CommonLib.MatrixValue ffiMatrix) args[i] = ffiMatrix.ToRect();
                var libPath = libIdx >= 0 && libIdx < _constants.Count ? _constants[libIdx] as string : null;
                var funcName = nameIdx >= 0 && nameIdx < _constants.Count ? _constants[nameIdx] as string : null;
                if (libPath != null && funcName != null)
                {
                    try
                    {
                        var handle = _runtime.Ffi.LoadLibrary(libPath);
                        var funcPtr = _runtime.Ffi.GetFunction(handle, funcName, libPath);
                        var result = _runtime.Ffi.CallFunction(funcPtr, args);
                        task.PushRef(result);
                    }
                    catch (Exception ex)
                    {
                        /* Simbolo/libreria FFI non risolti: errore BASIC ESPLICITO e catturabile
                         * (ON ERROR/TRY), codice 5 = IllegalFunctionCall come VM/C e C99Native.
                         * Prima si spingeva la stringa "[FFI_ERROR] ..." sullo stack come se
                         * fosse il risultato: nessun errore, out() intatti e valore credibile. */
                        var code = ex is BasicException be ? (double)be.Code
                                 : ex is FileNotFoundException ? 53.0   /* libreria assente */
                                 : 5.0;                                 /* simbolo assente */
                        RaiseBasicError(task, code, ex.Message);
                    }
                }
                else { task.PushRef(null); }
                return true;
            }

            // === FFI CALL (tipizzata: kind omogeneo 0=double, 1=int64) ===
            case OpCode.OP_CALL_FFI_T:
            {
                // Chunk v1 (pre-2026-11-06): il formato FFI era a maschere, non a descrittore.
                // Rifiuto ESPLICITO invece di un parse silenziosamente sbagliato.
                if (_chunkFormatVersion < 2)
                    throw new InvalidOperationException(
                        "FFI: chunk v1 (formato precedente al 2026-11-06) con chiamata FFI: ricompilare il sorgente.");
                var libIdx = ReadU16(task);
                var nameIdx = ReadU16(task);
                var argCount = ReadU16(task);
                var kindByte = ReadU8(task);
                var retIsString = ReadU8(task) != 0;
                var retLo = ReadU8(task);
                var retHi = ReadU8(task);
                // S1 (2026-11-06): bit7 di `kind` = ritorno UDT con layout esplicito (i due byte
                // successivi sono il conteggio foglie u16). Senza il bit resta il formato storico.
                var retUdtNew = (kindByte & 0x80) != 0;
                // bit 0x20 di `kind` = DECLARE NATIVE (ABI esatta: nessuna estensione automatica degli array).
                var nativeAbi = (kindByte & 0x20) != 0;
                var kind = kindByte & 0x1F;   // 0x20 = NATIVE, 0x40 = tabella firme callback, 0x80 = ritorno UDT
                var retUdtFieldCount = retUdtNew ? (retLo | (retHi << 8)) : retLo;
                var retUdtFieldKindMask = retUdtNew ? 0 : retHi;
                // Descrittore PER ARGOMENTO (2026-11-06, niente maschere nel formato):
                // 3 byte per argomento (flags, aux1, aux2). Le maschere sotto sono una
                // rappresentazione interna per il dispatch <= 32; non fanno parte del bytecode.
                if (argCount > PCodeFfiMaxArgs)
                    throw new InvalidOperationException($"FFI: troppi argomenti ({argCount}, max {PCodeFfiMaxArgs})");
                /* Maschere a 64 bit (2026-11-06): con `int` e `1 << pi` il 33-esimo
                 * argomento (pi = 32) riavvolgeva sul bit 0 → "argomento 0 atteso array".
                 * Il resto del path e' gia' a 64 bit (IL `long`, VM/C `uint64_t`). */
                long scalarKindMask = 0, arrayMask = 0, arrayElemIntMask = 0, stringMask = 0;
                long byrefScalarMask = 0, byrefScalarElemIntMask = 0;
                long callbackParamKindMask = 0, callbackParamHasRetMask = 0;
                var udtParamFieldCount = new int[PCodeFfiMaxArgs];
                var udtParamFieldKindMask = new int[PCodeFfiMaxArgs];
                var udtParamNew = new bool[PCodeFfiMaxArgs];
                var udtArrCount = new int[PCodeFfiMaxArgs];
                long byrefUdtMask = 0;
                var callbackParamArity = new int[PCodeFfiMaxArgs];
                for (var pi = 0; pi < PCodeFfiMaxArgs; pi++) callbackParamArity[pi] = 255;
                for (var pi = 0; pi < argCount; pi++)
                {
                    int f = ReadU8(task), a1 = ReadU8(task), a2 = ReadU8(task);
                    if ((f & 1) != 0) scalarKindMask |= 1L << pi;
                    if ((f & 2) != 0) arrayMask |= 1L << pi;
                    if ((f & 0x40) != 0) arrayElemIntMask |= 1L << pi;
                    if ((f & 4) != 0) stringMask |= 1L << pi;
                    if ((f & 8) != 0)
                    {
                        // BYREF UDT (2026-11-06): bit 0x20 = UDT → puntatore a struct.
                        if ((f & 0x20) != 0) byrefUdtMask |= 1L << pi;
                        else
                        {
                            byrefScalarMask |= 1L << pi;
                            if ((f & 0x80) != 0) byrefScalarElemIntMask |= 1L << pi;
                        }
                    }
                    if ((f & 0x10) != 0)
                    {
                        callbackParamArity[pi] = a1;
                        callbackParamKindMask |= (long)(a2 & 1) << pi;
                        callbackParamHasRetMask |= (long)((a2 >> 1) & 1) << pi;
                    }
                    if ((f & 2) != 0 && (f & 0x80) != 0)
                    {
                        // Array di UDT (S3): aux = conteggio foglie dell elemento
                        udtArrCount[pi] = a1 | (a2 << 8);
                    }
                    if ((f & 0x20) != 0)
                    {
                        if ((f & 0x80) != 0) { udtParamNew[pi] = true; udtParamFieldCount[pi] = a1 | (a2 << 8); }
                        else { udtParamFieldCount[pi] = a1; udtParamFieldKindMask[pi] = a2; }
                    }
                }
                Basic.Runtime.FfiUdtShape? ReadUdtShape(int fieldCount, byte kindMask, bool newLayout)
                {
                    var names = new string[fieldCount];
                    var symbolIdPaths = new int[fieldCount][];
                    var fieldIsInt = new bool[fieldCount];
                    var types = new byte[fieldCount];
                    var offsets = new int[fieldCount];
                    for (var fi = 0; fi < fieldCount; fi++)
                    {
                        var nameIdxF = ReadU16(task);
                        var symIdxF = ReadU16(task);
                        names[fi] = nameIdxF >= 0 && nameIdxF < _constants.Count ? _constants[nameIdxF] as string ?? "" : "";
                        // Struct annidate (2026-09-29): la seconda costante è un CSV di SymbolId,
                        // uno per segmento del path (non solo la foglia) — vedi FfiUdtShape.
                        var symCsv = symIdxF >= 0 && symIdxF < _constants.Count ? _constants[symIdxF] as string ?? "" : "";
                        symbolIdPaths[fi] = symCsv.Length == 0 ? Array.Empty<int>() : Array.ConvertAll(symCsv.Split(','), int.Parse);
                        fieldIsInt[fi] = !newLayout && (kindMask & (1 << fi)) != 0;
                    }
                    int size;
                    if (newLayout)
                    {
                        size = ReadU16(task);
                        for (var fi = 0; fi < fieldCount; fi++)
                        {
                            types[fi] = (byte)ReadU8(task);
                            offsets[fi] = ReadU16(task);
                            fieldIsInt[fi] = types[fi] != 0 && types[fi] != 8;
                        }
                    }
                    else
                    {
                        // Formato storico: campi consecutivi da 8 byte, kind dalla maschera.
                        size = fieldCount * 8;
                        for (var fi = 0; fi < fieldCount; fi++)
                        {
                            types[fi] = (byte)(fieldIsInt[fi] ? 1 : 0);
                            offsets[fi] = fi * 8;
                        }
                    }
                    return new Basic.Runtime.FfiUdtShape(names, symbolIdPaths, fieldIsInt, types, offsets, size);
                }
                var udtParamShapes = new Basic.Runtime.FfiUdtShape?[PCodeFfiMaxArgs];
                for (var pi = 0; pi < PCodeFfiMaxArgs; pi++)
                {
                    if (udtParamFieldCount[pi] > 0)
                        udtParamShapes[pi] = ReadUdtShape(udtParamFieldCount[pi], (byte)udtParamFieldKindMask[pi], udtParamNew[pi]);
                    else if (udtArrCount[pi] > 0)
                        // Array di UDT: shape dell ELEMENTO (formato nuovo, con size+tipo+offset).
                        udtParamShapes[pi] = ReadUdtShape(udtArrCount[pi], 0, true);
                }
                var retUdtShape = retUdtFieldCount > 0 ? ReadUdtShape(retUdtFieldCount, (byte)retUdtFieldKindMask, retUdtNew) : null;
                // Tabella firme callback SCALARI (2026-11-11, bit 0x40 di `kind`): per ogni
                // parametro callback, in ordine di slot, u8 arità + u8 retKind (0 double,
                // 1 int64, 2 void) + arità × u8 kind. Sostituisce l'assunzione storica di
                // firma interna omogenea (a2 bit0) e alza il limite di arità da 8 a 64.
                bool[][]? cbSigKinds = null;
                var cbSigRet = new int[PCodeFfiMaxArgs];
                if ((kindByte & 0x40) != 0)
                {
                    cbSigKinds = new bool[PCodeFfiMaxArgs][];
                    for (var pi = 0; pi < argCount && pi < PCodeFfiMaxArgs; pi++)
                    {
                        if (callbackParamArity[pi] == 255 || callbackParamArity[pi] >= 128) continue;
                        var ar = ReadU8(task);
                        cbSigRet[pi] = ReadU8(task);
                        var ks = new bool[ar];
                        for (var b = 0; b < ar; b++) ks[b] = ReadU8(task) != 0;
                        cbSigKinds[pi] = ks;
                    }
                }
                var args = new object?[argCount];
                var byrefBoxes = new Basic.Runtime.Gpm.ByRefBox?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                {
                    var popped = task.Sp > 0 ? task.PopRef() : null;
                    if (((byrefScalarMask & (1L << i)) != 0 || (byrefUdtMask & (1L << i)) != 0)
                        && popped is Basic.Runtime.Gpm.ByRefBox box)
                    {
                        byrefBoxes[i] = box;
                        args[i] = box.Value;
                    }
                    else args[i] = popped;
                }
                var libPath = libIdx >= 0 && libIdx < _constants.Count ? _constants[libIdx] as string : null;
                var funcName = nameIdx >= 0 && nameIdx < _constants.Count ? _constants[nameIdx] as string : null;
                                if (libPath != null && funcName != null)
                {
                    try
                    {
                        // Callback come PARAMETRO (2026-09-29): ogni slot con arità≠255 nella
                        // bytecode arriva qui come una ClosureData (popolata dal pop loop sopra,
                        // che per un parametro non-array/STRING/BYREF/UDT si limita a
                        // `args[i] = popped`). Prima della chiamata nativa la si sostituisce col
                        // PUNTATORE nativo reale del trampolino (un delegate .NET taggato
                        // UnmanagedFunctionPointer(Cdecl) — il CLR genera un thunk nativo VERO
                        // per OGNI istanza di delegate, richiamabile in modo sincrono e
                        // rientrante da codice nativo sullo STESSO thread: quando il nativo lo
                        // invoca, il thunk richiama CallFnSync, che rientra nel loop di dispatch
                        // di QUESTO stesso task — stesso meccanismo già usato da AMAP/ASORT per
                        // invocare una lambda BASIC in modo sincrono). Le istanze dei delegate
                        // vanno mantenute vive (GC-rooted) per tutta la durata della chiamata
                        // nativa sottostante — vedi callbackKeepAlive/GC.KeepAlive sotto.
                        var callbackKeepAlive = new List<Delegate>();
                        for (var i = 0; i < argCount && i < PCodeFfiMaxArgs; i++)
                        {
                            if (callbackParamArity[i] == 255) continue;
                            if (args[i] is not Basic.Runtime.Gpm.ClosureData cd)
                                throw new InvalidOperationException($"il parametro {i + 1} atteso come callback (AS FUNCTION/AS SUB) non è una closure valida.");
                            var cbIsInt = (callbackParamKindMask & (1L << i)) != 0;
                            var cbHasRet = (callbackParamHasRetMask & (1L << i)) != 0;
                            var (keepAlive, ptr) = BuildFfiCallbackTrampoline(task, cd, callbackParamArity[i], cbIsInt, cbHasRet,
                                cbSigKinds?[i], cbSigKinds != null ? cbSigRet[i] : -1);
                            callbackKeepAlive.Add(keepAlive);
                            args[i] = ptr.ToInt64();
                        }
                        var handle = _runtime.Ffi.LoadLibrary(libPath);
                        var funcPtr = _runtime.Ffi.GetFunction(handle, funcName, libPath);
                        var anyUdt = udtParamShapes.Any(s => s != null) || retUdtShape != null;
                        // Firma mista (2026-10): scalarKindMask non-uniforme con 'kind' (solo
                        // possibile quando non c'è nessun array/STRING/BYREF/UDT, per
                        // costruzione del guard di compilazione — Compiler.Expressions.cs) →
                        // serve SEMPRE il path per-parametro di CallFunctionTypedArr, mai il
                        // fast path CallFunctionTyped (che assume un kind unico per tutto).
                        var fullMaskForArgc = argCount >= PCodeFfiMaxArgs ? -1L : (1L << argCount) - 1;
                        var uniformScalarKind = scalarKindMask == (kind == 1 ? fullMaskForArgc : 0L);
                        var result = arrayMask != 0 || stringMask != 0 || retIsString || byrefScalarMask != 0 || anyUdt || !uniformScalarKind
                            ? _runtime.Ffi.CallFunctionTypedArr(funcPtr, args, nativeAbi ? (kind | 0x100) : kind, arrayMask, arrayElemIntMask, stringMask, retIsString, byrefScalarMask, byrefScalarElemIntMask, udtParamShapes, retUdtShape, scalarKindMask: scalarKindMask, byrefUdtMask: byrefUdtMask)
                            : _runtime.Ffi.CallFunctionTyped(funcPtr, args, (byte)kind);
                        GC.KeepAlive(callbackKeepAlive);
                        for (var i = 0; i < argCount; i++)
                            if (byrefBoxes[i] is { } b) b.Value = args[i];
                        task.PushRef(result);
                    }
                    catch (Exception ex)
                    {
                        /* Simbolo/libreria FFI non risolti: errore BASIC ESPLICITO e catturabile
                         * (ON ERROR/TRY), codice 5 = IllegalFunctionCall come VM/C e C99Native.
                         * Prima si spingeva la stringa "[FFI_ERROR] ..." sullo stack come se
                         * fosse il risultato: nessun errore, out() intatti e valore credibile. */
                        var code = ex is BasicException be ? (double)be.Code
                                 : ex is FileNotFoundException ? 53.0   /* libreria assente */
                                 : 5.0;                                 /* simbolo assente */
                        RaiseBasicError(task, code, ex.Message);
                    }
                }
                else { task.PushRef(null); }
                return true;
            }

            // === FETCH ===
            case OpCode.OP_FETCH:
            {
                // VisitFetchExpression/VisitFetchStatement (Compiler/Compiler.Expressions.cs,
                // Compiler/Compiler.Network.cs) pushano SEMPRE, in quest'ordine: Url, Options
                // (o OP_PUSH_NIL), indirizzo Ip della callback come double (o OP_PUSH_NIL).
                // Essendo uno stack LIFO, il valore in CIMA (primo PopRef) e' l'ULTIMO pushato,
                // cioe' la callback — NON l'url. Il vecchio codice faceva un solo PopRef e lo
                // trattava come url: prendeva quasi sempre il placeholder nil della callback,
                // ignorava url/options (mai consumati, corruzione dello stack per le istruzioni
                // successive), e chiamava un client HTTP nudo (BRT.Services.HttpService.HttpGet,
                // bloccante, solo GET testuale, nessuna opzione). Bug reale, mai notato perche'
                // nessun test P-CODE esercitava FETCH con esito osservabile.
                var variant = ReadU8(task);
                var callbackRef = task.Sp > 0 ? task.PopRef() : null;
                var optionsRef = task.Sp > 0 ? task.PopRef() : null;
                var url = task.Sp > 0 ? task.PopRef()?.ToString() : null;

                if (string.IsNullOrEmpty(url))
                {
                    task.PushRef(null);
                    return true;
                }

                var options = ConvertPCodeHttpOptions(optionsRef);
                var httpClientHelper = _runtime.HttpClientHelper;
                var scheduler = _scheduler;

                if (variant == 1)
                {
                    // Forma comando "FETCH url[, options] INTO Callback" (sempre con callback) o
                    // forma espressione con callback esplicita: la richiesta parte in background,
                    // il task CHIAMANTE non si blocca. Al completamento, spawna un nuovo
                    // PCodeTask che invoca la SUB di callback — stessa firma dell'AST
                    // (Interpreter/Gpm.Http.Client.cs, ExecuteHttpFetchCommandStep):
                    // SUB Callback(response$, error$).
                    int? callbackIp = callbackRef switch { double d => (int)d, int iv => iv, _ => null };
                    var globalsTemplate = task.Globals;
                    var cancellation = new CancellationTokenSource();
                    var externalOperation = scheduler?.RegisterExternalOperation(cancellation.Cancel, task);
                    _ = CompleteCallbackFetchAsync();

                    async Task CompleteCallbackFetchAsync()
                    {
                        try
                        {
                            CommonLib.Http.FetchResult result;
                            try
                            {
                                result = await httpClientHelper.PerformRequestAsync(url, options, cancellation.Token)
                                    .ConfigureAwait(false);
                            }
                            catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { return; }
                            catch (Exception ex)
                            {
                                result = new CommonLib.Http.FetchResult { StatusCode = 0, ErrorMessage = ex.Message };
                            }
                            if (callbackIp is { } ip && scheduler != null)
                            {
                                var cbTask = scheduler.CreateTask(ip, task);
                                cbTask.Scheduler = scheduler;
                                cbTask.Globals = globalsTemplate;
                                cbTask.PushRef(string.IsNullOrEmpty(result.ErrorMessage) ? result.Body : "");
                                cbTask.PushRef(result.ErrorMessage);
                                scheduler.Schedule(cbTask);
                            }
                        }
                        finally
                        {
                            externalOperation?.Dispose();
                            cancellation.Dispose();
                        }
                    }
                    task.PushRef(null); // Placeholder — il vero risultato va alla callback, non al chiamante.
                    return true;
                }

                // Forma espressione sincrona "DIM res AS FETCHRESULT = FETCH(url[, options])": la
                // richiesta parte in background e OP_FETCH restituisce SUBITO un
                // PCodeFutureValue, senza bloccare ne' il worker ne' il task — stesso pattern gia'
                // usato da OP_GO_FUNCTION per "GO FUNCTION ... INTO x". Chi legge il valore
                // (assegnazione a variabile poi accesso a un campo) attende implicitamente
                // tramite OP_LOAD_LOCAL/OP_LOAD_GLOBAL, che gia' riconoscono un PCodeFutureValue
                // e si parcheggiano fino alla risoluzione (vedi quei case). Limite noto: un
                // accesso concatenato SENZA passare da una variabile (es. FETCH(url).StatusCode
                // inline) non attende automaticamente, dato che l'implicit-await e' implementato
                // solo sul LOAD di una variabile, non su un valore intermedio sullo stack.
                var future = new PCodeFutureValue();
                var futureCancellation = new CancellationTokenSource();
                var futureExternalOperation = scheduler?.RegisterExternalOperation(futureCancellation.Cancel, task);
                _ = CompleteFutureFetchAsync();

                async Task CompleteFutureFetchAsync()
                {
                    try
                    {
                        var result = await httpClientHelper.PerformRequestAsync(url, options, futureCancellation.Token)
                            .ConfigureAwait(false);
                        future.Resolve(new Basic.IlBackend.PCode.Http.PCodeFetchResult(result, url), scheduler);
                    }
                    catch (OperationCanceledException) when (futureCancellation.IsCancellationRequested)
                    {
                        future.Cancel(scheduler);
                    }
                    catch (Exception ex) { future.Fail(ex, scheduler); }
                    finally
                    {
                        futureExternalOperation?.Dispose();
                        futureCancellation.Dispose();
                    }
                }
                task.PushRef(future);
                return true;
            }

            // === ON KEY ===
            case OpCode.OP_ON_MOUSE:
            {
                // ON MOUSE: stack = [eventKind, actionType, targetAddr]. Registra nel
                // EventDispatcher (RegisterMouse) — il dispatch (mouse TUI via TerminalReader,
                // mouse GUI via SdlManager) spawna il task dell'handler.
                var targetRaw = task.Sp > 0 ? task.PopRef() : null;
                var actionRaw = task.Sp > 0 ? task.PopRef() : null;
                var kindRaw = task.Sp > 0 ? task.PopRef() : null;
                var targetAddr = targetRaw is double td ? (int)td : targetRaw is int ti ? ti : -1;
                var actionType = actionRaw is double ad ? (int)ad : actionRaw is int ai ? ai : -1;
                var eventKind = kindRaw is double kd ? (int)kd : kindRaw is int ki ? ki : -1;
                if (actionType == 0 && eventKind >= 0)
                {
                    CommonLib.App.EventDispatcher.Instance.UnregisterMouse(eventKind);
                }
                else if (actionType > 0 && eventKind >= 0 && targetAddr >= 0)
                {
                    var globals = task.Globals;
                    CommonLib.App.EventDispatcher.Instance.RegisterMouse(eventKind, (x, y) => FireMouseEvent(targetAddr, globals));
                }
                return true;
            }
            case OpCode.OP_ON_KEY:
            {
                // ON KEY: stack = [KeyCode, ActionType, TargetAddress] (target ultimo pushato).
                // Registra l'handler nel EventDispatcher unificato (unico gestore eventi:
                // ON KEY / ON MOUSE / ON WINDOW / CTRL+C). Al dispatch (lettura tasti in
                // INKEY/INPUT/GET_KEY/GmBasicInputSource/READ Clipper) il task dell'handler
                // viene spawnato indipendentemente (stesso schema di FireWindowEvent).
                var targetRaw = task.Sp > 0 ? task.PopRef() : null;
                var actionRaw = task.Sp > 0 ? task.PopRef() : null;
                var keyRaw = task.Sp > 0 ? task.PopRef() : null;
                var targetAddr = targetRaw is double td ? (int)td : targetRaw is int ti ? ti : -1;
                var actionType = actionRaw is double ad ? (int)ad : actionRaw is int ai ? ai : -1;
                var keyCode = keyRaw is double kd ? (int)kd : keyRaw is int ki ? ki : -1;
                // KEY.MAKE(tasto, mod) produce una KeyCombination con i MODIFICATORI: se il
                // valore è già una combinazione (non un int nudo), usa i suoi modifiers —
                // prima erano sempre fissati a 0 (ON KEY (KEY.MAKE(F1, ALT)) registrava F1 senza ALT).
                var combo = keyRaw is CommonLib.Keyboard.KeyCombination kc
                    ? kc
                    : new CommonLib.Keyboard.KeyCombination((ConsoleKey)keyCode, 0);
                if (actionType == 0 && combo.Key != 0)
                {
                    // STOP: rimuove gli handler per quel tasto.
                    CommonLib.App.EventDispatcher.Instance.UnregisterKey(combo);
                }
                else if (actionType > 0 && combo.Key != 0 && targetAddr >= 0)
                {
                    var globals = task.Globals;
                    CommonLib.App.EventDispatcher.Instance.RegisterKey(combo, () => FireKeyEvent(targetAddr, globals));
                }
                return true;
            }

            // === REDIM ===
            case OpCode.OP_REDIM:
            case OpCode.OP_REDIM_PRESERVE:
            {
                var numDims = ReadU8(task);
                var udtDescIdx = ReadU16(task);
                System.Collections.IDictionary? redimUdtDesc = null;
                if (udtDescIdx < _constants.Count && _constants[udtDescIdx] is System.Collections.IDictionary rDesc && rDesc.Contains("FieldNames"))
                    redimUdtDesc = rDesc;
                var lengths = new int[numDims];
                for (var i = numDims - 1; i >= 0; i--)
                    lengths[i] = PopInt(task);
                var oldArr = task.Sp > 0 ? task.PopRef() : null;
                var isPreserve = (OpCode)_code[task.Ip - (1 + 1 + 2)] == OpCode.OP_REDIM_PRESERVE;
                // DIM n gives upper bound → size = n + 1 (classic BASIC 0..n)
                for (int li = 0; li < numDims; li++) lengths[li] += 1;
                // Crea nuovo array
                Array newArr;
                if (numDims == 1)
                    newArr = new object?[lengths[0]];
                else
                {
                    newArr = Array.CreateInstance(typeof(object), lengths);
                }
                // Se REDIM_PRESERVE, copia vecchi elementi
                if (isPreserve && oldArr is Array oldA)
                {
                    var copyCount = Math.Min(oldA.Length, newArr.Length);
                    var oldLb = oldA.GetLowerBound(0);
                    var newLb = newArr.GetLowerBound(0);
                    for (var i = 0; i < copyCount; i++)
                        newArr.SetValue(oldA.GetValue(oldLb + i), newLb + i);
                    // Fill remaining elements with default UDT instance or 0.0
                    for (var i = copyCount; i < newArr.Length; i++)
                        newArr.SetValue(redimUdtDesc != null ? MakeDefaultUdtInstance(redimUdtDesc) : 0.0, newLb + i);
                }
                else
                {
                    // Initialize all elements
                    for (var i = 0; i < newArr.Length; i++)
                        newArr.SetValue(redimUdtDesc != null ? MakeDefaultUdtInstance(redimUdtDesc) : 0.0, i);
                }
                task.PushRef(newArr);
                return true;
            }
            case OpCode.OP_FINALIZE_SCOPE:
            {
                var localIdx = ReadU16(task);
                if (task.Frame == null || localIdx >= task.Frame.LocalVariables.Length)
                    return true;
                FinalizeInstance(task, task.Frame.LocalVariables[localIdx]);
                return true;
            }
            case OpCode.OP_FINALIZE_GLOBAL:
            {
                var gblIdx = ReadU16(task);
                var instance = task.GetGlobal(gblIdx);
                if (instance is ByRefBox box)
                    instance = box.Value;
                FinalizeInstance(task, instance);
                return true;
            }
            case OpCode.OP_EVAL_MACRO_STMT:
            {
                // Macro &(expr) come statement: compila ed esegue la stringa (parità IL/Clipper).
                var src = task.Sp > 0 ? task.PopRef() as string : null;
                EvalMacroInPCode(task, src ?? "", asExpr: false);
                return true;
            }
            case OpCode.OP_EVAL_MACRO_EXPR:
            {
                // Macro &(expr)/EVAL come valore: compila ed esegue la stringa, restituisce il VALORE.
                var src = task.Sp > 0 ? task.PopRef() as string : null;
                var val = EvalMacroInPCode(task, src ?? "", asExpr: true);
                task.PushRef(val);
                return true;
            }


            case OpCode.OP_CLOSURE:
            {
                var constIdx = ReadU16(task);
                var numUpvalues = ReadU8(task);

                object?[]? capturedUpvalues = null;
                if (numUpvalues > 0)
                {
                    capturedUpvalues = new object?[numUpvalues];
                    for (int i = 0; i < numUpvalues; i++)
                    {
                        // 3 convenzioni (vedi Compiler.Upvalue.Kind): 1 = locale del frame
                        // chiamante (box write-through per gli scalari), 2 = UPVALUE del frame
                        // corrente (cattura TRANSITIVA lambda dentro lambda: si legge dalla
                        // closure del frame), 0 = legacy "closure esterna sullo stack".
                        var upKind = ReadU8(task);
                        var isLocal = upKind == 1;
                        var isParentUpvalue = upKind == 2;
                        var index = ReadU8(task);
                        if (isParentUpvalue)
                        {
                            capturedUpvalues[i] = task.Frame?.Closure?.GetUpvalue(index);
                        }
                        else if (isLocal)
                        {
                            var locals = task.Frame?.LocalVariables ?? task.GetLocals();
                            var slotValue = index < locals.Length ? locals[index] : null;
                            // Cattura per RIFERIMENTO SOLO per slot mutabili-scalari (null o
                            // VALUE type): l'upvalue è un ByRefBox PROXY allo slot — le scritture
                            // della closure e del chiamante condividono lo slot (bug closure_byref:
                            // SUB(){ x = x + 1 } non propagava la modifica col valore-snapshot).
                            // NON proxyare i REFERENCE type (dict/oggetti/array/chiusure): sono già
                            // condivisi per riferimento catturando il ref, e un proxy a uno slot di
                            // un frame POPPATO (es. il dict __context di FunctionsAdvanced Test 2,
                            // chiusura ritornata da CreaIncrementatore) leggerebbe garbage da uno
                            // slot riusato → incremento perso. Se lo slot contiene GIÀ un ByRefBox
                            // (param BYREF / slot promosso __ClosureCtx) lo si tiene com'è (un
                            // doppio wrap romperebbe la risoluzione a un solo livello).
                            /* Anche le STRINGHE vanno imbozzate sullo SLOT: una closure
                             * cattura la VARIABILE, non il valore (parità IL/VM-C/C99). Con
                             * la cattura dell'oggetto, `s = "b"` dopo la creazione sostituiva
                             * l'oggetto nello slot e la closure continuava a vedere "a"
                             * (lambda_nested_capture: s=b su IL/VM-C/C99, s=a su PCode).
                             * Per le stringhe il ByRefBox è sicuro anche a frame poppato:
                             * trattiene l'array dei locals (GC), non memoria nativa. Dict/
                             * oggetti/array/chiusure restano condivisi per riferimento. */
                            capturedUpvalues[i] = slotValue is ByRefBox
                                ? slotValue
                                : (slotValue is null || slotValue.GetType().IsValueType || slotValue is string
                                    ? (index < locals.Length ? new ByRefBox(locals, index) : null)
                                    : slotValue);
                        }
                        else
                        {
                            var enclosingClosure = task.Sp > 0 ? task.Stack[task.Sp - 1].Ref as ClosureData : null;
                            capturedUpvalues[i] = enclosingClosure?.GetUpvalue(index);
                        }
                    }
                }

                var cd = new ClosureData(constIdx);
                if (capturedUpvalues != null)
                {
                    for (int i = 0; i < capturedUpvalues.Length; i++)
                        cd.SetUpvalue(i, capturedUpvalues[i]);
                }
                task.PushRef(cd);
                return true;
            }
            case OpCode.OP_LOAD_UPVALUE:
            {
                var index = ReadU8(task);
                var closure = task.Frame?.Closure;
                var val = closure?.GetUpvalue(index);
                bool wasBox = val is ByRefBox;
                // Specchio OP_LOAD_LOCAL: un upvalue può essere un ByRefBox (parametro BYREF
                // o slot promosso) — la lettura va risolta al VALORE, non al box. Prima di
                // questo fix l'upvalue veniva spinta grezza: ByRefBox.ToString() non si
                // parsava come numero (→ 0) e l'aritmetica produceva 1 da 100 (closure su
                // parametro BYREF di FunctionsAdvanced Test 2). STORE_UPVALUE scriveva
                // comunque attraverso il box, quindi il valore si corrompeva invece di
                // restare 100,101,102.
                if (wasBox) val = ((ByRefBox)val).Value;
                task.PushRef(val);
                return true;
            }
            case OpCode.OP_STORE_UPVALUE:
            {
                var index = ReadU8(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                var closure = task.Frame?.Closure;
                if (closure != null)
                {
                    var existing = closure.GetUpvalue(index);
                    if (existing is ByRefBox brb)
                        brb.Value = val;
                    else
                        closure.SetUpvalue(index, val);
                }
                task.PushRef(val);
                return true;
            }

            case OpCode.OP_BREAKPOINT:
            {
                if (DebugServer != null)
                {
                    DebugServer.SignalPause(task);
                    DebugServer.WaitForContinue();
                }
                return true;
            }

            default:
                task.State = PCodeTaskState.Finished;
                return false;
        }
    }

    private void FinalizeInstance(PCodeTask task, object? instance)
    {
        if (instance is not Dictionary<string, object?> dict)
            return;
        if (!dict.TryGetValue("__class__", out var clsObj) ||
            clsObj is not Dictionary<string, object?> clsDesc)
            return;
        if (!clsDesc.TryGetValue("Destructor", out var dtorObj) ||
            dtorObj is not int dtorAddr)
            return;

        var newFrame = new PCodeCallFrame
        {
            SavedStackPointer = task.Sp,
            ReturnIp = task.Ip,
            Previous = task.Frame,
            LocalVariables = CreateLocals(),
        };
        EnterProcedureErrorScope(task, newFrame);
        newFrame.LocalVariables[0] = instance;
        task.Frame = newFrame;
        task.Ip = dtorAddr;
    }

    // ================================================================
    // FILE_CMD ROUTING
    // ================================================================

    /// <summary>Estrae il valore numerico da uno slot di stack, sia esso un numero nativo
    /// (Type=1) o un riferimento boxed (Type=2, es. una costante numerica PUSH_CONST).
    /// Usato dai case TIMEOUT del GO SELECT (parità Go case &lt;-time.After(d)).</summary>
    private static double SlotNumericValue(in StackEntry entry)
    {
        if (entry.Type == 1) return entry.Num;
        if (entry.Ref is double d) return d;
        if (entry.Ref is int i) return i;
        if (entry.Ref is long l) return l;
        if (entry.Ref is float f) return f;
        if (entry.Ref is decimal m) return (double)m;
        return 0.0;
    }

    /// <summary>Truthiness di un operando per OP_JUMP_IF_TRUE/FALSE. Una STRINGA segue la
    /// semantica Convert.ToBoolean ("True"→vero, "False"→falso, numerica→valore, ""→falso),
    /// NON la conversione a 0 con PopNum (che rendeva OGNI `IF <stringa>` falso). Parità IL/VM-C.</summary>
    private static bool PCodeTruthy(StackEntry? e, double numCond)
    {
        // Stringa: semantica Convert.ToBoolean ("True"→vero, "False"→falso, numerica→valore,
        // ""→falso). Per il resto si usa il valore di PopNum (NON rawCond.Num: PopNum applica
        // le conversioni per basic-type — es. BIT → -1/0 — che rawCond.Num non garantisce).
        if (e is { Type: 2, Ref: string s })
        {
            if (string.IsNullOrEmpty(s)) return false;
            if (s.Equals("False", StringComparison.OrdinalIgnoreCase)) return false;
            if (s.Equals("True", StringComparison.OrdinalIgnoreCase)) return true;
            return double.TryParse(s, System.Globalization.NumberStyles.Any,
                System.Globalization.CultureInfo.InvariantCulture, out var d) ? d != 0.0 : true;
        }
        return numCond != 0.0;
    }
}
