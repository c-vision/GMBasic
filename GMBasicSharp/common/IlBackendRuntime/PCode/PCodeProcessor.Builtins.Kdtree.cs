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
    /// <summary>KDTREE: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> KDTREEBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.KDTREE_CREATE,
        BuiltInFunction.KDTREE_QUERY,
        BuiltInFunction.KDTREE_QUERYDIST,
        BuiltInFunction.KDTREE_QUERYBALL,
    };

    /// <summary>Kdtree: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _kdtreeDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> KDTREEDispatch
        => _kdtreeDispatch ??= BuildKdtreeDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildKdtreeDispatch() => new()
    {
        [BuiltInFunction.KDTREE_CREATE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.KdTreeCreate(ToDoubleArr(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.KDTREE_QUERY] = (task, args) =>
        {
                    if (args.Length >= 3) return Basic.IlBackend.IlRuntime.KdTreeQuery(SafeDouble(args[0]), ToDoubleArr(ArgOr(args, 1)), SafeDouble(args[2]));
                    return Array.Empty<double>();
        },
        [BuiltInFunction.KDTREE_QUERYDIST] = (task, args) =>
        {
                    if (args.Length >= 3) return Basic.IlBackend.IlRuntime.KdTreeQueryDist(SafeDouble(args[0]), ToDoubleArr(ArgOr(args, 1)), SafeDouble(args[2]));
                    return Array.Empty<double>();
        },
        [BuiltInFunction.KDTREE_QUERYBALL] = (task, args) =>
        {
                    if (args.Length >= 3) return Basic.IlBackend.IlRuntime.KdTreeQueryBall(SafeDouble(args[0]), ToDoubleArr(ArgOr(args, 1)), SafeDouble(args[2]));
                    return Array.Empty<double>();
            
        },
    };

    private object? ExecuteBuiltInKdtree(PCodeTask task, BuiltInFunction fn, object?[] args)
        => KDTREEDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInKdtree (fn={fn}).");

            }
