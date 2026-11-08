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
    /// <summary>RDD: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> RDDBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.RDDGETFIELD,
        BuiltInFunction.RDDGETFLDSTATE,
        BuiltInFunction.RDDTODAY,
        BuiltInFunction.RDDCTOD,
        BuiltInFunction.RDDDTOC,
        BuiltInFunction.RDDCURVAL,
        BuiltInFunction.RDDFLOAT,
        BuiltInFunction.RDDVALTYPE,
        BuiltInFunction.RDDBUFFER,
        BuiltInFunction.RDDTABLEREVERT,
        BuiltInFunction.RDDTABLEUPDATE,
        BuiltInFunction.RDDOLDVAL,
        BuiltInFunction.RDDRLOCK,
        BuiltInFunction.RDDFLOCK,
        BuiltInFunction.RDDSETDELETED,
        BuiltInFunction.RDDDELETE,
        BuiltInFunction.RDDRECALL,
        BuiltInFunction.RDDREPLACE,
    };

    /// <summary>Rdd: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _rddDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> RDDDispatch
        => _rddDispatch ??= BuildRddDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildRddDispatch() => new()
    {
        [BuiltInFunction.RDDGETFIELD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddGetField(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.RDDGETFLDSTATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddGetFieldState(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.RDDTODAY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddToday();
        },
        [BuiltInFunction.RDDCTOD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddCtod(ArgOr(args, 0));
        },
        [BuiltInFunction.RDDDTOC] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddDtoc(ArgOr(args, 0));
        },
        [BuiltInFunction.RDDCURVAL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddCurval(ArgOr(args, 0));
        },
        [BuiltInFunction.RDDFLOAT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddFloat(ArgOr(args, 0));
        },
        [BuiltInFunction.RDDVALTYPE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddValType(ArgOr(args, 0));
        },
        [BuiltInFunction.RDDBUFFER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddBuffer(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.RDDTABLEREVERT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddTableRevert(ArgOr(args, 0)) ? -1.0 : 0.0;
        },
        [BuiltInFunction.RDDTABLEUPDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddTableUpdate(ArgOr(args, 0)) ? -1.0 : 0.0;
        },
        [BuiltInFunction.RDDOLDVAL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddOldVal(ArgOr(args, 0), ArgOr(args, 1));
        },
        [BuiltInFunction.RDDRLOCK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddRLock(ArgOr(args, 0)) ? 1.0 : 0.0;
        },
        [BuiltInFunction.RDDFLOCK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddFLock(ArgOr(args, 0)) ? 1.0 : 0.0;
        },
        [BuiltInFunction.RDDSETDELETED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.RddSetDeleted(ArgOr(args, 0), ArgOr(args, 1)) ? -1.0 : 0.0;
        },
        [BuiltInFunction.RDDDELETE] = (task, args) =>
        {
                /* Forma FUNZIONE RDD.DELETE(scope, expr): il primo arg è una keyword/condizione
                 * stringa → RddDeleteScoped (parity IL). Prima finiva in RddDelete((int)SafeDouble(scope))
                 * → scope "CURRENT"/"ALL" letto come 0=All. */
                var delScope = ArgOr(args, 0);
                if (delScope is string) return Basic.IlBackend.IlRuntime.RddDeleteScoped(delScope, ArgOr(args, 1));
                return Basic.IlBackend.IlRuntime.RddDelete(args.Length > 0 ? (int)SafeDouble(args[0]) : 0, ArgOr(args, 1), ArgOr(args, 2));
        },
        [BuiltInFunction.RDDRECALL] = (task, args) =>
        {
                /* Forma FUNZIONE RDD.RECALL(scope, expr): keyword stringa → RddRecallScoped (parity IL). */
                var recScope = ArgOr(args, 0);
                if (recScope is string) return Basic.IlBackend.IlRuntime.RddRecallScoped(recScope, ArgOr(args, 1));
                return Basic.IlBackend.IlRuntime.RddRecall(args.Length > 0 ? (int)SafeDouble(args[0]) : 0, ArgOr(args, 1), ArgOr(args, 2));
        },
        [BuiltInFunction.RDDREPLACE] = (task, args) =>
        {
                /* Forma FUNZIONE RDD.REPLACE(scope, field, value, expr): keyword stringa → RddReplaceScoped. */
                var repScope = ArgOr(args, 0);
                if (repScope is string) return Basic.IlBackend.IlRuntime.RddReplaceScoped(repScope, ArgOr(args, 1), ArgOr(args, 2), ArgOr(args, 3));
                return Basic.IlBackend.IlRuntime.RddReplace(ArgOr(args, 0), args.Length > 1 ? (int)SafeDouble(args[1]) : 0, ArgOr(args, 2), ArgOr(args, 3));
        },
    };

    private object? ExecuteBuiltInRdd(PCodeTask task, BuiltInFunction fn, object?[] args)
        => RDDDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInRdd (fn={fn}).");

}
