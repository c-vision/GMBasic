// G5 — Await operation unificata (piano §9): unit test del protocollo IlAwaitOperation.
// Copre: il ciclo Created/Pending/Completed/Consumed, la consumazione una-sola-volta, il
// cancel (Cancelled + OperationCanceledException al consume), il timer await (wake
// prematuro non consuma; solo deadline passata consuma) e l'helper SleepStep del runtime.

using System;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class IlAwaitOperationsTests
{
    private static IlActivationFrame NewFrame() => new IlActivationFrame { ProcedureId = 1, Locals = new object?[8] };

    [Fact]
    public void TimerAwait_ConsumeOnlyAfterDeadline_IsOneShot()
    {
        // deadline passata → consuma; un secondo TryConsume fallisce (Consumed, one-shot).
        var op = new IlTimerAwait(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() - 1);
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        op.TryStart(task, frame);
        Assert.Equal(IlAwaitStatus.Pending, op.Status);

        Assert.True(op.TryConsume(out _, out var err), "deadline passata deve consumare");
        Assert.Null(err);
        Assert.Equal(IlAwaitStatus.Consumed, op.Status);
        Assert.False(op.TryConsume(out _, out _), "secondo consume deve fallire (one-shot)");
    }

    [Fact]
    public void TimerAwait_SpuriousWake_DoesNotConsume()
    {
        // Wake prematuro (deadline futura): TryConsume NON consuma → il retry del blocco
        // ri-parcha sulla stessa op (generation-safe, il wake duplicato è innocuo).
        var op = new IlTimerAwait(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + 60_000);
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        op.TryStart(task, frame);
        Assert.False(op.TryConsume(out _, out _), "wake prematuro non deve consumare");
        Assert.Equal(IlAwaitStatus.Pending, op.Status);
    }

    [Fact]
    public void TimerAwait_Cancel_ConsumeReturnsCancelled()
    {
        var op = new IlTimerAwait(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + 60_000);
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        op.TryStart(task, frame);
        op.Cancel();
        Assert.Equal(IlAwaitStatus.Cancelled, op.Status);
        Assert.True(op.TryConsume(out _, out var err));
        Assert.IsType<OperationCanceledException>(err);
        Assert.Equal(IlAwaitStatus.Consumed, op.Status);
    }

    [Fact]
    public void SleepStep_ZeroDuration_ContinuesImmediately()
    {
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        Assert.True(IlGpmRuntime.SleepStep(task, frame, 0));
        Assert.True(IlGpmRuntime.SleepStep(task, frame, -1));
        Assert.Null(frame.Await);
    }

    [Fact]
    public void SleepStep_FirstPark_SetsTimerAwaitAndParks()
    {
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        Assert.False(IlGpmRuntime.SleepStep(task, frame, 50), "deve parcheggiare");
        var op = Assert.IsType<IlTimerAwait>(frame.Await);
        Assert.True(op.DeadlineMs > DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        Assert.True(task.SleepUntilMs > 0, "SleepUntilMs impostato dal TryStart");
    }

    [Fact]
    public void ChannelReceiveAwait_ImmediateValue_ConsumesWithoutReDequeue()
    {
        // TryStart completa subito (valore nel buffer); TryConsume consuma quel valore SENZA
        // ri-dequeuare un secondo valore (il bug del valore perso su canale con buffer > 1).
        var ch = new ChannelInfo(2);
        ch.TrySend(1);
        ch.TrySend(2);
        var task = new PCodeScheduler().CreateTask(0);
        var op = new IlChannelReceiveAwait(task, ch);
        Assert.True(op.TryStart(task, NewFrame()), "buffer non vuoto → ricezione immediata");
        Assert.True(op.TryConsume(out var value, out _));
        Assert.Equal(1.0, Convert.ToDouble(value));
        Assert.Equal(1, ch.Count); // il secondo valore (2) NON deve essere stato consumato
    }

    [Fact]
    public void ChannelSendAwait_ParksWhenFull_ReTriesOnWake()
    {
        // Buffer pieno → TryStart parcheggia (Pending). Al wake (spazio liberato) TryConsume
        // RI-TENTA il send: il valore va nel buffer SOLO ora (mai perso).
        var ch = new ChannelInfo(1);
        ch.TrySend(7); // pieno
        var task = new PCodeScheduler().CreateTask(0);
        var op = new IlChannelSendAwait(task, ch, 42);
        Assert.False(op.TryStart(task, NewFrame()), "buffer pieno → parcheggiato");
        Assert.Equal(IlAwaitStatus.Pending, op.Status);
        ch.TryReceive(out _); // spazio liberato
        Assert.True(op.TryConsume(out _, out _), "al wake ri-tenta e completa");
        Assert.Equal(1, ch.Count);
        ch.TryReceive(out var delivered);
        Assert.Equal(42.0, Convert.ToDouble(delivered));
    }

    [Fact]
    public void WaitGroupAwait_CompletesWhenCounterHitsZero()
    {
        var wg = new PCodeWaitGroup();
        wg.Add(2);
        var task = new PCodeScheduler().CreateTask(0);
        var op = new IlWaitGroupAwait(task, wg);
        Assert.False(op.TryStart(task, NewFrame()), "contatore > 0 → parcheggiato");
        Assert.False(op.TryConsume(out _, out _), "Done parziale non consuma");
        wg.DoneAndWake(null);
        wg.DoneAndWake(null);
        Assert.True(op.TryConsume(out _, out _), "contatore a zero → consuma");
    }

    [Fact]
    public void NetworkAwait_StartsOnce_CompletesAndConsumes()
    {
        // §11.4: l'op Task.Run viene avviata UNA volta; la completion completa + wake;
        // al resume TryConsume consegna il risultato (una-sola-volta).
        var started = 0;
        var task = new PCodeScheduler().CreateTask(0);
        Action<Action<object?, Exception?>> start = done =>
        {
            started++;
            done("risultato", null);
        };
        var op = new IlNetworkAwait(task, start);
        var frame = NewFrame();
        Assert.False(op.TryStart(task, frame), "la completion arriva via wake → sempre park");
        Assert.Equal(1, started);
        Assert.False(op.TryStart(task, frame), "TryStart idempotente: NON ri-avvia");
        Assert.Equal(1, started);
        Assert.True(op.TryConsume(out var value, out _), "completata → consuma");
        Assert.Equal("risultato", (string)value!);
        Assert.False(op.TryConsume(out _, out _), "one-shot");
    }

    [Fact]
    public void NetworkAwait_Cancel_ConsumeReturnsCancelled()
    {
        var task = new PCodeScheduler().CreateTask(0);
        var op = new IlNetworkAwait(task, _ => { });
        op.TryStart(task, NewFrame());
        op.Cancel();
        Assert.True(op.TryConsume(out _, out var err));
        Assert.IsType<OperationCanceledException>(err);
    }

    [Fact]
    public void NetworkStep_FirstEntryParks_ReEntryConsumes()
    {
        var task = new PCodeScheduler().CreateTask(0);
        var frame = NewFrame();
        var started = 0;
        Action<Action<object?, Exception?>> start = done =>
        {
            started++;
            done(42.0, null);
        };
        // primo ingresso: avvia + park
        Assert.False(IlGpmRuntime.NetworkStep(task, frame, start, out var v1));
        Assert.Equal(1, started);
        Assert.Null(v1);
        // re-entry (wake): consuma il risultato
        Assert.True(IlGpmRuntime.NetworkStep(task, frame, start, out var v2));
        Assert.Equal(42.0, Convert.ToDouble(v2));
        Assert.True(started == 1, "l'op non va ri-avviata al re-entry");
    }
}
