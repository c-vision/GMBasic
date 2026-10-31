// FILE: AI.Tests.AcpProtocolTests
using Basic.Agents;
using Basic.IlBackend;
using Basic.Runtime;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Integration")]
public sealed class AcpProtocolTests : IDisposable
{
    private readonly string _config = Path.Combine(Path.GetTempPath(), "gmb_acp_" + Guid.NewGuid().ToString("N") + ".json");

    public AcpProtocolTests()
    {
        Basic.AI.AiConfig.OverridePathForTests(_config);
        AgentServer.ResetForTests();
    }
    public void Dispose()
    {
        try { IlRuntime.AiAgentStopServer(); } catch { }
        AgentServer.ResetForTests();
        Basic.AI.AiConfig.ResetForTests();
        if (File.Exists(_config)) File.Delete(_config);
    }

    private static int NextPort()
    {
        var l = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        l.Start();
        var p = ((System.Net.IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return p;
    }

    [Fact]
    public void Send_ToRemoteAgentServer_ReturnsAnswer()
    {
        var port = NextPort();
        AgentServer.Instance.Create("RemoteAgent");
        AgentServer.Instance.Set("RemoteAgent", "systemPrompt", "You are remote.");
        IlRuntime.SetCurrent(new HeadlessRuntimeHost(Console.Out, new RuntimeConfig()));
        Assert.Equal(1.0, IlRuntime.AiAgentServe(port));
        System.Threading.Thread.Sleep(400);

        var baseUrl = $"http://127.0.0.1:{port}";
        Assert.True(AcpProtocol.Ping(baseUrl), "server remoto deve rispondere su /api/agents");

        var answer = AcpProtocol.Send("Caller", "RemoteAgent", baseUrl, "Saluta", "ciao remoto");
        // la risposta è JSON dal /execute del server remoto, contiene l'answer dell'agente remoto
        Assert.Contains("answer", answer);
        Assert.Contains("ciao remoto", answer);
    }

    [Fact]
    public void Send_ToUnreachable_ThrowsExplicit()
    {
        // porta chiusa → errore esplicito (mai hang: HttpClient timeout 60s ma connection refused è immediato)
        var ex = Assert.Throws<InvalidOperationException>(() =>
            AcpProtocol.Send("A", "B", "http://127.0.0.1:1", "t", "i"));
        Assert.Contains("non raggiungibile", ex.Message);
    }
}
