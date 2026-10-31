// FILE: Basic.CommonLib.Collections.IKeyValueIndexable.cs
// Interfaccia NON generica comune per le collection chiave→valore che la PCodeVM (e il backend
// IL) devono indicizzare a runtime senza reflection. La reflection per nome (GetMethod("Add"),
// TryGetValue, ecc.) impedisce a illink di rimuovere i metodi non usati dai binari NativeAOT:
// con il pattern-matching su questa interfaccia, i call sono statici e illink li segue.

namespace CommonLib.Collections;

/// <summary>
/// Contratto comune (non generico, su <c>object?</c>) per le collection indicizzabili per
/// chiave usate dal linguaggio (RedBlackTree, Cache, BPlusTree, ...). Espone le operazioni
/// che la PCodeVM fa a runtime su un'istanza tipizzata solo come <c>object?</c>.
/// </summary>
public interface IKeyValueIndexable
{
    /// <summary>Cerca la chiave; true se trovata (value = valore associato).</summary>
    bool TryGetValue(object? key, out object? value);

    /// <summary>Imposta o aggiunge la coppia chiave→valore (semantica indexer/Add).</summary>
    void Set(object? key, object? value);

    /// <summary>Recupera la coppia minima; true se l'albero non è vuoto.</summary>
    bool TryGetMin(out object? key, out object? value);

    /// <summary>Recupera la coppia massima; true se l'albero non è vuoto.</summary>
    bool TryGetMax(out object? key, out object? value);

    /// <summary>Enumera le coppie (per KEYS/iterazione non tipizzata).</summary>
    IEnumerable<KeyValuePair<object?, object?>> Entries();
}
