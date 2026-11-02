// FILE: AI.Tests.BrainRegistryTests
// Second Brain: BrainRegistry (USE/DETACH/LIST/INFO/available) + BrainInstaller (import zip +
// validazione manifest/checksum + rollback). Isolato dalla config utente reale (AiConfig +
// BrainRegistry overridden su path temporanei).
//
// NOTA (F9d, 2026-11-02): export/sign e il toolchain (stats/verify/create/dedupe/…)
// vivono ORA nel mondo C (`gmbadmin brain …`, runtime `gmb_brain_tool_*`). I test di quelle
// funzioni sono in `tools/sh/brain_c_smoke.sh` (stessa copertura, sul binario reale).
using Basic.AI;
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class BrainRegistryTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N") + ".json");

    public BrainRegistryTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.RagDefaultDir = Path.Combine(_brainsRoot, "rag-legacy");
        AiConfig.GraphsDefaultDir = Path.Combine(_brainsRoot, "graphs-legacy");
        BrainRegistry.OverridePathForTests(_registryPath);
        // Isola dal resolver C (cache ~/.gmbasic/brains della macchina).
        CommonLib.NativeBrains.OverrideRootsForTests(_brainsRoot);
    }

    public void Dispose()
    {
        CommonLib.NativeBrains.OverrideRootsForTests(null);
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Crea un cervello "installato" (graphs/&lt;name&gt;.json) sotto la root isolata.</summary>
    private void MakeInstalledBrain(string name)
    {
        var dir = Path.Combine(_brainsRoot, name, "graphs");
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, name + ".json"),
            "{\"nodes\":[{\"id\":\"a.md\",\"title\":\"a\"}],\"edges\":[],\"dangling\":[],\"folderCounts\":{}}");
        BrainRegistry.Register(name, source: null, vaultPath: Path.Combine(_brainsRoot, name), mode: "graph");
    }

    [Fact]
    public void Use_UnknownBrain_ReturnsFalse()
    {
        BrainRegistry.Use("ghost").Should().BeFalse();
    }

    [Fact]
    public void Use_InstalledBrain_Attaches_DetachKeepsFiles()
    {
        MakeInstalledBrain("mywiki");

        BrainRegistry.IsAvailableAsGraph("mywiki").Should().BeTrue();
        BrainRegistry.Use("mywiki").Should().BeTrue();
        BrainRegistry.Used().Should().Contain("mywiki");

        var info = BrainRegistry.Info("mywiki");
        info.Should().NotBeNull();
        info!.Available.Should().Contain("graph");

        BrainRegistry.Detach("mywiki").Should().BeTrue();
        BrainRegistry.Used().Should().NotContain("mywiki");
        BrainRegistry.IsAvailableAsGraph("mywiki").Should().BeTrue();
    }

    [Fact]
    public void List_ReturnsRegisteredAndAvailable()
    {
        MakeInstalledBrain("listbrain");
        BrainRegistry.List().Should().Contain("listbrain");
    }

    [Fact]
    public void AvailableModes_Empty_ForGhost()
    {
        BrainRegistry.AvailableModes("ghost").Should().BeEmpty();
    }

    [Fact]
    public void Install_CorruptZip_Fails_NoDirectory()
    {
        Directory.CreateDirectory(_brainsRoot);
        var zipPath = Path.Combine(_brainsRoot, "corrupt.zip");
        File.WriteAllText(zipPath, "not a zip at all");
        var dest = Path.Combine(_brainsRoot, "corrupt-install");

        var install = BrainInstaller.Install("corrupt", zipPath, dest);

        install.Ok.Should().BeFalse();
        install.Error.Should().NotBeNullOrEmpty();
        Directory.Exists(dest).Should().BeFalse();
    }
}
