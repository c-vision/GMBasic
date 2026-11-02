// AI/Decision/DecisionProviders.cs
//
// Provider deterministici (Mock/Rules) per il decision layer. Servono a testare il control flow
// e a dare un fallback prod-safe senza modello. Il provider locale MLX è un adapter sottile
// verso l'ABI v8 di LLMEnv (CAP_DECIDE); il remoto TypeSafe è HTTP; entrambi implementano la
// stessa interfaccia.

namespace Basic.Decision;

/// <summary>Mock deterministico: sceglie un'opzione fissa (la prima se non specificato).
/// Utile nei MiniTest e per il control flow senza rete/modello.</summary>
public sealed class MockDecisionProvider : ISystemOneProvider
{
    private readonly string? _optionId;
    private readonly double _probability;

    public MockDecisionProvider(string? optionId = null, double probability = 1.0, string name = "mock")
    {
        if (probability is < 0 or > 1) throw new ArgumentOutOfRangeException(nameof(probability));
        _optionId = optionId;
        _probability = probability;
        Name = name;
    }

    public string Name { get; }

    public DecisionCapabilities Capabilities =>
        DecisionCapabilities.Noul | DecisionCapabilities.Choice | DecisionCapabilities.Score |
        DecisionCapabilities.Probability;

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, CancellationToken cancellationToken = default)
    {
        request.Validate();
        cancellationToken.ThrowIfCancellationRequested();
        var option = _optionId is null
            ? request.Options[0]
            : request.Options.FirstOrDefault(o => o.Id.Equals(_optionId, StringComparison.OrdinalIgnoreCase))
              ?? throw new DecisionProviderException($"MockDecisionProvider: opzione '{_optionId}' non nel dominio.");
        return new[] { DecisionResult.FromOption(request, option, _probability, raw: "mock") };
    }
}

/// <summary>Regola deterministica: se lo stato contiene una chiave, sceglie l'opzione indicata.
/// Ordine di valutazione = ordine della lista. `Kind` null = vale per qualunque tipo.</summary>
public sealed record DecisionRule(string Contains, string OptionId, double Probability = 1.0, DecisionKind? Kind = null);

/// <summary>Provider a regole, prod-safe e senza modello/rete. Default: per NOUL riconosce
/// yes/true/sì e no/false; per CHOICE/SCORE sceglie la prima opzione con probabilità 0.5
/// (evidenza debole, mai un'autorizzazione implicita).</summary>
public sealed class RuleDecisionProvider : ISystemOneProvider
{
    private readonly IReadOnlyList<DecisionRule> _rules;

    public RuleDecisionProvider(IReadOnlyList<DecisionRule>? rules = null, string name = "rules")
    {
        _rules = rules ?? DefaultRules;
        Name = name;
    }

    public string Name { get; }

    public DecisionCapabilities Capabilities =>
        DecisionCapabilities.Noul | DecisionCapabilities.Choice | DecisionCapabilities.Score |
        DecisionCapabilities.Probability;

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, CancellationToken cancellationToken = default)
    {
        request.Validate();
        cancellationToken.ThrowIfCancellationRequested();

        foreach (var rule in _rules)
        {
            if (rule.Kind is { } kind && kind != request.Kind) continue;
            if (!request.State.Contains(rule.Contains, StringComparison.OrdinalIgnoreCase)) continue;
            var option = request.Options.FirstOrDefault(o => o.Id.Equals(rule.OptionId, StringComparison.OrdinalIgnoreCase));
            if (option is null) continue;   // regola non applicabile a questo dominio
            return new[] { DecisionResult.FromOption(request, option, rule.Probability, raw: $"rule:{rule.Contains}") };
        }

        // Nessuna regola: prima opzione, evidenza debole e dichiarata.
        return new[] { DecisionResult.FromOption(request, request.Options[0], 0.5, raw: "rule:default") };
    }

    private static readonly IReadOnlyList<DecisionRule> DefaultRules =
    [
        new("sì", "true", 1.0, DecisionKind.Noul),
        new("si", "true", 1.0, DecisionKind.Noul),
        new("yes", "true", 1.0, DecisionKind.Noul),
        new("true", "true", 1.0, DecisionKind.Noul),
        new("no", "false", 1.0, DecisionKind.Noul),
        new("false", "false", 1.0, DecisionKind.Noul),
    ];
}

/// <summary>Registry dei provider con catena di fallback. Ordine: preferito esplicito, poi
/// fallback dichiarati, poi tutti i registrati (ordine di registrazione). Un provider che non
/// supporta il Kind viene saltato; un'eccezione passa al successivo.</summary>
public sealed class DecisionProviderRegistry
{
    private readonly List<ISystemOneProvider> _providers = [];

    public IReadOnlyList<ISystemOneProvider> Providers => _providers;

    public DecisionProviderRegistry Register(ISystemOneProvider provider)
    {
        ArgumentNullException.ThrowIfNull(provider);
        _providers.RemoveAll(p => p.Name.Equals(provider.Name, StringComparison.OrdinalIgnoreCase));
        _providers.Add(provider);
        return this;
    }

    public ISystemOneProvider? Get(string name) =>
        _providers.FirstOrDefault(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, string? preferred = null,
        IReadOnlyList<string>? fallback = null, CancellationToken cancellationToken = default)
    {
        request.Validate();

        var chain = new List<ISystemOneProvider>();
        void AddByName(string? name)
        {
            if (string.IsNullOrWhiteSpace(name)) return;
            var provider = Get(name);
            if (provider is not null && !chain.Contains(provider)) chain.Add(provider);
        }

        AddByName(preferred);
        if (fallback is not null)
            foreach (var name in fallback) AddByName(name);
        foreach (var provider in _providers)
            if (!chain.Contains(provider)) chain.Add(provider);

        var errors = new List<string>();
        foreach (var provider in chain)
        {
            if (!provider.Capabilities.Supports(request.Kind)) continue;
            try
            {
                var results = provider.Evaluate(request, cancellationToken);
                if (results.Count > 0) return results;
                errors.Add($"{provider.Name}: nessun risultato");
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { errors.Add($"{provider.Name}: {ex.Message}"); }
        }

        throw new DecisionProviderException(
            $"Nessun provider ha prodotto una decisione ({request.Kind}). Tentativi: {string.Join("; ", errors)}");
    }
}
