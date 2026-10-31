// FILE: Basic.Runtime.Services.CryptoService.cs

using System.Security.Cryptography;
using System.Text;

namespace Basic.Runtime.Services;

public class CryptoService : ICryptoService
{
    public string HashMD5(string text) => HashToString(MD5.HashData(GetBytes(text)));
    public string HashSHA1(string text) => HashToString(SHA1.HashData(GetBytes(text)));
    public string HashSHA256(string text) => HashToString(SHA256.HashData(GetBytes(text)));
    public string HashSHA512(string text) => HashToString(SHA512.HashData(GetBytes(text)));

    public string HashHMAC(string key, string text)
    {
        using var hmac = new HMACSHA256(GetBytes(key));
        return HashToString(hmac.ComputeHash(GetBytes(text)));
    }

    public string EncryptAES(string plaintext, string key)
    {
        using var aes = Aes.Create();
        aes.Key = SHA256.HashData(GetBytes(key)).AsSpan(0, 32).ToArray();
        aes.GenerateIV();
        using var encryptor = aes.CreateEncryptor();
        var input = GetBytes(plaintext);
        var result = encryptor.TransformFinalBlock(input, 0, input.Length);
        return Convert.ToBase64String(aes.IV.Concat(result).ToArray());
    }

    public string DecryptAES(string ciphertext, string key)
    {
        var data = Convert.FromBase64String(ciphertext);
        using var aes = Aes.Create();
        aes.Key = SHA256.HashData(GetBytes(key)).AsSpan(0, 32).ToArray();
        aes.IV = data.AsSpan(0, 16).ToArray();
        using var decryptor = aes.CreateDecryptor();
        var result = decryptor.TransformFinalBlock(data, 16, data.Length - 16);
        return Encoding.UTF8.GetString(result);
    }

    public string GenerateRandomKey(int length)
    {
        var key = new byte[length];
        RandomNumberGenerator.Fill(key);
        return Convert.ToBase64String(key);
    }

    public string EncodeBase64(byte[] data) => Convert.ToBase64String(data);
    public byte[] DecodeBase64(string text) => Convert.FromBase64String(text);

    private static byte[] GetBytes(string s) => Encoding.UTF8.GetBytes(s);
    private static string HashToString(byte[] hash) => Convert.ToHexStringLower(hash);
}
