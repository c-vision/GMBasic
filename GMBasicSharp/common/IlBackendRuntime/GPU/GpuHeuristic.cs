// GPU/GpuHeuristic.cs — Euristiche CPU vs GPU

namespace Basic.IlBackend.GPU;

/// <summary>
/// Decide se usare GPU in base a tipo operazione e dimensione dati.
/// Soglie calibrate su Tesla T4 + PCIe 3.0 x16.
/// </summary>
public static class GpuHeuristic
{
    /// <summary>Soglie minime per usare GPU invece di CPU.</summary>
    private static readonly Dictionary<string, int> Thresholds = new()
    {
        ["AADD"] = 100_000,       // broadcast addition
        ["AMAP"] = 50_000,        // element-wise map
        ["AFILTER"] = 10_000,     // stream compaction
        ["DOT"] = 10_000,         // dot product
        ["TSMA"] = 5_000,         // moving average
        ["TSROLLSTDDEV"] = 5_000, // rolling stddev
        ["VECTOR_SEARCH"] = 1_000,// nearest neighbor
        ["SORT"] = 5_000,         // radix sort
    };

    /// <summary>Se true, usa GPU; altrimenti CPU.</summary>
    public static bool ShouldUse(string operation, int elementCount, Type? elementType = null)
    {
        if (GpuContext.ForceCPU) return false;
        if (GpuContext.ForceGPU) return true;
        if (!GpuContext.IsAvailable) return false;

        return Thresholds.TryGetValue(operation, out var threshold) 
            && elementCount >= threshold;
    }
}
