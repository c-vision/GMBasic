// FILE: Basic.Runtime.HeadlessRuntimeHost.cs

using Basic.Runtime.Services;

namespace Basic.Runtime;

/// <summary>
/// RuntimeHost headless per test e CI. Usa StringWriter per catturare l'output.
/// </summary>
public sealed class HeadlessRuntimeHost : IRuntimeHost
{
    /* I servizi grafici/audio NON si costruiscono all'avvio: la loro creazione carica
     * `libgmb_gfx` (SDL2/3, MoltenVK, freetype/harfbuzz + plumbing) e, per l'audio,
     * `SDL2_mixer` con la sua catena di codec. Un programma che stampa una riga non deve
     * pagare niente di tutto questo: si creano al PRIMO uso (prima chiamata grafica/audio),
     * tramite le factory impostate da IlRuntime.PrepareGraphicsHost. */
    private static IGraphicsService? _defaultGraphics;
    private static IAudioService? _defaultAudio;
    private static IClipboardService? _defaultClipboard;

    /// <summary>Factory (impostata dal runtime) per creare il servizio grafico al primo uso.</summary>
    public static Func<IGraphicsService?>? GraphicsFactory { get; set; }
    public static Func<IAudioService?>? AudioFactory { get; set; }
    public static Func<IClipboardService?>? ClipboardFactory { get; set; }

    /// <summary>GraphicsService di default usato dal ctor quando non ne viene passato uno
    /// esplicito. Creato PIGRAMENTE alla prima lettura (via <see cref="GraphicsFactory"/>).</summary>
    public static IGraphicsService? DefaultGraphics
    {
        get => _defaultGraphics ??= GraphicsFactory?.Invoke();
        set => _defaultGraphics = value;
    }
    /// <summary>AudioService di default (creato alla prima lettura).</summary>
    public static IAudioService? DefaultAudio
    {
        get => _defaultAudio ??= AudioFactory?.Invoke();
        set => _defaultAudio = value;
    }
    /// <summary>Clipboard di default (creato alla prima lettura).</summary>
    public static IClipboardService? DefaultClipboard
    {
        get => _defaultClipboard ??= ClipboardFactory?.Invoke();
        set => _defaultClipboard = value;
    }

    public IConsoleService Console { get; }
    public IKeyboardService Keyboard { get; }
    public IClipboardService Clipboard => _clipboard ??= ClipboardFactory?.Invoke() ?? new ClipboardService();
    public IFileService File { get; }
    public ITimeService Time { get; }
    public INetworkService Network { get; }
    public IHttpService Http { get; }
    public IAgentService Agent { get; }
    public ITeamService Team { get; }
    public IWorkflowService Workflow { get; }
    private IGraphicsService? _graphics;
    private IAudioService? _audio;
    private IClipboardService? _clipboard;

    /* PROPRIETÀ PIGRE: costruire i servizi grafici/audio costa il caricamento di
     * `libgmb_gfx` (SDL2/3, MoltenVK, freetype/harfbuzz, codec) e di `SDL2_mixer`: un
     * programma che stampa una riga non deve pagarli. Il servizio nasce alla PRIMA
     * chiamata grafica/audio (factory impostata da IlRuntime.PrepareGraphicsHost). */
    /// <summary>Callback invocata quando il servizio grafico viene CREATO (una volta sola).
    /// Serve a cablare gli eventi GUI (ON KEY/ON MOUSE) **nel momento in cui** nasce la
    /// finestra, senza crearle per controllare: prima lo faceva IlRuntime.SetCurrent leggendo
    /// la proprietà, che però la istanziava all'avvio di ogni programma (+8 MB).</summary>
    public static Action<IGraphicsService>? GraphicsCreated { get; set; }

    public IGraphicsService Graphics
    {
        get
        {
            if (_graphics is null)
            {
                _graphics = GraphicsFactory?.Invoke() ?? new NullGraphicsService();
                try { GraphicsCreated?.Invoke(_graphics); } catch { /* mai bloccare l'avvio */ }
            }
            return _graphics;
        }
    }


    /// <summary>Il servizio grafico SOLO se è già stato creato (nessuna factory): serve ai
    /// percorsi caldi come PRINT, che devono sapere se c'è una finestra attiva senza
    /// caricare `libgmb_gfx` (+SDL, ~8 MB) per chiederlo.</summary>
    public IGraphicsService? CreatedGraphics => _graphics;
    public IAudioService Audio => _audio ??= AudioFactory?.Invoke() ?? new NullAudioService();
    public CommonLib.Http.HttpClientHelper HttpClientHelper { get; } = new();
    public IDatabaseService Database { get; }
    public Basic.Databases.Rdd.DataSession SqlDataSession { get; } = new();
    public ICryptoService Crypto { get; }
    public IMemoryService Memory { get; }
    public IFfiService Ffi { get; }
    public System.Random Rng { get; } = new();
    public ISandboxService? Sandbox { get; }
    public RuntimeConfig Config { get; }

    private readonly HeadlessKeyboardService _headlessKeyboard;

    public HeadlessRuntimeHost(TextWriter? outputWriter = null, RuntimeConfig? config = null)
        : this(outputWriter, config, null)
    {
    }

    public HeadlessRuntimeHost(TextWriter? outputWriter, RuntimeConfig? config, IGraphicsService? graphics)
    {
        Config = config ?? new RuntimeConfig();
        Console = new HeadlessConsoleService(outputWriter ?? new StringWriter());
        _headlessKeyboard = new HeadlessKeyboardService();
        Keyboard = _headlessKeyboard;
        // Input scriptato dai test/CI (stessa variabile della gamba C99/VM-C):
        // ogni carattere diventa un tasto; \n / \r -> INVIO.
        Basic.Runtime.Services.NativeTui.InjectScriptFromEnv();
        { var script = System.Environment.GetEnvironmentVariable("GMB_TUI_SCRIPT");
          if (!string.IsNullOrEmpty(script))
              foreach (var ch in script)
                  _headlessKeyboard.QueueKey(ch == '\n' ? '\r' : ch); }
        _clipboard = null;   // lazy: vedi proprietà Clipboard
        File = new FileService();
        Time = new TimeService();
        // Rete REALE (TCP/UDP/SSL) — NetworkService implementa TCP completo; UDP/SSL
        // sono ancora stub (vedi NetworkService.cs). Prima NullNetworkService: TUTTI i
        // comandi TCP fallivano con "Network disabled" (i sample Network/ esistono ma
        // non giravano). RECV() è stato collegato a questo servizio (2026-08-09).
        Network = new NetworkService();
        // HTTP reale (client HttpClient + server HttpListener). Prima NullHttpService:
        // il BuiltInFunction FETCH(url) e HTTP.GET(url) fallivano con "HTTP disabled"
        // nei runner headless (solo il FETCH avanzato via HttpClientHelper funzionava).
        Http = new HttpService();
        Agent = new NullAgentService();
        Team = new NullTeamService();
        Workflow = new NullWorkflowService();
        _graphics = graphics;   // lazy: se null, la proprietà usa GraphicsFactory/Null
        _audio = null;          // lazy: la proprietà usa AudioFactory/Null
        // Un solo servizio: la partial nativa implementa anche DBF/ISAM/SPEED/Ledger,
        // quindi non c'e' piu' una scelta legata all'edizione.
        Database = new DatabaseService(SqlDataSession);
        Crypto = new CryptoService();
        Memory = new MemoryService();
        Ffi = new FfiService();
    }

    public void QueueKey(char c) => _headlessKeyboard.QueueKey(c);
    public void QueueLine(string line) => _headlessKeyboard.QueueLine(line);

    public object CoerceValue(object? value, DataType targetType)
    {
        if (value is null) return targetType switch
        {
            StringType => string.Empty,
            _ => 0.0
        };
        if (targetType is StringType) return value.ToString() ?? string.Empty;
        if (value is IConvertible conv)
        {
            try
            {
                return targetType switch
                {
                    DoubleType => conv.ToDouble(System.Globalization.CultureInfo.InvariantCulture),
                    IntegerType => conv.ToInt16(System.Globalization.CultureInfo.InvariantCulture),
                    LongType => conv.ToInt64(System.Globalization.CultureInfo.InvariantCulture),
                    _ => value
                };
            }
            catch { }
        }
        return value;
    }

    public DataType DeduceTypeFromSuffix(string name)
    {
        if (string.IsNullOrEmpty(name)) return new DoubleType();
        return name[^1] switch
        {
            '$' => new StringType(),
            '%' => new IntegerType(),
            '!' => new SingleType(),
            '#' => new DoubleType(),
            '&' => new LongType(),
            _ => new DoubleType()
        };
    }

    public object GetDefaultValue(DataType type) => type switch
    {
        StringType => string.Empty,
        _ => 0.0
    };

    public BasicException CreateError(string message, ErrorCode code)
        => new BasicException(code, default, message);
}
