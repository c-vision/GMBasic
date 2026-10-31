// FILE: Basic.Providers.ModelMemoryGuard
// Rifiuta il caricamento di un modello se la memoria disponibile+RECLAMABILE non basta per il
// caricamento MINIMO. Con swap su disco (mmap, gpuLayers=0) il minimo è KV cache + overhead: gli
// esperti/pesi restano su disco. Con offload (gpuLayers!=0) i pesi vanno in memoria → il minimo
// include la dimensione del modello. Usa CommonLib.SystemMemory (API native attendibili).
using System;
using System.IO;

namespace Basic.Providers;

internal static class ModelMemoryGuard
{
    private const long OverheadBytes = 2L << 30;   // compute buffer / runtime llama.cpp/MLX
    private const long KvBytesPerToken = 2L * 1024; // stima prudente K+V per token (2 KB)

    /// <summary>Divisore dei pesi che restano residenti come working set caldo sotto swap (÷8 ≈
    /// 12,5%). Stima, non misura: è la frazione di pagine mmap lette di recente che il SO non rigetta.</summary>
    private const long HotSetDivisor = 8;

    /// <summary>Vista di memoria disponibile+reclamabile, iniettabile per i test (stesso pattern di
    /// <see cref="ModelResidencyRegistry.AvailableBytes"/>): la guardia così è provabile senza
    /// dipendere dallo stato di memoria della macchina.</summary>
    internal static Func<ulong> AvailableBytes = CommonLib.SystemMemory.AvailableReclaimableBytes;

    /// <summary>Peso del working set caldo dei pesi lazy (byte). Una sola sorgente: lo usa la guardia
    /// e lo usa <see cref="MlxLlmCompleter"/> per il budget di residenza.</summary>
    public static long HotSetBytes(long modelBytes)
        => Math.Max(0, modelBytes) / HotSetDivisor;

    /// <summary>Caricamento MINIMO con swap su disco (mmap: llama.cpp). I pesi restano nel file
    /// mappato (evincibile), quindi il minimo è overhead + KV: si rifiuta SOLO se nemmeno questo
    /// entra nella memoria disponibile+reclamabile.</summary>
    public static void EnsureMinimalLoad(string providerName, string path, int ctxSize)
    {
        ulong avail = AvailableBytes();
        if (avail == 0) return;
        long required = OverheadBytes + Math.Max(0, ctxSize) * KvBytesPerToken;
        Fail(providerName, path, required, avail, "Il sistema è sotto pressione: libera memoria.");
    }

    /// <summary>Unico ingresso per i caricamenti che possono essere completi o lazy: il chiamante
    /// DEVE dichiarare il modo. Il difetto che questa firma chiude è di ORDINE — leggere l'opzione
    /// swap dopo la guardia significava rifiutare il caso in cui lo swap serve (modello più grande
    /// della RAM), e nessun controllo sui sorgenti lo avrebbe rivisto.</summary>
    public static void EnsureLoad(string providerName, string path, long modelBytes, int ctxSize,
                                  bool lazyWeights)
    {
        if (lazyWeights) EnsureLazyLoad(providerName, path, modelBytes, ctxSize);
        else EnsureFullLoad(providerName, path, modelBytes, ctxSize);
    }

    /// <summary>Pesi MLX LAZY/file-backed (swap: GMB_NO_PRETOUCH_WEIGHTS). I pesi restano nodi lazy
    /// mappati e il SO ne paga da disco solo una frazione → il minimo è overhead + KV + working set
    /// caldo, NON l'intera dimensione del modello. È ciò che rende caricabile un modello più grande
    /// della RAM.</summary>
    private static void EnsureLazyLoad(string providerName, string path, long modelBytes, int ctxSize)
    {
        ulong avail = AvailableBytes();
        if (avail == 0) return;
        long required = OverheadBytes + Math.Max(0, ctxSize) * KvBytesPerToken + HotSetBytes(modelBytes);
        Fail(providerName, path, required, avail,
            "Con lo swap dei pesi il minimo è il working set caldo (overhead + KV + ~1/" +
            HotSetDivisor + " dei pesi): riduci il contesto o libera memoria.");
    }

    /// <summary>Caricamento COMPLETO in RAM (swap dei pesi non chiesto). Il modello va interamente
    /// in memoria → si rifiuta se non entra.</summary>
    private static void EnsureFullLoad(string providerName, string path, long modelBytes, int ctxSize)
    {
        ulong avail = AvailableBytes();
        if (avail == 0) return;
        long required = modelBytes + OverheadBytes + Math.Max(0, ctxSize) * KvBytesPerToken;
        Fail(providerName, path, required, avail,
            "Caricamento interamente in RAM (swap dei pesi non attivo): registra il modello con " +
            "\"swap\":\"true\" per tenere i pesi lazy su disco, usa un modello più piccolo o libera memoria.");
    }

    private static void Fail(string providerName, string path, long required, ulong avail, string hint)
    {
        if ((ulong)required <= avail) return;
        long reqGb = required / (1L << 30);
        ulong availGb = avail / (1UL << 30);
        throw new InvalidOperationException(
            $"{providerName}: memoria INSUFFICIENTE per caricare '{Path.GetFileName(path)}'. " +
            $"Richiesti ~{reqGb} GB, disponibili+reclamabili ~{availGb} GB. " + hint);
    }

    /// <summary>Dimensione su disco di un modello: file singolo (GGUF) o somma ricorsiva di una
    /// cartella (MLX/safetensors). 0 se non determinabile.</summary>
    public static long SizeOnDisk(string path)
    {
        try
        {
            if (File.Exists(path)) return new FileInfo(path).Length;
            if (Directory.Exists(path))
            {
                long total = 0;
                foreach (var f in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
                {
                    try { total += new FileInfo(f).Length; } catch { }
                }
                return total;
            }
        }
        catch { }
        return 0;
    }
}
