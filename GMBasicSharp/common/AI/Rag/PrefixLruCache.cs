// FILE: Basic.AI.Rag.PrefixLruCache
// Cache LRU bounded per la T1 prefix cache: mappa (key -> value) con eviction
// Least-Recently-Used quando supera la capacita'. Componente puro (nessuna dipendenza
// nativa) -> testabile senza modello. Il completer la usa per il prefisso di sistema.
namespace Basic.AI.Rag;

public sealed class PrefixLruCache
{
    private readonly int _capacity;
    private readonly Dictionary<string, string> _map = new();
    private readonly LinkedList<string> _order = new();

    public PrefixLruCache(int capacity)
    {
        _capacity = capacity > 0 ? capacity : 16;
    }

    public int Count => _map.Count;

    /// <summary>Ritorna il valore associato alla chiave e la marca come recently-used
    /// (move-to-front). Ritorna null se assente.</summary>
    public string? Get(string key)
    {
        if (!_map.TryGetValue(key, out var v))
            return null;
        Touch(key);
        return v;
    }

    /// <summary>Inserisce (o aggiorna) la coppia key/value come most-recently-used. Se la
    /// cache supera la capacita', evict la Least-Recently-Used e la ritorna (perche' il
    /// chiamante possa cancellarne i file su disco). Ritorna null se nessuna eviction.</summary>
    public string? Put(string key, string value)
    {
        _map[key] = value;
        Touch(key);

        string? evicted = null;
        while (_map.Count > _capacity)
        {
            var oldest = _order.First!.Value;
            _order.RemoveFirst();
            _map.Remove(oldest);
            evicted = oldest;
        }
        return evicted;
    }

    public bool Contains(string key) => _map.ContainsKey(key);

    public IEnumerable<KeyValuePair<string, string>> Entries => _map;

    private void Touch(string key)
    {
        _order.Remove(key);
        _order.AddLast(key);
    }
}
