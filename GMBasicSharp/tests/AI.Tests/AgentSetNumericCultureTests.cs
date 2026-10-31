// AgentServer.Set: i valori numerici (temperature/maxtokens/thoughtTokens) devono fare il
// round-trip indipendente dalla cultura di processo (bug 2026-09-02: su macchine it-IT,
// double.TryParse("0.42") senza InvariantCulture produceva 42 -> la temperatura scritta 0.42
// veniva riletta come 42; Get serializza gia' con InvariantCulture). Il fix allinea Set a Get.
using Basic.Agents;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentSetNumericCultureTests : IDisposable
{
    public AgentSetNumericCultureTests() => AgentServer.ResetForTests();
    public void Dispose() => AgentServer.ResetForTests();

    [Theory]
    [InlineData("0.42", "0.42")]
    [InlineData("1.5", "1.5")]
    [InlineData("0.1", "0.1")]
    public void Set_Temperature_Roundtrips_Invariant(string write, string expected)
    {
        AgentServer.Instance.Create("t1");
        Assert.True(AgentServer.Instance.Set("t1", "temperature", write));
        Assert.Equal(expected, AgentServer.Instance.Get("t1", "temperature"));
    }

    [Fact]
    public void Set_MaxTokens_Roundtrips()
    {
        AgentServer.Instance.Create("t2");
        Assert.True(AgentServer.Instance.Set("t2", "maxTokens", "512"));
        Assert.Equal("512", AgentServer.Instance.Get("t2", "maxTokens"));
    }

    [Fact]
    public void Set_ThinkingTokens_Roundtrips()
    {
        AgentServer.Instance.Create("t3");
        Assert.True(AgentServer.Instance.Set("t3", "thoughtTokens", "12"));
        Assert.Equal("12", AgentServer.Instance.Get("t3", "thoughtTokens"));
    }

    [Theory]
    [InlineData("0", "auto")]
    [InlineData("1", "chromium")]
    [InlineData("2", "obscura")]
    public void Set_Browser_EngineCode_Roundtrips(string code, string engine)
    {
        // la superficie BASIC passa il motore come codice numerico %AUTO/%CHROMIUM/%OBSCURA (mai stringhe)
        AgentServer.Instance.Create("br" + code);
        Assert.True(AgentServer.Instance.Set("br" + code, "browser", code));
        Assert.Equal(engine, AgentServer.Instance.Get("br" + code, "browser"));
    }
}
