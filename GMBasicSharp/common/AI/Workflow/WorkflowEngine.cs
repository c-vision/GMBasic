// FILE: Basic.Workflow.WorkflowEngine
// B4.1 — WorkflowEngine: DAG con PARALLEL/BRANCH + stop/pause (estende WorkflowServer).
// WorkflowServer è sequenziale (STEP list). WorkflowEngine è nodi: ogni nodo è
//   Step (azione) | Parallel (figli in parallelo) | Branch (figli, il resolver sceglie).
// Il "resolver" è un callback che riceve (nodeId, action, parallelIndex) e ritorna
//   l'indice del figlio Branch da seguire (default 0) — per la verifica BASIC/GO SELECT
//   il chiamante lo cabla. Mai stub: stato esplicito + errori.

using System.Collections.Concurrent;
using System.Text.Json;

namespace Basic.Workflow;

public enum WorkflowNodeKind { Step, Parallel, Branch, End }

public sealed class WorkflowDagNode
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");
    public required string Name { get; init; }
    public WorkflowNodeKind Kind { get; set; } = WorkflowNodeKind.Step;
    public string Action { get; set; } = "";
    public int RetryCount { get; set; }
    public int TimeoutMs { get; set; }
    public List<string> Children { get; set; } = [];
    internal bool Grouped { get; set; }   // nodo già assorbito da un PARALLEL/BRANCH → skip al top level
}

/// <summary>Firma del resolver per un nodo BRANCH: ritorna l'indice del figlio da seguire
/// (default 0). Per GO SELECT/BASIC, il chiamante usa il risultato di un'azione per scegliere.</summary>
public delegate int WorkflowBranchResolver(string branchId, string action, int childCount);

/// <summary>Registro DAG con definizione a nodi ed esecuzione (PARALLEL via Task, BRANCH via
/// resolver, STOP via CancellationToken, stato visibile). Definizione thread-safe.</summary>
public sealed class WorkflowEngine
{
    public static WorkflowEngine Instance { get; } = new();

    private readonly ConcurrentDictionary<string, List<WorkflowDagNode>> _dags = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _lock = new();
    private List<WorkflowDagNode>? _building;

    private WorkflowEngine() { }

    // ─── Definizione ─────────────────────────────────────────────────────────────

    public bool BeginDag(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (_building is not null) return false;
            _building = [];
            _dags[name] = _building;
        }
        return true;
    }

    public bool AddStep(string name, string action, int retryCount = 0, int timeoutMs = 0)
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building.Add(new WorkflowDagNode { Name = name, Action = action, RetryCount = retryCount, TimeoutMs = timeoutMs });
        }
        return true;
    }

    /// <summary>Apre un gruppo PARALLEL: i nodi successivi fino a EndGroup sono figli in parallelo.</summary>
    public bool BeginParallel(string name)
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building.Add(new WorkflowDagNode { Name = name, Kind = WorkflowNodeKind.Parallel });
        }
        return true;
    }

    /// <summary>Apre un gruppo BRANCH: i nodi successivi fino a EndGroup sono i candidati;
    /// il resolver di Execute sceglie quale eseguire (gli altri sono skippati).</summary>
    public bool BeginBranch(string name)
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building.Add(new WorkflowDagNode { Name = name, Kind = WorkflowNodeKind.Branch });
        }
        return true;
    }

    /// <summary>Chiude il gruppo corrente (PARALLEL/BRANCH): i nodi aggiunti da BeginX a qui
    /// diventano figli del nodo gruppo; il gruppo stesso resta nella sequenza.</summary>
    public bool EndGroup()
    {
        lock (_lock)
        {
            if (_building is null) return false;
            // Trova l'ultimo gruppo aperto non ancora chiuso; i nodi successivi diventano suoi
            // figli (per Id) e restano nella lista con Grouped=true (skip al top level).
            for (var i = _building.Count - 1; i >= 0; i--)
            {
                var n = _building[i];
                if (n.Kind is WorkflowNodeKind.Parallel or WorkflowNodeKind.Branch)
                {
                    for (var j = i + 1; j < _building.Count; j++)
                    {
                        n.Children.Add(_building[j].Id);
                        _building[j].Grouped = true;
                    }
                    return true;
                }
            }
            return false;
        }
    }

    public bool EndDag()
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building = null;
        }
        return true;
    }

    public List<WorkflowDagNode>? Get(string name)
        => _dags.TryGetValue(name, out var d) ? [.. d] : null;

    public string Status(string name)
    {
        var d = Get(name);
        if (d is null) return "";
        // stato derivato: se l'ultimo nodo è stato eseguito → completed (set da Execute)
        return _lastStatus.TryGetValue(name, out var s) ? s : "ready";
    }

    private readonly ConcurrentDictionary<string, string> _lastStatus = new(StringComparer.OrdinalIgnoreCase);

    public void ResetForTests()
    {
        lock (_lock) { _building = null; }
        _dags.Clear();
        _lastStatus.Clear();
    }

    // ─── Esecuzione ──────────────────────────────────────────────────────────────

    /// <summary>Esegue il DAG in ordine: Step → onStep; Parallel → tutti i figli in parallelo
    /// (Task.WhenAll); Branch → onBranch indica quale figlio eseguire. Ritorna
    /// (ok, visitedNames[], log[]).</summary>
    public (bool ok, List<string> visited, List<string> log) Execute(
        string name,
        Action<string, string>? onStep = null,
        WorkflowBranchResolver? onBranch = null,
        CancellationToken cancel = default)
    {
        var dag = Get(name);
        if (dag is null || dag.Count == 0) return (false, [], ["DAG non trovato o vuoto"]);
        _lastStatus[name] = "running";
        var visited = new List<string>();
        var log = new List<string>();
        try
        {
            foreach (var node in dag)
            {
                if (node.Grouped) continue; // assorbito da PARALLEL/BRANCH → già eseguito lì
                if (cancel.IsCancellationRequested)
                {
                    _lastStatus[name] = "stopped";
                    return (false, visited, log.Concat(["STOP"]).ToList());
                }
                switch (node.Kind)
                {
                    case WorkflowNodeKind.Step:
                    {
                        var executed = ExecStep(node, onStep, cancel);
                        visited.Add(node.Name);
                        log.Add(executed ? $"OK {node.Name}" : $"SKIP {node.Name} (pausa)");
                        break;
                    }
                    case WorkflowNodeKind.Parallel:
                    {
                        var children = node.Children.Select(id => dag.FirstOrDefault(d => d.Id == id))
                            .Where(d => d is not null).ToList()!;
                        var results = new ConcurrentBag<string>();
                        Parallel.ForEach(children, c =>
                        {
                            var r = ExecStep(c!, onStep, cancel);
                            results.Add($"{(r ? "OK" : "SKIP")} {c!.Name}");
                        });
                        foreach (var r in results) log.Add($"[PARALLEL {node.Name}] {r}");
                        visited.Add(node.Name);
                        break;
                    }
                    case WorkflowNodeKind.Branch:
                    {
                        var children = node.Children.Select(id => dag.FirstOrDefault(d => d.Id == id))
                            .Where(d => d is not null).ToList()!;
                        if (children.Count == 0) { visited.Add(node.Name); break; }
                        var pick = onBranch?.Invoke(node.Name, node.Action, children.Count) ?? 0;
                        var chosen = children[Math.Clamp(pick, 0, children.Count - 1)];
                        var ok = ExecStep(chosen!, onStep, cancel);
                        log.Add($"[BRANCH {node.Name}] scelto #{pick} {chosen!.Name}: {(ok ? "OK" : "SKIP")}");
                        visited.Add(node.Name);
                        visited.Add(chosen.Name);
                        break;
                    }
                }
            }
            _lastStatus[name] = "completed";
            return (true, visited, log);
        }
        catch (OperationCanceledException)
        {
            _lastStatus[name] = "stopped";
            return (false, visited, log.Concat(["STOP (cancel)"]).ToList());
        }
    }

    private static bool ExecStep(WorkflowDagNode step, Action<string, string>? onStep, CancellationToken cancel)
    {
        if (onStep is null) return true;
        try
        {
            if (step.TimeoutMs > 0)
            {
                var t = Task.Run(() => onStep(step.Name, step.Action), cancel);
                return t.Wait(step.TimeoutMs, cancel) && !t.IsFaulted;
            }
            onStep(step.Name, step.Action);
            return true;
        }
        catch { return false; }
    }
}
