// FILE: Basic.AI.Knowledge.VaultManager
// Second Brain — L1 Vault (structure filesystem): un vault = folder tree dove `.md`=note,
// folders=organizzazione, paths=navigazione (`files over app`, compatibilita' Obsidian).
// Persistenza su <configDir>/vaults.db (tabelle `vaults` + `vault_files`) — configDir
// configurabile in system.settings.json sezione "paths" (default <root>/config).
// W1 del piano AI-Second-Brain-Vision: SOLO `CREATE RECURSE` + `INDEX` (incrementale per
// content_hash) + letture (LIST_FOLDERS, GET_INDEXED_FILE_COUNT, INFO, LIST).
// Frontmatter (W2), chunking, linking, graph, sync, chroot jail sono fasi successive.
using System.Text.Json;
using CommonLib;
using Microsoft.Data.Sqlite;

namespace Basic.AI.Knowledge;

public static class VaultManager
{
    public static string DbPath => GmbiPaths.VaultsDbFile;

    private static SqliteConnection Open()
    {
        var dir = Path.GetDirectoryName(DbPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        // Pooling=False: stessa convenzione di MemoryBrain (brain.db) — un DB eliminato
        // da ResetForTests non deve produrre 'readonly database'. Traffico basso.
        var conn = new SqliteConnection($"Data Source={DbPath};Pooling=False");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            CREATE TABLE IF NOT EXISTS vaults (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                root_path TEXT NOT NULL,
                pattern TEXT NOT NULL,
                index_version INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                indexed_at TEXT
            );
            CREATE TABLE IF NOT EXISTS vault_files (
                vault_id INTEGER NOT NULL,
                path TEXT NOT NULL,
                folder_path TEXT NOT NULL,
                content_hash TEXT NOT NULL,
                indexed_at TEXT NOT NULL DEFAULT (datetime('now')),
                PRIMARY KEY (vault_id, path)
            );
            CREATE TABLE IF NOT EXISTS vault_folders (
                vault_id INTEGER NOT NULL,
                folder_path TEXT NOT NULL,
                indexed INTEGER NOT NULL DEFAULT 1,
                tags TEXT NOT NULL DEFAULT '',
                PRIMARY KEY (vault_id, folder_path)
            );
            CREATE TABLE IF NOT EXISTS note_links (
                vault_id INTEGER NOT NULL,
                from_path TEXT NOT NULL,
                to_path TEXT NOT NULL,
                alias TEXT,
                PRIMARY KEY (vault_id, from_path, to_path)
            );
            """;
        cmd.ExecuteNonQuery();
        return conn;
    }

    /// <summary>Registra un vault (root_path + pattern) e lo indicizza subito.
    /// Upsert per nome: ricreare lo stesso nome aggiorna root/pattern e re-indicizza.
    /// Ritorna l'id del vault, o -1 se il path non esiste.</summary>
    /// <summary>Ultimo vault creato (fallback del vault attivo per i comandi NOTE, parità runtime C).</summary>
    public static string LastCreated { get; private set; } = "";

    public static long VaultCreate(string name, string rootPath, string? pattern = null)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(rootPath)) return -1;
        LastCreated = name;
        var full = Path.GetFullPath(rootPath);
        if (!Directory.Exists(full)) return -1;
        var pat = string.IsNullOrWhiteSpace(pattern) ? "*.md" : pattern;
        using var conn = Open();
        long id;
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = """
                INSERT INTO vaults(name, root_path, pattern)
                VALUES($n,$r,$p)
                ON CONFLICT(name) DO UPDATE SET root_path=$r, pattern=$p;
                SELECT id FROM vaults WHERE name=$n;
                """;
            cmd.Parameters.AddWithValue("$n", name);
            cmd.Parameters.AddWithValue("$r", full);
            cmd.Parameters.AddWithValue("$p", pat);
            id = (long)(cmd.ExecuteScalar() ?? -1L);
        }
        IndexVaultCore(conn, id, full, pat);
        return id;
    }

    /// <summary>Re-indicizza un vault per nome (incrementale: aggiorna i file cambiati,
    /// rimuove i file spariti). Ritorna il numero di file indicizzati, -1 se vault ignoto.</summary>
    public static long VaultIndex(string name)
    {
        using var conn = Open();
        long id; string root = "", pat = "";
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT id, root_path, pattern FROM vaults WHERE name=$n;";
            cmd.Parameters.AddWithValue("$n", name);
            using var r = cmd.ExecuteReader();
            if (!r.Read()) return -1;
            id = r.GetInt64(0); root = r.GetString(1); pat = r.GetString(2);
        }
        return IndexVaultCore(conn, id, root, pat);
    }

    /// <summary>Reindicizza UN file del vault (upsert hash). Usato dal FileWatcher on-change:
    /// il file esiste → aggiorna/inserisce la voce; il file non esiste → rimuove la voce stale.
    /// Ritorna 1 se indicizzato, 0 se rimosso, -1 se vault ignoto.</summary>
    public static long IndexFile(string name, string filePath)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return -1;
        var full = Path.GetFullPath(filePath);
        if (File.Exists(full))
        {
            var root = GetRoot(conn, vaultId);
            var hash = ComputeHash(full);
            using var cmd = conn.CreateCommand();
            cmd.CommandText = """
                INSERT INTO vault_files(vault_id, path, folder_path, content_hash)
                VALUES($v,$p,$f,$h)
                ON CONFLICT(vault_id, path) DO UPDATE SET content_hash=$h, indexed_at=datetime('now');
                """;
            cmd.Parameters.AddWithValue("$v", vaultId);
            cmd.Parameters.AddWithValue("$p", full);
            cmd.Parameters.AddWithValue("$f", FolderOf(root, full));
            cmd.Parameters.AddWithValue("$h", hash);
            cmd.ExecuteNonQuery();
            return 1;
        }
        // file sparito → rimuovi la voce stale (DELETE/rename fuori dal vault)
        using (var del = conn.CreateCommand())
        {
            del.CommandText = "DELETE FROM vault_files WHERE vault_id=$v AND path=$p;";
            del.Parameters.AddWithValue("$v", vaultId);
            del.Parameters.AddWithValue("$p", full);
            del.ExecuteNonQuery();
        }
        return 0;
    }

    /// <summary>Rimuove la voce di un file dall'indice (usato dal watcher su Delete/Rename).</summary>
    public static long RemoveFile(string name, string filePath)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return -1;
        using var del = conn.CreateCommand();
        del.CommandText = "DELETE FROM vault_files WHERE vault_id=$v AND path=$p;";
        del.Parameters.AddWithValue("$v", vaultId);
        del.Parameters.AddWithValue("$p", Path.GetFullPath(filePath));
        return del.ExecuteNonQuery();
    }

    private static string GetRoot(SqliteConnection conn, long vaultId)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT root_path FROM vaults WHERE id=$v;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        return cmd.ExecuteScalar() as string ?? "";
    }

    // ── Folder props (L1): AddFolder/RemoveFolder/SetFolderProps/ListFolderProps ──
    // Subvault e props per-cartella (INDEX bool, TAGS string) come Folder-Enhancement.md:54.
    // Ritorna 1 su OK, -1 se vault ignoto o cartella non esistente.

    /// <summary>Registra una cartella del vault (subvault) con props. La cartella deve esistere
    /// sotto la root (guard zip/escape).</summary>
    public static long AddFolder(string name, string folderPath, bool indexed = true, string tags = "")
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return -1;
        var root = GetRoot(conn, vaultId);
        var full = Path.GetFullPath(Path.Combine(root, folderPath.Replace('\\', '/')));
        var rootFull = Path.GetFullPath(root);
        if (!full.StartsWith(rootFull + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            return -1; // zip/escape
        if (!Directory.Exists(full)) return -1;
        var rel = Path.GetRelativePath(rootFull, full).Replace('\\', '/');
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            INSERT INTO vault_folders(vault_id, folder_path, indexed, tags)
            VALUES($v,$f,$i,$t)
            ON CONFLICT(vault_id, folder_path) DO UPDATE SET indexed=$i, tags=$t;
            """;
        cmd.Parameters.AddWithValue("$v", vaultId);
        cmd.Parameters.AddWithValue("$f", rel);
        cmd.Parameters.AddWithValue("$i", indexed ? 1 : 0);
        cmd.Parameters.AddWithValue("$t", tags ?? "");
        cmd.ExecuteNonQuery();
        return 1;
    }

    /// <summary>Rimuove la voce di una cartella dal registro (non cancella i file).</summary>
    public static long RemoveFolder(string name, string folderPath)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return -1;
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "DELETE FROM vault_folders WHERE vault_id=$v AND folder_path=$f;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        cmd.Parameters.AddWithValue("$f", folderPath.Trim('/').Replace('\\', '/'));
        return cmd.ExecuteNonQuery();
    }

    /// <summary>Aggiorna le props di una cartella registrata (INDEX bool, TAGS string).</summary>
    public static long SetFolderProps(string name, string folderPath, bool? indexed = null, string? tags = null)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return -1;
        var rel = folderPath.Trim('/').Replace('\\', '/');
        using (var upd = conn.CreateCommand())
        {
            upd.CommandText = """
                UPDATE vault_folders
                SET indexed = COALESCE($i, indexed),
                    tags = COALESCE($t, tags)
                WHERE vault_id=$v AND folder_path=$f;
                """;
            upd.Parameters.AddWithValue("$i", indexed.HasValue ? (object)(indexed.Value ? 1 : 0) : DBNull.Value);
            upd.Parameters.AddWithValue("$t", tags is null ? DBNull.Value : tags);
            upd.Parameters.AddWithValue("$v", vaultId);
            upd.Parameters.AddWithValue("$f", rel);
            var n = upd.ExecuteNonQuery();
            if (n > 0) return 1;
        }
        // non esiste: inserisci con i default
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            INSERT OR IGNORE INTO vault_folders(vault_id, folder_path, indexed, tags)
            VALUES($v,$f,$i,$t);
            """;
        cmd.Parameters.AddWithValue("$v", vaultId);
        cmd.Parameters.AddWithValue("$f", rel);
        cmd.Parameters.AddWithValue("$i", indexed ?? true ? 1 : 0);
        cmd.Parameters.AddWithValue("$t", tags ?? "");
        cmd.ExecuteNonQuery();
        return 1;
    }

    /// <summary>JSON delle cartelle registrate: [{folderPath, indexed, tags}].</summary>
    public static string ListFolderProps(string name)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return "[]";
        var rows = new List<object>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT folder_path, indexed, tags FROM vault_folders WHERE vault_id=$v ORDER BY folder_path;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        using var r = cmd.ExecuteReader();
        while (r.Read())
            rows.Add(new { folderPath = r.GetString(0), indexed = r.GetInt64(1) != 0, tags = r.GetString(2) });
        return System.Text.Json.JsonSerializer.Serialize(rows);
    }

    private static long IndexVaultCore(SqliteConnection conn, long vaultId, string root, string pattern)
    {        var files = Directory.GetFiles(root, pattern, SearchOption.AllDirectories);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        long count = 0;
        foreach (var file in files)
        {
            var full = Path.GetFullPath(file);
            seen.Add(full);
            var hash = ComputeHash(full);
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = """
                    INSERT INTO vault_files(vault_id, path, folder_path, content_hash)
                    VALUES($v,$p,$f,$h)
                    ON CONFLICT(vault_id, path) DO UPDATE SET content_hash=$h, indexed_at=datetime('now');
                    """;
                cmd.Parameters.AddWithValue("$v", vaultId);
                cmd.Parameters.AddWithValue("$p", full);
                cmd.Parameters.AddWithValue("$f", FolderOf(root, full));
                cmd.Parameters.AddWithValue("$h", hash);
                cmd.ExecuteNonQuery();
            }
            count++;
        }
        // Rimuove i file spariti (risultato di un DELETE/rename fuori dal vault).
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT path FROM vault_files WHERE vault_id=$v;";
            cmd.Parameters.AddWithValue("$v", vaultId);
            using var r = cmd.ExecuteReader();
            var stale = new List<string>();
            while (r.Read()) { var p = r.GetString(0); if (!seen.Contains(p)) stale.Add(p); }
            foreach (var p in stale)
            {
                using var del = conn.CreateCommand();
                del.CommandText = "DELETE FROM vault_files WHERE vault_id=$v AND path=$p;";
                del.Parameters.AddWithValue("$v", vaultId);
                del.Parameters.AddWithValue("$p", p);
                del.ExecuteNonQuery();
            }
        }
        using (var upd = conn.CreateCommand())
        {
            upd.CommandText = "UPDATE vaults SET indexed_at=datetime('now'), index_version=index_version+1 WHERE id=$v;";
            upd.Parameters.AddWithValue("$v", vaultId);
            upd.ExecuteNonQuery();
        }
        return count;
    }

    /// <summary>JSON info del vault: {name, rootPath, pattern, fileCount, folderCount,
    /// indexVersion, indexedAt}. Stringa vuota se il vault non esiste.</summary>
    public static string VaultInfo(string name)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT name, root_path, pattern, index_version, indexed_at FROM vaults WHERE name=$n;";
        cmd.Parameters.AddWithValue("$n", name);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return "";
        var vaultId = GetVaultId(conn, name);
        var fileCount = CountFiles(conn, vaultId);
        var folderCount = CountFolders(conn, vaultId);
        using var doc = JsonDocument.Parse(JsonSerializer.Serialize(new
        {
            name = r.GetString(0),
            rootPath = r.GetString(1),
            pattern = r.GetString(2),
            indexVersion = r.GetInt64(3),
            indexedAt = r.IsDBNull(4) ? null : r.GetString(4),
            fileCount,
            folderCount
        }));
        return doc.RootElement.GetRawText();
    }

    /// <summary>JSON array delle cartelle del vault (distinct, relative con '/').
    /// Ordinate per numero di file discendente.</summary>
    public static string ListFolders(string name)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        if (vaultId < 0) return "[]";
        var folders = new List<string>();
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT DISTINCT folder_path FROM vault_files WHERE vault_id=$v ORDER BY folder_path;";
            cmd.Parameters.AddWithValue("$v", vaultId);
            using var r = cmd.ExecuteReader();
            while (r.Read()) folders.Add(r.GetString(0));
        }
        return JsonSerializer.Serialize(folders);
    }

    /// <summary>Numero di file indicizzati nel vault (-1 se vault ignoto).</summary>
    public static long GetIndexedFileCount(string name)
    {
        using var conn = Open();
        var vaultId = GetVaultId(conn, name);
        return vaultId < 0 ? -1 : CountFiles(conn, vaultId);
    }

    /// <summary>JSON array dei nomi dei vault registrati.</summary>
    public static string ListVaults()
    {
        using var conn = Open();
        var names = new List<string>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT name FROM vaults ORDER BY name;";
        using var r = cmd.ExecuteReader();
        while (r.Read()) names.Add(r.GetString(0));
        return JsonSerializer.Serialize(names);
    }

    private static long GetVaultId(SqliteConnection conn, string name)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT id FROM vaults WHERE name=$n;";
        cmd.Parameters.AddWithValue("$n", name);
        return (long)(cmd.ExecuteScalar() ?? -1L);
    }

    private static long CountFiles(SqliteConnection conn, long vaultId)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT COUNT(*) FROM vault_files WHERE vault_id=$v;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        return (long)(cmd.ExecuteScalar() ?? 0L);
    }

    private static long CountFolders(SqliteConnection conn, long vaultId)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT COUNT(DISTINCT folder_path) FROM vault_files WHERE vault_id=$v;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        return (long)(cmd.ExecuteScalar() ?? 0L);
    }

    private static string FolderOf(string root, string full)
    {
        var rel = Path.GetRelativePath(root, full);
        var dir = Path.GetDirectoryName(rel);
        return string.IsNullOrEmpty(dir) ? "/" : dir.Replace('\\', '/');
    }

    private static string ComputeHash(string file)
    {
        using var sha = System.Security.Cryptography.SHA256.Create();
        var bytes = sha.ComputeHash(File.ReadAllBytes(file));
        return Convert.ToHexString(bytes);
    }

    /// <summary>Azzera lo stato (test): rimuove il file del vault e lo stato process-wide
    /// (`LastCreated`, che altrimenti resta dal vault creato da un'altra suite → risoluzione del
    /// vault non deterministica, vedi F4 di Knowledge/Brain-Scope-Modello).</summary>
    internal static void ResetForTests()
    {
        try { File.Delete(DbPath); } catch { }
        LastCreated = "";
    }
}
