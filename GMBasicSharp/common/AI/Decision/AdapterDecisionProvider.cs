// AI/Decision/AdapterDecisionProvider.cs
//
// P6 — adapter LLM: usa un completer GENERATIVO (ILlmCompleter) per rispondere a una
// DecisionRequest, invece del readout dei logits. Serve per modelli/endpoint che non
// espongono logprob ma sanno generare (remoti OpenAI-compatibili, adapter LoRA, ecc.).
//
// Semantica: il prompt elenca le opzioni, la risposta viene normalizzata e confrontata
// (id/label esatti → numero → parola intera → slot A..P). La probabilità è 1 per l'opzione
// scelta e 0 per le altre: è un'adapter, non un modello calibrato; confidence/calibrated
// restano null (mai copiare Probability in Confidence). Nessun match = errore esplicito.

using System.Text.RegularExpressions;
using Basic.AI.Rag;

namespace Basic.Decision;

public sealed class AdapterDecisionProvider : ISystemOneProvider
{
    private readonly ILlmCompleter _completer;
    private readonly string _systemPrompt;
    private readonly string _name;

    public AdapterDecisionProvider(ILlmCompleter completer, string? systemPrompt = null,
        string name = "adapter")
    {
        _completer = completer ?? throw new ArgumentNullException(nameof(completer));
        _systemPrompt = string.IsNullOrWhiteSpace(systemPrompt)
            ? "You are a decision engine. Choose exactly one allowed option."
            : systemPrompt!;
        _name = name;
    }

    public string Name => _name;

    public DecisionCapabilities Capabilities =>
        DecisionCapabilities.Noul | DecisionCapabilities.Choice | DecisionCapabilities.Score;

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request,
        CancellationToken cancellationToken = default)
    {
        request.Validate();
        cancellationToken.ThrowIfCancellationRequested();

        var labels = request.Options.Select(o => o.Display).ToArray();
        var user = $"State:\n{request.State}\nAllowed options: {string.Join(", ", labels)}\n" +
                   "Answer with exactly one option and nothing else.";
        var answer = (_completer.Complete(_systemPrompt, user) ?? "").Trim();
        var chosen = MatchOption(request, answer)
            ?? throw new DecisionProviderException(
                $"adapter: la risposta non corrisponde a un'opzione: '{Truncate(answer)}'");

        // Un solo risultato (l'opzione scelta), come Mock/Rules: il contratto
        // ISystemOneProvider restituisce l'evidenza della decisione, non la distribuzione.
        return [DecisionResult.FromOption(request, chosen, 1.0,
            raw: $"adapter:{Truncate(answer)}")];
    }

    private static string Truncate(string s) => s.Length <= 80 ? s : s[..80] + "…";

    /// <summary>Confronto tollerante ma deterministico: esatto (id/label) → numero → parola
    /// intera (label più lunga vince: evita che "1" catturi "10") → slot A..P iniziale.</summary>
    internal static DecisionOption? MatchOption(DecisionRequest request, string answer)
    {
        var text = answer.Trim().Trim('"', '\'', '.', '!', '?', ':', ';');
        if (text.Length == 0) return null;

        foreach (var o in request.Options)
            if (Eq(text, o.Id) || Eq(text, o.Display)) return o;

        if (double.TryParse(text, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var number))
        {
            foreach (var o in request.Options)
                if (double.TryParse(o.Display, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out var value)
                    && Math.Abs(value - number) < 1e-9)
                    return o;
        }

        var wordMatches = request.Options
            .Where(o => ContainsWord(text, o.Display) || ContainsWord(text, o.Id))
            .OrderByDescending(o => o.Display.Length)
            .ToList();
        if (wordMatches.Count > 0) return wordMatches[0];

        // Verbale A..P: il modello può rispondere "B" o "B) ...".
        var first = text.TrimStart('(', '[', ' ', '{');
        if (first.Length > 0)
            foreach (var o in request.Options)
                if (o.Id.Length == 1 && char.ToUpperInvariant(first[0]) == char.ToUpperInvariant(o.Id[0]))
                    return o;
        return null;
    }

    private static bool Eq(string a, string b) =>
        a.Equals(b, StringComparison.OrdinalIgnoreCase);

    private static bool ContainsWord(string haystack, string needle)
    {
        if (string.IsNullOrWhiteSpace(needle)) return false;
        return Regex.IsMatch(haystack, $@"(?<![\p{{L}}\p{{N}}]){Regex.Escape(needle)}(?![\p{{L}}\p{{N}}])",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
    }
}
