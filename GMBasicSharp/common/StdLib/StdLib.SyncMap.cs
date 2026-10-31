// FILE: Interpreter.StdLib.SyncMap.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: SyncMap — namespace 'SYNCMAP'
 * 4 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   DELETE
 *   LOAD
 *   LOADORSTORE
 *   STORE
 * TIPI (1):
 *   CONCURRENTMAP
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SYNCMAP NAMESPACE (parità Go sync.Map) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra il namespace 'SYNCMAP' con la mappa concorrente
    /// <c>DIM m AS CONCURRENTMAP</c> (parità Go sync.Map: Store/Load/Delete/LoadOrStore).
    /// </summary>
    private void CreateSyncMapNamespace()
    {
        const string namespaceName = "SYNCMAP";
        var mapParam = new Parameter(-1, T(TokenKind.Identifier, "m"), new ConcurrentMapType(),
            SymbolFlags.None, null, false);
        var keyParam = new Parameter(-1, T(TokenKind.Identifier, "k"), new AnyType(),
            SymbolFlags.None, null, false);
        var valParam = new Parameter(-1, T(TokenKind.Identifier, "v"), new AnyType(),
            SymbolFlags.None, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // STORE(m, k, v) (parità Go m.Store(key, value)).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SyncMapStore, "STORE"),
                ImmutableArray.Create(mapParam, keyParam, valParam), new VoidType(), TokenKind.SyncMapStore, true),
            // LOAD(m, k) → valore o NULL (parità Go value, ok := m.Load(key)).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SyncMapLoad, "LOAD"),
                ImmutableArray.Create(mapParam, keyParam), new AnyType(), TokenKind.SyncMapLoad, false),
            // DELETE(m, k) → valore rimosso o NULL (parità Go m.Delete(key)).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SyncMapDelete, "DELETE"),
                ImmutableArray.Create(mapParam, keyParam), new AnyType(), TokenKind.SyncMapDelete, false),
            // LOADORSTORE(m, k, v) → valore esistente o il nuovo (parità Go m.LoadOrStore).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SyncMapLoadOrStore, "LOADORSTORE"),
                ImmutableArray.Create(mapParam, keyParam, valParam), new AnyType(), TokenKind.SyncMapLoadOrStore, false)
        );

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "CONCURRENTMAP", new ConcurrentMapType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
