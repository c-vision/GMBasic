// FILE: Basic.Runtime.Configuration.RuntimeConfig.cs

namespace Basic.Runtime;

/// <summary>Configurazione del BASIC Runtime.</summary>
public record RuntimeConfig
{
    public int DefaultTimeoutMs { get; init; } = 30000;
    public bool EnableNetwork { get; init; } = true;
    public bool EnableDatabase { get; init; } = true;
    public bool EnableHttpServer { get; init; } = true;
    public bool EnableSandbox { get; init; } = false;
    public bool EnableFileSystem { get; init; } = true;
    public string? WorkingDirectory { get; init; }
    public string? DataDirectory { get; init; }
}
