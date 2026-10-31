// FILE: AI.Tests.FileWatcherTests
// F7 P2 — FileWatcher: policy AUTO reindicizza on-change (create/update/delete/rename),
// policy MANUAL resta manuale. VaultManager usa DbPath fisso (~/.gmbasic/vaults.db): come
// VaultManagerTests, ResetForTests isola il db reale; il watcher va stoppato PRIMA di
// cancellare la dir (thread async).
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class FileWatcherTests : IDisposable
{
    private readonly string _vaultDir = Path.Combine(Path.GetTempPath(), "gmbasic-watch-" + Guid.NewGuid().ToString("N"));

    public FileWatcherTests()
    {
        VaultManager.ResetForTests();
        Directory.CreateDirectory(_vaultDir);
        FileWatcher.StopAll();
    }

    public void Dispose()
    {
        // STOP il watcher PRIMA di cancellare la dir: un evento async in volo chiamerebbe
        // VaultManager mentre il db/la dir sono stati rimossi (race con altri test).
        FileWatcher.StopAll();
        try { Directory.Delete(_vaultDir, recursive: true); } catch { }
        VaultManager.ResetForTests();
    }

    [Fact]
    public void Policy_DefaultsToManual()
    {
        VaultManager.VaultCreate("wv", _vaultDir);
        FileWatcher.GetPolicy("wv").Should().Be(VaultIndexingPolicy.Manual);
    }

    [Fact]
    public void SetPolicy_Auto_StartsWatcher()
    {
        VaultManager.VaultCreate("wv", _vaultDir);
        FileWatcher.SetPolicy("wv", VaultIndexingPolicy.Auto).Should().Be(1);
        FileWatcher.GetPolicy("wv").Should().Be(VaultIndexingPolicy.Auto);
        FileWatcher.WatchedCount.Should().Be(1);
    }

    [Fact]
    public void SetPolicy_AutoOnUnknownVault_Throws()
    {
        Assert.Throws<ArgumentException>(() => FileWatcher.SetPolicy("ghost", VaultIndexingPolicy.Auto));
    }

    [Fact]
    public void Auto_ReindexesCreatedFile()
    {
        VaultManager.VaultCreate("wv", _vaultDir);
        VaultManager.VaultIndex("wv");
        FileWatcher.SetPolicy("wv", VaultIndexingPolicy.Auto);
        var file = Path.Combine(_vaultDir, "note.md");
        File.WriteAllText(file, "contenuto");

        // l'evento on-change è async: dà tempo al watcher di processare
        var deadline = DateTime.UtcNow.AddSeconds(5);
        while (VaultManager.GetIndexedFileCount("wv") == 0 && DateTime.UtcNow < deadline)
            System.Threading.Thread.Sleep(100);
        VaultManager.GetIndexedFileCount("wv").Should().BeGreaterThan(0);
    }

    [Fact]
    public void Manual_DoesNotReindexCreatedFile()
    {
        VaultManager.VaultCreate("wv", _vaultDir);
        VaultManager.VaultIndex("wv");
        // policy MANUAL (default): nessun watcher → il file nuovo NON entra nell'indice
        var file = Path.Combine(_vaultDir, "note.md");
        File.WriteAllText(file, "contenuto");
        System.Threading.Thread.Sleep(300);
        VaultManager.GetIndexedFileCount("wv").Should().Be(0);
    }

    [Fact]
    public void IndexFile_UpsertsHash()
    {
        VaultManager.VaultCreate("wv", _vaultDir);
        var file = Path.Combine(_vaultDir, "a.md");
        File.WriteAllText(file, "v1");
        VaultManager.IndexFile("wv", file).Should().Be(1);
        VaultManager.GetIndexedFileCount("wv").Should().Be(1);

        File.WriteAllText(file, "v2 aggiornato");
        VaultManager.IndexFile("wv", file).Should().Be(1);
        VaultManager.GetIndexedFileCount("wv").Should().Be(1);

        // file sparito → rimosso dall'indice
        File.Delete(file);
        VaultManager.IndexFile("wv", file).Should().Be(0);
        VaultManager.GetIndexedFileCount("wv").Should().Be(0);
    }
}
