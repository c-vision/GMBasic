// FILE: Interpreter.Gpm.Collections.Cache.Core.cs
using System.Collections;
using System.Collections.Concurrent;
// ReSharper disable CheckNamespace
// ReSharper disable MemberCanBePrivate.Local

/* 
 * Cache.cs: cache generica thread-safe con TTL e capienza massima (eviction FIFO approssimata)
 * 
 * FUNZIONI:
 * - Set(key, value, timeToLive): aggiunge o aggiorna elemento
 * - TryGetValue(key, out value): ottiene valore (controlla scadenza)
 * - Remove(key): rimuove elemento
 * - Clear(): svuota cache
 * - Dispose(): rilascia timer di pulizia
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una cache in memoria generica e thread-safe,
/// con supporto per scadenza temporale (TTL - Time-To-Live) e capienza massima.
///
/// Teoria e Algoritmi:
/// Questa implementazione di cache combina un `ConcurrentDictionary` per un accesso rapido e thread-safe
/// con una logica di rimozione (eviction) per gestire la scadenza degli elementi e il superamento della
/// capienza massima. La strategia di rimozione per capienza è una semplificazione di LRU (Least Recently Used):
/// quando la cache è piena, vengono rimossi gli elementi più vecchi in base all'ordine di inserimento.
/// Ogni elemento nella cache è incapsulato in un oggetto `CacheItem`, che memorizza il valore e la sua
/// scadenza (timestamp). Le operazioni di `Get` controllano automaticamente la scadenza ed eliminano
/// gli elementi scaduti ("lazy expiration"). Un meccanismo di pulizia periodica in background (`_cleanupTimer`)
/// garantisce che la memoria occupata da elementi scaduti venga rilasciata anche se non vengono più richiesti.
///
/// Performance:
/// Le operazioni di base come `Set` e `TryGetValue` beneficiano della performance O(1) in media
/// del `ConcurrentDictionary`. La gestione della scadenza aggiunge un piccolo overhead a ogni accesso
/// in lettura, ma rimane estremamente veloce. La pulizia per capienza, in questa implementazione,
/// potrebbe avere un impatto lineare sul numero di elementi da rimuovere, ma viene attivata solo al
/// superamento della soglia. La natura concorrenziale la rende adatta per ambienti multi-tasking,
/// dove più task BASIC potrebbero accedere alla stessa cache contemporaneamente senza necessità
/// di lock esterni.
/// </summary>
/// <typeparam name="TKey">Il tipo delle chiavi, che deve essere non nullo.</typeparam>
/// <typeparam name="TValue">Il tipo dei valori.</typeparam>
public class Cache<TKey, TValue> : IDisposable, IEnumerable<KeyValuePair<TKey, TValue>>, IKeyValueIndexable where TKey : notnull
{
    private sealed class CacheItem(TValue value, TimeSpan? timeToLive)
    {
        public TValue Value { get; } = value;
        public DateTimeOffset Expiration { get; } = timeToLive.HasValue
            ? DateTimeOffset.UtcNow.Add(timeToLive.Value)
            : DateTimeOffset.MaxValue;
        public bool IsExpired => DateTimeOffset.UtcNow > Expiration;
    }

    /// <summary>
    /// Il dizionario concorrente che memorizza le coppie chiave-valore della cache per un accesso thread-safe e performante.
    /// </summary>
    private readonly ConcurrentDictionary<TKey, CacheItem> _cache;
    
    /// <summary>
    /// La capacità massima della cache. Se nullo, la capacità è illimitata.
    /// </summary>
    private readonly int? _capacity;
    
    /// <summary>
    /// Il timer per la pulizia periodica degli elementi scaduti in background.
    /// </summary>
    private readonly Timer? _cleanupTimer;

    /// <summary>
    /// Ottiene il numero di elementi attualmente presenti nella cache.
    /// Questo valore potrebbe includere elementi scaduti che non sono ancora stati rimossi dal task di pulizia.
    /// </summary>
    public int Count => _cache.Count;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="Cache{TKey, TValue}"/>.
    /// </summary>
    /// <param name="capacity">La capienza massima della cache. Se superata, gli elementi più vecchi verranno rimossi. Se nullo, la capienza è illimitata.</param>
    /// <param name="cleanupFrequency">La frequenza con cui un task in background rimuoverà gli elementi scaduti. Se nullo, la pulizia avverrà solo su accesso.</param>
    public Cache(int? capacity = null, TimeSpan? cleanupFrequency = null)
    {
        _cache = new ConcurrentDictionary<TKey, CacheItem>();
        _capacity = capacity;

        if (cleanupFrequency.HasValue)
        {
            _cleanupTimer = new Timer(_ => CleanupExpired(), null, cleanupFrequency.Value, cleanupFrequency.Value);
        }
    }

    /// <summary>
    /// Aggiunge o aggiorna una coppia chiave-valore nella cache con una durata di vita specificata.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da aggiungere o aggiornare.</param>
    /// <param name="value">Il valore dell'elemento.</param>
    /// <param name="timeToLive">La durata di validità dell'elemento. Se nullo, l'elemento non scadrà mai.</param>
    public void Set(TKey key, TValue value, TimeSpan? timeToLive = null)
    {
        if (_capacity.HasValue && _cache.Count >= _capacity.Value && !_cache.ContainsKey(key))
        {
            // Strategia di rimozione semplice: rimuovi il primo elemento (approssimazione di FIFO/LRU).
            if (_cache.Keys.FirstOrDefault() is { } keyToRemove)
            {
                _cache.TryRemove(keyToRemove, out _);
            }
        }

        var cacheItem = new CacheItem(value, timeToLive);
        _cache[key] = cacheItem;
    }

    /// <summary>
    /// Ottiene il valore associato alla chiave specificata. Se l'elemento è scaduto, viene rimosso e il metodo fallisce.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da ottenere.</param>
    /// <param name="value">Quando questo metodo ritorna, contiene il valore associato alla chiave, se la chiave viene trovata e non è scaduta; altrimenti, il valore predefinito per TValue.</param>
    /// <returns><c>true</c> se la chiave è stata trovata e l'elemento non è scaduto; altrimenti, <c>false</c>.</returns>
    public bool TryGetValue(TKey key, out TValue? value)
    {
        if (_cache.TryGetValue(key, out var cacheItem))
        {
            if (cacheItem.IsExpired)
            {
                _cache.TryRemove(key, out _);
                value = default;
                return false;
            }

            value = cacheItem.Value;
            return true;
        }

        value = default;
        return false;
    }

    /// <summary>
    /// Rimuove l'elemento con la chiave specificata dalla cache.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da rimuovere.</param>
    /// <returns><c>true</c> se l'elemento è stato trovato e rimosso con successo; altrimenti, <c>false</c>.</returns>
    public bool Remove(TKey key)
    {
        return _cache.TryRemove(key, out _);
    }

    /// <summary>
    /// Rimuove tutti gli elementi dalla cache.
    /// </summary>
    public void Clear()
    {
        _cache.Clear();
    }

    /// <summary>
    /// Esegue una pulizia forzata, iterando su tutti gli elementi della cache e
    /// rimuovendo quelli che sono scaduti.
    /// </summary>
    private void CleanupExpired()
    {
        foreach (var pair in _cache)
        {
            if (pair.Value.IsExpired)
            {
                _cache.TryRemove(pair.Key, out _);
            }
        }
    }

    /// <summary>
    /// Rilascia le risorse utilizzate dalla cache, in particolare il timer di pulizia in background.
    /// </summary>
    public void Dispose()
    {
        _cleanupTimer?.Dispose();
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso gli elementi validi (non scaduti) della cache.
    /// </summary>
    /// <returns>Un enumeratore per le coppie chiave-valore valide.</returns>
    public IEnumerator<KeyValuePair<TKey, TValue>> GetEnumerator()
    {
        // Crea uno snapshot delle chiavi per un'iterazione thread-safe.
        var keys = _cache.Keys.ToArray();
        foreach (var key in keys)
        {
            // Usa TryGetValue per ottenere il valore, che gestisce anche la lazy expiration.
            if (TryGetValue(key, out var value))
            {
                yield return new KeyValuePair<TKey, TValue>(key, value!);
            }
        }
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la collezione.
    /// </summary>
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }

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
            if (value is TValue v) Set(k, v);
            else if (value is null && default(TValue) is null) Set(k, default!);
        }
    }

    bool IKeyValueIndexable.TryGetMin(out object? key, out object? value)
    {
        key = null;
        value = null;
        return false; // Cache: nessuna nozione di min/max
    }

    bool IKeyValueIndexable.TryGetMax(out object? key, out object? value)
    {
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
