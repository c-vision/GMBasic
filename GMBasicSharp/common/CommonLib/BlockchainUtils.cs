using System.Security.Cryptography;
using System.Text;
using Org.BouncyCastle.Crypto.Digests;
using Org.BouncyCastle.Math;
using Org.BouncyCastle.Asn1.Sec;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Crypto.Signers;

/* 
 * BlockchainUtils.cs: metodi statici per operazioni blockchain (hashing Keccak/BLAKE2/RIPEMD, Base58/Base32, RLP, indirizzi, firme ECDSA)
 * 
 * FUNZIONI:
 * - Keccak256(data): hash Keccak-256
 * - Blake2b256(data): hash BLAKE2b-256
 * - RipeMd160(data): hash RIPEMD-160
 * - Base58Encode(data): codifica Base58
 * - Base58Decode(base58): decodifica Base58
 * - Base32Encode(data): codifica Base32
 * - Base32Decode(base32): decodifica Base32
 * - RlpEncode(input): codifica RLP
 * - VarIntEncode(value): codifica VARINT
 * - VarIntDecode(data): decodifica VARINT
 * - EthAddressFromPub(publicKey): deriva indirizzo Ethereum da chiave pubblica
 * - EthAddressChecksum(address): indirizzo Ethereum con checksum EIP-55
 * - BtcAddressFromPub(publicKey, testnet): deriva indirizzo Bitcoin da chiave pubblica
 * - BlockDifficulty(blockHeader, bits): calcola difficoltà blocco
 * - EcdsaSecp256k1Sign(message, privateKey): firma ECDSA secp256k1
 * - Base58Util.Encode/Decode: utility Base58
 */

namespace CommonLib;

/// <summary>
/// Provides static methods for blockchain operations including hashing, encoding, address generation, and signing.
/// Supports Ethereum and Bitcoin address derivation, Base58/Base32 encoding, RLP encoding, and secp256k1 ECDSA signatures.
/// </summary>
public static class BlockchainUtils
{
    /// <summary>
    /// Computes the Keccak-256 hash of the given data.
    /// </summary>
    /// <param name="data">The input byte array to hash.</param>
    /// <returns>A 32-byte Keccak-256 hash.</returns>
    public static byte[] Keccak256(byte[] data)
    {
        var digest = new KeccakDigest(256);
        digest.BlockUpdate(data, 0, data.Length);
        var result = new byte[32];
        digest.DoFinal(result, 0);
        return result;
    }

    /// <summary>
    /// Computes the BLAKE2b-256 hash of the given data.
    /// </summary>
    /// <param name="data">The input byte array to hash.</param>
    /// <returns>A 32-byte BLAKE2b-256 hash.</returns>
    public static byte[] Blake2b256(byte[] data)
    {
        var digest = new Blake2bDigest(256);
        digest.BlockUpdate(data, 0, data.Length);
        var result = new byte[32];
        digest.DoFinal(result, 0);
        return result;
    }

    /// <summary>
    /// Computes the RIPEMD-160 hash of the given data.
    /// </summary>
    /// <param name="data">The input byte array to hash.</param>
    /// <returns>A 20-byte RIPEMD-160 hash.</returns>
    public static byte[] RipeMd160(byte[] data)
    {
        var digest = new RipeMD160Digest();
        digest.BlockUpdate(data, 0, data.Length);
        var result = new byte[20];
        digest.DoFinal(result, 0);
        return result;
    }

    /// <summary>
    /// Encodes a byte array into a Base58 string.
    /// </summary>
    /// <param name="data">The input byte array to encode.</param>
    /// <returns>A Base58-encoded string.</returns>
    public static string Base58Encode(byte[] data) => Base58Util.Encode(data);

    /// <summary>
    /// Decodes a Base58 string back into a byte array.
    /// </summary>
    /// <param name="base58">The Base58-encoded string.</param>
    /// <returns>The decoded byte array.</returns>
    /// <exception cref="FormatException">Thrown when the string contains an invalid Base58 character.</exception>
    public static byte[] Base58Decode(string base58) => Base58Util.Decode(base58);

    /// <summary>
    /// Encodes a byte array into a Base32 string using the RFC 4648 alphabet (A-Z, 2-7).
    /// </summary>
    /// <param name="data">The input byte array to encode.</param>
    /// <returns>A Base32-encoded string.</returns>
    public static string Base32Encode(byte[] data)
    {
        const string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        var outputLength = ((data.Length * 8) + 4) / 5;
        var result = new StringBuilder((outputLength + 7) / 8 * 8);
        int buffer = data[0], next = 1, bitsLeft = 8;
        while (bitsLeft > 0 || next < data.Length)
        {
            if (bitsLeft < 5)
            {
                if (next < data.Length)
                {
                    buffer <<= 8;
                    buffer |= data[next++] & 0xFF;
                    bitsLeft += 8;
                }
                else
                {
                    var pad = 5 - bitsLeft;
                    buffer <<= pad;
                    bitsLeft += pad;
                }
            }

            var index = 0x1F & (buffer >> (bitsLeft - 5));
            bitsLeft -= 5;
            result.Append(alphabet[index]);
        }

        return result.ToString();
    }

    /// <summary>
    /// Decodes a Base32 string back into a byte array.
    /// </summary>
    /// <param name="base32">The Base32-encoded string.</param>
    /// <returns>The decoded byte array.</returns>
    /// <exception cref="FormatException">Thrown when the string contains an invalid Base32 character.</exception>
    public static byte[] Base32Decode(string base32)
    {
        const string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        var buffer = 0;
        var bitsLeft = 0;
        var output = new byte[base32.Length * 5 / 8];
        var outIndex = 0;
        foreach (var c in base32.ToUpperInvariant())
        {
            if (char.IsWhiteSpace(c)) continue;
            var val = alphabet.IndexOf(c);
            if (val < 0) throw new FormatException($"Invalid char '{c}' in Base32 string.");
            buffer <<= 5;
            buffer |= val & 0x1F;
            bitsLeft += 5;
            if (bitsLeft >= 8)
            {
                output[outIndex++] = (byte)((buffer >> (bitsLeft - 8)) & 0xFF);
                bitsLeft -= 8;
            }
        }

        return output.Take(outIndex).ToArray();
    }

    /// <summary>
    /// Encodes a byte array using the Recursive Length Prefix (RLP) encoding scheme.
    /// </summary>
    /// <param name="input">The byte array to encode.</param>
    /// <returns>The RLP-encoded byte array.</returns>
    public static byte[] RlpEncode(byte[] input)
    {
        if (input is [< 0x80])
            return input;
        if (input.Length < 56)
        {
            var prefix = (byte)(0x80 + input.Length);
            return (new[] { prefix }).Concat(input).ToArray();
        }
        else
        {
            var lenBytes = ToMinimalByteArray(input.Length);
            var prefix = (byte)(0xB7 + lenBytes.Length);
            return (new[] { prefix }).Concat(lenBytes).Concat(input).ToArray();
        }
    }

    /// <summary>
    /// Converts an integer to its minimal byte array representation (no leading zero bytes).
    /// </summary>
    /// <param name="value">The integer value to convert.</param>
    /// <returns>The minimal byte array representation.</returns>
    private static byte[] ToMinimalByteArray(int value)
    {
        var bytes = BitConverter.GetBytes(value);
        if (BitConverter.IsLittleEndian)
            Array.Reverse(bytes);
        var idx = 0;
        while (idx < bytes.Length && bytes[idx] == 0) idx++;
        return bytes.Skip(idx).ToArray();
    }

    /// <summary>
    /// Encodes a 64-bit integer using variable-length encoding (VARINT).
    /// </summary>
    /// <param name="value">The integer value to encode.</param>
    /// <returns>The VARINT-encoded byte array.</returns>
    public static byte[] VarIntEncode(long value)
    {
        var buffer = new byte[10];
        var idx = 0;
        while (true)
        {
            var temp = (byte)(value & 0x7F);
            value >>= 7;
            if (value == 0)
            {
                buffer[idx++] = temp;
                break;
            }

            buffer[idx++] = (byte)(temp | 0x80);
        }

        return buffer.Take(idx).ToArray();
    }

    /// <summary>
    /// Decodes a variable-length encoded (VARINT) byte array back into a 64-bit integer.
    /// </summary>
    /// <param name="data">The VARINT-encoded byte array.</param>
    /// <returns>The decoded 64-bit integer.</returns>
    public static long VarIntDecode(byte[] data)
    {
        long result = 0;
        var shift = 0;
        foreach (var b in data)
        {
            result |= ((long)(b & 0x7F)) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }

        return result;
    }

    /// <summary>
    /// Derives an Ethereum address from the given public key.
    /// </summary>
    /// <param name="publicKey">The public key bytes (either 64-byte uncompressed or 65-byte with 0x04 prefix).</param>
    /// <returns>The Ethereum address as a hex string with "0x" prefix.</returns>
    public static string EthAddressFromPub(byte[] publicKey)
    {
        var pub = publicKey.Length == 65 && publicKey[0] == 0x04 ? publicKey.Skip(1).ToArray() : publicKey;
        var hash = Keccak256(pub);
        var addr = hash.Skip(12).ToArray();
        return "0x" + BitConverter.ToString(addr).Replace("-", "").ToLowerInvariant();
    }

    /// <summary>
    /// Computes the EIP-55 checksummed version of an Ethereum address.
    /// </summary>
    /// <param name="address">The raw Ethereum address (with or without "0x" prefix).</param>
    /// <returns>The mixed-case checksummed address with "0x" prefix.</returns>
    public static string EthAddressChecksum(string address)
    {
        var addr = address.ToLowerInvariant().Replace("0x", "");
        var hash = Keccak256(Encoding.ASCII.GetBytes(addr));
        var hashHex = BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
        var checksummed = "";
        for (var i = 0; i < addr.Length; i++)
            checksummed += int.Parse(hashHex[i].ToString(), System.Globalization.NumberStyles.HexNumber) >= 8
                ? char.ToUpper(addr[i])
                : addr[i];
        return "0x" + checksummed;
    }

    /// <summary>
    /// Derives a Bitcoin address from the given public key.
    /// </summary>
    /// <param name="publicKey">The public key bytes.</param>
    /// <param name="testnet">If <c>true</c>, generates a testnet address (prefix 0x6F); otherwise mainnet (prefix 0x00).</param>
    /// <returns>A Base58-encoded Bitcoin address.</returns>
    public static string BtcAddressFromPub(byte[] publicKey, bool testnet = false)
    {
        var sha256 = SHA256.HashData(publicKey);
        var ripe = RipeMd160(sha256);
        var prefix = testnet ? new byte[] { 0x6F } : new byte[] { 0x00 };
        var payload = prefix.Concat(ripe).ToArray();
        var checksum = SHA256.HashData(SHA256.HashData(payload)).Take(4).ToArray();
        var addr = payload.Concat(checksum).ToArray();
        return Base58Encode(addr);
    }

    /// <summary>
    /// Calculates the block difficulty from the block header and the compact bits representation.
    /// </summary>
    /// <param name="blockHeader">The block header bytes.</param>
    /// <param name="bits">The compact difficulty bits (as used in Bitcoin block headers).</param>
    /// <returns>A <see cref="BigInteger"/> representing the block difficulty.</returns>
    public static BigInteger BlockDifficulty(byte[] blockHeader, int bits)
    {
        var exponent = bits >> 24;
        var mantissa = bits & 0xFFFFFF;
        var target = BigInteger.ValueOf(mantissa).Multiply(BigInteger.ValueOf(256).Pow(exponent - 3));
        var hash = new BigInteger(1, SHA256.HashData(SHA256.HashData(blockHeader)));
        return target.Divide(hash);
    }

    /// <summary>
    /// Signs a message using ECDSA on the secp256k1 curve, with Keccak-256 hashing of the message.
    /// </summary>
    /// <param name="message">The message bytes to sign.</param>
    /// <param name="privateKey">The 32-byte private key.</param>
    /// <returns>A DER-encoded ECDSA signature.</returns>
    public static byte[] EcdsaSecp256k1Sign(byte[] message, byte[] privateKey)
    {
        var x9 = SecNamedCurves.GetByName("secp256k1");
        var domain = new ECDomainParameters(x9.Curve, x9.G, x9.N, x9.H, x9.GetSeed());

        var keyParams = new ECPrivateKeyParameters(new BigInteger(1, privateKey), domain);

        var signer = new ECDsaSigner();
        signer.Init(true, keyParams);

        var hash = Keccak256(message);
        var sig = signer.GenerateSignature(hash);

        return DerEncode(sig[0], sig[1]);
    }

    /// <summary>
    /// DER-encodes an ECDSA signature from its (r, s) components.
    /// </summary>
    /// <param name="r">The r component of the signature.</param>
    /// <param name="s">The s component of the signature.</param>
    /// <returns>The DER-encoded signature byte array.</returns>
    private static byte[] DerEncode(BigInteger r, BigInteger s)
    {
        var rBytes = r.ToByteArrayUnsigned();
        var sBytes = s.ToByteArrayUnsigned();
        var result = new byte[6 + rBytes.Length + sBytes.Length];
        result[0] = 0x30;
        result[1] = (byte)(4 + rBytes.Length + sBytes.Length);
        result[2] = 0x02;
        result[3] = (byte)rBytes.Length;
        Array.Copy(rBytes, 0, result, 4, rBytes.Length);
        result[4 + rBytes.Length] = 0x02;
        result[5 + rBytes.Length] = (byte)sBytes.Length;
        Array.Copy(sBytes, 0, result, 6 + rBytes.Length, sBytes.Length);
        return result;
    }

    /// <summary>
    /// Provides Base58 encoding and decoding using the Bitcoin alphabet (excluding 0, O, I, l).
    /// </summary>
    public static class Base58Util
    {
        private const string Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

        private static readonly int[] Indexes = new int[128];

        static Base58Util()
        {
            for (var i = 0; i < Indexes.Length; i++) Indexes[i] = -1;
            for (var i = 0; i < Alphabet.Length; i++) Indexes[Alphabet[i]] = i;
        }

        /// <summary>
        /// Encodes a byte array into a Base58 string.
        /// </summary>
        /// <param name="input">The input byte array to encode.</param>
        /// <returns>A Base58-encoded string. Returns an empty string if the input is empty.</returns>
        public static string Encode(byte[] input)
        {
            if (input.Length == 0) return "";

            var zeros = 0;
            while (zeros < input.Length && input[zeros] == 0) zeros++;

            var inputCopy = (byte[])input.Clone();
            var encoded = new StringBuilder();
            var startAt = zeros;
            while (startAt < inputCopy.Length)
            {
                var mod = DivMod58(inputCopy, startAt);
                if (inputCopy[startAt] == 0) startAt++;
                encoded.Insert(0, Alphabet[mod]);
            }

            for (var i = 0; i < zeros; i++) encoded.Insert(0, '1');
            return encoded.ToString();
        }

        /// <summary>
        /// Decodes a Base58 string back into a byte array.
        /// </summary>
        /// <param name="input">The Base58-encoded string.</param>
        /// <returns>The decoded byte array. Returns an empty array if the input is empty.</returns>
        /// <exception cref="FormatException">Thrown if the string contains an invalid Base58 character.</exception>
        public static byte[] Decode(string input)
        {
            if (input.Length == 0) return [];

            var input58 = new byte[input.Length];
            for (var i = 0; i < input.Length; ++i)
            {
                var c = input[i];
                var digit = c < 128 ? Indexes[c] : -1;
                if (digit < 0) throw new FormatException($"Invalid Base58 character `{c}` at position {i}");
                input58[i] = (byte)digit;
            }

            var zeros = 0;
            while (zeros < input58.Length && input58[zeros] == 0) zeros++;

            var decoded = new byte[input.Length];
            var j = decoded.Length;
            var startAt = zeros;
            while (startAt < input58.Length)
            {
                var mod = DivMod256(input58, startAt);
                if (input58[startAt] == 0) startAt++;
                decoded[--j] = (byte)mod;
            }

            while (j < decoded.Length && decoded[j] == 0) j++;

            var output = new byte[decoded.Length - j + zeros];
            for (var i = 0; i < zeros; i++) output[i] = 0;
            Array.Copy(decoded, j, output, zeros, output.Length - zeros);
            return output;
        }

        /// <summary>
        /// Performs division by 58 on a big-endian byte array, modifying it in-place.
        /// </summary>
        /// <param name="number">The big-endian byte array to divide.</param>
        /// <param name="startAt">The starting index within the array.</param>
        /// <returns>The remainder (0–57).</returns>
        private static int DivMod58(byte[] number, int startAt)
        {
            var remainder = 0;
            for (var i = startAt; i < number.Length; i++)
            {
                var digit256 = number[i] & 0xFF;
                var temp = remainder * 256 + digit256;
                number[i] = (byte)(temp / 58);
                remainder = temp % 58;
            }

            return remainder;
        }

        /// <summary>
        /// Performs division by 256 on a Base58-encoded byte array, modifying it in-place.
        /// </summary>
        /// <param name="number58">The Base58-encoded byte array to divide.</param>
        /// <param name="startAt">The starting index within the array.</param>
        /// <returns>The remainder (0–255).</returns>
        private static int DivMod256(byte[] number58, int startAt)
        {
            var remainder = 0;
            for (var i = startAt; i < number58.Length; i++)
            {
                var digit58 = number58[i] & 0xFF;
                var temp = remainder * 58 + digit58;
                number58[i] = (byte)(temp / 256);
                remainder = temp % 256;
            }

            return remainder;
        }

    }

    // --- HD Wallet: BIP39 mnemonic / BIP32 derivation / secp256k1 public key ---

    /// <summary>GENERATEMNEMONIC(wordCount?) — genera un mnemonic BIP39 (default 12 parole).</summary>
    public static string GenerateMnemonic(int wordCount = 12)
    {
        var entropyBytes = wordCount * 4 / 3;
        var entropy = new byte[entropyBytes];
        using var rng = System.Security.Cryptography.RandomNumberGenerator.Create();
        rng.GetBytes(entropy);
        var checksumSize = entropyBytes * 8 / 32;
        var hash = System.Security.Cryptography.SHA256.HashData(entropy);
        var bits = new System.Collections.BitArray(entropyBytes * 8 + checksumSize);
        for (int i = 0; i < entropy.Length; i++)
            for (int j = 0; j < 8; j++)
                bits[i * 8 + j] = (entropy[i] & (1 << (7 - j))) != 0;
        for (int i = 0; i < checksumSize; i++)
            bits[entropyBytes * 8 + i] = (hash[i / 8] & (1 << (7 - (i % 8)))) != 0;
        var result = new StringBuilder();
        for (int i = 0; i < wordCount; i++)
        {
            int idx = 0;
            for (int j = 0; j < 11; j++)
                if (bits[i * 11 + j]) idx |= 1 << (10 - j);
            if (i > 0) result.Append(' ');
            result.Append(Bip39Words[idx]);
        }
        return result.ToString();
    }

    /// <summary>MNEMONICTOSEED(mnemonic, passphrase?) — seed 64 byte da mnemonic BIP39.</summary>
    public static byte[] MnemonicToSeed(string mnemonic, string? passphrase = null)
    {
        var salt = System.Text.Encoding.UTF8.GetBytes("mnemonic" + (passphrase ?? ""));
        return System.Security.Cryptography.Rfc2898DeriveBytes.Pbkdf2(
            mnemonic, salt, 2048, System.Security.Cryptography.HashAlgorithmName.SHA512, 64);
    }

    /// <summary>DERIVEKEY(seed, path) — deriva chiave BIP32 dal seed; restituisce {privateKey, publicKey, chainCode}.</summary>
    public static Dictionary<string, byte[]> DeriveKey(byte[] seed, string path)
    {
        var hmac = new System.Security.Cryptography.HMACSHA512(System.Text.Encoding.UTF8.GetBytes("Bitcoin seed"));
        var i = hmac.ComputeHash(seed);
        var il = i.Take(32).ToArray();
        var ir = i.Skip(32).ToArray();
        var curve = SecNamedCurves.GetByName("secp256k1");
        var domain = new ECDomainParameters(curve.Curve, curve.G, curve.N, curve.H);
        var privKeyParam = new ECPrivateKeyParameters(new Org.BouncyCastle.Math.BigInteger(1, il), domain);
        var q = domain.G.Multiply(privKeyParam.D);
        var pubKey = q.GetEncoded(true);
        return new Dictionary<string, byte[]>
        {
            ["privateKey"] = il,
            ["publicKey"] = pubKey,
            ["chainCode"] = ir
        };
    }

    /// <summary>GETPUBLICKEY(privateKey) — chiave pubblica secp256k1 compressa (33 byte).</summary>
    public static byte[] GetPublicKey(byte[] privateKey)
    {
        var curve = SecNamedCurves.GetByName("secp256k1");
        var domain = new ECDomainParameters(curve.Curve, curve.G, curve.N, curve.H);
        var privKeyParam = new ECPrivateKeyParameters(new Org.BouncyCastle.Math.BigInteger(1, privateKey), domain);
        var q = domain.G.Multiply(privKeyParam.D);
        return q.GetEncoded(true); // compressed
    }

    private static readonly string[] Bip39Words =
    [
        "abandon","ability","able","about","above","absent","absorb","abstract","absurd","abuse",
        "access","accident","account","accuse","achieve","acid","acoustic","acquire","across","act",
        "action","actor","actress","actual","adapt","add","addict","address","adjust","admit",
        "adult","advance","advice","aerobic","affair","afford","afraid","again","age","agent",
        "agree","ahead","aim","air","airport","aisle","alarm","album","alcohol","alert",
        "alien","all","alley","allow","almost","alone","alpha","already","also","alter",
        "always","amateur","amazing","among","amount","amused","analyst","anchor","ancient","anger",
        "angle","angry","animal","ankle","announce","annual","another","answer","antenna","antique",
        "anxiety","any","apart","apology","appear","apple","approve","april","arch","arctic",
        "area","arena","argue","arm","armed","armor","army","around","arrange","arrest",
        "arrive","arrow","art","artefact","artist","artwork","ask","aspect","assault","asset",
        "assist","assume","asthma","athlete","atom","attack","attend","attitude","attract","auction",
        "audit","august","aunt","author","auto","autumn","average","avocado","avoid","awake",
        "aware","away","awesome","awful","awkward","axis","baby","bachelor","bacon","badge",
        "bag","balance","balcony","ball","bamboo","banana","banner","bar","barely","bargain",
        "barrel","base","basic","basket","battle","beach","bean","beauty","because","become",
        "beef","before","begin","behave","behind","believe","below","belt","bench","benefit",
        "best","betray","better","between","beyond","bicycle","bid","bike","bind","biology",
        "bird","birth","bitter","black","blade","blame","blanket","blast","bleak","bless",
        "blind","blood","blossom","blouse","blue","blur","blush","board","boat","body",
        "boil","bomb","bone","bonus","book","boost","border","boring","borrow","boss",
        "bottom","bounce","box","boy","bracket","brain","brand","brass","brave","bread",
        "breeze","brick","bridge","brief","bright","bring","brisk","broccoli","broken","bronze",
        "broom","brother","brown","brush","bubble","buddy","budget","buffalo","build","bulb",
        "bulk","bullet","bundle","bunker","burden","burger","burst","bus","business","busy",
        "butter","buyer","buzz","cabbage","cabin","cable","cactus","cage","cake","call",
        "calm","camera","camp","can","canal","cancel","candy","cannon","canoe","canvas",
        "canyon","capable","capital","captain","car","carbon","card","cargo","carpet","carry",
        "cart","case","cash","casino","castle","casual","cat","catalog","catch","category",
        "cattle","caught","cause","caution","cave","ceiling","celery","cement","census","century",
        "cereal","certain","chair","chalk","champion","change","chaos","chapter","charge","chase",
        "chat","cheap","check","cheese","chef","cherry","chest","chicken","chief","child",
        "chimney","choice","choose","chronic","chuckle","chunk","churn","cigar","cinnamon","circle",
        "citizen","city","civil","claim","clap","clarify","claw","clay","clean","clerk",
        "clever","click","client","cliff","climb","clinic","clip","clock","clog","close",
        "cloth","cloud","clown","club","clump","cluster","clutch","coach","coast","coconut",
        "code","coffee","coil","coin","collect","color","column","combine","come","comfort",
        "comic","common","company","concert","conduct","confirm","congress","connect","consider","control",
        "convince","cook","cool","copper","copy","coral","core","corn","correct","cost",
        "cotton","couch","country","couple","course","cousin","cover","coyote","crack","cradle",
        "craft","cram","crane","crash","crater","crawl","crazy","cream","credit","creek",
        "crew","cricket","crime","crisp","critic","crop","cross","crouch","crowd","crucial",
        "cruel","cruise","crumble","crunch","crush","cry","crystal","cube","culture","cup",
        "cupboard","curious","current","curtain","curve","cushion","custom","cute","cycle","dad",
        "damage","damp","dance","danger","daring","dash","daughter","dawn","day","deal",
        "debate","debris","decade","december","decide","decline","decorate","decrease","deer","defense",
        "define","defy","degree","delay","deliver","demand","demise","denial","dentist","deny",
        "depart","depend","deposit","depth","deputy","derive","describe","desert","design","desk",
        "despair","destroy","detail","detect","develop","device","devote","diagram","dial","diamond",
        "diary","dice","diesel","diet","differ","digital","dignity","dilemma","dinner","dinosaur",
        "direct","dirt","disagree","discover","disease","dish","dismiss","disorder","display","distance",
        "divert","divide","divorce","dizzy","doctor","document","dog","doll","dolphin","domain",
        "donate","donkey","donor","door","dose","double","dove","draft","dragon","drama",
        "drastic","draw","dream","dress","drift","drill","drink","drip","drive","drop",
        "drum","dry","duck","dumb","dune","during","dust","dutch","duty","dwarf",
        "dynamic","eager","eagle","early","earn","earth","easily","east","easy","echo",
        "ecology","economy","edge","edit","educate","effort","egg","eight","either","elbow",
        "elder","electric","elegant","element","elephant","elevator","elite","else","embark","embody",
        "embrace","emerge","emotion","employ","empower","empty","enable","enact","end","endless",
        "endorse","enemy","energy","enforce","engage","engine","enhance","enjoy","enlist","enough",
        "enrich","enroll","ensure","enter","entire","entry","envelope","episode","equal","equip",
        "era","erase","erode","erosion","error","erupt","escape","essay","essence","estate",
        "eternal","ethics","evidence","evil","evoke","evolve","exact","example","excess","exchange",
        "excite","exclude","excuse","execute","exercise","exhaust","exhibit","exile","exist","exit",
        "exotic","expand","expect","expire","explain","expose","express","extend","extra","eye",
        "eyebrow","fabric","face","faculty","fade","faint","faith","fall","false","fame",
        "family","famous","fan","fancy","fantasy","farm","fashion","fat","fatal","father",
        "fatigue","fault","favorite","feature","february","federal","fee","feed","feel","female",
        "fence","festival","fetch","fever","few","fiber","fiction","field","figure","file",
        "film","filter","final","find","fine","finger","finish","fire","firm","first",
        "fiscal","fish","fit","fitness","fix","flag","flame","flash","flat","flavor",
        "flee","flight","flip","float","flock","floor","flower","fluid","flush","fly",
        "foam","focus","fog","foil","fold","follow","food","foot","force","forest",
        "forget","fork","fortune","forum","forward","fossil","foster","found","fox","fragile",
        "frame","frequent","fresh","friend","fringe","frog","front","frost","frown","frozen",
        "fruit","fuel","fun","funny","furnace","fury","future","gadget","gain","galaxy",
        "gallery","game","gap","garage","garbage","garden","garlic","garment","gas","gasp",
        "gate","gather","gauge","gaze","general","genius","genre","gentle","genuine","gesture",
        "ghost","giant","gift","giggle","ginger","giraffe","girl","give","glad","glance",
        "glare","glass","glide","glimpse","globe","gloom","glory","glove","glow","glue",
        "goat","goddess","gold","good","goose","gorilla","gospel","gossip","govern","gown",
        "grab","grace","grain","grant","grape","grass","gravity","great","green","grid",
        "grief","grit","grocery","group","grow","grunt","guard","guess","guide","guilt",
        "guitar","gun","gym","habit","hair","half","hammer","hamster","hand","happy",
        "harbor","hard","harsh","harvest","hat","have","hawk","hazard","head","health",
        "heart","heavy","hedgehog","height","hello","helmet","help","hen","hero","hidden",
        "high","hill","hint","hip","hire","history","hobby","hockey","hold","hole",
        "holiday","hollow","home","honey","hood","hope","horn","horror","horse","hospital",
        "host","hotel","hour","hover","hub","huge","human","humble","humor","hundred",
        "hungry","hunt","hurdle","hurry","hurt","husband","hybrid","ice","icon","idea",
        "identify","idle","ignore","ill","illegal","illness","image","imitate","immense","immune",
        "impact","impose","improve","impulse","inch","include","income","increase","index","indicate",
        "indoor","industry","infant","inflict","inform","inhale","inherit","initial","inject","injury",
        "inmate","inner","innocent","input","inquiry","insane","insect","inside","inspire","install",
        "intact","interest","into","invest","invite","involve","iron","island","isolate","issue",
        "item","ivory","jacket","jaguar","jar","jazz","jealous","jeans","jelly","jewel",
        "job","join","joke","journey","joy","judge","juice","jump","jungle","junior",
        "junk","just","kangaroo","keen","keep","ketchup","key","kick","kid","kidney",
        "kind","kingdom","kiss","kit","kitchen","kite","kitten","kiwi","knee","knife",
        "knock","know","lab","label","labor","ladder","lady","lake","lamp","language",
        "laptop","large","later","latin","laugh","laundry","lava","law","lawn","lawsuit",
        "layer","lazy","leader","leaf","learn","leave","lecture","left","leg","legal",
        "legend","leisure","lemon","lend","length","lens","leopard","lesson","letter","level",
        "liar","liberty","library","license","life","lift","light","like","limb","limit",
        "link","lion","liquid","list","little","live","lizard","load","loan","lobster",
        "local","lock","logic","lonely","long","loop","lottery","loud","lounge","love",
        "loyal","lucky","luggage","lumber","lunar","lunch","luxury","lyrics","machine","mad",
        "magic","magnet","maid","mail","main","major","make","mammal","man","manage",
        "mandate","mango","mansion","manual","maple","marble","march","margin","marine","market",
        "marriage","mask","mass","master","match","material","math","matrix","matter","maximum",
        "maze","meadow","mean","measure","meat","mechanic","medal","media","melody","melt",
        "member","memory","mention","menu","mercy","merge","merit","merry","mesh","message",
        "metal","method","middle","midnight","milk","million","mimic","mind","minimum","minor",
        "minute","miracle","mirror","misery","miss","mistake","mix","mixed","mixture","mobile",
        "model","modify","mom","moment","monitor","monkey","monster","month","moon","moral",
        "more","morning","mosquito","mother","motion","motor","mountain","mouse","move","movie",
        "much","muffin","mule","multiply","muscle","museum","mushroom","music","must","mutual",
        "myself","mystery","myth","naive","name","napkin","narrow","nasty","nation","nature",
        "near","neck","need","negative","neglect","neither","nephew","nerve","nest","net",
        "network","neutral","never","news","next","nice","night","noble","noise","nominee",
        "noodle","normal","north","nose","notable","note","nothing","notice","novel","now",
        "nuclear","number","nurse","nut","oak","obey","object","oblige","obscure","observe",
        "obtain","obvious","occur","ocean","october","odor","off","offer","office","often",
        "oil","okay","old","olive","olympic","omit","once","one","onion","online",
        "only","open","opera","opinion","oppose","option","orange","orbit","orchard","order",
        "ordinary","organ","orient","original","orphan","ostrich","other","outdoor","outer","output",
        "outside","oval","oven","over","own","owner","oxygen","oyster","ozone","pact",
        "paddle","page","pair","palace","palm","panda","panel","panic","panther","paper",
        "parade","parent","park","parrot","party","pass","patch","path","patient","patrol",
        "pattern","pause","pave","payment","peace","peanut","pear","peasant","pelican","pen",
        "penalty","pencil","people","pepper","perfect","permit","person","pet","phone","photo",
        "phrase","physical","piano","picnic","picture","piece","pig","pigeon","pill","pilot",
        "pink","pioneer","pipe","pistol","pitch","pizza","place","planet","plastic","plate",
        "play","please","pledge","pluck","plug","plunge","poem","poet","point","polar",
        "pole","police","pond","pony","pool","popular","portion","position","possible","post",
        "potato","pottery","poverty","powder","power","practice","praise","predict","prefer","prepare",
        "present","pretty","prevent","price","pride","primary","print","priority","prison","private",
        "prize","problem","process","produce","profit","program","project","promote","proof","property",
        "prosper","protect","proud","provide","public","pudding","pull","pulp","pulse","pumpkin",
        "punch","pupil","puppy","purchase","purity","purpose","purse","push","put","puzzle",
        "pyramid","quality","quantum","quarter","question","quick","quit","quiz","quote","rabbit",
        "raccoon","race","rack","radar","radio","rail","rain","raise","rally","ramp",
        "ranch","random","range","rapid","rare","rate","rather","raven","raw","razor",
        "ready","real","reason","rebel","rebuild","recall","receive","recipe","record","recycle",
        "reduce","reflect","reform","refuse","region","regret","regular","reject","relax","release",
        "relief","rely","remain","remember","remind","remove","render","renew","rent","reopen",
        "repair","repeat","replace","report","require","rescue","resemble","resist","resource","response",
        "result","retire","retreat","return","reunion","reveal","review","reward","rhythm","rib",
        "ribbon","rice","rich","ride","ridge","rifle","right","rigid","ring","riot",
        "ripple","risk","ritual","rival","river","road","roast","robot","robust","rocket",
        "romance","roof","rookie","room","rose","rotate","rough","round","route","royal",
        "rubber","rude","rug","rule","run","runway","rural","sad","saddle","sadness",
        "safe","sail","salad","salmon","salon","salt","salute","same","sample","sand",
        "satisfy","satoshi","sauce","sausage","save","say","scale","scan","scare","scatter",
        "scene","scheme","school","science","scissors","scorpion","scout","scrap","screen","script",
        "scrub","sea","search","season","seat","second","secret","section","security","seed",
        "seek","segment","select","sell","seminar","senior","sense","sentence","series","service",
        "session","settle","setup","seven","shadow","shaft","shallow","share","shed","shell",
        "sheriff","shield","shift","shine","ship","shiver","shock","shoe","shoot","shop",
        "short","shoulder","shove","shrimp","shrug","shuffle","shy","sibling","sick","side",
        "siege","sight","sign","silent","silk","silly","silver","similar","simple","since",
        "sing","siren","sister","situate","six","size","skate","sketch","ski","skill",
        "skin","skirt","skull","slab","slam","sleep","slender","slice","slide","slight",
        "slim","slogan","slot","slow","slush","small","smart","smile","smoke","smooth",
        "snack","snake","snap","sniff","snow","soap","soccer","social","sock","soda",
        "soft","solar","soldier","solid","solution","solve","someone","song","soon","sorry",
        "sort","soul","sound","soup","source","south","space","spare","spatial","spawn",
        "speak","special","speed","spell","spend","sphere","spice","spider","spike","spin",
        "spirit","split","spoil","sponsor","spoon","sport","spot","spray","spread","spring",
        "spy","square","squeeze","squirrel","stable","stadium","staff","stage","stairs","stamp",
        "stand","start","state","stay","steak","steel","stem","step","stereo","stick",
        "still","sting","stock","stomach","stone","stool","story","stove","strategy","street",
        "strike","strong","struggle","student","stuff","stumble","style","subject","submit","subway",
        "success","such","sudden","suffer","sugar","suggest","suit","summer","sun","sunny",
        "sunset","super","supply","supreme","sure","surface","surge","surprise","surround","survey",
        "suspect","sustain","swallow","swamp","swap","swarm","swear","sweet","swift","swim",
        "swing","switch","sword","symbol","symptom","syrup","system","table","tackle","tag",
        "tail","talent","talk","tank","tape","target","task","taste","tattoo","taxi",
        "teach","team","tell","ten","tenant","tennis","tent","term","test","text",
        "thank","that","theme","then","theory","there","they","thing","this","thought",
        "three","thrive","throw","thumb","thunder","ticket","tide","tiger","tilt","timber",
        "time","tiny","tip","tired","tissue","title","toast","tobacco","today","toddler",
        "toe","together","toilet","token","tomato","tomorrow","tone","tongue","tonight","tool",
        "tooth","top","topic","topple","torch","tornado","tortoise","toss","total","tourist",
        "toward","tower","town","toy","track","trade","traffic","tragic","train","transfer",
        "trap","trash","travel","tray","treat","tree","trend","trial","tribe","trick",
        "trigger","trim","trip","trophy","trouble","truck","true","truly","trumpet","trust",
        "truth","try","tube","tuition","tumble","tuna","tunnel","turkey","turn","turtle",
        "twelve","twenty","twice","twin","twist","two","type","typical","ugly","umbrella",
        "unable","unaware","uncle","uncover","under","undo","unfair","unfold","unhappy","uniform",
        "unique","unit","universe","unknown","unlock","until","unusual","unveil","update","upgrade",
        "uphold","upon","upper","upset","urban","urge","usage","use","used","useful",
        "useless","usual","utility","vacant","vacuum","vague","valid","valley","valve","van",
        "vanish","vapor","various","vast","vault","vehicle","velvet","vendor","venture","venue",
        "verb","verify","version","very","vessel","veteran","viable","vibrant","vicious","victory",
        "video","view","village","vintage","violin","virtual","virus","visa","visit","visual",
        "vital","vivid","vocal","voice","void","volcano","volume","vote","voyage","wage",
        "wagon","wait","walk","wall","walnut","want","warfare","warm","warrior","wash",
        "wasp","waste","water","wave","way","wealth","weapon","wear","weasel","weather",
        "web","wedding","weekend","weird","welcome","west","wet","whale","what","wheat",
        "wheel","when","where","whip","whisper","wide","width","wife","wild","will",
        "win","window","wine","wing","wink","winner","winter","wire","wisdom","wise",
        "wish","witness","wolf","woman","wonder","wood","wool","word","work","world",
        "worry","worth","wrap","wreck","wrestle","wrist","write","wrong","yard","year",
        "yellow","you","young","youth","zebra","zero","zone","zoo"
    ];

}
