using Basic.Runtime.Gpm;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class PCodeSchedulerWorkStealingTests
{
    [Fact]
    public void GlobalInjection_IsVisibleToAnyProcessor()
    {
        using var scheduler = new PCodeScheduler();
        scheduler.ConfigureProcessors(2);
        var task = scheduler.CreateTask();

        scheduler.Schedule(task);

        Assert.Same(task, scheduler.FindRunnableTask(CancellationToken.None, 1));
        Assert.Equal(0, scheduler.GlobalQueueDepth);
    }

    [Fact]
    public void Wake_UsesLastProcessorRunNextSlot()
    {
        using var scheduler = new PCodeScheduler();
        scheduler.ConfigureProcessors(2);
        var task = scheduler.CreateTask();
        scheduler.Schedule(task);
        Assert.Same(task, scheduler.FindRunnableTask(CancellationToken.None, 1));
        scheduler.ReleaseExecuting(task);
        task.State = PCodeTaskState.Blocked;

        scheduler.WakeTask(task);

        Assert.Same(task, scheduler.FindRunnableTask(CancellationToken.None, 1));
        Assert.Equal(0, scheduler.LocalQueueDepth);
    }

    [Fact]
    public void IdleProcessor_StealsFromOppositeEndOfVictimQueue()
    {
        using var scheduler = new PCodeScheduler();
        scheduler.ConfigureProcessors(2);
        var first = scheduler.CreateTask();
        var second = scheduler.CreateTask();

        scheduler.YieldTask(first, 0);
        scheduler.YieldTask(second, 0);

        Assert.Same(first, scheduler.FindRunnableTask(CancellationToken.None, 1));
        Assert.Equal(1, scheduler.StealCount);
        Assert.Same(second, scheduler.FindRunnableTask(CancellationToken.None, 0));
    }

    [Fact]
    public void Scheduler_RecordsRunnableAndWaitMetrics()
    {
        using var scheduler = new PCodeScheduler();
        scheduler.ConfigureProcessors(1);
        var runnable = scheduler.CreateTask();
        scheduler.Schedule(runnable);

        Assert.Same(runnable, scheduler.FindRunnableTask(CancellationToken.None, 0));
        Assert.Equal(1, scheduler.DispatchCount);
        Assert.True(scheduler.AverageRunnableDelay >= TimeSpan.Zero);
        Assert.True(scheduler.MaximumRunnableDelay >= TimeSpan.Zero);

        var completedTask = scheduler.CreateTask();
        var completedWait = completedTask.BeginWait(PCodeWaitKind.Timer);
        Assert.True(completedWait.TryComplete(() => { }));

        var cancelledTask = scheduler.CreateTask();
        cancelledTask.BeginWait(PCodeWaitKind.Timer);
        cancelledTask.CancelPendingWait();

        var metrics = scheduler.GetWaitMetrics(PCodeWaitKind.Timer);
        Assert.Equal(1, metrics.Completed);
        Assert.Equal(1, metrics.Cancelled);
        Assert.True(metrics.TotalDuration >= TimeSpan.Zero);
        Assert.True(metrics.MaximumDuration >= TimeSpan.Zero);
    }

    [Fact]
    public void CompletionApplyFailure_StillWakesTaskExactlyOnce()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        var wait = task.BeginWait(PCodeWaitKind.External);
        var expected = new InvalidOperationException("completion conversion failed");

        Assert.True(wait.TryComplete(() => throw expected));
        Assert.False(wait.TryComplete(() => { }));

        Assert.Same(expected, task.PendingException);
        Assert.Same(task, scheduler.FindRunnableTask(CancellationToken.None));
        Assert.Equal(0, scheduler.GlobalQueueDepth);
    }
}
