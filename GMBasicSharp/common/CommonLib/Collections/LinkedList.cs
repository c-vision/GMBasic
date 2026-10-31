// FILE: Interpreter.Gpm.Collections.LinkedList.Core.cs
using System.Collections;
// ReSharper disable CheckNamespace
// ReSharper disable MemberCanBePrivate.Global

/* 
 * LinkedList.cs: Singly Linked List generica con puntatori head/tail, AddFirst/AddLast O(1), Remove O(n)
 * 
 * FUNZIONI:
 * - AddFirst(value): aggiunge nodo in testa (O(1))
 * - AddLast(value): aggiunge nodo in coda (O(1))
 * - Remove(value): rimuove prima occorrenza (O(n))
 * - RemoveFirst(): rimuove primo nodo (O(1))
 * - RemoveLast(): rimuove ultimo nodo (O(n))
 * - Find(value): trova primo nodo con il valore (O(n))
 * - Clear(): rimuove tutti i nodi (O(1))
 * - ToArray(): converte in array (O(n))
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una Lista Singolarmente Concatenata (Singly Linked List) generica.
///
/// Teoria e Algoritmi:
/// Una lista singolarmente concatenata è una struttura dati lineare composta da una sequenza di nodi. Ogni nodo
/// contiene un valore e un singolo puntatore al nodo successivo (`Next`). A differenza di una lista doppiamente
/// concatenata, l'attraversamento è possibile solo in una direzione (in avanti). Questa implementazione
/// mantiene puntatori al primo nodo (`_head`) e all'ultimo (`_tail`) per consentire inserimenti veloci
/// sia all'inizio che alla fine della lista. L'inserimento di un nodo in testa (`AddFirst`) o in coda (`AddLast`)
/// sono operazioni a tempo costante O(1). La rimozione di un nodo richiede un attraversamento per trovare il
/// nodo precedente, rendendola un'operazione O(n).
///
/// Performance:
/// Il vantaggio principale di una lista singolarmente concatenata è il suo basso overhead di memoria (un solo puntatore
/// per nodo) e le sue performance per le operazioni di inserimento in testa e in coda, che sono a tempo costante O(1).
/// Questo è più efficiente di una `List{T}` basata su array per gli inserimenti all'inizio. Tuttavia, l'accesso
/// a un elemento tramite indice, la ricerca di un elemento per valore e la rimozione di un elemento (escluso il primo)
/// non sono efficienti, in quanto richiedono un attraversamento lineare della lista, risultando in una complessità
/// temporale di O(n).
/// </summary>
/// <typeparam name="T">Il tipo di elementi nella lista.</typeparam>
public class SingleLinkedList<T> : IEnumerable<T>
{
    /// <summary>
    /// Rappresenta un singolo nodo in una <see cref="SingleLinkedList{T}"/>.
    /// Contiene il valore e un puntatore al nodo successivo.
    /// </summary>
    public sealed class Node
    {
        /// <summary>
        /// Ottiene o imposta il valore contenuto nel nodo.
        /// </summary>
        public T Value { get; set; }

        /// <summary>
        /// Ottiene il nodo successivo nella lista.
        /// </summary>
        public Node? Next { get; internal set; }

        /// <summary>
        /// Inizializza una nuova istanza della classe Node.
        /// </summary>
        /// <param name="value">Il valore da memorizzare nel nodo.</param>
        internal Node(T value)
        {
            Value = value;
        }
    }

    /// <summary>
    /// Il primo nodo della lista. Se null, la lista è vuota.
    /// </summary>
    private Node? _head;

    /// <summary>
    /// L'ultimo nodo della lista. Ottimizzazione per rendere `AddLast` un'operazione O(1).
    /// </summary>
    private Node? _tail;

    /// <summary>
    /// Ottiene il numero di nodi effettivamente contenuti nella lista.
    /// </summary>
    public int Count { get; private set; }

    /// <summary>
    /// Ottiene il primo nodo della lista.
    /// </summary>
    public Node? First => _head;

    /// <summary>
    /// Ottiene l'ultimo nodo della lista.
    /// </summary>
    public Node? Last => _tail;

    /// <summary>
    /// Aggiunge un nuovo nodo contenente il valore specificato all'inizio della lista.
    /// Questa operazione ha una complessità di O(1).
    /// </summary>
    /// <param name="value">Il valore da aggiungere all'inizio della lista.</param>
    /// <returns>Il nuovo nodo <see cref="Node"/> aggiunto.</returns>
    public Node AddFirst(T value)
    {
        var newNode = new Node(value) { Next = _head };
        _head = newNode;
        if (_tail == null)
        {
            _tail = _head;
        }

        Count++;
        return newNode;
    }

    /// <summary>
    /// Aggiunge un nuovo nodo contenente il valore specificato alla fine della lista.
    /// Questa operazione ha una complessità di O(1) grazie al puntatore `_tail`.
    /// </summary>
    /// <param name="value">Il valore da aggiungere alla fine della lista.</param>
    /// <returns>Il nuovo nodo <see cref="Node"/> aggiunto.</returns>
    public Node AddLast(T value)
    {
        var newNode = new Node(value);
        if (_head == null)
        {
            _head = newNode;
            _tail = newNode;
        }
        else
        {
            _tail!.Next = newNode;
            _tail = newNode;
        }

        Count++;
        return newNode;
    }

    /// <summary>
    /// Rimuove la prima occorrenza del valore specificato dalla lista.
    /// La ricerca del valore richiede un'operazione O(n).
    /// </summary>
    /// <param name="value">Il valore da rimuovere dalla lista.</param>
    /// <returns><c>true</c> se un elemento è stato trovato e rimosso; altrimenti, <c>false</c>.</returns>
    public bool Remove(T value)
    {
        if (_head == null) return false;

        // Caso speciale: il nodo da rimuovere è la testa.
        if (EqualityComparer<T>.Default.Equals(_head.Value, value))
        {
            _head = _head.Next;
            if (_head == null)
            {
                _tail = null; // La lista è diventata vuota.
            }

            Count--;
            return true;
        }

        var current = _head;
        while (current.Next != null)
        {
            if (EqualityComparer<T>.Default.Equals(current.Next.Value, value))
            {
                // Se stiamo per rimuovere la coda, aggiorniamo il puntatore `_tail`.
                if (current.Next == _tail)
                {
                    _tail = current;
                }

                current.Next = current.Next.Next;
                Count--;
                return true;
            }

            current = current.Next;
        }

        return false;
    }

    /// <summary>
    /// Rimuove il primo nodo dalla lista.
    /// Questa operazione ha una complessità di O(1).
    /// </summary>
    public void RemoveFirst()
    {
        if (_head == null)
        {
            throw new InvalidOperationException("La lista è vuota.");
        }
        _head = _head.Next;
        if (_head == null)
        {
            _tail = null;
        }
        Count--;
    }

    /// <summary>
    /// Rimuove l'ultimo nodo dalla lista.
    /// Questa operazione ha una complessità di O(n) perché deve trovare il penultimo elemento.
    /// </summary>
    public void RemoveLast()
    {
        if (_head == null)
        {
            throw new InvalidOperationException("La lista è vuota.");
        }

        if (_head == _tail) // C'è un solo elemento
        {
            _head = null;
            _tail = null;
        }
        else
        {
            // Trova il penultimo nodo
            var current = _head;
            while (current.Next != _tail)
            {
                current = current.Next!;
            }
            current.Next = null;
            _tail = current;
        }
        Count--;
    }
    
    /// <summary>
    /// Trova il primo nodo che contiene il valore specificato.
    /// Questa operazione ha una complessità di O(n).
    /// </summary>
    /// <param name="value">Il valore da cercare nella lista.</param>
    /// <returns>Il primo <see cref="Node"/> che contiene il valore, o <c>null</c> se non viene trovato.</returns>
    public Node? Find(T value)
    {
        var current = _head;
        while (current != null)
        {
            if (EqualityComparer<T>.Default.Equals(current.Value, value))
            {
                return current;
            }

            current = current.Next;
        }

        return null;
    }

    /// <summary>
    /// Rimuove tutti i nodi dalla lista.
    /// Questa operazione ha una complessità di O(1).
    /// </summary>
    public void Clear()
    {
        _head = null;
        _tail = null;
        Count = 0;
    }

    /// <summary>
    /// Converte la lista singolarmente concatenata in un array.
    /// L'operazione ha una complessità di O(n).
    /// </summary>
    /// <returns>Un nuovo array contenente tutti gli elementi della lista, in ordine.</returns>
    public T[] ToArray()
    {
        var array = new T[Count];
        var current = _head;
        var index = 0;
        while (current != null)
        {
            array[index++] = current.Value;
            current = current.Next;
        }
        return array;
    }
    
    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la lista dalla testa alla coda.
    /// </summary>
    /// <returns>Un enumeratore per la lista.</returns>
    public IEnumerator<T> GetEnumerator()
    {
        var current = _head;
        while (current != null)
        {
            yield return current.Value;
            current = current.Next;
        }
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la lista.
    /// </summary>
    /// <returns>Un enumeratore per la lista.</returns>
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
}
