// FILE: Basic.Agents.AgentBatch — Fase 1 GMBasic-Agent-Runtime (lato managed/IL): esegue N task
// agente/logici con concorrenza BOUNDED (≤ maxConcurrency), senza un thread OS per agente. Nel
// backend IL il meccanismo async è il thread pool .NET (Task.Run): un SemaphoreSlim limita quanti
// task girano in parallelo. Il punto "BLOCKED" è l'await sul semaforo, non l'occupazione di un
// thread per agente. (Nel mondo C la controparte è gmb_scheduler N worker — due implementazioni
// indipendenti, mai bridge C↔C#.)
using System;
using System.Collections.Concurrent;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.Agents;

public static class AgentBatch
{
    /// <summary>Risultato di un batch.</summary>
    public sealed class BatchResult<T>
    {
        public required T[] Results { get; init; }
        public int PeakConcurrency { get; init; }
        public double TotalMs { get; init; }
    }

    /// <summary>Esegue `worker(index)` su ogni item di [0,n) con concorrenza ≤ maxConcurrency.
    /// Ritorna i risultati in ordine + il picco di concorrenza osservato.</summary>
    public static BatchResult<T> Run<T>(int n, Func<int, T> worker, int maxConcurrency = 8)
    {
        GpmWorkerGuard.Check("AgentBatch.Run (AI.AGENTSWARM)");
        if (maxConcurrency < 1) maxConcurrency = 1;
        var results = new T[n];
        using var gate = new SemaphoreSlim(maxConcurrency);
        int active = 0, peak = 0;
        var peakLock = new object();
        var tasks = new Task[n];
        var sw = System.Diagnostics.Stopwatch.StartNew();
        for (int i = 0; i < n; i++)
        {
            var idx = i;
            tasks[i] = Task.Run(() =>
            {
                gate.Wait();
                int cur = Interlocked.Increment(ref active);
                lock (peakLock) if (cur > peak) peak = cur;
                try { results[idx] = worker(idx); }
                finally { Interlocked.Decrement(ref active); gate.Release(); }
            });
        }
        Task.WaitAll(tasks);
        sw.Stop();
        return new BatchResult<T> { Results = results, PeakConcurrency = peak, TotalMs = sw.Elapsed.TotalMilliseconds };
    }
}
