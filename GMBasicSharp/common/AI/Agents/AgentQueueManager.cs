// FILE: Basic.Agents.AgentQueueManager
// L1 — Queue/ACF — AgentQueueManager FIFO per agente + AgentMessage + WorkflowFabric
// Shape: AgentQueue[agent="Analista"] = FIFO [Task1, Task2] (max 1000)
//        AgentFabric{agent_id, inbox chan [100], outbox chan [100], control chan [10]}
//        AgentMessage{Id,From,To,Task,Input,Payment,Timestamp}
//        WorkflowFabric{Id,Steps,State}

namespace Basic.Agents;

/// <summary>Messaggio per la coda agente (ACF). JSON-serializzabile per debug.</summary>
public sealed record AgentMessage(
    string Id,
    string From,
    string To,
    string Task,
    string Input,
    double Payment,
    DateTime Timestamp)
{
    public static AgentMessage Create(string from, string to, string task, string input, double payment = 0) =>
        new(Guid.NewGuid().ToString("N"), from, to, task, input, payment, DateTime.UtcNow);

    public string ToJson() => System.Text.Json.JsonSerializer.Serialize(this);
    public static AgentMessage? FromJson(string json)
    {
        try { return System.Text.Json.JsonSerializer.Deserialize<AgentMessage>(json); } catch { return null; }
    }
}

/// <summary>Stato workflow fabric (per L1/L4). Non è WorkflowServer: è l'involucro ACF.</summary>
public sealed class WorkflowFabric
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");
    public List<string> Steps { get; set; } = [];
    public string State { get; set; } = "Idle"; // Idle, Running, Paused, Stopped, Completed, Failed
}

/// <summary>Manager FIFO per agente — coda in ingresso per task. Thread-safe, max 1000 per agente.
/// Usato da <c>AI AGENT SEND/RECEIVE/STOP/PAUSE/RESUME</c> e da <c>GO SELECT</c> coordinator.</summary>
public sealed class AgentQueueManager
{
    public static AgentQueueManager Instance { get; } = new();

    private const int MaxQueueSize = 1000;
    private readonly Dictionary<string, Queue<AgentMessage>> _queues = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, bool> _paused = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, bool> _stopped = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _lock = new();

    private AgentQueueManager() { }

    public bool QueueCreate(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (_queues.ContainsKey(agent)) return false;
            _queues[agent] = new Queue<AgentMessage>();
            _paused[agent] = false;
            _stopped[agent] = false;
        }
        return true;
    }

    public bool Enqueue(string agent, AgentMessage message)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        ArgumentNullException.ThrowIfNull(message);
        lock (_lock)
        {
            if (!_queues.TryGetValue(agent, out var q))
            {
                q = new Queue<AgentMessage>();
                _queues[agent] = q;
                _paused[agent] = false;
                _stopped[agent] = false;
            }
            if (_stopped.TryGetValue(agent, out var stopped) && stopped) return false;
            if (q.Count >= MaxQueueSize) return false;
            q.Enqueue(message);
        }
        return true;
    }

    public bool Enqueue(string agent, string from, string task, string input, double payment = 0) =>
        Enqueue(agent, AgentMessage.Create(from, agent, task, input, payment));

    public AgentMessage? Dequeue(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.TryGetValue(agent, out var q) || q.Count == 0) return null;
            if (_paused.TryGetValue(agent, out var paused) && paused) return null;
            if (_stopped.TryGetValue(agent, out var stopped) && stopped) return null;
            return q.Dequeue();
        }
    }

    /// <summary>Dequeue bloccante con timeout (per GO SELECT). Ritorna null su timeout o pausa.</summary>
    public AgentMessage? DequeueBlocking(string agent, int timeoutMs = 5000)
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            var msg = Dequeue(agent);
            if (msg != null) return msg;
            System.Threading.Thread.Sleep(10);
        }
        return null;
    }

    public string QueueStats(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.TryGetValue(agent, out var q))
                return $"{{\"agent\":\"{agent}\",\"count\":0,\"paused\":false,\"stopped\":false}}";
            var paused = _paused.TryGetValue(agent, out var p) && p;
            var stopped = _stopped.TryGetValue(agent, out var s) && s;
            return $"{{\"agent\":\"{agent}\",\"count\":{q.Count},\"paused\":{paused.ToString().ToLowerInvariant()},\"stopped\":{stopped.ToString().ToLowerInvariant()}}}";
        }
    }

    public bool QueueClear(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.TryGetValue(agent, out var q)) return false;
            q.Clear();
        }
        return true;
    }

    /// <summary>Azzera tutte le code/pausa/stop (isolamento tra test).</summary>
    public void ClearAll()
    {
        lock (_lock) { _queues.Clear(); _paused.Clear(); _stopped.Clear(); }
    }

    public bool Pause(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.ContainsKey(agent)) return false;
            _paused[agent] = true;
        }
        return true;
    }

    public bool Resume(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.ContainsKey(agent)) return false;
            _paused[agent] = false;
        }
        return true;
    }

    public bool Stop(string agent)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agent);
        lock (_lock)
        {
            if (!_queues.ContainsKey(agent)) return false;
            _stopped[agent] = true;
            _queues[agent].Clear();
        }
        return true;
    }

    public bool IsPaused(string agent)
    {
        lock (_lock) return _paused.TryGetValue(agent, out var p) && p;
    }

    public bool IsStopped(string agent)
    {
        lock (_lock) return _stopped.TryGetValue(agent, out var s) && s;
    }

    public int Count(string agent)
    {
        lock (_lock) return _queues.TryGetValue(agent, out var q) ? q.Count : 0;
    }

    internal static void ResetForTests()
    {
        lock (Instance._lock)
        {
            Instance._queues.Clear();
            Instance._paused.Clear();
            Instance._stopped.Clear();
        }
    }
}
