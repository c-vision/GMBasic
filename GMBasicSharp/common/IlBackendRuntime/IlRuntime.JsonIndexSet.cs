// FILE: IlBackendRuntime/IlRuntime.JsonIndexSet.cs
// Scrittura di un ELEMENTO di un valore JSON dinamico (lista): j[0] = v.
// Distinta da JsonFieldSet (chiave STRINGA su dict). Il backend IL, per un indice
// numerico, passava la chiave a JsonFieldSet convertendola in stringa: la lista
// restava invariata (bug o.arr[0] = 9 su IL/PCode? PCode usava CollectionIndexSet).
using System;
using System.Collections;
using System.Globalization;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    /// <summary>j[i] = v su un valore JSON (lista) o, se è un dict, con chiave
    /// numerica convertita. Null = no-op (parità VM).</summary>
    public static void JsonIndexSet(object? instance, object? index, object? value)
    {
        if (instance is null) return;
        if (instance is IList list)
        {
            int i = Convert.ToInt32(index, CultureInfo.InvariantCulture);
            if (i >= 0 && i < list.Count) list[i] = value;
            return;
        }
        if (instance is IDictionary dict)
        {
            dict[index!] = value;
            return;
        }
        // JsonElement immutabile / scalare: no-op come JsonFieldSet su non-dict.
    }
}
