// FILE: Basic.AI.Knowledge.BrainToolchain.V2
// Toolchain dei cervelli — V2 (TODO Brain-Toolchain-REPL-CLI): DEDUPE · EXTRACT · MERGE, per RAG e GRAPH.
// Stessa filosofia della V1: tooling fuori dal linguaggio, errori espliciti, `--dry-run` sulle scritture,
// riuso dello schema vettoriale esistente (`SqlVectorSchema.EnsureSchema`) — la copia porta con sé chunk ed
// embedding (BLOB) e le righe FTS5: MAI re-embedding.
//
// Nota di progetto: la copia è fatta per-documento con SQL parametrizzato (niente `ATTACH`, niente SQL
// generato a stringhe) così il comportamento è lo stesso su SQLite embedded e i tipi restano espliciti.

using System.Text;
using System.Text.Json;
using Basic.AI.Graph;
using Basic.SQLite.Vector;
using Microsoft.Data.Sqlite;

namespace Basic.AI.Knowledge;

public static partial class BrainToolchain
{
    // ── DEDUPE ────────────────────────────────────────────────────────────
    /// <summary>
    /// Rimuove i DUPLICATI. RAG: chunk con testo identico (normalizzato: trim/collapse spazi, case-insensitive)
    /// e chunk-crumbs sotto <paramref name="minChars"/>. GRAPH: edge identici (from/to/type) e self-loop.
    /// Senza <paramref name="apply"/> è un REPORT (nessuna modifica).
    /// </summary>
    public static ToolResult Dedupe(string name, bool apply = false, int minChars = 40, bool dryRun = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        if (BrainRegistry.Info(name) is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");

        var sb = new StringBuilder();
        var writes = apply && !dryRun;

        if (BrainRegistry.RagDbPath(name) is { } dbPath && File.Exists(dbPath))
        {
            using var conn = new SqliteConnection($"Data Source={dbPath}");
            conn.Open();
            var groups = new List<List<long>>();
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = """
                    SELECT MIN(id) AS keep, GROUP_CONCAT(id) AS ids
                    FROM vec_chunks
                    GROUP BY lower(trim(replace(replace(text, char(10), ' '), char(13), ' ')))
                    HAVING COUNT(*) > 1
                    """;
                using var r = cmd.ExecuteReader();
                while (r.Read())
                {
                    var ids = r.GetString(1).Split(',').Select(long.Parse).ToList();
                    groups.Add(ids.Skip(1).ToList());     // il primo (MIN id) resta
                }
            }
            var crumbs = Scalar(conn, $"SELECT COUNT(*) FROM vec_chunks WHERE length(trim(text)) < {Math.Max(1, minChars)}");
            var dupCount = groups.Sum(g => g.Count);
            sb.AppendLine($"RAG: {groups.Count} gruppi di chunk duplicati → {dupCount} rimovibili; "
                        + $"{crumbs} chunk-crumbs (< {minChars} char)");

            if (writes && (dupCount > 0 || crumbs > 0))
            {
                using var tx = conn.BeginTransaction();
                foreach (var id in groups.SelectMany(g => g)) DeleteChunkCascade(conn, id);
                using (var cmd = conn.CreateCommand())
                {
                    cmd.Transaction = tx;
                    cmd.CommandText = $"DELETE FROM vec_chunks WHERE length(trim(text)) < {Math.Max(1, minChars)}";
                    cmd.ExecuteNonQuery();
                }
                tx.Commit();
                sb.AppendLine($"RAG: rimossi {dupCount + crumbs} chunk (con embedding e righe FTS)");
            }
        }

        if (BrainRegistry.GraphJsonPath(name) is { } graphPath && File.Exists(graphPath))
        {
            var graph = WikiGraph.LoadFromJson(File.ReadAllText(graphPath));
            var seen = new HashSet<string>(StringComparer.Ordinal);
            var dupEdges = 0; var selfLoops = 0;
            foreach (var edge in graph.Edges)
            {
                if (edge.From == edge.To) { selfLoops++; continue; }
                if (!seen.Add($"{edge.From}|{edge.To}|{edge.Type}")) dupEdges++;
            }
            sb.AppendLine($"GRAPH: {dupEdges} edge duplicati, {selfLoops} self-loop");
            if (writes && (dupEdges > 0 || selfLoops > 0))
            {
                var removed = 0;
                var keep = new List<WikiEdge>();
                var seen2 = new HashSet<string>(StringComparer.Ordinal);
                foreach (var edge in graph.Edges)
                {
                    if (edge.From == edge.To || !seen2.Add($"{edge.From}|{edge.To}|{edge.Type}")) { removed++; continue; }
                    keep.Add(edge);
                }
                graph.Edges.Clear();
                graph.Edges.AddRange(keep);
                WriteGraphArtifacts(graph, name, graphPath);
                sb.AppendLine($"GRAPH: rimossi {removed} edge (export ricalcolati)");
            }
        }

        if (sb.Length == 0) return ToolResult.Fail($"cervello '{name}': nessun artefatto (né rag né graph)");
        if (!apply) sb.AppendLine("(report: usa --apply per rimuovere)");
        return new ToolResult(true, $"BRAIN DEDUPE — {name}:" + Environment.NewLine + sb.ToString().TrimEnd());
    }

    // ── EXTRACT ───────────────────────────────────────────────────────────
    /// <summary>
    /// Estrae un SOTTOINSIEME in un nuovo cervello. RAG: documenti il cui `source_id` contiene
    /// <paramref name="sourceFilter"/> (chunk + embedding copiati come BLOB). GRAPH: sottografo dei nodi il
    /// cui id contiene <paramref name="nodeFilter"/> più i vicini fino a <paramref name="hops"/>.
    /// </summary>
    public static ToolResult Extract(string name, string outName, string? sourceFilter = null,
        string? nodeFilter = null, int hops = 1, bool dryRun = false)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(outName))
            return ToolResult.Fail("uso: BRAIN EXTRACT <name> --out <nuovo> [--source substr] [--node substr] [--hops N]");
        if (BrainRegistry.Info(name) is null) return ToolResult.Fail($"cervello '{name}' non registrato");
        if (sourceFilter is null && nodeFilter is null)
            return ToolResult.Fail("serve --source <substr> (RAG) e/o --node <substr> (GRAPH)");
        if (BrainRegistry.Info(outName) is not null || BrainRegistry.AvailableModes(outName).Length > 0)
            return ToolResult.Fail($"il nome '{outName}' è già in uso");

        var sb = new StringBuilder();

        if (sourceFilter is not null)
        {
            var src = BrainRegistry.RagDbPath(name);
            if (src is null) return ToolResult.Fail($"cervello '{name}': nessun db RAG da estrarre");
            var dst = Path.Combine(AiConfig.RagDefaultDir, outName + ".db");
            if (dryRun)
            {
                using var rc = new SqliteConnection($"Data Source={src};Mode=ReadOnly");
                rc.Open();
                sb.AppendLine($"[dry-run] RAG: {CountDocsLike(rc, sourceFilter)} documenti con source_id LIKE "
                            + $"'%{sourceFilter}%' → {dst}");
            }
            else
            {
                if (!CopyRag(src, dst, sourceFilter, merge: false, out var docs, out var chunks, out var err))
                    return ToolResult.Fail($"estrazione RAG fallita: {err}");
                sb.AppendLine($"RAG: {docs} documenti, {chunks} chunk (con embedding) → {dst}");
            }
        }

        if (nodeFilter is not null)
        {
            var srcJson = BrainRegistry.GraphJsonPath(name);
            if (srcJson is null) return ToolResult.Fail($"cervello '{name}': nessun grafo da estrarre");
            var dstJson = Path.Combine(AiConfig.GraphsDefaultDir, outName + ".json");
            if (dryRun)
            {
                sb.AppendLine($"[dry-run] GRAPH: nodi con id LIKE '%{nodeFilter}%' + {hops} hop → {dstJson}");
            }
            else
            {
                var graph = WikiGraph.LoadFromJson(File.ReadAllText(srcJson));
                var keep = new HashSet<string>(graph.Nodes.Keys.Where(k => k.Contains(nodeFilter, StringComparison.OrdinalIgnoreCase)),
                    StringComparer.Ordinal);
                for (var h = 0; h < Math.Max(0, hops); h++)
                {
                    var current = keep.ToList();
                    foreach (var edge in graph.Edges)
                    {
                        if (current.Contains(edge.From)) keep.Add(edge.To);
                        if (current.Contains(edge.To)) keep.Add(edge.From);
                    }
                }
                if (keep.Count == 0) return ToolResult.Fail($"nessun nodo con id contenente '{nodeFilter}'");
                var sub = SubGraph(graph, keep);
                WriteGraphArtifacts(sub, outName, dstJson);
                sb.AppendLine($"GRAPH: {sub.NodeCount} nodi, {sub.Edges.Count} edge ({hops} hop) → {dstJson}");
            }
        }

        if (!dryRun)
        {
            var mode = sourceFilter is not null && nodeFilter is not null ? "both"
                : sourceFilter is not null ? "rag" : "graph";
            BrainRegistry.Register(outName, source: BrainRegistry.Info(name)?.Source, mode: mode);
            sb.AppendLine($"registrato: {outName} (mode {mode})");
        }
        return new ToolResult(true, $"BRAIN EXTRACT {name} → {outName}:" + Environment.NewLine + sb.ToString().TrimEnd());
    }

    // ── MERGE ─────────────────────────────────────────────────────────────
    /// <summary>
    /// Fonde più cervelli in uno nuovo. RAG: unione dei documenti con dedup per `source_id`
    /// (<paramref name="onConflict"/>: `skip` = tieni il primo, `replace` = tieni l'ultimo). GRAPH: unione di
    /// nodi/edge con dedup per id, export ricalcolati. Precondizione RAG: stessa dimensione embedding.
    /// </summary>
    public static ToolResult Merge(IReadOnlyList<string> names, string outName, string onConflict = "skip", bool dryRun = false)
    {
        if (names.Count < 2) return ToolResult.Fail("uso: BRAIN MERGE <a> <b> [<c>…] --out <nuovo>");
        if (string.IsNullOrWhiteSpace(outName)) return ToolResult.Fail("serve --out <nuovo nome>");
        if (BrainRegistry.Info(outName) is not null || BrainRegistry.AvailableModes(outName).Length > 0)
            return ToolResult.Fail($"il nome '{outName}' è già in uso");

        var dbs = new List<string>();
        var graphs = new List<string>();
        foreach (var n in names)
        {
            if (BrainRegistry.Info(n) is null) return ToolResult.Fail($"cervello '{n}' non registrato");
            if (BrainRegistry.RagDbPath(n) is { } p) dbs.Add(p);
            if (BrainRegistry.GraphJsonPath(n) is { } g) graphs.Add(g);
        }
        if (dbs.Count == 0 && graphs.Count == 0) return ToolResult.Fail("nessuno dei cervelli indicati ha artefatti");
        if (dbs.Count > 1)
        {
            var dims = dbs.Select(RagDimensions).Where(d => d > 0).Distinct().ToList();
            if (dims.Count > 1) return ToolResult.Fail($"dimensioni embedding diverse fra i cervelli ({string.Join(", ", dims)}): merge non sicuro");
        }

        var sb = new StringBuilder();
        var dstDb = Path.Combine(AiConfig.RagDefaultDir, outName + ".db");
        if (dbs.Count > 0)
        {
            if (dryRun) sb.AppendLine($"[dry-run] RAG: unione di {dbs.Count} db (on-conflict {onConflict}) → {dstDb}");
            else
            {
                if (!MergeRag(dbs, dstDb, onConflict, out var docs, out var chunks, out var err))
                    return ToolResult.Fail($"merge RAG fallito: {err}");
                sb.AppendLine($"RAG: {docs} documenti, {chunks} chunk → {dstDb}");
            }
        }
        if (graphs.Count > 0)
        {
            var dstJson = Path.Combine(AiConfig.GraphsDefaultDir, outName + ".json");
            if (dryRun) sb.AppendLine($"[dry-run] GRAPH: unione di {graphs.Count} grafi → {dstJson}");
            else
            {
                var union = new WikiGraph();
                var seenEdges = new HashSet<string>(StringComparer.Ordinal);
                foreach (var path in graphs)
                {
                    var g = WikiGraph.LoadFromJson(File.ReadAllText(path));
                    foreach (var (key, node) in g.Nodes) union.Nodes.TryAdd(key, node);
                    foreach (var edge in g.Edges)
                        if (seenEdges.Add($"{edge.From}|{edge.To}|{edge.Type}")) union.Edges.Add(edge);
                    foreach (var d in g.Dangling) if (!union.Dangling.Contains(d)) union.Dangling.Add(d);
                }
                WriteGraphArtifacts(union, outName, dstJson);
                sb.AppendLine($"GRAPH: {union.NodeCount} nodi, {union.Edges.Count} edge (unione di {graphs.Count} grafi) → {dstJson}");
            }
        }
        if (!dryRun)
        {
            var mode = dbs.Count > 0 && graphs.Count > 0 ? "both" : dbs.Count > 0 ? "rag" : "graph";
            BrainRegistry.Register(outName, mode: mode);
            sb.AppendLine($"registrato: {outName} (mode {mode})");
        }
        return new ToolResult(true, $"BRAIN MERGE [{string.Join(", ", names)}] → {outName}:" + Environment.NewLine + sb.ToString().TrimEnd());
    }

    // ── copia RAG (extract/merge) ─────────────────────────────────────────
    /// <summary>Copia in <paramref name="dstPath"/> i documenti di <paramref name="srcPath"/> il cui
    /// source_id contiene <paramref name="sourceFilter"/> (null = tutti). Con <paramref name="merge"/> i
    /// source_id già presenti non vengono duplicati. Chunk + embedding + FTS copiati.</summary>
    private static bool CopyRag(string srcPath, string dstPath, string? sourceFilter, bool merge,
        out long docs, out long chunks, out string error)
    {
        docs = 0; chunks = 0; error = "";
        try
        {
            if (File.Exists(dstPath)) File.Delete(dstPath);
            using var dst = OpenNew(dstPath, out var dstErr);
            if (dst is null) { error = dstErr; return false; }
            using var src = new SqliteConnection($"Data Source={srcPath};Mode=ReadOnly");
            src.Open();
            CopyCollections(src, dst);
            var collectionId = FirstCollectionId(dst);
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            using var cmd = src.CreateCommand();
            cmd.CommandText = sourceFilter is null
                ? "SELECT id, source_id FROM vec_documents ORDER BY id"
                : "SELECT id, source_id FROM vec_documents WHERE source_id LIKE $s ORDER BY id";
            if (sourceFilter is not null) cmd.Parameters.AddWithValue("$s", $"%{sourceFilter}%");
            var rows = new List<(long Id, string Source)>();
            using (var r = cmd.ExecuteReader())
                while (r.Read()) rows.Add((r.GetInt64(0), r.GetString(1)));

            foreach (var (docId, sourceId) in rows)
            {
                if (merge && !seen.Add(sourceId)) continue;
                chunks += CopyDocument(src, dst, docId, sourceId, collectionId);
                docs++;
            }
            return true;
        }
        catch (Exception ex) { error = ex.Message; return false; }
    }

    private static bool MergeRag(List<string> dbs, string dstPath, string onConflict,
        out long docs, out long chunks, out string error)
    {
        docs = 0; chunks = 0; error = "";
        try
        {
            if (File.Exists(dstPath)) File.Delete(dstPath);
            using var dst = OpenNew(dstPath, out var dstErr);
            if (dst is null) { error = dstErr; return false; }
            using (var first = new SqliteConnection($"Data Source={dbs[0]};Mode=ReadOnly"))
            {
                first.Open();
                CopyCollections(first, dst);
            }
            var collectionId = FirstCollectionId(dst);
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var replace = onConflict.Equals("replace", StringComparison.OrdinalIgnoreCase);

            foreach (var path in dbs)
            {
                using var src = new SqliteConnection($"Data Source={path};Mode=ReadOnly");
                src.Open();
                var rows = new List<(long Id, string Source)>();
                using (var cmd = src.CreateCommand())
                {
                    cmd.CommandText = "SELECT id, source_id FROM vec_documents ORDER BY id";
                    using var r = cmd.ExecuteReader();
                    while (r.Read()) rows.Add((r.GetInt64(0), r.GetString(1)));
                }
                foreach (var (docId, sourceId) in rows)
                {
                    var isNew = seen.Add(sourceId);
                    if (!isNew)
                    {
                        if (!replace) continue;
                        DeleteDocumentBySource(dst, sourceId);
                    }
                    chunks += CopyDocument(src, dst, docId, sourceId, collectionId);
                    docs++;
                }
            }
            return true;
        }
        catch (Exception ex) { error = ex.Message; return false; }
    }

    /// <summary>Apre (o crea) un db vettoriale nuovo applicando lo schema canonico. Null + errore se fallisce.</summary>
    private static SqliteConnection? OpenNew(string path, out string error)
    {
        error = "";
        try
        {
            var dir = Path.GetDirectoryName(Path.GetFullPath(path));
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            var conn = new SqliteConnection($"Data Source={path}");
            conn.Open();
            SqlVectorSchema.EnsureSchema(conn);
            return conn;
        }
        catch (Exception ex) { error = ex.Message; return null; }
    }

    private static void CopyCollections(SqliteConnection src, SqliteConnection dst)
    {
        using var cmd = src.CreateCommand();
        cmd.CommandText = "SELECT id, name, dimensions, metric, model_version, model_digest, created_at FROM vec_collections";
        using var r = cmd.ExecuteReader();
        while (r.Read())
        {
            using var ins = dst.CreateCommand();
            ins.CommandText = """
                INSERT OR IGNORE INTO vec_collections(id, name, dimensions, metric, model_version, model_digest, created_at)
                VALUES ($id, $name, $dims, $metric, $mv, $md, $created)
                """;
            ins.Parameters.AddWithValue("$id", r.GetInt64(0));
            ins.Parameters.AddWithValue("$name", r.GetString(1));
            ins.Parameters.AddWithValue("$dims", r.GetInt32(2));
            ins.Parameters.AddWithValue("$metric", r.GetString(3));
            ins.Parameters.AddWithValue("$mv", r.GetString(4));
            ins.Parameters.AddWithValue("$md", r.IsDBNull(5) ? DBNull.Value : r.GetValue(5));
            ins.Parameters.AddWithValue("$created", r.GetString(6));
            ins.ExecuteNonQuery();
        }
    }

    private static long FirstCollectionId(SqliteConnection conn) => Scalar(conn, "SELECT COALESCE(MIN(id), 0) FROM vec_collections");

    /// <summary>Copia documento + chunk + embedding + FTS dal sorgente al dst (nuovi id). Ritorna i chunk copiati.</summary>
    private static long CopyDocument(SqliteConnection src, SqliteConnection dst, long docId, string sourceId, long collectionId,
        string? originBrain = null)
    {
        string hash = "", status = "current", meta = "";
        using (var cmd = src.CreateCommand())
        {
            cmd.CommandText = "SELECT source_hash, status, COALESCE(metadata_json, '') FROM vec_documents WHERE id = $id";
            cmd.Parameters.AddWithValue("$id", docId);
            using var r = cmd.ExecuteReader();
            if (!r.Read()) return 0;
            hash = r.GetString(0); status = r.GetString(1); meta = r.GetString(2);
        }
        var tagged = originBrain is null ? (meta.Length == 0 ? null : meta)
            : string.IsNullOrWhiteSpace(meta) ? $"{{\"originBrain\":\"{originBrain}\"}}"
            : meta.TrimEnd().TrimEnd('}') + $",\"originBrain\":\"{originBrain}\"}}";
        long newDocId;
        using (var ins = dst.CreateCommand())
        {
            ins.CommandText = """
                INSERT INTO vec_documents(collection_id, source_id, source_hash, status, metadata_json, created_at, updated_at)
                VALUES ($cid, $src, $hash, $status, $meta, datetime('now'), datetime('now'));
                SELECT last_insert_rowid();
                """;
            ins.Parameters.AddWithValue("$cid", collectionId);
            ins.Parameters.AddWithValue("$src", sourceId);
            ins.Parameters.AddWithValue("$hash", hash);
            ins.Parameters.AddWithValue("$status", status);
            ins.Parameters.AddWithValue("$meta", (object?)tagged ?? DBNull.Value);
            newDocId = Convert.ToInt64(ins.ExecuteScalar());
        }

        var copied = 0L;
        using (var rc = src.CreateCommand())
        {
            rc.CommandText = "SELECT id, chunk_index, text, token_count, metadata_json FROM vec_chunks WHERE document_id = $d ORDER BY chunk_index";
            rc.Parameters.AddWithValue("$d", docId);
            using var rr = rc.ExecuteReader();
            var pending = new List<(long Id, string Text, int Idx, int? Tok, string? Meta)>();
            while (rr.Read())
                pending.Add((rr.GetInt64(0), rr.GetString(2), rr.GetInt32(1),
                    rr.IsDBNull(3) ? null : rr.GetInt32(3), rr.IsDBNull(4) ? null : rr.GetString(4)));
            foreach (var (oldChunkId, text, idx, tok, cmeta) in pending)
            {
                long newChunkId;
                using (var ic = dst.CreateCommand())
                {
                    ic.CommandText = """
                        INSERT INTO vec_chunks(document_id, chunk_index, text, token_count, metadata_json)
                        VALUES ($doc, $idx, $text, $tok, $meta);
                        SELECT last_insert_rowid();
                        """;
                    ic.Parameters.AddWithValue("$doc", newDocId);
                    ic.Parameters.AddWithValue("$idx", idx);
                    ic.Parameters.AddWithValue("$text", text);
                    ic.Parameters.AddWithValue("$tok", (object?)tok ?? DBNull.Value);
                    ic.Parameters.AddWithValue("$meta", (object?)cmeta ?? DBNull.Value);
                    newChunkId = Convert.ToInt64(ic.ExecuteScalar());
                }
                using (var re = src.CreateCommand())
                {
                    re.CommandText = "SELECT model_version, embedding, dimensions FROM vec_embeddings WHERE chunk_id = $c";
                    re.Parameters.AddWithValue("$c", oldChunkId);
                    using var ee = re.ExecuteReader();
                    while (ee.Read())
                    {
                        using var ie = dst.CreateCommand();
                        ie.CommandText = """
                            INSERT INTO vec_embeddings(chunk_id, model_version, embedding, dimensions)
                            VALUES ($c, $m, $b, $d)
                            """;
                        ie.Parameters.AddWithValue("$c", newChunkId);
                        ie.Parameters.AddWithValue("$m", ee.GetString(0));
                        ie.Parameters.AddWithValue("$b", (byte[])ee[1]);
                        ie.Parameters.AddWithValue("$d", ee.GetInt32(2));
                        ie.ExecuteNonQuery();
                    }
                }
                InsertFts(dst, newChunkId, text, collectionId);
                copied++;
            }
        }
        return copied;
    }

    private static void InsertFts(SqliteConnection conn, long chunkId, string text, long collectionId)
    {
        if (!SqlVectorFts.IsAvailable(conn)) return;
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "INSERT INTO fts_chunks(rowid, text, chunk_id, collection_id) VALUES ($id, $text, $id, $cid)";
        cmd.Parameters.AddWithValue("$id", chunkId);
        cmd.Parameters.AddWithValue("$text", text);
        cmd.Parameters.AddWithValue("$cid", collectionId);
        cmd.ExecuteNonQuery();
    }

    private static void DeleteDocumentBySource(SqliteConnection conn, string sourceId)
    {
        var ids = new List<long>();
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT id FROM vec_documents WHERE source_id = $s";
            cmd.Parameters.AddWithValue("$s", sourceId);
            using var r = cmd.ExecuteReader();
            while (r.Read()) ids.Add(r.GetInt64(0));
        }
        foreach (var id in ids)
        {
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = "SELECT id FROM vec_chunks WHERE document_id = $d";
                cmd.Parameters.AddWithValue("$d", id);
                using var r = cmd.ExecuteReader();
                var chunks = new List<long>();
                while (r.Read()) chunks.Add(r.GetInt64(0));
                foreach (var c in chunks) DeleteChunkCascade(conn, c);
            }
            using var del = conn.CreateCommand();
            del.CommandText = "DELETE FROM vec_documents WHERE id = $d";
            del.Parameters.AddWithValue("$d", id);
            del.ExecuteNonQuery();
        }
    }

    private static void DeleteChunkCascade(SqliteConnection conn, long chunkId)
    {
        SqlVectorFts.DeleteChunk(conn, chunkId);
        using (var e = conn.CreateCommand())
        {
            e.CommandText = "DELETE FROM vec_embeddings WHERE chunk_id = $id";
            e.Parameters.AddWithValue("$id", chunkId);
            e.ExecuteNonQuery();
        }
        using var c = conn.CreateCommand();
        c.CommandText = "DELETE FROM vec_chunks WHERE id = $id";
        c.Parameters.AddWithValue("$id", chunkId);
        c.ExecuteNonQuery();
    }

    /// <summary>Dimensione embedding del db (0 se non determinabile): precondizione dei merge.</summary>
    private static long RagDimensions(string dbPath)
    {
        try
        {
            using var conn = new SqliteConnection($"Data Source={dbPath};Mode=ReadOnly");
            conn.Open();
            return Scalar(conn, "SELECT COALESCE(MAX(dimensions), 0) FROM vec_embeddings");
        }
        catch { return 0; }
    }

    private static long CountDocsLike(SqliteConnection conn, string filter)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT COUNT(*) FROM vec_documents WHERE source_id LIKE $s";
        cmd.Parameters.AddWithValue("$s", $"%{filter}%");
        return Convert.ToInt64(cmd.ExecuteScalar());
    }

    private static WikiGraph SubGraph(WikiGraph source, HashSet<string> keep)
    {
        var sub = new WikiGraph();
        foreach (var (key, node) in source.Nodes)
            if (keep.Contains(key)) sub.Nodes[key] = node;
        foreach (var edge in source.Edges)
            if (keep.Contains(edge.From) && keep.Contains(edge.To)) sub.Edges.Add(edge);
        return sub;
    }

    private static void WriteGraphArtifacts(WikiGraph graph, string name, string jsonPath)
    {
        var dir = Path.GetDirectoryName(jsonPath)!;
        Directory.CreateDirectory(dir);
        var export = GraphExporter.Export(graph, name, AiConfig.GraphPageRankDamping, AiConfig.GraphPageRankIterations, withHtml: true);
        File.WriteAllText(jsonPath, export.Json);
        File.WriteAllText(Path.Combine(dir, name + ".stats.json"), export.StatsJson);
        File.WriteAllText(Path.Combine(dir, name + ".mermaid"), export.Mermaid);
        File.WriteAllText(Path.Combine(dir, name + ".graphml"), export.Graphml);
        if (export.Html is not null) File.WriteAllText(Path.Combine(dir, name + ".html"), export.Html);
    }
}
