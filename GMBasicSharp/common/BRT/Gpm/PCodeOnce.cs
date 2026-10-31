// FILE: Basic.Runtime.Gpm.PCodeOnce.cs
// PCODE ONCE — flag "esegui una sola volta" cooperativo (parità Go sync.Once).
// TryBegin: primo chiamante → Running; chiamate concorrenti → park (State=Blocked,
// registrazione atomica, wake via PCodeScheduler); dopo Complete → Done, i task parcheggiati
// ri-eseguono TryBegin e trovano Done (proseguono senza eseguire la procedura).

namespace Basic.Runtime.Gpm;

public sealed class PCodeOnce
{
    private const int NotRun = 0;
    private const int Running = 1;
    private const int Done = 2;

    private readonly object _lock = new();
    private int _state;
    private sealed record OnceWaiter(PCodeTask Task, PCodeWaitRegistration Registration);
    private Queue<OnceWaiter> _waiters = new();

    public bool IsDone { get { lock (_lock) return _state == Done; } }

    /// <summary>Esito di TryBegin.</summary>
    public enum OnceBeginResult { Started, Done, Parked }

    public OnceBeginResult TryBegin(PCodeTask task)
    {
        lock (_lock)
        {
            switch (_state)
            {
                case NotRun:
                    _state = Running;
                    return OnceBeginResult.Started;
                case Done:
                    return OnceBeginResult.Done;
                default:
                    var registration = task.BeginWait(PCodeWaitKind.Once, () => RemoveWaiter(task));
                    _waiters.Enqueue(new OnceWaiter(task, registration));
                    return OnceBeginResult.Parked;
            }
        }
    }

    /// <summary>Marca il Once come completato e sveglia TUTTI i waiter (parità Go: i chiamanti
    /// concorrenti attendono il primo Do e poi proseguono).</summary>
    public void Complete(PCodeScheduler? scheduler)
    {
        List<OnceWaiter>? toWake = null;
        lock (_lock)
        {
            _state = Done;
            if (_waiters.Count > 0) { toWake = new List<OnceWaiter>(_waiters); _waiters.Clear(); }
        }
        if (toWake != null)
            foreach (var waiter in toWake)
                waiter.Registration.TryComplete(() => { });
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var remaining = new Queue<OnceWaiter>();
            while (_waiters.Count > 0)
            {
                var w = _waiters.Dequeue();
                if (!ReferenceEquals(w.Task, task)) remaining.Enqueue(w);
            }
            _waiters = remaining;
        }
    }
}
