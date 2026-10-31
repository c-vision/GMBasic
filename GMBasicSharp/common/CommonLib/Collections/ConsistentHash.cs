// FILE: Interpreter.Gpm.Collections.ConsistentHash.Core.cs
using System.Text;
// ReSharper disable CheckNamespace

/* 
 * ConsistentHash.cs: anello di hashing coerente con repliche virtuali e MurmurHash2
 * 
 * FUNZIONI:
 * - AddNode(node): aggiunge nodo all'anello
 * - GetNode(key): ottiene nodo responsabile per una chiave
 * - RemoveNode(node): rimuove nodo dall'anello
 * - MurmurHash2.Hash(data): hash MurmurHash2 a 32-bit
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di un anello di hashing coerente (Consistent Hash Ring).
///
/// Teoria e Algoritmi:
/// L'hashing coerente è un algoritmo progettato per risolvere il problema della ridistribuzione delle chiavi
/// in un sistema distribuito quando il numero di bucket (es. server di cache) cambia. Un approccio ingenuo
/// (hash(key) % N) causerebbe un rimescolamento quasi totale delle chiavi. L'hashing coerente mappa sia i bucket
/// ("nodi") che le chiavi su un cerchio concettuale ("anello"). Una chiave viene assegnata al primo nodo che si
/// incontra procedendo in senso orario sull'anello. Per garantire una distribuzione uniforme, ogni nodo fisico
/// viene mappato in più punti sull'anello ("repliche" o "nodi virtuali").
/// Quando un nodo viene aggiunto o rimosso, solo una piccola frazione delle chiavi (in media k/N, dove k è il
/// numero totale di chiavi e N il numero di nodi) deve essere ri-mappata, minimizzando il carico di ridistribuzione.
/// Questa implementazione utilizza un `SortedDictionary` per rappresentare l'anello e l'algoritmo di hash MurmurHash2
/// per una distribuzione delle chiavi di alta qualità.
///
/// Performance:
/// - Aggiunta/Rimozione di un nodo: O(R * log(N*R)), dove R è il numero di repliche e N il numero di nodi, a causa
///   degli inserimenti nello `SortedDictionary` e della ricostruzione dell'array di chiavi.
/// - Ricerca di un nodo per una chiave (`GetNode`): O(log(N*R)) grazie alla ricerca binaria efficiente sull'array
///   ordinato delle chiavi dell'anello.
/// La complessità spaziale è O(N*R) per memorizzare tutti i nodi virtuali sull'anello.
/// </summary>
/// <typeparam name="T">Il tipo dei nodi (bucket) nel ring.</typeparam>
public class ConsistentHash<T>
{
    /// <summary>
    /// Un dizionario ordinato che rappresenta l'anello di hashing. La chiave è il valore hash di un nodo virtuale,
    /// e il valore è il nodo fisico a cui è associato.
    /// </summary>
    private readonly SortedDictionary<int, T> _circle = new();

    /// <summary>
    /// Il numero di nodi virtuali (repliche) da creare per ogni nodo fisico.
    /// </summary>
    private readonly int _replicas;
    
    /// <summary>
    /// Un array pre-ordinato delle chiavi (hash) presenti nell'anello.
    /// Questa è un'ottimizzazione per rendere la ricerca del nodo successivo O(log n) tramite ricerca binaria.
    /// </summary>
    private int[] _circleKeys = [];

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="ConsistentHash{T}"/>.
    /// </summary>
    /// <param name="nodes">Una collezione di nodi iniziali da aggiungere all'anello.</param>
    /// <param name="replicas">Il numero di nodi virtuali da creare per ogni nodo fisico. Un valore più alto garantisce una distribuzione migliore ma aumenta l'overhead di memoria.</param>
    public ConsistentHash(IEnumerable<T> nodes, int replicas)
    {
        _replicas = replicas > 0 ? replicas : 100; // Default a 100 se non valido
        foreach (var node in nodes)
        {
            AddNode(node);
        }
    }

    /// <summary>
    /// Aggiunge un nuovo nodo (bucket) all'anello di hashing.
    /// </summary>
    /// <param name="node">Il nodo da aggiungere.</param>
    public void AddNode(T node)
    {
        for (var i = 0; i < _replicas; i++)
        {
            // Usa ToString() per una rappresentazione più stabile e prevedibile dell'oggetto
            var key = node!.ToString() + i;
            var hash = GetHashCode(key);
            _circle[hash] = node;
        }

        _circleKeys = _circle.Keys.ToArray();
    }

    /// <summary>
    /// Ottiene il nodo (bucket) responsabile per la gestione della chiave data.
    /// </summary>
    /// <param name="key">La chiave da mappare a un nodo.</param>
    /// <returns>Il nodo a cui la chiave è stata assegnata.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se l'anello non contiene nodi.</exception>
    public T GetNode(string key)
    {
        if (_circle.Count == 0)
        {
            throw new InvalidOperationException("L'anello di hashing non contiene nodi.");
        }

        var hash = GetHashCode(key);
        var first = FindNextClockwiseNodeIndex(_circleKeys, hash);
        return _circle[_circleKeys[first]];
    }

    /// <summary>
    /// Rimuove un nodo (bucket) dall'anello di hashing.
    /// </summary>
    /// <param name="node">Il nodo da rimuovere.</param>
    /// <exception cref="ArgumentException">Lanciata se si tenta di rimuovere un nodo non presente.</exception>
    public void RemoveNode(T node)
    {
        var removedCount = 0;
        for (var i = 0; i < _replicas; i++)
        {
            var key = node!.ToString() + i;
            var hash = GetHashCode(key);
            if (_circle.Remove(hash))
            {
                removedCount++;
            }
        }

        if (removedCount == 0)
        {
            throw new ArgumentException("Impossibile rimuovere un nodo che non è mai stato aggiunto.", nameof(node));
        }

        _circleKeys = _circle.Keys.ToArray();
    }

    /// <summary>
    /// Trova l'indice del primo nodo sull'anello procedendo in senso orario a partire da un dato hash.
    /// Utilizza la ricerca binaria per performance ottimali.
    /// </summary>
    /// <param name="keys">L'array ordinato delle chiavi (hash) presenti sull'anello.</param>
    /// <param name="hashCode">L'hash della chiave da mappare.</param>
    /// <returns>L'indice nell'array `keys` del nodo responsabile.</returns>
    private static int FindNextClockwiseNodeIndex(int[] keys, int hashCode)
    {
        // Se l'hash è maggiore dell'ultimo nodo, torna al primo (wrap-around).
        if (hashCode > keys[^1])
        {
            return 0;
        }

        // Esegue una ricerca binaria per trovare il primo nodo con hash >= hashCode.
        var index = Array.BinarySearch(keys, hashCode);

        // Se BinarySearch restituisce un valore non negativo, è una corrispondenza esatta.
        if (index >= 0)
        {
            return index;
        }

        // Se restituisce un valore negativo, è il complemento bitwise dell'indice del primo elemento
        // più grande della nostra chiave. Questo è esattamente ciò che ci serve.
        index = ~index;

        return index;
    }

    /// <summary>
    /// Calcola l'hash di una chiave usando l'algoritmo MurmurHash2.
    /// </summary>
    private static int GetHashCode(string key)
    {
        return (int)MurmurHash2.Hash(Encoding.UTF8.GetBytes(key));
    }
}

/// <summary>
/// Implementazione dell'algoritmo di hash MurmurHash2 a 32-bit.
/// È un algoritmo di hash non crittografico noto per la sua eccellente distribuzione e velocità.
/// Adattato da https://github.com/wsq003/consistent-hash/blob/master/ConsistentHash.cs
/// </summary>
internal static class MurmurHash2
{
    private const uint M = 0x5bd1e995;
    private const int R = 24;

    internal static uint Hash(byte[] data, uint seed = 0xc58f1a7b)
    {
        var length = data.Length;
        if (length == 0) return 0;

        var h = seed ^ (uint)length;
        var i = 0;
        while (length >= 4)
        {
            var k = (uint)(data[i++] | data[i++] << 8 | data[i++] << 16 | data[i++] << 24);
            k *= M;
            k ^= k >> R;
            k *= M;
            h *= M;
            h ^= k;
            length -= 4;
        }

        switch (length)
        {
            case 3:
                h ^= (ushort)(data[i++] | data[i++] << 8);
                h ^= (uint)data[i] << 16;
                h *= M;
                break;
            case 2:
                h ^= (ushort)(data[i++] | data[i] << 8);
                h *= M;
                break;
            case 1:
                h ^= data[i];
                h *= M;
                break;
        }

        h ^= h >> 13;
        h *= M;
        h ^= h >> 15;

        return h;
    }
}
