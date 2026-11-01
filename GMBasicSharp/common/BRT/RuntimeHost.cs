using CommonLib.Keyboard;
using CommonLib.Clipboard;
// FILE: Basic.Runtime.RuntimeHost.cs

using Basic.Runtime.Services;

namespace Basic.Runtime;

/// <summary>
/// Implementazione di default del BASIC Runtime Host.
/// Usa System.Console e le API standard .NET per i servizi core.
/// Network/HTTP/Database richiedono l'interprete (non inclusi in BRT).
/// </summary>
public sealed class DefaultRuntimeHost : IRuntimeHost
{
    // === Services ===
    public IConsoleService Console { get; }
    public IKeyboardService Keyboard { get; }
    public IClipboardService Clipboard { get; }
    public IFileService File { get; }
    public ITimeService Time { get; }
    public INetworkService Network { get; }
    public IHttpService Http { get; }
    public IAgentService Agent { get; }
    public ITeamService Team { get; }
    public IWorkflowService Workflow { get; }
    public IGraphicsService Graphics { get; }
    public IAudioService Audio { get; }
    public CommonLib.Http.HttpClientHelper HttpClientHelper { get; } = new();
    public IDatabaseService Database { get; }
    public Basic.Databases.Rdd.DataSession SqlDataSession { get; } = new();
    public ICryptoService Crypto { get; }
    public IMemoryService Memory { get; }
    public IFfiService Ffi { get; }
    public System.Random Rng { get; } = new();
    public ISandboxService? Sandbox { get; }
    public RuntimeConfig Config { get; }

    public DefaultRuntimeHost(RuntimeConfig? config = null)
    {
        Config = config ?? new RuntimeConfig();
        Console = new ConsoleService();
        Clipboard = new ClipboardService();
        Keyboard = new KeyboardServiceConsole(Clipboard);
        File = new FileService();
        Time = new TimeService();
        Crypto = new CryptoService();
        Memory = new MemoryService();
        Ffi = new FfiService();

        // Rete REALE: NetworkService (TCP completo; UDP/SSL ancora stub). Prima
        // NullNetworkService: tutti i comandi TCP fallivano con "Network disabled".
        Network = new NetworkService();
        Http = new HttpService();
        Agent = new NullAgentService();
        Team = new NullTeamService();
        Workflow = new NullWorkflowService();
        Graphics = new NullGraphicsService();
        Audio = new NullAudioService();
        Database = new NullDatabaseService();
        Sandbox = null;
    }

    // === Type System ===
    public object CoerceValue(object? value, DataType targetType)
    {
        if (value is null) return targetType switch
        {
            StringType => string.Empty,
            _ => 0.0
        };
        if (targetType is StringType)
            return value.ToString() ?? string.Empty;
        if (value is IConvertible conv)
        {
            try
            {
                return targetType switch
                {
                    DoubleType => conv.ToDouble(System.Globalization.CultureInfo.InvariantCulture),
                    // INTEGER = 64 bit (parità gambe native): coercizzare a Int16 troncava
                    // in silenzio i valori > 32767 (e ToInt16 lanciava su quelli fuori range).
                    IntegerType => conv.ToInt64(System.Globalization.CultureInfo.InvariantCulture),
                    UIntegerType => conv.ToUInt64(System.Globalization.CultureInfo.InvariantCulture),
                    LongType => conv.ToInt64(System.Globalization.CultureInfo.InvariantCulture),
                    SingleType => conv.ToSingle(System.Globalization.CultureInfo.InvariantCulture),
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
