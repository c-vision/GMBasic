// FILE: Basic.AI.Knowledge.NoteManager
// Second Brain — L2 Note: CRUD di note .md dentro un vault (folder tree Obsidian).
// Ogni nota ha path relativo (normalizzato con '/'), frontmatter (YAML --- / TOML +++),
// title (primo H1 o nome file), tags, word_count, content. Riusa FrontmatterParser.
// Operazioni: CreatePath/GetContentByPath/UpdateAppend/DeletePath/RenamePath/MovePath/
// ExistsByPath/GetTitle/GetTags/GetWordCount/GetRelativePath/GetFolderPath/Breadcrumb.
using Basic.AI.Graph;

namespace Basic.AI.Knowledge;

public sealed record NoteRecord(
    string Path,           // path relativo normalizzato (es. "research/ai/transformers.md")
    string Title,          // primo H1 (# X) o nome file senza estensione
    string Content,        // body (senza frontmatter)
    Dictionary<string, List<string>> Frontmatter,
    List<string> Tags,     // da frontmatter tags
    long WordCount);

public static class NoteManager
{
    /// <summary>Root del vault (directory). Ritorna null se il vault non è registrato.
    /// Usa il VaultPath (sorgente originale) se disponibile, altrimenti la root import.
    ///
    /// F4 — se <paramref name="vaultName"/> è vuoto il vault si risolve nello SCOPE (catena
    /// `PROCESS` senza scope esplicito): l'ultimo vault creato nella sessione, altrimenti **l'unico**
    /// cervello dello scope. Con **più** cervelli si lancia un errore ESPLICITO (mai una scelta
    /// implicita del «primo brain used»: era non deterministica, dipendeva dall'ordine di `AI BRAIN USE`).</summary>
    public static string? VaultRoot(string vaultName, BrainScope? scope = null)
    {
        if (string.IsNullOrWhiteSpace(vaultName))
        {
            vaultName = VaultManager.LastCreated ?? "";
            if (string.IsNullOrWhiteSpace(vaultName))
            {
                var effective = (scope ?? BrainScope.FromRegistry()).Effective();
                if (effective.Count == 1)
                {
                    vaultName = effective[0].Name;
                }
                else if (effective.Count > 1)
                {
                    throw new InvalidOperationException(
                        $"vault ambiguo: {effective.Count} cervelli nello scope "
                        + $"({string.Join(", ", effective.Select(r => r.ToString()))}): specifica il nome del vault "
                        + "(es. `AI NOTE CREATE <vault>, <path>, …` oppure `AI VAULT CREATE <vault> …`).");
                }
            }
        }
        var info = BrainRegistry.Info(vaultName);
        if (info is { VaultPath: not null } && Directory.Exists(info.VaultPath)) return info.VaultPath;
        var brainRoot = BrainRegistry.BrainRoot(vaultName);
        if (brainRoot is not null) return brainRoot;
        // fallback: vault registrato in VaultManager (non necessariamente un brain)
        var vaultInfo = VaultManager.VaultInfo(vaultName);
        if (!string.IsNullOrEmpty(vaultInfo))
        {
            try
            {
                using var doc = System.Text.Json.JsonDocument.Parse(vaultInfo);
                var root = doc.RootElement.GetProperty("rootPath").GetString();
                if (!string.IsNullOrEmpty(root) && Directory.Exists(root)) return root;
            }
            catch { /* vault info non valida */ }
        }
        return null;
    }

    /// <summary>Converte un path relativo in un path assoluto dentro la root del vault.
    /// Normalizza le barre, blocca i path che escono dalla root (chroot-style).</summary>
    public static string? ResolvePath(string vaultName, string relPath)
    {
        var root = VaultRoot(vaultName);
        if (root is null) return null;
        var norm = relPath.Replace('\\', '/').TrimStart('/');
        var full = Path.GetFullPath(Path.Combine(root, norm));
        var rootFull = Path.GetFullPath(root);
        if (!full.StartsWith(rootFull, StringComparison.OrdinalIgnoreCase)) return null; // zip/escape
        return full;
    }

    /// <summary>True se la nota esiste nel vault (path relativo).</summary>
    public static bool ExistsByPath(string vaultName, string relPath)
        => ResolvePath(vaultName, relPath) is string full && File.Exists(full);

    /// <summary>Legge una nota (frontmatter + body). Ritorna null se non esiste.</summary>
    public static NoteRecord? GetByPath(string vaultName, string relPath)
    {
        if (ResolvePath(vaultName, relPath) is not string full || !File.Exists(full)) return null;
        try { return FromText(relPath, File.ReadAllText(full)); }
        catch { return null; }
    }

    /// <summary>Contenuto (body) della nota, o null.</summary>
    public static string? GetContentByPath(string vaultName, string relPath)
        => GetByPath(vaultName, relPath)?.Content;

    /// <summary>Crea (o sovrascrive) una nota. Auto-mkdirs del path. Se il testo non inizia con
    /// frontmatter, aggiunge un blocco YAML con title/tags quando forniti. Ritorna il path relativo.</summary>
    public static string? CreatePath(string vaultName, string relPath, string content, List<string>? tags = null)
    {
        if (ResolvePath(vaultName, relPath) is not string full) return null;
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var text = EnsureFrontmatter(content, relPath, tags);
        File.WriteAllText(full, text);
        return relPath;
    }

    /// <summary>Aggiorna il body della nota (preserva il frontmatter esistente).</summary>
    public static bool UpdateByPath(string vaultName, string relPath, string content)
    {
        var existing = GetByPath(vaultName, relPath);
        if (existing is null) return false;
        var fm = existing.Frontmatter;
        var text = RebuildWithFrontmatter(fm, content);
        if (ResolvePath(vaultName, relPath) is not string full) return false;
        File.WriteAllText(full, text);
        return true;
    }

    /// <summary>Appende testo al body della nota.</summary>
    public static bool UpdateAppend(string vaultName, string relPath, string append)
    {
        var existing = GetByPath(vaultName, relPath);
        if (existing is null) return false;
        return UpdateByPath(vaultName, relPath, existing.Content + (append ?? ""));
    }

    /// <summary>Cancella la nota. Ritorna true se esisteva.</summary>
    public static bool DeletePath(string vaultName, string relPath)
    {
        if (ResolvePath(vaultName, relPath) is not string full) return false;
        if (!File.Exists(full)) return false;
        File.Delete(full);
        return true;
    }

    /// <summary>Rinomina/sposta una nota (nuovo path relativo).</summary>
    public static bool MovePath(string vaultName, string oldRel, string newRel)
    {
        if (ResolvePath(vaultName, oldRel) is not string src || !File.Exists(src)) return false;
        if (ResolvePath(vaultName, newRel) is not string dst) return false;
        Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
        File.Move(src, dst, overwrite: true);
        return true;
    }

    // ── Letture ────────────────────────────────────────────────────────────

    public static string? GetTitle(string vaultName, string relPath)
        => GetByPath(vaultName, relPath)?.Title;

    public static string? GetTags(string vaultName, string relPath)
    {
        var n = GetByPath(vaultName, relPath);
        return n is null ? null : string.Join(",", n.Tags);
    }

    public static long GetWordCount(string vaultName, string relPath)
        => GetByPath(vaultName, relPath)?.WordCount ?? -1;

    /// <summary>Path relativo normalizzato (re-lativo alla root). Ritorna il path in input se
    /// il vault non è risolvibile.</summary>
    public static string RelativePath(string vaultName, string relPath) => relPath.Replace('\\', '/');

    public static string FolderPath(string vaultName, string relPath)
    {
        var p = relPath.Replace('\\', '/');
        var i = p.LastIndexOf('/');
        return i > 0 ? p[..i] : "/";
    }

    public static string Breadcrumb(string vaultName, string relPath)
    {
        var p = relPath.Replace('\\', '/');
        var i = p.LastIndexOf('/');
        return i > 0 ? p[..i].Replace('/', '.') : ".";
    }

    // ── Helpers ────────────────────────────────────────────────────────────

    private static NoteRecord FromText(string relPath, string text)
    {
        var fm = FrontmatterParser.Parse(text);
        var title = ExtractTitle(fm.Body) ?? System.IO.Path.GetFileNameWithoutExtension(relPath);
        var tags = fm.GetAll("tags").ToList();
        return new NoteRecord(relPath, title, fm.Body, fm.Fields, tags, CountWords(fm.Body));
    }

    private static string? ExtractTitle(string body)
    {
        foreach (var line in body.Split('\n'))
        {
            var t = line.Trim();
            if (t.StartsWith("# "))
                return t[2..].Trim();
        }
        return null;
    }

    private static long CountWords(string body)
    {
        long n = 0;
        foreach (var w in body.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries))
            if (!string.IsNullOrWhiteSpace(w)) n++;
        return n;
    }

    private static string EnsureFrontmatter(string content, string relPath, List<string>? tags)
    {
        if (content.StartsWith("---") || content.StartsWith("+++")) return content;
        var title = ExtractTitle(content) ?? System.IO.Path.GetFileNameWithoutExtension(relPath);
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("---");
        sb.AppendLine($"title: \"{title}\"");
        if (tags is { Count: > 0 })
            sb.AppendLine("tags: [" + string.Join(", ", tags.Select(t => $"\"{t}\"")) + "]");
        sb.AppendLine("---");
        sb.Append(content);
        return sb.ToString();
    }

    private static string RebuildWithFrontmatter(Dictionary<string, List<string>> fm, string body)
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("---");
        foreach (var (k, v) in fm)
        {
            if (v.Count == 1) sb.AppendLine($"{k}: \"{v[0]}\"");
            else sb.AppendLine($"{k}: [" + string.Join(", ", v.Select(x => $"\"{x}\"")) + "]");
        }
        sb.AppendLine("---");
        sb.Append(body);
        return sb.ToString();
    }
}
