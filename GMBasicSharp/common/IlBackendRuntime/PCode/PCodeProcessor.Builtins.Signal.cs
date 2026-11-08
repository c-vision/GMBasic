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
    /// <summary>SIGNAL: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SIGNALBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SIGNAL_BLACKMAN,
        BuiltInFunction.SIGNAL_BUTTER,
        BuiltInFunction.SIGNAL_CHEBY1,
        BuiltInFunction.SIGNAL_CHEBY2,
        BuiltInFunction.SIGNAL_CONVOLVE,
        BuiltInFunction.SIGNAL_CORRELATE,
        BuiltInFunction.SIGNAL_DECIMATE,
        BuiltInFunction.SIGNAL_ELLIP,
        BuiltInFunction.SIGNAL_FILTFILT,
        BuiltInFunction.SIGNAL_FINDPEAKS,
        BuiltInFunction.SIGNAL_FIRWIN,
        BuiltInFunction.SIGNAL_FREQZ,
        BuiltInFunction.SIGNAL_HAMMING,
        BuiltInFunction.SIGNAL_HANN,
        BuiltInFunction.SIGNAL_LFILTER,
        BuiltInFunction.SIGNAL_RESAMPLE,
        BuiltInFunction.SIGNAL_SAVGOLFILTER,
        BuiltInFunction.SIGNAL_WELCH,
    };

    /// <summary>Signal: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _signalDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SIGNALDispatch
        => _signalDispatch ??= BuildSignalDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSignalDispatch() => new()
    {
        [BuiltInFunction.SIGNAL_WELCH] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var np = (int)SafeDouble(ArgOr(args, 1));
                    var fs = args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 1.0;
                    return x.Length >= 200_000
                        ? ParkCpuBuiltin(task, () => LuStoreMatrix(CommonLib.Numerics.Welch(x, np, fs)))
                        : LuStoreMatrix(CommonLib.Numerics.Welch(x, np, fs));
                }
        },
        [BuiltInFunction.SIGNAL_SAVGOLFILTER] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var wl = (int)SafeDouble(ArgOr(args, 1));
                    var po = (int)SafeDouble(ArgOr(args, 2));
                    return x.Length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SavgolFilter(x, wl, po))
                        : CommonLib.Numerics.SavgolFilter(x, wl, po);
                }
        },
        [BuiltInFunction.SIGNAL_RESAMPLE] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var num = (int)SafeDouble(ArgOr(args, 1));
                    return x.Length >= 200_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Resample(x, num))
                        : CommonLib.Numerics.Resample(x, num);
                }
        },
        [BuiltInFunction.SIGNAL_DECIMATE] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var qd = (int)SafeDouble(ArgOr(args, 1));
                    return x.Length >= 200_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Decimate(x, qd))
                        : CommonLib.Numerics.Decimate(x, qd);
                }
        },
        [BuiltInFunction.SIGNAL_CONVOLVE] = (task, args) =>
        {
                    if (args.Length > 1 && args[0] is System.Collections.IList cv1 && args[1] is System.Collections.IList cv2)
                {
                    var left = ToDoubleArray(cv1);
                    var right = ToDoubleArray(cv2);
                    return (long)left.Length * right.Length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SignalConvolve(left, right))
                        : CommonLib.Numerics.SignalConvolve(left, right);
                }
                    return null;
        },
        [BuiltInFunction.SIGNAL_CORRELATE] = (task, args) =>
        {
                    if (args.Length > 1 && args[0] is System.Collections.IList cr1 && args[1] is System.Collections.IList cr2)
                {
                    var left = ToDoubleArray(cr1);
                    var right = ToDoubleArray(cr2);
                    return (long)left.Length * right.Length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SignalCorrelate(left, right))
                        : CommonLib.Numerics.SignalCorrelate(left, right);
                }
                    return null;
        },
        [BuiltInFunction.SIGNAL_HANN] = (task, args) =>
        {
                {
                    var length = (int)(args.Length > 0 ? SafeDouble(args[0]) : 0);
                    return length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SignalHann(length))
                        : CommonLib.Numerics.SignalHann(length);
                }
        },
        [BuiltInFunction.SIGNAL_HAMMING] = (task, args) =>
        {
                {
                    var length = (int)(args.Length > 0 ? SafeDouble(args[0]) : 0);
                    return length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SignalHamming(length))
                        : CommonLib.Numerics.SignalHamming(length);
                }
        },
        [BuiltInFunction.SIGNAL_BLACKMAN] = (task, args) =>
        {
                {
                    var length = (int)(args.Length > 0 ? SafeDouble(args[0]) : 0);
                    return length >= 100_000
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.SignalBlackman(length))
                        : CommonLib.Numerics.SignalBlackman(length);
                }
        },
        [BuiltInFunction.SIGNAL_BUTTER] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.SignalButter(SafeDouble(ArgOr(args, 0)), ToDoubleArr(ArgOr(args, 1)), args.Length > 2 ? args[2]?.ToString() ?? "low" : "low");
        },
        [BuiltInFunction.SIGNAL_CHEBY1] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.SignalCheby1(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), ToDoubleArr(ArgOr(args, 2)), args.Length > 3 ? args[3]?.ToString() ?? "low" : "low");
        },
        [BuiltInFunction.SIGNAL_CHEBY2] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.SignalCheby2(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), ToDoubleArr(ArgOr(args, 2)), args.Length > 3 ? args[3]?.ToString() ?? "low" : "low");
        },
        [BuiltInFunction.SIGNAL_ELLIP] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.SignalEllip(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), ToDoubleArr(ArgOr(args, 3)), args.Length > 4 ? args[4]?.ToString() ?? "low" : "low");
        },
        [BuiltInFunction.SIGNAL_LFILTER] = (task, args) =>
        {
                    if (args.Length > 1) return Basic.IlBackend.IlRuntime.SignalLfilter(SafeDouble(args[0]), ToDoubleArr(ArgOr(args, 1)));
                    return Array.Empty<double>();
        },
        [BuiltInFunction.SIGNAL_FILTFILT] = (task, args) =>
        {
                    if (args.Length > 1) return Basic.IlBackend.IlRuntime.SignalFiltfilt(SafeDouble(args[0]), ToDoubleArr(ArgOr(args, 1)));
                    return Array.Empty<double>();
        },
        [BuiltInFunction.SIGNAL_FREQZ] = (task, args) =>
        {
                    if (args.Length > 1) return Basic.IlBackend.IlRuntime.SignalFreqz(SafeDouble(args[0]), SafeDouble(args[1]));
                    return Array.Empty<double>();
        },
        [BuiltInFunction.SIGNAL_FIRWIN] = (task, args) =>
        {
                {
                    var numtaps = (int)SafeDouble(ArgOr(args, 0));
                    var cutoff = ToDoubleArr(ArgOr(args, 1));
                    var btype = args.Length > 2 && args[2] is string bt ? bt : "lowpass";
                    var window = args.Length > 3 && args[3] is string wn ? wn : "hann";
                    return Basic.IlBackend.IlRuntime.Firwin(numtaps, cutoff, btype, window);
                }
        },
        [BuiltInFunction.SIGNAL_FINDPEAKS] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    var distance = args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 1.0;
                    return Basic.IlBackend.IlRuntime.FindPeaks(x, distance);
                }
        },
    };

    private object? ExecuteBuiltInSignal(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SIGNALDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSignal (fn={fn}).");

            }
