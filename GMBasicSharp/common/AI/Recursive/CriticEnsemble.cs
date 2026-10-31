// FILE: Basic.Agents.Recursive.CriticEnsemble
// Estensione 9.5 (MAR/MIRROR/ReTreVal) — riflessione multi-persona: N critici (spec/edge/logic)
// valutano una traiettoria fallita, il judge sintetizza. Agganciabile a un agent in modalità
// reflection. Nessun training richiesto: solo orchestrazione di Ask.

using Basic.Agents;

namespace Basic.Agents.Recursive;

public static class CriticEnsemble
{
    public static readonly string[] DefaultCritics = ["spec", "edge", "logic"];

    /// <summary>Riflessione MAR: ogni critico (persona) analizza la traiettoria fallita, poi il
    /// judge sintetizza la riflessione. Ritorna la sintesi del judge (o la concatenazione se il
    /// judge non è disponibile).</summary>
    public static string Reflect(AgentServer server, string[]? criticAgents, string? judgeAgent, string failedTrajectory)
    {
        var critics = criticAgents is { Length: > 0 } ? criticAgents : DefaultCritics;
        var outputs = new List<string>();
        foreach (var c in critics)
        {
            var p = $"Sei il critico di persona '{c}'. Analizza criticamente questa traiettoria fallita e indica errori/miglioramenti:\n{failedTrajectory}";
            var ans = server.Ask(c, p);
            outputs.Add($"[{c}] {ans}");
        }
        if (!string.IsNullOrWhiteSpace(judgeAgent))
        {
            var synth = server.Ask(judgeAgent, $"Sei il giudice. Sintetizza la riflessione dei critici per evitare di ripetere l'errore:\n{string.Join("\n", outputs)}");
            return synth;
        }
        return string.Join("\n", outputs);
    }
}
