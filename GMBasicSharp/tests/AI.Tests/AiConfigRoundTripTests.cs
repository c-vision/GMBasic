// FILE: AI.Tests.AiConfigRoundTripTests
// Bug (solved) - AiConfig-Roundtrip-Perdita-Chiavi-JSON: `AiConfig.Save` ri-serializza il record
// `Settings`; senza [JsonExtensionData] qualunque chiave JSON non mappata veniva PERSA al primo
// salvataggio (incidente reale 2026-09-16: un test non isolato ha riscritto config/ai.json
// cancellando `defaultModel` e `defaultProvider`). Questi test sono SEMPRE isolati
// (AiConfig.OverridePathForTests) e verificano il round-trip completo.
using System.Text.Json;
using Basic.AI;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class AiConfigRoundTripTests : IDisposable
{
    private readonly string _configPath =
        Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-rt-" + Guid.NewGuid().ToString("N") + ".json");

    public AiConfigRoundTripTests()
    {
        AiConfig.OverridePathForTests(_configPath);
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    private string ReadRaw() => File.ReadAllText(_configPath);

    [Fact(DisplayName = "AiConfig: le chiavi JSON non mappate SOPRAVVIVONO a un salvataggio (incidente 2026-09-16)")]
    public void UnknownKeysSurviveSave()
    {
        // config utente "reale": chiavi note + chiavi NON mappate nel record (come defaultModel/defaultProvider)
        File.WriteAllText(_configPath, """
        {
          "modelPaths": ["/models"],
          "skillPaths": ["/skills"],
          "skillsDefaultPath": null,
          "skillsIndexPath": null,
          "pluginsDefaultPath": null,
          "systemPrompt": null,
          "logPath": null,
          "brainsDefaultDir": null,
          "defaultModel": "Qwen3.8-27B-8bit",
          "defaultProvider": "mlx",
          "chiaveFuturaDiUnAltraAI": {"annidato": [1, 2, 3]}
        }
        """);

        // un setter di AiConfig (il percorso che ha causato l'incidente) deve salvare SENZA perdere nulla
        AiConfig.BrainsDefaultDir = Path.Combine(Path.GetTempPath(), "gmbasic-brains-rt");

        var raw = ReadRaw();
        using var doc = JsonDocument.Parse(raw);
        var root = doc.RootElement;

        root.TryGetProperty("defaultModel", out var dm).Should().BeTrue("le chiavi non mappate non devono essere cancellate");
        dm.GetString().Should().Be("Qwen3.8-27B-8bit");
        root.GetProperty("defaultProvider").GetString().Should().Be("mlx");
        root.TryGetProperty("chiaveFuturaDiUnAltraAI", out var fut).Should().BeTrue();
        fut.GetProperty("annidato")[2].GetInt32().Should().Be(3);

        // la modifica richiesta è applicata e le chiavi note restano
        root.GetProperty("brainsDefaultDir").GetString().Should().EndWith("gmbasic-brains-rt");
        root.GetProperty("modelPaths")[0].GetString().Should().Be("/models");
    }

    [Fact(DisplayName = "AiConfig: round-trip Load→Save è stabile (nessuna perdita, nessuna aggiunta)")]
    public void RoundTripIsStable()
    {
        File.WriteAllText(_configPath, """
        { "modelPaths": [], "skillPaths": [], "skillsDefaultPath": null, "skillsIndexPath": null,
          "pluginsDefaultPath": null, "systemPrompt": null, "logPath": null,
          "ragDefaultDir": "/r", "graphPageRankIterations": 42, "defaultModel": "X" }
        """);

        _ = AiConfig.Load();                     // popola la cache (con Extra)
        AiConfig.Save(AiConfig.Load());          // salvataggio identico
        var first = ReadRaw();
        AiConfig.Save(AiConfig.Load());          // idempotente
        ReadRaw().Should().Be(first);

        using var doc = JsonDocument.Parse(first);
        doc.RootElement.GetProperty("graphPageRankIterations").GetInt32().Should().Be(42);
        doc.RootElement.GetProperty("defaultModel").GetString().Should().Be("X");
    }

    [Fact(DisplayName = "AiConfig: file assente → Empty e primo salvataggio crea il file")]
    public void MissingFileThenSaveCreatesIt()
    {
        if (File.Exists(_configPath)) File.Delete(_configPath);
        AiConfig.Load().ModelPaths.Should().BeEmpty();
        AiConfig.AddModelPath("/tmp/modelli");
        File.Exists(_configPath).Should().BeTrue();
        using var doc = JsonDocument.Parse(ReadRaw());
        doc.RootElement.GetProperty("modelPaths")[0].GetString().Should().EndWith("modelli");
    }

    [Fact(DisplayName = "AiConfig: la config REALE del repo sopravvive a un round-trip (copia isolata, mai scritta)")]
    public void RealRepoConfigSurvivesRoundTrip()
    {
        // Guardia: dipende dallo stato del repo — se non c'è, il test non ha nulla da verificare.
        var real = CommonLib.GmbiPaths.AiConfigFile;
        if (!File.Exists(real)) return;
        var before = File.ReadAllText(real);
        if (!before.Contains("defaultModel", StringComparison.Ordinal)) return;

        // Copia in un file temporaneo e round-trip sulla COPIA: la config reale NON viene toccata.
        File.WriteAllText(_configPath, before);
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.Save(AiConfig.Load());

        var after = ReadRaw();
        using var doc = JsonDocument.Parse(after);
        doc.RootElement.TryGetProperty("defaultModel", out _).Should().BeTrue();
        doc.RootElement.TryGetProperty("defaultProvider", out _).Should().BeTrue();
        File.ReadAllText(real).Should().Be(before, "il test lavora su una copia: la config reale resta identica");
    }

    [Fact(DisplayName = "AiConfig: Save non riscrive se il contenuto è identico (mtime invariato)")]
    public void SaveSkipsIdenticalContent()
    {
        File.WriteAllText(_configPath, """{ "modelPaths": ["/m"], "skillPaths": [] }""");
        AiConfig.Save(AiConfig.Load());
        var mtime1 = File.GetLastWriteTimeUtc(_configPath);
        Thread.Sleep(20);
        AiConfig.Save(AiConfig.Load());
        File.GetLastWriteTimeUtc(_configPath).Should().Be(mtime1, "una riscrittura identica del file utente è inutile");
    }
}
