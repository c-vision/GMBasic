// G3 — Parità Go 1:1: CLOSE (close(ch)), RECEIVE con OK flag (v, ok := <-ch) e CAPACITY
// (make(chan T, n)) nella GPM di ILBackend. La PCodeVM li ha; qui si verificano i runtime
// helper che la GPM IL usa (IlGpmRuntime.TrySendPark/TryReceivePark/CloseChannel) e la
// semantica del ChannelInfo condiviso.

using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class GpmV2ChannelParityTests
{
    private static PCodeTask NewTask(PCodeScheduler scheduler) =>
        new PCodeTask { Scheduler = scheduler, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

    [Fact]
    public void Capacity_BuffersUpToN_ThenParksSender()
    {
        // make(chan T, n): con capacità 2, i primi 2 SEND riescono subito (buffer); il 3° parcheggia.
        var scheduler = new PCodeScheduler();
        var ch = new ChannelInfo(2);
        var task = NewTask(scheduler);

        Assert.True(IlGpmRuntime.TrySendPark(task, ch, 1.0));
        Assert.True(IlGpmRuntime.TrySendPark(task, ch, 2.0));
        Assert.Equal(2, ch.Count);
        Assert.False(IlGpmRuntime.TrySendPark(task, ch, 3.0)); // buffer pieno → parcheggiato
        Assert.Equal(1, ch.WaitingSenderCount);
        Assert.Equal(PCodeTaskState.Blocked, task.State);
    }

    [Fact]
    public void Close_WakesReceivers_WithOkFalse()
    {
        // close(ch): i receiver in attesa si svegliano con ok=false (valore null).
        var scheduler = new PCodeScheduler();
        var ch = new ChannelInfo(2);
        var task = NewTask(scheduler);
        scheduler.Schedule(task);

        // Parcheggia un receiver (buffer vuoto).
        Assert.False(IlGpmRuntime.TryReceivePark(task, ch, out _));
        Assert.Equal(1, ch.WaitingReceiverCount);

        // Close: sveglia il receiver → il retry riceve null (ok=false) senza parcheggiare.
        IlGpmRuntime.CloseChannel(ch);
        Assert.True(ch.IsClosed);
        Assert.True(IlGpmRuntime.TryReceivePark(task, ch, out var value));
        Assert.Null(value); // ok=false per Go: valore nullo su canale chiuso
    }

    [Fact]
    public void Close_DoubleClose_Throws()
    {
        var ch = new ChannelInfo();
        IlGpmRuntime.CloseChannel(ch);
        Assert.Throws<System.InvalidOperationException>(() => IlGpmRuntime.CloseChannel(ch));
    }

    [Fact]
    public void Close_FlushesBufferedValues_BeforeOkFalse()
    {
        // Go: i valori buffered restano ricevibili dopo la chiusura; solo a buffer vuoto ok=false.
        var scheduler = new PCodeScheduler();
        var ch = new ChannelInfo(2);
        var task = NewTask(scheduler);

        Assert.True(IlGpmRuntime.TrySendPark(task, ch, 42.0));
        IlGpmRuntime.CloseChannel(ch);
        Assert.True(IlGpmRuntime.TryReceivePark(task, ch, out var v1));
        Assert.Equal(42.0, v1);
        Assert.True(IlGpmRuntime.TryReceivePark(task, ch, out var v2));
        Assert.Null(v2); // buffer vuoto e chiuso → ok=false
    }
}

[Collection("GpmRuntimeState")]
public class GpmV2MutexRuntimeTests
{
    [Fact]
    public void Mutex_LockParks_WakeOnUnlock_HandoffToWaiter()
    {
        // Go sync.Mutex: Unlock passa la proprietà al waiter svegliato (HANDOFF) — il risvegliato
        // NON deve ri-ritentare TryLock (il mutex è già ri-assegnato a lui); un TERZO task resta
        // in attesa finché il risvegliato non sblocca.
        var sched = new PCodeScheduler();
        var mu = new PCodeMutex();
        var t1 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var t2 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

        Assert.True(mu.TryLock(t1));
        Assert.False(mu.TryLock(t2));           // t2 parcheggiato
        Assert.Equal(PCodeTaskState.Blocked, t2.State);

        mu.Unlock(sched);                        // handoff a t2
        Assert.Equal(PCodeTaskState.Runnable, t2.State);
        Assert.True(mu.IsHeld);                  // ri-assegnato a t2

        var t3 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        Assert.False(mu.TryLock(t3));            // t2 possiede il mutex

        mu.Unlock(sched);                        // t2 rilascia → handoff a t3 (woken, owner)
        Assert.Equal(PCodeTaskState.Runnable, t3.State);
        Assert.True(mu.IsHeld);                  // t3 ora possiede il mutex
    }

    [Fact]
    public void Mutex_WokenWaiter_ReExecutingTryLock_GrantedOnHandoff()
    {
        // Path PCode OP_LOCK: al risveglio il task ri-esegue TryLock (Ip--). L'handoff di
        // Unlock ha già ri-assegnato il mutex a questo task (_owner == task) → TryLock deve
        // riconoscerlo e concedere, NON ri-parcheggiare (ri-parcheggio = deadlock: il mutex
        // non ha più un detentore che sblocchi).
        var sched = new PCodeScheduler();
        var mu = new PCodeMutex();
        var t1 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var t2 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

        Assert.True(mu.TryLock(t1));
        Assert.False(mu.TryLock(t2));           // t2 parcheggiato
        mu.Unlock(sched);                        // handoff a t2 (t2 Runnable, owner)
        Assert.Equal(PCodeTaskState.Runnable, t2.State);
        Assert.True(mu.IsHeld);

        Assert.True(mu.TryLock(t2));             // ri-esecuzione di OP_LOCK al wake → concesso
        Assert.True(mu.IsHeld);                  // t2 ora possiede (il grant non rilascia)
        Assert.False(mu.TryLock(t1));            // t1 è solo un waiter, non il destinatario
    }

    [Fact]
    public void RwMutex_ReadersShared_WriterExclusive_WriterPreference()
    {
        // Parità Go sync.RWMutex: più lettori condividono il lock; il writer è esclusivo;
        // writer-preference (un writer in attesa blocca i nuovi reader).
        var sched = new PCodeScheduler();
        var rw = new PCodeRwMutex();
        var t1 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var t2 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var w1 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

        Assert.True(rw.TryRLock(t1));            // lettore 1 entra
        Assert.True(rw.TryRLock(t2));            // lettore 2 entra (condiviso)
        Assert.Equal(2, rw.ReaderCount);
        Assert.False(rw.TryLock(w1));            // writer bloccato dai lettori
        Assert.False(rw.TryRLock(t1));           // writer-preference: nuovo reader bloccato dal writer in attesa

        rw.RUnlock(sched);                       // reader 1 esce
        rw.RUnlock(sched);                       // reader 2 esce → ReaderCount 0, handoff a w1
        Assert.Equal(0, rw.ReaderCount);
        Assert.True(rw.IsWriterActive);          // w1 è il writer in handoff (ri-assegnato)

        Assert.True(rw.TryLock(w1));             // w1 ri-esegue LOCK al wake → handoff grant
        Assert.True(rw.IsWriterActive);
        Assert.False(rw.TryRLock(t1));           // reader bloccato dal writer attivo
        rw.Unlock(sched);
        Assert.False(rw.IsWriterActive);
        Assert.True(rw.TryRLock(t1));            // reader rientra dopo il writer
    }

    [Fact]
    public void Once_FirstCallerRuns_ConcurrentWait_ThenDoneSkips()
    {
        // Parità Go sync.Once: il primo TryBegin → Started (esegue); i concorrenti → Parked;
        // dopo Complete tutti i waiter ri-tentano e trovano Done (proseguono senza ri-eseguire).
        var sched = new PCodeScheduler();
        var once = new PCodeOnce();
        var t1 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var t2 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var t3 = new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

        Assert.Equal(PCodeOnce.OnceBeginResult.Started, once.TryBegin(t1));
        Assert.Equal(PCodeOnce.OnceBeginResult.Parked, once.TryBegin(t2));  // concorrente
        Assert.Equal(PCodeOnce.OnceBeginResult.Parked, once.TryBegin(t3));
        Assert.False(once.IsDone);

        once.Complete(sched);
        Assert.True(once.IsDone);
        Assert.Equal(PCodeOnce.OnceBeginResult.Done, once.TryBegin(t2));  // wake → riprova → Done
        Assert.Equal(PCodeOnce.OnceBeginResult.Done, once.TryBegin(t3));
        Assert.Equal(PCodeOnce.OnceBeginResult.Done, once.TryBegin(t1));
    }
}

[Collection("GpmRuntimeState")]
public class GpmV2SelectRuntimeTests
{
    private static PCodeTask NewTask(PCodeScheduler sched) =>
        new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

    [Fact]
    public void Select_ReceiveWins_WhenValueArrives()
    {
        // GO SELECT: un SEND concorrente fa vincere il case RECEIVE (idx 0).
        var sched = new PCodeScheduler();
        var ch = new ChannelInfo();
        var task = NewTask(sched);
        Assert.True(IlGpmRuntime.TrySendPark(task, ch, 42.0)); // valore in buffer

        var ctx = new SelectContext();
        Assert.True(IlGpmRuntime.TrySelectSetup(task, new[] { ch, null }, new[] { false, true }, new[] { false, false },
            new[] { (object?)null, null }, new[] { 0L, 1000L }, ctx, out var idx, out var val, out _));
        Assert.Equal(0, idx);
        Assert.Equal(42.0, val);
    }

    [Fact]
    public void Select_TimeoutFires_WhenNoSend()
    {
        // GO SELECT (parità Go case <-time.After(d)): nessun valore → scatta il TIMEOUT.
        var sched = new PCodeScheduler();
        var ch = new ChannelInfo();
        var task = NewTask(sched);

        var ctx = new SelectContext();
        Assert.False(IlGpmRuntime.TrySelectSetup(task, new[] { ch, null }, new[] { false, true }, new[] { false, false },
            new[] { (object?)null, null }, new[] { 0L, 5L }, ctx, out _, out _, out _));
        Assert.Equal(PCodeTaskState.Blocked, task.State);

        Thread.Sleep(15); // scade il timer
        sched.WakeExpiredSleepers(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());

        // Retry: il task ri-esegue il select block → il TIMEOUT è pronto (deadline persistita
        // sul ctx condiviso dal frame).
        Assert.True(IlGpmRuntime.TrySelectSetup(task, new[] { ch, null }, new[] { false, true }, new[] { false, false },
            new[] { (object?)null, null }, new[] { 0L, 5L }, ctx, out var idx, out _, out _));
        Assert.Equal(1, idx);
    }
}

[Collection("GpmRuntimeState")]
public class GpmV2SelectNilChannelTests
{
    private static PCodeTask NewTask(PCodeScheduler sched) =>
        new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

    [Fact]
    public void Select_NilChannel_IsDisabled_TimeoutStillFires()
    {
        // Parità Go: un case su canale NIL non parcheggia mai; vince il TIMEOUT.
        var sched = new PCodeScheduler();
        var task = NewTask(sched);
        var nilCh = (ChannelInfo?)null;
        var ctx = new SelectContext();
        // case 0 = canale NIL (disabilitato), case 1 = TIMEOUT 5ms.
        Assert.False(IlGpmRuntime.TrySelectSetup(task, new[] { nilCh, null }, new[] { false, true }, new[] { false, false },
            new[] { (object?)null, null }, new[] { 0L, 5L }, ctx, out _, out _, out _));
        Assert.Equal(PCodeTaskState.Blocked, task.State);
        Thread.Sleep(15);
        sched.WakeExpiredSleepers(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        Assert.True(IlGpmRuntime.TrySelectSetup(task, new[] { nilCh, null }, new[] { false, true }, new[] { false, false },
            new[] { (object?)null, null }, new[] { 0L, 5L }, ctx, out var idx, out _, out _));
        Assert.Equal(1, idx); // il TIMEOUT vince, il case NIL è disabilitato
    }
}

[Collection("GpmRuntimeState")]
public class GpmV2SelectSendCaseTests
{
    private static PCodeTask NewTask(PCodeScheduler sched) =>
        new PCodeTask { Scheduler = sched, Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

    [Fact]
    public void Select_SendCase_Wins_WhenReceiverReady()
    {
        // GO SELECT con un case SEND (parità Go select { case ch <- v: }): un receiver in
        // attesa rende il SEND pronto.
        var sched = new PCodeScheduler();
        var ch = new ChannelInfo();
        var task = NewTask(sched);
        var recv = NewTask(sched);
        // parcheggia un receiver sul canale (buffer vuoto).
        Assert.False(IlGpmRuntime.TryReceivePark(recv, ch, out _));

        var ctx = new SelectContext();
        Assert.True(IlGpmRuntime.TrySelectSetup(task, new[] { ch }, new[] { false }, new[] { true },
            new[] { (object?)42.0 }, new[] { 0L }, ctx, out var idx, out _, out _));
        Assert.Equal(0, idx);
    }
}
