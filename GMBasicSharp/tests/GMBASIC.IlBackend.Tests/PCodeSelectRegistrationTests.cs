using Basic.Runtime.Gpm;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class PCodeSelectRegistrationTests
{
    [Fact]
    public void CancelledOrdinarySender_IsRemovedAndCannotWinLater()
    {
        var channel = new ChannelInfo(capacity: 1);
        Assert.True(channel.TrySend(1.0));
        var task = new PCodeTask();

        Assert.False(channel.TrySendOrPark(
            2.0, new ChannelSenderWaiter(task, 2.0, null, 0), out _));
        Assert.Equal(1, channel.WaitingSenderCount);

        task.CancelPendingWait();

        Assert.Equal(0, channel.WaitingSenderCount);
        Assert.True(channel.TryReceive(out var value));
        Assert.Equal(1.0, value);
        Assert.False(channel.TryDequeueSender(out _));
    }

    [Fact]
    public void CancelledOrdinaryReceiver_IsRemovedAndCannotWinLater()
    {
        var channel = new ChannelInfo();
        var task = new PCodeTask();

        Assert.False(channel.TryReceiveOrPark(
            new ChannelReceiverWaiter(task, null, 0), out _, out _));
        Assert.Equal(1, channel.WaitingReceiverCount);

        task.CancelPendingWait();

        Assert.Equal(0, channel.WaitingReceiverCount);
        Assert.True(channel.TrySend(3.0));
        Assert.False(channel.TryDequeueReceiver(out _));
    }

    [Fact]
    public void WinningReceiveCase_DetachesAllLosingRegistrations()
    {
        var channels = new[] { new ChannelInfo(), new ChannelInfo(), new ChannelInfo() };
        var task = new PCodeTask();
        var context = new SelectContext();

        for (var caseIndex = 0; caseIndex < channels.Length; caseIndex++)
        {
            context.RegisterChannel(channels[caseIndex]);
            Assert.False(channels[caseIndex].TryReceiveOrPark(
                new ChannelReceiverWaiter(task, context, caseIndex), out _, out _));
        }

        Assert.True(channels[1].TrySendOrPark(
            42.0, new ChannelSenderWaiter(new PCodeTask(), 42.0, null, 0), out var wokenReceiver));

        Assert.Same(task, wokenReceiver?.Task);
        Assert.Equal(1, context.WinningCaseIndex);
        Assert.All(channels, channel => Assert.Equal(0, channel.WaitingReceiverCount));
    }

    [Fact]
    public void ConcurrentReadyCases_WakeSelectExactlyOnce()
    {
        var channels = new[] { new ChannelInfo(), new ChannelInfo() };
        var task = new PCodeTask();
        var context = new SelectContext();
        for (var caseIndex = 0; caseIndex < channels.Length; caseIndex++)
        {
            context.RegisterChannel(channels[caseIndex]);
            Assert.False(channels[caseIndex].TryReceiveOrPark(
                new ChannelReceiverWaiter(task, context, caseIndex), out _, out _));
        }

        var wakeCount = 0;
        Parallel.For(0, channels.Length, i =>
        {
            if (channels[i].TrySendOrPark(
                    (double)i, new ChannelSenderWaiter(new PCodeTask(), (double)i, null, 0), out var receiver) &&
                receiver is not null)
                Interlocked.Increment(ref wakeCount);
        });

        Assert.Equal(1, wakeCount);
        Assert.InRange(context.WinningCaseIndex, 0, 1);
        Assert.All(channels, channel => Assert.Equal(0, channel.WaitingReceiverCount));
    }

    [Fact]
    public void CancelledSelect_DetachesEveryRegistration()
    {
        var channels = new[] { new ChannelInfo(), new ChannelInfo(), new ChannelInfo() };
        var task = new PCodeTask();
        var context = new SelectContext();
        for (var caseIndex = 0; caseIndex < channels.Length; caseIndex++)
        {
            context.RegisterChannel(channels[caseIndex]);
            Assert.False(channels[caseIndex].TryReceiveOrPark(
                new ChannelReceiverWaiter(task, context, caseIndex), out _, out _));
        }

        context.Cancel();

        Assert.Equal(-2, context.WinningCaseIndex);
        Assert.All(channels, channel => Assert.Equal(0, channel.WaitingReceiverCount));
    }

    [Fact]
    public void SharedSelectRegistration_CancelDetachesAllCasesAndRejectsLateWinner()
    {
        var channels = new[] { new ChannelInfo(), new ChannelInfo(), new ChannelInfo() };
        var task = new PCodeTask();
        var context = new SelectContext();
        var registration = task.BeginWait(PCodeWaitKind.Select, context.Cancel);

        for (var caseIndex = 0; caseIndex < channels.Length; caseIndex++)
        {
            context.RegisterChannel(channels[caseIndex]);
            Assert.False(channels[caseIndex].TryReceiveOrPark(
                new ChannelReceiverWaiter(task, context, caseIndex, registration), out _, out _));
        }

        task.CancelPendingWait();

        Assert.False(registration.IsPending);
        Assert.Equal(-2, context.WinningCaseIndex);
        Assert.All(channels, channel => Assert.Equal(0, channel.WaitingReceiverCount));
        Assert.True(channels[1].TrySend(42.0));
        Assert.False(channels[1].TryDequeueReceiver(out _));
    }

    [Fact]
    public void ConcurrentReadySelectSends_PublishExactlyOneValue()
    {
        var channels = new[] { new ChannelInfo(capacity: 1), new ChannelInfo(capacity: 1) };
        var task = new PCodeTask();
        var context = new SelectContext();
        var registration = task.BeginWait(PCodeWaitKind.Select, context.Cancel);
        var successes = 0;

        Parallel.For(0, channels.Length, caseIndex =>
        {
            if (channels[caseIndex].TrySendOrPark(caseIndex,
                    new ChannelSenderWaiter(task, caseIndex, context, caseIndex, registration), out _))
                Interlocked.Increment(ref successes);
        });

        Assert.Equal(1, successes);
        Assert.Equal(1, channels.Sum(channel => channel.Count));
        Assert.InRange(context.WinningCaseIndex, 0, 1);
    }

    [Fact]
    public void ConcurrentReadySelectReceives_ConsumeExactlyOneValue()
    {
        var channels = new[] { new ChannelInfo(capacity: 1), new ChannelInfo(capacity: 1) };
        Assert.All(channels, channel => Assert.True(channel.TrySend(7.0)));
        var task = new PCodeTask();
        var context = new SelectContext();
        var registration = task.BeginWait(PCodeWaitKind.Select, context.Cancel);
        var successes = 0;

        Parallel.For(0, channels.Length, caseIndex =>
        {
            if (channels[caseIndex].TryReceiveOrPark(
                    new ChannelReceiverWaiter(task, context, caseIndex, registration), out _, out _))
                Interlocked.Increment(ref successes);
        });

        Assert.Equal(1, successes);
        Assert.Equal(1, channels.Sum(channel => channel.Count));
        Assert.InRange(context.WinningCaseIndex, 0, 1);
    }
}
