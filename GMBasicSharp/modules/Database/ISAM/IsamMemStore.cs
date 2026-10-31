// FILE: Basic.ISAM.IsamMemStore

using System.Collections.Concurrent;
using System.Text;

namespace Basic.ISAM;

/// <summary>
/// Process-wide registry of in-memory ISAM table byte stores keyed by <see cref="IsamMemPath"/> name.
/// ponytail: single-process; no disk WAL; tables vanish when the process exits.
/// </summary>
internal static class IsamMemStore
{
    private static readonly ConcurrentDictionary<string, IsamMemTable> Tables =
        new(StringComparer.OrdinalIgnoreCase);

    internal static bool Exists(string key) => Tables.ContainsKey(key);

    internal static void CreateNew(string key, Config.IsamSchema schema, Encoding fileEncoding)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        var bytes = IsamFileHandle.BuildNewFileBytes(schema, fileEncoding);
        Tables[key] = new IsamMemTable(bytes);
    }

    internal static IsamMemStream OpenStream(string key)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        if (!Tables.TryGetValue(key, out var table))
        {
            throw new FileNotFoundException($"In-memory ISAM table '{key}' was not found.");
        }

        return new IsamMemStream(table);
    }

    /// <summary>Test-only: drops a table from the registry.</summary>
    internal static void Remove(string key) => Tables.TryRemove(key, out _);
}

/// <summary>Shared byte buffer for one <c>MEM:</c> table.</summary>
internal sealed class IsamMemTable
{
    private byte[] _data;
    private readonly object _sync = new();

    internal IsamMemTable(byte[] initialData) =>
        _data = initialData ?? throw new ArgumentNullException(nameof(initialData));

    internal long Length
    {
        get
        {
            lock (_sync)
            {
                return _data.Length;
            }
        }
    }

    internal void SetLength(long value)
    {
        if (value < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(value));
        }

        lock (_sync)
        {
            if (value == _data.Length)
            {
                return;
            }

            if (value < _data.Length)
            {
                Array.Resize(ref _data, (int)value);
                return;
            }

            var grown = new byte[value];
            Buffer.BlockCopy(_data, 0, grown, 0, _data.Length);
            _data = grown;
        }
    }

    internal int Read(long position, byte[] buffer, int offset, int count)
    {
        lock (_sync)
        {
            if (position >= _data.Length)
            {
                return 0;
            }

            var available = (int)Math.Min(count, _data.Length - position);
            Buffer.BlockCopy(_data, (int)position, buffer, offset, available);
            return available;
        }
    }

    internal void ReadSpan(long position, Span<byte> destination)
    {
        lock (_sync)
        {
            if (position + destination.Length > _data.Length)
            {
                throw new EndOfStreamException(
                    $"Lettura incompleta a offset {position}: attesi {destination.Length} byte, disponibili {_data.Length - position}.");
            }

            _data.AsSpan((int)position, destination.Length).CopyTo(destination);
        }
    }

    internal void Write(long position, ReadOnlySpan<byte> source)
    {
        lock (_sync)
        {
            var required = position + source.Length;
            if (required > _data.Length)
            {
                var newLen = Math.Max(required, _data.Length == 0 ? required : (long)(_data.Length * 1.5));
                if (newLen < required)
                {
                    newLen = required;
                }

                var grown = new byte[newLen];
                Buffer.BlockCopy(_data, 0, grown, 0, _data.Length);
                _data = grown;
            }

            source.CopyTo(_data.AsSpan((int)position));
        }
    }
}

/// <summary>Per-handle view over a shared <see cref="IsamMemTable"/> (independent cursor).</summary>
internal sealed class IsamMemStream : Stream
{
    private readonly IsamMemTable _table;
    private long _position;

    internal IsamMemStream(IsamMemTable table) => _table = table;

    public override bool CanRead => true;

    public override bool CanSeek => true;

    public override bool CanWrite => true;

    public override long Length => _table.Length;

    public override long Position
    {
        get => _position;
        set
        {
            if (value < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(value));
            }

            _position = value;
        }
    }

    public override void Flush()
    {
    }

    public override int Read(byte[] buffer, int offset, int count)
    {
        ArgumentNullException.ThrowIfNull(buffer);
        var read = _table.Read(_position, buffer, offset, count);
        _position += read;
        return read;
    }

    public override long Seek(long offset, SeekOrigin origin)
    {
        _position = origin switch
        {
            SeekOrigin.Begin => offset,
            SeekOrigin.Current => _position + offset,
            SeekOrigin.End => _table.Length + offset,
            _ => throw new ArgumentOutOfRangeException(nameof(origin)),
        };

        if (_position < 0)
        {
            throw new IOException("Seek before beginning of MEM table.");
        }

        return _position;
    }

    public override void SetLength(long value) => _table.SetLength(value);

    public override void Write(byte[] buffer, int offset, int count)
    {
        ArgumentNullException.ThrowIfNull(buffer);
        _table.Write(_position, buffer.AsSpan(offset, count));
        _position += count;
    }

    internal void TableReadAt(long offset, Span<byte> destination)
    {
        _table.ReadSpan(offset, destination);
    }

    internal void TableWriteAt(long offset, ReadOnlySpan<byte> data)
    {
        _table.Write(offset, data);
    }

    protected override void Dispose(bool disposing)
    {
        // ponytail: shared table outlives the handle; do not remove from IsamMemStore here.
    }
}
