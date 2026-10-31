// FILE: AI.Tests.BrainRegistryTests
// Second Brain: BrainRegistry (USE/DETACH/LIST/INFO/available) + BrainSigner (probe minimi)
// + BrainInstaller (import zip + validazione manifest/checksum + rollback). Isolato dalla
// config utente reale (AiConfig + BrainRegistry overridden su path temporanei).
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
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    private static void MakeVault(string dir, int files = 3)
    {
        Directory.CreateDirectory(dir);
        for (var i = 0; i < files; i++)
            File.WriteAllText(Path.Combine(dir, $"doc{i}.md"), $"# Doc {i}\nlink [[doc{(i + 1) % files}]]\n");
    }

    [Fact]
    public void Use_UnknownBrain_ReturnsFalse()
    {
        BrainRegistry.Use("ghost").Should().BeFalse();
    }

    [Fact]
    public void Sign_UserVaultMarkdown_AvailableAsGraph()
    {
        var vault = Path.Combine(_brainsRoot, "vault-src");
        MakeVault(vault);

        var result = BrainSigner.Sign("mywiki", vault);

        result.Valid.Should().BeTrue();
        result.Available.Should().Contain("graph");
        result.Probe.Should().ContainKey("graph");
        BrainRegistry.IsAvailableAsGraph("mywiki").Should().BeTrue();
        BrainRegistry.Use("mywiki").Should().BeTrue();
        BrainRegistry.Used().Should().Contain("mywiki");
        var info = BrainRegistry.Info("mywiki");
        info.Should().NotBeNull();
        info!.Available.Should().Contain("graph");
        info.Checksum.Should().NotBeNullOrEmpty();
        info.SignedBy.Should().Be("gmb-sign");
    }

    [Fact]
    public void Sign_EmptyDir_NotValid_WithReport()
    {
        var empty = Path.Combine(_brainsRoot, "empty");
        Directory.CreateDirectory(empty);

        var result = BrainSigner.Sign("emptybrain", empty);

        result.Valid.Should().BeFalse();
        result.Probe.Should().ContainKey("graph");
    }

    [Fact]
    public void Sign_MissingDir_NotValid()
    {
        var result = BrainSigner.Sign("ghost", Path.Combine(_brainsRoot, "nope"));
        result.Valid.Should().BeFalse();
    }

    [Fact]
    public void Install_ValidZip_RoundTrip()
    {
        var vault = Path.Combine(_brainsRoot, "vault-rt");
        MakeVault(vault);
        var sign = BrainSigner.Sign("rtbrain", vault);
        sign.Valid.Should().BeTrue();

        // export dello zip dal brain firmato (root import ha manifest + graphs/<name>.json)
        var zipPath = Path.Combine(_brainsRoot, "rtbrain.gmbrain.zip");
        BrainSigner.ExportSignedZip("rtbrain", zipPath).Should().BeTrue();

        var dest = Path.Combine(_brainsRoot, "imported");
        var install = BrainInstaller.Install("rtbrain", zipPath, dest, activate: true);

        install.Ok.Should().BeTrue();
        install.InstallRoot.Should().Be(dest);
        install.Available.Should().Contain("graph");
        Directory.Exists(Path.Combine(dest, "graphs")).Should().BeTrue();
        File.Exists(Path.Combine(dest, "brain.manifest.json")).Should().BeTrue();
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

    [Fact]
    public void Detach_KeepsFiles()
    {
        var vault = Path.Combine(_brainsRoot, "vault-det");
        MakeVault(vault);
        BrainSigner.Sign("detbrain", vault);
        BrainRegistry.Use("detbrain");

        BrainRegistry.Detach("detbrain").Should().BeTrue();
        BrainRegistry.Used().Should().NotContain("detbrain");
        BrainRegistry.IsAvailableAsGraph("detbrain").Should().BeTrue();
    }

    [Fact]
    public void List_ReturnsRegisteredAndAvailable()
    {
        var vault = Path.Combine(_brainsRoot, "vault-list");
        MakeVault(vault);
        BrainSigner.Sign("listbrain", vault);

        BrainRegistry.List().Should().Contain("listbrain");
    }

    [Fact]
    public void AvailableModes_Empty_ForGhost()
    {
        BrainRegistry.AvailableModes("ghost").Should().BeEmpty();
    }
}
