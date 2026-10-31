// FILE: Basic.LruCache.cs

using System.Runtime.CompilerServices;

// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Implementa una cache di tipo "Least Recently Used" (LRU).
/// Quando la cache raggiunge la sua capacità massima, l'elemento che non è stato
/// utilizzato per più tempo viene automaticamente rimosso per fare spazio.
/// Le operazioni di accesso e inserimento hanno una complessità media di O(1).
/// </summary>
/// <typeparam name="TKey">Il tipo della chiave.</typeparam>
/// <typeparam name="TValue">Il tipo del valore.</typeparam>
internal class LruCache<TKey, TValue> where TKey : notnull
{
    private readonly int _capacity;
    private readonly Dictionary<TKey, LinkedListNode<CacheItem>> _cache;
    private readonly LinkedList<CacheItem> _lruList;
    private readonly HashSet<TKey> _pinned = new();

    /// <summary>
    /// Rappresenta un singolo elemento all'interno della cache.
    /// </summary>
    private class CacheItem(TKey key, TValue value)
    {
        public TKey Key { get; } = key;
        public TValue Value { get; set; } = value;
    }

    public LruCache(int capacity)
    {
        if (capacity <= 0)
            throw new ArgumentException("La capacità della cache deve essere maggiore di zero.", nameof(capacity));
        
        _capacity = capacity;
        _cache = new Dictionary<TKey, LinkedListNode<CacheItem>>(capacity);
        _lruList = new LinkedList<CacheItem>();
    }

    /// <summary>
    /// Tenta di ottenere un valore dalla cache. Se l'elemento esiste,
    /// viene contrassegnato come usato di recente.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <param name="value">Il valore associato alla chiave, se trovato.</param>
    /// <returns>True se la chiave è presente nella cache, altrimenti False.</returns>
    [MethodImpl(MethodImplOptions.Synchronized)]
    public bool TryGetValue(TKey key, out TValue? value)
    {
        if (_cache.TryGetValue(key, out var node))
        {
            _lruList.Remove(node);
            _lruList.AddFirst(node);
            value = node.Value.Value;
            return true;
        }
        value = default;
        return false;
    }

    /// <summary>
    /// Aggiunge o aggiorna un elemento nella cache.
    /// Se l'elemento viene aggiunto e la cache è piena, l'elemento meno
    /// usato di recente viene rimosso.
    /// </summary>
    /// <param name="key">La chiave dell'elemento.</param>
    /// <param name="value">Il valore dell'elemento.</param>
    [MethodImpl(MethodImplOptions.Synchronized)]
    public void Pin(TKey key) => _pinned.Add(key);

    [MethodImpl(MethodImplOptions.Synchronized)]
    public void Unpin(TKey key) => _pinned.Remove(key);

    [MethodImpl(MethodImplOptions.Synchronized)]
    public void AddOrUpdate(TKey key, TValue value)
    {
        if (_cache.TryGetValue(key, out var existingNode))
        {
            existingNode.Value.Value = value;
            _lruList.Remove(existingNode);
            _lruList.AddFirst(existingNode);
        }
        else
        {
            if (_cache.Count >= _capacity)
            {
                var victim = _lruList.Last;
                while (victim != null && _pinned.Contains(victim.Value.Key))
                {
                    victim = victim.Previous;
                }

                if (victim != null)
                {
                    _cache.Remove(victim.Value.Key);
                    _lruList.Remove(victim);
                }
            }

            var newItem = new CacheItem(key, value);
            var newNode = new LinkedListNode<CacheItem>(newItem);
            _cache[key] = newNode;
            _lruList.AddFirst(newNode);
        }
    }

    /// <summary>
    /// Rimuove un elemento dalla cache.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da rimuovere.</param>
    [MethodImpl(MethodImplOptions.Synchronized)]
    public void Remove(TKey key)
    {
        if (_cache.TryGetValue(key, out var node))
        {
            _lruList.Remove(node);
            _cache.Remove(key);
        }
    }
    
    /// <summary>
    /// Svuota la cache.
    /// </summary>
    [MethodImpl(MethodImplOptions.Synchronized)]
    public void Clear()
    {
        _cache.Clear();
        _lruList.Clear();
    }
}