// FILE: Basic.AI.Knowledge.BrainSigner
// Second Brain — SIGN (BRAIN SIGN, §6.1ex2 del piano): firma e marca come valido un cervello
// FORNITO DALL'UTENTE (una cartella con file propri, NON generata da RAG BUILD/GRAPH BUILD),
// verificando con PROBE MINIMI se è utilizzabile come graph (nodi da .md/wikilink o grafo
// già pronto) o come vectordb (.db vettoriale interrogabile). Nessun embedding richiesto per
// la firma (valida la struttura, non la qualità semantica). Se nessun probe passa → nessuna
// firma + report esplicito. Prova anche a generare lo zip (--out).
using System.Text.Json;
using CommonLib;

namespace Basic.AI.Knowledge;

public sealed record BrainSignResult(string Name, List<string> Available, Dictionary<string, string> Probe, bool Valid);

public static class BrainSigner
{
    /// <summary>Esegue i probe minimi su <paramref name="dir"/> e, se almeno uno passa, firma il
    /// brain (registra in BrainRegistry con manifest). Ritorna l'esito con il report dei probe.</summary>
    public static BrainSignResult Sign(string name, string dir)
    {
        if (!Directory.Exists(dir))
            return new BrainSignResult(name, [], new Dictionary<string, string> { ["error"] = "cartella non trovata" }, false);

        var probe = new Dictionary<string, string>();

        // P2 — probe GRAPH: build in-memory su un campione (max 200 .md)
        var graphOk = ProbeGraph(dir, out var graphReport);
        probe["graph"] = graphReport;

        // P3 — probe VECTORDB: apre ogni .db/.sqlite, enumera vec_* / prova una query
        var ragOk = ProbeVectorDb(dir, out var ragReport);
        probe["vectordb"] = ragReport;

        var available = new List<string>();
        if (graphOk) available.Add("graph");
        if (ragOk) available.Add("rag");

        if (available.Count == 0)
            return new BrainSignResult(name, [], probe, false);

        // P4 — firma: manifest con available + sha256 dei file chiave + checksum
        var manifest = new BrainManifest
        {
            Name = name,
            Mode = available.Count == 2 ? "both" : available[0],
            CreatedAt = DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'"),
            Docs = CountMarkdown(dir),
            Available = available,
            SignedBy = "gmb-sign",
            Source = "user-provided",
            Probe = probe,
        };
        foreach (var f in SignatureFiles(dir, available))
        {
            if (File.Exists(f))
                manifest.Files[Path.GetRelativePath(dir, f).Replace('\\', '/')] = BrainManifest.HashFile(f);
        }

        var installRoot = Path.Combine(AiConfig.BrainsDefaultDir, name);
        Directory.CreateDirectory(installRoot);

        // registra anche i file chiave nella root import (così il brain è disponibile come rag/graph)
        var ragSrc = FindDb(dir);
        if (ragOk && ragSrc is not null)
        {
            var ragDst = Path.Combine(installRoot, "rag", name + ".db");
            Directory.CreateDirectory(Path.GetDirectoryName(ragDst)!);
            File.Copy(ragSrc, ragDst, overwrite: true);
            manifest.Files["rag/" + name + ".db"] = BrainManifest.HashFile(ragDst);
        }
        if (graphOk)
        {
            // genera l'export JSON del grafo dal vault (o usa un graph già pronto)
            var graphSrc = FindGraph(dir);
            string graphJson;
            if (graphSrc is not null) graphJson = File.ReadAllText(graphSrc);
            else graphJson = Basic.AI.Graph.GraphExporter.ToJson(Basic.AI.Graph.GraphStore.Build(dir));
            var graphDst = Path.Combine(installRoot, "graphs", name + ".json");
            Directory.CreateDirectory(Path.GetDirectoryName(graphDst)!);
            File.WriteAllText(graphDst, graphJson);
            manifest.Files["graphs/" + name + ".json"] = BrainManifest.HashFile(graphDst);
        }
        // aggiorna il manifest con i file effettivamente copiati nella root import
        File.WriteAllText(Path.Combine(installRoot, "brain.manifest.json"), manifest.ToJson(withChecksum: true));

        BrainRegistry.Register(name, vaultPath: dir, mode: manifest.Mode,
            checksum: manifest.Checksum, version: manifest.Version, docs: manifest.Docs,
            parsers: manifest.Parsers, probe: probe, signedBy: "gmb-sign");

        return new BrainSignResult(name, available, probe, true);
    }

    // ── Probe ──────────────────────────────────────────────────────────────

    private static bool ProbeGraph(string dir, out string report)
    {
        try
        {
            var mds = System.IO.Directory.GetFiles(dir, "*.md", SearchOption.AllDirectories).Take(200).ToArray();
            var existing = FindGraph(dir);
            if (mds.Length == 0 && existing is null)
            {
                report = "fail: 0 .md e nessun grafo json/graphml pronto";
                return false;
            }
            if (existing is not null)
            {
                report = $"ok: grafo pronto ({Path.GetFileName(existing)})";
                return true;
            }
            // build in-memory sul campione
            var graph = Basic.AI.Graph.GraphStore.Build(dir);
            if (graph.Nodes.Count >= 1)
            {
                report = $"ok: {graph.Nodes.Count} nodi, {graph.Edges.Count} link";
                return true;
            }
            report = "fail: 0 nodi (nessun .md con contenuto riconosciuto)";
            return false;
        }
        catch (Exception ex)
        {
            report = "fail: " + ex.Message;
            return false;
        }
    }

    private static bool ProbeVectorDb(string dir, out string report)
    {
        var db = FindDb(dir);
        if (db is null)
        {
            report = "fail: nessun .db/.sqlite";
            return false;
        }
        try
        {
            // apre come SQLite e prova a enumerare le collection vettoriali
            var alias = "BrainSignProbe";
            var ragDb = Basic.AI.Rag.RagDbSession.Open(db, alias);
            var collections = Basic.AI.Rag.AiVectorRuntime.ListCollections(alias);
            var names = collections.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (names.Length >= 1)
            {
                report = $"ok: {names.Length} collection vettoriali ({string.Join(",", names.Take(3))}{(names.Length > 3 ? "..." : "")})";
                return true;
            }
            report = "fail: sqlite ok ma nessuna tabella vec_* (non è un vectordb riconoscibile)";
            return false;
        }
        catch (Exception ex)
        {
            report = "fail: non è un vectordb valido (" + ex.Message + ")";
            return false;
        }
    }

    private static string? FindDb(string dir)
    {
        return System.IO.Directory.GetFiles(dir, "*.db", SearchOption.AllDirectories)
            .Concat(System.IO.Directory.GetFiles(dir, "*.sqlite", SearchOption.AllDirectories))
            .FirstOrDefault();
    }

    private static string? FindGraph(string dir)
    {
        foreach (var f in System.IO.Directory.GetFiles(dir, "*.json", SearchOption.AllDirectories))
            if (f.EndsWith(".graphml", StringComparison.OrdinalIgnoreCase)) continue;
            else if (f.EndsWith(".stats.json", StringComparison.OrdinalIgnoreCase)) continue;
            else if (f.EndsWith(".state.json", StringComparison.OrdinalIgnoreCase)) continue;
            else return f;
        foreach (var f in System.IO.Directory.GetFiles(dir, "*.graphml", SearchOption.AllDirectories))
            return f;
        return null;
    }

    private static IEnumerable<string> SignatureFiles(string dir, List<string> available)
    {
        if (available.Contains("rag") && FindDb(dir) is string db) yield return db;
        if (available.Contains("graph") && FindGraph(dir) is string g) yield return g;
    }

    private static long CountMarkdown(string dir)
    {
        try { return System.IO.Directory.GetFiles(dir, "*.md", SearchOption.AllDirectories).LongLength; }
        catch { return 0; }
    }

    /// <summary>Impacchetta un brain firmato in zip (BRAIN SIGN --out).</summary>
    public static bool ExportSignedZip(string name, string outZip)
    {
        var root = Path.Combine(AiConfig.BrainsDefaultDir, name);
        if (!Directory.Exists(root)) return false;
        var files = System.IO.Directory.GetFiles(root, "*", SearchOption.AllDirectories).ToList();
        if (files.Count == 0) return false;
        var dir = Path.GetDirectoryName(Path.GetFullPath(outZip));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        return CompressionUtils.CreateZip(outZip, files);
    }
}
