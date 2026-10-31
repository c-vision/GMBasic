// FILE: CommonLib.ArrayUtils.cs

/* 
 * ArrayUtils.cs: utilità per la manipolazione di array .NET in stile BASIC
 * 
 * FUNZIONI:
 * - Shuffle(array): mescola casualmente gli elementi di un array usando Fisher-Yates
 * - Reverse(array): inverte l'ordine degli elementi di un array
 * - BinarySearch(array, value): ricerca binaria in un array ordinato
 * - Join(array, separator): concatena gli elementi di un array in una stringa
 * - Chunk(array, chunkSize): suddivide un array in blocchi di dimensione fissa
 * - Flatten(array): appiattisce un array annidato di qualsiasi profondità
 * - Pad(array, newLength, padValue): ridimensiona un array con padding
 * - Zip(arrays): combina più array elemento per elemento
 */

namespace CommonLib;

/// <summary>
/// Utility per la manipolazione di array .NET in stile BASIC. Tutti i metodi sono statici,
/// stateless e agiscono su oggetti Array generici. Includono funzioni per mescolamento
/// (Fisher-Yates shuffle), inversione, ricerca binaria, concatenazione con separatore,
/// chunking, flatten di array annidati, padding e zip (combinazione elemento per elemento).
/// </summary>
public static class ArrayUtils
{
    /// <summary>Mescola casualmente gli elementi di un array usando Fisher-Yates in-place.</summary>
    /// <param name="array">Array da mescolare (modificato direttamente).</param>
    public static void Shuffle(Array array)
    {
        var rng = new Random();
        for (var i = array.Length - 1; i > 0; i--)
        {
            var j = rng.Next(i + 1);
            var temp = array.GetValue(i);
            array.SetValue(array.GetValue(j), i);
            array.SetValue(temp, j);
        }
    }

    /// <summary>Inverte l'ordine degli elementi di un array in-place.</summary>
    /// <param name="array">Array da invertire (modificato direttamente).</param>
    public static void Reverse(Array array)
    {
        int left = 0, right = array.Length - 1;
        while (left < right)
        {
            var tmp = array.GetValue(left);
            array.SetValue(array.GetValue(right), left);
            array.SetValue(tmp, right);
            left++;
            right--;
        }
    }

    /// <summary>Ricerca binaria in un array ordinato. L'array deve essere ordinato secondo il default comparer.</summary>
    /// <param name="array">Array ordinato da cercare.</param>
    /// <param name="value">Valore da cercare.</param>
    /// <returns>Indice 0-based dell'elemento, o -1 se non trovato.</returns>
    public static int BinarySearch(Array array, object? value)
    {
        int left = 0, right = array.Length - 1;
        while (left <= right)
        {
            var mid = (left + right) / 2;
            var cmp = Comparer<object>.Default.Compare(array.GetValue(mid), value);
            if (cmp == 0) return mid;
            if (cmp < 0) left = mid + 1;
            else right = mid - 1;
        }
        return -1;
    }

    /// <summary>Concatena gli elementi di un array in una stringa, separati da un delimitatore.</summary>
    /// <param name="array">Array di elementi da concatenare.</param>
    /// <param name="separator">Delimitatore tra gli elementi (default: ",").</param>
    /// <returns>Stringa con tutti i valori concatenati.</returns>
    public static string Join(Array array, string separator = ",")
    {
        var strs = new string[array.Length];
        for (var i = 0; i < array.Length; i++)
            strs[i] = array.GetValue(i)?.ToString() ?? "";
        return string.Join(separator, strs);
    }

    /// <summary>Suddivide un array in blocchi di dimensione fissa. Simile a Python: chunks(lista, n).</summary>
    /// <param name="array">Array sorgente da suddividere.</param>
    /// <param name="chunkSize">Dimensione massima di ogni blocco (deve essere > 0).</param>
    /// <returns>Array di array, ciascuno con al massimo chunkSize elementi.</returns>
    public static Array Chunk(Array array, int chunkSize)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(chunkSize);
        var elementType = array.GetType().GetElementType() ?? typeof(object);
        var list = new List<Array>();
        for (var i = 0; i < array.Length; i += chunkSize)
        {
            var size = Math.Min(chunkSize, array.Length - i);
            var chunk = Array.CreateInstance(elementType, size);
            Array.Copy(array, i, chunk, 0, size);
            list.Add(chunk);
        }
        var result = new Array[list.Count];
        for (var i = 0; i < list.Count; i++)
            result.SetValue(list[i], i);
        return result;
    }

    /// <summary>Appiattisce un array annidato di qualsiasi profondità in un array monodimensionale.</summary>
    /// <param name="array">Array potenzialmente annidato (es. [1, [2,3], 4]).</param>
    /// <returns>Nuovo array piatto con tutti gli elementi foglia in ordine.</returns>
    public static Array Flatten(Array array)
    {
        // GetElementType() di un array JAGGED (T[][]) ritorna T[] (UN livello sotto), non il
        // tipo FOGLIA T — per un array rank-1 (T[]) è già corretto (nessuna iterazione), ma
        // per un array annidato (T[][], risultato di un letterale {{...}} multi-livello) il
        // risultato appiattito veniva costruito con elementType = T[] invece di T: SetValue di
        // un valore foglia BOXED (es. un double) in uno slot T[] lancia InvalidCastException a
        // runtime (verificato) — bug reale, ArraysFluent.bas: AFLAT su array jagged.
        var elementType = array.GetType().GetElementType() ?? typeof(object);
        while (elementType is { IsArray: true })
            elementType = elementType.GetElementType() ?? typeof(object);
        var result = new List<object?>();
        DoFlatten(array);
        var flatArr = Array.CreateInstance(elementType, result.Count);
        for (var i = 0; i < result.Count; i++)
            flatArr.SetValue(result[i], i);
        return flatArr;

        void DoFlatten(Array arr)
        {
            foreach (var item in arr)
            {
                if (item is Array subArray)
                    DoFlatten(subArray);
                else
                    result.Add(item);
            }
        }
    }

    /// <summary>Ridimensiona un array con padding fino alla lunghezza desiderata. Tronca se newLength è minore.</summary>
    /// <param name="array">Array sorgente.</param>
    /// <param name="newLength">Lunghezza desiderata (deve essere >= 0).</param>
    /// <param name="padValue">Valore di riempimento per i nuovi elementi (default: null).</param>
    /// <returns>Nuovo array della lunghezza specificata.</returns>
    public static Array Pad(Array array, int newLength, object? padValue = null)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(newLength);
        var elementType = array.GetType().GetElementType() ?? typeof(object);
        var newArray = Array.CreateInstance(elementType, newLength);
        var minLen = Math.Min(array.Length, newLength);
        Array.Copy(array, newArray, minLen);
        for (var i = array.Length; i < newLength; i++)
            newArray.SetValue(padValue, i);
        return newArray;
    }

    /// <summary>Combina più array elemento per elemento (zip), producendo un array di tuple.</summary>
    /// <param name="arrays">Array di array da zippare. Almeno uno richiesto.</param>
    /// <returns>Array di array, ciascuno contenente gli elementi corrispondenti degli input.</returns>
    public static Array Zip(params Array[] arrays)
    {
        if (arrays is null || arrays.Length == 0)
            throw new ArgumentException("Devi fornire almeno un array a Zip.");
        var minLength = arrays.Min(a => a.Length);
        var result = new object[minLength];
        for (var i = 0; i < minLength; i++)
        {
            var tuple = new object?[arrays.Length];
            for (var j = 0; j < arrays.Length; j++)
                tuple[j] = arrays[j].GetValue(i);
            result.SetValue(tuple, i);
        }
        return result;
    }
}
