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
    /// <summary>INTERPOLATE: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> INTERPOLATEBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.INTERPOLATELINEAR,
        BuiltInFunction.INTERPOLATEPCHIP,
        BuiltInFunction.INTERPOLATEPOLYNOMIAL,
        BuiltInFunction.INTERPOLATESPLINE,
        BuiltInFunction.INTERPOLATESTEP,        BuiltInFunction.INTERP_SPLREP,
        BuiltInFunction.INTERP_SPLEV,
        BuiltInFunction.INTERPOLATECLAMP,
        BuiltInFunction.INTERPOLATEGENERIC,

    };

    /// <summary>Interpolate: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _interpolateDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> INTERPOLATEDispatch
        => _interpolateDispatch ??= BuildInterpolateDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildInterpolateDispatch() => new()
    {
        [BuiltInFunction.INTERPOLATELINEAR] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return Basic.IlBackend.IlRuntime.InterpolateLinear(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.INTERPOLATEPCHIP] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.InterpolatePchip(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.INTERPOLATEPOLYNOMIAL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.InterpolatePolynomial(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.INTERPOLATESPLINE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.InterpolateSpline(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.INTERPOLATESTEP] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.InterpolateStep(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.INTERP_SPLREP] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var y = ToDoubleArr(ArgOr(args, 1));
                    return x.Length >= 200_000
                        ? ParkCpuBuiltin(task, () => LuStoreSpline(CommonLib.Numerics.Splrep(x, y)))
                        : LuStoreSpline(CommonLib.Numerics.Splrep(x, y));
                }
        },
        [BuiltInFunction.INTERP_SPLEV] = (task, args) =>
        {
                {
                    var h = ToLongHandle(args, 0);
                    if (!_linDecompositions.TryGetValue(h, out var d) || d is not CommonLib.Numerics.CubicSplineRep sp)
                    return Array.Empty<double>();
                    var xq = ToDoubleArr(ArgOr(args, 1));
                    return xq.Length >= 200_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Splev(sp, xq))
                        : CommonLib.Numerics.Splev(sp, xq);
                }
        },
        [BuiltInFunction.INTERPOLATECLAMP] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    if (args.Length >= 3) return Basic.IlBackend.IlRuntime.InterpolateClamp(
                ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), SafeDouble(args[2]));
                    return 0.0;
        },
        [BuiltInFunction.INTERPOLATEGENERIC] = (task, args) =>
        {
                    if (args.Length >= 4) return Basic.IlBackend.IlRuntime.InterpolateGeneric(
                args[0]?.ToString() ?? "linear", ToDoubleArr(ArgOr(args, 1)), ToDoubleArr(ArgOr(args, 2)),
                SafeDouble(args[3]), args.Length > 4 && SafeDouble(args[4]) != 0 ? 1 : 0);
                    return 0.0;
        },
    };

    private object? ExecuteBuiltInInterpolate(PCodeTask task, BuiltInFunction fn, object?[] args)
        => INTERPOLATEDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInInterpolate (fn={fn}).");

            }
