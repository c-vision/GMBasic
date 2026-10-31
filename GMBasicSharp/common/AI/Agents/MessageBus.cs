// FILE: Basic.Agents.MessageBus
// L1 — MessageBus Pub/Sub per canali + bridge verso AgentQueueManager
// Usato da: AI CHANNEL, GO SELECT coordinator, TeamOrchestrator Broadcast

namespace Basic.Agents;

/// <summary>Bus Pub/Sub in-process per canali (string → lista subscriber).
/// Ogni subscriber è un handler <c>Action&lt;string channel, AgentMessage msg&gt;</c>.
/// Thread-safe, delivery sincrona (il chiamante blocca finché tutti gli handler hanno finito).</summary>
public sealed class MessageBus
{
    public static MessageBus Instance { get; } = new();

    private readonly Dictionary<string, List<Action<string, AgentMessage>>> _subscribers = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _lock = new();

    private MessageBus() { }

    public bool Subscribe(string channel, Action<string, AgentMessage> handler)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(channel);
        ArgumentNullException.ThrowIfNull(handler);
        lock (_lock)
        {
            if (!_subscribers.TryGetValue(channel, out var list))
            {
                list = [];
                _subscribers[channel] = list;
            }
            if (list.Contains(handler)) return false;
            list.Add(handler);
        }
        return true;
    }

    public bool Unsubscribe(string channel, Action<string, AgentMessage> handler)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(channel);
        ArgumentNullException.ThrowIfNull(handler);
        lock (_lock)
        {
            if (!_subscribers.TryGetValue(channel, out var list)) return false;
            var removed = list.Remove(handler);
            if (list.Count == 0) _subscribers.Remove(channel);
            return removed;
        }
    }

    public int Publish(string channel, AgentMessage message)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(channel);
        ArgumentNullException.ThrowIfNull(message);
        List<Action<string, AgentMessage>> snapshot;
        lock (_lock)
        {
            if (!_subscribers.TryGetValue(channel, out var list) || list.Count == 0) return 0;
            snapshot = [..list];
        }
        foreach (var h in snapshot)
        {
            try { h(channel, message); } catch { }
        }
        return snapshot.Count;
    }

    public int Publish(string channel, string from, string task, string input, double payment = 0) =>
        Publish(channel, AgentMessage.Create(from, channel, task, input, payment));

    public int SubscriberCount(string channel)
    {
        lock (_lock) return _subscribers.TryGetValue(channel, out var list) ? list.Count : 0;
    }

    public bool HasSubscribers(string channel)
    {
        lock (_lock) return _subscribers.TryGetValue(channel, out var list) && list.Count > 0;
    }

    public void Clear(string channel)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(channel);
        lock (_lock) _subscribers.Remove(channel);
    }

    internal static void ResetForTests()
    {
        lock (Instance._lock) Instance._subscribers.Clear();
    }
}
