// FILE: Basic.Agents.OrchestrationPersistence — Orchestrazione agenti: persistenza su disco.
// Salva/riprende lo stato dei registri di orchestrazione (Artifact, Handoff, Metrics,
// Checkpoint, Replay) in un database SQLite su file (tabella KV `orch_kv`). Il file è condiviso
// con il runtime C (stesso schema/format), così lo stato sopravvive al riavvio ed è scambiabile
// tra i due mondi. (Il VectorDB/RAG è orientato a embedding+chunking e non è adatto a stato KV.)
using Microsoft.Data.Sqlite;

namespace Basic.Agents;

/// <summary>Persistenza su disco (SQLite) dello stato dei registri di orchestrazione.</summary>
public static class OrchestrationPersistence
{
    private static readonly (string Name, Func<string> Export, Action<string> Import, Action Clear)[] Stores =
    {
        ("artifact", ArtifactStore.ExportState, ArtifactStore.ImportState, ArtifactStore.Clear),
        ("handoff", HandoffRegistry.ExportState, HandoffRegistry.ImportState, () => HandoffRegistry.Clear()),
        ("metrics", MetricsRegistry.ExportState, MetricsRegistry.ImportState, () => MetricsRegistry.Reset()),
        ("checkpoint", CheckpointRegistry.ExportState, CheckpointRegistry.ImportState, () => CheckpointRegistry.Clear()),
        ("replay", ReplayRegistry.ExportState, ReplayRegistry.ImportState, () => ReplayRegistry.Clear()),
        ("category", Basic.AI.Providers.CategoryRegistry.ExportState, Basic.AI.Providers.CategoryRegistry.ImportState, Basic.AI.Providers.CategoryRegistry.Clear),
        ("resource", ResourceRegistry.ExportState, ResourceRegistry.ImportState, ResourceRegistry.Clear),
        ("rules", RuleRegistry.ExportState, RuleRegistry.ImportState, () => RuleRegistry.Clear()),
        ("task", TaskGraphRegistry.ExportState, TaskGraphRegistry.ImportState, TaskGraphRegistry.Clear),
        ("plan", PlanRegistry.ExportState, PlanRegistry.ImportState, PlanRegistry.Clear),
        ("hook", HookRegistry.ExportState, HookRegistry.ImportState, HookRegistry.Clear),
    };

    /// <summary>Salva lo stato corrente in <paramref name="path"/> (SQLite). Ritorna il numero di
    /// scope salvati. **Non lancia mai**: su qualunque errore (path vuoto, I/O, SQLite) ritorna 0.
    /// 0 = fallimento, &gt;0 = scope salvati.</summary>
    public static double Save(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return 0.0;
        try
        {
            var dir = System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path));
            if (!string.IsNullOrEmpty(dir)) System.IO.Directory.CreateDirectory(dir);
            using var conn = new SqliteConnection($"Data Source={path}");
            conn.Open();
            Exec(conn, "CREATE TABLE IF NOT EXISTS orch_kv(scope TEXT PRIMARY KEY, state TEXT NOT NULL)");
            var n = 0;
            foreach (var s in Stores)
            {
                using var cmd = conn.CreateCommand();
                cmd.CommandText = "INSERT INTO orch_kv(scope,state) VALUES($s,$v) ON CONFLICT(scope) DO UPDATE SET state=$v";
                cmd.Parameters.AddWithValue("$s", s.Name);
                cmd.Parameters.AddWithValue("$v", s.Export());
                if (cmd.ExecuteNonQuery() >= 0) n++;
            }
            return n;
        }
        catch { return 0.0; }
    }

    /// <summary>Riprende lo stato da <paramref name="path"/> (i registri sono SOSTITUITI).
    /// **Non lancia mai**: ritorna il numero di scope ripristinati; 0 se il file non esiste o su
    /// qualunque errore (I/O, SQLite).</summary>
    public static double Load(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !System.IO.File.Exists(path)) return 0.0;
        try
        {
            using var conn = new SqliteConnection($"Data Source={path}");
            conn.Open();
            Exec(conn, "CREATE TABLE IF NOT EXISTS orch_kv(scope TEXT PRIMARY KEY, state TEXT NOT NULL)");
            var n = 0;
            foreach (var s in Stores)
            {
                string? v = null;
                using (var cmd = conn.CreateCommand())
                {
                    cmd.CommandText = "SELECT state FROM orch_kv WHERE scope=$s";
                    cmd.Parameters.AddWithValue("$s", s.Name);
                    v = cmd.ExecuteScalar() as string;
                }
                s.Clear();
                if (v != null) { s.Import(v); n++; }
            }
            return n;
        }
        catch { return 0.0; }
    }

    private static void Exec(SqliteConnection conn, string sql)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        cmd.ExecuteNonQuery();
    }
}
