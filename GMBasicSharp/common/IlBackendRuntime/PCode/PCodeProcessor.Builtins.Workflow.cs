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

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
            {
    /// <summary>WORKFLOW: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> WORKFLOWBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.WORKFLOWLIST,
        BuiltInFunction.WORKFLOWSTATUS,
        BuiltInFunction.WORKFLOWSTEPS,
    };

    /// <summary>Workflow: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private static readonly Dictionary<BuiltInFunction, Func<PCodeProcessor, PCodeTask, object?[], object?>> WORKFLOWDispatch = new()
    {
        [BuiltInFunction.WORKFLOWLIST] = (_, _, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica (separatore \n, canone) — unica impl.
                    return Basic.IlBackend.IlRuntime.WorkflowList();
        },
        [BuiltInFunction.WORKFLOWSTATUS] = (_, _, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return args.Length > 0 ? Basic.IlBackend.IlRuntime.WorkflowStatus(args[0]?.ToString() ?? "") : "";
        },
        [BuiltInFunction.WORKFLOWSTEPS] = (_, _, args) =>
        {
                    return args.Length > 0 ? Basic.IlBackend.IlRuntime.WorkflowSteps(args[0]?.ToString() ?? "") : "";
            
        },
    };

    private object? ExecuteBuiltInWorkflow(PCodeTask task, BuiltInFunction fn, object?[] args)
        => WORKFLOWDispatch.TryGetValue(fn, out var handler)
            ? handler(this, task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInWorkflow (fn={fn}).");

            }
