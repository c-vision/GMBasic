// FILE: Interpreter.Gpm.Collections.CircularQueue.Core.cs
using System.Collections;
// ReSharper disable CheckNamespace

/* 
 * CircularQueue.cs: coda circolare (ring buffer) a dimensione fissa, sovrascrive i più vecchi se piena
 * 
 * FUNZIONI:
 * - Enqueue(item): aggiunge elemento (O(1))
 * - Dequeue(): rimuove e restituisce (O(1))
 * - Peek(): ispeziona senza rimuovere (O(1))
 * - Clear(): svuota coda
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una Coda Circolare (Ring Buffer) generica a dimensione fissa.
///
/// Teoria e Algoritmi:
/// Una coda circolare è una struttura dati lineare che utilizza un buffer a dimensione fissa come se fosse
/// collegato in un anello. Viene gestita tramite due puntatori (indici): uno per la testa (`_head`), che indica
/// il prossimo elemento da leggere, e uno per la coda (`_tail`), che indica la prossima posizione libera in cui scrivere.
/// Quando un puntatore raggiunge la fine dell'array, "si riavvolge" (`wrap-around`) tornando a 0, creando
/// l'effetto circolare. Questa implementazione gestisce l'overflow sovrascrivendo l'elemento più vecchio
/// quando la capacità viene superata, spostando in avanti il puntatore della testa. Questo comportamento è
/// ideale per lo streaming di dati o per mantenere una cronologia degli "ultimi N eventi".
///
/// Performance:
/// Le operazioni chiave, `Enqueue` (aggiunta) e `Dequeue` (rimozione), sono estremamente efficienti e hanno
/// una complessità temporale a tempo costante O(1). Questo perché implicano solo calcoli aritmetici sugli
/// indici e accessi diretti all'array sottostante, senza la necessità di spostare elementi. La complessità
/// spaziale è O(N), dove N è la capacità fissa del buffer definita alla creazione.
/// </summary>
/// <typeparam name="T">Il tipo degli elementi nella coda.</typeparam>
public class CircularQueue<T> : IEnumerable<T>
{
    /// <summary>
    /// L'array a dimensione fissa che funge da buffer circolare per memorizzare gli elementi.
    /// </summary>
    private readonly T[] _buffer;
    
    /// <summary>
    /// L'indice che punta al primo elemento della coda (la "testa"), pronto per essere rimosso (Dequeue).
    /// </summary>
    private int _head;
    
    /// <summary>
    /// L'indice che punta alla prima posizione libera dopo l'ultimo elemento della coda (la "coda"),
    /// dove verrà inserito il prossimo elemento (Enqueue).
    /// </summary>
    private int _tail;
    
    /// <summary>
    /// Il numero di elementi attualmente presenti nella coda.
    /// </summary>
    private int _count;

    /// <summary>
    /// Ottiene il numero di elementi attualmente presenti nella coda.
    /// </summary>
    public int Count => _count;

    /// <summary>
    /// Ottiene la capacità massima della coda.
    /// </summary>
    public int Capacity => _buffer.Length;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="CircularQueue{T}"/> con una capacità specificata.
    /// </summary>
    /// <param name="capacity">La dimensione massima fissa della coda. Deve essere maggiore di 0.</param>
    /// <exception cref="ArgumentException">Lanciata se la capacità è minore o uguale a 0.</exception>
    public CircularQueue(int capacity)
    {
        if (capacity <= 0)
        {
            throw new ArgumentException("La capacità deve essere maggiore di 0.", nameof(capacity));
        }
        _buffer = new T[capacity];
        Clear();
    }
    
    /// <summary>
    /// Aggiunge un elemento alla coda della coda circolare.
    /// Se la coda è piena, l'elemento più vecchio viene sovrascritto.
    /// </summary>
    /// <param name="item">L'elemento da aggiungere.</param>
    public void Enqueue(T item)
    {
        _buffer[_tail] = item;
        _tail = (_tail + 1) % Capacity;

        if (_count == Capacity)
        {
            // Se la coda è piena, la testa avanza insieme alla coda (sovrascrittura).
            _head = (_head + 1) % Capacity;
        }
        else
        {
            _count++;
        }
    }

    /// <summary>
    /// Rimuove e restituisce l'elemento all'inizio della coda circolare.
    /// </summary>
    /// <returns>L'elemento rimosso dall'inizio della coda.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se la coda è vuota.</exception>
    public T Dequeue()
    {
        if (_count == 0)
        {
            throw new InvalidOperationException("La coda è vuota.");
        }
        
        var item = _buffer[_head];
        _buffer[_head] = default!; // Pulisce la posizione per il garbage collector
        _head = (_head + 1) % Capacity;
        _count--;

        if (_count == 0)
        {
            // Ottimizzazione: resetta i puntatori quando la coda si svuota.
            Clear();
        }

        return item;
    }
    
    /// <summary>
    /// Restituisce l'elemento all'inizio della coda senza rimuoverlo.
    /// </summary>
    /// <returns>L'elemento all'inizio della coda.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se la coda è vuota.</exception>
    public T Peek()
    {
        if (_count == 0)
        {
            throw new InvalidOperationException("La coda è vuota.");
        }
        return _buffer[_head];
    }
    
    /// <summary>
    /// Rimuove tutti gli elementi dalla coda circolare.
    /// </summary>
    public void Clear()
    {
        _head = 0;
        _tail = 0;
        _count = 0;
        Array.Clear(_buffer, 0, _buffer.Length);
    }
    
    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la coda circolare dalla testa alla coda.
    /// </summary>
    /// <returns>Un enumeratore per la coda.</returns>
    public IEnumerator<T> GetEnumerator()
    {
        if (_count == 0)
        {
            yield break;
        }
        
        var currentIndex = _head;
        for (var i = 0; i < _count; i++)
        {
            yield return _buffer[currentIndex];
            currentIndex = (currentIndex + 1) % Capacity;
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
