// FILE: Basic.AI.Knowledge.BrainToolchain.V3
// Toolchain dei cervelli — V3 (TODO Brain-Toolchain-REPL-CLI): CONVERT rag↔graph · VERIFY --golden (qualità)
// · DIFF · DOCTOR. Stessa filosofia delle V1/V2: tooling fuori dal linguaggio, una sola implementazione per
// REPL e CLI, errori espliciti, `--dry-run` dove si scrive, mai no-op silenziosi.

using System.Text;
using System.Text.Json;
using Basic.AI.Graph;
using Basic.AI.Rag;
using Microsoft.Data.Sqlite;

namespace Basic.AI.Knowledge;

public static partial class BrainToolchain
{
    // ── CONVERT (rag ↔ graph) ─────────────────────────────────────────────
    /// <summary>
    /// Converte un cervello da una modalità all'altra, DERIVANDO l'artefatto mancante dai contenuti presenti.
    /// - `rag` → `graph`: nodi = `source_id` del db RAG; edge = wikilink `[[…]]` trovati nei chunk, risolti per
    ///   basename sui nodi (come fa il grafo wiki). `minEdges` (default 1) impone un minimo: sotto soglia è un
    ///   errore esplicito, non un grafo vuoto.
    /// - `graph` → `rag`: un documento per nodo (titolo + frontmatter + archi uscenti/entranti) indicizzato con
    ///   l'embedder ATTIVO (richiede un provider embedding reale, come `RAG BUILD`).
    /// </summary>
    public static ToolResult ConvertMode(string name, string to, string? outName = null, bool inPlace = false,
        int minEdges = 1, bool dryRun = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        to = (to ?? "").Trim().ToLowerInvariant();
        if (to is not ("rag" or "graph")) return ToolResult.Fail("--to attesi: rag|graph");
        var info = BrainRegistry.Info(name);
        if (info is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");
        var target = outName ?? name;
        if (!inPlace && outName is null) return ToolResult.Fail("serve --out <nuovo> oppure --in-place");
        if (!inPlace && (BrainRegistry.Info(target) is not null || BrainRegistry.AvailableModes(target).Length > 0))
            return ToolResult.Fail($"il nome '{target}' è già in uso (usa --in-place per aggiornare lo stesso cervello)");

        return to == "graph"
            ? ConvertRagToGraph(name, target, minEdges, dryRun)
            : ConvertGraphToRag(name, target, dryRun);
    }

    private static ToolResult ConvertRagToGraph(string name, string target, int minEdges, bool dryRun)
    {
        var dbPath = BrainRegistry.RagDbPath(name);
        if (dbPath is null) return ToolResult.Fail($"cervello '{name}': nessun db RAG da convertire (usa RAG BUILD)");
        var graphJson = Path.Combine(AiConfig.GraphsDefaultDir, target + ".json");

        // nodi = source_id (normalizzati), testo per la ricerca dei wikilink = tutti i chunk del documento
        var sources = new List<string>();
        var texts = new Dictionary<string, StringBuilder>(StringComparer.Ordinal);
        using (var conn = new SqliteConnection($"Data Source={dbPath};Mode=ReadOnly"))
        {
            conn.Open();
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = "SELECT id, source_id FROM vec_documents ORDER BY id";
                using var r = cmd.ExecuteReader();
                while (r.Read())
                {
                    var sid = r.GetString(1);
                    sources.Add(sid);
                    texts[sid] = new StringBuilder();
                }
            }
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = """
                    SELECT d.source_id, c.text FROM vec_chunks c
                    JOIN vec_documents d ON d.id = c.document_id
                    """;
                using var r = cmd.ExecuteReader();
                while (r.Read())
                {
                    var sid = r.GetString(0);
                    if (texts.TryGetValue(sid, out var sb)) sb.Append(r.GetString(1)).Append('\n');
                }
            }
        }
        if (sources.Count == 0) return ToolResult.Fail($"cervello '{name}': nessun documento nel RAG");

        var rel = sources.Select(NormalizeSourceId).ToList();
        var byBase = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);   // basename senza .md → path
        foreach (var p in rel)
        {
            var baseName = Path.GetFileNameWithoutExtension(p);
            byBase.TryAdd(baseName, p);
            byBase.TryAdd(p, p);
        }
        var graph = new WikiGraph();
        foreach (var p in rel) graph.Nodes[p] = new WikiNode(p, Path.GetFileNameWithoutExtension(p), []);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (sid, sb) in texts)
        {
            var from = NormalizeSourceId(sid);
            foreach (var link in ExtractWikiLinks(sb.ToString()))
            {
                if (!byBase.TryGetValue(link, out var to)) { graph.Dangling.Add($"{from} -> {link}"); continue; }
                if (to == from) continue;
                if (seen.Add($"{from}|{to}")) graph.Edges.Add(new WikiEdge(from, to, "link"));
            }
        }
        if (dryRun)
            return new ToolResult(true, $"[dry-run] CONVERT rag→graph '{name}': {graph.NodeCount} nodi, "
                + $"{graph.Edges.Count} edge, {graph.Dangling.Count} dangling → {graphJson}");
        if (graph.Edges.Count < Math.Max(0, minEdges))
            return ToolResult.Fail($"conversione rag→graph: solo {graph.Edges.Count} edge trovati (minimo {minEdges}): "
                + "il RAG non contiene wikilink risolvibili (usa --min-edges 0 per accettare un grafo senza archi)");

        WriteGraphArtifacts(graph, target, graphJson);
        var source = BrainRegistry.Info(name)?.Source;
        if (target == name)
        {
            var modes = BrainRegistry.AvailableModes(name).ToList();
            if (!modes.Contains("graph")) modes.Add("graph");
            BrainRegistry.Register(name, source: source, vaultPath: BrainRegistry.Info(name)?.VaultPath,
                mode: modes.Count > 1 ? "both" : modes[0]);
        }
        else BrainRegistry.Register(target, source: source, mode: "graph");
        return new ToolResult(true, $"CONVERT rag→graph: {graph.NodeCount} nodi, {graph.Edges.Count} edge, "
            + $"{graph.Dangling.Count} dangling → {graphJson}");
    }

    private static ToolResult ConvertGraphToRag(string name, string target, bool dryRun)
    {
        var graphJson = BrainRegistry.GraphJsonPath(name);
        if (graphJson is null) return ToolResult.Fail($"cervello '{name}': nessun grafo da convertire (usa GRAPH BUILD)");
        var graph = WikiGraph.LoadFromJson(File.ReadAllText(graphJson));
        if (graph.NodeCount == 0) return ToolResult.Fail($"cervello '{name}': grafo senza nodi");

        var outgoing = graph.Edges.GroupBy(e => e.From, StringComparer.Ordinal)
            .ToDictionary(g => g.Key, g => g.Select(e => e.To).ToList(), StringComparer.Ordinal);
        var incoming = graph.Edges.GroupBy(e => e.To, StringComparer.Ordinal)
            .ToDictionary(g => g.Key, g => g.Select(e => e.From).ToList(), StringComparer.Ordinal);

        var docs = new List<(string SourceId, string Text)>();
        foreach (var (path, node) in graph.Nodes)
        {
            var sb = new StringBuilder();
            sb.AppendLine(node.Title.Length > 0 ? node.Title : path);
            if (node.Frontmatter.Count > 0)
                sb.AppendLine(string.Join("\n", node.Frontmatter.Select(kv => $"{kv.Key}: {string.Join(", ", kv.Value)}")));
            if (outgoing.TryGetValue(path, out var outs)) sb.AppendLine("collegamenti: " + string.Join(", ", outs.Take(40)));
            if (incoming.TryGetValue(path, out var ins)) sb.AppendLine("collegato da: " + string.Join(", ", ins.Take(40)));
            docs.Add((path, sb.ToString()));
        }
        var dbPath = Path.Combine(AiConfig.RagDefaultDir, target + ".db");
        if (dryRun)
            return new ToolResult(true, $"[dry-run] CONVERT graph→rag '{name}': {docs.Count} documenti → {dbPath}");

        int indexed;
        try
        {
            EnsureEmbeddingProviderForToolchain();
            var alias = RagDbSession.DefaultAlias(dbPath);
            RagDbSession.Open(dbPath, alias);
            RagDbSession.EnableWal(dbPath);
            var dims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
            AiVectorRuntime.EnsureCollection(alias, AiConfig.RagCollection, dims);
            indexed = 0;
            foreach (var (sourceId, text) in docs)
            {
                var outcome = AiVectorRuntime.IndexText(alias, AiConfig.RagCollection, sourceId, text);
                if (outcome != Basic.SQLite.Vector.IndexTextOutcome.Skipped) indexed++;
            }
        }
        // Errore ESPLICITO (mai un'eccezione non gestita né un no-op silenzioso): tipicamente manca un
        // provider embedding reale (il fixture è rifiutato per progetto).
        catch (Exception ex)
        {
            return ToolResult.Fail($"conversione graph→rag impossibile: {ex.Message}");
        }
        var srcSource = BrainRegistry.Info(name)?.Source;
        if (target == name)
        {
            var modes = BrainRegistry.AvailableModes(name).ToList();
            if (!modes.Contains("rag")) modes.Add("rag");
            BrainRegistry.Register(name, source: srcSource, vaultPath: BrainRegistry.Info(name)?.VaultPath,
                mode: modes.Count > 1 ? "both" : modes[0]);
        }
        else BrainRegistry.Register(target, source: srcSource, mode: "rag");
        return new ToolResult(true, $"CONVERT graph→rag: {indexed} documenti indicizzati → {dbPath}");
    }

    /// <summary>Normalizza un source_id in un path relativo (toglie il prefisso assoluto del vault, se noto).</summary>
    private static string NormalizeSourceId(string sourceId)
    {
        var s = sourceId.Replace('\\', '/');
        var idx = s.IndexOf("/Ai", StringComparison.Ordinal);
        if (s.StartsWith('/') && idx > 0) s = s[(idx + 1)..];      // "…/AiFinanceWiki/01_Strategies/x.md" → "AiFinanceWiki/…"
        return s.TrimStart('/');
    }

    private static IEnumerable<string> ExtractWikiLinks(string text)
    {
        var i = 0;
        while (i < text.Length)
        {
            var start = text.IndexOf("[[", i, StringComparison.Ordinal);
            if (start < 0) yield break;
            var end = text.IndexOf("]]", start + 2, StringComparison.Ordinal);
            if (end < 0) yield break;
            var inner = text[(start + 2)..end];
            var bar = inner.IndexOf('|');
            if (bar >= 0) inner = inner[..bar];
            var hash = inner.IndexOf('#');
            if (hash >= 0) inner = inner[..hash];
            inner = inner.Trim().TrimEnd('.');
            if (inner.Length > 0) yield return inner;
            i = end + 2;
        }
    }

    // ── VERIFY --golden (qualità del retrieval) ───────────────────────────
    /// <summary>
    /// Misura la QUALITÀ del retrieval RAG su un **golden set** JSONL: una riga per query con
    /// `{"query": "…", "expect_sources": ["substr", …]}`. Per ogni query calcola P@1, P@3, recall@k e MRR
    /// (un hit è rilevante se contiene uno degli `expect_sources`), aggrega le medie e — se `minP1` è dato —
    /// considera l'esito fallito sotto soglia (exit code ≠ 0 dalla CLI).
    /// </summary>
    public static ToolResult VerifyGolden(string name, string goldenPath, int topK = 5, double minP1 = double.NaN)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        if (!File.Exists(goldenPath)) return ToolResult.Fail($"golden set non trovato: {goldenPath}");
        var dbPath = BrainRegistry.RagDbPath(name);
        if (dbPath is null) return ToolResult.Fail($"cervello '{name}': nessun db RAG (VERIFY --golden richiede la modalità rag)");

        var rows = new List<(string Query, List<string> Expect)>();
        foreach (var line in File.ReadLines(goldenPath))
        {
            var t = line.Trim();
            if (t.Length == 0 || t.StartsWith('#')) continue;
            try
            {
                using var doc = JsonDocument.Parse(t);
                var q = doc.RootElement.TryGetProperty("query", out var qe) ? qe.GetString() ?? "" : "";
                var exp = doc.RootElement.TryGetProperty("expect_sources", out var ee) && ee.ValueKind == JsonValueKind.Array
                    ? ee.EnumerateArray().Select(x => x.GetString() ?? "").Where(x => x.Length > 0).ToList()
                    : [];
                if (q.Length > 0 && exp.Count > 0) rows.Add((q, exp));
            }
            catch (JsonException) { /* riga malformata: ignorata (il conteggio finale lo mostra) */ }
        }
        if (rows.Count == 0) return ToolResult.Fail($"golden set vuoto o malformato: {goldenPath} (attese righe JSON con query + expect_sources)");

        var alias = RagDbSession.DefaultAlias(dbPath);
        try
        {
            // La QUERY va embeddata: come `gmb rag build`/`BRAIN CREATE` va attivato il provider reale
            // (senza, SearchJson fallisce con "richiede un provider embedding attivo" — trovato dal demo).
            EnsureEmbeddingProviderForToolchain();
        }
        catch (Exception ex) { return ToolResult.Fail($"VERIFY --golden impossibile: {ex.Message}"); }
        RagDbSession.Open(dbPath, alias);
        var sb = new StringBuilder();
        double sumP1 = 0, sumP3 = 0, sumRecall = 0, sumMrr = 0;
        foreach (var (query, expect) in rows)
        {
            var json = AiVectorRuntime.SearchJson(alias, AiConfig.RagCollection, query, topK, hybrid: true);
            var hits = new List<string>();
            using (var doc = JsonDocument.Parse(json))
                if (doc.RootElement.TryGetProperty("hits", out var arr) && arr.ValueKind == JsonValueKind.Array)
                    foreach (var h in arr.EnumerateArray())
                        hits.Add(h.TryGetProperty("sourceId", out var s) ? s.GetString() ?? "" : "");

            bool Relevant(string hit) => expect.Any(e => hit.Contains(e, StringComparison.OrdinalIgnoreCase));
            var first = hits.FindIndex(Relevant);
            var p1 = first == 0 ? 1.0 : 0.0;
            var top3 = hits.Take(3).Count(Relevant) / (double)Math.Max(1, Math.Min(3, hits.Count));
            var found = hits.Take(topK).Where(Relevant).Distinct().Count();
            var recall = found / (double)expect.Count;
            var mrr = first >= 0 ? 1.0 / (first + 1) : 0.0;
            sumP1 += p1; sumP3 += top3; sumRecall += recall; sumMrr += mrr;
            sb.AppendLine($"  {(first == 0 ? "ok" : first >= 0 ? $"r{first + 1}" : "MISS"):<5} P@1={p1:F0} recall@{topK}={recall:F2} MRR={mrr:F2}  «{query}»");
        }
        var n = rows.Count;
        var p1m = sumP1 / n; var p3m = sumP3 / n; var rcm = sumRecall / n; var mrrm = sumMrr / n;
        var header = $"BRAIN VERIFY --golden — {name}: {n} query  ·  P@1 {p1m:P0} · P@3 {p3m:P0} · "
                   + $"recall@{topK} {rcm:P0} · MRR {mrrm:F2}";
        var ok = double.IsNaN(minP1) || p1m >= minP1;
        var text = header + Environment.NewLine + sb.ToString().TrimEnd();
        return ok ? new ToolResult(true, text)
                  : new ToolResult(false, text, $"P@1 {p1m:P0} sotto la soglia richiesta {minP1:P0}");
    }

    // ── DIFF ──────────────────────────────────────────────────────────────
    /// <summary>Confronta due cervelli: differenze RAG (source_id) e GRAPH (nodi/edge), con campioni.</summary>
    public static ToolResult Diff(string nameA, string nameB, bool asJson = false)
    {
        if (string.IsNullOrWhiteSpace(nameA) || string.IsNullOrWhiteSpace(nameB))
            return ToolResult.Fail("uso: BRAIN DIFF <a> <b> [--json]");
        foreach (var n in new[] { nameA, nameB })
            if (BrainRegistry.Info(n) is null) return ToolResult.Fail($"cervello '{n}' non registrato");

        var ragA = SourceIds(nameA); var ragB = SourceIds(nameB);
        var nodesA = NodeIds(nameA); var nodesB = NodeIds(nameB);
        var onlyA = ragA.Except(ragB, StringComparer.OrdinalIgnoreCase).ToList();
        var onlyB = ragB.Except(ragA, StringComparer.OrdinalIgnoreCase).ToList();
        var common = ragA.Intersect(ragB, StringComparer.OrdinalIgnoreCase).Count();
        var nodesOnlyA = nodesA.Except(nodesB, StringComparer.OrdinalIgnoreCase).ToList();
        var nodesOnlyB = nodesB.Except(nodesA, StringComparer.OrdinalIgnoreCase).ToList();

        if (asJson)
            return new ToolResult(true, JsonSerializer.Serialize(new
            {
                a = nameA,
                b = nameB,
                rag = new { a_documents = ragA.Count, b_documents = ragB.Count, common,
                            only_a = onlyA.Count, only_b = onlyB.Count, sample_only_a = onlyA.Take(10), sample_only_b = onlyB.Take(10) },
                graph = new { a_nodes = nodesA.Count, b_nodes = nodesB.Count,
                              only_a = nodesOnlyA.Count, only_b = nodesOnlyB.Count }
            }, new JsonSerializerOptions { WriteIndented = true }));

        var sb = new StringBuilder($"BRAIN DIFF — {nameA} vs {nameB}");
        sb.AppendLine($"  RAG:   {ragA.Count} vs {ragB.Count} documenti · comuni {common} · solo-A {onlyA.Count} · solo-B {onlyB.Count}");
        foreach (var s in onlyA.Take(5)) sb.AppendLine($"    - solo in {nameA}: {s}");
        foreach (var s in onlyB.Take(5)) sb.AppendLine($"    + solo in {nameB}: {s}");
        sb.AppendLine($"  GRAPH: {nodesA.Count} vs {nodesB.Count} nodi · solo-A {nodesOnlyA.Count} · solo-B {nodesOnlyB.Count}");
        foreach (var s in nodesOnlyA.Take(5)) sb.AppendLine($"    - solo in {nameA}: {s}");
        foreach (var s in nodesOnlyB.Take(5)) sb.AppendLine($"    + solo in {nameB}: {s}");
        return new ToolResult(true, sb.ToString().TrimEnd());
    }

    private static HashSet<string> SourceIds(string name)
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (BrainRegistry.RagDbPath(name) is not { } dbPath) return set;
        using var conn = new SqliteConnection($"Data Source={dbPath};Mode=ReadOnly");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT source_id FROM vec_documents";
        using var r = cmd.ExecuteReader();
        while (r.Read()) set.Add(r.GetString(0));
        return set;
    }

    private static HashSet<string> NodeIds(string name)
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (BrainRegistry.GraphJsonPath(name) is not { } graphPath) return set;
        try
        {
            var g = WikiGraph.LoadFromJson(File.ReadAllText(graphPath));
            foreach (var k in g.Nodes.Keys) set.Add(k);
        }
        catch { /* grafo illeggibile: insieme vuoto */ }
        return set;
    }

    // ── DOCTOR ────────────────────────────────────────────────────────────
    /// <summary>Verifica e MANUTENZIONE sicura: `VERIFY` + i fix non distruttivi (FTS5 rebuild, VACUUM,
    /// prune dei dangling del grafo) + riverifica. Ritorna KO esplicito se dopo i fix restano problemi.</summary>
    public static ToolResult Doctor(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        var before = Verify(name);
        if (before.Ok && !before.Text.Contains("[KO]")) return new ToolResult(true, $"BRAIN DOCTOR — {name}: nessun problema\n" + before.Text);

        var sb = new StringBuilder($"BRAIN DOCTOR — {name}: fix applicati");
        if (BrainRegistry.RagDbPath(name) is { } dbPath)
        {
            try
            {
                using (var conn = new SqliteConnection($"Data Source={dbPath}"))
                {
                    conn.Open();
                    Exec(conn, "PRAGMA optimize");
                    Exec(conn, "INSERT INTO fts_chunks(fts_chunks) VALUES('rebuild')");
                    Exec(conn, "VACUUM");
                }
                sb.AppendLine("  RAG: PRAGMA optimize + FTS5 rebuild + VACUUM");
            }
            catch (Exception ex) { sb.AppendLine($"  RAG: fix non applicabile ({ex.Message})"); }
        }
        if (BrainRegistry.GraphJsonPath(name) is { } graphPath)
        {
            try
            {
                var g = WikiGraph.LoadFromJson(File.ReadAllText(graphPath));
                var dangling = g.Dangling.Count;
                g.Dangling.Clear();
                WriteGraphArtifacts(g, name, graphPath);
                sb.AppendLine($"  GRAPH: rimossi {dangling} dangling, export ricalcolati");
            }
            catch (Exception ex) { sb.AppendLine($"  GRAPH: fix non applicabile ({ex.Message})"); }
        }
        var after = Verify(name);
        sb.AppendLine(after.Ok ? "riverifica: OK" : "riverifica: PROBLEMI RESIDUI\n" + after.Text);
        return after.Ok ? new ToolResult(true, sb.ToString().TrimEnd())
                        : new ToolResult(false, sb.ToString().TrimEnd(), after.Error ?? "problemi residui dopo i fix");
    }
}
