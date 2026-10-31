// FILE: AI.Tests.WorkflowEngineTests
using Basic.Workflow;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class WorkflowEngineTests : IDisposable
{
    public WorkflowEngineTests() => WorkflowEngine.Instance.ResetForTests();
    public void Dispose() => WorkflowEngine.Instance.ResetForTests();

    [Fact]
    public void Sequential_Dag_ExecutesInOrder()
    {
        var e = WorkflowEngine.Instance;
        Assert.True(e.BeginDag("W1"));
        Assert.True(e.AddStep("A", "actA"));
        Assert.True(e.AddStep("B", "actB"));
        Assert.True(e.AddStep("C", "actC"));
        Assert.True(e.EndDag());

        var order = new List<string>();
        var (ok, visited, log) = e.Execute("W1", (n, a) => order.Add(n));
        Assert.True(ok);
        Assert.Equal(["A", "B", "C"], order);
        Assert.Equal(["A", "B", "C"], visited);
        Assert.Equal("completed", e.Status("W1"));
    }

    [Fact]
    public void Parallel_Dag_RunsChildren()
    {
        var e = WorkflowEngine.Instance;
        e.BeginDag("WP");
        e.AddStep("A", "actA");
        e.BeginParallel("P1");
        e.AddStep("B", "actB");
        e.AddStep("C", "actC");
        e.EndGroup();
        e.AddStep("D", "actD");
        e.EndDag();

        var ran = new List<string>();
        var (ok, visited, log) = e.Execute("WP", (n, a) => { lock (ran) ran.Add(n); });
        Assert.True(ok);
        Assert.Contains("A", ran);
        Assert.Contains("B", ran);
        Assert.Contains("C", ran);
        Assert.Contains("D", ran);
        Assert.Contains(log, l => l.StartsWith("[PARALLEL P1]"));
        Assert.Equal("completed", e.Status("WP"));
    }

    [Fact]
    public void Branch_Dag_PicksChosenChild()
    {
        var e = WorkflowEngine.Instance;
        e.BeginDag("WB");
        e.AddStep("A", "actA");
        e.BeginBranch("BR");
        e.AddStep("B", "actB");
        e.AddStep("C", "actC");
        e.EndGroup();
        e.AddStep("D", "actD");
        e.EndDag();

        // resolver sceglie il figlio 1 (C) → B NON eseguito
        var ran = new List<string>();
        var (ok, visited, log) = e.Execute("WB", (n, a) => { lock (ran) ran.Add(n); }, (bid, act, cnt) => 1);
        Assert.True(ok);
        Assert.Contains("A", ran);
        Assert.DoesNotContain("B", ran);
        Assert.Contains("C", ran);
        Assert.Contains("D", ran);
        Assert.Contains(log, l => l.StartsWith("[BRANCH BR]"));
        Assert.Equal("completed", e.Status("WB"));
    }

    [Fact]
    public void Dag_Missing_ReturnsExplicitError()
    {
        var e = WorkflowEngine.Instance;
        var (ok, visited, log) = e.Execute("Nope", (n, a) => { });
        Assert.False(ok);
        Assert.Contains(log, l => l.Contains("non trovato"));
    }
}
