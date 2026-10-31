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
    /// <summary>OPT: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> OPTBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.OPT_BRENTQ,
        BuiltInFunction.OPT_BRENTH,
        BuiltInFunction.OPT_RIDDER,
        BuiltInFunction.OPT_TOMS748,
        BuiltInFunction.OPT_BRENTMIN,
        BuiltInFunction.OPT_MINIMIZESCALAR,
        BuiltInFunction.OPT_POWELL,
        BuiltInFunction.OPT_TNC,
        BuiltInFunction.OPT_COBYLA,
        BuiltInFunction.OPT_SLSQP,
        BuiltInFunction.OPT_MINIMIZE,
        BuiltInFunction.OPT_FSOLVE,
        BuiltInFunction.OPT_LEASTSQUARES,
        BuiltInFunction.OPT_LINPROG,
        BuiltInFunction.OPT_BRUTE,
        BuiltInFunction.OPT_BASINHOPPING,
        BuiltInFunction.OPT_DUALANNEALING,
    };

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _OptDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> OptDispatch
        => _OptDispatch ??= BuildOptDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildOptDispatch() => new()
    {
        [BuiltInFunction.OPT_BRENTQ] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData rtFn) return double.NaN;
                double RtF(double x) => SafeDouble(CallClosureSync(task, rtFn, x));
                var a = SafeDouble(ArgOr(args, 1));
                var b = SafeDouble(ArgOr(args, 2));
                return Basic.IlBackend.IlRuntime.Brentq(RtF, a, b);
        },
        [BuiltInFunction.OPT_BRENTH] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData rtFn) return double.NaN;
                double RtF(double x) => SafeDouble(CallClosureSync(task, rtFn, x));
                var a = SafeDouble(ArgOr(args, 1));
                var b = SafeDouble(ArgOr(args, 2));
                return Basic.IlBackend.IlRuntime.Brenth(RtF, a, b);
        },
        [BuiltInFunction.OPT_RIDDER] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData rtFn) return double.NaN;
                double RtF(double x) => SafeDouble(CallClosureSync(task, rtFn, x));
                var a = SafeDouble(ArgOr(args, 1));
                var b = SafeDouble(ArgOr(args, 2));
                return Basic.IlBackend.IlRuntime.Ridder(RtF, a, b);
        },
        [BuiltInFunction.OPT_TOMS748] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData rtFn) return double.NaN;
                double RtF(double x) => SafeDouble(CallClosureSync(task, rtFn, x));
                var a = SafeDouble(ArgOr(args, 1));
                var b = SafeDouble(ArgOr(args, 2));
                return Basic.IlBackend.IlRuntime.Toms748(RtF, a, b);
        },
        [BuiltInFunction.OPT_BRENTMIN] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData rtFn) return double.NaN;
                double RtF(double x) => SafeDouble(CallClosureSync(task, rtFn, x));
                var a = SafeDouble(ArgOr(args, 1));
                var b = SafeDouble(ArgOr(args, 2));
                return Basic.IlBackend.IlRuntime.BrentMin(RtF, a, b);
        },
        [BuiltInFunction.OPT_MINIMIZESCALAR] = (task, args) =>
        {
                if (args.Length < 3 || args[0] is not ClosureData msFn) return double.NaN;
                double MsF(double x) => SafeDouble(CallClosureSync(task, msFn, x));
                var method = args.Length > 3 && args[3] is string ms ? ms : "brent";
                return Basic.IlBackend.IlRuntime.MinimizeScalar(MsF, SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), method);
        },
        [BuiltInFunction.OPT_POWELL] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData mnFn) return Array.Empty<double>();
                Func<double[], double> mnObj = p => SafeDouble(CallClosureSync(task, mnFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 3 ? ArgOr(args, 3) : null);
                var method = args.Length > 2 && args[2] is string mstr ? mstr : "nelder-mead";
                return Basic.IlBackend.IlRuntime.Powell(mnObj, x0);
        },
        [BuiltInFunction.OPT_TNC] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData mnFn) return Array.Empty<double>();
                Func<double[], double> mnObj = p => SafeDouble(CallClosureSync(task, mnFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 3 ? ArgOr(args, 3) : null);
                var method = args.Length > 2 && args[2] is string mstr ? mstr : "nelder-mead";
                return Basic.IlBackend.IlRuntime.Tnc(mnObj, x0, bounds!);
        },
        [BuiltInFunction.OPT_COBYLA] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData mnFn) return Array.Empty<double>();
                Func<double[], double> mnObj = p => SafeDouble(CallClosureSync(task, mnFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 3 ? ArgOr(args, 3) : null);
                var method = args.Length > 2 && args[2] is string mstr ? mstr : "nelder-mead";
                return Basic.IlBackend.IlRuntime.Cobyla(mnObj, x0, bounds!);
        },
        [BuiltInFunction.OPT_SLSQP] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData mnFn) return Array.Empty<double>();
                Func<double[], double> mnObj = p => SafeDouble(CallClosureSync(task, mnFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 3 ? ArgOr(args, 3) : null);
                var method = args.Length > 2 && args[2] is string mstr ? mstr : "nelder-mead";
                return Basic.IlBackend.IlRuntime.Slsqp(mnObj, x0, bounds!);
        },
        [BuiltInFunction.OPT_MINIMIZE] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData mnFn) return Array.Empty<double>();
                Func<double[], double> mnObj = p => SafeDouble(CallClosureSync(task, mnFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 3 ? ArgOr(args, 3) : null);
                var method = args.Length > 2 && args[2] is string mstr ? mstr : "nelder-mead";
                return Basic.IlBackend.IlRuntime.Minimize(mnObj, x0, method, bounds!);
        },
        [BuiltInFunction.OPT_FSOLVE] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData fsFn) return Array.Empty<double>();
                Func<double[], double[]> fsVec = p => ToDoubleArr(CallClosureSync(task, fsFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                return Basic.IlBackend.IlRuntime.Fsolve(fsVec, x0);
        },
        [BuiltInFunction.OPT_LEASTSQUARES] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData fsFn) return Array.Empty<double>();
                Func<double[], double[]> fsVec = p => ToDoubleArr(CallClosureSync(task, fsFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                var x0 = ToDoubleArr(ArgOr(args, 1));
                var bounds = ToRectArr(args.Length > 2 ? ArgOr(args, 2) : null);
                return Basic.IlBackend.IlRuntime.LeastSquaresOpt(fsVec, x0, bounds!);
        },
        [BuiltInFunction.OPT_LINPROG] = (task, args) =>
        {
                var c = ToDoubleArr(ArgOr(args, 0));
                var aUb = args.Length > 1 ? ToJaggedArr(ArgOr(args, 1)) : null;
                var bUb = args.Length > 2 ? ToDoubleArr(ArgOr(args, 2)) : null;
                var aEq = args.Length > 3 ? ToJaggedArr(ArgOr(args, 3)) : null;
                var bEq = args.Length > 4 ? ToDoubleArr(ArgOr(args, 4)) : null;
                var bounds = args.Length > 5 ? ToRectArr(ArgOr(args, 5)) : null;
                return Basic.IlBackend.IlRuntime.LinProg(c, aUb!, bUb!, aEq!, bEq!, bounds!);
        },
        [BuiltInFunction.OPT_BRUTE] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData glFn) return Array.Empty<double>();
                Func<double[], double> glObj = p => SafeDouble(CallClosureSync(task, glFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                return Basic.IlBackend.IlRuntime.Brute(glObj, ToJaggedArr(ArgOr(args, 1)), (int)(args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 20));
        },
        [BuiltInFunction.OPT_BASINHOPPING] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData glFn) return Array.Empty<double>();
                Func<double[], double> glObj = p => SafeDouble(CallClosureSync(task, glFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                return Basic.IlBackend.IlRuntime.BasinHopping(glObj, ToDoubleArr(ArgOr(args, 1)), (int)(args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 50));
        },
        [BuiltInFunction.OPT_DUALANNEALING] = (task, args) =>
        {
                if (args.Length < 2 || args[0] is not ClosureData glFn) return Array.Empty<double>();
                Func<double[], double> glObj = p => SafeDouble(CallClosureSync(task, glFn,
                    OptList(global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(p))));
                return Basic.IlBackend.IlRuntime.DualAnnealing(glObj, ToJaggedArr(ArgOr(args, 1)), (int)(args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 1000));
        },
    };

    private object? ExecuteBuiltInOpt(PCodeTask task, BuiltInFunction fn, object?[] args)
        => OptDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInOpt (fn={fn}).");

}
