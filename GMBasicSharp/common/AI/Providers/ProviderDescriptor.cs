// FILE: Basic.Providers.ProviderDescriptor
namespace Basic.Providers;

/// <summary>Registered provider entry (metadata + mutable config).</summary>
public sealed class ProviderDescriptor
{
    private readonly Dictionary<string, string> _config;

    public ProviderDescriptor(string role, string name, string kind, IReadOnlyDictionary<string, string>? config = null,
        int priority = 0)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(role);
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(kind);
        if (!ProviderRole.IsKnown(role))
            throw new ArgumentException($"Unknown provider role '{role}'.", nameof(role));

        Role = ProviderRole.Normalize(role);
        Name = name;
        Kind = kind.ToLowerInvariant();
        Priority = priority;
        _config = config is null
            ? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, string>(config, StringComparer.OrdinalIgnoreCase);
    }

    public string Role { get; }
    public string Name { get; }
    public string Kind { get; }
    public int Priority { get; set; }
    public ProviderStatus Status { get; set; } = ProviderStatus.Available;
    public ModelCapability Capabilities { get; set; } = ModelCapability.Text;
    public string? ApiBaseUrl { get; set; }

    // AI-Provider-Discover-Routing-Plan: campi per modelli scoperti via API key (es.
    // OpenRouter) con routing credit-based. Popolati da PROVIDER.DISCOVER (port di
    // unicorn-server fetch_remote_models). IsFree/costi dal pricing del provider;
    // CreditsRemaining alimentato dal credit tracker (RecordCompletion) -- a 0 il
    // provider va Blocked e la rotazione passa al successivo.
    public bool IsFree { get; set; }
    public decimal CostInputPer1m { get; set; }
    public decimal CostOutputPer1m { get; set; }
    public int CreditsRemaining { get; set; } = -1; // -1 = illimitato

    private int _weight = 1;

    /// <summary>Relative weight for round-robin distribution among providers tied on
    /// <see cref="Priority"/> — see <c>PROVIDER.SETWEIGHT</c>. Clamped to a minimum of 1
    /// (a weight of 0 or less would never be selected, which isn't what "weight" means here;
    /// use <see cref="ProviderStatus.Blocked"/> to actually exclude a provider).</summary>
    public int Weight
    {
        get => _weight;
        set => _weight = Math.Max(1, value);
    }

    public IReadOnlyDictionary<string, string> Config => _config;

    public string? GetConfig(string key) => _config.GetValueOrDefault(key);

    public void SetConfig(string key, string value)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        _config[key] = value ?? string.Empty;
    }
}
