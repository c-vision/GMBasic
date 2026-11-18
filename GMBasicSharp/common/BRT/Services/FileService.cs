// FILE: Basic.Runtime.Services.FileService.cs

namespace Basic.Runtime.Services;

/// <summary>Implementazione di IFileService basata su System.IO.</summary>
public sealed class FileService : IFileService
{
    private readonly Dictionary<long, FileStream> _handles = new();
    private long _nextHandle = 1;
    // Reader di riga PERSISTENTE per handle (2026-10-01, fix Bugs/(solved) - PRINTFile-
    // Argomenti-Stringa-PCode-IL-Divergenza.md): ReadLine creava PRIMA un nuovo StreamReader
    // AD OGNI CHIAMATA — un reader bufferizza internamente (default 1KB) più byte di quanti
    // ReadLine() ne restituisca, e avanza lo STREAM sottostante di quanto ha BUFFERIZZATO (non
    // di quanto ha restituito). Scartando il reader a ogni chiamata (era `using`), la chiamata
    // successiva su un NUOVO reader partiva dalla posizione già avanzata dal buffering del
    // reader precedente — la SECONDA riga (e successive) andava persa. Un reader persistente
    // per handle, creato alla prima lettura e riusato, risolve il problema senza toccare
    // Write/Seek (che operano ancora direttamente su _handles[handle], invariati).
    private readonly Dictionary<long, StreamReader> _lineReaders = new();

    public long Open(string path, string mode)
    {
        var fm = mode.ToLower() switch
        {
            "input" or "r" => FileMode.Open,
            "output" or "w" => FileMode.Create,
            "append" or "a" => FileMode.Append,
            "binary" or "b" => FileMode.OpenOrCreate,
            _ => FileMode.OpenOrCreate
        };
        var fa = mode.ToLower() switch
        {
            "input" => FileAccess.Read,
            "output" => FileAccess.Write,
            // FileMode.Append + FileAccess.ReadWrite è INVALIDO per FileStream (ArgumentException
            // "Combining FileMode: Append with FileAccess: ReadWrite is invalid") e l'eccezione
            // veniva ingoiata dall'OPEN → il file non si apriva e il PRINT # successivo perdeva
            // la scrittura in silenzio (APPEND vuoto su PCode).
            "append" => FileAccess.Write,
            _ => FileAccess.ReadWrite
        };
        var fs = new FileStream(path, fm, fa);
        var handle = _nextHandle++;
        _handles[handle] = fs;
        return handle;
    }

    /// <summary>Stream sottostante dell'handle (per il codec dei record RANDOM), null se non aperto.</summary>
    public Stream? GetStream(long handle) => _handles.TryGetValue(handle, out var fs) ? fs : null;

    public void Close(long handle)
    {
        if (_lineReaders.Remove(handle, out var rdr)) rdr.Dispose();
        if (_handles.TryGetValue(handle, out var fs))
        {
            fs.Close();
            _handles.Remove(handle);
        }
    }

    public void Write(long handle, string text)
    {
        if (_handles.TryGetValue(handle, out var fs))
        {
            var bytes = System.Text.Encoding.UTF8.GetBytes(text);
            fs.Write(bytes, 0, bytes.Length);
        }
    }

    public void WriteBytes(long handle, byte[] data)
    {
        if (_handles.TryGetValue(handle, out var fs))
            fs.Write(data, 0, data.Length);
    }

    public void WriteLine(long handle, string? text)
    {
        Write(handle, (text ?? "") + Environment.NewLine);
    }

    public string? ReadLine(long handle)
    {
        if (!_handles.TryGetValue(handle, out var fs)) return null;
        if (!_lineReaders.TryGetValue(handle, out var reader))
        {
            reader = new StreamReader(fs, System.Text.Encoding.UTF8, leaveOpen: true);
            _lineReaders[handle] = reader;
        }
        return reader.EndOfStream ? null : reader.ReadLine();
    }

    public string? ReadAll(long handle)
    {
        if (!_handles.TryGetValue(handle, out var fs)) return null;
        using var reader = new StreamReader(fs, System.Text.Encoding.UTF8, leaveOpen: true);
        return reader.ReadToEnd();
    }

    public long Seek(long handle, long offset, int origin)
    {
        if (!_handles.TryGetValue(handle, out var fs)) return -1;
        var so = (SeekOrigin)origin;
        return fs.Seek(offset, so);
    }

    public long GetPosition(long handle)
    {
        if (!_handles.TryGetValue(handle, out var fs)) return -1;
        return fs.Position;
    }

    public long GetLength(long handle)
    {
        if (!_handles.TryGetValue(handle, out var fs)) return -1;
        return fs.Length;
    }

    /// <summary>EOF: se esiste un reader di riga per l'handle (ReadLine), usa il suo
    /// <see cref="StreamReader.EndOfStream"/> — la POSIZIONE del FileStream è a fine file
    /// perché il buffer ha già letto avanti l'intero blocco (bug: `DO WHILE NOT EOF(F)`
    /// leggeva una sola riga su PCode). Altrimenti posizione ≥ lunghezza.</summary>
    public bool IsEof(long handle)
    {
        if (_lineReaders.TryGetValue(handle, out var reader)) return reader.EndOfStream;
        if (!_handles.TryGetValue(handle, out var fs)) return true;
        return fs.Position >= fs.Length;
    }

    public bool Exists(string path) => File.Exists(path);
#pragma warning disable CA1416 // FileStream.Lock non supportato su macOS — gestito da try/catch (no-op)
    public void Lock(long handle, long offset, long length)
    {
        if (_handles.TryGetValue(handle, out var fs))
            fs.Lock(offset, length);
    }

    public void Unlock(long handle, long offset, long length)
    {
        if (_handles.TryGetValue(handle, out var fs))
            fs.Unlock(offset, length);
    }

    public void Delete(string path) => File.Delete(path);
    public void Copy(string source, string dest) => File.Copy(source, dest, true);
    public void Move(string source, string dest) => File.Move(source, dest, true);
    public long GetFileSize(string path) => new FileInfo(path).Length;
    public string[] GetFiles(string pattern) => Directory.GetFiles(".", pattern);
    public string[] GetDirectories(string path) => Directory.GetDirectories(path);
    public string GetCurrentDirectory() => Directory.GetCurrentDirectory();
    public void SetCurrentDirectory(string path) => Directory.SetCurrentDirectory(path);
    public void CreateDirectory(string path) => Directory.CreateDirectory(path);
    public bool DirectoryExists(string path) => Directory.Exists(path);
}
