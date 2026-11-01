// FILE: CommonLib.Http.HttpCompression.cs
// Negoziato della codifica di contenuto per le gambe MANAGED (PCode, IL) — equivalente
// esatto di http_pick_encoding/http_encode_body del runtime C (§0bis: il protocollo è uno,
// le implementazioni due: C per VM/C+C99Native, questa per IL+PCode).
//
// Regole (RFC 9110 §12.5.3): si sceglie la codifica MIGLIORE offerta dal client — "br" se
// offerta, altrimenti "gzip"; `q=0` è un RIFIUTO esplicito (non va usata), "*" vale per tutte
// le codifiche non elencate. La compressione si applica solo al testo e solo sopra 1 KB.

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;

namespace CommonLib.Http;

/// <summary>Negoziazione e applicazione di Content-Encoding (br/gzip) sulle risposte.</summary>
public static class HttpCompression
{
    /// <summary>Codifica scelta ("br" | "gzip") o null se il client non ne offre/utilizza nessuna.</summary>
    public static string? PickEncoding(string? acceptEncoding)
    {
        if (string.IsNullOrWhiteSpace(acceptEncoding)) return null;

        var br = false;
        var gzip = false;
        var star = false;

        foreach (var raw in acceptEncoding.Split(','))
        {
            var part = raw.Trim();
            if (part.Length == 0) continue;

            var semi = part.IndexOf(';');
            var name = (semi >= 0 ? part[..semi] : part).Trim();
            var q = 1.0;
            if (semi >= 0)
            {
                foreach (var prm in part[(semi + 1)..].Split(';'))
                {
                    var p = prm.Trim();
                    if (p.StartsWith("q=", StringComparison.OrdinalIgnoreCase)
                        && double.TryParse(p[2..], System.Globalization.NumberStyles.Float,
                                           System.Globalization.CultureInfo.InvariantCulture, out var parsed))
                        q = parsed;
                }
            }
            if (q <= 0.0) continue;   // rifiuto esplicito

            if (name.Equals("br", StringComparison.OrdinalIgnoreCase)) br = true;
            else if (name.Equals("gzip", StringComparison.OrdinalIgnoreCase)) gzip = true;
            else if (name == "*") star = true;
        }

        if (br) return "br";
        if (gzip || star) return "gzip";
        return null;
    }

    /// <summary>Il tipo di contenuto merita compressione? (testo/JSON/SVG/JS/XML)</summary>
    public static bool IsCompressible(string? contentType)
    {
        if (string.IsNullOrEmpty(contentType)) return false;
        return contentType.StartsWith("text/", StringComparison.OrdinalIgnoreCase)
            || contentType.Contains("json", StringComparison.OrdinalIgnoreCase)
            || contentType.Contains("javascript", StringComparison.OrdinalIgnoreCase)
            || contentType.Contains("svg", StringComparison.OrdinalIgnoreCase)
            || contentType.Contains("xml", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Comprime con l'algoritmo indicato ("br" o "gzip").</summary>
    public static byte[] Encode(string algo, byte[] data)
    {
        using var outStream = new MemoryStream();
        if (string.Equals(algo, "br", StringComparison.OrdinalIgnoreCase))
        {
            using (var br = new BrotliStream(outStream, CompressionLevel.Optimal, leaveOpen: true))
                br.Write(data, 0, data.Length);
        }
        else
        {
            using (var gz = new GZipStream(outStream, CompressionLevel.Optimal, leaveOpen: true))
                gz.Write(data, 0, data.Length);
        }
        return outStream.ToArray();
    }
}
