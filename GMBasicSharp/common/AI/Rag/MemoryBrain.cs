// FILE: Basic.AI.MemoryBrain
// Second brain condiviso: memoria persistente su SQLite (<configDir>/brain.db) con
// embedding vettoriale (provider embedding attivo) + fallback lessicale. Accessibile da
// tutti gli agenti e i team. Fase 1 del piano GMBASIC-Second-Brain-Piano.
using System.Text;
using System.Text.Json;
using CommonLib;
using Microsoft.Data.Sqlite;

namespace Basic.AI;

public static class MemoryBrain
{
    public enum MemoryKind { Fact, Note, Episodic }

    public static string DbPath => GmbiPaths.BrainDbFile;

    private static SqliteConnection Open()
    {
        var dir = Path.GetDirectoryName(DbPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        // Pooling=False: una connessione poolata a un DB eliminato (es. ResetForTests)
        // produrrebbe 'readonly database'. Il brain ha traffico basso.
        var conn = new SqliteConnection($"Data Source={DbPath};Pooling=False");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            CREATE TABLE IF NOT EXISTS memories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                text TEXT NOT NULL,
                source TEXT,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                embedding TEXT
            );
            CREATE TABLE IF NOT EXISTS watched (
                path TEXT PRIMARY KEY,
                last_hash TEXT
            );
            CREATE TABLE IF NOT EXISTS links (
                from_path TEXT NOT NULL,
                to_name TEXT NOT NULL,
                PRIMARY KEY (from_path, to_name)
            );
            """;
        cmd.ExecuteNonQuery();
        return conn;
    }

    /// <summary>Aggiunge una memoria (fatto/nota/episodica) con embedding vettoriale.</summary>
    public static long Add(string text, MemoryKind kind, string? source = null, int dimensions = 0)
    {
        if (string.IsNullOrWhiteSpace(text)) return -1;
        var emb = TryEmbed(text, dimensions);
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "INSERT INTO memories(kind, text, source, embedding) VALUES($k,$t,$s,$e); SELECT last_insert_rowid();";
        cmd.Parameters.AddWithValue("$k", kind.ToString());
        cmd.Parameters.AddWithValue("$t", text);
        cmd.Parameters.AddWithValue("$s", (object?)source ?? DBNull.Value);
        cmd.Parameters.AddWithValue("$e", emb is null ? DBNull.Value : JsonSerializer.Serialize(emb));
        return (long)(cmd.ExecuteScalar() ?? -1L);
    }

    public static bool Forget(long id)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "DELETE FROM memories WHERE id=$id;";
        cmd.Parameters.AddWithValue("$id", id);
        return cmd.ExecuteNonQuery() > 0;
    }

    /// <summary>Import esplicito di un file: .md/.txt come testo, immagini via OCR.</summary>
    public static long ImportFile(string path, int dimensions = 0)
    {
        var ext = Path.GetExtension(path);
        if (IsTextDoc(ext))
        {
            var content = File.ReadAllText(path);
            return string.IsNullOrWhiteSpace(content) ? -1 : Add(content, MemoryKind.Note, source: path, dimensions);
        }
        if (IsImage(ext))
        {
            var text = TryOcr(path);
            return string.IsNullOrWhiteSpace(text) ? -1 : Add(text!, MemoryKind.Note, source: path, dimensions);
        }
        return -1;
    }

    /// <summary>Retrieval: top-k memorie per similarita' (cosine sull'embedding, fallback
    /// lessicale se manca il provider embedding). Formato: id|kind|testo.</summary>
    public static string Search(string query, int topK = 5, int dimensions = 0)
    {
        var q = TryEmbed(query, dimensions);
        using var conn = Open();
        var rows = new List<(long Id, string Kind, string Text, float[]? Emb)>();
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT id, kind, text, embedding FROM memories;";
            using var r = cmd.ExecuteReader();
            while (r.Read())
            {
                var emb = r.IsDBNull(3) ? null : JsonSerializer.Deserialize<float[]>(r.GetString(3));
                rows.Add((r.GetInt64(0), r.GetString(1), r.GetString(2), emb));
            }
        }

        var scored = rows
            .Select(row => (Row: row, Score: Score(row, q, query)))
            .Where(x => x.Score > 0)
            .OrderByDescending(x => x.Score)
            .Take(topK)
            .ToList();

        var sb = new StringBuilder();
        foreach (var (row, _) in scored)
            sb.AppendLine($"{row.Id}|{row.Kind}|{row.Text}");
        return sb.ToString().TrimEnd();
    }

    private static double Score((long Id, string Kind, string Text, float[]? Emb) row, float[]? queryEmb, string query)
    {
        if (queryEmb is { Length: > 0 } q && row.Emb is { Length: > 0 } e && q.Length == e.Length)
            return Cosine(q, e);
        return row.Text.Contains(query, StringComparison.OrdinalIgnoreCase) ? 0.5 : 0;
    }

    private static double Cosine(float[] a, float[] b)
    {
        double dot = 0, na = 0, nb = 0;
        for (var i = 0; i < a.Length; i++)
        {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }
        if (na <= 0 || nb <= 0) return 0;
        return dot / (Math.Sqrt(na) * Math.Sqrt(nb));
    }

    private static float[]? TryEmbed(string text, int dimensions)
    {
        try
        {
            var dims = dimensions > 0 ? dimensions : Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
            return dims > 0 ? Basic.AI.Rag.AiVectorRuntime.Embed(text, dims) : null;
        }
        catch { return null; }
    }

    // ── Cartelle osservate (watch folder) ──────────────────────────────────────

    public static IReadOnlyList<string> Watched()
    {
        using var conn = Open();
        var list = new List<string>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT path FROM watched;";
        using var r = cmd.ExecuteReader();
        while (r.Read()) list.Add(r.GetString(0));
        return list;
    }

    public static void AddWatch(string path)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "INSERT OR IGNORE INTO watched(path, last_hash) VALUES($p, '');";
        cmd.Parameters.AddWithValue("$p", path);
        cmd.ExecuteNonQuery();
    }

    /// <summary>Riscansiona le cartelle osservate e indicizza i file cambiati (hash del
    /// contenuto): .md/.txt come note (con wiki-link → grafo), immagini (png/jpg/gif/webp)
    /// via OCR se il provider visione e' attivo. Ritorna quanti nuovi documenti ingeriti.</summary>
    public static int RefreshWatched(int dimensions = 0)
    {
        var count = 0;
        foreach (var path in Watched())
        {
            if (!Directory.Exists(path)) continue;
            foreach (var file in Directory.GetFiles(path, "*", SearchOption.AllDirectories))
            {
                var ext = Path.GetExtension(file);
                if (!IsTextDoc(ext) && !IsImage(ext)) continue;
                var hash = ComputeHash(file);
                var last = GetLastHash(file);
                if (hash == last) continue;
                try
                {
                    if (IsTextDoc(ext))
                    {
                        var content = File.ReadAllText(file);
                        if (!string.IsNullOrWhiteSpace(content) && Add(content, MemoryKind.Note, source: file, dimensions) > 0)
                        {
                            foreach (var link in ExtractWikiLinks(content))
                                AddLink(file, link);
                            count++;
                        }
                    }
                    else
                    {
                        var text = TryOcr(file);
                        if (!string.IsNullOrWhiteSpace(text) && Add(text, MemoryKind.Note, source: file, dimensions) > 0)
                            count++;
                        else if (text is null) continue; // nessun OCR disponibile: non marcare come indicizzato
                    }
                    SetLastHash(file, hash);
                }
                catch { /* file illeggibile: salta */ }
            }
        }
        return count;
    }

    private static bool IsTextDoc(string ext) =>
        ext.EndsWith(".md", StringComparison.OrdinalIgnoreCase) || ext.EndsWith(".txt", StringComparison.OrdinalIgnoreCase);

    private static bool IsImage(string ext) =>
        ext is ".png" or ".jpg" or ".jpeg" or ".gif" or ".webp" or ".bmp";

    /// <summary>OCR di un'immagine tramite il provider visione attivo (ruolo ocr). Ritorna
    /// il testo estratto, o null se nessun provider visione e' disponibile.</summary>
    private static string? TryOcr(string file)
    {
        try
        {
            var server = Basic.Providers.ProviderServer.Instance;
            if (string.IsNullOrEmpty(server.GetActiveName("ocr"))) return null;
            var instance = server.GetActiveCompletion("ocr");
            if (instance is not Basic.AI.Rag.IVisionCompleter vision) return null;
            var bytes = File.ReadAllBytes(file);
            var format = Path.GetExtension(file).TrimStart('.').ToLowerInvariant();
            if (format == "jpg") format = "jpeg";
            var text = vision.DescribeImage(bytes, format, "Extract all text from this image.");
            return string.IsNullOrWhiteSpace(text) ? null : text;
        }
        catch { return null; }
    }

    /// <summary>Estrae i wiki-link Obsidian ([[Nome]] o [[Nome|alias]]) da un contenuto md.</summary>
    public static IReadOnlyList<string> ExtractWikiLinks(string content)
    {
        var list = new List<string>();
        foreach (System.Text.RegularExpressions.Match m in
                 System.Text.RegularExpressions.Regex.Matches(content, @"\[\[([^\]|]+)(?:\|[^\]]+)?\]\]"))
            list.Add(m.Groups[1].Value.Trim());
        return list;
    }

    // ── Grafo relazionale (sync Obsidian) ──────────────────────────────────────

    public static void AddLink(string fromPath, string toName)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "INSERT OR IGNORE INTO links(from_path, to_name) VALUES($f,$t);";
        cmd.Parameters.AddWithValue("$f", fromPath);
        cmd.Parameters.AddWithValue("$t", toName);
        cmd.ExecuteNonQuery();
    }

    public static IReadOnlyList<(string From, string To)> Links()
    {
        using var conn = Open();
        var list = new List<(string, string)>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT from_path, to_name FROM links;";
        using var r = cmd.ExecuteReader();
        while (r.Read()) list.Add((r.GetString(0), r.GetString(1)));
        return list;
    }

    private static string GetLastHash(string file)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT last_hash FROM watched WHERE path=$p;";
        cmd.Parameters.AddWithValue("$p", file);
        return (cmd.ExecuteScalar() as string) ?? "";
    }

    private static void SetLastHash(string file, string hash)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "UPDATE watched SET last_hash=$h WHERE path=$p;";
        cmd.Parameters.AddWithValue("$h", hash);
        cmd.Parameters.AddWithValue("$p", file);
        cmd.ExecuteNonQuery();
    }

    private static string ComputeHash(string file)
    {
        using var sha = System.Security.Cryptography.SHA256.Create();
        var bytes = sha.ComputeHash(File.ReadAllBytes(file));
        return Convert.ToHexString(bytes);
    }

    /// <summary>Azzera lo stato (test): rimuove il file del brain.</summary>
    internal static void ResetForTests()
    {
        try { File.Delete(DbPath); } catch { }
    }
}
