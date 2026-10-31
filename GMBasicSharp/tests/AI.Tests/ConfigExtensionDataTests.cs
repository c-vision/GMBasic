// FILE: AI.Tests.ConfigExtensionDataTests
// Stessa classe del bug (solved) - AiConfig-Roundtrip-Perdita-Chiavi-JSON: TUTTI i `*Config.Save`
// ri-serializzano il proprio record; senza [JsonExtensionData] ogni chiave JSON non mappata veniva
// persa. Qui la verifica per McpConfig (config/mcp.json) e ConnectorConfig (config/connectors.json),
// con config dir TEMPORANEA (mai i file reali del repo).
using System.Text.Json;
using Basic.Connectors;
using Basic.Mcp;
using CommonLib;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class ConfigExtensionDataTests : IDisposable
{
    private readonly string _configDir = Path.Combine(Path.GetTempPath(), "gmbasic-confdir-" + Guid.NewGuid().ToString("N"));

    public ConfigExtensionDataTests()
    {
        Directory.CreateDirectory(_configDir);
        GmbiPaths.OverrideConfigDirForTests(_configDir);
    }

    public void Dispose()
    {
        GmbiPaths.ResetConfigDirOverrideForTests();
        McpConfig.ResetForTests();
        ConnectorConfig.ResetForTests();
        try { Directory.Delete(_configDir, recursive: true); } catch { }
    }

    [Fact(DisplayName = "McpConfig: le chiavi non mappate sopravvivono al salvataggio")]
    public void McpUnknownKeysSurvive()
    {
        var path = Path.Combine(_configDir, "mcp.json");
        File.WriteAllText(path, """
        { "servers": [ { "name": "s1", "kind": "http", "url": "http://127.0.0.1:1", "enabled": true } ],
          "chiaveIgnota": "KEEP", "annidato": { "a": 1 } }
        """);

        McpConfig.Load();                                   // deserializza (chiave ignota → Extra)
        McpConfig.SetEnabled("s1", false);                  // percorso reale di scrittura

        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        doc.RootElement.TryGetProperty("chiaveIgnota", out var k).Should().BeTrue("le chiavi non mappate non vanno cancellate");
        k.GetString().Should().Be("KEEP");
        doc.RootElement.GetProperty("annidato").GetProperty("a").GetInt32().Should().Be(1);
        doc.RootElement.GetProperty("servers")[0].GetProperty("enabled").GetBoolean().Should().BeFalse();
    }

    [Fact(DisplayName = "ConnectorConfig: le chiavi non mappate sopravvivono al salvataggio")]
    public void ConnectorUnknownKeysSurvive()
    {
        var path = Path.Combine(_configDir, "connectors.json");
        File.WriteAllText(path, """{ "connectors": [], "chiaveIgnota": "KEEP" }""");

        var s = ConnectorConfig.Load();
        ConnectorConfig.Save(s);                            // riscrittura identica → nessuna perdita

        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        doc.RootElement.TryGetProperty("chiaveIgnota", out var k).Should().BeTrue();
        k.GetString().Should().Be("KEEP");
        doc.RootElement.TryGetProperty("connectors", out _).Should().BeTrue();
    }
}
