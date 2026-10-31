// FILE: Basic.Runtime.Gpm.PCodeCond.cs
// PCODE COND — variabile di condizione (parità Go sync.Cond). WAIT rilascia il mutex
// associato e parcheggia; SIGNAL/BROADCAST svegliano il primo/tutti i waiter. Il task
// svegliato prosegue sull'OP_LOCK successivo (emesso dal compiler) per riacquisire il mutex.

using System.Collections.Generic;

namespace Basic.Runtime.Gpm;

public sealed class PCodeCond
{
    private readonly object _lock = new();
    private sealed record CondWaiter(PCodeTask Task, PCodeWaitRegistration Registration);
    private readonly Queue<CondWaiter> _waiters = new();

    public int WaiterCount { get { lock (_lock) return _waiters.Count; } }

    /// <summary>Parcheggia il task finché un SIGNAL/BROADCAST non lo sveglia.
    /// Ritorna false (parcheggiato); al wake il task riparte DALL'OP_LOCK successivo.</summary>
    public bool TryWait(PCodeTask task)
    {
        lock (_lock)
        {
            var registration = task.BeginWait(PCodeWaitKind.Cond, () => RemoveWaiter(task));
            _waiters.Enqueue(new CondWaiter(task, registration));
            return false;
        }
    }

    /// <summary>Sveglia al più UN waiter (parità Go c.Signal()).</summary>
    public bool Signal(PCodeScheduler? scheduler)
    {
        CondWaiter? toWake = null;
        lock (_lock)
        {
            if (_waiters.Count > 0)
                toWake = _waiters.Dequeue();
        }
        if (toWake != null)
        {
            toWake.Registration.TryComplete(() => { });
            return true;
        }
        return false;
    }

    /// <summary>Sveglia TUTTI i waiter (parità Go c.Broadcast()).</summary>
    public void Broadcast(PCodeScheduler? scheduler)
    {
        List<CondWaiter> toWake;
        lock (_lock)
        {
            toWake = [.. _waiters];
            _waiters.Clear();
        }
        foreach (var w in toWake)
            w.Registration.TryComplete(() => { });
    }

    private void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var list = new List<CondWaiter>(_waiters);
            _waiters.Clear();
            foreach (var w in list)
                if (!ReferenceEquals(w.Task, task))
                    _waiters.Enqueue(w);
        }
    }
}
