// FILE: Interpreter.Gpm.Collections.SparseSet.Core.cs

using System.Collections;

// ReSharper disable CheckNamespace

/* 
 * SparseSet.cs: Sparse Set per insiemi di interi con operazioni O(1) (Add, Remove, HasItem)
 * 
 * FUNZIONI:
 * - Add(value): aggiunge elemento (O(1))
 * - Remove(value): rimuove elemento (O(1))
 * - HasItem(value): verifica presenza (O(1))
 * - Clear(): svuota set (O(1))
 */

namespace CommonLib.Collections;

/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati Sparse Set.
///
/// Teoria e Algoritmi:
/// Uno Sparse Set è una struttura dati altamente specializzata e performante, progettata per memorizzare un
/// insieme di interi non negativi provenienti da un universo molto grande ma "sparso" (pochi elementi
/// effettivamente presenti). La sua efficienza deriva dall'uso di due array:
/// - `_dense[]`: Memorizza gli elementi effettivamente presenti nel set, in modo compatto, all'interno
///   della sua capacità.
/// - `_sparse[]`: Funge da "mappa inversa". Per un valore `v`, `_sparse[v]` contiene l'indice in cui
///   `v` si trova nell'array `_dense`.
/// Questa architettura a doppio array permette di eseguire le operazioni principali in tempo costante:
/// - `Add(v)`: Aggiunge `v` alla fine dell'array `_dense` e registra la sua nuova posizione in `_sparse[v]`.
/// - `HasItem(v)`: Esegue un doppio controllo in O(1): verifica che l'indice in `_sparse[v]` sia valido e che
///   l'elemento corrispondente in `_dense` sia effettivamente `v`.
/// - `Remove(v)`: Per rimuovere `v`, trova la sua posizione `i` in `_dense`. Poi, prende l'ultimo elemento
///   dell'array `_dense` e lo sposta nella posizione `i`, sovrascrivendo `v`. Infine, aggiorna l'indice in
///   `_sparse` per l'elemento spostato. Questo evita "buchi" e costose operazioni di shift degli elementi.
///
/// Performance:
/// Tutte le operazioni chiave - `Add`, `Remove`, e `HasItem` - hanno una complessità temporale a tempo costante O(1).
/// Anche l'operazione di `Clear` è O(1), poiché richiede solo di resettare un contatore. L'iterazione sugli
/// elementi presenti è O(k), dove k è il numero di elementi nel set (non la capacità). Lo svantaggio
/// principale è la complessità spaziale, che è O(M + N), dove M è il valore massimo (`maxVal`) e N è la
/// capacità (`capacity`), e può essere significativa se l'universo dei valori è molto grande.
/// </summary>
public class SparseSet : IEnumerable<int>
{
    private readonly int[] _dense;
    private readonly int[] _sparse;
    private readonly int _maxValue;

    /// <summary>
    /// Ottiene il numero di elementi attualmente contenuti nel set.
    /// </summary>
    public int Count { get; private set; }

    /// <summary>
    /// Ottiene la capacità massima di elementi che il set può contenere.
    /// </summary>
    public int Capacity => _dense.Length;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="SparseSet"/>.
    /// </summary>
    /// <param name="maxValue">Il valore intero massimo (inclusivo) che può essere memorizzato nel set. Definisce la dimensione dell'array "sparso".</param>
    /// <param name="capacity">La capacità massima di elementi che il set può contenere. Definisce la dimensione dell'array "denso".</param>
    /// <exception cref="ArgumentOutOfRangeException">Lanciata se `maxValue` o `capacity` sono negativi.</exception>
    public SparseSet(int maxValue, int capacity)
    {
        if (maxValue < 0)
            throw new ArgumentOutOfRangeException(nameof(maxValue), "Il valore massimo non può essere negativo.");
        if (capacity < 0)
            throw new ArgumentOutOfRangeException(nameof(capacity), "La capacità non può essere negativa.");

        _maxValue = maxValue;
        _sparse = new int[maxValue + 1];
        _dense = new int[capacity];
        Count = 0;
    }

    /// <summary>
    /// Aggiunge un intero non negativo al set. L'operazione ha una complessità di O(1).
    /// </summary>
    /// <param name="value">Il valore da aggiungere.</param>
    /// <exception cref="InvalidOperationException">Lanciata se il set ha raggiunto la sua capacità massima o se il valore è al di fuori dell'intervallo consentito.</exception>
    public void Add(int value)
    {
        if (value < 0 || value > _maxValue)
            throw new InvalidOperationException(
                $"Il valore {value} è fuori dall'intervallo consentito [0, {_maxValue}].");

        if (HasItem(value))
            return;

        if (Count >= Capacity)
            throw new InvalidOperationException("Lo Sparse Set ha raggiunto la sua capacità massima.");

        _dense[Count] = value;
        _sparse[value] = Count;
        Count++;
    }

    /// <summary>
    /// Rimuove un intero dal set. L'operazione ha una complessità di O(1).
    /// </summary>
    /// <param name="value">Il valore da rimuovere.</param>
    /// <returns><c>true</c> se l'elemento è stato trovato e rimosso; altrimenti, <c>false</c>.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se il valore è al di fuori dell'intervallo consentito.</exception>
    public bool Remove(int value)
    {
        if (value < 0 || value > _maxValue)
            throw new InvalidOperationException(
                $"Il valore {value} è fuori dall'intervallo consentito [0, {_maxValue}].");

        if (!HasItem(value))
        {
            return false;
        }

        // Trova l'indice dell'elemento da rimuovere nell'array denso
        var indexToRemove = _sparse[value];

        // Prendi l'ultimo elemento nell'array denso
        var lastValue = _dense[Count - 1];

        // Sovrascrivi l'elemento da rimuovere con l'ultimo elemento
        _dense[indexToRemove] = lastValue;

        // Aggiorna la mappa sparsa per riflettere la nuova posizione dell'ultimo elemento
        _sparse[lastValue] = indexToRemove;

        Count--;
        return true;
    }

    /// <summary>
    /// Verifica se un valore è contenuto nel set. L'operazione ha una complessità di O(1).
    /// </summary>
    /// <param name="value">Il valore da cercare.</param>
    /// <returns><c>true</c> se l'elemento è presente; altrimenti, <c>false</c>.</returns>
    public bool HasItem(int value)
    {
        if (value < 0 || value > _maxValue)
            return false;

        var index = _sparse[value];
        return index >= 0 && index < Count && _dense[index] == value;
    }

    /// <summary>
    /// Rimuove tutti gli elementi dal set. L'operazione ha una complessità di O(1).
    /// </summary>
    public void Clear()
    {
        Count = 0;
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso gli elementi presenti nel set.
    /// </summary>
    public IEnumerator<int> GetEnumerator()
    {
        for (var i = 0; i < Count; i++)
        {
            yield return _dense[i];
        }
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la collezione.
    /// </summary>
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
}
