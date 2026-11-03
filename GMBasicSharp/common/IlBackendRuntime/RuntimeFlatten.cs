// RuntimeFlatten.cs
//
// F5 (Incongruenze-Compilatore): una sola conversione "array BASIC → double[]" per i runtime
// managed. Prima IL (`IlRuntime.ArrayToDoubleArray`) e PCode (`PCodeProcessor.ToDoubleArr`)
// avevano due implementazioni con semantica DIVERSA sugli array annidati (IL un livello,
// PCode ricorsiva). La forma canonica è quella ricorsiva — la stessa di
// `gmb_vm_flatten_dbl` del runtime C (VM/C) e del PCode.
//
// La conversione degli scalari usa `IlRuntime.ToDouble` (bool → -1/0, stringa numerica
// parsata, altrimenti 0): la convenzione documentata e condivisa con il runtime C.

using System.Collections;
using System.Globalization;
using System.Linq;

namespace Basic.IlBackend;

public static class RuntimeFlatten
{
    /// <summary>Appiattisce (ricorsivamente) array/collezioni annidate in un double[].
    /// Stringhe e dizionari non sono attraversati (sono scalari).</summary>
    public static double[] ToDoubles(object? value)
    {
        if (value is double[] da) return da;
        if (value is int[] ia) return ia.Select(x => (double)x).ToArray();
        if (value is float[] fa) return fa.Select(x => (double)x).ToArray();

        var flat = new System.Collections.Generic.List<double>();
        void Walk(object? item)
        {
            switch (item)
            {
                case null:
                    flat.Add(0.0);
                    return;
                case double[] d:
                    flat.AddRange(d);
                    return;
                case int[] i:
                    foreach (var v in i) flat.Add(v);
                    return;
                case float[] f:
                    foreach (var v in f) flat.Add(v);
                    return;
                case string:
                    flat.Add(IlRuntime.ToDouble(item));
                    return;
                case IDictionary:
                    flat.Add(SafeDouble(item));
                    return;
                case IEnumerable en:
                    foreach (var sub in en) Walk(sub);
                    return;
                default:
                    flat.Add(SafeDouble(item));
                    return;
            }
        }
        Walk(value);
        return flat.ToArray();
    }

    /// <summary>Conversione di uno scalare che non deve MAI lanciare (un elemento di un array
    /// dinamico può essere un oggetto non numerico: vale 0, come il runtime C).</summary>
    private static double SafeDouble(object? value)
    {
        try { return IlRuntime.ToDouble(value); }
        catch { return 0.0; }
    }
}
