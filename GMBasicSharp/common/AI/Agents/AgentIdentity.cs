// FILE: Basic.Agents.AgentIdentity
// B5.1 — AgentIdentity: WITH KEYPAIR (ECDSA secp256k1) + SIGN/VERIFY su AgentRecord.
// Riusa CommonLib.CryptoUtils (GenerateKeyPair ECDSA secp256k1, SignData/VerifySignature,
// PEM export/import). Mai stub: errori espliciti.

using System.Security.Cryptography;
using System.Text;
using CommonLib;

namespace Basic.Agents;

public static class AgentIdentity
{
    public const string Curve = "secp256k1";

    /// <summary>Genera una coppia ECDSA secp256k1 e la imposta su un AgentRecord
    /// (PublicKey/PrivateKey PEM). Ritorna la public key PEM.</summary>
    public static string AttachKeypair(AgentRecord agent)
    {
        ArgumentNullException.ThrowIfNull(agent);
        var (priv, pub) = CryptoUtils.GenerateKeyPair("ECDSA", null, Curve);
        var privPem = CryptoUtils.ExportKeyToPem(priv, includePrivate: true);
        var pubPem = CryptoUtils.ExportKeyToPem(pub, includePrivate: false);
        agent.PrivateKey = privPem;
        agent.PublicKey = pubPem;
        return pubPem;
    }

    /// <summary>Genera una coppia e ritorna (privatePem, publicPem).</summary>
    public static (string privPem, string pubPem) CreateKeypair()
    {
        var (priv, pub) = CryptoUtils.GenerateKeyPair("ECDSA", null, Curve);
        return (CryptoUtils.ExportKeyToPem(priv, includePrivate: true),
                CryptoUtils.ExportKeyToPem(pub, includePrivate: false));
    }

    /// <summary>Firma il messaggio con la chiave privata dell'agente. Ritorna firma base64.</summary>
    public static string Sign(AgentRecord agent, string message)
    {
        ArgumentNullException.ThrowIfNull(agent);
        if (string.IsNullOrWhiteSpace(agent.PrivateKey))
            throw new InvalidOperationException($"Agent '{agent.Name}' non ha chiave privata (CREATE ... WITH KEYPAIR).");
        var keys = CryptoUtils.ImportKeyFromPem(agent.PrivateKey);
        var priv = keys.FirstOrDefault(k => k.IsPrivate)
            ?? throw new InvalidOperationException("PEM privata non contiene una chiave privata.");
        var sig = CryptoUtils.SignData(Encoding.UTF8.GetBytes(message), priv, "SHA256withECDSA");
        return Convert.ToBase64String(sig);
    }

    /// <summary>Verifica la firma con la chiave pubblica dell'agente.</summary>
    public static bool Verify(AgentRecord agent, string message, string signatureBase64)
    {
        ArgumentNullException.ThrowIfNull(agent);
        if (string.IsNullOrWhiteSpace(agent.PublicKey)) return false;
        try
        {
            var keys = CryptoUtils.ImportKeyFromPem(agent.PublicKey);
            var pub = keys.FirstOrDefault(k => !k.IsPrivate);
            if (pub is null) return false;
            var sig = Convert.FromBase64String(signatureBase64);
            return CryptoUtils.VerifySignature(Encoding.UTF8.GetBytes(message), sig, pub, "SHA256withECDSA");
        }
        catch { return false; }
    }
}
