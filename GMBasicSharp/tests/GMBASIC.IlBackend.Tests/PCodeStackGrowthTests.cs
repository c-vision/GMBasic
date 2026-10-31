using Basic.Runtime.Gpm;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class PCodeStackGrowthTests
{
    [Fact]
    public void DefaultOperandAndFrameStacks_StartEmptyAndGrowOnDemand()
    {
        var task = new PCodeTask();
        var frame = new PCodeCallFrame();

        Assert.Empty(task.Stack);
        Assert.Empty(frame.FrameStack);

        task.PushRef("task");
        frame.PushFrameValue("frame");

        Assert.Equal("task", task.PopRef());
        Assert.Equal("frame", frame.PopFrameValue());
    }

    [Fact]
    public void SchedulerCreatedTask_LeavesOperandStackUnallocatedUntilFirstPush()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();

        Assert.Empty(task.Stack);

        task.PushRef("scheduled");

        Assert.NotEmpty(task.Stack);
        Assert.Equal("scheduled", task.PopRef());
    }

    [Fact]
    public void SchedulerCreatedTask_UsesConfiguredLocalCapacity()
    {
        using var scheduler = new PCodeScheduler { LocalCapacity = 7 };
        var task = scheduler.CreateTask();

        Assert.Equal(7, task.LocalCapacity);
        Assert.Equal(7, task.GetLocals().Length);
    }

    [Fact]
    public void LocalCount_SurvivesChunkSerializationAndUsesMaximumWhenMerged()
    {
        var main = new global::Compiler.CompiledChunk { LocalCount = 3 };
        main.Code.Add((byte)global::Compiler.OpCode.OP_HALT);
        main.Lines.Add(0);
        var library = new global::Compiler.CompiledChunk { LocalCount = 11 };
        library.Code.Add((byte)global::Compiler.OpCode.OP_HALT);
        library.Lines.Add(0);
        library.TryRegions[0] = new global::Compiler.TryRegionMetadata(1, 2);

        var restored = Basic.Runtime.ChunkA1Serializer.Deserialize(
            Basic.Runtime.ChunkA1Serializer.Serialize(library));
        var merged = global::Compiler.ChunkMerger.Merge(main, [restored]);

        Assert.Equal(11, restored.LocalCount);
        Assert.Equal(11, merged.LocalCount);
        Assert.Equal(new global::Compiler.TryRegionMetadata(1, 2), restored.TryRegions[0]);
        Assert.Equal(new global::Compiler.TryRegionMetadata(2, 3), merged.TryRegions[1]);
    }

    [Fact]
    public void OperandStack_GrowsWithoutDroppingValues_AndClearsPoppedReferences()
    {
        var task = new PCodeTask { Stack = new StackEntry[2], MaximumStackCapacity = 16 };
        var marker = new object();

        task.PushNum(12.5);
        task.PushString("two");
        task.PushRef(marker);
        task.PushBool(true);

        Assert.True(task.Stack.Length >= 4);
        Assert.True((bool)task.PopRef()!);
        Assert.Same(marker, task.PopRef());
        Assert.Equal("two", task.PopRef());
        Assert.Equal(12.5, task.PopNum());
        Assert.Equal(0, task.Sp);
        Assert.All(task.Stack, entry => Assert.Null(entry.Ref));
    }

    [Fact]
    public void OperandStack_AboveConfiguredMaximum_ThrowsDeterministicError()
    {
        var task = new PCodeTask { Stack = new StackEntry[2], MaximumStackCapacity = 3 };
        task.PushNum(1);
        task.PushNum(2);
        task.PushNum(3);

        var error = Assert.Throws<PCodeStackOverflowException>(() => task.PushNum(4));
        Assert.Contains("operand", error.Message);
        Assert.Contains("maximum is 3", error.Message);
    }

    [Fact]
    public void FrameStack_GrowsAndPreservesLifoOrder()
    {
        var frame = new PCodeCallFrame
        {
            FrameStack = new object?[1],
            MaximumFrameStackCapacity = 8
        };

        frame.PushFrameValue("first");
        frame.PushFrameValue("second");
        frame.PushFrameValue("third");

        Assert.True(frame.FrameStack.Length >= 3);
        Assert.Equal("third", frame.PopFrameValue());
        Assert.Equal("second", frame.PopFrameValue());
        Assert.Equal("first", frame.PopFrameValue());
        Assert.Null(frame.PopFrameValue());
        Assert.All(frame.FrameStack, Assert.Null);
    }

    [Fact]
    public void FrameStack_AboveConfiguredMaximum_ThrowsDeterministicError()
    {
        var frame = new PCodeCallFrame
        {
            FrameStack = new object?[1],
            MaximumFrameStackCapacity = 2
        };
        frame.PushFrameValue(1);
        frame.PushFrameValue(2);

        var error = Assert.Throws<PCodeStackOverflowException>(() => frame.PushFrameValue(3));
        Assert.Contains("frame", error.Message);
        Assert.Contains("maximum is 2", error.Message);
    }

    [Fact]
    public void Scheduler_ReclaimsEachFinishedTaskIncrementally()
    {
        using var scheduler = new PCodeScheduler();

        for (var i = 0; i < 2_000; i++)
        {
            var task = scheduler.CreateTask(i);
            task.State = PCodeTaskState.Finished;
            scheduler.ReclaimFinishedTask(task);
        }

        Assert.Equal(0, scheduler.TrackedTaskCount);
        Assert.False(scheduler.HasActiveTasks);
    }

    [Fact]
    public void Scheduler_RejectsReclaimOfRunnableTask()
    {
        using var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask();

        Assert.Throws<InvalidOperationException>(() => scheduler.ReclaimFinishedTask(task));
        Assert.Equal(1, scheduler.TrackedTaskCount);
    }
}
