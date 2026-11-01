// FILE: CommonLib.Http.HttpRange.cs
// Range HTTP (RFC 9110 §14) per le gambe MANAGED (PCode e IL): equivalente esatto di
// gmb_http_parse_range del runtime C (§0bis — il protocollo è uno, le implementazioni due:
// C per le gambe native, questa per quelle managed).
//
// Perché serve: senza Range un client non può riprendere un download interrotto né cercare
// dentro un video. Il server deve mandare solo la fetta richiesta con 206 + Content-Range,
// dichiarare Accept-Ranges: bytes e rispondere 416 quando la fetta è fuori dal file.

using System;
using System.IO;

namespace CommonLib.Http;

/// <summary>Esito del parse di un header <c>Range</c>.</summary>
public enum HttpRangeKind
{
    /// <summary>Nessun range utilizzabile (header assente, malformato o multi-range): corpo intero, 200.</summary>
    None,
    /// <summary>Range valido: <c>start..end</c> inclusivi, già clampati dentro il file.</summary>
    Valid,
    /// <summary>Non soddisfacibile: <c>start &gt;= total</c> → 416 con <c>Content-Range: bytes */total</c>.</summary>
    Unsatisfiable,
}

/// <summary>
/// Parse di <c>Range: bytes=…</c> e lettura della fetta. Stesse regole del runtime C
/// (<c>gmb_http_parse_range</c>): sono supportate le tre forme reali usate dai client
/// (<c>0-99</c>, <c>100-</c>, <c>-50</c>), il multi-range è rifiutato (corpo intero) e uno start
/// oltre la fine è "non soddisfacibile".
/// </summary>
public static class HttpRange
{
    public static HttpRangeKind Parse(string? header, long total, out long start, out long end)
    {
        start = 0;
        end = 0;
        if (string.IsNullOrEmpty(header) || total <= 0) return HttpRangeKind.None;

        var p = header.AsSpan().TrimStart(" \t");
        if (p.Length < 5 || !p[..5].Equals("bytes", StringComparison.OrdinalIgnoreCase)) return HttpRangeKind.None;
        p = p[5..].TrimStart(" \t=");

        // Suffisso: gli ULTIMI n byte ("bytes=-50").
        if (p.Length > 0 && p[0] == '-')
        {
            p = p[1..];
            if (p.Length == 0 || !char.IsAsciiDigit(p[0])) return HttpRangeKind.None;
            var (n, okN) = ReadLong(p, out var restN);
            restN = restN.TrimStart(" ");
            if (!okN || (restN.Length > 0 && restN[0] != ',')) return HttpRangeKind.None;
            if (n <= 0) return HttpRangeKind.None;
            if (n > total) n = total;
            start = total - n;
            end = total - 1;
            return HttpRangeKind.Valid;
        }

        if (p.Length == 0 || !char.IsAsciiDigit(p[0])) return HttpRangeKind.None;
        var (s, okS) = ReadLong(p, out var rest);
        if (!okS || rest.Length == 0 || rest[0] != '-') return HttpRangeKind.None;
        rest = rest[1..];

        long e;
        if (rest.Length > 0 && char.IsAsciiDigit(rest[0]))
        {
            var (parsed, okE) = ReadLong(rest, out var restE);
            restE = restE.TrimStart(" ");
            if (!okE || (restE.Length > 0 && restE[0] != ',')) return HttpRangeKind.None;
            if (parsed < s) return HttpRangeKind.None;
            e = parsed;
        }
        else
        {
            rest = rest.TrimStart(" ");
            if (rest.Length > 0 && rest[0] != ',') return HttpRangeKind.None;
            e = total - 1;
        }

        if (s >= total) return HttpRangeKind.Unsatisfiable;
        if (e >= total) e = total - 1;
        start = s;
        end = e;
        return HttpRangeKind.Valid;
    }

    /// <summary>Legge solo la fetta richiesta (nessuna copia del file intero in memoria).</summary>
    public static byte[] ReadSlice(string path, long start, long count)
    {
        if (count <= 0) return [];
        var buffer = new byte[count];
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 64 * 1024);
        if (start > 0) fs.Seek(start, SeekOrigin.Begin);
        var read = 0;
        while (read < buffer.Length)
        {
            var n = fs.Read(buffer, read, buffer.Length - read);
            if (n <= 0) break;
            read += n;
        }
        if (read == buffer.Length) return buffer;
        var trimmed = new byte[read];
        Array.Copy(buffer, trimmed, read);
        return trimmed;
    }

    private static (long value, bool ok) ReadLong(ReadOnlySpan<char> text, out ReadOnlySpan<char> rest)
    {
        var i = 0;
        long value = 0;
        while (i < text.Length && char.IsAsciiDigit(text[i]))
        {
            // Saturazione: un numero enorme non deve andare in overflow (diventerebbe un range
            // "negativo" o un 416 sbagliato).
            if (value > (long.MaxValue - 9) / 10) { i++; continue; }
            value = value * 10 + (text[i] - '0');
            i++;
        }
        rest = text[i..];
        return (value, i > 0);
    }
}
