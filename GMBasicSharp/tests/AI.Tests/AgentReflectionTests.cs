// FILE: AI.Tests.AgentReflectionTests
using Basic.Agents;
using Basic.AI;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentReflectionTests : IDisposable
{
    public AgentReflectionTests()
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
    public void Reflect_WithMemoryEnabled_StoresFactsAndEpisodicSummary()
    {
        AgentServer.Instance.Create("refl");
        var agent = AgentServer.Instance.List().First(a => a.Name == "refl");
        agent.MemoryEnabled = true;

        AgentReflection.Reflect(agent, new StubReflectionCompleter(),
            "Utente: parliamo del progetto\nAgente: usiamo SQLite per i dati");

        var facts = MemoryBrain.Search("SQLite");
        facts.Should().Contain("SQLite");
        var episodic = MemoryBrain.Search("turno su");
        episodic.Should().Contain("turno su");
    }

    [Fact]
    public void Reflect_WithMemoryDisabled_DoesNothing()
    {
        AgentServer.Instance.Create("refloff");
        var agent = AgentServer.Instance.List().First(a => a.Name == "refloff");
        agent.MemoryEnabled = false;

        AgentReflection.Reflect(agent, new StubReflectionCompleter(), "Utente: x\nAgente: y");

        MemoryBrain.Search("SQLite").Should().BeEmpty();
    }

    [Fact]
    public void Reflect_FixtureCompleter_Skipped()
    {
        AgentServer.Instance.Create("reflfix");
        var agent = AgentServer.Instance.List().First(a => a.Name == "reflfix");
        agent.MemoryEnabled = true;

        AgentReflection.Reflect(agent, new FixtureLlmCompleter(), "Utente: x\nAgente: y");

        MemoryBrain.Search("insufficient").Should().BeEmpty(); // niente rumore dal fixture
    }

    private sealed class StubReflectionCompleter : ILlmCompleter
    {
        public string ModelName => "stub-refl";
        public string Complete(string systemPrompt, string userPrompt)
        {
            if (userPrompt.Contains("estrattore", StringComparison.OrdinalIgnoreCase))
                return "l'utente usa SQLite per i dati\nil progetto e' GMBASIC";
            if (userPrompt.Contains("Riassumi", StringComparison.OrdinalIgnoreCase))
                return "turno su SQLite";
            return "";
        }
    }
}
