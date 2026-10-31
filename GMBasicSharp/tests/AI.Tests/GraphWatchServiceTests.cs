// FILE: AI.Tests.GraphWatchServiceTests
// 2026-09-06 — GraphWatchService (parità graphify --watch): il watchdog rigenera il grafo
// quando un file .md cambia/crea/cancella nella cartella wiki (debounce ~1s, prune su delete).
using Basic.AI.Graph;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class GraphWatchServiceTests : IDisposable
{
    private readonly string _wiki = Path.Combine(Path.GetTempPath(), "gmbasic-graphwatch-" + Guid.NewGuid().ToString("N"));
    private readonly string _graphs = Path.Combine(Path.GetTempPath(), "gmbasic-graphwatch-g-" + Guid.NewGuid().ToString("N"));

    public GraphWatchServiceTests()
    {
        Directory.CreateDirectory(_wiki);
        Directory.CreateDirectory(_graphs);
        GraphWatchService.StopAll();
    }

    public void Dispose()
    {
        GraphWatchService.StopAll();
        try { Directory.Delete(_wiki, recursive: true); } catch { }
        try { Directory.Delete(_graphs, recursive: true); } catch { }
    }

    private void WriteNote(string name, string body)
    {
        var path = Path.Combine(_wiki, name);
        if (File.Exists(path)) File.SetLastWriteTimeUtc(path, DateTime.UtcNow.AddSeconds(1)); // forza evento anche se contenuto uguale
        File.WriteAllText(path, $"---\ntitle: {Path.GetFileNameWithoutExtension(name)}\n---\n{body}");
    }

    private int WaitForNodeCount(int expected, int timeoutMs = 8000)
    {
        var stats = Path.Combine(_graphs, "watch.stats.json");
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            if (File.Exists(stats))
            {
                try
                {
                    using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(stats));
                    if (doc.RootElement.TryGetProperty("nodes", out var n) && n.GetInt32() == expected)
                        return n.GetInt32();
                }
                catch { /* rebuild in corso: stats a meta scrittura */ }
            }
            Thread.Sleep(300);
        }
        return -1;
    }

    [Fact(DisplayName = "GraphWatchService: cartella inesistente → errore esplicito")]
    public void Start_MissingDir_Throws()
    {
        var act = () => GraphWatchService.Start(Path.Combine(_wiki, "non-esiste"), _graphs, "watch");
        act.Should().Throw<DirectoryNotFoundException>();
        GraphWatchService.WatchedCount.Should().Be(0);
    }

    [Fact(DisplayName = "GraphWatchService: create file .md rigenera il grafo")]
    public void Watch_Rebuilds_On_Create()
    {
        WriteNote("a.md", "nota a, vedi [[b]]");
        WriteNote("b.md", "nota b, vedi [[a]]");
        // build iniziale manuale (il watch osserva da ora)
        GraphBuildService.Run(new GraphBuildOptions(_wiki, _graphs, "watch"));
        GraphWatchService.Start(_wiki, _graphs, "watch").Should().Be(1);
        GraphWatchService.WatchedCount.Should().Be(1);

        // aggiungo una nota → il watch deve rigenerare a 3 nodi
        WriteNote("c.md", "nota c, vedi [[a]]");
        WaitForNodeCount(3).Should().Be(3);
        GraphWatchService.Stop(_wiki).Should().Be(1);
    }

    [Fact(DisplayName = "GraphWatchService: delete .md → prune (nodi scendono)")]
    public void Watch_Prunes_On_Delete()
    {
        WriteNote("a.md", "nota a, vedi [[b]]");
        WriteNote("b.md", "nota b, vedi [[a]]");
        GraphBuildService.Run(new GraphBuildOptions(_wiki, _graphs, "watch"));
        GraphWatchService.Start(_wiki, _graphs, "watch").Should().Be(1);

        WriteNote("c.md", "nota c, vedi [[a]]");
        WaitForNodeCount(3).Should().Be(3);

        File.Delete(Path.Combine(_wiki, "c.md"));
        WaitForNodeCount(2).Should().Be(2);
        GraphWatchService.Stop(_wiki).Should().Be(1);
    }
}
