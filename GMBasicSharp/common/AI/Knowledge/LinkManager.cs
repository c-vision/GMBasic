// FILE: Basic.AI.Knowledge.LinkManager
// Second Brain — L3 Linking: wikilink [[note|alias]] / [[note#heading]] dentro un vault.
// Estrae i link da ogni nota, li risolve contro l'indice dei file reali (WikiLinkParser),
// e calcola backlink (linked_from) + link uscenti (linked_to). Operazioni: LinksOut(path),
// Backlinks(path), AllLinks(), SetAlias (per la risoluzione dei path), ResolveLink.
// Riusa WikiLinkParser (common/AI/Graph) per regex+risoluzione.
using Basic.AI.Graph;

namespace Basic.AI.Knowledge;

public sealed record LinkInfo(
    string From,           // path relativo della nota sorgente
    string To,             // path relativo risolto della destinazione (o null → dangling)
    string? Alias,
    string? Heading,
    bool Resolved);        // false = dangling link

public static class LinkManager
{
    /// <summary>Tutti i file .md/.markdown del vault (path relativi normalizzati).</summary>
    public static IReadOnlyList<string> EnumerateNotes(string vaultName)
    {
        var root = NoteManager.VaultRoot(vaultName);
        if (root is null) return [];
        var list = new List<string>();
        foreach (var f in Directory.GetFiles(root, "*.md", SearchOption.AllDirectories))
            list.Add(WikiLinkParser.Normalize(Path.GetRelativePath(root, f)));
        foreach (var f in Directory.GetFiles(root, "*.markdown", SearchOption.AllDirectories))
        {
            var rel = WikiLinkParser.Normalize(Path.GetRelativePath(root, f));
            if (!list.Contains(rel, StringComparer.Ordinal)) list.Add(rel);
        }
        list.Sort(StringComparer.Ordinal);
        return list;
    }

    /// <summary>Link uscenti di una nota ([[...]] nel body), risolti contro l'indice.
    /// Ritorna lista di LinkInfo (con Resolved=false per i dangling). I link manuali
    /// (Link/SetAlias) si sommano ai wikilink estratti.</summary>
    public static List<LinkInfo> LinksOut(string vaultName, string path)
    {
        var root = NoteManager.VaultRoot(vaultName);
        var index = new HashSet<string>(EnumerateNotes(vaultName), StringComparer.Ordinal);
        var result = new List<LinkInfo>();
        if (root is null) return result;
        var content = NoteManager.GetContentByPath(vaultName, path);
        if (content is not null)
        {
            foreach (var link in WikiLinkParser.Extract(content))
            {
                var resolved = WikiLinkParser.Resolve(link.Target, index);
                result.Add(new LinkInfo(
                    WikiLinkParser.Normalize(path), resolved ?? link.Target,
                    link.Alias, link.Heading, resolved is not null));
            }
        }
        // link manuali (persistiti) — si sommano ai wikilink
        var norm = WikiLinkParser.Normalize(path);
        foreach (var m in ManualLinks(vaultName))
            if (m.From.Equals(norm, StringComparison.OrdinalIgnoreCase))
                result.Add(new LinkInfo(norm, m.To, m.Alias, null, index.Contains(m.To)));
        return result;
    }

    /// <summary>Crea (o aggiorna) un LINK manuale bidirezionale a→b (persistito in note_links).
    /// I link manuali si sommano ai wikilink estratti dal body. Ritorna 1 su OK.</summary>
    public static int Link(string vaultName, string fromPath, string toPath, string? alias = null)
    {
        var root = NoteManager.VaultRoot(vaultName);
        if (root is null) return -1;
        using var conn = OpenDb();
        var vaultId = GetVaultId(conn, vaultName);
        if (vaultId < 0) return -1;
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            INSERT INTO note_links(vault_id, from_path, to_path, alias)
            VALUES($v,$f,$t,$a)
            ON CONFLICT(vault_id, from_path, to_path) DO UPDATE SET alias=$a;
            """;
        cmd.Parameters.AddWithValue("$v", vaultId);
        cmd.Parameters.AddWithValue("$f", WikiLinkParser.Normalize(fromPath));
        cmd.Parameters.AddWithValue("$t", WikiLinkParser.Normalize(toPath));
        cmd.Parameters.AddWithValue("$a", alias is null ? DBNull.Value : alias);
        cmd.ExecuteNonQuery();
        return 1;
    }

    /// <summary>Rimuove un link manuale a→b. Ritorna 1 se rimosso, 0 se non esisteva.</summary>
    public static int Unlink(string vaultName, string fromPath, string toPath)
    {
        using var conn = OpenDb();
        var vaultId = GetVaultId(conn, vaultName);
        if (vaultId < 0) return -1;
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "DELETE FROM note_links WHERE vault_id=$v AND from_path=$f AND to_path=$t;";
        cmd.Parameters.AddWithValue("$v", vaultId);
        cmd.Parameters.AddWithValue("$f", WikiLinkParser.Normalize(fromPath));
        cmd.Parameters.AddWithValue("$t", WikiLinkParser.Normalize(toPath));
        return (int)cmd.ExecuteNonQuery();
    }

    /// <summary>Imposta l'alias di un link manuale (aggiorna se esiste, crea altrimenti).</summary>
    public static int SetAlias(string vaultName, string fromPath, string toPath, string alias)
    {
        var n = Unlink(vaultName, fromPath, toPath);
        if (n < 0) return -1;
        return Link(vaultName, fromPath, toPath, alias);
    }

    /// <summary>Link manuali persistiti del vault (nota_links), come (from,to,alias).</summary>
    public static List<(string From, string To, string? Alias)> ManualLinks(string vaultName)
    {
        var result = new List<(string, string, string?)>();
        try
        {
            using var conn = OpenDb();
            var vaultId = GetVaultId(conn, vaultName);
            if (vaultId < 0) return result;
            using var cmd = conn.CreateCommand();
            cmd.CommandText = "SELECT from_path, to_path, alias FROM note_links WHERE vault_id=$v;";
            cmd.Parameters.AddWithValue("$v", vaultId);
            using var r = cmd.ExecuteReader();
            while (r.Read())
                result.Add((r.GetString(0), r.GetString(1), r.IsDBNull(2) ? null : r.GetString(2)));
        }
        catch { /* best-effort: link manuali non disponibili → solo wikilink */ }
        return result;
    }

    /// <summary>Apre il db vaults (stessa DbPath di VaultManager).</summary>
    private static Microsoft.Data.Sqlite.SqliteConnection OpenDb()
    {
        var dir = Path.GetDirectoryName(VaultManager.DbPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var conn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={VaultManager.DbPath};Pooling=False");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
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

    /// <summary>id del vault per nome, o -1.</summary>
    private static long GetVaultId(Microsoft.Data.Sqlite.SqliteConnection conn, string name)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT id FROM vaults WHERE name=$n;";
        cmd.Parameters.AddWithValue("$n", name);
        return cmd.ExecuteScalar() is long l ? l : -1;
    }

    /// <summary>Backlink di una nota: le note che puntano a path (direttamente o per nome base).</summary>
    public static List<string> Backlinks(string vaultName, string path)
    {
        var target = WikiLinkParser.Normalize(path);
        var result = new List<string>();
        foreach (var note in EnumerateNotes(vaultName))
        {
            if (note.Equals(target, StringComparison.OrdinalIgnoreCase)) continue;
            foreach (var link in LinksOut(vaultName, note))
                if (link.Resolved && link.To.Equals(target, StringComparison.OrdinalIgnoreCase))
                {
                    result.Add(note);
                    break;
                }
        }
        result.Sort(StringComparer.Ordinal);
        return result;
    }

    /// <summary>Tutti i link del vault (da ogni nota). Ritorna LinkInfo di tutte le note.</summary>
    public static List<LinkInfo> AllLinks(string vaultName)
    {
        var result = new List<LinkInfo>();
        foreach (var note in EnumerateNotes(vaultName))
            result.AddRange(LinksOut(vaultName, note));
        return result;
    }

    /// <summary>Risolve un target di wikilink contro i file reali del vault. Ritorna il path
    /// relativo normalizzato, o null se dangling.</summary>
    public static string? ResolveLink(string vaultName, string target)
    {
        var index = new HashSet<string>(EnumerateNotes(vaultName), StringComparer.Ordinal);
        return WikiLinkParser.Resolve(target, index);
    }

    /// <summary>Conteggio dangling links (link a note inesistenti) nel vault.</summary>
    public static long DanglingCount(string vaultName)
    {
        long n = 0;
        foreach (var link in AllLinks(vaultName))
            if (!link.Resolved) n++;
        return n;
    }

    /// <summary>JSON con il grafo di linking del vault: {nodes, edges, dangling}. Coerente con
    /// GraphExporter.ToJson (per GraphStore.LoadFromJson).</summary>
    public static string ToJson(string vaultName)
    {
        var nodes = new List<object>();
        foreach (var n in EnumerateNotes(vaultName))
        {
            var title = WikiLinkParser.TitleOf(n, null);
            var fm = FrontmatterParser.Parse(File.ReadAllText(
                Path.Combine(NoteManager.VaultRoot(vaultName)!, n.Replace('/', Path.DirectorySeparatorChar))));
            nodes.Add(new { id = n, title, frontmatter = fm.Fields });
        }
        var edges = new List<object>();
        var dangling = new List<string>();
        foreach (var link in AllLinks(vaultName))
        {
            if (link.Resolved) edges.Add(new { from = link.From, to = link.To, alias = link.Alias });
            else dangling.Add($"{link.From} → {link.To}");
        }
        return System.Text.Json.JsonSerializer.Serialize(new { nodes, edges, dangling });
    }
}
