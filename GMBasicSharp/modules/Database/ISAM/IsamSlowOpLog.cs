// FILE: Basic.ISAM.IsamSlowOpLog

using System.Diagnostics;

namespace Basic.ISAM;

/// <summary>
/// Optional structured slow-operation logging for ISAM maintenance paths.
/// Set <c>ISAM_SLOW_OP_MS</c> to a positive integer threshold (milliseconds).
/// Deadlock events are always logged when detected.
/// </summary>
public static class IsamSlowOpLog
{
    private static readonly int? ThresholdMs = ParseThreshold();

    /// <summary>
    /// Times an operation and writes one line to <see cref="Console.Error"/> when elapsed ms ≥ threshold.
    /// </summary>
    public static SlowOpScope Measure(string operation, string? detail = null) =>
        new(operation, detail, ThresholdMs);

    /// <summary>
    /// Logs a deadlock event (always emitted, independent of threshold).
    /// </summary>
    public static void LogDeadlock(string detail) =>
        Console.Error.WriteLine($"isam_slow_op op=deadlock ms=0 detail=\"{Escape(detail)}\"");

    private static int? ParseThreshold()
    {
        var raw = Environment.GetEnvironmentVariable("ISAM_SLOW_OP_MS");
        return int.TryParse(raw, out var ms) && ms > 0 ? ms : null;
    }

    private static string Escape(string value) => value.Replace('"', '\'');

    public sealed class SlowOpScope : IDisposable
    {
        private readonly string _operation;
        private readonly string? _detail;
        private readonly int? _thresholdMs;
        private readonly Stopwatch _stopwatch = Stopwatch.StartNew();
        private bool _disposed;

        internal SlowOpScope(string operation, string? detail, int? thresholdMs)
        {
            _operation = operation;
            _detail = detail;
            _thresholdMs = thresholdMs;
        }

        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;
            var elapsed = _stopwatch.ElapsedMilliseconds;
            if (_thresholdMs is not { } threshold || elapsed < threshold)
            {
                return;
            }

            var detailPart = string.IsNullOrEmpty(_detail) ? string.Empty : $" detail=\"{Escape(_detail)}\"";
            Console.Error.WriteLine($"isam_slow_op op={_operation} ms={elapsed}{detailPart}");
        }
    }
}
