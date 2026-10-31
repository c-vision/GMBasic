// FILE: Interpreter.Gpm.Collections.BPlusTree.Core.cs
using System.Collections;
// ReSharper disable CheckNamespace
// ReSharper disable MemberCanBePrivate.Local

/* 
 * BPlusTree.cs: B+ Tree in-memory (solo foglie con dati, foglie collegate in lista, split/merge automatici)
 * 
 * FUNZIONI:
 * - Add(key, value): aggiunge o aggiorna elemento
 * - TryGetValue(key, out value): cerca per chiave
 * - Delete(key): rimuove elemento
 * - TryGetMin/TryGetMax: chiave minima/massima
 * - Clear(): svuota albero
 * - Count: numero elementi
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati B+ Tree generica, per uso in-memory.
///
/// Teoria e Algoritmi:
/// Un B+ Tree è un albero M-ario auto-bilanciato, ottimizzato per sistemi che leggono e scrivono
/// grandi blocchi di dati, come filesystem e database, ma estremamente performante anche come
/// struttura dati in-memory. Le sue proprietà chiave sono:
/// 1. I dati (coppie chiave-valore) sono memorizzati esclusivamente nei nodi foglia.
/// 2. I nodi interni contengono solo chiavi "separatore" per guidare la ricerca.
/// 3. Tutti i nodi foglia si trovano allo stesso livello, garantendo un bilanciamento perfetto.
/// 4. I nodi foglia sono collegati tra loro in una lista doppiamente concatenata, permettendo
///    un'iterazione sequenziale (in-order) estremamente veloce senza attraversare l'albero.
/// Le operazioni di modifica (`Add`, `Delete`) mantengono l'albero bilanciato tramite due meccanismi:
/// - Divisione (Split): Quando un nodo si riempie, viene diviso in due, e una chiave mediana viene
///   "promossa" al nodo genitore.
/// - Fusione/Rotazione (Merge/Rotation): Quando un nodo scende sotto la soglia minima di riempimento
///   dopo una cancellazione, prende in prestito elementi da un fratello (rotazione) o si unisce a esso (fusione).
///
/// Performance:
/// L'altezza di un B+ Tree è estremamente bassa, O(log_M n), dove M è l'ordine dell'albero.
/// - Ricerca, Inserimento, Cancellazione: Hanno una complessità temporale garantita di O(log n).
/// - Iterazione: Grazie alla lista concatenata dei nodi foglia, l'iterazione su tutti gli elementi
///   o su un intervallo è un'operazione O(n) o O(k) (dove k è la dimensione dell'intervallo)
///   altamente performante, superando di gran lunga gli alberi binari per le scansioni ordinate.
/// </summary>
/// <typeparam name="TKey">Il tipo delle chiavi.</typeparam>
/// <typeparam name="TValue">Il tipo dei valori.</typeparam>
public class BPlusTree<TKey, TValue> : IEnumerable<KeyValuePair<TKey, TValue>>, IKeyValueIndexable where TKey : notnull
{
    /// <summary>
    /// Rappresenta la classe base astratta per tutti i nodi (interni e foglia) in un B+ Tree.
    /// Contiene le proprietà comuni come la lista delle chiavi, il puntatore al genitore e l'ordine dell'albero.
    /// </summary>
    private abstract class Node(int order)
    {
        /// <summary>
        /// La lista delle chiavi contenute in questo nodo. Nei nodi interni, sono separatori; nelle foglie, sono le chiavi dei dati.
        /// </summary>
        public List<TKey> Keys { get; } = new(order);
        
        /// <summary>
        /// Il puntatore al nodo genitore. È nullo solo per la radice.
        /// </summary>
        public InternalNode? Parent { get; set; }

        /// <summary>
        /// L'ordine (grado massimo) dell'albero, che determina la capacità di questo nodo.
        /// </summary>
        public int Order { get; } = order;
        
        /// <summary>
        /// Il numero minimo di chiavi che un nodo (diverso dalla radice) deve contenere per non essere in underflow.
        /// Tipicamente è la metà della capacità massima.
        /// </summary>
        public int MinKeys { get; } = (order - 1) / 2;

        /// <summary>
        /// Verifica se il nodo ha superato il numero massimo di chiavi consentito (Order - 1),
        /// e necessita quindi di uno split. Deve scattare SOLO dopo l'overflow (Keys.Count > Order - 1),
        /// non al raggiungimento della piena capacità: uno split prematuro (a Keys.Count == Order - 1)
        /// non lascia keys sufficienti per soddisfare MinKeys su entrambi i lati dopo la promozione
        /// della chiave mediana in un nodo interno, specialmente per Order piccoli (es. 3).
        /// </summary>
        public bool IsFull => Keys.Count > Order - 1;
        
        /// <summary>
        /// Verifica se il nodo è in stato di underflow, ovvero ha meno del numero minimo di chiavi consentite.
        /// </summary>
        public bool IsUnderflow => Keys.Count < MinKeys;
        
        /// <summary>
        /// Trova l'indice del figlio appropriato in cui continuare la ricerca per una data chiave.
        /// L'implementazione varia tra nodi interni e nodi foglia.
        /// </summary>
        /// <param name="key">La chiave da cercare.</param>
        /// <param name="comparer">Il comparatore da utilizzare per la ricerca binaria.</param>
        /// <returns>L'indice del figlio da seguire.</returns>
        public abstract int GetChildIndex(TKey key, IComparer<TKey> comparer);
    }

    /// <summary>
    /// Rappresenta un nodo interno in un B+ Tree. I nodi interni contengono solo chiavi che agiscono
    /// da "separatori" per guidare la ricerca verso i nodi figli corretti. Non contengono valori di dati.
    /// </summary>
    private sealed class InternalNode(int order) : Node(order)
    {
        /// <summary>
        /// La lista dei nodi figli di questo nodo interno. Un nodo interno ha sempre una quantità di figli
        /// pari al numero di chiavi più uno (Keys.Count + 1).
        /// </summary>
        public List<Node> Children { get; } = new(order);

        /// <summary>
        /// Trova l'indice del figlio da seguire usando una ricerca binaria sulle chiavi.
        /// Se la chiave viene trovata (corrispondenza esatta), la ricerca deve proseguire nel sottoalbero a destra di quella chiave.
        /// Se la chiave non viene trovata, il risultato del complemento bitwise di BinarySearch indica direttamente
        /// l'indice del primo elemento più grande, che corrisponde all'indice del sottoalbero corretto.
        /// </summary>
        /// <param name="key">La chiave da cercare.</param>
        /// <param name="comparer">Il comparatore da utilizzare per la ricerca.</param>
        /// <returns>L'indice (0-based) nella lista Children del nodo figlio da attraversare.</returns>
        public override int GetChildIndex(TKey key, IComparer<TKey> comparer)
        {
            var index = Keys.BinarySearch(key, comparer);
            return index >= 0 ? index + 1 : ~index;
        }
    }

    /// <summary>
    /// Rappresenta un nodo foglia in un B+ Tree. A differenza dei nodi interni, i nodi foglia
    /// contengono sia le chiavi che i valori di dati associati. Inoltre, includono puntatori
    /// Next e Previous ai nodi foglia adiacenti, formando una lista doppiamente concatenata
    /// alla base dell'albero per consentire un'iterazione sequenziale efficiente.
    /// </summary>
    private sealed class LeafNode(int order) : Node(order)
    {
        /// <summary>
        /// La lista dei valori associati alle chiavi in questo nodo foglia. L'indice di un valore in questa lista
        /// corrisponde all'indice della sua chiave nella lista Keys ereditata.
        /// </summary>
        public List<TValue> Values { get; } = new(order);
        
        /// <summary>
        /// Puntatore al nodo foglia successivo nella lista doppiamente concatenata. È null per l'ultimo nodo foglia dell'albero.
        /// </summary>
        public LeafNode? Next { get; set; }
        
        /// <summary>
        /// Puntatore al nodo foglia precedente nella lista doppiamente concatenata. È null per il primo nodo foglia dell'albero.
        /// </summary>
        public LeafNode? Previous { get; set; }

        /// <summary>
        /// Per un nodo foglia, GetChildIndex non viene utilizzato per trovare un figlio (poiché non ne ha),
        /// ma sfrutta il risultato del complemento bitwise di BinarySearch per restituire l'indice esatto
        /// in cui una nuova chiave dovrebbe essere inserita per mantenere l'ordine.
        /// </summary>
        /// <param name="key">La chiave da cercare.</param>
        /// <param name="comparer">Il comparatore da utilizzare per la ricerca.</param>
        /// <returns>Il complemento bitwise del risultato di BinarySearch, che corrisponde all'indice di inserimento.</returns>
        public override int GetChildIndex(TKey key, IComparer<TKey> comparer)
        {
            var index = Keys.BinarySearch(key, comparer);
            return ~index;
        }
    }

    /// <summary>
    /// Il comparatore utilizzato per ordinare e cercare le chiavi in tutti i nodi dell'albero.
    /// L'iniezione di un comparatore nel costruttore rimuove la necessità di un vincolo generico
    /// IComparable sul tipo della chiave, rendendo la struttura più flessibile.
    /// </summary>
    private readonly IComparer<TKey> _comparer;

    /// <summary>
    /// L'ordine (o grado) del B+ Tree, che definisce il numero massimo di figli che un nodo può avere.
    /// Questo valore, specificato alla creazione, determina la capacità di ogni nodo.
    /// </summary>
    private readonly int _order;

    /// <summary>
    /// Il campo privato che funge da radice del B+ Tree. È il punto di ingresso per tutte le operazioni.
    /// Può essere un InternalNode o, se l'albero è piccolo, un LeafNode. Se null, l'albero è vuoto.
    /// </summary>
    private Node? _root;

    /// <summary>
    /// Un riferimento diretto al primo nodo foglia (il più a sinistra) dell'albero.
    /// Questa è un'ottimizzazione che permette di iniziare un'iterazione sequenziale completa in tempo O(1)
    /// (dopo la prima ricerca), senza dover navigare l'albero dalla radice ogni volta.
    /// </summary>
    private LeafNode? _firstLeaf;

    /// <summary>
    /// Ottiene il numero di coppie chiave-valore contenute nell'albero.
    /// </summary>
    public int Count { get; private set; }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="BPlusTree{TKey, TValue}"/>.
    /// </summary>
    /// <param name="order">L'ordine dell'albero. Deve essere almeno 3.</param>
    /// <param name="comparer">Il comparatore da utilizzare.</param>
    public BPlusTree(int order, IComparer<TKey>? comparer = null)
    {
        if (order < 3) throw new ArgumentException("L'ordine di un B+ Tree deve essere almeno 3.", nameof(order));
        _order = order;
        _comparer = comparer ?? Comparer<TKey>.Default;
    }
    
    /// <summary>
    /// Trova la coppia chiave-valore con la chiave minima nell'albero.
    /// L'operazione ha una complessità di O(log n) per trovare la prima foglia.
    /// </summary>
    /// <param name="key">Quando il metodo ritorna, contiene la chiave minima se l'albero non è vuoto.</param>
    /// <param name="value">Quando il metodo ritorna, contiene il valore minimo se l'albero non è vuoto.</param>
    /// <returns><c>true</c> se l'albero non è vuoto; altrimenti, <c>false</c>.</returns>
    public bool TryGetMin(out TKey? key, out TValue? value)
    {
        key = default;
        value = default;
        if (_firstLeaf == null || _firstLeaf.Keys.Count == 0)
        {
            return false;
        }

        key = _firstLeaf.Keys[0];
        value = _firstLeaf.Values[0];
        return true;
    }

    /// <summary>
    /// Trova la coppia chiave-valore con la chiave massima nell'albero.
    /// L'operazione ha una complessità di O(log n) per trovare l'ultima foglia.
    /// </summary>
    /// <param name="key">Quando il metodo ritorna, contiene la chiave massima se l'albero non è vuoto.</param>
    /// <param name="value">Quando il metodo ritorna, contiene il valore massimo se l'albero non è vuoto.</param>
    /// <returns><c>true</c> se l'albero non è vuoto; altrimenti, <c>false</c>.</returns>
    public bool TryGetMax(out TKey? key, out TValue? value)
    {
        key = default;
        value = default;
        if (_root == null) return false;

        var leaf = _root;
        while (leaf is InternalNode internalNode)
        {
            leaf = internalNode.Children[^1];
        }
        var lastLeaf = (LeafNode)leaf;

        if (lastLeaf.Keys.Count == 0)
        {
            return false;
        }

        key = lastLeaf.Keys[^1];
        value = lastLeaf.Values[^1];
        return true;
    }
    
    /// <summary>
    /// Aggiunge o aggiorna una coppia chiave-valore.
    /// </summary>
    public void Add(TKey key, TValue value)
    {
        if (_root == null)
        {
            _root = new LeafNode(_order);
            _firstLeaf = (LeafNode)_root;
        }

        var leaf = FindLeafNode(key);
        var index = leaf.Keys.BinarySearch(key, _comparer);
        if (index >= 0)
        {
            leaf.Values[index] = value;
            return;
        }

        index = ~index;
        leaf.Keys.Insert(index, key);
        leaf.Values.Insert(index, value);
        Count++;

        if (leaf.IsFull)
        {
            Split(leaf);
        }
    }

    /// <summary>
    /// Trova e restituisce il valore associato a una chiave.
    /// </summary>
    public bool TryGetValue(TKey key, out TValue? value)
    {
        value = default;
        if (_root == null) return false;

        var leaf = FindLeafNode(key);
        var index = leaf.Keys.BinarySearch(key, _comparer);
        if (index < 0) return false;
        
        value = leaf.Values[index];
        return true;
    }

    /// <summary>
    /// Rimuove un elemento con la chiave specificata dall'albero.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da rimuovere.</param>
    /// <returns><c>true</c> se l'elemento è stato trovato e rimosso; altrimenti, <c>false</c>.</returns>
    public bool Delete(TKey key)
    {
        if (_root == null) return false;
        var leaf = FindLeafNode(key);
        var index = leaf.Keys.BinarySearch(key, _comparer);
        if (index < 0) return false;

        leaf.Keys.RemoveAt(index);
        leaf.Values.RemoveAt(index);
        Count--;

        if (leaf.IsUnderflow && leaf != _root)
        {
            HandleUnderflow(leaf);
        }
        
        // Se la radice diventa un nodo interno senza chiavi, la nuova radice è il suo unico figlio.
        if (_root is InternalNode internalRoot && internalRoot.Keys.Count == 0)
        {
            _root = internalRoot.Children[0];
            _root.Parent = null;
        }

        return true;
    }
    
    /// <summary>
    /// Rimuove tutti gli elementi dall'albero.
    /// </summary>
    public void Clear()
    {
        _root = null;
        _firstLeaf = null;
        Count = 0;
    }

    /// <summary>
    /// Naviga l'albero dalla radice per trovare il nodo foglia corretto in cui una data chiave
    /// dovrebbe risiedere o essere inserita. L'operazione attraversa i nodi interni, utilizzando
    /// GetChildIndex ad ogni passo per decidere quale puntatore figlio seguire, fino a raggiungere
    /// un nodo di tipo LeafNode.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <returns>Il nodo foglia (LeafNode) responsabile per la chiave data.</returns>
    private LeafNode FindLeafNode(TKey key)
    {
        var node = _root;
        while (node is InternalNode internalNode)
        {
            node = internalNode.Children[internalNode.GetChildIndex(key, _comparer)];
        }
        return (LeafNode)node!;
    }
    
    /// <summary>
    /// Gestisce la logica di divisione di un nodo (sia interno che foglia) che ha superato la sua capienza massima.
    /// Il nodo viene diviso a metà, viene creato un nuovo nodo "fratello" a destra, e la chiave mediana
    /// viene "promossa" al nodo genitore per agire da separatore. L'operazione è ricorsiva: se la promozione
    /// della chiave causa un overflow anche nel genitore, il processo di divisione si ripete su di esso,
    /// propagandosi potenzialmente fino alla radice.
    /// </summary>
    /// <param name="node">Il nodo che è pieno e deve essere diviso.</param>
    private void Split(Node node)
    {
        var midIndex = node.Keys.Count / 2;
        var medianKey = node.Keys[midIndex];

        if (node is LeafNode leaf)
        {
            var newLeaf = new LeafNode(_order) { Parent = node.Parent };
            var rangeCount = leaf.Keys.Count - midIndex;
            newLeaf.Keys.AddRange(leaf.Keys.GetRange(midIndex, rangeCount));
            newLeaf.Values.AddRange(leaf.Values.GetRange(midIndex, rangeCount));
            leaf.Keys.RemoveRange(midIndex, rangeCount);
            leaf.Values.RemoveRange(midIndex, rangeCount);

            newLeaf.Next = leaf.Next;
            if (newLeaf.Next != null) newLeaf.Next.Previous = newLeaf;
            leaf.Next = newLeaf;
            newLeaf.Previous = leaf;

            PromoteKey(medianKey, node, newLeaf);
        }
        else if (node is InternalNode internalNode)
        {
            var newInternal = new InternalNode(_order) { Parent = node.Parent };
            var rangeStart = midIndex + 1;
            var keyRangeCount = internalNode.Keys.Count - rangeStart;
            newInternal.Keys.AddRange(internalNode.Keys.GetRange(rangeStart, keyRangeCount));
            internalNode.Keys.RemoveRange(midIndex, keyRangeCount + 1);

            var childRangeCount = keyRangeCount + 1;
            newInternal.Children.AddRange(internalNode.Children.GetRange(rangeStart, childRangeCount));
            internalNode.Children.RemoveRange(rangeStart, childRangeCount);

            foreach (var child in newInternal.Children) child.Parent = newInternal;
            
            PromoteKey(medianKey, node, newInternal);
        }
    }

    /// <summary>
    /// Inserisce una chiave mediana nel nodo genitore durante un'operazione di Split.
    /// Questo metodo gestisce la creazione di una nuova radice se il nodo diviso era la radice originale
    /// (aumentando così l'altezza dell'albero), oppure inserisce la chiave e il puntatore al nuovo nodo
    /// destro nel genitore esistente.
    /// </summary>
    /// <param name="key">La chiave mediana da promuovere al genitore.</param>
    /// <param name="left">Il nodo originale (ora il figlio sinistro della chiave promossa).</param>
    /// <param name="right">Il nuovo nodo creato dalla divisione (ora il figlio destro della chiave promossa).</param>
    private void PromoteKey(TKey key, Node left, Node right)
    {
        var parent = left.Parent;
        if (parent == null)
        {
            _root = new InternalNode(_order)
            {
                Keys = { key },
                Children = { left, right }
            };
            left.Parent = (InternalNode)_root;
            right.Parent = (InternalNode)_root;
            return;
        }
        
        var index = parent.GetChildIndex(key, _comparer);
        parent.Keys.Insert(index, key);
        parent.Children.Insert(index + 1, right);
        right.Parent = parent;

        if (parent.IsFull)
        {
            Split(parent);
        }
    }
    
    /// <summary>
    /// Gestisce un nodo che è in stato di "underflow" (ha meno del numero minimo di chiavi consentite)
    /// dopo un'operazione di cancellazione. L'algoritmo tenta prima di ribilanciare prendendo in prestito
    /// una chiave da un nodo fratello adiacente (ridistribuzione/rotazione). Se entrambi i fratelli sono
    /// al minimo della loro capacità, esegue una fusione (merge) del nodo con uno dei suoi fratelli.
    /// </summary>
    /// <param name="node">Il nodo in stato di underflow che necessita di essere ribilanciato.</param>
    private void HandleUnderflow(Node node)
    {
        var parent = node.Parent!;
        var indexInParent = parent.Children.IndexOf(node);

        // Prova a prendere in prestito dal fratello sinistro
        if (indexInParent > 0)
        {
            var leftSibling = parent.Children[indexInParent - 1];
            if (leftSibling.Keys.Count > leftSibling.MinKeys)
            {
                // Rotazione a destra
                if (node is LeafNode leaf)
                {
                    var leftLeaf = (LeafNode)leftSibling;
                    var borrowedKey = leftLeaf.Keys[^1];
                    var borrowedValue = leftLeaf.Values[^1];
                    leftLeaf.Keys.RemoveAt(leftLeaf.Keys.Count - 1);
                    leftLeaf.Values.RemoveAt(leftLeaf.Values.Count - 1);
                    leaf.Keys.Insert(0, borrowedKey);
                    leaf.Values.Insert(0, borrowedValue);
                    parent.Keys[indexInParent - 1] = borrowedKey;
                }
                else if (node is InternalNode internalNode)
                {
                    var leftInternal = (InternalNode)leftSibling;
                    internalNode.Keys.Insert(0, parent.Keys[indexInParent - 1]);
                    parent.Keys[indexInParent - 1] = leftInternal.Keys[^1];
                    leftInternal.Keys.RemoveAt(leftInternal.Keys.Count - 1);
                    internalNode.Children.Insert(0, leftInternal.Children[^1]);
                    leftInternal.Children[^1].Parent = internalNode;
                    leftInternal.Children.RemoveAt(leftInternal.Children.Count - 1);
                }
                return;
            }
        }
        
        // Prova a prendere in prestito dal fratello destro
        if (indexInParent < parent.Children.Count - 1)
        {
            var rightSibling = parent.Children[indexInParent + 1];
            if (rightSibling.Keys.Count > rightSibling.MinKeys)
            {
                // Rotazione a sinistra
                if (node is LeafNode leaf)
                {
                    var rightLeaf = (LeafNode)rightSibling;
                    var borrowedKey = rightLeaf.Keys[0];
                    var borrowedValue = rightLeaf.Values[0];
                    rightLeaf.Keys.RemoveAt(0);
                    rightLeaf.Values.RemoveAt(0);
                    leaf.Keys.Add(borrowedKey);
                    leaf.Values.Add(borrowedValue);
                    parent.Keys[indexInParent] = rightLeaf.Keys[0];
                }
                else if (node is InternalNode internalNode)
                {
                    var rightInternal = (InternalNode)rightSibling;
                    internalNode.Keys.Add(parent.Keys[indexInParent]);
                    parent.Keys[indexInParent] = rightInternal.Keys[0];
                    rightInternal.Keys.RemoveAt(0);
                    internalNode.Children.Add(rightInternal.Children[0]);
                    rightInternal.Children[0].Parent = internalNode;
                    rightInternal.Children.RemoveAt(0);
                }
                return;
            }
        }
        
        // Altrimenti, esegui la fusione
        if (indexInParent > 0)
        {
            Merge(parent.Children[indexInParent - 1], node, indexInParent - 1);
        }
        else
        {
            Merge(node, parent.Children[indexInParent + 1], indexInParent);
        }
    }
    
    /// <summary>
    /// Unisce due nodi fratelli adiacenti (sinistro e destro) quando entrambi sono al minimo delle loro chiavi.
    /// L'operazione sposta tutto il contenuto del nodo destro nel nodo sinistro, tira giù la chiave separatrice
    /// dal nodo genitore e rimuove il puntatore al nodo destro dal genitore. Questa operazione può causare
    /// un underflow a catena nel genitore, che viene gestito ricorsivamente da HandleUnderflow.
    /// </summary>
    /// <param name="left">Il nodo fratello di sinistra in cui verranno uniti i dati.</param>
    /// <param name="right">Il nodo fratello di destra che verrà unito e poi rimosso.</param>
    /// <param name="separatorIndexInParent">L'indice della chiave, nel genitore, che separa i due nodi.</param>
    private void Merge(Node left, Node right, int separatorIndexInParent)
    {
        var parent = left.Parent!;

        if (left is LeafNode leftLeaf && right is LeafNode rightLeaf)
        {
            leftLeaf.Keys.AddRange(rightLeaf.Keys);
            leftLeaf.Values.AddRange(rightLeaf.Values);
            leftLeaf.Next = rightLeaf.Next;
            if (rightLeaf.Next != null) rightLeaf.Next.Previous = leftLeaf;
        }
        else if (left is InternalNode leftInternal && right is InternalNode rightInternal)
        {
            leftInternal.Keys.Add(parent.Keys[separatorIndexInParent]);
            leftInternal.Keys.AddRange(rightInternal.Keys);
            leftInternal.Children.AddRange(rightInternal.Children);
            foreach (var child in rightInternal.Children) child.Parent = leftInternal;
        }

        parent.Keys.RemoveAt(separatorIndexInParent);
        parent.Children.RemoveAt(separatorIndexInParent + 1);

        if (parent.IsUnderflow && parent != _root)
        {
            HandleUnderflow(parent);
        }
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso tutte le coppie chiave-valore nell'albero in ordine crescente di chiave.
    /// L'iterazione è altamente efficiente in quanto sfrutta la lista doppiamente concatenata dei nodi foglia,
    /// partendo dal primo nodo (_firstLeaf) e seguendo i puntatori Next fino alla fine.
    /// </summary>
    /// <returns>Un enumeratore per la collezione.</returns>
    public IEnumerator<KeyValuePair<TKey, TValue>> GetEnumerator()
    {
        if (_firstLeaf == null) yield break;
        var leaf = _firstLeaf;
        while (leaf != null)
        {
            for (var i = 0; i < leaf.Keys.Count; i++)
            {
                yield return new KeyValuePair<TKey, TValue>(leaf.Keys[i], leaf.Values[i]);
            }
            leaf = leaf.Next;
        }
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la collezione. Questo è un'implementazione esplicita dell'interfaccia non generica.
    /// </summary>
    /// <returns>Un enumeratore per la collezione.</returns>
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    // ── IKeyValueIndexable (dispatch non riflessivo per la PCodeVM) ──
    bool IKeyValueIndexable.TryGetValue(object? key, out object? value)
    {
        if (key is TKey k && TryGetValue(k, out var v))
        {
            value = v;
            return true;
        }
        value = null;
        return false;
    }

    void IKeyValueIndexable.Set(object? key, object? value)
    {
        if (key is TKey k)
        {
            if (value is TValue v) Add(k, v);
            else if (value is null && default(TValue) is null) Add(k, default!);
        }
    }

    bool IKeyValueIndexable.TryGetMin(out object? key, out object? value)
    {
        if (TryGetMin(out var k, out var v))
        {
            key = k;
            value = v;
            return true;
        }
        key = null;
        value = null;
        return false;
    }

    bool IKeyValueIndexable.TryGetMax(out object? key, out object? value)
    {
        if (TryGetMax(out var k, out var v))
        {
            key = k;
            value = v;
            return true;
        }
        key = null;
        value = null;
        return false;
    }

    IEnumerable<KeyValuePair<object?, object?>> IKeyValueIndexable.Entries()
    {
        foreach (var kv in this)
            yield return new KeyValuePair<object?, object?>(kv.Key, kv.Value);
    }
}
