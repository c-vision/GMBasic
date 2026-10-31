// FILE: Basic.Agents.AgentEventLog
namespace Basic.Agents;

public enum AgentEventKind
{
    Created,
    Deleted,
    Asked,
    ToolCalled,
    TeamCreated,
    TeamDeleted,
    TeamMemberAdded,
    TeamMemberRemoved,
    TeamMessage,
    PermissionRequested,
    PermissionApproved,
    PermissionDenied
}

public sealed record AgentEvent(DateTimeOffset Timestamp, AgentEventKind Kind, string AgentName, string Detail,
    string PrevHash = "", string Hash = "");

/// <summary>In-process ring buffer of agent lifecycle events, plus optional trace-to-stderr —
/// G10: hash-chain audit (SHA256 prevHash+payload → link antimanomissione).</summary>
public sealed class AgentEventLog
{
    private const int MaxEvents = 500;

    public static AgentEventLog Instance { get; } = new();

    private readonly Lock _lock = new();
    private readonly LinkedList<AgentEvent> _events = new();
    private string _lastHash = "";

    private AgentEventLog()
    {
    }

    /// <summary>TRACE ON: events are recorded and echoed to stderr. TRACE OFF: recording stops
    /// entirely (no ring-buffer overhead for agents nobody is watching). TRACE VERBOSE: recorded
    /// and echoed with full detail (prompt/response text) instead of just the event kind.</summary>
    public bool TraceEnabled { get; private set; }

    public bool VerboseEnabled { get; private set; }

    public void SetMode(string mode)
    {
        switch ((mode ?? string.Empty).Trim().ToUpperInvariant())
        {
            case "ON":
                TraceEnabled = true;
                VerboseEnabled = false;
                break;
            case "VERBOSE":
                TraceEnabled = true;
                VerboseEnabled = true;
                break;
            case "OFF":
                TraceEnabled = false;
                VerboseEnabled = false;
                break;
            default:
                throw new ArgumentException($"Unknown TRACE mode '{mode}'. Expected ON, OFF or VERBOSE.");
        }
    }

    public void Record(AgentEventKind kind, string agentName, string detail)
    {
        if (!TraceEnabled)
            return;

        var ts = DateTimeOffset.UtcNow;
        // G10: hash-chain — SHA256(prevHash + payload)
        var payload = $"{ts:O}|{kind}|{agentName}|{detail}";
        string hash, prevHash;
        lock (_lock)
        {
            prevHash = _lastHash;
            hash = Sha256(_lastHash + "|" + payload);
            _lastHash = hash;
        }
        var evt = new AgentEvent(ts, kind, agentName, detail, prevHash, hash);
        lock (_lock)
        {
            _events.AddLast(evt);
            while (_events.Count > MaxEvents)
                _events.RemoveFirst();
        }

        Console.Error.WriteLine(Format(evt, VerboseEnabled));
    }

    /// <summary>G10: verifica la catena di hash. Ritorna true se ogni evento riferisce
    /// il hash corretto del precedente (nessuna rottura/manomissione).</summary>
    public bool VerifyChain()
    {
        lock (_lock)
        {
            string prev = "";
            foreach (var e in _events)
            {
                var payload = $"{e.Timestamp:O}|{e.Kind}|{e.AgentName}|{e.Detail}";
                var expectedPrev = prev;
                if (e.Hash != Sha256(prev + "|" + payload)) return false;
                if (!string.IsNullOrEmpty(e.PrevHash) && e.PrevHash != expectedPrev) return false;
                prev = e.Hash;
            }
            return true;
        }
    }

    /// <summary>G10: esporta la catena come testo (per evidence bundle).</summary>
    public string ExportChain()
    {
        var sb = new System.Text.StringBuilder();
        lock (_lock)
        {
            foreach (var e in _events)
                sb.AppendLine($"{e.Hash} {e.Timestamp:O} {e.Kind} agent={e.AgentName} {e.Detail}");
        }
        return sb.ToString();
    }

    private static string Sha256(string input)
    {
        var bytes = System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(input));
        return System.Convert.ToHexString(bytes).ToLowerInvariant();
    }

    public IReadOnlyList<AgentEvent> Snapshot()
    {
        lock (_lock)
            return [.._events];
    }

    public void Clear()
    {
        lock (_lock)
            _events.Clear();
    }

    public static string Format(AgentEvent e, bool verbose) =>
        verbose
            ? $"[AGENT {e.Kind}] {e.Timestamp:O} agent={e.AgentName} {e.Detail}"
            : $"[AGENT {e.Kind}] agent={e.AgentName}";

    internal static void ResetForTests()
    {
        Instance.Clear();
        Instance.TraceEnabled = false;
        Instance.VerboseEnabled = false;
    }
}
