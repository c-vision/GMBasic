// FILE: Basic.Agents.TeamServer
using System.Linq;
using System.Text.Json;
using CommonLib;
using System.Text.Json.Serialization;

namespace Basic.Agents;

/// <summary>Registry in-process dei TEAM di agenti: creazione, membri, conversazione.
/// Complementa AgentServer (che gestisce i singoli agenti) con il raggruppamento
/// e l'orchestrazione a livello team.</summary>
/// <remarks>Singleton + persistenza JSON; upgrade path: ISAM (AgentIsamStore pattern).</remarks>
public sealed class TeamServer
{
    public static TeamServer Instance { get; } = new();

    private readonly Dictionary<string, TeamRecord> _teams = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _lock = new();

    private TeamServer() { }

    internal static void ResetForTests()
    {
        lock (Instance._lock) Instance._teams.Clear();
    }

    // ─── CRUD team ───────────────────────────────────────────────────────────────

    public bool Create(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (_teams.ContainsKey(name)) return false;
            _teams[name] = new TeamRecord { Name = name };
        }
        AgentEventLog.Instance.Record(AgentEventKind.TeamCreated, name, "team created");
        return true;
    }

    public bool Delete(string name)
    {
        lock (_lock)
        {
            if (!_teams.Remove(name)) return false;
        }
        AgentEventLog.Instance.Record(AgentEventKind.TeamDeleted, name, "team deleted");
        return true;
    }

    public bool Exists(string name)
    {
        lock (_lock) return _teams.ContainsKey(name);
    }

    public List<TeamRecord> List()
    {
        lock (_lock) return [.. _teams.Values];
    }

    public TeamRecord? Get(string name)
    {
        lock (_lock) return _teams.TryGetValue(name, out var t) ? t : null;
    }

    // ─── Membri ──────────────────────────────────────────────────────────────────

    public bool AddAgent(string teamName, string agentName, TeamRole role = TeamRole.Worker, string? rolePrompt = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(teamName);
        ArgumentException.ThrowIfNullOrWhiteSpace(agentName);
        if (!AgentServer.Instance.Exists(agentName)) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            if (team.Members.Any(m => m.AgentName.Equals(agentName, StringComparison.OrdinalIgnoreCase))) return false;
            team.Members.Add(new TeamMember { AgentName = agentName, Role = role, RolePrompt = rolePrompt });
            AgentEventLog.Instance.Record(AgentEventKind.TeamMemberAdded, teamName, $"agent={agentName} role={role}");
            return true;
        }
    }

    public bool RemoveAgent(string teamName, string agentName)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            return team.Members.RemoveAll(m => m.AgentName.Equals(agentName, StringComparison.OrdinalIgnoreCase)) > 0;
        }
    }

    public bool SetRole(string teamName, string agentName, TeamRole role)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            var m = team.Members.FirstOrDefault(x => x.AgentName.Equals(agentName, StringComparison.OrdinalIgnoreCase));
            if (m is null) return false;
            m.Role = role;
            return true;
        }
    }

    public bool SetStrategy(string teamName, TeamStrategy strategy)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.Strategy = strategy;
            return true;
        }
    }

    public bool SetCoordinator(string teamName, string? agentName)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            if (agentName is not null && !team.Members.Any(m => m.AgentName.Equals(agentName, StringComparison.OrdinalIgnoreCase)))
                return false;
            team.Coordinator = agentName;
            return true;
        }
    }

    // ─── RecursiveMAS R1: recursion config ───────────────────────────────────

    public bool SetRecursion(string teamName, int rounds)
    {
        if (rounds < 1 || rounds > 8) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.RecursionRounds = rounds;
            return true;
        }
    }

    public bool SetLatent(string teamName, string mode)
    {
        var m = mode.Trim().ToLowerInvariant();
        if (m is not ("text" or "embed" or "native" or "kv")) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.LatentMode = m;
            return true;
        }
    }

    public bool SetLink(string teamName, string which, string? cfgJson)
    {
        var w = which.Trim().ToLowerInvariant();
        if (w is not ("inner" or "outer" or "distill")) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            if (w == "inner") team.InnerLinkConfig = string.IsNullOrWhiteSpace(cfgJson) ? null : cfgJson;
            else if (w == "outer") team.OuterLinkConfig = string.IsNullOrWhiteSpace(cfgJson) ? null : cfgJson;
            else team.OuterLinkConfig = string.IsNullOrWhiteSpace(cfgJson) ? null : cfgJson; // distill alias outer
            return true;
        }
    }

    public bool SetThoughts(string teamName, int m)
    {
        if (m < 1 || m > 32) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.LatentThoughts = m;
            return true;
        }
    }

    public int GetRecursion(string teamName)
    {
        lock (_lock) return _teams.TryGetValue(teamName, out var t) ? t.RecursionRounds : 1;
    }

    // ── Estensioni §9.4/9.5: DAG, reflection, critic, prune ─────────────────

    public bool SetDag(string teamName, int dMax, int lMax, int nMax)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            if (dMax > 0) team.DMax = dMax;
            if (lMax > 0) team.LMax = lMax;
            if (nMax > 0) team.NMax = nMax;
            return true;
        }
    }

    public bool SetComplexity(string teamName, double threshold)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.ComplexityThreshold = threshold;
            return true;
        }
    }

    public bool SetReflection(string teamName, string mode)
    {
        var m = mode.Trim().ToLowerInvariant();
        if (m is not ("intra" or "inter" or "both" or "off")) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.ReflectionMode = m;
            return true;
        }
    }

    public bool SetCritic(string teamName, string personas, string judge)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            if (!string.IsNullOrWhiteSpace(personas)) team.CriticPersonas = personas;
            if (!string.IsNullOrWhiteSpace(judge)) team.JudgeAgent = judge;
            return true;
        }
    }

    public bool SetPrune(string teamName, int topK)
    {
        if (topK < 1) return false;
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.PruneTopK = topK;
            return true;
        }
    }

    public string GetLatent(string teamName)
    {
        lock (_lock) return _teams.TryGetValue(teamName, out var t) ? t.LatentMode : "text";
    }

    public string GetLinkInfo(string teamName)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var t)) return "{}";
            return System.Text.Json.JsonSerializer.Serialize(new { inner = t.InnerLinkConfig, outer = t.OuterLinkConfig, recursion = t.RecursionRounds, latent = t.LatentMode, thoughts = t.LatentThoughts });
        }
    }

    // ─── Conversazione ───────────────────────────────────────────────────────────

    public bool PostMessage(string teamName, string agentName, string content)
    {
        lock (_lock)
        {
            if (!_teams.TryGetValue(teamName, out var team)) return false;
            team.Conversation.Add(new TeamMessage(agentName, content, DateTimeOffset.UtcNow));
        }
        AgentEventLog.Instance.Record(AgentEventKind.TeamMessage, teamName, $"agent={agentName}");
        return true;
    }

    public List<TeamMessage> GetConversation(string teamName)
    {
        lock (_lock) return _teams.TryGetValue(teamName, out var t) ? [.. t.Conversation] : [];
    }

    // ─── Persistenza JSON ────────────────────────────────────────────────────────

    /// <summary>Salva il team (`.team.json`) e il checkpoint dei link R_in/R_out
    /// (file `<path>.links.json` accanto). Default: marketplace locale
    /// `<configDir>/recursivemas/&lt;team&gt;/&lt;team&gt;.team.json` (A.16).</summary>
    public void Save(string teamName, string? path = null)
    {
        var team = Get(teamName) ?? throw new ArgumentException($"Team '{teamName}' non trovato.");
        var filePath = path ?? DefaultMarketplacePath(teamName);
        var snapshot = JsonSerializer.Serialize(team, JsonOptions);
        var dir = Path.GetDirectoryName(filePath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(filePath, snapshot);
        // A.4/R6: persistenza RecursiveLink (inner per agente + outer) accanto al team.
        Basic.Agents.Recursive.RecursiveTrainer.SaveLinks(teamName, filePath + ".links.json");
    }

    /// <summary>Carica il team (`.team.json`) e ripristina i checkpoint link se presenti.</summary>
    public bool Load(string teamName, string? path = null)
    {
        var filePath = path ?? DefaultMarketplacePath(teamName);
        if (!File.Exists(filePath)) return false;
        var team = JsonSerializer.Deserialize<TeamRecord>(File.ReadAllText(filePath), JsonOptions);
        if (team is null) return false;
        lock (_lock)
        {
            _teams[teamName] = team;
        }
        Basic.Agents.Recursive.RecursiveTrainer.LoadLinks(teamName, filePath + ".links.json");
        return true;
    }

    /// <summary>Percorso marketplace locale (A.16): GmbiPaths.RecursiveMasDir/&lt;team&gt;/&lt;team&gt;.team.json</summary>
    private static string DefaultMarketplacePath(string teamName)
    {
        var safe = new string(teamName.Where(char.IsLetterOrDigit).ToArray());
        if (safe.Length == 0) safe = "team";
        return Path.Combine(GmbiPaths.RecursiveMasDir, safe, safe + ".team.json");
    }

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };
}
