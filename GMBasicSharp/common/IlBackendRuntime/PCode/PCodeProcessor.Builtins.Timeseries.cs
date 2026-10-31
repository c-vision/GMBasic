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
    /// <summary>TS_: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> TimeseriesBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.TS_LOWPASS,
        BuiltInFunction.TS_HIGHPASS,
        BuiltInFunction.TS_BANDPASS,
        BuiltInFunction.TS_BANDSTOP,
    };

    /// <summary>Timeseries: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _timeseriesDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> TIMESERIESDispatch
        => _timeseriesDispatch ??= BuildTimeseriesDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildTimeseriesDispatch() => new()
    {
        [BuiltInFunction.TS_LOWPASS] = (task, args) =>
        {
                if (args.Length > 2) return Basic.IlBackend.IlRuntime.TimeSeriesLowPass(ToDoubleArr(ArgOr(args, 0)), SafeDouble(args[1]), SafeDouble(args[2]));
                return Array.Empty<double>();
        },
        [BuiltInFunction.TS_HIGHPASS] = (task, args) =>
        {
                if (args.Length > 2) return Basic.IlBackend.IlRuntime.TimeSeriesHighPass(ToDoubleArr(ArgOr(args, 0)), SafeDouble(args[1]), SafeDouble(args[2]));
                return Array.Empty<double>();
        },
        [BuiltInFunction.TS_BANDPASS] = (task, args) =>
        {
                if (args.Length > 3) return Basic.IlBackend.IlRuntime.TimeSeriesBandPass(ToDoubleArr(ArgOr(args, 0)), SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]));
                return Array.Empty<double>();
        },
        [BuiltInFunction.TS_BANDSTOP] = (task, args) =>
        {
                if (args.Length > 3) return Basic.IlBackend.IlRuntime.TimeSeriesBandStop(ToDoubleArr(ArgOr(args, 0)), SafeDouble(args[1]), SafeDouble(args[2]), SafeDouble(args[3]));
                return Array.Empty<double>();
        },
    };

    private object? ExecuteBuiltInTimeseries(PCodeTask task, BuiltInFunction fn, object?[] args)
        => TIMESERIESDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInTimeseries (fn={fn}).");

}
