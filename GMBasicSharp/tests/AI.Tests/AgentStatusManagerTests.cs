// FILE: AI.Tests.AgentStatusManagerTests
using Basic.Agents;
using Basic.AI.Language;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentStatusManagerTests : IDisposable
{
    public AgentStatusManagerTests()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
    }
    public void Dispose()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
    }

    [Fact]
    public void GetAgentStatusJson_IncludesIdentityAndMembership()
    {
        AgentServer.Instance.Create("analista");
        AgentServer.Instance.Create("revisore");
        TeamServer.Instance.Create("Squadra");
        TeamServer.Instance.AddAgent("Squadra", "analista");

        var json = AgentStatusManager.GetAgentStatusJson("analista");
        Assert.Contains("\"name\":\"analista\"", json);
        Assert.Contains("teams", json);
        Assert.Contains("Squadra", json);
        Assert.Contains("hasKeypair", json);
    }

    [Fact]
    public void GetTeamStatusJson_IncludesStrategyAndMembers()
    {
        AgentServer.Instance.Create("a1");
        TeamServer.Instance.Create("T1");
        TeamServer.Instance.AddAgent("T1", "a1");
        var json = AgentStatusManager.GetTeamStatusJson("T1");
        Assert.Contains("\"name\":\"T1\"", json);
        Assert.Contains("memberCount", json);
        Assert.Contains("a1", json);
    }

    [Fact]
    public void MissingAgent_ReturnsExplicitError()
    {
        var json = AgentStatusManager.GetAgentStatusJson("ghost");
        Assert.Contains("non trovato", json);
    }

    [Fact]
    public void LanguageManifest_Write_ProducesJsonWithNamespaces()
    {
        var path = Path.Combine(Path.GetTempPath(), "gmb-lang.json");
        var res = LanguageManifest.WriteTo(path);
        Assert.Equal(path, res);
        var json = File.ReadAllText(path);
        Assert.Contains("namespaces", json);
        Assert.Contains("generatedAt", json);
        try { if (File.Exists(path)) File.Delete(path); } catch { }
    }

    [Fact]
    public void LanguageManifest_Search_FindsFunction()
    {
        var json = LanguageManifest.Build();
        var res = LanguageManifest.Search("KgQuery", json);
        Assert.Contains("KgQuery", res);
        var notFound = LanguageManifest.Search("zzz_inestistente_zzz", json);
        Assert.Contains("\"count\":0", notFound);
    }
}
