// FILE: Interpreter.Gpm.Collections.BinaryHeap.Core.cs
using System.Collections;
// ReSharper disable CheckNamespace

/* 
 * BinaryHeap.cs: heap binario (Min-Heap/Max-Heap) con Add O(log n), Extract O(log n), Peek O(1)
 * 
 * FUNZIONI:
 * - Add(item): inserisce elemento (O(log n))
 * - Extract(): rimuove e restituisce radice (O(log n))
 * - Peek(): restituisce radice senza rimuovere (O(1))
 * - Clear(): svuota heap
 * - ToArray(): esporta in array
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati Heap Binario (Binary Heap) generica.
///
/// Teoria e Algoritmi:
/// Un Heap Binario è una struttura dati ad albero binario quasi completo che soddisfa la "proprietà di heap".
/// A seconda della variante, può essere un Min-Heap (ogni nodo genitore è minore o uguale ai suoi figli) o
/// un Max-Heap (ogni nodo genitore è maggiore o uguale ai suoi figli). Questa struttura è comunemente
/// implementata usando un array o una lista, dove le relazioni genitore-figlio sono calcolate tramite
/// aritmetica sugli indici: per un nodo all'indice `i`, i figli si trovano a `2*i + 1` e `2*i + 2`,
/// e il genitore si trova a `(i - 1) / 2`. Questo approccio basato su array è estremamente efficiente
/// in termini di spazio, eliminando l'overhead dei puntatori.
/// Le operazioni principali sono:
/// - Inserimento (`Add`): Un nuovo elemento viene aggiunto alla fine dell'array. Successivamente, l'algoritmo
///   `HeapifyUp` ("bubble-up") lo sposta verso l'alto, scambiandolo con il suo genitore finché la proprietà di heap è ripristinata.
/// - Estrazione della Radice (`Extract`): L'elemento alla radice (indice 0) viene rimosso. Viene sostituito
///   con l'ultimo elemento dell'array, e l'algoritmo `HeapifyDown` ("bubble-down") lo sposta verso il basso,
///   scambiandolo con il figlio appropriato (il più piccolo in un Min-Heap) finché la proprietà di heap non è ripristinata.
///
/// Performance:
/// L'altezza di un heap binario è sempre O(log n). Di conseguenza, le operazioni fondamentali di inserimento (`Add`)
/// ed estrazione della radice (`Extract`) hanno una complessità temporale garantita di O(log n). L'operazione
/// per visualizzare l'elemento minimo o massimo (`Peek`) è a tempo costante O(1), poiché si trova sempre
/// alla radice dell'albero (indice 0 dell'array). La complessità spaziale è O(n). Queste caratteristiche
/// rendono l'Heap Binario una delle implementazioni più efficienti per le code di priorità.
/// </summary>
/// <typeparam name="T">Il tipo degli elementi nell'heap.</typeparam>
public class BinaryHeap<T> : IEnumerable<T> where T: notnull
{
    /// <summary>
    /// La lista interna che memorizza gli elementi dell'heap secondo la struttura ad albero,
    /// dove le relazioni genitore-figlio sono calcolate tramite aritmetica sugli indici.
    /// </summary>
    private readonly List<T> _items;
    
    /// <summary>
    /// Il comparatore utilizzato per ordinare gli elementi, determinando se si tratta
    /// di un Min-Heap (comparatore di default) o di un Max-Heap (comparatore inverso).
    /// </summary>
    private readonly IComparer<T> _comparer;

    /// <summary>
    /// Ottiene il numero di elementi contenuti nell'heap.
    /// </summary>
    public int Count => _items.Count;

    /// <summary>
    /// Inizializza una nuova istanza vuota della classe <see cref="BinaryHeap{T}"/>.
    /// </summary>
    /// <param name="comparer">Il comparatore da utilizzare per ordinare gli elementi.
    /// Per un Min-Heap, usare `Comparer&lt;T&gt;.Default`. Per un Max-Heap, usare un comparatore inverso.</param>
    public BinaryHeap(IComparer<T> comparer)
    {
        _items = [];
        _comparer = comparer;
    }

    /// <summary>
    /// Aggiunge un elemento all'heap mantenendo la proprietà di heap.
    /// L'operazione ha una complessità temporale di O(log n).
    /// </summary>
    /// <param name="item">L'elemento da aggiungere all'heap.</param>
    public void Add(T item)
    {
        _items.Add(item);
        HeapifyUp(_items.Count - 1);
    }

    /// <summary>
    /// Rimuove e restituisce l'elemento radice dell'heap (l'elemento con la priorità più alta).
    /// L'operazione ha una complessità temporale di O(log n).
    /// </summary>
    /// <returns>L'elemento rimosso dalla radice dell'heap.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se l'heap è vuoto.</exception>
    public T Extract()
    {
        if (_items.Count == 0)
        {
            throw new InvalidOperationException("L'heap è vuoto.");
        }

        var root = _items[0];
        var lastItem = _items[^1];
        _items[0] = lastItem;
        _items.RemoveAt(_items.Count - 1);

        if (_items.Count > 0)
        {
            HeapifyDown(0);
        }

        return root;
    }

    /// <summary>
    /// Restituisce l'elemento radice dell'heap senza rimuoverlo.
    /// L'operazione ha una complessità temporale di O(1).
    /// </summary>
    /// <returns>L'elemento alla radice dell'heap.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se l'heap è vuoto.</exception>
    public T Peek()
    {
        if (_items.Count == 0)
        {
            throw new InvalidOperationException("L'heap è vuoto.");
        }

        return _items[0];
    }

    /// <summary>
    /// Rimuove tutti gli elementi dall'heap.
    /// </summary>
    public void Clear()
    {
        _items.Clear();
    }

    /// <summary>
    /// Ripristina la proprietà di heap spostando un elemento verso l'alto nell'albero.
    /// Viene chiamato dopo un inserimento.
    /// </summary>
    /// <param name="index">L'indice dell'elemento da spostare verso l'alto.</param>
    private void HeapifyUp(int index)
    {
        while (index > 0)
        {
            var parentIndex = (index - 1) / 2;
            if (_comparer.Compare(_items[index], _items[parentIndex]) >= 0)
            {
                break; // La proprietà di heap è soddisfatta.
            }

            Swap(index, parentIndex);
            index = parentIndex;
        }
    }

    /// <summary>
    /// Ripristina la proprietà di heap spostando un elemento verso il basso nell'albero.
    /// Viene chiamato dopo l'estrazione della radice.
    /// </summary>
    /// <param name="index">L'indice dell'elemento da spostare verso il basso.</param>
    private void HeapifyDown(int index)
    {
        while (true)
        {
            var leftChildIndex = 2 * index + 1;
            var rightChildIndex = 2 * index + 2;
            var smallestChildIndex = index;

            if (leftChildIndex < _items.Count &&
                _comparer.Compare(_items[leftChildIndex], _items[smallestChildIndex]) < 0)
            {
                smallestChildIndex = leftChildIndex;
            }

            if (rightChildIndex < _items.Count &&
                _comparer.Compare(_items[rightChildIndex], _items[smallestChildIndex]) < 0)
            {
                smallestChildIndex = rightChildIndex;
            }

            if (smallestChildIndex == index)
            {
                break; // La proprietà di heap è soddisfatta.
            }

            Swap(index, smallestChildIndex);
            index = smallestChildIndex;
        }
    }

    /// <summary>
    /// Scambia due elementi nella lista interna dell'heap.
    /// </summary>
    /// <param name="index1">Il primo indice.</param>
    /// <param name="index2">Il secondo indice.</param>
    private void Swap(int index1, int index2)
    {
        (_items[index1], _items[index2]) = (_items[index2], _items[index1]);
    }

    /// <summary>
    /// Restituisce un array contenente tutti gli elementi dell'heap nella loro disposizione interna.
    /// L'ordine non corrisponde all'ordine di priorità (ad eccezione del primo elemento).
    /// </summary>
    /// <returns>Un array di oggetti.</returns>
    public T[] ToArray()
    {
        return [.._items];
    }
    
    /// <summary>
    /// Restituisce un enumeratore che itera sugli elementi dell'heap.
    /// L'ordine di iterazione non è garantito come ordinato.
    /// </summary>
    public IEnumerator<T> GetEnumerator()
    {
        return _items.GetEnumerator();
    }
    
    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la collezione.
    /// </summary>
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
}
