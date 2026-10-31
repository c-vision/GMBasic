// FILE: Basic.AI.Knowledge.GraphManager
// Knowledge Graph tipizzato (L5 del piano Second Brain): NON solo wikilink. Ogni documento è
// arricchito con 8 campi (folder, tag, author, date, strategy, asset, concept, linked) e le
// relazioni sono EDGE TIPIZZATI WEIGHTED {source, target, type, weight} — LINK (wikilink 1.0),
// TAG_SHARE (Jaccard), FOLDER_CHILD (0.8), CONTENT_MENTION (0.6), TEMPORAL_NEAR (decay).
// Comando legacy: GRAPH BUILD/UPDATE <dir> (backend RDD MEMORY/JSON), GRAPH FIND PATH a TO b.
// Gli algoritmi canonici (ShortestPath/Dijkstra/PageRank/...) vivono nel runtime condiviso;
// qui resta la pipeline di costruzione del grafo arricchito + storage RDD.
using Basic.AI.Graph;

namespace Basic.AI.Knowledge;

/// <summary>Tipi di relazione del knowledge graph (12 tipi del piano, sottoset canonico
/// implementato: LINK/TAG_SHARE/FOLDER_CHILD/CONTENT_MENTION/TEMPORAL_NEAR).</summary>
public enum GraphEdgeType
{
    Link,            // wikilink risolto [[A]] → [[B]] — peso 1.0
    TagShare,        // Jaccard su tag condivisi ≥ 0.5 — peso = jaccard
    FolderChild,     // stessa cartella (co-locazione) — peso 0.8
    ContentMention,  // titolo di B menzionato nel body di A (senza wikilink) — peso 0.6
    TemporalNear,    // date vicine (entro 7gg) — peso = decay temporale
}

/// <summary>Edge tipizzato e pesato del knowledge graph.</summary>
public sealed record GraphEdge(string Source, string Target, GraphEdgeType Type, double Weight);

/// <summary>Nodo arricchito: 8 campi per il retrieval strutturale.</summary>
public sealed record GraphNode(
    string Path,                          // path relativo normalizzato
    string Title,                         // alias link o nome file senza estensione
    string Folder,                        // folder di appartenenza
    IReadOnlyList<string> Tags,           // tag (frontmatter tags + tag del folder)
    string? Author,                       // author (frontmatter)
    string? Date,                         // date (frontmatter, ISO)
    string? Strategy,                     // strategy (frontmatter)
    string? Asset,                        // asset (frontmatter)
    IReadOnlyList<string> Concepts,       // concept (frontmatter, multi)
    IReadOnlyList<string> Linked);         // target dei wikilink risolti (outgoing)

/// <summary>Grafo arricchito costruito da GRAPH BUILD: nodi 8-campi + edge tipizzati weighted.</summary>
public sealed class RichGraph
{
    public Dictionary<string, GraphNode> Nodes { get; } = new(StringComparer.Ordinal);
    public List<GraphEdge> Edges { get; } = [];
    public List<string> Dangling { get; } = [];
    public Dictionary<string, int> FolderCounts { get; } = new(StringComparer.Ordinal);

    public int NodeCount => Nodes.Count;
    public int EdgeCount => Edges.Count;

    /// <summary>Densità del grafo (2E / V(V-1)), 0 se V&lt;2.</summary>
    public double Density
    {
        get
        {
            var v = Nodes.Count;
            if (v < 2) return 0;
            return 2.0 * Edges.Count / (v * (v - 1.0));
        }
    }

    /// <summary>Grado medio (totalDegree / V).</summary>
    public double AverageDegree
    {
        get
        {
            var v = Nodes.Count;
            if (v == 0) return 0;
            return 2.0 * Edges.Count / v;
        }
    }

    /// <summary>Edge che toccano un nodo (outgoing+incoming).</summary>
    public IReadOnlyList<GraphEdge> Incident(string node) =>
        Edges.Where(e => e.Source == node || e.Target == node).ToList();

    /// <summary>Vicini diretti di un nodo (qualsiasi tipo edge).</summary>
    public IReadOnlyList<string> Neighbors(string node) =>
        Edges.Where(e => e.Source == node).Select(e => e.Target)
             .Concat(Edges.Where(e => e.Target == node).Select(e => e.Source))
             .Distinct().ToList();

    /// <summary>Path più corto (BFS, archi non pesati) tra from e to. Vuoto se non raggiungibile.</summary>
    public IReadOnlyList<string> ShortestPath(string from, string to)
    {
        if (from == to) return [from];
        var prev = new Dictionary<string, string>();
        var q = new Queue<string>();
        var visited = new HashSet<string>(StringComparer.Ordinal);
        q.Enqueue(from);
        visited.Add(from);
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            foreach (var nb in Neighbors(cur))
            {
                if (visited.Contains(nb)) continue;
                visited.Add(nb);
                prev[nb] = cur;
                if (nb == to)
                {
                    var path = new List<string>();
                    for (var x = to; x != null; x = prev.GetValueOrDefault(x)) { path.Add(x); if (x == from) break; }
                    path.Reverse();
                    return path;
                }
                q.Enqueue(nb);
            }
        }
        return [];
    }

    /// <summary>Ritorna i componenti fortemente/weakly connessi (BFS su archi non diretti).</summary>
    public IReadOnlyList<IReadOnlyList<string>> ConnectedComponents()
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        var comps = new List<IReadOnlyList<string>>();
        foreach (var node in Nodes.Keys)
        {
            if (seen.Contains(node)) continue;
            var comp = new List<string>();
            var q = new Queue<string>();
            q.Enqueue(node);
            seen.Add(node);
            while (q.Count > 0)
            {
                var cur = q.Dequeue();
                comp.Add(cur);
                foreach (var nb in Neighbors(cur))
                {
                    if (seen.Contains(nb)) continue;
                    seen.Add(nb);
                    q.Enqueue(nb);
                }
            }
            comps.Add(comp);
        }
        return comps;
    }
}

    /// <summary>Backend di storage RDD del grafo (astrazione GMBasic, non DB specifico).</summary>
    public enum GraphRddBackend
    {
        Memory,  // in-memory (default)
        Json,    // persistenza su <name>.graph.json
        Sqlite,  // riservato (futuro)
        Isam,    // riservato (futuro)
        Dbf,     // riservato (futuro)
    }

    /// <summary>Builder del knowledge graph arricchito da un vault .md (riusa GraphStore per
    /// wikilink/frontmatter e aggiunge gli edge tipizzati). Riusabile da REPL, CLI e BASIC.</summary>
    public static class GraphManager
    {
        /// <summary>Grafo attivo per il backend RDD MEMORY (stessa sessione/processo).</summary>
        public static RichGraph? ActiveGraph { get; set; }

        /// <summary>Nome del grafo attivo (basename della build corrente).</summary>
        public static string? ActiveName { get; set; }

        /// <summary>Directory in cui è stato persistito il grafo attivo (o GraphsDefaultDir).</summary>
        public static string? ActiveDir { get; set; }

        /// <summary>Backend RDD di default per GRAPH BUILD/UPDATE (default MEMORY).</summary>
        public static GraphRddBackend DefaultBackend { get; set; } = GraphRddBackend.Memory;
    /// <summary>Peso minimo di Jaccard sui tag per creare un edge TAG_SHARE.</summary>
    public const double TagShareThreshold = 0.5;

    /// <summary>Finestra temporale (giorni) per l'edge TEMPORAL_NEAR.</summary>
    public const int TemporalWindowDays = 7;

    /// <summary>Costruisce il grafo arricchito da un vault wiki.</summary>
    public static RichGraph Build(string wikiDir, IReadOnlySet<string>? includeExtensions = null)
    {
        var baseGraph = GraphStore.Build(wikiDir, includeExtensions);
        var rich = new RichGraph();

        foreach (var (rel, node) in baseGraph.Nodes)
        {
            var folder = rel.Contains('/') ? rel[..rel.LastIndexOf('/')] : "";
            var fm = node.Frontmatter;
            var tags = fm.TryGetValue("tags", out var t) ? t : [];
            var author = fm.TryGetValue("author", out var a) && a.Count > 0 ? a[0] : null;
            var date = fm.TryGetValue("date", out var d) && d.Count > 0 ? d[0] : null;
            var strategy = fm.TryGetValue("strategy", out var s) && s.Count > 0 ? s[0] : null;
            var asset = fm.TryGetValue("asset", out var as_) && as_.Count > 0 ? as_[0] : null;
            var concepts = fm.TryGetValue("concept", out var c) ? c : [];

            var linked = baseGraph.Edges.Where(e => e.From == rel).Select(e => e.To).ToList();
            rich.Nodes[rel] = new GraphNode(rel, node.Title, folder, tags, author, date, strategy, asset,
                concepts, linked);
            if (folder.Length > 0)
                rich.FolderCounts[folder] = rich.FolderCounts.GetValueOrDefault(folder) + 1;
        }

        // Edge LINK: wikilink risolti (peso 1.0).
        foreach (var e in baseGraph.Edges)
            rich.Edges.Add(new GraphEdge(e.From, e.To, GraphEdgeType.Link, 1.0));
        rich.Dangling.AddRange(baseGraph.Dangling);

        // Edge TAG_SHARE: Jaccard su tag condivisi tra coppie di nodi (peso = jaccard).
        var nodes = rich.Nodes.Values.ToList();
        for (var i = 0; i < nodes.Count; i++)
            for (var j = i + 1; j < nodes.Count; j++)
            {
                var a = nodes[i];
                var b = nodes[j];
                var inter = a.Tags.Intersect(b.Tags, StringComparer.OrdinalIgnoreCase).Count();
                var union = a.Tags.Union(b.Tags, StringComparer.OrdinalIgnoreCase).Count();
                if (union == 0) continue;
                var jac = (double)inter / union;
                if (jac >= TagShareThreshold && a.Path != b.Path)
                    rich.Edges.Add(new GraphEdge(a.Path, b.Path, GraphEdgeType.TagShare, Math.Round(jac, 3)));
            }

        // Edge FOLDER_CHILD: co-locazione nella stessa cartella (peso 0.8).
        var byFolder = nodes.GroupBy(n => n.Folder).Where(g => g.Key.Length > 0).ToList();
        foreach (var grp in byFolder)
        {
            var members = grp.ToList();
            for (var i = 0; i < members.Count; i++)
                for (var j = i + 1; j < members.Count; j++)
                {
                    var a = members[i];
                    var b = members[j];
                    if (a.Path == b.Path) continue;
                    // solo se non c'è già un edge diretto LINK tra i due (evita ridondanza pesante)
                    var hasLink = rich.Edges.Any(e => e.Type == GraphEdgeType.Link &&
                        (e.Source == a.Path && e.Target == b.Path || e.Source == b.Path && e.Target == a.Path));
                    if (!hasLink)
                        rich.Edges.Add(new GraphEdge(a.Path, b.Path, GraphEdgeType.FolderChild, 0.8));
                }
        }

        // Edge CONTENT_MENTION + TEMPORAL_NEAR (body cross-references e vicinanza temporale).
        var bodyByNode = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var rel in baseGraph.Nodes.Keys)
        {
            var full = Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar));
            try { bodyByNode[rel] = File.ReadAllText(full); } catch { bodyByNode[rel] = ""; }
        }
        var titleToPath = baseGraph.Nodes.ToDictionary(kv => kv.Value.Title, kv => kv.Key, StringComparer.OrdinalIgnoreCase);

        foreach (var a in nodes)
        {
            var body = bodyByNode.GetValueOrDefault(a.Path) ?? "";
            var bodyLower = body.ToLowerInvariant();
            // CONTENT_MENTION: un titolo di un ALTRO nodo appare nel body senza essere wikilink.
            foreach (var (title, path) in titleToPath)
            {
                if (path == a.Path) continue;
                if (bodyLower.Contains(title.ToLowerInvariant()))
                {
                    var hasLink = rich.Edges.Any(e => e.Type == GraphEdgeType.Link && e.Source == a.Path && e.Target == path);
                    if (!hasLink)
                        rich.Edges.Add(new GraphEdge(a.Path, path, GraphEdgeType.ContentMention, 0.6));
                }
            }

            // TEMPORAL_NEAR: date ISO a ≤ 7 giorni di distanza → decay.
            if (DateTime.TryParse(a.Date, out var dateA))
                foreach (var b in nodes)
                {
                    if (b.Path == a.Path || !DateTime.TryParse(b.Date, out var dateB)) continue;
                    var days = Math.Abs((dateA - dateB).TotalDays);
                    if (days <= TemporalWindowDays)
                    {
                        var weight = Math.Round(Math.Exp(-days / TemporalWindowDays), 3);
                        var hasAny = rich.Edges.Any(e =>
                            (e.Source == a.Path && e.Target == b.Path || e.Source == b.Path && e.Target == a.Path));
                        if (!hasAny)
                            rich.Edges.Add(new GraphEdge(a.Path, b.Path, GraphEdgeType.TemporalNear, weight));
                    }
                }
        }

        return rich;
    }

    /// <summary>Serializza il grafo arricchito in JSON (formato esteso rispetto a GraphExporter:
    /// edge tipizzati + 8 campi).</summary>
    public static string ToJson(RichGraph g)
    {
        using var ms = new System.IO.MemoryStream();
        using (var w = new System.Text.Json.Utf8JsonWriter(ms))
        {
            w.WriteStartObject();
            w.WriteString("format", "gmbrain-richgraph");
            w.WriteNumber("version", 1);
            w.WriteStartArray("nodes");
            foreach (var n in g.Nodes.Values)
            {
                w.WriteStartObject();
                w.WriteString("id", n.Path);
                w.WriteString("title", n.Title);
                w.WriteString("folder", n.Folder);
                WriteStringArray(w, "tags", n.Tags);
                if (n.Author is not null) w.WriteString("author", n.Author);
                if (n.Date is not null) w.WriteString("date", n.Date);
                if (n.Strategy is not null) w.WriteString("strategy", n.Strategy);
                if (n.Asset is not null) w.WriteString("asset", n.Asset);
                WriteStringArray(w, "concept", n.Concepts);
                WriteStringArray(w, "linked", n.Linked);
                w.WriteEndObject();
            }
            w.WriteEndArray();
            w.WriteStartArray("edges");
            foreach (var e in g.Edges)
            {
                w.WriteStartObject();
                w.WriteString("from", e.Source);
                w.WriteString("to", e.Target);
                w.WriteString("type", e.Type.ToString());
                w.WriteNumber("weight", e.Weight);
                w.WriteEndObject();
            }
            w.WriteEndArray();
            w.WriteStartArray("dangling");
            foreach (var d in g.Dangling) w.WriteStringValue(d);
            w.WriteEndArray();
            w.WriteStartObject("folderCounts");
            foreach (var (k, v) in g.FolderCounts) w.WriteNumber(k, v);
            w.WriteEndObject();
            w.WriteEndObject();
        }
        return System.Text.Encoding.UTF8.GetString(ms.ToArray());
    }

    private static void WriteStringArray(System.Text.Json.Utf8JsonWriter w, string name, IReadOnlyList<string> vals)
    {
        w.WriteStartArray(name);
        foreach (var v in vals) w.WriteStringValue(v);
        w.WriteEndArray();
    }

    /// <summary>Ricostruisce un RichGraph dal JSON di ToJson.</summary>
    public static RichGraph LoadFromJson(string json)
    {
        var g = new RichGraph();
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        var root = doc.RootElement;

        if (root.TryGetProperty("nodes", out var nodes) && nodes.ValueKind == System.Text.Json.JsonValueKind.Array)
        {
            foreach (var n in nodes.EnumerateArray())
            {
                var id = n.TryGetProperty("id", out var idP) ? idP.GetString() ?? "" : "";
                if (string.IsNullOrEmpty(id)) continue;
                var title = n.TryGetProperty("title", out var t) ? t.GetString() ?? "" : "";
                var folder = n.TryGetProperty("folder", out var f) ? f.GetString() ?? "" : "";
                var author = n.TryGetProperty("author", out var au) && au.ValueKind == System.Text.Json.JsonValueKind.String ? au.GetString() : null;
                var date = n.TryGetProperty("date", out var da) && da.ValueKind == System.Text.Json.JsonValueKind.String ? da.GetString() : null;
                var strategy = n.TryGetProperty("strategy", out var st) && st.ValueKind == System.Text.Json.JsonValueKind.String ? st.GetString() : null;
                var asset = n.TryGetProperty("asset", out var as_) && as_.ValueKind == System.Text.Json.JsonValueKind.String ? as_.GetString() : null;
                g.Nodes[id] = new GraphNode(id, title, folder,
                    ReadStringArray(n, "tags"), author, date, strategy, asset,
                    ReadStringArray(n, "concept"), ReadStringArray(n, "linked"));
                if (folder.Length > 0)
                    g.FolderCounts[folder] = g.FolderCounts.GetValueOrDefault(folder) + 1;
            }
        }

        if (root.TryGetProperty("edges", out var edges) && edges.ValueKind == System.Text.Json.JsonValueKind.Array)
        {
            foreach (var e in edges.EnumerateArray())
            {
                var from = e.TryGetProperty("from", out var f) ? f.GetString() ?? "" : "";
                var to = e.TryGetProperty("to", out var t) ? t.GetString() ?? "" : "";
                var type = e.TryGetProperty("type", out var tp) && Enum.TryParse<GraphEdgeType>(tp.GetString(), out var tpv) ? tpv : GraphEdgeType.Link;
                var weight = e.TryGetProperty("weight", out var w) && w.TryGetDouble(out var wv) ? wv : 1.0;
                if (!string.IsNullOrEmpty(from) && !string.IsNullOrEmpty(to))
                    g.Edges.Add(new GraphEdge(from, to, type, weight));
            }
        }

        if (root.TryGetProperty("dangling", out var dang) && dang.ValueKind == System.Text.Json.JsonValueKind.Array)
            foreach (var d in dang.EnumerateArray())
                if (d.ValueKind == System.Text.Json.JsonValueKind.String) g.Dangling.Add(d.GetString() ?? "");

        return g;
    }

    private static IReadOnlyList<string> ReadStringArray(System.Text.Json.JsonElement n, string name)
    {
        if (!n.TryGetProperty(name, out var p) || p.ValueKind != System.Text.Json.JsonValueKind.Array) return [];
        var vals = new List<string>();
        foreach (var item in p.EnumerateArray())
            if (item.ValueKind == System.Text.Json.JsonValueKind.String) vals.Add(item.GetString() ?? "");
        return vals;
    }

    /// <summary>Path del file JSON di un grafo arricchito in graphsDir.</summary>
    public static string JsonPath(string graphsDir, string name) => Path.Combine(graphsDir, name + ".graph.json");

    /// <summary>Carica il grafo arricchito da <paramref name="name"/> in <paramref name="graphsDir"/>
    /// (se il file .graph.json esiste), altrimenti il WikiGraph classico come RichGraph minimo.</summary>
    public static RichGraph? TryLoad(string graphsDir, string name)
    {
        var path = JsonPath(graphsDir, name);
        if (!File.Exists(path)) return null;
        try { return LoadFromJson(File.ReadAllText(path)); }
        catch { return null; }
    }

    /// <summary>Salva il grafo arricchito su <paramref name="graphsDir"/>/<name>.graph.json.</summary>
    public static void Save(RichGraph g, string graphsDir, string name)
    {
        if (!Directory.Exists(graphsDir)) Directory.CreateDirectory(graphsDir);
        File.WriteAllText(JsonPath(graphsDir, name), ToJson(g));
    }

    /// <summary>Parse del backend RDD dal nome (MEMORY/JSON/SQLITE/ISAM/DBF), case-insensitive.
    /// Valori non riconosciuti → GraphRddBackend.Memory.</summary>
    public static GraphRddBackend ParseBackend(string? backend) =>
        backend?.ToUpperInvariant() switch
        {
            "JSON" => GraphRddBackend.Json,
            "SQLITE" => GraphRddBackend.Sqlite,
            "ISAM" => GraphRddBackend.Isam,
            "DBF" => GraphRddBackend.Dbf,
            _ => GraphRddBackend.Memory,
        };

    // ── Backend SQLITE (F7 P2): persiste nodi/edge in <name>.graph.db ──
    // Tabelle: graph_nodes(id TEXT PRIMARY KEY, title, folder, tags, author, date, strategy,
    // asset, concept, linked, json JSON) e graph_edges(from, to, type, weight). ISAM/DBF
    // richiedono driver non supportati da questo percorso RDD → errore esplicito, mai stub.

    /// <summary>Path del db SQLite del grafo arricchito.</summary>
    public static string SqlitePath(string graphsDir, string name) => Path.Combine(graphsDir, name + ".graph.db");

    /// <summary>Salva il grafo arricchito nel backend SQLite (tabella nodes/edges).</summary>
    public static void SaveSqlite(RichGraph g, string graphsDir, string name)
    {
        if (!Directory.Exists(graphsDir)) Directory.CreateDirectory(graphsDir);
        var dbPath = SqlitePath(graphsDir, name);
        using var conn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={dbPath}");
        conn.Open();
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = """
                CREATE TABLE IF NOT EXISTS graph_nodes(
                    id TEXT PRIMARY KEY, title TEXT, folder TEXT, tags TEXT, author TEXT,
                    date TEXT, strategy TEXT, asset TEXT, concept TEXT, linked TEXT, payload TEXT);
                CREATE TABLE IF NOT EXISTS graph_edges(
                    from_id TEXT, to_id TEXT, type TEXT, weight REAL);
                """;
            cmd.ExecuteNonQuery();
        }
        using var tx = conn.BeginTransaction();
        using (var del = conn.CreateCommand()) { del.Transaction = tx; del.CommandText = "DELETE FROM graph_nodes; DELETE FROM graph_edges;"; del.ExecuteNonQuery(); }
        using (var insN = conn.CreateCommand())
        {
            insN.Transaction = tx;
            insN.CommandText = "INSERT OR REPLACE INTO graph_nodes(id,title,folder,tags,author,date,strategy,asset,concept,linked,payload) VALUES($id,$t,$f,$tags,$a,$d,$s,$as,$c,$l,$p);";
            insN.Parameters.Add("$id", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$t", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$f", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$tags", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$a", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$d", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$s", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$as", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$c", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$l", Microsoft.Data.Sqlite.SqliteType.Text);
            insN.Parameters.Add("$p", Microsoft.Data.Sqlite.SqliteType.Text);
            foreach (var n in g.Nodes.Values)
            {
                insN.Parameters["$id"].Value = n.Path;
                insN.Parameters["$t"].Value = n.Title;
                insN.Parameters["$f"].Value = n.Folder;
                insN.Parameters["$tags"].Value = string.Join(",", n.Tags);
                insN.Parameters["$a"].Value = (object?)n.Author ?? DBNull.Value;
                insN.Parameters["$d"].Value = (object?)n.Date ?? DBNull.Value;
                insN.Parameters["$s"].Value = (object?)n.Strategy ?? DBNull.Value;
                insN.Parameters["$as"].Value = (object?)n.Asset ?? DBNull.Value;
                insN.Parameters["$c"].Value = string.Join(",", n.Concepts);
                insN.Parameters["$l"].Value = string.Join(",", n.Linked);
                insN.Parameters["$p"].Value = DBNull.Value;
                insN.ExecuteNonQuery();
            }
        }
        using (var insE = conn.CreateCommand())
        {
            insE.Transaction = tx;
            insE.CommandText = "INSERT INTO graph_edges(from_id,to_id,type,weight) VALUES($f,$t,$ty,$w);";
            insE.Parameters.Add("$f", Microsoft.Data.Sqlite.SqliteType.Text);
            insE.Parameters.Add("$t", Microsoft.Data.Sqlite.SqliteType.Text);
            insE.Parameters.Add("$ty", Microsoft.Data.Sqlite.SqliteType.Text);
            insE.Parameters.Add("$w", Microsoft.Data.Sqlite.SqliteType.Real);
            foreach (var e in g.Edges)
            {
                insE.Parameters["$f"].Value = e.Source;
                insE.Parameters["$t"].Value = e.Target;
                insE.Parameters["$ty"].Value = e.Type.ToString();
                insE.Parameters["$w"].Value = e.Weight;
                insE.ExecuteNonQuery();
            }
        }
        tx.Commit();
    }

    /// <summary>Carica un grafo arricchito dal backend SQLite, o null se il db non esiste.</summary>
    public static RichGraph? TryLoadSqlite(string graphsDir, string name)
    {
        var dbPath = SqlitePath(graphsDir, name);
        if (!File.Exists(dbPath)) return null;
        try { return LoadSqlite(dbPath); }
        catch { return null; }
    }

    /// <summary>Legge un RichGraph da un db SQLite (tabelle graph_nodes/graph_edges).</summary>
    public static RichGraph LoadSqlite(string dbPath)
    {
        var g = new RichGraph();
        using var conn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={dbPath}");
        conn.Open();
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT id,title,folder,tags,author,date,strategy,asset,concept,linked FROM graph_nodes;";
            using var r = cmd.ExecuteReader();
            while (r.Read())
            {
                var path = r.GetString(0);
                var tags = SplitCsv(r.IsDBNull(3) ? "" : r.GetString(3));
                var concepts = SplitCsv(r.IsDBNull(8) ? "" : r.GetString(8));
                var linked = SplitCsv(r.IsDBNull(9) ? "" : r.GetString(9));
                g.Nodes[path] = new GraphNode(path, r.GetString(1), r.GetString(2), tags,
                    NullStr(r, 4), NullStr(r, 5), NullStr(r, 6), NullStr(r, 7), concepts, linked);
                var folder = r.GetString(2);
                if (folder.Length > 0)
                    g.FolderCounts[folder] = g.FolderCounts.GetValueOrDefault(folder) + 1;
            }
        }
        using (var cmd = conn.CreateCommand())
        {
            cmd.CommandText = "SELECT from_id,to_id,type,weight FROM graph_edges;";
            using var r = cmd.ExecuteReader();
            while (r.Read())
            {
                var type = Enum.TryParse<GraphEdgeType>(r.GetString(2), out var t) ? t : GraphEdgeType.Link;
                g.Edges.Add(new GraphEdge(r.GetString(0), r.GetString(1), type, r.GetDouble(3)));
            }
        }
        return g;
    }

    private static string? NullStr(Microsoft.Data.Sqlite.SqliteDataReader r, int i) =>
        r.IsDBNull(i) ? null : r.GetString(i);

    private static IReadOnlyList<string> SplitCsv(string s) =>
        s.Length == 0 ? [] : s.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);


    /// <summary>Converte un RichGraph (8 campi + edge tipizzati) nel WikiGraph classico
    /// (nodi/edge wikilink) che le funzioni GRAPH.* single-brain consumano. Gli edge tipizzati
    /// diventano WikiEdge(from,to,null); il frontmatter del nodo mantiene i 8 campi.</summary>
    public static Basic.AI.Graph.WikiGraph ToWikiGraph(RichGraph g)
    {
        var wg = new Basic.AI.Graph.WikiGraph();
        foreach (var n in g.Nodes.Values)
        {
            var fm = new Dictionary<string, List<string>>(StringComparer.Ordinal);
            if (n.Author is not null) fm["author"] = [n.Author];
            if (n.Date is not null) fm["date"] = [n.Date];
            if (n.Strategy is not null) fm["strategy"] = [n.Strategy];
            if (n.Asset is not null) fm["asset"] = [n.Asset];
            if (n.Concepts.Count > 0) fm["concept"] = [.. n.Concepts];
            if (n.Tags.Count > 0) fm["tags"] = [.. n.Tags];
            fm["folder"] = [n.Folder];
            wg.Nodes[n.Path] = new Basic.AI.Graph.WikiNode(n.Path, n.Title, fm);
        }
        foreach (var e in g.Edges)
            wg.Edges.Add(new Basic.AI.Graph.WikiEdge(e.Source, e.Target, null));
        wg.Dangling.AddRange(g.Dangling);
        foreach (var (k, v) in g.FolderCounts) wg.FolderCounts[k] = v;
        return wg;
    }
}
