using Basic.Runtime.Gpm;
using Xunit;

namespace GMBASIC.IlBackend.Tests;

public sealed class PCodeFutureStateTests
{
    [Fact]
    public void External_wait_completion_and_cancel_have_a_single_winner()
    {
        var task = new PCodeTask();
        var completionCount = 0;
        var wait = task.BeginWait(PCodeWaitKind.Graphics);

        Assert.True(wait.TryComplete(() => completionCount++));
        Assert.False(wait.TryComplete(() => completionCount++));
        Assert.False(wait.Cancel());
        Assert.Equal(1, completionCount);
    }

    [Fact]
    public void Cancelled_external_wait_rejects_a_late_callback()
    {
        var task = new PCodeTask();
        var completionCount = 0;
        var wait = task.BeginWait(PCodeWaitKind.Graphics);

        task.CancelPendingWait();

        Assert.False(wait.TryComplete(() => completionCount++));
        Assert.Equal(0, completionCount);
    }

    [Fact]
    public void Cancel_is_a_distinct_terminal_state_and_late_completions_are_ignored()
    {
        var future = new PCodeFutureValue();

        future.Cancel(null);
        future.Resolve(42.0, null);
        future.Fail(new InvalidOperationException("late"), null);

        Assert.True(future.IsResolved);
        Assert.True(future.IsCancelled);
        Assert.Equal(PCodeFutureState.Cancelled, future.State);
        Assert.Null(future.Result);
        Assert.IsType<OperationCanceledException>(future.FailureException);
    }

    [Fact]
    public void Value_and_error_remain_distinct_terminal_states()
    {
        var value = new PCodeFutureValue();
        value.Resolve("ok", null);

        var error = new PCodeFutureValue();
        error.Fail(new InvalidOperationException("boom"), null);

        Assert.Equal(PCodeFutureState.Value, value.State);
        Assert.Equal("ok", value.Result);
        Assert.Equal(PCodeFutureState.Error, error.State);
        Assert.Equal("boom", error.FailureException?.Message);
    }

    [Fact]
    public void Scheduler_cancels_detached_external_operations_once()
    {
        using var scheduler = new PCodeScheduler();
        var cancellationCount = 0;
        using var registration = scheduler.RegisterExternalOperation(() => cancellationCount++);

        scheduler.RequestCancellation();
        scheduler.RequestCancellation();

        Assert.Equal(1, cancellationCount);
        Assert.Equal(0, scheduler.PendingExternalOperationCount);
    }

    [Fact]
    public void Completed_external_operation_unregisters_without_cancellation()
    {
        using var scheduler = new PCodeScheduler();
        var cancellationCount = 0;
        var registration = scheduler.RegisterExternalOperation(() => cancellationCount++);

        registration.Dispose();
        scheduler.RequestCancellation();

        Assert.Equal(0, cancellationCount);
        Assert.Equal(0, scheduler.PendingExternalOperationCount);
    }

    [Fact]
    public void Nursery_cancellation_reaches_descendants_but_not_independent_roots()
    {
        using var scheduler = new PCodeScheduler();
        var root = scheduler.CreateTask();
        var child = scheduler.CreateTask(parent: root);
        var grandchild = scheduler.CreateTask(parent: child);
        var independent = scheduler.CreateTask();
        var ownedExternalCancelled = 0;
        var independentExternalCancelled = 0;
        using var owned = scheduler.RegisterExternalOperation(() => ownedExternalCancelled++, grandchild);
        using var other = scheduler.RegisterExternalOperation(() => independentExternalCancelled++, independent);

        scheduler.RequestCancellation(root);

        Assert.True(root.CancellationRequested);
        Assert.True(child.CancellationRequested);
        Assert.True(grandchild.CancellationRequested);
        Assert.False(independent.CancellationRequested);
        Assert.Equal(1, ownedExternalCancelled);
        Assert.Equal(0, independentExternalCancelled);
        Assert.Equal(1, scheduler.PendingExternalOperationCount);
    }

    [Fact]
    public void Nursery_cancels_io_owned_by_an_already_reclaimed_child()
    {
        using var scheduler = new PCodeScheduler();
        var root = scheduler.CreateTask();
        var child = scheduler.CreateTask(parent: root);
        var cancellationCount = 0;
        using var operation = scheduler.RegisterExternalOperation(() => cancellationCount++, child);
        child.State = PCodeTaskState.Finished;
        scheduler.ReclaimFinishedTask(child);

        scheduler.RequestCancellation(root);

        Assert.Equal(1, cancellationCount);
        Assert.Equal(0, scheduler.PendingExternalOperationCount);
    }

    [Fact]
    public void Child_created_during_parent_cancellation_inherits_cancelled_context()
    {
        using var scheduler = new PCodeScheduler();
        var root = scheduler.CreateTask();
        scheduler.RequestCancellation(root);

        var lateChild = scheduler.CreateTask(parent: root);

        Assert.True(lateChild.CancellationRequested);
    }

    [Fact]
    public void External_operation_registered_after_owner_cancellation_is_cancelled_immediately()
    {
        using var scheduler = new PCodeScheduler();
        var root = scheduler.CreateTask();
        scheduler.RequestCancellation(root);
        var cancellationCount = 0;

        using var registration = scheduler.RegisterExternalOperation(() => cancellationCount++, root);

        Assert.Equal(1, cancellationCount);
        Assert.Equal(0, scheduler.PendingExternalOperationCount);
    }
}
