// FILE: AI.Tests.MemoryBrainTests
using Basic.Agents;
using Basic.AI;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class MemoryBrainTests : IDisposable
{
    public MemoryBrainTests()
    {
        AgentServer.ResetForTests();
        MemoryBrain.ResetForTests();
    }
    public void Dispose()
    {
        AgentServer.ResetForTests();
        MemoryBrain.ResetForTests();
    }

    [Fact]
    public void AddAndSearch_FindsByLexicalFallback()
    {
        MemoryBrain.Add("l'utente preferisce SQLite per i dati", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Add("il server di test e' su :8899", MemoryBrain.MemoryKind.Fact);

        var res = MemoryBrain.Search("SQLite");

        res.Should().Contain("SQLite");
        res.Should().NotContain(":8899");
    }

    [Fact]
    public void Forget_Removes()
    {
        var id = MemoryBrain.Add("fatto temporaneo", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Search("temporaneo").Should().Contain("temporaneo");

        MemoryBrain.Forget(id).Should().BeTrue();
        MemoryBrain.Search("temporaneo").Should().BeEmpty();
    }

    [Fact]
    public void Watch_Refresh_IndexesChangedMarkdown()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb-brain-watch-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var file = Path.Combine(dir, "note.md");
        File.WriteAllText(file, "Appunti sul progetto: usare il brain per la memoria.");

        try
        {
            MemoryBrain.AddWatch(dir);
            MemoryBrain.RefreshWatched().Should().Be(1);

            var res = MemoryBrain.Search("brain");
            res.Should().Contain("brain");
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    [Fact]
    public void Watch_MarkdownWithWikiLinks_BuildsGraph()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb-brain-links-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var file = Path.Combine(dir, "note.md");
        File.WriteAllText(file, "Vedi anche [[Second Brain]] e [[Obsidian|sync]].");

        try
        {
            MemoryBrain.AddWatch(dir);
            MemoryBrain.RefreshWatched().Should().Be(1);

            var links = MemoryBrain.Links();
            links.Should().Contain(l => l.From == file && l.To == "Second Brain");
            links.Should().Contain(l => l.From == file && l.To == "Obsidian");
            MemoryBrain.ExtractWikiLinks("x [[A]] [[B|alias]]").Should().Equal("A", "B");
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    [Fact]
    public void ResolveSystemPrompt_WithMemory_IncludesRelevantFacts()
    {
        MemoryBrain.Add("il progetto usa SQLite per i dati", MemoryBrain.MemoryKind.Fact);
        MemoryBrain.Add("il server di test e' su :8899", MemoryBrain.MemoryKind.Fact);

        AgentServer.Instance.Create("mem");
        var agent = AgentServer.Instance.List().First(a => a.Name == "mem");
        agent.MemoryEnabled = true;

        var prompt = agent.ResolveSystemPrompt("SQLite");
        prompt.Should().Contain("## Memoria (second brain)");
        prompt.Should().Contain("Memoria rilevante");
        prompt.Should().Contain("SQLite");

        agent.MemoryEnabled = false;
        agent.ResolveSystemPrompt("SQLite").Should().NotContain("Memoria rilevante");
    }
}
