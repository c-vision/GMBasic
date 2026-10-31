// FILE: Basic.AI.Knowledge.ProvenanceManager
// §2 del piano Second Brain — Provenance di una nota del vault: firma HMAC-SHA256 con chiave
// condivisa e verifica. Parità di output con il runtime C (gmb_provenance_sign/verify in
// gmb_crypto.c, hex lowercase HMACSHA256) — §0bis: UNA implementazione condivisa, i backend
// C emettono la gmb_*, C# la stessa logica via System.Security.Cryptography.
// Mai stub: chiave vuota / testo vuoto → errore esplicito.
// Share(encrypt): AES-256-GCM (parità runtime C gmb_aes_encrypt_password) — non più placeholder.
// Gap documentati (non stub): SetPermissions, DAO.Propose (fuori scope runtime condiviso).
using System.Security.Cryptography;
using System.Text;

namespace Basic.AI.Knowledge;

/// <summary>Sottostrato crittografico della provenance: firma HMAC-SHA256 con chiave condivisa
/// → hex lowercase, e verifica a confronto costante. Stesso output di gmb_provenance_sign/
/// verify del runtime C (parità verificata).</summary>
public static class ProvenanceCrypto
{
    /// <summary>Firma HMAC-SHA256 del contenuto con la chiave condivisa → hex lowercase.
    /// Stesso output di gmb_provenance_sign del runtime C.</summary>
    public static string Sign(string content, string key)
    {
        ArgumentException.ThrowIfNullOrEmpty(content);
        ArgumentException.ThrowIfNullOrEmpty(key);
        using var hmac = new HMACSHA256(Encoding.UTF8.GetBytes(key));
        var mac = hmac.ComputeHash(Encoding.UTF8.GetBytes(content));
        return Convert.ToHexString(mac).ToLowerInvariant();
    }

    /// <summary>Verifica la firma hex (confronto costante). Stesso risultato di
    /// gmb_provenance_verify del runtime C.</summary>
    public static bool Verify(string content, string key, string signatureHex)
    {
        ArgumentException.ThrowIfNullOrEmpty(content);
        ArgumentException.ThrowIfNullOrEmpty(key);
        if (string.IsNullOrEmpty(signatureHex)) return false;
        var expected = Sign(content, key);
        return FixedTimeEquals(expected, signatureHex);
    }

    /// <summary>Confronto costante nel tempo (niente early-exit sulla lunghezza).</summary>
    private static bool FixedTimeEquals(string a, string b)
    {
        var na = a.Length;
        var nb = b.Length;
        var n = Math.Max(na, nb);
        uint diff = 0;
        for (var i = 0; i < n; i++)
        {
            var x = (uint)(i < na ? a[i] : 0);
            var y = (uint)(i < nb ? b[i] : 0);
            diff |= x ^ y;
        }
        return diff == 0;
    }
}

/// <summary>Provenance di dominio: firma/verifica di una NOTA nel vault (path relativo) con
/// un'identità (chiave). Il sottostrato crittografico (ProvenanceCrypto) ha parità con il
/// runtime C.</summary>
public static class ProvenanceManager
{
    /// <summary>Firma una nota del vault: ritorna la firma hex del suo contenuto con la chiave
    /// dell'identità. La nota deve esistere nel vault del brain attivo (NoteManager).</summary>
    public static string SignNote(string vault, string notePath, string identityKey)
    {
        var content = NoteManager.GetContentByPath(vault, notePath);
        if (content is null) throw new FileNotFoundException($"nota non trovata nel vault: {notePath}");
        return ProvenanceCrypto.Sign(content, identityKey);
    }

    /// <summary>Verifica la firma di una nota del vault (true se il contenuto attuale produce
    /// esattamente la firma data con la chiave).</summary>
    public static bool VerifyNote(string vault, string notePath, string identityKey, string signatureHex)
    {
        var content = NoteManager.GetContentByPath(vault, notePath);
        if (content is null) return false;
        return ProvenanceCrypto.Verify(content, identityKey, signatureHex);
    }

    /// <summary>Cifra un testo per la condivisione con AES-256-GCM (parità col runtime C
    /// gmb_aes_encrypt_password). Il risultato è base64 di [salt|nonce|tag|ciphertext].
    /// Stessa chiave cifra/decifra — la chiave è l'identità condivisa tra mittente e destinatario.</summary>
    public static string ShareEncrypt(string content, string identityKey)
    {
        ArgumentException.ThrowIfNullOrEmpty(content);
        ArgumentException.ThrowIfNullOrEmpty(identityKey);
        var enc = CommonLib.CryptoUtils.CoreAesEncrypt(Encoding.UTF8.GetBytes(content), identityKey);
        return Convert.ToBase64String(enc);
    }

    /// <summary>Decifra un testo cifrato da ShareEncrypt con la stessa chiave (AES-256-GCM).
    /// Password o ciphertext sbagliati → AuthenticationTagMismatchException esplicita.</summary>
    public static string ShareDecrypt(string cipherBase64, string identityKey)
    {
        ArgumentException.ThrowIfNullOrEmpty(cipherBase64);
        ArgumentException.ThrowIfNullOrEmpty(identityKey);
        var plain = CommonLib.CryptoUtils.CoreAesDecrypt(Convert.FromBase64String(cipherBase64), identityKey);
        return Encoding.UTF8.GetString(plain);
    }
}
