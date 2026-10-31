using System.Security.Cryptography;
using Org.BouncyCastle.Asn1.X9;
using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Digests;
using Org.BouncyCastle.Crypto.Encodings;
using Org.BouncyCastle.Crypto.Engines;
using Org.BouncyCastle.Crypto.Generators;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.OpenSsl;
using Org.BouncyCastle.Security;

/* 
 * CryptoUtils.cs: metodi statici per hashing, crittografia, numeri casuali, derivazione chiavi, Base64/hex e HMAC
 * 
 * FUNZIONI:
 * - CoreRandomInt(min, max, seed): numero casuale intero
 * - CoreRandomDouble(min, max, seed): numero casuale double
 * - CoreRandomCryptoBytes(count): byte casuali crittografici
 * - CoreRandomNormal(mean, stddev, seed): numero da distribuzione normale
 * - CoreHashData(algorithmName, data): hash di dati (MD5, SHA1, SHA256, ecc.)
 * - CoreHashFile(algorithmName, filePath): hash di file
 * - CoreSupportedHashAlgorithms(): elenco algoritmi hash supportati
 * - CoreHmac(algorithmName, key, data): HMAC
 * - CoreAesEncrypt(data, password): crittografia AES-256-GCM
 * - CoreAesDecrypt(encryptedData, password): decrittografia AES-256-GCM
 * - CoreDeriveKeyFromPassword(password, salt, iterations, keySizeBytes): derivazione chiave PBKDF2
 * - CoreGetRandomBytes(count): byte casuali crittografici
 * - CoreToBase64(data): converte in Base64
 * - CoreFromBase64(base64String): decodifica da Base64
 * - CoreToHexString(bytes): converte in stringa esadecimale
 * - CoreFromHex(hexString): decodifica da esadecimale
 * - CoreConstantTimeEquals(a, b): confronto constant-time di byte array
 * - GenerateKeyPair(algorithm, keySize, curveName): genera coppia di chiavi RSA o ECDSA
 * - ExportKeyToPem(key, includePrivate): esporta chiave in formato PEM
 * - ImportKeyFromPem(pemData): importa chiavi da PEM
 * - SignData(data, privateKey, algorithm): firma dati con chiave privata
 * - VerifySignature(data, signature, publicKey, algorithm): verifica firma
 * - EncryptAsymmetric(data, publicKey): crittografia RSA con OAEP-SHA256
 * - DecryptAsymmetric(encryptedData, privateKey): decrittografia RSA
 */

namespace CommonLib;

/// <summary>
/// Provides static cryptographic utility methods for hashing, encryption, random number generation,
/// key derivation, Base64/hex conversion, and HMAC computation.
/// Supports both .NET built-in algorithms and BouncyCastle algorithms.
/// </summary>
public static class CryptoUtils
{
    private static readonly ThreadLocal<Random> DefaultRandom = new(() => new Random());

    private const int AesKeySizeBits = 256;
    private const int AesNonceSizeBytes = 12;
    private const int AesTagSizeBytes = 16;
    private const int Pbkdf2Iterations = 100000;
    private const int SaltSizeBytes = 16;

    /// <summary>
    /// Generates a random integer within the specified inclusive range.
    /// </summary>
    /// <param name="min">The inclusive lower bound of the random number.</param>
    /// <param name="max">The exclusive upper bound of the random number.</param>
    /// <param name="seed">An optional seed value for reproducible results.</param>
    /// <returns>A random integer in the range [min, max).</returns>
    public static int CoreRandomInt(int min, int max, int? seed = null)
    {
        var rnd = seed.HasValue ? new Random(seed.Value) : DefaultRandom.Value ?? new Random();
        return rnd.Next(min, max);
    }

    /// <summary>
    /// Generates a random double within the specified range.
    /// </summary>
    /// <param name="min">The inclusive lower bound of the random number.</param>
    /// <param name="max">The exclusive upper bound of the random number.</param>
    /// <param name="seed">An optional seed value for reproducible results.</param>
    /// <returns>A random double in the range [min, max).</returns>
    public static double CoreRandomDouble(double min, double max, int? seed = null)
    {
        var rnd = seed.HasValue ? new Random(seed.Value) : DefaultRandom.Value ?? new Random();
        return rnd.NextDouble() * (max - min) + min;
    }

    /// <summary>
    /// Generates a cryptographically secure random byte array.
    /// </summary>
    /// <param name="count">The number of random bytes to generate.</param>
    /// <returns>A byte array filled with cryptographically strong random values.</returns>
    public static byte[] CoreRandomCryptoBytes(int count)
    {
        var buffer = new byte[count];
        RandomNumberGenerator.Fill(buffer);
        return buffer;
    }

    /// <summary>
    /// Generates a random number from a normal (Gaussian) distribution using the Box-Muller transform.
    /// </summary>
    /// <param name="mean">The mean (center) of the distribution. Default is 0.0.</param>
    /// <param name="stddev">The standard deviation (spread) of the distribution. Default is 1.0.</param>
    /// <param name="seed">An optional seed value for reproducible results.</param>
    /// <returns>A random double sampled from the specified normal distribution.</returns>
    public static double CoreRandomNormal(double mean = 0.0, double stddev = 1.0, int? seed = null)
    {
        var rnd = seed.HasValue ? new Random(seed.Value) : DefaultRandom.Value ?? new Random();
        var u1 = 1.0 - rnd.NextDouble();
        var u2 = 1.0 - rnd.NextDouble();
        var randStdNormal = Math.Sqrt(-2.0 * Math.Log(u1)) * Math.Sin(2.0 * Math.PI * u2);
        return mean + stddev * randStdNormal;
    }

    /// <summary>
    /// Computes a hash of the given data using the specified algorithm.
    /// Supports MD5, SHA1, SHA224, SHA256, SHA384, SHA512, SHA3-256, SHA3-512, RIPEMD160, KECCAK256, and BLAKE2B-256.
    /// </summary>
    /// <param name="algorithmName">The name of the hash algorithm (case-insensitive).</param>
    /// <param name="data">The input byte array to hash.</param>
    /// <returns>The hash digest bytes.</returns>
    /// <exception cref="ArgumentException">Thrown when <paramref name="algorithmName"/> is not supported.</exception>
    public static byte[] CoreHashData(string algorithmName, byte[] data)
    {
        switch (algorithmName.ToUpperInvariant())
        {
            case "MD5": return MD5.HashData(data);
            case "SHA1": return SHA1.HashData(data);
            case "SHA224": return ComputeBouncyCastleHash(new Sha224Digest(), data);
            case "SHA256": return SHA256.HashData(data);
            case "SHA384": return SHA384.HashData(data);
            case "SHA512": return SHA512.HashData(data);
            case "SHA3-256": return ComputeBouncyCastleHash(new Sha3Digest(256), data);
            case "SHA3-512": return ComputeBouncyCastleHash(new Sha3Digest(512), data);
            case "RIPEMD160": return ComputeBouncyCastleHash(new RipeMD160Digest(), data);
            case "KECCAK256": return ComputeBouncyCastleHash(new KeccakDigest(256), data);
            case "BLAKE2B-256": return ComputeBouncyCastleHash(new Blake2bDigest(256), data);
            default:
                throw new ArgumentException($"Unsupported hash algorithm: {algorithmName}", nameof(algorithmName));
        }
    }

    /// <summary>
    /// Computes a hash of the contents of the specified file using the given algorithm.
    /// Supports MD5, SHA1, SHA224, SHA256, SHA384, SHA512, SHA3-256, SHA3-512, RIPEMD160, KECCAK256, and BLAKE2B-256.
    /// </summary>
    /// <param name="algorithmName">The name of the hash algorithm (case-insensitive).</param>
    /// <param name="filePath">The path to the file to hash.</param>
    /// <returns>The hash digest bytes.</returns>
    /// <exception cref="ArgumentException">Thrown when <paramref name="algorithmName"/> is not supported.</exception>
    public static byte[] CoreHashFile(string algorithmName, string filePath)
    {
        using var stream = File.OpenRead(filePath);
        switch (algorithmName.ToUpperInvariant())
        {
            case "MD5": return MD5.HashData(stream);
            case "SHA1": return SHA1.HashData(stream);
            case "SHA224": return ComputeBouncyCastleHash(new Sha224Digest(), stream);
            case "SHA256": return SHA256.HashData(stream);
            case "SHA384": return SHA384.HashData(stream);
            case "SHA512": return SHA512.HashData(stream);
            case "SHA3-256": return ComputeBouncyCastleHash(new Sha3Digest(256), stream);
            case "SHA3-512": return ComputeBouncyCastleHash(new Sha3Digest(512), stream);
            case "RIPEMD160": return ComputeBouncyCastleHash(new RipeMD160Digest(), stream);
            case "KECCAK256": return ComputeBouncyCastleHash(new KeccakDigest(256), stream);
            case "BLAKE2B-256": return ComputeBouncyCastleHash(new Blake2bDigest(256), stream);
            default:
                throw new ArgumentException($"Unsupported hash algorithm: {algorithmName}", nameof(algorithmName));
        }
    }

    /// <summary>
    /// Returns an array of all supported hash algorithm names.
    /// </summary>
    /// <returns>A string array containing the names of all supported hash algorithms.</returns>
    public static string[] CoreSupportedHashAlgorithms()
    {
        return
        [
            "MD5", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512",
            "SHA3-256", "SHA3-512", "RIPEMD160", "KECCAK256", "BLAKE2B-256"
        ];
    }

    /// <summary>
    /// Computes a Hash-Based Message Authentication Code (HMAC) using the specified algorithm.
    /// </summary>
    /// <param name="algorithmName">The HMAC algorithm name (SHA1, SHA256, SHA384, SHA512, or MD5 — case-insensitive).</param>
    /// <param name="key">The secret key for the HMAC computation.</param>
    /// <param name="data">The message data to authenticate.</param>
    /// <returns>The HMAC digest bytes.</returns>
    /// <exception cref="ArgumentException">Thrown when <paramref name="algorithmName"/> is not supported.</exception>
    public static byte[] CoreHmac(string algorithmName, byte[] key, byte[] data)
    {
        switch (algorithmName.ToUpperInvariant())
        {
            case "SHA1":
                using (var hmac = new HMACSHA1(key)) return hmac.ComputeHash(data);
            case "SHA256":
                using (var hmac = new HMACSHA256(key)) return hmac.ComputeHash(data);
            case "SHA384":
                using (var hmac = new HMACSHA384(key)) return hmac.ComputeHash(data);
            case "SHA512":
                using (var hmac = new HMACSHA512(key)) return hmac.ComputeHash(data);
            case "MD5":
                using (var hmac = new HMACMD5(key)) return hmac.ComputeHash(data);
            default:
                throw new ArgumentException($"Unsupported HMAC algorithm: {algorithmName}", nameof(algorithmName));
        }
    }

    /// <summary>
    /// Encrypts data using AES-256-GCM with a password-based key. The salt, nonce, and authentication tag
    /// are prepended to the ciphertext for storage.
    /// </summary>
    /// <param name="data">The plaintext byte array to encrypt.</param>
    /// <param name="password">The password used to derive the encryption key via PBKDF2.</param>
    /// <returns>A byte array containing [salt | nonce | tag | ciphertext].</returns>
    public static byte[] CoreAesEncrypt(byte[] data, string password)
    {
        var salt = RandomNumberGenerator.GetBytes(SaltSizeBytes);
        var key = Rfc2898DeriveBytes.Pbkdf2(password, salt, Pbkdf2Iterations, HashAlgorithmName.SHA256, AesKeySizeBits / 8);
        var nonce = RandomNumberGenerator.GetBytes(AesNonceSizeBytes);
        var tag = new byte[AesTagSizeBytes];
        var ciphertext = new byte[data.Length];

        using var aesGcm = new AesGcm(key, AesTagSizeBytes);
        aesGcm.Encrypt(nonce, data, ciphertext, tag);

        var result = new byte[salt.Length + nonce.Length + tag.Length + ciphertext.Length];
        Buffer.BlockCopy(salt, 0, result, 0, salt.Length);
        Buffer.BlockCopy(nonce, 0, result, salt.Length, nonce.Length);
        Buffer.BlockCopy(tag, 0, result, salt.Length + nonce.Length, tag.Length);
        Buffer.BlockCopy(ciphertext, 0, result, salt.Length + nonce.Length + tag.Length, ciphertext.Length);

        Array.Clear(key, 0, key.Length);

        return result;
    }

    /// <summary>
    /// Decrypts data that was encrypted with <see cref="CoreAesEncrypt"/>. Expects the concatenated
    /// [salt | nonce | tag | ciphertext] format produced by the encrypt method.
    /// </summary>
    /// <param name="encryptedData">The encrypted byte array in [salt | nonce | tag | ciphertext] format.</param>
    /// <param name="password">The password used to derive the decryption key via PBKDF2.</param>
    /// <returns>The decrypted plaintext byte array.</returns>
    /// <exception cref="ArgumentException">Thrown when <paramref name="encryptedData"/> is too short to contain the salt, nonce, and tag.</exception>
    /// <exception cref="AuthenticationTagMismatchException">Thrown when the authentication tag does not match (data corruption or wrong password).</exception>
    public static byte[] CoreAesDecrypt(byte[] encryptedData, string password)
    {
        if (encryptedData.Length < SaltSizeBytes + AesNonceSizeBytes + AesTagSizeBytes)
            throw new ArgumentException("Invalid encrypted data format or length.", nameof(encryptedData));

        var salt = encryptedData[..SaltSizeBytes];
        var nonce = encryptedData[SaltSizeBytes..(SaltSizeBytes + AesNonceSizeBytes)];
        var tag = encryptedData[
            (SaltSizeBytes + AesNonceSizeBytes)..(SaltSizeBytes + AesNonceSizeBytes + AesTagSizeBytes)];
        var ciphertext = encryptedData[(SaltSizeBytes + AesNonceSizeBytes + AesTagSizeBytes)..];

        var key = Rfc2898DeriveBytes.Pbkdf2(password, salt, Pbkdf2Iterations, HashAlgorithmName.SHA256, AesKeySizeBits / 8);
        var plaintext = new byte[ciphertext.Length];

        using var aesGcm = new AesGcm(key, AesTagSizeBytes);
        aesGcm.Decrypt(nonce, ciphertext, tag, plaintext);

        Array.Clear(key, 0, key.Length);

        return plaintext;
    }

    /// <summary>
    /// Derives a cryptographic key from a password using PBKDF2 with SHA-256.
    /// </summary>
    /// <param name="password">The password to derive the key from.</param>
    /// <param name="salt">The cryptographic salt bytes.</param>
    /// <param name="iterations">The number of PBKDF2 iterations.</param>
    /// <param name="keySizeBytes">The desired key size in bytes.</param>
    /// <returns>The derived key bytes.</returns>
    public static byte[] CoreDeriveKeyFromPassword(string password, byte[] salt, int iterations, int keySizeBytes)
    {
        return Rfc2898DeriveBytes.Pbkdf2(password, salt, iterations, HashAlgorithmName.SHA256, keySizeBytes);
    }

    /// <summary>
    /// Generates a cryptographically secure random byte array of the specified length.
    /// </summary>
    /// <param name="count">The number of random bytes to generate.</param>
    /// <returns>A byte array filled with cryptographically random values.</returns>
    public static byte[] CoreGetRandomBytes(int count) => RandomNumberGenerator.GetBytes(count);

    /// <summary>
    /// Converts a byte array to a Base64-encoded string.
    /// </summary>
    /// <param name="data">The input byte array.</param>
    /// <returns>A Base64-encoded string.</returns>
    public static string CoreToBase64(byte[] data) => Convert.ToBase64String(data);

    /// <summary>
    /// Decodes a Base64-encoded string back into a byte array.
    /// </summary>
    /// <param name="base64String">The Base64-encoded string.</param>
    /// <returns>The decoded byte array.</returns>
    /// <exception cref="FormatException">Thrown if the input is not a valid Base64 string.</exception>
    public static byte[] CoreFromBase64(string base64String) => Convert.FromBase64String(base64String);

    /// <summary>
    /// Converts a byte array to a Base64URL-encoded string (RFC 4648 §5, without padding).
    /// </summary>
    public static string CoreToBase64Url(byte[] data)
        => Convert.ToBase64String(data).TrimEnd('=').Replace('+', '-').Replace('/', '_');

    /// <summary>
    /// Decodes a Base64URL string (with or without padding) back into a byte array.
    /// </summary>
    /// <exception cref="FormatException">Thrown if the input is not valid Base64URL.</exception>
    public static byte[] CoreFromBase64Url(string base64UrlString)
    {
        var s = base64UrlString.Trim().Replace('-', '+').Replace('_', '/');
        switch (s.Length % 4)
        {
            case 2: s += "=="; break;
            case 3: s += "="; break;
            case 1: throw new FormatException("Invalid Base64URL string.");
        }
        return Convert.FromBase64String(s);
    }

    /// <summary>
    /// Converts a byte array to a lowercase hex string.
    /// </summary>
    /// <param name="bytes">The input byte array.</param>
    /// <returns>A lowercase hex-encoded string.</returns>
    public static string CoreToHexString(byte[] bytes) => Convert.ToHexString(bytes).ToLowerInvariant();

    /// <summary>
    /// Decodes a hex string into a byte array.
    /// </summary>
    /// <param name="hexString">The hex-encoded string (with or without uppercase/lowercase).</param>
    /// <returns>The decoded byte array.</returns>
    /// <exception cref="FormatException">Thrown if the input is not a valid hex string.</exception>
    public static byte[] CoreFromHex(string hexString) => Convert.FromHexString(hexString);

    /// <summary>
    /// Compares two byte arrays for equality in constant time, mitigating timing side-channel attacks.
    /// </summary>
    /// <param name="a">The first byte array.</param>
    /// <param name="b">The second byte array.</param>
    /// <returns><c>true</c> if the arrays have the same length and identical content; otherwise <c>false</c>.</returns>
    public static bool CoreConstantTimeEquals(byte[] a, byte[] b)
    {
        if (a.Length != b.Length) return false;
        var diff = 0;
        for (var i = 0; i < a.Length; i++)
            diff |= a[i] ^ b[i];
        return diff == 0;
    }

    /// <summary>
    /// Computes a hash using a BouncyCastle <see cref="IDigest"/> over a byte array.
    /// </summary>
    /// <param name="digest">The BouncyCastle digest instance.</param>
    /// <param name="data">The input byte array to hash.</param>
    /// <returns>The computed hash digest bytes.</returns>
    internal static byte[] ComputeBouncyCastleHash(IDigest digest, byte[] data)
    {
        var output = new byte[digest.GetDigestSize()];
        digest.BlockUpdate(data, 0, data.Length);
        digest.DoFinal(output, 0);
        return output;
    }

    /// <summary>
    /// Computes a hash using a BouncyCastle <see cref="IDigest"/> over a stream's content.
    /// </summary>
    /// <param name="digest">The BouncyCastle digest instance.</param>
    /// <param name="stream">The input stream to hash (read from the current position).</param>
    /// <returns>The computed hash digest bytes.</returns>
    internal static byte[] ComputeBouncyCastleHash(IDigest digest, Stream stream)
    {
        var buffer = new byte[4096];
        int bytesRead;
        while ((bytesRead = stream.Read(buffer, 0, buffer.Length)) > 0)
        {
            digest.BlockUpdate(buffer, 0, bytesRead);
        }

        var output = new byte[digest.GetDigestSize()];
        digest.DoFinal(output, 0);
        return output;
    }

    /// <summary>Generates an RSA or ECDSA key pair.</summary>
    /// <param name="algorithm">"RSA" or "ECDSA".</param>
    /// <param name="keySize">RSA key size (min 2048). Ignored for ECDSA.</param>
    /// <param name="curveName">ECDSA curve (e.g. "secp256k1"). Ignored for RSA.</param>
    /// <returns>A tuple with private and public key parameters.</returns>
    public static (AsymmetricKeyParameter privateKey, AsymmetricKeyParameter publicKey) GenerateKeyPair(
        string algorithm, int? keySize = null, string? curveName = null)
    {
        AsymmetricCipherKeyPair keyPair;

        if (algorithm.Equals("RSA", StringComparison.OrdinalIgnoreCase))
        {
            var generator = new RsaKeyPairGenerator();
            var rsaKeySize = keySize ?? 2048;
            if (rsaKeySize < 2048) throw new ArgumentException("RSA key size must be at least 2048 bits.");
            generator.Init(new KeyGenerationParameters(new SecureRandom(), rsaKeySize));
            keyPair = generator.GenerateKeyPair();
        }
        else if (algorithm.Equals("ECDSA", StringComparison.OrdinalIgnoreCase))
        {
            var curve = ECNamedCurveTable.GetByName(curveName ?? "secp256k1");
            if (curve == null) throw new ArgumentException($"Curve ECDSA non supportata: {curveName}");
            var domainParams = new ECDomainParameters(curve.Curve, curve.G, curve.N, curve.H);
            var generator = new ECKeyPairGenerator("ECDSA");
            generator.Init(new ECKeyGenerationParameters(domainParams, new SecureRandom()));
            keyPair = generator.GenerateKeyPair();
        }
        else
        {
            throw new ArgumentException("Unsupported key algorithm. Use 'RSA' or 'ECDSA'.");
        }

        return (keyPair.Private, keyPair.Public);
    }

    /// <summary>Exports a key to PEM-encoded string.</summary>
    public static string ExportKeyToPem(AsymmetricKeyParameter key, bool includePrivate = false)
    {
        using var stringWriter = new StringWriter();
        var pemWriter = new PemWriter(stringWriter);
        pemWriter.WriteObject(key);
        return stringWriter.ToString();
    }

    /// <summary>Imports one or more keys from a PEM-encoded string.</summary>
    /// <returns>Array of keys (typically private + public from a key pair).</returns>
    public static AsymmetricKeyParameter[] ImportKeyFromPem(string pemData)
    {
        using var stringReader = new StringReader(pemData);
        var pemReader = new PemReader(stringReader);
        var result = new List<AsymmetricKeyParameter>();

        while (true)
        {
            var keyObject = pemReader.ReadObject();
            if (keyObject == null) break;

            if (keyObject is AsymmetricCipherKeyPair pair)
            {
                result.Add(pair.Private);
                result.Add(pair.Public);
            }
            else if (keyObject is AsymmetricKeyParameter key)
            {
                result.Add(key);
            }
            else
            {
                throw new ArgumentException("Invalid or unsupported PEM format.");
            }
        }

        if (result.Count == 0)
            throw new ArgumentException("No valid key found in PEM data.");

        return result.ToArray();
    }

    /// <summary>Sceglie l'algoritmo di firma SHA256 corretto in base al tipo di chiave BouncyCastle
    /// (RSA o EC) — SignData/VerifySignature richiedono un nome algoritmo che combaci col tipo di
    /// chiave, altrimenti BouncyCastle lancia InvalidKeyException a runtime.</summary>
    public static string GetSignatureAlgorithm(AsymmetricKeyParameter key) => key switch
    {
        ECKeyParameters => "SHA256withECDSA",
        RsaKeyParameters => "SHA256withRSA",
        _ => "SHA256withRSA"
    };

    /// <summary>Signs data with a private key using the specified algorithm (e.g. "SHA256withRSA", "SHA256withECDSA").</summary>
    public static byte[] SignData(byte[] data, AsymmetricKeyParameter privateKey, string algorithm)
    {
        if (!privateKey.IsPrivate)
            throw new ArgumentException("A private key is required for signing.", nameof(privateKey));

        var signer = SignerUtilities.GetSigner(algorithm);
        signer.Init(true, privateKey);
        signer.BlockUpdate(data, 0, data.Length);
        return signer.GenerateSignature();
    }

    /// <summary>Verifies a signature against data with a public key.</summary>
    public static bool VerifySignature(byte[] data, byte[] signature, AsymmetricKeyParameter publicKey, string algorithm)
    {
        if (publicKey.IsPrivate)
            throw new ArgumentException("A public key is required for verification.", nameof(publicKey));

        var signer = SignerUtilities.GetSigner(algorithm);
        signer.Init(false, publicKey);
        signer.BlockUpdate(data, 0, data.Length);
        return signer.VerifySignature(signature);
    }

    /// <summary>Encrypts data with an RSA public key using OAEP-SHA256 padding.</summary>
    public static byte[] EncryptAsymmetric(byte[] data, AsymmetricKeyParameter publicKey)
    {
        if (publicKey.IsPrivate)
            throw new ArgumentException("A public key is required for encryption.", nameof(publicKey));
        if (publicKey is not RsaKeyParameters rsaKey)
            throw new ArgumentException("Asymmetric encryption is only supported for RSA keys.", nameof(publicKey));

        var engine = new RsaEngine();
        var cipher = new OaepEncoding(engine, new Sha256Digest());
        cipher.Init(true, publicKey);

        var blockSize = (rsaKey.Modulus.BitLength + 7) / 8 - 2 * new Sha256Digest().GetDigestSize() - 2;
        using var ms = new MemoryStream();
        for (var offset = 0; offset < data.Length; offset += blockSize)
        {
            var chunkSize = Math.Min(blockSize, data.Length - offset);
            var chunk = cipher.ProcessBlock(data, offset, chunkSize);
            ms.Write(chunk, 0, chunk.Length);
        }
        return ms.ToArray();
    }

    /// <summary>Decrypts data with an RSA private key using OAEP-SHA256 padding.</summary>
    public static string CorePasswordHash(string password)
    {
        var salt = new byte[16];
        using var rng = System.Security.Cryptography.RandomNumberGenerator.Create();
        rng.GetBytes(salt);
        var hash = System.Security.Cryptography.Rfc2898DeriveBytes.Pbkdf2(
            password, salt, 100000, System.Security.Cryptography.HashAlgorithmName.SHA256, 32);
        return Convert.ToBase64String(salt) + "." + Convert.ToBase64String(hash);
    }

    public static bool CorePasswordVerify(string password, string hashWithSalt)
    {
        var parts = hashWithSalt.Split('.');
        if (parts.Length != 2) return false;
        var salt = Convert.FromBase64String(parts[0]);
        var storedHash = Convert.FromBase64String(parts[1]);
        var computedHash = System.Security.Cryptography.Rfc2898DeriveBytes.Pbkdf2(
            password, salt, 100000, System.Security.Cryptography.HashAlgorithmName.SHA256, 32);
        return CryptographicOperations.FixedTimeEquals(storedHash, computedHash);
    }

    public static byte[] DecryptAsymmetric(byte[] encryptedData, AsymmetricKeyParameter privateKey)
    {
        if (!privateKey.IsPrivate)
            throw new ArgumentException("A private key is required for decryption.", nameof(privateKey));
        if (privateKey is not RsaKeyParameters rsaKey)
            throw new ArgumentException("Asymmetric decryption is only supported for RSA keys.", nameof(privateKey));

        var engine = new RsaEngine();
        var cipher = new OaepEncoding(engine, new Sha256Digest());
        cipher.Init(false, privateKey);

        var blockSize = (rsaKey.Modulus.BitLength + 7) / 8;
        using var ms = new MemoryStream();
        for (var offset = 0; offset < encryptedData.Length; offset += blockSize)
        {
            var chunkSize = Math.Min(blockSize, encryptedData.Length - offset);
            var chunk = cipher.ProcessBlock(encryptedData, offset, chunkSize);
            ms.Write(chunk, 0, chunk.Length);
        }
        return ms.ToArray();
    }

    // --- ChaCha20 / ChaCha20Poly1305 / Password hash (BouncyCastle) ---

    /// <summary>ChaCha20Encrypt(data, key, nonce) — cifra un blocco con ChaCha20.</summary>
    public static byte[] CoreChaCha20Encrypt(byte[] data, byte[] key, byte[] nonce)
    {
        var engine = new Org.BouncyCastle.Crypto.Engines.ChaCha7539Engine();
        engine.Init(true, new ParametersWithIV(new KeyParameter(key), nonce));
        return ProcessStream(engine, data);
    }

    /// <summary>ChaCha20Decrypt(data, key, nonce) — decifra un blocco con ChaCha20.</summary>
    public static byte[] CoreChaCha20Decrypt(byte[] data, byte[] key, byte[] nonce)
    {
        var engine = new Org.BouncyCastle.Crypto.Engines.ChaCha7539Engine();
        engine.Init(false, new ParametersWithIV(new KeyParameter(key), nonce));
        return ProcessStream(engine, data);
    }

    /// <summary>ChaCha20Poly1305Encrypt — AEAD; restituisce {ciphertext, tag}.</summary>
    public static (byte[] ciphertext, byte[] tag) CoreChaCha20Poly1305Encrypt(byte[] plaintext, byte[] key, byte[] nonce, byte[]? aad = null)
    {
        var cipher = new Org.BouncyCastle.Crypto.Modes.ChaCha20Poly1305();
        var aeadParams = new AeadParameters(new KeyParameter(key), 128, nonce, aad ?? Array.Empty<byte>());
        cipher.Init(true, aeadParams);
        var ct = new byte[cipher.GetOutputSize(plaintext.Length)];
        var len = cipher.ProcessBytes(plaintext, 0, plaintext.Length, ct, 0);
        len += cipher.DoFinal(ct, len);
        var ciphertext = new byte[plaintext.Length];
        var tag = new byte[16];
        Array.Copy(ct, 0, ciphertext, 0, plaintext.Length);
        Array.Copy(ct, plaintext.Length, tag, 0, 16);
        return (ciphertext, tag);
    }

    /// <summary>ChaCha20Poly1305Decrypt — AEAD; restituisce il plaintext o null se tag non valido.</summary>
    public static byte[]? CoreChaCha20Poly1305Decrypt(byte[] ciphertext, byte[] tag, byte[] key, byte[] nonce, byte[]? aad = null)
    {
        try
        {
            var cipher = new Org.BouncyCastle.Crypto.Modes.ChaCha20Poly1305();
            var aeadParams = new AeadParameters(new KeyParameter(key), 128, nonce, aad ?? Array.Empty<byte>());
            cipher.Init(false, aeadParams);
            var combined = new byte[ciphertext.Length + tag.Length];
            Array.Copy(ciphertext, 0, combined, 0, ciphertext.Length);
            Array.Copy(tag, 0, combined, ciphertext.Length, tag.Length);
            var pt = new byte[cipher.GetOutputSize(combined.Length)];
            var len = cipher.ProcessBytes(combined, 0, combined.Length, pt, 0);
            len += cipher.DoFinal(pt, len);
            var result = new byte[len];
            Array.Copy(pt, 0, result, 0, len);
            return result;
        }
        catch { return null; }
    }

    private static byte[] ProcessStream(IStreamCipher engine, byte[] data)
    {
        var output = new byte[data.Length];
        engine.ProcessBytes(data, 0, data.Length, output, 0);
        return output;
    }
}
