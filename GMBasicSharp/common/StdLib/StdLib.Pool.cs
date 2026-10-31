// FILE: Interpreter.StdLib.Pool.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Pool — namespace 'POOL'
 * 2 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (2):
 *   GET
 *   PUT
 * TIPI (1):
 *   POOL
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- POOL NAMESPACE (parità Go sync.Pool) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra il namespace 'POOL' con il pool di oggetti riusabili
    /// <c>DIM p AS POOL</c> (parità Go sync.Pool: Get/Put).
    /// </summary>
    private void CreatePoolNamespace()
    {
        const string namespaceName = "POOL";
        var poolParam = new Parameter(-1, T(TokenKind.Identifier, "p"), new PoolType(),
            SymbolFlags.None, null, false);
        var itemParam = new Parameter(-1, T(TokenKind.Identifier, "v"), new InferredType(),
            SymbolFlags.None, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // GET(p) → primo elemento disponibile o NULL (parità Go pool.Get()).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.PoolGet, "GET"),
                ImmutableArray.Create(poolParam), new InferredType(), TokenKind.PoolGet, false),
            // PUT(p, v) → restituisce un elemento al pool (parità Go pool.Put(x)).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.PoolPut, "PUT"),
                ImmutableArray.Create(poolParam, itemParam), new VoidType(), TokenKind.PoolPut, true)
        );

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "POOL", new PoolType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
