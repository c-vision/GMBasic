// FILE: Basic.AI.Rag.RagIndexService
// Servizio di indicizzazione RAG di un wiki: enumerazione, frontmatter, chunk, embedding,
// resume/diff. Riusabile da REPL (RAG BUILD/UPDATE), CLI (gmb rag ...) e BASIC/agenti.
// Il collo di bottiglia è l'embedding (non SQLite): il servizio fa batching + dedup per hash +
// resume per-file e valida la presenza di un provider embedding REALE prima di partire.
using Basic.AI.Graph;
using Basic.SQLite.Vector;

namespace Basic.AI.Rag;

/// <summary>Opzioni di build RAG.</summary>
public sealed record RagBuildOptions(
    string WikiDir,
    string DbAlias,
    string DbPath,
    string Collection = "wiki",
    ChunkingOptions? Chunking = null,
    bool DryRun = false,
    bool Force = false,
    bool Prune = false,
    string? EmbeddingProviderName = null,
    BinaryTextExtractor? BinaryTextExtractor = null);

/// <summary>Risultato di un run di build.</summary>
public sealed record RagBuildResult(
    int Indexed, int Updated, int Skipped, int Removed, int TotalFiles,
    TimeSpan Elapsed, string StatePath, string DbPath);

/// <summary>Estrazione testo da file binari (pdf/docx/...). Il servizio resta indipendente da
/// ConversionLib (evita il ciclo di progetto ConversionLib→AI→ConversionLib): il chiamante
/// (REPL/CLI) fornisce il delegate che usa <c>DocumentConverter</c>. Se null, i binari sono
/// saltati (Skipped) senza errore.</summary>
public delegate string? BinaryTextExtractor(string filePath);

/// <summary>Callback di progresso (percentuale, file correnti, contatori).</summary>
public delegate void RagProgressHandler(int processed, int total, string currentFile, int indexed, int skipped);

public static class RagIndexService
{
    /// <summary>Estensioni indicizzate dal build RAG (markdown + convertibili).</summary>
    public static readonly IReadOnlySet<string> SupportedExtensions = new HashSet<string>(
        [".md", ".markdown", ".txt", ".html", ".pdf", ".docx", ".xlsx", ".csv", ".pptx", ".epub", ".rtf", ".odt", ".json", ".xml", ".yaml"],
        StringComparer.OrdinalIgnoreCase);

    /// <summary>Valida che ci sia un provider embedding REALE attivo (mai il fixture 8-dim).</summary>
    public static void ValidateEmbeddingProvider()
    {
        var provider = EmbeddingProviderResolver.Default;
        if (provider is null)
            throw new InvalidOperationException(
                "Nessun provider di embedding attivo. Registrane uno reale, es.: AI REGISTER emb, %EMBEDDING, llamacpp, {\"modelPath\":\"...gguf\"} + AI PROVIDER ACTIVE %EMBEDDING, emb");
        // Il fixture è sempre NON usabile per RAG (vettori deterministici a bassa dim): il check
        // sul tipo è il discriminator affidabile, a prescindere dalle dimensioni dichiarate.
        if (provider.GetType().Name.Contains("Fixture", StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException(
                "Provider di embedding FIXTURE (deterministico) — NON usare per RAG. Registra un provider reale (llamacpp/mlx/ollama/openai) via PROVIDERS WIZARD o AI REGISTER.");
    }

    /// <summary>Esegue il build (o update) RAG con resume/diff. <paramref name="openDb"/> apre il db
    /// SQLite su file con l'alias indicato (chiamata dal REPL/CLI che possiede il DataSession).</summary>
    public static RagBuildResult Run(RagBuildOptions opts, RagProgressHandler? progress = null,
        Action<string>? log = null, CancellationToken ct = default)
    {
        ValidateEmbeddingProvider();

        var wikiDir = Path.GetFullPath(opts.WikiDir);
        if (!Directory.Exists(wikiDir)) throw new DirectoryNotFoundException($"Wiki dir non trovata: {wikiDir}");

        var statePath = Path.ChangeExtension(opts.DbPath, ".state.json");
        var state = BuildState.Load(statePath);
        state.Kind = BuildKind.Rag;
        state.WikiDir = wikiDir;
        if (opts.Force) { state.Files.Clear(); state.ProcessedFiles = 0; }

        var chunking = opts.Chunking ?? new ChunkingOptions(2048, 256);
        var dims = EmbeddingProviderResolver.ActiveDimensions;

        // enumerazione + hash (per il diff/update)
        var files = new List<string>();
        var hashes = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var rel in EnumerateFiles(wikiDir))
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
        var total = files.Count;
        state.TotalFiles = total;

        if (opts.DryRun)
        {
            // stima: chunk attesi ≈ testo totale / (max-overlap)
            long totalChars = 0;
            foreach (var rel in files)
                totalChars += new FileInfo(Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar))).Length;
            var estChunks = Math.Max(1, (int)(totalChars / Math.Max(1, chunking.MaxChars - chunking.OverlapChars)));
            log?.Invoke($"DRY-RUN {wikiDir}: {total} file, {estChunks:N0} chunk stimati, dim embedding {dims}, provider {EmbeddingProviderResolver.Default?.GetType().Name}");
            log?.Invoke($"  nuovi: {diff.New.Count}, modificati: {diff.Changed.Count}, invariati: {diff.Unchanged.Count}, rimossi: {diff.Removed.Count}");
            return new RagBuildResult(0, 0, diff.Unchanged.Count, 0, total, TimeSpan.Zero, statePath, opts.DbPath);
        }

        // --- pipeline di indicizzazione ---
        AiVectorRuntime.EnsureCollection(opts.DbAlias, opts.Collection, dims);
        var indexed = 0; var updated = 0; var skipped = 0; var removed = 0;
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var pending = diff.New.Concat(diff.Changed).ToList();

        for (var i = 0; i < pending.Count; i++)
        {
            ct.ThrowIfCancellationRequested();
            var rel = pending[i];
            var full = Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar));
            var outcome = IndexOne(full, rel, opts.DbAlias, opts.Collection, chunking, opts.BinaryTextExtractor);
            if (outcome == IndexTextOutcome.Indexed) indexed++;
            else if (outcome == IndexTextOutcome.Updated) updated++;
            else skipped++;
            state.MarkProcessed(rel, hashes[rel]);
            if (i % 50 == 0) state.Save(); // checkpoint batch
            progress?.Invoke(i + 1, pending.Count, rel, indexed, skipped);
        }

        // prune (rimozioni) — solo se richiesto
        if (opts.Prune)
        {
            foreach (var rel in diff.Removed)
            {
                AiVectorRuntime.RemoveSource(opts.DbAlias, opts.Collection, Path.GetFullPath(Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar))));
                state.Remove(rel);
                removed++;
            }
        }

        state.Save();
        sw.Stop();
        return new RagBuildResult(indexed, updated, skipped + diff.Unchanged.Count, removed, total, sw.Elapsed, statePath, opts.DbPath);
    }

    /// <summary>Conta i file .md (o estensioni supportate) del wiki.</summary>
    /// <summary>Cartelle e file di SERVIZIO mai indicizzati (config dell'editor, VCS, cache): non sono
    /// contenuto del vault e inquinano il retrieval (osservato 2026-09-16: il brain `airline` aveva fra i
    /// documenti `/.obsidian/app.json`, `graph.json`, …). Vale per QUALSIASI vault (fix generico).</summary>
    private static readonly string[] ExcludedSegments =
        [".obsidian", ".git", ".trash", "node_modules", ".idea", ".vscode", "__pycache__", ".next"];

    private static bool IsServicePath(string fullPath)
    {
        var name = Path.GetFileName(fullPath);
        if (name is ".DS_Store" or "Thumbs.db") return true;
        var norm = fullPath.Replace('\\', '/');
        foreach (var seg in ExcludedSegments)
            if (norm.Contains("/" + seg + "/", StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    public static IReadOnlyList<string> EnumerateFiles(string wikiDir)
    {
        var result = new List<string>();
        foreach (var f in Directory.GetFiles(wikiDir, "*.*", SearchOption.AllDirectories))
        {
            var ext = Path.GetExtension(f);
            if (!SupportedExtensions.Contains(ext)) continue;
            if (IsServicePath(f)) continue;      // config editor / VCS / cache: mai contenuto del vault
            result.Add(WikiLinkParser.Normalize(Path.GetRelativePath(wikiDir, f)));
        }
        result.Sort(StringComparer.Ordinal);
        return result;
    }

    private static IndexTextOutcome IndexOne(string fullPath, string rel, string dbAlias, string collection,
        ChunkingOptions chunking, BinaryTextExtractor? binaryExtractor)
    {
        var text = ReadContent(fullPath, binaryExtractor);
        if (string.IsNullOrWhiteSpace(text)) return IndexTextOutcome.Skipped;

        var fm = FrontmatterParser.Parse(text);
        var body = fm.Body.Length > 0 ? fm.Body : text;
        var metadata = fm.Fields.Count > 0 ? FrontmatterParser.ToMetadataJson(fm) : null;

        // sourceId = path assoluto (per RemoveSource/--prune coerenza)
        var sourceId = Path.GetFullPath(fullPath);
        // Indicizza l'INTERO body in UNA chiamata: SqlVectorIndexer.IndexText fa il chunking
        // interno (TextChunker) con le opzioni. Prima qui si splittava in chunk e si chiamava
        // IndexTextWithMetadata per ognuno → ogni chiamata faceva DeleteBySourceId + rechunk,
        // cancellando il documento e i chunk della chiamata precedente (libro 725KB → 2 chunk
        // invece di ~400). Fix 2026-09-06 (doppio chunking + delete ricorsivo).
        return AiVectorRuntime.IndexTextWithMetadata(dbAlias, collection, sourceId, body, metadata, chunking);
    }

    /// <summary>Legge il contenuto testuale di un file indicizzabile. I formati binari (pdf/docx/...)
    /// NON sono convertiti qui: la conversione avviene nel chiamante (REPL/CLI, che referenzia
    /// ConversionLib) prima di chiamare il servizio — per i binari il chiamante passa già il testo
    /// estratto via DocumentConverter. Questo mantiene common/AI indipendente da ConversionLib.</summary>
    private static string ReadContent(string path, BinaryTextExtractor? binaryExtractor)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        if (ext is ".md" or ".markdown" or ".txt" or ".html" or ".json" or ".xml" or ".yaml" or ".csv")
            return File.ReadAllText(path);
        // Binari (pdf/docx/...): il chiamante (REPL/CLI) fornisce il delegate che usa
        // DocumentConverter. Se assente, il binario è saltato (Skipped) senza errore.
        return binaryExtractor?.Invoke(path) ?? "";
    }
}
