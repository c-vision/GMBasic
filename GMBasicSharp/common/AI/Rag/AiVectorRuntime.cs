// FILE: Basic.AI.Rag.AiVectorRuntime
using Basic.Databases.Rdd;
using Basic.SQLite.Vector;
using Microsoft.Data.Sqlite;
using System.Text.Json;

namespace Basic.AI.Rag;

/// <summary>BASIC/GPM entry points for semantic vector search on open SQLite aliases.</summary>
public static class AiVectorRuntime
{
    /// <summary>DataSession condiviso (inizializzato dalla GPM all'avvio).</summary>
    public static DataSession? SqlDataSession { get; set; }

    /// <summary>Reranker di DEFAULT per i punti di accesso RAG (SearchJson/RagContext/Ask):
    /// KeywordReranker (zero-dependency, lessicale) attivo per default — misura 2026-09-08:
    /// AI.Tests Fast 468/468 PASS col default-on (prima: solo RRF). Leva di retrieval→rerank di
    /// QMD/fnd. Disattivabile impostando null; soglia di evidenza separata. NB: per un
    /// giudizio semantico vero sostituire con OnnxCrossEncoderReranker (richiede modello).</summary>
    public static Basic.SQLite.Vector.IReranker? DefaultReranker { get; set; } = new Basic.SQLite.Vector.KeywordReranker();

    /// <summary>Soglia di evidenza minima per il reranker di default (-inf = disabilitata).
    /// Con reranker attivo: se il miglior punteggio reranked è sotto soglia → InsufficientEvidence.</summary>
    public static double DefaultRerankEvidenceMin { get; set; } = double.NegativeInfinity;

    /// <summary>Traduce il modo-reranker BASIC (%RERANK_*) in un IReranker esplicito.
    /// 0 (%RERANK_OFF) → null (OFF, bypassa anche DefaultReranker); 1 (%RERANK_KEYWORD) →
    /// KeywordReranker; 2 (%RERANK_ONNX) → errore esplicito (non ancora wired); altri → errore.</summary>
    public static Basic.SQLite.Vector.IReranker? RerankerFor(double mode)
    {
        if (mode <= 0) return null;                                   // OFF
        if (mode == 1) return new Basic.SQLite.Vector.KeywordReranker();
        throw new InvalidOperationException(mode == 2
            ? "Reranker ONNX non ancora wired: usare %RERANK_KEYWORD o %RERANK_OFF."
            : $"Modo reranker sconosciuto: {mode} (usare %RERANK_OFF/%RERANK_KEYWORD/%RERANK_ONNX).");
    }

    private static SqlDriver ResolveDriver(string alias)
    {
        var ds = SqlDataSession ?? throw new InvalidOperationException("AiVectorRuntime.SqlDataSession not initialized.");
        var area = ds.FindAreaByAlias(alias);
        if (area != 0) return (SqlDriver)ds.GetWorkArea(area)!.Driver!;
        // Fallback: ":memory:" trova qualsiasi SQLite in-memory aperto
        if (alias.Equals(":memory:", StringComparison.OrdinalIgnoreCase))
        {
            for (int i = 1; i <= DataSession.MaxWorkareas; i++)
                if (ds.GetWorkArea(i)?.Driver is SqlDriver sd && sd.IsOpen)
                    return sd;
        }
        throw new InvalidOperationException($"SQL database '{alias}' is not open.");
    }

    private static void WithSqlConnection(string dbAlias, Action<SqliteConnection> action)
    {
        // 1) alias di un'area SQL già aperta (VECTORDB OPEN).
        try
        {
            ResolveDriver(dbAlias).WithConnection(conn =>
            {
                if (conn is SqliteConnection sc) action(sc);
                else throw new InvalidOperationException("VectorDB requires a SQLite connection.");
            });
            return;
        }
        catch (InvalidOperationException) { /* non è un alias aperto: prova come path */ }
        // 2a) `:memory:`: SQLite in-memory vive quanto la CONNESSIONE. Il runtime C cachea
        // l'handle per path (gmb_vecdb g_vdb), quindi `INDEXTEXT` poi `SEARCH` vedono lo stesso
        // DB. Qui serve la stessa persistenza: UNA connessione per alias, tenuta viva (prima ne
        // apriva una nuova a ogni chiamata → SEARCH su DB vuoto, divergenza vs VM/C).
        if (string.Equals(dbAlias, ":memory:", StringComparison.OrdinalIgnoreCase))
        {
            var mem = MemoryConns.GetOrAdd(dbAlias, _ =>
            {
                var c = new Microsoft.Data.Sqlite.SqliteConnection("Data Source=:memory:");
                c.Open();
                SqlVectorSchema.EnsureSchema(c);
                return c;
            });
            action(mem);
            return;
        }
        // 2b) path/file: apri (o crea) il DB SQLite al path indicato — parità col backend C.
        // Se dbAlias è una directory (es. un vault), usa un file DB al suo interno.
        var dbPath = System.IO.Directory.Exists(dbAlias) ? System.IO.Path.Combine(dbAlias, "vec.db") : dbAlias;
        using var conn2 = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={dbPath}");
        conn2.Open();
        SqlVectorSchema.EnsureSchema(conn2);
        action(conn2);
    }

    // Connessioni `:memory:` persistenti per alias (parità con la cache g_vdb del runtime C).
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, Microsoft.Data.Sqlite.SqliteConnection> MemoryConns = new();

    /// <summary>Chiude e svuota le connessioni `:memory:` persistenti. Isolamento per-RUN: in un
    /// host in-process che esegue PIÙ programmi (harness IL/PCode, REPL), il DB in-memory non deve
    /// trapelare da un run/leg all'altro (altrimenti il 2° run vede i dati del 1° → Updated invece
    /// di Indexed). Chiamata dal reset di stato del runtime e dall'harness di parità.</summary>
    public static void ResetMemoryConnections()
    {
        foreach (var kv in MemoryConns)
        { try { kv.Value.Dispose(); } catch { } }
        MemoryConns.Clear();
    }
    /// <summary><c>AIVECTOR.EMBED(text)</c> — returns L2-normalized embedding vector.</summary>
    public static float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        if (dimensions <= 0)
            throw new ArgumentOutOfRangeException(nameof(dimensions));

        var provider = EmbeddingProviderResolver.Default;
        if (provider is null)
            throw new InvalidOperationException("No embedding provider configured. Use PROVIDER.REGISTER to set one.");
        return provider.Embed(text, dimensions);
    }

    /// <summary><c>AIVECTOR.ENSURECOLLECTION</c> — returns internal collection id.</summary>
    public static long EnsureCollection(string dbAlias, string collection, int dimensions = 0, string? format = null)
    {
        var dims = dimensions > 0 ? dimensions : EmbeddingProviderResolver.ActiveDimensions;
        return WithCollection(dbAlias, collection, dims, static (_, _, collectionId) => collectionId, format);
    }

    /// <summary><c>AIVECTOR.INDEXTEXT</c> — ingest source text into a named collection.</summary>
    public static IndexTextOutcome IndexText(string dbAlias, string collection, string sourceId, string text) =>
        IndexTextWithMetadata(dbAlias, collection, sourceId, text, null);

    /// <summary><c>AIVECTOR.INDEXMETA</c> — ingest with governance metadata JSON.</summary>
    public static IndexTextOutcome IndexTextWithMetadata(
        string dbAlias,
        string collection,
        string sourceId,
        string text,
        string? metadataJson,
        Basic.SQLite.Vector.ChunkingOptions? chunking = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbAlias);
        ArgumentException.ThrowIfNullOrWhiteSpace(collection);
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        ArgumentException.ThrowIfNullOrEmpty(text);

        var metadata = VectorMetadataJson.ParseDocumentMetadata(metadataJson);
        IndexTextOutcome outcome = IndexTextOutcome.Skipped;
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (_, indexer, collectionId) =>
            {
                outcome = indexer.IndexText(collectionId, sourceId, text, metadata, chunking);
                return true;
            });
        // C2 — ON INSERT EXECUTE AGENT: dopo un insert/update riuscito su una collection con
        // trigger registrato (AgentInsertTrigger), l'agente associato viene eseguito (Ask).
        if (outcome is IndexTextOutcome.Indexed or IndexTextOutcome.Updated)
            Basic.Agents.AgentInsertTrigger.OnInserted(dbAlias, collection, sourceId, text, outcome);
        return outcome;
    }

    /// <summary><c>AIVECTOR.REMOVESOURCE</c> — delete document and embeddings by source id.</summary>
    public static void RemoveSource(string dbAlias, string collection, string sourceId)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (_, indexer, collectionId) =>
            {
                indexer.DeleteBySourceId(collectionId, sourceId);
                return true;
            });
    }

    /// <summary><c>AIVECTOR.PURGEDEPRECATED</c> — purge documents with non-current status.</summary>
    public static int PurgeDeprecated(string dbAlias, string collection) =>
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (_, indexer, collectionId) => indexer.PurgeDeprecated(collectionId));

    /// <summary><c>AIVECTOR.COUNT</c> — embedding row count in collection.</summary>
    public static int CountEmbeddings(string dbAlias, string collection) =>
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (_, indexer, collectionId) => indexer.CountEmbeddings(collectionId));

    /// <summary><c>AIVECTOR.SEARCH</c> — JSON hits; hybrid=1 usa FTS5+RRF.
    /// <paramref name="reranker"/> esplicito; se null usa <see cref="DefaultReranker"/> (null =
    /// OFF). Il reranker si applica al percorso hybrid (vector+FTS); per il solo vettoriale
    /// (hybrid=0) resta il comportamento storico.</summary>
    public static string SearchJson(
        string dbAlias,
        string collection,
        string query,
        int topK,
        bool hybrid = true,
        string? filterJson = null,
        Basic.SQLite.Vector.IReranker? reranker = null,
        double rerankEvidenceMin = double.NaN,
        bool explain = false,
        bool rerankEnabled = true)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(query);
        if (topK <= 0)
            throw new ArgumentOutOfRangeException(nameof(topK));

        var filter = VectorMetadataJson.ParseSearchFilter(filterJson);
        VectorSearchResponse response = new(VectorSearchStatus.InsufficientEvidence, []);
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (connection, _, collectionId) =>
            {
                var search = new SqlVectorSearch(connection, EmbeddingProviderResolver.Default);
                // reranker esplicito > default; OFF (rerankEnabled=false) forza null anche se DefaultReranker è impostato
                var rk = reranker ?? (rerankEnabled ? DefaultReranker : null);
                var ev = double.IsNaN(rerankEvidenceMin) ? DefaultRerankEvidenceMin : rerankEvidenceMin;
                response = hybrid
                    ? search.HybridSearchByText(collectionId, query, topK, filter,
                        reranker: rk, rerankEvidenceMin: ev, includeExplain: explain)
                    : search.SearchByText(collectionId, query, topK, filter);
                return true;
            });
        return VectorSearchJson.Serialize(response);
    }

    /// <summary><c>AIVECTOR.RAGCONTEXT</c> — hybrid vector+FTS search, formatted grounded context.</summary>
    public static string RagContext(string dbAlias, string collection, string query, int topK,
        Basic.SQLite.Vector.IReranker? reranker = null, double rerankEvidenceMin = double.NaN,
        bool rerankEnabled = true) =>
        HybridSearchBundle(dbAlias, collection, query, topK, reranker, rerankEvidenceMin, rerankEnabled).Context;

    /// <summary><c>AIVECTOR.ASK</c> — hybrid RAG retrieval + LLM completion.</summary>
    public static string Ask(string dbAlias, string collection, string question, int topK,
        Basic.SQLite.Vector.IReranker? reranker = null, double rerankEvidenceMin = double.NaN,
        bool rerankEnabled = true) =>
        RagAskPipeline.Ask(HybridSearchBundle(dbAlias, collection, question, topK, reranker, rerankEvidenceMin, rerankEnabled), question);

    /// <summary><c>AIVECTOR.LOADDOCUMENTS</c> — load, parse, chunk, and index files from path.</summary>
    public static int LoadDocuments(string dbAlias, string collection, string path, string? pattern = null, string? metadataJson = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbAlias);
        ArgumentException.ThrowIfNullOrWhiteSpace(collection);
        ArgumentException.ThrowIfNullOrWhiteSpace(path);

        var extPattern = string.IsNullOrWhiteSpace(pattern) ? "*.txt,*.html,*.md" : pattern;
        var extensions = ParsePattern(extPattern);
        var directory = Path.GetDirectoryName(path);
        var isDirectory = Directory.Exists(path);
        var count = 0;

        if (isDirectory)
        {
            foreach (var dirFile in Directory.GetFiles(path, "*.*", SearchOption.AllDirectories))
            {
                var ext = "*." + Path.GetExtension(dirFile).TrimStart('.');
                if (!extensions.Contains(ext, StringComparer.OrdinalIgnoreCase))
                    continue;
                var text = LoadFileContent(dirFile);
                if (!string.IsNullOrWhiteSpace(text))
                {
                    var outcome = IndexTextWithMetadata(dbAlias, collection, dirFile, text, metadataJson);
                    if (outcome == IndexTextOutcome.Indexed || outcome == IndexTextOutcome.Updated)
                        count++;
                }
            }
        }
        else if (File.Exists(path))
        {
            var ext = "*." + Path.GetExtension(path).TrimStart('.');
            if (extensions.Contains(ext, StringComparer.OrdinalIgnoreCase))
            {
                var text = LoadFileContent(path);
                if (!string.IsNullOrWhiteSpace(text))
                {
                    var outcome = IndexTextWithMetadata(dbAlias, collection, path, text, metadataJson);
                    if (outcome == IndexTextOutcome.Indexed || outcome == IndexTextOutcome.Updated)
                        count++;
                }
            }
        }
        else
        {
            throw new ArgumentException($"Path not found: {path}");
        }

        return count;
    }

    private static List<string> ParsePattern(string pattern)
    {
        return pattern.Split(',')
            .Select(p => p.Trim())
            .Where(p => !string.IsNullOrEmpty(p))
            .ToList();
    }

    private static string LoadFileContent(string filePath)
    {
        var ext = Path.GetExtension(filePath).ToLowerInvariant();
        var content = File.ReadAllText(filePath);

        if (ext == ".html")
        {
            // Simple HTML tag stripping
            content = System.Text.RegularExpressions.Regex.Replace(content, "<script[^>]*>.*?</script>", "", System.Text.RegularExpressions.RegexOptions.Singleline | System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            content = System.Text.RegularExpressions.Regex.Replace(content, "<style[^>]*>.*?</style>", "", System.Text.RegularExpressions.RegexOptions.Singleline | System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            content = System.Text.RegularExpressions.Regex.Replace(content, "<[^>]+>", "");
        }

        return content;
    }

    internal static RagContextBundle HybridSearchBundle(
        string dbAlias, string collection, string query, int topK,
        Basic.SQLite.Vector.IReranker? reranker = null,
        double rerankEvidenceMin = double.NaN,
        bool rerankEnabled = true)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbAlias);
        ArgumentException.ThrowIfNullOrWhiteSpace(collection);
        ArgumentException.ThrowIfNullOrWhiteSpace(query);
        if (topK <= 0)
            throw new ArgumentOutOfRangeException(nameof(topK));

        var rk = reranker ?? (rerankEnabled ? DefaultReranker : null);
        var ev = double.IsNaN(rerankEvidenceMin) ? DefaultRerankEvidenceMin : rerankEvidenceMin;
        var bundle = RagContextBundle.FromResults([]);
        WithCollection(dbAlias, collection, EmbeddingProviderResolver.ActiveDimensions,
            (connection, _, collectionId) =>
            {
                var search = new SqlVectorSearch(connection, EmbeddingProviderResolver.Default);
                var response = search.HybridSearchByText(collectionId, query, topK,
                    reranker: rk, rerankEvidenceMin: ev);
                bundle = RagContextBundle.FromResults(response.Results);
                return true;
            });
        return bundle;
    }

    /// <summary><c>AIVECTOR.LISTCOLLECTIONS</c> — returns JSON array of collection names.</summary>
    public static string ListCollections(string dbAlias)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbAlias);

        // Parità VM (SysCall.VECTORDB_LIST_COLLECTIONS): ":memory:" apre una NUOVA
        // connessione SQLite (database vuoto) — NON riusa il driver aperto col fallback
        // di ResolveDriver (che troverebbe p.e. DocsDb e ne elencherebbe le collezioni).
        // Golden AivectorLoadDocs: LISTCOLLECTIONS(":memory:") = vuoto anche se
        // ENSURECOLLECTION("DocsDb") ha creato "docs" su un'altra area.
        if (dbAlias.Equals(":memory:", StringComparison.OrdinalIgnoreCase))
        {
            // Parità VM: ritorna i NOMI separati da newline (string.Join), NON JSON.
            // Una connessione :memory: fresca è sempre vuota -> stringa vuota (golden
            // AivectorLoadDocs/VectorDbBasic: "Collezioni: " / "Collections:" + riga vuota).
            using var fresh = new Microsoft.Data.Sqlite.SqliteConnection("Data Source=:memory:");
            fresh.Open();
            SqlVectorSchema.EnsureSchema(fresh);
            using var cmd = fresh.CreateCommand();
            cmd.CommandText = "SELECT name FROM vec_collections ORDER BY name";
            using var reader = cmd.ExecuteReader();
            var names = new System.Collections.Generic.List<string>();
            while (reader.Read())
                names.Add(reader.GetString(0));
            return string.Join("\n", names);
        }

        var result = "[]";
        WithSqlConnection(dbAlias, connection =>
        {
            SqlVectorSchema.EnsureSchema(connection);
            using var cmd = connection.CreateCommand();
            cmd.CommandText = "SELECT name, dimensions, metric, model_version FROM vec_collections ORDER BY name";
            using var reader = cmd.ExecuteReader();
            var collections = new System.Text.StringBuilder("[");
            var first = true;
            while (reader.Read())
            {
                if (!first) collections.Append(',');
                first = false;
                collections.Append("{\"name\":\"");
                collections.Append(reader.GetString(0));
                collections.Append("\",\"dimensions\":");
                collections.Append(reader.GetInt32(1));
                collections.Append(",\"metric\":\"");
                collections.Append(reader.GetString(2));
                collections.Append("\",\"modelVersion\":\"");
                collections.Append(reader.GetString(3));
                collections.Append("\"}");
            }
            collections.Append(']');
            result = collections.ToString();
        });
        return result;
    }

    private static T WithCollection<T>(
        string dbAlias,
        string collection,
        int dimensions,
        Func<SqliteConnection, SqlVectorIndexer, long, T> action,
        string? format = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbAlias);
        ArgumentException.ThrowIfNullOrWhiteSpace(collection);
        // P0a/P0a2 (decisione utente 2026-09-12): senza modello configurato l'operazione vettoriale
        // FALLISCE con errore che nomina il rimedio — mai fallback silenzioso sul fixture (che
        // prima era installato dal costruttore di PCodeVm). Stesso messaggio di BrainVaultBuild.
        if (EmbeddingProviderResolver.Default is null)
            throw new InvalidOperationException(
                "Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)");
        T result = default!;
        WithSqlConnection(dbAlias, connection =>
        {
            var indexer = new SqlVectorIndexer(connection, EmbeddingProviderResolver.Default);
            var collectionId = indexer.EnsureCollection(collection, dimensions, VectorMetric.Cosine, format: format);
            result = action(connection, indexer, collectionId);
        });
        return result;
    }

    /// <summary>GenTest (R3): genera testset sintetico da una collection vettoriale usando l'LLM attivo.
    /// Salva golden set JSON in samples/Fixtures/rag-eval/ e ritorna il path del file.</summary>
    public static string GenTest(string collection, int n, string types)
    {
        if (n <= 0) n = 5;
        var typeList = (types ?? "simple").Split(',').Select(t => t.Trim().ToLowerInvariant()).ToList();
        if (typeList.Count == 0) typeList = ["simple"];

        var fixturesDir = System.IO.Path.GetFullPath(
            System.IO.Path.Combine(
                System.IO.Directory.GetCurrentDirectory(),
                "samples", "Fixtures", "rag-eval"));
        System.IO.Directory.CreateDirectory(fixturesDir);

        var cases = new List<object>();

        WithSqlConnection(collection, connection =>
        {
            SqlVectorSchema.EnsureSchema(connection);

            // Get collection id
            using (var cmd = connection.CreateCommand())
            {
                cmd.CommandText = "SELECT id FROM vec_collections WHERE name = @name";
                cmd.Parameters.AddWithValue("@name", collection);
                using var reader = cmd.ExecuteReader();
                if (!reader.Read())
                    throw new InvalidOperationException($"Collection '{collection}' not found.");
                var collId = reader.GetInt64(0);
                reader.Close();

                // Sample random chunks
                using var cmd2 = connection.CreateCommand();
                cmd2.CommandText = """
                    SELECT c.id, c.text
                    FROM vec_chunks c
                    JOIN vec_documents d ON c.document_id = d.id
                    WHERE d.collection_id = @cid AND d.status = 'current'
                    ORDER BY RANDOM() LIMIT @n
                    """;
                cmd2.Parameters.AddWithValue("@cid", collId);
                cmd2.Parameters.AddWithValue("@n", n * 3);
                var chunks = new List<(long id, string text)>();
                using (var reader2 = cmd2.ExecuteReader())
                {
                    while (reader2.Read())
                        chunks.Add((reader2.GetInt64(0), reader2.GetString(1)));
                }

                cmd2.Dispose();
                if (chunks.Count == 0)
                    throw new InvalidOperationException($"No chunks found in collection '{collection}'.");

            // Get the LLM completer (best-effort)
            ILlmCompleter? completer = null;
            try { completer = Basic.Providers.ProviderServer.Instance.GetActiveCompletion(); }
            catch { /* no LLM configured */ }

                var generated = 0;
                for (var i = 0; generated < n && i < chunks.Count; i++)
                {
                    var type = typeList[generated % typeList.Count];
                    var chunk = chunks[i];

                    if (completer != null)
                    {
                        var prompt = type switch
                        {
                            "simple" => $"Generate ONE simple question from this text. Text: {chunk.text}\n\nReturn ONLY valid JSON: {{\"question\":\"...\",\"expected_answer\":\"...\"}}",
                            "multi-hop" => $"Generate ONE question requiring multiple facts from: {chunk.text}\n\nReturn ONLY valid JSON: {{\"question\":\"...\",\"expected_answer\":\"...\"}}",
                            "adversarial" => $"Generate ONE tricky question with lexical distractor for: {chunk.text}\n\nReturn ONLY valid JSON: {{\"question\":\"...\",\"expected_answer\":\"...\"}}",
                            _ => $"Generate ONE question from: {chunk.text}\n\nReturn ONLY valid JSON: {{\"question\":\"...\",\"expected_answer\":\"...\"}}"
                        };

                        try
                        {
                            var response = completer.Complete(
                                "You are a test data generator. Generate concise, factual questions. Return ONLY valid JSON.",
                                prompt);

                            // Find LAST '{' to skip thinking trace
                            var jsonStart = response.LastIndexOf('{');
                            var jsonEnd = response.LastIndexOf('}');
                            if (jsonStart >= 0 && jsonEnd > jsonStart)
                            {
                                var json = response[jsonStart..(jsonEnd + 1)];
                                // Find the balanced JSON object (skip trailing array brackets/extra chars)
                                var objStart = json.IndexOf('{');
                                if (objStart >= 0)
                                {
                                    int depth = 0, objEnd = -1;
                                    for (int j = objStart; j < json.Length; j++)
                                    {
                                        if (json[j] == '{') depth++;
                                        else if (json[j] == '}') { depth--; if (depth == 0) { objEnd = j; break; } }
                                    }
                                    if (objEnd > objStart) json = json[objStart..(objEnd + 1)];
                                }
                                using var doc = JsonDocument.Parse(json);
                                var root = doc.RootElement;
                                string question = "", answer = "";

                                if (root.ValueKind == JsonValueKind.Object)
                                {
                                    question = root.TryGetProperty("question", out var q) ? q.GetString() ?? "" : "";
                                    answer = root.TryGetProperty("expected_answer", out var ea) ? ea.GetString() ?? ""
                                           : root.TryGetProperty("answer", out var a) ? a.GetString() ?? "" : "";
                                    // Fallback: extract from "questions" array of {question, answer} objects
                                    if (string.IsNullOrEmpty(question) && root.TryGetProperty("questions", out var qs) && qs.ValueKind == JsonValueKind.Array && qs.GetArrayLength() > 0)
                                    {
                                        var first = qs[0];
                                        if (first.ValueKind == JsonValueKind.Object)
                                        {
                                            question = first.TryGetProperty("question", out var qq) ? qq.GetString() ?? "" : "";
                                            answer = first.TryGetProperty("answer", out var aa) ? aa.GetString() ?? ""
                                                   : first.TryGetProperty("expected_answer", out var ee) ? ee.GetString() ?? "" : "";
                                        }
                                        else if (first.ValueKind == JsonValueKind.String)
                                        {
                                            question = first.GetString() ?? "";
                                        }
                                    }
                                }
                                else if (root.ValueKind == JsonValueKind.Array && root.GetArrayLength() > 0)
                                {
                                    var first = root[0];
                                    if (first.ValueKind == JsonValueKind.Object)
                                    {
                                        question = first.TryGetProperty("question", out var q) ? q.GetString() ?? "" : "";
                                        answer = first.TryGetProperty("answer", out var a) ? a.GetString() ?? ""
                                               : first.TryGetProperty("expected_answer", out var ea) ? ea.GetString() ?? "" : "";
                                    }
                                }

                                if (!string.IsNullOrWhiteSpace(question) && !string.IsNullOrWhiteSpace(answer))
                                {
                                    cases.Add(new
                                    {
                                        id = $"{type}_{generated}",
                                        type,
                                        question,
                                        expected_answer = answer,
                                        ideal_chunks = new[] { chunk.id }
                                    });
                                    generated++;
                                }
                            }
                        }
                        catch (Exception ex) { 
                            System.Console.Error.WriteLine("[GENTEST-ERR] " + ex.Message.Replace("\n", "\\n"));
                        }
                    }
                    else
                    {
                        var snippet = chunk.text.Length > 100 ? chunk.text[..100] + "..." : chunk.text;
                        cases.Add(new
                        {
                            id = $"{type}_{generated}",
                            type,
                            question = $"What is the content of this text? (auto: {snippet})",
                            expected_answer = chunk.text.Length > 200 ? chunk.text[..200] + "..." : chunk.text,
                            ideal_chunks = new[] { chunk.id }
                        });
                        generated++;
                    }
                }
            }
        });

        // Build golden set JSON
        var json = JsonSerializer.Serialize(new
        {
            schema_version = "1.0",
            dataset_version = "1.0",
            created_at = System.DateTime.UtcNow.ToString("O"),
            source = "ai-vectordb-gentest",
            collection,
            cases
        }, new JsonSerializerOptions { WriteIndented = true });

        var sha256 = System.Security.Cryptography.SHA256.HashData(
            System.Text.Encoding.UTF8.GetBytes(json));
        var hash = Convert.ToHexStringLower(sha256);

        // Safe filename from collection name
        var safeName = new string(collection.Where(c => char.IsAsciiLetterOrDigit(c) || c == '-' || c == '_').ToArray());
        if (string.IsNullOrEmpty(safeName)) safeName = "collection";

        var fileName = $"gentest_{safeName}_{System.DateTime.UtcNow:yyyyMMddHHmmss}.json";
        var filePath = System.IO.Path.Combine(fixturesDir, fileName);
        System.IO.File.WriteAllText(filePath, json);

        return $"Golden set saved: {filePath} ({cases.Count} cases, sha256={hash})";
    }

    // ── RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione tier-1
    // (R2). Formule IR standard, euristische deterministiche; il canone C lowered identico in
    // gmb_runtime_c/src/gmb_rag_eval.c (§0bis: VM/C e C99 chiamano il runtime C, IL/PCodeVM
    // queste funzioni — parità verificata per EXPECT condiviso).
    private static readonly string[] RagEvalStopwords =
    {
        "the", "a", "an", "of", "to", "in", "on", "at", "is", "are", "was", "were", "be",
        "and", "or", "for", "with", "by", "from", "it", "its", "this", "that",
        "what", "who", "how", "why", "when", "where", "which",
    };

    /// <summary>ID da CSV: separati da virgola, trim, vuoti scartati (confronto case-insensitive).</summary>
    private static List<string> RagEvalSplitIds(string csv)
    {
        var list = new List<string>();
        if (string.IsNullOrEmpty(csv)) return list;
        foreach (var part in csv.Split(','))
        {
            var id = part.Trim();
            if (id.Length > 0) list.Add(id);
        }
        return list;
    }

    /// <summary>Token ASCII [a-z0-9]+ (lowercase): STESSA regola della lowering C.</summary>
    private static List<string> RagEvalSplitTokens(string text)
    {
        var tokens = new List<string>();
        if (string.IsNullOrEmpty(text)) return tokens;
        var start = -1;
        for (var i = 0; i <= text.Length; i++)
        {
            var isWord = i < text.Length && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= '0' && text[i] <= '9'));
            if (isWord && start < 0) start = i;
            else if (!isWord && start >= 0)
            {
                var buf = text.Substring(start, i - start).ToLowerInvariant();
                tokens.Add(buf);
                start = -1;
            }
        }
        return tokens;
    }

    private static double RagEvalOverlapShare(List<string> source, HashSet<string> target)
    {
        if (source.Count == 0) return 0;
        double hits = 0;
        foreach (var t in source) if (target.Contains(t)) hits++;
        return hits / source.Count;
    }

    public static double PrecisionAt(string retrieved, string ideal, int k)
    {
        if (k <= 0) return 0;
        var ids = RagEvalSplitIds(retrieved);
        var rel = RagEvalSplitIds(ideal).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var n = Math.Min(k, ids.Count);
        double hits = 0;
        for (var i = 0; i < n; i++) if (rel.Contains(ids[i])) hits++;
        return n > 0 ? hits / n : 0;
    }

    public static double RecallAt(string retrieved, string ideal, int k)
    {
        var ids = RagEvalSplitIds(retrieved);
        var idealIds = RagEvalSplitIds(ideal);
        if (idealIds.Count == 0) return 0;
        var rel = idealIds.ToHashSet(StringComparer.OrdinalIgnoreCase);
        var n = Math.Min(k <= 0 ? ids.Count : k, ids.Count);
        double hits = 0;
        for (var i = 0; i < n; i++) if (rel.Contains(ids[i])) hits++;
        return hits / idealIds.Count;
    }

    public static double Mrr(string retrieved, string ideal)
    {
        var ids = RagEvalSplitIds(retrieved);
        var rel = RagEvalSplitIds(ideal).ToHashSet(StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < ids.Count; i++)
            if (rel.Contains(ids[i])) return 1.0 / (i + 1);
        return 0;
    }

    /// <summary>NDCG@k binario — STESSA formula di EvalHarness.NdcgAtK (canone harness).</summary>
    public static double NdcgAt(string retrieved, string ideal, int k)
    {
        var ids = RagEvalSplitIds(retrieved);
        var rel = RagEvalSplitIds(ideal).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var n = Math.Min(k, ids.Count);
        if (n == 0) return 0;
        double dcg = 0;
        for (var i = 0; i < n; i++) dcg += rel.Contains(ids[i]) ? 1.0 / Math.Log2(i + 2) : 0;
        double idcg = 0;
        var idealN = Math.Min(k, rel.Count);
        for (var i = 0; i < idealN; i++) idcg += 1.0 / Math.Log2(i + 2);
        return idcg > 0 ? dcg / idcg : 0;
    }

    /// <summary>Faithfulness-lite: quota di token della risposta radicati nel contesto (0 se risposta vuota).</summary>
    public static double Faithfulness(string context, string answer)
        => RagEvalOverlapShare(RagEvalSplitTokens(answer), RagEvalSplitTokens(context).ToHashSet());

    /// <summary>Hallucination-candidate-lite: complemento del faithfulness (token risposta assenti dal contesto).</summary>
    public static double Hallucination(string context, string answer)
        => 1.0 - Faithfulness(context, answer);

    /// <summary>Relevance-lite: quota di token della domanda presenti nella risposta.</summary>
    public static double Relevance(string question, string answer)
        => RagEvalOverlapShare(RagEvalSplitTokens(question), RagEvalSplitTokens(answer).ToHashSet());

    /// <summary>Completeness-lite: come Relevance ma sui soli token di contenuto (≥3 char, non stopwords);
    /// se non ci sono token di contenuto ricade su Relevance.</summary>
    public static double Completeness(string question, string answer)
    {
        var tokens = RagEvalSplitTokens(question);
        var content = new List<string>();
        foreach (var t in tokens)
            if (t.Length >= 3 && !RagEvalStopwords.Contains(t)) content.Add(t);
        if (content.Count == 0) return Relevance(question, answer);
        return RagEvalOverlapShare(content, RagEvalSplitTokens(answer).ToHashSet());
    }

    // ── RAG eval avanzato (2026-09-15): diagnostica, calibrazione, drift, A/B test.
    // Le implementazioni composite chiamano le funzioni C native via P/Invoke (gmb_rageval_advanced.c)
    // o implementano l'algoritmo equivalente in C# puro.

    /// <summary>Diagnosi root-cause di un risultato di valutazione RAG.
    /// Parità con `gmb_rageval_diagnose` (native/gmb_runtime_c/src/gmb_rageval_advanced.c):
    /// input oggetto {retrieved[], ideal[], faithfulness, relevance, hallucination,
    /// completeness, context_len}; output {"cause","evidence","recommendation"}.
    /// Aggiorna l'aggregatore consumato da <see cref="DiagReport"/>.</summary>
    public static string DiagResultJson(string evalJson)
    {
        double faithfulness = 0, hallucination = 0, completeness = 0;
        int contextLen = 0;
        var retrieved = new List<string>();
        var ideal = new List<string>();
        try
        {
            using var doc = JsonDocument.Parse(evalJson);
            var root = doc.RootElement;
            if (root.ValueKind == JsonValueKind.Object)
            {
                faithfulness = RagNum(root, "faithfulness");
                hallucination = RagNum(root, "hallucination");
                completeness = RagNum(root, "completeness");
                contextLen = (int)RagNum(root, "context_len");
                RagStrings(root, "retrieved", retrieved);
                RagStrings(root, "ideal", ideal);
            }
        }
        catch { /* JSON non valido → default 0, come il runtime C (che non fallisce mai) */ }

        string cause, evidence, rec;
        if (faithfulness < 0.3 && hallucination > 0.7)
        {
            cause = "generation_hallucination";
            evidence = $"faithfulness={F3(faithfulness)} < 0.3 and hallucination={F3(hallucination)} > 0.7";
            rec = "increase context grounding, reduce temperature, or add factual constraints";
        }
        else if (completeness < 0.4)
        {
            cause = "generation_incomplete";
            evidence = $"completeness={F3(completeness)} < 0.4";
            rec = "increase max_tokens, improve instruction to cover all points, or chunk input";
        }
        else if (contextLen < 200)
        {
            cause = "context_insufficient";
            evidence = $"context_len={contextLen} < 200";
            rec = "increase chunk size, add more retrieved documents, or expand context window";
        }
        else if (ideal.Count > 0)
        {
            bool idealInRetrieved = ideal.Any(id => retrieved.Contains(id));
            if (idealInRetrieved)
            {
                cause = "retrieval_ranking";
                evidence = $"ideal documents present in retrieved but not at top positions (ideal={ideal.Count} items, retrieved={retrieved.Count} items)";
                rec = "improve ranking score, try reranking, or adjust hybrid search weights";
            }
            else
            {
                cause = "retrieval_miss";
                evidence = $"ideal documents missing from retrieved results (ideal={ideal.Count} items, retrieved={retrieved.Count} items)";
                rec = "expand search query, reduce top_k, lower similarity threshold, or add synonyms";
            }
        }
        else
        {
            cause = "pipeline_error";
            evidence = $"no specific diagnosis criteria met (faith={F3(faithfulness)}, hall={F3(hallucination)}, compl={F3(completeness)}, ctx={contextLen})";
            rec = "check pipeline configuration, verify embedding model, or inspect input quality";
        }

        lock (_diagLock)
        {
            _diagTotal++;
            switch (cause)
            {
                case "retrieval_miss": _diagRetrievalMiss++; break;
                case "retrieval_ranking": _diagRetrievalRanking++; break;
                case "context_insufficient": _diagContextInsufficient++; break;
                case "generation_hallucination": _diagGenHallucination++; break;
                case "generation_incomplete": _diagGenIncomplete++; break;
                default: _diagPipelineError++; break;
            }
        }
        return JsonSerializer.Serialize(new { cause, evidence, recommendation = rec });
    }

    /// <summary>Restituisce report aggregato della diagnosi cumulativa.
    /// Parità con `gmb_rageval_diagnose_report`: {"total":N,"causes":{...}}.</summary>
    public static string DiagReport()
    {
        int total, rm, rr, ci, gh, gi, pe;
        lock (_diagLock)
        {
            total = _diagTotal; rm = _diagRetrievalMiss; rr = _diagRetrievalRanking;
            ci = _diagContextInsufficient; gh = _diagGenHallucination;
            gi = _diagGenIncomplete; pe = _diagPipelineError;
        }
        return JsonSerializer.Serialize(new
        {
            total,
            causes = new
            {
                retrieval_miss = rm,
                retrieval_ranking = rr,
                context_insufficient = ci,
                generation_hallucination = gh,
                generation_incomplete = gi,
                pipeline_error = pe,
            }
        });
    }

    // Aggregatore delle diagnosi — parità con i contatori statici rga_diag_* del runtime C
    // (gmb_rageval_advanced.c). Stato di processo, come nel mondo C.
    private static readonly object _diagLock = new();
    private static int _diagTotal;
    private static int _diagRetrievalMiss;
    private static int _diagRetrievalRanking;
    private static int _diagContextInsufficient;
    private static int _diagGenHallucination;
    private static int _diagGenIncomplete;
    private static int _diagPipelineError;

    private static double RagNum(JsonElement obj, string name)
        => obj.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetDouble() : 0;

    private static void RagStrings(JsonElement obj, string name, List<string> outList)
    {
        if (!obj.TryGetProperty(name, out var arr) || arr.ValueKind != JsonValueKind.Array) return;
        foreach (var item in arr.EnumerateArray())
            if (item.ValueKind == JsonValueKind.String) outList.Add(item.GetString() ?? "");
    }

    private static string F3(double v)
        => v.ToString("0.000", System.Globalization.CultureInfo.InvariantCulture);

    /// <summary>Calibra la soglia di accettazione dati casi annotati (umano vs giudice).</summary>
    public static string Calibrate(string casesJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(casesJson);
            var arr = doc.RootElement;
            if (arr.ValueKind != JsonValueKind.Array) return "{\"status\":\"error\",\"message\":\"expected JSON array\"}";
            int n = arr.GetArrayLength();
            if (n < 5) return "{\"status\":\"insufficient_data\",\"message\":\"need >= 5 cases\"}";

            var umano = new List<int>(); var giudice = new List<int>();
            foreach (var item in arr.EnumerateArray())
            {
                umano.Add(item.TryGetProperty("umano", out var u) ? (u.GetInt32() > 0 ? 1 : 0) : 0);
                giudice.Add(item.TryGetProperty("giudice", out var g) ? (g.GetInt32() > 0 ? 1 : 0) : 0);
            }

            // Cohen's kappa
            int a = 0, b = 0, c = 0, d = 0;
            for (int i = 0; i < n; i++)
            {
                if (umano[i] == 1 && giudice[i] == 1) a++;
                else if (umano[i] == 1 && giudice[i] == 0) b++;
                else if (umano[i] == 0 && giudice[i] == 1) c++;
                else d++;
            }
            double po = (double)(a + d) / n;
            double pa = (double)(a + b) / n * (a + c) / n;
            double pb = (double)(c + d) / n * (b + d) / n;
            double pe = pa + pb;
            double kappa = Math.Abs(1 - pe) < 1e-10 ? 1.0 : (po - pe) / (1 - pe);

            // Wilson 95% CI (z=1.96)
            double p = po;
            double z = 1.96;
            double denom = 1 + z * z / n;
            double center = (p + z * z / (2 * n)) / denom;
            double margin = z / denom * Math.Sqrt(p * (1 - p) / n + z * z / (4 * n * n));
            double ciLow = Math.Max(0, center - margin);
            double ciHigh = Math.Min(1, center + margin);

            // Threshold sweep
            double bestKappa = -2;
            double bestTh = 0.5;
            for (double th = 0.05; th <= 0.95; th += 0.05)
            {
                int ta = 0, tb = 0, tc = 0, td = 0;
                for (int i = 0; i < n; i++)
                {
                    double gs = giudice[i];
                    bool pred = gs >= th;
                    if (umano[i] == 1 && pred) ta++;
                    else if (umano[i] == 1 && !pred) tb++;
                    else if (umano[i] == 0 && pred) tc++;
                    else td++;
                }
                double p_o = (double)(ta + td) / n;
                double p_a = (double)(ta + tb) / n * (ta + tc) / n;
                double p_b = (double)(tc + td) / n * (tb + td) / n;
                double p_e = p_a + p_b;
                double k = Math.Abs(1 - p_e) < 1e-10 ? 1.0 : (p_o - p_e) / (1 - p_e);
                if (k > bestKappa) { bestKappa = k; bestTh = th; }
            }

            var result = new { status = "ok", total = n, kappa = Math.Round(kappa, 4), threshold = bestTh,
                wilson95 = new { low = Math.Round(ciLow, 4), high = Math.Round(ciHigh, 4) },
                far = n > 0 ? (double)c / n : 0, frr = n > 0 ? (double)b / n : 0 };
            return JsonSerializer.Serialize(result);
        }
        catch (Exception ex) { return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}"; }
    }

    /// <summary>Valuta gate di accettazione baseline vs current, restituisce {verdict, delta, pass}.</summary>
    public static string EvalGate(string baseline, string current)
    {
        try
        {
            var bl = JsonDocument.Parse(baseline);
            var cur = JsonDocument.Parse(current);
            var bRoot = bl.RootElement;
            var cRoot = cur.RootElement;
            var deltas = new List<object>();
            bool anyFail = false, anyWarn = false;
            foreach (var prop in bRoot.EnumerateObject())
            {
                if (cRoot.TryGetProperty(prop.Name, out var cv) && prop.Value.ValueKind == JsonValueKind.Number && cv.ValueKind == JsonValueKind.Number)
                {
                    double bv = prop.Value.GetDouble(), curV = cv.GetDouble();
                    double delta = curV - bv;
                    bool isRetrieval = prop.Name == "p_at" || prop.Name == "recall" || prop.Name == "ndcg" || prop.Name == "mrr";
                    double threshold = isRetrieval ? 0.05 : 0.10;
                    if (delta < -threshold && isRetrieval) anyFail = true;
                    else if (delta < -threshold) anyWarn = true;
                    deltas.Add(new { metric = prop.Name, before = bv, after = curV, delta = Math.Round(delta, 4) });
                }
            }
            string verdict = anyFail ? "fail" : anyWarn ? "warn" : "pass";
            var result = new { status = "ok", verdict, deltas };
            return JsonSerializer.Serialize(result);
        }
        catch (Exception ex)
        {
            return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}";
        }
    }

    /// <summary>Dettaglio differenziale tra baseline e current (per EVALDIFF).</summary>
    public static string EvalDiff(string baseline, string current)
    {
        try
        {
            var bl = JsonDocument.Parse(baseline);
            var cur = JsonDocument.Parse(current);
            var metrics = new Dictionary<string, object>();
            foreach (var prop in bl.RootElement.EnumerateObject())
            {
                if (cur.RootElement.TryGetProperty(prop.Name, out var curVal))
                {
                    if (prop.Value.ValueKind == JsonValueKind.Number && curVal.ValueKind == JsonValueKind.Number)
                    {
                        double diff = curVal.GetDouble() - prop.Value.GetDouble();
                        metrics[prop.Name] = new { baseline = prop.Value.GetDouble(), current = curVal.GetDouble(), diff };
                    }
                }
            }
            var result = new { status = "ok", metrics };
            return JsonSerializer.Serialize(result);
        }
        catch (Exception ex)
        {
            return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}";
        }
    }

    /// <summary>Crea/aggiorna baseline da un sample JSON (metriche di riferimento).</summary>
    private static string? _baselineCache;  // in-memory baseline
    public static string Baseline(string sampleJson)
    {
        try
        {
            var sample = JsonDocument.Parse(sampleJson);
            _baselineCache = sampleJson;
            var metrics = new List<object>();
            foreach (var prop in sample.RootElement.EnumerateObject())
            {
                if (prop.Value.ValueKind == JsonValueKind.Number)
                    metrics.Add(new { name = prop.Name, value = prop.Value.GetDouble() });
            }
            var result = new { status = "ok", message = "baseline captured", metrics };
            return JsonSerializer.Serialize(result);
        }
        catch (Exception ex)
        {
            return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}";
        }
    }

    /// <summary>Drift detection: calcola PSI (Population Stability Index) tra baseline e finestra corrente.</summary>
    public static double Drift(string baseline, string window)
    {
        try
        {
            var bl = JsonDocument.Parse(baseline);
            var wn = JsonDocument.Parse(window);
            var blVals = bl.RootElement.TryGetProperty("values", out var bv) ? bv : bl.RootElement;
            var wnVals = wn.RootElement.TryGetProperty("values", out var wv) ? wv : wn.RootElement;
            if (blVals.ValueKind != JsonValueKind.Array || wnVals.ValueKind != JsonValueKind.Array)
                return double.NaN;
            const int bins = 10;
            int[] blC = new int[bins], wnC = new int[bins];
            int blN = 0, wnN = 0;
            foreach (var item in blVals.EnumerateArray()) { double v = item.GetDouble(); blC[(int)Math.Clamp(v * bins / 100.0, 0, bins - 1)]++; blN++; }
            foreach (var item in wnVals.EnumerateArray()) { double v = item.GetDouble(); wnC[(int)Math.Clamp(v * bins / 100.0, 0, bins - 1)]++; wnN++; }
            double psi = 0;
            for (int i = 0; i < bins; i++)
            {
                double pi = blN > 0 ? (double)blC[i] / blN : 1.0 / bins;
                double qi = wnN > 0 ? (double)wnC[i] / wnN : 1.0 / bins;
                if (pi > 0 && qi > 0) psi += (pi - qi) * Math.Log(pi / qi);
            }
            return psi;
        }
        catch { return double.NaN; }
    }

    /// <summary>A/B test: z-test di due proporzioni. Input {"n","success"} per variante
    /// (contratto della guida §8.8 e dei MiniTest), output {"z_score","p_value","effect_size"}.
    /// Parità con `gmb_rageval_ab_test` (runtime C).</summary>
    public static string AbTest(string variantA, string variantB)
    {
        try
        {
            double nA = RagNum(JsonDocument.Parse(variantA).RootElement, "n");
            double sA = RagNum(JsonDocument.Parse(variantA).RootElement, "success");
            double nB = RagNum(JsonDocument.Parse(variantB).RootElement, "n");
            double sB = RagNum(JsonDocument.Parse(variantB).RootElement, "success");
            if (nA <= 0 || nB <= 0)
                return "{\"status\":\"error\",\"message\":\"n deve essere > 0\"}";
            double p1 = sA / nA, p2 = sB / nB;
            double se = Math.Sqrt(p1 * (1 - p1) / nA + p2 * (1 - p2) / nB);
            double z = se > 0 ? (p1 - p2) / se : 0;
            double p = 2 * (1 - NormalCdf(Math.Abs(z)));
            double effectSize = p1 - p2;
            return JsonSerializer.Serialize(new { z_score = z, p_value = p, effect_size = effectSize });
        }
        catch (Exception ex)
        {
            return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}";
        }
    }

    /// <summary>Compara configurazioni multiple JSON-side-by-side.</summary>
    public static string Compare(string configsJson)
    {
        try
        {
            var configs = JsonDocument.Parse(configsJson);
            var arr = configs.RootElement;
            if (arr.ValueKind != JsonValueKind.Array)
                return "{\"status\":\"error\",\"message\":\"expected JSON array\"}";

            var names = new List<string>();
            var metricSets = new List<Dictionary<string, double>>();
            foreach (var cfg in arr.EnumerateArray())
            {
                string name = cfg.TryGetProperty("config", out var c) && c.TryGetProperty("name", out var nn) ? nn.GetString() ?? $"cfg_{names.Count}" : $"cfg_{names.Count}";
                names.Add(name);
                var ms = new Dictionary<string, double>();
                if (cfg.TryGetProperty("results", out var res))
                    foreach (var rp in res.EnumerateObject())
                        if (rp.Value.ValueKind == JsonValueKind.Number)
                            ms[rp.Name] = rp.Value.GetDouble();
                metricSets.Add(ms);
            }

            var winners = new List<object>();
            if (metricSets.Count >= 2)
            {
                var allMetrics = new HashSet<string>();
                foreach (var ms in metricSets) foreach (var k in ms.Keys) allMetrics.Add(k);
                foreach (var metric in allMetrics)
                {
                    double best = double.MinValue;
                    string bestName = names[0];
                    foreach (int i in Enumerable.Range(0, names.Count))
                    {
                        if (metricSets[i].TryGetValue(metric, out var v) && v > best) { best = v; bestName = names[i]; }
                    }
                    if (best > double.MinValue)
                        winners.Add(new { metric, winner = bestName, value = best });
                }
            }

            var result = new { status = "ok", count = names.Count, configs = names, winners };
            return JsonSerializer.Serialize(result);
        }
        catch (Exception ex)
        {
            return $"{{\"status\":\"error\",\"message\":\"{ex.Message}\"}}";
        }
    }

    private static double NormalCdf(double x)
    {
        // Approssimazione Abramowitz & Stegun 26.2.17.
        double t = 1.0 / (1.0 + 0.2316419 * Math.Abs(x));
        double d = 0.3989422804014327 * Math.Exp(-x * x / 2);
        double p = d * t * (0.319381530 + t * (-0.356563782 + t * (1.781477937 + t * (-1.821255978 + t * 1.330274429))));
        return x >= 0 ? 1.0 - p : p;
    }
}
