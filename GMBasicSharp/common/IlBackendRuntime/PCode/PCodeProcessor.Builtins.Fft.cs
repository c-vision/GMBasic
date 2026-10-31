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
    /// <summary>FFT: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> FFTBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.FFTCOMPLEX,
        BuiltInFunction.FFT_RFFT,
        BuiltInFunction.FFT_IRFFT,
        BuiltInFunction.FFT,
        BuiltInFunction.FFTFREQ,
        BuiltInFunction.FFTSHIFT,
    };

    /// <summary>Fft: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _fftDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> FFTDispatch
        => _fftDispatch ??= BuildFftDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildFftDispatch() => new()
    {
        [BuiltInFunction.FFTCOMPLEX] = (task, args) =>
        {
                // Runtime comune (CommonLib — object[,] n×2 re/im per riga, come FFT reale).
                // Prima ritornava IlRuntime.FftComplex (double[][] via MatrixToJagged): il DIM
                // matrice PCode materializza object[,], NON double[][] → output vuoto (no-op).
                    if (args.Length >= 2)
                {
                    var real = ToDoubleArr(ArgOr(args, 0));
                    var imag = ToDoubleArr(ArgOr(args, 1));
                    return real.Length >= 4096
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Fft(real, imag))
                        : CommonLib.Numerics.Fft(real, imag);
                }
                    var fftInput = ToDoubleArr(ArgOr(args, 0));
                    return fftInput.Length >= 4096
                        ? ParkCpuBuiltin(task, () => Basic.IlBackend.IlRuntime.Fft(fftInput))
                        : Basic.IlBackend.IlRuntime.Fft(fftInput);
        },
        [BuiltInFunction.FFT_RFFT] = (task, args) =>
        {
                {
                    var x = ToDoubleArr(ArgOr(args, 0));
                    object? ComputeRfft()
                {
                    var r = CommonLib.Numerics.Rfft(x);
                    var m = r.GetLength(0);
                    var dm = new double[m, 2];
                for (var i = 0; i < m; i++) { dm[i, 0] = Convert.ToDouble(r[i, 0]); dm[i, 1] = Convert.ToDouble(r[i, 1]); }
                    return LuStoreMatrix(dm);
                }
                    return x.Length >= 4096
                        ? ParkCpuBuiltin(task, ComputeRfft)
                        : ComputeRfft();
                }
        },
        [BuiltInFunction.FFT_IRFFT] = (task, args) =>
        {
                {
                    var re = ToDoubleArr(ArgOr(args, 0));
                    var im = ToDoubleArr(ArgOr(args, 1));
                    return re.Length >= 4096
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Irfft(re, im))
                        : CommonLib.Numerics.Irfft(re, im);
                }
        },
        [BuiltInFunction.FFT] = (task, args) =>
        {
                    if (args.Length > 0 && args[0] is System.Collections.IList fftIn)
                {
                    var input = ToDoubleArray(fftIn);
                    return input.Length >= 4096
                        ? ParkCpuBuiltin(task, () => CommonLib.Numerics.Fft(input))
                        : CommonLib.Numerics.Fft(input);
                }
                    return null;
        },
        [BuiltInFunction.FFTFREQ] = (task, args) =>
        {
                    if (args.Length >= 1)
                    return Basic.IlBackend.IlRuntime.FftFreq(SafeDouble(args[0]), args.Length > 1 ? SafeDouble(args[1]) : 1.0);
                    return Array.Empty<double>();
        },
        [BuiltInFunction.FFTSHIFT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.FftShift(ToDoubleArr(ArgOr(args, 0)));
        },
    };

    private object? ExecuteBuiltInFft(PCodeTask task, BuiltInFunction fn, object?[] args)
        => FFTDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInFft (fn={fn}).");

            }
