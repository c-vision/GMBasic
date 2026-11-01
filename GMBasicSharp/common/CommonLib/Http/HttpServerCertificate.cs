// FILE: CommonLib.Http.HttpServerCertificate.cs
// Caricamento del certificato del server TLS: UNA implementazione per le gambe managed
// (PCode e IL), che prima accettavano SOLO .pfx/PKCS#12 mentre le gambe native (VM/C,
// C99Native, OpenSSL) accettano anche il PEM (certificato + chiave privata). Risultato del
// difetto: con `CertificatePath = "cert.pem"` il server PCode moriva con "ASN1 corrupted data"
// e quello IL restava addirittura *plain* — un client TLS non connetteva.
//
// Regole:
//   - .pfx/.p12 (o estensione sconosciuta) → loader PKCS#12 (percorso storico);
//   - .pem/.crt/.cer con CERTIFICATO e CHIAVE nello stesso file → CreateFromPemFile(path);
//   - altrimenti si cerca la chiave accanto: `<stem>-key.pem`, `<stem>.key`, `key.pem`.

using System;
using System.IO;
using System.Security.Cryptography.X509Certificates;

namespace CommonLib.Http;

/// <summary>Certificato server TLS da PEM o PFX (stessa accettazione delle gambe native).</summary>
public static class HttpServerCertificate
{
    /// <summary>Ritorna il certificato o null se il file non è leggibile/non è un certificato.</summary>
    public static X509Certificate2? Load(string? path, string? password)
    {
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) return null;
        try
        {
            var isPem = path.EndsWith(".pem", StringComparison.OrdinalIgnoreCase)
                        || path.EndsWith(".crt", StringComparison.OrdinalIgnoreCase)
                        || path.EndsWith(".cer", StringComparison.OrdinalIgnoreCase);

            if (!isPem) return LoadPkcs12(path, password);

            var text = File.ReadAllText(path);
            if (text.Contains("BEGIN CERTIFICATE") && text.Contains("PRIVATE KEY"))
                return X509Certificate2.CreateFromPemFile(path);

            // certificato e chiave in due file: si cerca la chiave accanto
            var dir = Path.GetDirectoryName(path);
            if (string.IsNullOrEmpty(dir)) dir = ".";
            var stem = Path.GetFileNameWithoutExtension(path);
            foreach (var candidate in new[] { stem + "-key.pem", stem + ".key", stem + "-key", "key.pem", "privkey.pem" })
            {
                var keyPath = Path.Combine(dir, candidate);
                if (File.Exists(keyPath)) return X509Certificate2.CreateFromPemFile(path, keyPath);
            }

            // ultimo tentativo: potrebbe essere un PKCS#12 con estensione .pem
            return LoadPkcs12(path, password);
        }
        catch
        {
            return null;
        }
    }

    private static X509Certificate2 LoadPkcs12(string path, string? password)
        => string.IsNullOrEmpty(password)
            ? X509CertificateLoader.LoadPkcs12FromFile(path, null)
            : X509CertificateLoader.LoadPkcs12FromFile(path, password);
}
