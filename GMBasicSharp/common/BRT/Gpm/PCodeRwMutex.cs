// FILE: Basic.Runtime.Gpm.PCodeRwMutex.cs
// PCODE RWMUTEX — lock lettore/scrittore cooperativo (parità Go sync.RWMutex).
// RLOCK consente a più lettori di condividere il lock; LOCK (scrittore) è esclusivo.
// WRITER-PREFERENCE (parità Go): se un scrittore è in attesa, i nuovi RLOCK si accodano
// dietro di lui (non lo "saltano"), evitando la starvation dello scrittore.
// Contratto park §16 (State=Blocked dentro lo stesso lock, registrazione atomica, wake via
// PCodeScheduler), identico a PCodeMutex/PCodeWaitGroup.

namespace Basic.Runtime.Gpm;

public sealed class PCodeRwMutex
{
    private readonly object _lock = new();
    private int _readers;
    private bool _writerActive;
    private PCodeTask? _writerOwner;
    private int _writerWaiting;
    private sealed record RwMutexWaiter(PCodeTask Task, PCodeWaitRegistration Registration, bool IsWriter);
    private Queue<RwMutexWaiter> _waiters = new();

    public bool IsWriterActive { get { lock (_lock) return _writerActive; } }
    public int ReaderCount { get { lock (_lock) return _readers; } }

    /// <summary>TryLock non bloccante per lo scrittore (parità Go rw.TryLock(), Go 1.18):
    /// acquisisce se nessun lettore/scrittore attivo, altrimenti false SENZA parcheggiare.</summary>
    public bool TryLockNonBlocking()
    {
        lock (_lock)
        {
            if (!_writerActive && _readers == 0 && _writerWaiting == 0)
            {
                _writerActive = true;
                _writerOwner = null;
                return true;
            }
            return false;
        }
    }

    /// <summary>LOCK (scrittore): esclusivo. Parcheggia se ci sono lettori attivi, un altro
    /// scrittore attivo, o scrittori già in coda (writer-preference).</summary>
    public bool TryLock(PCodeTask task)
    {
        lock (_lock)
        {
            if (!_writerActive && _readers == 0 && _writerWaiting == 0)
            {
                _writerActive = true;
                _writerOwner = task;
                return true;
            }
            // Handoff: il risvegliato ri-esegue TryLock (path PCode OP_LOCK con Ip--) e l'handoff
            // gli ha già ri-assegnato il lock → riconosci il destinatario e concedi.
            if (ReferenceEquals(_writerOwner, task))
            {
                _writerOwner = null;
                return true;
            }
            var registration = task.BeginWait(PCodeWaitKind.Mutex, () => RemoveWaiter(task));
            _waiters.Enqueue(new RwMutexWaiter(task, registration, IsWriter: true));
            _writerWaiting++;
            return false;
        }
    }

    /// <summary>UNLOCK (scrittore): rilascia e sveglia il primo waiter (writer o batch reader).
    /// </summary>
    public void Unlock(PCodeScheduler? scheduler)
    {
        RwMutexWaiter? toWake = null;
        lock (_lock)
        {
            _writerActive = false;
            _writerOwner = null;
            toWake = TakeNextCompatibleWaiterLocked();
        }
        if (toWake != null)
            toWake.Registration.TryComplete(() => { });
    }

    /// <summary>RLOCK (lettore): condiviso tra lettori. Parcheggia se un scrittore è attivo o
    /// in attesa (writer-preference).</summary>
    public bool TryRLock(PCodeTask task)
    {
        lock (_lock)
        {
            if (!_writerActive && _writerWaiting == 0)
            {
                _readers++;
                return true;
            }
            var registration = task.BeginWait(PCodeWaitKind.Mutex, () => RemoveWaiter(task));
            _waiters.Enqueue(new RwMutexWaiter(task, registration, IsWriter: false));
            return false;
        }
    }

    /// <summary>RUNLOCK (lettore): decrementa i lettori; quando l'ultimo rilascia, sveglia il
    /// primo waiter (writer in testa → riacquista).</summary>
    public void RUnlock(PCodeScheduler? scheduler)
    {
        RwMutexWaiter? toWake = null;
        lock (_lock)
        {
            if (_readers > 0) _readers--;
            if (_readers == 0)
                toWake = TakeNextCompatibleWaiterLocked();
        }
        if (toWake != null)
            toWake.Registration.TryComplete(() => { });
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var remaining = new Queue<RwMutexWaiter>();
            while (_waiters.Count > 0)
            {
                var w = _waiters.Dequeue();
                if (!ReferenceEquals(w.Task, task)) remaining.Enqueue(w);
                else if (w.IsWriter) _writerWaiting--;
            }
            _waiters = remaining;
        }
    }

    /// <summary>Prende il primo waiter la cui condizione di ingresso è già soddisfatta dallo
    /// stato corrente (handoff): un writer se _readers==0; un reader se non c'è writer attivo
    /// né writer in attesa. Il mutex resta in uno stato coerente per il risvegliato.</summary>
    private RwMutexWaiter? TakeNextCompatibleWaiterLocked()
    {
        while (_waiters.Count > 0)
        {
            var candidate = _waiters.Peek();
            if (candidate.IsWriter)
            {
                if (_readers == 0 && !_writerActive)
                {
                    _waiters.Dequeue();
                    _writerWaiting--;
                    _writerActive = true;
                    _writerOwner = candidate.Task;
                    return candidate;
                }
                return null; // un writer in testa ma non ancora acquisibile: non svegliare reader
            }
            else
            {
                if (_writerActive || _writerWaiting > 0)
                    return null; // reader in testa ma bloccato dalla writer-preference
                // lettore acquisibile: svegliarlo; altri reader restano in coda (sveglia 1 alla volta)
                _waiters.Dequeue();
                _readers++;
                return candidate;
            }
        }
        return null;
    }
}
