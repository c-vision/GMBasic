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
    /// <summary>HTTP: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> HTTPBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.HTTPGET,
        BuiltInFunction.HTTPPOST,
        BuiltInFunction.HTTPPUT,
        BuiltInFunction.HTTPDELETE,
    };

    /// <summary>Http: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _httpDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> HTTPDispatch
        => _httpDispatch ??= BuildHttpDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildHttpDispatch() => new()
    {
        [BuiltInFunction.HTTPGET] = (task, args) =>
        {
                    return ParkHttpBuiltin(task, "GET", args);
        },
        [BuiltInFunction.HTTPPOST] = (task, args) =>
        {
                    return ParkHttpBuiltin(task, "POST", args);
        },
        [BuiltInFunction.HTTPPUT] = (task, args) =>
        {
                    return ParkHttpBuiltin(task, "PUT", args);
        },
        [BuiltInFunction.HTTPDELETE] = (task, args) =>
        {
                    return ParkHttpBuiltin(task, "DELETE", args);
        },
    };

    private object? ExecuteBuiltInHttp(PCodeTask task, BuiltInFunction fn, object?[] args)
        => HTTPDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInHttp (fn={fn}).");

            }
