// FILE: AI.Tests.SecondBrainMemoryTests
// §3 Memoria: MemoryRanker (mem0 scoring) + LoopDetector (5 meccanismi anti-loop).
using Basic.AI;
using Basic.AI.Agents;
using Basic.AI.Rag;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class SecondBrainMemoryTests : IDisposable
{
    public SecondBrainMemoryTests()
    {
        MemoryBrain.ResetForTests();
        LoopDetector.ResetForTests();
    }

    public void Dispose()
    {
        MemoryBrain.ResetForTests();
        LoopDetector.ResetForTests();
    }

    [Fact]
    public void MemoryRanker_Frequency_CountsRepeats()
    {
        MemoryBrain.Add("l'utente preferisce SQLite per i dati", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Add("l'utente preferisce SQLite per i dati", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Add("il server di test è su :8899", MemoryBrain.MemoryKind.Fact);

        var freq = MemoryRanker.FrequencyOf("l'utente preferisce SQLite per i dati");
        Assert.True(freq >= 2);

        var ranked = MemoryRanker.SearchRanked("SQLite", 5);
        Assert.Contains("l'utente preferisce SQLite", ranked);
    }

    [Fact]
    public void MemoryRanker_Consolidate_Dedups()
    {
        MemoryBrain.Add("Fatto importante e durevole", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Add("fatto importante e durevole ", MemoryBrain.MemoryKind.Fact);
        MemoryRanker.Consolidate();
        var ranked = MemoryRanker.SearchRanked("importante", 10);
        // dopo consolidazione, una sola riga per lo stesso testo (le righe sono id|kind|text|...)
        var lines = ranked.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var texts = lines.Select(l => l.Split('|')[2]).ToList();
        Assert.Single(texts);
    }

    [Fact]
    public void LoopDetector_ActionDedup_StopsAfterThreshold()
    {
        var v1 = LoopDetector.TrackAction("agent1", "TOOL.X", "arg=1", threshold: 3);
        Assert.False(v1.IsLoop);
        LoopDetector.TrackAction("agent1", "TOOL.X", "arg=1", threshold: 3);
        var v3 = LoopDetector.TrackAction("agent1", "TOOL.X", "arg=1", threshold: 3);
        Assert.True(v3.IsLoop);
        Assert.Equal("action-dedup", v3.Reason);
    }

    [Fact]
    public void LoopDetector_StateProbe_StopsOnRepeatedOutput()
    {
        LoopDetector.TrackOutput("agent2", "output iniziale", 3);
        LoopDetector.TrackOutput("agent2", "stesso output", 3);
        LoopDetector.TrackOutput("agent2", "stesso output", 3);
        LoopDetector.TrackOutput("agent2", "stesso output", 3);
        var v4 = LoopDetector.TrackOutput("agent2", "stesso output", 3);
        Assert.True(v4.IsLoop);
        Assert.Equal("state-probe", v4.Reason);
    }

    [Fact]
    public void LoopDetector_Budget_Exhausted()
    {
        var v1 = LoopDetector.TrackBudget("agent3", 60, maxTokens: 100);
        Assert.False(v1.IsLoop);
        var v2 = LoopDetector.TrackBudget("agent3", 60, maxTokens: 100);
        Assert.True(v2.IsLoop);
        Assert.Equal("budget", v2.Reason);
    }

    [Fact]
    public void LoopDetector_CycleMemoryPrompt_Warns()
    {
        LoopDetector.TrackAction("agent4", "TOOL.Y", "x", threshold: 3);
        LoopDetector.TrackAction("agent4", "TOOL.Y", "x", threshold: 3);
        var prompt = LoopDetector.CycleMemoryPrompt("agent4", "TOOL.Y", "x");
        Assert.NotNull(prompt);
        Assert.Contains("AVVISO LOOP", prompt);
    }

    [Fact]
    public void LoopDetector_RecordLoop_WritesBrain()
    {
        LoopDetector.RecordLoop("agent5", new LoopVerdict(true, "budget", "esaurito"));
        var ranked = MemoryRanker.SearchRanked("Loop rilevato", 5);
        Assert.Contains("Loop rilevato", ranked);
    }
}
