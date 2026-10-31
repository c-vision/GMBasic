using System;
using System.Threading.Tasks;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class BlackboardTests
{
    [Fact]
    public void ConcurrentWrites_AllVisible_NoLostUpdates()
    {
        Blackboard.Clear();
        const int N = 200;
        Parallel.For(0, N, i => Blackboard.Write("k" + i, "v" + i));
        for (int i = 0; i < N; i++)
            Assert.Equal("v" + i, Blackboard.Read("k" + i));   // last-write-wins, no race loss
        Assert.Equal(N, Blackboard.Count);
        Blackboard.Clear();
    }

    [Fact]
    public void Wait_ReturnsWhenWritten_OrEmptyOnTimeout()
    {
        Blackboard.Clear();
        // timeout -> "" se nessuno scrive
        Assert.Equal("", Blackboard.Wait("bb_wait_none", 200));
        // write da un altro task -> WAIT si sospende e riprende col valore
        var writer = Task.Run(() => { Task.Delay(150).Wait(); Blackboard.Write("bb_ready", "result"); });
        Assert.Equal("result", Blackboard.Wait("bb_ready", 3000));
        writer.Wait();
        Blackboard.Clear();
    }
}
