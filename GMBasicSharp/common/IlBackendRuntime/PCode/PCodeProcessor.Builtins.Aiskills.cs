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
    /// <summary>AISKILLS: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AISKILLSBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AISKILLSADD,
        BuiltInFunction.AISKILLSREMOVE,
        BuiltInFunction.AISKILLSREFRESH,
        BuiltInFunction.AISKILLSSETINDEX,
        BuiltInFunction.AISKILLSMARKETPLACE,
        BuiltInFunction.AISKILLSPROMPT,
        BuiltInFunction.AISKILLSLIST,
        BuiltInFunction.AISKILLSPATHS,
    };

    /// <summary>Aiskills: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aiskillsDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AISKILLSDispatch
        => _aiskillsDispatch ??= BuildAiskillsDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAiskillsDispatch() => new()
    {
        [BuiltInFunction.AISKILLSADD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsAdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISKILLSREMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsRemove(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISKILLSREFRESH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsRefresh();
        },
        [BuiltInFunction.AISKILLSSETINDEX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsSetIndex(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISKILLSMARKETPLACE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsMarketplace(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISKILLSPROMPT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsPrompt();
        },
        [BuiltInFunction.AISKILLSLIST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsList();
        },
        [BuiltInFunction.AISKILLSPATHS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSkillsPaths();
            
        },
    };

    private object? ExecuteBuiltInAiskills(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AISKILLSDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAiskills (fn={fn}).");

}
