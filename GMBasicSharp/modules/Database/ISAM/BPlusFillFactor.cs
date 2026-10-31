// FILE: Basic.ISAM.BPlusFillFactor

namespace Basic.ISAM;

/// <summary>
/// Fill factor B+ (ISAM-C6): split a 2/3 invece di 50/50.
/// </summary>
internal static class BPlusFillFactor
{
    internal const int SplitNumerator = 667;
    internal const int SplitDenominator = 1000;

    /// <summary>Indice split foglia (~67% resta a sinistra).</summary>
    internal static int ComputeLeafSplitIndex(int keyCount) =>
        ClampSplitIndex(keyCount, keyCount - 1);

    /// <summary>Indice chiave promossa nodo interno (almeno 1 chiave per lato).</summary>
    internal static int ComputeInternalSplitIndex(int keyCount) =>
        ClampSplitIndex(keyCount, Math.Max(1, keyCount - 2));

    private static int ClampSplitIndex(int keyCount, int maxIndex)
    {
        if (keyCount <= 1)
        {
            return 1;
        }

        var target = keyCount * SplitNumerator / SplitDenominator;
        return Math.Clamp(Math.Max(1, target), 1, maxIndex);
    }

    /// <summary>Minimo chiavi per nodo non-radice (~33% capacità).</summary>
    internal static int ComputeMinKeys(int maxKeys) => Math.Max(1, maxKeys * (SplitDenominator - SplitNumerator) / SplitDenominator);
}
