// FILE: Basic.IlBackend.Gpm.Await.IlAwaitOperations.cs
// G5 — Await operation unificata (piano §9): concretizzazione del protocollo IlAwaitOperation
// per i park point della GPM v2. Il contratto:
//   Created → Pending (TryStart) → Completed/Faulted (una sola completion, CompleteOnce) →
//   Consumed (TryConsume al resume)  oppure  Pending → Cancelled (Cancel, wake).
// Generation: ogni op ha una generation; i callback di wake TARDIVI/duplicati sono innocui
// perché TryConsume accetta la transizione una sola volta (Status → Consumed) e la verifica
// dell'op corrente è sempre sul frame (frame.Await).

using System;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Gpm;

/// <summary>Base comune delle await operation v2: gestione generazionale dello stato e
/// consumazione una-sola-volta. Le op concrete implementano TryStart/Cancel.</summary>
public abstract class IlAwaitOperationBase : IlAwaitOperation
{
    public long Generation { get; protected set; }
    public IlAwaitStatus Status { get; protected set; } = IlAwaitStatus.Created;

    protected object? Result;
    protected Exception? Error;

    public virtual bool TryConsume(out object? result, out Exception? error)
    {
        if (Status is not (IlAwaitStatus.Completed or IlAwaitStatus.Faulted or IlAwaitStatus.Cancelled))
        {
            result = null;
            error = null;
            return false;
        }
        result = Result;
        // Un'op CANCELLATA senza errore esplicito produce OperationCanceledException al
        // consume: il blocco la tratterà come Throw (ON ERROR/TRY possono catturarla).
        error = Error ?? (Status == IlAwaitStatus.Cancelled
            ? new OperationCanceledException("Await cancellato.")
            : null);
        Status = IlAwaitStatus.Consumed;
        return true;
    }

    /// <summary>Completa l'op una sola volta (le completion concorrenti successive sono ignorate).</summary>
    protected void CompleteOnce(object? result, Exception? error)
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Result = result;
            Error = error;
            Status = error is null ? IlAwaitStatus.Completed : IlAwaitStatus.Faulted;
            Generation++;
        }
    }

    public abstract bool TryStart(PCodeTask task, IlActivationFrame frame);

    public abstract void Cancel();
}

/// <summary>Await di un TIMER (SLEEP): parcheggia fino alla deadline; il wake dello scheduler
/// È la completion. TryConsume verifica la deadline (un wake prematuro → non consuma → il
/// retry del blocco ri-parcha sulla stessa op, generation-safe). Cancel → Cancelled + wake.</summary>
public sealed class IlTimerAwait : IlAwaitOperationBase
{
    public long DeadlineMs { get; }

    public IlTimerAwait(long deadlineMs)
    {
        DeadlineMs = deadlineMs;
        Status = IlAwaitStatus.Pending;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        task.SleepUntilMs = DeadlineMs;
        return false; // sempre park: la completion arriva via wake dello scheduler
    }

    public override bool TryConsume(out object? result, out Exception? error)
    {
        if (Status == IlAwaitStatus.Cancelled)
        {
            result = null;
            error = new OperationCanceledException("SLEEP cancellato.");
            Status = IlAwaitStatus.Consumed;
            return true;
        }
        // Il wake dello scheduler può essere prematuro (spurious): consuma SOLO se la deadline
        // è passata — altrimenti il retry del blocco ri-parcha (generation-safe).
        if (DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() >= DeadlineMs)
            CompleteOnce(null, null);
        return base.TryConsume(out result, out error);
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di un RECEIVE su canale (parità Go `v := <-ch`): registra il receiver
/// UNA volta (TryStart idempotente — un retry per wake prematuro NON ri-registra un secondo
/// waiter), completa una sola volta con il valore ricevuto. Il valore vive sull'op e viene
/// consumato al resume (generation-safe).</summary>
public sealed class IlChannelReceiveAwait : IlAwaitOperationBase
{
    private readonly ChannelInfo _channel;
    private readonly ChannelReceiverWaiter _waiter;

    public IlChannelReceiveAwait(PCodeTask task, ChannelInfo channel)
    {
        _channel = channel;
        _waiter = new ChannelReceiverWaiter(task, null, 0);
    }

    public string DebugState => $"{Status}/g{Generation}/{_channel.DebugSnapshot}";

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        // Retry (wake prematuro): l'op è già registrata sul canale → resta parcheggiata.
        if (Status is IlAwaitStatus.Pending) return false;
        if (Status == IlAwaitStatus.Cancelled) return false;
        Status = IlAwaitStatus.Pending;
        if (_channel.TryReceiveOrPark(_waiter, out var value, out var wokenSender))
        {
            CompleteOnce(value, null);
            if (wokenSender is { } snd) snd.TryWake(task.Scheduler);
            return true;
        }
        return false;
    }

    public override bool TryConsume(out object? result, out Exception? error)
    {
        // Ri-tenta il receive SOLO se l'op è ancora Pending (parcheggiata): il wake del
        // canale sveglia il task e il valore è (o sarà) nel buffer — parità Go (il task
        // risvegliato ri-tenta la ricezione). Se TryStart ha GIÀ completato (receive
        // immediato), TryConsume consuma il risultato senza ri-dequeuare un secondo valore.
        if (Status is IlAwaitStatus.Pending)
        {
            if (_channel.TryReceive(out var value))
            {
                CompleteOnce(value, null);
            }
            else if (_channel.IsClosed)
            {
                CompleteOnce(null, null);
            }
            else if (_channel.TryReceiveOrPark(_waiter, out var reValue, out var wokenSender))
            {
                // Wake spurioso (un competitor ha vinto il valore): il buffer era vuoto ma
                // nel frattempo un sender è arrivato → consumalo. Altrimenti TryReceiveOrPark
                // RI-ACCADA il receiver nella coda del canale — senza, il task svegliato e
                // poi ri-parcheggiato dal driver NON è più nella coda → un successivo SEND
                // non lo sveglia mai → deadlock (handoff 1-slot, LifecycleChurn/StressMix).
                CompleteOnce(reValue, null);
                if (wokenSender is { } snd) snd.TryWake(_waiter.Task.Scheduler);
            }
        }
        return base.TryConsume(out result, out error);
    }

    public override void Cancel()
    {
        // Il waiter resta registrato sul canale (verrà svegliato da un eventuale sender): la
        // consumazione vede Cancelled e il blocco gestisce l'annullamento. Nessuna completion
        // doppia possibile (one-shot TryConsume).
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di un SEND su canale (parità Go `ch <- v`): registra il sender una volta.
/// Al wake (spazio liberato) il sender RI-TENTA il send (il valore va nel buffer ora — il
/// wake non trasferisce il valore, il task risvegliato ritenta, parità Go). If non riesce
/// (un altro sender ha vinto la corsa), resta Pending e il retry ri-parcha.</summary>
public sealed class IlChannelSendAwait : IlAwaitOperationBase
{
    private readonly PCodeTask _task;
    private readonly ChannelInfo _channel;
    private readonly ChannelSenderWaiter _waiter;

    public IlChannelSendAwait(PCodeTask task, ChannelInfo channel, object? value)
    {
        _task = task;
        _channel = channel;
        _waiter = new ChannelSenderWaiter(task, value, null, 0);
    }

    public string DebugState => $"{Status}/g{Generation}/{_channel.DebugSnapshot}";

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        if (Status is IlAwaitStatus.Pending or IlAwaitStatus.Cancelled) return false;
        Status = IlAwaitStatus.Pending;
        if (_channel.TrySendOrPark(_waiter.Value, _waiter, out var wokenReceiver))
        {
            CompleteOnce(null, null);
            if (wokenReceiver is { } rcv) rcv.TryWake(task.Scheduler);
            return true;
        }
        return false;
    }

    public override bool TryConsume(out object? result, out Exception? error)
    {
        // Il wake del sender = spazio liberato: RI-TENTA il send (il valore va nel buffer
        // solo ora). Se non riesce, resta Pending → base non consuma → il retry ri-parcha
        // (generation-safe; il waiter è già stato rimosso dal canale al wake).
        if (Status is IlAwaitStatus.Pending)
        {
            if (_channel.TrySendOrPark(_waiter.Value, _waiter, out var wokenReceiver))
            {
                CompleteOnce(null, null);
                if (wokenReceiver is { } rcv) rcv.TryWake(_task.Scheduler);
            }
        }
        return base.TryConsume(out result, out error);
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di un WaitGroup (parità Go `wg.Wait()`): parcheggia finché il contatore non
/// è zero. TryStart registra il waiter UNA volta (ShouldWait); il wake arriva SOLO da
/// DoneAndWake quando il contatore tocca zero (che pulisce i waiter) — quindi nessun wake
/// prematuro: al resume TryConsume verifica Counter == 0 e completa.</summary>
public sealed class IlWaitGroupAwait : IlAwaitOperationBase
{
    private readonly PCodeTask _task;
    private readonly PCodeWaitGroup _group;
    private bool _registered;

    public IlWaitGroupAwait(PCodeTask task, PCodeWaitGroup group)
    {
        _task = task;
        _group = group;
        Status = IlAwaitStatus.Pending;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        if (Status is IlAwaitStatus.Cancelled) return false;
        if (!_registered)
        {
            if (!_group.ShouldWait(task))
            {
                // Contatore già a zero: pronto subito.
                CompleteOnce(null, null);
                return true;
            }
            _registered = true;
        }
        return false;
    }

    public override bool TryConsume(out object? result, out Exception? error)
    {
        // Wake da DoneAndWake (contatore a zero) → completa. Un wake anomalo con contatore
        // ancora > 0 non consuma (il retry ri-parcha sull'op già registrata).
        if (_group.Counter > 0) return base.TryConsume(out result, out error);
        CompleteOnce(null, null);
        return base.TryConsume(out result, out error);
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            if (_registered) _group.RemoveWaiter(_task);
            Generation++;
        }
    }
}

/// <summary>Await di una COND WAIT (parità Go sync.Cond.Wait): marca la fase "parcheggiato sul
/// cond" — il wake arriva dal SIGNAL/BROADCAST (PCodeCond.TryWait → registrazione → wake). La
/// ri-acquisizione del mutex (fase 2) è gestita dal CondWaitStep, NON qui.</summary>
public sealed class IlCondWaitAwait : IlAwaitOperationBase
{
    public IlCondWaitAwait()
    {
        Status = IlAwaitStatus.Pending;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame) => false;

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di ONCE RUN (parità Go sync.Once.Do): il chiamante che trova il Once in
/// Running parcheggia (registrazione via PCodeOnce.TryBegin → task.BeginWait); al wake
/// (once.Complete) il Once è Done → rientra e salta la procedura. Marker nel frame; lo stato
/// autorevole è il PCodeOnce.</summary>
public sealed class IlOnceAwait : IlAwaitOperationBase
{
    public IlOnceAwait()
    {
        Status = IlAwaitStatus.Pending;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame) => false;

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di una op di RETE/asincrona dietro Task.Run (piano §11.4). Usa il primitivo
/// CONDIVISO con la PCodeVM — <see cref="PCodeTask.BeginWait"/> / <see cref="PCodeWaitRegistration"/>
/// (one-shot TryComplete + wake automatico + generation + cancel via detach), lo stesso di
/// PCodeProcessor.ParkAsync — mai un wake manuale parallelo.</summary>
public sealed class IlNetworkAwait : IlAwaitOperationBase
{
    private readonly PCodeTask _task;
    private readonly Action<Action<object?, Exception?>> _start;
    private PCodeWaitRegistration? _registration;
    private bool _started;

    public IlNetworkAwait(PCodeTask task, Action<Action<object?, Exception?>> start)
    {
        _task = task;
        _start = start;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        if (Status is IlAwaitStatus.Cancelled) return false;
        if (Status == IlAwaitStatus.Created && !_started)
        {
            _started = true;
            Status = IlAwaitStatus.Pending;
            // Primitivo condiviso: BeginWait registra il task (State=Blocked) e ritorna la
            // registration generazionale; TryComplete (one-shot CAS) applica la completion e
            // sveglia il task. Cancel via registration.Cancel.
            var registration = task.BeginWait(PCodeWaitKind.Network, null);
            _registration = registration;
            _start((res, err) => registration.TryComplete(() => CompleteOnce(res, err)));
        }
        return false; // sempre park: la completion arriva via TryComplete → wake
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            _registration?.Cancel();
            Generation++;
        }
    }
}

/// <summary>Await di un WAITKEY (G7): il task si registra in IlRuntime.InkeyWaiters e
/// parcheggia; IlRuntime.QueueInkey lo sveglia; al resume TryConsume prende il tasto dalla
/// coda. Usa frame.Await (non i campi I/O task-global del ponytail).</summary>
public sealed class IlWaitKeyAwait : IlAwaitOperationBase
{
    private readonly PCodeTask _task;
    private bool _started;

    public IlWaitKeyAwait(PCodeTask task)
    {
        _task = task;
        Status = IlAwaitStatus.Pending;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        if (Status is IlAwaitStatus.Cancelled) return false;
        if (!_started)
        {
            _started = true;
            IlRuntime.RegisterInkeyWaiter(task);
        }
        return false;
    }

    public override bool TryConsume(out object? result, out Exception? error)
    {
        if (Status == IlAwaitStatus.Cancelled)
        {
            result = null;
            error = new OperationCanceledException("WAITKEY cancellato.");
            Status = IlAwaitStatus.Consumed;
            return true;
        }
        if (IlRuntime.TryDequeueInkey(out var key))
        {
            result = key;
            error = null;
            Status = IlAwaitStatus.Consumed;
            return true;
        }
        result = null;
        error = null;
        return false;
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            Generation++;
        }
    }
}

/// <summary>Await di un'op GRAFICA (G7): l'op (Gfx.SyncGpm/ScreenGpm/...) viene avviata una
/// volta; la completion (main-thread SDL) completa via PCodeWaitRegistration + wake. Usa
/// frame.Await, non i campi I/O task-global.</summary>
public sealed class IlGraphicsAwait : IlAwaitOperationBase
{
    private readonly PCodeTask _task;
    private readonly Action<Action<object?, Exception?>> _enqueue;
    private PCodeWaitRegistration? _registration;
    private bool _started;

    public IlGraphicsAwait(PCodeTask task, Action<Action<object?, Exception?>> enqueue)
    {
        _task = task;
        _enqueue = enqueue;
    }

    public override bool TryStart(PCodeTask task, IlActivationFrame frame)
    {
        if (Status is IlAwaitStatus.Cancelled) return false;
        if (!_started)
        {
            _started = true;
            Status = IlAwaitStatus.Pending;
            var registration = task.BeginWait(PCodeWaitKind.Graphics, null);
            _registration = registration;
            _enqueue((res, err) => registration.TryComplete(() => CompleteOnce(res, err)));
        }
        return false;
    }

    public override void Cancel()
    {
        if (Status is IlAwaitStatus.Created or IlAwaitStatus.Pending)
        {
            Status = IlAwaitStatus.Cancelled;
            _registration?.Cancel();
            Generation++;
        }
    }
}
