using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;
using Basic.ConversionLib;
using Basic.ConversionLib.Formats;
using Basic.ConversionLib.Render;
using Basic.ModelDownload;
using Basic.AI.ModelDownload;
using Basic.AI.Rag;
using Basic.AI.Graph;
using Basic.SQLite.Vector;
using Basic.Runtime;
using CommonLib.Consoles;
using CommonLib.Screens;
using CommonGraphics;
using CommonGraphics.Consoles;
using CommonGraphics.Keyboard;

namespace Basic.Executor;

public enum ReplState
{
    Normal,
    WaitingForConfirmation
}

public class ReplEngine
{
    private readonly SortedDictionary<int, string> _programLines = new();
    private readonly List<string> _commandHistory = [];
    private readonly StringBuilder _inputBuffer = new(256);
    private GMBasic.Notebook.NotebookServer? _notebookServer;
    private GMBasic.Notebook.NotebookKernel? _notebookKernel;
    private int _inputCursorPos;
    // Console attiva (IConsole: Sdl/Standard/System). null -> System.Console diretto.
    // static: la REPL è single-instance e gli helper sono usati anche in metodi static.
    private static IConsole? _activeConsole;
    private static bool _useSdl;
    private static CommonLib.Keyboard.IKeyboardService? _kb;
    // Coda continuazioni async: le `await` dei comandi tornano sul MAIN thread (il loop le
    // drena ad ogni giro). Così il comando è async/park-based (GPM §0ter) e il loop SDL
    // continua a pompare (PollEvents/Present) — niente GetResult() bloccante.
    private readonly System.Collections.Concurrent.ConcurrentQueue<Action> _uiQueue = new();
    private sealed class LoopSyncContext : SynchronizationContext
    {
        private readonly System.Collections.Concurrent.ConcurrentQueue<Action> _q;
        private readonly Thread _main;
        public LoopSyncContext(System.Collections.Concurrent.ConcurrentQueue<Action> q) { _q = q; _main = Thread.CurrentThread; }
        public override void Post(SendOrPostCallback d, object? s) => _q.Enqueue(() => d(s));
        public override void Send(SendOrPostCallback d, object? s) { if (Thread.CurrentThread == _main) d(s); else Post(d, s); }
    }
    private int _historyIndex;
    private int _autoNext;
    private int _autoInc = 10;
    private bool _isReplCommandExecuting;
    private bool IsInteractive => _activeConsole != null || !Console.IsInputRedirected;
    // ── I/O instradato sulla console attiva (IConsole), fallback System.Console ──────────
    // Parità col riferimento: output/cursore/clear/present via _activeConsole; input via _kb.
    private static TextWriter Out => _activeConsole?.Out ?? Console.Out;
    private static TextWriter OutErr => _activeConsole?.Error ?? Console.Error;
    private static int TermWidth { get { try { return _activeConsole?.WindowWidth ?? Console.WindowWidth; } catch { return 80; } } }
    private static int TermHeight { get { try { return _activeConsole?.WindowHeight ?? Console.WindowHeight; } catch { return 25; } } }
    private static int CurLeft
    {
        get { try { return _activeConsole != null ? _activeConsole.CursorLeft : Console.CursorLeft; } catch { return 0; } }
        set { try { if (_activeConsole != null) _activeConsole.CursorLeft = value; else Console.CursorLeft = value; } catch { } }
    }
    private static int CurTop
    {
        get { try { return _activeConsole != null ? _activeConsole.CursorTop : Console.CursorTop; } catch { return 0; } }
        set { try { if (_activeConsole != null) _activeConsole.CursorTop = value; else Console.CursorTop = value; } catch { } }
    }
    private static void SetCursorPos(int left, int top) { try { if (_activeConsole != null) _activeConsole.SetCursorPosition(left, top); else Console.SetCursorPosition(left, top); } catch { } }
    private static void Present() { try { _activeConsole?.Present(); } catch { } }
    private string _replProgramSource = string.Empty;
    private string _projectPath = Directory.GetCurrentDirectory();
    private PrgChunk? _prgChunk;
    private string? _prgChunkPath;
    private bool _usePrgVm;
    private bool _quitting;
    private int _lastW, _lastH, _inputLine;
    private CancellationTokenSource _runCts = new();
    private Program.ExecutorOptions _opts = new();
    private CommandLineArgs? _cliArgs;
    private string _prompt = "";
    private HelpIndex? _helpIndex;
    private IReadOnlyList<SampleInfo> _activeSampleList = [];
    private static IReadOnlyList<SampleInfo>? _masterSampleCatalog;
    private static string? _samplesRoot;

    public sealed record SampleInfo(SampleType Type, string FilePath, string DisplayName, string Description, string FullContent)
    {
        public SampleInfo(SampleType type, string filePath, string displayName, string description)
            : this(type, filePath, displayName, description, string.Empty) { }
    }

    public enum SampleType { File, Directory, ParentDirectory }

    static ReplEngine()
    {
        _samplesRoot = ResolveSamplesRoot();
        _masterSampleCatalog = BuildSampleCatalog();
    }

    private static string? ResolveSamplesRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (var i = 0; i < 10 && dir != null; i++, dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "Executor", "Samples");
            if (Directory.Exists(candidate)) return candidate;
        }
        var fallback = Path.Combine(AppContext.BaseDirectory, "Samples");
        return Directory.Exists(fallback) ? fallback : null;
    }

    private static IReadOnlyList<SampleInfo> BuildSampleCatalog()
    {
        var catalog = new List<SampleInfo>();
        if (_samplesRoot == null || !Directory.Exists(_samplesRoot)) return catalog;
        var allFiles = Directory.GetFiles(_samplesRoot, "*.bas", SearchOption.AllDirectories);
        foreach (var filePath in allFiles)
        {
            string desc, content;
            try { content = File.ReadAllText(filePath); desc = ExtractSampleDescription(content, Path.GetFileNameWithoutExtension(filePath)); }
            catch { desc = Path.GetFileNameWithoutExtension(filePath); content = string.Empty; }
            catalog.Add(new SampleInfo(SampleType.File, filePath, Path.GetFileName(filePath), desc, content));
        }
        return catalog.OrderBy(s => s.DisplayName).ToList();
    }

    private static string ExtractSampleDescription(string source, string fallback)
    {
        var titleRe = new Regex(@"PRINT\s+""║\s*(.*?)\s*║""", RegexOptions.IgnoreCase);
        var remRe = new Regex(@"^\s*(?:REM|//)\s+.*?-\s+(.*)", RegexOptions.IgnoreCase | RegexOptions.Multiline);
        using var reader = new StringReader(source);
        for (var i = 0; i < 20; i++)
        {
            var line = reader.ReadLine();
            if (line == null) break;
            var tm = titleRe.Match(line);
            if (tm.Success) return Regex.Replace(tm.Groups[1].Value.Trim(), @"\s+", " ");
            var rm = remRe.Match(line);
            if (rm.Success) return Regex.Replace(rm.Groups[1].Value.Trim(), @"\s+", " ");
        }
        return fallback;
    }

    public event Action? BreakRequested;

    private readonly Dictionary<string, Func<string, CancellationToken, Task>> _commands;

    public ReplEngine()
    {
        _commands = new Dictionary<string, Func<string, CancellationToken, Task>>(StringComparer.OrdinalIgnoreCase)
        {
            ["QUIT"] = HandleQuitAsync,
            ["EXIT"] = HandleQuitAsync,
            ["CLS"] = HandleClsAsync,
            ["PROMPT"] = HandlePromptAsync,
            ["HELP"] = HandleHelpAsync,
            ["LIST"] = HandleListAsync,
            ["EDIT"] = HandleEditAsync,
            ["IDE"] = HandleEditAsync,   /* alias: il REPL nativo usa IDE per lo stesso editor */
            ["NEW"] = HandleNewAsync,
            ["AUTO"] = HandleAutoAsync,
            ["SAVE"] = HandleSaveAsync,
            ["LOAD"] = HandleLoadAsync,
            ["DELETE"] = HandleDeleteAsync,
            ["DEL"] = HandleDeleteAsync,
            ["RENUM"] = HandleRenumAsync,
            ["SAMPLE"] = HandleSampleAsync,
            ["COMPILE"] = HandleCompileAsync,
            ["BSP"] = HandleCompileBspAsync,
            ["RUN"] = HandleRunAsync,
            ["EXEC"] = HandleExecAsync,
            ["LOADPRG"] = HandleLoadPrgAsync,
            ["PRGINFO"] = HandlePrgInfoAsync,
            ["AST"] = HandleAstAsync,
            ["DASM"] = HandleDasmAsync,
            ["IL"] = HandleIlAsync,
            ["GPM"] = HandleGpmAsync,
            ["LS"] = HandleLsAsync,
            ["CD"] = HandleCdAsync,
            ["WHERE"] = HandleWhereAsync,
            ["WH"] = HandleWhereAsync,
            ["GREP"] = HandleGrepAsync,
            ["RG"] = HandleGrepAsync,
            ["RN"] = HandleRenameFileAsync,
            ["RENAME"] = HandleRenameFileAsync,
            ["RM"] = HandleRenameFileAsync,
            ["DL"] = HandleDeleteFileAsync,
            ["DELF"] = HandleDeleteFileAsync,
            ["CP"] = HandleCopyFileAsync,
            ["COPY"] = HandleCopyFileAsync,
            ["DP"] = HandleDuplicateFileAsync,
            ["DUPLICATE"] = HandleDuplicateFileAsync,
            ["BL"] = HandleBackupFileAsync,
            ["BACKUP"] = HandleBackupFileAsync,
            ["DEPLOY"] = HandleDeployAsync,
            ["CONVERT"] = HandleConvertAsync,
            ["BATCH"] = HandleBatchAsync,
            ["CONVFMT"] = HandleConvFmtAsync,
            ["WEB2MD"] = HandleWeb2MdAsync,
            ["PROVIDERS"] = HandleProvidersAsync,
            ["DEP"] = HandleDeployAsync,
            ["PACKAGE"] = HandlePackageAsync,
            ["RESTORE"] = HandleRestoreAsync,
            ["UPDATE"] = HandleUpdateAsync,
            ["TRUST"] = HandleTrustAsync,
            ["ROLLBACK"] = HandleRollbackAsync,
            ["BUMP"] = HandleBumpAsync,
            ["ADD"] = HandleAddAsync,
            ["REMOVE"] = HandleRemoveAsync,
            ["CACHE"] = HandleCacheAsync,
            ["HF"] = HandleHfDownloadAsync,
            ["MLX"] = HandleMlxDownloadAsync,
            ["SETUP"] = HandleSetupAsync,
            ["LIBS"] = HandleLibsAsync,
            ["AIPATH"] = HandleAiPathAsync,
            ["LLM"] = HandleLlmAsync,
            ["RAG"] = HandleRagAsync,
            ["GRAPH"] = HandleGraphAsync,
            ["BRAIN"] = HandleBrainAsync,
            ["FIXTURES"] = HandleFixturesAsync,
            ["EVAL"] = HandleEvalAsync,
        };
        InitHelp();
    }

    private void InitHelp()
    {
        try
        {
            var baseDir = AppDomain.CurrentDomain.BaseDirectory;
            var helpsDir = Path.Combine(baseDir, "Helps");
            if (!Directory.Exists(helpsDir))
                helpsDir = Path.Combine(Directory.GetCurrentDirectory(), "Executor", "Helps");
            var commandsPath = Path.Combine(baseDir, "commands.txt");
            if (!File.Exists(commandsPath))
                commandsPath = Path.Combine(Directory.GetCurrentDirectory(), "commands.txt");
            _helpIndex = HelpIndex.Load(helpsDir, commandsPath);
        }
        catch { _helpIndex = null; }
    }

    // ── Avvio console (port di GMBasic-Deprecated Executor.Startup.cs) ──────────────
    // Regola: System/Standard → TERMINALE; SDL (default, o -c64) → FINESTRA grafica senza
    // terminale. Qui il ramo SDL: la SdlConsole (CommonGraphics) + il loop pompa SDL
    // (PollEvents/Present) sul main thread, e Console.* è rediretto sulla console attiva.
    private void InitConsole(CommandLineArgs cli)
    {
        var headless = Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") is "1" or "true";
        if (headless) return;
        // SDL attivo = default (o -c64); spento da -c/-console; -s = System (terminale).
        var useSdl = cli.UseSdl && !cli.UseSystemConsole;

        ScreenConfig cfg;
        if (cli.UseC64Mode)
        {
            var c64 = ScreenConfig.GetDefault(ScreenMode.C64);
            cfg = c64 with
            {
                CharGridWidth = cli.GridWidth ?? c64.CharGridWidth,
                CharGridHeight = cli.GridHeight ?? c64.CharGridHeight,
            };
        }
        else
        {
            // Console TESTUALE: configurazione GESTITA (stessi valori di ScreenMode.Text).
            // Chiamare `ScreenConfig.GetDefault` qui costringeva a caricare `libgmb_gfx` — e con
            // essa SDL2/SDL3, mixer+codec audio, crypto/ssl, zstd/brotli/lzma/lz4, MoltenVK:
            // ~26 MB per stampare una riga. La dylib si carica solo se serve davvero (SDL o
            // comandi grafici).
            var baseCfg = ScreenConfig.ManagedTextDefault;
            cfg = baseCfg with
            {
                CharGridWidth = cli.GridWidth ?? 100,
                CharGridHeight = cli.GridHeight ?? 40,
                ResolvedPalette = ScreenConfig.Palettes.Vga16Managed,
                DefaultFgColor = 15,
                DefaultBgColor = 1,
            };
        }

        if (useSdl)
        {
            var clip = new CommonGraphics.Clipboard.ClipboardServiceSdl();
            var sdl = new SdlConsole(clip, cfg);
            _activeConsole = sdl;
            _kb = CommonGraphics.Keyboard.Keyboard.KeyboardServiceSdl.Instance;
            _useSdl = true;
            // SDL_QUIT della finestra -> esce dal loop (parità RunMainLoop: QuitRequested).
            SdlManager.Instance.QuitRequested += _ => { ActiveConsoleIO.QuitRequested = true; _quitting = true; };
        }
        else
        {
            // Standard (-c) o System (-s): IConsole con buffer (Standard) / wrapper (System).
            if (cli.UseSystemConsole)
            {
                var sys = new SystemConsole(cfg);
                _activeConsole = sys;
                _kb = new CommonLib.Keyboard.KeyboardServiceConsole(new CommonLib.Clipboard.ClipboardServiceSystem());
                _kb.Start();   // SENZA Start() il listener dei tasti non parte: input morto
                // Parità visiva con Standard/SDL/C64: palette applicata e schermo pulito
                // con lo sfondo attivo (la StandardConsole lo fa già nel costruttore).
                try { sys.ResetColor(); sys.Clear(); } catch (IOException) { }
            }
            else
            {
                var clip = new CommonLib.Clipboard.ClipboardServiceSystem();
                var kb = new CommonLib.Keyboard.KeyboardServiceConsole(clip);
                _activeConsole = new StandardConsole(cfg, kb, clip);
                _kb = kb;
                _kb.Start();   // idem: il listener dei tasti va avviato esplicitamente
            }
        }

        // I/O globale: tutti i componenti (REPL, HelpForm, ClipperInput, ConsoleRenderer, IDE)
        // usano ActiveConsoleIO; System.Console resta solo in Standard/SystemConsole.
        ActiveConsoleIO.Console = _activeConsole;
        ActiveConsoleIO.Keyboard = _kb;
        ActiveConsoleIO.Pump = () =>
        {
            if (_useSdl) { try { SdlManager.Instance.PollEvents(); } catch { } }
            try { _activeConsole?.Present(); } catch { }
        };
        // ClipperInput (HelpForm/menu/IDE) deve leggere dalla console attiva, non dal
        // terminale. KeySource blocca (pompa SDL) finché arriva un tasto → finestra viva.
        if (_activeConsole != null)
        {
            ClipperLib.Clipper.ClipperInput.SetKeySource(() =>
            {
                if (ActiveConsoleIO.QuitRequested) return 27;   // ESC: sblocca i loop in uscita
                var k = ActiveConsoleIO.ReadKey(-1);
                if (k is not { } ki) return 0;
                return ki.Key switch
                {
                    ConsoleKey.Enter => 13, ConsoleKey.Escape => 27, ConsoleKey.Backspace => 8, ConsoleKey.Tab => 9,
                    ConsoleKey.UpArrow => 0x1e, ConsoleKey.DownArrow => 0x1f, ConsoleKey.LeftArrow => 0x1c, ConsoleKey.RightArrow => 0x1d,
                    _ => ki.KeyChar
                };
            });
        }
    }

    public void Run(CommandLineArgs cliArgs)
    {
        _cliArgs = cliArgs;
        Console.OutputEncoding = Encoding.UTF8;
        InitConsole(cliArgs);   // DOPO OutputEncoding (che altrimenti ricrea Console.Out e annulla SetOut)

        // Collega i flag backend: -m/-managed (DLL+apphost) e -n/-native (binario nativo).
        _opts = _opts with { Managed = cliArgs.Managed, Native = cliArgs.Native };
        // Collega la modalità GPM (-gpm:auto|always|linear / env GMBASIC_GPM).
        _opts = _opts with { GpmMode = cliArgs.GpmMode };
        // Collega --no-optimize/--dump-il (prima non arrivavano mai a _opts: Program.Main
        // calcolava un ExecutorOptions separato via ParseArgs() e lo scartava, passando qui
        // solo cliArgs — che Arguments.cs non popolava affatto per questi due flag).
        _opts = _opts with { NoOptimize = cliArgs.NoOptimize, DumpIl = cliArgs.DumpIl };
        _opts = _opts with { CompileBspDir = cliArgs.CompileBspDir };
        // -full (distribuzione managed: closure copiata davvero + framework .NET accanto):
        // anche questo flag non arrivava a _opts, quindi i rami `if (_opts.Full)` erano
        // morti all'avvio. Con il default symlink di -m diventa il discriminante fra
        // "eseguo" (link) e "impacchetto" (copia).
        _opts = _opts with { Full = cliArgs.Full };
        // Collega --pcode (PCodeVm C# in-process): stesso bug di sopra, _opts.PCodeOnly
        // (popolato dal flag -pcode, singolo trattino, in Program.ParseArgs) non arrivava
        // mai qui — il flag realmente letto da questo punto in poi è --pcode (Arguments.cs,
        // cliArgs.UsePCodeVm), quindi si collega quello.
        _opts = _opts with { PCodeOnly = cliArgs.UsePCodeVm };

        // Moduli IL esterni: --il-module <path.dll> (ripetibile) → IlModuleLoader.Load.
        // Il loader generico li registra come namespace BASIC (`NS.METHOD(...)`) prima della
        // compilazione di qualsiasi script/REPL.
        foreach (var modulePath in cliArgs.IlModules)
        {
            if (!File.Exists(modulePath))
            {
                WriteError($"?Modulo IL non trovato: {modulePath}");
                continue;
            }
            try
            {
                var n = Basic.IlBackend.IlModuleLoader.Load(Path.GetFullPath(modulePath));
                Out.WriteLine($"[IL-MODULE] caricato {Path.GetFileName(modulePath)}: {n} namespace.");
            }
            catch (Exception ex)
            {
                WriteError($"?Errore caricamento modulo IL {modulePath}: {ex.GetBaseException().Message}");
            }
        }

        if (_activeConsole == null && Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") != "1") ApplyConsolePalette();

        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true;
            if (_runCts is { IsCancellationRequested: false })
            {
                _runCts.Cancel();
                ResetRunCts();
            }
            BreakRequested?.Invoke();
        };

        if (!string.IsNullOrWhiteSpace(_opts.CompileBspDir))
        {
            HandleCompileBspAsync(_opts.CompileBspDir, CancellationToken.None).GetAwaiter().GetResult();
            return;
        }

        if (!string.IsNullOrWhiteSpace(_cliArgs?.FileName))
        {
            var file = _cliArgs.FileName;
            if (file.EndsWith(".prg", StringComparison.OrdinalIgnoreCase))
            {
                if (PrgFormat.IsValid(file))
                {
                    _prgChunk = PrgFormat.Read(file);
                    _prgChunkPath = file;
                    _usePrgVm = true;
                    RunPrgVmSafe();
                }
                else
                {
                    WriteError($"?Invalid .prg: {file}");
                }
            }
            else if (File.Exists(file))
            {
                if (_opts.Native)
                    RunNativeAotSafe(file);
                else if (_opts.Managed)
                    RunNativeSafe(file);
                else if (_cliArgs.IlModules.Count > 0)
                    RunIlManagedSafe(file); // moduli IL esterni: solo backend IL in-process
                else if (_opts.PCodeOnly)
                    RunPCodeVmManagedSafe(file);
                else
                    RunPCodeSafe(file);
            }
            else if (File.Exists(file + ".bas"))
            {
                if (_opts.Native)
                    RunNativeAotSafe(file + ".bas");
                else if (_opts.Managed)
                    RunNativeSafe(file + ".bas");
                else if (_cliArgs.IlModules.Count > 0)
                    RunIlManagedSafe(file + ".bas");
                else if (_opts.PCodeOnly)
                    RunPCodeVmManagedSafe(file + ".bas");
                else
                    RunPCodeSafe(file + ".bas");
            }
            else if (File.Exists(Path.Combine(_projectPath, file)))
            {
                if (_opts.Native)
                    RunNativeAotSafe(Path.Combine(_projectPath, file));
                else if (_opts.Managed)
                    RunNativeSafe(Path.Combine(_projectPath, file));
                else if (_cliArgs.IlModules.Count > 0)
                    RunIlManagedSafe(Path.Combine(_projectPath, file));
                else if (_opts.PCodeOnly)
                    RunPCodeVmManagedSafe(Path.Combine(_projectPath, file));
                else
                    RunPCodeSafe(Path.Combine(_projectPath, file));
            }
            else
            {
                WriteError($"?File not found: {file}");
            }

            if (!_opts.StayInRepl)
                return;
        }

        // --run <cmd>: esegue un comando/riga all'avvio e termina (parità vecchio
        // Executor.Startup REPL: "Esegui un comando diretto, es. AST ALL"). Non entra
        // nel loop interattivo → la REPL è pilotabile non-interattivamente.
        if (!string.IsNullOrWhiteSpace(_cliArgs?.RunCommand))
        {
            HandleReplInput(_cliArgs.RunCommand.Trim(), CancellationToken.None).GetAwaiter().GetResult();
            return;
        }

        _activeConsole?.Show();   // apre la finestra SdlConsole (parità RunMainLoop: _activeConsole.Show())
        // Le continuazioni async dei comandi tornano sul main thread (drenate dal loop).
        SynchronizationContext.SetSynchronizationContext(new LoopSyncContext(_uiQueue));
        PrintWelcome();
        try { _inputLine = CurTop; } catch { _inputLine = 7; }
        _lastW = TermWidth; _lastH = TermHeight;

        while (!_quitting)
        {
            while (_uiQueue.TryDequeue(out var _act)) { try { _act(); } catch { } }   // continuazioni async (main thread)
            if (_useSdl) SdlManager.Instance.PollEvents();   // pompa SDL sul main thread (parità RunMainLoop)
            int w = TermWidth, h = TermHeight;
            if (w != _lastW || h != _lastH)
            {
                _lastW = w; _lastH = h;
                if (_activeConsole == null)   // SDL/Standard: clear/colori li gestisce la console
                {
                    ClearConsoleScreen();
                    ApplyConsoleColors();
                    Out.WriteLine("READY.");
                    Out.Write(_inputBuffer.ToString());
                    SetCursorPos(_inputCursorPos, 1);
                }
            }
            if (!IsInteractive) { if (_cliArgs == null || !_cliArgs.IsScriptMode) return; Thread.Sleep(100); continue; }
            ProcessReplInput(_runCts.Token);
            if (_useSdl) { Present(); Thread.Sleep(1); }
            else if (_activeConsole != null) { (_activeConsole as StandardConsole)?.PollForResize(); Present(); Thread.Sleep(16); }
            else Thread.Sleep(16);
        }

        Out.WriteLine("\nGoodbye.");
        ResetConsoleColors();
    }

    private static void ApplyConsolePalette()
    {
        var pal = ScreenConfig.Palettes.GetConsoleNamed().ToRgbArray();
        var bg = pal[1]; var fg = pal[15];
        Out.Write($"\x1b[48;2;{bg[0]};{bg[1]};{bg[2]}m\x1b[38;2;{fg[0]};{fg[1]};{fg[2]}m\x1b[2J\x1b[H");
    }

    private static void ApplyConsoleColors()
    {
        var pal = ScreenConfig.Palettes.GetConsoleNamed().ToRgbArray();
        var bg = pal[1]; var fg = pal[15];
        Out.Write($"\x1b[48;2;{bg[0]};{bg[1]};{bg[2]}m\x1b[38;2;{fg[0]};{fg[1]};{fg[2]}m");
    }

    private static void ClearConsoleScreen()
    {
        var pal = ScreenConfig.Palettes.GetConsoleNamed().ToRgbArray();
        var bg = pal[1];
        Out.Write($"\x1b[48;2;{bg[0]};{bg[1]};{bg[2]}m\x1b[2J\x1b[H");
    }

    private static void ResetConsoleColors() => Out.Write("\x1b[0m");
    private static void DisableMouseTracking() => Out.Write("\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l");
    private static void WriteError(string message) { Out.Write("\x1b[33m"); Out.WriteLine(message); Out.Write("\x1b[0m"); ApplyConsoleColors(); }

    private void ResetRunCts()
    {
        var old = Interlocked.Exchange(ref _runCts, new CancellationTokenSource());
        try { old?.Dispose(); } catch { /* ignore */ }
    }

    /// <summary>PCodeVm C# in-process (--pcode): nessun confine di processo da rispettare,
    /// è puro C# come il resto di Executor — a differenza del default (RunPCodeSafe), che
    /// ora spawna gmb_vm_host come processo separato (vedi GmbVmHostRunner).</summary>
    private void RunPCodeVmManagedSafe(string file)
    {
        var savedCwd = Directory.GetCurrentDirectory();
        // Host grafico SOLO su terminale interattivo: se lo stdout non e' un TTY (pipe/CI/
        // capture) eseguiamo headless — niente SDL da aprire, i park grafici completano subito.
        var savedHeadless = Environment.GetEnvironmentVariable("GMBASIC_HEADLESS");
        var headless = Console.IsOutputRedirected || savedHeadless == "1";
        if (headless) Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", "1");
        try
        {
            // file va reso assoluto PRIMA di cambiare CWD: altrimenti Orchestrator.Compile
            // (poco sotto) risolverebbe un path relativo contro la CWD appena cambiata
            // invece che contro quella originale — bug verificato dal vivo (percorso
            // raddoppiato: ".../Files/samples/Files/x.bas").
            file = Path.GetFullPath(file);
            var fileDir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(fileDir)) Directory.SetCurrentDirectory(fileDir);

            // Entry-point del programma: SET PATH di default = la sua cartella, e base per
            // PROJECTFOLDER/GETFOLDERPATH (APPPATH espose la lista SET PATH).
            global::Basic.IlBackend.IlRuntime.SetEntrySourcePath(file);

            if (OperatingSystem.IsWindows()) Console.CursorVisible = false;
            var program = global::Basic.Orchestrator.Compile(file, null, false, "vm");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }

            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);
            chunk = MergeVmChunks(chunk, program);

            // Pre-carica le librerie FFI dei pacchetti REQUIRE risolti: il FfiService del
            // PCodeVm (istanza fresca a ogni run) le risolve per basename (DECLARE LIB "nome").
            foreach (var lib in program.ResolvedFfiLibs)
            {
                if (string.IsNullOrEmpty(lib) || !File.Exists(lib)) continue;
                try { global::Basic.Runtime.Services.FfiService.Preload(lib); } catch { /* best effort */ }
            }

            // Output LIVE sulla console (tee) + cattura per l'exit code: prima si stampava
            // tutto alla fine, quindi un daemon (webapp) non scriveva nulla nel log finché
            // non usciva. `result.Output` NON va ristampato (è già passato dal tee).
            var result = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(
                chunk, _runCts.Token, prepareGraphicsHost: !headless, liveOut: Console.Out);
            Out.WriteLine(result.Success ? "\nExecution completed." : $"\n[P-CODE ERROR] exit code {result.ExitCode}");
        }
        finally
        {
            if (OperatingSystem.IsWindows()) Console.CursorVisible = true;
            if (headless && savedHeadless == null) Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", null);
            Directory.SetCurrentDirectory(savedCwd);
        }
    }

    /// <summary>Esegue un .bas tramite il BACKEND IL in-process (CompileToAssembly +
    /// reflection-invoke). È il percorso usato quando sono caricati moduli IL esterni
    /// (--il-module): il compilatore P-Code/VM non può chiamare MethodInfo .NET.</summary>
    private void RunIlManagedSafe(string file)
    {
        var savedCwd = Directory.GetCurrentDirectory();
        // Come --pcode: host grafico solo su TTY; altrimenti headless (niente SDL/park appesi).
        var savedHeadlessIl = Environment.GetEnvironmentVariable("GMBASIC_HEADLESS");
        var headlessIl = Console.IsOutputRedirected || savedHeadlessIl == "1";
        if (headlessIl) Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", "1");
        try
        {
            file = Path.GetFullPath(file);
            var fileDir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(fileDir)) Directory.SetCurrentDirectory(fileDir);

            // Entry-point del programma: SET PATH di default = la sua cartella, e base per
            // PROJECTFOLDER/GETFOLDERPATH (APPPATH espose la lista SET PATH).
            global::Basic.IlBackend.IlRuntime.SetEntrySourcePath(file);

            var program = global::Basic.Orchestrator.Compile(file, null, false, "il");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }
            // Esecuzione IL IN-PROCESS: il BasicProgram è vivo → cattura globali di EVAL/&/macro
            // contro program.Symbols (SetMacroContext). Senza, EVAL/& su variabili GLOBALI falliscono
            // silenziosamente (solo gli eseguibili separati usano la tabella .globals.json).
            global::Basic.IlBackend.IlRuntime.SetMacroContext(program);

            var asmBuilder = global::Basic.IlBackend.IlProgramCompiler.CompileToAssembly(
                program, "GmbIl_" + Path.GetFileNameWithoutExtension(file), out _);
            var dll = Path.Combine(Path.GetTempPath(), "GmbIl_" + Guid.NewGuid().ToString("N") + ".dll");
            try
            {
                using (var fs = File.Create(dll)) asmBuilder.Save(fs);
                var assembly = global::System.Reflection.Assembly.LoadFile(dll);
                var programType = assembly.GetType("GeneratedProgram");
                var main = programType?.GetMethod("Main", global::System.Reflection.BindingFlags.Public | global::System.Reflection.BindingFlags.Static);
                if (main == null) { WriteError("?Backend IL: entry point non trovato."); return; }

                var originalOut = Console.Out;
                var sw = new StringWriter();
                // Tee: l'output del programma va live sulla console E nella cattura
                // (prima finiva solo in `sw` e si vedeva soltanto alla fine).
                var tee = new CommonLib.TeeTextWriter(sw, originalOut);
                try
                {
                    Console.SetOut(tee);
                    var task = Task.Run(() => main.Invoke(null, null));
                    if (!task.Wait(TimeSpan.FromSeconds(60)))
                    {
                        WriteError("?Backend IL: timeout di esecuzione.");
                        return;
                    }
                    task.GetAwaiter().GetResult();
                    Console.SetOut(originalOut);
                }
                finally { Console.SetOut(originalOut); }
            }
            finally { try { File.Delete(dll); } catch { } }
        }
        finally
        {
            // Ferma il feeder console avviato dal prologo del Main IL emesso (il REPL
            // riprende il prompt con ProcessReplInput che legge System.Console — un feeder
            // ancora attivo gli ruberebbe i tasti). In esecuzione flat l'eseguibile termina
            // col processo; qui serve lo Stop esplicito a fine run in-process.
            Basic.IlBackend.IlConsoleKeyFeeder.Stop();
            if (headlessIl && savedHeadlessIl == null) Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", null);
            Directory.SetCurrentDirectory(savedCwd);
        }
    }

    private void RunPCodeSafe(string file)
    {
        var token = _runCts.Token;
        var savedCwd = Directory.GetCurrentDirectory();
        try
        {
            // CWD = directory del programma: i sample con path relativi (es. fixtures in
            // "samples/Fixtures") creano i loro file NELLA LORO directory, non nella root
            // del repo (come GMBasic).
            // file va reso assoluto PRIMA di cambiare CWD: altrimenti Orchestrator.Compile
            // (poco sotto) risolverebbe un path relativo contro la CWD appena cambiata
            // invece che contro quella originale — bug verificato dal vivo (percorso
            // raddoppiato: ".../Files/samples/Files/x.bas").
            file = Path.GetFullPath(file);
            var fileDir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(fileDir)) Directory.SetCurrentDirectory(fileDir);

            // Entry-point del programma: SET PATH di default = la sua cartella, e base per
            // PROJECTFOLDER/GETFOLDERPATH (APPPATH espose la lista SET PATH).
            global::Basic.IlBackend.IlRuntime.SetEntrySourcePath(file);

            if (OperatingSystem.IsWindows()) Console.CursorVisible = false;
            var program = global::Basic.Orchestrator.Compile(file, null, false, "vm");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }

            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);
            chunk = MergeVmChunks(chunk, program);

            // Le librerie FFI dei pacchetti REQUIRE risolti vengono pre-caricate in
            // gmb_vm_host (--ffi-lib): DECLARE ... LIB "nome" le risolve per basename.
            RunGmbVmHost(chunk, token, program.ResolvedFfiLibs);
        }
        catch (OperationCanceledException) { /* Ctrl+C */ }
        finally
        {
            if (OperatingSystem.IsWindows()) Console.CursorVisible = true;
            Directory.SetCurrentDirectory(savedCwd);
        }
    }

    /// <summary>Esegue un CompiledChunk con l'Executor C standalone (GmbVmHostRunner,
    /// processo separato — mai più P/Invoke in-process come il vecchio VmCBridge) e
    /// stampa l'output in streaming sulla console, come faceva prima VmCBridge.Run.</summary>
    private void RunGmbVmHost(global::Compiler.CompiledChunk chunk, CancellationToken token,
        IReadOnlyList<string>? ffiLibs = null)
    {
        var exitCode = GmbVmHostRunner.Run(chunk, token,
            onOutLine: Console.WriteLine,
            onErrLine: line => Console.Error.WriteLine(line),
            ffiLibs: ffiLibs);
        if (exitCode == -1) return; // errore già segnalato da GmbVmHostRunner
        if (!token.IsCancellationRequested)
        {
            if (exitCode != 0)
                Out.WriteLine($"\n[VM/C ERROR] exit code {exitCode}");
            else
                Out.WriteLine("\nExecution completed.");
        }
    }

    /// <summary>Fonde i chunk VM dei pacchetti REQUIRE (vm-chunk) nel chunk principale:
    /// .chunk (A1/GMBC, formato completo con globals/procs) → ChunkA1Serializer.Deserialize,
    /// .prg (subset legacy) → PrgFormat.Read. Poi ChunkMerger.Merge (re-basing di
    /// costanti/globali/indirizzi). Se non ci sono pacchetti chunk, ritorna il chunk invariato.</summary>
    private static global::Compiler.CompiledChunk MergeVmChunks(global::Compiler.CompiledChunk main,
        global::Basic.BasicProgram program)
    {
        if (program.ResolvedVmChunks.Count == 0) return main;
        var libs = new List<global::Compiler.CompiledChunk>();
        foreach (var path in program.ResolvedVmChunks)
        {
            if (!File.Exists(path)) continue;
            try
            {
                if (path.EndsWith(".prg", StringComparison.OrdinalIgnoreCase))
                {
                    var p = global::Basic.Runtime.PrgFormat.Read(path);
                    var lib = new global::Compiler.CompiledChunk();
                    lib.Code.AddRange(p.Code);
                    foreach (var c in p.Constants) lib.Constants.Add(c);
                    foreach (var l in p.Lines) lib.Lines.Add(l);
                    libs.Add(lib);
                }
                else
                {
                    libs.Add(global::Basic.Runtime.ChunkA1Serializer.Deserialize(File.ReadAllBytes(path)));
                }
            }
            catch (Exception ex)
            {
                Out.WriteLine($"?Chunk VM di pacchetto non caricabile ({path}): {ex.GetBaseException().Message}");
            }
        }
        return libs.Count == 0 ? main : global::Compiler.ChunkMerger.Merge(main, libs,
            program.ChunkExternProcedureNames);
    }
    private void RunNativeSafe(string file)
    {
        var token = _runCts.Token;
        System.Diagnostics.Process? proc = null;
        try
        {
            if (OperatingSystem.IsWindows()) Console.CursorVisible = false;
            var program = global::Basic.Orchestrator.Compile(file, null, false, "il");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }

            var outputDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin");
            // -m = ESECUZIONE (iterazione): la closure del runtime va solo RISOLTA, non
            // copiata. Copiarla costava 262 MB e 178 file a ogni run (onnxruntime + torch/MLX
            // + 114 dll) per eseguire un `PRINT`. Con i symlink (stesso meccanismo dei
            // runner di test, vedi linkRuntime) il run resta <1 s e non consuma disco.
            // La copia REALE serve solo alla distribuzione: `-full` (o COMPILE ... MANAGED).
            var exePath = global::Basic.IlBackend.IlProgramCompiler.Compile(program, file, outputDir, Path.GetFileNameWithoutExtension(file),
                noOptimize: _opts.NoOptimize, dumpIl: _opts.DumpIl, linkRuntime: !_opts.Full);
            CommonLib.ProgramBundle.CopyNextTo(outputDir, file, Out.WriteLine);
            Out.WriteLine($"[IL] compiled -> {exePath}");

            if (_opts.Full)
            {
                CopyNetRuntime(outputDir);
                Out.WriteLine($"[IL] full: .NET runtime copied to {outputDir}");
            }

            var effectiveExe = exePath;
            var effectiveArgs = OperatingSystem.IsWindows() ? "" : $"exec \"{exePath}.dll\"";
            var psi = new System.Diagnostics.ProcessStartInfo(effectiveExe, effectiveArgs)
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(exePath) ?? ".",
            };

            proc = System.Diagnostics.Process.Start(psi);
            if (proc == null) return;

            using var reg = token.Register(() =>
            {
                try { proc.Kill(true); } catch { /* ignore */ }
            });

            // Streaming RAW (non per-riga): il TUI scrive con ANSI escape (SetCursorPosition/
            // colori) SENZA newline — per-riga (BeginOutputReadLine) tratterrebbe il frame
            // come righe mai terminate e l'output comparirebbe solo a fine processo (TUI
            // morta, event-loop "apparentemente" perso). ReadLineAsync/by-line va bene per
            // output testuale, non per ANSI raw.
            var outStream = proc.StandardOutput.BaseStream;
            var errStream = proc.StandardError.BaseStream;
            var outTask = Task.Run(() => outStream.CopyToAsync(Console.OpenStandardOutput()));
            var errTask = Task.Run(() => errStream.CopyToAsync(Console.OpenStandardError()));
            proc.WaitForExit();
            try { Task.WaitAll(outTask, errTask); } catch { /* pipe chiusa a fine processo */ }

            if (!token.IsCancellationRequested)
                Out.WriteLine("\n[Native] Execution completed.");
        }
        catch (OperationCanceledException) { /* Ctrl+C */ }
        finally { if (OperatingSystem.IsWindows()) Console.CursorVisible = true; proc?.Dispose(); }
    }


    /// <summary>
    /// Compila un programma BASIC in un VERO eseguibile nativo (NativeAOT, nessun runtime
    /// .NET richiesto né incorporato) e lo esegue. Usa <c>--aot</c>.
    /// </summary>
    private void RunNativeAotSafe(string file)
    {
        try
        {
            var program = global::Basic.Orchestrator.Compile(file, null, false, "il");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }

            var outputDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin", "native");
            var exePath = global::Basic.IlBackend.IlProgramCompiler.CompileNative(program, file, outputDir);
            Out.WriteLine($"[NATIVE] compiled -> {exePath}");

            // WorkingDirectory esplicito: senza, il processo erediterebbe la CWD di
            // Executor (root del repo, se lanciato da lì) invece di scrivere accanto
            // all'eseguibile compilato — causa storica di sporcizia in root (AGENTS.md §9).
            var psi = new System.Diagnostics.ProcessStartInfo(exePath) { UseShellExecute = false, WorkingDirectory = outputDir };
            using var proc = System.Diagnostics.Process.Start(psi);
            if (proc == null) { WriteError("?Native process non avviato."); return; }
            proc.WaitForExit();
        }
        catch (Exception ex)
        {
            WriteError($"?Native AOT: {ex.Message}");
        }
    }

    private void RunPrgVmSafe()
    {
        var token = _runCts.Token;
        var savedCwd = Directory.GetCurrentDirectory();
        try
        {
            if (OperatingSystem.IsWindows()) Console.CursorVisible = false;

            // CWD = cartella del .prg (se nota): senza questo, gmb_vm_host erediterebbe
            // la CWD di Executor stesso (root del repo, se lanciato da lì) invece di
            // scrivere accanto al file — causa storica di sporcizia in root (AGENTS.md §9).
            if (!string.IsNullOrEmpty(_prgChunkPath))
            {
                var dir = Path.GetDirectoryName(Path.GetFullPath(_prgChunkPath));
                if (!string.IsNullOrEmpty(dir)) Directory.SetCurrentDirectory(dir);
            }

            var chunk = new global::Compiler.CompiledChunk();
            foreach (var b in _prgChunk!.Code) chunk.Code.Add(b);
            foreach (var c in _prgChunk.Constants) chunk.Constants.Add(c);
            foreach (var l in _prgChunk.Lines) chunk.Lines.Add(l);

            RunGmbVmHost(chunk, token);
        }
        finally
        {
            if (OperatingSystem.IsWindows()) Console.CursorVisible = true;
            Directory.SetCurrentDirectory(savedCwd);
        }
    }

    private void PrintWelcome()
    {
        // Versione dell'engine: stessa di `GMB_PKG_ENGINE_VERSION` (gate minGmbVersion).
        const string title = "GMBASIC 1.0.0";
        const string copyright = "(C) 2026 MINARDI GAETANO. ALL RIGHTS RESERVED.";
        var mode = _cliArgs?.UseC64Mode == true ? "C64 MODE"
                 : _useSdl ? "SDL CONSOLE MODE"
                 : (_cliArgs?.UseSystemConsole == true ? "SYSTEM CONSOLE MODE" : "STANDARD CONSOLE MODE");
        var w = TermWidth;
        Out.WriteLine();
        Out.WriteLine(Center(title, w));
        Out.WriteLine(Center(copyright, w));
        Out.WriteLine(Center($"{mode} - C# VERSION", w));
        Out.WriteLine();
        Out.WriteLine("READY.");

        static string Center(string s, int width)
        {
            if (s.Length >= width) return s;
            var padding = (width - s.Length) / 2;
            return s.PadLeft(s.Length + padding);
        }
    }

    // Esegue un comando REPL in modo ASINCRONO (non blocca il loop). Le `await` tornano al
    // main loop via LoopSyncContext; a fine comando stampa READY.+prompt e presenta.
    private async Task RunCommandAsync(string commandToRun, string prompt, CancellationToken ct)
    {
        try { await HandleReplInput(commandToRun, ct); }
        catch (Exception ex) { Out.WriteLine($"?{ex.Message}"); }
        finally
        {
            _isReplCommandExecuting = false;
            ApplyConsoleColors();
            Out.WriteLine("READY.");
            Out.Write(prompt);
            Present();
        }
    }

    private void ProcessReplInput(CancellationToken cancellationToken)
    {
        if (_isReplCommandExecuting) return;
        if (!IsInteractive) return;

        char keyChar; ConsoleKey key;
        if (_kb != null)   // console attiva: un tasto non-bloccante dal keyboard service
                           // (parità Repl.ProcessReplInput: _keyboardService.TryReadKey(false))
        {
            var ki = _kb.TryReadKey(false);
            if (ki is not { } k1) return;
            keyChar = k1.KeyChar; key = k1.Key;
        }
        else                    // terminale: System.Console
        {
            if (!Console.KeyAvailable) return;
            var ki = Console.ReadKey(true);
            keyChar = ki.KeyChar; key = ki.Key;
        }

        var prompt = _autoNext > 0 ? $"{_autoNext} " : _prompt;
        var startColumn = prompt.Length;
        var isTypingNumberedLine = _inputBuffer.Length > 0 && char.IsDigit(_inputBuffer[0]);

        if (char.IsLetter(keyChar) && !isTypingNumberedLine)
            keyChar = char.ToUpperInvariant(keyChar);

        switch (key)
        {
            case ConsoleKey.Enter:
                _isReplCommandExecuting = true;
                {
                    var commandToRun = _inputBuffer.ToString();
                    _inputBuffer.Clear();
                    _inputCursorPos = 0;
                    Out.WriteLine();
                    // ASYNC NON BLOCCANTE (GPM §0ter): il comando gira come task async, le sue
                    // `await` tornano sul main loop (drenate); il loop NON si blocca e continua
                    // a pompare SDL (PollEvents/Present). Niente GetResult() → niente freeze.
                    _ = RunCommandAsync(commandToRun, prompt, cancellationToken);
                }
                break;

            case ConsoleKey.Backspace:
                if (_inputCursorPos > 0)
                {
                    _historyIndex = _commandHistory.Count;
                    _inputCursorPos--;
                    _inputBuffer.Remove(_inputCursorPos, 1);
                    RedrawLine(prompt);
                }
                break;

            case ConsoleKey.Delete:
                if (_inputCursorPos < _inputBuffer.Length)
                {
                    _historyIndex = _commandHistory.Count;
                    _inputBuffer.Remove(_inputCursorPos, 1);
                    RedrawLine(prompt);
                }
                break;

            case ConsoleKey.LeftArrow:
                if (_inputCursorPos > 0) { _inputCursorPos--; CurLeft = startColumn + _inputCursorPos; }
                break;

            case ConsoleKey.RightArrow:
                if (_inputCursorPos < _inputBuffer.Length) { _inputCursorPos++; CurLeft = startColumn + _inputCursorPos; }
                break;

            case ConsoleKey.Home:
                _inputCursorPos = 0; CurLeft = startColumn;
                break;

            case ConsoleKey.End:
                _inputCursorPos = _inputBuffer.Length; CurLeft = startColumn + _inputCursorPos;
                break;

            case ConsoleKey.UpArrow:
                if (_commandHistory.Count > 0)
                {
                    _historyIndex = Math.Max(0, _historyIndex - 1);
                    var cmd = _commandHistory[_historyIndex];
                    _inputBuffer.Clear().Append(cmd);
                    _inputCursorPos = _inputBuffer.Length;
                    RedrawLine(prompt);
                }
                break;

            case ConsoleKey.DownArrow:
                if (_commandHistory.Count > 0)
                {
                    _historyIndex++;
                    if (_historyIndex >= _commandHistory.Count)
                    {
                        _historyIndex = _commandHistory.Count;
                        _inputBuffer.Clear();
                    }
                    else
                    {
                        _inputBuffer.Clear().Append(_commandHistory[_historyIndex]);
                    }
                    _inputCursorPos = _inputBuffer.Length;
                    RedrawLine(prompt);
                }
                break;

            default:
                if (!char.IsControl(keyChar))
                {
                    _historyIndex = _commandHistory.Count;
                    _inputBuffer.Insert(_inputCursorPos, keyChar);
                    _inputCursorPos++;
                    RedrawLine(prompt);
                }
                break;
        }
    }

    private void RedrawLine(string prompt)
    {
        var top = CurTop;
        SetCursorPos(0, top);
        // Cancella la riga corrente in modo ROBUSTO: TermWidth può essere 0/1 in un
        // terminale ANSI non ancora dimensionato (pty/harness) → new string(' ', -1) lancia
        // ArgumentOutOfRangeException al primo carattere digitato (REPL C# non riceveva input).
        var width = TermWidth;
        if (width > 1)
            Out.Write(new string(' ', width - 1));
        else
            Out.Write("\x1b[2K"); // ANSI erase-line (fallback quando la larghezza è ignota)
        SetCursorPos(0, top);
        Out.Write(prompt);
        Out.Write(_inputBuffer.ToString());
        SetCursorPos(prompt.Length + _inputCursorPos, top);
    }

    private async Task HandleReplInput(string input, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(input)) return;

        if (_commandHistory.Count == 0 || _commandHistory[^1] != input)
            _commandHistory.Add(input);

        if (input.StartsWith('?'))
        {
            await HandleHelpAsync(input[1..].Trim(), cancellationToken);
            return;
        }

        if (uint.TryParse(input.Trim().Split(' ')[0], out var lineNumber))
        {
            var spaceIdx = input.Trim().IndexOf(' ');
            if (spaceIdx < 0)
                _programLines.Remove((int)lineNumber);
            else
                _programLines[(int)lineNumber] = input.Trim();
            _replProgramSource = string.Join(Environment.NewLine, _programLines.Values);
            return;
        }

        var commandMatch = Regex.Match(input.Trim().ToUpper(), @"^[A-Z_]+");
        if (commandMatch.Success && _commands.TryGetValue(commandMatch.Value, out var handler))
        {
            var args = input.Trim()[commandMatch.Length..].Trim();
            await handler(args, cancellationToken);
            return;
        }

        if (!input.Trim().StartsWith('\''))
            WriteError("?Unknown command or invalid input.");
    }

    // ================================================================
    // COMMAND HANDLERS
    // ================================================================

    private Task HandleQuitAsync(string _, CancellationToken __) { _quitting = true; return Task.CompletedTask; }
    private Task HandleClsAsync(string _, CancellationToken __) { ApplyConsolePalette(); return Task.CompletedTask; }
    private Task HandlePromptAsync(string args, CancellationToken __) { var t = args.Trim(); _prompt = t.Length > 0 ? t + " " : ""; return Task.CompletedTask; }

    private Task HandleGpmAsync(string args, CancellationToken __)
    {
        var t = args.Trim();
        if (t.Length == 0)
        {
            Out.WriteLine($"GPM: {_opts.GpmMode}");
            return Task.CompletedTask;
        }
        if (Enum.TryParse<C99GpmMode>(t, true, out var m))
        {
            _opts = _opts with { GpmMode = m };
            Out.WriteLine($"GPM: {m}");
        }
        else
        {
            WriteError("?GPM auto|always|linear");
        }
        return Task.CompletedTask;
    }

    private Task HandleHelpAsync(string args, CancellationToken __)
    {
        if (_helpIndex == null) return Task.CompletedTask;

        new HelpForm(_helpIndex).Show();
        return Task.CompletedTask;
    }

    private Task HandleListAsync(string _, CancellationToken __)
    {
        foreach (var (num, line) in _programLines)
            Out.WriteLine(line.Contains(' ') ? line : $"{num} {line}");
        if (_programLines.Count == 0)
            Out.WriteLine("(no program lines)");
        return Task.CompletedTask;
    }

    private Task HandleEditAsync(string args, CancellationToken __)
    {
        var fileName = args.Trim().Trim('"');
        if (string.IsNullOrEmpty(fileName))
            fileName = _replProgramSource.Length > 0 ? _replProgramSource : "untitled.bas";
        else if (!fileName.EndsWith(".bas", StringComparison.OrdinalIgnoreCase))
            fileName += ".bas";

        if (File.Exists(fileName) && _programLines.Count == 0)
            LoadFileIntoProgram(fileName);

        var editor = new GMBasicIde(_programLines, fileName);
        editor.Run();

        _replProgramSource = fileName;
        RestoreReplScreen($"Editor closed. {_programLines.Count} lines in memory.");
        return Task.CompletedTask;
    }

    private void LoadFileIntoProgram(string path)
    {
        try
        {
            _programLines.Clear();
            foreach (var line in File.ReadAllLines(path))
            {
                var trimmed = line.TrimStart();
                var space = trimmed.IndexOf(' ');
                if (space > 0 && int.TryParse(trimmed[..space], out var num))
                    _programLines[num] = trimmed[(space + 1)..];
            }
        }
        catch { }
    }

    private Task HandleNewAsync(string _, CancellationToken __)
    {
        _programLines.Clear();
        _replProgramSource = string.Empty;
        _prgChunk = null;
        _usePrgVm = false;
        Out.WriteLine("Program cleared.");
        return Task.CompletedTask;
    }

    private Task HandleAutoAsync(string args, CancellationToken __)
    {
        if (string.IsNullOrWhiteSpace(args)) { _autoNext = 10; _autoInc = 10; }
        else
        {
            var parts = args.Split(',', ' ');
            if (int.TryParse(parts[0], out var start)) _autoNext = start;
            if (parts.Length > 1 && int.TryParse(parts[^1], out var inc)) _autoInc = inc;
        }
        return Task.CompletedTask;
    }

    private async Task HandleSaveAsync(string args, CancellationToken cancellationToken)
    {
        var filename = args.Trim();
        if (string.IsNullOrWhiteSpace(filename)) filename = "program.bas";
        if (!filename.EndsWith(".bas", StringComparison.OrdinalIgnoreCase)) filename += ".bas";

        var path = Path.Combine(_projectPath, filename);
        var content = string.Join(Environment.NewLine, _programLines.Values);
        await File.WriteAllTextAsync(path, content, cancellationToken);
        Out.WriteLine($"Saved to {path}");
    }

    private async Task HandleLoadAsync(string args, CancellationToken cancellationToken)
    {
        var filename = args.Trim();
        if (string.IsNullOrWhiteSpace(filename)) { WriteError("?Usage: LOAD <filename>"); return; }
        if (!filename.EndsWith(".bas", StringComparison.OrdinalIgnoreCase)) filename += ".bas";

        var path = Path.Combine(_projectPath, filename);
        if (!File.Exists(path)) { WriteError($"?File not found: {path}"); return; }

        _programLines.Clear();
        _replProgramSource = await File.ReadAllTextAsync(path, cancellationToken);
        var lines = _replProgramSource.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries);
        foreach (var line in lines)
        {
            var trimmed = line.Trim();
            if (string.IsNullOrWhiteSpace(trimmed)) continue;
            if (uint.TryParse(trimmed.Split(' ')[0], out var num))
                _programLines[(int)num] = trimmed;
        }
        Out.WriteLine($"Loaded {_programLines.Count} lines from {filename}");
    }

    private Task HandleDeleteAsync(string args, CancellationToken __)
    {
        if (string.IsNullOrWhiteSpace(args)) { WriteError("?Usage: DELETE <line> or DELETE <start>-<end>"); return Task.CompletedTask; }
        if (args.Contains('-'))
        {
            var parts = args.Split('-');
            if (int.TryParse(parts[0].Trim(), out var start) && int.TryParse(parts[1].Trim(), out var end))
                foreach (var k in _programLines.Keys.Where(k => k >= start && k <= end).ToList())
                    _programLines.Remove(k);
        }
        else if (int.TryParse(args.Trim(), out var lineNum))
        {
            _programLines.Remove(lineNum);
        }
        _replProgramSource = string.Join(Environment.NewLine, _programLines.Values);
        return Task.CompletedTask;
    }

    private Task HandleRenumAsync(string _, CancellationToken __)
    {
        if (_programLines.Count == 0) return Task.CompletedTask;
        var lines = _programLines.ToList();
        _programLines.Clear();
        var newNum = 10;
        foreach (var (_, line) in lines)
        {
            var content = line.Contains(' ') ? line[(line.IndexOf(' ') + 1)..] : line;
            _programLines[newNum] = $"{newNum} {content}";
            newNum += 10;
        }
        _replProgramSource = string.Join(Environment.NewLine, _programLines.Values);
        Out.WriteLine("Renumbered.");
        return Task.CompletedTask;
    }

    private int _sampleFooterLine;

    private void DisplaySampleList(IReadOnlyList<SampleInfo> samples)
    {
        if (samples.Count == 0) { Out.WriteLine("(Nessun file .bas trovato)"); return; }
        var w = TermWidth;
        var iw = samples.Count.ToString().Length;
        var nw = Math.Min(40, w - iw - 15);
        var dw = w - iw - nw - 10;
        if (dw < 10) dw = 10;
        Out.WriteLine($"{"#".PadLeft(iw)}  {"Nome".PadRight(nw)}  {"Descrizione".PadRight(dw)}");
        Out.WriteLine(new string('-', iw + nw + dw + 4));
        for (var i = 0; i < samples.Count; i++)
        {
            var s = samples[i];
            var idx = (i + 1).ToString().PadLeft(iw);
            var name = s.DisplayName.Length > nw ? s.DisplayName[..(nw - 3)] + "..." : s.DisplayName.PadRight(nw);
            var desc = s.Description.Length > dw ? s.Description[..(dw - 3)] + "..." : s.Description.PadRight(dw);
            Out.WriteLine($"{idx}  {name}  {desc}");
        }
        Out.WriteLine(new string('-', iw + nw + dw + 4));
        _sampleFooterLine = CurTop;
    }

    private SampleInfo? FindOrSelectSample(string args)
    {
        var arg = args.Trim();
        if (int.TryParse(arg, out var num))
        {
            var list = _activeSampleList.Count > 0 ? _activeSampleList : _masterSampleCatalog ?? (IReadOnlyList<SampleInfo>)Array.Empty<SampleInfo>();
            if (list.Count == 0) { WriteError("?Nessun campione disponibile."); return null; }
            if (num < 1 || num > list.Count) { WriteError($"?Numero non valido. Usare 1-{list.Count}."); return null; }
            return list[num - 1];
        }
        var catalog = _masterSampleCatalog;
        if (catalog == null || catalog.Count == 0) { WriteError("?Catalogo sample non disponibile."); return null; }
        if (string.IsNullOrEmpty(arg))
        {
            _activeSampleList = catalog;
            Out.WriteLine($"Elenco completo dei sample ({catalog.Count}):");
            DisplaySampleList(_activeSampleList);
            Out.WriteLine("Usare [SAMPLE | AST | DASM] <numero> per selezionare.");
            return null;
        }
        var filtered = catalog.Where(s =>
            s.Description.Contains(arg, StringComparison.OrdinalIgnoreCase) ||
            s.DisplayName.Contains(arg, StringComparison.OrdinalIgnoreCase) ||
            s.FullContent.Contains(arg, StringComparison.OrdinalIgnoreCase)).ToList();
        _activeSampleList = filtered;
        Out.WriteLine($"Trovati {filtered.Count} sample per '{arg}':");
        DisplaySampleList(filtered);
        Out.WriteLine("Usare [SAMPLE | AST | DASM] <numero> per selezionare da questo elenco.");
        return null;
    }

    private Task HandleSampleAsync(string args, CancellationToken __)
    {
        var parts = args.Trim().Split([' '], StringSplitOptions.RemoveEmptyEntries);
        var raw = false;
        var filtered = new List<string>();
        foreach (var p in parts)
            if (string.Equals(p, "-r", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "-raw", StringComparison.OrdinalIgnoreCase)) raw = true;
            else filtered.Add(p);

        var selected = FindOrSelectSample(string.Join(" ", filtered));
        if (selected == null) return Task.CompletedTask;
        try
        {
            var source = File.ReadAllText(selected.FilePath);
            _programLines.Clear();
            _replProgramSource = source;
            foreach (var line in source.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
            {
                var trimmed = line.Trim();
                if (string.IsNullOrWhiteSpace(trimmed)) continue;
                if (uint.TryParse(trimmed.Split(' ')[0], out var n)) _programLines[(int)n] = trimmed;
            }

            if (raw)
            {
                ApplyConsolePalette();
                Out.WriteLine($"--- {selected.DisplayName} ---");
                Out.WriteLine(source);
            }
            else
            {
                var editor = new GWBasicEditor(_programLines, selected.DisplayName);
                editor.Run(false);
                _replProgramSource = selected.FilePath;
                RestoreReplScreen($"Editor closed. {_programLines.Count} lines in memory.");
            }
        }
        catch (Exception ex) { WriteError($"?Errore: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private void RestoreReplScreen(string? status = null)
    {
        DisableMouseTracking();
        ApplyConsolePalette();
        if (status != null) Out.WriteLine(status);
        Out.Write(_inputBuffer.ToString());
        SetCursorPos(_inputCursorPos, status != null ? 2 : 1);
    }

    private async Task HandleCompileAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var (managed, native) = ParseBackendFlags(parts);
        var dumpFlag = parts.Contains("-dump");
        var full = parts.Contains("-full");
        // -all: distribuzione managed COMPLETA (nessun filtro ClosureSlim). Il filtro di
        // default omette i gruppi non raggiungibili (CONVERT/onnx/provider SQL) e i binari
        // degli altri RID; è stampato a ogni esclusione, e -all è la via per averli tutti.
        var all = parts.Contains("-all");
        var ridIdx = Array.IndexOf(parts, "-r");
        var rid = ridIdx >= 0 && ridIdx + 1 < parts.Length ? parts[ridIdx + 1] : null;
        var dirIdx = Array.IndexOf(parts, "-d");
        var outDirName = dirIdx >= 0 && dirIdx + 1 < parts.Length ? parts[dirIdx + 1] : null;
        var excludeFlags = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "-n", "-m", "-native", "-managed", "-dump", "-full", "-all", "-r", "-d" };
        if (rid != null) excludeFlags.Add(rid);
        if (outDirName != null) excludeFlags.Add(outDirName);
        var fileArgs = parts.Where(p => !excludeFlags.Contains(p)).ToArray();
        var file = fileArgs.Length > 0 ? string.Join(" ", fileArgs).Trim().Trim('"') : null;

        if (string.IsNullOrWhiteSpace(file))
        {
            WriteError("?Usage: COMPILE <file.bas> [-n|-m] [-dump] [-full] [-all] [-r <rid>] [-d <out_dir>]");
            return;
        }

        if (!File.Exists(file))
        {
            WriteError($"?File not found: {file}");
            return;
        }

        try
        {
            var program = global::Basic.Orchestrator.Compile(file, null, false);
            if (program?.MainModule == null)
            {
                WriteError("?Compilation failed.");
                return;
            }

            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);

            var outputDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, outDirName ?? "bin");
            Directory.CreateDirectory(outputDir);
            // Il compilato si distribuisce SENZA compilatore: porta con sé settings e asset
            // (font/sprite). Vale per OGNI modalità, anche quella senza librerie.
            var bundled = CommonLib.ProgramBundle.CopyNextTo(outputDir, file, Out.WriteLine);
            if (bundled.Count > 0) Out.WriteLine($"[bundle] {bundled.Count} file accanto all'eseguibile (settings + Resources)");

            var prgFile = Path.Combine(outputDir, Path.GetFileNameWithoutExtension(file) + ".prg");
            var prg = new PrgChunk
            {
                Code = [.. chunk.Code],
                Constants = [.. chunk.Constants],
                Lines = [.. chunk.Lines],
                SourceName = Path.GetFileName(file),
            };
            PrgFormat.Write(prgFile, prg);
            Out.WriteLine($"[P-Code] {file} -> {prgFile} ({chunk.Code.Count} opcodes)");

            if (dumpFlag)
            {
                var pasmFile = Path.Combine(outputDir, Path.GetFileNameWithoutExtension(file) + ".pasm");
                var disassembly = global::Compiler.Disassembler.Disassemble(chunk, Path.GetFileName(file));
                await File.WriteAllTextAsync(pasmFile, disassembly, ct);
                Out.WriteLine($"[P-Code] dump -> {pasmFile}");
            }

            if (managed)
            {
                try
                {

            var exePath = global::Basic.IlBackend.IlProgramCompiler.Compile(program, file, outputDir, Path.GetFileNameWithoutExtension(file),
                noOptimize: _opts.NoOptimize, dumpIl: _opts.DumpIl, slim: !all);
                    Out.WriteLine($"[IL] managed -> {exePath}");

                    if (full)
                    {
                        CopyNetRuntime(outputDir);
                        Out.WriteLine($"[IL] full: .NET runtime copied");
                    }

                    if (dumpFlag)
                    {
                        var asmPath = Path.ChangeExtension(exePath, ".asm");
                        if (File.Exists(asmPath))
                            Out.WriteLine($"[IL] dump -> {asmPath}");
                        else
                        {
                            asmPath = Path.ChangeExtension(file, ".asm");
                            if (File.Exists(asmPath))
                                Out.WriteLine($"[IL] dump -> {asmPath}");
                        }
                    }
                }
                catch (Exception ex)
                {
                    WriteError($"?IL compilation failed: {ex.Message}");
                }
            }
            else if (native)
            {
                try
                {
                    var nativeDir = Path.Combine(outputDir, "native");
                    var nativeExe = global::Basic.IlBackend.IlProgramCompiler.CompileNative(
                        program, file, nativeDir);
                    // Accanto al binario nativo, non solo nella cartella del progetto.
                    var nb = CommonLib.ProgramBundle.CopyNextTo(nativeDir, file, Out.WriteLine);
                    if (nb.Count > 0) Out.WriteLine($"[bundle] {nb.Count} file accanto al binario nativo");
                    Out.WriteLine($"[NATIVE] compiled -> {nativeExe}");
                }
                catch (Exception ex)
                {
                    WriteError($"?Native AOT compilation failed: {ex.Message}");
                }
            }
        }
        catch (Exception ex)
        {
            WriteError($"?Error: {ex.Message}");
        }
    }

    // Sottocomandi del package manager inoltrati alla CLI (CliPackages.Pkg): tassonomia unica.
    private static readonly HashSet<string> CliPackageSubcommands = new(StringComparer.OrdinalIgnoreCase)
    {
        "BUILD","INFO","LIST","REMOVE","SEARCH","TREE","OUTDATED","VENDOR","WHY","PRUNE","DEDUPE",
        "AUDIT","SBOM","DEPRECATE","UNDEPRECATE","TAG","TAGS","SYNC","OWNERS","YANK"
    };

    private async Task HandlePackageAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (parts.Length >= 1 && string.Equals(parts[0], "RESTORE", StringComparison.OrdinalIgnoreCase))
        {
            await HandleRestoreAsync(string.Join(' ', parts.Skip(1)), ct);
            return;
        }
        if (parts.Length >= 1 && string.Equals(parts[0], "UPDATE", StringComparison.OrdinalIgnoreCase))
        {
            await HandleUpdateAsync(string.Join(' ', parts.Skip(1)), ct);
            return;
        }
        // Sottocomandi CLI del package manager non-REPL (why/prune/dedupe/audit/sbom/deprecate/build/
        // info/list/remove/search/tree/outdated/vendor/owners/yank/tag/tags/sync): delega alla CLI.
        if (parts.Length >= 1 && CliPackageSubcommands.Contains(parts[0]))
        {
            try { CliPackages.Pkg(parts); }
            catch (Exception ex) { WriteError($"?PACKAGE {parts[0].ToUpperInvariant()}: {ex.Message}"); }
            return;
        }
        if (parts.Length < 2)
        {
            WriteError("?Usage: PACKAGE INSTALL <name> [@ <version>] [FROM <url>] | PACKAGE RUN <name> | PACKAGE RESTORE [dir] [--locked] [FROM <url>]");
            return;
        }
        if (string.Equals(parts[0], "RUN", StringComparison.OrdinalIgnoreCase))
        {
            await HandlePackageRunAsync(parts[1].Trim('"'), ct);
            return;
        }
        if (!string.Equals(parts[0], "INSTALL", StringComparison.OrdinalIgnoreCase))
        {
            WriteError("?Usage: PACKAGE INSTALL <name> [@ <version>] [FROM <url>] | PACKAGE RUN <name> | PACKAGE RESTORE [dir] [--locked] [FROM <url>]");
            return;
        }

        var name = parts[1].Trim('"');
        string? version = null;
        string? registryUrl = null;
        var local = false;
        string? toDir = null;

        for (var i = 2; i < parts.Length; i++)
        {
            if (parts[i] == "@" && i + 1 < parts.Length)
                version = parts[++i].Trim('"');
            else if (string.Equals(parts[i], "FROM", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                registryUrl = parts[++i].Trim('"');
            // LOCAL: installa in <dir dell'eseguibile>/packages (come `gmb pkg install --local`)
            else if (string.Equals(parts[i], "LOCAL", StringComparison.OrdinalIgnoreCase))
                local = true;
            // TO "<dir>": installa in <dir>/packages
            else if (string.Equals(parts[i], "TO", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                toDir = parts[++i].Trim('"');
        }

        Out.WriteLine($"Downloading {name}" + (version != null ? $"@{version}" : "") + "...");

        try
        {
            var pm = new Parser.PackageManager();
            if (registryUrl != null) pm.RegistryUrl = registryUrl;
            Basic.InstalledPackage pkg;
            if (local || toDir != null)
            {
                var target = toDir != null
                    ? Path.Combine(toDir, "packages")
                    : CommonLib.GmbiPaths.LocalPackagesDir;
                Directory.CreateDirectory(target);   // creata se manca
                pkg = await pm.InstallIntoAsync(name, version, target);
            }
            else
            {
                // senza LOCAL/TO la destinazione è la cache; se lì manca e la stessa versione
                // è nell'installazione locale, la SPOSTA (stessa regola della CLI)
                pkg = await pm.InstallIntoAsync(name, version, pm.CacheRoot);
            }
            Out.WriteLine($"Installed: {pkg.Manifest.Name} v{pkg.Manifest.Version}");
            Out.WriteLine($"  Path: {pkg.InstallPath}");
            if (pkg.Manifest.Exports.Length > 0)
                Out.WriteLine($"  Modules: {string.Join(", ", pkg.Manifest.Exports)}");
            if (pkg.Manifest.Dependencies.Count > 0)
                Out.WriteLine($"  Dependencies: {string.Join(", ", pkg.Manifest.Dependencies.Select(d => $"{d.Key} {d.Value}"))}");
        }
        catch (PackageException ex)
        {
            WriteError($"?Package error: {ex.Message}");
        }
        catch (HttpRequestException ex)
        {
            WriteError($"?Network error: {ex.Message}");
        }
        catch (Exception ex)
        {
            WriteError($"?Error: {ex.Message}");
        }
    }

    // RESTORE [dir] [--locked|LOCKED] [FROM <url>]  — installa le dipendenze del progetto.
    //   --locked -> usa le versioni di gmb-package.lock; altrimenti scansiona i REQUIRE nei sorgenti.
    private Task HandleRestoreAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var locked = false;
        string? dir = null;
        string? registry = null;
        string? cacheDir = null;
        for (var i = 0; i < parts.Length; i++)
        {
            var p = parts[i];
            if (string.Equals(p, "--locked", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "LOCKED", StringComparison.OrdinalIgnoreCase))
                locked = true;
            else if (string.Equals(p, "FROM", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                registry = parts[++i].Trim('"');
            // Stesse opzioni della CLI (`gmb restore --registry … --cache …`): prima `--registry`
            // veniva scartato come "flag sconosciuto" e l'URL successivo finiva in `dir`
            // → "Could not find a part of the path '.../http:/localhost:8099'".
            else if (string.Equals(p, "--registry", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                registry = parts[++i].Trim('"');
            else if (string.Equals(p, "--cache", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                cacheDir = parts[++i].Trim('"');
            else if (!p.StartsWith('-'))
                dir ??= p.Trim('"');
        }
        var root = string.IsNullOrWhiteSpace(dir) ? _projectPath : Path.GetFullPath(dir);
        try
        {
            var pm = new Parser.PackageManager();
            if (registry != null) pm.RegistryUrl = registry;
            if (cacheDir != null) pm.CacheRoot = Path.GetFullPath(cacheDir);
            Out.WriteLine($"Restore{(locked ? " (locked)" : "")} in {root}...");
            CliPackages.DoRestore(pm, new PackageBuildContext(), root, locked);
        }
        catch (Exception ex) { WriteError($"?Restore error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    // UPDATE [dir] [--check|CHECK] [--yes|YES] [FROM <url>]  — aggiorna TUTTI i pacchetti del progetto.
    //   --check -> mostra solo il piano (dry-run); --yes -> nessuna conferma.
    private Task HandleUpdateAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var check = false;
        var yes = false;
        string? dir = null;
        string? registry = null;
        string? cacheDir = null;
        for (var i = 0; i < parts.Length; i++)
        {
            var p = parts[i];
            if (string.Equals(p, "--check", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "CHECK", StringComparison.OrdinalIgnoreCase))
                check = true;
            else if (string.Equals(p, "--yes", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "YES", StringComparison.OrdinalIgnoreCase))
                yes = true;
            else if (string.Equals(p, "FROM", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                registry = parts[++i].Trim('"');
            // Stesse opzioni della CLI (`gmb update --registry … --cache …`); prima l'URL
            // finiva in `dir` (stesso bug di RESTORE).
            else if (string.Equals(p, "--registry", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                registry = parts[++i].Trim('"');
            else if (string.Equals(p, "--cache", StringComparison.OrdinalIgnoreCase) && i + 1 < parts.Length)
                cacheDir = parts[++i].Trim('"');
            else if (!p.StartsWith('-'))
                dir ??= p.Trim('"');
        }
        var root = string.IsNullOrWhiteSpace(dir) ? _projectPath : Path.GetFullPath(dir);
        try
        {
            var pm = new Parser.PackageManager();
            if (registry != null) pm.RegistryUrl = registry;
            if (cacheDir != null) pm.CacheRoot = Path.GetFullPath(cacheDir);
            Out.WriteLine($"Update{(check ? " (check)" : "")} in {root}...");
            CliPackages.DoUpdate(pm, new PackageBuildContext(), root, check, yes);
        }
        catch (Exception ex) { WriteError($"?Update error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    // ROLLBACK [dir] [--list] [--to X] — ripristina uno storico lock.
    private Task HandleRollbackAsync(string args, CancellationToken ct)
        => DelegAsync(args, CliPackages.Rollback, "Rollback");

    // BUMP <dir> <major|minor|patch|prerelease|X.Y.Z> — aggiorna la versione nel manifest.
    private Task HandleBumpAsync(string args, CancellationToken ct)
        => DelegAsync(args, CliPackages.Bump, "Bump");

    // ADD <name[@ver]> [--dev] [--dir dir] — aggiunge/aggiorna una dipendenza nel gmb-package.json.
    private Task HandleAddAsync(string args, CancellationToken ct)
        => DelegAsync(args, CliPackages.Add, "Add");

    // REMOVE <name> [--dir dir] — rimuove una dipendenza dal gmb-package.json.
    private Task HandleRemoveAsync(string args, CancellationToken ct)
        => DelegAsync(args, CliPackages.RemoveDep, "Remove");

    // CACHE list|verify|dedup|gc|clean [--cache dir] — gestione cache pacchetti.
    private Task HandleCacheAsync(string args, CancellationToken ct)
        => DelegAsync(args, CliPackages.Cache, "Cache");

    private Task DelegAsync(string args, Action<string[]> fn, string label)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        try { fn(parts); }
        catch (Exception ex) { WriteError($"?{label} error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    // TRUST ADD|REMOVE|LIST|FINGERPRINT — keyring locale delle chiavi fidate.
    private Task HandleTrustAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        try { CliPackages.Trust(parts); }
        catch (Exception ex) { WriteError($"?Trust error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private async Task HandlePackageRunAsync(string name, CancellationToken ct)
    {
        try
        {
            var pm = new Parser.PackageManager();
            Out.WriteLine($"Resolving {name}...");
            var pkg = await pm.DownloadPackageAsync(name);
            var chunkPath = pm.ResolveVmChunkPath(pkg);
            if (chunkPath == null || !File.Exists(chunkPath))
            {
                WriteError($"?Package '{name}' non è un vm-chunk standalone (nessun binario .chunk/.prg per il backend vm).");
                return;
            }

            global::Compiler.CompiledChunk chunk;
            if (chunkPath.EndsWith(".prg", StringComparison.OrdinalIgnoreCase))
            {
                var prg = global::Basic.Runtime.PrgFormat.Read(chunkPath);
                chunk = new global::Compiler.CompiledChunk();
                chunk.Code.AddRange(prg.Code);
                foreach (var c in prg.Constants) chunk.Constants.Add(c);
                foreach (var l in prg.Lines) chunk.Lines.Add(l);
            }
            else
            {
                chunk = global::Basic.Runtime.ChunkA1Serializer.Deserialize(File.ReadAllBytes(chunkPath));
            }

            Out.WriteLine($"Running {name} ({chunkPath}) via gmb_vm_host...");
            RunGmbVmHost(chunk, ct, []);
        }
        catch (OperationCanceledException) { /* Ctrl+C */ }
        catch (PackageException ex) { WriteError($"?Package error: {ex.Message}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
    }

    private async Task HandleCompileBspAsync(string args, CancellationToken ct)
    {
        var dir = args.Trim().Trim('"');
        if (string.IsNullOrWhiteSpace(dir)) { WriteError("?Usage: BSP <directory>"); return; }
        if (!Path.IsPathRooted(dir)) dir = Path.Combine(_projectPath, dir);
        if (!Directory.Exists(dir)) { WriteError($"?Directory not found: {dir}"); return; }

        var bspFiles = Directory.GetFiles(dir, "*.bsp", SearchOption.AllDirectories);
        if (bspFiles.Length == 0) { WriteError($"?No .bsp files found in {dir}"); return; }

        Out.WriteLine($"Pre-compiling {bspFiles.Length} BSP pages in {dir}...");
        var ok = 0; var err = 0;
        foreach (var file in bspFiles)
        {
            try
            {
                // Usa la stessa pipeline del server HTTP reale (Basic.Pipeline.SourcePipeline),
                // NON Basic.Orchestrator: quest'ultimo non registra i globali BSP (REQ/RES/
                // SESSION/FORM/QUERY/PAGE), quindi qualunque pagina che li referenzia falliva con
                // "Simbolo non definito", e le pagine che compilavano comunque finivano in cache
                // con GlobalCount=0 — TryServeBspPage le avrebbe servite SENZA popolare i globali.
                // Sidecar A1 per la VM/C: il renderer nativo esegue <file>.bsp.chunk in una VM
                // indipendente per richiesta (vedi native/gmb_vm_c/gmb_vm_bsp.c). Chunk COMPLETO
                // (non la CachedPage parziale: mancano Data/Procs/Arities/TryRegions/LocalCount).
                // Scrittura unica in SourcePipeline.EnsureBspSidecar (la usa anche il server
                // quando serve una pagina con il motore C).
                global::Basic.Pipeline.SourcePipeline.SidecarWriter ??=
                    static chunk => global::Basic.Runtime.ChunkA1Serializer.Serialize(chunk);
                var succeeded = global::Basic.Pipeline.SourcePipeline.EnsureBspSidecar(file, force: true);
                if (!succeeded) { Out.WriteLine($"  FAIL {Path.GetFileName(file)}: compilation error"); err++; continue; }
                var chunkPath = file + ".chunk";
                var cached = global::CommonLib.Http.BspRuntime.CacheGet(file);
                Out.WriteLine($"  OK   {Path.GetFileName(file)} ({cached?.PCode.Length ?? 0} opcodes, sidecar {Path.GetFileName(chunkPath)})");
                ok++;
            }
            catch (Exception ex) { Out.WriteLine($"  FAIL {Path.GetFileName(file)}: {ex.Message}"); err++; }
        }
        Out.WriteLine($"Pre-compilation: {ok} OK, {err} FAIL out of {bspFiles.Length}");
    }

    private Task HandleRunAsync(string args, CancellationToken cancellationToken)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var (managed, native) = ParseBackendFlags(parts);
        var nonFlags = parts.Where(p => p is not ("-n" or "-m" or "--native" or "--managed")).ToArray();
        var fileArg = nonFlags.Length > 0 ? nonFlags[0].Trim().Trim('"') : null;
        var outDirArg = nonFlags.Length > 1 ? nonFlags[1].Trim().Trim('"') : null;

        if (_usePrgVm && _prgChunk != null)
        {
            RunPrgVmAsync(cancellationToken).GetAwaiter().GetResult();
            return Task.CompletedTask;
        }

        if (!string.IsNullOrWhiteSpace(fileArg))
        {
            RunFileAsync(fileArg, managed, native, cancellationToken, outDirArg);
            return Task.CompletedTask;
        }

        if (_programLines.Count == 0 && string.IsNullOrWhiteSpace(_replProgramSource))
        {
            WriteError("?Program is empty.");
            return Task.CompletedTask;
        }

        try
        {
            var content = string.IsNullOrWhiteSpace(_replProgramSource)
                ? string.Join(Environment.NewLine, _programLines.Values)
                : _replProgramSource;

            var tmpFile = Path.Combine(Path.GetTempPath(), $"repl_{Guid.NewGuid():N}.bas");
            File.WriteAllText(tmpFile, content);

            if (native)
            {
                RunNativeAotAsync(tmpFile, cancellationToken);
            }
            else if (managed)
            {
                RunFileAsync(tmpFile, true, false, cancellationToken);
            }
            else
            {
                RunPCodeAsync(tmpFile, cancellationToken);
            }
        }
        catch (Exception ex)
        {
            WriteError($"?Error: {ex.Message}");
        }

        return Task.CompletedTask;
    }

    private void RunFileAsync(string file, bool managed, bool native, CancellationToken ct, string? outDirArg = null)
    {
        if (!file.EndsWith(".bas", StringComparison.OrdinalIgnoreCase))
        {
            if (file.EndsWith(".prg", StringComparison.OrdinalIgnoreCase))
            {
                HandleLoadPrgAsync(file, ct).GetAwaiter().GetResult();
                if (_prgChunk != null) RunPrgVmAsync(ct).GetAwaiter().GetResult();
                return;
            }
            file += ".bas";
        }

        if (!File.Exists(file))
        {
            WriteError($"?File not found: {file}");
            return;
        }

        if (native)
        {
            RunNativeAotAsync(file, ct);
        }
        else if (managed)
        {
            var outputDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, outDirArg ?? "bin");
            var program = global::Basic.Orchestrator.Compile(file, null, false, "il");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }
            // Esecuzione (non distribuzione): closure risolta con symlink, non copiata
            // (262 MB/178 file per run → pochi MB). Vedi RunNativeSafe.
            var exePath = global::Basic.IlBackend.IlProgramCompiler.Compile(program, file, outputDir,
                Path.GetFileNameWithoutExtension(file), noOptimize: _opts.NoOptimize, dumpIl: _opts.DumpIl,
                linkRuntime: !_opts.Full);
            Out.WriteLine($"[IL] managed -> {exePath}");
            var effectiveExe = exePath;
            var effectiveArgs = OperatingSystem.IsWindows() ? "" : $"exec \"{exePath}.dll\"";
            var psi = new System.Diagnostics.ProcessStartInfo(effectiveExe, effectiveArgs)
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(exePath)) ?? ".",
            };
            using var proc = System.Diagnostics.Process.Start(psi);
            if (proc != null)
            {
                proc.OutputDataReceived += (_, e) => { if (e.Data != null) Out.WriteLine(e.Data); };
                proc.ErrorDataReceived += (_, e) => { if (e.Data != null) Console.Error.WriteLine(e.Data); };
                proc.BeginOutputReadLine();
                proc.BeginErrorReadLine();
                proc.WaitForExit();
            }
            Out.WriteLine("\n[Native] Execution completed.");
        }
        else
        {
            var prgDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, outDirArg ?? "bin");
            var prgFile = Path.Combine(prgDir, Path.GetFileNameWithoutExtension(file) + ".prg");
            if (File.Exists(prgFile) && PrgFormat.IsValid(prgFile) &&
                File.GetLastWriteTimeUtc(prgFile) > File.GetLastWriteTimeUtc(file))
            {
                Out.WriteLine($"[P-Code] {Path.GetFileName(prgFile)} is up to date. Running...");
                _prgChunk = PrgFormat.Read(prgFile);
                _prgChunkPath = prgFile;
                _usePrgVm = true;
                RunPrgVmAsync(ct).GetAwaiter().GetResult();
            }
            else
            {
                RunPCodeAsync(file, ct);
            }
        }
    }

    private void RunNativeAotAsync(string file, CancellationToken ct)
    {
        try
        {
            var program = global::Basic.Orchestrator.Compile(file, null, false, "il");
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return; }
            var outputDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin", "native");
            var exePath = global::Basic.IlBackend.IlProgramCompiler.CompileNative(program, file, outputDir);
            Out.WriteLine($"[NATIVE] compiled -> {exePath}");
            var psi = new System.Diagnostics.ProcessStartInfo(exePath) { UseShellExecute = false };
            using var proc = System.Diagnostics.Process.Start(psi);
            proc?.WaitForExit();
        }
        catch (Exception ex)
        {
            WriteError($"?Native AOT: {ex.Message}");
        }
    }

    private static void CopyNetRuntime(string destDir)
    {
        var fxDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            ".dotnet", "shared", "Microsoft.NETCore.App");
        if (!Directory.Exists(fxDir))
        {
            fxDir = "/usr/local/share/dotnet/shared/Microsoft.NETCore.App";
            if (!Directory.Exists(fxDir)) return;
        }
        var latestVer = Directory.GetDirectories(fxDir).Select(Path.GetFileName).OrderDescending().FirstOrDefault();
        if (latestVer == null) return;
        var srcDir = Path.Combine(fxDir, latestVer);
        foreach (var f in Directory.GetFiles(srcDir, "*"))
        {
            var dest = Path.Combine(destDir, Path.GetFileName(f));
            if (!File.Exists(dest)) File.Copy(f, dest, true);
        }
    }

    private static void ExecFile(string exePath)
    {
        try
        {
            Console.CursorVisible = false;
            var effectiveExe = exePath;
            var effectiveArgs = OperatingSystem.IsWindows() ? "" : $"exec \"{exePath}.dll\"";
            var psi = new System.Diagnostics.ProcessStartInfo(effectiveExe, effectiveArgs)
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(exePath)) ?? ".",
            };
            using var proc = System.Diagnostics.Process.Start(psi);
            if (proc != null)
            {
                proc.OutputDataReceived += (_, e) => { if (e.Data != null) Out.WriteLine(e.Data); };
                proc.ErrorDataReceived += (_, e) => { if (e.Data != null) Console.Error.WriteLine(e.Data); };
                proc.BeginOutputReadLine();
                proc.BeginErrorReadLine();
                proc.WaitForExit();
            }
            Out.WriteLine("\n[Native] Execution completed.");
        }
        finally { if (OperatingSystem.IsWindows()) Console.CursorVisible = true; }
    }

    private Task HandleExecAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var (managed, native) = ParseBackendFlags(parts);
        var fileArgs = parts.Where(p => p is not ("-n" or "-m" or "--native" or "--managed")).ToArray();
        var file = fileArgs.Length > 0 ? fileArgs[0].Trim().Trim('"') : null;

        if (string.IsNullOrWhiteSpace(file))
        {
            WriteError("?Usage: EXEC <file> [-n | -m]");
            return Task.CompletedTask;
        }

        try
        {
            if (native)
            {
                var exePath = file;
                if (!exePath.EndsWith(GetPlatformExeExtension()))
                {
                    var baseName = Path.ChangeExtension(file, null);
                    exePath = baseName + GetPlatformExeExtension();
                }

                if (!File.Exists(exePath))
                {
                    WriteError($"?Executable not found: {exePath}");
                    Out.WriteLine("  Use COMPILE <file.bas> -n first.");
                    return Task.CompletedTask;
                }

                ExecFile(exePath);
            }
            else if (managed)
            {
                var dllPath = file.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ? file :
                    Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin", Path.GetFileNameWithoutExtension(file) + ".dll");
                if (!File.Exists(dllPath))
                {
                    WriteError($"?Managed DLL not found: {dllPath}");
                    Out.WriteLine("  Use COMPILE <file.bas> -m first.");
                    return Task.CompletedTask;
                }
                var exe = Path.ChangeExtension(dllPath, null);
                var effectiveExe = exe;
                var effectiveArgs = OperatingSystem.IsWindows() ? "" : $"exec \"{dllPath}\"";
                var psi = new System.Diagnostics.ProcessStartInfo(effectiveExe, effectiveArgs)
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(dllPath)) ?? ".",
                };
                using var proc = System.Diagnostics.Process.Start(psi);
                if (proc != null)
                {
                    proc.OutputDataReceived += (_, e) => { if (e.Data != null) Out.WriteLine(e.Data); };
                    proc.ErrorDataReceived += (_, e) => { if (e.Data != null) Console.Error.WriteLine(e.Data); };
                    proc.BeginOutputReadLine();
                    proc.BeginErrorReadLine();
                    proc.WaitForExit();
                }
                Out.WriteLine("\n[Native] Execution completed.");
            }
            else
            {
                var prgFile = file.EndsWith(".prg", StringComparison.OrdinalIgnoreCase) ? file :
                    Path.Combine(Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin"), Path.GetFileNameWithoutExtension(file) + ".prg");
                if (!File.Exists(prgFile))
                {
                    WriteError($"?PRG not found: {prgFile}");
                    Out.WriteLine("  Use COMPILE <file.bas> first.");
                    return Task.CompletedTask;
                }

                if (!PrgFormat.IsValid(prgFile))
                {
                    WriteError($"?Invalid .prg: {prgFile}");
                    return Task.CompletedTask;
                }

                _prgChunk = PrgFormat.Read(prgFile);
                _prgChunkPath = prgFile;
                _usePrgVm = true;
                RunPrgVmAsync(ct).GetAwaiter().GetResult();
            }
        }
        catch (Exception ex)
        {
            WriteError($"?Error: {ex.Message}");
        }

        return Task.CompletedTask;
    }

    /// <summary>Estrae i flag backend da un comando REPL: -m/-managed → managed
    /// (DLL+apphost), -n/-native → binario nativo (NativeAOT). Restituisce (managed, native).</summary>
    private static (bool Managed, bool Native) ParseBackendFlags(string[] parts) =>
        (parts.Contains("-m") || parts.Contains("--managed"),
         parts.Contains("-n") || parts.Contains("--native"));

    private static string GetPlatformExeExtension()
    {
        if (OperatingSystem.IsWindows()) return ".exe";
        return ""; // macOS/Linux: no extension
    }
    private void RunPCodeAsync(string file, CancellationToken ct)
    {
        RunPCodeSafe(file);
    }

    

    private async Task RunPrgVmAsync(CancellationToken ct)
    {
        RunPrgVmSafe();
    }

    private Task HandleLoadPrgAsync(string args, CancellationToken __)
    {
        var file = args.Trim().Trim('"');
        if (string.IsNullOrWhiteSpace(file)) { WriteError("?Usage: LOADPRG <file.prg>"); return Task.CompletedTask; }
        if (!File.Exists(file)) { WriteError($"?File not found: {file}"); return Task.CompletedTask; }
        if (!PrgFormat.IsValid(file)) { WriteError($"?Invalid .prg: {file}"); return Task.CompletedTask; }

        try
        {
            _prgChunk = PrgFormat.Read(file);
            _prgChunkPath = file;
            _usePrgVm = true;
            Out.WriteLine($"Loaded: {file} ({_prgChunk.Code.Length} opcodes)");
        }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandlePrgInfoAsync(string args, CancellationToken __)
    {
        var file = args.Trim().Trim('"');
        if (!string.IsNullOrWhiteSpace(file))
        {
            if (!File.Exists(file)) { WriteError($"?File not found: {file}"); return Task.CompletedTask; }
            try
            {
                var chunk = PrgFormat.Read(file);
                Out.WriteLine($"{Path.GetFileName(file)}: {chunk.Code.Length} opcodes, {chunk.Constants.Length} constants");
            }
            catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        }
        else if (_prgChunk != null)
        {
            Out.WriteLine($"Loaded PRG: {_prgChunk.Code.Length} opcodes, {_prgChunk.Constants.Length} constants");
        }
        else
        {
            WriteError("?Usage: PRGINFO <file.prg>");
        }
        return Task.CompletedTask;
    }

    private async Task HandleAstAsync(string args, CancellationToken ct)
    {
        var parts = args.Trim().Split([' '], StringSplitOptions.RemoveEmptyEntries);
        var raw = false;
        var filtered = new List<string>();
        foreach (var p in parts)
            if (string.Equals(p, "-r", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "-raw", StringComparison.OrdinalIgnoreCase)) raw = true;
            else filtered.Add(p);
        var cmd = filtered.Count > 0 ? filtered[0].ToUpperInvariant() : string.Empty;

        if (cmd is "ALL" or "ERR" or "STOP")
        {
            var stopOnError = cmd == "STOP"; var showSuccess = cmd != "ERR";
            var list = _activeSampleList.Count > 0 ? _activeSampleList : _masterSampleCatalog ?? [];
            var files = list.Where(s => s.Type == SampleType.File).ToList();
            Out.WriteLine($"Test AST su {files.Count} file ({(stopOnError ? "STOP" : showSuccess ? "ALL" : "ERR")})...");
            int ok = 0, fail = 0; var failed = new List<string>();
            foreach (var f in files)
            {
                try { var p = global::Basic.Orchestrator.Compile(f.FilePath, null, false); if (p?.MainModule == null) throw new("null module"); ok++; if (showSuccess) Out.Write("."); }
                catch (Exception ex) { fail++; failed.Add(f.DisplayName); Out.WriteLine($"\n  ERRORE: {f.DisplayName} - {ex.Message}"); if (stopOnError) break; }
            }
            Out.WriteLine($"\nOK={ok} ERRORI={fail}");
            if (fail > 0 && !stopOnError) { Out.WriteLine("File con errori:"); foreach (var f in failed) Out.WriteLine($"  - {f}"); }
            return;
        }

        var sample = FindOrSelectSample(string.Join(" ", filtered));
        if (sample == null) return;
        Out.WriteLine($"Analizzo: {sample.Description}...");
        try
        {
            var program = global::Basic.Orchestrator.Compile(sample.FilePath, null, false);
            if (program?.MainModule == null) { WriteError("?Compilazione fallita."); return; }
            if (raw)
            {
                ApplyConsolePalette();
                global::Basic.Parser.DumpAstWithSymbolsAndScopes(program);
                Out.WriteLine(new string('-', 70));
            }
            else
            {
                var sw = new StringWriter();
                var oldOut = Console.Out;
                Console.SetOut(sw);
                try { global::Basic.Parser.DumpAstWithSymbolsAndScopes(program); }
                finally { Console.SetOut(oldOut); }
                PaginatedMenu.Show(sw.ToString(), $"AST: {sample.DisplayName}");
            }
        }
        catch (Exception ex) { WriteError($"?Errore: {ex.Message}"); }
    }

    private Task HandleDasmAsync(string args, CancellationToken __)
    {
        var parts = args.Trim().Split([' '], StringSplitOptions.RemoveEmptyEntries);
        var raw = false;
        var filtered = new List<string>();
        foreach (var p in parts)
            if (string.Equals(p, "-r", StringComparison.OrdinalIgnoreCase) || string.Equals(p, "-raw", StringComparison.OrdinalIgnoreCase)) raw = true;
            else filtered.Add(p);
        var cmd = filtered.Count > 0 ? filtered[0].ToUpperInvariant() : string.Empty;

        if (cmd is "ALL" or "ERR" or "STOP")
        {
            var stopOnError = cmd == "STOP"; var showSuccess = cmd != "ERR";
            var list = _activeSampleList.Count > 0 ? _activeSampleList : _masterSampleCatalog ?? [];
            var files = list.Where(s => s.Type == SampleType.File).ToList();
            Out.WriteLine($"Test DASM su {files.Count} file ({(stopOnError ? "STOP" : showSuccess ? "ALL" : "ERR")})...");
            int ok = 0, fail = 0; var failed = new List<string>();
            foreach (var f in files)
            {
                try { var p = global::Basic.Orchestrator.Compile(f.FilePath, null, false); var c = new global::Compiler.Compiler(); c.Compile(p); ok++; if (showSuccess) Out.Write("."); }
                catch (Exception ex) { fail++; failed.Add(f.DisplayName); Out.WriteLine($"\n  ERRORE: {f.DisplayName} - {ex.Message}"); if (stopOnError) break; }
            }
            Out.WriteLine($"\nOK={ok} ERRORI={fail}");
            if (fail > 0 && !stopOnError) { Out.WriteLine("File con errori:"); foreach (var f in failed) Out.WriteLine($"  - {f}"); }
            return Task.CompletedTask;
        }

        var sample = FindOrSelectSample(string.Join(" ", filtered));
        if (sample == null) return Task.CompletedTask;
        Out.WriteLine($"Disassemblo: {sample.Description}...");
        try
        {
            var program = global::Basic.Orchestrator.Compile(sample.FilePath, null, false);
            if (program?.MainModule == null) { WriteError("?Compilazione fallita."); return Task.CompletedTask; }
            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);
            var disasm = global::Compiler.Disassembler.Disassemble(chunk, sample.DisplayName);
            if (raw)
            {
                ApplyConsolePalette();
                Out.WriteLine(disasm);
            }
            else
            {
                PaginatedMenu.Show(disasm, $"Disassembly: {sample.DisplayName}");
            }
        }
        catch (Exception ex) { WriteError($"?Errore: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleIlAsync(string args, CancellationToken __)
    {
        var file = args.Trim().Trim('"');
        if (string.IsNullOrWhiteSpace(file))
        {
            if (_programLines.Count == 0) { WriteError("?No program loaded."); return Task.CompletedTask; }
            var tmpFile = Path.Combine(Path.GetTempPath(), $"il_{Guid.NewGuid():N}.bas");
            File.WriteAllText(tmpFile, string.Join(Environment.NewLine, _programLines.Values));
            file = tmpFile;
        }
        else if (!File.Exists(file))
        {
            WriteError($"?File not found: {file}");
            return Task.CompletedTask;
        }

        try
        {
            var program = global::Basic.Orchestrator.Compile(file, null, false);
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return Task.CompletedTask; }
            // Solo per MOSTRARE l'asm: la closure del runtime non serve copiata.
            var exePath = global::Basic.IlBackend.IlProgramCompiler.Compile(program, file, Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin"), Path.GetFileNameWithoutExtension(file), linkRuntime: true);
            Out.WriteLine($"[IL] compiled -> {exePath}");
            var asmPath = Path.ChangeExtension(exePath, ".asm");
            if (File.Exists(asmPath))
                PaginatedMenu.Show(File.ReadAllText(asmPath), $"IL Assembly: {Path.GetFileName(asmPath)}");
            else
                Out.WriteLine("(no .asm file found)");
        }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }

        return Task.CompletedTask;
    }

    private Task HandleLsAsync(string args, CancellationToken __)
    {
        var dir = string.IsNullOrWhiteSpace(args) ? _projectPath : Path.GetFullPath(Path.Combine(_projectPath, args));
        if (!Directory.Exists(dir)) { WriteError($"?Directory not found: {dir}"); return Task.CompletedTask; }
        foreach (var entry in Directory.GetFileSystemEntries(dir))
            Out.WriteLine($"  {Path.GetFileName(entry)}{(Directory.Exists(entry) ? "/" : "")}");
        return Task.CompletedTask;
    }

    private Task HandleCdAsync(string args, CancellationToken __)
    {
        var target = args.Trim();
        if (string.IsNullOrWhiteSpace(target)) { Out.WriteLine(_projectPath); return Task.CompletedTask; }
        var newPath = Path.GetFullPath(Path.Combine(_projectPath, target));
        if (Directory.Exists(newPath)) { _projectPath = newPath; Out.WriteLine(_projectPath); }
        else WriteError($"?Directory not found: {target}");
        return Task.CompletedTask;
    }

    private Task HandleWhereAsync(string _, CancellationToken __) { Out.WriteLine(_projectPath); return Task.CompletedTask; }

    /// <summary>GREP: ricerca file avanzata via il motore C condiviso (P/Invoke su
    /// libgmb_grep, stessa sorgente di VM/C e C99). Parsing shell-like + opts JSON.</summary>
    private Task HandleGrepAsync(string args, CancellationToken __)
    {
        var toks = SplitShellArgs(args);
        var opts = new System.Text.StringBuilder("{");
        bool first = true;
        void Add(string kv) { if (!first) opts.Append(','); first = false; opts.Append(kv); }
        string? pattern = null, path = null;
        for (int i = 0; i < toks.Count; i++)
        {
            var t = toks[i];
            switch (t)
            {
                case "-F" or "--fixed": Add("\"fixed\":true"); break;
                case "-i": Add("\"ignore_case\":true"); break;
                case "-w": Add("\"word\":true"); break;
                case "-v": Add("\"invert\":true"); break;
                case "-r" or "-R": Add("\"recursive\":true"); break;
                case "-n": Add("\"line_number\":true"); break;
                case "-l": Add("\"files_with_matches\":true"); break;
                case "-c": Add("\"count_only\":true"); break;
                case "-o": Add("\"only_matching\":true"); break;
                case "-.": Add("\"hidden\":true"); break;
                case "--type" when i + 1 < toks.Count: Add($"\"type\":{JsonStr(toks[++i])}"); break;
                case "--include" when i + 1 < toks.Count: Add($"\"include\":{JsonStr(toks[++i])}"); break;
                case "--exclude" when i + 1 < toks.Count: Add($"\"exclude\":{JsonStr(toks[++i])}"); break;
                case "--depth" when i + 1 < toks.Count: Add($"\"max_depth\":{int.Parse(toks[++i])}"); break;
                case "-m" when i + 1 < toks.Count: Add($"\"max_count\":{int.Parse(toks[++i])}"); break;
                case "-A" when i + 1 < toks.Count: Add($"\"context_after\":{int.Parse(toks[++i])}"); break;
                case "-B" when i + 1 < toks.Count: Add($"\"context_before\":{int.Parse(toks[++i])}"); break;
                case "-C" when i + 1 < toks.Count: { var cv = int.Parse(toks[++i]); Add($"\"context_before\":{cv}"); Add($"\"context_after\":{cv}"); } break;
                default: if (pattern is null) pattern = t; else path = t; break;
            }
        }
        if (pattern is null)
        {
            WriteError("uso: GREP [-rinlcFwvo-.] [--type EXT] [--include G] [--exclude G] [--depth N] PATTERN [PATH]");
            return Task.CompletedTask;
        }
        Add($"\"pattern\":{JsonStr(pattern)}");
        opts.Append('}');
        var dir = path is null ? _projectPath : Path.GetFullPath(Path.Combine(_projectPath, path));
        try
        {
            var hits = Basic.NativeGrep.SearchJson(opts.ToString(), dir);
            var txt = Basic.NativeGrep.FormatJson(hits, true, true);
            if (txt.Length > 0) Out.Write(txt);
        }
        catch (Exception ex) { WriteError($"?GREP non disponibile ({ex.Message})"); }
        return Task.CompletedTask;
    }

    private static string JsonStr(string s) =>
        "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";

    /// <summary>Tokenizer shell-like minimale: rispetta apici singoli/doppi.</summary>
    private static List<string> SplitShellArgs(string s)
    {
        var toks = new List<string>();
        var cur = new System.Text.StringBuilder();
        char quote = '\0'; bool inTok = false;
        foreach (var c in s)
        {
            if (quote != '\0') { if (c == quote) quote = '\0'; else cur.Append(c); inTok = true; }
            else if (c == '"' || c == '\'') { quote = c; inTok = true; }
            else if (char.IsWhiteSpace(c)) { if (inTok) { toks.Add(cur.ToString()); cur.Clear(); inTok = false; } }
            else { cur.Append(c); inTok = true; }
        }
        if (inTok) toks.Add(cur.ToString());
        return toks;
    }

    private Task HandleRenameFileAsync(string args, CancellationToken __)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 2) { WriteError("?Usage: RENAME <old> <new>"); return Task.CompletedTask; }
        var oldPath = Path.Combine(_projectPath, parts[0]);
        var newPath = Path.Combine(_projectPath, parts[1]);
        try { File.Move(oldPath, newPath); Out.WriteLine($"Renamed: {parts[0]} -> {parts[1]}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleDeleteFileAsync(string args, CancellationToken __)
    {
        if (string.IsNullOrWhiteSpace(args)) { WriteError("?Usage: DELF <file>"); return Task.CompletedTask; }
        var path = Path.Combine(_projectPath, args.Trim());
        try { File.Delete(path); Out.WriteLine($"Deleted: {args}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleCopyFileAsync(string args, CancellationToken __)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 2) { WriteError("?Usage: COPY <src> <dst>"); return Task.CompletedTask; }
        var src = Path.Combine(_projectPath, parts[0]);
        var dst = Path.Combine(_projectPath, parts[1]);
        try { File.Copy(src, dst, true); Out.WriteLine($"Copied: {parts[0]} -> {parts[1]}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleDuplicateFileAsync(string args, CancellationToken __)
    {
        if (string.IsNullOrWhiteSpace(args)) { WriteError("?Usage: DUPLICATE <file>"); return Task.CompletedTask; }
        var path = Path.Combine(_projectPath, args.Trim());
        var dir = Path.GetDirectoryName(path) ?? _projectPath;
        var name = Path.GetFileNameWithoutExtension(path);
        var ext = Path.GetExtension(path);
        var dupPath = Path.Combine(dir, $"{name}_copy{ext}");
        try { File.Copy(path, dupPath, true); Out.WriteLine($"Duplicated: {args} -> {Path.GetFileName(dupPath)}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleBackupFileAsync(string args, CancellationToken __)
    {
        if (string.IsNullOrWhiteSpace(args)) { WriteError("?Usage: BACKUP <file>"); return Task.CompletedTask; }
        var path = Path.Combine(_projectPath, args.Trim());
        var bakPath = path + ".bak";
        try { File.Copy(path, bakPath, true); Out.WriteLine($"Backup: {args} -> {Path.GetFileName(bakPath)}"); }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private Task HandleDeployAsync(string args, CancellationToken __)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var runtime = parts.SkipWhile(p => p != "-r").Skip(1).FirstOrDefault();
        var fileArgs = parts.Where(p => p != "-r" && p != runtime).ToArray();
        var file = fileArgs.Length > 0 ? fileArgs[0].Trim().Trim('"') : null;

        if (string.IsNullOrWhiteSpace(file))
        {
            WriteError("?Usage: DEPLOY <file.bas> [-r <runtime>]");
            return Task.CompletedTask;
        }

        if (!file.EndsWith(".bas", StringComparison.OrdinalIgnoreCase))
            file += ".bas";

        if (!File.Exists(file))
        {
            WriteError($"?File not found: {file}");
            return Task.CompletedTask;
        }

        try
        {
            var program = global::Basic.Orchestrator.Compile(file, null, false);
            if (program?.MainModule == null) { WriteError("?Compilation failed."); return Task.CompletedTask; }

            // DEPLOY impacchetta SOLO exe+dll+runtimeconfig+asm: la closure del runtime non
            // entra nello zip, quindi copiarla (262 MB) è puro costo. Symlink come nel
            // percorso di esecuzione.
            var exePath = global::Basic.IlBackend.IlProgramCompiler.Compile(program, file, Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin"), Path.GetFileNameWithoutExtension(file), linkRuntime: true);
            Out.WriteLine($"[IL] compiled -> {exePath}");

            var outputDir = Path.GetDirectoryName(exePath)!;
            var baseName = Path.GetFileNameWithoutExtension(exePath);
            var zipName = Path.Combine(_projectPath, $"{baseName}.zip");

            var filesToPack = new List<string> { exePath };

            var dllPath = Path.Combine(outputDir, $"{baseName}.dll");
            if (File.Exists(dllPath)) filesToPack.Add(dllPath);

            var configPath = Path.Combine(outputDir, $"{baseName}.runtimeconfig.json");
            if (File.Exists(configPath)) filesToPack.Add(configPath);

            var asmPath = Path.Combine(outputDir, $"{baseName}.asm");
            if (File.Exists(asmPath)) filesToPack.Add(asmPath);

            using var zip = ZipFile.Open(zipName, ZipArchiveMode.Create);
            foreach (var f in filesToPack)
            {
                var entryName = Path.GetFileName(f);
                zip.CreateEntry(entryName);
                using var entryStream = zip.GetEntry(entryName)!.Open();
                using var fileStream = File.OpenRead(f);
                fileStream.CopyTo(entryStream);
            }

            Out.WriteLine($"[DEPLOY] {zipName} ({filesToPack.Count} files)");
        }
        catch (Exception ex)
        {
            WriteError($"?Error: {ex.Message}");
        }

        return Task.CompletedTask;
    }

    private async Task HandleConvertAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { WriteError("?Usage: CONVERT \"file.ext\" [-o out.ext]"); return; }
        var file = parts[0];
        var outIdx = parts.FindIndex(p => p == "-o");
        var outFile = outIdx >= 0 && outIdx + 1 < parts.Count ? parts[outIdx + 1] : null;
        if (!File.Exists(file)) { WriteError($"?File not found: {file}"); return; }
        try
        {
            var bytes = await File.ReadAllBytesAsync(file, ct);
            var inExt = Path.GetExtension(file);
            var outExt = outFile != null ? Path.GetExtension(outFile) : ".md";
            var result = DocumentConverter.FromBytes(bytes, inExt, outExt);
            if (outFile != null) { await File.WriteAllTextAsync(outFile, result, ct); Out.WriteLine($"[OK] {outFile} ({result.Length} bytes)"); }
            else Out.WriteLine(result);
        }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
    }

    private async Task HandleBatchAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { WriteError("?Usage: BATCH \"dir/\" [-r] [-o ext] or BATCH \"*.ext\""); return; }
        var pattern = parts[0];
        var recursive = parts.Contains("-r");
        var outIdx = parts.FindIndex(p => p == "-o");
        var outExt = outIdx >= 0 && outIdx + 1 < parts.Count ? parts[outIdx + 1] : ".md";
        if (!outExt.StartsWith('.')) outExt = "." + outExt;

        var dir = Directory.Exists(pattern) ? pattern : Path.GetDirectoryName(pattern) ?? ".";
        var searchPattern = Directory.Exists(pattern) ? "*.*" : Path.GetFileName(pattern);
        var searchOpt = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
        string[] files;
        try { files = Directory.GetFiles(dir, searchPattern, searchOpt); }
        catch (Exception ex) { WriteError($"?{ex.Message}"); return; }

        if (files.Length == 0) { WriteError("?No files found"); return; }
        int ok = 0;
        foreach (var f in files)
        {
            ct.ThrowIfCancellationRequested();
            try
            {
                var ext = Path.GetExtension(f).ToLowerInvariant();
                if (ext is ".md" or ".html" or ".txt" or ".exe" or ".dll" or ".pdb") continue;
                var bytes = await File.ReadAllBytesAsync(f, ct);
                var result = DocumentConverter.FromBytes(bytes, ext, outExt);
                var outFile = Path.ChangeExtension(f, outExt);
                await File.WriteAllTextAsync(outFile, result, ct);
                Out.WriteLine($"[OK] {Path.GetFileName(f)} -> {Path.GetFileName(outFile)}");
                ok++;
            }
            catch (Exception ex) { WriteError($"?{Path.GetFileName(f)}: {ex.Message}"); }
        }
        Out.WriteLine($"Done: {ok}/{files.Length} converted");
    }

    private Task HandleConvFmtAsync(string _, CancellationToken __)
    {
        Out.WriteLine("Input formats:  " + Basic.IlBackend.ConversionLazy.ConvertFormats());
        Out.WriteLine("Output formats: md, html, txt");
        return Task.CompletedTask;
    }

    private async Task HandleWeb2MdAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { WriteError("?Usage: WEB2MD \"https://...\" [-o out.md]"); return; }
        var url = parts[0];
        var outIdx = parts.FindIndex(p => p == "-o");
        var outFile = outIdx >= 0 && outIdx + 1 < parts.Count ? parts[outIdx + 1] : null;
        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("GMBasic/1.0");
            var html = await http.GetStringAsync(url, ct);
            var doc = HtmlConverter.Parse(System.Text.Encoding.UTF8.GetBytes(html));
            var result = MarkdownRenderer.Render(doc);
            if (outFile != null) { await File.WriteAllTextAsync(outFile, result, ct); Out.WriteLine($"[OK] {outFile}"); }
            else Out.WriteLine(result);
        }
        catch (Exception ex) { WriteError($"?Error: {ex.Message}"); }
    }

    private static List<string> ParseArgs(string args)
    {
        var result = new List<string>();
        var sb = new System.Text.StringBuilder();
        bool inQuotes = false;
        for (int i = 0; i < args.Length; i++)
        {
            var c = args[i];
            if (c == '"') { inQuotes = !inQuotes; continue; }
            if (c == ' ' && !inQuotes) { if (sb.Length > 0) { result.Add(sb.ToString()); sb.Clear(); } continue; }
            sb.Append(c);
        }
        if (sb.Length > 0) result.Add(sb.ToString());
        return result;
    }

    private async Task HandleProvidersAsync(string args, CancellationToken ct)
    {
        Basic.Providers.ProviderConfig.EnsureDefaults();
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        
        if (parts.Length > 0 && parts[0].Equals("WIZARD", StringComparison.OrdinalIgnoreCase))
        { await ProviderWizard(ct); return; }
        if (parts.Length > 0 && parts[0].Equals("LOAD", StringComparison.OrdinalIgnoreCase))
        { Basic.Providers.ProviderConfig.Load(); Out.WriteLine("[OK] Providers loaded."); return; }
        if (parts.Length > 0 && parts[0].Equals("SAVE", StringComparison.OrdinalIgnoreCase))
        { Basic.Providers.ProviderConfig.Save(); Out.WriteLine($"[OK] Saved to {Basic.Providers.ProviderConfig.DefaultPath}"); return; }
        if (parts.Length > 0 && parts[0].Equals("LIST", StringComparison.OrdinalIgnoreCase))
        { ListProviders(); return; }
        if (parts.Length > 0 && parts[0].Equals("SETKEY", StringComparison.OrdinalIgnoreCase) && parts.Length >= 3)
        {
            var name = parts[1]; var key = parts[2];
            var desc = new Basic.Providers.ProviderDescriptor("completion", name, "openai");
            desc.SetConfig("apiKey", key);
            Basic.Providers.ProviderServer.Instance.Register("completion", name, "openai", desc.Config);
            Basic.Providers.ProviderConfig.Save();
            Out.WriteLine($"[OK] API key set for '{name}' and saved.");
            return;
        }

        // Interactive menu
        while (true)
        {
            _activeConsole?.Clear();
            Out.WriteLine("╔══════════════════════════════════════════╗");
            Out.WriteLine("║           AI Providers                   ║");
            Out.WriteLine("╠══════════════════════════════════════════╣");
            Out.WriteLine("║ [1] List all providers                   ║");
            Out.WriteLine("║ [2] Set API key for a provider           ║");
            Out.WriteLine("║ [3] Wizard (add new provider)            ║");
            Out.WriteLine("║ [4] Load from file                       ║");
            Out.WriteLine("║ [5] Save to file                         ║");
            Out.WriteLine("║ [0] Exit                                 ║");
            Out.WriteLine("╚══════════════════════════════════════════╝");
            Out.WriteLine($"Config: {Basic.Providers.ProviderConfig.DefaultPath}");
            Out.Write("Choice: ");
            var key = Console.ReadKey(true).KeyChar;
            Out.WriteLine(key);
            switch (key)
            {
                case '1': ListProviders(); break;
                case '2': await SetApiKey(); break;
                case '3': await ProviderWizard(ct); break;
                case '4': Basic.Providers.ProviderConfig.Load(); Out.WriteLine("[OK] Loaded."); break;
                case '5': Basic.Providers.ProviderConfig.Save(); Out.WriteLine("[OK] Saved."); break;
                case '0': return;
            }
            if (key != '0') { Out.WriteLine("\nPress any key..."); Console.ReadKey(true); }
        }
    }

    private static void ListProviders()
    {
        Out.WriteLine();
        try
        {
            var path = Basic.Providers.ProviderConfig.DefaultPath;
            if (!File.Exists(path)) { Out.WriteLine("(no providers configured)"); return; }
            var json = File.ReadAllText(path);
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            if (!doc.RootElement.TryGetProperty("providers", out var arr)) { Out.WriteLine("(empty)"); return; }
            Out.WriteLine($"{"Name",-28} {"Kind",-14} {"Model",-25} Key");
            Out.WriteLine(new string('-', 90));
            foreach (var p in arr.EnumerateArray())
            {
                var name = p.GetProperty("name").GetString() ?? "";
                var kind = p.TryGetProperty("kind", out var k) ? k.GetString() ?? "" : "";
                var cfg = p.TryGetProperty("config", out var c) ? c : default;
                var model = cfg.ValueKind == System.Text.Json.JsonValueKind.Object && cfg.TryGetProperty("model", out var m) ? m.GetString() ?? "" : "";
                var hasKey = cfg.ValueKind == System.Text.Json.JsonValueKind.Object && cfg.TryGetProperty("apiKey", out var ak) && ak.GetString()?.Length > 0;
                Out.WriteLine($"{name,-28} {kind,-14} {model,-25} {(hasKey ? "***" : "-")}");
            }
        }
        catch (Exception ex) { WriteError($"?{ex.Message}"); }
    }

    private static async Task SetApiKey()
    {
        Out.Write("Provider name: ");
        var name = Console.ReadLine()?.Trim() ?? "";
        if (string.IsNullOrEmpty(name)) return;
        Out.Write("API Key: ");
        var key = Console.ReadLine()?.Trim() ?? "";
        var desc = new Basic.Providers.ProviderDescriptor("completion", name, "openai");
        desc.SetConfig("apiKey", key);
        Basic.Providers.ProviderServer.Instance.Register("completion", name, "openai", desc.Config);
        Basic.Providers.ProviderConfig.Save();
        Out.WriteLine($"[OK] Key set for '{name}'.");
    }

    private static async Task ProviderWizard(CancellationToken ct)
    {
        Out.WriteLine("Role [completion|embedding] (default completion): ");
        var role = Console.ReadLine()?.Trim().ToLowerInvariant() ?? "";
        if (role is not ("completion" or "embedding")) role = "completion";

        Out.WriteLine("Provider type [openai|anthropic|ollama|llamacpp|mlx|deepseek|openrouter/...]: ");
        var slug = Console.ReadLine()?.Trim().ToLowerInvariant() ?? "";
        if (string.IsNullOrEmpty(slug)) return;

        string kind;
        if (slug is "llamacpp" or "mlx" or "onnx")
            kind = slug;
        else
            kind = slug == "anthropic" ? "anthropic" : slug == "ollama" ? "ollama" : "openai";

        Out.Write($"Name [{slug}-default]: ");
        var name = Console.ReadLine()?.Trim();
        if (string.IsNullOrEmpty(name)) name = $"{slug}-default";

        var config = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (kind is "llamacpp" or "mlx")
        {
            Out.Write("Model path / dir (es. ~/ai/Models/... o ~/ai/Models/gguf/x.gguf): ");
            var modelPath = Console.ReadLine()?.Trim() ?? "";
            if (string.IsNullOrEmpty(modelPath)) { Out.WriteLine("?path modello obbligatorio"); return; }
            config[kind == "llamacpp" ? "modelPath" : "modelDir"] = modelPath;
            Out.Write("Context size [4096]: ");
            if (int.TryParse(Console.ReadLine()?.Trim(), out var ctx) && ctx > 0) config["ctxSize"] = ctx.ToString();
            Out.Write("Embedding dimensions [384]: ");
            if (int.TryParse(Console.ReadLine()?.Trim(), out var dims) && dims > 0) config["dimensions"] = dims.ToString();
        }
        else
        {
            Out.Write("API Key (optional): ");
            var apiKey = Console.ReadLine()?.Trim() ?? "";
            Out.Write("Model: ");
            var model = Console.ReadLine()?.Trim() ?? "";
            config["model"] = model;
            if (!string.IsNullOrEmpty(apiKey)) config["apiKey"] = apiKey;
            if (Basic.Providers.KnownProviders.Map.TryGetValue(slug, out var info))
            {
                config["baseUrl"] = info.BaseUrl;
                foreach (var (k, v) in info.Headers) config[k] = v;
            }
        }

        Basic.Providers.ProviderServer.Instance.Register(role, name, kind, config);
        Basic.Providers.ProviderConfig.Save();
        Out.WriteLine($"[OK] Provider '{name}' ({role}, {kind}) registered and saved to {Basic.Providers.ProviderConfig.DefaultPath}");
    }

    // ================================================================
    // HF / MLX / AIPATH — model download commands (AI/ModelDownload)
    // ================================================================

    private static readonly HttpClient ModelDownloadHttp = new();

    private sealed class DownloadArgError(string message) : Exception(message);

    private Task HandleHfDownloadAsync(string args, CancellationToken ct) => HandleModelDownloadAsync(ModelDownloadKind.Hf, args, ct);
    private Task HandleMlxDownloadAsync(string args, CancellationToken ct) => HandleModelDownloadAsync(ModelDownloadKind.Mlx, args, ct);

    /// <summary>SETUP [TORCH|BROWSER|OCR|ALL]|STATUS|RM|HELP — comando unificato per i
    /// componenti runtime scaricabili on-demand (sostituisce ML e BROWSER). Manifest in
    /// Resources/; cache componente ~/.gmbasic/libs/&lt;comp&gt;/&lt;rid&gt;/ (GmbiPaths).</summary>
    /// <summary>Comando REPL `LIBS [update|check|publish|help] ...` — stessa logica del CLI
    /// `gmb libs` (bundle delle librerie native di supporto per GMBasicModules).</summary>
    private async Task HandleLibsAsync(string args, CancellationToken ct)
    {
        var argv = new List<string> { "libs" };
        argv.AddRange(args.Split(' ', StringSplitOptions.RemoveEmptyEntries));
        if (argv.Count == 1) argv.Add("help");
        CliLibs.TryDispatch(argv.ToArray());
        await Task.CompletedTask;
    }

    private async Task HandleSetupAsync(string args, CancellationToken ct)
    {
        var parts = args.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var sub = parts.Length > 0 ? parts[0].ToUpperInvariant() : "SETUP";
        var rest = parts.Skip(1).ToArray();
        var force = rest.Contains("--force", StringComparer.OrdinalIgnoreCase);
        var quiet = rest.Contains("--quiet", StringComparer.OrdinalIgnoreCase);
        var yes = rest.Contains("--yes", StringComparer.OrdinalIgnoreCase);

        switch (sub)
        {
            case "HELP":
                PrintSetupHelp();
                break;
            case "STATUS":
            {
                var rid = MlCache.ResolveRid();
                Out.WriteLine($"RID={rid}");
                foreach (var c in SetupComponents.All)
                    Out.WriteLine($"  {c.Name,-8} cache={c.CacheDir()}  stato={(c.IsCached(rid) ? "COMPLETA" : "assente → SETUP " + c.Name)}");
                break;
            }
            case "RM":
            {
                var target = rest.Length > 0 ? rest[0].ToUpperInvariant() : "ALL";
                await RemoveSetupAsync(target, force, ct).ConfigureAwait(false);
                break;
            }
            default:
            {
                var target = sub == "SETUP" ? (rest.Length > 0 ? rest[0].ToUpperInvariant() : "ALL") : sub;
                if (target != "ALL" && SetupComponents.Find(target) == null)
                {
                    PrintSetupHelp();
                    break;
                }
                if (target == "ALL" && !yes && !force)
                {
                    Out.WriteLine("Componenti da scaricare in <cache>/<comp>/<rid>/ (~/.gmbasic/libs):");
                    foreach (var c in SetupComponents.All)
                        Out.WriteLine($"  {c.Name,-8} {c.Description}");
                    Out.Write("Procedere? [y/N] ");
                    var a = Console.ReadLine();
                    if (a == null || !(a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase)
                                    || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase)))
                    { Out.WriteLine("annullato."); break; }
                }
                await RunSetupAsync(target, force, quiet, ct).ConfigureAwait(false);
                break;
            }
        }
    }

    private async Task RunSetupAsync(string target, bool force, bool quiet, CancellationToken ct)
    {
        var comps = target == "ALL" ? SetupComponents.All : new[] { SetupComponents.Find(target)! };
        foreach (var c in comps)
        {
            var rid = MlCache.ResolveRid();
            if (!force && c.IsCached(rid))
            {
                Out.WriteLine($"[{c.Name.ToLowerInvariant()}] già in cache e valido: {c.CacheDir()}");
                continue;
            }
            if (force)
            {
                var dir = c.CacheDir();
                if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
            }
            Out.WriteLine($"[{c.Name.ToLowerInvariant()}] RID={rid} — scarico in {c.CacheDir()}...");
            var bar = new ConsoleProgressBar();
            var res = await c.Setup(quiet ? null : Console.WriteLine,
                (done, total) => bar.Report(new ModelDownloader.DownloadProgress(done, total, total > 0 ? done * 100.0 / total : 0, null)),
                ct).ConfigureAwait(false);
            bar.Finish();
            if (res.Success)
                Out.WriteLine($"[{c.Name.ToLowerInvariant()}] OK: {res.CacheDir} ({res.Files.Count} file). {c.ReadyNote}");
            else
                WriteError($"?{c.Name} setup fallito: {res.Error}");
        }
    }

    private async Task RemoveSetupAsync(string target, bool force, CancellationToken ct)
    {
        var comps = target == "ALL" ? SetupComponents.All : new[] { SetupComponents.Find(target)! };
        foreach (var c in comps)
        {
            var dir = c.CacheDir();
            if (!Directory.Exists(dir)) { Out.WriteLine($"[{c.Name.ToLowerInvariant()}] nessuna cache da eliminare."); continue; }
            var ok = force;
            if (!ok)
            {
                Out.Write($"[{c.Name.ToLowerInvariant()}] eliminare {dir}? [y/N] ");
                var a = Console.ReadLine();
                ok = a != null && (a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase) || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase));
            }
            if (ok) { Directory.Delete(dir, recursive: true); Out.WriteLine($"[{c.Name.ToLowerInvariant()}] cache eliminata."); }
            else Out.WriteLine($"[{c.Name.ToLowerInvariant()}] annullato.");
        }
    }

    private static void PrintSetupHelp()
    {
        Out.WriteLine("SETUP — gestione componenti runtime scaricabili on-demand (manifest in Resources/)");
        Out.WriteLine("  SETUP [TORCH|BROWSER|OCR|ALL] [--force] [--quiet] [--yes]");
        Out.WriteLine("  SETUP STATUS                stato cache di tutti i componenti");
        Out.WriteLine("  SETUP RM <TORCH|BROWSER|OCR|ALL> [--force]");
        Out.WriteLine();
        Out.WriteLine("Componenti (in <cache>/<comp>/<rid>/ = ~/.gmbasic/libs):");
        foreach (var c in SetupComponents.All)
            Out.WriteLine($"  {c.Name,-8} {c.Description}");
        Out.WriteLine("  ALL        tutti i componenti sopra");
        Out.WriteLine();
        Out.WriteLine("Nessun modello/libreria è dato per scontato sull'host: si scarica solo");
        Out.WriteLine("quando serve, con SHA256 verificato. ML e BROWSER sono stati sostituiti da SETUP.");
    }

    private async Task HandleModelDownloadAsync(ModelDownloadKind kind, string args, CancellationToken ct)
    {
        var tokens = TokenizeDownloadArgs(args);
        if (tokens.Count == 0 || tokens[0] is "--help" or "-h")
        {
            PrintDownloadHelp(kind);
            return;
        }

        string repoId = tokens[0];
        string? file = null, dest = null;
        int stallTimeout = 5, stallRetries = 1, workers = 8;
        bool noXet = false, noToken = false, noCurl = false, quiet = false;

        try
        {
            for (var i = 1; i < tokens.Count; i++)
            {
                switch (tokens[i])
                {
                    case "--file": file = RequireDownloadArgValue(tokens, ref i, "--file"); break;
                    case "--dest": dest = RequireDownloadArgValue(tokens, ref i, "--dest"); break;
                    case "--stall-timeout": stallTimeout = ParseIntArg(RequireDownloadArgValue(tokens, ref i, "--stall-timeout"), "--stall-timeout"); break;
                    case "--stall-retries": stallRetries = ParseIntArg(RequireDownloadArgValue(tokens, ref i, "--stall-retries"), "--stall-retries"); break;
                    case "--workers": workers = ParseIntArg(RequireDownloadArgValue(tokens, ref i, "--workers"), "--workers"); break;
                    case "--no-xet": noXet = true; break;
                    case "--no-token": noToken = true; break;
                    case "--no-curl": noCurl = true; break;
                    case "--quiet": quiet = true; break;
                    case "--help": case "-h": PrintDownloadHelp(kind); return;
                    default: WriteError($"?Opzione sconosciuta: {tokens[i]}. Usare '{(kind == ModelDownloadKind.Hf ? "HF" : "MLX")} --help' per l'elenco completo."); return;
                }
            }
        }
        catch (DownloadArgError ex) { WriteError($"?{ex.Message}"); return; }

        if (kind == ModelDownloadKind.Mlx && file is not null)
        {
            WriteError("?--file non e' supportato da MLX (scarica sempre l'intero repo). Usare HF per selezionare un singolo file.");
            return;
        }

        var opts = new ModelDownloadOptions
        {
            RepoId = repoId,
            Kind = kind,
            File = file,
            Dest = dest,
            StallTimeoutMinutes = stallTimeout,
            StallRetries = stallRetries,
            Workers = workers,
            NoXet = noXet,
            NoToken = noToken,
            NoCurl = noCurl,
            Quiet = quiet,
        };

        Out.WriteLine($"[{(kind == ModelDownloadKind.Hf ? "HF" : "MLX")}] Download '{repoId}'...");
        ModelDownloadResult result;
        try
        {
            result = await ModelDownloader.DownloadAsync(opts, ModelDownloadHttp, Console.WriteLine, ct).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            Out.WriteLine("\n[interrotto]");
            return;
        }

        if (!result.Success)
        {
            WriteError($"?Download fallito: {result.Error}");
            return;
        }

        Out.WriteLine($"[OK] {result.Files.Count} file salvati in {result.DestDir}");
        foreach (var f in result.Files) Out.WriteLine($"  {f}");
    }

    private static List<string> TokenizeDownloadArgs(string args)
    {
        var tokens = new List<string>();
        var current = new StringBuilder();
        var inQuotes = false;
        foreach (var ch in args)
        {
            if (ch == '"') { inQuotes = !inQuotes; continue; }
            if (!inQuotes && char.IsWhiteSpace(ch))
            {
                if (current.Length > 0) { tokens.Add(current.ToString()); current.Clear(); }
                continue;
            }
            current.Append(ch);
        }
        if (current.Length > 0) tokens.Add(current.ToString());
        return tokens;
    }

    private static string RequireDownloadArgValue(List<string> tokens, ref int i, string flag)
    {
        if (i + 1 >= tokens.Count) throw new DownloadArgError($"{flag} richiede un valore.");
        return tokens[++i];
    }

    private static int ParseIntArg(string value, string flag)
    {
        if (!int.TryParse(value, out var n) || n < 0) throw new DownloadArgError($"{flag}: valore numerico non valido '{value}'.");
        return n;
    }

    private static void PrintDownloadHelp(ModelDownloadKind kind)
    {
        var isHf = kind == ModelDownloadKind.Hf;
        var cmd = isHf ? "HF" : "MLX";
        Out.WriteLine();
        Out.WriteLine(isHf
            ? "HF <repoId> [opzioni]  -  Scarica uno o piu' file da un repo HuggingFace (tipicamente GGUF)."
            : "MLX <repoId> [opzioni]  -  Scarica un repo MLX completo da HuggingFace (stessa semantica di 'rapid-mlx pull').");
        Out.WriteLine();
        Out.WriteLine("Sintassi:");
        Out.WriteLine(isHf
            ? "  HF <org/repo> [--file <nome|pattern>] [opzioni comuni]"
            : "  MLX <org/repo> [opzioni comuni]");
        Out.WriteLine();
        Out.WriteLine("Trasporto: curl (con resume -C -, retry interno, e watchdog di stallo che");
        Out.WriteLine("killa e riavvia il download se il file non cresce per --stall-timeout minuti);");
        Out.WriteLine("stessa logica dimostrata stabile in Rapid-MLX-NextGen. Nessun trasporto");
        Out.WriteLine("alternativo e' implementato in questa porta (vedi --no-curl sotto).");
        Out.WriteLine();
        Out.WriteLine("Parametri:");
        if (isHf)
        {
            Out.WriteLine("  --file <nome|pattern>   Nome esatto, oppure pattern con wildcard '*', del file");
            Out.WriteLine("                          da scaricare. Obbligatorio se il repo contiene piu' di");
            Out.WriteLine("                          un file .gguf (evita download accidentali dell'intero");
            Out.WriteLine("                          repo). Se il pattern seleziona piu' file, vengono");
            Out.WriteLine("                          scaricati tutti (utile per GGUF shardati multi-parte).");
        }
        Out.WriteLine("  --dest <path>           Cartella di destinazione. Default: primo percorso");
        Out.WriteLine("                          configurato con AIPATH, altrimenti");
        Out.WriteLine($"                          samples/ai/models/{(isHf ? "gguf" : "mlx")}/{(isHf ? "" : "<repo>/")}.");
        Out.WriteLine("  --stall-timeout <min>   Minuti senza crescita del file prima di killare e");
        Out.WriteLine("                          riavviare il download (resume automatico). Default: 5.");
        Out.WriteLine("  --stall-retries <n>     Numero di riavvii per stallo prima di arrendersi. Default: 1.");
        Out.WriteLine("  --workers <n>           Download paralleli quando ci sono piu' file da scaricare");
        Out.WriteLine("                          (repo MLX multi-file, o pattern HF multi-match). Default: 8.");
        Out.WriteLine("  --no-xet                Accettato per compatibilita' con la sintassi originale;");
        Out.WriteLine("                          e' un no-op qui perche' curl scarica sempre via HTTP");
        Out.WriteLine("                          classico dall'endpoint 'resolve' e non usa mai Xet.");
        Out.WriteLine("  --no-token              Non invia il token HF (variabile HF_TOKEN o");
        Out.WriteLine("                          ~/.cache/huggingface/token) anche se disponibile.");
        Out.WriteLine("  --no-curl               Non supportato in questa porta nativa: termina subito con");
        Out.WriteLine("                          errore invece di far finta di avere un fallback stabile.");
        Out.WriteLine("  --quiet                 Sopprime i messaggi di progresso (solo esito finale).");
        Out.WriteLine("  --help, -h              Mostra questo help.");
        Out.WriteLine();
        Out.WriteLine("Verifica integrita': dopo ogni file, confronto dimensione (e sha256 se il repo");
        Out.WriteLine("e' LFS-tracked) contro il manifest HuggingFace; in caso di mismatch il file viene");
        Out.WriteLine("cancellato e riprovato.");
        Out.WriteLine();
        Out.WriteLine("Esempio (tutti i parametri):");
        Out.WriteLine(isHf
            ? "  HF bartowski/Llama-3.2-1B-Instruct-GGUF --file Llama-3.2-1B-Instruct-Q4_K_M.gguf \\"
            : "  MLX mlx-community/Qwen2.5-0.5B-Instruct-4bit \\");
        Out.WriteLine("      --dest ~/ai/models --stall-timeout 5 --stall-retries 2 --workers 4 \\");
        Out.WriteLine("      --no-xet --no-token --quiet");
        Out.WriteLine();
        Out.WriteLine($"Vedi anche: {(isHf ? "MLX" : "HF")}, AIPATH.");
    }

    private Task HandleAiPathAsync(string args, CancellationToken __)
    {
        var trimmed = args.Trim();
        if (trimmed.Length == 0)
        {
            PrintAiPath();
            return Task.CompletedTask;
        }

        if (trimmed.StartsWith('='))
        {
            var value = trimmed[1..].Trim();
            var paths = value.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();
            // Config AI unica: il REPL scrive gli stessi modelPaths dei comandi BASIC AI PATH MODEL.
            var cfg = Basic.AI.AiConfig.Load();
            Basic.AI.AiConfig.Save(cfg with { ModelPaths = paths });
            PrintAiPath();
            return Task.CompletedTask;
        }

        WriteError("?Sintassi non valida. Usare 'AIPATH' per leggere, 'AIPATH=path1;path2;' per impostare.");
        return Task.CompletedTask;
    }

    private static void PrintAiPath()
    {
        var paths = Basic.AI.AiConfig.ModelPaths;
        Out.WriteLine(paths.Count == 0
            ? "AIPATH= (non impostato; HF/MLX useranno samples/ai/models/<tipo>/ come default)"
            : $"AIPATH={string.Join(';', paths)}");
    }

    // ── LLM (port 1:1 di GMBasic Executor/ReplEngine.HandleLlmAsync): elenca i modelli
    //    trovati nei percorsi AIPATH, rilevando il tipo dal contenuto della cartella. ──
    private Task HandleLlmAsync(string args, CancellationToken __)
    {
        var filterArg = args.Trim();
        if (filterArg is "--help" or "-h")
        {
            PrintLlmHelp();
            return Task.CompletedTask;
        }

        DetectedModelType? filter;
        switch (filterArg.ToUpperInvariant())
        {
            case "": filter = null; break;
            case "GGUF": filter = DetectedModelType.Gguf; break;
            case "MLX": filter = DetectedModelType.Mlx; break;
            case "OLLAMA": filter = DetectedModelType.Ollama; break;
            default: WriteError($"?Tipo sconosciuto '{filterArg}'. Usare GGUF, MLX oppure OLLAMA."); return Task.CompletedTask;
        }

        var configured = Basic.AI.AiConfig.ModelPaths;
        var roots = configured.Count > 0 ? configured.ToList() : ModelDownloader.DefaultAiPathFallbacks().ToList();
        roots.Add("~/.ollama/models"); // sempre controllato: store Ollama, mai scaricato da noi, solo trovato

        var models = ModelScanner.ScanAll(roots);
        if (filter is { } f) models = models.Where(m => m.Type == f).ToList();
        models = models.OrderBy(m => m.Type).ThenBy(m => m.Name, StringComparer.OrdinalIgnoreCase).ToList();

        if (models.Count == 0)
        {
            Out.WriteLine(configured.Count == 0
                ? "Nessun modello trovato (AIPATH non impostato; cercato nei default samples/ai/models/gguf|mlx e ~/.ollama/models)."
                : "Nessun modello trovato nei percorsi AIPATH configurati.");
            return Task.CompletedTask;
        }

        var nameWidth = Math.Max(20, models.Max(m => m.Name.Length));
        foreach (var m in models)
        {
            var tag = m.Type switch { DetectedModelType.Gguf => "[GGUF]  ", DetectedModelType.Mlx => "[MLX]   ", _ => "[OLLAMA]" };
            var pathNote = m.Type == DetectedModelType.Ollama ? "(gestito da ollama, non scaricabile da qui)" : m.Path;
            Out.WriteLine($"{tag} {m.Name.PadRight(nameWidth)}  {pathNote}");
        }
        return Task.CompletedTask;
    }

    private static void PrintLlmHelp()
    {
        Out.WriteLine();
        Out.WriteLine("LLM [tipo]  -  Elenca i modelli trovati in tutti i percorsi AIPATH (rilevando il");
        Out.WriteLine("               tipo dal contenuto della cartella, mai dichiarato a mano).");
        Out.WriteLine();
        Out.WriteLine("Sintassi:");
        Out.WriteLine("  LLM              Elenca tutti i modelli trovati.");
        Out.WriteLine("  LLM GGUF         Solo modelli GGUF (un file .gguf trovato in una cartella AIPATH).");
        Out.WriteLine("  LLM MLX          Solo repo MLX (cartella con config.json + almeno un .safetensors).");
        Out.WriteLine("  LLM OLLAMA       Solo modelli gestiti dallo store locale di Ollama.");
        Out.WriteLine("  LLM --help, -h   Mostra questo help.");
        Out.WriteLine();
        Out.WriteLine("Percorsi cercati: AIPATH se impostato, altrimenti i default samples/ai/models/gguf/ e");
        Out.WriteLine("samples/ai/models/mlx/. ~/.ollama/models e' sempre controllato in aggiunta.");
        Out.WriteLine();
        Out.WriteLine("Vedi anche: HF, MLX, AIPATH.");
    }

    // ── RAG / GRAPH (REPL): costruzione RAG wiki + grafi Obsidian ──────────
    // La logica vive nei runtime riusabili (RagIndexService / GraphBuildService in common/AI);
    // qui solo parsing args, progress bar e orchestrazione dell'aperitura db.

    private async Task HandleRagAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { PrintRagHelp(); return; }
        var cmd = parts[0].ToUpperInvariant();
        try
        {
            switch (cmd)
            {
                case "BUILD": await RunRagBuildAsync(parts.Skip(1).ToList(), isUpdate: false, ct); break;
                case "UPDATE":
                case "REFRESH": await RunRagBuildAsync(parts.Skip(1).ToList(), isUpdate: true, ct); break;
                case "STATUS": RunRagStatus(parts.Skip(1).ToList()); break;
                case "QUERY": await RunRagQueryAsync(parts.Skip(1).ToList(), ct); break;
                case "HELP": PrintRagHelp(); break;
                default: WriteError("?Comando RAG sconosciuto: BUILD|UPDATE|STATUS|QUERY (usa RAG HELP)"); break;
            }
        }
        catch (OperationCanceledException) { Out.WriteLine("[STOP] interrotto. Riprendi con RAG BUILD ... --continue"); }
        catch (Exception ex) { WriteError($"?{ex.Message}"); }
    }

    private async Task HandleGraphAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { PrintGraphHelp(); return; }
        var cmd = parts[0].ToUpperInvariant();
        try
        {
            switch (cmd)
            {
                case "BUILD": await RunGraphBuildAsync(parts.Skip(1).ToList(), isUpdate: false, ct); break;
                case "UPDATE":
                case "REFRESH": await RunGraphBuildAsync(parts.Skip(1).ToList(), isUpdate: true, ct); break;
                case "VIEW": await RunGraphViewAsync(parts.Skip(1).ToList(), ct); break;
                case "WATCH":
                    RunGraphWatch(parts.Skip(1).ToList(), start: true); break;
                case "UNWATCH":
                    RunGraphWatch(parts.Skip(1).ToList(), start: false); break;
                case "ENRICH":
                    if (parts.Count > 1 && parts[1].ToUpperInvariant() == "SEMANTIC")
                    { RunGraphEnrichSemantic(parts.Skip(2).ToList()); break; }
                    WriteError("?Uso: GRAPH ENRICH SEMANTIC <wikiDir> [-g dir] [-n nome] [--max N]"); break;
                case "EXPORT":
                    if (parts.Count > 1 && parts[1].ToUpperInvariant() == "OBSIDIAN")
                    { RunGraphExportObsidian(parts.Skip(2).ToList()); break; }
                    WriteError("?Uso: GRAPH EXPORT OBSIDIAN [-g dir] [-n nome] [--out dir]"); break;
                case "STATUS": RunGraphStatus(parts.Skip(1).ToList()); break;
                case "RDD":
                    if (parts.Count < 2) { WriteError("?Uso: GRAPH RDD MEMORY|JSON|SQLITE [ISAM|DBF riservati: richiedono Commercial]"); break; }
                    RunGraphRdd(parts[1]); break;
                case "HELP": PrintGraphHelp(); break;
                default: WriteError("?Comando GRAPH sconosciuto: BUILD|UPDATE|VIEW|WATCH|UNWATCH|EXPORT OBSIDIAN|ENRICH SEMANTIC|RDD|STATUS (usa GRAPH HELP)"); break;
            }
        }
        catch (OperationCanceledException) { Out.WriteLine("[STOP] interrotto. Riprendi con GRAPH BUILD ... --continue"); }
        catch (Exception ex) { WriteError($"?{ex.Message}"); }
    }

    // Help inline dei comandi REPL RAG/GRAPH (gli help dei comandi REPL sono nel codice, non nei
    // JSON di REPL/Helps/ — quelli sono per le funzioni BASIC qualificate).
    private static void PrintRagHelp()
    {
        Out.WriteLine("RAG — costruzione/query di un indice vettoriale da un wiki");
        Out.WriteLine("  RAG BUILD <wikiDir> [-r dir] [-n db] [-A alias] [-C coll] [-c N] [-o N] [-D] [-f] [-p]");
        Out.WriteLine("  RAG UPDATE <wikiDir> [opzioni]   (alias: RAG REFRESH = UPDATE con -p)");
        Out.WriteLine("  RAG STATUS [-r dir] [-n db]");
        Out.WriteLine("  RAG QUERY \"<query>\" [-r dir] [-n db] [-A alias] [-C coll] [-t N]");
        Out.WriteLine("  RAG HELP");
        Out.WriteLine("  -r/--rag        cartella RAG (default: AiConfig.ragDefaultDir, <root>/rag/)");
        Out.WriteLine("  -n/--name       nome db (default: nome cartella wiki; status/query: l'unico *.db/*.state.json nella dir)");
        Out.WriteLine("  -A/--alias      alias runtime (default: basename del db — unico per db, così più RAG");
        Out.WriteLine("                  coesistono nella stessa sessione senza collidere sull'alias fisso)");
        Out.WriteLine("  -C/--collection nome collection RAG (default: AiConfig.ragCollection, 'wiki')");
        Out.WriteLine("  -c/--chunk      chunking MaxChars (default 2048, da config ragChunkMaxChars)");
        Out.WriteLine("  -o/--overlap    chunking Overlap (default 256, da config ragChunkOverlap)");
        Out.WriteLine("  -t/--topk       top-k della query (default 5)");
        Out.WriteLine("  -D/--dry-run    stima file/chunk/tempo senza indicizzare");
        Out.WriteLine("  -f/--force      ricostruisce da zero anche se db/stato esistono");
        Out.WriteLine("  -p/--prune      rimuove i file spariti dal wiki (update/refresh)");
        Out.WriteLine("  Richiede un provider embedding reale (llamacpp/mlx/ollama) — rifiuta il fixture.");
    }

    private static void PrintGraphHelp()
    {
        Out.WriteLine("GRAPH — costruzione del grafo Obsidian di un wiki (nodi=.md, edge=wikilink)");
        Out.WriteLine("  GRAPH BUILD <wikiDir> [-g dir] [-n nome] [-d D] [-i N] [-D] [-f] [-p] [-L] [-H]");
        Out.WriteLine("  GRAPH UPDATE <wikiDir> [opzioni]   (alias: GRAPH REFRESH = UPDATE con -p)");
        Out.WriteLine("  GRAPH STATUS [-g dir] [-n nome]");
        Out.WriteLine("  GRAPH RDD MEMORY|JSON|SQLITE          (backend storage dei GRAPH BUILD legacy)");
        Out.WriteLine("  GRAPH VIEW [-g dir] [-n nome] [--no-open]   (apre graph.html nel browser)");
        Out.WriteLine("  GRAPH WATCH <wikiDir> [-g dir] [-n nome]    (auto-rebuild a ogni modifica .md)");
        Out.WriteLine("  GRAPH UNWATCH <wikiDir>                      (ferma il watchdog)");
        Out.WriteLine("  GRAPH EXPORT OBSIDIAN [-g dir] [-n nome] [--out dir]   (vault Obsidian navigabile)");
        Out.WriteLine("  GRAPH ENRICH SEMANTIC <wikiDir> [-g dir] [-n nome] [--max N]   (LLM deduce relazioni implicite)");
        Out.WriteLine("  GRAPH HELP");
        Out.WriteLine("  -g/--graphs              cartella graphs (default: <root>/graphs/)");
        Out.WriteLine("  -n/--name                nome (default: nome cartella wiki)");
        Out.WriteLine("  -d/--pagerank-damping    damping factor (default 0.85, config graphPageRankDamping)");
        Out.WriteLine("  -i/--pagerank-iterations iterazioni PageRank (default 100, config graphPageRankIterations)");
        Out.WriteLine("  -D/--dry-run             stima senza generare gli export");
        Out.WriteLine("  -f/--force               ricostruisce da zero");
        Out.WriteLine("  -p/--prune               rimuove i nodi dei file spariti (update/refresh)");
        Out.WriteLine("  -L/--louvain             community detection Louvain (modularity) invece del label propagation");
        Out.WriteLine("  -H/--no-html             salta graph.html (viewer) — per wiki molto grandi");
        Out.WriteLine("  Output: <name>.{graphml,mermaid,json,stats.json,html} in graphsDir. GRAPH BUILD genera");
        Out.WriteLine("  anche graph.html (viewer interattivo autonomo: community/degree, filtri, ricerca); usa");
    }

    private async Task RunRagBuildAsync(List<string> args, bool isUpdate, CancellationToken ct)
    {
        var wikiDir = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (wikiDir is null) { WriteError("?Serve il path del wiki: RAG BUILD <wikiDir>"); return; }
        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var dbPath = Path.Combine(ragDir, name + ".db");
        var chunk = int.TryParse(RagGraphFlags.Value(args, "--chunk", "-c"), out var c) && c > 0 ? c : (int?)null;
        var overlap = int.TryParse(RagGraphFlags.Value(args, "--overlap", "-o"), out var o) && o >= 0 ? o : (int?)null;
        var collection = RagGraphFlags.Value(args, "--collection", "-C") ?? Basic.AI.AiConfig.RagCollection;
        var dryRun = RagGraphFlags.Has(args, "--dry-run", "-D");
        var force = RagGraphFlags.Has(args, "--force", "-f");
        var prune = RagGraphFlags.Has(args, "--prune", "-p") || isUpdate;
        // Alias runtime: default = basename del db (unico per db, così più RAG coesistono nello
        // stesso DataSession senza collidere sull'alias fisso "RagDb"); override con --alias/-A.
        var alias = RagGraphFlags.Value(args, "--alias", "-A") ?? RagDbSession.DefaultAlias(dbPath);

        // 1. popola il resolver dal provider embedding attivo di ProviderServer (REPL/CLI non
        //    passano da IlRuntime, dove per i programmi BASIC questo avviene) e valida che sia reale.
        RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
        RagIndexService.ValidateEmbeddingProvider();

        // 2. apre il db (WAL) e registra l'alias
        RagDbSession.Open(dbPath, alias);
        RagDbSession.EnableWal(dbPath);

        var chunking = new ChunkingOptions(chunk ?? Basic.AI.AiConfig.RagChunkOptions().MaxChars,
            overlap ?? Basic.AI.AiConfig.RagChunkOptions().OverlapChars);

        var opts = new RagBuildOptions(wikiDir, alias, dbPath, collection, chunking, dryRun, force, prune,
            BinaryTextExtractor: path => Basic.ConversionLib.DocumentConverter.Convert(path));
        Out.WriteLine($"{(dryRun ? "DRY-RUN" : isUpdate ? "UPDATE" : "BUILD")} RAG: {wikiDir} → {dbPath} (alias '{alias}', chunk {chunking.MaxChars}/{chunking.OverlapChars})");
        var bar = new ConsoleProgressBar();
        var result = await Task.Run(() => RagIndexService.Run(opts,
            (processed, total, current, indexed, skipped) => bar.Report(new ModelDownloader.DownloadProgress(
                processed, total, total > 0 ? processed * 100.0 / total : 0, current)),
            Console.WriteLine, ct), ct).ConfigureAwait(false);
        bar.Finish();
        Out.WriteLine($"[OK] indicizzati {result.Indexed}, aggiornati {result.Updated}, skipped {result.Skipped}, rimossi {result.Removed} — {result.Elapsed.TotalSeconds:F1}s — stato {result.StatePath}");
        Out.WriteLine($"Verifica: RAG QUERY \"...\" --rag {ragDir} --name {name}");
    }

    private void RunRagStatus(List<string> args)
    {
        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleArtifactName(ragDir, "*.db");
        if (name is null) { WriteError($"?db non specificato: usa --name (nessun *.db univoco in {ragDir})"); return; }
        var dbPath = Path.Combine(ragDir, name + ".db");
        var statePath = Path.ChangeExtension(dbPath, ".state.json");
        // Mostra il provider attivo SENZA istanziarlo (GetActiveName non carica il modello).
        try { Basic.Providers.ProviderConfig.Load(); } catch { }
        var activeName = Basic.Providers.ProviderServer.Instance.GetActiveName("embedding");
        Out.WriteLine($"RAG STATUS — {ragDir}");
        Out.WriteLine($"  db: {(File.Exists(dbPath) ? dbPath + " (presente)" : dbPath + " (assente)")}");
        Out.WriteLine($"  provider embedding attivo: {activeName ?? "(nessuno)"}" + (activeName?.StartsWith("fixture", StringComparison.OrdinalIgnoreCase) == true ? " — NON USARE per RAG" : ""));
        if (File.Exists(statePath))
        {
            var st = BuildState.Load(statePath);
            Out.WriteLine($"  stato: {st.ProcessedFiles}/{st.TotalFiles} file, ultimo aggiornamento {st.UpdatedAt:u}");
            Out.WriteLine($"  corrente: {st.CurrentFile}");
        }
        else Out.WriteLine("  stato: nessun build registrato");
    }

    private async Task RunRagQueryAsync(List<string> args, CancellationToken ct)
    {
        var query = args.Count > 0 && !args[0].StartsWith("--") ? string.Join(' ', args.TakeWhile(a => !a.StartsWith("--"))) : null;
        if (string.IsNullOrWhiteSpace(query)) { WriteError("?Serve la query: RAG QUERY \"testo\" [--topk N]"); return; }
        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleArtifactName(ragDir, "*.db");
        if (name is null) { WriteError($"?db non specificato: usa --name (nessun *.db univoco in {ragDir})"); return; }
        var topk = int.TryParse(RagGraphFlags.Value(args, "--topk", "-t"), out var t) && t > 0 ? t : 5;
        var collection = RagGraphFlags.Value(args, "--collection", "-C") ?? Basic.AI.AiConfig.RagCollection;
        var dbPath = Path.Combine(ragDir, name + ".db");
        if (!File.Exists(dbPath)) { WriteError($"?db non trovato: {dbPath}"); return; }
        var alias = RagGraphFlags.Value(args, "--alias", "-A") ?? RagDbSession.DefaultAlias(dbPath);
        RagDbSession.Open(dbPath, alias);
        // La QUERY embed usa il provider REALE (come il build): senza questo
        // EmbeddingProviderResolver.Default è null → la query NON degrada più sul
        // FixtureEmbeddingProvider (P0a, decisione utente 2026-09-12) — fallisce esplicito.
        RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
        RagIndexService.ValidateEmbeddingProvider();
        await Task.Run(() => Out.WriteLine(AiVectorRuntime.SearchJson(alias, collection, query, topk)), ct).ConfigureAwait(false);
    }

    private async Task RunGraphBuildAsync(List<string> args, bool isUpdate, CancellationToken ct)
    {
        var wikiDir = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (wikiDir is null) { WriteError("?Serve il path del wiki: GRAPH BUILD <wikiDir>"); return; }
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var dryRun = RagGraphFlags.Has(args, "--dry-run", "-D");
        var force = RagGraphFlags.Has(args, "--force", "-f");
        var prune = RagGraphFlags.Has(args, "--prune", "-p") || isUpdate;

        var damping = double.TryParse(RagGraphFlags.Value(args, "--pagerank-damping", "-d"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var d) && d > 0 && d <= 1 ? d : Basic.AI.AiConfig.GraphPageRankDamping;
        var iterations = int.TryParse(RagGraphFlags.Value(args, "--pagerank-iterations", "-i"), out var it) && it > 0 ? it : Basic.AI.AiConfig.GraphPageRankIterations;
        var withHtml = !RagGraphFlags.Has(args, "--no-html", "-H");
        var louvain = RagGraphFlags.Has(args, "--louvain", "-L");

        var opts = new GraphBuildOptions(wikiDir, graphsDir, name, dryRun, force, prune,
            PageRankDamping: damping, PageRankIterations: iterations, WithHtml: withHtml, UseLouvain: louvain);
        Out.WriteLine($"{(dryRun ? "DRY-RUN" : isUpdate ? "UPDATE" : "BUILD")} GRAPH: {wikiDir} → {graphsDir}/{name} (html {(withHtml ? "sì" : "no")}" + (louvain ? ", community Louvain" : "") + ")");
        var bar = new ConsoleProgressBar();
        var result = await Task.Run(() => GraphBuildService.Run(opts,
            (processed, total, current) => bar.Report(new ModelDownloader.DownloadProgress(
                processed, total, total > 0 ? processed * 100.0 / total : 0, current)),
            Console.WriteLine, ct), ct).ConfigureAwait(false);
        bar.Finish();
        Out.WriteLine($"[OK] nodi {result.Nodes}, edge {result.Edges}, dangling {result.Dangling} — {result.Elapsed.TotalSeconds:F1}s — stats {result.StatsPath}");
    }

    /// <summary>GRAPH EXPORT OBSIDIAN [-g dir] [-n nome] [--out dir]: genera un vault Obsidian
    /// navigabile dal .json esistente (index.md + una nota per community con wikilink).</summary>
    private void RunGraphExportObsidian(List<string> args)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleGraphName(graphsDir);
        if (name is null) { WriteError($"?graph non specificato: usa --name (nessun *.state.json univoco in {graphsDir})"); return; }
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { WriteError($"?json non trovato: {jsonPath} (esegui prima GRAPH BUILD)"); return; }
        var outDir = RagGraphFlags.Value(args, "--out", "-o") ?? Path.Combine(graphsDir, name + "_obsidian");
        var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(File.ReadAllText(jsonPath));
        Basic.AI.Graph.GraphExporter.WriteObsidianVault(graph, name, outDir);
        Out.WriteLine($"[OK] vault Obsidian: {outDir} (index.md + communities/ — apri la cartella in Obsidian)");
    }

    /// <summary>GRAPH WATCH &lt;wikiDir&gt; [-g dir] [-n nome] / GRAPH UNWATCH &lt;wikiDir&gt;:
    /// watchdog che rigenera il grafo al cambio dei file .md (debounce ~1s).</summary>
    private void RunGraphWatch(List<string> args, bool start)
    {
        var wikiDir = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (start)
        {
            if (wikiDir is null) { WriteError("?Serve il path del wiki: GRAPH WATCH <wikiDir>"); return; }
            var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
            var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
            try
            {
                Basic.AI.Graph.GraphWatchService.Start(wikiDir, graphsDir, name);
                Out.WriteLine($"[WATCH] osservo {Path.GetFullPath(wikiDir)} → rebuild automatico su ogni modifica .md (debounce ~1s). GRAPH UNWATCH per fermare.");
                Out.WriteLine($"[WATCH] cartelle osservate: {Basic.AI.Graph.GraphWatchService.WatchedCount}");
            }
            catch (Exception ex) { WriteError($"?{ex.Message}"); }
        }
        else
        {
            if (wikiDir is null) { WriteError("?Serve il path del wiki: GRAPH UNWATCH <wikiDir>"); return; }
            var r = Basic.AI.Graph.GraphWatchService.Stop(wikiDir);
            Out.WriteLine(r == 1 ? $"[WATCH] fermato: {wikiDir}" : $"[WATCH] nessun watcher attivo su {wikiDir}");
        }
    }

    /// <summary>GRAPH ENRICH SEMANTIC &lt;wikiDir&gt; [-g dir] [-n nome] [--max N] [--batch N]:
    /// semantic extraction LLM — un LLM legge le note e deduce edge semantici impliciti
    /// (Type="semantic", INFERRED) tra note esistenti. Richiede un provider %CHAT attivo.
    /// --max N limita il numero di note analizzate (per wiki grandi); --batch N la finestra
    /// per invocazione LLM. Riscrive <nome>.json/stats/html con i nuovi edge.</summary>
    private void RunGraphEnrichSemantic(List<string> args)
    {
        var wikiDir = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (wikiDir is null) { WriteError("?Uso: GRAPH ENRICH SEMANTIC <wikiDir> [-g dir] [-n nome]"); return; }
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { WriteError($"?json non trovato: {jsonPath} (esegui prima GRAPH BUILD)"); return; }
        var maxNotes = int.TryParse(RagGraphFlags.Value(args, "--max", "-m"), out var m) && m > 0 ? m : int.MaxValue;
        var batch = int.TryParse(RagGraphFlags.Value(args, "--batch", "-b"), out var b) && b > 0 ? b : 6;

        // provider %CHAT attivo (mai stub: senza LLM → errore esplicito)
        try { Basic.Providers.ProviderConfig.Load(); } catch { }
        Basic.AI.Rag.ILlmCompleter? llm;
        try { llm = Basic.Providers.ProviderServer.Instance.GetActiveCompletion("completion"); }
        catch (Exception ex) { WriteError($"?{ex.Message} — attiva un provider chat (AI SET ACTIVE %CHAT, nome)"); return; }

        // carica il grafo esistente e rilegge i file .md per il body
        var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(File.ReadAllText(jsonPath));
        var files = Basic.AI.Graph.GraphStore.EnumerateMarkdown(wikiDir)
            .Where(p => graph.Nodes.ContainsKey(p)).Take(maxNotes).ToList();
        if (files.Count == 0) { WriteError("?nessuna nota nel grafo da analizzare"); return; }
        var notes = new List<Basic.AI.Graph.GraphSemanticNote>();
        foreach (var rel in files)
        {
            var full = Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar));
            string text;
            try { text = File.ReadAllText(full); } catch { continue; }
            var body = Basic.AI.Graph.FrontmatterParser.Parse(text).Body;
            notes.Add(new Basic.AI.Graph.GraphSemanticNote(rel,
                graph.Nodes[rel].Frontmatter.TryGetValue("title", out var t) && t.Count > 0 ? t[0] : rel,
                string.IsNullOrWhiteSpace(body) ? text : body));
        }
        Out.WriteLine($"[ENRICH SEMANTIC] {wikiDir} → analizzo {notes.Count} note con LLM ({llm.ModelName}), batch {batch} …");
        int added;
        try { added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(graph, notes, llm, batch); }
        catch (Exception ex) { WriteError($"?{ex.Message}"); return; }
        Out.WriteLine(added < 0
            ? "[ENRICH SEMANTIC] nessun edge dedotto (risposta LLM non valida)"
            : $"[ENRICH SEMANTIC] aggiunti {added} edge semantici — rigenero gli export");
        if (added <= 0) return;
        var export = Basic.AI.Graph.GraphExporter.Export(graph, name, Basic.AI.AiConfig.GraphPageRankDamping, Basic.AI.AiConfig.GraphPageRankIterations, withHtml: true);
        var basePath = Path.Combine(graphsDir, name);
        File.WriteAllText(basePath + ".json", export.Json);
        File.WriteAllText(basePath + ".stats.json", export.StatsJson);
        if (export.Html is not null) File.WriteAllText(basePath + ".html", export.Html);
        if (export.Report is not null) File.WriteAllText(basePath + ".report.md", export.Report);
        Out.WriteLine($"[ENRICH SEMANTIC] export aggiornato: {basePath} (.json / .stats.json / .html)");
    }

    private void RunGraphStatus(List<string> args)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleGraphName(graphsDir);
        if (name is null) { WriteError($"?graph non specificato: usa --name (nessun *.state.json univoco in {graphsDir})"); return; }
        var statePath = Path.Combine(graphsDir, name + ".state.json");
        var statsPath = Path.Combine(graphsDir, name + ".stats.json");
        Out.WriteLine($"GRAPH STATUS — {graphsDir}");
        Out.WriteLine($"  stats: {(File.Exists(statsPath) ? statsPath + " (presente)" : statsPath + " (assente)")}");
        if (File.Exists(statePath))
        {
            var st = BuildState.Load(statePath);
            Out.WriteLine($"  stato: {st.ProcessedFiles}/{st.TotalFiles} file, ultimo aggiornamento {st.UpdatedAt:u}");
        }
        else Out.WriteLine("  stato: nessun build registrato");
    }

    /// <summary>GRAPH RDD &lt;backend&gt; — seleziona il backend di storage RDD per i GRAPH BUILD/UPDATE
    /// legacy (GraphManager 8 campi): MEMORY | JSON | SQLITE. ISAM/DBF oggi non sono supportati
    /// da questo percorso (non e' una questione di edizione: la matrice Edition non esiste piu').</summary>
    private void RunGraphRdd(string backend)
    {
        var b = Basic.AI.Knowledge.GraphManager.ParseBackend(backend);
        if (b is Basic.AI.Knowledge.GraphRddBackend.Isam or Basic.AI.Knowledge.GraphRddBackend.Dbf)
        {
            WriteError($"?GRAPH RDD {b} non e' supportato da questo percorso — usa MEMORY, JSON o SQLITE");
            return;
        }
        if (b is not (Basic.AI.Knowledge.GraphRddBackend.Memory
            or Basic.AI.Knowledge.GraphRddBackend.Json
            or Basic.AI.Knowledge.GraphRddBackend.Sqlite))
        {
            WriteError($"?GRAPH RDD backend non riconosciuto: {backend} (usa MEMORY, JSON o SQLITE)");
            return;
        }
        Basic.AI.Knowledge.GraphManager.DefaultBackend = b;
        Out.WriteLine($"[OK] GRAPH RDD backend = {b} (prossimi GRAPH BUILD/UPDATE legacy lo useranno)");
    }

    /// <summary>GRAPH VIEW [name]: rigenera graph.html dal .json esistente (se manca l'html o è
    /// vecchio) e lo apre nel browser. Senza --name usa l'unico *.json nella dir graphs.</summary>
    private async Task RunGraphViewAsync(List<string> args, CancellationToken ct)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n")
            ?? RagGraphFlags.SingleArtifactName(graphsDir, "*.state.json");
        if (name is null) { WriteError($"?graph non specificato: usa --name (nessun *.state.json univoco in {graphsDir})"); return; }
        name = name.EndsWith(".state", StringComparison.OrdinalIgnoreCase) ? name[..^".state".Length] : name;
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { WriteError($"?json non trovato: {jsonPath} (esegui prima GRAPH BUILD)"); return; }
        var htmlPath = Path.Combine(graphsDir, name + ".html");

        var doOpen = !RagGraphFlags.Has(args, "--no-open", "-n");
        // regenera l'html se manca o se il json è più nuovo dell'html
        bool regen = !File.Exists(htmlPath) || File.GetLastWriteTimeUtc(jsonPath) > File.GetLastWriteTimeUtc(htmlPath).AddSeconds(1);
        if (regen)
        {
            Out.WriteLine($"VIEW: {jsonPath} → {htmlPath} …");
            var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(await File.ReadAllTextAsync(jsonPath, ct));
            await File.WriteAllTextAsync(htmlPath, Basic.AI.Graph.GraphExporter.ToHtml(graph, name), ct);
            Out.WriteLine($"[OK] {htmlPath}");
        }
        else Out.WriteLine($"VIEW: {htmlPath} (già aggiornato)");

        if (doOpen && File.Exists(htmlPath))
        {
            try
            {
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                {
                    FileName = htmlPath,
                    UseShellExecute = true,
                });
            }
            catch (Exception ex) { WriteError($"?apertura browser fallita: {ex.Message} — apri manualmente {htmlPath}"); }
        }
    }

    // ── BRAIN (Second Brain): USE/DETACH/LIST/INFO/INSTALL/EXPORT/SIGN ─────
    // La costruzione dei cervelli resta a RAG BUILD/GRAPH BUILD. Qui si collegano
    // (USE), si importano da zip (INSTALL), si esportano (EXPORT), si firmano (SIGN).
    /// <summary>EVAL [RUN] [path] — Eval Harness (M6): esegue le suite metriche
    /// (llm.tokps, rag.ndcg, team.acc, credit.blocked) e scrive eval_report.json se path dato.
    /// Fixture deterministico se nessun provider LLM attivo (mai rete implicita).</summary>
    private Task HandleEvalAsync(string args, CancellationToken __)
    {
        var parts = ParseArgs(args);
        var fixture = Basic.AI.Rag.LlmCompleterResolver.Default is null;
        string? path = null;
        var i = parts.Count > 0 && parts[0].Equals("RUN", StringComparison.OrdinalIgnoreCase) ? 1 : 0;
        if (i < parts.Count) path = parts[i];
        Out.WriteLine($"EVAL RUN (fixture={fixture})...");
        try
        {
            var json = Basic.AI.Eval.EvalHarness.RunAll(path, fixture: fixture);
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            var root = doc.RootElement;
            Out.WriteLine($"total: {root.GetProperty("total").GetProperty("passed").GetInt32()}/{root.GetProperty("total").GetProperty("cases").GetInt32()} pass");
            foreach (var suite in root.GetProperty("suites").EnumerateArray())
            {
                var name = suite.GetProperty("Name").GetString();
                var metrics = suite.GetProperty("Metrics").EnumerateArray()
                    .Select(m => $"{m.GetProperty("Key").GetString()}={m.GetProperty("Value")}")
                    .ToList();
                Out.WriteLine($"  {name}: {string.Join("  ", metrics)}");
            }
            if (path != null) Out.WriteLine($"report → {path}");
        }
        catch (Exception ex)
        {
             WriteError($"?EVAL fallito: {ex.Message}");
         }
         return Task.CompletedTask;
     }

    /// <summary>NOTEBOOK — server HTTP del kernel notebook (Fase 1 Interactive-Notebook-Plan).
    /// Comandi REPL-side (host tool, NON comandi BASIC): NOTEBOOK START [port] | STOP | STATUS.
    /// Il server espone POST /execute {cellId, source} + /vars + /reset + GET /health su
    /// 127.0.0.1 (vedi NotebookServer). Bind localhost only, come McpHttpListenerHost.</summary>
    private Task HandleNotebookAsync(string args, CancellationToken __)
    {
        var parts = ParseArgs(args);
        var cmd = parts.Count > 0 ? parts[0].ToUpperInvariant() : "STATUS";
        try
        {
            switch (cmd)
            {
                case "START":
                {
                    if (_notebookServer != null) { Out.WriteLine($"NOTEBOOK: già attivo su porta {_notebookServer.Port}"); return Task.CompletedTask; }
                    var port = parts.Count > 1 && int.TryParse(parts[1], out var p) ? p : 8030;
                    _notebookServer = new GMBasic.Notebook.NotebookServer(port);
                    _notebookServer.Start();
                    Out.WriteLine($"NOTEBOOK: server attivo su http://127.0.0.1:{_notebookServer.Port}/ (POST /execute, /vars, /reset; GET /health)");
                    break;
                }
                case "STOP":
                {
                    if (_notebookServer == null) { Out.WriteLine("NOTEBOOK: non attivo"); return Task.CompletedTask; }
                    var s = _notebookServer; _notebookServer = null;
                    s.Dispose();
                    Out.WriteLine("NOTEBOOK: server fermato");
                    break;
                }
                case "STATUS":
                    Out.WriteLine(_notebookServer != null
                        ? $"NOTEBOOK: attivo su http://127.0.0.1:{_notebookServer.Port}/"
                        : "NOTEBOOK: non attivo (NOTEBOOK START [port])");
                    break;
                case "SAVE":
                {
                    if (parts.Count < 2) { WriteError("?NOTEBOOK SAVE <path.gmnb>"); return Task.CompletedTask; }
                    var kernel = GetReplNotebookKernel();
                    var path = ResolveNotebookPath(parts[1]);
                    kernel.Save(path);
                    Out.WriteLine($"NOTEBOOK: salvato {path} ({kernel.Cells().Count} celle)");
                    break;
                }
                case "LOAD":
                {
                    if (parts.Count < 2) { WriteError("?NOTEBOOK LOAD <path.gmnb>"); return Task.CompletedTask; }
                    var kernel = GetReplNotebookKernel();
                    var path = ResolveNotebookPath(parts[1]);
                    var results = kernel.Load(path);
                    var failed = results.Count(r => !r.Result.Ok);
                    Out.WriteLine($"NOTEBOOK: caricato {path} ({results.Count} celle, {failed} errori)");
                    break;
                }
                case "EXPORT":
                {
                    if (parts.Count < 2) { WriteError("?NOTEBOOK EXPORT <path.bas|html|pdf>"); return Task.CompletedTask; }
                    var kernel = GetReplNotebookKernel();
                    var path = ResolveNotebookPath(parts[1]);
                    if (path.EndsWith(".html", StringComparison.OrdinalIgnoreCase))
                    {
                        kernel.ExportHtmlFile(path);
                        Out.WriteLine($"NOTEBOOK: esportato {path} (HTML con output celle — apri e stampa per il PDF)");
                    }
                    else if (path.EndsWith(".pdf", StringComparison.OrdinalIgnoreCase))
                    {
                        // Export PDF VERO via Chromium headless (NotebookKernel.ExportPdfFile, opzione D)
                        kernel.ExportPdfFile(path);
                        Out.WriteLine($"NOTEBOOK: esportato {path} (PDF reale via Chromium)");
                    }
                    else
                    {
                        kernel.ExportBasic(path);
                        Out.WriteLine($"NOTEBOOK: esportato {path} ({kernel.Cells().Count} celle; magic → REM)");
                    }
                    break;
                }
                case "AUTOSAVE":
                {
                    // NOTEBOOK AUTOSAVE <path.gmnb> | NOTEBOOK AUTOSAVE OFF
                    if (parts.Count < 2) { WriteError("?NOTEBOOK AUTOSAVE <path.gmnb> | OFF"); return Task.CompletedTask; }
                    var kernel = GetReplNotebookKernel();
                    if (parts[1].Equals("OFF", StringComparison.OrdinalIgnoreCase))
                    {
                        kernel.AutosavePath = null;
                        if (_notebookServer != null) _notebookServer.AutosavePath = null;
                        Out.WriteLine("NOTEBOOK: autosave disattivato");
                        break;
                    }
                    var path = ResolveNotebookPath(parts[1]);
                    kernel.AutosavePath = path;
                    if (_notebookServer != null) _notebookServer.AutosavePath = path;
                    Out.WriteLine($"NOTEBOOK: autosave → {path}");
                    break;
                }
                default:
                    WriteError("?Comando NOTEBOOK sconosciuto: START [port] | STOP | STATUS");
                    break;
            }
        }
        catch (Exception ex)
        {
            WriteError($"?NOTEBOOK: {ex.Message}");
        }
        return Task.CompletedTask;
    }

    /// <summary>Kernel del notebook per LOAD/SAVE/uso diretto nel REPL (distinto dal kernel del
    /// server, che vive in NotebookServer). Creato lazy al primo uso.</summary>
    private GMBasic.Notebook.NotebookKernel GetReplNotebookKernel()
        => _notebookKernel ??= new GMBasic.Notebook.NotebookKernel();

    /// <summary>Risolve un path del notebook: relativo → rispetto alla CWD del progetto REPL.
    /// NB: il file resta nella working directory del processo (mai in root del repo).</summary>
    private static string ResolveNotebookPath(string p)
        => Path.IsPathRooted(p) ? p : Path.Combine(Directory.GetCurrentDirectory(), p);


    /// <summary>FIXTURES — le fixture del COMPILATORE (dati di input di sample e test).
    /// Vivono SEMPRE e SOLO in `shared/samples/Fixtures/<nome>/`; il manifest è in
    /// `shared/settings/fixtures-manifest.json`. Il download usa `gmb_fetch` (resume + sha256).
    /// La REPL deve saperle scaricare come la CLI e il compilatore.</summary>
    private Task HandleFixturesAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        var cmd = parts.Count > 0 ? parts[0].ToUpperInvariant() : "LIST";
        try
        {
            switch (cmd)
            {
                case "LIST":
                case "STATUS":
                {
                    var status = CommonLib.NativeFixtures.Status();
                    if (string.IsNullOrEmpty(status))
                    {
                        WriteError("?FIXTURES: manifest non trovato (shared/settings/fixtures-manifest.json)");
                        break;
                    }
                    foreach (var entry in status.Split(';', StringSplitOptions.RemoveEmptyEntries))
                    {
                        var colon = entry.LastIndexOf(':');
                        var name = colon > 0 ? entry[..colon] : entry;
                        var state = colon > 0 ? entry[(colon + 1)..] : "?";
                        Out.WriteLine($"  {name,-16} {state}");
                    }
                    break;
                }
                case "ENSURE":
                {
                    var (n, err) = CommonLib.NativeFixtures.EnsureAllMissingWithError();
                    if (n < 0) WriteError($"?FIXTURES ENSURE: {err}");
                    else Out.WriteLine(n == 0
                        ? "FIXTURES: nessuna fixture mancante"
                        : $"FIXTURES: {n} fixture scaricate in shared/samples/Fixtures");
                    break;
                }
                case "PULL":
                {
                    var names = parts.Skip(1).Where(p => !p.StartsWith("--")).ToList();
                    if (names.Count == 0) { WriteError("?Uso: FIXTURES PULL <nome…> (vedi FIXTURES LIST)"); break; }
                    var failed = 0;
                    foreach (var nm in names)
                    {
                        var (ok, dir, err) = CommonLib.NativeFixtures.Pull(nm);
                        if (ok) Out.WriteLine($"  {nm} -> {dir}");
                        else { failed++; WriteError($"  {nm}: {err}"); }
                    }
                    if (failed == 0) Out.WriteLine("FIXTURES: fatto");
                    break;
                }
                case "HELP":
                default:
                    Out.WriteLine("FIXTURES — fixture del compilatore (samples/Fixtures/)");
                    Out.WriteLine("  FIXTURES LIST            stato (presente/mancante) delle fixture del manifest");
                    Out.WriteLine("  FIXTURES ENSURE          scarica SOLO quelle mancanti (sempre in shared/samples/Fixtures)");
                    Out.WriteLine("  FIXTURES PULL <nome…>    scarica esplicitamente (es. FIXTURES PULL darkpool)");
                    Out.WriteLine("  manifest: shared/settings/fixtures-manifest.json — download con resume + sha256");
                    break;
            }
        }
        catch (Exception ex) { WriteError($"?FIXTURES: {ex.Message}"); }
        return Task.CompletedTask;
    }

    private async Task HandleBrainAsync(string args, CancellationToken ct)
    {
        var parts = ParseArgs(args);
        if (parts.Count == 0) { PrintBrainHelp(); return; }
        var cmd = parts[0].ToUpperInvariant();
        try
        {
            switch (cmd)
            {
                case "USE": RunBrainUse(parts.Skip(1).ToList()); break;
                case "DETACH": RunBrainDetach(parts.Skip(1).ToList()); break;
                case "LIST": RunBrainList(); break;
                case "INFO": RunBrainInfo(parts.Skip(1).ToList()); break;
                case "SCOPE": RunBrainScope(parts.Skip(1).ToList()); break;
                case "STATS": RunBrainTool("stats", parts.Skip(1).ToList()); break;
                case "VERIFY": RunBrainTool("verify", parts.Skip(1).ToList()); break;
                case "CREATE": RunBrainTool("create", parts.Skip(1).ToList()); break;
                case "OPTIMIZE": RunBrainTool("optimize", parts.Skip(1).ToList()); break;
                case "BACKUP": RunBrainTool("backup", parts.Skip(1).ToList()); break;
                case "RENAME": RunBrainTool("rename", parts.Skip(1).ToList()); break;
                case "DEDUPE": RunBrainTool("dedupe", parts.Skip(1).ToList()); break;
                case "EXTRACT": RunBrainTool("extract", parts.Skip(1).ToList()); break;
                case "MERGE": RunBrainTool("merge", parts.Skip(1).ToList()); break;
                case "BUILDVAULT": RunBrainTool("buildvault", parts.Skip(1).ToList()); break;
                case "REGISTER": RunBrainTool("register", parts.Skip(1).ToList()); break;
                case "DIFF": RunBrainTool("diff", parts.Skip(1).ToList()); break;
                case "DOCTOR": RunBrainTool("doctor", parts.Skip(1).ToList()); break;
                case "CONVERT": RunBrainTool("convert", parts.Skip(1).ToList()); break;
                case "RESOLVE": RunBrainResolve(parts.Skip(1).ToList()); break;
                case "INSTALL": await RunBrainInstallAsync(parts.Skip(1).ToList(), ct); break;
                case "EXPORT": RunBrainExport(parts.Skip(1).ToList()); break;
                case "SIGN": RunBrainSign(parts.Skip(1).ToList()); break;
                case "HELP": PrintBrainHelp(); break;
                default: WriteError("?Comando BRAIN sconosciuto: USE|DETACH|LIST|INFO|SCOPE|RESOLVE|STATS|VERIFY|CREATE|REGISTER|BUILDVAULT|OPTIMIZE|BACKUP|RENAME|DEDUPE|EXTRACT|MERGE|CONVERT|DIFF|DOCTOR|INSTALL|EXPORT|SIGN (usa BRAIN HELP)"); break;
            }
        }
        catch (Exception ex)
        {
            WriteError($"?{ex.Message}");
        }
    }

    /// <summary>Argomenti POSIZIONALI di un comando BRAIN, escludendo i flag e i LORO VALORI
    /// (`--out x`, `--on-conflict y`, `--mode both`, …): senza, `BRAIN MERGE a b --out c` passava
    /// anche `c` fra i cervelli da fondere (bug trovato dal demo tools/brain_toolchain_demo.sh).</summary>
    private static List<string> BrainPositionalArgs(List<string> args)
    {
        string[] withValue = ["--out", "--mode", "--source", "--from", "--to", "--on-conflict",
                              "--hops", "--min-edges", "--min-p1", "--topk", "-t", "--path", "--golden"];
        var result = new List<string>();
        for (var i = 0; i < args.Count; i++)
        {
            var a = args[i];
            if (a.StartsWith("--") || a.StartsWith("-"))
            {
                if (withValue.Contains(a, StringComparer.OrdinalIgnoreCase)) i++;   // salta il valore
                continue;
            }
            result.Add(a);
        }
        return result;
    }

    /// <summary>Toolchain dei cervelli (V1): stessa implementazione della CLI (`gmb brain …`).</summary>
    private void RunBrainTool(string op, List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        var json = args.Contains("--json", StringComparer.OrdinalIgnoreCase);
        var dryRun = args.Contains("--dry-run", StringComparer.OrdinalIgnoreCase);
        Basic.AI.Knowledge.ToolResult r = op switch
        {
            "stats" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Stats(name, json),
            "verify" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : RagGraphFlags.Value(args, "--golden", null) is { } golden
                        ? Basic.AI.Knowledge.BrainToolchain.VerifyGolden(name, golden,
                            int.TryParse(RagGraphFlags.Value(args, "--topk", "-t"), out var gk) && gk > 0 ? gk : 5,
                            double.TryParse(RagGraphFlags.Value(args, "--min-p1", null), System.Globalization.NumberStyles.Float,
                                System.Globalization.CultureInfo.InvariantCulture, out var mp1) ? mp1 : double.NaN)
                        : Basic.AI.Knowledge.BrainToolchain.Verify(name, json),
            "diff" => name is null || args.Count < 2 || args[1].StartsWith("--")
                     ? Basic.AI.Knowledge.ToolResult.Fail("uso: BRAIN DIFF <a> <b> [--json]")
                     : Basic.AI.Knowledge.BrainToolchain.Diff(name, args[1], json),
            "doctor" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Doctor(name),
            "convert" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: BRAIN CONVERT <name> --to rag|graph [--out <nuovo>|--in-place] [--min-edges N] [--dry-run]")
                     : Basic.AI.Knowledge.BrainToolchain.ConvertMode(name, RagGraphFlags.Value(args, "--to", null) ?? "",
                         RagGraphFlags.Value(args, "--out", null), args.Contains("--in-place", StringComparer.OrdinalIgnoreCase),
                         int.TryParse(RagGraphFlags.Value(args, "--min-edges", null), out var me) ? me : 1, dryRun),
            "create" => Basic.AI.Knowledge.BrainToolchain.Create(
                     name ?? "", RagGraphFlags.Value(args, "--from", null) ?? "",
                     RagGraphFlags.Value(args, "--mode", null) ?? "both",
                     args.Contains("--register", StringComparer.OrdinalIgnoreCase),
                     args.Contains("--activate", StringComparer.OrdinalIgnoreCase),
                     dryRun,
                     path => Basic.ConversionLib.DocumentConverter.Convert(path)),
            "optimize" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Optimize(name, dryRun, args.Contains("--reindex-fts", StringComparer.OrdinalIgnoreCase)),
            "backup" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Backup(name, RagGraphFlags.Value(args, "--out", null)),
            "buildvault" => Basic.AI.Knowledge.BrainToolchain.BuildVault(RagGraphFlags.Value(args, "--out", null)),
            "register" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: BRAIN REGISTER <name> [--mode rag|graph|both] [--source dir] [--activate]")
                     : Basic.AI.Knowledge.BrainToolchain.Register(name, RagGraphFlags.Value(args, "--mode", null),
                         RagGraphFlags.Value(args, "--source", null), args.Contains("--activate", StringComparer.OrdinalIgnoreCase)),
            "dedupe" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Dedupe(name,
                         apply: args.Contains("--apply", StringComparer.OrdinalIgnoreCase), dryRun: dryRun),
            "extract" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: BRAIN EXTRACT <name> --out <nuovo> [--source substr] [--node substr] [--hops N]")
                     : Basic.AI.Knowledge.BrainToolchain.Extract(name, RagGraphFlags.Value(args, "--out", null) ?? "",
                         RagGraphFlags.Value(args, "--source", null), RagGraphFlags.Value(args, "--node", null),
                         int.TryParse(RagGraphFlags.Value(args, "--hops", null), out var hp) && hp > 0 ? hp : 1, dryRun),
            "merge" => Basic.AI.Knowledge.BrainToolchain.Merge(
                         BrainPositionalArgs(args),
                         RagGraphFlags.Value(args, "--out", null) ?? "",
                         RagGraphFlags.Value(args, "--on-conflict", null) ?? "skip", dryRun),
            "rename" => name is null || args.Count < 2 || args[1].StartsWith("--")
                     ? Basic.AI.Knowledge.ToolResult.Fail("uso: BRAIN RENAME <old> <new> [--dry-run]")
                     : Basic.AI.Knowledge.BrainToolchain.Rename(name, args[1], dryRun),
            _ => Basic.AI.Knowledge.ToolResult.Fail($"operazione '{op}' non supportata")
        };
        if (!string.IsNullOrEmpty(r.Text)) Out.WriteLine(r.Text);
        if (!r.Ok) WriteError($"?{r.Error ?? "operazione fallita"}");
    }

    /// <summary>Scope della SESSIONE REPL (livello SESSION della catena): brain dichiarati/alias per QUESTA
    /// sessione, senza toccare il registry globale. Null = nessuna dichiarazione (INHERIT dal PROCESS).</summary>
    private IReadOnlyList<Basic.AI.Knowledge.BrainRef>? _sessionBrains;
    private bool _sessionPrivate;
    private readonly List<string> _sessionWithout = [];

    private Basic.AI.Knowledge.BrainScope CurrentBrainScope()
    {
        var scope = Basic.AI.Knowledge.BrainScope.FromRegistry();
        return _sessionBrains is null && !_sessionPrivate && _sessionWithout.Count == 0
            ? scope
            : scope.Session(_sessionBrains, _sessionPrivate, _sessionWithout.Count > 0 ? _sessionWithout : null);
    }

    private void RunBrainUse(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (name is null) { PrintBrainHelp(); return; }

        // `BRAIN USE <name> [AS <alias>] [--scope session|process]`
        //  - process (default): attiva GLOBALMENTE (registry, come sempre);
        //  - session: dichiara nel livello SESSION della REPL (con alias opzionale), nessuna scrittura
        //    del registry (§0ter: nessuna mutazione globale implicita).
        string? alias = null;
        var asIdx = args.FindIndex(a => a.Equals("AS", StringComparison.OrdinalIgnoreCase));
        if (asIdx >= 0 && asIdx + 1 < args.Count) alias = args[asIdx + 1].Trim('"');
        var scopeArg = RagGraphFlags.Value(args, "--scope", null) ?? "process";
        var sessionScope = scopeArg.Equals("session", StringComparison.OrdinalIgnoreCase);

        if (sessionScope)
        {
            if (!Basic.AI.Knowledge.BrainRegistry.List().Contains(name, StringComparer.OrdinalIgnoreCase))
            {
                WriteError($"?cervello '{name}' non registrato: esegui BRAIN INSTALL o RAG BUILD/GRAPH BUILD");
                return;
            }
            var list = (_sessionBrains ?? []).Where(r => !r.Matches(name)).Append(new Basic.AI.Knowledge.BrainRef(name, alias)).ToList();
            _sessionBrains = list;
            Out.WriteLine($"[OK] brain '{name}'{(alias is null ? "" : $" AS {alias}")} dichiarato nello SCOPE DELLA SESSIONE "
                            + $"(registry globale invariato) — BRAIN RESOLVE {alias ?? name} --explain per la catena");
            return;
        }

        if (!Basic.AI.Knowledge.BrainRegistry.Use(name))
        {
            WriteError($"?cervello '{name}' non disponibile: esegui BRAIN INSTALL o RAG BUILD/GRAPH BUILD");
            return;
        }
        Out.WriteLine($"[OK] brain '{name}' collegato — available: [{string.Join(", ", Basic.AI.Knowledge.BrainRegistry.AvailableModes(name))}]");
    }

    /// <summary>`BRAIN SCOPE [INHERIT|PRIVATE] [--without a,b] [--reset]`: livello SESSION della catena.
    /// INHERIT (default) = i brain attivati restano visibili; PRIVATE = la sessione vede SOLO i brain
    /// dichiarati con `BRAIN USE … --scope session`. `--without` sottrae nomi/alias ereditati.</summary>
    private void RunBrainScope(List<string> args)
    {
        if (args.Contains("--reset", StringComparer.OrdinalIgnoreCase))
        {
            _sessionBrains = null; _sessionPrivate = false; _sessionWithout.Clear();
            Out.WriteLine("[OK] scope di sessione azzerato (INHERIT dal livello PROCESS)");
            return;
        }
        var withoutArg = RagGraphFlags.Value(args, "--without", null);
        if (!string.IsNullOrWhiteSpace(withoutArg))
        {
            _sessionWithout.Clear();
            _sessionWithout.AddRange(withoutArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
        }
        var mode = args.FirstOrDefault(a => !a.StartsWith("--"))?.ToUpperInvariant();
        if (mode is "PRIVATE") _sessionPrivate = true;
        else if (mode is "INHERIT") _sessionPrivate = false;
        else if (mode is not null) { WriteError("?BRAIN SCOPE: attesi INHERIT|PRIVATE [--without a,b] [--reset]"); return; }

        Out.WriteLine($"scope di sessione: {(_sessionPrivate ? "PRIVATE" : "INHERIT")}"
            + (_sessionWithout.Count > 0 ? $" WITHOUT {string.Join(",", _sessionWithout)}" : "")
            + (_sessionBrains is null ? "" : $" — dichiarati: {string.Join(", ", _sessionBrains.Select(r => r.ToString()))}"));
        foreach (var l in CurrentBrainScope().Explain()) Out.WriteLine(l);
    }

    private void RunBrainDetach(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (name is null) { PrintBrainHelp(); return; }
        Out.WriteLine(Basic.AI.Knowledge.BrainRegistry.Detach(name)
            ? $"[OK] brain '{name}' scollegato (i file restano)"
            : $"?brain '{name}' non era collegato");
    }

    private void RunBrainList()
    {
        var names = Basic.AI.Knowledge.BrainRegistry.List();
        if (names.Length == 0) { Out.WriteLine("(nessun cervello registrato)"); return; }
        var used = Basic.AI.Knowledge.BrainRegistry.Used();
        foreach (var n in names)
        {
            var info = Basic.AI.Knowledge.BrainRegistry.Info(n);
            var mark = used.Contains(n, StringComparer.OrdinalIgnoreCase) ? "*" : " ";
            var avail = info is null ? "" : string.Join(",", info.Available);
            Out.WriteLine($"{mark} {n}  [{avail}]");
        }
        Out.WriteLine("* = collegato (BRAINUSED)");
    }

    private void RunBrainInfo(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (name is null) { PrintBrainHelp(); return; }
        var info = Basic.AI.Knowledge.BrainRegistry.Info(name);
        if (info is null) { WriteError($"?cervello '{name}' non trovato"); return; }
        Out.WriteLine($"name:      {info.Name}");
        Out.WriteLine($"mode:      {info.Mode}");
        Out.WriteLine($"available: [{string.Join(", ", info.Available)}]");
        Out.WriteLine($"checksum:  {info.Checksum ?? "-"}");
        Out.WriteLine($"attached:  {info.Attached}");
        Out.WriteLine($"source:    {info.Source ?? "-"}");
        Out.WriteLine($"docs:      {info.Docs}");
        if (info.Probe.Count > 0)
            foreach (var (k, v) in info.Probe) Out.WriteLine($"probe.{k}: {v}");
    }

    private void RunBrainResolve(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (name is null) { PrintBrainHelp(); return; }
        var explain = args.Contains("--explain", StringComparer.OrdinalIgnoreCase);
        // F1 del modello di scope (Knowledge/Brain-Scope-Modello.md): diagnostica READ-ONLY della catena.
        // Oggi l'unico livello popolato è PROCESS (brain attivati con BRAIN USE); AGENT/TASK/SESSION
        // arriveranno con F2/F3. --explain mostra la catena e il set effettivo.
        var scope = CurrentBrainScope();
        if (explain)
        {
            Out.WriteLine("scope (dal più interno al più esterno):");
            foreach (var l in scope.Explain()) Out.WriteLine(l);
        }
        var res = scope.Resolve(name);
        if (res is null)
        {
            WriteError($"?brain '{name}' non risolto nello scope corrente — controlla BRAIN LIST, "
                     + "oppure attivalo con BRAIN USE (globale) o BRAIN USE … --scope session");
            return;
        }
        var info = Basic.AI.Knowledge.BrainRegistry.Info(res.Ref.Name);
        Out.WriteLine($"[OK] '{name}' → brain '{res.Ref.Name}'"
            + (res.Ref.Alias is { Length: > 0 } a ? $" (alias '{a}')" : "")
            + $" — risolto al livello {res.Level}"
            + (info is null ? "" : $" — available: [{string.Join(", ", info.Available)}]"));
    }

    private async Task RunBrainInstallAsync(List<string> args, CancellationToken ct)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        var source = args.Count > 1 && !args[1].StartsWith("--") ? args[1] : null;
        if (name is null || source is null) { PrintBrainHelp(); return; }
        var dest = RagGraphFlags.Value(args, "--path", "-p");
        var activate = RagGraphFlags.Has(args, "--activate", "-a");
        Out.WriteLine($"INSTALL BRAIN: {name} ← {source}");
        var result = await Task.Run(() => Basic.AI.Knowledge.BrainInstaller.Install(name, source, dest, activate), ct)
            .ConfigureAwait(false);
        if (!result.Ok)
        {
            WriteError($"?{result.Error}");
            return;
        }
        Out.WriteLine($"[OK] brain '{name}' installato in {result.InstallRoot} — available: [{string.Join(", ", result.Available)}]");
    }

    private void RunBrainExport(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (name is null) { PrintBrainHelp(); return; }
        var outZip = RagGraphFlags.Value(args, "--out", "-o");
        var result = Basic.AI.Knowledge.BrainPackager.Export(name, outZip);
        if (result is null)
        {
            WriteError($"?cervello '{name}' non disponibile: costruiscilo con RAG BUILD/GRAPH BUILD");
            return;
        }
        Out.WriteLine($"[OK] esportato {result.ZipPath} — available: [{string.Join(", ", result.Available)}]");
    }

    private void RunBrainSign(List<string> args)
    {
        var name = args.Count > 0 && !args[0].StartsWith("--") ? args[0] : null;
        var dir = args.Count > 1 && !args[1].StartsWith("--") ? args[1] : null;
        if (name is null || dir is null) { PrintBrainHelp(); return; }
        var outZip = RagGraphFlags.Value(args, "--out", "-o");
        var result = Basic.AI.Knowledge.BrainSigner.Sign(name, dir);
        if (!result.Valid)
        {
            WriteError($"?il cervello '{name}' NON è valido (nessun probe passato):");
            foreach (var (k, v) in result.Probe) WriteError($"  {k}: {v}");
            return;
        }
        Out.WriteLine($"[OK] brain '{name}' firmato — available: [{string.Join(", ", result.Available)}]");
        foreach (var (k, v) in result.Probe) Out.WriteLine($"  probe.{k}: {v}");
        if (!string.IsNullOrEmpty(outZip))
        {
            if (Basic.AI.Knowledge.BrainSigner.ExportSignedZip(name, outZip)) Out.WriteLine($"[OK] zip: {outZip}");
            else WriteError($"?export zip fallito: {outZip}");
        }
    }

    private void PrintBrainHelp()
    {
        Out.WriteLine("BRAIN USE <name> [...]         collega un brain già disponibile (multi: USE a, b, c)");
        Out.WriteLine("BRAIN DETACH <name>            scollega (i file restano)");
        Out.WriteLine("BRAIN LIST                     elenca i brain registrati (* = collegato)");
        Out.WriteLine("BRAIN INFO <name>              info + available + checksum");
        Out.WriteLine("BRAIN STATS <name> [--json]                    statistiche (documenti/chunk, nodi/edge, dimensione, manifest)");
        Out.WriteLine("BRAIN VERIFY <name> [--json]                  verifica di INTEGRITA' (manifest+checksum, db RAG, grafo)");
        Out.WriteLine("BRAIN VERIFY <name> --golden <file.jsonl> [--topk N] [--min-p1 0.8]   QUALITA' del retrieval (P@1/P@3/recall/MRR)");
        Out.WriteLine("BRAIN CONVERT <name> --to rag|graph [--out <nuovo>|--in-place] [--min-edges N] [--dry-run]");
        Out.WriteLine("BRAIN DIFF <a> <b> [--json]                    differenze (documenti RAG e nodi del grafo)");
        Out.WriteLine("BRAIN DOCTOR <name>                            VERIFY + fix sicuri (FTS5 rebuild, VACUUM, prune dangling) + riverifica");
        Out.WriteLine("BRAIN CREATE <name> --from <dir> [--mode rag|graph|both] [--register] [--activate] [--dry-run]");
        Out.WriteLine("BRAIN OPTIMIZE <name> [--dry-run] [--reindex-fts]   VACUUM+optimize+FTS5 (rag), recalcolo export (graph)");
        Out.WriteLine("BRAIN BACKUP <name> [--out file.zip]           zip .gmbrain (alias di EXPORT, nome con timestamp)");
        Out.WriteLine("BRAIN RENAME <old> <new> [--dry-run]           rinomina artefatti + registry (root dei cervelli invariata)");
        Out.WriteLine("BRAIN REGISTER <name> [--mode rag|graph|both] [--source dir] [--activate]   censce artefatti già su disco");
        Out.WriteLine("BRAIN BUILDVAULT [--out dir]                   rigenera il brain di auto-conoscenza (StdLib → pagine + RAG)");
        Out.WriteLine("BRAIN DEDUPE <name> [--apply] [--dry-run]     duplicati: chunk (testo identico + crumbs) e edge del grafo");
        Out.WriteLine("BRAIN EXTRACT <name> --out <nuovo> [--source substr] [--node substr] [--hops N]");
        Out.WriteLine("BRAIN MERGE <a> <b> [c…] --out <nuovo> [--on-conflict skip|replace]");
        Out.WriteLine("BRAIN SCOPE [INHERIT|PRIVATE] [--without a,b] [--reset]   livello SESSIONE della catena di scope");
        Out.WriteLine("BRAIN USE <name> [AS <alias>] [--scope session]            dichiara nella sessione (registry invariato)");
        Out.WriteLine("BRAIN RESOLVE <name|alias> [--explain]   risolve un nome/alias nella catena di scope (--explain mostra i livelli)");
        Out.WriteLine("BRAIN INSTALL <name> <zip|url> [--path dir] [--activate]   importa uno zip .gmbrain");
        Out.WriteLine("BRAIN EXPORT <name> [--out f]  genera lo zip di un brain esistente");
        Out.WriteLine("BRAIN SIGN <name> <dir> [--out f]  firma un cervello utente (probe minimi graph/vectordb)");
        Out.WriteLine("  costruzione dei cervelli: RAG BUILD / GRAPH BUILD");
    }

}

