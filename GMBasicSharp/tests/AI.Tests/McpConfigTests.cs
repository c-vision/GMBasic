// FILE: AI.Tests.McpConfigTests
using Basic.Mcp;
using CommonLib;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpConfigTests : IDisposable
{
    // Isolamento (stessa lezione del bug AiConfig 2026-08-28 / 2026-09-16): questi test mutavano il
    // file MCP REALE (config/mcp.json) affidandosi al ripristino in `finally`. Ora la config dir è
    // temporanea: un run ucciso a metà non può più inquinare i file utente.
    private readonly string _configDir = Path.Combine(Path.GetTempPath(), "gmbasic-mcpconf-" + Guid.NewGuid().ToString("N"));

    public McpConfigTests()
    {
        Directory.CreateDirectory(_configDir);
        GmbiPaths.OverrideConfigDirForTests(_configDir);
    }

    public void Dispose()
    {
        GmbiPaths.ResetConfigDirOverrideForTests();
        McpConfig.ResetForTests();
        try { Directory.Delete(_configDir, recursive: true); } catch { }
    }
    [Fact]
    public void AddRemoveServer_PersistsInList()
    {
        var before = McpConfig.Load();
        try
        {
            McpConfig.AddServer(new McpServerEntry("test-server", "http", Url: "http://127.0.0.1:1/mcp"));
            McpConfig.Get("test-server").Should().NotBeNull();
            McpConfig.Get("test-server")!.Url.Should().Be("http://127.0.0.1:1/mcp");
            McpConfig.Get("test-server")!.Enabled.Should().BeTrue();

            McpConfig.RemoveServer("test-server").Should().BeTrue();
            McpConfig.Get("test-server").Should().BeNull();
        }
        finally
        {
            McpConfig.Save(before);
        }
    }

    [Fact]
    public void SetEnabled_TogglesWithoutRemoving()
    {
        var before = McpConfig.Load();
        try
        {
            McpConfig.AddServer(new McpServerEntry("toggler", "http", Url: "http://127.0.0.1:1/mcp"));
            McpConfig.SetEnabled("toggler", false).Should().BeTrue();
            McpConfig.Get("toggler")!.Enabled.Should().BeFalse();
            McpConfig.SetEnabled("toggler", true).Should().BeTrue();
            McpConfig.Get("toggler")!.Enabled.Should().BeTrue();
            McpConfig.RemoveServer("toggler");
        }
        finally
        {
            McpConfig.Save(before);
        }
    }

    [Fact]
    public void AddServer_DuplicateNameIsRejected()
    {
        var before = McpConfig.Load();
        try
        {
            var first = new McpServerEntry("dup", "http", Url: "http://a");
            McpConfig.AddServer(first).Should().BeTrue();
            McpConfig.AddServer(new McpServerEntry("dup", "http", Url: "http://b")).Should().BeFalse();
            McpConfig.Get("dup")!.Url.Should().Be("http://a");
            McpConfig.RemoveServer("dup");
        }
        finally
        {
            McpConfig.Save(before);
        }
    }
}
