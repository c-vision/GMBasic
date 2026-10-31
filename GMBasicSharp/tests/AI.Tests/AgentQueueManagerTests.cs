// FILE: AI.Tests.AgentQueueManagerTests
using Basic.Agents;
using Basic.Workflow;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentQueueManagerTests : IDisposable
{
    public AgentQueueManagerTests()
    {
        AgentServer.ResetForTests();
        // Reset queue and bus via reflection (internal)
        typeof(AgentQueueManager).GetMethod("ResetForTests", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)?.Invoke(null, null);
        typeof(MessageBus).GetMethod("ResetForTests", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)?.Invoke(null, null);
        WorkflowServer.Instance.GetType().GetMethod("ResetForTests", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)?.Invoke(null, null);
        Basic.Agents.TeamServer.ResetForTests();
    }
    public void Dispose()
    {
        AgentServer.ResetForTests();
        typeof(AgentQueueManager).GetMethod("ResetForTests", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)?.Invoke(null, null);
        typeof(MessageBus).GetMethod("ResetForTests", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)?.Invoke(null, null);
    }

    [Fact]
    public void Queue_CreateSendReceive_StopPauseResume()
    {
        var qm = AgentQueueManager.Instance;
        Assert.True(qm.QueueCreate("A"));
        Assert.True(qm.Enqueue("A", "user", "Task1", "input1"));
        Assert.True(qm.Enqueue("A", "user", "Task2", "input2"));
        Assert.Equal(2, qm.Count("A"));

        var m1 = qm.Dequeue("A");
        Assert.NotNull(m1);
        Assert.Equal("Task1", m1!.Task);

        Assert.True(qm.Pause("A"));
        Assert.True(qm.IsPaused("A"));
        Assert.True(qm.Enqueue("A", "user", "Task3", "input3"));
        Assert.Equal(2, qm.Count("A")); // Task2 + Task3 (Task1 already dequeued)
        Assert.Null(qm.Dequeue("A")); // blocked by pause

        Assert.True(qm.Resume("A"));
        Assert.False(qm.IsPaused("A"));
        var m2 = qm.Dequeue("A");
        Assert.NotNull(m2);
        Assert.Equal("Task2", m2!.Task);
        var m3 = qm.Dequeue("A");
        Assert.Equal("Task3", m3!.Task);

        Assert.True(qm.Stop("A"));
        Assert.True(qm.IsStopped("A"));
        Assert.Equal(0, qm.Count("A"));
        Assert.False(qm.Enqueue("A", "user", "Task4", "input4"));
        Assert.Null(qm.Dequeue("A"));
    }

    [Fact]
    public void Channel_PublishSubscribe()
    {
        var bus = MessageBus.Instance;
        int recv = 0;
        void H(string ch, AgentMessage msg) => recv++;

        Assert.Equal(0, bus.SubscriberCount("chan1"));
        Assert.True(bus.Subscribe("chan1", H));
        Assert.Equal(1, bus.SubscriberCount("chan1"));
        Assert.Equal(1, bus.Publish("chan1", "user", "TaskBus", "inputBus"));
        Assert.Equal(1, recv);
        Assert.True(bus.Unsubscribe("chan1", H));
        Assert.Equal(0, bus.SubscriberCount("chan1"));
        Assert.Equal(0, bus.Publish("chan1", "user", "TaskBus2", "input2"));
        Assert.Equal(1, recv);
    }

    [Fact]
    public void Team_Broadcast_EnqueuesToAllMembers()
    {
        AgentServer.Instance.Create("A1");
        AgentServer.Instance.Create("A2");
        Basic.Agents.TeamServer.Instance.Create("DreamTeam");
        Basic.Agents.TeamServer.Instance.AddAgent("DreamTeam", "A1");
        Basic.Agents.TeamServer.Instance.AddAgent("DreamTeam", "A2");

        var qm = AgentQueueManager.Instance;
        qm.QueueCreate("A1");
        qm.QueueCreate("A2");

        var count = Basic.Agents.TeamOrchestrator.Broadcast("DreamTeam", "Hello team");
        Assert.Equal(2, count);
        Assert.Equal(1, qm.Count("A1"));
        Assert.Equal(1, qm.Count("A2"));

        var m1 = qm.Dequeue("A1");
        Assert.Equal("Hello team", m1!.Input);
        var m2 = qm.Dequeue("A2");
        Assert.Equal("Hello team", m2!.Input);
    }

    [Fact]
    public void Team_Redefine_UpdatesMembers()
    {
        AgentServer.Instance.Create("Old1");
        Basic.Agents.TeamServer.Instance.Create("RTeam");
        Basic.Agents.TeamServer.Instance.AddAgent("RTeam", "Old1");
        var before = Basic.Agents.TeamServer.Instance.Get("RTeam")!;
        Assert.Single(before.Members);

        var n = Basic.Agents.TeamOrchestrator.Redefine("RTeam", "A1,A2,A3");
        Assert.Equal(3, n);
        var after = Basic.Agents.TeamServer.Instance.Get("RTeam")!;
        Assert.Equal(3, after.Members.Count);
        Assert.Contains(after.Members, m => m.AgentName == "A1");
    }

    [Fact]
    public void Workflow_Sequential_WithQueue()
    {
        var wf = "WF1";
        WorkflowServer.Instance.BeginDefinition(wf);
        WorkflowServer.Instance.AddStep("s1", "action1");
        WorkflowServer.Instance.AddStep("s2", "action2");
        WorkflowServer.Instance.EndDefinition();

        bool s1Called = false, s2Called = false;
        var ok = WorkflowServer.Instance.Execute(wf, (name, action) =>
        {
            if (name == "s1") s1Called = true;
            if (name == "s2") s2Called = true;
        });
        Assert.True(ok);
        Assert.True(s1Called && s2Called);
        Assert.Equal("completed", WorkflowServer.Instance.GetStatus(wf));
    }

    [Fact]
    public void GoSelect_QueueCoordinator_SelectsFirstAvailable()
    {
        var qm = AgentQueueManager.Instance;
        qm.QueueCreate("Q1");
        qm.QueueCreate("Q2");
        qm.Enqueue("Q2", "user", "TaskQ2", "inputQ2");

        // Simulate GO SELECT: poll both queues, first with message wins
        AgentMessage? selected = null;
        string? winner = null;
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < 1000)
        {
            selected = qm.Dequeue("Q1");
            if (selected != null) { winner = "Q1"; break; }
            selected = qm.Dequeue("Q2");
            if (selected != null) { winner = "Q2"; break; }
            System.Threading.Thread.Sleep(10);
        }
        Assert.NotNull(selected);
        Assert.Equal("Q2", winner);
        Assert.Equal("TaskQ2", selected!.Task);
    }
}
