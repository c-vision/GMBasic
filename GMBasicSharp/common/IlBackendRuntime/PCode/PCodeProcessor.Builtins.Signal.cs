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
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).
    /// Resta solo WELCH (handle matrice): le altre 17 funzioni sono nella lib FFI `signal`.</summary>
    private static readonly HashSet<BuiltInFunction> SIGNALBuiltInFunctions = new HashSet<BuiltInFunction>
            {
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
    };

    private object? ExecuteBuiltInSignal(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SIGNALDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSignal (fn={fn}).");

            }
