// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>TEAM: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> TEAMBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.TEAMLIST,
        BuiltInFunction.TEAMAGENTS,
        BuiltInFunction.TEAMTRAINSTATUS,
        BuiltInFunction.TEAMLINKINFO,
        BuiltInFunction.TEAMRECURSION,
        BuiltInFunction.TEAMLATENT,
        BuiltInFunction.TEAMRUNTIMESTATS,
        BuiltInFunction.TEAMRUNSTART,
        BuiltInFunction.TEAMRUNWAIT,
        BuiltInFunction.TEAMONSTREAM,
        BuiltInFunction.BLACKBOARDWRITE,
        BuiltInFunction.BLACKBOARDREAD,
        BuiltInFunction.BLACKBOARDWAIT,
        BuiltInFunction.AGENTSWARM,
    };

    /// <summary>Team: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _teamDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> TEAMDispatch
        => _teamDispatch ??= BuildTeamDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildTeamDispatch() => new()
    {
        [BuiltInFunction.TEAMLIST] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica (separatore \n, canone) — unica impl.
                return Basic.IlBackend.IlRuntime.TeamList();
        },
        [BuiltInFunction.TEAMAGENTS] = (task, args) =>
        {
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.TeamAgents(args[0]?.ToString() ?? "");
                return "";
        },
        [BuiltInFunction.TEAMTRAINSTATUS] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamTrainStatus(args[0]?.ToString() ?? "") : "{}";
        },
        [BuiltInFunction.TEAMLINKINFO] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamLinkInfo(args[0]?.ToString() ?? "") : "{}";
        },
        [BuiltInFunction.TEAMRECURSION] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamRecursion(args[0]?.ToString() ?? "") : 1.0;
        },
        [BuiltInFunction.TEAMLATENT] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamLatent(args[0]?.ToString() ?? "") : "text";
        },
        [BuiltInFunction.TEAMRUNTIMESTATS] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamRuntimeStats(args[0]?.ToString() ?? "") : "{}";
        },
        [BuiltInFunction.TEAMRUNSTART] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.TeamRunStart(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.TEAMRUNWAIT] = (task, args) =>
        {
                return args.Length > 0 ? Basic.IlBackend.IlRuntime.TeamRunWait(args[0]?.ToString() ?? "") : "Error: handle mancante";
        },
        [BuiltInFunction.TEAMONSTREAM] = (task, args) => Basic.IlBackend.IlRuntime.TeamOnStream(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.BLACKBOARDWRITE] = (task, args) => Basic.IlBackend.IlRuntime.BlackboardWrite(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
        [BuiltInFunction.BLACKBOARDREAD] = (task, args) => Basic.IlBackend.IlRuntime.BlackboardRead(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        // BLACKBOARDWAIT: park-based (GPM, fix 2026-09-09 — vedi Todo/(open) - AgentBatch-GPM-
        // Park-Based-Rewrite-Tutti-Backend §0bis). L'attesa sincrona (tcs.Task.Wait) gira su un
        // thread del blocking pool (mai sul worker GPM): il task parcheggia e il worker resta
        // libero di schedulare altri task. Stesso pattern di AI.AIAGENTASK (ParkBlocking). Un
        // timeout garantisce la liberazione dello slot anche se la chiave non viene mai scritta.
        [BuiltInFunction.BLACKBOARDWAIT] = (task, args) =>
        {
            var bbKey = args.Length > 0 ? args[0]?.ToString() ?? "" : "";
            var bbTimeout = args.Length > 1 && double.TryParse(args[1]?.ToString(), out var to) ? to : 10000;
            ParkBlocking(task,
                () => Basic.IlBackend.IlRuntime.BlackboardWait(bbKey, bbTimeout),
                result => task.PushRef(result));
            return ParkedBuiltinResult;
        },
        // AGENTSWARM: park-based (GPM, fix 2026-09-09). AgentBatch.Run (Task.Run+WaitAll) gira su
        // un thread del blocking pool, MAI sul worker GPM — il task parcheggia (ParkBlocking) e il
        // worker schedulatore prosegue. Idem BLACKBOARDWAIT. GpmWorkerGuard.Check dentro
        // AgentBatch.Run non scatterà: il thread pool .NET non è un worker GPM (InGpmDriver=false).
        [BuiltInFunction.AGENTSWARM] = (task, args) =>
        {
            var swarmN = args.Length > 0 && double.TryParse(args[0]?.ToString(), out var nn) ? nn : 0;
            var swarmPrefix = args.Length > 1 ? args[1]?.ToString() ?? "" : "";
            var swarmTask = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
            var swarmCc = args.Length > 3 && double.TryParse(args[3]?.ToString(), out var cc) ? cc : 8;
            ParkBlocking(task,
                () => Basic.IlBackend.IlRuntime.AiAgentSwarm(swarmN, swarmPrefix, swarmTask, swarmCc),
                result => task.PushRef(result));
            return ParkedBuiltinResult;
        },
    };

    private object? ExecuteBuiltInTeam(PCodeTask task, BuiltInFunction fn, object?[] args)
        => TEAMDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInTeam (fn={fn}).");

}
