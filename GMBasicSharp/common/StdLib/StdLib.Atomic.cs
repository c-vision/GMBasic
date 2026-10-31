// FILE: Interpreter.StdLib.Atomic.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Atomic — namespace 'ATOMIC'
 * 5 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (5):
 *   COMPARESWAP
 *   FETCHVALUE
 *   INCREMENT
 *   STORE
 *   SWAP
 * TIPI (1):
 *   ATOMIC
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- ATOMIC NAMESPACE (parità Go sync/atomic) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra il namespace 'ATOMIC' con le primitive di accesso atomico a un box
    /// <c>DIM x AS ATOMIC</c> (parità Go sync/atomic: AddInt64/LoadInt64/StoreInt64/
    /// CompareAndSwapInt64).
    /// </summary>
    private void CreateAtomicNamespace()
    {
        const string namespaceName = "ATOMIC";
        var atomicParam = new Parameter(-1, T(TokenKind.Identifier, "x"), new AtomicType(),
            SymbolFlags.None, null, false);
        var deltaParam = new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(),
            SymbolFlags.None, null, false);
        var valueParam = new Parameter(-1, T(TokenKind.Identifier, "v"), new LongType(),
            SymbolFlags.None, null, false);
        var oldParam = new Parameter(-1, T(TokenKind.Identifier, "oldValue"), new LongType(),
            SymbolFlags.None, null, false);
        var newParam = new Parameter(-1, T(TokenKind.Identifier, "newValue"), new LongType(),
            SymbolFlags.None, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // INCREMENT(x, n) → valore PRECEDENTE (parità Go atomic.AddInt64).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AtomicIncrement, "INCREMENT"),
                ImmutableArray.Create(atomicParam, deltaParam), new LongType(), TokenKind.AtomicIncrement, false),
            // FETCHVALUE(x) → valore corrente (parità Go atomic.LoadInt64).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AtomicFetchValue, "FETCHVALUE"),
                ImmutableArray.Create(atomicParam), new LongType(), TokenKind.AtomicFetchValue, false),
            // STORE(x, v) (parità Go atomic.StoreInt64).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AtomicStore, "STORE"),
                ImmutableArray.Create(atomicParam, valueParam), new VoidType(), TokenKind.AtomicStore, true),
            // COMPARESWAP(x, oldValue, newValue) → BIT (parità Go atomic.CompareAndSwapInt64).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AtomicCompareSwap, "COMPARESWAP"),
                ImmutableArray.Create(atomicParam, oldParam, newParam), new BitType(), TokenKind.AtomicCompareSwap, false),
            // SWAP(x, newValue) → valore PRECEDENTE (parità Go atomic.SwapInt64).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AtomicSwap, "SWAP"),
                ImmutableArray.Create(atomicParam, newParam), new LongType(), TokenKind.AtomicSwap, false)
        );

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ATOMIC", new AtomicType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
