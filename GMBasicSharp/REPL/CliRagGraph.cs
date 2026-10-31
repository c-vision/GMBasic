// REPL/CliRagGraph.cs — comandi CLI unificati `gmb rag ...` / `gmb graph ...` per costruire
// RAG da wiki + grafi Obsidian. Riusa gli stessi servizi runtime dei comandi REPL
// (RagIndexService / GraphBuildService in common/AI) — stesse opzioni, stesso resume.
//   gmb rag build   <wikiDir> [--rag dir] [--name db] [--chunk N] [--overlap N] [--dry-run] [--force] [--prune]
//   gmb rag update  <wikiDir> ...   (alias: gmb rag refresh)
//   gmb rag status  [--rag dir] [--name db]
//   gmb rag query   "<q>" [--rag dir] [--name db] [--topk N]
//   gmb graph build <wikiDir> [--graphs dir] [--name nome] [--dry-run] [--force] [--prune]
//   gmb graph update <wikiDir> ...  (alias: gmb graph refresh)
//   gmb graph status [--graphs dir] [--name nome]
using Basic.AI.Graph;
using Basic.AI.Rag;
using Basic.SQLite.Vector;

namespace Basic.Executor;

public static class CliRagGraph
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0) return false;
        var root = args[0].ToLowerInvariant();
        if (root is not ("rag" or "graph")) return false;
        var rest = args.Skip(1).ToArray();
        Run(() =>
        {
            if (root == "rag") DispatchRag(rest);
            else DispatchGraph(rest);
        });
        return true;
    }

    private static void Run(Action fn)
    {
        try { fn(); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); }
    }

    // ── RAG ────────────────────────────────────────────────────────────────
    private static void DispatchRag(string[] args)
    {
        var cmd = args.Length > 0 ? args[0].ToLowerInvariant() : "help";
        switch (cmd)
        {
            case "build": RagBuild(args.Skip(1).ToArray(), isUpdate: false); break;
            case "update":
            case "refresh": RagBuild(args.Skip(1).ToArray(), isUpdate: true); break;
            case "status": RagStatus(args.Skip(1).ToArray()); break;
            case "query": RagQuery(args.Skip(1).ToArray()); break;
            default: RagHelp(); break;
        }
    }

    private static void RagBuild(string[] args, bool isUpdate)
    {
        var wikiDir = ArgValue(args, 0);
        if (wikiDir is null) { RagHelp(); return; }
        RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
        RagIndexService.ValidateEmbeddingProvider();

        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var dryRun = RagGraphFlags.Has(args, "--dry-run", "-D");
        var force = RagGraphFlags.Has(args, "--force", "-f");
        // ⚠ `--no-prune` (2026-09-16): un `update` su una cartella DIVERSA da quella del build originale
        // rimuoveva TUTTI i documenti dell'altra cartella (l'update implica prune e il diff è calcolato sul
        // wikiDir corrente). Con --no-prune si aggiungono più cartelle allo stesso db (build multi-root).
        var noPrune = RagGraphFlags.Has(args, "--no-prune", null);
        var prune = !noPrune && (RagGraphFlags.Has(args, "--prune", "-p") || isUpdate);
        var chunk = int.TryParse(RagGraphFlags.Value(args, "--chunk", "-c"), out var c) && c > 0 ? c : (int?)null;
        var overlap = int.TryParse(RagGraphFlags.Value(args, "--overlap", "-o"), out var o) && o >= 0 ? o : (int?)null;
        var collection = RagGraphFlags.Value(args, "--collection", "-C") ?? Basic.AI.AiConfig.RagCollection;
        var dbPath = Path.Combine(ragDir, name + ".db");
        // Alias runtime: default = basename del db (unico per db, così più RAG coesistono nello
        // stesso DataSession senza collidere sull'alias fisso "RagDb"); override con --alias/-A.
        var alias = RagGraphFlags.Value(args, "--alias", "-A") ?? RagDbSession.DefaultAlias(dbPath);

        RagDbSession.Open(dbPath, alias);
        RagDbSession.EnableWal(dbPath);
        var chunking = new ChunkingOptions(chunk ?? Basic.AI.AiConfig.RagChunkOptions().MaxChars,
            overlap ?? Basic.AI.AiConfig.RagChunkOptions().OverlapChars);

        Console.WriteLine($"{(dryRun ? "DRY-RUN" : isUpdate ? "UPDATE" : "BUILD")} RAG: {wikiDir} → {dbPath} (collection '{collection}', alias '{alias}')");
        var result = RagIndexService.Run(new RagBuildOptions(wikiDir, alias, dbPath, collection, chunking, dryRun, force, prune,
                BinaryTextExtractor: path => Basic.ConversionLib.DocumentConverter.Convert(path)),
            progress: null, Console.WriteLine);
        Console.WriteLine($"[OK] indicizzati {result.Indexed}, aggiornati {result.Updated}, skipped {result.Skipped}, rimossi {result.Removed} — {result.Elapsed.TotalSeconds:F1}s");
    }

    private static void RagStatus(string[] args)
    {
        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleArtifactName(ragDir, "*.db");
        if (name is null) { Console.Error.WriteLine("?db non specificato: usa --name (nessun *.db univoco in " + ragDir + ")"); return; }
        var dbPath = Path.Combine(ragDir, name + ".db");
        var statePath = Path.ChangeExtension(dbPath, ".state.json");
        // Mostra il provider attivo SENZA istanziarlo (GetActiveName non carica il modello).
        try { Basic.Providers.ProviderConfig.Load(); } catch { }
        var activeName = Basic.Providers.ProviderServer.Instance.GetActiveName("embedding");
        Console.WriteLine($"RAG STATUS — {ragDir}");
        Console.WriteLine($"  db: {(File.Exists(dbPath) ? "presente" : "assente")} ({dbPath})");
        Console.WriteLine($"  provider embedding attivo: {activeName ?? "(nessuno)"}" + (activeName?.StartsWith("fixture", StringComparison.OrdinalIgnoreCase) == true ? " — NON USARE per RAG" : ""));
        if (File.Exists(statePath))
        {
            var st = BuildState.Load(statePath);
            Console.WriteLine($"  stato: {st.ProcessedFiles}/{st.TotalFiles} file, {st.UpdatedAt:u}");
        }
    }

    private static void RagQuery(string[] args)
    {
        var query = args.Length > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (string.IsNullOrWhiteSpace(query)) { RagHelp(); return; }
        var ragDir = RagGraphFlags.Value(args, "--rag", "-r") ?? Basic.AI.AiConfig.RagDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleArtifactName(ragDir, "*.db");
        if (name is null) { Console.Error.WriteLine("?db non specificato: usa --name (nessun *.db univoco in " + ragDir + ")"); return; }
        var topk = int.TryParse(RagGraphFlags.Value(args, "--topk", "-t"), out var t) && t > 0 ? t : 5;
        var dbPath = Path.Combine(ragDir, name + ".db");
        if (!File.Exists(dbPath)) { Console.Error.WriteLine($"?db non trovato: {dbPath}"); return; }
        var alias = RagGraphFlags.Value(args, "--alias", "-A") ?? RagDbSession.DefaultAlias(dbPath);
        RagDbSession.Open(dbPath, alias);
        // La QUERY embed usa il provider REALE (come il build): senza questo
        // EmbeddingProviderResolver.Default è null → la query NON degrada più sul
        // FixtureEmbeddingProvider (P0a, decisione utente 2026-09-12) — fallisce esplicito.
        RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
        RagIndexService.ValidateEmbeddingProvider();
        var collection = RagGraphFlags.Value(args, "--collection", "-C") ?? Basic.AI.AiConfig.RagCollection;
        Console.WriteLine(AiVectorRuntime.SearchJson(alias, collection, query, topk));
    }

    // ── GRAPH ──────────────────────────────────────────────────────────────
    private static void DispatchGraph(string[] args)
    {
        var cmd = args.Length > 0 ? args[0].ToLowerInvariant() : "help";
        switch (cmd)
        {
            case "build": GraphBuild(args.Skip(1).ToArray(), isUpdate: false); break;
            case "update":
            case "refresh": GraphBuild(args.Skip(1).ToArray(), isUpdate: true); break;
            case "status": GraphStatus(args.Skip(1).ToArray()); break;
            case "view": GraphView(args.Skip(1).ToArray()); break;
            case "watch": GraphWatch(args.Skip(1).ToArray(), start: true); break;
            case "unwatch": GraphWatch(args.Skip(1).ToArray(), start: false); break;
            case "export":
            case "export-obsidian": GraphExportObsidian(args.Skip(1).ToArray()); break;
            case "enrich":
            case "enrich-semantic": GraphEnrichSemantic(args.Skip(1).ToArray()); break;
            default: GraphHelp(); break;
        }
    }

    /// <summary>gmb graph watch <wikiDir> [-g dir] [-n nome] / gmb graph unwatch <wikiDir>.</summary>
    private static void GraphWatch(string[] args, bool start)
    {
        var wikiDir = args.Length > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (start)
        {
            if (wikiDir is null) { Console.Error.WriteLine("?Serve il path del wiki: gmb graph watch <wikiDir>"); return; }
            var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
            var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
            try
            {
                Basic.AI.Graph.GraphWatchService.Start(wikiDir, graphsDir, name);
                Console.WriteLine($"[WATCH] osservo {Path.GetFullPath(wikiDir)} — ogni modifica .md rigenera il grafo (debounce ~1s). Ctrl+C per uscire.");
                Console.WriteLine($"[WATCH] cartelle osservate: {Basic.AI.Graph.GraphWatchService.WatchedCount}");
            }
            catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); return; }
            // blocca il processo finché non si interrompe (il watch gira in background)
            Console.CancelKeyPress += (_, e) => { e.Cancel = true; Basic.AI.Graph.GraphWatchService.StopAll(); Environment.Exit(0); };
            var mre = new System.Threading.ManualResetEventSlim(false);
            Console.CancelKeyPress += (_, _) => mre.Set();
            mre.Wait();
        }
        else
        {
            if (wikiDir is null) { Console.Error.WriteLine("?Serve il path del wiki: gmb graph unwatch <wikiDir>"); return; }
            var r = Basic.AI.Graph.GraphWatchService.Stop(wikiDir);
            Console.WriteLine(r == 1 ? $"[WATCH] fermato: {wikiDir}" : $"[WATCH] nessun watcher attivo su {wikiDir}");
        }
    }

    /// <summary>gmb graph export-obsidian [-g dir] [-n nome] [--out dir]: vault Obsidian dal .json.</summary>
    private static void GraphExportObsidian(string[] args)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleGraphName(graphsDir);
        if (name is null) { Console.Error.WriteLine("?graph non specificato: usa --name (nessun *.state.json univoco in " + graphsDir + ")"); return; }
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { Console.Error.WriteLine($"?json non trovato: {jsonPath} (esegui prima gmb graph build)"); return; }
        var outDir = RagGraphFlags.Value(args, "--out", "-o") ?? Path.Combine(graphsDir, name + "_obsidian");
        var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(File.ReadAllText(jsonPath));
        Basic.AI.Graph.GraphExporter.WriteObsidianVault(graph, name, outDir);
        Console.WriteLine($"[OK] vault Obsidian: {outDir} (index.md + communities/ — apri in Obsidian)");
    }

    /// <summary>gmb graph enrich-semantic <wikiDir> [-g dir] [-n nome] [--max N] [--batch N]:
    /// semantic extraction LLM (edge impliciti tra note, Type=semantic INFERRED). Richiede %CHAT.</summary>
    private static void GraphEnrichSemantic(string[] args)
    {
        var wikiDir = args.Length > 0 && !args[0].StartsWith("--") ? args[0] : null;
        if (wikiDir is null) { Console.Error.WriteLine("?Uso: gmb graph enrich-semantic <wikiDir> [-g dir] [-n nome]"); return; }
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { Console.Error.WriteLine($"?json non trovato: {jsonPath} (esegui prima gmb graph build)"); return; }
        var maxNotes = int.TryParse(RagGraphFlags.Value(args, "--max", "-m"), out var m) && m > 0 ? m : int.MaxValue;
        var batch = int.TryParse(RagGraphFlags.Value(args, "--batch", "-b"), out var b) && b > 0 ? b : 6;

        try { Basic.Providers.ProviderConfig.Load(); } catch { }
        Basic.AI.Rag.ILlmCompleter? llm;
        try { llm = Basic.Providers.ProviderServer.Instance.GetActiveCompletion("completion"); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message} — attiva un provider chat (AI SET ACTIVE %CHAT, nome)"); return; }

        var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(File.ReadAllText(jsonPath));
        var files = Basic.AI.Graph.GraphStore.EnumerateMarkdown(wikiDir)
            .Where(p => graph.Nodes.ContainsKey(p)).Take(maxNotes).ToList();
        if (files.Count == 0) { Console.Error.WriteLine("?nessuna nota nel grafo da analizzare"); return; }
        var notes = new List<Basic.AI.Graph.GraphSemanticNote>();
        foreach (var rel in files)
        {
            string text;
            try { text = File.ReadAllText(Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar))); } catch { continue; }
            var body = Basic.AI.Graph.FrontmatterParser.Parse(text).Body;
            notes.Add(new Basic.AI.Graph.GraphSemanticNote(rel,
                graph.Nodes[rel].Frontmatter.TryGetValue("title", out var t) && t.Count > 0 ? t[0] : rel,
                string.IsNullOrWhiteSpace(body) ? text : body));
        }
        Console.WriteLine($"[ENRICH SEMANTIC] {wikiDir} → analizzo {notes.Count} note con LLM ({llm.ModelName}), batch {batch} …");
        int added;
        try { added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(graph, notes, llm, batch); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); return; }
        Console.WriteLine(added < 0
            ? "[ENRICH SEMANTIC] nessun edge dedotto (risposta LLM non valida)"
            : $"[ENRICH SEMANTIC] aggiunti {added} edge semantici — rigenero gli export");
        if (added <= 0) return;
        var export = Basic.AI.Graph.GraphExporter.Export(graph, name, Basic.AI.AiConfig.GraphPageRankDamping, Basic.AI.AiConfig.GraphPageRankIterations, withHtml: true);
        var basePath = Path.Combine(graphsDir, name);
        File.WriteAllText(basePath + ".json", export.Json);
        File.WriteAllText(basePath + ".stats.json", export.StatsJson);
        if (export.Html is not null) File.WriteAllText(basePath + ".html", export.Html);
        if (export.Report is not null) File.WriteAllText(basePath + ".report.md", export.Report);
        Console.WriteLine($"[ENRICH SEMANTIC] export aggiornato: {basePath} (.json / .stats.json / .html)");
    }

    /// <summary>gmb graph view [-g dir] [-n nome] [--no-open]: apre il graph.html nel browser,
    /// rigenerandolo dal .json se assente o vecchio.</summary>
    private static void GraphView(string[] args)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? RagGraphFlags.SingleGraphName(graphsDir);
        if (name is null) { Console.Error.WriteLine("?graph non specificato: usa --name (nessun *.state.json univoco in " + graphsDir + ")"); return; }
        var jsonPath = Path.Combine(graphsDir, name + ".json");
        if (!File.Exists(jsonPath)) { Console.Error.WriteLine($"?json non trovato: {jsonPath} (esegui prima gmb graph build)"); return; }
        var htmlPath = Path.Combine(graphsDir, name + ".html");
        bool regen = !File.Exists(htmlPath) || File.GetLastWriteTimeUtc(jsonPath) > File.GetLastWriteTimeUtc(htmlPath).AddSeconds(1);
        if (regen)
        {
            Console.WriteLine($"VIEW: {jsonPath} → {htmlPath} …");
            var graph = Basic.AI.Graph.WikiGraph.LoadFromJson(File.ReadAllText(jsonPath));
            File.WriteAllText(htmlPath, Basic.AI.Graph.GraphExporter.ToHtml(graph, name));
            Console.WriteLine($"[OK] {htmlPath}");
        }
        else Console.WriteLine($"VIEW: {htmlPath} (già aggiornato)");
        if (RagGraphFlags.Has(args, "--no-open", "-O")) return;
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = htmlPath,
                UseShellExecute = true,
            });
        }
        catch (Exception ex) { Console.Error.WriteLine($"?apertura browser fallita: {ex.Message}"); }
    }

    private static void GraphBuild(string[] args, bool isUpdate)
    {
        var wikiDir = ArgValue(args, 0);
        if (wikiDir is null) { GraphHelp(); return; }
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n") ?? Path.GetFileName(wikiDir.TrimEnd('/', '\\'));
        var dryRun = RagGraphFlags.Has(args, "--dry-run", "-D");
        var force = RagGraphFlags.Has(args, "--force", "-f");
        var prune = RagGraphFlags.Has(args, "--prune", "-p") || isUpdate;
        var damping = double.TryParse(RagGraphFlags.Value(args, "--pagerank-damping", "-d"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var d) && d > 0 && d <= 1 ? d : Basic.AI.AiConfig.GraphPageRankDamping;
        var iterations = int.TryParse(RagGraphFlags.Value(args, "--pagerank-iterations", "-i"), out var it) && it > 0 ? it : Basic.AI.AiConfig.GraphPageRankIterations;
        var withHtml = !RagGraphFlags.Has(args, "--no-html", "-H");
        var louvain = RagGraphFlags.Has(args, "--louvain", "-L");

        Console.WriteLine($"{(dryRun ? "DRY-RUN" : isUpdate ? "UPDATE" : "BUILD")} GRAPH: {wikiDir} → {graphsDir}/{name} (pagerank damping {damping}, iter {iterations}, html {(withHtml ? "sì" : "no")}" + (louvain ? ", community Louvain" : "") + ")");
        var result = GraphBuildService.Run(new GraphBuildOptions(wikiDir, graphsDir, name, dryRun, force, prune,
                PageRankDamping: damping, PageRankIterations: iterations, WithHtml: withHtml, UseLouvain: louvain),
            progress: null, Console.WriteLine);
        Console.WriteLine($"[OK] nodi {result.Nodes}, edge {result.Edges}, dangling {result.Dangling} — {result.Elapsed.TotalSeconds:F1}s — stats {result.StatsPath}");
    }

    private static void GraphStatus(string[] args)
    {
        var graphsDir = RagGraphFlags.Value(args, "--graphs", "-g") ?? Basic.AI.AiConfig.GraphsDefaultDir;
        var name = RagGraphFlags.Value(args, "--name", "-n")
            ?? RagGraphFlags.SingleGraphName(graphsDir);
        if (name is null) { Console.Error.WriteLine("?graph non specificato: usa --name (nessun *.state.json univoco in " + graphsDir + ")"); return; }
        var statePath = Path.Combine(graphsDir, name + ".state.json");
        Console.WriteLine($"GRAPH STATUS — {graphsDir}");
        if (File.Exists(statePath))
        {
            var st = BuildState.Load(statePath);
            Console.WriteLine($"  stato: {st.ProcessedFiles}/{st.TotalFiles} file, {st.UpdatedAt:u}");
        }
        else Console.WriteLine("  stato: nessun build registrato");
    }

    // ── Help ───────────────────────────────────────────────────────────────
    private static void RagHelp()
    {
        Console.WriteLine("gmb rag build   <wikiDir> [-r dir] [-n db] [-A alias] [-C coll] [-c N] [-o N] [-D] [-f] [-p]");
        Console.WriteLine("gmb rag update  <wikiDir> [opzioni]   (alias: gmb rag refresh)");
        Console.WriteLine("gmb rag status  [-r dir] [-n db]");
        Console.WriteLine("gmb rag query   \"<query>\" [-r dir] [-n db] [-A alias] [-C coll] [-t N]");
        Console.WriteLine("  -r/--rag        cartella RAG (default: AiConfig.ragDefaultDir, <root>/rag/)");
        Console.WriteLine("  -n/--name       nome db (default: nome cartella wiki; status/query: l'unico *.db/*.state.json nella dir)");
        Console.WriteLine("  -A/--alias      alias runtime (default: basename del db — unico per db, così più RAG");
        Console.WriteLine("                  coesistono nella stessa sessione senza collidere sull'alias fisso)");
        Console.WriteLine("  -C/--collection nome collection RAG (default: AiConfig.ragCollection, 'wiki')");
        Console.WriteLine("  -c/--chunk      chunking MaxChars (default 2048, da config ragChunkMaxChars)");
        Console.WriteLine("  -o/--overlap    chunking Overlap (default 256, da config ragChunkOverlap)");
        Console.WriteLine("  -t/--topk       top-k della query (default 5)");
        Console.WriteLine("  -D/--dry-run    stima file/chunk/tempo senza indicizzare");
        Console.WriteLine("  -f/--force      ricostruisce da zero anche se db/stato esistono");
        Console.WriteLine("  -p/--prune      rimuove i file spariti dal wiki (update/refresh)");
        Console.WriteLine("  --no-prune      NON rimuovere nulla (per aggiungere più cartelle allo stesso db, build multi-root)");
    }

    private static void GraphHelp()
    {
        Console.WriteLine("gmb graph build   <wikiDir> [-g dir] [-n nome] [-d D] [-i N] [-D] [-f] [-p] [-H]");
        Console.WriteLine("gmb graph update  <wikiDir> [opzioni]   (alias: gmb graph refresh)");
        Console.WriteLine("gmb graph status  [-g dir] [-n nome]");
        Console.WriteLine("gmb graph view    [-g dir] [-n nome] [--no-open]   (apre graph.html nel browser)");
        Console.WriteLine("gmb graph watch   <wikiDir> [-g dir] [-n nome]      (auto-rebuild a ogni modifica .md)");
        Console.WriteLine("gmb graph unwatch <wikiDir>                         (ferma il watchdog)");
        Console.WriteLine("gmb graph export-obsidian [-g dir] [-n nome] [--out dir]   (vault Obsidian navigabile)");
        Console.WriteLine("gmb graph enrich-semantic <wikiDir> [-g dir] [-n nome] [--max N]   (LLM deduce relazioni implicite)");
        Console.WriteLine("  -g/--graphs              cartella graphs (default: <root>/graphs/)");
        Console.WriteLine("  -n/--name                nome (default: nome cartella wiki)");
        Console.WriteLine("  -d/--pagerank-damping    damping factor (default 0.85, config graphPageRankDamping)");
        Console.WriteLine("  -i/--pagerank-iterations iterazioni PageRank (default 100, config graphPageRankIterations)");
        Console.WriteLine("  -D/--dry-run             stima senza generare gli export");
        Console.WriteLine("  -f/--force               ricostruisce da zero");
        Console.WriteLine("  -p/--prune               rimuove i nodi dei file spariti (update/refresh)");
        Console.WriteLine("  -L/--louvain             community detection Louvain (modularity) invece del label propagation");
        Console.WriteLine("  -H/--no-html             salta graph.html (viewer) — per wiki molto grandi");
        Console.WriteLine("  GRAPH BUILD genera <name>.{graphml,mermaid,json,stats.json,html} (viewer interattivo).");
    }

    private static string? ArgValue(string[] args, int index) =>
        args.Length > index && !args[index].StartsWith("--") && !args[index].StartsWith("-") ? args[index] : null;
}
