// AI/Decision/MemoryDecisions.cs
//
// P9 — CONTRATTO d'uso del provider DECIDE per la memoria (KNOWLEDGEDRIFT).
//
// Qui NON c'è lo storage/lifecycle della memoria (WATCH/IMPORT/SUPERSEDE/RECEIPT restano nel
// workstream memoria): c'è solo il ponte fra una DecisionResult e un MEMORY RESULT
// `{verdict: STRONG|WEAK|NONE, confidence}` + le richieste canoniche per i giudizi semantici
// (currentness, supersession, contradiction). AI DECIDE produce evidenza, non scrive memoria.

namespace Basic.Decision;

/// <summary>Verdict di memoria, strutturalmente identico a DECISION&lt;T&gt;: probabilità +
/// confidenza + astensione (NONE).</summary>
public enum MemoryVerdict
{
    None = 0,
    Weak = 1,
    Strong = 2,
}

public sealed record MemoryDecision(MemoryVerdict Verdict, double Probability, double? Confidence);

/// <summary>Contratto memory↔DECIDE: richieste canoniche e mappatura probabilità→verdict.
/// Le soglie sono esplicite e configurabili (niente magia nascosta).</summary>
public static class MemoryDecisions
{
    public const double StrongThreshold = 0.75;
    public const double WeakThreshold = 0.40;

    /// <summary>Il fatto/claim è ancora corrente rispetto al candidato?</summary>
    public static DecisionRequest IsCurrent(string claim, string candidate, string? instructions = null)
        => DecisionSlots.CreateNoul(
            $"Claim: {claim}\nCandidato: {candidate}\nIl claim è ancora corrente?",
            instructions ?? "Rispondi true se il claim è ancora valido e corrente, false altrimenti.");

    /// <summary>Il candidato sostituisce (supersede) il fatto esistente?</summary>
    public static DecisionRequest Supersedes(string existing, string incoming, string? instructions = null)
        => DecisionSlots.CreateNoul(
            $"Esistente: {existing}\nNuovo: {incoming}\nIl nuovo sostituisce l'esistente?",
            instructions ?? "Rispondi true se il nuovo fatto rende obsoleto l'esistente.");

    /// <summary>I due fatti si contraddicono?</summary>
    public static DecisionRequest Contradicts(string a, string b, string? instructions = null)
        => DecisionSlots.CreateNoul(
            $"Fatto A: {a}\nFatto B: {b}\nI due fatti si contraddicono?",
            instructions ?? "Rispondi true se i due fatti non possono essere entrambi veri.");

    /// <summary>Mappa l'evidenza di DECIDE nel verdict di memoria. Per NOUL il valore booleano
    /// orienta la probabilità (false → 1−p); per CHOICE/SCORE vale la probabilità dell'opzione
    /// scelta. Sotto `weak` → NONE (astensione), sopra `strong` → STRONG, altrimenti WEAK.</summary>
    public static MemoryDecision ToVerdict(DecisionResult result, double strong = StrongThreshold,
        double weak = WeakThreshold)
    {
        ArgumentNullException.ThrowIfNull(result);
        if (!(weak >= 0.0 && weak < strong && strong <= 1.0))
            throw new ArgumentOutOfRangeException(nameof(weak),
                $"soglie non valide: 0 <= weak ({weak}) < strong ({strong}) <= 1.");

        var positive = result.Value switch
        {
            bool b => b,
            _ => true,
        };
        var probability = positive ? result.Probability : 1.0 - result.Probability;
        var verdict = !positive || probability < weak
            ? MemoryVerdict.None
            : probability >= strong ? MemoryVerdict.Strong : MemoryVerdict.Weak;
        return new MemoryDecision(verdict, probability, result.Confidence);
    }
}
