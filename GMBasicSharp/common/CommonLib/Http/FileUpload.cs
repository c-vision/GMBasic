namespace CommonLib.Http;

/// <summary>
/// Core condiviso di validazione file upload (Security P2 prep, 2026-09-02). Funzioni pure
/// senza stato: whitelist estensione, sniffing magic-number, nome file sicuro. I futuri builtin
/// (o librerie HTTP/MVC) chiameranno questi core — mai logica inline nei backend. Parità con il
/// runtime C previsto (gmb_*) quando verrà implementato.
/// </summary>
public static class FileUpload
{
    /// <summary>True se l'estensione del file è nella whitelist (case-insensitive).</summary>
    public static bool HasAllowedExtension(string fileName, IEnumerable<string> allowedExtensions)
    {
        var ext = Path.GetExtension(fileName ?? "");
        if (string.IsNullOrEmpty(ext)) return false;
        var norm = ext.TrimStart('.').ToLowerInvariant();
        return allowedExtensions.Any(e => e.TrimStart('.').Equals(norm, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>Sniffa il content-type dai primi byte (magic number). Ritorna null se sconosciuto.</summary>
    public static string? DetectContentType(byte[] head)
    {
        if (head == null || head.Length == 0) return null;
        if (StartsWith(head, [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])) return "image/png";
        if (StartsWith(head, [0xFF, 0xD8, 0xFF])) return "image/jpeg";
        if (StartsWithAscii(head, "GIF87a") || StartsWithAscii(head, "GIF89a")) return "image/gif";
        if (StartsWithAscii(head, "%PDF-")) return "application/pdf";
        if (StartsWith(head, [0x50, 0x4B, 0x03, 0x04])) return "application/zip";
        if (StartsWith(head, [0x1F, 0x8B])) return "application/gzip";
        return null;
    }

    /// <summary>Sanifica un nome file: rimuove separatori di path e risalite, limita lunghezza.
    /// Se il nome risultante è vuoto restituisce "file".</summary>
    public static string SecureFileName(string fileName)
    {
        var name = (fileName ?? "").Replace('\\', '/');
        var slash = name.LastIndexOf('/');
        if (slash >= 0) name = name[(slash + 1)..];
        foreach (var bad in new[] { "..", "/", ":", "*", "?", "\"", "<", ">", "|" })
            name = name.Replace(bad, "");
        name = name.Trim();
        if (name.Length == 0) name = "file";
        if (name.Length > 120) name = name[..120];
        return name;
    }

    private static bool StartsWith(byte[] data, byte[] magic)
    {
        if (data.Length < magic.Length) return false;
        for (var i = 0; i < magic.Length; i++)
            if (data[i] != magic[i]) return false;
        return true;
    }

    private static bool StartsWithAscii(byte[] data, string ascii)
    {
        if (data.Length < ascii.Length) return false;
        for (var i = 0; i < ascii.Length; i++)
            if (data[i] != (byte)ascii[i]) return false;
        return true;
    }
}
