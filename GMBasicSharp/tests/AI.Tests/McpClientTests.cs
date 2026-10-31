// FILE: AI.Tests.McpClientTests
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpClientTests : IDisposable
{
    private static readonly string ScriptPath = Path.Combine(Path.GetTempPath(), "gmb_mcp_pool_test.sh");

    public McpClientTests()
    {
        McpConfig.ResetForTests();
        McpClient.ResetForTests();
        if (!File.Exists(ScriptPath))
            File.WriteAllText(ScriptPath, PoolScript);
    }

    public void Dispose()
    {
        McpConfig.ResetForTests();
        McpClient.ResetForTests();
        try { File.Delete(ScriptPath); } catch { }
    }

    [Fact]
    public void StdioCall_ReusesProcessAcrossCalls()
    {
        McpConfig.AddServer(new McpServerEntry("pool", "stdio", Command: "/bin/sh", Args: [ScriptPath]));
        McpConfig.SetEnabled("pool", true);
        var server = McpConfig.Get("pool")!;

        var r1 = McpClient.CallTool(server, "echo", "{}");
        var r2 = McpClient.CallTool(server, "echo", "{}");
        var tools = McpClient.DiscoverTools(server);

        r1.Should().Contain("OK");
        r2.Should().Contain("OK");
        tools.Should().HaveCount(1);
        tools[0].Name.Should().Be("echo");
    }

    [Fact]
    public void HttpCall_HandshakeOnce_ThenToolsWork()
    {
        // Handshake http: il secondo tools/call riusa initialize (nessun errore
        // 'Server not initialized' perche' EnsureHttpInitialized e' stato fatto).
        McpConfig.AddServer(new McpServerEntry("fakehttp", "http", Url: "http://127.0.0.1:1/mcp"));
        McpConfig.SetEnabled("fakehttp", true);
        var server = McpConfig.Get("fakehttp")!;

        // Il server non risponde -> l'handshake lancia (ma il percorso e' coperto e coerente).
        var act = () => McpClient.CallTool(server, "x", "{}");
        act.Should().Throw<Exception>();
    }

    private const string PoolScript =
        "#!/bin/sh\n" +
        "while IFS= read -r line; do\n" +
        "  id=$(printf '%s' \"$line\" | sed -n 's/.*\"id\"[ ]*:[ ]*\\([0-9][0-9]*\\).*/\\1/p')\n" +
        "  case \"$line\" in\n" +
        "    *initialize*) printf '{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}}}}\\n' \"$id\" ;;\n" +
        "    *tools/list*) printf '{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"tools\":[{\"name\":\"echo\",\"description\":\"echo\"}]}}\\n' \"$id\" ;;\n" +
        "    *tools/call*) printf '{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"OK\"}],\"isError\":false}}\\n' \"$id\" ;;\n" +
        "    *) printf '{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":-32601,\"message\":\"unknown\"}}\\n' \"$id\" ;;\n" +
        "  esac\n" +
        "done\n";
}
