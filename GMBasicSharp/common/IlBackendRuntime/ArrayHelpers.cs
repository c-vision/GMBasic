// FILE: Basic.IlBackend.ArrayHelpers.cs

namespace Basic.IlBackend;

/// <summary>
/// Helper generici invocati direttamente dal codice IL emesso per le funzioni array in stile
/// Clipper (ACLONE, AMAP, AFILTER, ecc.). Le convenzioni di ciascun metodo (indicizzazione,
/// valore di default per "non trovato", ecc.) rispecchiano il tree-walking Interpreter e il
/// sample Executor/Samples/Clipper/ClipperArrays.bas (la fonte più vicina a uno "spec"
/// documentato), non il PCodeVm dove alcune di queste funzioni divergono per convenzione
/// (es. ASCAN: qui 0-based/-1 come l'Interpreter, il PCodeVm usa invece 1-based/0 —
/// divergenza preesistente tra i due motori, non introdotta qui).
/// </summary>
public static class ArrayHelpers
{
    public static T[] AClone<T>(T[] array) => (T[])array.Clone();

    /// <summary>Converte un array al tipo del parametro della callback (es. double[] →
    /// int16[] per FN(n AS INTEGER) — il delegate della lambda è tipizzato sul parametro).</summary>
    public static TTo[] ConvertArray<TFrom, TTo>(TFrom[] source)
    {
        var result = new TTo[source.Length];
        for (var i = 0; i < source.Length; i++)
            result[i] = (TTo)System.Convert.ChangeType(source[i]!, typeof(TTo), System.Globalization.CultureInfo.InvariantCulture);
        return result;
    }

    /// <summary>AMAP su una LISTA/espressione IEnumerable (DIM x = [..]) — non solo array.</summary>
    public static TResult[] AMapSeq<T, TResult>(System.Collections.Generic.IEnumerable<T> source, Func<T, TResult> mapFunc)
        => source.Select(mapFunc).ToArray();

    /// <summary>AFILTER su una LISTA/espressione IEnumerable (DIM x = [..]) — non solo array.</summary>
    public static T[] AFilterSeq<T>(System.Collections.Generic.IEnumerable<T> source, Func<T, bool> filterFunc)
        => source.Where(filterFunc).ToArray();

    /// <summary>AFILTER con predicato NUMERICO (lambda `AS INTEGER => (n MOD 2) = 0` — il
    /// delegate è Func<T, short> 0/1, non Func<T,bool>). Testa != 0, come la VM.</summary>
    public static T[] AFilterNumeric<T, TNum>(T[] array, Func<T, TNum> filterFunc) where TNum : struct
    {
        var result = new List<T>();
        foreach (var item in array)
        {
            var v = filterFunc(item);
            var ok = Convert.ToDouble(v, System.Globalization.CultureInfo.InvariantCulture) != 0.0;
            if (ok) result.Add(item);
        }
        return result.ToArray();
    }

    /// <summary>Alloca un array di reference type INIZIALIZZANDO ogni elemento a new T()
    /// (per DIM x(n) AS CLASS/UDT — il VM materializza gli elementi, il CLR no). SENZA
    /// vincolo new(): una classe con SOLO costruttori a parametri (es. Forma con
    /// SUB NEW(x,y)) non può new T() — gli elementi restano null (default) e vanno
    /// assegnati dal codice (idioma comune: DIM base(2) AS Classe poi base(0)=...).</summary>
    public static T[] NewInstanceArray<T>(int length)
    {
        var arr = new T[length];
        if (typeof(T).GetConstructor(Type.EmptyTypes) is not null)
            for (var i = 0; i < length; i++) arr[i] = (T)System.Activator.CreateInstance(typeof(T))!;
        return arr;
    }

    /// <summary>Alloca un array di STRING inizializzando ogni elemento a "" (default BASIC
    /// per una stringa non assegnata — il CLR con `new string[n]` li lascia null, e
    /// LEN/TRIM/concatenazione su un elemento mai assegnato darebbe NullReferenceException,
    /// a differenza di uno scalare STRING che l'IL backend inizializza già a "").</summary>
    public static string[] NewStringArray(int length)
    {
        var arr = new string[length];
        Array.Fill(arr, string.Empty);
        return arr;
    }

    public static T[] AUnique<T>(T[] array)
    {
        var seen = new HashSet<T>();
        var list = new List<T>();
        foreach (var item in array)
            if (seen.Add(item)) list.Add(item);
        return list.ToArray();
    }

    public static string[] ATokens(string? source, string? delimiters)
    {
        var s = source ?? string.Empty;
        var delims = delimiters?.ToCharArray() ?? [' ', '\t', ',', ';'];
        return s.Split(delims, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    }

    public static TResult[] AMap<T, TResult>(T[] array, Func<T, TResult> mapFunc)
    {
        var result = new TResult[array.Length];
        for (var i = 0; i < array.Length; i++) result[i] = mapFunc(array[i]);
        return result;
    }

    public static T[] AFilter<T>(T[] array, Func<T, bool> filterFunc)
    {
        var list = new List<T>();
        foreach (var item in array)
            if (filterFunc(item)) list.Add(item);
        return list.ToArray();
    }

    public static TAcc AReduce<T, TAcc>(T[] array, Func<TAcc, T, TAcc> reduceFunc, TAcc initialValue)
    {
        var acc = initialValue;
        foreach (var item in array) acc = reduceFunc(acc, item);
        return acc;
    }

    public static T AReduceNoInitial<T>(T[] array, Func<T, T, T> reduceFunc)
    {
        if (array.Length == 0) return default!;
        var acc = array[0];
        for (var i = 1; i < array.Length; i++) acc = reduceFunc(acc, array[i]);
        return acc;
    }

    public static long ACount<T>(T[] array, Func<T, bool> conditionFunc)
    {
        long count = 0;
        foreach (var item in array)
            if (conditionFunc(item)) count++;
        return count;
    }

    /// <summary>
    /// Indice 0-based del primo elemento uguale a <paramref name="value"/>, -1 se non trovato —
    /// stessa convenzione di EvaluateAScanFromStack nel tree-walking Interpreter e del sample
    /// Clipper/ClipperArrays.bas (coerente anche con ABinarySearch, che delega direttamente a
    /// CommonLib.ArrayUtils.BinarySearch, anch'esso 0-based/-1).
    /// </summary>
    public static long AScanValue<T>(T[] array, T value)
    {
        for (var i = 0; i < array.Length; i++)
            if (EqualityComparer<T>.Default.Equals(array[i], value)) return i;
        return -1;
    }

    /// <summary>Indice 0-based del primo elemento che soddisfa il predicato, -1 se nessuno lo soddisfa (vedi AScanValue).</summary>
    public static long AScanPredicate<T>(T[] array, Func<T, bool> predicateFunc)
    {
        for (var i = 0; i < array.Length; i++)
            if (predicateFunc(array[i])) return i;
        return -1;
    }

    public static void AEval<T>(T[] array, Action<T> actionFunc)
    {
        foreach (var item in array) actionFunc(item);
    }

    public static void AEvalIndexed<T>(T[] array, Action<T, long> actionFunc)
    {
        for (var i = 0; i < array.Length; i++) actionFunc(array[i], i);
    }

    public static void ASort<T>(T[] array, bool descending)
    {
        Array.Sort(array);
        if (descending) Array.Reverse(array);
    }

    /// <summary>
    /// TResult è aperto (non vincolato a int) perché la funzione di confronto utente può
    /// dichiarare un tipo di ritorno BASIC qualunque (AS INTEGER/LONG/DOUBLE/...), che mappa
    /// a un CLR diverso da Int32 — l'unico che System.Comparison&lt;T&gt; accetterebbe
    /// direttamente. Convert.ToDouble normalizza il segno indipendentemente dal tipo
    /// numerico concreto restituito dalla lambda.
    /// </summary>
    public static void ASortWithComparer<T, TResult>(T[] array, Func<T, T, TResult> compareFunc, bool descending)
    {
        // Insertion sort STABILE che sposta a sinistra finché `compareFunc(key, prev) < 0`.
        // Gestisce SIA il comparatore a 3 vie (segno: <0 = prima) SIA il predicato booleano
        // BASIC (TRUE = -1 = "prima"; FALSE = 0 = "non prima"). Array.Sort trattava il
        // predicato FALSE (0) come "uguale" e non ordinava; questa implementazione è identica
        // alla `gmb_array_sort_*_cmp` del runtime C (C99Native) e alla VM/C.
        // NB: `Convert.ToDouble(bool)` darebbe +1 (CLR), invertendo l'ordine → normalizza a -1.
        static double Norm(TResult raw) => raw is bool bv ? (bv ? -1.0 : 0.0) : Convert.ToDouble(raw);
        for (var i = 1; i < array.Length; i++)
        {
            var key = array[i];
            var j = i - 1;
            while (j >= 0 && Norm(compareFunc(key, array[j])) < 0)
            {
                array[j + 1] = array[j];
                j--;
            }
            array[j + 1] = key;
        }
        if (descending) Array.Reverse(array);
    }
}
