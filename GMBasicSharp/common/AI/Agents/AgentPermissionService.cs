// FILE: Basic.Agents.AgentPermissionService
// Valutazione delle policy di permesso (Sandbox/Supervised/YOLO) per le invocazioni dei tool
// dell'agente. L'UNICO gate: lo stesso perimetro serve sia il tool-loop locale (InvokeToolCall)
// sia il server MCP remoto (McpServer.HandleToolsCallAsync). Decisioni loggare in AgentEventLog.
using Basic.Mcp;

namespace Basic.Agents;

/// <summary>Esito della valutazione della policy per un tool.</summary>
public enum AgentVerdict
{
    Allow,
    Deny,
    Approve
}

/// <summary>Policy di permesso dell'agente: classifica il rischio per nome tool/procedura e
/// decide Allow/Deny/Approve in base al livello + agli override per-tool.</summary>
public sealed class AgentPermissionService
{
    public static AgentPermissionService Instance { get; } = new();

    private readonly Lock _lock = new();
    private readonly Dictionary<string, PendingApproval> _pending = new(StringComparer.OrdinalIgnoreCase);
    private int _nextId;

    private AgentPermissionService()
    {
    }

    // ── G4: Risk category classification ─────────────────────────────────────
    public enum RiskCategory
    {
        Read, Write, Upload, Post, Payment, AccountChange, Destructive
    }

    /// <summary>Classifica il rischio del tool per nome. Usato dal profilo GOVERNED per
    /// richiedere approval su ogni operazione non-read.</summary>
    public static RiskCategory Classify(string toolName)
    {
        if (string.IsNullOrWhiteSpace(toolName)) return RiskCategory.Destructive;
        var low = toolName.ToLowerInvariant();
        if (low.Contains("navigate") || low.Contains("snapshot") || low.Contains("accesstree") ||
            low.Contains("get_cookies") || low.Contains("find") || low.Contains("netlog") ||
            low.Contains("console") || low.Contains("read") || low.Contains("list") ||
            low.Contains("status") || low.Contains("get"))
            return RiskCategory.Read;
        if (low.Contains("upload") || low.Contains("download"))
            return RiskCategory.Upload;
        if (low.Contains("payment") || low.Contains("checkout") || low.Contains("purchase"))
            return RiskCategory.Payment;
        if (low.Contains("delete") || low.Contains("destroy") || low.Contains("remove") ||
            low.Contains("clear"))
            return RiskCategory.Destructive;
        if (low.Contains("post") || low.Contains("submit") || low.Contains("send"))
            return RiskCategory.Post;
        if (low.Contains("account") || low.Contains("password") || low.Contains("profile") ||
            low.Contains("auth") || low.Contains("login") || low.Contains("signup"))
            return RiskCategory.AccountChange;
        // scrittura generica: fill/click/set/change/write/save/evaluate
        return RiskCategory.Write;
    }

    // ── Classificazione rischio (v1: per nome tool/procedura) ─────────────────
    private static readonly string[] RiskyPrefixes =
    [
        "browser_download", "browser_set_cookie", "browser_set_user_agent",
        "browser_set_headers", "browser_set_fetch_headers", "browser_auth",
        "browser_block_urls", "browser_to_rag", "browser_profile",
        "browser_task_save", "browser_task_resume", "browser_audit_step", "browser_replay",
    ];

    public static bool IsRisky(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return false;
        foreach (var prefix in RiskyPrefixes)
            if (name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                return true;
        return McpToolRegistry.Instance.Get(name) is null;
    }

    public AgentVerdict Evaluate(AgentRecord agent, string toolName)
    {
        if (agent is null || string.IsNullOrWhiteSpace(toolName)) return AgentVerdict.Deny;

        if (agent.ToolPolicies.TryGetValue(toolName, out var policy))
        {
            return policy switch
            {
                "allow" => AgentVerdict.Allow,
                "deny" => AgentVerdict.Deny,
                "approve" => AgentVerdict.Approve,
                _ => AgentVerdict.Allow
            };
        }

        // G4: GOVERNED profile → ogni non-read richiede approval
        var cat = Classify(toolName);
        if (agent.GovernanceProfile == 2 && cat != RiskCategory.Read)
        {
            // allowance per PermissionLevel
            if (agent.PermissionLevel == AgentPermissionLevel.YOLO) return AgentVerdict.Allow;
            if (agent.PermissionLevel == AgentPermissionLevel.Sandbox) return AgentVerdict.Deny;
            return AgentVerdict.Approve;
        }

        if (!IsRisky(toolName))
            return AgentVerdict.Allow;

        return agent.PermissionLevel switch
        {
            AgentPermissionLevel.YOLO => AgentVerdict.Allow,
            AgentPermissionLevel.Sandbox => AgentVerdict.Deny,
            AgentPermissionLevel.Supervised => AgentVerdict.Approve,
            _ => AgentVerdict.Allow
        };
    }

    // ── Pending-approval (Supervised) ──────────────────────────────────────────
    /// <summary>Richiede l'approvazione e ATTENDE la decisione (con timeout, fail-closed).
    /// Ritorna true se approvato. Blocca il chiamante (ASK) — mai un worker GPM.</summary>
    public bool RequestApprovalAndWait(AgentRecord agent, string toolName, int timeoutMs = 30000)
    {
        string id;
        var signal = new ManualResetEventSlim(false);
        lock (_lock)
        {
            id = (_nextId++).ToString(System.Globalization.CultureInfo.InvariantCulture);
            _pending[id] = new PendingApproval(id, agent.Name, toolName, DateTimeOffset.Now, signal);
        }
        AgentEventLog.Instance.Record(AgentEventKind.PermissionRequested, agent.Name,
            $"tool={toolName} approval={id}");

        var approved = false;
        try
        {
            approved = signal.Wait(timeoutMs);
        }
        catch
        {
            approved = false;
        }
        finally
        {
            lock (_lock) _pending.Remove(id);
        }

        if (!approved)
            AgentEventLog.Instance.Record(AgentEventKind.PermissionDenied, agent.Name,
                $"tool={toolName} approval={id} (timeout)");
        return approved;
    }

    /// <summary>Decide una pending-approval (dal frontend/web). Ritorna false se l'id non esiste.</summary>
    public bool Decide(string id, bool allow, string? note = null)
    {
        PendingApproval? p;
        lock (_lock)
        {
            if (!_pending.TryGetValue(id, out p)) return false;
            _pending.Remove(id);
        }
        AgentEventLog.Instance.Record(allow ? AgentEventKind.PermissionApproved : AgentEventKind.PermissionDenied,
            p.AgentName, $"tool={p.ToolName} approval={id}{(note is null ? "" : $" note={note}")}");
        try { p.Signal.Set(); return true; }
        catch { return false; }
    }

    /// <summary>Snapshot delle approval in attesa (per il web frontend).</summary>
    public IReadOnlyList<PendingApprovalView> PendingApprovals()
    {
        lock (_lock)
            return _pending.Values.Select(p => new PendingApprovalView(p.Id, p.AgentName, p.ToolName, p.RequestedAt))
                .ToList();
    }

    public sealed record PendingApproval(string Id, string AgentName, string ToolName, DateTimeOffset RequestedAt, ManualResetEventSlim Signal);

    public sealed record PendingApprovalView(string Id, string AgentName, string ToolName, DateTimeOffset RequestedAt);
}
