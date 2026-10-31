// FILE: Basic.Runtime.IServices.ICryptoService.cs

namespace Basic.Runtime;

/// <summary>Servizio crittografico per il BASIC Runtime.</summary>
public interface ICryptoService
{
    string HashMD5(string text);
    string HashSHA1(string text);
    string HashSHA256(string text);
    string HashSHA512(string text);
    string HashHMAC(string key, string text);
    string EncryptAES(string plaintext, string key);
    string DecryptAES(string ciphertext, string key);
    string GenerateRandomKey(int length);
    string EncodeBase64(byte[] data);
    byte[] DecodeBase64(string text);
}
