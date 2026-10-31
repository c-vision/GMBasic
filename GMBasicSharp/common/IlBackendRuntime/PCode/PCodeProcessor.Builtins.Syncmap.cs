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
    /// <summary>SYNCMAP: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SYNCMAPBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SYNCMAPSTORE,
        BuiltInFunction.SYNCMAPLOAD,
        BuiltInFunction.SYNCMAPDELETE,
        BuiltInFunction.SYNCMAPLOADORSTORE,
    };

    /// <summary>Syncmap: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private static readonly Dictionary<BuiltInFunction, Func<PCodeProcessor, PCodeTask, object?[], object?>> SYNCMAPDispatch = new()
    {
        [BuiltInFunction.SYNCMAPSTORE] = (_, _, args) =>
        {
                {
                    if (args.Length < 3 || args[0] is not Basic.Runtime.Gpm.PCodeConcurrentMap mapSt) return 0.0;
                mapSt.Store(args[1]!, args[2]);
                    return 0.0;
                }
        },
        [BuiltInFunction.SYNCMAPLOAD] = (_, _, args) =>
        {
                {
                    if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodeConcurrentMap mapLd) return null;
                    return mapLd.Load(args[1]!);
                }
        },
        [BuiltInFunction.SYNCMAPDELETE] = (_, _, args) =>
        {
                {
                    if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodeConcurrentMap mapDel) return null;
                    return mapDel.Delete(args[1]!);
                }
        },
        [BuiltInFunction.SYNCMAPLOADORSTORE] = (_, _, args) =>
        {
                {
                    if (args.Length < 3 || args[0] is not Basic.Runtime.Gpm.PCodeConcurrentMap mapLos) return null;
                    return mapLos.LoadOrStore(args[1]!, args[2]);
                }
        },
    };

    private object? ExecuteBuiltInSyncmap(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SYNCMAPDispatch.TryGetValue(fn, out var handler)
            ? handler(this, task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSyncmap (fn={fn}).");

            }
