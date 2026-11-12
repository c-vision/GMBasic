// FILE: Basic.Providers.LlmModelNative — binding P/Invoke al MODEL COMPILER LLM (dylib gmb_ai_llm).
//
// PERCHÉ P/Invoke (AGENTS, REGOLA PRIMARIA 2026-09-23): la logica pesante (quantizzazione bit-exact,
// formati GGUF/safetensors, IQ2_XXS, imatrix, .gmbarch) è GIÀ in C con parità verificata contro
// llama.cpp. Ricopiarla in C# perderebbe correttezza e prestazioni → la sorgente resta UNA: il C#
// CHIAMA la funzione C (direzione C#→C ammessa; C→C# mai).
//
// Sorgente C: native/gmb_runtime_c/src/llmmodelio_model.c + gmb_arch.c.
// Dylib: Resources/libs/<rid>/libgmb_ai_llm.dylib (target CMake `gmb_ai_llm`), risolta da
// Basic.Providers.AiNativeResolver (prefisso gmb_).
using System.Runtime.InteropServices;

namespace Basic.Providers;

public static partial class LlmModelNative
{
    private const string LibName = "gmb_ai_llm";

    static LlmModelNative()
    {
        // Resolver condiviso AI (l'assembly non trova le dylib accanto al binario pubblicato).
        AiNativeResolver.EnsureRegistered(typeof(LlmModelNative).Assembly);
    }

    // Ritornano 0 su successo, -1 su errore (il messaggio va su stderr del runtime C).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_plan(string input, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_quantize(string input, string output);

    /// <summary>LLM QUANT &lt;in&gt;, &lt;out&gt;, &lt;bpw&gt; — auto-allocazione bit per-tensore sotto il
    /// budget bpw (gmbllm, PUL, §E3). Stessa sorgente C di `llmmodelio_model_quantize_auto`.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_quantize_auto(string input, string output, double targetBpw);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_merge(string a, string b, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_prune(string input, string output, long keep);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_extract(string input, string output, long layer, long expert);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_profile(string model, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_save(string input, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_ablate(string input, string output, string pattern);
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_remove_layer(string input, string output, string layer);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_diff(string a, string b, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_checkpoint(string src, string store, string name);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_rollback(string store, string name, string dst);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_add(string basePath, string modulePath, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_adapter(string basePath, string adapterPath, string output, string alpha);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_module(string basePath, string gmmodPath, string modulePath, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_modality(string basePath, string mmprojPath, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_validate(string path, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_compile(string path, string output);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_quality(string model, string prompt, string output, string refModel);

    /// <summary>TRAIN LORA SAVE: adapter RAW (<ckpt>_best_*.raw) -> .safetensors.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_adapter_save(string ckpt, string outPath);

    /// <summary>TRAIN LORA LOAD: .safetensors -> adapter RAW (<out>_best_*.raw).</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_adapter_load(string adapter, string outPrefix);

    /// <summary>TRAIN EVAL GATE: scrive in <paramref name="outBuf"/> il verdetto JSON
    /// (ACCEPT/REJECT-FLAG/ERROR) confrontando la ppl di cand vs ref. Ritorna 0.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_gate(string refModel, string candModel, string prompt, byte[] outBuf, int cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_policy_cmd(string sub, string a, string b, string c);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_arch_cmd(string sub, string a, string b);

    /// <summary>Come llmmodelio_arch_cmd ma CATTURA il testo in `out` (IL/PCodeVM non hanno lo stdout
    /// nativo del processo C: il runtime lo stampa col proprio console).</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_arch_cmd_to(string sub, string a, string b, byte[] outBuf, int cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_policy_cmd_to(string sub, string a, string b, string c, byte[] outBuf, int cap);
    // Policy Livello B (mondo gestito): piano enumerabile + override per-tensore.
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_policy_item_count();
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_policy_item_to(int idx, byte[] outBuf, int cap);
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_policy_set_item(string name, string fmt);

    // ── RISOLUTORE DEI PATH DEI MODELLI (opzione `path.models_root`) ─────────────────────────
    // `%MODELS%/<x>` e `~/ai/Models/<x>` -> `<radice>/<x>`; `~/<x>` -> `<HOME>/<x>`; resto
    // INVARIATO. La regola vive in UN posto solo (`gmb_runtime_c/src/gmb_paths.c`) e le gambe
    // gestite la CHIAMANO via P/Invoke invece di reimplementarla (AGENTS: C#→C, mai due copie).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    private static partial System.IntPtr gmb_paths_model_path(string inPath, byte[] outBuf, nuint cap);

    /// <summary>Risolve il path di un modello con la regola del runtime C. Le forme non
    /// riconosciute (path assoluti, relativi, nomi) tornano identiche; vuoto resta vuoto.</summary>
    public static string ResolveModelPath(string path)
    {
        if (string.IsNullOrEmpty(path)) return path ?? string.Empty;
        var buf = new byte[4096];
        _ = gmb_paths_model_path(path, buf, (nuint)buf.Length);
        int n = System.Array.IndexOf(buf, (byte)0); if (n < 0) n = buf.Length;
        return System.Text.Encoding.UTF8.GetString(buf, 0, n);
    }

    // Model compiler LLM: verbi TENSOR/KERNEL/MODULE/KV (superficie .gmbarch data-driven).
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_tensor_cmd_to(string sub, string a, string b, string c, byte[] outBuf, int cap);
    /// <summary>B2 — LLM.TOKENCOUNT(model, testo): numero di token (>=0) o -1.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_token_count(string model, string text);
    /// <summary>B2 (2026-10-30) — LLM.TOKENS(model, testo): scrive gli id ("74,71,…") nel buffer.
    /// Ritorna il numero di token (>=0) o -1. Sorgente unica: gmbllm/llmmodelio_tokenizer.c.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_tokenize_ids_to(string model, string text, byte[] outBuf, nuint cap);

    /// <summary>B2 (2026-10-30) — LLM.DETOKENIZE(model, "74,71,…"): scrive il testo nel buffer.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_detokenize_to(string model, string ids, byte[] outBuf, nuint cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_kernel_cmd_to(string sub, string a, string b, byte[] outBuf, int cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_module_cmd_to(string sub, string a, string b, byte[] outBuf, int cap);

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_kv_cmd_to(string sub, string a, string b, byte[] outBuf, int cap);

    /// <summary>LLM MODEL CONVERT src, dst, fmt — GGUF/MLX usano il codec (gmb_conv_*_arch);
    /// Formati supportati: GGUF | MLX.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int llmmodelio_model_convert(string src, string dst, string fmt);

    // Modality DICHIARATA (da .gmmod | dir HF | .gguf), come stringa statica NATIVA ("" se non
    // risolta) — si copia subito in managed, niente free. Usata dai consumatori (scanner/loader)
    // per usare la modality dichiarata nell'ABI invece della convenzione di naming.
    [DllImport(LibName, EntryPoint = "llmmodelio_model_modality_str", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr llmmodelio_model_modality_str_raw([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    /// <summary>Modalità dichiarata (text|vision|audio|ocr|mtp|omni), o "" se non risolta.</summary>
    public static string ModalityOf(string path)
    {
        try
        {
            var p = llmmodelio_model_modality_str_raw(path);
            return p == IntPtr.Zero ? "" : (Marshal.PtrToStringUTF8(p) ?? "");
        }
        catch { return ""; }
    }

    // ASR.RECOGNIZE(model$, wav$[, outPath$]): engine ASR nativo C (gmb_asr_cmd), ritorna testo
    // malloc'd (da liberare con gmb_asr_free). outPath$ = file di transcript scritto IN LIVE
    // (streaming); "" = trascrizione completa. Compilato in libgmb_ai_llm.
    [DllImport(LibName, EntryPoint = "gmb_asr_cmd", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr gmb_asr_cmd_raw([MarshalAs(UnmanagedType.LPUTF8Str)] string model,
                                                 [MarshalAs(UnmanagedType.LPUTF8Str)] string wav,
                                                 [MarshalAs(UnmanagedType.LPUTF8Str)] string outPath);
    [DllImport(LibName, EntryPoint = "gmb_asr_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_asr_free(IntPtr p);

    /// <summary>ASR.RECOGNIZE(model, wav[, outPath]) con esito DISTINTO: <c>null</c> se il motore
    /// ha FALLITO (modello/WAV non caricabile, eccezione P/Invoke), altrimenti il testo (anche
    /// vuoto se la trascrizione è legittimamente vuota). Prima il fallimento veniva appiattito su
    /// "" e nessuna gamba gestita poteva alzare l'errore: <c>ON ERROR</c> restava muto su IL e
    /// PCode mentre le gambe C restituivano un valore silenzioso. Con outPath non vuoto scrive i
    /// pezzi in live sul file.</summary>
    public static string? AsrTranscribeOrNull(string model, string wav, string outPath)
    {
        try
        {
            var p = gmb_asr_cmd_raw(model ?? "", wav ?? "", outPath ?? "");
            if (p == IntPtr.Zero) return null;
            var s = Marshal.PtrToStringUTF8(p) ?? "";
            gmb_asr_free(p);
            return s;
        }
        catch { return null; }
    }

    /// <summary>Variante tollerante (mai null): "" quando il motore fallisce. Per i chiamanti che
    /// non hanno un gestore d'errore; le gambe BASIC usano <see cref="AsrTranscribeOrNull"/> e
    /// sollevano l'errore catturabile.</summary>
    public static string AsrTranscribe(string model, string wav, string outPath)
        => AsrTranscribeOrNull(model, wav, outPath) ?? "";
}
