// FILE: Basic.AI.Rag.RagAskPipeline
using Basic.SQLite.Vector;

namespace Basic.AI.Rag;

/// <summary>Grounded RAG: hybrid retrieval → prompt → LLM completion.</summary>
public static class RagAskPipeline
{
    private const string InsufficientMessage = "[insufficient evidence] No relevant documents found.";

    private const string DefaultSystemPrompt =
        "Answer using only the provided context. If the answer is not in the context, say you do not know.";

    /// <summary>Runs hybrid search and completes with the configured <see cref="ILlmCompleter"/>.</summary>
    public static string Ask(
        RagContextBundle bundle,
        string question,
        ILlmCompleter? completer = null,
        string? systemPrompt = null)
    {
        ArgumentNullException.ThrowIfNull(bundle);
        ArgumentException.ThrowIfNullOrWhiteSpace(question);

        if (bundle.Attributions.Count == 0 || string.IsNullOrWhiteSpace(bundle.Context))
            return InsufficientMessage;

        completer ??= LlmCompleterResolver.Default;
        if (completer is null)
            // Parità C (gmb_llm_complete) e sentinella canonica NoCompleter (mai testo finto).
            return Basic.AI.Agents.AiWorkerRuntime.NoCompleter;
        systemPrompt ??= DefaultSystemPrompt;
        var userPrompt = $"Context:\n{bundle.Context}\n\nQuestion: {question}";
        return completer.Complete(systemPrompt, userPrompt);
    }
}
