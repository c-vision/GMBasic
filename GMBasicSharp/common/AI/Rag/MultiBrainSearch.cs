// FILE: Basic.AI.Rag.MultiBrainSearch
// Second Brain — multi-use (R7): interroga TUTTI i brain collegati (BrainRegistry.Used) e
// fonde i risultati con RRF (k=60, come §1 L4 del piano), aggiungendo la provenance "brain"
// a ogni hit. Per ogni brain apre il suo db vettoriale (RagDbSession.Open) e fa la stessa
// query; la chiave di dedup cross-brain è sourceId (path assoluto del documento). Nessuna
// duplicazione: riusa AiVectorRuntime/SearchJson per ogni brain.
using System.Text.Json;

namespace Basic.AI.Rag;

public static class MultiBrainSearch
{
    private const int RrfK = 60;

    /// <summary>SearchMulti(query, topK, collection, brains?, reranker, minEvidence, explain):
    /// interroga ogni brain used (o i nomi passati), fonde con RRF e ritorna JSON {status,
    /// hits:[{chunkId, sourceId, score, text, brain, rrf}]}. reranker = %RERANK_* (0 OFF,
    /// 1 KEYWORD default, 2 ONNX→errore); minEvidence: se finito e un brain non supera la soglia
    /// dopo il rerank → quel brain è escluso dal merge (nessuna evidenza). explain non è
    /// supportato sul merge multi-brain → errore esplicito (mai no-op silenzioso).</summary>
    public static string SearchMulti(string query, int topK, string collection = "wiki", string[]? brains = null,
        double reranker = 1, double minEvidence = double.NaN, double explain = 0,
        Basic.AI.Knowledge.BrainScope? scope = null)
    {
        if (explain != 0) throw new NotSupportedException(
            "SearchMulti: explain non supportato sul merge multi-brain (usare AIVECTOR.SEARCH single-collection).");
        var rerankEnabled = reranker != 0;
        var rk = Basic.AI.Rag.AiVectorRuntime.RerankerFor(reranker);
        var ev = double.IsNaN(minEvidence) ? double.NegativeInfinity : minEvidence;
        // F4 — il default è il SET EFFETTIVO dello scope (catena CALL→TASK→AGENT→SESSION→PROCESS); senza
        // scope il livello PROCESS equivale ai brain attivati (`AI BRAIN USE`), quindi il comportamento
        // resta invariato per chi non passa uno scope.
        var names = brains is { Length: > 0 } ? brains
            : scope is not null ? scope.Effective().Select(r => r.Name).ToArray()
            : Basic.AI.Knowledge.BrainRegistry.Used();
        if (names.Length == 0)
            return "{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain collegato (AI BRAIN USE)\"}";

        // per-brain: {sourceId → (chunkId, text, rank, score, modelVersion)}
        var ready = new List<(string Brain, Dictionary<string, (long ChunkId, string Text, int Rank, float Score, string Model)> Hits)>();

        foreach (var name in names)
        {
            var dbPath = Basic.AI.Knowledge.BrainRegistry.RagDbPath(name);
            if (dbPath is null) continue;
            try
            {
                var alias = "MultiBrain_" + name.Replace("_", "").Replace("-", "") + "_" + Guid.NewGuid().ToString("N")[..6];
                RagDbSession.Open(dbPath, alias);
                var json = AiVectorRuntime.SearchJson(alias, collection, query, topK,
                    hybrid: true, reranker: rk, rerankEvidenceMin: ev, rerankEnabled: rerankEnabled);
                using var doc = JsonDocument.Parse(json);
                // se la soglia di evidenza è attiva e questo brain NON la supera → escludilo
                // (nessuna evidenza sufficiente): coerente con il gating di HybridSearchByText.
                if (double.IsFinite(ev) && doc.RootElement.TryGetProperty("status", out var st)
                    && st.GetString() == "InsufficientEvidence")
                    continue;
                if (!doc.RootElement.TryGetProperty("hits", out var hits) || hits.ValueKind != JsonValueKind.Array)
                    continue;
                var dict = new Dictionary<string, (long ChunkId, string Text, int Rank, float Score, string Model)>();
                var rank = 0;
                foreach (var h in hits.EnumerateArray())
                {
                    var sourceId = h.TryGetProperty("sourceId", out var sid) ? sid.GetString() ?? "" : "";
                    var chunkId = h.TryGetProperty("chunkId", out var cid) ? cid.GetInt64() : 0;
                    var text = h.TryGetProperty("text", out var txt) ? txt.GetString() ?? "" : "";
                    var score = h.TryGetProperty("score", out var sc) && sc.TryGetSingle(out var f) ? f : 0f;
                    var model = h.TryGetProperty("modelVersion", out var mv) ? mv.GetString() ?? "" : "";
                    dict[sourceId] = (chunkId, text, rank, score, model);
                    rank++;
                }
                ready.Add((name, dict));
            }
            catch { /* brain non interrogabile: salta */ }
        }

        if (ready.Count == 0)
            return "{\"status\":\"InsufficientEvidence\",\"hits\":[],\"error\":\"nessun brain interrogabile\"}";

        // RRF: per ogni brain, contributo 1/(k+rank) per le sue hit; somma per sourceId
        var rrf = new Dictionary<string, double>();
        var meta = new Dictionary<string, (string Brain, long ChunkId, string Text, float Score, string Model)>();
        foreach (var (brain, hits) in ready)
        {
            foreach (var (sourceId, h) in hits)
            {
                rrf[sourceId] = rrf.GetValueOrDefault(sourceId) + 1.0 / (RrfK + h.Rank);
                meta[sourceId] = (brain, h.ChunkId, h.Text, h.Score, h.Model);
            }
        }

        var merged = rrf
            .OrderByDescending(kv => kv.Value)
            .Take(topK)
            .Select(kv => new
            {
                chunkId = meta[kv.Key].ChunkId,
                sourceId = kv.Key,
                text = meta[kv.Key].Text,
                score = meta[kv.Key].Score,
                brain = meta[kv.Key].Brain,
                rrf = Math.Round(kv.Value, 6),
            })
            .ToArray();

        return JsonSerializer.Serialize(new { status = "Success", hits = merged });
    }

    /// <summary>RAGCONTEXTMULTI(query, topK, reranker, minEvidence): contesto grounded aggregato
    /// dai top hit dei brain used (testi + attribuzioni con sourceId/brain).</summary>
    public static RagContextBundle RagContextMulti(string query, int topK, double reranker = 1, double minEvidence = double.NaN,
        string[]? brains = null, Basic.AI.Knowledge.BrainScope? scope = null)
        => BuildBundle(SearchMulti(query, topK, "wiki", brains, reranker, minEvidence, 0, scope));

    /// <summary>ASKMULTI(question, topK, reranker, minEvidence): RAG grounded multi-brain.</summary>
    public static string AskMulti(string question, int topK, double reranker = 1, double minEvidence = double.NaN,
        string[]? brains = null, Basic.AI.Knowledge.BrainScope? scope = null)
        => RagAskPipeline.Ask(RagContextMulti(question, topK, reranker, minEvidence, brains, scope), question);

    private static RagContextBundle BuildBundle(string searchJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(searchJson);
            if (!doc.RootElement.TryGetProperty("hits", out var hits) || hits.ValueKind != JsonValueKind.Array)
                return new RagContextBundle(string.Empty, []);
            var results = new List<Basic.SQLite.Vector.VectorSearchResult>();
            foreach (var h in hits.EnumerateArray())
            {
                var sourceId = h.TryGetProperty("sourceId", out var sid) ? sid.GetString() ?? "" : "";
                var text = h.TryGetProperty("text", out var txt) ? txt.GetString() ?? "" : "";
                var score = h.TryGetProperty("score", out var sc) && sc.TryGetSingle(out var f) ? f : 0f;
                var brain = h.TryGetProperty("brain", out var br) ? br.GetString() ?? "" : "";
                results.Add(new Basic.SQLite.Vector.VectorSearchResult(
                    0, 0, sourceId + " [" + brain + "]", text, score, "", null));
            }
            return Basic.AI.Rag.RagContextBundle.FromResults(results);
        }
        catch
        {
            return new RagContextBundle(string.Empty, []);
        }
    }
}
