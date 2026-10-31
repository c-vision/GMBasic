// FILE: Basic.Providers.ProviderHealthMonitor
namespace Basic.Providers;

/// <summary>Background service that periodically runs a minimal connectivity test on every
/// registered provider and marks it Available/Unavailable accordingly.</summary>
/// <remarks>
/// ponytail: never touches Blocked/Damaged (sticky manual overrides, see <see cref="ProviderStatus"/>).
/// Started explicitly via PROVIDER.STARTHEALTHCHECK — not auto-started on REGISTER, so a BASIC
/// script/test that never asks for it never gets a background timer thread.
/// </remarks>
public sealed class ProviderHealthMonitor : IDisposable
{
    public const int DefaultIntervalMinutes = 10;

    public static ProviderHealthMonitor Instance { get; } = new();

    private readonly Lock _lock = new();
    private Timer? _timer;
    private int _intervalMinutes = DefaultIntervalMinutes;

    private ProviderHealthMonitor()
    {
    }

    public bool IsRunning
    {
        get
        {
            lock (_lock)
                return _timer is not null;
        }
    }

    public int IntervalMinutes
    {
        get
        {
            lock (_lock)
                return _intervalMinutes;
        }
    }

    /// <summary>Starts (or restarts with a new interval) the background sweep.</summary>
    /// <param name="intervalMinutes">Interval in minutes; 0 or negative uses <see cref="DefaultIntervalMinutes"/>.</param>
    public void Start(int intervalMinutes = 0)
    {
        var effective = intervalMinutes > 0 ? intervalMinutes : DefaultIntervalMinutes;
        lock (_lock)
        {
            _intervalMinutes = effective;
            var period = TimeSpan.FromMinutes(effective);
            _timer?.Dispose();
            _timer = new Timer(_ => RunSweepSafe(), null, period, period);
        }
    }

    /// <summary>Changes the interval of an already-running monitor without a full restart.
    /// If not running, only records the interval for the next <see cref="Start"/>.</summary>
    public void SetInterval(int intervalMinutes)
    {
        var effective = intervalMinutes > 0 ? intervalMinutes : DefaultIntervalMinutes;
        lock (_lock)
        {
            _intervalMinutes = effective;
            if (_timer is not null)
            {
                var period = TimeSpan.FromMinutes(effective);
                _timer.Change(period, period);
            }
        }
    }

    public void Stop()
    {
        lock (_lock)
        {
            _timer?.Dispose();
            _timer = null;
        }
    }

    private void RunSweepSafe()
    {
        try
        {
            RunSweep();
        }
        catch
        {
            // ponytail: a sweep failure (e.g. transient exception iterating providers) must never
            // kill the Timer callback thread or stop future ticks.
        }
    }

    /// <summary>Runs one sweep synchronously, right now — independent of whether the background
    /// timer is running. Useful to check/refresh status on demand without waiting for the next
    /// scheduled tick.</summary>
    public void RunSweep()
    {
        foreach (var (role, name) in ProviderServer.Instance.GetAllProviderKeys())
        {
            if (ProviderServer.Instance.GetStatus(name) is ProviderStatus.Blocked or ProviderStatus.Damaged)
                continue;

            ProviderServer.Instance.RunMinimalTest(role, name);
        }
    }

    public void Dispose() => Stop();
}
