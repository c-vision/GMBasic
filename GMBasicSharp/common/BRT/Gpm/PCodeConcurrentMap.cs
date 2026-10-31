// FILE: Basic.Runtime.Gpm.PCodeConcurrentMap.cs
// PCODE SYNC.MAP — mappa concorrente (parità Go sync.Map). Store/Load/Delete/LoadOrStore
// thread-safe sotto GOMAXPROCS>1.

using System.Collections.Concurrent;

namespace Basic.Runtime.Gpm;

public sealed class PCodeConcurrentMap
{
    private readonly ConcurrentDictionary<object, object?> _map = new();

    public void Store(object key, object? value) => _map[key] = value;

    public object? Load(object key) => _map.TryGetValue(key, out var v) ? v : null;

    public object? Delete(object key) => _map.TryRemove(key, out var v) ? v : null;

    public object? LoadOrStore(object key, object? value) => _map.GetOrAdd(key, value);

    public int Count => _map.Count;
}
