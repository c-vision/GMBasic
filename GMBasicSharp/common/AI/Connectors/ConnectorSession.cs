// FILE: Basic.Connectors.ConnectorSession
// Sessione per-connector (state, audit, rate limit) — parte del runtime F0.

namespace Basic.Connectors;

/// <summary>Stato di una sessione connettore (per-connector, non per-tool).</summary>
public sealed class ConnectorSession
{
    public string ConnectorName { get; }
    public DateTimeOffset ConnectedAt { get; } = DateTimeOffset.Now;
    public DateTimeOffset LastUsedAt { get; private set; } = DateTimeOffset.Now;
    public int CallCount { get; private set; }
    public int ErrorCount { get; private set; }
    public string? LastError { get; private set; }
    public bool IsConnected { get; private set; } = true;
    public Dictionary<string, string> State { get; } = new(StringComparer.OrdinalIgnoreCase);

    public ConnectorSession(string connectorName) => ConnectorName = connectorName;

    public void RecordCall(bool success, string? error = null)
    {
        CallCount++;
        LastUsedAt = DateTimeOffset.Now;
        if (!success) { ErrorCount++; LastError = error; }
        else LastError = null;
    }

    public void Disconnect() => IsConnected = false;
    public void SetState(string key, string value) => State[key] = value;
    public string? GetState(string key) => State.TryGetValue(key, out var v) ? v : null;
}

/// <summary>Manager sessioni connettori (singleton, per-connector, con rate limit).</summary>
public sealed class ConnectorSessionManager
{
    public static ConnectorSessionManager Instance { get; } = new();
    private readonly System.Collections.Concurrent.ConcurrentDictionary<string, ConnectorSession> _sessions = new(StringComparer.OrdinalIgnoreCase);
    private readonly System.Collections.Concurrent.ConcurrentDictionary<string, (int Count, DateTimeOffset WindowStart)> _rate = new(StringComparer.OrdinalIgnoreCase);
    private const int DefaultRateLimitPerMinute = 120;

    private ConnectorSessionManager() { }

    public ConnectorSession GetOrCreate(string connectorName) =>
        _sessions.GetOrAdd(connectorName, n => new ConnectorSession(n));

    public ConnectorSession? Get(string connectorName) =>
        _sessions.TryGetValue(connectorName, out var s) ? s : null;

    public void Remove(string connectorName)
    {
        _sessions.TryRemove(connectorName, out _);
        _rate.TryRemove(connectorName, out _);
    }

    /// <summary>Rate limit per-connector (fixed window, per minute). Ritorna false se superato.</summary>
    public bool TryAcquire(string connectorName, int limitPerMinute = DefaultRateLimitPerMinute)
    {
        var now = DateTimeOffset.Now;
        var entry = _rate.GetOrAdd(connectorName, _ => (0, now));
        var elapsed = now - entry.WindowStart;
        if (elapsed.TotalMinutes >= 1)
        {
            _rate[connectorName] = (1, now);
            return true;
        }
        if (entry.Count >= limitPerMinute) return false;
        _rate[connectorName] = (entry.Count + 1, entry.WindowStart);
        return true;
    }

    public void RecordCall(string connectorName, bool success, string? error = null)
    {
        var s = GetOrCreate(connectorName);
        s.RecordCall(success, error);
    }

    internal void ResetForTests()
    {
        _sessions.Clear();
        _rate.Clear();
    }
}
