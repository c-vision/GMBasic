// AI/Decision/DecisionContracts.cs
//
// Contratto del decision layer (JEV / SystemOne). Vive in GMBasic perché è la superficie
// linguistica/policy/provider; il core di inferenza (logits + softmax calibrata) è in LLMEnv
// (ABI v8 CAP_DECIDE) e il training della testa in gmbllm (PUL).
//
// Regola: `AI DECIDE` produce EVIDENZA (valore + probabilità + confidenza), mai side effect.

namespace Basic.Decision;

/// <summary>Tipo di decisione (canonico upstream ms-swift).</summary>
public enum DecisionKind
{
    /// <summary>BOOLEAN-like: opzioni canoniche false/true.</summary>
    Noul,
    /// <summary>Una scelta tra opzioni stringa (es. "billing","technical","sales").</summary>
    Choice,
    /// <summary>Uno score su una scala (es. 0..10).</summary>
    Score,
}

/// <summary>Una possibile risposta della decisione. `TokenId` è il token singolo risolto dal
/// tokenizer del modello: il core LLMEnv lavora su un solo token per opzione (vincolo HARD).</summary>
public sealed record DecisionOption(
    string Id,
    string? Label = null,
    int? TokenId = null,
    float Bias = 0f,
    float Temperature = 1f)
{
    public string Display => Label ?? Id;
}

/// <summary>Richiesta di decisione: stato osservato + dominio di opzioni.</summary>
public sealed record DecisionRequest(
    string State,
    DecisionKind Kind,
    IReadOnlyList<DecisionOption> Options,
    string? Instructions = null,
    TimeSpan? Timeout = null)
{
    public void Validate()
    {
        if (Options is null || Options.Count == 0)
            throw new ArgumentException("DecisionRequest: almeno un'opzione è obbligatoria.", nameof(Options));
        if (Kind == DecisionKind.Noul && Options.Count != 2)
            throw new ArgumentException("DecisionRequest: NOUL richiede esattamente due opzioni (false/true).", nameof(Options));
        var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var option in Options)
        {
            if (string.IsNullOrWhiteSpace(option.Id))
                throw new ArgumentException("DecisionRequest: ogni opzione deve avere un Id.", nameof(Options));
            if (!ids.Add(option.Id))
                throw new ArgumentException($"DecisionRequest: opzione duplicata '{option.Id}'.", nameof(Options));
            if (option.Temperature <= 0)
                throw new ArgumentException($"DecisionRequest: temperatura non valida per '{option.Id}'.", nameof(Options));
        }
    }
}

/// <summary>Risultato di una decisione. `Value` è tipato per Kind: bool (Noul), string
/// (Choice), double (Score). `Calibrated` è null quando la calibrazione non è disponibile:
/// NON va mai copiato da Probability (canone del piano JEV).</summary>
public sealed record DecisionResult(
    DecisionKind Kind,
    string OptionId,
    object? Value,
    double Probability,
    double? Confidence = null,
    double? Calibrated = null,
    string? Raw = null)
{
    public static DecisionResult FromOption(DecisionRequest request, DecisionOption option,
        double probability, double? confidence = null, double? calibrated = null, string? raw = null)
    {
        object? value = request.Kind switch
        {
            DecisionKind.Noul => ParseBool(option),
            DecisionKind.Score => double.TryParse(option.Display, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var score) ? score : null,
            _ => option.Display,
        };
        return new DecisionResult(request.Kind, option.Id, value, probability, confidence, calibrated, raw);
    }

    private static bool ParseBool(DecisionOption option)
    {
        var text = option.Display.Trim();
        return text.Equals("true", StringComparison.OrdinalIgnoreCase)
            || text.Equals("yes", StringComparison.OrdinalIgnoreCase)
            || text.Equals("sì", StringComparison.OrdinalIgnoreCase)
            || text.Equals("si", StringComparison.OrdinalIgnoreCase)
            || text == "1"
            || text.Equals("on", StringComparison.OrdinalIgnoreCase);
    }
}

[Flags]
public enum DecisionCapabilities
{
    None = 0,
    Noul = 1 << 0,
    Choice = 1 << 1,
    Score = 1 << 2,
    Probability = 1 << 3,
    Confidence = 1 << 4,
    Calibration = 1 << 5,
    Local = 1 << 6,
    SharedPrefill = 1 << 7,
    SuffixBatch = 1 << 8,
}

public static class DecisionCapabilitiesExtensions
{
    public static DecisionCapabilities ForKind(DecisionKind kind) => kind switch
    {
        DecisionKind.Noul => DecisionCapabilities.Noul,
        DecisionKind.Choice => DecisionCapabilities.Choice,
        DecisionKind.Score => DecisionCapabilities.Score,
        _ => DecisionCapabilities.None,
    };

    public static bool Supports(this DecisionCapabilities caps, DecisionKind kind)
        => (caps & ForKind(kind)) == ForKind(kind);
}

/// <summary>Provider di decisione (Mock/Rules/local MLX/HTTP TypeSafe/LLM adapter).
/// Stesso contratto in C# e C; i backend chiamano, non implementano logica inline.</summary>
public interface ISystemOneProvider
{
    string Name { get; }
    DecisionCapabilities Capabilities { get; }
    IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, CancellationToken cancellationToken = default);
}

/// <summary>Capability opzionale: un completer LLM che sa anche leggere una decisione dai
/// logits (provider locale MLX, ABI v8 CAP_DECIDE). I provider non-LLM (Mock/Rules/HTTP)
/// implementano direttamente ISystemOneProvider.</summary>
public interface IDecisionCompleter
{
    IReadOnlyList<DecisionResult> Decide(DecisionRequest request);
}

/// <summary>Capability opzionale: un provider che sa valutare PIÙ domande sullo stesso stato
/// in un solo giro (prefill/KV condiviso). Il runtime la usa per la forma batch di AI DECIDE;
/// se non disponibile, il batch resta una sequenza di decisioni singole.</summary>
public interface IDecisionGroupCompleter
{
    IReadOnlyList<DecisionResult> DecideGroup(IReadOnlyList<DecisionRequest> requests);
}

public sealed class DecisionProviderException : Exception
{
    public DecisionProviderException(string message) : base(message) { }
    public DecisionProviderException(string message, Exception inner) : base(message, inner) { }
}
