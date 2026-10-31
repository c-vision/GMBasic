using System.Diagnostics;

namespace Basic.IlBackend.GPU;

public static class GpuContext
{
    private static IlGpuCompute? _compute;
    private static readonly object _lock = new();

    static GpuContext()
    {
        // Auto-init su primo accesso. Non blocca l'avvio.
        ThreadPool.QueueUserWorkItem(dummy => { var c = Compute; });
    }

    public static bool IsAvailable { get; private set; }
    public static bool ForceCPU { get; set; }
    public static bool ForceGPU { get; set; }

    public static IlGpuCompute Compute
    {
        get { Init(); return _compute!; }
    }

    private static void Init()
    {
        if (_compute != null) return;
        lock (_lock)
        {
            if (_compute != null) return;
            var c = new IlGpuCompute();
            c.Initialize();
            _compute = c;
            IsAvailable = c.IsAvailable;
            Trace.WriteLine($"[GPU] Compute: {c.Name} (available={c.IsAvailable})");
        }
    }

    public static void Shutdown()
    {
        lock (_lock) { _compute?.Dispose(); _compute = null; }
    }
}
