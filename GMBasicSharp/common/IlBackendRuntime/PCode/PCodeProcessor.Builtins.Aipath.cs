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
    /// <summary>AIPATH: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AIPATHBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIPATHMODELADD,
        BuiltInFunction.AIPATHMODELREMOVE,
        BuiltInFunction.AIPATHSKILLADD,
        BuiltInFunction.AIPATHSKILLREMOVE,
        BuiltInFunction.AIPATHPPLUGINADD,
        BuiltInFunction.AIPATHPPLUGINREMOVE,
    };

    /// <summary>Aipath: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aipathDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AIPATHDispatch
        => _aipathDispatch ??= BuildAipathDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAipathDispatch() => new()
    {
        [BuiltInFunction.AIPATHMODELADD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathModelAdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPATHMODELREMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathModelRemove(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPATHSKILLADD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathSkillAdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPATHSKILLREMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathSkillRemove(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPATHPPLUGINADD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathPluginAdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPATHPPLUGINREMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiPathPluginRemove(ArgOr(args, 0)?.ToString() ?? "");
        },
    };

    private object? ExecuteBuiltInAipath(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AIPATHDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAipath (fn={fn}).");

}
