// FILE: Basic.AI.Knowledge.BrainPackager
// Second Brain — EXPORT (BRAIN EXPORT, §6.1quater del piano): genera lo ZIP .gmbrain di un
// cervello esistente (già costruito da RAG BUILD/GRAPH BUILD o importato), con
// brain.manifest.json come firma (formato/checksum, §6.1ex). Lo zip si sposta fra macchine e
// si ri-importa con BRAIN INSTALL. La costruzione del cervello NON è compito di questo modulo
// (REPL/CLI la fanno); qui si impacchetta SOLO ciò che è già su disco.
using CommonLib;

namespace Basic.AI.Knowledge;

public sealed record BrainPackResult(string ZipPath, string Name, List<string> Available, string ManifestPath);

public static class BrainPackager
{
    /// <summary>Esporta il cervello <paramref name="name"/> in <paramref name="outZip"/>.
    /// Raccoglie rag/&lt;name&gt;.db + .state.json e/o graphs/&lt;name&gt;.* secondo
    /// AvailableModes, calcola gli sha256 e scrive brain.manifest.json dentro lo zip.
    /// Default outZip: AiConfig.BrainsExportsDir/&lt;name&gt;.gmbrain.zip.</summary>
    public static BrainPackResult? Export(string name, string? outZip = null)
    {
        var available = BrainRegistry.AvailableModes(name);
        if (available.Length == 0) return null;

        var root = BrainRegistry.BrainRoot(name);
        if (root is null) return null;

        // staging in /tmp: raccogli i file effettivi (import o legacy) in una struttura canonica
        var staging = Path.Combine(Path.GetTempPath(), "gmbrain-export-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(staging, "rag"));
        Directory.CreateDirectory(Path.Combine(staging, "graphs"));

        var manifest = new BrainManifest
        {
            Name = name,
            Mode = available.Length == 2 ? "both" : available[0],
            CreatedAt = DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'"),
            Available = [.. available],
            Docs = CountDocs(root),
            SignedBy = "gmb-export",
        };

        var files = new List<string>();

        // rag
        if (BrainRegistry.RagDbPath(name) is string ragDb)
        {
            var relDb = Path.Combine(staging, "rag", name + ".db");
            Copy(ragDb, relDb);
            files.Add(relDb);
            manifest.Files["rag/" + name + ".db"] = BrainManifest.HashFile(relDb);
            var state = Path.ChangeExtension(ragDb, ".state.json");
            if (File.Exists(state))
            {
                var relState = Path.Combine(staging, "rag", name + ".state.json");
                Copy(state, relState);
                files.Add(relState);
                manifest.Files["rag/" + name + ".state.json"] = BrainManifest.HashFile(relState);
            }
        }

        // graphs
        if (BrainRegistry.GraphJsonPath(name) is string graphJson)
        {
            var graphDir = Path.GetDirectoryName(graphJson);
            if (graphDir is not null)
            {
                foreach (var ext in new[] { "json", "graphml", "mermaid", "stats.json", "state.json" })
                {
                    var src = Path.Combine(graphDir, name + "." + ext);
                    if (!File.Exists(src)) continue;
                    var rel = Path.Combine(staging, "graphs", name + "." + ext);
                    Copy(src, rel);
                    files.Add(rel);
                    manifest.Files["graphs/" + name + "." + ext] = BrainManifest.HashFile(rel);
                }
            }
        }

        var manifestPath = Path.Combine(staging, "brain.manifest.json");
        File.WriteAllText(manifestPath, manifest.ToJson(withChecksum: true));
        files.Add(manifestPath);

        var zipPath = outZip ?? Path.Combine(AiConfig.BrainsExportsDir, name + ".gmbrain.zip");
        var dir = Path.GetDirectoryName(Path.GetFullPath(zipPath));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        CompressionUtils.CreateZip(zipPath, files);

        try { Directory.Delete(staging, true); } catch { }

        return new BrainPackResult(zipPath, name, [.. available], manifestPath);
    }

    private static void Copy(string src, string dst)
    {
        var dir = Path.GetDirectoryName(dst);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.Copy(src, dst, overwrite: true);
    }

    private static long CountDocs(string root)
    {
        try
        {
            return Directory.EnumerateFiles(root, "*.md", SearchOption.AllDirectories).LongCount()
                + Directory.EnumerateFiles(root, "*.txt", SearchOption.AllDirectories).LongCount();
        }
        catch { return 0; }
    }
}
