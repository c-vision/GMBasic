namespace CommonLib.Http;

/// <summary>
/// Store di processo condiviso per TUTTE le sessioni HTTP (route e pagine BSP). Default:
/// in-memory. Sostituibile a runtime (HTTP CONFIG .SessionFile) con un backend persistente
/// (es. SQLite/RDD-SQL in BRT) — la sostituzione è atomica e vale per i manager già ripuntati.
/// </summary>
public static class SessionStores
{
    private static readonly object Lock = new();
    private static BspSessionManager _default = new();

    /// <summary>Manager attivo di processo.</summary>
    public static BspSessionManager Default => _default;

    /// <summary>Sostituisce il manager (nuovo backend). Il vecchio viene disposto.</summary>
    public static void Configure(ISessionPersistence? persistence)
    {
        lock (Lock)
        {
            var old = _default;
            _default = new BspSessionManager(null, persistence);
            try { old.Dispose(); } catch { }
        }
    }
}
