// FILE: AI.Tests.AgentIdentityTests
using Basic.Agents;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentIdentityTests : IDisposable
{
    public AgentIdentityTests()
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
    public void AttachKeypair_Then_SignVerify()
    {
        AgentServer.Instance.Create("bot");
        var agent = AgentServer.Instance.List().First(a => a.Name == "bot");
        var pub = AgentIdentity.AttachKeypair(agent);
        Assert.NotNull(pub);
        Assert.Contains("PUBLIC KEY", pub);

        var sig = AgentIdentity.Sign(agent, "msg-importante");
        Assert.True(AgentIdentity.Verify(agent, "msg-importante", sig));
        Assert.False(AgentIdentity.Verify(agent, "msg-tampered", sig));
    }

    [Fact]
    public void Sign_WithoutPrivateKey_ThrowsExplicit()
    {
        AgentServer.Instance.Create("nokey");
        var agent = AgentServer.Instance.List().First(a => a.Name == "nokey");
        var ex = Assert.Throws<InvalidOperationException>(() => AgentIdentity.Sign(agent, "x"));
        Assert.Contains("KEYPAIR", ex.Message);
    }

    [Fact]
    public void Keypair_Persisted_Across_SaveLoad()
    {
        AgentServer.Instance.Create("persist-id");
        var agent = AgentServer.Instance.List().First(a => a.Name == "persist-id");
        AgentIdentity.AttachKeypair(agent);
        var sigBefore = AgentIdentity.Sign(agent, "roundtrip");

        var path = Path.Combine(Path.GetTempPath(), "gmb-ident.agent.json");
        AgentServer.Instance.Save("persist-id", path);
        AgentServer.Instance.Delete("persist-id");
        AgentServer.Instance.Load("persist-id", path);

        var restored = AgentServer.Instance.List().First(a => a.Name == "persist-id");
        Assert.True(AgentIdentity.Verify(restored, "roundtrip", sigBefore));
        try { if (File.Exists(path)) File.Delete(path); } catch { }
    }
}
