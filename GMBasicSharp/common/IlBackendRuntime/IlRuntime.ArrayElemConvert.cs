// FILE: IlBackendRuntime/IlRuntime.ArrayElemConvert.cs
// Conversione del KIND dell'elemento di un array al confine di una chiamata a procedura
// utente (2026-11-04). Su IL gli array BASIC sono array CLR tipizzati (double[]/long[]),
// mentre C99/PCode/VM-C hanno un'unica rappresentazione numerica: senza questa conversione
// un `DIM x(1) AS LONG` passato a un parametro `a() AS DOUBLE` faceva leggere i bit IEEE-754
// come int64 (valori spazzatura, solo su IL).
using System;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    /// <summary>Vista `double[]` dell'array: copia convertita se l'elemento è intero.
    /// Accetta anche SEQUENZE (List/Queue/Stack/Set): AMAX/AMIN/ASUM su LIST&lt;INTEGER&gt;
    /// ricevevano un long[] e la funzione leggeva i bit come double (garbage).</summary>
    public static double[] ToDoubleArray(object? a) => a switch
    {
        double[] d => d,
        long[] l => Array.ConvertAll(l, v => (double)v),
        ulong[] u => Array.ConvertAll(u, v => (double)v),
        System.Collections.IEnumerable en when a is not string => ConvertToDoubles(en),
        _ => Array.Empty<double>()
    };

    private static double[] ConvertToDoubles(System.Collections.IEnumerable en)
    {
        var list = new System.Collections.Generic.List<double>();
        foreach (var x in en) list.Add(ToDouble(x));
        return list.ToArray();
    }

    /// <summary>Vista `long[]` dell'array: copia convertita se l'elemento è double.</summary>
    public static long[] ToLongArray(object? a) => a switch
    {
        long[] l => l,
        double[] d => Array.ConvertAll(d, v => (long)v),
        ulong[] u => Array.ConvertAll(u, v => (long)v),
        _ => Array.Empty<long>()
    };

    /// <summary>Copia indietro dopo la chiamata (il callee può aver scritto nel buffer).</summary>
    public static void CopyBackDouble(object? dst, double[] src)
    {
        if (dst is long[] l) { for (var i = 0; i < l.Length && i < src.Length; i++) l[i] = (long)src[i]; }
        else if (dst is double[] d) Array.Copy(src, d, Math.Min(src.Length, d.Length));
    }

    /// <summary>Copia indietro dopo la chiamata (il callee può aver scritto nel buffer).</summary>
    public static void CopyBackLong(object? dst, long[] src)
    {
        if (dst is double[] d) { for (var i = 0; i < d.Length && i < src.Length; i++) d[i] = (double)src[i]; }
        else if (dst is long[] l) Array.Copy(src, l, Math.Min(src.Length, l.Length));
    }
}
