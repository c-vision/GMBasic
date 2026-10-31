// FILE: CommonLib.CompressionUtils.cs
// Core per il namespace BASIC COMPRESSION: ZIP/UNZIP (archivi) e compressione
// di stringhe con tutti gli algoritmi .NET disponibili (GZIP, DEFLATE, BROTLI,
// ZLIB) più BZIP2 (BouncyCastle). I dati sono trasferiti in BASIC come stringhe
// Base64 (per il caso testo) o come byte[].

using System.IO.Compression;
using System.Text;
using Org.BouncyCastle.Utilities.Bzip2;

namespace CommonLib;

public static class CompressionUtils
{
    /// <summary>Elenco degli algoritmi supportati (separato da virgole).</summary>
    public const string SupportedAlgorithms = "GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ";

    // ------------------------------------------------------------------
    // GZIP
    // ------------------------------------------------------------------
    public static byte[] Gzip(byte[] data)
    {
        using var ms = new MemoryStream();
        using (var gz = new GZipStream(ms, CompressionMode.Compress, leaveOpen: true))
            gz.Write(data, 0, data.Length);
        return ms.ToArray();
    }

    public static byte[] Ungzip(byte[] data)
    {
        using var ms = new MemoryStream(data);
        using var gz = new GZipStream(ms, CompressionMode.Decompress);
        using var outMs = new MemoryStream();
        gz.CopyTo(outMs);
        return outMs.ToArray();
    }

    // ------------------------------------------------------------------
    // DEFLATE
    // ------------------------------------------------------------------
    public static byte[] Deflate(byte[] data)
    {
        using var ms = new MemoryStream();
        using (var df = new DeflateStream(ms, CompressionMode.Compress, leaveOpen: true))
            df.Write(data, 0, data.Length);
        return ms.ToArray();
    }

    public static byte[] Undeflate(byte[] data)
    {
        using var ms = new MemoryStream(data);
        using var df = new DeflateStream(ms, CompressionMode.Decompress);
        using var outMs = new MemoryStream();
        df.CopyTo(outMs);
        return outMs.ToArray();
    }

    // ------------------------------------------------------------------
    // BROTLI
    // ------------------------------------------------------------------
    public static byte[] Brotli(byte[] data)
    {
        using var ms = new MemoryStream();
        using (var br = new BrotliStream(ms, CompressionMode.Compress, leaveOpen: true))
            br.Write(data, 0, data.Length);
        return ms.ToArray();
    }

    public static byte[] Unbrotli(byte[] data)
    {
        using var ms = new MemoryStream(data);
        using var br = new BrotliStream(ms, CompressionMode.Decompress);
        using var outMs = new MemoryStream();
        br.CopyTo(outMs);
        return outMs.ToArray();
    }

    // ------------------------------------------------------------------
    // ZLIB
    // ------------------------------------------------------------------
    public static byte[] Zlib(byte[] data)
    {
        using var ms = new MemoryStream();
        using (var zl = new ZLibStream(ms, CompressionMode.Compress, leaveOpen: true))
            zl.Write(data, 0, data.Length);
        return ms.ToArray();
    }

    public static byte[] Unzlib(byte[] data)
    {
        using var ms = new MemoryStream(data);
        using var zl = new ZLibStream(ms, CompressionMode.Decompress);
        using var outMs = new MemoryStream();
        zl.CopyTo(outMs);
        return outMs.ToArray();
    }

    // ------------------------------------------------------------------
    // BZIP2 (BouncyCastle)
    // ------------------------------------------------------------------
    public static byte[] Bzip2(byte[] data)
    {
        using var ms = new MemoryStream();
        var bz = new CBZip2OutputStream(ms);
        bz.Write(data, 0, data.Length);
        bz.Finish();
        bz.Close();
        return ms.ToArray();
    }

    public static byte[] Bunzip2(byte[] data)
    {
        using var ms = new MemoryStream(data);
        var bz = new CBZip2InputStream(ms);
        using var outMs = new MemoryStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = bz.Read(buf, 0, buf.Length)) > 0)
            outMs.Write(buf, 0, n);
        return outMs.ToArray();
    }

    // ------------------------------------------------------------------
    // Dispatch per algoritmo
    // ------------------------------------------------------------------
    public static byte[] Compress(byte[] data, string algorithm)
    {
        switch (algorithm.Trim().ToUpperInvariant())
        {
            case "GZIP": return Gzip(data);
            case "DEFLATE": return Deflate(data);
            case "BROTLI": return Brotli(data);
            case "ZLIB": return Zlib(data);
            case "BZIP2": return Bzip2(data);
            default:
                throw new ArgumentException($"Algoritmo di compressione sconosciuto: '{algorithm}'. Usare uno tra: {SupportedAlgorithms}.");
        }
    }

    public static byte[] Decompress(byte[] data, string algorithm)
    {
        switch (algorithm.Trim().ToUpperInvariant())
        {
            case "GZIP": return Ungzip(data);
            case "DEFLATE": return Undeflate(data);
            case "BROTLI": return Unbrotli(data);
            case "ZLIB": return Unzlib(data);
            case "BZIP2": return Bunzip2(data);
            default:
                throw new ArgumentException($"Algoritmo di decompressione sconosciuto: '{algorithm}'. Usare uno tra: {SupportedAlgorithms}.");
        }
    }

    /// <summary>Comprime una stringa UTF-8 e la restituisce come Base64.</summary>
    public static string CompressString(string text, string algorithm)
    {
        var a = (algorithm ?? "").Trim().ToUpperInvariant();
        // ZSTD/LZ4/XZ: delegato alla sorgente C condivisa (una sola implementazione).
        if (a is "ZSTD" or "LZ4" or "XZ" or "LZMA")
            return NativeCompress.CompressB64(a is "LZMA" ? "xz" : a.ToLowerInvariant(), text ?? "");
        return Convert.ToBase64String(Compress(Encoding.UTF8.GetBytes(text ?? ""), algorithm));
    }

    /// <summary>Decomprime una stringa Base64 compressa e restituisce il testo UTF-8.</summary>
    public static string DecompressString(string data, string algorithm)
    {
        var a = (algorithm ?? "").Trim().ToUpperInvariant();
        if (a is "ZSTD" or "LZ4" or "XZ" or "LZMA")
            return NativeCompress.DecompressB64(a is "LZMA" ? "xz" : a.ToLowerInvariant(), data ?? "");
        return Encoding.UTF8.GetString(Decompress(Convert.FromBase64String(data ?? ""), algorithm));
    }

    // ------------------------------------------------------------------
    // Archivi ZIP
    // ------------------------------------------------------------------

    /// <summary>Crea un archivio ZIP dai percorsi indicati (file o cartelle, separati da ';').</summary>
    public static bool CreateZip(string zipPath, IEnumerable<string> files)
    {
        if (string.IsNullOrWhiteSpace(zipPath)) throw new ArgumentException("Percorso ZIP mancante.");
        var entries = new List<string>();
        foreach (var f in files)
        {
            if (string.IsNullOrWhiteSpace(f)) continue;
            if (File.Exists(f)) entries.Add(f);
            else if (Directory.Exists(f))
                entries.AddRange(Directory.GetFiles(f, "*", SearchOption.AllDirectories));
            else
                throw new FileNotFoundException($"File da comprimere non trovato: {f}");
        }
        var dir = Path.GetDirectoryName(Path.GetFullPath(zipPath));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        // ZipFile.Open(Create) usa FileMode.CreateNew: sovrascrivi un archivio esistente.
        if (File.Exists(zipPath)) File.Delete(zipPath);
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var f in entries)
        {
            var entryName = f;
            if (Path.IsPathRooted(entryName))
            {
                // usa il nome relativo alla radice comune se possibile
                var root = entries.Select(Path.GetDirectoryName)
                                  .Where(d => d != null)
                                  .OrderBy(d => d!.Length)
                                  .FirstOrDefault();
                if (root != null && entryName.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                    entryName = entryName[(root.Length + 1)..];
            }
            zip.CreateEntryFromFile(f, entryName.Replace('\\', '/'));
        }
        return true;
    }

    /// <summary>Estrae un archivio ZIP in una cartella di destinazione.</summary>
    public static bool ExtractZip(string zipPath, string destDir)
    {
        if (!File.Exists(zipPath)) throw new FileNotFoundException($"Archivio ZIP non trovato: {zipPath}");
        if (string.IsNullOrWhiteSpace(destDir)) throw new ArgumentException("Cartella di destinazione mancante.");
        Directory.CreateDirectory(destDir);
        ZipFile.ExtractToDirectory(zipPath, destDir, overwriteFiles: true);
        return true;
    }

    /// <summary>Elenca le voci di un archivio ZIP (una per riga).</summary>
    public static string ListZip(string zipPath)
    {
        if (!File.Exists(zipPath)) throw new FileNotFoundException($"Archivio ZIP non trovato: {zipPath}");
        using var zip = ZipFile.OpenRead(zipPath);
        return string.Join("\n", zip.Entries.Select(e => e.FullName));
    }
}
