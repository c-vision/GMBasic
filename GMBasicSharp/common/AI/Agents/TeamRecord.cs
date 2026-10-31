// FILE: Basic.Agents.TeamRecord
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.Agents;

/// <summary>Ruolo di un membro nel team (tipizzato — validazione a compile-time).</summary>
public enum TeamRole
{
    Coordinator,
    Worker,
    Critic,
    Observer
}

/// <summary>Strategia di orchestrazione del team.</summary>
public enum TeamStrategy
{
    Sequential,
    RoundRobin,
    Coordinator,
    Mixture,
    Distillation,
    Deliberation,
    Dag
}

/// <summary>Membro del team: riferimento a un agente + ruolo + override prompt.</summary>
public sealed class TeamMember
{
    public required string AgentName { get; init; }
    public TeamRole Role { get; set; } = TeamRole.Worker;
    public string? RolePrompt { get; set; }
}

/// <summary>Messaggio nella conversazione del team.</summary>
public sealed record TeamMessage(string AgentName, string Content, DateTimeOffset Timestamp);

/// <summary>Stato di un team di agenti: membri, conversazione, strategia.</summary>
public sealed class TeamRecord
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");
    public required string Name { get; init; }
    // Set accessibile: JsonSerializer non popola le proprietà get-only (il LOAD
    // deserializzava 'members' ma la lista restava vuota → TEAM.AGENTS vuoto).
    public List<TeamMember> Members { get; set; } = [];
    public List<TeamMessage> Conversation { get; set; } = [];
    public TeamStrategy Strategy { get; set; } = TeamStrategy.Sequential;
    public string? Coordinator { get; set; }
    public Dictionary<string, string> Metadata { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    // ── RecursiveMAS R1: loop ricorsivo configurabile (text baseline) ─────────
    /// <summary>Numero di round ricorsivi r (1..8, default 1 = comportamento pre-R1).</summary>
    public int RecursionRounds { get; set; } = 1;
    /// <summary>Modalità latent: "text" | "embed" | "native" (default "text").</summary>
    public string LatentMode { get; set; } = "text";
    /// <summary>Numero di latent thoughts per agente (1..32, default 8).</summary>
    public int LatentThoughts { get; set; } = 8;
    /// <summary>Config JSON outer link (es. {"hidden":256,"arch":"mlp"}), null = identità.</summary>
    public string? OuterLinkConfig { get; set; }
    /// <summary>Config JSON inner link, null = identità.</summary>
    public string? InnerLinkConfig { get; set; }
    /// <summary>Limite token per recursion round (guardia, default 2048).</summary>
    public int MaxRecursionTokens { get; set; } = 2048;

    // ── Estensioni RecursiveMAS §9 ───────────────────────────────────────────
    /// <summary>AGoT DAG (9.4): profondità max di espansione (default 1).</summary>
    public int DMax { get; set; } = 1;
    /// <summary>AGoT DAG (9.4): numero max di sotto-problemi per nodo (default 3).</summary>
    public int LMax { get; set; } = 3;
    /// <summary>AGoT DAG (9.4): numero max di nodi (default 3).</summary>
    public int NMax { get; set; } = 3;
    /// <summary>AGoT DAG (9.4): soglia di complessità per l'espansione ricorsiva.</summary>
    public double ComplexityThreshold { get; set; } = 0.5;
    /// <summary>MAR/MIRROR/ReTreVal (9.5): modalità riflessione del team (intra|inter|both|off).</summary>
    public string ReflectionMode { get; set; } = "off";
    /// <summary>MAR (9.5): persone critiche separate da '|' (es. spec|edge|logic).</summary>
    public string? CriticPersonas { get; set; }
    /// <summary>MAR (9.5): agente judge per la sintesi dei critici.</summary>
    public string? JudgeAgent { get; set; }
    /// <summary>ReTreVal (9.5): top-k dei nodi che sopravvivono al prune (default 3).</summary>
    public int PruneTopK { get; set; } = 3;
}
