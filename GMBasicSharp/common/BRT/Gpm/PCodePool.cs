// FILE: Basic.Runtime.Gpm.PCodePool.cs
// PCODE POOL — riuso di oggetti (parità Go sync.Pool). GET popa il primo elemento
// disponibile (o null se vuoto), PUT lo restituisce al pool.

using System.Collections.Generic;

namespace Basic.Runtime.Gpm;

public sealed class PCodePool
{
    private readonly object _lock = new();
    private readonly LinkedList<object?> _items = new();

    public object? Get()
    {
        lock (_lock)
        {
            if (_items.First is { } first)
            {
                _items.RemoveFirst();
                return first.Value;
            }
            return null;
        }
    }

    public void Put(object? item)
    {
        if (item == null) return;
        lock (_lock)
        {
            _items.AddFirst(item);
        }
    }
}
