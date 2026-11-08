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
    /// <summary>ISAM: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> ISAMBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.ISAMINFO,
        BuiltInFunction.ISAMLOG,
        BuiltInFunction.ISAMREPLICATIONSHIP,
        BuiltInFunction.ISAMREPLICATIONRECV,
    };

    /// <summary>Isam: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _isamDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> ISAMDispatch
        => _isamDispatch ??= BuildIsamDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildIsamDispatch() => new()
    {
        [BuiltInFunction.ISAMINFO] = (task, args) =>
        {
                // Runtime comune (IlRuntime.IsamInfo): JSON d'ispezione completo via
                // IsamDriver/Engine (parità col vecchio PCode che accedeva a ResolveIsamArea)
                // — unica implementazione. Ritorna stringa JSON (canone IL), non JsonWrapper.
                    return Basic.IlBackend.IlRuntime.IsamInfo(
                args.Length > 0 ? args[0]?.ToString() : null);
                // ISAM.LOG([file | alias]) → riepilogo WAL (stringa).
        },
        [BuiltInFunction.ISAMLOG] = (task, args) =>
        {
                {
                    var isamArea2 = ResolveIsamArea(task, args.Length > 0 ? args[0] : null);
                    if (isamArea2?.Engine == null) return "";
                    return isamArea2.Engine.GetWalLogSummary();
                }
                // ISAM.REPLICATIONSHIP([file | alias,] host, port) → LONG.
        },
        [BuiltInFunction.ISAMREPLICATIONSHIP] = (task, args) =>
        {
                {
                try
                {
                    var (iEng, iHost, iPort) = args.Length >= 3
                ? (ResolveIsamArea(task, args[0])?.Engine, args[1]?.ToString() ?? "", (int)SafeDouble(args[2]))
                : (ResolveIsamArea(task, null)?.Engine, args[0]?.ToString() ?? "", (int)SafeDouble(args[1]));
                    return iEng != null ? (long)iEng.ReplicationShipTcp(iHost, iPort) : 0L;
                }
                    catch { return 0L; }
                }
                // ISAM.REPLICATIONRECV([file | alias,] port) → LONG.
        },
        [BuiltInFunction.ISAMREPLICATIONRECV] = (task, args) =>
        {
                {
                try
                {
                    var (iEng2, iPort2) = args.Length >= 2
                ? (ResolveIsamArea(task, args[0])?.Engine, (int)SafeDouble(args[1]))
                : (ResolveIsamArea(task, null)?.Engine, (int)SafeDouble(args[0]));
                    return iEng2 != null ? (long)iEng2.ReplicationRecvTcp(iPort2) : 0L;
                }
                    catch { return 0L; }
                }
            
                // SPEED.SETDURABILITY([dbAlias,] mode) → mode applicato (Full/Buffered).
        },
    };

    private object? ExecuteBuiltInIsam(PCodeTask task, BuiltInFunction fn, object?[] args)
        => ISAMDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInIsam (fn={fn}).");

            }
