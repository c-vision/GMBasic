// FILE: AI.Tests.TeamOrchestratorTests
using Basic.Agents;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class TeamOrchestratorTests : IDisposable
{
    public TeamOrchestratorTests()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
    }

    [Fact]
    public void Run_Sequential_AsksEveryMemberOnceAndRecordsConversation()
    {
        AgentServer.Instance.Create("seq-a");
        AgentServer.Instance.Create("seq-b");
        TeamServer.Instance.Create("team-seq");
        TeamServer.Instance.AddAgent("team-seq", "seq-a");
        TeamServer.Instance.AddAgent("team-seq", "seq-b");

        var team = TeamServer.Instance.Get("team-seq")!;
        var result = TeamOrchestrator.Run(team, "task di prova");

        result.Should().NotBeNullOrEmpty();
        team.Conversation.Should().HaveCount(2);
        team.Conversation.Select(m => m.AgentName).Should().Equal("seq-a", "seq-b");
    }

    [Fact]
    public void Run_Coordinator_FallsBackToSequentialWhenNoCoordinator()
    {
        AgentServer.Instance.Create("cb-a");
        AgentServer.Instance.Create("cb-b");
        TeamServer.Instance.Create("team-cb");
        TeamServer.Instance.AddAgent("team-cb", "cb-a");
        TeamServer.Instance.AddAgent("team-cb", "cb-b");
        TeamServer.Instance.SetStrategy("team-cb", TeamStrategy.Coordinator);

        var team = TeamServer.Instance.Get("team-cb")!;
        var result = TeamOrchestrator.Run(team, "task");

        result.Should().NotBeNullOrEmpty();
        // Nessun coordinator: fallback a sequential (2 membri, 1 giro).
        team.Conversation.Should().HaveCount(2);
    }

    [Fact]
    public void Run_Coordinator_AsksWorkersThenSynthesizes()
    {
        AgentServer.Instance.Create("co-agent");
        AgentServer.Instance.Create("w1");
        AgentServer.Instance.Create("w2");
        TeamServer.Instance.Create("team-co");
        TeamServer.Instance.SetCoordinator("team-co", "co-agent");
        TeamServer.Instance.AddAgent("team-co", "co-agent", TeamRole.Coordinator);
        TeamServer.Instance.AddAgent("team-co", "w1");
        TeamServer.Instance.AddAgent("team-co", "w2");
        TeamServer.Instance.SetStrategy("team-co", TeamStrategy.Coordinator);

        var team = TeamServer.Instance.Get("team-co")!;
        var result = TeamOrchestrator.Run(team, "task");

        result.Should().NotBeNullOrEmpty();
        // 2 worker + 1 sintesi coordinator.
        team.Conversation.Should().HaveCount(3);
    }
}
