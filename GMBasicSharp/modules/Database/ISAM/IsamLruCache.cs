// FILE: Basic.ISAM.IsamLruCache

namespace Basic.ISAM;

/// <summary>
/// LRU condivisa ISAM (ISAM-C1): cap 128 entry, intra-process only.
/// ponytail: pin su radice/cursore attivo; no ref-count globale (upgrade: SQLite pcache).
/// </summary>
internal sealed class IsamLruCache<TKey, TValue> where TKey : notnull
{
    internal const int DefaultCapacity = 128;

    private readonly LruCache<TKey, TValue> _inner = new(DefaultCapacity);

    public bool TryGetValue(TKey key, out TValue? value) => _inner.TryGetValue(key, out value);

    public void AddOrUpdate(TKey key, TValue value) => _inner.AddOrUpdate(key, value);

    public void Pin(TKey key) => _inner.Pin(key);

    public void Unpin(TKey key) => _inner.Unpin(key);

    public void Remove(TKey key) => _inner.Remove(key);

    public void Clear() => _inner.Clear();
}
