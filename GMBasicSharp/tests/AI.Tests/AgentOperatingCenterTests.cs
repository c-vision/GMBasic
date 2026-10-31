// FILE: AI.Tests.AgentOperatingCenterTests
using Basic.Agents;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentOperatingCenterTests : IDisposable
{
    // Config AI su file TEMPORANEO (bug 2026-08-28): questi test leggono SystemPrompt/LogPath
    // e non devono dipendere dalla config utente reale ~/.gmbasic/ai.json.
    private readonly string _configPath =
        Path.Combine(Path.GetTempPath(), "gmbasic-aoc-" + Guid.NewGuid().ToString("N") + ".json");

    public AgentOperatingCenterTests()
    {
        Basic.AI.AiConfig.OverridePathForTests(_configPath);
        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
    }

    public void Dispose()
    {
        Basic.AI.AiConfig.ResetForTests();
        if (File.Exists(_configPath)) File.Delete(_configPath);
        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
    }

    [Fact]
    public void List_ReturnsAllRegisteredAgents()
    {
        AgentServer.Instance.Create("a1");
        AgentServer.Instance.Create("a2");

        AgentServer.Instance.List().Select(a => a.Name).Should().Contain(["a1", "a2"]);
    }

    [Fact]
    public void GetStatusSummary_IncludesModelAndCounts()
    {
        AgentServer.Instance.Create("agent-x");
        AgentServer.Instance.Ask("agent-x", "hello");

        var status = AgentServer.Instance.GetStatusSummary("agent-x");

        status.Should().Contain("name=agent-x");
        status.Should().Contain("historyCount=2"); // user + assistant
        status.Should().Contain("toolCount=0");
    }

    [Fact]
    public void EventLog_WhenTraceOff_RecordsNothing()
    {
        AgentEventLog.Instance.SetMode("OFF");
        AgentServer.Instance.Create("silent-agent");

        AgentEventLog.Instance.Snapshot().Should().BeEmpty();
    }

    [Fact]
    public void EventLog_WhenTraceOn_RecordsCreatedAndAsked()
    {
        AgentEventLog.Instance.SetMode("ON");
        AgentServer.Instance.Create("traced-agent");
        AgentServer.Instance.Ask("traced-agent", "hi");

        var events = AgentEventLog.Instance.Snapshot();
        events.Should().Contain(e => e.Kind == AgentEventKind.Created && e.AgentName == "traced-agent");
        events.Should().Contain(e => e.Kind == AgentEventKind.Asked && e.AgentName == "traced-agent");
    }

    [Fact]
    public void SetMode_RejectsUnknownValue()
    {
        var act = () => AgentEventLog.Instance.SetMode("BOGUS");
        act.Should().Throw<ArgumentException>();
    }
    [Fact]
    public void Skills_DefaultOff_AndCanBeEnabled()
    {
        AgentServer.Instance.Create("sk1");

        AgentServer.Instance.Get("sk1", "skills").Should().Be("OFF");

        AgentServer.Instance.Set("sk1", "skills", "ON").Should().BeTrue();
        AgentServer.Instance.Get("sk1", "skills").Should().Be("ON");
    }
    [Fact]
    public void SkillsEnabled_IsPersistedAcrossSaveLoad()
    {
        AgentServer.Instance.Create("persist-skills");
        AgentServer.Instance.Set("persist-skills", "skills", "ON");

        var path = Path.Combine(Path.GetTempPath(), "gmb-persist-skills.agent.json");
        AgentServer.Instance.Save("persist-skills", path);
        AgentServer.Instance.Delete("persist-skills");

        AgentServer.Instance.Load("persist-skills", path);
        AgentServer.Instance.Get("persist-skills", "skills").Should().Be("ON");

        File.Delete(path);
    }
    [Fact]
    public void ResolveSystemPrompt_IncludesLogFolder()
    {
        AgentServer.Instance.Create("logprompt");
        var record = AgentServer.Instance.List().First(a => a.Name == "logprompt");

        var prompt = record.ResolveSystemPrompt();
        prompt.Should().Contain("Agent log folder");
        prompt.Should().Contain(Basic.AI.AiConfig.LogPath);
        prompt.Should().Contain("autonomous AI agent"); // SystemPrompt base incluso
    }

    [Fact]
    public void AskStream_StreamsTokenByTokenAndRecordsFullAnswer()
    {
        AgentServer.Instance.Create("stream-agent");
        var chunks = new List<string>();

        AgentServer.Instance.AskStream("stream-agent", "prova stream", chunks.Add);

        // Il FixtureLlmCompleter streamma parola per parola: >1 chunk, non uno solo.
        chunks.Should().NotBeEmpty();
        chunks.Count.Should().BeGreaterThan(1);

        var full = string.Concat(chunks);
        full.Should().Contain("prova stream");

        AgentServer.Instance.GetStatusSummary("stream-agent").Should().Contain("historyCount=2");
    }
}
