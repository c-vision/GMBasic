/* FILE: native/gmb_db_c/src/ISAM/SchemaManager.c */
/* Port 1:1 di modules/Database/ISAM/SchemaManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.SchemaManager.cs
//
//using System.Text;
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//namespace Basic.ISAM;
//
///// <summary>
///// Fornisce metodi statici per gestire la definizione, serializzazione, validazione e interpretazione
///// dello schema logico di un file ISAM.
///// </summary>
//public static class SchemaManager
//{
//    private static readonly byte[] SchemaMagic = "ISCH"u8.ToArray();
//    private const int SchemaFormatVersion = 2;
//    private const int SchemaFormatVersionLegacy = 1;
//    /// <summary>
//    /// Calcola la dimensione fissa in byte di un singolo record dati, basandosi sullo schema fornito.
//    /// La dimensione totale include lo spazio per i metadati del record (Stato e Versione).
//    /// </summary>
//    /// <param name="schema">Lo schema del record di cui calcolare la dimensione.</param>
//    /// <returns>La dimensione totale del record in byte.</returns>
//    public static int CalculateRecordSize(Config.IsamSchema schema)
//    {
//        var metadataSize = System.Runtime.InteropServices.Marshal.SizeOf<RecordMetadata>();
//        return metadataSize + schema.Fields.Sum(field => GetFieldSize(field.DataType, field.Length));
//    }
//
//    /// <summary>
//    /// Serializza una mappa di nomi di indici e offset delle loro radici in un array di byte.
//    /// </summary>
//    /// <param name="map">Il dizionario da serializzare.</param>
//    /// <returns>La rappresentazione binaria della mappa.</returns>
//    public static byte[] SerializeIndexMap(Dictionary<string, long> map)
//    {
//        using var stream = new MemoryStream();
//        using var writer = new BinaryWriter(stream);
//        writer.Write(map.Count);
//        foreach (var kvp in map)
//        {
//            writer.Write(kvp.Key);
//            writer.Write(kvp.Value);
//        }
//
//        return stream.ToArray();
//    }
//
//    /// <summary>
//    /// Deserializza un array di byte in una mappa di nomi di indici e offset delle loro radici.
//    /// </summary>
//    /// <param name="buffer">Il buffer contenente la mappa serializzata.</param>
//    /// <returns>Il dizionario ricostruito.</returns>
//    public static Dictionary<string, long> DeserializeIndexMap(byte[] buffer)
//    {
//        var map = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
//        using var stream = new MemoryStream(buffer);
//        using var reader = new BinaryReader(stream);
//        var count = reader.ReadInt32();
//        for (var i = 0; i < count; i++)
//        {
//            map[reader.ReadString()] = reader.ReadInt64();
//        }
//
//        return map;
//    }
//
//    /// <summary>
//    /// Serializza un oggetto <see cref="Config.IsamSchema"/> in un array di byte per la persistenza su disco.
//    /// </summary>
//    /// <param name="schema">L'oggetto schema da serializzare.</param>
//    /// <returns>La rappresentazione binaria dello schema.</returns>
//    public static byte[] SerializeSchema(Config.IsamSchema schema)
//    {
//        using var stream = new MemoryStream();
//        using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
//        writer.Write(SchemaMagic);
//        writer.Write(SchemaFormatVersion);
//        writer.Write(schema.TypeName);
//        WriteIndexDefinition(writer, schema.PrimaryKey);
//        writer.Write(schema.SecondaryKeys.Count);
//        foreach (var secondary in schema.SecondaryKeys)
//        {
//            WriteIndexDefinition(writer, secondary);
//        }
//
//        writer.Write(schema.Fields.Count);
//        foreach (var field in schema.Fields)
//        {
//            WriteFieldSchema(writer, field);
//        }
//
//        return stream.ToArray();
//    }
//
//    /// <summary>
//    /// Deserializza un array di byte in un oggetto <see cref="Config.IsamSchema"/>.
//    /// </summary>
//    /// <param name="buffer">Il buffer contenente lo schema serializzato.</param>
//    /// <returns>L'oggetto <see cref="Config.IsamSchema"/> ricostruito.</returns>
//    public static Config.IsamSchema DeserializeSchema(byte[] buffer)
//    {
//        using var stream = new MemoryStream(buffer);
//        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
//
//        var magic = reader.ReadBytes(SchemaMagic.Length);
//        if (!magic.AsSpan().SequenceEqual(SchemaMagic))
//        {
//            throw new InvalidDataException("Blocco schema ISAM non valido o formato legacy non supportato.");
//        }
//
//        var version = reader.ReadInt32();
//        if (version is not (SchemaFormatVersion or SchemaFormatVersionLegacy))
//        {
//            throw new InvalidDataException($"Versione schema ISAM non supportata: {version}.");
//        }
//
//        var typeName = reader.ReadString();
//        var primaryKey = ReadIndexDefinition(reader, version);
//        var secondaryCount = reader.ReadInt32();
//        var secondaryKeys = new List<Config.IndexDefinition>(secondaryCount);
//        for (var i = 0; i < secondaryCount; i++)
//        {
//            secondaryKeys.Add(ReadIndexDefinition(reader, version));
//        }
//
//        var fieldCount = reader.ReadInt32();
//        var fields = new List<Config.IsamFieldSchema>(fieldCount);
//        for (var i = 0; i < fieldCount; i++)
//        {
//            fields.Add(ReadFieldSchema(reader));
//        }
//
//        return new Config.IsamSchema(typeName, fields, primaryKey, secondaryKeys);
//    }
//
//    private static void WriteIndexDefinition(BinaryWriter writer, Config.IndexDefinition definition)
//    {
//        writer.Write(definition.Name);
//        writer.Write(definition.KeyExpression);
//        writer.Write(definition.KeyLength);
//        writer.Write(definition.IsUnique);
//        writer.Write(definition.IsUnsafe);
//        writer.Write(definition.PartialPredicate ?? string.Empty);
//    }
//
//    private static Config.IndexDefinition ReadIndexDefinition(BinaryReader reader, int schemaVersion)
//    {
//        var name = reader.ReadString();
//        var keyExpression = reader.ReadString();
//        var keyLength = reader.ReadInt32();
//        var isUnique = reader.ReadBoolean();
//        var isUnsafe = reader.ReadBoolean();
//        string? partialPredicate = null;
//        if (schemaVersion >= SchemaFormatVersion)
//        {
//            var raw = reader.ReadString();
//            partialPredicate = string.IsNullOrWhiteSpace(raw) ? null : raw;
//        }
//
//        return new Config.IndexDefinition(name, keyExpression, keyLength, isUnique, isUnsafe, partialPredicate);
//    }
//
//    private static void WriteFieldSchema(BinaryWriter writer, Config.IsamFieldSchema field)
//    {
//        writer.Write(field.Name);
//        writer.Write(GetDataTypeCode(field.DataType));
//        writer.Write(field.Length);
//    }
//
//    private static Config.IsamFieldSchema ReadFieldSchema(BinaryReader reader)
//    {
//        var name = reader.ReadString();
//        var typeCode = reader.ReadString();
//        var length = reader.ReadInt32();
//        var dataType = CreateDataType(typeCode, length);
//        return new Config.IsamFieldSchema(name, dataType, length);
//    }
//
//    private static string GetDataTypeCode(DataType dataType) => dataType switch
//    {
//        StringType => nameof(StringType),
//        JsonType => nameof(JsonType),
//        BitType => nameof(BitType),
//        ByteType => nameof(ByteType),
//        SByteType => nameof(SByteType),
//        IntegerType => nameof(IntegerType),
//        UIntegerType => nameof(UIntegerType),
//        LongType => nameof(LongType),
//        ULongType => nameof(ULongType),
//        QuadType => nameof(QuadType),
//        UQuadType => nameof(UQuadType),
//        SingleType => nameof(SingleType),
//        DoubleType => nameof(DoubleType),
//        CurrencyType => nameof(CurrencyType),
//        DateType => nameof(DateType),
//        TimeType => nameof(TimeType),
//        TimeSpanType => nameof(TimeSpanType),
//        DateTimeType => nameof(DateTimeType),
//        _ => throw new NotSupportedException($"Tipo di dato '{dataType.GetType().Name}' non serializzabile nello schema ISAM.")
//    };
//
//    private static DataType CreateDataType(string typeCode, int length) => typeCode switch
//    {
//        nameof(StringType) => new StringType(length > 0 ? length : null),
//        nameof(JsonType) => new JsonType(),
//        nameof(BitType) => new BitType(),
//        nameof(ByteType) => new ByteType(),
//        nameof(SByteType) => new SByteType(),
//        nameof(IntegerType) => new IntegerType(),
//        nameof(UIntegerType) => new UIntegerType(),
//        nameof(LongType) => new LongType(),
//        nameof(ULongType) => new ULongType(),
//        nameof(QuadType) => new QuadType(),
//        nameof(UQuadType) => new UQuadType(),
//        nameof(SingleType) => new SingleType(),
//        nameof(DoubleType) => new DoubleType(),
//        nameof(CurrencyType) => new CurrencyType(),
//        nameof(DateType) => new DateType(),
//        nameof(TimeType) => new TimeType(),
//        nameof(TimeSpanType) => new TimeSpanType(),
//        nameof(DateTimeType) => new DateTimeType(),
//        _ => throw new NotSupportedException($"Codice tipo schema ISAM sconosciuto: '{typeCode}'.")
//    };
//
//    /// <summary>
//    /// Valida la compatibilità strutturale tra lo schema definito nel codice e quello letto dal file.
//    /// La compatibilità richiede una corrispondenza esatta di nome del tipo, numero di campi, e per ogni
//    /// campo: nome, tipo e lunghezza. Vengono verificate anche le definizioni degli indici.
//    /// </summary>
//    /// <param name="codeSchema">Lo schema derivato dal `TYPE` nel codice BASIC.</param>
//    /// <param name="fileSchema">Lo schema letto dall'header del file .isdb.</param>
//    /// <returns>True se gli schemi sono compatibili, altrimenti False.</returns>
//    public static bool AreSchemasCompatible(Config.IsamSchema codeSchema, Config.IsamSchema fileSchema)
//    {
//        if (!codeSchema.TypeName.Equals(fileSchema.TypeName, StringComparison.OrdinalIgnoreCase))
//        {
//            Console.Error.WriteLine($"[SCHEMA] TypeName mismatch: '{codeSchema.TypeName}' vs '{fileSchema.TypeName}'");
//            return false;
//        }
//
//        if (codeSchema.Fields.Count != fileSchema.Fields.Count)
//        {
//            Console.Error.WriteLine($"[SCHEMA] Field count: {codeSchema.Fields.Count} vs {fileSchema.Fields.Count}");
//            return false;
//        }
//
//        for (var i = 0; i < codeSchema.Fields.Count; i++)
//        {
//            var codeField = codeSchema.Fields[i];
//            var fileField = fileSchema.Fields[i];
//
//            if (!codeField.Name.Equals(fileField.Name, StringComparison.OrdinalIgnoreCase))
//            {
//                Console.Error.WriteLine($"[SCHEMA] Field[{i}] name: '{codeField.Name}' vs '{fileField.Name}'");
//                return false;
//            }
//            if (codeField.DataType.GetType() != fileField.DataType.GetType())
//            {
//                Console.Error.WriteLine($"[SCHEMA] Field[{i}] type: {codeField.DataType.GetType().Name} vs {fileField.DataType.GetType().Name}");
//                return false;
//            }
//            if (codeField.Length != fileField.Length)
//            {
//                Console.Error.WriteLine($"[SCHEMA] Field[{i}] length: {codeField.Length} vs {fileField.Length}");
//                return false;
//            }
//        }
//
//        if (!codeSchema.PrimaryKey.Equals(fileSchema.PrimaryKey))
//        {
//            Console.Error.WriteLine($"[SCHEMA] PK mismatch: {codeSchema.PrimaryKey} vs {fileSchema.PrimaryKey}");
//            return false;
//        }
//        if (!codeSchema.SecondaryKeys.SequenceEqual(fileSchema.SecondaryKeys))
//        {
//            Console.Error.WriteLine($"[SCHEMA] SecondaryKeys mismatch");
//            return false;
//        }
//
//        return true;
//    }
//
//    /// <summary>
//    /// Serializza un valore di chiave (già calcolato) nella sua rappresentazione binaria.
//    /// Il valore viene convertito in una stringa maiuscola, poi in byte, e infine
//    /// troncato o riempito (padding con spazi) per adattarlo alla lunghezza fissa definita dall'indice.
//    /// </summary>
//    /// <param name="value">Il valore della chiave da serializzare (es. una stringa o un numero).</param>
//    /// <param name="indexDef">La definizione dell'indice, usata per ottenere la lunghezza della chiave.</param>
//    /// <returns>La rappresentazione binaria della chiave, normalizzata alla lunghezza corretta.</returns>
//    public static byte[] SerializeKeyValue(object? value, Config.IndexDefinition indexDef)
//    {
//        var strValue = FormatKeyString(value, indexDef.KeyLength);
//        var bytes = Encoding.UTF8.GetBytes(strValue);
//
//        var keyBuffer = new byte[indexDef.KeyLength];
//        Array.Fill(keyBuffer, (byte)' ');
//        Buffer.BlockCopy(bytes, 0, keyBuffer, 0, Math.Min(bytes.Length, keyBuffer.Length));
//
//        return keyBuffer;
//    }
//
//    private static string FormatKeyString(object? value, int keyLength)
//    {
//        if (value is null)
//        {
//            return string.Empty;
//        }
//
//        // ponytail: zero-pad numerici così l'ordinamento lessicografico byte[] == ordinamento numerico
//        return value switch
//        {
//            byte or sbyte or short or ushort or int or uint or long or ulong
//                => Convert.ToInt64(value, System.Globalization.CultureInfo.InvariantCulture)
//                    .ToString(System.Globalization.CultureInfo.InvariantCulture)
//                    .PadLeft(keyLength, '0'),
//            float or double or decimal
//                => Convert.ToDouble(value, System.Globalization.CultureInfo.InvariantCulture)
//                    .ToString("G17", System.Globalization.CultureInfo.InvariantCulture)
//                    .PadLeft(keyLength, '0'),
//            _ => (value.ToString() ?? string.Empty).ToUpperInvariant(),
//        };
//    }
//
//    /// <summary>
//    /// Deserializza una mappa di nomi di indici e offset delle loro radici direttamente da uno stream.
//    /// </summary>
//    /// <param name="stream">Lo stream da cui leggere i dati, posizionato all'inizio della mappa.</param>
//    /// <returns>Il dizionario ricostruito.</returns>
//    public static Dictionary<string, long> DeserializeIndexMapFromStream(Stream stream)
//    {
//        var map = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
//        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
//        var count = reader.ReadInt32();
//        for (var i = 0; i < count; i++)
//        {
//            map[reader.ReadString()] = reader.ReadInt64();
//        }
//
//        return map;
//    }
//
//    /// <summary>
//    /// Helper privato per ottenere la dimensione fissa in byte di un singolo tipo di dato.
//    /// </summary>
//    /// <param name="dataType">Il tipo di dato da misurare.</param>
//    /// <param name="length">La lunghezza specifica per i tipi a lunghezza variabile (es. String).</param>
//    /// <returns>La dimensione in byte.</returns>
//    private static int GetFieldSize(DataType dataType, int length) => dataType switch
//    {
//        StringType => length,
//        JsonType => sizeof(long),
//        BitType => sizeof(bool),
//        ByteType => sizeof(byte),
//        SByteType => sizeof(sbyte),
//        IntegerType => sizeof(short),
//        UIntegerType => sizeof(ushort),
//        LongType => sizeof(int),
//        ULongType => sizeof(uint),
//        QuadType => sizeof(long),
//        UQuadType => sizeof(ulong),
//        SingleType => sizeof(float),
//        DoubleType => sizeof(double),
//        CurrencyType => sizeof(decimal),
//        DateType => sizeof(long),
//        TimeType => sizeof(long),
//        TimeSpanType => sizeof(long),
//        DateTimeType => sizeof(long),
//        _ => throw new NotSupportedException($"Tipo di dato '{dataType.GetType().Name}' non supportato nei file ISAM.")
//    };
//
//    internal static int GetFieldStorageSize(DataType dataType, int length) => GetFieldSize(dataType, length);
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

/* .NET BinaryWriter (UTF-8): 7-bit length + bytes; ints little-endian. */
typedef struct {
    uint8_t* p;
    int n, cap;
} Bw;

static void bw_grow(Bw* b, int add)
{
    if (b->n + add <= b->cap) return;
    b->cap = b->cap ? b->cap * 2 : 256;
    while (b->n + add > b->cap) b->cap *= 2;
    b->p = (uint8_t*)realloc(b->p, (size_t)b->cap);
}

static void bw_u8(Bw* b, uint8_t v) { bw_grow(b, 1); b->p[b->n++] = v; }
static void bw_i32(Bw* b, int32_t v)
{
    bw_grow(b, 4);
    b->p[b->n++] = (uint8_t)v; b->p[b->n++] = (uint8_t)(v >> 8);
    b->p[b->n++] = (uint8_t)(v >> 16); b->p[b->n++] = (uint8_t)(v >> 24);
}
static void bw_i64(Bw* b, int64_t v)
{
    int i;
    bw_grow(b, 8);
    for (i = 0; i < 8; i++) b->p[b->n++] = (uint8_t)(v >> (8 * i));
}
static void bw_7bit(Bw* b, unsigned v)
{
    while (v >= 0x80) { bw_u8(b, (uint8_t)(v | 0x80)); v >>= 7; }
    bw_u8(b, (uint8_t)v);
}
static void bw_str(Bw* b, const char* s)
{
    size_t n = s ? strlen(s) : 0;
    bw_7bit(b, (unsigned)n);
    if (n) { bw_grow(b, (int)n); memcpy(b->p + b->n, s, n); b->n += (int)n; }
}
static void bw_bool(Bw* b, int v) { bw_u8(b, v ? 1 : 0); }

int SchemaManager_GetFieldSize(const char* typeCode, int length)
{
    if (!typeCode) return -1;
    if (strcmp(typeCode, "StringType") == 0) return length;
    if (strcmp(typeCode, "JsonType") == 0) return 8;
    if (strcmp(typeCode, "BitType") == 0) return 1;
    if (strcmp(typeCode, "ByteType") == 0) return 1;
    if (strcmp(typeCode, "SByteType") == 0) return 1;
    if (strcmp(typeCode, "IntegerType") == 0) return 2;
    if (strcmp(typeCode, "UIntegerType") == 0) return 2;
    if (strcmp(typeCode, "LongType") == 0) return 4;
    if (strcmp(typeCode, "ULongType") == 0) return 4;
    if (strcmp(typeCode, "QuadType") == 0) return 8;
    if (strcmp(typeCode, "UQuadType") == 0) return 8;
    if (strcmp(typeCode, "SingleType") == 0) return 4;
    if (strcmp(typeCode, "DoubleType") == 0) return 8;
    if (strcmp(typeCode, "CurrencyType") == 0) return 16;
    if (strcmp(typeCode, "DateType") == 0) return 8;
    if (strcmp(typeCode, "TimeType") == 0) return 8;
    if (strcmp(typeCode, "TimeSpanType") == 0) return 8;
    if (strcmp(typeCode, "DateTimeType") == 0) return 8;
    return -1;
}

int SchemaManager_CalculateRecordSize(const IsamSchema* schema)
{
    int i, n = RecordMetadata_Size;
    if (!schema) return -1;
    for (i = 0; i < schema->FieldCount; i++) {
        int sz = SchemaManager_GetFieldSize(schema->Fields[i].DataTypeCode, schema->Fields[i].Length);
        if (sz < 0) return -1;
        n += sz;
    }
    return n;
}

static void schema_write_index(Bw* b, const IsamIndexDefinition* d)
{
    bw_str(b, d->Name);
    bw_str(b, d->KeyExpression);
    bw_i32(b, d->KeyLength);
    bw_bool(b, d->IsUnique);
    bw_bool(b, d->IsUnsafe);
    bw_str(b, d->PartialPredicate);
}

int SchemaManager_SerializeIndexMap(const char** names, const int64_t* offsets, int count,
                                    uint8_t** out, int* outLen)
{
    Bw b = {0};
    int i;
    bw_i32(&b, count);
    for (i = 0; i < count; i++) {
        bw_str(&b, names[i]);
        bw_i64(&b, offsets[i]);
    }
    *out = b.p;
    *outLen = b.n;
    return GMB_DB_OK;
}

int SchemaManager_SerializeSchema(const IsamSchema* schema, uint8_t** out, int* outLen)
{
    Bw b = {0};
    int i;
    if (!schema) return GMB_DB_ERR;
    bw_grow(&b, 4); memcpy(b.p, SchemaManager_SchemaMagic, 4); b.n = 4;
    bw_i32(&b, SchemaManager_SchemaFormatVersion);
    bw_str(&b, schema->TypeName);
    schema_write_index(&b, &schema->PrimaryKey);
    bw_i32(&b, schema->SecondaryKeyCount);
    for (i = 0; i < schema->SecondaryKeyCount; i++)
        schema_write_index(&b, &schema->SecondaryKeys[i]);
    bw_i32(&b, schema->FieldCount);
    for (i = 0; i < schema->FieldCount; i++) {
        bw_str(&b, schema->Fields[i].Name);
        bw_str(&b, schema->Fields[i].DataTypeCode);
        bw_i32(&b, schema->Fields[i].Length);
    }
    *out = b.p;
    *outLen = b.n;
    return GMB_DB_OK;
}

/* C#: method SchemaManager.DeserializeIndexMap */
int SchemaManager_SerializeKeyValueI64(int64_t value, int keyLength, uint8_t* out)
{
    char num[32];
    int n, copy;
    if (!out || keyLength <= 0) return GMB_DB_ERR;
    snprintf(num, sizeof(num), "%lld", (long long)value);
    n = (int)strlen(num);
    memset(out, '0', (size_t)keyLength);
    if (n >= keyLength) {
        memcpy(out, num, (size_t)keyLength);
        return GMB_DB_OK;
    }
    copy = n;
    memcpy(out + (keyLength - copy), num, (size_t)copy);
    return GMB_DB_OK;
}

int SchemaManager_SerializeKeyValueText(const char* text, int keyLength, uint8_t* out)
{
    int i, n;
    if (!out || keyLength <= 0) return GMB_DB_ERR;
    memset(out, ' ', (size_t)keyLength);
    if (!text) return GMB_DB_OK;
    n = (int)strlen(text);
    if (n > keyLength) n = keyLength;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        out[i] = (uint8_t)toupper(c);
    }
    return GMB_DB_OK;
}

typedef struct {
    const uint8_t* p;
    int n, i, err;
} Br;

static uint8_t br_u8(Br* r)
{
    if (r->err || r->i >= r->n) { r->err = 1; return 0; }
    return r->p[r->i++];
}

static int32_t br_i32(Br* r)
{
    uint32_t v = br_u8(r);
    v |= (uint32_t)br_u8(r) << 8;
    v |= (uint32_t)br_u8(r) << 16;
    v |= (uint32_t)br_u8(r) << 24;
    return (int32_t)v;
}

static unsigned br_7bit(Br* r)
{
    unsigned v = 0, shift = 0;
    for (;;) {
        uint8_t b = br_u8(r);
        v |= (unsigned)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) return v;
        shift += 7;
        if (shift > 28) { r->err = 1; return 0; }
    }
}

static void br_str(Br* r, char* out, int cap)
{
    unsigned n = br_7bit(r);
    int i;
    if (!out || cap <= 0) { r->err = 1; return; }
    if (r->err || r->i + (int)n > r->n || (int)n >= cap) { r->err = 1; out[0] = 0; return; }
    for (i = 0; i < (int)n; i++) out[i] = (char)br_u8(r);
    out[n] = 0;
}

static void br_index(Br* r, int version, IsamIndexDefinition* d)
{
    memset(d, 0, sizeof(*d));
    br_str(r, d->Name, (int)sizeof(d->Name));
    br_str(r, d->KeyExpression, (int)sizeof(d->KeyExpression));
    d->KeyLength = br_i32(r);
    d->IsUnique = br_u8(r) ? 1 : 0;
    d->IsUnsafe = br_u8(r) ? 1 : 0;
    if (version >= SchemaManager_SchemaFormatVersion)
        br_str(r, d->PartialPredicate, (int)sizeof(d->PartialPredicate));
}

int SchemaManager_DeserializeSchema(const uint8_t* buf, int len, IsamSchema* schema,
                                    IsamFieldSchema* fields, int fieldCap,
                                    IsamIndexDefinition* secs, int secCap)
{
    Br r;
    int version, i, nsec, nfields;
    if (!buf || len < 8 || !schema || !fields || fieldCap <= 0) return GMB_DB_ARG;
    memset(schema, 0, sizeof(*schema));
    r.p = buf;
    r.n = len;
    r.i = 0;
    r.err = 0;
    if (len < 4 || memcmp(buf, SchemaManager_SchemaMagic, 4) != 0) return GMB_DB_ERR;
    r.i = 4;
    version = br_i32(&r);
    if (version != SchemaManager_SchemaFormatVersion && version != 1) return GMB_DB_ERR;
    br_str(&r, schema->TypeName, (int)sizeof(schema->TypeName));
    br_index(&r, version, &schema->PrimaryKey);
    nsec = br_i32(&r);
    if (nsec < 0 || nsec > secCap) return GMB_DB_ERR;
    if (nsec > 0 && !secs) return GMB_DB_ERR;
    for (i = 0; i < nsec; i++) br_index(&r, version, &secs[i]);
    nfields = br_i32(&r);
    if (nfields < 1 || nfields > fieldCap) return GMB_DB_ERR;
    for (i = 0; i < nfields; i++) {
        memset(&fields[i], 0, sizeof(fields[i]));
        br_str(&r, fields[i].Name, (int)sizeof(fields[i].Name));
        br_str(&r, fields[i].DataTypeCode, (int)sizeof(fields[i].DataTypeCode));
        fields[i].Length = br_i32(&r);
    }
    if (r.err) return GMB_DB_ERR;
    schema->Fields = fields;
    schema->FieldCount = nfields;
    /* Punta SEMPRE al buffer secs (capacita' = secCap) quando fornito: con nsec==0 il C#
     * consente comunque CREATE INDEX a runtime; senza questo, Engine_CreateIndex
     * fallisce perche' trova SecondaryKeys NULL. */
    schema->SecondaryKeys = secs;
    schema->SecondaryKeyCount = nsec;
    return GMB_DB_OK;
}

int IsamFileHandle_BuildNewFileBytes(const IsamSchema* schema, uint8_t** out, int* outLen)
{
    uint8_t *schemaBuf = NULL, *mapBuf = NULL, *file;
    int schemaLen = 0, mapLen = 0, recordSize, fileLen, i;
    IsamFileHeader h;
    int64_t heapStart;
    if (!schema) return GMB_DB_ERR;
    recordSize = SchemaManager_CalculateRecordSize(schema);
    if (recordSize < 0) return GMB_DB_ERR;
    if (SchemaManager_SerializeSchema(schema, &schemaBuf, &schemaLen) != GMB_DB_OK) return GMB_DB_ERR;
    if (SchemaManager_SerializeIndexMap(NULL, NULL, 0, &mapBuf, &mapLen) != GMB_DB_OK) {
        free(schemaBuf); return GMB_DB_ERR;
    }
    /* secondary names → map: C# writes each SecondaryKeys name with offset -1 */
    if (schema->SecondaryKeyCount > 0) {
        const char** names = (const char**)malloc((size_t)schema->SecondaryKeyCount * sizeof(char*));
        int64_t* offs = (int64_t*)malloc((size_t)schema->SecondaryKeyCount * sizeof(int64_t));
        free(mapBuf);
        for (i = 0; i < schema->SecondaryKeyCount; i++) {
            names[i] = schema->SecondaryKeys[i].Name;
            offs[i] = -1;
        }
        if (SchemaManager_SerializeIndexMap(names, offs, schema->SecondaryKeyCount, &mapBuf, &mapLen) != GMB_DB_OK) {
            free(names); free(offs); free(schemaBuf); return GMB_DB_ERR;
        }
        free(names); free(offs);
    }
    heapStart = (int64_t)Config_HeaderSize + schemaLen + mapLen;
    memset(&h, 0, sizeof(h));
    memcpy(h.Magic, Config_MagicBytes, 8);
    h.Version = 1;
    h.RecordSize = recordSize;
    h.SchemaOffset = Config_HeaderSize;
    h.SchemaLength = schemaLen;
    h.SecondaryIndexMapOffset = (int64_t)Config_HeaderSize + schemaLen;
    h.PrimaryKeyRootOffset = -1;
    h.FreeListHeadOffset = -1;
    h.NextAvailablePageOffset = heapStart;
    h.LobHeapStartOffset = heapStart;
    h.PageFreeListHeadOffset = -1;
    for (i = 0; i < 15; i++) h.SecondaryIndexRootOffsets[i] = -1;
    fileLen = (int)heapStart;
    if (fileLen < Config_HeaderSize) fileLen = Config_HeaderSize;
    file = (uint8_t*)calloc(1, (size_t)fileLen);
    memcpy(file, &h, sizeof(h));
    memcpy(file + Config_HeaderSize, schemaBuf, (size_t)schemaLen);
    memcpy(file + Config_HeaderSize + schemaLen, mapBuf, (size_t)mapLen);
    free(schemaBuf); free(mapBuf);
    *out = file;
    *outLen = fileLen;
    return GMB_DB_OK;
}

static int isam_mkdir_parent(const char* filePath)
{
    char buf[PATH_MAX], *slash;
    snprintf(buf, sizeof(buf), "%s", filePath);
    slash = strrchr(buf, '/');
    if (!slash || slash == buf) return 0;
    *slash = 0;
    if (mkdir(buf, 0755) == 0 || errno == EEXIST) return 0;
    return -1;
}

int IsamFileHandle_CreateNew(const char* filePath, const IsamSchema* schema)
{
    uint8_t* bytes = NULL;
    int len = 0;
    FILE* f;
    if (!filePath || !schema) return GMB_DB_ERR;
    if (IsamMemPath_IsMemPath(filePath)) {
        char key[256];
        if (IsamMemPath_NormalizeKey(filePath, key, (int)sizeof(key)) != GMB_DB_OK) return GMB_DB_ARG;
        return IsamMemStore_CreateNew(key, schema);
    }
    isam_mkdir_parent(filePath);
    if (IsamFileHandle_BuildNewFileBytes(schema, &bytes, &len) != GMB_DB_OK) return GMB_DB_ERR;
    f = fopen(filePath, "wb");
    if (!f) { free(bytes); return GMB_DB_ERR; }
    if ((int)fwrite(bytes, 1, (size_t)len, f) != len) { fclose(f); free(bytes); return GMB_DB_ERR; }
    fflush(f); fclose(f); free(bytes);
    return GMB_DB_OK;
}

int IsamFileHandle_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_create_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    FILE* f;
    IsamFileHeader h;
    uint8_t magic[4];
    int recSize;
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
    recSize = SchemaManager_CalculateRecordSize(&schema);
    if (recSize != RecordMetadata_Size + 2 + 20) return -2;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -3;
    f = fopen(path, "rb");
    if (!f) return -4;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -5; }
    if (h.RecordSize != recSize) { fclose(f); return -6; }
    fseek(f, (long)h.SchemaOffset, SEEK_SET);
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return -7; }
    fclose(f);
    if (memcmp(magic, SchemaManager_SchemaMagic, 4) != 0) return -8;
    {
        IsamFileHandle* fh = IsamFileHandle_Open(path, &schema);
        uint8_t mag[8];
        if (!fh) return -9;
        if (IsamFileHandle_EnableMmapReadOnly(fh) != GMB_DB_OK) {
            IsamFileHandle_Dispose(fh);
            return -10;
        }
        if (!IsamFileHandle_get_UseMmapReadOnly(fh)) {
            IsamFileHandle_Dispose(fh);
            return -11;
        }
        if (IsamFileHandle_ReadBytesAt(fh, 0, mag, 8) != GMB_DB_OK) {
            IsamFileHandle_Dispose(fh);
            return -12;
        }
        if (memcmp(mag, Config_MagicBytes, 8) != 0) {
            IsamFileHandle_Dispose(fh);
            return -13;
        }
        IsamFileHandle_Dispose(fh);
    }
    return GMB_DB_OK;
}

/* leftover inventory stubs that would collide were replaced above */
