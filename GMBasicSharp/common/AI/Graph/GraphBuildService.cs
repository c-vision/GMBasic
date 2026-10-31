// FILE: Basic.AI.Graph.GraphBuildService
// Servizio di build/update del grafo wiki (nodi=file .md, edge=wikilink risolti, dangling
// contati). Scrive <name>.{graphml,mermaid,json,stats.json} in graphsDir + stato per resume.
// Riusabile da REPL (GRAPH BUILD/UPDATE), CLI (gmb graph ...) e BASIC/agenti.
using Basic.AI.Rag;

namespace Basic.AI.Graph;

/// <summary>Opzioni di build grafo.</summary>
public sealed record GraphBuildOptions(
    string WikiDir,
    string GraphsDir,
    string Name,
    bool DryRun = false,
    bool Force = false,
    bool Prune = false,
    bool Rerank = false,
    double PageRankDamping = 0.85,
    int PageRankIterations = 100,
    bool WithHtml = true,
    bool UseLouvain = false);

/// <summary>Risultato di un run di build grafo.</summary>
public sealed record GraphBuildResult(
    int Nodes, int Edges, int Dangling, int NewNodes, int ChangedNodes, int RemovedNodes,
    TimeSpan Elapsed, string StatsPath, string StatePath);

/// <summary>Callback di progresso del build grafo.</summary>
public delegate void GraphProgressHandler(int processed, int total, string currentFile);

public static class GraphBuildService
{
    /// <summary>Esegue il build (o update) del grafo con resume. <paramref name="log"/> per i
    /// messaggi, <paramref name="progress"/> per la barra.</summary>
    public static GraphBuildResult Run(GraphBuildOptions opts, GraphProgressHandler? progress = null,
        Action<string>? log = null, CancellationToken ct = default)
    {
        var wikiDir = Path.GetFullPath(opts.WikiDir);
        if (!Directory.Exists(wikiDir)) throw new DirectoryNotFoundException($"Wiki dir non trovata: {wikiDir}");
        if (!Directory.Exists(opts.GraphsDir)) Directory.CreateDirectory(opts.GraphsDir);

        var statePath = Path.Combine(opts.GraphsDir, opts.Name + ".state.json");
        var state = BuildState.Load(statePath);
        state.Kind = BuildKind.Graph;
        state.WikiDir = wikiDir;
        if (opts.Force) { state.Files.Clear(); state.ProcessedFiles = 0; }

        var files = new List<string>();
        var hashes = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var rel in GraphStore.EnumerateMarkdown(wikiDir))
        {
            try
            {
                hashes[rel] = BuildState.HashFile(Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar)));
                files.Add(rel);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                log?.Invoke($"?skip file non leggibile (path invalido?): {rel} — {ex.Message}");
            }
        }
        var diff = state.Diff(hashes);
        state.TotalFiles = files.Count;

        if (opts.DryRun)
        {
            log?.Invoke($"DRY-RUN {wikiDir}: {files.Count} file .md, nuovi: {diff.New.Count}, modificati: {diff.Changed.Count}, invariati: {diff.Unchanged.Count}, rimossi: {diff.Removed.Count}");
            return new GraphBuildResult(0, 0, 0, 0, 0, 0, TimeSpan.Zero, "", statePath);
        }

        // costruttore del grafo: per UPDATE ricostruisce solo i file toccati e merge con l'export
        // precedente (ri-parse completo è semplice e corretto: il costo è il parse, non il merge).
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var graph = GraphStore.Build(wikiDir);
        state.ProcessedFiles = graph.NodeCount;

        // prune nodi rimossi
        if (opts.Prune)
            foreach (var rel in diff.Removed) state.Remove(rel);

        foreach (var rel in diff.New.Concat(diff.Changed))
            state.MarkProcessed(rel, hashes[rel]);
        state.Save();

        // export
        var export = GraphExporter.Export(graph, opts.Name, opts.PageRankDamping, opts.PageRankIterations, opts.WithHtml, opts.UseLouvain);
        var basePath = Path.Combine(opts.GraphsDir, opts.Name);
        File.WriteAllText(basePath + ".graphml", export.Graphml);
        File.WriteAllText(basePath + ".mermaid", export.Mermaid);
        File.WriteAllText(basePath + ".json", export.Json);
        File.WriteAllText(basePath + ".stats.json", export.StatsJson);
        if (export.Html is not null) File.WriteAllText(basePath + ".html", export.Html);
        if (export.Report is not null) File.WriteAllText(basePath + ".report.md", export.Report);
        sw.Stop();

        return new GraphBuildResult(graph.NodeCount, graph.EdgeCount, graph.Dangling.Count,
            diff.New.Count, diff.Changed.Count, diff.Removed.Count, sw.Elapsed,
            basePath + ".stats.json", statePath);
    }
}
