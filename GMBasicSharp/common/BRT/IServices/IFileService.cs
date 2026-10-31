// FILE: Basic.Runtime.IServices.IFileService.cs

namespace Basic.Runtime;

/// <summary>Servizio file system per il BASIC Runtime.</summary>
public interface IFileService
{
    long Open(string path, string mode);
    void Close(long handle);
    void Write(long handle, string text);
    void WriteBytes(long handle, byte[] data);
    void WriteLine(long handle, string? text);
    string? ReadLine(long handle);
    string? ReadAll(long handle);
    long Seek(long handle, long offset, int origin);
    long GetPosition(long handle);
    long GetLength(long handle);
    /// <summary>Stream sottostante dell'handle (usato dal codec dei record RANDOM, PUT/GET #n, rec, UDT). Default: nessuno.</summary>
    Stream? GetStream(long handle) => null;
    /// <summary>True se non restano dati da leggere (EOF). Default: posizione ≥ lunghezza. Le
    /// implementazioni con un reader di riga BUFFERIZZATO devono sovrascrivere (EndOfStream):
    /// il buffer avanza lo stream sottostante dell'INTERO blocco letto, quindi la POSIZIONE è a
    /// fine file anche con righe ancora da leggere.</summary>
    bool IsEof(long handle) => GetPosition(handle) >= GetLength(handle);
    bool Exists(string path);
    void Lock(long handle, long offset, long length);
    void Unlock(long handle, long offset, long length);
    void Delete(string path);
    void Copy(string source, string dest);
    void Move(string source, string dest);
    long GetFileSize(string path);
    string[] GetFiles(string pattern);
    string[] GetDirectories(string path);
    string GetCurrentDirectory();
    void SetCurrentDirectory(string path);
    void CreateDirectory(string path);
    bool DirectoryExists(string path);
}
