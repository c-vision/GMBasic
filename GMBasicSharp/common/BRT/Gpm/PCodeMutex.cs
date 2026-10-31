// FILE: Basic.Runtime.Gpm.PCodeMutex.cs
// PCODE MUTEX — lock esclusivo cooperativo (parità Go sync.Mutex).
// LOCK parcheggia il task se il mutex è già detenuto; UNLOCK sveglia al più un
// waiter. Contratto park §16 (State=Blocked dentro lo stesso lock, registrazione
// atomica, wake via PCodeScheduler), identico a PCodeWaitGroup.

namespace Basic.Runtime.Gpm;

public sealed class PCodeMutex
{
    private readonly object _lock = new();
    private bool _held;
    private PCodeTask? _owner;
    private sealed record MutexWaiter(PCodeTask Task, PCodeWaitRegistration Registration);
    private Queue<MutexWaiter> _waiters = new();

    public bool IsHeld { get { lock (_lock) return _held; } }

    /// <summary>Prova ad acquisire il mutex. Se già detenuto, registra il task come in attesa
    /// e ritorna false (parità Go mu.Lock() bloccante).</summary>
    /// <summary>TryLock non bloccante (parità Go mu.TryLock(), Go 1.18): acquisisce se
    /// libero, altrimenti ritorna false SENZA parcheggiare.</summary>
    public bool TryLockNonBlocking()
    {
        lock (_lock)
        {
            if (_held) return false;
            _held = true;
            _owner = null;
            return true;
        }
    }

    public bool TryLock(PCodeTask task)
    {
        lock (_lock)
        {
            if (!_held)
            {
                _held = true;
                _owner = task;
                return true;
            }
            // HANDOFF (path PCode OP_LOCK): al risveglio il task ri-esegue TryLock (Ip--);
            // Unlock ha già ri-assegnato il mutex a questo task (_owner == task) → riconosciamo
            // il destinatario e gli concediamo il lock SENZA ri-parcheggiarlo (altrimenti:
            // _held=true → re-park infinito = deadlock, il mutex non ha più un detentore che
            // sblocchi). Parità Go: Unlock passa la proprietà al waiter svegliato.
            if (ReferenceEquals(_owner, task))
            {
                _owner = null;
                return true;
            }
            var registration = task.BeginWait(PCodeWaitKind.Mutex, () => RemoveWaiter(task));
            _waiters.Enqueue(new MutexWaiter(task, registration));
            return false;
        }
    }

    /// <summary>Rilascia il mutex e sveglia al più un waiter (parità Go mu.Unlock()).</summary>
    public void Unlock(PCodeScheduler? scheduler)
    {
        MutexWaiter? toWake = null;
        lock (_lock)
        {
            _held = false;
            _owner = null;
            if (_waiters.Count > 0) { toWake = _waiters.Dequeue(); _held = true; _owner = toWake.Task; }
        }
        if (toWake != null)
            toWake.Registration.TryComplete(() => { });
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var remaining = new Queue<MutexWaiter>();
            while (_waiters.Count > 0)
            {
                var w = _waiters.Dequeue();
                if (!ReferenceEquals(w.Task, task)) remaining.Enqueue(w);
            }
            _waiters = remaining;
        }
    }
}
