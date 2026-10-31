// IlBackend/GPU/IlGpuCompute.cs — Compute acceleration via ILGPU + CPU fallback

using ILGPU;
using ILGPU.Runtime;

namespace Basic.IlBackend.GPU;

public sealed class IlGpuCompute : IDisposable
{
    private Context? _context;
    private Accelerator? _accelerator;
    private readonly Dictionary<string, object> _kernels = new(); // cached compiled kernels

    public string Name => _accelerator?.Name ?? "CPU Multi-Core";
    public bool IsAvailable { get; private set; }
    internal Accelerator? Accel => _accelerator;

    public void Initialize()
    {
        try
        {
            _context = Context.Create(b => b.Default().EnableAlgorithms());
            _accelerator = _context.GetPreferredDevice(preferCPU: false).CreateAccelerator(_context);
            IsAvailable = true;
        }
        catch { _context?.Dispose(); _context = null; }
    }

    public void Dispose() { _accelerator?.Dispose(); _context?.Dispose(); }

    // ==================== TimeSeries ====================

    public static double[] ExecuteMovingAverage(double[] data, int period)
    {
        int n = data.Length - period + 1;
        var result = new double[n];
        if (TryGpu(data, out var accel, out var ctx))
            GpuMovingAverage(accel, ctx, data, result, period, n);
        else
            CpuMovingAverage(data, result, period, n);
        return result;
    }

    public static double[] ExecuteRollingStdDev(double[] data, int period)
    {
        int n = data.Length - period + 1;
        var result = new double[n];
        if (TryGpu(data, out var accel, out var ctx))
            GpuRollingStdDev(accel, ctx, data, result, period, n);
        else
            CpuRollingStdDev(data, result, period, n);
        return result;
    }

    public static double[] ExecuteRollingCovariance(double[] a, double[] b, int period)
    {
        int n = Math.Min(a.Length, b.Length) - period + 1;
        var result = new double[n];
        CpuRollingCovariance(a, b, result, period, n);
        return result;
    }

    public static double[] ExecuteRollingCorrelation(double[] a, double[] b, int period)
    {
        int n = Math.Min(a.Length, b.Length) - period + 1;
        var result = new double[n];
        var cov = new double[n]; var sa = new double[n]; var sb = new double[n];
        CpuRollingCovariance(a, b, cov, period, n);
        CpuRollingStdDev(a, sa, period, n);
        CpuRollingStdDev(b, sb, period, n);
        for (int i = 0; i < n; i++) { var d = sa[i] * sb[i]; result[i] = d > 1e-15 ? cov[i] / d : 0; }
        return result;
    }

    public static double[] ExecuteDifference(double[] data, int order) => CpuDifference(data, order);

    // ==================== Statistics ====================

    public static double ExecuteSum(double[] data) => CpuSum(data);
    public static double ExecuteMean(double[] data) => CpuMean(data);
    public static double ExecuteVariance(double[] data) => CpuVariance(data);
    public static double ExecuteStdDev(double[] data) => Math.Sqrt(CpuVariance(data));
    public static double ExecuteCorrelation(double[] a, double[] b) => CpuCorrelation(a, b);

    // ==================== GPU helpers ====================

    private static bool TryGpu<T>(T[] data, out Accelerator accel, out Context ctx) where T : unmanaged
    {
        accel = null!; ctx = null!;
        if (!GpuContext.IsAvailable || data.Length < 5000) return false;
        var c = GpuContext.Compute;
        if (c._accelerator == null || c._context == null) return false;
        accel = c._accelerator; ctx = c._context;
        return true;
    }

    // ==================== GPU Kernel Implementations ====================

    private static void GpuMovingAverage(Accelerator accel, Context ctx, double[] data, double[] result, int period, int n)
    {
        using var dBuf = accel.Allocate1D(data);
        using var rBuf = accel.Allocate1D<double>(n);
        Action<Index1D, ArrayView<double>, ArrayView<double>, int, int> kernel =
            (idx, src, dst, p, len) =>
        {
            if (idx >= len) return;
            double sum = 0;
            for (int j = 0; j < p; j++) sum += src[idx + j];
            dst[idx] = sum / p;
        };
        var compiled = KernelLoaders.LoadAutoGroupedStreamKernel(accel, kernel);
        compiled(n, dBuf.View, rBuf.View, period, n);
        accel.Synchronize();
        rBuf.CopyToCPU(result);
    }

    private static void GpuRollingStdDev(Accelerator accel, Context ctx, double[] data, double[] result, int period, int n)
    {
        using var dBuf = accel.Allocate1D(data);
        using var rBuf = accel.Allocate1D<double>(n);
        Action<Index1D, ArrayView<double>, ArrayView<double>, int, int> kernel =
            (idx, src, dst, p, len) =>
        {
            if (idx >= len) return;
            double sum = 0, sumSq = 0;
            for (int j = 0; j < p; j++) { var v = src[idx + j]; sum += v; sumSq += v * v; }
            var mean = sum / p;
            dst[idx] = Math.Sqrt(Math.Max(0, sumSq / p - mean * mean));
        };
        var compiled = KernelLoaders.LoadAutoGroupedStreamKernel(accel, kernel);
        compiled(n, dBuf.View, rBuf.View, period, n);
        accel.Synchronize();
        rBuf.CopyToCPU(result);
    }

    // ==================== CPU Fallback ====================

    private static void CpuMovingAverage(double[] data, double[] result, int period, int n)
    {
        Parallel.For(0, n, i => { double s = 0; for (int j = 0; j < period; j++) s += data[i + j]; result[i] = s / period; });
    }

    private static void CpuRollingStdDev(double[] data, double[] result, int period, int n)
    {
        Parallel.For(0, n, i => {
            double s = 0, sq = 0;
            for (int j = 0; j < period; j++) { var v = data[i + j]; s += v; sq += v * v; }
            var m = s / period; result[i] = Math.Sqrt(Math.Max(0, sq / period - m * m));
        });
    }

    private static void CpuRollingCovariance(double[] a, double[] b, double[] result, int period, int n)
    {
        Parallel.For(0, n, i => {
            double sa = 0, sb = 0, sab = 0;
            for (int j = 0; j < period; j++) { var va = a[i + j]; var vb = b[i + j]; sa += va; sb += vb; sab += va * vb; }
            result[i] = sab / period - (sa / period) * (sb / period);
        });
    }

    private static double[] CpuDifference(double[] data, int order)
    {
        int n = data.Length - order; var r = new double[n];
        Parallel.For(0, n, i => r[i] = data[i + order] - data[i]);
        return r;
    }

    private static double CpuSum(double[] d) { double s = 0; Parallel.For(0, d.Length, () => 0.0, (i, _, local) => local + d[i], local => { lock (d) s += local; }); return s; }
    private static double CpuMean(double[] d) => CpuSum(d) / d.Length;
    private static double CpuVariance(double[] d) { var m = CpuMean(d); double sq = 0; Parallel.For(0, d.Length, () => 0.0, (i, _, local) => local + Math.Pow(d[i] - m, 2), local => { lock (d) sq += local; }); return sq / d.Length; }
    private static double CpuCorrelation(double[] a, double[] b)
    {
        int n = Math.Min(a.Length, b.Length);
        double sa = 0, sb = 0; for (int i = 0; i < n; i++) { sa += a[i]; sb += b[i]; }
        double ma = sa / n, mb = sb / n, num = 0, da = 0, db = 0;
        Parallel.For(0, n, () => (0.0, 0.0, 0.0), (i, _, local) => {
            var dx = a[i] - ma; var dy = b[i] - mb;
            return (local.Item1 + dx * dy, local.Item2 + dx * dx, local.Item3 + dy * dy);
        }, local => { lock (a) { num += local.Item1; da += local.Item2; db += local.Item3; } });
        return num / Math.Sqrt(da * db);
    }
}
