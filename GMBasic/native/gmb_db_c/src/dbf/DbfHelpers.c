/* FILE: native/gmb_db_c/src/DBF/DbfHelpers.c */
/* Port 1:1 di modules/Database/DBF/DbfHelpers.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfHelpers.cs
//
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic.DBF;
//
///// <summary>
///// Fornisce metodi helper per la normalizzazione delle chiavi di indice (collation).
///// </summary>
//internal static class CollationHelper
//{
//    private static readonly byte[] UpperAscii = BuildUpperAscii();
//
//    /// <summary>
//    /// Converte una chiave sorgente in un formato normalizzato per l'indice,
//    /// gestendo il padding e la conversione a maiuscolo.
//    /// </summary>
//    /// <param name="src">I byte della chiave originale.</param>
//    /// <param name="dst">Lo span di destinazione in cui scrivere la chiave normalizzata.</param>
//    /// <param name="caseInsensitive">Se true, converte i caratteri in maiuscolo.</param>
//    /// <param name="fixedLen">La lunghezza fissa della chiave dell'indice.</param>
//    public static void ToIndexKey(ReadOnlySpan<byte> src, Span<byte> dst, bool caseInsensitive, int fixedLen)
//    {
//        var n = Math.Min(src.Length, fixedLen);
//        for (var i = 0; i < n; i++)
//        {
//            var b = src[i];
//            dst[i] = caseInsensitive ? UpperAscii[b] : b;
//        }
//        // Riempie il resto con spazi (padding)
//        for (var i = n; i < fixedLen; i++)
//        {
//            dst[i] = (byte)' ';
//        }
//    }
//
//    /// <summary>
//    /// Pre-calcola una tabella di lookup per la conversione veloce a maiuscolo.
//    /// </summary>
//    private static byte[] BuildUpperAscii()
//    {
//        var t = new byte[256];
//        for (var i = 0; i < 256; i++)
//        {
//            t[i] = (byte)char.ToUpperInvariant((char)i);
//        }
//        return t;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal static class CollationHelper */
static unsigned char CollationHelper_UpperAscii[256];
static int CollationHelper_UpperReady;

/* Pre-calcola una tabella di lookup per la conversione veloce a maiuscolo. */
static void CollationHelper_BuildUpperAscii(void)
{
    int i;
    if (CollationHelper_UpperReady) return;
    for (i = 0; i < 256; i++)
        CollationHelper_UpperAscii[i] = (unsigned char)toupper((unsigned char)i);
    CollationHelper_UpperReady = 1;
}

/* Converte una chiave sorgente in un formato normalizzato per l'indice,
 * gestendo il padding e la conversione a maiuscolo. */
void CollationHelper_ToIndexKey(const unsigned char* src, int srcLen,
                                unsigned char* dst, int caseInsensitive, int fixedLen)
{
    int n, i;
    CollationHelper_BuildUpperAscii();
    n = srcLen < fixedLen ? srcLen : fixedLen;
    for (i = 0; i < n; i++)
        dst[i] = caseInsensitive ? CollationHelper_UpperAscii[src[i]] : src[i];
    /* Riempie il resto con spazi (padding) */
    for (i = n; i < fixedLen; i++)
        dst[i] = (unsigned char)' ';
}

