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
    /// <summary>SPATIAL: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SPATIALBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SPATIAL_PDIST,
        BuiltInFunction.SPATIAL_CDIST,
        BuiltInFunction.SPATIAL_DISTANCEMATRIX,
        BuiltInFunction.SPATIAL_CONVEXHULL,
    };

    /// <summary>Spatial: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _spatialDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SPATIALDispatch
        => _spatialDispatch ??= BuildSpatialDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSpatialDispatch() => new()
    {
        [BuiltInFunction.SPATIAL_PDIST] = (task, args) =>
        {
                {
                    var X = ToJaggedArr(ArgOr(args, 0));
                    var metric = args.Length > 1 && args[1] is string ms ? ms : "euclidean";
                    return X.Length >= 400
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Pdist(X, metric, 2))
                        : CommonLib.Numerics.Pdist(X, metric, 2);
                }
        },
        [BuiltInFunction.SPATIAL_CDIST] = (task, args) =>
        {
                {
                    var X = ToJaggedArr(ArgOr(args, 0));
                    var Y = ToJaggedArr(ArgOr(args, 1));
                    var metric = args.Length > 2 && args[2] is string ms2 ? ms2 : "euclidean";
                    return X.Length >= 500 || Y.Length >= 500
                        ? ParkCpuBuiltin(task, () => LuStoreMatrix(CommonLib.Numerics.Cdist(X, Y, metric, 2)))
                        : LuStoreMatrix(CommonLib.Numerics.Cdist(X, Y, metric, 2));
                }
        },
        [BuiltInFunction.SPATIAL_DISTANCEMATRIX] = (task, args) =>
        {
                {
                    var X = ToJaggedArr(ArgOr(args, 0));
                    var Y = ToJaggedArr(ArgOr(args, 1));
                    return X.Length >= 1000 || Y.Length >= 1000
                        ? ParkCpuBuiltin(task, () => LuStoreMatrix(CommonLib.Numerics.DistanceMatrix(X, Y)))
                        : LuStoreMatrix(CommonLib.Numerics.DistanceMatrix(X, Y));
                }
        },
        [BuiltInFunction.SPATIAL_CONVEXHULL] = (task, args) =>
        {
                {
                    var X = ToJaggedArr(ArgOr(args, 0));
                    return X.Length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.ConvexHull(X))
                        : CommonLib.Numerics.ConvexHull(X);
                }
        },
    };

    private object? ExecuteBuiltInSpatial(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SPATIALDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSpatial (fn={fn}).");

            }
