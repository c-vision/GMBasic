/* FILE: native/gmb_db_c/src/ISAM/BPlusFillFactor.c */
/* Port 1:1 di modules/Database/ISAM/BPlusFillFactor.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.BPlusFillFactor
//
//namespace Basic.ISAM;
//
///// <summary>
///// Fill factor B+ (ISAM-C6): split a 2/3 invece di 50/50.
///// </summary>
//internal static class BPlusFillFactor
//{
//    internal const int SplitNumerator = 667;
//    internal const int SplitDenominator = 1000;
//
//    /// <summary>Indice split foglia (~67% resta a sinistra).</summary>
//    internal static int ComputeLeafSplitIndex(int keyCount) =>
//        ClampSplitIndex(keyCount, keyCount - 1);
//
//    /// <summary>Indice chiave promossa nodo interno (almeno 1 chiave per lato).</summary>
//    internal static int ComputeInternalSplitIndex(int keyCount) =>
//        ClampSplitIndex(keyCount, Math.Max(1, keyCount - 2));
//
//    private static int ClampSplitIndex(int keyCount, int maxIndex)
//    {
//        if (keyCount <= 1)
//        {
//            return 1;
//        }
//
//        var target = keyCount * SplitNumerator / SplitDenominator;
//        return Math.Clamp(Math.Max(1, target), 1, maxIndex);
//    }
//
//    /// <summary>Minimo chiavi per nodo non-radice (~33% capacità).</summary>
//    internal static int ComputeMinKeys(int maxKeys) => Math.Max(1, maxKeys * (SplitDenominator - SplitNumerator) / SplitDenominator);
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal static class BPlusFillFactor */
enum {
    BPlusFillFactor_SplitNumerator = 667,
    BPlusFillFactor_SplitDenominator = 1000
};

static int BPlusFillFactor_ClampSplitIndex(int keyCount, int maxIndex)
{
    int target;
    if (keyCount <= 1) return 1;
    target = keyCount * BPlusFillFactor_SplitNumerator / BPlusFillFactor_SplitDenominator;
    if (target < 1) target = 1;
    if (target > maxIndex) target = maxIndex;
    return target;
}

/* Indice split foglia (~67% resta a sinistra). */
int BPlusFillFactor_ComputeLeafSplitIndex(int keyCount)
{
    return BPlusFillFactor_ClampSplitIndex(keyCount, keyCount - 1);
}

/* Indice chiave promossa nodo interno (almeno 1 chiave per lato). */
int BPlusFillFactor_ComputeInternalSplitIndex(int keyCount)
{
    int maxIndex = keyCount - 2;
    if (maxIndex < 1) maxIndex = 1;
    return BPlusFillFactor_ClampSplitIndex(keyCount, maxIndex);
}

/* Minimo chiavi per nodo non-radice (~33% capacità). */
int BPlusFillFactor_ComputeMinKeys(int maxKeys)
{
    int n = maxKeys * (BPlusFillFactor_SplitDenominator - BPlusFillFactor_SplitNumerator)
        / BPlusFillFactor_SplitDenominator;
    return n < 1 ? 1 : n;
}

