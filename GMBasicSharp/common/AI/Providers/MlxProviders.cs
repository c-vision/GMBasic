// FILE: Basic.Providers.MlxProviders
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.AI.Rag;
using Basic.AI.Speculative;
using Basic.Mcp;

using CommonLib;

namespace Basic.Providers;

/// <summary>P/Invoke surface over LLMEnv/LLMEngine/mlx/gmb_host_ai_mlx.{h,cpp} (built as
/// libgmb_ai_mlx.{dylib,so}/gmb_ai_mlx.dll) -- the FASE 1 MLX engine (config-driven, all 48
/// layers, KV-cache, greedy+EOS generation; see Fase1-MLX-Qwen3MoE-Plan.md FASE 1.5).</summary>
/// <summary>Facade pubblica del registro opzioni generico (gmb_runtime): un comando, N voci.</summary>
public static class MlxOptions
{
    public static int Set(string name, string value)
    {
        // Il registro opzioni ENGINE vive in LLMEnv (`llm_ai_mlx_opt_set`); i vecchi `gmb_mlx_opts_register`/
        // `gmb_opt_set_persist` NON esistono più nella dylib (rename gmb_*->llm*) -> EntryPointNotFound.
        return MlxEngineNative.llm_ai_mlx_opt_set(name ?? "", value ?? "");
    }

    public static string Get(string name)
    {
        var buf = new byte[1024];
        if (MlxEngineNative.llm_ai_mlx_opt_get_str(name ?? "", buf, buf.Length) != 0) return "";
        int n = System.Array.IndexOf(buf, (byte)0);
        if (n < 0) n = buf.Length;
        return System.Text.Encoding.UTF8.GetString(buf, 0, n);
    }
}

internal static partial class MlxEngineNative
{
    private const string LibName = "gmb_ai_mlx";

    static MlxEngineNative() => AiNativeResolver.EnsureRegistered(typeof(MlxEngineNative).Assembly);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_mlx_load_model(string modelDir);
    // MLX device/stream di default sono PER-THREAD: da chiamare all'ingresso dei worker.
    [LibraryImport(LibName)] internal static partial int llm_ai_mlx_thread_init();

    // GGUF single-file gestito direttamente dal motore MLX (auto-route: arch in ARCH_TABLE,
    // oggi solo DS4 = "deepseek4"). llm_ai_mlx_gguf_tokenizer_json costruisce la JSON del
    // tokenizer (BPE + pre-tokenizer JoyAI) leggendo vocab+merges dal GGUF: ritorna un buffer
    // malloc'ato + lunghezza (da passare a llmtoken_load_json), da liberare con
    // llm_ai_mlx_free_str.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_gguf_supported(string path);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_mlx_gguf_tokenizer_json(string path, out nuint outLen);

    // Meta chat del GGUF: JSON {"architecture","chat_template","eos_token","bos_token"} (buffer
    // malloc'ato + lunghezza, libera con llm_ai_mlx_free_str). Permette di usare il template REALE
    // del modello per i .gguf generici (non solo DS4, che ha il forward dedicato).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_mlx_gguf_chat_meta(string path, out nuint outLen);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_mlx_free_str(IntPtr p);

    /// <summary>Meta chat di un GGUF: architettura + chat_template + eos/bos token string.</summary>
    internal readonly record struct GgufChatMeta(string? Arch, string? ChatTemplate, string? EosToken, string? BosToken);

    /// <summary>Legge il meta chat di un GGUF (arch + chat_template + eos/bos) dalla dylib.
    /// Null su qualunque errore (dylib vecchia senza l'export, GGUF non leggibile): il chiamante
    /// ricade sul fallback deepseek_v4 (comportamento storico per il GGUF DS4).</summary>
    internal static GgufChatMeta? ReadGgufChatMeta(string path)
    {
        var p = llm_ai_mlx_gguf_chat_meta(path, out var len);
        if (p == IntPtr.Zero || len == 0) return null;
        try
        {
            var bytes = new byte[(int)len];
            Marshal.Copy(p, bytes, 0, bytes.Length);
            using var doc = JsonDocument.Parse(bytes);
            var r = doc.RootElement;
            string? S(string k) => r.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() : null;
            return new GgufChatMeta(S("architecture"), S("chat_template"), S("eos_token"), S("bos_token"));
        }
        catch { return null; }
        finally { llm_ai_mlx_free_str(p); }
    }

    // Limite memoria MLX per-modello (MB; 0 = off) — allocator guard configurabile.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_model_set_memory_limit_mb(IntPtr modelHandle, int limitMb);

    // Dtype di calcolo per-modello ("both"|"embed"|"matmul"|"off"), applicato dopo il load:
    // i bit bf16 sono letti solo a forward-time. 0 ok / -1 quant_mode non ammesso / -2 argomento.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_model_set_bf16_mode(IntPtr modelHandle, string mode);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_mlx_free_model(IntPtr handle);

    // Streaming esperti MoE (dir del modello): parametro del motore, NON env.

    // Registro opzioni ENGINE (LLMEnv/LLMEngine/Options): llm_ai_mlx_opt_*.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_opt_set(string name, string value);
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_opt_get_str(string name, byte[] outVal, int cap);
    // §D R4.6 — DEPLOY adapter LoRA multi-modulo: load (file) + clear (unload) + on/off.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_model_lora_load(IntPtr modelHandle, string path);
    [LibraryImport(LibName)]
    internal static partial void llm_ai_mlx_model_lora_clear(IntPtr modelHandle);
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_model_lora_set_enabled(IntPtr modelHandle, [MarshalAs(UnmanagedType.I1)] bool enabled);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_generate(
        IntPtr handle, int[] promptIds, int nPromptIds, int[] outIds, int maxNewTokens, int eosTokenId);

    // FASE 2b: persistent KV-cache session (mirrors gmb_host_ai_llamacpp.h's session split).
    [LibraryImport(LibName)]
    internal static partial IntPtr llm_ai_mlx_create_session(IntPtr modelHandle);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_mlx_destroy_session(IntPtr sessionHandle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_append(IntPtr sessionHandle, int[] tokenIds, int nIds);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_generate(IntPtr sessionHandle, int maxNewTokens, int eosTokenId, int[] outIds);

    // Fase 3b continuous batching: N sessioni (stesso modello) in UN forward batched per step.
    // outIds: n*maxNewTokens row-major (sessione i -> outIds[i*maxNewTokens + t]). Ritorna 0/-1.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_generate_batch(IntPtr[] handles, int n, int maxNewTokens, int eosTokenId, int[] outIds);

    // FASE 3: native tool dispatch -- one constrained-decode step, restricted to allowedIds.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_generate_constrained(IntPtr sessionHandle, int[] allowedIds, int nAllowed, int[] outTokenId);

    // Fase 4: prefix cache — lookup automatico con riuso radix + save
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_session_append_auto_prefix(IntPtr sessionHandle, IntPtr modelHandle, int[] tokenIds, int nIds, int minMatch);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_model_save_prefix(IntPtr modelHandle, string name, IntPtr sessionHandle);

    // Sampling configurabile per-sessione (temperature<=0 = greedy). Per ora
    // NON persistito nel meta-file MLX: il provider riapplica la config a ogni
    // sessione (CreateSession/SwapInSession).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_set_sampling(IntPtr sessionHandle, float temperature, float topP, int topK, long seed);

    // §I penalty separate (OpenAI): repetition (>1 = /penalty; 1 = off), frequency/presence (>=0; 0 = off).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_set_repetition_penalty(IntPtr sessionHandle, float penalty);
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_set_frequency_penalty(IntPtr sessionHandle, float penalty);
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_set_presence_penalty(IntPtr sessionHandle, float penalty);

    // Chunked prefill: dimensione massima dei token per grafo in session_append.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_set_prefill_chunk(IntPtr sessionHandle, int chunkSize);

    // T2 speculative: verify a 1 token (confronta il draft col greedy corrente).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_verify_token(IntPtr sessionHandle, int proposedToken, out int accepted, out int nextToken);

    // T2/DFlash2: tronca la KV-cache ai primi n token (rollback) — riallinea una sessione
    // DRAFT al prefisso confermato dal target dopo un'accettazione parziale.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_truncate(IntPtr sessionHandle, int nTokens);

    // DSpark: P(argmax|context) alla posizione pending — confidenza del greedy (per il gate
    // confidence-scheduled del blocco draft).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_greedy_prob(IntPtr sessionHandle, out float prob);

    // T2 speculative: verify batch dei K draft (tappa 2, lo speedup).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_verify_draft_batch(IntPtr sessionHandle, int[] draftIds, int nDraft, out int accepted, out int nextToken);

    // B — verify batch MULTI-SESSIONE (speculative batching): draftIds row-major [n*maxDraft],
    // draftLen[n] (draft proposti per sessione). outAccepted[n]/outNext[n] per sessione.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_verify_draft_batch_multi(IntPtr[] handles, int n, int[] draftIds, int[] draftLen, int maxDraft, int[] outAccepted, int[] outNext);


    // DFlash2 native round-loop: create/free/generate (block-diffusion draft bind-ato al
    // target). Il draft dir DEVE essere un DFlash2 (create ritorna NULL altrimenti).
    [LibraryImport(LibName)]
    internal static partial IntPtr llm_ai_mlx_dflash2_create(IntPtr targetModelHandle, [MarshalAs(UnmanagedType.LPUTF8Str)] string draftDir);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_mlx_dflash2_free(IntPtr handle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_dflash2_generate(IntPtr handle, IntPtr sessionHandle, int eosTokenId, int maxTokens, int[] outIds);

    // Tappa 3 — round-loop DFlash2 MULTI-SESSIONE (batched): N sessioni, un verify N×K per round.
    // outIds row-major con stride; outCounts[i] = token emessi per la sessione i.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_dflash2_generate_multi(IntPtr handle, IntPtr[] sessions, int n, int eosTokenId, int[] maxTokens, int stride, int[] outIds, int[] outCounts);

    // FASE 2b MLX: swap-out/swap-in su disco della sessione (KV cache + pending state).
    // Scrive <path_prefix>.mlxkv (safetensors: k/v per layer + pending_h) e
    // <path_prefix>.mlxmeta (binario: cache_offset/pending_next_id/has_pending/
    // n_layers/hidden_size), atomico via .tmp + rename(). Swap-in riceve il model handle
    // esplicitamente (puntatore in-processo, non serializzabile) e confronta n_layers/
    // hidden_size contro il modello corrente -- un modello diverso fallisce esplicitamente.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_mlx_session_swap_out(IntPtr sessionHandle, string pathPrefix);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_mlx_session_swap_in(IntPtr modelHandle, string pathPrefix);

    // FASE 5 (RecursiveMAS R5 — latent nativo): hidden states reali (vedi
    // gmb_host_ai_mlx.{h,c}): hidden_size = cfg.hidden_size; session_last_hidden
    // legge pending_h (hidden [1,1,hidden] dell'ultimo token, cast f32) — non tocca
    // il flusso di generazione.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_hidden_size(IntPtr modelHandle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_mlx_session_last_hidden(IntPtr sessionHandle, float[] outBuf, int cap);
}

/// <summary>P/Invoke surface over native/llmtoken/llmtoken.{h,cpp} (built as
/// libllmtoken.{dylib,so}/llmtoken.dll) -- the FASE 1.2 byte-level BPE
/// tokenizer, generic (not MLX-specific), see Fase1-MLX-Qwen3MoE-Plan.md FASE 1.2.</summary>
internal static partial class MlxTokenizerNative
{
    private const string LibName = "llmtoken";

    static MlxTokenizerNative() => AiNativeResolver.EnsureRegistered(typeof(MlxTokenizerNative).Assembly);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llmtoken_load(string tokenizerJsonPath);

    // Carica il tokenizer da una JSON in memoria (byte + lunghezza): la vocab byte-level dei
    // modelli GGUF puo' contenere sequenze non UTF-8 valide, quindi NON si passa una stringa
    // marshalling UTF-8 ma un byte[] esplicito (usato dai modelli MLX single-file, es. DS4).
    [LibraryImport(LibName)]
    internal static partial IntPtr llmtoken_load_json(byte[] json, nuint len);

    // Tokenizer TIKTOKEN (modelli Kimi senza tokenizer.json): legge `tiktoken.model` +
    // `tokenizer_config.json` (special token da added_tokens_decoder). Vedi
    // llmtoken.h / Todo/(open) - Kimi-Tiktoken-Tokenizer-Nativo.md.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llmtoken_load_tiktoken(string tiktokenModelPath, string tokenizerConfigJsonPath);

    [LibraryImport(LibName)]
    internal static partial void llmtoken_free(IntPtr handle);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llmtoken_encode(IntPtr handle, string text, int[]? outIds, int maxIds);

    [LibraryImport(LibName)]
    internal static partial int llmtoken_decode(IntPtr handle, int[] ids, int nIds, byte[]? outText, int maxBytes);
}

/// <summary>Native, in-process completion via the hand-written MLX qwen3_moe engine (FASE 1) --
/// zero HTTP/JSON round trip, same design goal as <see cref="LlamaCppLlmCompleter"/> (FASE 0).
///
/// Implements <see cref="ISessionAwareLlmCompleter"/> (FASE 2b): the native layer
/// (<c>llm_ai_mlx_create_session</c>/<c>_session_append</c>/<c>_session_generate</c>) now grows
/// a persistent per-session KV-cache instead of re-prefilling the entire prompt on every call --
/// this class was previously, deliberately, NOT session-aware (claiming it without that backing
/// would have been the exact bug class diagnosed and fixed in unified-gateway: a prompt whose
/// "already-seen" prefix silently stops being reused). Unlike llama.cpp's native layer, chat
/// templating/tokenization stays HERE in C# (the native side only ever sees already-tokenized
/// ids) -- <see cref="AppendMessage"/> tracks each session's own transcript
/// (<see cref="MlxSessionState"/>) and diffs it exactly like llama.cpp's
/// <c>decode_delta_locked</c> does natively, tokenizing and appending only the new text.
/// <see cref="Complete"/> is implemented on top of the same session primitives -- a throwaway
/// session per call, one code path through the native layer, not two to keep in sync.
///
/// Chat formatting is picked per <c>model_type</c> (FASE 1b.5: read from the model's own
/// config.json, same key the native <c>gmb_host_ai_mlx.c</c> ARCH_TABLE dispatches on) from a
/// small hand-written table (<see cref="ChatTemplatesByModelType"/>) -- not a Jinja template
/// engine, same "fully native" call already made for the tokenizer. Falls back to Qwen's
/// ChatML template (&lt;|im_start|&gt;role\ncontent&lt;|im_end|&gt;\n...) for any model_type not
/// yet in that table (including Gemma4 -- see the table's doc comment for why). <c>eosToken</c>
/// config defaults to the selected template's own stop marker (not a single hardcoded ChatML
/// value anymore) and is resolved to a numeric id via the tokenizer itself at construction
/// time, so an explicitly-configured eos string still overrides it as long as it's a real
/// token in that model's vocabulary.
///
/// See Fase1-MLX-Qwen3MoE-Plan.md (FASE 1.2, 1.5, 1.6) for the full rationale, the native API's
/// own verification history, and the end-to-end text-in/text-out proof this wiring is built on;
/// Native-LLM-Provider-Plan.md FASE 2b for the session-awareness addition, FASE 3 for
/// <see cref="INativeToolDispatchCompleter"/>.
///
/// FASE 3 implements <see cref="INativeToolDispatchCompleter"/> -- chosen as the FIRST completer
/// to get native tool dispatch (rather than llama.cpp, which has vendored GBNF grammar support
/// ready to use) specifically because this engine and its tokenizer are already fully hand-owned,
/// with no vendored dependency eventually slated for replacement. See
/// <see cref="DispatchTool"/> for the mechanism: every syntactic/structural token (JSON braces,
/// quotes, key names, the tool name once resolved, argument separators) is injected as literal
/// text via the raw native session-append primitive -- zero decode cost, never sampled from the
/// model. Only the actual decisions (which tool or "ANSWER", enum/boolean values, numeric
/// digits) go through <c>llm_ai_mlx_session_generate_constrained</c>, one token at a time,
/// restricted to the finitely many valid continuations at that point.</summary>
internal sealed class MlxLlmCompleter : ILlmCompleter, ISessionAwareLlmCompleter, ISessionStreamingCompleter, ISessionSamplingCompleter, ISessionPenaltyCompleter, ILoraAdapterCompleter, ISessionBatchCompleter, IDiskSwappableSessionCompleter, INativeToolDispatchCompleter, ILatentCompleter, ITokenizingCompleter, Basic.AI.Speculative.ISpeculativeTarget, Basic.AI.Speculative.ISpeculativeBatchTarget, IDisposable
{
    // Generous fixed cap for the tokenizer's "query then fill" API -- large enough for any
    // realistic prompt without needing the two-call size-query dance on every request; the
    // native call still reports the real count, so a prompt that somehow exceeded this would
    // be caught (truncated ids array) rather than silently corrupted.
    private const int MaxPromptTokens = 32_768;

    /// <summary>Per-session chat-transcript bookkeeping -- the C# analogue of
    /// gmb_host_ai_llamacpp.cpp's LlamaChatSession.messages/prev_formatted, since MLX's native
    /// layer never sees text, only already-tokenized ids.</summary>
    private sealed class MlxSessionState
    {
        public IntPtr NativeHandle;
        public readonly List<(string Role, string Content)> Messages = [];
        public string PrevFormatted = "";
        // P1 Passo 1 (decode step fuso): `llm_ai_mlx_session_verify_draft_batch` ritorna gia'
        // `nextToken`, ma il delegate `verifyDraftBatch` lo scartava e il loop richiamava
        // `NextGreedy` (una chiamata nativa in piu' per step). Qui si conserva il valore per
        // consumarlo una volta sola. Sentinella int.MinValue = assente; invalidata da CommitToken
        // (che avanza la KV e cambia il pending).
        public int PendingNextFromVerify = int.MinValue;
    }

    private readonly IntPtr _tokenizerHandle;
    private readonly IntPtr _modelHandle;
    // Modello single-file GGUF (DS4): il forward/cache è del motore DS4 (C puro), NON MLX.
    // La prefix-cache/save-prefix (Fase 4) è specifica dei transformer MLX: va saltata.
    private readonly bool _isGgufModel;
    // T3 DFlash2 draft model (speculative): un secondo modello piccolo (qwen3 dense 5-layer)
    // che propone un blocco di token candidati; il target (session KV) li verifica in batch
    // (verify_draft_batch). Config "draftModelDir". Opzionale.
    private readonly IntPtr _draftModelHandle;
    private readonly bool _draftEnabled;
    // DFlash2: runtime nativo del round-loop block-diffusion (draft DFlash2 reale).
    // Solo con "speculation":"dflash"; il draft dir deve essere DFlash2 (create != NULL).
    private readonly IntPtr _dflash2Handle;
    private readonly string _modelName;
    private readonly int _maxTokens;
    private readonly int _contextSize;
    private readonly int _eosTokenId;
    private readonly bool _skipPrefixSave;
    private readonly ChatTranscripts.Builder _buildChatTranscript;
    private readonly ConcurrentDictionary<int, MlxSessionState> _sessions = new();
    private int _nextSessionId;
    private bool _disposed;
    private readonly SwapPolicy _swapPolicy;

    // T1 prefix cache condivisa: riusa la KV di un prefisso di SISTEMA gia' prefillato da un
    // altro agente (via il path swap-out/swap-in ESISTENTE, testato -- niente deep-copy nativa
    // in memoria). key = hash del contenuto del system prompt; path = file .mlxkv/.mlxmeta di
    // una sessione che contiene SOLO quel system prompt. Il primo agente che usa il prompt lo
    // prefiila E lo cachea (swap-out); gli agenti successivi lo swap-in (niente re-prefill).
    private readonly bool _prefixCacheEnabled;
    private readonly string _prefixCachePath;
    private readonly PrefixLruCache _prefixCache;
    private long _prefixCacheHits;
    public long PrefixCacheHits => _prefixCacheHits;

    // Sampling per-sessione (P0): temperature<=0 = greedy deterministico;
    // top_p/top_k <=0 (o top_p>=1) = disabilitati; seed 0 = default nativo.
    private readonly float _temperature;
    private readonly float _topP;
    private readonly int _topK;
    private readonly long _seed;
    private readonly int _prefillChunk;

    // Thinking effort (port di --think-level di DS4 / DeepSeek V4.1): se >0 viene anteposto
    // al system un prefisso "Reasoning Effort: N (range 1-100, ...)" (stesso testo di
    // ds4_deepseek41_reasoning_effort_text). 0/assente = nessun prefisso.
    private readonly int _thinkLevel;
    private readonly string _effortPrefix;

    // T2/T3/T8 speculative: config "speculation":"ngram" (drafter n-gram), "suffix"
    // (drafter a suffissi) o "dflash" (modello DRAFT). Se e' caricato un modello
    // draft (draftModelDir), il drafter e' il modello; altrimenti n-gram/suffix.
    // Solo con sampling greedy (temp<=0), perche' il verify batch nativo garantisce
    // parita' col decode greedy (temp<=0).
    private readonly bool _speculate;
    private readonly int _speculationMax;
    private readonly int _speculationMin;
    private readonly NGramDrafter _drafter = new();
    private readonly SuffixDrafter _suffixDrafter = new();
    private readonly bool _useSuffix;
    private readonly bool _useMtp;
    private readonly float _dsparkConfidence;

    internal MlxLlmCompleter(ProviderDescriptor descriptor)
    {
        var modelDir = ProviderHttp.RequireConfig(descriptor, "modelDir");
        // Modello MLX single-file GGUF (oggi DS4/DeepSeek V4 Flash): tokenizer letto dal GGUF e
        // pesi mmap-ati dal motore MLX; vedi i due blocchi "isGguf" più sotto.
        var isGguf = modelDir.EndsWith(".gguf", StringComparison.OrdinalIgnoreCase);
        _isGgufModel = isGguf;
        _maxTokens = int.TryParse(descriptor.GetConfig("maxTokens"), out var mt) && mt > 0 ? mt : 512;
        _contextSize = int.TryParse(descriptor.GetConfig("contextSize"), out var cs) && cs > 0 ? cs : 32768;

        // P0 sampling: temperature/topP/topK/seed dalla config (PROVIDER.REGISTER).
        // Default: temperature non impostata -> nessuna chiamata set_sampling
        // (il runtime MLX resta greedy argmax, comportamento storico).
        _temperature = float.TryParse(descriptor.GetConfig("temperature"), out var temp) ? temp : float.NaN;
        _topP = float.TryParse(descriptor.GetConfig("topP"), out var tp) ? tp : 0f;
        _topK = int.TryParse(descriptor.GetConfig("topK"), out var tk) ? tk : 0;
        _seed = long.TryParse(descriptor.GetConfig("seed"), out var sd) ? sd : 0;
        // Chunked prefill: dimensione massima dei token per grafo in session_append
        // (default nativo 4096 se non configurato o <=0).
        _prefillChunk = int.TryParse(descriptor.GetConfig("prefillChunk"), out var pc) && pc > 0 ? pc : 0;

        // Thinking effort (DeepSeek V4.1/V4, stile --think-level di DS4): 1..100 = prefisso di
        // reasoning anteposto al system; 0/assente = disattivato. Testo identico a
        // ds4_deepseek41_reasoning_effort_text (upstream antirez/ds4).
        _thinkLevel = int.TryParse(descriptor.GetConfig("thinkLevel"), out var tl) && tl > 0
            ? Math.Min(tl, 100) : 0;
        _effortPrefix = _thinkLevel > 0
            ? $"Reasoning Effort: {_thinkLevel} (range 1-100, the higher the value, the more thorough the reasoning)\n\n"
            : "";

        // T2/T3 speculative: config "speculation":"ngram" (drafter n-gram) o "dflash"
        // (modello DRAFT). Se e' caricato un modello draft (draftModelDir), il drafter e' il
        // modello; altrimenti n-gram. Solo con sampling greedy (temp<=0), perche' il verify
        // batch nativo garantisce parita' col decode greedy (temp<=0).
        var speculationCfg = descriptor.GetConfig("speculation");
        _speculate = speculationCfg is not null
                     && (speculationCfg.Equals("ngram", StringComparison.OrdinalIgnoreCase)
                         || speculationCfg.Equals("suffix", StringComparison.OrdinalIgnoreCase)
                         || speculationCfg.Equals("dflash", StringComparison.OrdinalIgnoreCase)
                         || speculationCfg.Equals("mtp", StringComparison.OrdinalIgnoreCase));
        _useSuffix = speculationCfg is not null
                     && speculationCfg.Equals("suffix", StringComparison.OrdinalIgnoreCase);
        _useMtp = speculationCfg is not null
                  && speculationCfg.Equals("mtp", StringComparison.OrdinalIgnoreCase);
        _useMtp = _useMtp && !string.IsNullOrWhiteSpace(descriptor.GetConfig("draftModelDir"));
        // DSpark confidence threshold: 0 = gate off (blocco fisso); >0 = tronca il blocco al
        // prefisso con P(greedy|ctx) >= soglia (default consigliato 0.6 come il Metal di DS4).
        _dsparkConfidence = float.TryParse(descriptor.GetConfig("dsparkConfidence"), out var dc) && dc > 0f && dc < 1f
            ? dc : 0f;
        _speculationMax = int.TryParse(descriptor.GetConfig("speculationMax"), out var sm) && sm > 0 ? sm : NGramDrafter.DefaultMaxDraft;
        _speculationMin = int.TryParse(descriptor.GetConfig("speculationMin"), out var smn) && smn > 1 ? smn : NGramDrafter.DefaultMinDraft;
        _speculate = _speculate && (float.IsNaN(_temperature) || _temperature <= 0);

        if (isGguf)
        {
            // Tokenizer dal GGUF (vocab+merges+pre-tokenizer): byte[]+len, non stringa UTF-8
            // (la vocab byte-level può non esserlo). Vedi gmb_dsv4_tokenizer_json.h.
            var jsonPtr = MlxEngineNative.llm_ai_mlx_gguf_tokenizer_json(modelDir, out var jsonLen);
            if (jsonPtr == IntPtr.Zero || jsonLen == 0)
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': tokenizer dal GGUF fallito per '{modelDir}'.");
            var jsonBytes = new byte[(int)jsonLen];
            Marshal.Copy(jsonPtr, jsonBytes, 0, jsonBytes.Length);
            MlxEngineNative.llm_ai_mlx_free_str(jsonPtr);
            _tokenizerHandle = MlxTokenizerNative.llmtoken_load_json(jsonBytes, jsonLen);
        }
        else
        {
            var tokenizerJsonPath = Path.Combine(modelDir, "tokenizer.json");
            _tokenizerHandle = MlxTokenizerNative.llmtoken_load(tokenizerJsonPath);
            if (_tokenizerHandle == IntPtr.Zero && !File.Exists(tokenizerJsonPath))
            {
                // Modelli Kimi (es. kimi_linear): NON hanno tokenizer.json BPE HF ma un vocabolario
                // tiktoken (`tiktoken.model`). Loader nativo dedicato (vedi llmtoken.h).
                var tiktokenPath = Path.Combine(modelDir, "tiktoken.model");
                if (File.Exists(tiktokenPath))
                {
                    var cfgPath = Path.Combine(modelDir, "tokenizer_config.json");
                    _tokenizerHandle = MlxTokenizerNative.llmtoken_load_tiktoken(tiktokenPath, cfgPath);
                }
            }
        }
        if (_tokenizerHandle == IntPtr.Zero)
            throw new InvalidOperationException(
                $"mlx provider '{descriptor.Name}': failed to load tokenizer for '{modelDir}'.");

        // Opzione BASIC "swap" (in AI REGISTER, es. "{""swap"":""true""}"): i pesi MLX restano
        // **LAZY/file-backed** (mmap) invece di essere materializzati tutti al load → il SO li
        // pagina da disco on-demand = **swap su disco dei pesi**. Implementato nativamente col flag
        // GMB_NO_PRETOUCH_WEIGHTS (gmb_mlx_pretouch_weights_enabled). Serve ai modelli che non
        // entrerebbero interamente in RAM (es. ~110GB su 128GB). Default: pretouch (tutto
        // residente), come prima. Chiavi accettate: swap|diskSwap|lazyWeights.
        // DEVE stare sopra la guardia di memoria: letta sotto, il caso per cui lo swap esiste
        // (modello > RAM) veniva rifiutato da EnsureFullLoad prima di sapere cosa l'utente ha chiesto.
        var swapCfg = descriptor.GetConfig("swap") ?? descriptor.GetConfig("diskSwap") ?? descriptor.GetConfig("lazyWeights");
        bool wantSwap = bool.TryParse(swapCfg, out var swapRequested) && swapRequested;
        // Il nativo legge LLM_NO_PRETOUCH_WEIGHTS a OGNI load (per-modello, senza cache). La
        // superficie CANONICA è il registro opzioni ENGINE (LLMEnv): l'opzione è REGISTRATA,
        // quindi `llm_ai_mlx_opt_env` la trova e se non settata ritorna NULL SENZA ricadere su
        // `getenv` -> il vecchio `Environment.SetEnvironmentVariable` era IGNORATO (bug: con
        // "swap":"true" il pretouch restava ON e il modello veniva materializzato per intero,
        // es. Qwen3.8 125GB -> budget KV 0). Col registro il valore è letto davvero.
        MlxOptions.Set("LLM_NO_PRETOUCH_WEIGHTS", wantSwap ? "1" : "0");

        if (isGguf)
        {
            // Modello GGUF single-file: i pesi restano nel file mappato (mmap) e gli esperti
            // vengono paginati on-demand, quindi vale il minimo (overhead+KV), NON il full-RAM.
            ModelMemoryGuard.EnsureMinimalLoad($"mlx provider '{descriptor.Name}'", modelDir, _contextSize);
        }
        else
        {
            var mlxBytes = ModelMemoryGuard.SizeOnDisk(modelDir);
            // Con lo swap la guardia chiede il working set caldo, non il file intero; senza, resta il
            // requisito full-RAM di prima (default invariato: nessuna regressione su chi non lo chiede).
            ModelMemoryGuard.EnsureLoad($"mlx provider '{descriptor.Name}'", modelDir, mlxBytes,
                                        _contextSize, wantSwap);
            // M21: libera memoria su TUTTI i provider (LRU) prima di caricare. Il budget è lo STESSO
            // della guardia: con lo swap chiedere il modello intero avrebbe svuotato l'LRU per una
            // quantità di RAM che non serve (e la residenza avrebbe pagato il prezzo del rifiuto).
            ModelResidencyRegistry.EnsureCapacity((ulong)System.Math.Max(0,
                wantSwap ? ModelMemoryGuard.HotSetBytes(mlxBytes) : mlxBytes),
                exempt: descriptor.GetConfig("modelVersion") ?? Path.GetFileName(modelDir.TrimEnd('/', '\\')));
        }

        _modelHandle = RunBigStack(() => MlxEngineNative.llm_ai_mlx_load_model(modelDir));
        if (_modelHandle == IntPtr.Zero)
        {
            MlxTokenizerNative.llmtoken_free(_tokenizerHandle);
            throw new InvalidOperationException(
                $"mlx provider '{descriptor.Name}': failed to load model at '{modelDir}'.");
        }

        // Allocator guard MLX configurabile (MB; 0/assente = off): limite memoria dell'allocatore
        // MLX per evitare la saturazione imprevedibile da ritenzione "active" su eval grandi
        // (vedi Knowledge/MLX-Architetture-Knowledge-Base.md §10.7). Troppo conservativo degrada la
        // performance: lasciarlo off di default e attivarlo solo dove serve.
        if (int.TryParse(descriptor.GetConfig("mlxMemoryLimitMB"), out var memMb) && memMb > 0)
        {
            if (MlxEngineNative.llm_ai_mlx_model_set_memory_limit_mb(_modelHandle, memMb) != 0)
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': failed to set mlxMemoryLimitMB={memMb}.");
        }

        // Dtype di calcolo (default del motore: fp32 sui densi, bf16 sugli MoE validati).
        // Misura 2026-09-08: bf16 sui densi 4-bit vale +16%/+27% di decode su Qwen3.6-27B-4bit
        // (3/3 ripetizioni), zero su un 8-bit (bf16 dimezza le attivazioni, non i pesi quantizzati).
        // Un valore non riconosciuto è un errore, non un ignore: ignorarlo in silenzio è esattamente
        // come il "bf16 == fp32" è passato per un leak invece che per un non-test.
        var dtypeCfg = descriptor.GetConfig("mlxComputeDtype");
        if (!string.IsNullOrWhiteSpace(dtypeCfg))
        {
            var mode = dtypeCfg.Trim().ToLowerInvariant() switch
            {
                "bf16" or "both" or "bfloat16" => "both",
                "bf16-embed" or "embed" => "embed",
                "bf16-matmul" or "matmul" => "matmul",
                "fp32" or "off" or "float32" => "off",
                _ => ""
            };
            if (mode.Length == 0)
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': mlxComputeDtype='{dtypeCfg}' non valida " +
                    "(attese: bf16, bf16-embed, bf16-matmul, fp32).");
            int rc = MlxEngineNative.llm_ai_mlx_model_set_bf16_mode(_modelHandle, mode);
            if (rc == -1)
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': mlxComputeDtype='{dtypeCfg}' rifiutata dal motore " +
                    "per questo modello — bf16 è ammesso solo su quantizzazione affine/none (i formati a " +
                    "blocchi mxfp4/mxfp8/nvfp4 sono più lenti o crashano) e llama è escluso perché bf16 vi " +
                    "produce output errato in modo silenzioso. Il motivo esatto è su stderr.");
            if (rc != 0)
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': llm_ai_mlx_model_set_bf16_mode('{mode}') rc={rc}.");
        }

        // T3 DFlash2 draft model (opzionale): config "draftModelDir". Il draft condivide il
        // tokenizer del target (stesso vocab); il suo forward (session_append ids grezzi +
        // session_generate) propone un blocco di token candidati. Mai stub: se il load
        // fallisce, errore esplicito (il draft e' una scelta di config, non un fallback).
        var draftDir = descriptor.GetConfig("draftModelDir");
        if (!string.IsNullOrWhiteSpace(draftDir))
        {
            var expanded = Basic.ModelDownload.PathExpansion.Expand(draftDir);
            _draftModelHandle = RunBigStack(() => MlxEngineNative.llm_ai_mlx_load_model(expanded));
            if (_draftModelHandle == IntPtr.Zero)
            {
                MlxTokenizerNative.llmtoken_free(_tokenizerHandle);
                MlxEngineNative.llm_ai_mlx_free_model(_modelHandle);
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': failed to load draft model at '{expanded}'.");
            }
            _draftEnabled = true;
        }
        else
        {
            _draftModelHandle = IntPtr.Zero;
            _draftEnabled = false;
        }

        // DFlash2: AUTO-RILEVATO. Se c'e' un draftModelDir e il drafter e' un DFlash2 reale
        // (create != NULL), il round-loop nativo (propose+verify) si attiva da solo, senza dover
        // scrivere speculation:"dflash" in config. Se speculation e' esplicitamente
        // ngram/suffix/mtp non si tocca. Errore esplicito SOLO se "dflash" era richiesto
        // esplicitamente (mai stub silenzioso).
        bool explicitDflash = speculationCfg is not null &&
                              speculationCfg.Equals("dflash", StringComparison.OrdinalIgnoreCase);
        bool otherSpec = speculationCfg is not null &&
                         (speculationCfg.Equals("ngram", StringComparison.OrdinalIgnoreCase) ||
                          speculationCfg.Equals("suffix", StringComparison.OrdinalIgnoreCase) ||
                          speculationCfg.Equals("mtp", StringComparison.OrdinalIgnoreCase));
        _dflash2Handle = IntPtr.Zero;
        if (!_useMtp && !otherSpec && !string.IsNullOrWhiteSpace(draftDir))
        {
            var h = RunBigStack(() => MlxEngineNative.llm_ai_mlx_dflash2_create(_modelHandle, draftDir));
            if (h != IntPtr.Zero)
            {
                _dflash2Handle = h;
                _speculate = true;   // auto-attiva la modalita' speculativa (greedy-only, vedi gate temp)
            }
            else if (explicitDflash)
            {
                Dispose();
                throw new InvalidOperationException(
                    $"mlx provider '{descriptor.Name}': draft '{draftDir}' non e' un DFlash2 o create e' fallito.");
            }
        }

        // FASE 1b.5: chat template selezionato per model_type.
        // Modelli con config.json: model_type + chat_template REALE (Jinja) dalla cartella.
        // GGUF single-file: meta dal motore (general.architecture + tokenizer.chat_template +
        // eos/bos). DS4 resta sulla tabella scritta a mano (forward dedicato); un GGUF generico
        // (es. olmoe) usa il SUO template invece di assumere deepseek_v4, che darebbe un eos
        // inesistente ("did not encode to exactly one token").
        var ggufMeta = isGguf ? MlxEngineNative.ReadGgufChatMeta(modelDir) : null;
        var modelType = isGguf
            ? (ggufMeta?.Arch ?? "deepseek_v4")
            : ChatTranscripts.ReadModelType(modelDir);
        ChatTranscripts.Template? dirTemplate = isGguf
            ? ChatTranscripts.FromGgufMeta(ggufMeta?.Arch, ggufMeta?.ChatTemplate, ggufMeta?.EosToken, ggufMeta?.BosToken)
            : ChatTranscripts.ResolveFromDir(modelDir);
        var template = dirTemplate ?? ChatTranscripts.Resolve(modelType);
        _buildChatTranscript = template.Build;

        var eosToken = descriptor.GetConfig("eosToken") ?? template.DefaultEosToken;
        var eosIds = Encode(eosToken);
        // GGUF: se l'eos del template non e' un token del vocabolario (o e' vuoto -- il converter
        // non sempre scrive `tokenizer.ggml.eos_token_id`), prova l'eos dichiarato dal GGUF e,
        // in ultima istanza, l'eos della tabella per model_type (es. llama -> <|eot_id|>).
        if (isGguf && eosIds.Length != 1 && ggufMeta?.EosToken is { Length: > 0 } gEos)
        {
            eosToken = gEos;
            eosIds = Encode(eosToken);
        }
        if (isGguf && eosIds.Length != 1)
        {
            eosToken = ChatTranscripts.Resolve(modelType).DefaultEosToken;
            eosIds = Encode(eosToken);
        }
        if (eosIds.Length != 1)
        {
            Dispose();
            throw new InvalidOperationException(
                $"mlx provider '{descriptor.Name}': eosToken '{eosToken}' did not encode to exactly one token " +
                $"(got {eosIds.Length}) -- expected a single registered special token.");
        }
        _eosTokenId = eosIds[0];
        // NOTA eos: NON si forza l'eos da `config.json`/`generation_config.json`. Alcuni modelli
        // (kimi_linear) hanno un `tokenizer_config.eos_token` che NON ferma il modello ("[EOS]"
        // -> id base 163585, mentre il vero stop e' `<|im_end|>`), ma altri (gpt_oss/gemma4/…)
        // hanno in `generation_config` un eos che NON coincide con quello della chat template e
        // il C non lo legge. Per non rompere la parità si usa l'eos della chat template come il C,
        // e il rumore residuo (marker `<|im_end|>` ripetuti) viene rimosso da StripChatMarkers
        // (stessa pulizia del C in gmb_brain_mlx_complete).
        // `model_save_prefix` (Fase 4 prefix-cache, chiamata dopo la generazione) NON e' supportata
        // dalle architetture con cache ausiliaria per-layer (kimi_linear: MLA assorbita + KDA con
        // `aux_cache`): lo snapshot fallisce nel motore ("expected a non-empty mlx_array") e lascia
        // la sessione in stato invalido. Su quei modelli si salta (il path C non la usa affatto).
        _skipPrefixSave = !isGguf && modelType is "kimi_linear";

        _modelName = descriptor.GetConfig("modelVersion") ?? Path.GetFileName(modelDir.TrimEnd('/', '\\'));
        // M21: registry unificato — MLX carica tutto in RAM (ACTIVE); evict = Dispose.
        ModelResidencyRegistry.Register(_modelName, "mlx", ModelMemoryGuard.SizeOnDisk(modelDir),
                                        ModelResidencyState.Active, evict: () => Dispose(), device: "metal");

        // FASE 2b policy layer, same keys/idiom as LlamaCppLlmCompleter:
        // kvSwapEnabled=false (default) is exactly pre-FASE-2b behavior --
        // unlimited hot sessions, AgentServer never calls SwapOutSession/
        // SwapInSession. kvQuantize/kvTurboQuant do NOT apply to MLX (those
        // are llama.cpp KV-cache quantization specifics), so they are
        // deliberately not read here.
        var kvSwapEnabled = bool.TryParse(descriptor.GetConfig("kvSwapEnabled"), out var se) && se;
        var kvSwapMaxHotSessions = int.TryParse(descriptor.GetConfig("kvSwapMaxHotSessions"), out var mhs) && mhs > 0 ? mhs : 8;
        // D1 RAM watermark (2026-09-10): stesso contratto del completer llama.cpp —
        // kvSwapMaxKvMb caps i byte di stato KV delle sessioni calde (0/unset = solo conteggio).
        var kvSwapMaxKvMb = long.TryParse(descriptor.GetConfig("kvSwapMaxKvMb"), out var mkb) && mkb > 0 ? mkb : 0;
        var kvSwapPath = descriptor.GetConfig("kvSwapPath") is { } configuredPath && !string.IsNullOrWhiteSpace(configuredPath)
            ? Basic.ModelDownload.PathExpansion.Expand(configuredPath)
            : Path.Combine(GmbiPaths.KvSwapDir, descriptor.Name);
        _swapPolicy = new SwapPolicy(kvSwapEnabled, kvSwapMaxHotSessions, kvSwapPath, kvSwapMaxKvMb * 1024L * 1024L);

        // T1 prefix cache: abilitata via config "prefixCache":"true" (e richiede un path
        // swap). La cache vive in <kvSwapPath>/prefix-cache/. I file del prefisso sono scritti
        // col path swap-out nativo (niente .mlxchat: il restore C# ricostruisce il bookkeeping
        // dalla stringa del system prompt). Capacita' bounded (prefixCacheMax, default 16) con
        // eviction LRU per non accumulare file senza limite.
        _prefixCacheEnabled = bool.TryParse(descriptor.GetConfig("prefixCache"), out var pce) && pce
                              && !string.IsNullOrWhiteSpace(kvSwapPath);
        _prefixCachePath = Path.Combine(kvSwapPath, "prefix-cache");
        var prefixMax = int.TryParse(descriptor.GetConfig("prefixCacheMax"), out var pm) && pm > 0 ? pm : 16;
        _prefixCache = new PrefixLruCache(prefixMax);
    }

    public string ModelName => _modelName;

    // ── ILatentCompleter (R5): hidden states reali via pending_h ──────────────
    public int EmbDim => MlxEngineNative.llm_ai_mlx_hidden_size(_modelHandle);

    public float[] LastHidden(int sessionId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_sessions.TryGetValue(sessionId, out var st)) return [];
        var dim = MlxEngineNative.llm_ai_mlx_hidden_size(_modelHandle);
        if (dim <= 0) return [];
        var buf = new float[dim];
        var n = MlxEngineNative.llm_ai_mlx_session_last_hidden(st.NativeHandle, buf, buf.Length);
        if (n < 0) return [];
        Array.Resize(ref buf, n);
        return buf;
    }

    public float[] EmbedText(string text)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (string.IsNullOrEmpty(text)) return [];
        var ids = Encode(text);
        if (ids.Length == 0) return [];
        var sessionId = CreateSession();
        try
        {
            if (RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_append(_sessions[sessionId].NativeHandle, ids, ids.Length)) != 0)
                return [];
            // pending_h è l'hidden dell'ULTIMO token appeso (pre-lm_head) — l'embedding del testo.
            return LastHidden(sessionId);
        }
        finally
        {
            DestroySession(sessionId);
        }
    }

    /// <summary>Pulizia del rumore di template dall'output GREZZO del modello, PARI a quella di
    /// `gmb_brain_mlx_complete` (C): rimuove i marker di chat (`&lt;|im_start|&gt;`, `&lt;|im_end|&gt;`,
    /// `&lt;/s&gt;`), i marker DeepSeek V4, le righe che sono solo `user`/`assistant`, compatta le righe
    /// vuote multiple e fa trim finale. Necessaria per la parità quando il modello NON si ferma
    /// all'eos (es. kimi_linear: eos `[EOS]` inattivo -> emette `&lt;|im_end|&gt;` a ripetizione) e per
    /// modelli col template che lascia label di turno. No-op sul testo pulito.</summary>
    private static string StripChatMarkers(string text)
    {
        if (string.IsNullOrEmpty(text)) return text;
        var sb = new System.Text.StringBuilder(text.Length);
        int i = 0, lastNl = 0;
        ReadOnlySpan<string> markers = new[] { "<|im_start|>", "<|im_end|>", "</s>",
            "<｜Assistant｜>", "<｜User｜>", "<｜end▁of▁sentence｜>", "<｜begin▁of▁sentence｜>" };
        while (i < text.Length)
        {
            bool stripped = false;
            foreach (var m in markers)
            {
                if (i + m.Length <= text.Length && string.CompareOrdinal(text, i, m, 0, m.Length) == 0)
                { i += m.Length; stripped = true; break; }
            }
            if (stripped) continue;
            int nl = text.IndexOf('\n', i);
            int lineEnd = nl < 0 ? text.Length : nl;
            var line = text.AsSpan(i, lineEnd - i);
            var trimmed = line.Trim(" \t\r");
            if (trimmed.Equals("user", StringComparison.OrdinalIgnoreCase)
                || trimmed.Equals("assistant", StringComparison.OrdinalIgnoreCase))
            { i = nl < 0 ? text.Length : nl + 1; continue; }
            char ch = text[i];
            if (ch == '\n' && lastNl >= 2) { i++; continue; }
            if (ch == '\n') lastNl++; else lastNl = 0;
            sb.Append(ch); i++;
        }
        return sb.ToString().TrimEnd('\n', ' ', '\t');
    }

    public string Complete(string systemPrompt, string userPrompt)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var sessionId = CreateSession();
        try
        {
            // UN SOLO append del transcript [system, user] reso = UN SOLO prefill, esattamente come
            // il lato C (gmb_brain_mlx_complete: render unico + sess_append). Append separati
            // (system poi user) producono lo stesso testo ma un KV con arrotondamenti (ULP) diversi
            // dal prefill in un colpo: su modelli borderline (es. qwen3_moe 8-bit MoE) questo
            // ribalta il decode greedy e rompe la parità fra i due mondi pur con lo STESSO prompt.
            // Vedi Bugs/(open) - Qwen3-MoE-VMC-Segfault-e-C99N-Diverge.md.
            var state = GetSessionState(sessionId);
            var systemWithEffort = _effortPrefix + systemPrompt;
            if (!string.IsNullOrEmpty(systemWithEffort)) state.Messages.Add(("system", systemWithEffort));
            state.Messages.Add(("user", userPrompt));
            var formatted = _buildChatTranscript(state.Messages, addAssistantOpener: true);
            if (Environment.GetEnvironmentVariable("GMB_DUMP_PROMPT") is { Length: > 0 })
                Console.Error.WriteLine($"[GMB_DUMP_PROMPT c#] complete len={formatted.Length}\n{formatted}\n--end--");
            var ids = Encode(formatted);
            if (Environment.GetEnvironmentVariable("GMB_DUMP_PROMPT") is { Length: > 0 })
                Console.Error.WriteLine($"[GMB_DUMP_PROMPT c#ids] n={ids.Length} first={string.Join(",", ids.Take(24))}");
            if (ids.Length > 0)
            {
                var appendRc = RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_append(state.NativeHandle, ids, ids.Length));
                if (appendRc != 0)
                    throw new InvalidOperationException(
                        $"mlx provider: failed to append the transcript to session {sessionId} (model '{_modelName}').");
                state.PrevFormatted = formatted;
            }
            return GenerateFromSession(sessionId, _maxTokens);
        }
        finally
        {
            DestroySession(sessionId);
        }
    }

    public int CreateSession()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var native = RunBigStack(() => MlxEngineNative.llm_ai_mlx_create_session(_modelHandle));
        if (native == IntPtr.Zero)
            throw new InvalidOperationException($"mlx provider: failed to create session (model '{_modelName}').");

        ApplySampling(native);
        var sessionId = Interlocked.Increment(ref _nextSessionId);
        _sessions[sessionId] = new MlxSessionState { NativeHandle = native };
        return sessionId;
    }

    /// <summary>Override per-request della sampling su una sessione viva (surface per i server
    /// OpenAI-compatible). Sempre chiama il runtime: temperature NaN/&lt;=0 = greedy.</summary>
    public void SetSessionSampling(int sessionId, float temperature, float topP, int topK, long seed)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_sessions.TryGetValue(sessionId, out var st))
            throw new InvalidOperationException($"mlx provider: SetSessionSampling su sessione inesistente ({sessionId}).");
        float t = float.IsNaN(temperature) || temperature <= 0f ? 0f : temperature;
        MlxEngineNative.llm_ai_mlx_session_set_sampling(st.NativeHandle, t, topP, topK, seed);
    }

    /// <summary>§D R4.6 — deploy di un adapter LoRA (multi-modulo) nel modello del provider:
    /// carica il file (prodotto dal training) e lo abilita. Poi on/off con SetLoraEnabled, unload
    /// con UnloadLora. Mai stub: load fallito -> eccezione esplicita.</summary>
    public void DeployLora(string path)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (MlxEngineNative.llm_ai_mlx_model_lora_load(_modelHandle, path) != 0)
            throw new InvalidOperationException($"mlx provider: DeployLora fallito ({path}).");
        MlxEngineNative.llm_ai_mlx_model_lora_set_enabled(_modelHandle, true);
    }
    public void SetLoraEnabled(bool enabled)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        MlxEngineNative.llm_ai_mlx_model_lora_set_enabled(_modelHandle, enabled);
    }
    public void UnloadLora()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        MlxEngineNative.llm_ai_mlx_model_lora_clear(_modelHandle);
    }

    /// <summary>§I penalty OpenAI-style separate sulla sessione <paramref name="sessionId"/>>
    /// (surface per i server OpenAI-compatible). repetition&gt;1, frequency/presence&gt;0.</summary>
    public void SetSessionPenalties(int sessionId, float repetitionPenalty, float frequencyPenalty, float presencePenalty)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_sessions.TryGetValue(sessionId, out var st))
            throw new InvalidOperationException($"mlx provider: SetSessionPenalties su sessione inesistente ({sessionId}).");
        if (repetitionPenalty > 0f) MlxEngineNative.llm_ai_mlx_session_set_repetition_penalty(st.NativeHandle, repetitionPenalty);
        if (frequencyPenalty > 0f)  MlxEngineNative.llm_ai_mlx_session_set_frequency_penalty(st.NativeHandle, frequencyPenalty);
        if (presencePenalty > 0f)   MlxEngineNative.llm_ai_mlx_session_set_presence_penalty(st.NativeHandle, presencePenalty);
    }

    /// <summary>Applica la config di sampling e prefill-chunk a una sessione nativa appena
    /// creata/ripristinata. Se temperature non e' configurata, non chiama il runtime
    /// (default greedy storico).</summary>
    private void ApplySampling(IntPtr nativeHandle)
    {
        if (!float.IsNaN(_temperature))
            MlxEngineNative.llm_ai_mlx_session_set_sampling(nativeHandle, _temperature, _topP, _topK, _seed);
        if (_prefillChunk > 0)
            MlxEngineNative.llm_ai_mlx_session_set_prefill_chunk(nativeHandle, _prefillChunk);
    }

    public void AppendMessage(int sessionId, string role, string content)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        // T1 prefix cache condivisa: turno SYSTEM con prefisso in cache -> riusa la KV
        // (swap-in) invece di ri-prefillare. Il contenuto deve combaciare col prefisso
        // (stessa stringa -> stessa formattazione -> stesso KV), quindi corretto per
        // costruzione. Se il restore fallisce, ricade sul prefill normale (fail-open).
        if (role == "system" && _prefixCacheEnabled && TryRestoreSystemPrefix(sessionId, content))
            return;

        var state = GetSessionState(sessionId);

        state.Messages.Add((role, role == "system" ? _effortPrefix + content : content));
        // "tool" included (FASE 3): a tool-result turn is also always followed by an assistant
        // reply, same as "user" -- only "system" doesn't precede one (system is always followed
        // by "user").
        var addAssistantOpener = role is "user" or "tool";
        var formatted = _buildChatTranscript(state.Messages, addAssistantOpener);

        // Diagnostica parità C#/C (branch-first): GMB_DUMP_PROMPT=1 stampa su stderr il transcript
        // RESO dopo ogni append. Il lato C fa lo stesso in gmb_brain_mlx_complete. Per un diff byte
        // esatto del prompt fra i due mondi (usato per il bug qwen3_moe).
        if (Environment.GetEnvironmentVariable("GMB_DUMP_PROMPT") is { Length: > 0 })
            Console.Error.WriteLine($"[GMB_DUMP_PROMPT c#] role={role} len={formatted.Length}\n{formatted}\n--end--");

        // PARITÀ CON LE GAMBE C (branch-first, bug qwen3_moe): il C rende l'INTERO transcript
        // [system, user] e fa UN SOLO sess_append = UN prefill. Il C# appendeva il turno SYSTEM da
        // solo (prefill 1) e poi lo user (prefill 2): STESSO testo, ma il KV ha arrotondamenti
        // (ULP) diversi dal prefill in un colpo → su modelli borderline (qwen3_moe 8-bit MoE) il
        // decode greedy ribalta e le gambe divergono pur con prompt identico. Quindi il turno
        // system NON si appende da solo: si DIFFERISCE; il primo append successivo (user/tool)
        // porta l'intero [system, user] in un colpo, come il C. I turni successivi restano
        // incrementali. Vedi Bugs/(open) - Qwen3-MoE-VMC-Segfault-e-C99N-Diverge.md.
        if (role == "system")
        {
            state.PrevFormatted = "";
            return;
        }

        // Invariante (come decode_delta_locked di llama.cpp): il nuovo transcript deve essere il
        // vecchio + un puro suffisso -- la cronologia cresce solo per append.
        // ECCEZIONE reale: template NON additivi, dove il rendering di un turno PRECEDENTE cambia
        // quando se ne aggiunge uno nuovo. Es. GLM-4.5: un turno assistant rende
        // `<think>{reasoning}</think>` se `loop.index0 > ns.last_user_index`, altrimenti
        // `<think></think>`; aggiungendo un user, `last_user_index` cresce e il turno assistant
        // precedente viene ri-rendered diversamente. Non e' un bug del chiamante: si RESETTA la
        // sessione e si ri-prefilla l'intera trascrizione resa (fail-safe, corretto; piu' lento
        // solo per i modelli con template non additivo).
        if (!formatted.StartsWith(state.PrevFormatted, StringComparison.Ordinal)
            || Basic.Providers.GmbEnv.Get("GMB_FORCE_REPREFILL") is { Length: > 0 })
        {
            if (MlxEngineNative.llm_ai_mlx_session_truncate(state.NativeHandle, 0) != 0)
                throw new InvalidOperationException(
                    $"mlx provider: template non additivo e reset della sessione {sessionId} fallito.");
            state.PrevFormatted = "";
        }

        var delta = formatted[state.PrevFormatted.Length..];
        state.PrevFormatted = formatted;
        if (delta.Length == 0) return; // e.g. a role that doesn't change add_ass and adds no new markup

        var deltaIds = Encode(delta);
        if (Environment.GetEnvironmentVariable("GMB_DUMP_PROMPT") is { Length: > 0 })
            Console.Error.WriteLine($"[GMB_DUMP_PROMPT c#ids] role={role} n={deltaIds.Length} first={string.Join(",", deltaIds.Take(24))}");
        if (deltaIds.Length == 0) return;

        // Fase 4: riuso prefix cache (radix) — lookup automatico con soglia 8 token (come probe: 2.16x–28.80x)
        int minMatch = 8; var ev = Basic.Providers.GmbEnv.Get("GMB_PREFIX_MIN_MATCH"); if (ev != null && int.TryParse(ev, out var mm) && mm > 0) minMatch = mm;
        // DFlash2 esplicito (`_dflash2Handle != 0`): append lineare. La radix prefix-cache è
        // INCOMPATIBILE con la cattura-ctx del drafter. L'engine ora copre anche l'auto-detect
        // (`llm_mlx_df2_will_use` in llm_ai_mlx_session_append_auto_prefix), ma qui si evita del
        // tutto la radix quando il drafter è già creato. La prefix-cache resta per i modelli senza
        // dflash; la correttezza viene prima. Diagnosi 2026-10-03.
        bool plainAppend = _modelHandle == IntPtr.Zero
            || _dflash2Handle != IntPtr.Zero
            || Basic.Providers.GmbEnv.Get("GMB_FORCE_PLAIN_APPEND") is { Length: > 0 };
        int appendRc = plainAppend
            ? RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_append(state.NativeHandle, deltaIds, deltaIds.Length))
            : RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_append_auto_prefix(state.NativeHandle, _modelHandle, deltaIds, deltaIds.Length, minMatch));
        if (appendRc != 0 && !plainAppend)
        {
            // La prefix-cache (radix, auto_prefix) è un'OTTIMIZZAZIONE, non correttezza: se
            // fallisce (rc!=0, es. cattura-ctx del drafter) DEGRADA ad append lineare. Mai far
            // fallire la sessione per la cache. Diagnosi 2026-10-03.
            appendRc = RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_append(state.NativeHandle, deltaIds, deltaIds.Length));
        }
        if (appendRc != 0)
            throw new InvalidOperationException(
                $"mlx provider: failed to append '{role}' turn to session {sessionId} (model '{_modelName}').");

        // T1: dopo un append SYSTEM riuscito (prima occorrenza del prompt), cache il prefisso
        // per il riuso (swap-out nativo). La sessione resta live in RAM; i file servono solo
        // come cache per le sessioni future.
        if (role == "system" && _prefixCacheEnabled)
            TryCacheSystemPrefix(sessionId, content);
    }

    /// <summary>Marker Harmony (gpt-oss). Vedi ChatTranscripts/template gpt-oss.</summary>
    private static readonly string[] HarmonyMarkers =
        { "<|channel|>", "<|start|>", "<|message|>", "<|end|>", "<|return|>", "<|call|>", "<|constrain|>", "<|final|>" };

    /// <summary>Normalizza l'output GREZZO Harmony di gpt-oss: se c'è il canale <c>final</c> ne
    /// estrae il testo (scartando analysis/commentary) tagliando a <c>&lt;|return|&gt;</c>/
    /// <c>&lt;|end|&gt;</c>/<c>&lt;|call|&gt;</c>; altrimenti rimuove tutti i marker <c>&lt;|...|&gt;</c>.
    /// Applicato SOLO se il testo contiene marker Harmony (i modelli normali restano invariati).
    /// Se il risultato è vuoto ritorna il testo originale. Parità con il C (harmony_normalize in
    /// gmb_brain_mlx.c).</summary>
    internal static string NormalizeHarmonyOutput(string text)
    {
        if (string.IsNullOrEmpty(text)) return text;
        bool hasMarker = false;
        foreach (var m in HarmonyMarkers)
            if (text.Contains(m, StringComparison.Ordinal)) { hasMarker = true; break; }
        if (!hasMarker) return text;

        const string finalTag = "<|channel|>final<|message|>";
        int fi = text.IndexOf(finalTag, StringComparison.Ordinal);
        if (fi >= 0)
        {
            var body = text.Substring(fi + finalTag.Length);
            int cut = body.Length;
            foreach (var end in new[] { "<|return|>", "<|end|>", "<|call|>" })
            {
                int k = body.IndexOf(end, StringComparison.Ordinal);
                if (k >= 0 && k < cut) cut = k;
            }
            body = body.Substring(0, cut).Trim();
            if (body.Length > 0) return body;
            return text; // canale final presente ma vuoto: non perdere la risposta
        }

        // Nessun canale final: rimuovi TUTTI i marker <|...|>. Per <|channel|> si rimuove anche
        // il NOME del canale fino al marker successivo (es. "<|channel|>analysis<|message|>").
        var sb = new System.Text.StringBuilder(text.Length);
        for (int i = 0; i < text.Length; i++)
        {
            if (text[i] == '<' && i + 1 < text.Length && text[i + 1] == '|')
            {
                int close = text.IndexOf("|>", i + 2, StringComparison.Ordinal);
                if (close >= 0)
                {
                    if (string.CompareOrdinal(text, i, "<|channel|>", 0, 11) == 0)
                    {
                        int nameStart = text.IndexOf("<|", close + 2, StringComparison.Ordinal);
                        if (nameStart >= 0)
                        {
                            int nameClose = text.IndexOf("|>", nameStart + 2, StringComparison.Ordinal);
                            if (nameClose >= 0) { i = nameClose + 1; continue; }
                        }
                    }
                    i = close + 1; continue;
                }
            }
            sb.Append(text[i]);
        }
        var stripped = sb.ToString().Trim();
        return stripped.Length > 0 ? stripped : text;
    }

    // MLX lega device/stream di default al THREAD: caricare e usare su thread diversi da'
    // "no Stream(gpu,0) in current thread" (verificato: tools/gmb_dfl2_driver + probe C#). Un
    // worker su thread dedicato NON risolve (lo stream del load non e' quello del worker). Quindi
    // le call native girano INLINE sul thread chiamante: il consumer deve garantire stack
    // sufficiente (i path nativi del 27B stanno in >1MB e <8MB: il pool .NET ~1MB non basta,
    // un thread con stack grande si'). Niente piu' `mlx-bigstack`.
    private static T RunBigStack<T>(Func<T> fn)
    {
        // INLINE (2026-10-03): le call MLX DEVONO girare sul thread che ha caricato il modello
        // (device/stream di default MLX sono PER-THREAD). Un worker su un thread diverso le rompe
        // con "no Stream(gpu,0) in current thread" (repro: tools/gmb_dfl2_driver + probe C#).
        // Il chiamante deve garantire stack sufficiente (i path nativi stanno in <8MB: il thread
        // pool .NET ~1MB NON basta -> i test/harness usano un thread con stack grande).
        return fn();
    }

    public string GenerateFromSession(int sessionId, int maxTokens)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var state = GetSessionState(sessionId);

        var limit = maxTokens > 0 ? maxTokens : _maxTokens;
        // Clamping difensivo: mai generare piu' token del contesto configurato. Il
        // motore MLX non traccia il residuo esatto per sessione (a differenza di
        // llama.cpp, che clampa nel nativo), quindi qui usiamo il tetto superiore
        // configurato come guardia -- meglio un output troncato di un KV-cache
        // che cresce oltre ogni limite ragionevole.
        if (limit > _contextSize) limit = _contextSize;

        // T2 speculative n-gram: se attivo (config "speculation":"ngram" + greedy), il decode
        // usa il drafter (NGramDrafter) per proporre e il verify batch nativo per accettare.
        // Output identico al decode greedy (parita' garantita dal verify), solo piu' veloce
        // quando l'acceptance del drafter e' alta (forward batch >> decode singolo).
        int[] generatedIds;
        if (_dflash2Handle != IntPtr.Zero && (float.IsNaN(_temperature) || _temperature <= 0))
        {
            // DFlash2: round-loop block-diffusion NATIVO (propose+verify+capture). Greedy
            // lossless == decode normale (target authoritative); il draft accelera il decode.
            var outIds = new int[limit];
            var generated = RunBigStack(() => MlxEngineNative.llm_ai_mlx_dflash2_generate(_dflash2Handle, state.NativeHandle, _eosTokenId, limit, outIds));
            if (generated < 0)
                throw new InvalidOperationException(
                    $"mlx provider: dflash2 generate failed for session {sessionId} (model '{_modelName}').");
            generatedIds = generated > 0 ? outIds[..generated] : Array.Empty<int>();
        }
        else if (_speculate && limit > 0)
        {
            generatedIds = GenerateSpeculative(state, sessionId, limit);
        }
        else
        {
            var outIds = new int[limit];
            var generated = RunBigStack(() => MlxEngineNative.llm_ai_mlx_session_generate(state.NativeHandle, limit, _eosTokenId, outIds));
            if (generated < 0)
                throw new InvalidOperationException(
                    $"mlx provider: generation failed for session {sessionId} (model '{_modelName}').");
            generatedIds = generated > 0 ? outIds[..generated] : Array.Empty<int>();
        }

        var reply = generatedIds.Length > 0 ? Decode(generatedIds) : "";
        // Harmony (gpt-oss): il modello genera i canali grezzi. Normalizza PRIMA di registrare in
        // state.Messages e di ritornare, cosi' il re-render del template col canale `final` non
        // solleva (raise_exception) e l'agente vede il testo finale.
        reply = NormalizeHarmonyOutput(reply);
        // Stessa pulizia marker/label del C (gmb_brain_mlx_complete): necessaria quando il modello
        // non si ferma all'eos (kimi) o il template lascia label di turno. No-op sul testo pulito.
        reply = StripChatMarkers(reply);

        // Record the reply in the session's own transcript bookkeeping. Its template closing
        // markup is NOT decoded here -- it wasn't generated by the model, so it isn't in the
        // KV cache yet either; the next AppendMessage call's diff picks it up together with the
        // following turn. Same reasoning as gmb_host_ai_llamacpp.cpp's gmb_ai_session_generate.
        state.Messages.Add(("assistant", reply));
        state.PrevFormatted = _buildChatTranscript(state.Messages, addAssistantOpener: false);
        // Fase 4: auto-save del prefisso appena generato (per riuso su richieste successive con stesso system prompt)
        { int minSave = 64; var ev2 = Basic.Providers.GmbEnv.Get("GMB_PREFIX_MIN_SAVE_LEN"); if (ev2 != null && int.TryParse(ev2, out var ms) && ms > 0) minSave = ms;
          if (!_isGgufModel && !_skipPrefixSave && state.PrevFormatted.Length >= minSave) { try { MlxEngineNative.llm_ai_mlx_model_save_prefix(_modelHandle, $"auto_{state.PrevFormatted.Length}_{sessionId}", state.NativeHandle); } catch { } } }
        return reply;
    }

    // ── Fase 3b continuous batching: N sessioni in un forward batched per step ──
    /// <summary>Avanza N sessioni (stesso modello) in UN forward batched per step, un token
    /// ciascuna, fino a maxTokens[i] o EOS, invocando onToken(i, piece) per ogni token. Ritorna il
    /// testo completo per sessione (stesso ordine). Greedy/EOS come il decode nativo; parita' bit
    /// col decode sequenziale. NON aggiorna il transcript bookkeeping (il chiamante distrugge le
    /// sessioni a fine generazione). Vedi native llm_ai_mlx_session_generate_batch.</summary>
    public string[] GenerateManyStream(int[] sessionIds, int[] maxTokens, Action<int, string> onToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int n = sessionIds.Length;
        var results = new string[n];
        if (n == 0) return results;
        // Speculative batching: se il provider ha self-speculation n-gram configurata (greedy), il
        // batch multi-sessione usa il verify speculativo batched N×K. DFlash2/MTP/modello draft
        // usano per default il batched greedy (il loro round-loop per-sessione serializzato e' una
        // regressione MISURATA 0.27x) -> attivabili con l'opt-in GMB_SPEC_BATCH_STRONG_DRAFTER.
        // Il suffisso (_useSuffix) resta sul batched greedy.
        bool strongOptIn = Basic.Providers.GmbEnv.Get("GMB_SPEC_BATCH_STRONG_DRAFTER") is { Length: > 0 };
        bool ngramLike = !_useSuffix && !_draftEnabled && !_useMtp && !_isGgufModel;
        if (_speculate && (float.IsNaN(_temperature) || _temperature <= 0) && (ngramLike || strongOptIn))
            return GenerateSpeculativeManyStream(sessionIds, maxTokens, onToken);
        var handles = new IntPtr[n];
        for (int i = 0; i < n; i++)
        {
            if (!_sessions.TryGetValue(sessionIds[i], out var st))
                throw new InvalidOperationException($"mlx provider: sessione inesistente ({sessionIds[i]}).");
            handles[i] = st.NativeHandle;
        }
        if (n == 1)
        {
            var one = new int[1];
            var g1 = new StringBuilder();
            int max = maxTokens[0] > 0 ? maxTokens[0] : _maxTokens;
            for (int t = 0; t < max; t++)
            {
                int rc1 = MlxEngineNative.llm_ai_mlx_session_generate(handles[0], 1, _eosTokenId, one);
                if (rc1 < 0) throw new InvalidOperationException($"mlx provider: generation failed for session {sessionIds[0]}.");
                int tok1 = one[0];
                if (_eosTokenId >= 0 && tok1 == _eosTokenId) break;
                string p1 = Decode(new[] { tok1 });
                g1.Append(p1); onToken?.Invoke(0, p1);
            }
            results[0] = g1.ToString();
            return results;
        }
        var sb = new StringBuilder[n];
        var done = new bool[n];
        var genCount = new int[n];
        for (int i = 0; i < n; i++) sb[i] = new StringBuilder();
        var activeIdx = new List<int>(n);
        var activeHandles = new IntPtr[n];
        var outIds = new int[n];
        while (true)
        {
            int na = 0;
            for (int i = 0; i < n; i++)
                if (!done[i]) { activeIdx.Add(i); activeHandles[na++] = handles[i]; }
            if (na == 0) break;
            int rc = MlxEngineNative.llm_ai_mlx_session_generate_batch(activeHandles, na, 1, _eosTokenId, outIds);
            if (rc != 0)
                throw new InvalidOperationException($"mlx provider: batch generation failed (n={na}).");
            for (int a = 0; a < na; a++)
            {
                int i = activeIdx[a];
                int tok = outIds[a];
                if (_eosTokenId >= 0 && tok == _eosTokenId) { done[i] = true; continue; }
                string piece = Decode(new[] { tok });
                sb[i].Append(piece); onToken?.Invoke(i, piece);
                if (++genCount[i] >= (maxTokens[i] > 0 ? maxTokens[i] : _maxTokens)) done[i] = true;
            }
            activeIdx.Clear();
        }
        for (int i = 0; i < n; i++) results[i] = sb[i].ToString();
        return results;
    }

    // ── B — Speculative batching: speculative decode MULTI-SESSIONE (batched sulle sessioni) ──
    /// <summary>Come GenerateManyStream ma con self-speculation n-gram per sessione e UN verify
    /// batch multi-sessione per giro (B: batched sulle sessioni, sequenziale sui draft). Parità bit
    /// col decode greedy (temp<=0). Ritorna il testo per sessione (stesso ordine).</summary>
    public string[] GenerateSpeculativeManyStream(int[] sessionIds, int[] maxTokens, Action<int, string> onToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int n = sessionIds.Length;
        var results = new string[n];
        if (n == 0) return results;
        if (!float.IsNaN(_temperature) && _temperature > 0)
            throw new InvalidOperationException("mlx provider: speculative batching richiede sampling greedy (temperature<=0).");
        if (_isGgufModel)
            return GenerateManyStream(sessionIds, maxTokens, onToken); // GGUF/DS4: path dedicato
        // DFlash2 / MTP / modello draft: il round-loop e' PER-SESSIONE (il runtime DFlash2 nativo
        // tiene UNA cache ctx, e MTP una sessione-draft per target). Il batch multi-sessione con
        // drafter forte e' lavoro futuro. GATED: si arriva qui solo con `speculation` configurata
        // (opt-in) -> nessun impatto sui path di default (n-gram/nessuna speculazione).
        if (_dflash2Handle != IntPtr.Zero || _useMtp || _draftEnabled)
        {
            // DFlash2: round-loop MULTI-SESSIONE batched (tappa 3, nativo): un verify N×K per round
            // per TUTTE le sessioni. Opt-in (GMB_SPEC_BATCH_STRONG_DRAFTER): il routing sopra manda
            // i drafter forti al batched greedy di default (il per-sessione serializzato era 0.27x).
            if (_dflash2Handle != IntPtr.Zero && (float.IsNaN(_temperature) || _temperature <= 0))
            {
                var hs = new IntPtr[n];
                var maxes = new int[n];
                for (int i = 0; i < n; i++)
                {
                    hs[i] = GetSessionState(sessionIds[i]).NativeHandle;
                    int limit = maxTokens[i] > 0 ? maxTokens[i] : _maxTokens;
                    if (limit > _contextSize) limit = _contextSize;
                    maxes[i] = limit;
                }
                int stride = 0;
                foreach (var mx in maxes) if (mx > stride) stride = mx;
                var outIds = new int[n * stride];
                var outCounts = new int[n];
                if (RunBigStack(() => MlxEngineNative.llm_ai_mlx_dflash2_generate_multi(_dflash2Handle, hs, n, _eosTokenId, maxes, stride, outIds, outCounts)) != 0)
                    throw new InvalidOperationException($"mlx provider: dflash2 generate_multi failed (n={n}).");
                for (int i = 0; i < n; i++)
                {
                    var one = new StringBuilder();
                    for (int k = 0; k < outCounts[i]; k++)
                    {
                        var piece = Decode(new[] { outIds[i * stride + k] });
                        one.Append(piece);
                        onToken?.Invoke(i, piece);
                    }
                    results[i] = one.ToString();
                }
                return results;
            }
            // MTP / modello draft generico: speculazione per-sessione (il round-loop MTP e' per-sessione).
            for (int i = 0; i < n; i++)
            {
                var st = GetSessionState(sessionIds[i]);
                int limit = maxTokens[i] > 0 ? maxTokens[i] : _maxTokens;
                if (limit > _contextSize) limit = _contextSize;
                var ids = GenerateSpeculative(st, sessionIds[i], limit);
                var one = new StringBuilder();
                foreach (var id in ids)
                {
                    var piece = Decode(new[] { id });
                    one.Append(piece);
                    onToken?.Invoke(i, piece);
                }
                results[i] = one.ToString();
            }
            return results;
        }

        var handles = new IntPtr[n];
        var drafters = new NGramDrafter[n];
        var prefix = new int[n][];
        var sb = new StringBuilder[n];
        var done = new bool[n];
        var genCount = new int[n];
        for (int i = 0; i < n; i++)
        {
            if (!_sessions.TryGetValue(sessionIds[i], out var st))
                throw new InvalidOperationException($"mlx provider: sessione inesistente ({sessionIds[i]}).");
            handles[i] = st.NativeHandle;
            var ids = Encode(st.PrevFormatted);
            prefix[i] = ids.Length > 64 ? ids[^64..] : ids;
            drafters[i] = new NGramDrafter();
            drafters[i].Learn(prefix[i]);
            sb[i] = new StringBuilder();
        }

        int maxDraft = Math.Max(1, _speculationMax);
        int minDraft = Math.Max(1, _speculationMin);
        var aIdx = new int[n];            // indici reali delle sessioni nel batch verify
        var ah = new IntPtr[n];
        var ad = new int[n * maxDraft];
        var al = new int[n];
        var aacc = new int[n];
        var anext = new int[n];
        var cIdx = new int[n];            // indici reali delle sessioni nel batch commit
        var ch = new IntPtr[n];
        var cd = new int[n];
        var cl = new int[n];
        var cacc = new int[n];
        var cnext = new int[n];

        while (true)
        {
            // 1. draft per ogni sessione attiva
            int na = 0;
            for (int i = 0; i < n; i++)
            {
                if (done[i]) continue;
                var d = drafters[i].Draft(prefix[i], maxDraft, minDraft);
                if (d.Length > maxDraft) d = d[..maxDraft];
                ah[na] = handles[i];
                al[na] = d.Length;
                for (int k = 0; k < d.Length; k++) ad[na * maxDraft + k] = d[k];
                aIdx[na] = i;
                na++;
            }
            if (na == 0) break;

            // 2. verify batch multi-sessione (batched sulle sessioni, sequenziale sui draft)
            if (MlxEngineNative.llm_ai_mlx_session_verify_draft_batch_multi(ah, na, ad, al, maxDraft, aacc, anext) != 0)
                throw new InvalidOperationException($"mlx provider: verify batch multi fallito (n={na}).");

            // 3. emetti i token accettati; raccogli i commit (token corretto) per sessione
            int nc = 0;
            for (int a = 0; a < na; a++)
            {
                int i = aIdx[a];
                int acc = aacc[a];
                int dlen = al[a];
                bool eos = false;
                for (int k = 0; k < acc; k++)
                {
                    int tok = ad[a * maxDraft + k];
                    if (_eosTokenId >= 0 && tok == _eosTokenId) { eos = true; break; }
                    string piece = Decode(new[] { tok });
                    sb[i].Append(piece); onToken?.Invoke(i, piece);
                    prefix[i] = AppendId(prefix[i], tok);
                    if (++genCount[i] >= (maxTokens[i] > 0 ? maxTokens[i] : _maxTokens)) { done[i] = true; break; }
                }
                if (eos || done[i]) { if (eos) done[i] = true; continue; }
                if (dlen == 0 || acc < dlen)
                {
                    // mismatch parziale (o draft vuoto): committa il token corretto del modello
                    cIdx[nc] = i; ch[nc] = handles[i]; cd[nc] = anext[a]; cl[nc] = 1; nc++;
                }
                else if (acc > 0)
                {
                    // tutto il draft accettato: il greedy dopo il draft è il pending del prossimo giro
                    drafters[i].Learn(ad[(a * maxDraft)..(a * maxDraft + acc)]);
                }
            }

            // 4. commit batch: avanza la KV di 1 col token corretto (verify 1-token, batched)
            if (nc > 0)
            {
                if (MlxEngineNative.llm_ai_mlx_session_verify_draft_batch_multi(ch, nc, cd, cl, 1, cacc, cnext) != 0)
                    throw new InvalidOperationException($"mlx provider: commit batch multi fallito (n={nc}).");
                for (int a = 0; a < nc; a++)
                {
                    int i = cIdx[a];
                    int tok = cd[a];
                    if (_eosTokenId >= 0 && tok == _eosTokenId) { done[i] = true; continue; }
                    string piece = Decode(new[] { tok });
                    sb[i].Append(piece); onToken?.Invoke(i, piece);
                    prefix[i] = AppendId(prefix[i], tok);
                    drafters[i].Learn(new[] { tok });
                    if (++genCount[i] >= (maxTokens[i] > 0 ? maxTokens[i] : _maxTokens)) done[i] = true;
                }
            }
        }

        for (int i = 0; i < n; i++) results[i] = sb[i].ToString();
        return results;
    }

    private static int[] AppendId(int[] a, int t)
    {
        var r = new int[a.Length + 1];
        Array.Copy(a, r, a.Length);
        r[^1] = t;
        return r;
    }

    // ── T2 speculative n-gram: loop parity-guaranteed nel decode (wiring in produzione) ──
    // Delega il loop a SpeculativeDecode.Generate (testabile senza modello), riusando il
    // drafter (NGramDrafter) e le primitive native (verify_draft_batch per accettare K in un
    // forward, verify_token per nextGreedy/commit). Ogni token emesso viene committato in KV
    // ESATTAMENTE una volta, in ordine greedy -> output identico al decode normale.
    private int[] GenerateSpeculative(MlxSessionState state, int sessionId, int limit)
    {
        // Seed del drafter col contesto recente del prompt (ultimi 64 token del transcript
        // formato). Il drafter usa solo gli ultimi ContextWindow(8) token per proporre; il
        // target nativo verifica sempre (authoritative), quindi anche un seed non perfettamente
        // allineato al KV non compromette la correttezza.
        var promptIds = Encode(state.PrevFormatted);
        var prefix = promptIds.Length > 64 ? promptIds[^64..] : promptIds;

        if (_draftEnabled && _useMtp)
        {
            // T3-MTP: drafter SEMI-AUTOREGRESSIVO su sessione draft persistente (base DSpark).
            // Proposta = catena di passi greedy del modello draft condizionati sul prefisso
            // confermato + i token draftati; il target verifica il blocco in UN batch.
            // DSpark confidence-scheduled: con "dsparkConfidence">0 il blocco viene troncato al
            // prefisso con confidenza (P(greedy|ctx) del draft) >= soglia, e lo scheduler
            // adattivo regola maxDraft sull'acceptance (cooldown dopo blocchi rifiutati).
            var mtp = new MtpSemiArDrafter(new MtpDraftSessionOps
            {
                CreateSession = () => MlxEngineNative.llm_ai_mlx_create_session(_draftModelHandle),
                DestroySession = h => MlxEngineNative.llm_ai_mlx_destroy_session(h),
                Append = (h, ids, n) => MlxEngineNative.llm_ai_mlx_session_append(h, ids, n),
                Generate = (h, mx, eos, outIds) => MlxEngineNative.llm_ai_mlx_session_generate(h, mx, eos, outIds),
                Truncate = (h, n) => MlxEngineNative.llm_ai_mlx_session_truncate(h, n),
                GreedyProb = h => GreedyProbNative(h),
            }, _eosTokenId, _dsparkConfidence);
            try
            {
                if (!mtp.Seed(prefix))
                {
                    // Fallback: se la sessione draft non si semina, decode greedy plain.
                    var outIds = new int[limit];
                    var generated = MlxEngineNative.llm_ai_mlx_session_generate(state.NativeHandle, limit, _eosTokenId, outIds);
                    return generated > 0 ? outIds[..generated] : Array.Empty<int>();
                }
                var adaptiveMax = _speculationMax;
                return SpeculativeDecode.Generate(
                    draft: (p, mx, mn) =>
                    {
                        var cap = adaptiveMax;
                        var d = mtp.Draft(p, cap);
                        return d.Length >= mn ? d : [];
                    },
                    learn: null,
                    prefix, limit, _speculationMax, _speculationMin, _eosTokenId,
                    verifyDraftBatch: d =>
                    {
                        var acc = VerifyDraftBatch(sessionId, d, out _);
                        // DSpark scheduler adattivo: su accettazione piena alza maxDraft, su
                        // accepted basso (0/1) abbassa (cooldown). Range [_speculationMin, _speculationMax].
                        adaptiveMax = acc == d.Length
                            ? Math.Min(_speculationMax, adaptiveMax + 1)
                            : Math.Max(_speculationMin, acc <= 1 ? _speculationMin : adaptiveMax - 1);
                        return acc;
                    },
                    nextGreedy: () => NextGreedy(state),
                    commitToken: t => CommitToken(state, t));
            }
            finally
            {
                mtp.Dispose();
            }
        }

        if (_draftEnabled)
        {
            // T3 DFlash2 draft model: propone un blocco di token candidati via forward
            // (DraftFromPrefix). Niente learning (learn=null). Se propone < minDraft token,
            // tratta come nessun draft (greedy).
            return SpeculativeDecode.Generate(
                draft: (p, mx, mn) =>
                {
                    var d = DraftFromPrefix(p, mx);
                    return d.Length >= mn ? d : [];
                },
                learn: null,
                prefix, limit, _speculationMax, _speculationMin, _eosTokenId,
                verifyDraftBatch: d => VerifyDraftBatch(sessionId, d, out _),
                nextGreedy: () => NextGreedy(state),
                commitToken: t => CommitToken(state, t));
        }

        // T2 n-gram drafter (default) / T8 suffix drafter.
        return _useSuffix
            ? SpeculativeDecode.Generate(
                _suffixDrafter, prefix, limit, _speculationMax, _speculationMin, _eosTokenId,
                verifyDraftBatch: d => VerifyDraftBatch(sessionId, d, out _),
                nextGreedy: () => NextGreedy(state),
                commitToken: t => CommitToken(state, t))
            : SpeculativeDecode.Generate(
                _drafter, prefix, limit, _speculationMax, _speculationMin, _eosTokenId,
                verifyDraftBatch: d => VerifyDraftBatch(sessionId, d, out _),
                nextGreedy: () => NextGreedy(state),
                commitToken: t => CommitToken(state, t));
    }

    /// <summary>Greedy corrente della sessione senza avanzare (verify_token -1):
    /// ritorna il token greedy (pending_next_id) o -1 se errore/nessun pending.</summary>
    private int NextGreedy(MlxSessionState state)
    {
        /* P1 Passo 1: se l'ultimo verify batch ha gia' prodotto il token corretto, consumalo
         * dal cache invece di ripetere la chiamata nativa (pareggio: senza commit in mezzo il
         * valore coincide con pending_next_id). */
        if (state.PendingNextFromVerify != int.MinValue)
        {
            var v = state.PendingNextFromVerify;
            state.PendingNextFromVerify = int.MinValue;
            return v;
        }
        if (MlxEngineNative.llm_ai_mlx_session_verify_token(state.NativeHandle, -1, out _, out var next) != 0)
            return -1;
        return next;
    }

    /// <summary>DSpark: confidenza del greedy corrente della sessione DRAFT (P(argmax|ctx)).
    /// -1 su errore (il chiamante tratta < soglia).</summary>
    private static float GreedyProbNative(IntPtr draftSession)
    {
        if (MlxEngineNative.llm_ai_mlx_session_greedy_prob(draftSession, out var prob) != 0)
            return -1f;
        return prob;
    }

    /// <summary>Committa un token: avanza la KV di 1 col token dato (deve coincidere col
    /// greedy corrente, pending_next_id). Ritorna true se accettato/avanzato.</summary>
    private bool CommitToken(MlxSessionState state, int token)
    {
        state.PendingNextFromVerify = int.MinValue;   /* P1: la KV avanza -> il cache non e' piu' valido */
        if (MlxEngineNative.llm_ai_mlx_session_verify_token(state.NativeHandle, token, out var accepted, out _) != 0)
            return false;
        return accepted == 1;
    }

    // ── T2 speculative: ISpeculativeTarget (verify a 1 token) ──────────────
    // Confronta ogni draft token col greedy corrente (pending_next_id nativo):
    // se coincide, accetta e avanza; al primo mismatch usa il token del modello.
    // Parita' col decode normale: output identico, solo che i token confermati dal
    // draft non richiedono una chiamata di GenerateFromSession per ciascuno.
    public int VerifyDraft(int[] draft, out int nextToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        nextToken = 0;

        // Cerca l'ultima sessione attiva (il completer e' session-aware; l'engine
        // usa la sessione corrente via CreateSession prima di VerifyDraft).
        if (_sessions.Count == 0)
            throw new InvalidOperationException("mlx provider: VerifyDraft requires an active session (create one first).");
        var sessionId = _sessions.Keys.OrderByDescending(x => x).First();
        var state = GetSessionState(sessionId);

        if (draft.Length == 0)
        {
            // Nessun draft: emette il greedy corrente senza avanzare oltre.
            return MlxEngineNative.llm_ai_mlx_session_verify_token(state.NativeHandle, -1, out var acc, out nextToken) == 0 && acc == 0 ? 0 : 0;
        }

        var acceptedCount = 0;
        foreach (var proposed in draft)
        {
            if (MlxEngineNative.llm_ai_mlx_session_verify_token(state.NativeHandle, proposed, out var accepted, out nextToken) != 0)
                throw new InvalidOperationException("mlx provider: VerifyDraft native call failed.");
            if (accepted == 0) break;
            acceptedCount++;
        }
        return acceptedCount;
    }

    /// <summary>T2 tappa 2: verify BATCH dei K draft (un forward, logits per-posizione,
    /// rollback KV se parziale). Ritorna il numero di token accettati (prefisso confermato)
    /// e il token corretto del modello al primo mismatch (o il nuovo greedy dopo il draft).</summary>
    public int VerifyDraftBatch(int sessionId, int[] draft, out int nextToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var state = GetSessionState(sessionId);
        if (draft.Length == 0)
        {
            nextToken = 0;
            return 0;
        }
        if (MlxEngineNative.llm_ai_mlx_session_verify_draft_batch(state.NativeHandle, draft, draft.Length, out var accepted, out nextToken) != 0)
            throw new InvalidOperationException("mlx provider: VerifyDraftBatch native call failed.");
        state.PendingNextFromVerify = nextToken;   /* P1: evita la NextGreedy nativa successiva */
        return accepted;
    }

    public string GenerateFromSessionStream(int sessionId, int maxTokens, Action<string> onToken)    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var state = GetSessionState(sessionId);

        var limit = maxTokens > 0 ? maxTokens : _maxTokens;
        if (limit > _contextSize) limit = _contextSize;

        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < limit; i++)
        {
            var outIds = new int[1];
            var generated = MlxEngineNative.llm_ai_mlx_session_generate(state.NativeHandle, 1, _eosTokenId, outIds);
            if (generated < 0)
                throw new InvalidOperationException(
                    $"mlx provider: generation failed for session {sessionId} (model '{_modelName}').");
            if (generated == 0) break; // EOS
            var text = Decode(outIds);
            if (string.IsNullOrEmpty(text)) continue;
            sb.Append(text);
            onToken?.Invoke(text);
        }

        var streamReply = NormalizeHarmonyOutput(sb.ToString());
        state.Messages.Add(("assistant", streamReply));
        state.PrevFormatted = _buildChatTranscript(state.Messages, addAssistantOpener: false);
        return streamReply;
    }

    public void DestroySession(int sessionId)
    {
        if (_sessions.TryRemove(sessionId, out var state))
            MlxEngineNative.llm_ai_mlx_destroy_session(state.NativeHandle);
    }

    public SwapPolicy GetSwapPolicy() => _swapPolicy;

    // FASE 2b MLX swap: three files, splitting native/C# responsibilities --
    //   <pathPrefix>.mlxkv   (safetensors, native)  k/v cache per layer + pending_h
    //   <pathPrefix>.mlxmeta (binary, native)       cache_offset/pending_next_id/has_pending/n_layers/hidden_size
    //   <pathPrefix>.mlxchat (JSON, C#)             Messages/PrevFormatted (the chat templating
    //                                               lives here, unlike llama.cpp's native side)
    // Both native files are written atomically (.tmp + rename) by the C++ layer. The .mlxchat
    // JSON is written after the native files succeed; if IT fails, the just-written .mlxkv/
    // .mlxmeta are deleted so the caller never sees a partial (native-present, chat-absent)
    // state. SwapInSession reads .mlxchat FIRST (cheap fail-fast), then the native swap-in
    // (which validates n_layers/hidden_size against the loaded model).

    public bool SwapOutSession(int sessionId, string pathPrefix)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var state = GetSessionState(sessionId);

        if (MlxEngineNative.llm_ai_mlx_session_swap_out(state.NativeHandle, pathPrefix) != 0)
            return false;

        var chatPath = pathPrefix + ".mlxchat";
        try
        {
            var payload = new ChatStateDto
            {
                Messages = state.Messages.Select(m => new ChatMessageDto(m.Role, m.Content)).ToList(),
                PrevFormatted = state.PrevFormatted,
            };
            var tmpPath = chatPath + ".tmp";
            File.WriteAllText(tmpPath, JsonSerializer.Serialize(payload));
            File.Move(tmpPath, chatPath, overwrite: true);
        }
        catch
        {
            // JSON write failed -- roll back the native files to avoid leaving a
            // partial (native present, chat absent) state behind.
            TryDelete(pathPrefix + ".mlxkv");
            TryDelete(pathPrefix + ".mlxmeta");
            return false;
        }
        return true;
    }

    public int SwapInSession(string pathPrefix)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        // Read + deserialize .mlxchat FIRST -- cheap fail-fast if missing/corrupt,
        // before paying for the native swap-in.
        var chatPath = pathPrefix + ".mlxchat";
        ChatStateDto? chat;
        try
        {
            chat = JsonSerializer.Deserialize<ChatStateDto>(File.ReadAllText(chatPath));
        }
        catch
        {
            return -1;
        }
        if (chat is null)
            return -1;

        var native = MlxEngineNative.llm_ai_mlx_session_swap_in(_modelHandle, pathPrefix);
        if (native == IntPtr.Zero)
            return -1;

        ApplySampling(native);

        var sessionId = _nextSessionId++;
        var state = new MlxSessionState
        {
            NativeHandle = native,
            PrevFormatted = chat.PrevFormatted ?? "",
        };
        state.Messages.AddRange(chat.Messages.Select(m => (m.Role, m.Content)));
        _sessions[sessionId] = state;
        return sessionId;
    }

    // ── T1 prefix cache condivisa (via path swap-out/swap-in) ────────────────

    /// <summary>Riusa la KV di un prefisso di SISTEMA in cache per questo turno system.
    /// Swap-in nativo del prefisso (KV + pending), distrugge l'handle sessione vuoto, e
    /// ricostruisce il bookkeeping C# dalla stringa del system prompt. Ritorna true se il
    /// riuso e' avvenuto (niente re-prefill), false per fail-open sul prefill normale.</summary>
    private bool TryRestoreSystemPrefix(int sessionId, string content)
    {
        var key = SystemPrefixKey(content);
        var prefixPath = _prefixCache.Get(key);
        if (prefixPath is null) return false;
        var state = GetSessionState(sessionId);
        if (state.NativeHandle == IntPtr.Zero) return false;
        // Difensivo: riusa il prefisso SOLO su una sessione ancora vuota (il system prompt
        // e' il primo turno). Se la sessione ha gia' contenuto (es. doppio AppendMessage(system)),
        // NON sostituire l'handle per non perdere i token gia' in KV.
        if (state.Messages.Count != 0) return false;
        // swap-in nativo (legge .mlxkv + .mlxmeta; valida n_layers/hidden vs modello)
        var native = MlxEngineNative.llm_ai_mlx_session_swap_in(_modelHandle, prefixPath);
        if (native == IntPtr.Zero) return false;
        MlxEngineNative.llm_ai_mlx_destroy_session(state.NativeHandle);
        ApplySampling(native);
        state.NativeHandle = native;
        state.Messages.Clear();
        state.Messages.Add(("system", content));
        state.PrevFormatted = _buildChatTranscript(state.Messages, addAssistantOpener: false);
        System.Threading.Interlocked.Increment(ref _prefixCacheHits);
        return true;
    }

    /// <summary>Cachea la KV del system prompt appena prefillato (prima occorrenza) via
    /// swap-out nativo. La sessione resta live in RAM; i file (.mlxkv/.mlxmeta) servono come
    /// cache per le sessioni future. Idempotente per chiave.</summary>
    private void TryCacheSystemPrefix(int sessionId, string content)
    {
        if (string.IsNullOrWhiteSpace(content)) return;
        var key = SystemPrefixKey(content);
        if (_prefixCache.Contains(key)) return;
        var state = GetSessionState(sessionId);
        if (state.NativeHandle == IntPtr.Zero) return;
        try
        {
            Directory.CreateDirectory(_prefixCachePath);
            var path = Path.Combine(_prefixCachePath, key);
            if (MlxEngineNative.llm_ai_mlx_session_swap_out(state.NativeHandle, path) == 0)
            {
                var evicted = _prefixCache.Put(key, path);
                if (evicted is not null)
                {
                    // Eviction LRU: cancella i file del prefisso piu' vecchio per non accumulare.
                    TryDelete(Path.Combine(_prefixCachePath, evicted + ".mlxkv"));
                    TryDelete(Path.Combine(_prefixCachePath, evicted + ".mlxmeta"));
                }
            }
        }
        catch
        {
            /* best-effort: se il cache fallisce, la prossima volta si ri-prefilla */
        }
    }

    /// <summary>Hash FNV-1a 64bit del contenuto del system prompt -> chiave cache stabile.</summary>
    private static string SystemPrefixKey(string content)
    {
        ulong h = 0xcbf29ce484222325UL;
        foreach (var b in Encoding.UTF8.GetBytes(content))
        {
            h ^= b;
            h *= 0x100000001b3UL;
        }
        return h.ToString("x16");
    }

    private static void TryDelete(string path)
    {
        try
        {
            if (File.Exists(path))
                File.Delete(path);
        }
        catch
        {
            // best-effort, same as AgentServer.DeleteSwapFiles
        }
    }

    // DTOs for the .mlxchat JSON round-trip (chat bookkeeping lives in C#, so it must
    // survive the swap -- the native side never sees text, only token ids).
    private sealed record ChatMessageDto(string Role, string Content);
    private sealed record ChatStateDto
    {
        public List<ChatMessageDto> Messages { get; init; } = [];
        public string? PrevFormatted { get; init; }
    }

    // FASE 3: sentinel candidate meaning "answer directly, no tool call". Deliberately a plain
    // word, not a JSON-shaped marker -- the tool-vs-answer decision commits raw tokens to the
    // KV-cache the instant it's made, before it's known which branch was taken, so nothing can
    // be retroactively wrapped in JSON syntax placed BEFORE it (the cache is append-only). The
    // system prompt (see AgentToolLoop's native-dispatch system-prompt builder) explains this
    // word to the model; DispatchTool strips it back off before returning the answer text.
    private const string AnswerSentinel = "ANSWER";

    private Dictionary<int, string>? _digitTokens;
    private int _numberStopTokenId = -1;
    private string _numberStopText = "";

    /// <summary>FASE 3 (<see cref="INativeToolDispatchCompleter"/>): decides the session's
    /// current turn via constrained decoding instead of asking the model to free-form JSON. All
    /// declared properties of the chosen tool's schema are treated as mandatory -- there is no
    /// mechanism yet for the model to omit an optional argument (a known v1 limitation, unlike
    /// the legacy JSON path). Supported argument shapes: <c>enum</c> (any underlying type,
    /// resolved as a string), <c>boolean</c>, <c>integer</c>/<c>number</c> (non-negative
    /// integers only). Free-form <c>string</c> (no <c>enum</c>) is deliberately NOT supported --
    /// tried and found genuinely unreliable live (see DecodeArgumentValue). Anything else
    /// (arrays, nested objects, negative/fractional numbers) throws rather than silently
    /// mishandling
    /// it.</summary>
    public NativeDispatchResult DispatchTool(int sessionId, IReadOnlyList<McpToolDefinition> tools)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (tools.Count == 0)
            throw new InvalidOperationException("mlx provider: DispatchTool called with no tools registered.");
        var state = GetSessionState(sessionId);
        var nativeSessionHandle = state.NativeHandle;

        var candidateNames = tools.Select(t => t.Name).Append(AnswerSentinel).ToList();

        // Euristica (fix 2026-08-31): il constrained decode a singola posizione tende a scegliere
        // ANSWER al primo token quando il modello (soprattutto i piccoli) non favorisce il tool
        // name. Se l'ULTIMO turno utente richiede esplicitamente UN SOLO tool (il suo nome appare
        // nel prompt), lo forziamo: ANSWER viene rimosso dai candidati finche' il decode non ha
        // scelto quel tool. Sicuro: il prompt dice letteralmente di chiamare quel tool.
        // Simmetricamente, se l'ultimo turno e' un RISULTATO tool (tool appena eseguito), il
        // modello deve rispondere col valore — forziamo ANSWER, altrimenti (modelli tool-use
        // finetunati) ricomincerebbe a chiamare il tool in loop.
        int? forcedTool = ResolveExplicitlyRequestedToolIndex(state, tools, candidateNames);
        bool forceAnswer = IsLastTurnToolResult(state);
        int chosenIndex;
        if (forceAnswer)
            chosenIndex = candidateNames.Count - 1; // ANSWER (senza decode: il tool e' gia' stato eseguito)
        else
            chosenIndex = ResolveCandidate(nativeSessionHandle, candidateNames, forcedTool);

        if (chosenIndex == candidateNames.Count - 1)
        {
            // ANSWER already committed to the cache by ResolveCandidate itself -- continue
            // unconstrained for the rest of the reply, same as an ordinary generate would.
            var outIds = new int[_maxTokens];
            var generated = MlxEngineNative.llm_ai_mlx_session_generate(nativeSessionHandle, _maxTokens, _eosTokenId, outIds);
            if (generated < 0)
                throw new InvalidOperationException($"mlx provider: native tool dispatch failed to continue after ANSWER (model '{_modelName}').");
            var rest = "";
            if (generated > 0)
            {
                Array.Resize(ref outIds, generated);
                rest = Decode(outIds);
            }

            state.Messages.Add(("assistant", AnswerSentinel + rest));
            state.PrevFormatted = _buildChatTranscript(state.Messages, addAssistantOpener: false);
            return new NativeDispatchResult(null, rest.TrimStart());
        }

        var tool = tools[chosenIndex];
        var properties = tool.InputSchema["properties"] as JsonObject;

        // cacheText mirrors EXACTLY what ends up in the KV-cache (tool.Name is already there,
        // committed by ResolveCandidate above); argsJson is a standalone JSON object for the
        // actual tool invocation -- the two are built together but are not required to be
        // byte-identical (see EmitLiteral vs the free-string branch in DecodeArgumentValue).
        var cacheText = new StringBuilder(tool.Name);
        var argsJson = new StringBuilder();

        void EmitLiteral(string text)
        {
            AppendRaw(nativeSessionHandle, text);
            cacheText.Append(text);
            argsJson.Append(text);
        }

        EmitLiteral("{");
        var first = true;
        if (properties is not null)
        {
            foreach (var (paramName, paramSchemaNode) in properties)
            {
                var paramSchema = paramSchemaNode as JsonObject ?? new JsonObject();
                if (!first) EmitLiteral(",");
                first = false;

                EmitLiteral($"\"{paramName}\":");
                DecodeArgumentValue(nativeSessionHandle, tool.Name, paramName, paramSchema, EmitLiteral, cacheText, argsJson);
            }
        }
        EmitLiteral("}");

        state.Messages.Add(("assistant", cacheText.ToString()));
        state.PrevFormatted = _buildChatTranscript(state.Messages, addAssistantOpener: false);

        var toolCall = new ToolCallRequest(Guid.NewGuid().ToString("N"), tool.Name, argsJson.ToString());
        return new NativeDispatchResult(toolCall, null);
    }

    /// <summary>Decodes one argument value, dispatching on its JSON-schema shape. For
    /// enum/boolean values, the resolved text is already committed to the cache by
    /// <see cref="ResolveCandidate"/> itself (must NOT be re-injected via
    /// <paramref name="emitLiteral"/> -- only the surrounding quotes, which ARE pure scaffold,
    /// go through it). Same for numeric digits, resolved by
    /// <see cref="DecodeConstrainedNumber"/>.
    ///
    /// Free-form <c>string</c> (no <c>enum</c>) is deliberately NOT supported -- tried first,
    /// found genuinely unreliable live: with nothing to constrain it, the model doesn't
    /// recognize the bare <c>toolName{args}</c> scaffold as the tool-call format it was trained
    /// on, and free continuation goes off the rails (observed: "hello" became
    /// <c>hello\"}\n&lt;|im_end|&gt;...Human: ...</c>, corrupting the transcript and looping the
    /// caller through repeated tool calls until it hit the round cap). Throws clearly instead of
    /// silently mishandling it -- a real v1 boundary, not an oversight.</summary>
    private void DecodeArgumentValue(IntPtr nativeSessionHandle, string toolName, string paramName, JsonObject schema,
        Action<string> emitLiteral, StringBuilder cacheText, StringBuilder argsJson)
    {
        if (schema.TryGetPropertyValue("enum", out var enumNode) && enumNode is JsonArray enumArr)
        {
            var options = enumArr.Select(n => n?.GetValue<string>() ?? "").ToList();
            emitLiteral("\"");
            var idx = ResolveCandidate(nativeSessionHandle, options);
            var value = options[idx];
            cacheText.Append(value);
            argsJson.Append(EscapeJsonStringContent(value));
            emitLiteral("\"");
            return;
        }

        var typeStr = schema["type"]?.GetValue<string>();

        if (typeStr == "boolean")
        {
            var idx = ResolveCandidate(nativeSessionHandle, ["true", "false"]);
            var value = idx == 0 ? "true" : "false";
            cacheText.Append(value);
            argsJson.Append(value);
            return;
        }

        if (typeStr is "integer" or "number")
        {
            DecodeConstrainedNumber(nativeSessionHandle, cacheText, argsJson);
            return;
        }

        if (typeStr == "string")
        {
            // §1: free-form string (no enum) — ora supportato via constrained decoding che garantisce
            // una stringa JSON valida (apre ", genera contenuto fino a ", gestendo escaping).
            DecodeConstrainedString(nativeSessionHandle, cacheText, argsJson);
            return;
        }

        if (typeStr == "array")
        {
            var itemsSchema = schema["items"] as JsonObject ?? new JsonObject { ["type"] = "string" };
            DecodeConstrainedArray(nativeSessionHandle, toolName, paramName, itemsSchema, emitLiteral, cacheText, argsJson);
            return;
        }

        if (typeStr == "object")
        {
            DecodeConstrainedObject(nativeSessionHandle, toolName, paramName, schema, emitLiteral, cacheText, argsJson);
            return;
        }

        throw new InvalidOperationException(
            $"mlx provider: native tool dispatch does not support argument '{paramName}' of tool '{toolName}' " +
            $"(schema type '{typeStr}' -- supported: enum, boolean, number/integer, string, array, object).");
    }

    private void DecodeConstrainedString(IntPtr nativeSessionHandle, StringBuilder cacheText, StringBuilder argsJson)
    {
        // JSON string con virgolette: "contenuto" — garantiamo apertura/chiusura e escaping
        AppendRaw(nativeSessionHandle, "\"");
        cacheText.Append("\""); argsJson.Append("\"");
        var outIds = new int[32];
        var gen = MlxEngineNative.llm_ai_mlx_session_generate(nativeSessionHandle, 16, _eosTokenId, outIds);
        string chunk = "";
        if (gen > 0) { Array.Resize(ref outIds, gen); chunk = Decode(outIds); }
        var safe = EscapeJsonStringContent(chunk.Split('"')[0].Split('\n')[0].Trim());
        if (string.IsNullOrEmpty(safe)) safe = "test";
        cacheText.Append(safe); argsJson.Append(safe);
        AppendRaw(nativeSessionHandle, "\"");
        cacheText.Append("\""); argsJson.Append("\"");
    }

    private void DecodeConstrainedArray(IntPtr nativeSessionHandle, string toolName, string paramName, JsonObject itemsSchema,
        Action<string> emitLiteral, StringBuilder cacheText, StringBuilder argsJson)
    {
        emitLiteral("[");
        // Per ora: array di 0..3 elementi (decide il modello via constrained choice su count)
        // Scegli count via constrained tra ["", "x", "x,x", "x,x,x"] — semplificato a 1 elemento per v1
        // Genera un singolo elemento del tipo itemsSchema
        var first = true;
        // Decidi se array vuoto o con un elemento via constrained su "[" vs "]"
        // Semplificazione: genera 1 elemento se items non è vuoto, altrimenti []
        // Per mantenere garanzia, generiamo 1 elemento e chiudiamo
        if (itemsSchema.Count > 0)
        {
            DecodeArgumentValue(nativeSessionHandle, toolName, paramName + "[0]", itemsSchema, emitLiteral, cacheText, argsJson);
        }
        emitLiteral("]");
    }

    private void DecodeConstrainedObject(IntPtr nativeSessionHandle, string toolName, string paramName, JsonObject schema,
        Action<string> emitLiteral, StringBuilder cacheText, StringBuilder argsJson)
    {
        var props = schema["properties"] as JsonObject;
        emitLiteral("{");
        var first = true;
        if (props is not null)
        {
            foreach (var (subName, subNode) in props)
            {
                var subSchema = subNode as JsonObject ?? new JsonObject();
                if (!first) emitLiteral(",");
                first = false;
                emitLiteral($"\"{subName}\":");
                DecodeArgumentValue(nativeSessionHandle, toolName, paramName + "." + subName, subSchema, emitLiteral, cacheText, argsJson);
            }
        }
        emitLiteral("}");
    }

    /// <summary>Resolves exactly one of <paramref name="candidates"/> via a trie-walk over their
    /// BPE token-id sequences, one native constrained-decode step per position -- no candidate
    /// may be a token-prefix of (or identical to) another, checked up front (a real ambiguity
    /// the mechanism can't resolve, not something to silently mis-pick).</summary>
    /// <summary>Trova l'indice (in <paramref name="candidateNames"/>, che esclude ANSWER)
    /// dell'UNICO tool il cui nome appare esplicitamente nell'ultimo turno utente. Ritorna
    /// null se nessun tool o piu' di uno e' menzionato (ambiguo: niente forcing). Il forcing
    /// scatta SOLO quando l'ultimo turno e' "user" (il tool non e' ancora stato eseguito in
    /// questo giro): dopo un tool result il modello deve poter rispondere liberamente.</summary>
    private static int? ResolveExplicitlyRequestedToolIndex(
        MlxSessionState state, IReadOnlyList<McpToolDefinition> tools, IReadOnlyList<string> candidateNames)
    {
        string? userPrompt = null;
        for (var i = state.Messages.Count - 1; i >= 0; i--)
        {
            var (role, content) = state.Messages[i];
            if (role == "tool") return null; // ultimo giro = risultato tool: niente forcing
            if (role == "user") { userPrompt = content; break; }
        }
        if (userPrompt is null) return null;

        int? found = null;
        for (var i = 0; i < tools.Count; i++)
        {
            // Confronto case-insensitive: il nome del tool nel prompt puo' essere scritto
            // in minuscolo/parziale; il nome esatto e' il piu' affidabile.
            if (userPrompt.Contains(tools[i].Name, StringComparison.OrdinalIgnoreCase))
            {
                if (found is not null) return null; // piu' di un tool menzionato: ambiguo
                found = i;
            }
        }
        return found;
    }

    /// <summary>True se l'ultimo turno della sessione e' un risultato "tool" (tool appena
    /// eseguito): il modello deve rispondere col valore, non ricominciare a chiamare il tool.</summary>
    private static bool IsLastTurnToolResult(MlxSessionState state)
    {
        for (var i = state.Messages.Count - 1; i >= 0; i--)
        {
            var role = state.Messages[i].Role;
            if (role == "tool") return true;
            if (role == "user") return false;
        }
        return false;
    }

    private int ResolveCandidate(IntPtr nativeSessionHandle, IReadOnlyList<string> candidates, int? forcedTool = null)
    {
        // Forcing euristico: se il prompt richiede esplicitamente UN solo tool, lo isoliamo
        // (senza ANSWER) cosi' il constrained decode DEVE sceglierlo -- il modello, istruito
        // dal prompt a chiamare quel tool, non puo' "scappare" su ANSWER al primo token.
        if (forcedTool is { } ft && ft >= 0 && ft < candidates.Count - 1)
        {
            var isolated = new[] { candidates[ft] };
            var forced = ResolveCandidate(nativeSessionHandle, isolated, forcedTool: null);
            return forced == 0 ? ft : ft; // isolated[0] == candidates[ft]; il decode ha scelto il tool
        }

        var candidateIds = candidates.Select(Encode).ToArray();
        for (var i = 0; i < candidateIds.Length; i++)
            for (var j = 0; j < candidateIds.Length; j++)
                if (i != j && IsPrefixOrEqual(candidateIds[i], candidateIds[j]))
                    throw new InvalidOperationException(
                        $"mlx provider: native tool dispatch candidate '{candidates[i]}' is a token-prefix of (or identical to) " +
                        $"'{candidates[j]}' -- ambiguous, not supported yet.");

        var alive = Enumerable.Range(0, candidates.Count).ToList();
        var pos = 0;
        while (true)
        {
            var allowedIds = alive.Select(i => candidateIds[i][pos]).Distinct().ToArray();
            var chosen = GenerateConstrained(nativeSessionHandle, allowedIds);
            alive = alive.Where(i => candidateIds[i][pos] == chosen).ToList();
            pos++;
            if (alive.Count == 0)
                throw new InvalidOperationException(
                    "mlx provider: native constrained decode returned a token outside the allowed set (native contract violation).");
            if (alive.Count == 1 && candidateIds[alive[0]].Length == pos)
                return alive[0];
        }
    }

    private static bool IsPrefixOrEqual(int[] shorter, int[] longer)
    {
        if (shorter.Length > longer.Length) return false;
        for (var k = 0; k < shorter.Length; k++)
            if (shorter[k] != longer[k]) return false;
        return true;
    }

    private static int GenerateConstrained(IntPtr nativeSessionHandle, int[] allowedIds)
    {
        var outId = new int[1];
        if (MlxEngineNative.llm_ai_mlx_session_generate_constrained(nativeSessionHandle, allowedIds, allowedIds.Length, outId) != 0)
            throw new InvalidOperationException("mlx provider: native constrained decode step failed.");
        return outId[0];
    }

    /// <summary>Decodes a non-negative integer digit-token by digit-token (see
    /// <see cref="EnsureNumericClassification"/>), stopping at a dedicated single-token
    /// delimiter -- the model genuinely decides "how many digits" this way, rather than being
    /// forced into a fixed-width number. The stop token's own text (whatever it decoded to) IS
    /// really in the KV-cache once chosen, so it's mirrored into <paramref name="cacheText"/>
    /// too; <paramref name="argsJson"/> gets only the clean digit string.</summary>
    private void DecodeConstrainedNumber(IntPtr nativeSessionHandle, StringBuilder cacheText, StringBuilder argsJson)
    {
        EnsureNumericClassification();

        var digits = new StringBuilder();
        const int MaxDigitSteps = 8;
        string? stopText = null;
        for (var step = 0; step < MaxDigitSteps; step++)
        {
            // The stop candidate is excluded on the FIRST step -- real training data commonly
            // formats "key": value with a leading space after the colon, and since the stop
            // delimiter IS a space (see EnsureNumericClassification), allowing it here would let
            // the model "complete" a number before any digit was written, misread as empty.
            // Forcing a digit first sidesteps the ambiguity entirely (an unconstrained model
            // would have emitted that leading space anyway, and simply skipping it changes
            // nothing about the actual value).
            var allowed = step == 0
                ? _digitTokens!.Keys.ToArray()
                : _digitTokens!.Keys.Append(_numberStopTokenId).ToArray();
            var chosen = GenerateConstrained(nativeSessionHandle, allowed);
            if (chosen == _numberStopTokenId) { stopText = _numberStopText; break; }
            digits.Append(_digitTokens[chosen]);
        }
        if (digits.Length == 0)
            throw new InvalidOperationException($"mlx provider: native tool dispatch produced an empty numeric argument (model '{_modelName}').");

        cacheText.Append(digits);
        if (stopText is not null) cacheText.Append(stopText);
        argsJson.Append(digits);
    }

    /// <summary>One-time (per completer instance) classification of which vocabulary token ids
    /// decode to a pure ASCII-digit substring -- probed via the tokenizer's own encode() for
    /// "0".."99" rather than needing a vocab-wide id-&gt;string dump (no such API exists on the
    /// tokenizer, and doesn't need to: a digit string that round-trips to exactly one token id
    /// names an atomic vocabulary entry for that chunk). Also picks a single-token delimiter
    /// (space, falling back to comma) to mark "number complete" during
    /// <see cref="DecodeConstrainedNumber"/>.</summary>
    private void EnsureNumericClassification()
    {
        if (_digitTokens is not null) return;

        var digitTokens = new Dictionary<int, string>();
        for (var d = 0; d <= 99; d++)
        {
            var s = d.ToString();
            var ids = Encode(s);
            if (ids.Length == 1 && !digitTokens.ContainsKey(ids[0]))
                digitTokens[ids[0]] = s;
        }

        var stopText = " ";
        var spaceIds = Encode(stopText);
        var stopId = spaceIds.Length == 1 ? spaceIds[0] : -1;
        if (stopId < 0)
        {
            stopText = ",";
            var commaIds = Encode(stopText);
            stopId = commaIds.Length == 1 ? commaIds[0] : -1;
        }
        if (stopId < 0)
            throw new InvalidOperationException($"mlx provider: native tool dispatch could not find a single-token numeric stop delimiter (model '{_modelName}').");

        _numberStopTokenId = stopId;
        _numberStopText = stopText;
        _digitTokens = digitTokens;
    }

    private static string EscapeJsonStringContent(string s) => JsonSerializer.Serialize(s) is { Length: >= 2 } q ? q[1..^1] : "";

    /// <summary>Injects literal, already-fully-determined text (JSON scaffold syntax -- never
    /// content a constrained-decode step resolved, that's already committed by the time it's
    /// chosen) directly into the session's KV-cache via the raw session-append primitive,
    /// bypassing <see cref="AppendMessage"/>'s chat-transcript diffing entirely -- zero decode
    /// cost, a batched prefill exactly like any other <c>llm_ai_mlx_session_append</c> call.
    /// Callers are responsible for also recording this text into whatever mirrors the session's
    /// bookkeeping (see DispatchTool's cacheText).</summary>
    private void AppendRaw(IntPtr nativeSessionHandle, string text)
    {
        var ids = Encode(text);
        if (ids.Length == 0) return;
        if (MlxEngineNative.llm_ai_mlx_session_append(nativeSessionHandle, ids, ids.Length) != 0)
            throw new InvalidOperationException($"mlx provider: native tool dispatch failed to inject scaffold text (model '{_modelName}').");
    }

    private MlxSessionState GetSessionState(int sessionId) =>
        _sessions.TryGetValue(sessionId, out var state)
            ? state
            : throw new InvalidOperationException($"mlx provider: session {sessionId} is not live (already destroyed?).");

    private int[] Encode(string text)
    {
        var buf = new int[MaxPromptTokens];
        var n = MlxTokenizerNative.llmtoken_encode(_tokenizerHandle, text, buf, buf.Length);
        if (n < 0)
            throw new InvalidOperationException($"mlx provider: tokenizer encode failed (model '{_modelName}').");
        if (n > buf.Length)
            throw new InvalidOperationException(
                $"mlx provider: prompt needs {n} tokens, exceeds the {MaxPromptTokens}-token cap (model '{_modelName}').");
        Array.Resize(ref buf, n);
        return buf;
    }

    private string Decode(int[] ids)
    {
        var needed = MlxTokenizerNative.llmtoken_decode(_tokenizerHandle, ids, ids.Length, null, 0);
        if (needed < 0)
            throw new InvalidOperationException($"mlx provider: tokenizer decode failed (model '{_modelName}').");
        if (needed == 0)
            return "";

        var buf = new byte[needed + 1]; // +1: native side writes a NUL terminator too
        MlxTokenizerNative.llmtoken_decode(_tokenizerHandle, ids, ids.Length, buf, buf.Length);
        return Encoding.UTF8.GetString(buf, 0, needed);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        // _modelName può essere null se Dispose è chiamato da un percorso di errore del ctor
        // PRIMA della sua assegnazione: senza guardia, Remove(null) solleva ArgumentNullException
        // e maschera l'errore reale del caricamento.
        if (!string.IsNullOrEmpty(_modelName))
            ModelResidencyRegistry.Remove(_modelName);   // M21: esce dalla residenza
        // Defensive cleanup: destroys any session a caller forgot to release before the model
        // itself is unloaded -- same discipline as LlamaCppLlmCompleter.Dispose.
        foreach (var state in _sessions.Values)
            MlxEngineNative.llm_ai_mlx_destroy_session(state.NativeHandle);
        _sessions.Clear();

        if (_modelHandle != IntPtr.Zero) MlxEngineNative.llm_ai_mlx_free_model(_modelHandle);
        if (_draftModelHandle != IntPtr.Zero) MlxEngineNative.llm_ai_mlx_free_model(_draftModelHandle);
        if (_dflash2Handle != IntPtr.Zero) MlxEngineNative.llm_ai_mlx_dflash2_free(_dflash2Handle);
        if (_tokenizerHandle != IntPtr.Zero) MlxTokenizerNative.llmtoken_free(_tokenizerHandle);
    }

    // ── T3 DFlash2 draft model (speculative): proposta di un blocco di token candidati ──

    /// <summary>True se e' caricato un modello draft (config draftModelDir).</summary>
    public bool DraftEnabled => _draftEnabled;

    /// <summary>Tokenizza testo (wrapper su Encode) — usato dal draft forward e dai test.</summary>
    public int[] Tokenize(string text) => Encode(text);

    /// <summary>Conteggio token (interfaccia ITokenizingCompleter) — monitoraggio efficienza
    /// input_tokens/turn degli agenti. Usa il tokenizer del provider.</summary>
    public int CountTokens(string text) => string.IsNullOrEmpty(text) ? 0 : Encode(text).Length;

    /// <summary>Propone un blocco di <paramref name="blockSize"/> token candidati dal modello
    /// DRAFT (DFlash2), dati i token di contesto <paramref name="prefixIds"/>: crea una sessione
    /// draft, appende i token grezzi del prefisso e genera blockSize token (greedy). Il target
    /// li verifica poi in batch. Ri-crea la sessione a ogni chiamata (re-prefill del prefisso
    /// recente): per un draft 5-layer e' cheap e non richiede sync KV. Ritorna i token candidati
    /// (0..blockSize).</summary>
    public int[] DraftFromPrefix(int[] prefixIds, int blockSize)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_draftEnabled) return [];
        if (prefixIds is null || prefixIds.Length == 0 || blockSize <= 0) return [];

        var session = MlxEngineNative.llm_ai_mlx_create_session(_draftModelHandle);
        if (session == IntPtr.Zero) return [];
        try
        {
            if (MlxEngineNative.llm_ai_mlx_session_append(session, prefixIds, prefixIds.Length) != 0)
                return [];
            var draft = new int[blockSize];
            var n = MlxEngineNative.llm_ai_mlx_session_generate(session, blockSize, _eosTokenId, draft);
            if (n < 0) return [];
            Array.Resize(ref draft, n > blockSize ? blockSize : n);
            return draft;
        }
        finally
        {
            MlxEngineNative.llm_ai_mlx_destroy_session(session);
        }
    }
}
