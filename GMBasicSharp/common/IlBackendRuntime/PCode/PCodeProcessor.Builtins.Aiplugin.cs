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
    /// <summary>AIPLUGIN: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AIPLUGINBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIPLUGINSADD,
        BuiltInFunction.AIPLUGINSREMOVE,
        BuiltInFunction.AIPLUGINSREFRESH,
        BuiltInFunction.AIPLUGINSSETINDEX,
        BuiltInFunction.AIPLUGINSLIST,
        BuiltInFunction.AIPLUGINSENABLE,
        BuiltInFunction.AIPLUGINSDISABLE,
        BuiltInFunction.AIPLUGINSMARKETPLACE,
        BuiltInFunction.AIPLUGINSCREATE,
        BuiltInFunction.AIPLUGINCALL,
        BuiltInFunction.AIPLUGININFO,
        BuiltInFunction.AIPLUGINLIST,
    };

    /// <summary>Aiplugin: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aipluginDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AIPLUGINDispatch
        => _aipluginDispatch ??= BuildAipluginDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAipluginDispatch() => new()
    {
        [BuiltInFunction.AIPLUGINSADD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsAdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINSREMOVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsRemove(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINSREFRESH] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsRefresh();
        },
        [BuiltInFunction.AIPLUGINSSETINDEX] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsSetIndex(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINSLIST] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsList();
        },
        [BuiltInFunction.AIPLUGINSENABLE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsEnable(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINSDISABLE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsDisable(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINSMARKETPLACE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsMarketplace(
                        ArgOr(args, 0)?.ToString() ?? "",
                        args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.AIPLUGINSCREATE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginsCreate(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINCALL] = (task, args) =>
        {
                    // Invoke-by-name via IlRuntime.InvokeBasicProcedureGpm: quando la procedura è un
                    // chunk PCode (registrata dal PCodeProcessor), l'hook spawna il task PCode col
                    // future; altrimenti il path IL. La GPM è l'unico esecutore, mai il thread corrente.
                    return Basic.IlBackend.IlRuntime.AiPluginCall(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGININFO] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginInfo(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPLUGINLIST] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPluginList();
        },
    };

    private object? ExecuteBuiltInAiplugin(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AIPLUGINDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAiplugin (fn={fn}).");

}
