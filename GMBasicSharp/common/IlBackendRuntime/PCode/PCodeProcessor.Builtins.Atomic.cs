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
    /// <summary>ATOMIC: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> ATOMICBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.ATOMICINCREMENT,
        BuiltInFunction.ATOMICFETCHVALUE,
        BuiltInFunction.ATOMICSTORE,
        BuiltInFunction.ATOMICCOMPARESWAP,
        BuiltInFunction.ATOMICSWAP,
    };

    /// <summary>Atomic: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private static readonly Dictionary<BuiltInFunction, Func<PCodeProcessor, PCodeTask, object?[], object?>> ATOMICDispatch = new()
    {
        [BuiltInFunction.ATOMICINCREMENT] = (_, _, args) =>
        {
                {
                // INCREMENT(x, n): ritorna il NUOVO valore (parità atomic.AddInt64).
                if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodeAtomic atomAdd) return 0.0;
                return atomAdd.Add(Convert.ToInt64(SafeDouble(args[1])));
                }
        },
        [BuiltInFunction.ATOMICFETCHVALUE] = (_, _, args) =>
        {
                {
                if (args.Length < 1 || args[0] is not Basic.Runtime.Gpm.PCodeAtomic atomLoad) return 0.0;
                return atomLoad.Load();
                }
        },
        [BuiltInFunction.ATOMICSTORE] = (_, _, args) =>
        {
                {
                if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodeAtomic atomStore) return 0.0;
                atomStore.Store(Convert.ToInt64(SafeDouble(args[1])));
                return 0.0;
                }
        },
        [BuiltInFunction.ATOMICCOMPARESWAP] = (_, _, args) =>
        {
                {
                // COMPARESWAP(x, old, new): ritorna BIT (1 se il CAS è riuscito, 0 altrimenti —
                // parità IL/VM/C/C99Native; Go CompareAndSwapInt64 ritorna bool → convenzione 1/0).
                if (args.Length < 3 || args[0] is not Basic.Runtime.Gpm.PCodeAtomic atomCas) return 0.0;
                return atomCas.CompareSwap(Convert.ToInt64(SafeDouble(args[1])), Convert.ToInt64(SafeDouble(args[2]))) ? 1.0 : 0.0;
                }
        },
        [BuiltInFunction.ATOMICSWAP] = (_, _, args) =>
        {
                {
                // SWAP(x, new): ritorna il valore PRECEDENTE (parità Go atomic.SwapInt64).
                if (args.Length < 2 || args[0] is not Basic.Runtime.Gpm.PCodeAtomic atomSwap) return 0.0;
                return atomSwap.Swap(Convert.ToInt64(SafeDouble(args[1])));
                }
        },
    };

    private object? ExecuteBuiltInAtomic(PCodeTask task, BuiltInFunction fn, object?[] args)
        => ATOMICDispatch.TryGetValue(fn, out var handler)
            ? handler(this, task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAtomic (fn={fn}).");

}
