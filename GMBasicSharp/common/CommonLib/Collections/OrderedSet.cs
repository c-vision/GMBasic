// FILE: CommonLib/Collections/OrderedSet.cs
// HASHSET lato MANAGED: insieme a ORDINE DI INSERIMENTO.
//
// Perché: su IL/PCode un HASHSET è un HashSet<T>/HashSet<object?>, la cui
// enumerazione è per bucket/hash e NON stabile (dopo una Remove un nuovo Add può
// occupare uno slot libero e cambiare l'ordine). C99 e VM/C memorizzano i set in
// liste ordinate per inserimento, quindi i risultati di Union/Intersect/Except
// uscivano in ordini diversi fra le gambe ([2,9,3] vs [9,3,2]).
// Questa classe dà al PCode un contenitore con la STESSA semantica di ordine del
// runtime C, conservando il nome BASIC "HASHSET`1" (mappato in TypeNameOf).

using System.Collections;
using System.Collections.Generic;

namespace CommonLib.Collections;

/// <summary>Insieme (elementi unici) in ordine di inserimento, su object?.</summary>
public sealed class OrderedSet : IEnumerable<object?>
{
    private readonly List<object?> _items = new();

    public int Count => _items.Count;

    public bool Add(object? value)
    {
        if (_items.Contains(value)) return false;
        _items.Add(value);
        return true;
    }

    public bool Contains(object? value) => _items.Contains(value);

    public bool Remove(object? value) => _items.Remove(value);

    public void Clear() => _items.Clear();

    public List<object?> ToList() => new(_items);

    public IEnumerator<object?> GetEnumerator() => _items.GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
