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
    /// <summary>DATE: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> DATEBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.DATEVALUE,
        BuiltInFunction.DATESERIAL,
        BuiltInFunction.DATE_FUNC,
        BuiltInFunction.STRDATE,
        BuiltInFunction.DATEFROMUNIX,
        BuiltInFunction.DATEDIFF,
        BuiltInFunction.DATEADD,
    };

    /// <summary>Date: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _dateDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> DATEDispatch
        => _dateDispatch ??= BuildDateDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildDateDispatch() => new()
    {
        [BuiltInFunction.DATEVALUE] = (task, args) =>
        {
                // Canone: serial OADate numerico (come IL e i backend C), non DateTime — altrimenti PRINT/FORMAT divergono.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.DateValue(args[0]?.ToString() ?? "").ToOADate();
                return Basic.IlBackend.IlRuntime.DateValue("").ToOADate();
        },
        [BuiltInFunction.DATESERIAL] = (task, args) =>
        {
                if (args.Length >= 3) return Basic.IlBackend.IlRuntime.DateSerial(
                (short)SafeDouble(args[0]), (short)SafeDouble(args[1]), (short)SafeDouble(args[2])).ToOADate();
                return Basic.IlBackend.IlRuntime.DateSerial(0, 1, 1).ToOADate();
        },
        [BuiltInFunction.DATE_FUNC] = (task, args) =>
        {
                // Canone data/ora (UTC): DATE() = seriale giorno numerico; STRDATE()/DATE$ = stringa.
                return Basic.IlBackend.IlRuntime.DaySerial();
        },
        [BuiltInFunction.STRDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.StrDate();
        },
        [BuiltInFunction.DATEFROMUNIX] = (task, args) =>
        {
                // Runtime comune (IlRuntime): DateTime — unica implementazione.
                if (args.Length > 0) return Basic.IlBackend.IlRuntime.DateFromUnix((long)SafeDouble(args[0]));
                return 0.0;
        },
        [BuiltInFunction.DATEDIFF] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                if (args.Length >= 3) return Basic.IlBackend.IlRuntime.DateDiff(
                args[0]?.ToString() ?? "d", SafeDouble(args[1]), SafeDouble(args[2]));
                return 0.0;
        },
        [BuiltInFunction.DATEADD] = (task, args) =>
        {
                // Runtime comune (IlRuntime): CoreDateAdd — unica implementazione.
                if (args.Length >= 3) return Basic.IlBackend.IlRuntime.DateAdd(
                args[0]?.ToString() ?? "d", SafeDouble(args[1]), DateTime.FromOADate(SafeDouble(args[2])));
                return 0.0;
        },
    };

    private object? ExecuteBuiltInDate(PCodeTask task, BuiltInFunction fn, object?[] args)
        => DATEDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInDate (fn={fn}).");

}
