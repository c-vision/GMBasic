// FILE: Basic.Agents.SwapPathLock
namespace Basic.Agents;

/// <summary>Cross-PROCESS exclusive lock for one FASE 2b swap pathPrefix. In-process safety is
/// already covered by AgentRecord.StateLock (only one thread ever touches a given agent's
/// session); this covers the separate case the design flagged as open: two different GMBASIC
/// OS processes pointed at the same kvSwapPath. Without it, two processes' SwapOutSession calls
/// racing on the same pathPrefix would both write to the same fixed ".kv.tmp"/".meta.tmp" names
/// (gmb_host_ai_llamacpp.cpp's atomic tmp+rename scheme assumes a single writer at a time) --
/// interleaved fwrite calls from two OS processes to the same tmp file can genuinely corrupt it,
/// unlike two threads in one process (already serialized by the native session_mutex). Realistic
/// trigger: an agent saved via AIAGENT.SAVE and reloaded via AIAGENT.LOAD in two separate
/// processes both sharing the same kvSwapPath -- AgentPersistence preserves the agent's Id, which
/// is what the swap path is keyed on.
///
/// Implemented as a FileStream opened with FileShare.None on a ".lock" sidecar -- an OS-level
/// exclusive lock any process can observe, not just an in-process construct.</summary>
internal sealed class SwapPathLock : IDisposable
{
    private readonly FileStream _stream;

    private SwapPathLock(FileStream stream) => _stream = stream;

    /// <summary>Blocks (polling) until the lock is acquired or <paramref name="timeout"/>
    /// elapses. Returns null on timeout -- callers decide whether that's fail-open (swap-out:
    /// leave the session live in RAM, same philosophy as a failed SwapOutSession call) or a hard
    /// error (swap-in: there's nothing to fall back to, the session's state only exists on disk).</summary>
    public static SwapPathLock? TryAcquire(string pathPrefix, TimeSpan timeout)
    {
        var lockPath = pathPrefix + ".lock";
        var deadline = DateTime.UtcNow + timeout;
        while (true)
        {
            try
            {
                var stream = new FileStream(lockPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
                return new SwapPathLock(stream);
            }
            catch (IOException)
            {
                if (DateTime.UtcNow >= deadline) return null;
                Thread.Sleep(20);
            }
        }
    }

    public void Dispose() => _stream.Dispose();
}
