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
    /// <summary>SPECIAL: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SPECIALBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.SPECIAL_GAMMA,
        BuiltInFunction.SPECIAL_LGAMMA,
        BuiltInFunction.SPECIAL_BETA,
        BuiltInFunction.SPECIAL_ERF,
        BuiltInFunction.SPECIAL_ERFC,
        BuiltInFunction.SPECIAL_GAMMAINC,
        BuiltInFunction.SPECIAL_GAMMAINCC,
        BuiltInFunction.SPECIAL_BETAINC,
        BuiltInFunction.SPECIAL_DIGAMMA,
        BuiltInFunction.SPECIAL_BESSELJ,
        BuiltInFunction.SPECIAL_BESSELY,
        BuiltInFunction.SPECIAL_BESSELI,
        BuiltInFunction.SPECIAL_BESSELK,
        BuiltInFunction.SPECIAL_BETALN,
        BuiltInFunction.SPECIAL_ERFINV,
        BuiltInFunction.SPECIAL_ERFCINV,
        BuiltInFunction.SPECIAL_GAMMAINCINV,
        BuiltInFunction.SPECIAL_GAMMAINCCINV,
        BuiltInFunction.SPECIAL_BETAINCINV,
    };

    /// <summary>Special: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _specialDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SPECIALDispatch
        => _specialDispatch ??= BuildSpecialDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSpecialDispatch() => new()
    {
        [BuiltInFunction.SPECIAL_GAMMA] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Gamma(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_LGAMMA] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.LGamma(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BETA] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.Beta(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_ERF] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Erf(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_ERFC] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Erfc(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_GAMMAINC] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.GammaInc(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_GAMMAINCC] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.GammaIncc(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BETAINC] = (task, args) =>
        {
                return args.Length > 2 && args[0] != null && args[1] != null && args[2] != null
                ? Basic.IlBackend.IlRuntime.BetaInc(SafeDouble(args[0]), SafeDouble(args[1]), SafeDouble(args[2])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_DIGAMMA] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.Digamma(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BESSELJ] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.BesselJ(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BESSELY] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.BesselY(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BESSELI] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.BesselI(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BESSELK] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.BesselK(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BETALN] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.BetaLn(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_ERFINV] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.ErfInv(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_ERFCINV] = (task, args) =>
        {
                return args.Length > 0 && args[0] != null ? Basic.IlBackend.IlRuntime.ErfcInv(SafeDouble(args[0])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_GAMMAINCINV] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.GammaIncInv(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_GAMMAINCCINV] = (task, args) =>
        {
                return args.Length > 1 && args[0] != null && args[1] != null
                ? Basic.IlBackend.IlRuntime.GammaInccInv(SafeDouble(args[0]), SafeDouble(args[1])) : 0.0;
        },
        [BuiltInFunction.SPECIAL_BETAINCINV] = (task, args) =>
        {
                return args.Length > 2 && args[0] != null && args[1] != null && args[2] != null
                ? Basic.IlBackend.IlRuntime.BetaIncInv(SafeDouble(args[0]), SafeDouble(args[1]), SafeDouble(args[2])) : 0.0;
        },
    };

    private object? ExecuteBuiltInSpecial(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SPECIALDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSpecial (fn={fn}).");

}
