// FILE: Basic.AI.Knowledge.BrainToolchain
// Gestione dei cervelli da REPL/CLI (V1 del TODO Brain-Toolchain-REPL-CLI):
// CREATE · STATS · VERIFY (integrità) · OPTIMIZE (rag) · BACKUP (+ rename/remove artefatti).
//
// Principi: operazioni di TOOLING (nessuna superficie BASIC nuova — ADR content-agnostic);
// una sola implementazione usata da `BRAIN …` (REPL) e `gmb brain …` (CLI); `--dry-run` dove si
// scrive; scritture atomiche (dir temporanea + rename); errori ESPLICITI (mai no-op silenziosi);
// manifest coerente (BrainManifest/BrainPackager).
//
// V1 = integrità e operatività quotidiana. DEDUPE/EXTRACT/MERGE = V2, CONVERT/qualità-golden = V3
// (vedi AiWiki Todo/(open) - Brain-Toolchain-REPL-CLI.md).

using System.Text;
using System.Text.Json;
using Basic.AI.Rag;
using Microsoft.Data.Sqlite;

namespace Basic.AI.Knowledge;

/// <summary>Esito di un'operazione del toolchain (testo da mostrare + ok/errore).</summary>
public sealed record ToolResult(bool Ok, string Text, string? Error = null)
{
    public static ToolResult Fail(string error) => new(false, "", error);
}

/// <summary>Operazioni di gestione dei cervelli (REPL/CLI). V1.</summary>
public static partial class BrainToolchain
{
    // ── STATS ─────────────────────────────────────────────────────────────
    /// <summary>Statistiche del cervello: modalità, documenti/nodi, dimensione artefatti, manifest.</summary>
    public static ToolResult Stats(string name, bool asJson = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        var info = BrainRegistry.Info(name);
        if (info is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");

        var sb = new StringBuilder();
        long dbBytes = 0, chunkCount = 0, docCount = 0;
        int graphNodes = 0, graphEdges = 0, graphDangling = 0, communities = 0;

        if (BrainRegistry.RagDbPath(name) is { } dbPath && File.Exists(dbPath))
        {
            dbBytes = new FileInfo(dbPath).Length;
            try
            {
                using var conn = new SqliteConnection($"Data Source={dbPath};Mode=ReadOnly");
                conn.Open();
                docCount = Scalar(conn, "SELECT COUNT(*) FROM vec_documents");
                chunkCount = Scalar(conn, "SELECT COUNT(*) FROM vec_chunks");
            }
            catch (Exception ex) { return ToolResult.Fail($"db RAG illeggibile ({dbPath}): {ex.Message}"); }
        }

        if (BrainRegistry.GraphJsonPath(name) is { } graphPath && File.Exists(graphPath))
        {
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(graphPath));
                if (doc.RootElement.TryGetProperty("nodes", out var nodes) && nodes.ValueKind == JsonValueKind.Array)
                    graphNodes = nodes.GetArrayLength();
                if (doc.RootElement.TryGetProperty("edges", out var edges) && edges.ValueKind == JsonValueKind.Array)
                    graphEdges = edges.GetArrayLength();
                if (doc.RootElement.TryGetProperty("dangling", out var dang) && dang.ValueKind == JsonValueKind.Array)
                    graphDangling = dang.GetArrayLength();
            }
            catch (Exception ex) { return ToolResult.Fail($"grafo illeggibile ({graphPath}): {ex.Message}"); }
            var statsPath = Path.ChangeExtension(graphPath, ".stats.json");
            if (File.Exists(statsPath))
            {
                try
                {
                    using var st = JsonDocument.Parse(File.ReadAllText(statsPath));
                    if (st.RootElement.TryGetProperty("communities", out var c) && c.ValueKind == JsonValueKind.Number)
                        communities = c.GetInt32();
                }
                catch { /* stats opzionale */ }
            }
        }

        if (asJson)
        {
            return new ToolResult(true, JsonSerializer.Serialize(new
            {
                name = info.Name,
                mode = info.Mode,
                available = info.Available,
                attached = info.Attached,
                docs = info.Docs,
                rag = new { present = dbBytes > 0, bytes = dbBytes, documents = docCount, chunks = chunkCount },
                graph = new { present = graphNodes > 0, nodes = graphNodes, edges = graphEdges, dangling = graphDangling, communities },
                checksum = info.Checksum,
                vaultPath = info.VaultPath,
                brainRoot = BrainRegistry.BrainRoot(name)
            }));
        }

        sb.AppendLine($"BRAIN STATS — {info.Name}");
        sb.AppendLine($"  mode:       {info.Mode}   available: [{string.Join(", ", info.Available)}]");
        sb.AppendLine($"  attached:   {info.Attached}");
        sb.AppendLine($"  docs:       {(info.Docs > 0 ? info.Docs : docCount)}");
        if (dbBytes > 0) sb.AppendLine($"  rag:        {docCount} documenti, {chunkCount} chunk, {dbBytes / 1024.0 / 1024.0:F1} MB");
        else sb.AppendLine("  rag:        assente");
        if (graphNodes > 0) sb.AppendLine($"  graph:      {graphNodes} nodi, {graphEdges} edge, {graphDangling} dangling, {communities} community");
        else sb.AppendLine("  graph:      assente");
        sb.AppendLine($"  checksum:   {info.Checksum ?? "-"}");
        sb.AppendLine($"  root:       {BrainRegistry.BrainRoot(name) ?? "-"}");
        return new ToolResult(true, sb.ToString().TrimEnd());
    }

    // ── VERIFY (integrità) ────────────────────────────────────────────────
    /// <summary>Verifica di integrità: manifest+checksum, db RAG apribile/coerente, grafo deserializzabile.
    /// Ritorna ok=false se una modalità PRESENTE non è integra (le modalità assenti non sono errori).</summary>
    public static ToolResult Verify(string name, bool asJson = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        var info = BrainRegistry.Info(name);
        if (info is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");

        var checks = new List<(string Name, bool Ok, string Detail)>();
        var manifest = BrainRegistry.TryLoadManifest(name);
        var hasRagFile = BrainRegistry.RagDbPath(name) is { } p1 && File.Exists(p1);
        var hasGraphFile = BrainRegistry.GraphJsonPath(name) is { } p2 && File.Exists(p2);

        // 1) manifest + checksum dei file. Manifest ASSENTE non è un errore: i cervelli costruiti
        // localmente (RAG BUILD/GRAPH BUILD) non lo hanno — lo produce EXPORT/INSTALL. È un errore solo
        // un manifest PRESENTE con checksum incoerenti (file mancanti o modificati).
        if (manifest is null) checks.Add(("manifest", true, "assente (brain costruito localmente: esportabile con BRAIN BACKUP)"));
        else
        {
            var files = manifest.Files ?? [];
            var bad = new List<string>();
            foreach (var (rel, expected) in files)
            {
                var full = Path.Combine(BrainRegistry.BrainRoot(name) ?? "", rel.Replace('/', Path.DirectorySeparatorChar));
                if (!File.Exists(full)) { bad.Add($"{rel}: mancante"); continue; }
                var actual = BrainManifest.HashFile(full);
                if (!string.Equals(actual, expected, StringComparison.OrdinalIgnoreCase)) bad.Add($"{rel}: checksum diverso");
            }
            checks.Add(("manifest.checksum", bad.Count == 0,
                bad.Count == 0 ? $"{files.Count} file verificati" : string.Join("; ", bad.Take(5))));
        }

        // 2) RAG: db apribile, integrità SQLite, dimensioni embedding coerenti
        if (BrainRegistry.RagDbPath(name) is { } dbPath && File.Exists(dbPath))
        {
            try
            {
                using var conn = new SqliteConnection($"Data Source={dbPath};Mode=ReadOnly");
                conn.Open();
                var integrity = ScalarText(conn, "PRAGMA integrity_check");
                checks.Add(("rag.integrity", integrity.Equals("ok", StringComparison.OrdinalIgnoreCase), integrity));
                var docs = Scalar(conn, "SELECT COUNT(*) FROM vec_documents");
                var chunks = Scalar(conn, "SELECT COUNT(*) FROM vec_chunks");
                checks.Add(("rag.contents", chunks > 0, $"{docs} documenti, {chunks} chunk"));
                var dims = Scalar(conn, "SELECT COUNT(*) FROM vec_embeddings");
                checks.Add(("rag.embeddings", chunks == 0 || dims > 0, $"{dims} embedding (chunk attesi {chunks})"));
                var fts = Scalar(conn, "SELECT COUNT(*) FROM fts_chunks");
                checks.Add(("rag.fts", fts == 0 || fts > 0, $"{fts} righe FTS5"));
            }
            catch (Exception ex) { checks.Add(("rag.open", false, ex.Message)); }
        }

        // 3) GRAF: json deserializzabile + nodi coerenti con gli edge
        if (BrainRegistry.GraphJsonPath(name) is { } graphPath && File.Exists(graphPath))
        {
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(graphPath));
                var nodes = doc.RootElement.TryGetProperty("nodes", out var n) ? n.GetArrayLength() : 0;
                var edges = doc.RootElement.TryGetProperty("edges", out var e) ? e.GetArrayLength() : 0;
                var dangling = doc.RootElement.TryGetProperty("dangling", out var d) ? d.GetArrayLength() : 0;
                checks.Add(("graph.json", nodes > 0, $"{nodes} nodi, {edges} edge, {dangling} dangling"));
                var ratio = edges > 0 ? (double)dangling / edges : 0;
                checks.Add(("graph.dangling", ratio < 0.5, $"rapporto dangling {ratio:P0} (soglia 50%)"));
            }
            catch (Exception ex) { checks.Add(("graph.open", false, ex.Message)); }
        }

        // Nessun artefatto su disco = cervello REGISTRATO MA VUOTO (es. l'artefatto è stato cancellato):
        // è un problema, non un "niente da verificare" (l'utente si aspetta che il cervello funzioni).
        if (!hasRagFile && !hasGraphFile)
            checks.Add(("artefatti", false, "né db RAG né grafo presenti su disco: il cervello è registrato ma vuoto (ricostruire con BRAIN CREATE o RAG/GRAPH BUILD)"));

        var failed = checks.Where(c => !c.Ok).ToList();
        if (asJson)
        {
            return new ToolResult(failed.Count == 0, JsonSerializer.Serialize(new
            {
                name,
                ok = failed.Count == 0,
                checks = checks.Select(c => new { name = c.Name, ok = c.Ok, detail = c.Detail })
            }, new JsonSerializerOptions { WriteIndented = true }));
        }
        var sb = new StringBuilder();
        sb.AppendLine($"BRAIN VERIFY — {name}: {(failed.Count == 0 ? "OK" : $"{failed.Count} PROBLEMI")}");
        foreach (var c in checks) sb.AppendLine($"  [{(c.Ok ? "ok" : "KO")}] {c.Name}: {c.Detail}");
        var text = sb.ToString().TrimEnd();
        return failed.Count == 0 ? new ToolResult(true, text) : new ToolResult(false, text, $"verifica fallita: {failed.Count} controlli KO");
    }

    // ── CREATE ────────────────────────────────────────────────────────────
    /// <summary>Costruisce un cervello da una cartella (`--from`): rag e/o graph, con registrazione opzionale.
    /// Riusa la pipeline ufficiale (`RagIndexService` / `GraphStore`+`GraphExporter`) — nessun builder nuovo.</summary>
    public static ToolResult Create(string name, string fromDir, string mode, bool register, bool activate, bool dryRun,
        Rag.BinaryTextExtractor? binaryExtractor = null)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        if (string.IsNullOrWhiteSpace(fromDir) || !Directory.Exists(fromDir))
            return ToolResult.Fail($"cartella di origine inesistente: '{fromDir}'");
        mode = (mode ?? "both").ToLowerInvariant();
        if (mode is not ("rag" or "graph" or "both")) return ToolResult.Fail("--mode attesi: rag|graph|both");

        var sb = new StringBuilder();
        var doRag = mode is "rag" or "both";
        var doGraph = mode is "graph" or "both";

        if (doRag)
        {
            var ragDir = AiConfig.RagDefaultDir;
            Directory.CreateDirectory(ragDir);
            var dbPath = Path.Combine(ragDir, name + ".db");
            if (dryRun)
            {
                sb.AppendLine($"[dry-run] RAG: {Path.GetFullPath(fromDir)} → {dbPath}");
            }
            else
            {
                // Attiva il provider come fa `gmb rag build` (RagDbSession.EnsureEmbeddingFromProviders):
                // senza, CREATE rag/both falliva sempre con "nessun provider attivo" pur essendoci un
                // provider reale configurato (providers.json) — trovato dal demo tools/brain_toolchain_demo.sh.
                try { EnsureEmbeddingProviderForToolchain(); }
                catch (Exception ex) { return ToolResult.Fail($"CREATE rag impossibile: {ex.Message}"); }
                var alias = RagDbSession.DefaultAlias(dbPath);
                RagDbSession.Open(dbPath, alias);
                RagDbSession.EnableWal(dbPath);
                var result = RagIndexService.Run(new RagBuildOptions(fromDir, alias, dbPath, AiConfig.RagCollection,
                        AiConfig.RagChunkOptions(), DryRun: false, Force: false, Prune: false,
                        BinaryTextExtractor: binaryExtractor),
                    progress: null, log: null);
                sb.AppendLine($"RAG: {result.Indexed} indicizzati, {result.Updated} aggiornati, {result.Skipped} invariati — {result.Elapsed.TotalSeconds:F1}s → {dbPath}");
            }
        }

        if (doGraph)
        {
            var graphsDir = AiConfig.GraphsDefaultDir;
            Directory.CreateDirectory(graphsDir);
            if (dryRun)
            {
                sb.AppendLine($"[dry-run] GRAPH: {Path.GetFullPath(fromDir)} → {Path.Combine(graphsDir, name + ".json")}");
            }
            else
            {
                var graph = Graph.GraphStore.Build(Path.GetFullPath(fromDir));
                var export = Graph.GraphExporter.Export(graph, name, AiConfig.GraphPageRankDamping, AiConfig.GraphPageRankIterations, withHtml: true);
                File.WriteAllText(Path.Combine(graphsDir, name + ".mermaid"), export.Mermaid);
                File.WriteAllText(Path.Combine(graphsDir, name + ".graphml"), export.Graphml);
                File.WriteAllText(Path.Combine(graphsDir, name + ".json"), export.Json);
                File.WriteAllText(Path.Combine(graphsDir, name + ".stats.json"), export.StatsJson);
                if (export.Html is not null) File.WriteAllText(Path.Combine(graphsDir, name + ".html"), export.Html);
                sb.AppendLine($"GRAPH: {graph.NodeCount} nodi, {graph.Edges.Count} edge, {graph.Dangling.Count} dangling → {Path.Combine(graphsDir, name + ".json")}");
            }
        }

        if (!dryRun && register)
        {
            BrainRegistry.Register(name, source: Path.GetFullPath(fromDir), vaultPath: Path.GetFullPath(fromDir), mode: mode);
            sb.AppendLine($"registrato: {name} (mode {mode})");
            if (activate)
            {
                BrainRegistry.Use(name);
                sb.AppendLine($"attivato: {name}");
            }
        }
        return new ToolResult(true, sb.ToString().TrimEnd());
    }

    // ── REGISTER (censisce artefatti già presenti su disco) ───────────────
    /// <summary>
    /// Registra nel registry un cervello i cui artefatti ESISTONO GIÀ su disco (costruiti con
    /// `gmb raf/graph build`): senza registrazione il cervello è utilizzabile (`BRAIN USE`) ma non compare
    /// in `BRAIN LIST`/`BRAIN INFO`/`BRAIN STATS`. `mode` (rag|graph|both) è dedotta se omessa.
    /// </summary>
    public static ToolResult Register(string name, string? mode = null, string? source = null, bool activate = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        var modes = BrainRegistry.AvailableModes(name);
        if (modes.Length == 0)
            return ToolResult.Fail($"cervello '{name}': nessun artefatto su disco (rag/<name>.db o graphs/<name>.json): "
                + "costruiscilo con `gmb rag build`/`gmb graph build` o `BRAIN CREATE`");
        var inferred = mode ?? (modes.Length > 1 ? "both" : modes[0]);
        if (inferred is not ("rag" or "graph" or "both"))
            return ToolResult.Fail("--mode attesi: rag|graph|both");
        BrainRegistry.Register(name, source: source, mode: inferred);
        var sb = new StringBuilder($"registrato: {name} (mode {inferred}, available [{string.Join(", ", modes)}])");
        if (activate)
        {
            var ok = BrainRegistry.Use(name);
            sb.Append(ok ? " — collegato" : " — collegamento NON riuscito (verifica gli artefatti)");
        }
        return new ToolResult(true, sb.ToString());
    }

    // ── BUILDVAULT (brain di auto-conoscenza: StdLib → pagine + RAG) ──────
    /// <summary>
    /// Rigenera il **brain di auto-conoscenza** (`brainvault`): pagine `.md` per namespace dalla StdLib
    /// (`BrainVaultBuilder`) + indice RAG in `<rag>/brainvault.db` + registrazione. È lo stesso percorso di
    /// `AI.BRAINVAULTBUILD` dal BASIC, ma eseguibile da CLI **attivando prima il provider embedding** come fa
    /// `gmb rag build` (senza, il runtime IL rifiuta con "richiede un provider embedding attivo").
    /// Usato anche per il RIPRISTINO dopo la cancellazione di `rag/` (incidente 2026-09-16).
    /// </summary>
    public static ToolResult BuildVault(string? outputDir = null)
    {
        try
        {
            var emb = Rag.RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
            if (emb is null)
                return ToolResult.Fail("nessun provider embedding reale disponibile: configura i provider (`gmb providers`) "
                    + "o usa un provider embedding non-fixture, poi riprova");
            var dir = outputDir ?? Path.Combine(Path.GetTempPath(), "gmbasic-brainvault-pages");
            var (written, indexed) = BrainVaultBuilder.BuildBrainVault(dir);
            if (written == 0) return ToolResult.Fail("nessuna pagina generata (StdLib vuota?)");
            var dbPath = BrainRegistry.RagDbPath("brainvault") ?? Path.Combine(AiConfig.RagDefaultDir, "brainvault.db");
            return new ToolResult(true, $"BRAIN BUILDVAULT: {written} pagine in {dir}, {indexed} indicizzate → {dbPath}");
        }
        catch (Exception ex) { return ToolResult.Fail($"BUILDVAULT fallito: {ex.Message}"); }
    }

    // ── OPTIMIZE (rag; graph = verifica) ──────────────────────────────────
    /// <summary>Manutenzione del cervello: RAG = VACUUM + PRAGMA optimize + FTS5 optimize (opz. rebuild);
    /// GRAPH = ricalcolo degli export dai dati correnti (pagerank/community) senza toccare i nodi.</summary>
    public static ToolResult Optimize(string name, bool dryRun = false, bool reindexFts = false)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        var info = BrainRegistry.Info(name);
        if (info is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");

        var sb = new StringBuilder();
        if (BrainRegistry.RagDbPath(name) is { } dbPath && File.Exists(dbPath))
        {
            var before = new FileInfo(dbPath).Length;
            if (dryRun)
            {
                sb.AppendLine($"[dry-run] RAG {dbPath}: VACUUM + PRAGMA optimize{(reindexFts ? " + FTS5 rebuild" : " + FTS5 optimize")} (attuale {before / 1024.0 / 1024.0:F1} MB)");
            }
            else
            {
                using (var conn = new SqliteConnection($"Data Source={dbPath}"))
                {
                    conn.Open();
                    Exec(conn, "PRAGMA optimize");
                    if (reindexFts)
                    {
                        Exec(conn, "INSERT INTO fts_chunks(fts_chunks) VALUES('rebuild')");
                        sb.AppendLine("RAG: FTS5 rebuild");
                    }
                    else
                    {
                        Exec(conn, "INSERT INTO fts_chunks(fts_chunks) VALUES('optimize')");
                        sb.AppendLine("RAG: FTS5 optimize");
                    }
                }
                // VACUUM richiede la connessione senza transazione attive: connessione dedicata.
                using (var vacuum = new SqliteConnection($"Data Source={dbPath}"))
                {
                    vacuum.Open();
                    Exec(vacuum, "VACUUM");
                }
                var after = new FileInfo(dbPath).Length;
                sb.AppendLine($"RAG: {before / 1024.0 / 1024.0:F1} MB → {after / 1024.0 / 1024.0:F1} MB");
            }
        }
        else sb.AppendLine("RAG: assente (niente da ottimizzare)");

        if (BrainRegistry.GraphJsonPath(name) is { } graphPath && File.Exists(graphPath))
        {
            if (dryRun)
            {
                sb.AppendLine($"[dry-run] GRAPH {graphPath}: ricalcolo export (pagerank/community)");
            }
            else
            {
                var graph = Graph.WikiGraph.LoadFromJson(File.ReadAllText(graphPath));
                var export = Graph.GraphExporter.Export(graph, name, AiConfig.GraphPageRankDamping, AiConfig.GraphPageRankIterations, withHtml: true);
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(graphPath)!, name + ".stats.json"), export.StatsJson);
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(graphPath)!, name + ".mermaid"), export.Mermaid);
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(graphPath)!, name + ".graphml"), export.Graphml);
                if (export.Html is not null) File.WriteAllText(Path.Combine(Path.GetDirectoryName(graphPath)!, name + ".html"), export.Html);
                sb.AppendLine("GRAPH: export ricalcolati (stats/mermaid/graphml/html)");
            }
        }
        else sb.AppendLine("GRAPH: assente (niente da ottimizzare)");

        return new ToolResult(true, sb.ToString().TrimEnd());
    }

    // ── RENAME (V1b) ──────────────────────────────────────────────────────
    private static readonly string[] ArtifactExtensions =
        ["json", "stats.json", "mermaid", "graphml", "html", "report.md"];

    /// <summary>Rinomina un cervello: sposta gli artefatti (rag db + state, export del grafo, dir import)
    /// e aggiorna il registry (la modalità/stato collegato si conservano). `--dry-run` mostra le mosse.
    /// MOVE (verso un'altra root di cervelli) non è supportato: la root è globale (`AiConfig.BrainsDefaultDir`).</summary>
    public static ToolResult Rename(string name, string newName, bool dryRun = false)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(newName))
            return ToolResult.Fail("servono nome attuale e nuovo nome");
        if (string.Equals(name, newName, StringComparison.OrdinalIgnoreCase))
            return ToolResult.Fail("il nuovo nome è identico a quello attuale");
        var info = BrainRegistry.Info(name);
        if (info is null) return ToolResult.Fail($"cervello '{name}' non registrato (BRAIN LIST)");
        if (BrainRegistry.Info(newName) is not null || BrainRegistry.AvailableModes(newName).Length > 0
            || File.Exists(Path.Combine(AiConfig.RagDefaultDir, newName + ".db"))
            || File.Exists(Path.Combine(AiConfig.GraphsDefaultDir, newName + ".json"))
            || Directory.Exists(Path.Combine(AiConfig.BrainsDefaultDir, newName)))
            return ToolResult.Fail($"il nome '{newName}' è già in uso (cervello registrato o artefatti presenti)");

        var moves = new List<(string From, string To)>();
        var ragDir = AiConfig.RagDefaultDir;
        foreach (var suffix in new[] { ".db", ".db-wal", ".db-shm", ".state.json" })
        {
            var from = Path.Combine(ragDir, name + suffix);
            if (File.Exists(from)) moves.Add((from, Path.Combine(ragDir, newName + suffix)));
        }
        var graphsDir = AiConfig.GraphsDefaultDir;
        foreach (var ext in ArtifactExtensions)
        {
            var from = Path.Combine(graphsDir, name + "." + ext);
            if (File.Exists(from)) moves.Add((from, Path.Combine(graphsDir, newName + "." + ext)));
        }
        var brainDir = Path.Combine(AiConfig.BrainsDefaultDir, name);
        if (Directory.Exists(brainDir)) moves.Add((brainDir, Path.Combine(AiConfig.BrainsDefaultDir, newName)));

        if (moves.Count == 0) return ToolResult.Fail($"cervello '{name}': nessun artefatto da rinominare");
        if (dryRun)
        {
            var dry = new StringBuilder($"[dry-run] RENAME {name} → {newName} ({moves.Count} elementi):");
            foreach (var (from, to) in moves) dry.AppendLine($"  {from} → {to}");
            dry.AppendLine($"  registry: register('{newName}') + unregister('{name}')"
                + (info.Attached ? $" + ri-collegamento ('{newName}')" : ""));
            return new ToolResult(true, dry.ToString().TrimEnd());
        }

        var sb = new StringBuilder();
        foreach (var (from, to) in moves)
        {
            try
            {
                if (Directory.Exists(from)) Directory.Move(from, to);
                else File.Move(from, to, overwrite: false);
                sb.AppendLine($"  {from} → {to}");
            }
            catch (Exception ex) { return ToolResult.Fail($"rinomina interrotta su '{from}': {ex.Message}"); }
        }
        // dir import: gli artefatti interni sono nominati col nome del cervello → rinominali e aggiorna il manifest
        var newDir = Path.Combine(AiConfig.BrainsDefaultDir, newName);
        if (Directory.Exists(newDir))
        {
            foreach (var inner in new[] { $"rag/{name}.db", $"rag/{name}.state.json", $"graphs/{name}.json" })
            {
                var from = Path.Combine(newDir, inner.Replace('/', Path.DirectorySeparatorChar));
                var to = Path.Combine(newDir, inner.Replace('/' + name, '/' + newName).Replace('/', Path.DirectorySeparatorChar));
                if (File.Exists(from) && !File.Exists(to)) File.Move(from, to);
            }
            var manifestPath = Path.Combine(newDir, "brain.manifest.json");
            if (File.Exists(manifestPath))
            {
                try
                {
                    if (BrainRegistry.TryLoadManifest(newName) is { } m)
                    {
                        m.Name = newName;
                        m.Files = m.Files.ToDictionary(
                            kv => kv.Key.Replace(name, newName, StringComparison.Ordinal),
                            kv => kv.Value, StringComparer.Ordinal);
                        File.WriteAllText(manifestPath, m.ToJson(withChecksum: true));
                    }
                }
                catch { /* manifest non riscrivibile: resta il file, VERIFY lo segnalerà */ }
            }
        }
        // registry
        BrainRegistry.Register(newName, source: info.Source, vaultPath: info.VaultPath, mode: info.Mode);
        BrainRegistry.Unregister(name);
        var reattached = false;
        if (info.Attached)
        {
            BrainRegistry.Detach(name);
            reattached = BrainRegistry.Use(newName);
        }
        sb.AppendLine($"registry: '{name}' → '{newName}' (mode {info.Mode})" + (reattached ? ", ri-collegato" : ""));
        return new ToolResult(true, $"RENAME {name} → {newName}:" + Environment.NewLine + sb.ToString().TrimEnd());
    }

    // ── BACKUP (= EXPORT con nome timestamp) ──────────────────────────────
    /// <summary>Zip `.gmbrain` del cervello (riusa BrainPackager); se `--out` è omesso il file va in
    /// `brains-exports/<name>-<timestamp>.gmbrain.zip`.</summary>
    public static ToolResult Backup(string name, string? outZip = null)
    {
        if (string.IsNullOrWhiteSpace(name)) return ToolResult.Fail("serve il nome del cervello");
        if (string.IsNullOrWhiteSpace(outZip))
        {
            var dir = AiConfig.BrainsExportsDir;
            Directory.CreateDirectory(dir);
            outZip = Path.Combine(dir, $"{name}-{DateTime.Now:yyyyMMdd-HHmmss}.gmbrain.zip");
        }
        var result = BrainPackager.Export(name, outZip);
        return result is null
            ? ToolResult.Fail($"cervello '{name}' non esportabile (nessun artefatto rag/graph)")
            : new ToolResult(true, $"backup: {result.ZipPath} — available: [{string.Join(", ", result.Available)}]");
    }

    /// <summary>
    /// Hook TEST-SCOPED (pattern del repo: `AiConfig.OverridePathForTests`,
    /// `BrainRegistry.OverridePathForTests`): disattiva la validazione «provider embedding REALE»
    /// delle operazioni del toolchain. Serve ai test con `FixtureEmbeddingProvider` (deterministico),
    /// che in PRODUZIONE è giustamente rifiutato. Mai true fuori dai test.
    /// </summary>
    internal static bool SkipEmbeddingValidationForTests { get; set; }

    /// <summary>Validazione provider embedding per le operazioni del toolchain (con hook test-scoped).</summary>
    private static void EnsureEmbeddingProviderForToolchain()
    {
        if (SkipEmbeddingValidationForTests) return;
        RagDbSession.EnsureEmbeddingFromProviders(Basic.Providers.ProviderServer.Instance);
        RagIndexService.ValidateEmbeddingProvider();
    }

    // ── helper ────────────────────────────────────────────────────────────
    private static long Scalar(SqliteConnection conn, string sql)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        var v = cmd.ExecuteScalar();
        return v is null or DBNull ? 0 : Convert.ToInt64(v);
    }

    private static string ScalarText(SqliteConnection conn, string sql)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        return cmd.ExecuteScalar()?.ToString() ?? "";
    }

    private static void Exec(SqliteConnection conn, string sql)
    {
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        cmd.ExecuteNonQuery();
    }
}
