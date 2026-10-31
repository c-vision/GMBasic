using System.Text;

namespace CommonLib;

/// <summary>
/// Core condiviso JWT HS256 (Security P2, 2026-09-02). Funzioni pure di composizione sulle
/// primitive esistenti (HMAC-SHA256 + base64url RFC 4648 §5, niente padding). Parità con le
/// corrispondenti gmb_jwt_* del runtime C. I backend chiamano questi core, mai inline.
/// Formato token: header.payload.signature — header fisso {"alg":"HS256","typ":"JWT"}.
/// </summary>
public static class Jwt
{
    private const string Header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";

    /// <summary>Crea un token JWT HS256 firmato. `claims` è testo JSON (es. JSON.STRINGIFY).</summary>
    public static string Create(string claims, string secret)
    {
        var h = CryptoUtils.CoreToBase64Url(Encoding.UTF8.GetBytes(Header));
        var p = CryptoUtils.CoreToBase64Url(Encoding.UTF8.GetBytes(claims ?? ""));
        var input = h + "." + p;
        var sig = CryptoUtils.CoreToBase64Url(CryptoUtils.CoreHmac("SHA256",
            Encoding.UTF8.GetBytes(secret ?? ""), Encoding.UTF8.GetBytes(input)));
        return input + "." + sig;
    }

    /// <summary>True se token ha 3 parti e la firma HS256 ricalcolata coincide (parità C).</summary>
    public static bool Verify(string token, string secret)
    {
        var parts = Split(token);
        if (parts == null) return false;
        var expected = CryptoUtils.CoreToBase64Url(CryptoUtils.CoreHmac("SHA256",
            Encoding.UTF8.GetBytes(secret ?? ""), Encoding.UTF8.GetBytes(parts[0] + "." + parts[1])));
        return string.Equals(parts[2], expected, StringComparison.Ordinal);
    }

    /// <summary>Decodifica il payload (parte 2) in testo UTF-8, senza verifica (per ispezione).
    /// Token malformato → stringa vuota.</summary>
    public static string Payload(string token)
    {
        var parts = Split(token);
        if (parts == null) return "";
        try
        {
            return Encoding.UTF8.GetString(CryptoUtils.CoreFromBase64Url(parts[1]));
        }
        catch
        {
            return "";
        }
    }

    private static string[]? Split(string token)
    {
        if (string.IsNullOrEmpty(token)) return null;
        var parts = token.Split('.');
        if (parts.Length != 3) return null;
        if (parts[0].Length == 0 || parts[1].Length == 0 || parts[2].Length == 0) return null;
        return parts;
    }
}
