// FILE: AI.Tests.TeamErrorHandlingTests
// TeamOrchestrator item 2: gestione errori dei membri. Un membro che fallisce (provider non
// configurato) NON deve abortire il team: AskMember logga, raccoglie in team.Metadata["memberErrors"]
// e gli altri membri continuano. Qui entrambi i membri falliscono (nessun provider) → Run deve
// ritornare senza lanciare e memberErrors == 2.
using Basic.Agents;
using Basic.Providers;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class TeamErrorHandlingTests : IDisposable
{
    public TeamErrorHandlingTests()
    {
        ProviderServer.ResetForTests();
    }

    public void Dispose()
    {
        ProviderServer.ResetForTests();
    }

    [Fact]
    public void MemberFailure_DoesNotAbortTeam_AndCountsErrors()
    {
        var server = AgentServer.Instance;
        server.Create("fail1");
        server.Create("fail2");          // provider pinnato a "ghost" (inesistente) -> Ask lancia
        server.Set("fail1", "provider", "ghost");
        server.Set("fail2", "provider", "ghost");

        var team = new TeamRecord { Name = "et", Strategy = TeamStrategy.Sequential };
        team.Members.Add(new TeamMember { AgentName = "fail1" });
        team.Members.Add(new TeamMember { AgentName = "fail2" });
        TeamServer.Instance.Create("et");
        TeamServer.Instance.AddAgent("et", "fail1");
        TeamServer.Instance.AddAgent("et", "fail2");

        // Non deve lanciare: ogni membro fallito è gestito (skip con log), non abortisce il team.
        var result = TeamOrchestrator.Run(team, "task qualunque");
        Assert.NotNull(result);
        var errs = team.Metadata.TryGetValue("memberErrors", out var v) && int.TryParse(v, out var n) ? n : 0;
        Assert.True(errs >= 2, $"attesi >=2 errori membro gestiti, trovati {errs}");
    }
}

[Trait("Category", "Fast")]
public sealed class TeamStreamingTests : IDisposable
{
    private readonly System.Collections.Generic.List<string> _seen = new();
    public TeamStreamingTests() { Basic.Providers.ProviderServer.ResetForTests(); }
    public void Dispose()
    {
        TeamOrchestrator.MemberCompleted -= OnMember;
        Basic.Providers.ProviderServer.ResetForTests();
    }
    private void OnMember(string tm, string mb, string tx) => _seen.Add(mb);

    [Fact]
    public void MemberCompleted_FiresPerMember_DuringRun()
    {
        var server = AgentServer.Instance;
        server.Create("sf1");
        server.Create("sf2");
        server.Set("sf1", "provider", "ghost");
        server.Set("sf2", "provider", "ghost");
        var team = new TeamRecord { Name = "st", Strategy = TeamStrategy.Sequential };
        team.Members.Add(new TeamMember { AgentName = "sf1" });
        team.Members.Add(new TeamMember { AgentName = "sf2" });
        TeamOrchestrator.MemberCompleted += OnMember;
        TeamOrchestrator.Run(team, "task");
        Assert.Contains("sf1", _seen);
        Assert.Contains("sf2", _seen);
    }
}

[Trait("Category", "Fast")]
public sealed class TeamEndToEndAsyncStreamTests : IDisposable
{
    private readonly System.Collections.Generic.List<(string Team, string Member, string Text)> _stream = new();
    public TeamEndToEndAsyncStreamTests() { Basic.Providers.ProviderServer.ResetForTests(); }
    public void Dispose()
    {
        Basic.Agents.TeamOrchestrator.MemberCompleted -= On;
        Basic.Providers.ProviderServer.ResetForTests();
    }
    private void On(string tm, string mb, string tx) { lock (_stream) _stream.Add((tm, mb, tx)); }

    [Fact]
    public void AsyncRunWaitAndStream_WithRealFixtureMembers_EndToEnd()
    {
        var server = AgentServer.Instance;
        server.Create("e1");
        server.Create("e2");
        server.Set("e1", "provider", "fixture-completion");   // risponde davvero (deterministico)
        server.Set("e2", "provider", "fixture-completion");
        Basic.Agents.TeamServer.Instance.Create("e2eT");
        Basic.Agents.TeamServer.Instance.AddAgent("e2eT", "e1");
        Basic.Agents.TeamServer.Instance.AddAgent("e2eT", "e2");
        Basic.Agents.TeamOrchestrator.MemberCompleted += On;

        var handle = Basic.IlBackend.IlRuntime.TeamRunStart("e2eT", "rispondi in una riga: ciao");
        Assert.StartsWith("team_", handle);
        var result = Basic.IlBackend.IlRuntime.TeamRunWait(handle);

        Assert.False(string.IsNullOrWhiteSpace(result), "atteso output non vuoto dal team con membri fixture");
        lock (_stream) { Assert.True(_stream.Count >= 2, $"attesi >=2 eventi streaming, visti {_stream.Count}"); }
    }
}
