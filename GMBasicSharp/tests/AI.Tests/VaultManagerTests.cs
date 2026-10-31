// FILE: AI.Tests.VaultManagerTests
// Second Brain L1 (W1): VaultManager + VaultIndex su vaults.db.
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class VaultManagerTests : IDisposable
{
    public VaultManagerTests()
    {
        VaultManager.ResetForTests();
    }

    public void Dispose()
    {
        VaultManager.ResetForTests();
    }

    private static string MakeVault(string name, int files)
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb-vault-" + name + "-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(dir, "sub"));
        for (var i = 0; i < files; i++)
            File.WriteAllText(Path.Combine(dir, i < files / 2 ? $"doc{i}.md" : Path.Combine("sub", $"doc{i}.md")), $"# Doc {i}\ncontenuto {i}");
        return dir;
    }

    [Fact]
    public void Create_IndexesMarkdownFiles()
    {
        var dir = MakeVault("create", 4);
        try
        {
            var id = VaultManager.VaultCreate("kb", dir);
            id.Should().BeGreaterThan(0);
            VaultManager.GetIndexedFileCount("kb").Should().Be(4);
            VaultManager.VaultInfo("kb").Should().Contain("\"fileCount\":4");
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void Create_RespectsPattern()
    {
        var dir = MakeVault("pattern", 4);
        try
        {
            VaultManager.VaultCreate("kbpat", dir, "*.md");
            VaultManager.GetIndexedFileCount("kbpat").Should().Be(4);
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void Index_Incremental_AddsAndRemoves()
    {
        var dir = MakeVault("inc", 2);
        try
        {
            VaultManager.VaultCreate("kbinc", dir);
            VaultManager.GetIndexedFileCount("kbinc").Should().Be(2);

            File.WriteAllText(Path.Combine(dir, "extra.md"), "# Extra\nnuovo file");
            VaultManager.VaultIndex("kbinc").Should().Be(3);

            File.Delete(Path.Combine(dir, "doc0.md"));
            VaultManager.VaultIndex("kbinc").Should().Be(2);
            VaultManager.GetIndexedFileCount("kbinc").Should().Be(2);
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void ListFolders_ReturnsRelativeFolders()
    {
        var dir = MakeVault("folders", 4);
        try
        {
            VaultManager.VaultCreate("kbfold", dir);
            var folders = VaultManager.ListFolders("kbfold");
            folders.Should().Contain("/");
            folders.Should().Contain("sub");
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void VaultIndex_ContentHash_And_IsIndexed()
    {
        var dir = MakeVault("index", 2);
        try
        {
            VaultManager.VaultCreate("kbix", dir);
            var file = Path.Combine(dir, "doc0.md");
            VaultIndex.IsIndexed("kbix", file).Should().BeTrue();
            VaultIndex.GetContentHash("kbix", file).Should().NotBeNullOrEmpty();
            VaultIndex.GetFolderPath("kbix", file).Should().Be("/");

            var subFile = Path.Combine(dir, "sub", "doc1.md");
            VaultIndex.IsIndexed("kbix", subFile).Should().BeTrue();
            VaultIndex.GetFolderPath("kbix", subFile).Should().Be("sub");
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void ListVaults_ReturnsRegisteredNames()
    {
        var dir = MakeVault("list", 1);
        try
        {
            VaultManager.VaultCreate("kblist", dir);
            VaultManager.ListVaults().Should().Contain("kblist");
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void Create_NonExistentPath_ReturnsMinusOne()
    {
        VaultManager.VaultCreate("ghost", "/nonexistent/gmb-vault-" + Guid.NewGuid().ToString("N"))
            .Should().Be(-1);
    }

    [Fact]
    public void Info_UnknownVault_ReturnsEmpty()
    {
        VaultManager.VaultInfo("nope").Should().BeEmpty();
    }
}
