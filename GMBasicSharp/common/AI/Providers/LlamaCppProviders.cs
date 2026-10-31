// FILE: Basic.Providers.LlamaCppProviders
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using Basic.AI.Rag;
using Basic.Mcp;

using CommonLib;

namespace Basic.Providers;

/// <summary>P/Invoke surface over native/gmb_host_ai/gmb_host_ai_llamacpp.{h,cpp} (built as
/// libllm_ai_llamacpp.{dylib,so}/llm_ai_llamacpp.dll). Kept as a thin partial class so the
/// source-generated marshalling (LibraryImport) stays out of the completer's own logic.</summary>
internal static partial class LlamaCppNative
{
    private const string LibName = "llm_ai_llamacpp";

    static LlamaCppNative() => AiNativeResolver.EnsureRegistered(typeof(LlamaCppNative).Assembly);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_load_model(string modelPath, int nGpuLayers);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_unload_model(int modelHandle);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_shutdown();

    // Owned by the model (valid only until llm_ai_unload_model) -- do not free.
    [LibraryImport(LibName)]
    internal static partial IntPtr llm_ai_model_name(int modelHandle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_create_session(int modelHandle, int ctxSize, int kvTypeK, int kvTypeV);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_destroy_session(int sessionHandle);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_session_append(int sessionHandle, string role, string content);

    // Returned IntPtr is native-malloc'd -- must go through llm_ai_free_string, never
    // Marshal.FreeHGlobal, so this is NOT declared to auto-marshal as `string`.
    [LibraryImport(LibName)]
    internal static partial IntPtr llm_ai_session_generate(int sessionHandle, int maxTokens);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_free_string(IntPtr s);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_session_swap_out(int sessionHandle, string pathPrefix);

    // FASE 5B: OCR/visione embedded via llama.cpp mtmd (gmb_host_ai_ocr.h).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_ocr_load(string modelPath, string mmprojPath, int nCtx, int nGpuLayers);

    // Variante guidata dal descriptor .gmmod (base+mmproj+modalità DICHIARATI dall'ABI).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_ocr_load_gmmod(string gmmodPath, int nCtx, int nGpuLayers);

    /// <summary>Path del descriptor .gmmod: config "gmmodPath" oppure "&lt;modelStem&gt;.gmmod"
    /// accanto al modello. Null se assente (il chiamante ricade su model+mmproj).</summary>
    internal static string? ResolveGmmod(ProviderDescriptor descriptor, string modelPath)
    {
        try
        {
            var explicitPath = descriptor.GetConfig("gmmodPath");
            if (!string.IsNullOrWhiteSpace(explicitPath) && File.Exists(explicitPath)) return explicitPath;
            var candidate = Path.ChangeExtension(modelPath, ".gmmod");
            if (File.Exists(candidate)) return candidate;
        }
        catch { /* best effort */ }
        return null;
    }

    [LibraryImport(LibName)]
    internal static partial void llm_ai_ocr_unload(IntPtr handle);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_ocr_read_image(IntPtr handle, string imagePath, string prompt, int maxTokens);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_ai_ocr_read_audio(IntPtr handle, string audioPath, string prompt, int maxTokens);

    [LibraryImport(LibName)]
    internal static partial void llm_ai_ocr_free_string(IntPtr s);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_session_swap_in(string pathPrefix);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_token_count(int sessionHandle);

    // Sampling configurabile per-sessione (temperature<=0 = greedy). Persistito
    // nel meta-file di swap, quindi un round-trip swap-in/out ripristina la stessa
    // configurazione. top_p/top_k <=0 (o top_p>=1) = disabilitati.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_set_sampling(int sessionHandle, float temperature, float topP, int topK, long seed);

    // KV-cache manager (P0): clear/trim/stats sulla memoria del contesto.
    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_cache_clear(int sessionHandle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_cache_trim(int sessionHandle, int keep);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_cache_stats(int sessionHandle, out int nCtx, out int nTokens, out nint bytes, out int posMin, out int posMax);

    // R5: latent nativo (hidden states) — gmb_host_ai_llamacpp.c
    [LibraryImport(LibName)]
    internal static partial int llm_ai_emb_dim(int modelHandle);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_last_hidden(int sessionHandle, float[] outBuf, int cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int llm_ai_model_embed_text(int modelHandle, string text, float[] outBuf, int cap);

    // Estensione 9.1 (LatentMAS): KV-cache transfer IN-RAM (niente I/O su disco).
    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_kv_export(int sessionHandle, byte[] outBuf, int cap);

    [LibraryImport(LibName)]
    internal static partial int llm_ai_session_kv_import(int sessionHandle, byte[] buf, int n);
}

/// <summary>Native, in-process GGUF completion via llama.cpp -- zero HTTP/JSON round trip.
/// One instance loads one model into memory for its lifetime; ProviderServer caches instances
/// per provider name, so the (expensive) model load in the constructor runs once, not per ASK.
///
/// Implements <see cref="ISessionAwareLlmCompleter"/> (FASE 2): callers that want persistent,
/// incrementally-decoded conversations (one native KV cache per AgentRecord, never re-tokenizing
/// prior turns) create a session explicitly. <see cref="Complete"/> (the plain
/// <see cref="ILlmCompleter"/> path, kept for callers that don't check for session-awareness)
/// is implemented on top of the same session primitives -- a throwaway session per call, so
/// there is exactly one code path through the native layer, not two to keep in sync.
///
/// See Native-LLM-Provider-Plan.md (FASE 0 + FASE 2) for the full rationale and config keys.</summary>
internal sealed class LlamaCppLlmCompleter : ILlmCompleter, ISessionAwareLlmCompleter, ISessionStreamingCompleter, IDiskSwappableSessionCompleter, ILatentCompleter, IKvTransfer, IKvCacheManager, IToolCallingCompleter, IDisposable
{
    // ggml_type values (ggml.h) relevant to KV-cache quantization -- F16 is llama.cpp's own
    // unquantized default; Q8_0/Q4_0 are the block-quantized types this wrapper's
    // kvQuantize/kvQuantizeBits/kvTurboQuant config keys map to. No user-configurable "group
    // size" exists here (unlike Rapid-MLX-NextGen's own custom quantizer) -- ggml's Q8_0/Q4_0
    // block size (32) is fixed by the type itself, so kvQuantizeGroupSize from the original
    // design table was deliberately not implemented: it wouldn't correspond to anything real.
    private const int GgmlTypeF16 = 1;
    private const int GgmlTypeQ4_0 = 2;
    private const int GgmlTypeQ8_0 = 8;

    private readonly int _modelHandle;
    private readonly string _modelName;
    private readonly int _ctxSize;
    private readonly int _maxTokens;
    private readonly int _kvTypeK;
    private readonly int _kvTypeV;
    private readonly SwapPolicy _swapPolicy;
    private readonly ConcurrentDictionary<int, byte> _liveSessions = new();
    private bool _disposed;

    // Sampling per-sessione (P0): temperature<=0 = greedy deterministico;
    // top_p/top_k <=0 (o top_p>=1) = disabilitati; seed 0 = default nativo.
    private readonly float _temperature;
    private readonly float _topP;
    private readonly int _topK;
    private readonly long _seed;

    internal LlamaCppLlmCompleter(ProviderDescriptor descriptor)
    {
        var modelPath = ProviderHttp.RequireConfig(descriptor, "modelPath");
        _ctxSize = int.TryParse(descriptor.GetConfig("ctxSize"), out var ctx) && ctx > 0 ? ctx : 4096;
        // -1 = offload every layer to whichever GPU backend this native build was compiled
        // with (Metal/CUDA/ROCm/Vulkan); 0 forces CPU-only. See CMakeLists.txt for the
        // per-platform backend selection this pin depends on.
        var gpuLayers = int.TryParse(descriptor.GetConfig("gpuLayers"), out var gl) ? gl : -1;
        _maxTokens = int.TryParse(descriptor.GetConfig("maxTokens"), out var mt) && mt > 0 ? mt : 2048;

        // P0 sampling: temperature/topP/topK/seed dalla config (PROVIDER.REGISTER).
        // Default: temperature non impostata -> non chiama set_sampling, il runtime
        // usa il suo default storico (temp 0.8 llama.cpp / greedy MLX).
        _temperature = float.TryParse(descriptor.GetConfig("temperature"), out var temp) ? temp : float.NaN;
        _topP = float.TryParse(descriptor.GetConfig("topP"), out var tp) ? tp : 0f;
        _topK = int.TryParse(descriptor.GetConfig("topK"), out var tk) ? tk : 0;
        _seed = long.TryParse(descriptor.GetConfig("seed"), out var sd) ? sd : 0;

        // FASE 2b: KV-cache quantization -- shrinks the per-token memory cost of every
        // session's KV cache (measured in FASE 2b: proportional to tokens decoded, not
        // ctx_size), so more sessions fit in the same kvSwapMaxHotSessions budget before
        // eviction kicks in. kvQuantize=false (default) is exactly the original F16 behavior.
        var kvQuantize = bool.TryParse(descriptor.GetConfig("kvQuantize"), out var kq) && kq;
        var kvTurboQuant = bool.TryParse(descriptor.GetConfig("kvTurboQuant"), out var ktq) && ktq;
        var kvQuantizeBits = int.TryParse(descriptor.GetConfig("kvQuantizeBits"), out var kqb) ? kqb : 8;
        if (!kvQuantize) { _kvTypeK = GgmlTypeF16; _kvTypeV = GgmlTypeF16; }
        else if (kvTurboQuant) { _kvTypeK = GgmlTypeQ8_0; _kvTypeV = GgmlTypeQ4_0; } // "k8v4" -- K at 8-bit, V at 4-bit, the asymmetric mode Rapid-MLX-NextGen calls TurboQuant
        else { var t = kvQuantizeBits == 4 ? GgmlTypeQ4_0 : GgmlTypeQ8_0; _kvTypeK = t; _kvTypeV = t; }

        // FASE 2b policy layer: kvSwapEnabled=false (default) is exactly pre-FASE-2b behavior --
        // unlimited hot sessions, AgentServer never calls SwapOutSession/SwapInSession. Default
        // kvSwapMaxHotSessions of 8 is arbitrary (no measured production workload to calibrate
        // against yet) but only takes effect when a caller opts in.
        var kvSwapEnabled = bool.TryParse(descriptor.GetConfig("kvSwapEnabled"), out var se) && se;
        var kvSwapMaxHotSessions = int.TryParse(descriptor.GetConfig("kvSwapMaxHotSessions"), out var mhs) && mhs > 0 ? mhs : 8;
        // D1 RAM watermark (2026-09-10): kvSwapMaxKvMb caps the total KV state bytes of this
        // completer's hot sessions (llama_state_get_size via llm_ai_session_cache_stats),
        // independent of the session count cap. 0/unset = disabled (count-only, legacy).
        var kvSwapMaxKvMb = long.TryParse(descriptor.GetConfig("kvSwapMaxKvMb"), out var mkb) && mkb > 0 ? mkb : 0;
        var kvSwapPath = descriptor.GetConfig("kvSwapPath") is { } configuredPath && !string.IsNullOrWhiteSpace(configuredPath)
            ? Basic.ModelDownload.PathExpansion.Expand(configuredPath)
            : Path.Combine(GmbiPaths.KvSwapDir, descriptor.Name);
        _swapPolicy = new SwapPolicy(kvSwapEnabled, kvSwapMaxHotSessions, kvSwapPath, kvSwapMaxKvMb * 1024L * 1024L);

        // Rifiuto se non c'è memoria per il caricamento MINIMO (mmap su disco: pesi nel file
        // mappato evincibile, footprint = overhead + KV). Evita di saturare la macchina.
        ModelMemoryGuard.EnsureMinimalLoad($"llama.cpp provider '{descriptor.Name}'", modelPath, _ctxSize);

        _modelHandle = LlamaCppNative.llm_ai_load_model(modelPath, gpuLayers);
        if (_modelHandle < 0)
            throw new InvalidOperationException(
                $"llama.cpp provider '{descriptor.Name}': failed to load model at '{modelPath}'.");

        _modelName = descriptor.GetConfig("modelVersion")
            ?? Marshal.PtrToStringUTF8(LlamaCppNative.llm_ai_model_name(_modelHandle))
            ?? modelPath;
        // M21: registry unificato — llama.cpp usa mmap (pesi file-backed → WARM); evict = Dispose.
        ModelResidencyRegistry.Register(_modelName, "llamacpp", ModelMemoryGuard.SizeOnDisk(modelPath),
                                        ModelResidencyState.Warm, evict: () => Dispose(),
                                        device: gpuLayers != 0 ? "metal" : "cpu");
    }

    public SwapPolicy GetSwapPolicy() => _swapPolicy;

    public string ModelName => _modelName;

    // ── ILatentCompleter (R5): hidden states nativi ──────────────────────────
    public int EmbDim => LlamaCppNative.llm_ai_emb_dim(_modelHandle);

    public float[] LastHidden(int sessionId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var dim = LlamaCppNative.llm_ai_emb_dim(_modelHandle);
        if (dim <= 0) return [];
        var buf = new float[dim];
        var n = LlamaCppNative.llm_ai_session_last_hidden(sessionId, buf, buf.Length);
        if (n < 0) return [];
        Array.Resize(ref buf, n);
        return buf;
    }

    public float[] EmbedText(string text)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (string.IsNullOrEmpty(text)) return [];
        var dim = LlamaCppNative.llm_ai_emb_dim(_modelHandle);
        if (dim <= 0) return [];
        var buf = new float[dim];
        var n = LlamaCppNative.llm_ai_model_embed_text(_modelHandle, text, buf, buf.Length);
        if (n < 0) return [];
        Array.Resize(ref buf, n);
        return buf;
    }

    // ── IKvTransfer (9.1 LatentMAS): KV-cache transfer IN-RAM ────────────────
    public bool TransferKv(int srcSessionId, int dstSessionId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_liveSessions.ContainsKey(srcSessionId) || !_liveSessions.ContainsKey(dstSessionId)) return false;
        var size = LlamaCppNative.llm_ai_session_kv_export(srcSessionId, [], 0);
        if (size <= 0) return false;
        var buf = new byte[size];
        var n = LlamaCppNative.llm_ai_session_kv_export(srcSessionId, buf, buf.Length);
        if (n <= 0) return false;
        return LlamaCppNative.llm_ai_session_kv_import(dstSessionId, buf, n) == 0;
    }

    public string Complete(string systemPrompt, string userPrompt)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var sessionId = CreateSession();
        try
        {
            if (!string.IsNullOrEmpty(systemPrompt))
                AppendMessage(sessionId, "system", systemPrompt);
            AppendMessage(sessionId, "user", userPrompt);
            return GenerateFromSession(sessionId, _maxTokens);
        }
        finally
        {
            DestroySession(sessionId);
        }
    }

    public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools)
    {
        if (tools == null || tools.Count == 0)
            return new ToolCallOutcome(Complete(systemPrompt, userPrompt), null);

        // Tool-calling su llama.cpp (FASE locale): il runtime nativo non ha API tools dedicata,
        // quindi si usa il prompt-engineering strutturato (stesso formato del fallback
        // AgentToolLoop) — il modello riceve la lista tool nel system prompt e deve
        // emettere JSON {"tool_calls":[{"name":"...","arguments":{}}]} se vuole chiamare.
        var sb = new System.Text.StringBuilder(systemPrompt ?? "");
        sb.AppendLine();
        sb.AppendLine("## Available tools (respond with JSON {\"tool_calls\":[{\"name\":\"...\",\"arguments\":{}}]} to invoke):");
        foreach (var t in tools)
            sb.AppendLine($"- {t.Name}: {t.Description}");
        var toolSystem = sb.ToString();
        var raw = Complete(toolSystem, userPrompt);
        var calls = ToolCallParser.TryParse(raw);
        if (calls != null && calls.Count > 0)
            return new ToolCallOutcome(null, calls);
        return new ToolCallOutcome(raw, null);
    }

    public int CreateSession()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var sessionId = LlamaCppNative.llm_ai_create_session(_modelHandle, _ctxSize, _kvTypeK, _kvTypeV);
        if (sessionId < 0)
            throw new InvalidOperationException($"llama.cpp provider: failed to create session (model '{_modelName}').");

        ApplySampling(sessionId);
        _liveSessions[sessionId] = 0;
        return sessionId;
    }

    /// <summary>Applica la config di sampling a una sessione appena creata/ripristinata.
    /// Se temperature non e' stata configurata, non chiama il runtime (default storico).</summary>
    private void ApplySampling(int sessionId)
    {
        if (float.IsNaN(_temperature)) return;
        LlamaCppNative.llm_ai_session_set_sampling(sessionId, _temperature, _topP, _topK, _seed);
    }

    public void AppendMessage(int sessionId, string role, string content)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        if (LlamaCppNative.llm_ai_session_append(sessionId, role, content) != 0)
            throw new InvalidOperationException(
                $"llama.cpp provider: failed to append '{role}' turn to session {sessionId} (model '{_modelName}').");
    }

    public string GenerateFromSession(int sessionId, int maxTokens)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var resultPtr = LlamaCppNative.llm_ai_session_generate(sessionId, maxTokens > 0 ? maxTokens : _maxTokens);
        if (resultPtr == IntPtr.Zero)
            throw new InvalidOperationException(
                $"llama.cpp provider: generation failed for session {sessionId} (model '{_modelName}').");

        try
        {
            return Marshal.PtrToStringUTF8(resultPtr) ?? "";
        }
        finally
        {
            LlamaCppNative.llm_ai_free_string(resultPtr);
        }
    }

    public string GenerateFromSessionStream(int sessionId, int maxTokens, Action<string> onToken)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var limit = maxTokens > 0 ? maxTokens : _maxTokens;
        var sb = new System.Text.StringBuilder();

        for (var i = 0; i < limit; i++)
        {
            var resultPtr = LlamaCppNative.llm_ai_session_generate(sessionId, 1);
            if (resultPtr == IntPtr.Zero) break; // errore -> fermati
            string text;
            try { text = Marshal.PtrToStringUTF8(resultPtr) ?? ""; }
            finally { LlamaCppNative.llm_ai_free_string(resultPtr); }

            if (string.IsNullOrEmpty(text)) break; // EOS: nessun token residuo
            sb.Append(text);
            onToken?.Invoke(text);
        }

        return sb.ToString();
    }

    public void DestroySession(int sessionId)
    {
        if (_liveSessions.TryRemove(sessionId, out _))
            LlamaCppNative.llm_ai_destroy_session(sessionId);
    }

    // ── KV-cache manager (P0): primitive base sulla memoria del contesto ────

    public void ClearCache(int sessionId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (LlamaCppNative.llm_ai_session_cache_clear(sessionId) != 0)
            throw new InvalidOperationException($"llama.cpp provider: cache clear failed for session {sessionId}.");
    }

    public void TrimCache(int sessionId, int dropTokens)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (dropTokens < 0) throw new ArgumentOutOfRangeException(nameof(dropTokens));
        if (LlamaCppNative.llm_ai_session_cache_trim(sessionId, dropTokens) != 0)
            throw new InvalidOperationException($"llama.cpp provider: cache trim failed for session {sessionId}.");
    }

    public IKvCacheManager.CacheStats GetCacheStats(int sessionId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var rc = LlamaCppNative.llm_ai_session_cache_stats(
            sessionId, out var nCtx, out var nTokens, out var bytes, out var posMin, out var posMax);
        if (rc != 0)
            throw new InvalidOperationException($"llama.cpp provider: cache stats failed for session {sessionId}.");
        return new IKvCacheManager.CacheStats(nCtx, nTokens, bytes, posMin, posMax);
    }

    public bool SwapOutSession(int sessionId, string pathPrefix)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_liveSessions.ContainsKey(sessionId))
            throw new InvalidOperationException($"llama.cpp provider: session {sessionId} is not live (already destroyed or swapped out).");

        return LlamaCppNative.llm_ai_session_swap_out(sessionId, pathPrefix) == 0;
    }

    public int SwapInSession(string pathPrefix)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var sessionId = LlamaCppNative.llm_ai_session_swap_in(pathPrefix);
        if (sessionId >= 0)
        {
            // Il runtime ripristina il sampling dal meta-file (GMBKV03); se il
            // provider ha una temperature configurata, prevale quella corrente.
            ApplySampling(sessionId);
            _liveSessions[sessionId] = 0;
        }
        return sessionId;
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        ModelResidencyRegistry.Remove(_modelName);   // M21: esce dalla residenza

        // Defensive cleanup: destroys any session a caller forgot to release
        // (e.g. an AgentRecord whose owning ProviderServer instance got
        // evicted/replaced without going through AgentServer.Delete) before
        // the model itself is unloaded.
        foreach (var sessionId in _liveSessions.Keys)
            LlamaCppNative.llm_ai_destroy_session(sessionId);
        _liveSessions.Clear();

        LlamaCppNative.llm_ai_unload_model(_modelHandle);
    }
}

/// <summary>FASE 5B: OCR/visione embedded via llama.cpp mtmd — implementa
/// <see cref="IVisionCompleter"/>, così <c>AI.OCR</c> funziona contro questo provider senza
/// alcuna modifica al dispatch (il dispatch in AI.OCR verifica già
/// <c>completer is IVisionCompleter</c>). Il motore è mtmd (vendored in
/// native/gmb_host_ai/build/_deps/llama_cpp-src/tools/mtmd/): carica il modello testo GGUF
/// + l'encoder visione mmproj GGUF, e legge un'immagine da file per turno (usa-e-getta, il
/// KV-cache non è persistente tra chiamate — coerente con l'input immagine nuova a ogni
/// chiamata). Cross-platform (llama.cpp gira su CUDA/ROCm/Vulkan/CPU, non solo Apple
/// Silicon). Config: modelPath (GGUF), mmprojPath (GGUF), contextSize, gpuLayers.
/// Il prompt per PaddleOCR è "OCR:" (recognition testo); l'istruzione OCR del modello è
/// passata direttamente al wrapper che aggiunge il framing del chat template.</summary>
internal sealed class LlamaCppVisionCompleter : IVisionCompleter, IDisposable
{
    private readonly IntPtr _ocrHandle;
    private readonly string _modelName;
    private bool _disposed;

    internal LlamaCppVisionCompleter(ProviderDescriptor descriptor)
    {
        var modelPath = ProviderHttp.RequireConfig(descriptor, "modelPath");
        var mmprojPath = descriptor.GetConfig("mmprojPath") ?? "";
        var ctx = int.TryParse(descriptor.GetConfig("contextSize"), out var c) && c > 0 ? c : 8192;
        var gpuLayers = int.TryParse(descriptor.GetConfig("gpuLayers"), out var gl) && gl >= 0 ? gl : 99;

        var _gmmod = LlamaCppNative.ResolveGmmod(descriptor, modelPath);
        if (_gmmod is null && string.IsNullOrWhiteSpace(mmprojPath))
            throw new InvalidOperationException("llamacpp vision/audio: richiesto mmprojPath o un descriptor .gmmod accanto al modello");
        _ocrHandle = _gmmod != null
            ? LlamaCppNative.llm_ai_ocr_load_gmmod(_gmmod, ctx, gpuLayers)
            : LlamaCppNative.llm_ai_ocr_load(modelPath, mmprojPath, ctx, gpuLayers);
        if (_ocrHandle == IntPtr.Zero)
            throw new InvalidOperationException(
                $"llamacpp vision provider '{descriptor.Name}': failed to load OCR model — check modelPath/mmprojPath GGUF files.");

        _modelName = descriptor.GetConfig("model") ?? Path.GetFileNameWithoutExtension(modelPath);
    }

    public string ModelName => _modelName;

    /// <summary>Questo provider è visione-only (mtmd OCR): non ha un percorso di
    /// completamento testuale puro. AIAGENT.ASK/Complete su di lui lanciano
    /// NotSupportedException con messaggio chiaro invece di un no-op silenzioso
    /// (regola anti-stub del progetto) -- usare AI.OCR per l'uso corretto.</summary>
    public string Complete(string systemPrompt, string userPrompt) =>
        throw new NotSupportedException(
            $"llamacpp vision provider '{_modelName}' is OCR-only — use AI.OCR (IVisionCompleter), not AIAGENT.ASK.");

    /// <summary>OcrImage: salva i byte immagine in un file temporaneo e lo passa al wrapper
    /// nativo (mtmd_helper_bitmap_init_from_file usa stb_image vendored — nessuna pipeline di
    /// decodifica in C#). Il prompt è inoltrato così com'è; il wrapper aggiunge il framing
    /// del chat template e il marker immagine.</summary>
    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var ext = string.IsNullOrWhiteSpace(format) ? ".png" : format.StartsWith('.') ? format : "." + format;
        var tmpPath = Path.Combine(Path.GetTempPath(), "gmb-ocr-" + Guid.NewGuid().ToString("N") + ext);
        try
        {
            File.WriteAllBytes(tmpPath, imageBytes);
            var result = LlamaCppNative.llm_ai_ocr_read_image(_ocrHandle, tmpPath, prompt, 512);
            if (result == IntPtr.Zero)
                return "";
            try
            {
                return Marshal.PtrToStringUTF8(result) ?? "";
            }
            finally
            {
                LlamaCppNative.llm_ai_ocr_free_string(result);
            }
        }
        finally
        {
            try { File.Delete(tmpPath); } catch { /* best effort */ }
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        LlamaCppNative.llm_ai_ocr_unload(_ocrHandle);
    }
}

/// <summary>FASE 5B: trascrizione audio embedded via llama.cpp mtmd (Qwen2.5-Omni) —
/// implementa <see cref="IAudioCompleter"/>, così <c>AI.TRANSCRIBE</c> funziona contro
/// questo provider senza modifiche al dispatch. Il motore è lo stesso wrapper mtmd
/// dell'OCR: il modello Qwen2.5-Omni (GGUF testo + mmproj con projector audio) prende
/// l'audio come bitmap e genera il testo trascritto col framing ChatML. L'audio è
/// caricato da file (wav/mp3/flac via miniaudio, auto-detect). Config: modelPath
/// (GGUF testo), mmprojPath (mmproj con projector audio), contextSize, gpuLayers.
/// Complete() lancia NotSupportedException (audio-only, anti-stub).</summary>
internal sealed class LlamaCppAudioCompleter : IAudioCompleter, IDisposable
{
    private readonly IntPtr _ocrHandle;
    private readonly string _modelName;
    private bool _disposed;

    internal LlamaCppAudioCompleter(ProviderDescriptor descriptor)
    {
        var modelPath = ProviderHttp.RequireConfig(descriptor, "modelPath");
        var mmprojPath = descriptor.GetConfig("mmprojPath") ?? "";
        var ctx = int.TryParse(descriptor.GetConfig("contextSize"), out var c) && c > 0 ? c : 8192;
        var gpuLayers = int.TryParse(descriptor.GetConfig("gpuLayers"), out var gl) && gl >= 0 ? gl : 99;

        var _gmmod = LlamaCppNative.ResolveGmmod(descriptor, modelPath);
        if (_gmmod is null && string.IsNullOrWhiteSpace(mmprojPath))
            throw new InvalidOperationException("llamacpp vision/audio: richiesto mmprojPath o un descriptor .gmmod accanto al modello");
        _ocrHandle = _gmmod != null
            ? LlamaCppNative.llm_ai_ocr_load_gmmod(_gmmod, ctx, gpuLayers)
            : LlamaCppNative.llm_ai_ocr_load(modelPath, mmprojPath, ctx, gpuLayers);
        if (_ocrHandle == IntPtr.Zero)
            throw new InvalidOperationException(
                $"llamacpp audio provider '{descriptor.Name}': failed to load model — check modelPath/mmprojPath GGUF files.");

        _modelName = descriptor.GetConfig("model") ?? Path.GetFileNameWithoutExtension(modelPath);
    }

    public string ModelName => _modelName;

    /// <summary>Audio-only provider: no plain-text completion path.</summary>
    public string Complete(string systemPrompt, string userPrompt) =>
        throw new NotSupportedException(
            $"llamacpp audio provider '{_modelName}' is audio-only — use AI.TRANSCRIBE (IAudioCompleter), not AIAGENT.ASK.");

    /// <summary>TranscribeAudio: salva i byte audio in un file temporaneo e lo passa al
    /// wrapper nativo (mtmd_helper_bitmap_init_from_file auto-detecta audio da magic
    /// bytes, miniaudio per wav/mp3/flac).</summary>
    public string TranscribeAudio(byte[] audioBytes, string format)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var ext = string.IsNullOrWhiteSpace(format) ? ".wav" : format.StartsWith('.') ? format : "." + format;
        var tmpPath = Path.Combine(Path.GetTempPath(), "gmb-audio-" + Guid.NewGuid().ToString("N") + ext);
        try
        {
            File.WriteAllBytes(tmpPath, audioBytes);
            var result = LlamaCppNative.llm_ai_ocr_read_audio(_ocrHandle, tmpPath, "Transcribe the audio.", 512);
            if (result == IntPtr.Zero)
                return "";
            try
            {
                return Marshal.PtrToStringUTF8(result) ?? "";
            }
            finally
            {
                LlamaCppNative.llm_ai_ocr_free_string(result);
            }
        }
        finally
        {
            try { File.Delete(tmpPath); } catch { /* best effort */ }
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        LlamaCppNative.llm_ai_ocr_unload(_ocrHandle);
    }
}
