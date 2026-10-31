// FILE: AI.Tests.VaultScaleBenchmarkTests
// Benchmark di scala Second Brain (TODO vault: Benchmark-SecondBrain-27k-Scala-Parita):
// VaultManager C# (VaultCreate/VaultIndex) su AiFinanceWiki (~17k .md) — confronto col
// percorso C (native/gmb_runtime_c/tests/bench_vault_27k.c). NON va in CI: ~30-60s.
// Env-gated: salta se il vault AiFinanceWiki non esiste (GMB_BENCH_FINANCE_DIR override).
using System.Diagnostics;
using Basic.AI.Knowledge;
using Xunit;
using Xunit.Abstractions;

namespace AI.Tests;

[Trait("Category", "Benchmark")]
public sealed class VaultScaleBenchmarkTests(ITestOutputHelper output)
{
    [Fact]
    public void VaultIndex_AiFinanceWiki_Scale()
    {
        // Nessun path personale: il vault si indica con GMB_BENCH_FINANCE_DIR, altrimenti si salta.
        var root = Environment.GetEnvironmentVariable("GMB_BENCH_FINANCE_DIR");
        if (string.IsNullOrWhiteSpace(root) || !Directory.Exists(root))
        {
            output.WriteLine($"SKIP: vault '{root}' non trovato (imposta GMB_BENCH_FINANCE_DIR)");
            return;
        }
        var mdCount = Directory.EnumerateFiles(root, "*.md", SearchOption.AllDirectories).Count();
        var tempConfig = Path.Combine(Path.GetTempPath(), $"gmb-vaultbench-{Guid.NewGuid():N}");
        CommonLib.GmbiPaths.OverrideConfigDirForTests(tempConfig);
        VaultManager.ResetForTests();
        const string name = "bench-finance-cs";
        try
        {
            var swCreate = Stopwatch.StartNew();
            var id = VaultManager.VaultCreate(name, root, "*.md");
            swCreate.Stop();
            Assert.True(id > 0, $"VaultCreate fallito (id={id})");

            var count = VaultManager.GetIndexedFileCount(name);
            output.WriteLine($"C# vault create: {swCreate.Elapsed.TotalSeconds:F2}s fileCount={count} (md={mdCount})");
            output.WriteLine($"C# info: {VaultManager.VaultInfo(name)}");

            // re-index (incrementale: dovrebbe essere ~istantaneo, file invariati)
            var swIdx = Stopwatch.StartNew();
            var n = VaultManager.VaultIndex(name);
            swIdx.Stop();
            output.WriteLine($"C# vault reindex: {swIdx.Elapsed.TotalSeconds:F2}s files={n}");

            Assert.Equal(mdCount, count);
        }
        finally
        {
            VaultManager.ResetForTests();
            CommonLib.GmbiPaths.ResetConfigDirOverrideForTests();
        }
    }
}
