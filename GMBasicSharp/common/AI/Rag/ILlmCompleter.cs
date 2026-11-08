// FILE: Basic.AI.Rag.ILlmCompleter
using Basic.Mcp;

namespace Basic.AI.Rag;

/// <summary>Completes a grounded prompt (system + user) — HTTP provider LATER.</summary>
public interface ILlmCompleter
{
    string ModelName { get; }

    string Complete(string systemPrompt, string userPrompt);

    /// <summary>Generates K independent completions for the same prompt (for self-consistency).
    /// Default implementation calls <see cref="Complete"/> K times sequentially.
    /// Providers that support batched generation (e.g. MLX/llamacpp with continuous batching)
    /// should override for efficiency. Runs in a GPM task (park-based).</summary>
    /// <param name="systemPrompt">System prompt (same for all K).</param>
    /// <param name="userPrompt">User prompt (same for all K).</param>
    /// <param name="k">Number of responses to generate (default 5).</param>
    /// <param name="temperature">Sampling temperature (default 0.7).</param>
    /// <param name="maxTokens">Max tokens per response.</param>
    /// <returns>Array of K response strings.</returns>
    string[] AskMulti(string systemPrompt, string userPrompt, int k = 5, float temperature = 0.7f, int maxTokens = 1024)
    {
        var responses = new string[k];
        for (int i = 0; i < k; i++)
        {
            responses[i] = Complete(systemPrompt, userPrompt);
        }
        return responses;
    }
}

/// <summary>Optional extension to <see cref="ILlmCompleter"/> per i provider che espongono un
/// tokenizer (MLX/llamacpp in-process): permette di contare i token di un testo (es. per il
/// monitoraggio di efficienza input_tokens/turn degli agenti). Chi non lo implementa (HTTP)
/// non espone il conteggio — il chiamante ricade su una stima o su null.</summary>
public interface ITokenizingCompleter : ILlmCompleter
{
    /// <summary>Numero di token del testo (usando il tokenizer del provider), o &lt;0 se non
    /// conteggiabile. Mai stub: errore/negativo esplicito se non supportato.</summary>
    int CountTokens(string text);
}

/// <summary>Optional extension to <see cref="IToolCallingCompleter"/> for providers whose
/// multi-round protocol needs STRUCTURED tool results (Anthropic: content block
/// <c>tool_result</c> with <c>tool_use_id</c>) instead of plain text in the transcript.</summary>
public interface IToolResultAwareCompleter : IToolCallingCompleter
{
    /// <summary>Continues the conversation after one or more tool executions. Each entry
    /// carries the original <c>tool_use_id</c> so the provider can link the result back.</summary>
    ToolCallOutcome CompleteWithToolResults(
        string systemPrompt,
        string userPrompt,
        IReadOnlyList<Basic.Mcp.McpToolDefinition> tools,
        IReadOnlyList<ToolExecution> executed);
}

/// <summary>Esito dell'esecuzione di un tool per il round successivo.</summary>
public sealed record ToolExecution(string ToolUseId, string Name, string Result);

/// <summary>Outcome di una generazione con tool-calling nativo: testo finale OPPURE
/// tool_calls strutturati (mai entrambi).</summary>
public sealed record ToolCallOutcome(string? Text, IReadOnlyList<Basic.Mcp.ToolCallRequest>? ToolCalls);

/// <summary>Optional extension to <see cref="ILlmCompleter"/> for HTTP completers that support
/// NATIVE function/tool calling (OpenAI/Ollama <c>tools</c>+<c>tool_choice</c>). Callers use it
/// instead of prompting for ad-hoc JSON + <c>ToolCallParser</c>: the model returns structured
/// <c>tool_calls</c> directly. Implementations that don't support it simply don't implement the
/// interface.</summary>
public interface IToolCallingCompleter : ILlmCompleter
{
    /// <summary>Generates one turn aware of <paramref name="tools"/>. The outcome is either a
    /// final answer (<see cref="ToolCallOutcome.Text"/>) or structured tool calls
    /// (<see cref="ToolCallOutcome.ToolCalls"/>).</summary>
    ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<Basic.Mcp.McpToolDefinition> tools);
}

/// <summary>Optional extension to <see cref="ILlmCompleter"/> for completers that can deliver
/// the reply incrementally (e.g. SSE streaming from an OpenAI-compatible API) instead of one
/// blocking <see cref="ILlmCompleter.Complete"/> call. Callers check for this via
/// <c>completer is IStreamingLlmCompleter</c> — implementations that don't support it simply
/// don't implement the interface, and callers fall back to <see cref="ILlmCompleter.Complete"/>
/// (delivering the whole answer as a single chunk). The iterator yields content DELTAS — the
/// caller is responsible for concatenating them into the full answer.</summary>
public interface IStreamingLlmCompleter : ILlmCompleter
{
    /// <summary>Yields each content delta of the reply (token by token for a streaming
    /// provider, or a single yield with the whole answer). Must be fully materialized by the
    /// caller on the same thread it was requested from.</summary>
    IEnumerable<string> CompleteStream(string systemPrompt, string userPrompt);
}

/// <summary>Optional extension to <see cref="ILlmCompleter"/> for completers that can keep a
/// native conversation session alive across turns (e.g. an in-process KV cache) instead of
/// re-sending/re-tokenizing the full transcript on every call. Callers check for this via
/// <c>completer is ISessionAwareLlmCompleter</c> — implementations that don't support it simply
/// don't implement the interface, and callers fall back to <see cref="ILlmCompleter.Complete"/>.</summary>
public interface ISessionAwareLlmCompleter : ILlmCompleter
{
    /// <summary>Creates a new persistent session, returns its handle.</summary>
    int CreateSession();

    /// <summary>Appends one chat turn to the session and decodes only the new text into its
    /// KV cache — prior turns are never re-tokenized.</summary>
    void AppendMessage(int sessionId, string role, string content);

    /// <summary>Generates a reply continuing from the session's current state (i.e. after the
    /// most recent <see cref="AppendMessage"/> call) and records it as an assistant turn.</summary>
    string GenerateFromSession(int sessionId, int maxTokens);

    /// <summary>Releases a session's KV cache. Safe to call on an already-destroyed handle.</summary>
    void DestroySession(int sessionId);
}

/// <summary>Optional extension to <see cref="ISessionAwareLlmCompleter"/> for completers that
/// can deliver the session's reply TOKEN BY TOKEN (generating with max_tokens=1 repeatedly —
/// the native KV cache advances between calls). Callers check via
/// <c>completer is ISessionStreamingCompleter</c> and gate it per agent
/// (<c>AI AGENT SET nome, sessionstreaming, ON</c>), falling back to
/// <see cref="ISessionAwareLlmCompleter.GenerateFromSession"/> (whole reply) when off.</summary>
public interface ISessionStreamingCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Generates the reply in streaming, invoking <paramref name="onToken"/> for each
    /// decoded token. Returns the concatenated full reply (and records it as the assistant
    /// turn, same as <see cref="GenerateFromSession"/>).</summary>
    string GenerateFromSessionStream(int sessionId, int maxTokens, Action<string> onToken);
}

/// <summary>Optional extension to <see cref="ISessionAwareLlmCompleter"/> for completers that can
/// avanzare un SET di sessioni vive in UN solo forward batched per step (continuous batching,
/// Fase 3b). Il server OpenAI-compatible C# lo usa nello scheduler multi-richiesta: con >=2
/// sessioni attive ogni step decodifica un token per TUTTE in un unico forward (attention varlen +
/// FFN/proiezioni batched, parita' bit col decode sequenziale). Chi non lo implementa resta sul
/// decode per-sessione; il chiamante verifica <c>completer is ISessionBatchCompleter</c>.</summary>
public interface ISessionBatchCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Genera in parallelo (un forward batched per step) fino a maxTokens[i] per la
    /// sessione sessionIds[i], invocando onToken(i, piece) per ogni token decodificato (sul thread
    /// chiamante). Greedy/EOS come il decode nativo. Ritorna il testo completo per sessione
    /// (stesso ordine di sessionIds).</summary>
    string[] GenerateManyStream(int[] sessionIds, int[] maxTokens, Action<int, string> onToken);
}

/// <summary>Optional extension to <see cref="ISessionAwareLlmCompleter"/> for completers that
/// can override the SAMPLING of an existing, already-created session (per-request
/// temperature/top_p/top_k/seed di un server). La sampling default del provider (config alla
/// creazione) resta se il chiamante non imposta alcun override. Convenzione identica al resto:
/// chi non lo supporta semplicemente non implementa l'interfaccia, e il chiamante usa la
/// sampling fissa del provider.</summary>
public interface ISessionSamplingCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Override della sampling sulla sessione <paramref name="sessionId"/> già viva
    /// (deve essere stata creata da <see cref="ISessionAwareLlmCompleter.CreateSession"/>).
    /// Semantica allineata al runtime nativo: <paramref name="temperature"/> NaN o &lt;=0 =
    /// greedy deterministico; top_p/top_k &lt;=0 (o top_p&gt;=1) = disabilitati; seed 0 = default
    /// nativo. Mai stub: sessione inesistente -> eccezione esplicita.</summary>
    void SetSessionSampling(int sessionId, float temperature, float topP, int topK, long seed);
}

/// <summary>Optional extension per le penalty OpenAI-style su una sessione viva (§I).
/// Semantica allineata al runtime nativo: <paramref name="repetitionPenalty"/> &gt; 1 penalizza i
/// token già visti (1 = off); <paramref name="frequencyPenalty"/> / <paramref name="presencePenalty"/>
/// &gt;= 0 (0 = off; <c>logits[t] -= fp*count(t)</c> e <c>-= pp</c> se count&gt;0). Si applicano nel
/// path sampling (temperature &gt; 0), come la repetition penalty. Mai stub: sessione inesistente
/// -&gt; eccezione esplicita.</summary>
/// <summary>§D R4.6 — deploy di un adapter LoRA (multi-modulo) nel provider: carica il file
/// prodotto dal training (`DeployLora`), lo abilita/disabilita (`SetLoraEnabled`) e lo scarica
/// (`UnloadLora`). Mai stub: load fallito -&gt; eccezione esplicita.</summary>
public interface ILoraAdapterCompleter
{
    void DeployLora(string path);
    void SetLoraEnabled(bool enabled);
    void UnloadLora();
}

public interface ISessionPenaltyCompleter : ISessionAwareLlmCompleter
{
    void SetSessionPenalties(int sessionId, float repetitionPenalty, float frequencyPenalty, float presencePenalty);
}

/// <summary>Optional extension to <see cref="ISessionAwareLlmCompleter"/> for completers that
/// can serialize a session's full state (KV cache + transcript bookkeeping) to disk and
/// recreate it later — FASE 2b, so a long-lived agent doesn't have to hold every session's KV
/// cache in RAM forever. Callers check for this via
/// <c>completer is IDiskSwappableSessionCompleter</c>, same optional-interface convention as
/// <see cref="ISessionAwareLlmCompleter"/> itself — implementations that don't support it (e.g.
/// MLX, which has no persistent KV cache across calls at all yet) simply don't implement it.</summary>
public interface IDiskSwappableSessionCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Serializes <paramref name="sessionId"/>'s full state to files under
    /// <paramref name="pathPrefix"/>, written atomically (temp file + rename — a crash mid-write
    /// never corrupts a previously swapped-out session at the same prefix). Does NOT destroy the
    /// session — call <see cref="ISessionAwareLlmCompleter.DestroySession"/> afterward to
    /// actually free the RAM. Returns true on success.</summary>
    bool SwapOutSession(int sessionId, string pathPrefix);

    /// <summary>Recreates a session from files written by <see cref="SwapOutSession"/>. Returns
    /// the NEW session handle, or -1 on failure (missing/corrupt files, incompatible model). The
    /// original swapped-out handle is gone and must not be reused.</summary>
    int SwapInSession(string pathPrefix);

    /// <summary>This completer's swap-eviction policy, read once from its provider config
    /// (kvSwap* keys) at construction. Callers that manage multiple sessions across agents (see
    /// <c>AgentServer</c>) use this to decide whether/when to swap the least-recently-used
    /// session for THIS completer to disk before creating a new one.</summary>
    SwapPolicy GetSwapPolicy();
}

/// <summary>Optional extension to <see cref="ISessionAwareLlmCompleter"/> for completers that can
/// decide, at the native driver level via constrained decoding, whether to answer a turn directly
/// or call one of the given tools -- FASE 3 ("dispatch a puntatore di funzione"). Unlike the
/// legacy path (<c>AgentToolLoop</c>'s default: prompt-engineer a JSON instruction into the
/// system prompt, then hope the model's free-text reply parses as valid JSON via
/// <see cref="Basic.Mcp.ToolCallParser"/>), a completer implementing this interface never asks
/// the model to spell out JSON syntax token by token -- every syntactic/structural token (braces,
/// quotes, key names) is injected as literal, zero-decode-cost text by the completer itself; the
/// model only ever makes the actual decisions (which tool, which argument values), one
/// constrained token at a time. Callers check for this via
/// <c>completer is INativeToolDispatchCompleter</c>, same optional-interface convention as
/// <see cref="ISessionAwareLlmCompleter"/> itself -- implementations that don't support it (e.g.
/// llama.cpp, which stays on the legacy JSON path for now; see
/// Native-LLM-Provider-Plan.md FASE 3) simply don't implement it.</summary>
public interface INativeToolDispatchCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Decides the session's current turn: either a tool to call (with its arguments
    /// already resolved and JSON-encoded) or a complete final-answer text -- exactly one of
    /// <see cref="NativeDispatchResult.ToolCall"/>/<see cref="NativeDispatchResult.FinalAnswer"/>
    /// is non-null. Unlike <see cref="ISessionAwareLlmCompleter.GenerateFromSession"/>, this
    /// consumes and records the ENTIRE turn itself (including the final-answer text, when that's
    /// the outcome) -- callers must not also call <c>GenerateFromSession</c> afterward for the
    /// same turn, since the native session's KV-cache is append-only and the turn is already
    /// fully committed either way.
    ///
    /// v1 argument coverage: enum-constrained strings, boolean, and number/integer only --
    /// free-form string arguments (no enum) throw rather than being silently mishandled (found
    /// genuinely unreliable live; see the implementing completer's DecodeArgumentValue for what
    /// went wrong). Tools with an unsupported argument shape simply can't be dispatched
    /// natively yet.</summary>
    NativeDispatchResult DispatchTool(int sessionId, IReadOnlyList<McpToolDefinition> tools);
}

/// <summary>Outcome of one <see cref="INativeToolDispatchCompleter.DispatchTool"/> call. Exactly
/// one field is non-null.</summary>
public readonly record struct NativeDispatchResult(ToolCallRequest? ToolCall, string? FinalAnswer);

/// <summary>Swap-eviction policy for one completer instance. <see cref="Enabled"/> false means
/// "never swap" — unlimited hot sessions, the FASE 2 behavior from before FASE 2b existed.
/// <see cref="SwapDir"/> is the completer's own default (mirrors GmbiPaths kv-swap:
/// convention), already '~'-expanded — callers append their own per-session filename.
/// <see cref="MaxKvBytes"/> (D1 RAM watermark, 2026-09-10): 0 = disabled, only the session
/// count cap applies; &gt;0 = evict LRU until the completer's hot sessions' total KV state
/// (as measured by <c>gmb_ai_session_cache_stats</c> bytes) stays under the budget. Both
/// caps can be active at once — whichever trips first evicts.</summary>
public readonly record struct SwapPolicy(bool Enabled, int MaxHotSessions, string SwapDir, long MaxKvBytes = 0);
