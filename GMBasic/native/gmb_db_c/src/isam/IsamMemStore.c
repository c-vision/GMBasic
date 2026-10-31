/* FILE: native/gmb_db_c/src/ISAM/IsamMemStore.c */
/* Port 1:1 di modules/Database/ISAM/IsamMemStore.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamMemStore
//
//using System.Collections.Concurrent;
//using System.Text;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Process-wide registry of in-memory ISAM table byte stores keyed by <see cref="IsamMemPath"/> name.
///// ponytail: single-process; no disk WAL; tables vanish when the process exits.
///// </summary>
//internal static class IsamMemStore
//{
//    private static readonly ConcurrentDictionary<string, IsamMemTable> Tables =
//        new(StringComparer.OrdinalIgnoreCase);
//
//    internal static bool Exists(string key) => Tables.ContainsKey(key);
//
//    internal static void CreateNew(string key, Config.IsamSchema schema, Encoding fileEncoding)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(key);
//        var bytes = IsamFileHandle.BuildNewFileBytes(schema, fileEncoding);
//        Tables[key] = new IsamMemTable(bytes);
//    }
//
//    internal static IsamMemStream OpenStream(string key)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(key);
//        if (!Tables.TryGetValue(key, out var table))
//        {
//            throw new FileNotFoundException($"In-memory ISAM table '{key}' was not found.");
//        }
//
//        return new IsamMemStream(table);
//    }
//
//    /// <summary>Test-only: drops a table from the registry.</summary>
//    internal static void Remove(string key) => Tables.TryRemove(key, out _);
//}
//
///// <summary>Shared byte buffer for one <c>MEM:</c> table.</summary>
//internal sealed class IsamMemTable
//{
//    private byte[] _data;
//    private readonly object _sync = new();
//
//    internal IsamMemTable(byte[] initialData) =>
//        _data = initialData ?? throw new ArgumentNullException(nameof(initialData));
//
//    internal long Length
//    {
//        get
//        {
//            lock (_sync)
//            {
//                return _data.Length;
//            }
//        }
//    }
//
//    internal void SetLength(long value)
//    {
//        if (value < 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(value));
//        }
//
//        lock (_sync)
//        {
//            if (value == _data.Length)
//            {
//                return;
//            }
//
//            if (value < _data.Length)
//            {
//                Array.Resize(ref _data, (int)value);
//                return;
//            }
//
//            var grown = new byte[value];
//            Buffer.BlockCopy(_data, 0, grown, 0, _data.Length);
//            _data = grown;
//        }
//    }
//
//    internal int Read(long position, byte[] buffer, int offset, int count)
//    {
//        lock (_sync)
//        {
//            if (position >= _data.Length)
//            {
//                return 0;
//            }
//
//            var available = (int)Math.Min(count, _data.Length - position);
//            Buffer.BlockCopy(_data, (int)position, buffer, offset, available);
//            return available;
//        }
//    }
//
//    internal void ReadSpan(long position, Span<byte> destination)
//    {
//        lock (_sync)
//        {
//            if (position + destination.Length > _data.Length)
//            {
//                throw new EndOfStreamException(
//                    $"Lettura incompleta a offset {position}: attesi {destination.Length} byte, disponibili {_data.Length - position}.");
//            }
//
//            _data.AsSpan((int)position, destination.Length).CopyTo(destination);
//        }
//    }
//
//    internal void Write(long position, ReadOnlySpan<byte> source)
//    {
//        lock (_sync)
//        {
//            var required = position + source.Length;
//            if (required > _data.Length)
//            {
//                var newLen = Math.Max(required, _data.Length == 0 ? required : (long)(_data.Length * 1.5));
//                if (newLen < required)
//                {
//                    newLen = required;
//                }
//
//                var grown = new byte[newLen];
//                Buffer.BlockCopy(_data, 0, grown, 0, _data.Length);
//                _data = grown;
//            }
//
//            source.CopyTo(_data.AsSpan((int)position));
//        }
//    }
//}
//
///// <summary>Per-handle view over a shared <see cref="IsamMemTable"/> (independent cursor).</summary>
//internal sealed class IsamMemStream : Stream
//{
//    private readonly IsamMemTable _table;
//    private long _position;
//
//    internal IsamMemStream(IsamMemTable table) => _table = table;
//
//    public override bool CanRead => true;
//
//    public override bool CanSeek => true;
//
//    public override bool CanWrite => true;
//
//    public override long Length => _table.Length;
//
//    public override long Position
//    {
//        get => _position;
//        set
//        {
//            if (value < 0)
//            {
//                throw new ArgumentOutOfRangeException(nameof(value));
//            }
//
//            _position = value;
//        }
//    }
//
//    public override void Flush()
//    {
//    }
//
//    public override int Read(byte[] buffer, int offset, int count)
//    {
//        ArgumentNullException.ThrowIfNull(buffer);
//        var read = _table.Read(_position, buffer, offset, count);
//        _position += read;
//        return read;
//    }
//
//    public override long Seek(long offset, SeekOrigin origin)
//    {
//        _position = origin switch
//        {
//            SeekOrigin.Begin => offset,
//            SeekOrigin.Current => _position + offset,
//            SeekOrigin.End => _table.Length + offset,
//            _ => throw new ArgumentOutOfRangeException(nameof(origin)),
//        };
//
//        if (_position < 0)
//        {
//            throw new IOException("Seek before beginning of MEM table.");
//        }
//
//        return _position;
//    }
//
//    public override void SetLength(long value) => _table.SetLength(value);
//
//    public override void Write(byte[] buffer, int offset, int count)
//    {
//        ArgumentNullException.ThrowIfNull(buffer);
//        _table.Write(_position, buffer.AsSpan(offset, count));
//        _position += count;
//    }
//
//    internal void TableReadAt(long offset, Span<byte> destination)
//    {
//        _table.ReadSpan(offset, destination);
//    }
//
//    internal void TableWriteAt(long offset, ReadOnlySpan<byte> data)
//    {
//        _table.Write(offset, data);
//    }
//
//    protected override void Dispose(bool disposing)
//    {
//        // ponytail: shared table outlives the handle; do not remove from IsamMemStore here.
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef _WIN32
#include <strings.h>
#endif

#define ISAM_MEM_MAX 32
#define ISAM_MEM_DIR "native/gmb_db_c/build"

typedef struct IsamMemSlot {
    char key[256];
    char path[1024];
    int inUse;
} IsamMemSlot;

static IsamMemSlot g_mem[ISAM_MEM_MAX];

static void mem_sanitize(const char* key, char* out, int cap)
{
    int i, n = 0;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!key) return;
    for (i = 0; key[i] && n < cap - 1; i++) {
        unsigned char c = (unsigned char)key[i];
        out[n++] = (isalnum(c) || c == '_' || c == '-') ? (char)c : '_';
    }
    out[n] = 0;
    if (!out[0] && cap > 1) {
        out[0] = 't';
        out[1] = 0;
    }
}

static IsamMemSlot* mem_find(const char* key)
{
    int i;
    if (!key || !key[0]) return NULL;
    for (i = 0; i < ISAM_MEM_MAX; i++) {
        if (g_mem[i].inUse && gmb_strcasecmp(g_mem[i].key, key) == 0)
            return &g_mem[i];
    }
    return NULL;
}

static IsamMemSlot* mem_alloc(const char* key)
{
    IsamMemSlot* s = mem_find(key);
    int i;
    char safe[128];
    if (s) return s;
    for (i = 0; i < ISAM_MEM_MAX; i++) {
        if (!g_mem[i].inUse) {
            memset(&g_mem[i], 0, sizeof(g_mem[i]));
            snprintf(g_mem[i].key, sizeof(g_mem[i].key), "%s", key);
            mem_sanitize(key, safe, (int)sizeof(safe));
            snprintf(g_mem[i].path, sizeof(g_mem[i].path), "%s/isam_mem_%s.isdb", ISAM_MEM_DIR, safe);
            g_mem[i].inUse = 1;
            return &g_mem[i];
        }
    }
    return NULL;
}

int IsamMemStore_Exists(const char* key)
{
    return mem_find(key) != NULL;
}

int IsamMemStore_CreateNew(const char* key, const IsamSchema* schema)
{
    IsamMemSlot* s;
    uint8_t* bytes = NULL;
    int len = 0;
    FILE* f;
    if (!key || !key[0] || !schema) return GMB_DB_ARG;
    mkdir(ISAM_MEM_DIR, 0755);
    s = mem_alloc(key);
    if (!s) return GMB_DB_ERR;
    if (IsamFileHandle_BuildNewFileBytes(schema, &bytes, &len) != GMB_DB_OK) return GMB_DB_ERR;
    f = fopen(s->path, "wb");
    if (!f) { free(bytes); return GMB_DB_ERR; }
    if ((int)fwrite(bytes, 1, (size_t)len, f) != len) {
        fclose(f);
        free(bytes);
        return GMB_DB_ERR;
    }
    fflush(f);
    fclose(f);
    free(bytes);
    return GMB_DB_OK;
}

int IsamMemStore_BackingPath(const char* key, char* out, int cap)
{
    IsamMemSlot* s = mem_find(key);
    if (!out || cap <= 0) return GMB_DB_ARG;
    out[0] = 0;
    if (!s) return GMB_DB_ERR;
    snprintf(out, (size_t)cap, "%s", s->path);
    return GMB_DB_OK;
}

int IsamMemStore_Remove(const char* key)
{
    IsamMemSlot* s = mem_find(key);
    if (!s) return GMB_DB_OK;
    unlink(s->path);
    memset(s, 0, sizeof(*s));
    return GMB_DB_OK;
}

int IsamMemStore_SelfCheck(void)
{
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine *e, *e2;
    IsamFieldValue vals[2];
    char nameOut[32];
    const char* path = "MEM:SelfCheck";

    IsamMemStore_Remove("SelfCheck");
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -310;
    if (!IsamMemStore_Exists("SelfCheck")) return -311;
    e = Engine_Open(path, &schema);
    if (!e) return -312;
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 1;
    vals[1].text = "N1";
    if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -313; }
    Engine_Dispose(e);
    e2 = Engine_Open(path, &schema);
    if (!e2) return -314;
    if (Engine_GetTotalRecordCount(e2) != 1) { Engine_Dispose(e2); return -315; }
    if (Engine_SeekI64(e2, 1) != GMB_DB_OK) { Engine_Dispose(e2); return -316; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e2, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1 ||
        strcmp(nameOut, "N1") != 0) {
        Engine_Dispose(e2); return -317;
    }
    if (Engine_Reindex(e2) != GMB_DB_OK) { Engine_Dispose(e2); return -318; }
    if (Engine_SeekI64(e2, 1) != GMB_DB_OK) { Engine_Dispose(e2); return -319; }
    Engine_Dispose(e2);
    if (IsamMemStore_Remove("SelfCheck") != GMB_DB_OK) return -320;
    if (IsamMemStore_Exists("SelfCheck")) return -321;
    return GMB_DB_OK;
}
