// FILE: Interpreter.Parser.PackageBuilder.cs
// Strumenti di CREAZIONE/DISTRIBUZIONE pacchetti (gap pip/cargo):
//   - Init: scaffold gmb-package.json v2 + src/
//   - Pack : produce MyPackage-1.2.0.gmbpkg (zip manifest + exports + binaries[backend] + ffi)
//   - Publish: carica il .gmbpkg sul registry (POST multipart + token auth)
//   - ValidateManifest: regole nome/versione/artefatti
//   - hash per-file: sha256 (lock + integrità) delegati al C (gmb_pkg_pkg_hash_files)
using System.Collections.Immutable;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace Basic;

/// <summary>Errori di validazione di un manifest.</summary>
public sealed class PackageValidationException : Exception
{
    public PackageValidationException(string message) : base(message) { }
}

public static class PackageBuilder
{
    /// <summary>Regole nome pacchetto: [a-z0-9][a-z0-9._-]* (lowercase, no spazi) oppure
    /// scope contributor "@user/name" (modello GO): "^@[a-z0-9._-]+/[a-z0-9][a-z0-9._-]*$".
    /// Lo scope è l'account registry; internamente il path cache usa SafePackageDir (no '/').</summary>
    public static readonly System.Text.RegularExpressions.Regex NameRegex =
        new("^(?:@[a-z0-9][a-z0-9._-]*\\/)?[a-z0-9][a-z0-9._-]*$", System.Text.RegularExpressions.RegexOptions.Compiled);

    /// <summary>Regole versione: SemVer 2.0 valido.</summary>
    public static bool IsValidName(string name) => NameRegex.IsMatch(name ?? "");
    public static bool IsValidVersion(string version) => Parser.PackageManager.ParseSemVer(version) != null;

    /// <summary>Valida un manifest: nome, versione, exports/binaries/ffi coerenti, edition.</summary>
    public static void ValidateManifest(PackageManifest m)
    {
        if (!IsValidName(m.Name))
            throw new PackageValidationException($"Nome pacchetto non valido: '{m.Name}' (serve [a-z0-9][a-z0-9._-]*, lowercase).");
        if (!IsValidVersion(m.Version))
            throw new PackageValidationException($"Versione non valida (SemVer 2.0): '{m.Version}'.");
        if (m.ContentType == PackageContentType.DocsOnly) return; // nessun artefatto richiesto
        if (m.Exports.Length == 0 && m.Binaries.Count == 0 && m.Ffi == null)
            throw new PackageValidationException($"Pacchetto '{m.Name}' senza alcun artefatto (exports/binaries/ffi).");
        if (m.Settings?.RequiresCommercial == true && string.IsNullOrEmpty(m.Edition) == false && m.Edition == "any")
            throw new PackageValidationException($"Pacchetto commerciale '{m.Name}' deve dichiarare un'edizione.");
    }

    /// <summary>Scaffold di un nuovo pacchetto: gmb-package.json v2 + src/.</summary>
    public static string Init(string dir, string name, string version, PackageContentType type = PackageContentType.Source,
        string edition = "Community", string? api = null)
    {
        if (!IsValidName(name)) throw new PackageValidationException($"Nome non valido: '{name}'.");
        if (!IsValidVersion(version)) throw new PackageValidationException($"Versione non valida: '{version}'.");

        Directory.CreateDirectory(dir);
        var manifest = new PackageManifest
        {
            Name = name,
            Version = version,
            Description = "",
            ContentType = type,
            Edition = edition,
            Exports = type is PackageContentType.Source or PackageContentType.Mixed or PackageContentType.DotNet or PackageContentType.VmChunk
                ? [.. new[] { "src/" }] : [],
            Dependencies = ImmutableDictionary<string, string>.Empty,
        };
        if (type == PackageContentType.CFFI && api != null)
            manifest.Ffi = new FfiSection { Api = api, Headers = [], Libs = ImmutableDictionary<string, string>.Empty, Link = null, DeclareExample = null };

        var manifestPath = Path.Combine(dir, "gmb-package.json");
        File.WriteAllText(manifestPath, JsonSerializer.Serialize(new
        {
            name = manifest.Name,
            version = manifest.Version,
            description = manifest.Description,
            contentType = manifest.ContentType.ToString().ToLowerInvariant(),
            edition = manifest.Edition,
            backends = manifest.Backends.Length > 0 ? manifest.Backends.ToArray() : null,
            exports = manifest.Exports.Length > 0 ? manifest.Exports.ToArray() : null,
            binaries = manifest.Binaries.Count > 0 ? manifest.Binaries.ToDictionary(kv => kv.Key, kv => kv.Value.Platforms ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)) : null,
            ffi = manifest.Ffi == null ? null : new { api = manifest.Ffi.Api, headers = manifest.Ffi.Headers, libs = manifest.Ffi.Libs, link = manifest.Ffi.Link, declareExample = manifest.Ffi.DeclareExample },
            settings = manifest.Settings == null ? null : new { requiresCommercial = manifest.Settings.RequiresCommercial, minGmbVersion = manifest.Settings.MinGmbVersion },
            dependencies = manifest.Dependencies.Count > 0 ? manifest.Dependencies : null,
        }, new JsonSerializerOptions { WriteIndented = true }));

        var src = Path.Combine(dir, "src");
        Directory.CreateDirectory(src);
        if (manifest.Exports.Length > 0 && !File.Exists(Path.Combine(src, "Main.bas")))
            File.WriteAllText(Path.Combine(src, "Main.bas"), $"' {name} v{version}\nPRINT \"{name} ready\"\n");

        return manifestPath;
    }

    /// <summary>Crea il .gmbpkg (zip): manifest + exports + binaries[backend] + ffi, con firma.</summary>
    public static string Pack(string dir, string outFile, IEnumerable<string>? backends = null,
        string? secret = null, string? privatePem = null)
    {
        var manifestPath = Path.Combine(dir, "gmb-package.json");
        if (!File.Exists(manifestPath)) throw new PackageValidationException($"gmb-package.json mancante in {dir}.");
        using (var doc = JsonDocument.Parse(File.ReadAllText(manifestPath)))
            ValidateManifest(Parser.PackageManager.ParseManifest(doc.RootElement));

        // FIRMA — una sola famiglia, VERIFICABILE.
        // `--key-file` = seed/chiave Ed25519 esplicita. `--key <segreto>` NON è più HMAC: una
        // firma simmetrica non è verificabile dal client (manca il segreto) e veniva mostrata come
        // "non firmato". Il segreto viene derivato in una seed Ed25519 (SHA-256) e la firma diventa
        // pubblica: chiunque può verificarla con la .gmbpkg.pub che viaggia nel pacchetto.
        if (string.IsNullOrEmpty(privatePem) && !string.IsNullOrEmpty(secret))
            privatePem = Convert.ToBase64String(SHA256.HashData(Encoding.UTF8.GetBytes(secret)));

        if (!NativePkg.Pack(dir, outFile, null))
            throw new PackageValidationException($"pack fallito per '{dir}'.");

        if (!string.IsNullOrEmpty(privatePem))
        {
            // Firma PUBBLICA (Ed25519 detached, fallback ECDSA) — in attesa del port C/monocypher,
            // le voci .gmbpkg.sig/.gmbpkg.pub si aggiungono allo zip creato dal C.
            var manifestBytes = File.ReadAllBytes(manifestPath);
            var sigB64 = SignData(manifestBytes, privatePem);
            var pubKey = GetPublicKey(privatePem);
            var noBom = new UTF8Encoding(false);
            using var zip = ZipFile.Open(outFile, ZipArchiveMode.Update);
            using (var w = new StreamWriter(zip.CreateEntry(".gmbpkg.sig").Open(), noBom)) w.Write(sigB64);
            using (var w = new StreamWriter(zip.CreateEntry(".gmbpkg.pub").Open(), noBom)) w.Write(pubKey);
        }
        return outFile;
    }

    /// <summary>Elenca i file (relativi) che `gmb pack` includerebbe — ordine deterministico.
    /// Applica manifest.files (allowlist) e manifest.ignore (esclusioni). Condiviso con `pack --dry-run`.</summary>
    public static List<string> ComputePackFiles(PackageManifest manifest, string dir, IEnumerable<string>? backends = null)
    {
        _ = manifest; // il perimetro file lo calcola il C dal manifest su disco
        var csv = backends == null ? "all" : string.Join(',', backends);
        return NativePkg.PackFiles(dir, csv); // C: gmb_pkg_pack_files (+ files/ignore)
    }

    /// <summary>Match glob per path relativi (delega al riferimento C: gmb_pkg_glob_match):
    /// * (segmento), ** (qualsiasi), ?; un pattern senza '/' matcha anche il basename.</summary>
    public static bool GlobMatch(string pattern, string relPath) => NativePkg.GlobMatch(pattern, relPath);

    /// <summary>Verifica la firma detached (.gmbpkg.sig + .gmbpkg.pub) di un .gmbpkg. True se assente o valida.</summary>
    public static bool VerifyGmbpkg(string gmbpkgPath)
    {
        using var zip = ZipFile.OpenRead(gmbpkgPath);
        var sigEntry = zip.GetEntry(".gmbpkg.sig");
        var pubEntry = zip.GetEntry(".gmbpkg.pub");
        if (sigEntry == null || pubEntry == null) return true; // non firmato → ok (integrity sha256 separata)
        using var sigReader = new StreamReader(sigEntry.Open());
        using var pubReader = new StreamReader(pubEntry.Open());
        var sigB64 = sigReader.ReadToEnd().Trim();
        var pubPem = pubReader.ReadToEnd();
        var manEntry = zip.GetEntry("gmb-package.json");
        if (manEntry == null) return false;
        using var ms = new MemoryStream();
        using (var s = manEntry.Open()) s.CopyTo(ms);
        return VerifyData(ms.ToArray(), pubPem, sigB64);
    }

    /// <summary>Verifica la firma detached di un pacchetto installato (file .gmbpkg.sig/.gmbpkg.pub in installDir).</summary>
    public static bool VerifyInstalledSignature(PackageManifest m, string installDir)
    {
        var sigPath = Path.Combine(installDir, ".gmbpkg.sig");
        var pubPath = Path.Combine(installDir, ".gmbpkg.pub");
        var manPath = Path.Combine(installDir, "gmb-package.json");
        if (!File.Exists(sigPath) || !File.Exists(pubPath)) return true; // non firmato
        return VerifyData(File.ReadAllBytes(manPath), File.ReadAllText(pubPath), File.ReadAllText(sigPath).Trim());
    }

    /// <summary>HMAC-SHA256 hex (firma simmetrica, legacy).</summary>
    public static string HmacSha256(string secret, string data)
    {
        using var h = new HMACSHA256(Encoding.UTF8.GetBytes(secret));
        return Convert.ToHexString(h.ComputeHash(Encoding.UTF8.GetBytes(data))).ToLowerInvariant();
    }

    /// <summary>Algoritmo di firma pubblica: Ed25519 (RFC 8032).</summary>
    public const string SigAlgo = "ed25519";

    /// <summary>Genera una coppia di chiavi Ed25519 (seed privato 32B / chiave pubblica 32B, base64).</summary>
    public static (string PrivateKey, string PublicKey) GenerateKeyPair()
        => NativePkg.EdKeygen() ?? throw new PackageException("generazione chiave Ed25519 fallita."); // C (OpenSSL)

    private static bool TryB64Length(string s, int len, out byte[] bytes)
    {
        bytes = [];
        try { bytes = Convert.FromBase64String(s.Trim()); return bytes.Length == len; }
        catch { return false; }
    }

    /// <summary>Firma dati → base64. privateKey = seed Ed25519 (32B b64) o PEM ECDSA (fallback).
    /// Ed25519 DELEGATO al C (gmb_pkg_ed25519_sign, OpenSSL, RFC 8032).</summary>
    public static string SignData(byte[] data, string privateKey)
    {
        if (TryB64Length(privateKey, 32, out _))
            return NativePkg.EdSign(privateKey, data) ?? throw new PackageException("firma Ed25519 fallita.");
        return NativePkg.EcdsaSign(privateKey, data) ?? throw new PackageException("firma ECDSA fallita."); // C (OpenSSL)
    }

    /// <summary>Verifica firma. publicKey = pub Ed25519 (32B b64) o PEM ECDSA (fallback).</summary>
    public static bool VerifyData(byte[] data, string publicKey, string signatureB64)
    {
        try { return NativePkg.EdVerify(publicKey, data, signatureB64); } // C (Ed25519 b64 o PEM ECDSA)
        catch { return false; }
    }

    /// <summary>Firma un manifest (dati JSON) e ritorna la sezione signature da inserire.</summary>
    public static (string PublicKey, string Value) SignManifest(byte[] manifestJson, string privateKey)
        => (GetPublicKey(privateKey), SignData(manifestJson, privateKey));

    /// <summary>Deriva la chiave pubblica dalla privata (seed Ed25519 b64).</summary>
    public static string GetPublicKey(string privateKey)
    {
        if (TryB64Length(privateKey, 32, out _))
            return NativePkg.EdPublic(privateKey) ?? ""; // C (Ed25519)
        return NativePkg.EcdsaPublic(privateKey) ?? ""; // C (ECDSA PEM)
    }

        /// <summary>SHA256 hex di un file (delega al riferimento C: gmb_pkg_sha256_file).</summary>
        public static string Sha256File(string path) => NativePkg.Sha256File(path);

    /// <summary>
    /// BUILD publisher-side del pacchetto (manifest.build) per la piattaforma corrente: compila
    /// il C e produce gli artefatti ffi (es. lib/&lt;platform&gt;/lib*.dylib) PRIMA di packare/pubblicare.
    /// Ritorna (ok, log). Niente "magia": esegue il comando dichiarato nel manifest.
    /// </summary>
    public static (bool Ok, string Log) RunBuild(string dir, PackageManifest m, string platformKey)
    {
        _ = m; // il C legge il manifest da dir
        // Build publisher-side DELEGATO al riferimento C (gmb_pkg_run_build: make/cmake/command).
        return (NativePkg.RunBuild(dir, platformKey), "");
    }

    /// <summary>Verifica l'integrità (sha256 per-file) di un pacchetto installato. Ritorna lista file mancanti/alterati.</summary>
    public static List<string> VerifyIntegrity(PackageManifest m, string installDir, Dictionary<string, string> expected)
    {
        var issues = new List<string>();
        foreach (var (rel, hash) in expected)
        {
            var full = Path.Combine(installDir, rel);
            if (!File.Exists(full)) { issues.Add($"{rel}: mancante"); continue; }
            if (!string.Equals(Sha256File(full), hash, StringComparison.OrdinalIgnoreCase))
                issues.Add($"{rel}: hash alterato");
        }
        return issues;
    }
}
