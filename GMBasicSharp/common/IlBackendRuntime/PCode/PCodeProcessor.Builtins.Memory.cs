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
    /// <summary>MEMORY: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> MEMORYBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.MEMORYASK,
        BuiltInFunction.MEMORYDELETE,
        BuiltInFunction.MEMORYFORGET,
        BuiltInFunction.MEMORYIMPORT,
        BuiltInFunction.MEMORYLIST,
        BuiltInFunction.MEMORYLOAD,
        BuiltInFunction.MEMORYNOTE,
        BuiltInFunction.MEMORYREFRESH,
        BuiltInFunction.MEMORYSAVE,
        BuiltInFunction.MEMORYSEARCH,
        BuiltInFunction.MEMORYSTORE,
        BuiltInFunction.MEMORYWATCH,
        BuiltInFunction.MEMORYRANK,
        BuiltInFunction.MEMORYCONSOLIDATE,
    };

    /// <summary>Memory: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _memoryDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> MEMORYDispatch
        => _memoryDispatch ??= BuildMemoryDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildMemoryDispatch() => new()
    {
        [BuiltInFunction.MEMORYSTORE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                return Basic.IlBackend.IlRuntime.MemoryStore(
                ArgOr(args, 0)?.ToString() ?? "",
                args.Length > 1 ? ArgOr(args, 1) : null,
                args.Length > 2 && SafeDouble(ArgOr(args, 2)) != 0 ? 1 : 0);
        },
        [BuiltInFunction.MEMORYLOAD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryLoad(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYSEARCH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemorySearch(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYDELETE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryDelete(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYLIST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryList();
        },
        [BuiltInFunction.MEMORYASK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryAsk(ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? SafeDouble(args[1]) : 0);
        },
        [BuiltInFunction.MEMORYFORGET] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryForget(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYIMPORT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryImport(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYNOTE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryNote(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYREFRESH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryRefresh();
        },
        [BuiltInFunction.MEMORYSAVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemorySave(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYWATCH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryWatch(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.MEMORYRANK] = (task, args) =>
        {
                // Default topK=5 come IL (EmitMemoryAskCall: Ldc_R8 5.0); 0 darebbe Take(0)=vuoto.
                return Basic.IlBackend.IlRuntime.MemoryRank(ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? SafeDouble(args[1]) : 5);
        },
        [BuiltInFunction.MEMORYCONSOLIDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MemoryConsolidate();
        },
    };

    private object? ExecuteBuiltInMemory(PCodeTask task, BuiltInFunction fn, object?[] args)
        => MEMORYDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInMemory (fn={fn}).");

}
