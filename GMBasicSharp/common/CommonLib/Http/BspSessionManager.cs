using System.Collections.Concurrent;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace CommonLib.Http;

/// <summary>Snapshot serializzabile di una sessione (persistenza esterna).</summary>
public readonly record struct SessionSnapshot(string DataJson, long LastAccessUtcTicks);

/// <summary>
/// Persistenza esterna delle sessioni (backend opzionali, 2026-09-02): memory è il default
/// dentro <see cref="BspSessionManager"/>; un'implementazione SQLite (RDD-SQL, sempre presente)
/// vive in BRT. Il gestore fa snapshot periodici + salvataggio su create/destroy/cleanup.
/// </summary>
public interface ISessionPersistence
{
    IReadOnlyDictionary<string, SessionSnapshot> Load();
    void Save(IReadOnlyDictionary<string, SessionSnapshot> all);
}

/// <summary>
/// Gestore sessioni per BASIC Scripting Pages. Basato su cookie GMBSESSION con ID random.
/// Thread-safe (ConcurrentDictionary), timeout configurabile, pulizia periodica.
/// Persistenza opzionale: se fornita (ISessionPersistence), carica lo snapshot all'avvio e
/// salva periodicamente (timer) + a ogni create/destroy/cleanup.
/// </summary>
public sealed class BspSessionManager : IDisposable
{
    private readonly ConcurrentDictionary<string, SessionEntry> _sessions = new();
    private readonly TimeSpan _timeout;
    private readonly Timer _cleanupTimer;
    private readonly ISessionPersistence? _persistence;
    private readonly Timer? _persistTimer;
    private readonly object _persistLock = new();

    public BspSessionManager(TimeSpan? timeout = null, ISessionPersistence? persistence = null)
    {
        _timeout = timeout ?? TimeSpan.FromMinutes(30);
        _persistence = persistence;
        _cleanupTimer = new Timer(CleanupExpired, null, TimeSpan.FromMinutes(5), TimeSpan.FromMinutes(5));
        if (_persistence != null)
        {
            LoadPersisted();
            _persistTimer = new Timer(_ => Persist(), null, TimeSpan.FromSeconds(2), TimeSpan.FromSeconds(2));
        }
    }

    public void Dispose()
    {
        try { _cleanupTimer.Dispose(); } catch { }
        try { _persistTimer?.Dispose(); } catch { }
        Persist();
    }

    /// <summary>Scrive subito lo snapshot su persistenza (se configurata) — utile nei test.</summary>
    public void Flush() => Persist();

    private void LoadPersisted()
    {
        if (_persistence == null) return;
        try
        {
            var now = DateTime.UtcNow;
            foreach (var kv in _persistence.Load())
            {
                try
                {
                    var node = JsonNode.Parse(kv.Value.DataJson);
                    var data = node as JsonObject ?? new JsonObject();
                    var last = DateTime.FromFileTimeUtc(kv.Value.LastAccessUtcTicks);
                    if (now - last <= _timeout)
                        _sessions[kv.Key] = new SessionEntry(data, last, last);
                }
                catch { /* sessione corrotta: ignorata */ }
            }
        }
        catch { /* file non leggibile: si parte vuoti */ }
    }

    private void Persist()
    {
        var p = _persistence;
        if (p == null) return;
        try
        {
            var snap = new Dictionary<string, SessionSnapshot>(StringComparer.Ordinal);
            var now = DateTime.UtcNow;
            foreach (var kv in _sessions)
            {
                if (now - kv.Value.LastAccess > _timeout) continue;
                string json;
                lock (kv.Value.Sync)
                {
                    json = kv.Value.Data.ToJsonString();
                }
                snap[kv.Key] = new SessionSnapshot(json, kv.Value.LastAccess.ToFileTimeUtc());
            }
            lock (_persistLock)
            {
                p.Save(snap);
            }
        }
        catch { /* persistenza best-effort */ }
    }

    /// <summary>Crea una nuova sessione e restituisce il cookie da inviare al client.</summary>
    public (string SessionId, string CookieValue) CreateSession()
    {
        var id = GenerateId();
        _sessions[id] = new SessionEntry(new JsonObject(), DateTime.UtcNow, DateTime.UtcNow);
        Persist();
        return (id, $"GMBSESSION={id}; Path=/; HttpOnly; SameSite=Lax");
    }

    /// <summary>Recupera i dati di una sessione esistente, o null se scaduta/inesistente.</summary>
    public JsonObject? GetSession(string sessionId)
    {
        if (_sessions.TryGetValue(sessionId, out var entry))
        {
            if (DateTime.UtcNow - entry.LastAccess > _timeout)
            {
                _sessions.TryRemove(sessionId, out _);
                Persist();
                return null;
            }
            entry.LastAccess = DateTime.UtcNow;
            return entry.Data;
        }
        return null;
    }

    /// <summary>Ottiene o crea una sessione dal cookie di richiesta.</summary>
    public (string SessionId, string? SetCookie, JsonObject Data) GetOrCreate(string? cookieHeader)
    {
        var existingId = ExtractSessionId(cookieHeader);
        if (existingId != null)
        {
            var data = GetSession(existingId);
            if (data != null)
                return (existingId, null, data);
        }
        var (id, cookie) = CreateSession();
        return (id, cookie, GetSession(id)!);
    }

    /// <summary>Cancella una sessione (logout).</summary>
    public void Destroy(string sessionId)
    {
        if (_sessions.TryRemove(sessionId, out _))
            Persist();
    }

    /// <summary>Numero di sessioni attive.</summary>
    public int Count => _sessions.Count;

    private static string GenerateId()
    {
        return Guid.NewGuid().ToString("N");
    }

    private static string? ExtractSessionId(string? cookieHeader)
    {
        if (string.IsNullOrEmpty(cookieHeader)) return null;
        var parts = cookieHeader.Split(';');
        foreach (var part in parts)
        {
            var trimmed = part.Trim();
            if (trimmed.StartsWith("GMBSESSION=", StringComparison.OrdinalIgnoreCase))
                return trimmed["GMBSESSION=".Length..].Trim();
        }
        return null;
    }

    private void CleanupExpired(object? state)
    {
        var now = DateTime.UtcNow;
        var removed = false;
        foreach (var kvp in _sessions)
        {
            if (now - kvp.Value.LastAccess > _timeout)
            {
                _sessions.TryRemove(kvp.Key, out _);
                removed = true;
            }
        }
        if (removed) Persist();
    }

    internal sealed class SessionEntry
    {
        public JsonObject Data { get; }
        public object Sync { get; } = new();
        public DateTime Created { get; }
        public DateTime LastAccess { get; set; }

        public SessionEntry(JsonObject data, DateTime created, DateTime lastAccess)
        {
            Data = data;
            Created = created;
            LastAccess = lastAccess;
        }
    }
}
