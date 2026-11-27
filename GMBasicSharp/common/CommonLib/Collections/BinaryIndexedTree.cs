// FILE: Interpreter.Gpm.Collections.BinaryIndexedTree.Core.cs
// ReSharper disable CheckNamespace

/* 
 * BinaryIndexedTree.cs: albero di Fenwick per somme prefisse e di intervallo in O(log n)
 * 
 * FUNZIONI:
 * - BinaryIndexedTree(source): costruisce da array
 * - Update(index, delta): aggiorna valore (O(log n))
 * - Query(index): somma prefissa fino a index (O(log n))
 * - QueryRange(start, end): somma intervallo (O(log n))
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di un Albero di Fenwick (Binary Indexed Tree - BIT).
///
/// Teoria e Algoritmi:
/// Un Albero di Fenwick è una struttura dati che permette di calcolare in modo estremamente efficiente le somme
/// dei prefissi di un array e di aggiornare i suoi valori. La sua genialità risiede nel rappresentare un albero
/// concettuale utilizzando un semplice array, dove ogni indice `i` memorizza la somma cumulativa di un
/// intervallo specifico di elementi dell'array originale. La dimensione di questo intervallo è determinata
/// dall'ultimo bit impostato a 1 nella rappresentazione binaria dell'indice.
/// Gli algoritmi chiave sono:
/// - Aggiornamento (`Update`): Quando un valore all'indice `i` dell'array originale viene modificato, l'aggiornamento
///   viene propagato verso l'alto nell'albero del BIT. Questo si ottiene aggiungendo il delta a tutti gli indici
///   `j` nel BIT tali che `j` è un "antenato" di `i`. Gli antenati si trovano navigando tramite l'operazione
///   `j += j & -j` (aggiungendo l'ultimo bit impostato).
/// - Query (`Query`): Per calcolare la somma cumulativa fino all'indice `i`, si sommano i valori nel BIT
///   partendo dall'indice `i` e navigando verso "il basso e a sinistra" fino alla radice. I nodi da sommare si
///   trovano tramite l'operazione `i -= i & -i` (rimuovendo l'ultimo bit impostato).
///
/// Performance:
/// La struttura dell'Albero di Fenwick garantisce che sia le operazioni di aggiornamento che quelle di query
/// abbiano una complessità temporale di O(log n), dove 'n' è la dimensione dell'array. Questo lo rende
/// significativamente più veloce di un approccio ingenuo (dove l'aggiornamento è O(1) ma la query è O(n), o
/// viceversa con un array di somme prefisse). La complessità spaziale è O(n) per memorizzare l'albero.
/// </summary>
/// <typeparam name="T">Il tipo di elementi (numerici) nell'albero.</typeparam>
public class BinaryIndexedTree<T> where T : notnull
{
    /// <summary>
    /// L'array interno che rappresenta l'Albero di Fenwick. La sua dimensione è N+1
    /// poiché gli indici del BIT sono 1-based per semplificare l'aritmetica.
    /// </summary>
    private readonly T[] _tree;
    
    /// <summary>
    /// La dimensione logica dell'array originale (N), su cui opera l'albero.
    /// </summary>
    private readonly int _size;

    /// <summary>
    /// Ottiene il numero di elementi che la struttura può contenere.
    /// </summary>
    public int Count => _size;
    
    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="BinaryIndexedTree{T}"/> basata su un array di dati esistente.
    /// L'albero viene costruito in tempo O(n log n).
    /// </summary>
    /// <param name="source">L'array di dati iniziale da cui costruire l'albero.</param>
    public BinaryIndexedTree(T[] source)
    {
        _size = source.Length;
        _tree = new T[_size + 1]; // Gli indici del BIT sono 1-based
        for (var i = 0; i < _size; i++)
        {
            Update(i, source[i]);
        }
    }
    
    /// <summary>
    /// Aggiorna il valore di un elemento all'indice specificato e propaga la modifica nell'albero.
    /// L'operazione ha una complessità temporale di O(log n).
    /// </summary>
    /// <param name="index">L'indice (0-based) dell'elemento da aggiornare.</param>
    /// <param name="delta">La differenza (nuovo valore - vecchio valore) da aggiungere.</param>
    public void Update(int index, T delta)
    {
        index++;
        while (index <= _size)
        {
            var current = _tree[index];
            _tree[index] = current is null ? delta : Add(current, delta);
            index += index & -index;
        }
    }

    /// <summary>
    /// Calcola la somma cumulativa di tutti gli elementi dall'inizio dell'array fino a un indice specificato (inclusivo).
    /// L'operazione ha una complessità temporale di O(log n).
    /// </summary>
    /// <param name="index">L'indice (0-based) fino a cui calcolare la somma.</param>
    /// <returns>La somma cumulativa degli elementi fino all'indice specificato.</returns>
    public T Query(int index)
    {
        /* Indice fuori dalla dimensione (albero VUOTO o oltre _size): somma 0/default come
         * il runtime C (gmb_coll_bitree_query) e VM/C, invece di IndexOutOfRangeException. */
        if (index < 0 || index >= _size) return default!;
        // Converte l'indice in 1-based
        index++;
        T sum = default!;
        while (index > 0)
        {
            sum = sum is null ? _tree[index] : Add(sum, _tree[index]);
            // Spostati all'indice del prossimo "figlio" concettuale da sommare
            index -= index & -index;
        }
        return sum;
    }
    
    /// <summary>
    /// Calcola la somma degli elementi in un intervallo specifico [start, end] (inclusivo).
    /// L'operazione ha una complessità temporale di O(log n).
    /// </summary>
    /// <param name="start">L'indice di inizio dell'intervallo (0-based).</param>
    /// <param name="end">L'indice di fine dell'intervallo (0-based).</param>
    /// <returns>La somma degli elementi nell'intervallo specificato.</returns>
    public T QueryRange(int start, int end)
    {
        if (start > end)
        {
            return default!;
        }
        return start == 0 ? Query(end) : Subtract(Query(end), Query(start - 1));
    }

    /// <summary>
    /// Esegue un'addizione generica tra due valori di tipo T, sfruttando l'operatore '+' dinamico.
    /// Questo metodo è il cuore delle operazioni di aggiornamento e query dell'albero.
    /// </summary>
    /// <param name="a">Il primo operando.</param>
    /// <param name="b">Il secondo operando.</param>
    /// <returns>La somma di a e b.</returns>
    private static T Add(T a, T b)
    {
        // L'uso di 'dynamic' permette di gestire i tipi numerici senza vincoli complessi.
        // È efficiente perché il compilatore C# crea e cachea i delegati per le operazioni.
        dynamic da = a;
        dynamic db = b;
        return da + db;
    }

    /// <summary>
    /// Esegue una sottrazione generica tra due valori di tipo T, sfruttando l'operatore '-' dinamico.
    /// Utilizzato per calcolare le somme di intervalli.
    /// </summary>
    /// <param name="a">Il minuendo.</param>
    /// <param name="b">Il sottraendo.</param>
    /// <returns>La differenza tra a e b.</returns>
    private static T Subtract(T a, T b)
    {
        dynamic da = a;
        dynamic db = b;
        return da - db;
    }
}
