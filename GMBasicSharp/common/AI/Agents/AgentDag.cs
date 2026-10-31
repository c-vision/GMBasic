// FILE: Basic.Agents.AgentDag — Fase 3 GMBasic-Agent-Runtime: DAG generico disaccoppiato da
// RecursiveMAS (strato SEPARATO, non tocca TeamOrchestrator.RunDag/training latente). Fan-out
// parallelo a concorrenza bounded; fan-in: ogni nodo riceve i risultati dei suoi DependsOn.
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.Agents;

public sealed class DagNode
{
    public required string Id { get; init; }
    public required string[] DependsOn { get; init; }
    /// <summary>Compute riceve i risultati dei DependsOn concatenati (una riga l'uno).</summary>
    public required Func<string, string> Compute { get; init; }
}

public sealed class DagOutcome
{
    public required bool Ok { get; init; }
    public required IReadOnlyDictionary<string, string> Results { get; init; }
    public string? Error { get; init; }
    public int PeakConcurrency { get; init; }
}

public static class AgentDag
{
    public static DagOutcome Execute(IReadOnlyList<DagNode> nodes, int maxConcurrency = 8)
    {
        GpmWorkerGuard.Check("AgentDag.Execute");
        if (maxConcurrency < 1) maxConcurrency = 1;
        var byId = new Dictionary<string, DagNode>(StringComparer.OrdinalIgnoreCase);
        foreach (var n in nodes)
            if (!byId.TryAdd(n.Id, n))
                return new DagOutcome { Ok = false, Results = new Dictionary<string, string>(), Error = $"id duplicato {n.Id}" };
        // validazione dipendenze + ordine topologico (Kahn)
        foreach (var n in nodes)
            foreach (var d in n.DependsOn)
                if (!byId.ContainsKey(d))
                    return new DagOutcome { Ok = false, Results = new Dictionary<string, string>(), Error = $"dipendenza '{d}' di '{n.Id}' inesistente" };
        var order = new List<string>();
        var indeg = nodes.ToDictionary(n => n.Id, n => n.DependsOn.Length, StringComparer.OrdinalIgnoreCase);
        var queue = new Queue<string>(nodes.Where(n => n.DependsOn.Length == 0).Select(n => n.Id));
        while (queue.Count > 0)
        {
            var id = queue.Dequeue();
            order.Add(id);
            foreach (var other in nodes)
                if (Array.IndexOf(other.DependsOn, id) >= 0)
                    if (--indeg[other.Id] == 0) queue.Enqueue(other.Id);
        }
        if (order.Count != nodes.Count)
            return new DagOutcome { Ok = false, Results = new Dictionary<string, string>(), Error = "ciclo nel DAG" };

        var results = new ConcurrentDictionary<string, string>();
        var taskMap = new ConcurrentDictionary<string, Task>(StringComparer.OrdinalIgnoreCase);
        var gate = new SemaphoreSlim(maxConcurrency);
        int active = 0, peak = 0;
        object peakLock = new();
        foreach (var id in order)
        {
            var node = byId[id];
            var deps = node.DependsOn.Select(d => taskMap[d]).ToArray();
            var t = Task.Run(async () =>
            {
                if (deps.Length > 0) await Task.WhenAll(deps);
                await gate.WaitAsync();
                int cur = Interlocked.Increment(ref active);
                lock (peakLock) if (cur > peak) peak = cur;
                try
                {
                    var sb = new System.Text.StringBuilder();
                    foreach (var d in node.DependsOn) sb.AppendLine(results[d]);
                    results[id] = node.Compute(sb.ToString().TrimEnd());
                }
                finally { Interlocked.Decrement(ref active); gate.Release(); }
            });
            taskMap[id] = t;
        }
        Task.WaitAll(order.Select(id => taskMap[id]).ToArray());
        return new DagOutcome { Ok = true, Results = results, PeakConcurrency = peak };
    }
}
