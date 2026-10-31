// FILE: Basic.AI.Knowledge.Enricher
// §2 del piano Second Brain — LLM Enrichment del vault: SUMMARIZATION/TAGGING/
// ENTITY_EXTRACTION su un testo, e QA.ANSWER grounded (retrieval vettoriale da un brain
// RAG + completamento LLM con citations). Usa l'LLM attivo (LlmCompleterResolver.Default,
// = AI.ACTIVE %CHAT) e la pipeline RAG condivisa (AiVectorRuntime/Ask). Mai stub: se non
// c'è completer configurato, ritorna errore esplicito.
using Basic.AI.Rag;

namespace Basic.AI.Knowledge;

/// <summary>Tipo di enrichment richiesto su un testo.</summary>
public enum EnrichMode
{
    Summarization,
    Tagging,
    EntityExtraction,
}

/// <summary>Enrichment LLM del vault (sintesi/tag/entità) + QA grounded con citations.
/// Riusabile da REPL, CLI e BASIC/agenti.</summary>
public static class Enricher
{
    private const string NoCompleter = "Error: nessun provider LLM configurato (AI SET ACTIVE)";

    /// <summary>Esegue l'enrichment richiesto sul testo. Ritorna il testo LLM (sintesi,
    /// lista tag o entità) — mai un risultato silenzioso: senza completer → errore esplicito.</summary>
    public static string Enrich(string text, EnrichMode mode)
    {
        if (string.IsNullOrWhiteSpace(text)) return "";
        var completer = LlmCompleterResolver.Default;
        if (completer is null) return NoCompleter;

        var (system, user) = mode switch
        {
            EnrichMode.Summarization => (
                "Sei un assistente che sintetizza note di un vault di conoscenza. " +
                "Produci SOLO il riassunto, senza premesse, senza citazioni.",
                $"Sintetizza il seguente testo in un riassunto conciso (max ~100 parole):\n\n{Truncate(text, 8000)}"),
            EnrichMode.Tagging => (
                "Sei un classificatore di note. Ritorna SOLO una lista di tag separati da virgola, " +
                "in minuscolo, senza altri testo.",
                $"Suggerisci 3-8 tag per questa nota (argomenti chiave, concetti, domini):\n\n{Truncate(text, 8000)}"),
            EnrichMode.EntityExtraction => (
                "Sei un estrattore di entità. Ritorna SOLO un JSON con chiavi PERSON, ORG, DATE, " +
                "LOC e PRODUCT, ciascuna una lista di stringhe (vuota se assente).",
                $"Estrai le entità dal seguente testo:\n\n{Truncate(text, 8000)}"),
            _ => throw new ArgumentOutOfRangeException(nameof(mode)),
        };
        return completer.Complete(system, user).Trim();
    }

    /// <summary>Suggerisce tag (alias comodo di Enrich in modalità Tagging).</summary>
    public static string SuggestTags(string text) => Enrich(text, EnrichMode.Tagging);

    /// <summary>QA.ANSWER(question, brain, contextWindow): retrieval vettoriale dal brain RAG
    /// (db <brain>.db, collezione wiki), prompt grounded Context+Question → LLM. Ritorna JSON
    /// con risposta + citations [{sourceId, score}] — o errore esplicito se non risolvibile.</summary>
    public static string QaAnswer(string question, string brain, int contextWindow = 3)
    {
        if (string.IsNullOrWhiteSpace(question)) return "{\"status\":\"Error\",\"error\":\"domanda vuota\"}";
        if (contextWindow <= 0) contextWindow = 3;

        var dbPath = Basic.AI.Knowledge.BrainRegistry.RagDbPath(brain);
        if (dbPath is null || !File.Exists(dbPath))
            return $"{{\"status\":\"Error\",\"error\":\"brain '{brain}' non disponibile come rag\"}}";

        var dbAlias = "QaDb";
        RagDbSession.Open(dbPath, dbAlias);
        var bundle = AiVectorRuntime.HybridSearchBundle(dbAlias, "wiki", question, contextWindow);
        if (bundle.Attributions.Count == 0)
            return "{\"status\":\"InsufficientEvidence\",\"error\":\"nessun chunk rilevante\",\"citations\":[]}";

        var completer = LlmCompleterResolver.Default;
        if (completer is null)
            return "{\"status\":\"Error\",\"error\":\"" + NoCompleter + "\"}";
        var answer = RagAskPipeline.Ask(bundle, question, completer);

        var citations = bundle.Attributions.Select(a => new { a.SourceId, a.Score }).ToList();
        return System.Text.Json.JsonSerializer.Serialize(new
        {
            status = "Success",
            brain,
            question,
            answer,
            citations,
        });
    }

    /// <summary>Trunca il testo a maxChars (per limitare il prompt).</summary>
    private static string Truncate(string text, int maxChars)
        => text.Length <= maxChars ? text : text[..maxChars] + "…";
}
