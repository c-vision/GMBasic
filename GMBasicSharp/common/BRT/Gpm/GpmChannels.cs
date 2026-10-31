using System.Diagnostics.CodeAnalysis;
// FILE: Basic.Runtime.Gpm.GpmChannels.cs
// GPM CHANNELS — canali, select context per la P-CODE VM.
// Non duplica Interpreter/Executor.Types.cs (usa PCodeTask, non BasicTask).
//
// Parallelismo reale (vedi PCodeVm.cs, GpmCore.cs): più worker thread OS possono chiamare
// DispatchOpcode su PCodeTask diversi CONTEMPORANEAMENTE, quindi ogni ChannelInfo protegge il
// proprio stato (buffer + code d'attesa) con un lock interno — buffer/WaitingReceivers/
// WaitingSenders sono privati apposta: ogni accesso passa dai metodi qui sotto, mai da codice
// esterno che tocca i campi direttamente (altrimenti la lock non protegge nulla).

namespace Basic.Runtime.Gpm;

public sealed record ChannelReceiverWaiter(
    PCodeTask Task, SelectContext? SelectCtx, int CaseIndex,
    PCodeWaitRegistration? Registration = null)
{
    public bool TryWake(PCodeScheduler? scheduler) => Registration!.TryComplete(() => { });
}

public sealed record ChannelSenderWaiter(
    PCodeTask Task, object? Value, SelectContext? SelectCtx, int CaseIndex,
    PCodeWaitRegistration? Registration = null)
{
    public bool TryWake(PCodeScheduler? scheduler) => Registration!.TryComplete(() => { });
}

/// <summary>
/// Canale di comunicazione tra goroutine (PCodeTask).
/// Ogni variabile CHANNEL OF T contiene un riferimento a un ChannelInfo.
/// I metodi Enqueue/TryDequeue su receiver/sender sono liste "chi svegliare quando qualcosa
/// cambia" — il trasferimento del valore passa SEMPRE dal buffer (TrySend/TryReceive): un task
/// risvegliato ri-esegue l'opcode SEND/RECV da capo (Ip riavvolto) e ritenta l'operazione.
/// </summary>
public sealed class ChannelInfo
{
    private readonly object _lock = new();
    private readonly Queue<object?> _buffer = new();
    private readonly Queue<ChannelReceiverWaiter> _waitingReceivers = new();
    private readonly Queue<ChannelSenderWaiter> _waitingSenders = new();
    private bool _closed;

    /// <summary>Capacita del buffer. 0 = unbuffered (sincrono).</summary>
    public int Capacity { get; }

    /// <summary>Elementi nel buffer.</summary>
    public int Count { get { lock (_lock) return _buffer.Count; } }
    public int WaitingReceiverCount { get { lock (_lock) return _waitingReceivers.Count; } }
    public int WaitingSenderCount { get { lock (_lock) return _waitingSenders.Count; } }
    public bool IsClosed { get { lock (_lock) return _closed; } }
    public string DebugSnapshot
    {
        get
        {
            lock (_lock)
                return $"buf={_buffer.Count},recv={_waitingReceivers.Count},send={_waitingSenders.Count},closed={_closed}";
        }
    }

    /// <summary>Crea un canale con capacita data. Default 0 = unbuffered (handshake), parita' Go make(chan T).</summary>
    public ChannelInfo(int capacity = 0)
    {
        Capacity = Math.Max(0, capacity);
    }

    /// <summary>Parità Go close(ch): chiude il canale ATOMICAMENTE. Dopo la chiusura:
    /// un SEND produce un errore (panic Go, errore BASIC catturabile qui); un RECEIVE su
    /// buffer vuoto restituisce subito (value=null, ok=false); i receiver in attesa vengono
    /// tutti svegliati con ok=false; i sender in attesa ricevono errore. Una chiusura
    /// successiva (double-close) è un errore (panic). Ritorna false se il canale era già chiuso.</summary>
    public bool Close()
    {
        lock (_lock)
        {
            if (_closed) return false;
            _closed = true;
            // Sveglia tutti i receiver in attesa: riceveranno ok=false (canale chiuso).
            // NB: il risveglio NON deve pushatare nulla sullo stack del task. Un receiver
            // parcheggiato ha già riavvolto Ip (OP_RECV/OP_RECV_OK fanno task.Ip--) e al wake
            // RI-ESEGUE l'opcode, che ripopa il canale e legge ok=0 da TryReceiveOrPark. Se qui
            // pushatassimo null, il retry lo scambierebbe per il canale (chOk=null -> no-op) e
            // il task resterebbe appeso (flaky hang del pattern RECEIVE ... OK + CLOSE).
            while (_waitingReceivers.TryDequeue(out var receiver))
            {
                if (receiver.SelectCtx != null)
                {
                    receiver.SelectCtx.TrySetResult(receiver.CaseIndex, null, closed: true);
                }
                else
                {
                    receiver.Registration?.TryComplete(() => { });
                }
            }
            // Sveglia tutti i sender in attesa: l'invio su chiuso fallisce (errore/panic).
            while (_waitingSenders.TryDequeue(out var sender))
            {
                if (sender.SelectCtx != null)
                {
                    sender.SelectCtx.TrySetResult(sender.CaseIndex, null, closed: true);
                }
                else
                {
                    sender.Registration?.TryComplete(() => sender.Task.PendingException =
                        new InvalidOperationException("send su canale chiuso"));
                }
            }
            return true;
        }
    }

    /// <summary>Prova a inviare. Se il buffer non e' pieno, accoda.</summary>
    public bool TrySend(object? value)
    {
        lock (_lock)
        {
            if (_closed) return false; // Go: send su chiuso fallisce (panic nel park path).
            if (_buffer.Count >= (Capacity > 0 ? Capacity : 1)) return false;
            _buffer.Enqueue(value);
            return true;
        }
    }

    /// <summary>Prova a ricevere. Se il buffer non e' vuoto, preleva.</summary>
    public bool TryReceive(out object? value)
    {
        lock (_lock)
        {
            if (_buffer.TryDequeue(out value))
            {
                // Bug pre-esistente (esposto dal run-to-suspend del v2): liberare uno slot deve
                // svegliare un SENDER in attesa — un sender parcheggiato perché il buffer era
                // pieno restava bloccato per sempre (TryReceiveOrPark/TryReceiveWake lo fanno
                // già). Parità Go `<-ch`: una ricezione libera uno slot → il sender bloccato
                // prosegue. Idempotente (CAS sulla registration), sicuro anche dove il caller
                // sveglia già il sender a parte (PCodeVM select).
                if (TryDequeueActiveSender(out var freed))
                    freed.TryWake(null);
                return true;
            }
            value = null;
            return false; // buffer vuoto (chiuso o no): SELECT lo tratta come non-pronto.
        }
    }

    /// <summary>Ricezione per GO SELECT (fase di prova): come <see cref="TryReceiveOrPark"/>
    /// ma SENZA registrarsi se il buffer è vuoto. Se riesce, sveglia anche il sender in
    /// attesa (TryDequeueSender) — senza questo il sender parcheggiato resterebbe bloccato
    /// per sempre (il GO SELECT non registra i receiver in fase di prova). Atomico.</summary>
    public bool TryReceiveWake(out object? value, out PCodeTask? wokenSenderTask)
    {
        lock (_lock)
        {
            if (_buffer.TryDequeue(out value))
            {
                if (TryDequeueActiveSender(out var snd))
                {
                    snd!.TryWake(snd.Task.Scheduler);
                    wokenSenderTask = null;
                }
                else wokenSenderTask = null;
                return true;
            }
            wokenSenderTask = null;
            return false;
        }
    }

    /// <summary>Invio per GO SELECT (fase di prova): come <see cref="TrySendOrPark"/> ma
    /// SENZA registrarsi se il buffer è pieno. Se riesce, sveglia anche il receiver in
    /// attesa. Atomico.</summary>
    public bool TrySendWake(object? value, out PCodeTask? wokenReceiverTask)
    {
        lock (_lock)
        {
            if (_buffer.Count < (Capacity > 0 ? Capacity : 1))
            {
                _buffer.Enqueue(value);
                if (TryDequeueActiveReceiver(out var rcv))
                {
                    rcv!.TryWake(rcv.Task.Scheduler);
                    wokenReceiverTask = null;
                }
                else wokenReceiverTask = null;
                return true;
            }
            wokenReceiverTask = null;
            return false;
        }
    }

    public void EnqueueReceiver(ChannelReceiverWaiter entry)
    { lock (_lock) _waitingReceivers.Enqueue(entry); }

    public bool TryDequeueReceiver([NotNullWhen(true)] out ChannelReceiverWaiter? entry)
    { lock (_lock) return TryDequeueActiveReceiver(out entry); }

    public void EnqueueSender(ChannelSenderWaiter entry)
    { lock (_lock) _waitingSenders.Enqueue(entry); }

    public bool TryDequeueSender([NotNullWhen(true)] out ChannelSenderWaiter? entry)
    { lock (_lock) return TryDequeueActiveSender(out entry); }

    /// <summary>Invio "tutto o niente" atomico: prova a inviare, e se il buffer è pieno
    /// registra il mittente come in attesa, in UN SOLO lock hold. Necessario sotto vero
    /// parallelismo: TrySend fallito + EnqueueSender come DUE chiamate separate lascerebbe una
    /// finestra in cui un RECEIVE concorrente libera spazio nel buffer proprio fra le due
    /// chiamate — non trova ancora nessun mittente registrato (non l'ha ancora fatto), non
    /// sveglia nessuno, e questo mittente si registra un istante dopo per uno spazio che
    /// nessuno saprà mai essersi liberato: bloccato per sempre (bug osservato come hang
    /// intermittente in GoRoutinesWorkers.bas sotto vero parallelismo).
    /// Imposta anche <c>Task.State = Blocked</c> DENTRO lo stesso lock, nello stesso istante in
    /// cui il task diventa visibile in WaitingSenders — se questa assegnazione avvenisse dopo
    /// (fuori dal lock, nel chiamante), un wake concorrente potrebbe trovare il task già
    /// registrato ma ancora "Running" e ri-accodarlo (InQueue è ancora false) MENTRE il thread
    /// originale sta ancora eseguendo il suo stesso tick — due thread sulla stessa PCodeTask
    /// insieme, corruzione (altra causa dello stesso hang intermittente). Il chiamante deve
    /// preparare Ip/stack per il retry PRIMA di questa chiamata (vedi OP_SEND), non dopo,
    /// altrimenti lo stesso problema si ripresenta sullo stato dello stack invece che su State.</summary>
    public bool TrySendOrPark(object? value, ChannelSenderWaiter senderEntry,
        out ChannelReceiverWaiter? wokenReceiver)
    {
        lock (_lock)
        {
            if (_closed)
                throw new InvalidOperationException("send su canale chiuso");
            if (_buffer.Count < (Capacity > 0 ? Capacity : 1))
            {
                // Durante la registrazione multi-canale un altro case può vincere fra il check
                // IsPending del chiamante e questo lock. Rivendica il SELECT PRIMA di mutare il
                // buffer: se abbiamo perso, non inviare un secondo valore fantasma.
                if (senderEntry.SelectCtx != null &&
                    !senderEntry.SelectCtx.TrySetResult(senderEntry.CaseIndex, null))
                {
                    wokenReceiver = null;
                    return false;
                }
                _buffer.Enqueue(value);
                wokenReceiver = TryDequeueActiveReceiver(out var rcv) ? rcv : null;
                return true;
            }
            if (senderEntry.SelectCtx == null)
            {
                // Re-park: rimuove i waiter STANTII dello stesso task (accumulati da re-entry
                // senza wake reale) + riusa la registration Pending (evita che BeginWait la
                // cancelli mentre un receiver sta per completarla → wake perso, deadlock
                // multi-worker nel handoff 1-slot).
                RemoveWaiter(senderEntry.Task);
                var registration = senderEntry.Task.ReuseOrBeginWait(
                    PCodeWaitKind.Channel, () => RemoveWaiter(senderEntry.Task));
                senderEntry = senderEntry with { Registration = registration };
            }
            else
            {
                senderEntry.Task.State = PCodeTaskState.Blocked;
            }
            _waitingSenders.Enqueue(senderEntry);
            wokenReceiver = null;
            return false;
        }
    }

    /// <summary>Ricezione "tutto o niente" atomica: prova a ricevere, e se il buffer è vuoto
    /// registra il ricevente come in attesa, in UN SOLO lock hold — stessa motivazione di
    /// <see cref="TrySendOrPark"/>, ma sul lato opposto (un SEND concorrente che riempie il
    /// buffer proprio nella finestra fra un TryReceive fallito e l'EnqueueReceiver separato).
    /// Stesso motivo per l'assegnazione di <c>Task.State = Blocked</c> dentro il lock.</summary>
    public bool TryReceiveOrPark(ChannelReceiverWaiter receiverEntry, out object? value,
        out ChannelSenderWaiter? wokenSender)
    {
        lock (_lock)
        {
            // Canale chiuso e buffer vuoto: receive restituisce subito ok=false (Go).
            // Se il buffer ha ancora elementi, vengono serviti PRIMA (Go: i valori buffered
            // restano ricevibili dopo la chiusura, poi ok=false).
            if (_closed && !_buffer.TryPeek(out value))
            {
                if (receiverEntry.SelectCtx != null)
                    receiverEntry.SelectCtx.TrySetResult(receiverEntry.CaseIndex, null, closed: true);
                else
                    receiverEntry.Registration?.TryComplete(() => receiverEntry.Task.PushRef(null));
                wokenSender = null;
                return true;
            }
            if (_buffer.TryPeek(out value))
            {
                // Come sul SEND: il claim precede il dequeue, altrimenti due case pronti in
                // concorrenza potrebbero entrambi consumare pur avendo un solo winner.
                if (receiverEntry.SelectCtx != null &&
                    !receiverEntry.SelectCtx.TrySetResult(receiverEntry.CaseIndex, value))
                {
                    wokenSender = null;
                    return false;
                }
                _buffer.Dequeue();
                wokenSender = TryDequeueActiveSender(out var snd) ? snd : null;
                return true;
            }
            if (receiverEntry.SelectCtx == null)
            {
                // Re-park: rimuove i waiter STANTII dello stesso task + riusa la registration
                // Pending (evita che BeginWait la cancelli mentre un sender sta per completarla
                // → wake perso, deadlock multi-worker nel handoff 1-slot).
                RemoveWaiter(receiverEntry.Task);
                var registration = receiverEntry.Task.ReuseOrBeginWait(
                    PCodeWaitKind.Channel, () => RemoveWaiter(receiverEntry.Task));
                receiverEntry = receiverEntry with { Registration = registration };
            }
            else
            {
                receiverEntry.Task.State = PCodeTaskState.Blocked;
            }
            _waitingReceivers.Enqueue(receiverEntry);
            wokenSender = null;
            return false;
        }
    }

    public void RemoveSelectWaiters(SelectContext context)
    {
        lock (_lock)
        {
            RemoveWhere(_waitingReceivers, entry => ReferenceEquals(entry.SelectCtx, context));
            RemoveWhere(_waitingSenders, entry => ReferenceEquals(entry.SelectCtx, context));
        }
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            RemoveWhere(_waitingReceivers, entry => ReferenceEquals(entry.Task, task));
            RemoveWhere(_waitingSenders, entry => ReferenceEquals(entry.Task, task));
        }
    }

    private bool TryDequeueActiveReceiver(out ChannelReceiverWaiter? entry)
    {
        while (_waitingReceivers.TryDequeue(out ChannelReceiverWaiter? item))
        {
            entry = item;
            if (entry.SelectCtx == null)
            {
                if (entry.Registration == null || entry.Registration.IsPending)
                    return true;
                continue;
            }
            if (entry.SelectCtx!.TrySetResult(entry.CaseIndex, null))
            {
                entry.SelectCtx.DetachLosers();
                return true;
            }
        }
        entry = default!;
        return false;
    }

    private bool TryDequeueActiveSender([NotNullWhen(true)] out ChannelSenderWaiter? entry)
    {
        while (_waitingSenders.TryDequeue(out ChannelSenderWaiter? item))
        {
            entry = item;
            if (entry.SelectCtx == null)
            {
                if (entry.Registration == null || entry.Registration.IsPending)
                    return true;
                continue;
            }
            if (entry.SelectCtx!.TrySetResult(entry.CaseIndex, null))
            {
                entry.SelectCtx.DetachLosers();
                return true;
            }
        }
        entry = default!;
        return false;
    }

    private static void RemoveWhere<T>(Queue<T> queue, Func<T, bool> predicate)
    {
        var count = queue.Count;
        for (var i = 0; i < count; i++)
        {
            var item = queue.Dequeue();
            if (!predicate(item))
                queue.Enqueue(item);
        }
    }
}

/// <summary>
/// Contesto per GO SELECT. Coordina l'attesa su piu canali: un solo case può vincere, i loser
/// vengono rimossi da tutti i canali e la PCodeWaitRegistration condivisa decide atomicamente
/// fra winner e cancellazione del PCB.
/// </summary>
public sealed class SelectContext
{
    private readonly object _lock = new();
    private readonly HashSet<ChannelInfo> _channels = new();
    private int _winningCaseIndex = -1;

    /// <summary>Deadline assoluta (ms epoch) del case TIMEOUT, persistita dal runtime della
    /// GPM IL v2 perché il retry (ri-esecuzione del select block dopo il wake) sappia che il
    /// TIMEOUT è scaduto anche se SleepUntilMs è stato azzerato dal wake.</summary>
    public long TimeoutDeadlineMs = -1;

    public int WinningCaseIndex { get { lock (_lock) return _winningCaseIndex; } }
    public object? ReceivedValue { get; private set; }
    public bool Closed { get; private set; }
    public bool IsPending => WinningCaseIndex == -1;

    public void RegisterChannel(ChannelInfo channel)
    {
        lock (_lock)
            _channels.Add(channel);
    }

    /// <summary>Prova a impostare il case vincente. True se questo case ha vinto.</summary>
    public bool TrySetResult(int caseIndex, object? value)
        => TrySetResult(caseIndex, value, closed: false);

    /// <summary>Prova a impostare il case vincente, segnalando se la vittoria deriva da una
    /// chiusura del canale (parità Go: receive su closed → ok=false).</summary>
    public bool TrySetResult(int caseIndex, object? value, bool closed)
    {
        lock (_lock)
        {
            if (_winningCaseIndex != -1) return false;
            _winningCaseIndex = caseIndex;
            ReceivedValue = value;
            Closed = closed;
            return true;
        }
    }

    public void DetachLosers()
    {
        ChannelInfo[] channels;
        lock (_lock)
            channels = _channels.ToArray();
        foreach (var channel in channels)
            channel.RemoveSelectWaiters(this);
    }

    /// <summary>Termina una SELECT che non deve più riprendere e rimuove tutte le registration.</summary>
    public void Cancel()
    {
        lock (_lock)
        {
            if (_winningCaseIndex == -1)
                _winningCaseIndex = -2;
        }
        DetachLosers();
    }
}
