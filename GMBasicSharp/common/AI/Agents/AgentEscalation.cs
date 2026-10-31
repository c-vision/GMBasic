// FILE: Basic.Agents.AgentEscalation — N4 Needle3 (policy di escalation cheap-first).
using System.Collections.Generic;

namespace Basic.Agents;

/// <summary>Livello della scala di escalation (N4, Needle3): dal più economico al più costoso.</summary>
public enum EscalationLevel
{
    ToolModel = 0,
    Jev = 1,
    LargeLlm = 2,
    Human = 3,
}

/// <summary>Policy di escalation dichiarata cheap-first (N4, Needle3):
/// small/tool model → Jev → LLM grande → conferma umana, con soglie, motivi e budget.
/// Non euristica sparsa: una tabella ordinata + una regola.</summary>
public sealed class EscalationPolicy
{
    /// <summary>Soglia di accettazione per livello: sotto, si sale di un gradino.</summary>
    public double[] AcceptThresholds { get; init; } = { 0.6, 0.7, 0.8, 1.0 };

    /// <summary>Costo stimato per livello (stessa unità arbitraria, solo ordinamento relativo).</summary>
    public double[] EstimatedCosts { get; init; } = { 1.0, 5.0, 50.0, 1000.0 };

    /// <summary>Livello massimo raggiungibile (default: tutta la scala).</summary>
    public EscalationLevel MaxLevel { get; init; } = EscalationLevel.Human;

    /// <summary>Decide il prossimo passo: (stesso livello = eseguire, livello sopra = escalare).
    /// Una decisione non calibrata o un motivo esplicito (NO_TOOL, UNSUPPORTED, ERROR) saltano
    /// subito al livello sopra, senza consumare il tentativo corrente.</summary>
    public (EscalationLevel Level, EscalationReason Reason) Decide(
        EscalationLevel current, DecisionResult decision, EscalationReason forcedReason = EscalationReason.None)
    {
        if (forcedReason != EscalationReason.None)
            return (StepUp(current), forcedReason);

        var reason = decision.EscalationOrNone(ThresholdFor(current));
        if (reason == EscalationReason.None) return (current, EscalationReason.None);
        return (StepUp(current), reason);
    }

    private double ThresholdFor(EscalationLevel level)
    {
        int i = (int)level;
        if (i < 0) return AcceptThresholds[0];
        if (i >= AcceptThresholds.Length) return 1.0;
        return AcceptThresholds[i];
    }

    private EscalationLevel StepUp(EscalationLevel current)
    {
        int next = (int)current + 1;
        int max = (int)MaxLevel;
        if (next > max) next = max;
        if (next > (int)EscalationLevel.Human) next = (int)EscalationLevel.Human;
        return (EscalationLevel)next;
    }
}
