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
    /// <summary>CONTEXT: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> CONTEXTBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.CONTEXTBACKGROUND,
        BuiltInFunction.CONTEXTWITHCANCEL,
        BuiltInFunction.CONTEXTCANCEL,
        BuiltInFunction.CONTEXTDONE,
        BuiltInFunction.CONTEXTCANCELLED,
    };

    /// <summary>Context: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private static readonly Dictionary<BuiltInFunction, Func<PCodeProcessor, PCodeTask, object?[], object?>> CONTEXTDispatch = new()
    {
        [BuiltInFunction.CONTEXTBACKGROUND] = (_, _, args) =>
        {
                    return new Basic.Runtime.Gpm.PCodeContext();
        },
        [BuiltInFunction.CONTEXTWITHCANCEL] = (_, _, args) =>
        {
                {
                    if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodeContext parentCtx) return 0.0;
                    return new Basic.Runtime.Gpm.PCodeContext(parentCtx);
                }
        },
        [BuiltInFunction.CONTEXTCANCEL] = (_, _, args) =>
        {
                {
                    if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodeContext cancelCtx) return 0.0;
                cancelCtx.Cancel();
                    return 0.0;
                }
        },
        [BuiltInFunction.CONTEXTDONE] = (_, _, args) =>
        {
                {
                    if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodeContext doneCtx) return 0.0;
                    return doneCtx.Done(); // ChannelInfo (ref) chiuso alla cancellazione
                }
        },
        [BuiltInFunction.CONTEXTCANCELLED] = (_, _, args) =>
        {
                {
                    if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodeContext cancelledCtx) return 0.0;
                    return cancelledCtx.IsCancelled ? 1.0 : 0.0;
                }
        },
    };

    private object? ExecuteBuiltInContext(PCodeTask task, BuiltInFunction fn, object?[] args)
        => CONTEXTDispatch.TryGetValue(fn, out var handler)
            ? handler(this, task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInContext (fn={fn}).");

            }
