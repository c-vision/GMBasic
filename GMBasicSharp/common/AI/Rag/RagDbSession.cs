// FILE: Basic.AI.Rag.RagDbSession
// Apre/gestisce un database SQLite RAG su file e lo registra nel DataSession condiviso
// (AiVectorRuntime.SqlDataSession), così REPL/CLI/test possono usare AiVectorRuntime con un
// alias senza passare dal BASIC. Riusabile ovunque.
using Basic.Databases.Rdd;
using Basic.Providers;
using Basic.SQLite.Vector;

namespace Basic.AI.Rag;

/// <summary>Handle di un db RAG aperto: alias + path.</summary>
public sealed record RagDb(string Alias, string DbPath);

public static class RagDbSession
{
    /// <summary>Alias runtime di default per un db RAG: basename del file senza estensione.
    /// Più db RAG possono coesistere nella stessa sessione (DataSession condiviso) perché ogni
    /// alias è unico per db — il vecchio alias fisso "RagDb" collideva aprendo più di un db.
    /// L'utente può sovrascrivere con un alias esplicito quando serve (--alias).</summary>
    public static string DefaultAlias(string dbPath)
    {
        var name = Path.GetFileNameWithoutExtension(dbPath);
        if (string.IsNullOrWhiteSpace(name)) name = "rag";
        return name;
    }
    /// <summary>Apre (o riusa) un db SQLite su file con l'alias dato e lo registra nel DataSession
    /// condiviso. Ritorna l'alias. Se il db esiste già, lo apre in modalità append (WAL).</summary>
    public static RagDb Open(string dbPath, string alias)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(dbPath));
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);

        var ds = AiVectorRuntime.SqlDataSession;
        if (ds is null)
        {
            ds = new DataSession();
            AiVectorRuntime.SqlDataSession = ds;
        }

        // se l'alias è già aperto, lo riusa
        try { if (ds.FindAreaByAlias(alias) != 0) return new RagDb(alias, dbPath); } catch { /* riapre */ }

        var driver = new SqlDriver();
        driver.Open(new DbOpenInfo
        {
            FileName = $"Data Source={Path.GetFullPath(dbPath)}",
            // L'alias NON è il nome tabella: questo driver apre un db RAG con schema vec_*
            // (AIVECTOR.*). Con Alias = alias, SqlDriver.LoadTableData farebbe "SELECT * FROM
            // RagDb" → "no such table: RagDb" (stesso fix di IlRuntime.SqlOpen/PCodeVM). L'area
            // viene registrata sotto l'alias reale qui sotto.
            Alias = "",
            Driver = "SQLite",
        });
        var area = ds.AllocateArea();
        ds.RegisterDirect(area, driver, alias);
        return new RagDb(alias, dbPath);
    }

    /// <summary>Abilita WAL sul db (letture concorrenti durante un build).</summary>
    public static void EnableWal(string dbPath)
    {
        using var conn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={Path.GetFullPath(dbPath)}");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "PRAGMA journal_mode=WAL;";
        cmd.ExecuteNonQuery();
    }

    /// <summary>Popola <see cref="EmbeddingProviderResolver"/> dal provider embedding attivo di
    /// ProviderServer (REPL/CLI, che non passano da IlRuntime dove questo avviene per i programmi
    /// BASIC). Carica prima <c>providers.json</c> (ProviderConfig.Load — il server non lo fa da
    /// solo) così i provider registrati dall'utente (llamacpp/mlx/ollama) sono disponibili.
    /// Preferisce un provider non-fixture, RISPETTANDO la scelta esplicita dell'utente: se
    /// l'attivo corrente (activeIndex in providers.json / AI SET ACTIVE) è già un provider reale
    /// e disponibile, lo usa SENZA sovrascriverlo col primo dell'order. Solo se l'attivo è il
    /// fixture (o non disponibile) attiva il primo non-fixture dell'order (il round-robin di
    /// default sceglierebbe comunque il primo Available, spesso il fixture). Ritorna l'embedding
    /// provider attivo, o null se nessuno è registrato/healthy.</summary>
    public static IEmbeddingProvider? EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer server)
    {
        try { Basic.Providers.ProviderConfig.Load(); }
        catch (Exception ex) { Console.Error.WriteLine($"?provider config non caricata: {ex.Message}"); }

        // Preferisci un provider embedding NON-fixture (llamacpp/mlx/ollama/openai). RISPETTA la
        // scelta esplicita dell'utente (activeIndex/GetActiveName): se l'attivo corrente è già
        // un provider reale e disponibile, non lo sovrascrive con il primo non-fixture dell'order
        // (bug 2026-09-07: il SetActive forzato ignorava activeIndex — es. utente su mlx-emb ma
        // il build usava il primo non-fixture dell'order). Solo se l'attivo è il fixture (o non
        // disponibile) fallisce al primo non-fixture dell'order.
        SelectEmbeddingActive(server);

        IEmbeddingProvider? emb;
        try { emb = server.GetActiveEmbedding(); }
        catch { return null; } // nessun provider embedding registrato/healthy
        if (emb is null) return null;

        // P0a2 (decisione utente 2026-09-12): il fixture NON è una risoluzione accettabile.
        // Se l'attivo è il fixture, ritorniamo null (e NON lo installiamo nel resolver):
        // i chiamanti REPL/CLI validano con ValidateEmbeddingProvider() e fermano l'operazione
        // con l'errore che nomina il rimedio. Prima: il fixture veniva adottato silenziosamente
        // e il null ritornato era scartato dai 4 chiamanti.
        if (emb.GetType().Name.Contains("Fixture", StringComparison.OrdinalIgnoreCase))
            return null;

        // Provider reale → adotta nel resolver e la sua dimensione.
        EmbeddingProviderResolver.Default = emb;
        EmbeddingProviderResolver.ActiveDimensions = server.ActiveDimensions;
        return emb;
    }

    /// <summary>Selezione del provider embedding attivo per il RAG: rispetta la scelta esplicita
    /// dell'utente (GetActiveName/activeIndex) se è già un provider reale e disponibile; solo se
    /// l'attivo è il fixture (o non disponibile) attiva il primo non-fixture dell'order. Esposto
    /// internal per il test (il chiamante pubblico fa prima ProviderConfig.Load).</summary>
    internal static void SelectEmbeddingActive(Basic.Providers.ProviderServer server)
    {
        var currentName = server.GetActiveName(ProviderRole.Embedding);
        bool currentIsReal = currentName is not null
            && !currentName.StartsWith("fixture", StringComparison.OrdinalIgnoreCase)
            && server.GetStatus(currentName) == ProviderStatus.Available;
        if (currentIsReal) return;

        foreach (var name in server.List(ProviderRole.Embedding))
        {
            if (name.StartsWith("fixture", StringComparison.OrdinalIgnoreCase)) continue;
            if (server.GetStatus(name) != ProviderStatus.Available) continue;
            try { server.SetActive(ProviderRole.Embedding, name); break; }
            catch { /* ignora e prova il prossimo */ }
        }
    }
}
