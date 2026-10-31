// FILE: Basic.Providers.DistillNative — P/Invoke al dominio AI DISTILL (Stream D).
// La logica dataset/report vive in native/gmb_runtime_c/src/gmb_distill.c (§0bis), compilata
// nella dylib gmb_ai_llm (raggiungibile dal mondo gestito). Direzione C#→C (AGENTS: ammessa).
using System.Runtime.InteropServices;

namespace Basic.Providers;

public static partial class DistillNative
{
    private const string LibName = "gmb_ai_llm";

    static DistillNative()
    {
        AiNativeResolver.EnsureRegistered(typeof(DistillNative).Assembly);
    }

    /// <summary>BUILD: jsonl di coppie → .bin train/hold. Ritorna n coppie o -1 in errore.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_build(string jsonlPath, string tokenizerJson,
                                                string outPrefix, int window, double holdRatio, int maxPairs);

    /// <summary>BUILD con opts JSON (formato/response identici ai backend C). 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_build_opts(string jsonlPath, string optsJson, IntPtr outBuf, int cap);

    /// <summary>Report JSON dello stato (pairs/train/hold/error). 0 = ok.</summary>
    [LibraryImport(LibName)]
    public static partial int gmb_distill_report(IntPtr outBuf, int cap);

    [LibraryImport(LibName)]
    public static partial int gmb_distill_pairs();

    [LibraryImport(LibName)]
    public static partial int gmb_distill_trainsamples();

    [LibraryImport(LibName)]
    public static partial int gmb_distill_holdsamples();

    /// <summary>SAMPLE: genera k risposte per ogni prompt e filtra (VERIFY). 0 = ok, -1 = errore.
    /// Scrive la response/report JSON in outBuf.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_sample_opts(string prompts, string optsJson, IntPtr outBuf, int cap);

    [LibraryImport(LibName)]
    public static partial int gmb_distill_samplen();

    [LibraryImport(LibName)]
    public static partial int gmb_distill_samplefail();

    /// <summary>1.0 = ultimo SAMPLE completato.</summary>
    [LibraryImport(LibName)]
    public static partial double gmb_distill_sampledone();

    /// <summary>EVAL paired (idx 0 base / idx 1 adapter): genera e misura token/pass-rate. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_eval_opts(string prompts, string optsJson, IntPtr outBuf, int cap);

    [LibraryImport(LibName)]
    public static partial double gmb_distill_evaltok(int idx);

    [LibraryImport(LibName)]
    public static partial double gmb_distill_evalpass(int idx);

    /// <summary>Acceptance rate spec-decode (D5) della config idx (0/1).</summary>
    [LibraryImport(LibName)]
    public static partial double gmb_distill_evalacc(int idx);

    /// <summary>APPLY (schema LORA APPLY + backup opzionale del checkpoint). 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_apply_opts(string ckpt, string optsJson, IntPtr outBuf, int cap);

    /// <summary>RESTORE: ricopia i file dal backup al checkpoint di origine. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_restore_opts(string backup, IntPtr outBuf, int cap);

    /// <summary>TRAIN (head-only con frozen): addestra e ritorna lo stato. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_distill_train_opts(string dataset, string optsJson, IntPtr outBuf, int cap);

    [LibraryImport(LibName)]
    public static partial double gmb_distill_loss();

    [LibraryImport(LibName)]
    public static partial double gmb_distill_hold();

    /// <summary>Path ckpt (const char* statico): letto via IntPtr per non farne free.</summary>
    [LibraryImport(LibName)]
    public static partial IntPtr gmb_distill_ckpt();

    [LibraryImport(LibName)]
    public static partial double gmb_distill_done();
}
