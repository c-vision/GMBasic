// FILE: Basic.Agents.AgentPermissionLevel
namespace Basic.Agents;

/// <summary>Livello di permesso di un agente (modello Sandbox/Supervised/YOLO — vedi
/// ).</summary>
public enum AgentPermissionLevel
{
    /// <summary>Solo tool a rischio 'safe' (letture + interazione core); i 'risky' → errore esplicito.</summary>
    Sandbox,

    /// <summary>I tool 'risky' richiedono approvazione (human-in-the-loop): l'ASK attende
    /// (con timeout, fail-closed) finché la decisione arriva (approve/deny).</summary>
    Supervised,

    /// <summary>Autonomia totale senza conferme, ma audit completo + override per-tool 'deny'.</summary>
    YOLO
}
