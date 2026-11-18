// FILE: Basic.Providers.MoestreamServer
// Provider AI embedded "moestream" (M14): motore MoE out-of-core (native C, libgmb_ai_moestream).
// P/Invoke come gli altri LLM embedded (eccezione AI: il motore resta in C, il C# lo usa come
// libreria). Gestisce PIU' modelli in memoria per nome; default SWAP (streaming su disco),
// RESIDENT forza il caricamento intero in RAM senza swap. La generazione arrivera' con
// l'architettura del modello (M15): qui e' il piano di gestione/configurazione.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace Basic.Providers;

public static partial class MoestreamNative
{
    // Motore MoE di LLMEnv (sorgente unica): la dylib espone i simboli `llmengine_ai_moestream_*`.
    // Il nome file resta `libllmengine_moe_stream` (installato in Resources/libs/<rid>/).
    private const string LibName = "llmengine_moe_stream";

    static MoestreamNative()
    {
        // Resolver condiviso AI (gestisce gmb_ai_moestream + tutte le altre lib gmb_*):
        // il binario IL nativo (NativeAOT) non ha le dylib accanto → senza resolver falliva.
        AiNativeResolver.EnsureRegistered(typeof(MoestreamNative).Assembly);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Config
    {
        public int Mode;       // 0 SWAP, 1 RESIDENT
        public int Device;     // 0 AUTO, 1 CPU, 2 METAL
        public ulong CacheMb;
        public uint Workers;
        public uint TopN;
        public uint Cap;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Stats
    {
        public int Loaded;
        public int Mode;
        public int Device;
        public uint Layers;
        public uint Experts;
        public ulong BytesRead;
        public ulong ExpertBytes;
        public ulong ResidentBytes;
        public ulong CacheHits;
        public ulong CacheMisses;
        public ulong CacheEvictions;
        public ulong ResidentHits;   // K2: hit su esperto residente da >=1 step (non prefetch)
        public ulong PrefetchPredictions;
        public ulong PrefetchHits;
        public ulong PrefetchWasted;
        public ulong UnresolvableExperts;   // K5: esperti assenti/corrotti (output non affidabile)
    }

    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_create")]
    public static partial IntPtr gmb_ai_moestream_create();
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_destroy")]
    public static partial void gmb_ai_moestream_destroy(IntPtr p);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_load", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_load(IntPtr p, string name, string path, ref Config cfg);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_unload", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_unload(IntPtr p, string name);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_reconfigure", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_reconfigure(IntPtr p, string name, ref Config cfg);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_set_cache", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_set_cache(IntPtr p, string name, ulong cacheMb);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_set_prefetch", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_set_prefetch(IntPtr p, string name, uint topn, uint cap);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_set_device", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_set_device(IntPtr p, string name, int device);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_count")]
    public static partial int gmb_ai_moestream_count(IntPtr p);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_name_at")]
    public static partial IntPtr gmb_ai_moestream_name_at(IntPtr p, uint i);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_stats", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_stats(IntPtr p, string name, out Stats stats);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_status_str")]
    public static partial IntPtr gmb_ai_moestream_status_str(int status);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_generate", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_generate(IntPtr p, string name,
        [In] int[] promptIds, uint nPrompt, [Out] int[] outIds, uint nOut);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_extract", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_extract(IntPtr p, string name, string tensor, string outPath);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_promote", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_promote(IntPtr p, string name);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_demote", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_demote(IntPtr p, string name);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_set_mem_available")]
    public static partial void gmb_ai_moestream_set_mem_available(IntPtr p, ulong bytes);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_set_priority", StringMarshalling = StringMarshalling.Utf8)]
    public static partial void gmb_ai_moestream_set_priority(IntPtr p, string name, int priority);
    // K1: trace esperti `.moetrace` (replay offline moe_sim).
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_trace_start", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_trace_start(IntPtr p, string name, string path);
    [LibraryImport(LibName, EntryPoint = "llmengine_ai_moestream_trace_stop", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_moestream_trace_stop(IntPtr p, string name);
}

/// <summary>Errore BASIC CATTURABILE (ERR 5) per un verbo `LLM MODEL …` / `AI MODEL …` fallito:
/// stesso codice delle gambe C (`gmb_raise_error(5, …)` nell'emitter C99, `gmb_raise_basic_error(task,
/// 5.0, …)` in VM/C). Prima qui si lanciava `InvalidOperationException`, che le gambe gestite
/// mappavano su ERR 200: lo stesso statement fallito dava 5 su C99Native/VM/C e 200 su IL/PCode.</summary>
internal static class MoestreamErrors
{
    internal static Exception Verb(string message) =>
        new BasicException(ErrorCode.IllegalFunctionCall, SourcePosition.Unknown, message);
}

/// <summary>Gestore dei modelli MoE out-of-core. Registro multi-modello; policy per modello.</summary>
public sealed class MoestreamServer
{
    public static MoestreamServer Instance { get; } = new();

    static MoestreamServer()
    {
        // M21: il registry unificato legge la memoria reale; moestream è registrato per la VISTA
        // (la sua policy di memoria vive nel provider C, per la parità coi backend C).
        ModelResidencyRegistry.AvailableBytes = static () => CommonLib.SystemMemory.AvailableReclaimableBytes();
    }

    private readonly object _lock = new();
    private IntPtr _prov = IntPtr.Zero;

    private IntPtr Handle()
    {
        lock (_lock)
        {
            if (_prov == IntPtr.Zero)
            {
                _prov = MoestreamNative.gmb_ai_moestream_create();
                if (_prov == IntPtr.Zero) throw MoestreamErrors.Verb("moestream: creazione del provider nativo fallita.");
            }
            return _prov;
        }
    }

    private static string StatusText(int rc)
    {
        var p = MoestreamNative.gmb_ai_moestream_status_str(rc);
        return p == IntPtr.Zero ? $"status {rc}" : (Marshal.PtrToStringUTF8(p) ?? $"status {rc}");
    }

    /// <summary>M21 orchestrazione: priorità di eviction del modello (guidata da AI SET PRIORITY).</summary>
    public void SetPriority(string name, int priority)
    {
        if (string.IsNullOrWhiteSpace(name)) return;
        MoestreamNative.gmb_ai_moestream_set_priority(Handle(), name, priority);
        ModelResidencyRegistry.SetPriority(name, priority);
    }

    /// <summary>Statistiche tipizzate del modello (byte esperti ecc.) — per il scheduler M21.</summary>
    private MoestreamNative.Stats GetStats(string name)
    {
        int rc = MoestreamNative.gmb_ai_moestream_stats(Handle(), name, out var s);
        if (rc != 0) throw MoestreamErrors.Verb(StatusText(rc));
        return s;
    }

    /// <summary>M21 scheduler: inietta nel provider C (condiviso, unica implementazione) la memoria
    /// disponibile+reclamabile, così load RESIDENT e PROMOTE fanno LRU demote/evict.</summary>
    private void InjectAvailableMemory() =>
        MoestreamNative.gmb_ai_moestream_set_mem_available(Handle(), CommonLib.SystemMemory.AvailableReclaimableBytes());

    private static string Esc(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n");

    /// <summary>Vero se il path e' un container MoE `.gmoe` (rilevamento per AI REGISTER).</summary>
    public static bool LooksLikeModel(string path) =>
        !string.IsNullOrWhiteSpace(path) && path.EndsWith(".gmoe", StringComparison.OrdinalIgnoreCase);

    /// <summary>Converte un checkpoint HF/MLX in un container `.gmoe` (P/Invoke al runtime C, offline).
    /// `extraArgs` serve ai test (range layer/esperti); in produzione resta vuoto (full-grid).</summary>
    public static void Convert(string src, string dst, params string[] extraArgs)
    {
        if (!Directory.Exists(src)) throw new DirectoryNotFoundException($"checkpoint HF/MLX non trovato: {src}");
        // P/Invoke (AGENTS: C#→C ammesso) — NIENTE subprocess. Default storici: full-grid (l1/e1=0
        // = tutti), dense=1, ctx 4096. `extraArgs` (--layers A:B, --experts A:B, --ctx N,
        // --meta-layers N, --meta-experts N) sovrascrivono, come faceva il driver.
        uint l0 = 0, l1 = 0, e0 = 0, e1 = 0, ctx = 4096; long ml = -1, me = -1; int dense = 1;
        for (int i = 0; i < extraArgs.Length; i++)
        {
            string Val() => i + 1 < extraArgs.Length ? extraArgs[++i] : "";
            var a = extraArgs[i];
            if (a == "--layers") { var p = Val().Split(':'); if (p.Length == 2) { uint.TryParse(p[0], out l0); uint.TryParse(p[1], out l1); } }
            else if (a == "--experts") { var p = Val().Split(':'); if (p.Length == 2) { uint.TryParse(p[0], out e0); uint.TryParse(p[1], out e1); } }
            else if (a == "--dense") dense = 1;
            else if (a == "--ctx") { uint.TryParse(Val(), out var c); ctx = c; }
            else if (a == "--meta-layers") long.TryParse(Val(), out ml);
            else if (a == "--meta-experts") long.TryParse(Val(), out me);
        }
        int rc = ModelIoNative.gmb_model_convert_ex(src, dst, l0, l1, e0, e1, dense, ctx, ml, me);
        if (rc != 0)
            throw MoestreamErrors.Verb($"moestream: conversione fallita (rc {rc}): {src} -> {dst}");
    }

    /// <summary>Quantizza un modello HF/MLX (F32/F16/BF16) in formato MLX affine RTN
    /// (processo esterno, offline). `bits` 2..8; `group` minore o uguale a 0: default 64.</summary>
    public static void Quantize(string src, string dst, int bits, int group)
    {
        if (!Directory.Exists(src)) throw new DirectoryNotFoundException($"modello HF/MLX non trovato: {src}");
        if (bits < 2 || bits > 8 || (32 % bits) != 0)
            throw new ArgumentOutOfRangeException(nameof(bits), "bits deve essere 2..8 e dividere 32");
        if (group <= 0) group = 64;
        // P/Invoke (AGENTS: C#→C ammesso) — NIENTE subprocess.
        int rc = ModelIoNative.gmb_modelio_quantize(src, dst, bits, group, null, 0);
        if (rc != 0)
            throw MoestreamErrors.Verb($"moestream: quantizzazione fallita (rc {rc}): {src} -> {dst}");
    }

    /// <summary>Ispeziona (read-only) un modello su disco: ritorna il report del discovery
    /// (descriptor arch_family/dims/rope/router/norm/flags + confidenza), via P/Invoke al runtime C.</summary>
    public static string Inspect(string dir)
    {
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException($"modello HF/MLX non trovato: {dir}");
        // P/Invoke (AGENTS: C#→C ammesso) — NIENTE subprocess.
        var buf = new byte[16384];
        int n = ModelIoNative.gmb_modelio_inspect(dir, buf, buf.Length);
        if (n < 0) throw MoestreamErrors.Verb($"moestream: inspect fallita: {dir}");
        return System.Text.Encoding.UTF8.GetString(buf, 0, n);
    }

    /// <summary>Se `path` è una cartella HF/MLX, la converte (una sola volta) in `<path>/<nome>.gmoe`
    /// e ritorna quel path; se è già un `.gmoe` lo ritorna invariato. Il `.gmoe` diventa così un
    /// artefatto automatico: l'utente passa il modello, non il container.</summary>
    public static string EnsureConverted(string name, string path)
    {
        if (LooksLikeModel(path) || !Directory.Exists(path)) return path;
        var dst = Path.Combine(path, (string.IsNullOrWhiteSpace(name) ? "model" : name) + ".gmoe");
        if (!File.Exists(dst)) Convert(path, dst);
        return dst;
    }

    public string Load(string name, string path, int mode, int device, ulong cacheMb, uint workers, uint topn, uint cap)
    {
        path = EnsureConverted(name, path);
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        if (string.IsNullOrWhiteSpace(path)) return "{\"ok\":false,\"error\":\"path modello vuoto\"}";
        var cfg = new MoestreamNative.Config { Mode = mode, Device = device, CacheMb = cacheMb, Workers = workers, TopN = topn, Cap = cap };
        // M21: il provider C (condiviso) fa LRU demote/evict se la memoria non basta (load RESIDENT).
        InjectAvailableMemory();
        int rc = MoestreamNative.gmb_ai_moestream_load(Handle(), name, path, ref cfg);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        RegisterDescriptor(name, path, mode, device, cacheMb, topn);
        var st = GetStats(name);
        ModelResidencyRegistry.Register(name, "moestream", (long)st.ExpertBytes,
                                        mode == 1 ? ModelResidencyState.Active : ModelResidencyState.Warm,
                                        evict: () => Instance.Unload(name), demote: () => Instance.Demote(name),
                                        device: device switch { 2 => "metal", 1 => "cpu", _ => "auto" });
        string modeName = mode == 1 ? "resident" : "swap";
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"mode\":\"" + modeName + "\"}";
    }

    public string Unload(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_unload(Handle(), name);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        ModelResidencyRegistry.Remove(name);
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"unloaded\":true}";
    }

    /// <summary>Estrae un blob embeddato nel container `.gmoe` (es. `__tokenizer__`, `__model_type__`)
    /// su file. Ritorna i byte scritti, o &lt;0. Serve per il tokenizer embeddato (un solo file).</summary>
    public int Extract(string name, string tensor, string outPath)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(tensor) || string.IsNullOrWhiteSpace(outPath))
            return -1;
        return MoestreamNative.gmb_ai_moestream_extract(Handle(), name, tensor, outPath);
    }

    /// <summary>AI MODEL PROMOTE nome — porta il modello in ACTIVE (tutti gli esperti in RAM).</summary>
    public string Promote(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) throw MoestreamErrors.Verb("nome modello vuoto");
        InjectAvailableMemory();   // M21 scheduler nel provider C (LRU demote/evict)
        int rc = MoestreamNative.gmb_ai_moestream_promote(Handle(), name);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        ModelResidencyRegistry.SetResidency(name, ModelResidencyState.Active);
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"residency\":\"active\"}";
    }

    /// <summary>AI MODEL DEMOTE nome — riporta il modello in WARM (esperti on-demand).</summary>
    public string Demote(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) throw MoestreamErrors.Verb("nome modello vuoto");
        int rc = MoestreamNative.gmb_ai_moestream_demote(Handle(), name);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        ModelResidencyRegistry.SetResidency(name, ModelResidencyState.Warm);
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"residency\":\"warm\"}";
    }

    /// <summary>Generazione greedy: prompt (token-id) -> token-id generati (completer C#).</summary>
    public int[] Generate(string name, int[] promptIds, int maxOut)
    {
        if (string.IsNullOrWhiteSpace(name)) throw MoestreamErrors.Verb("nome modello vuoto");
        if (promptIds is null || promptIds.Length == 0) throw MoestreamErrors.Verb("prompt vuoto");
        if (maxOut <= 0) maxOut = 1;
        ModelResidencyRegistry.Touch(name);
        var outIds = new int[maxOut];
        int got = MoestreamNative.gmb_ai_moestream_generate(Handle(), name, promptIds, (uint)promptIds.Length, outIds, (uint)maxOut);
        if (got < 0) throw MoestreamErrors.Verb(StatusText(got));
        return outIds[..Math.Min(got, maxOut)];
    }

    public string Reconfigure(string name, int mode, int device, ulong cacheMb, uint workers, uint topn, uint cap)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        var cfg = new MoestreamNative.Config { Mode = mode, Device = device, CacheMb = cacheMb, Workers = workers, TopN = topn, Cap = cap };
        int rc = MoestreamNative.gmb_ai_moestream_reconfigure(Handle(), name, ref cfg);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\"}";
    }

    /// <summary>AI MODEL CACHE &lt;nome&gt;, &lt;MB&gt; — cambia il budget della expert cache (0 = off).</summary>
    public string SetCache(string name, ulong cacheMb)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_set_cache(Handle(), name, cacheMb);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"cacheMb\":" + cacheMb + "}";
    }

    /// <summary>LLM MODEL TRACE &lt;nome&gt;, &lt;path&gt; — registra il trace esperti `.moetrace` (K1):
    /// è il carburante del replay offline `moe_sim` (sizing cache senza rifare il run).</summary>
    public string TraceStart(string name, string path)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        if (string.IsNullOrWhiteSpace(path)) return "{\"ok\":false,\"error\":\"path trace vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_trace_start(Handle(), name, path);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"trace\":\"" + Esc(path) + "\"}";
    }

    /// <summary>LLM MODEL TRACE &lt;nome&gt;, OFF — ferma il trace (idempotente se non attivo).</summary>
    public string TraceStop(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_trace_stop(Handle(), name);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"trace\":\"off\"}";
    }

    /// <summary>AI MODEL PREFETCH &lt;nome&gt;, &lt;topn&gt; — 0 = OFF (nessun prefetch).</summary>
    public string SetPrefetch(string name, uint topn, uint cap)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_set_prefetch(Handle(), name, topn, cap);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"prefetch\":" + (topn == 0 ? "false" : "true") + ",\"topN\":" + topn + "}";
    }

    /// <summary>AI MODEL DEVICE &lt;nome&gt;, &lt;AUTO|CPU|METAL&gt; — METAL fallisce se assente.</summary>
    public string SetDevice(string name, string mode)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int dev = mode?.Trim().ToUpperInvariant() switch { "METAL" => 2, "CPU" => 1, _ => 0 };
        int rc = MoestreamNative.gmb_ai_moestream_set_device(Handle(), name, dev);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        string d = dev switch { 2 => "metal", 1 => "cpu", _ => "auto" };
        ModelResidencyRegistry.SetDevice(name, d);
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"device\":\"" + d + "\"}";
    }

    public string Stats(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return "{\"ok\":false,\"error\":\"nome modello vuoto\"}";
        int rc = MoestreamNative.gmb_ai_moestream_stats(Handle(), name, out var s);
        if (rc != 0) throw MoestreamErrors.Verb("{\"ok\":false,\"error\":\"" + Esc(StatusText(rc)) + "\"}");
        string mode = s.Mode == 1 ? "resident" : "swap";
        string dev = s.Device switch { 2 => "metal", 1 => "cpu", _ => "auto" };
        ulong req = s.CacheHits + s.CacheMisses;
        double hit = req > 0 ? 100.0 * s.CacheHits / req : 0.0;
        double rec = s.PrefetchPredictions > 0 ? 100.0 * s.PrefetchHits / s.PrefetchPredictions : 0.0;
        double res = s.CacheHits > 0 ? 100.0 * s.ResidentHits / s.CacheHits : 0.0;
        return "{\"ok\":true,\"model\":\"" + Esc(name) + "\",\"mode\":\"" + mode + "\",\"device\":\"" + dev +
               "\",\"layers\":" + s.Layers + ",\"experts\":" + s.Experts +
               ",\"bytesRead\":" + s.BytesRead + ",\"expertBytes\":" + s.ExpertBytes +
               ",\"residentBytes\":" + s.ResidentBytes +
               ",\"cacheHits\":" + s.CacheHits + ",\"cacheMisses\":" + s.CacheMisses +
               ",\"cacheEvictions\":" + s.CacheEvictions +
               ",\"residentHits\":" + s.ResidentHits +
               ",\"residentHitPct\":" + res.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) +
               ",\"cacheHitPct\":" + hit.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) +
               ",\"prefetchPredictions\":" + s.PrefetchPredictions +
               ",\"prefetchHits\":" + s.PrefetchHits + ",\"prefetchWasted\":" + s.PrefetchWasted +
               ",\"prefetchRecallPct\":" + rec.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) +
               ",\"unresolvableExperts\":" + s.UnresolvableExperts +
               ",\"residency\":\"" + (s.Mode == 1 ? "active" : "warm") + "\"}";
    }

    public string List()
    {
        int n = MoestreamNative.gmb_ai_moestream_count(Handle());
        var sb = new System.Text.StringBuilder("{\"ok\":true,\"count\":").Append(n).Append(",\"models\":[");
        for (uint i = 0; i < (uint)n; i++)
        {
            if (i > 0) sb.Append(',');
            var p = MoestreamNative.gmb_ai_moestream_name_at(Handle(), i);
            sb.Append('"').Append(Esc(Marshal.PtrToStringUTF8(p) ?? "")).Append('"');
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Registra il modello nel ProviderServer (kind "moestream") così appare in AI PROVIDERS.
    /// Il modello resta gestito nativamente da questo server.</summary>
    private static void RegisterDescriptor(string name, string path, int mode, int device, ulong cacheMb, uint topn)
    {
        try
        {
            var cfg = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["modelPath"] = path,
                ["mode"] = mode == 1 ? "resident" : "swap",
                ["device"] = device switch { 2 => "metal", 1 => "cpu", _ => "auto" },
                ["cacheMb"] = cacheMb.ToString(System.Globalization.CultureInfo.InvariantCulture),
                ["prefetchTopN"] = topn.ToString(System.Globalization.CultureInfo.InvariantCulture),
            };
            ProviderServer.Instance.Register("completion", name, "moestream", cfg);
        }
        catch (ArgumentException)
        {
            // nome/ruolo non valido: il modello nativo e' comunque caricato; il registry e' advisory.
        }
    }
}
