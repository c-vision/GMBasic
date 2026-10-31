// FILE: Basic.Runtime.Gpm.PCodeWaitGroup.cs
// PCODE WAITGROUP — contatore di attesa cooperativo. Stessa semantica del WAITGROUP BASIC.
// Spostato qui da PCodeProcessor.Helpers.cs (dov'era private sealed class PCodeWaitGroup)
// per renderlo accessibile a IslandRuntime, che deve replicare la semantica di park/block
// §16 per le isole native che usano WAITGROUP/DONE.
//
// Usato sia dalla P-CODE VM (via OP_WG_WAIT/OP_WG_DONE) sia dalle isole native (via
// IslandRuntime.WaitGroupWait/WaitGroupDone). Il contratto park §16 (State=Blocked dentro
// lo stesso lock, registrazione atomica, wake via PCodeScheduler) è invariato.

namespace Basic.Runtime.Gpm;

public sealed class PCodeWaitGroup
{
    private readonly object _lock = new();
    private int _counter;
    private sealed record WaitGroupWaiter(PCodeTask Task, PCodeWaitRegistration Registration);
    private readonly Queue<WaitGroupWaiter> _waiters = new();

    public int Counter { get { lock (_lock) return _counter; } }

    private static readonly bool Dbg = Environment.GetEnvironmentVariable("GMB_DBG_WG") == "1";

    public void Add(int count = 1) { lock (_lock) { _counter += count; if (Dbg) Console.Error.WriteLine($"[WG {GetHashCode()}] Add({count}) -> counter={_counter} tid={System.Threading.Thread.CurrentThread.ManagedThreadId}"); } }

    public void DoneAndWake(PCodeScheduler? scheduler)
    {
        List<WaitGroupWaiter>? toWake = null;
        lock (_lock)
        {
            _counter--;
            if (Dbg) Console.Error.WriteLine($"[WG {GetHashCode()}] Done() -> counter={_counter} waiters={_waiters.Count} tid={System.Threading.Thread.CurrentThread.ManagedThreadId}");
            if (_counter > 0) return;
            if (_waiters.Count > 0) { toWake = new List<WaitGroupWaiter>(_waiters); _waiters.Clear(); }
        }
        if (toWake != null)
            foreach (var waiter in toWake) waiter.Registration.TryComplete(() => { });
    }

    public bool ShouldWait(PCodeTask task)
    {
        lock (_lock)
        {
            if (Dbg) Console.Error.WriteLine($"[WG {GetHashCode()}] ShouldWait: counter={_counter} tid={System.Threading.Thread.CurrentThread.ManagedThreadId}");
            if (_counter <= 0) return false;
            var registration = task.BeginWait(PCodeWaitKind.WaitGroup, () => RemoveWaiter(task));
            _waiters.Enqueue(new WaitGroupWaiter(task, registration));
            return true;
        }
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var count = _waiters.Count;
            for (var i = 0; i < count; i++)
            {
                var waiter = _waiters.Dequeue();
                if (!ReferenceEquals(waiter.Task, task))
                    _waiters.Enqueue(waiter);
            }
        }
    }
}
