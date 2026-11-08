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
    /// <summary>SOLVE: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SOLVEBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SOLVE,
        BuiltInFunction.SOLVEODE,
        BuiltInFunction.SOLVE_IVP,
    };

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _SolveDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SolveDispatch
        => _SolveDispatch ??= BuildSolveDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSolveDispatch() => new()
    {
        [BuiltInFunction.SOLVE] = (task, args) =>
        {
            if (args.Length >= 5 && args[0] is ClosureData odeF)
            {
                try
                {
                    Func<double, double, double> ofn = (t, yy) => SafeDouble(CallClosureSync(task, odeF, t, yy));
                    return Basic.IlBackend.IlRuntime.SolveOde(ofn, SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]), SafeDouble(args[4]));
                }
                catch { }
            }
            /* LINALG.SOLVE(A, b): 2 argomenti (matrice + vettore/array) → solve lineare. */
            if (args.Length == 2) return LuSolve(args);
            return null;
        },
        [BuiltInFunction.SOLVEODE] = (task, args) =>
        {
            if (args.Length >= 5 && args[0] is ClosureData odeF)
            {
                try
                {
                    Func<double, double, double> ofn = (t, yy) => SafeDouble(CallClosureSync(task, odeF, t, yy));
                    return Basic.IlBackend.IlRuntime.SolveOde(ofn, SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]), SafeDouble(args[4]));
                }
                catch { }
            }
            return null;
        },
        [BuiltInFunction.SOLVE_IVP] = (task, args) =>
        {
            if (args.Length >= 4 && args[0] is ClosureData ivpF)
            {
                try
                {
                    Func<double, double[], double[]> ifn = (t, yy) => ToDoubleArr(CallClosureSync(task, ivpF, t,
                        OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(yy))));
                    return LuSolveIvp(ifn, ToDoubleArr(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
                }
                catch { }
            }
            return 0L;
            if (args.Length >= 2)
                return Basic.IlBackend.IlRuntime.Trapezoid(ToDoubleArr(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)));
            return 0.0;
        },
    };
    private object? ExecuteBuiltInSolve(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SolveDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSolve (fn={fn}).");

            }
