// FILE: Basic.DBF.DbfMemo.cs

using System.Buffers;
using System.Runtime.InteropServices;
using System.Text;

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

/// <summary>
/// Fornisce un metodo factory per creare l'istanza corretta di IMemoFile
/// in base al formato del file (.DBT, .FPT) e alla versione del file DBF associato.
/// </summary>
internal static class DbfMemoFactory
{
    public static IMemoFile Create(string dbfFilePath, DbfVersion dbfVersion, bool createIfMissing = false)
    {
        if (!dbfVersion.SupportsMemoFields())
            return NullMemoFile.Instance;
        
        var memoFilePath = FindMemoFile(dbfFilePath);
        if (memoFilePath == null && createIfMissing)
        {
            memoFilePath = dbfVersion is DbfVersion.VisualFoxPro or DbfVersion.VisualFoxProWithMemo
                ? Path.ChangeExtension(dbfFilePath, ".fpt")
                : Path.ChangeExtension(dbfFilePath, ".dbt");
            CreateEmptyMemoFile(memoFilePath, dbfVersion);
        }

        if (memoFilePath == null)
            return NullMemoFile.Instance;

        var extension = Path.GetExtension(memoFilePath).ToLowerInvariant();
        return extension switch
        {
            ".fpt" => new FptFile(memoFilePath, writable: createIfMissing),
            ".dbt" => CreateDbtMemoFile(memoFilePath, dbfVersion, createIfMissing),
            _ => NullMemoFile.Instance
        };
    }

    private static string? FindMemoFile(string dbfFilePath)
    {
        var basePath = Path.ChangeExtension(dbfFilePath, null);
        return new[] { ".fpt", ".dbt" }.Select(ext => basePath + ext).FirstOrDefault(File.Exists);
    }

    private static void CreateEmptyMemoFile(string memoFilePath, DbfVersion dbfVersion)
    {
        if (dbfVersion is DbfVersion.VisualFoxPro or DbfVersion.VisualFoxProWithMemo)
        {
            using var fs = new FileStream(memoFilePath, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            var header = new byte[512];
            header[6] = 0x40;
            header[7] = 0x00;
            fs.Write(header, 0, header.Length);
            return;
        }

        File.Create(memoFilePath).Dispose();
    }

    private static IMemoFile CreateDbtMemoFile(string memoFilePath, DbfVersion dbfVersion, bool writable)
    {
        return dbfVersion == DbfVersion.DBase3PlusWithMemo 
            ? new Db3MemoFile(memoFilePath, writable) 
            : new Db4MemoFile(memoFilePath, writable);
    }
}

/// <summary>
/// Lettore/scrittore per file memo di dBase III (.DBT).
/// </summary>
internal sealed class Db3MemoFile : IMemoFile
{
    private const int BlockSize = 512;
    private const byte MemoTerminator = 0x1A;
    private readonly FileStream? _stream;
    private readonly Lock _lock = new();

    public Db3MemoFile(string filePath, bool writable = false)
    {
        try
        {
            if (!File.Exists(filePath) && !writable)
            {
                return;
            }

            var access = writable ? FileAccess.ReadWrite : FileAccess.Read;
            var mode = File.Exists(filePath) ? FileMode.Open : FileMode.Create;
            _stream = new FileStream(filePath, mode, access, FileShare.ReadWrite);
        }
        catch { /* Ignora se il file non può essere aperto */ }
    }

    public MemoData? GetMemo(int index)
    {
        if (_stream is null || index <= 0) return null;

        lock (_lock)
        {
            var position = (long)index * BlockSize;
            if (position >= _stream.Length) return null;
            _stream.Position = position;

            using var ms = new MemoryStream();
            var buffer = ArrayPool<byte>.Shared.Rent(BlockSize);
            try
            {
                while (true)
                {
                    var bytesRead = _stream.Read(buffer, 0, BlockSize);
                    if (bytesRead == 0) break;
                    
                    var span = buffer.AsSpan(0, bytesRead);
                    var termIndex = span.IndexOf(MemoTerminator);

                    if (termIndex != -1)
                    {
                        ms.Write(span[..termIndex]);
                        break;
                    }
                    ms.Write(span);
                }
                return ms.Length > 0 ? new TextMemo(ms.ToArray()) : null;
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(buffer);
            }
        }
    }

    public int WriteMemo(ReadOnlySpan<byte> data, bool isText = true)
    {
        if (_stream is null || !isText || data.IsEmpty) return 0;

        lock (_lock)
        {
            var blockIndex = AllocateBlock();
            _stream.Position = (long)blockIndex * BlockSize;

            using var payload = new MemoryStream(data.Length + 1);
            payload.Write(data);
            payload.WriteByte(MemoTerminator);
            var bytes = payload.ToArray();

            Span<byte> block = stackalloc byte[BlockSize];
            for (var offset = 0; offset < bytes.Length; offset += BlockSize)
            {
                var chunk = Math.Min(BlockSize, bytes.Length - offset);
                if (chunk < BlockSize)
                {
                    block.Clear();
                    bytes.AsSpan(offset, chunk).CopyTo(block);
                    _stream.Write(block);
                }
                else
                {
                    _stream.Write(bytes, offset, chunk);
                }
            }

            _stream.Flush();
            return blockIndex;
        }
    }

    private int AllocateBlock()
    {
        if (_stream!.Length == 0)
        {
            // ponytail: blocco 0 riservato per compat dBase III
            _stream.SetLength(BlockSize);
            return 1;
        }

        var remainder = _stream.Length % BlockSize;
        if (remainder != 0)
        {
            _stream.SetLength(_stream.Length + (BlockSize - remainder));
        }

        return (int)(_stream.Length / BlockSize);
    }

    public void Dispose() => _stream?.Dispose();
}

/// <summary>
/// Lettore/scrittore per file memo di dBase IV (.DBT).
/// </summary>
internal sealed class Db4MemoFile : IMemoFile
{
    private const int BlockSize = 512;
    private readonly FileStream? _stream;
    private readonly Lock _lock = new();

    public Db4MemoFile(string filePath, bool writable = false)
    {
        try
        {
            if (!File.Exists(filePath) && !writable)
            {
                return;
            }

            var access = writable ? FileAccess.ReadWrite : FileAccess.Read;
            var mode = File.Exists(filePath) ? FileMode.Open : FileMode.Create;
            _stream = new FileStream(filePath, mode, access, FileShare.ReadWrite);
        }
        catch { /* Ignora */ }
    }

    public MemoData? GetMemo(int index)
    {
        if (_stream is null || index <= 0) return null;
        lock (_lock)
        {
            var position = (long)index * BlockSize;
            if (position + 8 > _stream.Length) return null;
            _stream.Position = position;

            Span<byte> headerBuffer = stackalloc byte[8];
            _stream.ReadExactly(headerBuffer);
            
            var length = MemoryMarshal.Read<uint>(headerBuffer[4..]);
            if (length == 0 || length > _stream.Length - _stream.Position) return null;
            
            var data = new byte[length];
            _stream.ReadExactly(data, 0, (int)length);
            return new TextMemo(data);
        }
    }

    public int WriteMemo(ReadOnlySpan<byte> data, bool isText = true)
    {
        if (_stream is null || !isText) return 0;

        lock (_lock)
        {
            var blockIndex = AllocateBlock();
            _stream.Position = (long)blockIndex * BlockSize;

            Span<byte> header = stackalloc byte[8];
            header.Clear();
            MemoryMarshal.Write(header[4..], (uint)data.Length);
            _stream.Write(header);
            if (!data.IsEmpty)
            {
                _stream.Write(data);
            }

            var used = 8 + data.Length;
            var padding = (BlockSize - (used % BlockSize)) % BlockSize;
            if (padding > 0)
            {
                _stream.Write(new byte[padding]);
            }

            _stream.Flush();
            return blockIndex;
        }
    }

    private int AllocateBlock()
    {
        if (_stream!.Length == 0)
        {
            _stream.SetLength(BlockSize);
            return 1;
        }

        var remainder = _stream.Length % BlockSize;
        if (remainder != 0)
        {
            _stream.SetLength(_stream.Length + (BlockSize - remainder));
        }

        return (int)(_stream.Length / BlockSize);
    }

    public void Dispose() => _stream?.Dispose();
}

/// <summary>
/// Lettore/scrittore per file memo di Visual FoxPro (.FPT).
/// </summary>
internal sealed class FptFile : IMemoFile
{
    private const int HeaderSize = 512;
    private const int BlockHeaderSize = 8;
    private const int LockTimeoutMs = 5000;
    private readonly FileStream? _stream;
    private readonly ushort _blockSize;
    private readonly Lock _lock = new();
    private readonly bool _writable;
    private readonly Stack<uint> _freeBlocks = new(); // XSharp: free block list

    public FptFile(string filePath, bool writable = false)
    {
        _writable = writable;
        try
        {
            var access = writable ? FileAccess.ReadWrite : FileAccess.Read;
            var mode = File.Exists(filePath) ? FileMode.Open : FileMode.Create;
            _stream = new FileStream(filePath, mode, access, FileShare.ReadWrite);
            if (_stream.Length < HeaderSize)
            {
                if (writable)
                {
                    var header = new byte[HeaderSize];
                    header[6] = 0x40;
                    header[7] = 0x00;
                    _stream.Write(header, 0, header.Length);
                }
                else
                {
                    return;
                }
            }

            Span<byte> headerBuffer = stackalloc byte[HeaderSize];
            _stream.Position = 0;
            _stream.ReadExactly(headerBuffer);
            var blockSizeBytes = headerBuffer.Slice(6, 2).ToArray();
            if (BitConverter.IsLittleEndian) Array.Reverse(blockSizeBytes);
            _blockSize = BitConverter.ToUInt16(blockSizeBytes, 0);
            if (_blockSize == 0) _blockSize = 64;
            // Load free block list (XSharp)
            if (_writable) LoadFreeBlocks();
        }
        catch { /* Ignora */ }
    }

    /// <summary>Carica i blocchi liberi dall'header (linked list).</summary>
    private void LoadFreeBlocks()
    {
        if (_stream == null) return;
        _stream.Position = 0;
        Span<byte> hdr = stackalloc byte[4];
        _stream.ReadExactly(hdr);
        if (BitConverter.IsLittleEndian) hdr.Reverse();
        var nextFree = BitConverter.ToUInt32(hdr);
        var visited = new HashSet<uint>();
        var entry = new byte[8];
        while (nextFree > 0 && nextFree * (long)_blockSize < _stream.Length && !visited.Contains(nextFree))
        {
            visited.Add(nextFree);
            _freeBlocks.Push(nextFree);
            var pos = (long)nextFree * _blockSize;
            _stream.Position = pos;
            _stream.ReadExactly(entry);
            nextFree = BitConverter.ToUInt32(entry);
        }
    }

    /// <summary>Rilascia un blocco nella free list.</summary>
    public void FreeBlock(int blockIndex)
    {
        if (_stream == null || !_writable || blockIndex <= 0) return;
        lock (_lock)
        {
            _stream.Position = 0;
            Span<byte> hdr = stackalloc byte[4];
            _stream.ReadExactly(hdr);
            if (BitConverter.IsLittleEndian) hdr.Reverse();
            uint oldNextFree = BitConverter.ToUInt32(hdr);

            var pos = (long)blockIndex * _blockSize;
            Span<byte> entry = stackalloc byte[8];
            entry.Clear();
            var nextBytes = BitConverter.GetBytes(oldNextFree);
            if (BitConverter.IsLittleEndian) Array.Reverse(nextBytes);
            nextBytes.CopyTo(entry);
            _stream.Position = pos;
            _stream.Write(entry);

            var blockBytes = BitConverter.GetBytes((uint)blockIndex);
            if (BitConverter.IsLittleEndian) Array.Reverse(blockBytes);
            _stream.Position = 0;
            _stream.Write(blockBytes);
            _stream.Flush();
            _freeBlocks.Push((uint)blockIndex);
        }
    }

    public MemoData? GetMemo(int index)
    {
        if (_stream is null || _blockSize == 0 || index <= 0) return null;
        lock (_lock)
        {
            var position = (long)index * _blockSize;
            if (position + BlockHeaderSize > _stream.Length) return null;
            _stream.Position = position;

            Span<byte> blockHeaderBuffer = stackalloc byte[BlockHeaderSize];
            _stream.ReadExactly(blockHeaderBuffer);

            var typeBytes = blockHeaderBuffer[..4].ToArray();
            var lengthBytes = blockHeaderBuffer.Slice(4, 4).ToArray();
            if (BitConverter.IsLittleEndian) { Array.Reverse(typeBytes); Array.Reverse(lengthBytes); }
            var type = BitConverter.ToUInt32(typeBytes, 0);
            var length = BitConverter.ToUInt32(lengthBytes, 0);

            if (length == 0 || length > _stream.Length - _stream.Position) return null;
            
            var data = new byte[length];
            _stream.ReadExactly(data, 0, (int)length);
            
            return type == 1 ? new TextMemo(data) : new BinaryMemo(data);
        }
    }

    public int WriteMemo(ReadOnlySpan<byte> data, bool isText = true)
    {
        if (_stream is null || !_writable) return 0;

        lock (_lock)
        {
            var blockIndex = AllocateBlock();
            var pos = (long)blockIndex * _blockSize;

            // Lock retry (XSharp)
            var deadline = Environment.TickCount64 + LockTimeoutMs;
            bool locked = false;
            while (Environment.TickCount64 < deadline)
            {
                try { if (OperatingSystem.IsWindows()) { _stream.Lock(pos, _blockSize); } locked = true; break; }
                catch (IOException) { Thread.Sleep(10); }
            }

            _stream.Position = pos;

            Span<byte> blockHeader = stackalloc byte[BlockHeaderSize];
            blockHeader.Clear();
            var type = isText ? 1u : 0u;
            var typeBytes = BitConverter.GetBytes(type);
            var lengthBytes = BitConverter.GetBytes((uint)data.Length);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(typeBytes);
                Array.Reverse(lengthBytes);
            }

            typeBytes.CopyTo(blockHeader);
            lengthBytes.CopyTo(blockHeader[4..]);
            _stream.Write(blockHeader);
            if (!data.IsEmpty)
            {
                _stream.Write(data);
            }

            var used = BlockHeaderSize + data.Length;
            var padding = (_blockSize - (used % _blockSize)) % _blockSize;
            if (padding > 0)
            {
                _stream.Write(new byte[padding]);
            }

            _stream.Flush();
            if (locked) try { if (OperatingSystem.IsWindows()) { _stream.Unlock(pos, _blockSize); } } catch { }
            return blockIndex;
        }
    }

    private int AllocateBlock()
    {
        // Reuse free block if available (XSharp)
        if (_freeBlocks.Count > 0)
            return (int)_freeBlocks.Pop();

        if (_stream!.Length <= HeaderSize)
        {
            _stream.SetLength(HeaderSize + _blockSize);
            return 1;
        }

        var remainder = (_stream.Length - HeaderSize) % _blockSize;
        if (remainder != 0)
        {
            _stream.SetLength(_stream.Length + (_blockSize - remainder));
        }

        return (int)((_stream.Length - HeaderSize) / _blockSize);
    }

    public void Dispose() => _stream?.Dispose();
}
