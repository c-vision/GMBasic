// AI/Decision/NativeClm.cs
//
// Provider `clm` (Contrastive Language Models) del primitivo `AI DECIDE`, via P/Invoke sul runtime C
// condiviso (libgmb_clm ← native/gmb_runtime_c: gmb_decision_clm.c + gmb_ai_decide.c). Direzione
// unica C# → C: il C# CONSUMA la fonte C (wire /v1/systemone, prosa dello stato, head di
// proiezione, encoder MLX last-token) e non ne tiene una copia. La config del provider %DECIDE
// (che vive nel registry C#) viene passata in JSON: baseUrl/apiKey/timeoutMs/instructions per il
// percorso remoto, head/encoder per il locale. Ritorna lo stesso JSON di `AI DECIDE`.

using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using Basic.Providers;

namespace Basic.Decision;

public static class NativeClm
{
    private const string Lib = "gmb_clm";

    static NativeClm() => AiNativeResolver.EnsureRegistered(typeof(NativeClm).Assembly);

    [DllImport(Lib, EntryPoint = "gmb_ai_decide_clm_run_json", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr RunNative(string state, string kind, string configJson);

    [DllImport(Lib, EntryPoint = "gmb_ai_decide_free_json", CallingConvention = CallingConvention.Cdecl)]
    private static extern void FreeNative(IntPtr p);

    /// <summary>Esegue AI DECIDE col provider clm. <paramref name="config"/> = config del provider %DECIDE.
    /// Ritorna il JSON del risultato o <c>{"error":"…"}</c> (anche se la libreria non è caricabile).</summary>
    public static string Run(string state, string kind, IReadOnlyDictionary<string, string> config)
    {
        var cfg = new JsonObject();
        foreach (var (k, v) in config) cfg[k] = v;
        IntPtr p;
        try { p = RunNative(state, kind, cfg.ToJsonString()); }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            return new JsonObject { ["error"] = "clm: libgmb_clm non caricabile (" + ex.GetType().Name + ")" }.ToJsonString();
        }
        if (p == IntPtr.Zero) return "{\"error\":\"clm: memoria esaurita\"}";
        try { return Marshal.PtrToStringUTF8(p) ?? ""; }
        finally { FreeNative(p); }
    }
}
