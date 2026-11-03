using Basic.Agents;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RouterAgentTests : IDisposable
{
    public RouterAgentTests() { AgentServer.ResetForTests(); ProviderServer.ResetForTests(); }
    public void Dispose() { AgentServer.ResetForTests(); ProviderServer.ResetForTests(); }

    private static AgentRecord Create(string name)
    {
        AgentServer.Instance.Create(name);
        return AgentServer.Instance.List().First(a => a.Name == name);
    }

    [Fact]
    public void RouterOff_ReturnsNull()
    {
        var agent = Create("Roff");
        AgentServer.PickRoutedProvider(agent, "qualunque prompt").Should().BeNull();
    }

    [Fact]
    public void RouterOn_MissingProviders_ThrowsExplicit()
    {
        var agent = Create("Rcfg");
        AgentServer.Instance.Set("Rcfg", "router", "ON");
        var ex = Record.Exception(() => AgentServer.PickRoutedProvider(agent, "prompt"));
        ex.Should().NotBeNull();
        ex!.Message.Should().Contain("smallprovider");
    }

    [Fact]
    public void RouterOn_ShortSimplePrompt_UsesSmallProvider()
    {
        var agent = Create("Rsmall");
        AgentServer.Instance.Set("Rsmall", "router", "ON");
        AgentServer.Instance.Set("Rsmall", "smallprovider", "pSmall");
        AgentServer.Instance.Set("Rsmall", "frontierprovider", "pFront");

        var picked = AgentServer.PickRoutedProvider(agent, "Qual e' la capitale dell'Italia?");
        picked.Should().Be("pSmall");
    }

    [Fact]
    public void DecideMock_RoutesToSmall()
    {
        var agent = Create("RdecideMock");
        AgentServer.Instance.Set("RdecideMock", "router", "ON");
        AgentServer.Instance.Set("RdecideMock", "smallprovider", "pSmall");
        AgentServer.Instance.Set("RdecideMock", "frontierprovider", "pFront");
        AgentServer.Instance.Set("RdecideMock", "decide", "mock");

        AgentServer.PickRoutedProvider(agent, "sì, richiesta complessa").Should().Be("pSmall");
    }

    [Fact]
    public void DecideRules_Yes_No_RoutesFrontierAndSmall()
    {
        var agent = Create("RdecideRules");
        AgentServer.Instance.Set("RdecideRules", "router", "ON");
        AgentServer.Instance.Set("RdecideRules", "smallprovider", "pSmall");
        AgentServer.Instance.Set("RdecideRules", "frontierprovider", "pFront");
        AgentServer.Instance.Set("RdecideRules", "decide", "rules");

        AgentServer.PickRoutedProvider(agent, "sì, serve il modello grande").Should().Be("pFront");
        AgentServer.PickRoutedProvider(agent, "no, basta il piccolo").Should().Be("pSmall");
    }

    [Fact]
    public void DecideOff_UsesTes()
    {
        var agent = Create("RdecideOff");
        AgentServer.Instance.Set("RdecideOff", "router", "ON");
        AgentServer.Instance.Set("RdecideOff", "smallprovider", "pSmall");
        AgentServer.Instance.Set("RdecideOff", "frontierprovider", "pFront");
        AgentServer.Instance.Set("RdecideOff", "decide", "off");

        var hard = new string('x', 4000) + " analizza confronta e sintetizza la dimostrazione"
            + " valutando criticamente il refactor e il teorema matematico";
        AgentServer.PickRoutedProvider(agent, hard).Should().Be("pFront");
    }

    [Fact]
    public void RouterOn_HardLongPrompt_UsesFrontierProvider()
    {
        var agent = Create("Rfront");
        AgentServer.Instance.Set("Rfront", "router", "ON");
        AgentServer.Instance.Set("Rfront", "smallprovider", "pSmall");
        AgentServer.Instance.Set("Rfront", "frontierprovider", "pFront");

        var hard = new string('x', 4000) + " analizza confronta e sintetizza la dimostrazione"
            + " valutando criticamente il refactor e il teorema matematico";
        var picked = AgentServer.PickRoutedProvider(agent, hard);
        picked.Should().Be("pFront");
    }
}
