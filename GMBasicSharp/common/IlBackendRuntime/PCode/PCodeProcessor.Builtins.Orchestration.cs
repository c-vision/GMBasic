// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Orchestration.cs
// P-CODE VM: dominio Orchestrazione agenti (A1) — categorie di task → provider/modello.
// Refactor §0quater: dominio isolato in un file dedicato (dispatch a tabella, non switch).

using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Orchestrazione agenti (A1): categorie di task → provider/modello.</summary>
    private static readonly HashSet<BuiltInFunction> ORCHESTRATIONBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.CATEGORYSET,
        BuiltInFunction.CATEGORYFALLBACK,
        BuiltInFunction.CATEGORYAUTO,
        BuiltInFunction.CATEGORYPICK,
        BuiltInFunction.CATEGORYLIST,
        BuiltInFunction.TASKDEFINE,
        BuiltInFunction.TASKDEPENDS,
        BuiltInFunction.TASKASSIGN,
        BuiltInFunction.TASKSTATUS,
        BuiltInFunction.TASKLIST,
        BuiltInFunction.TASKRUN,
        BuiltInFunction.PLANDEFINE,
        BuiltInFunction.PLANADD,
        BuiltInFunction.PLANAPPROVE,
        BuiltInFunction.PLANSTATUS,
        BuiltInFunction.PLANLIST,
        BuiltInFunction.PLANRUN,
        BuiltInFunction.HOOKREGISTER,
        BuiltInFunction.HOOKUNREGISTER,
        BuiltInFunction.HOOKLIST,
        BuiltInFunction.HOOKEVENTS,
        BuiltInFunction.HOOKFIRE,
        BuiltInFunction.ARTIFACTPUT,
        BuiltInFunction.ARTIFACTLINK,
        BuiltInFunction.ARTIFACTGET,
        BuiltInFunction.ARTIFACTLIST,
        BuiltInFunction.ARTIFACTREMOVE,
        BuiltInFunction.RESOURCESET,
        BuiltInFunction.RESOURCEGET,
        BuiltInFunction.RESOURCELIST,
        BuiltInFunction.RESOURCEAVAILABLE,
        BuiltInFunction.RESOURCESWARMSIZE,
        BuiltInFunction.HANDOFF,
        BuiltInFunction.HANDOFFCOMPLETE,
        BuiltInFunction.HANDOFFSTATUS,
        BuiltInFunction.HANDOFFLIST,
        BuiltInFunction.HANDOFFCLEAR,
        BuiltInFunction.RULESADD,
        BuiltInFunction.RULESFOR,
        BuiltInFunction.RULESLIST,
        BuiltInFunction.RULESREMOVE,
        BuiltInFunction.RULESCLEAR,
        BuiltInFunction.GOALSET,
        BuiltInFunction.GOALGET,
        BuiltInFunction.GOALSTATUS,
        BuiltInFunction.GOALTICK,
        BuiltInFunction.GOALCLEAR,
        BuiltInFunction.REPLAYSTART,
        BuiltInFunction.REPLAYRECORD,
        BuiltInFunction.REPLAYSTOP,
        BuiltInFunction.REPLAYGET,
        BuiltInFunction.REPLAYLIST,
        BuiltInFunction.REPLAYCLEAR,
        BuiltInFunction.REPLAYRUN,
        BuiltInFunction.GOALAGENT,
        BuiltInFunction.METRICRECORD,
        BuiltInFunction.METRICGET,
        BuiltInFunction.METRICLIST,
        BuiltInFunction.FLEETSTATS,
        BuiltInFunction.METRICRESET,
        BuiltInFunction.HASHLINE,
        BuiltInFunction.HASHEDIT,
        BuiltInFunction.HASHVERIFY,
        BuiltInFunction.CHECKPOINT,
        BuiltInFunction.RESUME,
        BuiltInFunction.CHECKPOINTLIST,
        BuiltInFunction.CHECKPOINTCLEAR,
        BuiltInFunction.ORCHSAVE,
        BuiltInFunction.ORCHLOAD,
        BuiltInFunction.TASKCLEAR,
        BuiltInFunction.PLANCLEAR,
    };

    /// <summary>Orchestrazione: dispatch per BuiltInFunction a TABELLA (Dictionary).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _orchestrationDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> OrchestrationDispatch
        => _orchestrationDispatch ??= BuildOrchestrationDispatch();

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildOrchestrationDispatch() => new()
    {
        [BuiltInFunction.CATEGORYSET] = (task, args) =>
            Basic.IlBackend.IlRuntime.CategorySet(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.CATEGORYFALLBACK] = (task, args) =>
            Basic.IlBackend.IlRuntime.CategoryFallback(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.CATEGORYAUTO] = (task, args) =>
            Basic.IlBackend.IlRuntime.CategoryAuto(
                args.Length > 0 && double.TryParse(args[0]?.ToString(), out var f) ? f : 0),
        [BuiltInFunction.CATEGORYPICK] = (task, args) =>
            Basic.IlBackend.IlRuntime.CategoryPick(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.CATEGORYLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.CategoryList(),
        [BuiltInFunction.TASKDEFINE] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskDefine(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.TASKDEPENDS] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskDepends(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.TASKASSIGN] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskAssign(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.TASKSTATUS] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskStatus(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.TASKLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskList(),
        [BuiltInFunction.TASKRUN] = (task, args) =>
            Basic.IlBackend.IlRuntime.TaskRun(),
        [BuiltInFunction.PLANDEFINE] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanDefine(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.PLANADD] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanAdd(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.PLANAPPROVE] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanApprove(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.PLANSTATUS] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanStatus(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.PLANLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanList(),
        [BuiltInFunction.PLANRUN] = (task, args) =>
            Basic.IlBackend.IlRuntime.PlanRun(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.HOOKREGISTER] = (task, args) =>
            Basic.IlBackend.IlRuntime.HookRegister(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.HOOKUNREGISTER] = (task, args) =>
            Basic.IlBackend.IlRuntime.HookUnregister(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.HOOKLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.HookList(),
        [BuiltInFunction.HOOKEVENTS] = (task, args) =>
            Basic.IlBackend.IlRuntime.HookEvents(),
        [BuiltInFunction.HOOKFIRE] = (task, args) =>
            Basic.IlBackend.IlRuntime.HookFire(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.ARTIFACTPUT] = (task, args) =>
            Basic.IlBackend.IlRuntime.ArtifactPut(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.ARTIFACTLINK] = (task, args) =>
            Basic.IlBackend.IlRuntime.ArtifactLink(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.ARTIFACTGET] = (task, args) =>
            Basic.IlBackend.IlRuntime.ArtifactGet(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.ARTIFACTLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.ArtifactList(),
        [BuiltInFunction.ARTIFACTREMOVE] = (task, args) =>
            Basic.IlBackend.IlRuntime.ArtifactRemove(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.RESOURCESET] = (task, args) =>
            Basic.IlBackend.IlRuntime.ResourceSet(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.RESOURCEGET] = (task, args) =>
            Basic.IlBackend.IlRuntime.ResourceGet(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.RESOURCELIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.ResourceList(),
        [BuiltInFunction.RESOURCEAVAILABLE] = (task, args) =>
            Basic.IlBackend.IlRuntime.ResourceAvailable(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.RESOURCESWARMSIZE] = (task, args) =>
            Basic.IlBackend.IlRuntime.ResourceSwarmSize(
                args.Length > 0 && double.TryParse(args[0]?.ToString(), out var n) ? n : 0),
        [BuiltInFunction.HANDOFF] = (task, args) =>
            Basic.IlBackend.IlRuntime.Handoff(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.HANDOFFCOMPLETE] = (task, args) =>
            Basic.IlBackend.IlRuntime.HandoffComplete(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.HANDOFFSTATUS] = (task, args) =>
            Basic.IlBackend.IlRuntime.HandoffStatus(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.HANDOFFLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.HandoffList(),
        [BuiltInFunction.HANDOFFCLEAR] = (task, args) =>
            Basic.IlBackend.IlRuntime.HandoffClear(),
        [BuiltInFunction.RULESADD] = (task, args) =>
            Basic.IlBackend.IlRuntime.RulesAdd(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.RULESFOR] = (task, args) =>
            Basic.IlBackend.IlRuntime.RulesFor(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.RULESLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.RulesList(),
        [BuiltInFunction.RULESREMOVE] = (task, args) =>
            Basic.IlBackend.IlRuntime.RulesRemove(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.RULESCLEAR] = (task, args) =>
            Basic.IlBackend.IlRuntime.RulesClear(),
        [BuiltInFunction.GOALSET] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalSet(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.GOALGET] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalGet(),
        [BuiltInFunction.GOALSTATUS] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalStatus(),
        [BuiltInFunction.GOALTICK] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalTick(),
        [BuiltInFunction.GOALCLEAR] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalClear(),
        [BuiltInFunction.REPLAYSTART] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayStart(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.REPLAYRECORD] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayRecord(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.REPLAYSTOP] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayStop(),
        [BuiltInFunction.REPLAYGET] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayGet(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.REPLAYLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayList(),
        [BuiltInFunction.REPLAYCLEAR] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayClear(),
        [BuiltInFunction.REPLAYRUN] = (task, args) =>
            Basic.IlBackend.IlRuntime.ReplayRun(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.GOALAGENT] = (task, args) =>
            Basic.IlBackend.IlRuntime.GoalAgent(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.METRICRECORD] = (task, args) =>
            Basic.IlBackend.IlRuntime.MetricRecord(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                args.Length > 2 && double.TryParse(args[2]?.ToString(), out var mv) ? mv : 0),
        [BuiltInFunction.METRICGET] = (task, args) =>
            Basic.IlBackend.IlRuntime.MetricGet(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.METRICLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.MetricList(),
        [BuiltInFunction.FLEETSTATS] = (task, args) =>
            Basic.IlBackend.IlRuntime.FleetStats(),
        [BuiltInFunction.METRICRESET] = (task, args) =>
            Basic.IlBackend.IlRuntime.MetricReset(),
        [BuiltInFunction.HASHLINE] = (task, args) =>
            Basic.IlBackend.IlRuntime.HashLine(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.HASHEDIT] = (task, args) =>
            Basic.IlBackend.IlRuntime.HashEdit(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                args.Length > 2 ? args[2]?.ToString() ?? "" : ""),
        [BuiltInFunction.HASHVERIFY] = (task, args) =>
            Basic.IlBackend.IlRuntime.HashVerify(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.CHECKPOINT] = (task, args) =>
            Basic.IlBackend.IlRuntime.Checkpoint(
                args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.RESUME] = (task, args) =>
            Basic.IlBackend.IlRuntime.Resume(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.CHECKPOINTLIST] = (task, args) =>
            Basic.IlBackend.IlRuntime.CheckpointList(),
        [BuiltInFunction.CHECKPOINTCLEAR] = (task, args) =>
            Basic.IlBackend.IlRuntime.CheckpointClear(),
        [BuiltInFunction.ORCHSAVE] = (task, args) =>
            Basic.IlBackend.IlRuntime.OrchSave(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.ORCHLOAD] = (task, args) =>
            Basic.IlBackend.IlRuntime.OrchLoad(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.TASKCLEAR] = (task, args) => Basic.IlBackend.IlRuntime.TaskClear(),
        [BuiltInFunction.PLANCLEAR] = (task, args) => Basic.IlBackend.IlRuntime.PlanClear(),
    };

    private object? ExecuteBuiltInOrchestration(PCodeTask task, BuiltInFunction fn, object?[] args)
        => OrchestrationDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInOrchestration (fn={fn}).");
}
