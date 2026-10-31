// FILE: Basic.DBF.DbfHelpers.cs


// ReSharper disable InconsistentNaming

namespace Basic.DBF;

/// <summary>
/// Fornisce metodi helper per la normalizzazione delle chiavi di indice (collation).
/// </summary>
internal static class CollationHelper
{
    private static readonly byte[] UpperAscii = BuildUpperAscii();

    /// <summary>
    /// Converte una chiave sorgente in un formato normalizzato per l'indice,
    /// gestendo il padding e la conversione a maiuscolo.
    /// </summary>
    /// <param name="src">I byte della chiave originale.</param>
    /// <param name="dst">Lo span di destinazione in cui scrivere la chiave normalizzata.</param>
    /// <param name="caseInsensitive">Se true, converte i caratteri in maiuscolo.</param>
    /// <param name="fixedLen">La lunghezza fissa della chiave dell'indice.</param>
    public static void ToIndexKey(ReadOnlySpan<byte> src, Span<byte> dst, bool caseInsensitive, int fixedLen)
    {
        var n = Math.Min(src.Length, fixedLen);
        for (var i = 0; i < n; i++)
        {
            var b = src[i];
            dst[i] = caseInsensitive ? UpperAscii[b] : b;
        }
        // Riempie il resto con spazi (padding)
        for (var i = n; i < fixedLen; i++)
        {
            dst[i] = (byte)' ';
        }
    }

    /// <summary>
    /// Pre-calcola una tabella di lookup per la conversione veloce a maiuscolo.
    /// </summary>
    private static byte[] BuildUpperAscii()
    {
        var t = new byte[256];
        for (var i = 0; i < 256; i++)
        {
            t[i] = (byte)char.ToUpperInvariant((char)i);
        }
        return t;
    }
}