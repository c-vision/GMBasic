// FILE: Basic.Workflow.WorkflowServer
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.Workflow;

/// <summary>Passo di un workflow: nome + azione (stringa, es. nome di un tool/procedura).</summary>
public sealed record WorkflowStep(string Name, string Action, int RetryCount = 0, int TimeoutMs = 0);

/// <summary>Definizione di un workflow: sequenza di passi + stato di esecuzione.</summary>
public sealed class WorkflowDefinition
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");
    public required string Name { get; init; }
    public List<WorkflowStep> Steps { get; set; } = [];
    public string Status { get; set; } = "defined";
    public int CurrentStep { get; set; } = -1;
    public List<string> ExecutionLog { get; set; } = [];
    public bool IsBuilding { get; set; } = true;
}

/// <summary>Registry in-process dei workflow: definizione (WORKFLOW/STEP/END) ed esecuzione
/// (EXECUTE). Complementa TeamServer: i workflow orchestrano PASSI (azioni), i team orchestrano AGENTI.</summary>
public sealed class WorkflowServer
{
    public static WorkflowServer Instance { get; } = new();

    private readonly Dictionary<string, WorkflowDefinition> _workflows = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _lock = new();
    private WorkflowDefinition? _building;

    private WorkflowServer() { }

    // ─── Definizione ─────────────────────────────────────────────────────────────

    public bool BeginDefinition(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (_building is not null) return false; // già in definizione
            var wf = new WorkflowDefinition { Name = name };
            _workflows[name] = wf;
            _building = wf;
        }
        return true;
    }

    public bool AddStep(string name, string action, int retryCount = 0, int timeoutMs = 0)
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building.Steps.Add(new WorkflowStep(name, action, retryCount, timeoutMs));
        }
        return true;
    }

    public bool EndDefinition()
    {
        lock (_lock)
        {
            if (_building is null) return false;
            _building.IsBuilding = false;
            _building.Status = _building.Steps.Count > 0 ? "ready" : "empty";
            _building = null;
        }
        return true;
    }

    public bool AddStepToWorkflow(string workflowName, string stepName, string action, int retryCount = 0, int timeoutMs = 0)
    {
        if (string.IsNullOrWhiteSpace(workflowName) || string.IsNullOrWhiteSpace(stepName)) return false;
        lock (_lock)
        {
            // Se è in costruzione (BASIC AI WORKFLOW ... END) → usa il builder corrente
            if (_building is not null && _building.Name.Equals(workflowName, StringComparison.OrdinalIgnoreCase))
            {
                _building.Steps.Add(new WorkflowStep(stepName, action ?? "", retryCount, timeoutMs));
                return true;
            }
            if (!_workflows.TryGetValue(workflowName, out var wf) || wf.IsBuilding) return false;
            wf.Steps.Add(new WorkflowStep(stepName, action ?? "", retryCount, timeoutMs));
            if (wf.Status is "empty" or "completed" or "failed") wf.Status = "ready";
        }
        return true;
    }

    // ─── Interrogazione ──────────────────────────────────────────────────────────

    public List<WorkflowDefinition> List()
    {
        lock (_lock) return [.. _workflows.Values];
    }

    public WorkflowDefinition? Get(string name)
    {
        lock (_lock) return _workflows.TryGetValue(name, out var w) ? w : null;
    }

    public string GetStatus(string name)
    {
        lock (_lock) return _workflows.TryGetValue(name, out var w) ? w.Status : "";
    }

    public string GetSteps(string name)
    {
        lock (_lock) return _workflows.TryGetValue(name, out var w) ? string.Join(",", w.Steps.Select(s => s.Name)) : "";
    }

    public bool Delete(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return false;
        lock (_lock)
        {
            if (_building is not null && _building.Name.Equals(name, StringComparison.OrdinalIgnoreCase)) return false; // in costruzione → non cancellare
            return _workflows.Remove(name);
        }
    }

    // ─── Esecuzione ──────────────────────────────────────────────────────────────

    /// <summary>Esegue il workflow: per ogni passo registra l'esecuzione nello log con
    /// semantica retry/timeout. Le azioni sono stringhe (nome di tool/procedura) — la VM può
    /// estenderlo per chiamare procedure reali; il base esegue i passi in sequenza e logga.
    /// retry = numero di tentativi aggiuntivi dopo il primo fallimento (totale = retry+1).
    /// timeoutMs = 0 → nessun limite; &gt;0 → se onStep supera il limite, si considera timeout
    /// e si ritenta fino ad esaurimento retry, poi il workflow va in "failed".</summary>
    public bool Execute(string name, Action<string, string>? onStep = null)
    {
        WorkflowDefinition wf;
        List<WorkflowStep> steps;
        lock (_lock)
        {
            if (!_workflows.TryGetValue(name, out wf!) || wf.IsBuilding) return false;
            wf.Status = "running";
            wf.ExecutionLog.Clear();
            steps = [.. wf.Steps];
        }

        for (var i = 0; i < steps.Count; i++)
        {
            var step = steps[i];
            lock (_lock) { wf.CurrentStep = i; }

            var succeeded = false;
            Exception? lastEx = null;
            var maxAttempts = Math.Max(1, step.RetryCount + 1);
            for (var attempt = 0; attempt < maxAttempts; attempt++)
            {
                var attemptLabel = maxAttempts > 1 ? $" attempt {attempt + 1}/{maxAttempts}" : "";
                try
                {
                    if (onStep is not null)
                    {
                        if (step.TimeoutMs > 0)
                        {
                            var task = System.Threading.Tasks.Task.Run(() => onStep(step.Name, step.Action));
                            var completed = task.Wait(step.TimeoutMs);
                            if (!completed) throw new TimeoutException($"step '{step.Name}' timeout after {step.TimeoutMs}ms{attemptLabel}");
                            if (task.IsFaulted) throw task.Exception!.InnerException ?? task.Exception!;
                        }
                        else
                        {
                            onStep(step.Name, step.Action);
                        }
                    }

                    // Successo: log e esci dal loop retry
                    lock (_lock)
                    {
                        var entry = maxAttempts > 1
                            ? $"[{i}] {step.Name}: {step.Action} (retry={step.RetryCount}, timeout={step.TimeoutMs}ms, attempts={attempt + 1})"
                            : $"[{i}] {step.Name}: {step.Action}";
                        wf.ExecutionLog.Add(entry);
                    }
                    succeeded = true;
                    break;
                }
                catch (TimeoutException tex)
                {
                    lastEx = tex;
                    lock (_lock)
                    {
                        wf.ExecutionLog.Add($"[{i}] {step.Name}: TIMEOUT after {step.TimeoutMs}ms{attemptLabel} — {tex.Message}");
                    }
                    // ritenta se restano tentativi, altrimenti fallisci
                }
                catch (Exception ex)
                {
                    lastEx = ex;
                    lock (_lock)
                    {
                        wf.ExecutionLog.Add($"[{i}] {step.Name}: ERROR{attemptLabel} — {ex.GetBaseException().Message}");
                    }
                    // ritenta se restano tentativi
                }
            }

            if (!succeeded)
            {
                lock (_lock)
                {
                    wf.CurrentStep = -1;
                    wf.Status = "failed";
                    wf.ExecutionLog.Add($"[workflow '{name}'] FAILED at step [{i}] {step.Name} after {maxAttempts} attempts — {lastEx?.GetBaseException().Message}");
                }
                return false;
            }
        }

        lock (_lock)
        {
            wf.CurrentStep = -1;
            wf.Status = "completed";
        }
        return true;
    }
}
