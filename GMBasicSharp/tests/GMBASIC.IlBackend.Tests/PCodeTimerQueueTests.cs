using Basic.Runtime.Gpm;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class PCodeTimerQueueTests
{
    [Fact]
    public void TimerHeap_WakesOnlyAtDeadline()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        task.State = PCodeTaskState.Blocked;
        task.SleepUntilMs = 100;

        scheduler.WakeExpiredSleepers(99);
        Assert.Equal(PCodeTaskState.Blocked, task.State);
        Assert.Equal(100, scheduler.EarliestSleepDeadline());

        scheduler.WakeExpiredSleepers(100);
        Assert.Equal(PCodeTaskState.Runnable, task.State);
        Assert.Equal(0, task.SleepUntilMs);
        Assert.Null(scheduler.EarliestSleepDeadline());
    }

    [Fact]
    public void RescheduledTimer_InvalidatesOlderGeneration()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        task.State = PCodeTaskState.Blocked;
        task.SleepUntilMs = 100;
        task.SleepUntilMs = 200;

        scheduler.WakeExpiredSleepers(100);
        Assert.Equal(PCodeTaskState.Blocked, task.State);
        Assert.Equal(200, scheduler.EarliestSleepDeadline());

        scheduler.WakeExpiredSleepers(200);
        Assert.Equal(PCodeTaskState.Runnable, task.State);
    }

    [Fact]
    public void ReclaimedTask_InvalidatesPendingTimer()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        task.State = PCodeTaskState.Blocked;
        task.SleepUntilMs = 100;
        task.State = PCodeTaskState.Finished;

        scheduler.ReclaimFinishedTask(task);

        Assert.Null(scheduler.EarliestSleepDeadline());
        scheduler.WakeExpiredSleepers(100);
        Assert.Equal(PCodeTaskState.Finished, task.State);
    }

    [Fact]
    public void IslandSleep_UsesGenerationalTimerAndResumesOnce()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        task.Frame = new PCodeCallFrame();

        Assert.Equal(IslandResult.Blocked, IslandRuntime.Sleep(task, 100));
        var deadline = task.SleepUntilMs;
        Assert.True(deadline > 0);
        Assert.Equal(PCodeTaskState.Blocked, task.State);

        scheduler.WakeExpiredSleepers(deadline);
        var resumed = scheduler.FindRunnableTask(CancellationToken.None);
        Assert.Same(task, resumed);
        scheduler.ReleaseExecuting(task);

        Assert.Equal(IslandResult.Continue, IslandRuntime.Sleep(task, 100));
        Assert.Equal(0, task.Frame.IslandSleepDeadlineMs);
        Assert.Equal(0, scheduler.GlobalQueueDepth);
    }

    [Fact]
    public void IslandSleep_CancelWinsAgainstLateDeadline()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();
        task.Frame = new PCodeCallFrame();

        Assert.Equal(IslandResult.Blocked, IslandRuntime.Sleep(task, 100));
        var deadline = task.SleepUntilMs;
        scheduler.RequestCancellation(task);
        scheduler.WakeExpiredSleepers(deadline);

        Assert.Same(task, scheduler.FindRunnableTask(CancellationToken.None));
        Assert.Equal(0, scheduler.GlobalQueueDepth);
    }
}
