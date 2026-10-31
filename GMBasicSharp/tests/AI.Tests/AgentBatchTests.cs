using System;
using System.Threading;
using System.Threading.Tasks;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class AgentBatchTests
{
    [Fact]
    public void BoundedConcurrency_PeakNotExceedingCap_AllResultsOk()
    {
        // 100 task logici (simulano 100 agenti che aspettano I/O ~50ms) con cap 6.
        // Il picco di concorrenza NON deve superare 6: NON un thread/agente.
        var r = AgentBatch.Run(100, i => { Thread.Sleep(40); return "r" + i; }, maxConcurrency: 6);
        Assert.Equal(100, r.Results.Length);
        Assert.Equal("r0", r.Results[0]);
        Assert.Equal("r99", r.Results[99]);
        Assert.True(r.PeakConcurrency <= 6, $"picco {r.PeakConcurrency} > cap 6");
        // seriazione: 100*40ms/6 ≈ 667ms; con un thread/agente sarebbe ~40ms ma 100 thread.
        Assert.True(r.TotalMs >= 500, $"troppo veloce per cap 6: {r.TotalMs}ms");
    }
}
