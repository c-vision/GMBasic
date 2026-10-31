// FILE: CommonLib.UdtUtils.cs

namespace CommonLib;

using System.Runtime.CompilerServices;

/// <summary>Utility per User-Defined Types (UDT).</summary>
public static class UdtUtils
{
    private static long _varPtrCounter = 0x100000;
    private static readonly Dictionary<int, long> _addresses = new();

    /// <summary>Token opaco di identità per VARPTR/STRPTR (canone 2026-09-30: mai 0, non
    /// dereferenziabile). Tipi valore → chiave per CONTENUTO (su IL ogni chiamata re-boxa e
    /// l'identità sarebbe instabile fra chiamate); riferimenti → identità dell'oggetto
    /// (stabile finché è vivo). Stesso spazio handle della VM/C (da 0x100000).</summary>
    public static long VarPtr(object? value)
    {
        if (value == null) return 0;
        var t = value.GetType();
        int hash = t.IsValueType ? HashCode.Combine(t, value) : RuntimeHelpers.GetHashCode(value);
        if (_addresses.TryGetValue(hash, out long addr)) return addr;
        addr = Interlocked.Increment(ref _varPtrCounter);
        _addresses[hash] = addr;
        return addr;
    }

    /// <summary>Calcola l'allineamento in byte per un tipo UDT.</summary>
    /// <param name="fieldSizes">Array delle dimensioni in byte di ciascun campo.</param>
    /// <returns>Allineamento massimo richiesto (1, 2, 4, o 8).</returns>
    public static int UdtAligned(params int[] fieldSizes)
    {
        int maxAlign = 1;
        foreach (int size in fieldSizes)
        {
            if (size >= 8) maxAlign = Math.Max(maxAlign, 8);
            else if (size >= 4) maxAlign = Math.Max(maxAlign, 4);
            else if (size >= 2) maxAlign = Math.Max(maxAlign, 2);
        }
        return maxAlign;
    }
}
