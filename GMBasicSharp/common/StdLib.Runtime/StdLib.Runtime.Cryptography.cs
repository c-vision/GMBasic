// FILE: StdLib.Runtime.Cryptography.cs
// Dominio Cryptography della StdLib.Runtime: le 18 funzioni marcate NativeExecutionKind.Pure in
// StdLib.Cryptography.cs (hash, codifica hex/base64, cifratura simmetrica deterministica). PURO
// per costruzione: nessun metodo qui accede a stato che sopravvive oltre la singola chiamata,
// nessun handle/registry (quelle sono le funzioni Stateful — Sign/Verify/EncryptAsym/DecryptAsym/
// KeyGen/KeyExport/KeyImport/AesEncrypt(genera IV)/HashFile/RandomCryptoBytes — che restano fuori,
// gestite dal servizio crypto di ciascun motore). Rispecchia esattamente le firme di
// CommonLib.CryptoUtils (nessun riordino argomenti) — arità presa da IlBackend/IlRuntime.cs, che
// le implementava già correttamente contro CommonLib.CryptoUtils direttamente (a differenza di
// PCodeVm, che per MD5/SHA1/SHA256/SHA512/AESDECRYPT passava invece per un servizio host separato
// — vedi Fase 10 in docs/StdLib-Runtime-Plan.md).

namespace Basic.StdLibRuntime;

public static class CryptographyFunctions
{
    public static byte[] Md5(byte[] data) => CommonLib.CryptoUtils.CoreHashData("MD5", data);
    public static byte[] Sha1(byte[] data) => CommonLib.CryptoUtils.CoreHashData("SHA1", data);
    public static byte[] Sha256(byte[] data) => CommonLib.CryptoUtils.CoreHashData("SHA256", data);
    public static byte[] Sha512(byte[] data) => CommonLib.CryptoUtils.CoreHashData("SHA512", data);

    public static string StrMd5(byte[] data) => CommonLib.CryptoUtils.CoreToHexString(Md5(data));
    public static string StrSha1(byte[] data) => CommonLib.CryptoUtils.CoreToHexString(Sha1(data));
    public static string StrSha256(byte[] data) => CommonLib.CryptoUtils.CoreToHexString(Sha256(data));
    public static string StrSha512(byte[] data) => CommonLib.CryptoUtils.CoreToHexString(Sha512(data));

    public static byte[] AesDecrypt(byte[] data, string key) => CommonLib.CryptoUtils.CoreAesDecrypt(data, key);

    public static byte[] ChaCha20Encrypt(byte[] data, byte[] key, byte[] nonce) => CommonLib.CryptoUtils.CoreChaCha20Encrypt(data, key, nonce);
    public static byte[] ChaCha20Decrypt(byte[] data, byte[] key, byte[] nonce) => CommonLib.CryptoUtils.CoreChaCha20Decrypt(data, key, nonce);
    public static (byte[] Ciphertext, byte[] Tag) ChaCha20Poly1305Encrypt(byte[] plaintext, byte[] key, byte[] nonce, byte[]? aad) => CommonLib.CryptoUtils.CoreChaCha20Poly1305Encrypt(plaintext, key, nonce, aad);
    public static byte[]? ChaCha20Poly1305Decrypt(byte[] ciphertext, byte[] tag, byte[] key, byte[] nonce, byte[]? aad) => CommonLib.CryptoUtils.CoreChaCha20Poly1305Decrypt(ciphertext, tag, key, nonce, aad);

    public static string ToBase64(byte[] data) => CommonLib.CryptoUtils.CoreToBase64(data);
    public static byte[] FromBase64(string value) => CommonLib.CryptoUtils.CoreFromBase64(value);
    public static string ToHex(byte[] data) => CommonLib.CryptoUtils.CoreToHexString(data);
    public static byte[] FromHex(string value) => CommonLib.CryptoUtils.CoreFromHex(value);

    public static string HmacSha256(byte[] data, byte[] key) => CommonLib.CryptoUtils.CoreToHexString(CommonLib.CryptoUtils.CoreHmac("SHA256", key, data));
    public static string HmacSha384(byte[] data, byte[] key) => CommonLib.CryptoUtils.CoreToHexString(CommonLib.CryptoUtils.CoreHmac("SHA384", key, data));
    public static string HmacSha512(byte[] data, byte[] key) => CommonLib.CryptoUtils.CoreToHexString(CommonLib.CryptoUtils.CoreHmac("SHA512", key, data));

    public static string ToBase64Url(byte[] data) => CommonLib.CryptoUtils.CoreToBase64Url(data);
    public static byte[] FromBase64Url(string value) => CommonLib.CryptoUtils.CoreFromBase64Url(value);

    public static bool PasswordVerify(string password, string hash) => CommonLib.CryptoUtils.CorePasswordVerify(password, hash);
}
