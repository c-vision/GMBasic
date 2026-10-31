/* FILE: native/gmb_db_c/src/ISAM/IsamExportImport.c */
/* Port 1:1 di modules/Database/ISAM/IsamExportImport.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamExportImport
//
//using System.Text;
//using System.Text.Json;
//using System.Text.Json.Nodes;
//using Basic.DBF;
//using Basic.Tools.Isam;
//using Basic.Databases;
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Export/import JSON round-trip per tooling offline (Phase G3).
///// </summary>
//public static class IsamExportImport
//{
//    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };
//
//    /// <summary>Esporta schema ISCH + record attivi in JSON.</summary>
//    public static void ExportJson(string sourceIsdb, string outputJsonPath, KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
//        ArgumentException.ThrowIfNullOrWhiteSpace(outputJsonPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//
//        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
//        var records = new JsonArray();
//        using (var engine = IsamOfflineToolkit.OpenEngine(sourceIsdb, evaluator))
//        {
//            engine.Go(Config.CursorMoveOperation.Top);
//            while (!engine.IsEof())
//            {
//                var record = engine.GetCurrentRecord();
//                if (record is not null)
//                {
//                    records.Add(RecordToJson(snapshot.Schema, record));
//                }
//
//                engine.Skip();
//            }
//        }
//
//        var payload = new JsonObject
//        {
//            ["fileFormatVersion"] = snapshot.Header.Version,
//            ["exportFormatVersion"] = 1,
//            ["typeName"] = snapshot.Schema.TypeName,
//            ["primaryKey"] = IndexToJson(snapshot.Schema.PrimaryKey),
//            ["secondaryKeys"] = new JsonArray(snapshot.Schema.SecondaryKeys.Select(IndexToJson).ToArray()),
//            ["fields"] = new JsonArray(snapshot.Schema.Fields.Select(FieldToJson).ToArray()),
//            ["records"] = records,
//        };
//
//        var json = payload.ToJsonString(JsonOptions);
//        File.WriteAllText(outputJsonPath, json, Encoding.UTF8);
//    }
//
//    /// <summary>Crea un nuovo <c>.isdb</c> da export JSON.</summary>
//    public static void ImportJson(string destinationIsdb, string inputJsonPath, KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(destinationIsdb);
//        ArgumentException.ThrowIfNullOrWhiteSpace(inputJsonPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//
//        if (!File.Exists(inputJsonPath))
//        {
//            throw new FileNotFoundException("JSON export not found.", inputJsonPath);
//        }
//
//        if (File.Exists(destinationIsdb))
//        {
//            throw new InvalidOperationException($"Destination already exists: {destinationIsdb}");
//        }
//
//        var root = JsonNode.Parse(File.ReadAllText(inputJsonPath, Encoding.UTF8))
//                   ?? throw new InvalidDataException("Invalid JSON export.");
//
//        var schema = ParseSchema(root);
//        IsamFileHandle.CreateNew(destinationIsdb, schema, Encoding.UTF8);
//
//        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
//        using var handle = new IsamFileHandle(destinationIsdb, schema, udt, LockingStrategy.IntraProcess);
//        using var engine = new Engine(handle) { Evaluator = evaluator };
//
//        var records = root["records"]?.AsArray()
//                      ?? throw new InvalidDataException("JSON export missing 'records' array.");
//
//        foreach (var node in records)
//        {
//            if (node is not JsonObject row)
//            {
//                continue;
//            }
//
//            engine.Append(ParseRecord(schema, udt, row), evaluator);
//        }
//
//        handle.Commit();
//    }
//
//    /// <summary>Esporta record attivi in un file DBF Visual FoxPro (Phase G3).</summary>
//    public static void ExportDbf(string sourceIsdb, string outputDbfPath, KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
//        ArgumentException.ThrowIfNullOrWhiteSpace(outputDbfPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//
//        if (File.Exists(outputDbfPath))
//        {
//            throw new InvalidOperationException($"Destination already exists: {outputDbfPath}");
//        }
//
//        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
//        var dbfFields = TranslateIsamSchemaToDbf(snapshot.Schema);
//        var encoding = Encoding.UTF8;
//        DbfFile.CreateNew(outputDbfPath, encoding, dbfFields);
//
//        using (var dbfFile = new DbfFile(outputDbfPath, exclusive: true))
//        using (var isamEngine = IsamOfflineToolkit.OpenEngine(sourceIsdb, evaluator))
//        {
//            var payload = new byte[dbfFile.Header.RecordLength - 1];
//            isamEngine.Go(Config.CursorMoveOperation.Top);
//            while (!isamEngine.IsEof())
//            {
//                var record = isamEngine.GetCurrentRecord();
//                if (record is not null)
//                {
//                    WriteDbfRecordPayload(dbfFields, RecordToDbfValues(snapshot.Schema, record), payload, encoding);
//                    dbfFile.AppendRawRecord(payload);
//                }
//
//                isamEngine.Skip();
//            }
//        }
//    }
//
//    /// <summary>Importa un file DBF in un nuovo <c>.isdb</c> (Phase G3 round-trip).</summary>
//    public static void ImportDbf(
//        string sourceDbf,
//        string destinationIsdb,
//        KeyExpressionEvaluatorCallback evaluator,
//        string? primaryKeyFieldName = null,
//        int primaryKeyLength = 16)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(sourceDbf);
//        ArgumentException.ThrowIfNullOrWhiteSpace(destinationIsdb);
//        ArgumentNullException.ThrowIfNull(evaluator);
//
//        if (!File.Exists(sourceDbf))
//        {
//            throw new FileNotFoundException("DBF file not found.", sourceDbf);
//        }
//
//        if (File.Exists(destinationIsdb))
//        {
//            throw new InvalidOperationException($"Destination already exists: {destinationIsdb}");
//        }
//
//        using var dbfEngine = new DbfEngine(sourceDbf, null, false, LockingStrategy.IntraProcess);
//        var dbfFields = dbfEngine.GetFields();
//        if (dbfFields.Count == 0)
//        {
//            throw new InvalidDataException("DBF file has no fields.");
//        }
//
//        var pkName = primaryKeyFieldName ?? InferPrimaryKeyField(dbfFields);
//        var typeName = Path.GetFileNameWithoutExtension(destinationIsdb);
//        var schema = Engine.TranslateDbfSchemaToIsam(dbfFields, pkName, primaryKeyLength, typeName);
//        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
//        Engine.ConvertFromDbf(sourceDbf, destinationIsdb, pkName, primaryKeyLength, udt, evaluator);
//    }
//
//    private static JsonObject IndexToJson(Config.IndexDefinition index) => new()
//    {
//        ["name"] = index.Name,
//        ["keyExpression"] = index.KeyExpression,
//        ["keyLength"] = index.KeyLength,
//        ["isUnique"] = index.IsUnique,
//        ["isUnsafe"] = index.IsUnsafe,
//        ["partialPredicate"] = index.PartialPredicate ?? string.Empty,
//    };
//
//    private static JsonObject FieldToJson(Config.IsamFieldSchema field) => new()
//    {
//        ["name"] = field.Name,
//        ["type"] = field.DataType.GetType().Name,
//        ["length"] = field.Length,
//    };
//
//    private static JsonObject RecordToJson(Config.IsamSchema schema, UdtInstance record)
//    {
//        var obj = new JsonObject();
//        foreach (var field in schema.Fields)
//        {
//            var udtField = record.Definition.Fields.First(f =>
//                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
//            var value = record.Fields[udtField.SymbolId];
//            obj[field.Name] = ValueToJson(value);
//        }
//
//        return obj;
//    }
//
//    private static JsonNode? ValueToJson(object? value) => value switch
//    {
//        null => null,
//        string s => JsonValue.Create(s),
//        bool b => JsonValue.Create(b),
//        long l => JsonValue.Create(l),
//        int i => JsonValue.Create(i),
//        double d => JsonValue.Create(d),
//        float f => JsonValue.Create(f),
//        decimal m => JsonValue.Create(m),
//        _ => JsonValue.Create(value.ToString()),
//    };
//
//    private static Config.IsamSchema ParseSchema(JsonNode root)
//    {
//        var typeName = root["typeName"]?.GetValue<string>()
//                       ?? throw new InvalidDataException("JSON export missing typeName.");
//        var primaryKey = ParseIndex(root["primaryKey"]
//                                    ?? throw new InvalidDataException("JSON export missing primaryKey."));
//        var secondaryKeys = root["secondaryKeys"]?.AsArray() ?? [];
//        var secondaries = secondaryKeys
//            .Select(n => ParseIndex(n ?? throw new InvalidDataException("Null secondary key.")))
//            .ToList();
//        var fieldsNode = root["fields"]?.AsArray()
//                         ?? throw new InvalidDataException("JSON export missing fields.");
//        var fields = fieldsNode
//            .Select(n => ParseField(n ?? throw new InvalidDataException("Null field.")))
//            .ToList();
//        return new Config.IsamSchema(typeName, fields, primaryKey, secondaries);
//    }
//
//    private static Config.IndexDefinition ParseIndex(JsonNode node) => new(
//        node["name"]?.GetValue<string>() ?? throw new InvalidDataException("Index missing name."),
//        node["keyExpression"]?.GetValue<string>() ?? string.Empty,
//        node["keyLength"]?.GetValue<int>() ?? 0,
//        node["isUnique"]?.GetValue<bool>() ?? false,
//        node["isUnsafe"]?.GetValue<bool>() ?? false,
//        ParseOptionalString(node["partialPredicate"]));
//
//    private static Config.IsamFieldSchema ParseField(JsonNode node)
//    {
//        var name = node["name"]?.GetValue<string>() ?? throw new InvalidDataException("Field missing name.");
//        var typeCode = node["type"]?.GetValue<string>() ?? throw new InvalidDataException("Field missing type.");
//        var length = node["length"]?.GetValue<int>() ?? 0;
//        return new Config.IsamFieldSchema(name, ParseDataType(typeCode, length), length);
//    }
//
//    private static DataType ParseDataType(string typeCode, int length) => typeCode switch
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
//        _ => throw new NotSupportedException($"Unsupported field type in JSON export: {typeCode}."),
//    };
//
//    private static string? ParseOptionalString(JsonNode? node)
//    {
//        if (node is null)
//        {
//            return null;
//        }
//
//        var text = node.GetValue<string>();
//        return string.IsNullOrWhiteSpace(text) ? null : text;
//    }
//
//    private static UdtInstance ParseRecord(Config.IsamSchema schema, UserDefinedType udt, JsonObject row)
//    {
//        var fields = new Dictionary<int, object?>();
//        foreach (var field in schema.Fields)
//        {
//            var udtField = udt.Fields.First(f =>
//                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
//            row.TryGetPropertyValue(field.Name, out var jsonValue);
//            fields[udtField.SymbolId] = ParseFieldValue(field, jsonValue);
//        }
//
//        return new UdtInstance(udt, fields);
//    }
//
//    private static object? ParseFieldValue(Config.IsamFieldSchema field, JsonNode? jsonValue)
//    {
//        if (jsonValue is null)
//        {
//            return null;
//        }
//
//        return field.DataType switch
//        {
//            LongType => jsonValue.GetValue<long>(),
//            IntegerType => jsonValue.GetValue<int>(),
//            UIntegerType => (uint)jsonValue.GetValue<long>(),
//            ULongType => (ulong)jsonValue.GetValue<long>(),
//            StringType => jsonValue.GetValue<string>() ?? string.Empty,
//            JsonType => jsonValue.ToJsonString(),
//            BitType => jsonValue.GetValue<bool>(),
//            ByteType => (byte)jsonValue.GetValue<int>(),
//            SByteType => (sbyte)jsonValue.GetValue<int>(),
//            SingleType => (float)jsonValue.GetValue<double>(),
//            DoubleType => jsonValue.GetValue<double>(),
//            CurrencyType => jsonValue.GetValue<decimal>(),
//            DateType => jsonValue.GetValue<DateOnly>(),
//            TimeType => jsonValue.GetValue<TimeOnly>(),
//            DateTimeType => jsonValue.GetValue<DateTime>(),
//            TimeSpanType => jsonValue.GetValue<TimeSpan>(),
//            _ => jsonValue.GetValue<string>(),
//        };
//    }
//
//    private static List<DbfField> TranslateIsamSchemaToDbf(Config.IsamSchema schema)
//    {
//        var fields = new List<DbfField>(schema.Fields.Count);
//        foreach (var field in schema.Fields)
//        {
//            var (type, length, decimals) = MapIsamFieldToDbf(field);
//            fields.Add(DbfField.Create(field.Name, type, length, decimals));
//        }
//
//        return fields;
//    }
//
//    private static (DbfFieldType Type, byte Length, byte Decimals) MapIsamFieldToDbf(Config.IsamFieldSchema field) =>
//        field.DataType switch
//        {
//            StringType => (DbfFieldType.Character, (byte)Math.Clamp(field.Length > 0 ? field.Length : 32, 1, 254), 0),
//            LongType or IntegerType => (DbfFieldType.Integer, 4, 0),
//            UIntegerType or ULongType => (DbfFieldType.Numeric, 18, 0),
//            BitType => (DbfFieldType.Logical, 1, 0),
//            DateType => (DbfFieldType.Date, 8, 0),
//            DateTimeType => (DbfFieldType.DateTime, 8, 0),
//            SingleType => (DbfFieldType.Float, 20, 6),
//            DoubleType => (DbfFieldType.Double, 8, 0),
//            CurrencyType => (DbfFieldType.Currency, 8, 0),
//            JsonType => (DbfFieldType.Character, 254, 0), // ponytail: JSON as memo-less C(254) for export
//            _ => (DbfFieldType.Character, (byte)Math.Clamp(field.Length > 0 ? field.Length : 32, 1, 254), 0),
//        };
//
//    private static Dictionary<string, object?> RecordToDbfValues(Config.IsamSchema schema, UdtInstance record)
//    {
//        var values = new Dictionary<string, object?>(schema.Fields.Count, StringComparer.OrdinalIgnoreCase);
//        foreach (var field in schema.Fields)
//        {
//            var udtField = record.Definition.Fields.First(f =>
//                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
//            values[field.Name] = record.Fields[udtField.SymbolId];
//        }
//
//        return values;
//    }
//
//    private static void WriteDbfRecordPayload(
//        IReadOnlyList<DbfField> fields,
//        IReadOnlyDictionary<string, object?> values,
//        Span<byte> payload,
//        Encoding encoding)
//    {
//        payload.Clear();
//        var offset = 0;
//        foreach (var field in fields)
//        {
//            values.TryGetValue(field.Name, out var value);
//            DbfFieldWriter.Write(payload.Slice(offset, field.Length), field, value, encoding);
//            offset += field.Length;
//        }
//    }
//
//    private static string InferPrimaryKeyField(IReadOnlyList<DbfField> fields)
//    {
//        foreach (var field in fields)
//        {
//            if (field.Name.Equals("Id", StringComparison.OrdinalIgnoreCase))
//            {
//                return field.Name;
//            }
//        }
//
//        return fields[0].Name;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#ifndef _WIN32
#include <strings.h>
#endif
#include <unistd.h>

#define XI_MAX_FIELDS 64
#define XI_MAX_SECS 4

static void xi_putc_esc(FILE* f, const char* s)
{
    unsigned char c;
    fputc('"', f);
    if (!s) { fputc('"', f); return; }
    while ((c = (unsigned char)*s++) != 0) {
        if (c == '"' || c == '\\') { fputc('\\', f); fputc((int)c, f); }
        else if (c == '\n') { fputs("\\n", f); }
        else fputc((int)c, f);
    }
    fputc('"', f);
}

static void xi_index(FILE* f, const IsamIndexDefinition* d)
{
    fprintf(f, "{\"name\":");
    xi_putc_esc(f, d->Name);
    fprintf(f, ",\"keyExpression\":");
    xi_putc_esc(f, d->KeyExpression);
    fprintf(f, ",\"keyLength\":%d,\"isUnique\":%s,\"isUnsafe\":%s,\"partialPredicate\":",
            d->KeyLength, d->IsUnique ? "true" : "false", d->IsUnsafe ? "true" : "false");
    xi_putc_esc(f, d->PartialPredicate);
    fputc('}', f);
}

static int xi_is_text(const IsamFieldSchema* fs)
{
    return strcmp(fs->DataTypeCode, "StringType") == 0 ||
           strcmp(fs->DataTypeCode, "JsonType") == 0;
}

static int xi_clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void xi_trim_trail(char* s)
{
    int n;
    if (!s) return;
    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
}

static void xi_stem(const char* path, char* out, int cap)
{
    const char *s = path, *slash;
    char* dot;
    if (!path || !out || cap <= 0) return;
    slash = strrchr(path, '/');
#ifdef _WIN32
    {
        const char* b = strrchr(path, '\\');
        if (b && (!slash || b > slash)) slash = b;
    }
#endif
    if (slash) s = slash + 1;
    snprintf(out, (size_t)cap, "%s", s);
    dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

static int xi_load_schema(const char* path, IsamSchema* schema,
                          IsamFieldSchema* fields, int fieldCap,
                          IsamIndexDefinition* secs, int secCap)
{
    FILE* f;
    IsamFileHeader h;
    uint8_t* buf;
    int rc;
    if (!path || !schema || !fields) return GMB_DB_ARG;
    f = fopen(path, "rb");
    if (!f) return GMB_DB_ERR;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (h.SchemaLength <= 0 || h.SchemaLength > 1 << 20) {
        fclose(f);
        return GMB_DB_ERR;
    }
    buf = (uint8_t*)malloc((size_t)h.SchemaLength);
    if (!buf) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fseek(f, (long)h.SchemaOffset, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)h.SchemaLength, f) != (size_t)h.SchemaLength) {
        free(buf);
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    rc = SchemaManager_DeserializeSchema(buf, (int)h.SchemaLength, schema,
                                         fields, fieldCap, secs, secCap);
    free(buf);
    return rc;
}

static void xi_map_isam_to_dbf(const IsamFieldSchema* f, DbfField* out)
{
    uint8_t t = DbfFieldType_Character;
    int len = 32, dec = 0;
    const char* c = f->DataTypeCode;
    if (strcmp(c, "StringType") == 0) {
        t = DbfFieldType_Character;
        len = xi_clamp(f->Length > 0 ? f->Length : 32, 1, 254);
    } else if (strcmp(c, "JsonType") == 0) {
        t = DbfFieldType_Character;
        len = 254; /* ponytail: JSON as memo-less C(254) — stesso C# */
    } else if (strcmp(c, "LongType") == 0 || strcmp(c, "IntegerType") == 0) {
        t = DbfFieldType_Integer;
        len = 4;
    } else if (strcmp(c, "UIntegerType") == 0 || strcmp(c, "ULongType") == 0) {
        t = DbfFieldType_Numeric;
        len = 18;
    } else if (strcmp(c, "BitType") == 0) {
        t = DbfFieldType_Logical;
        len = 1;
    } else if (strcmp(c, "DateType") == 0) {
        t = DbfFieldType_Date;
        len = 8;
    } else if (strcmp(c, "DateTimeType") == 0) {
        t = DbfFieldType_DateTime;
        len = 8;
    } else if (strcmp(c, "SingleType") == 0) {
        t = DbfFieldType_Float;
        len = 20;
        dec = 6;
    } else if (strcmp(c, "DoubleType") == 0) {
        t = DbfFieldType_Double;
        len = 8;
    } else if (strcmp(c, "CurrencyType") == 0) {
        t = DbfFieldType_Currency;
        len = 8;
    } else {
        t = DbfFieldType_Character;
        len = xi_clamp(f->Length > 0 ? f->Length : 32, 1, 254);
    }
    *out = DbfField_Create(f->Name, t, (uint8_t)len, (uint8_t)dec);
}

static void xi_map_dbf_to_isam(const DbfField* d, IsamFieldSchema* f)
{
    memset(f, 0, sizeof(*f));
    snprintf(f->Name, sizeof(f->Name), "%s", d->Name);
    f->Length = d->Length;
    switch (d->Type) {
    case DbfFieldType_Character:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "StringType");
        break;
    case DbfFieldType_Integer:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "LongType");
        break;
    case DbfFieldType_Logical:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "BitType");
        f->Length = 1;
        break;
    case DbfFieldType_Numeric:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "DoubleType");
        break;
    case DbfFieldType_Float:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "SingleType");
        break;
    case DbfFieldType_Date:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "DateType");
        break;
    case DbfFieldType_DateTime:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "DateTimeType");
        break;
    case DbfFieldType_Double:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "DoubleType");
        break;
    case DbfFieldType_Currency:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "CurrencyType");
        break;
    default:
        snprintf(f->DataTypeCode, sizeof(f->DataTypeCode), "%s", "StringType");
        break;
    }
}

static const DbfField* xi_infer_pk(const DbfField* fields, int n, const char* name)
{
    int i;
    if (name && name[0]) {
        for (i = 0; i < n; i++) {
            if (gmb_strcasecmp(fields[i].Name, name) == 0) return &fields[i];
        }
        return NULL;
    }
    for (i = 0; i < n; i++) {
        if (gmb_strcasecmp(fields[i].Name, "Id") == 0) return &fields[i];
    }
    return n > 0 ? &fields[0] : NULL;
}

static void xi_fmt_value(const IsamFieldSchema* fs, const IsamFieldValue* v, char* buf, int cap)
{
    const char* t;
    if (!buf || cap <= 0) return;
    buf[0] = 0;
    if (xi_is_text(fs)) {
        t = v->textOut ? v->textOut : v->text;
        snprintf(buf, (size_t)cap, "%s", t ? t : "");
        return;
    }
    if (strcmp(fs->DataTypeCode, "BitType") == 0) {
        snprintf(buf, (size_t)cap, "%s", v->i64 ? "T" : "F");
        return;
    }
    if (IsamField_IsFloat(fs)) { IsamField_FormatFloat(fs, v->i64, buf, cap); return; }
    snprintf(buf, (size_t)cap, "%lld", (long long)v->i64);
}

static void xi_free_text(IsamFieldValue* fields, int n)
{
    int i;
    if (!fields) return;
    for (i = 0; i < n; i++) {
        free(fields[i].textOut);
        fields[i].textOut = NULL;
    }
}

int IsamExportImport_ExportJson(const char* sourceIsdb, const char* outputJsonPath, const IsamSchema* schema)
{
    Engine* e;
    FILE* f;
    IsamFieldValue* fields;
    int i, firstRec = 1, n;
    if (!sourceIsdb || !outputJsonPath || !schema) return GMB_DB_ARG;
    e = Engine_Open(sourceIsdb, schema);
    if (!e) return GMB_DB_ERR;
    n = schema->FieldCount;
    fields = (IsamFieldValue*)calloc((size_t)n, sizeof(*fields));
    if (!fields) { Engine_Dispose(e); return GMB_DB_ERR; }
    for (i = 0; i < n; i++) {
        if (!xi_is_text(&schema->Fields[i])) continue;
        fields[i].textOutCap = schema->Fields[i].Length + 1;
        if (fields[i].textOutCap < 2) fields[i].textOutCap = 2;
        fields[i].textOut = (char*)malloc((size_t)fields[i].textOutCap);
        if (!fields[i].textOut) {
            while (--i >= 0) free(fields[i].textOut);
            free(fields);
            Engine_Dispose(e);
            return GMB_DB_ERR;
        }
    }
    f = fopen(outputJsonPath, "wb");
    if (!f) {
        for (i = 0; i < n; i++) free(fields[i].textOut);
        free(fields);
        Engine_Dispose(e);
        return GMB_DB_ERR;
    }
    fprintf(f, "{\n  \"fileFormatVersion\": 1,\n  \"exportFormatVersion\": 1,\n  \"typeName\":");
    xi_putc_esc(f, schema->TypeName);
    fprintf(f, ",\n  \"primaryKey\":");
    xi_index(f, &schema->PrimaryKey);
    fprintf(f, ",\n  \"secondaryKeys\":[");
    for (i = 0; i < schema->SecondaryKeyCount; i++) {
        if (i) fputc(',', f);
        xi_index(f, &schema->SecondaryKeys[i]);
    }
    fprintf(f, "],\n  \"fields\":[");
    for (i = 0; i < n; i++) {
        if (i) fputc(',', f);
        fprintf(f, "{\"name\":");
        xi_putc_esc(f, schema->Fields[i].Name);
        fprintf(f, ",\"type\":");
        xi_putc_esc(f, schema->Fields[i].DataTypeCode);
        fprintf(f, ",\"length\":%d}", schema->Fields[i].Length);
    }
    fprintf(f, "],\n  \"records\":[");
    if (Engine_Go(e, CursorMoveOperation_Top) == GMB_DB_OK) {
        while (!Engine_IsEof(e)) {
            if (Engine_GetCurrent(e, fields, n, NULL) == GMB_DB_OK) {
                if (!firstRec) fputc(',', f);
                firstRec = 0;
                fputc('{', f);
                for (i = 0; i < n; i++) {
                    if (i) fputc(',', f);
                    xi_putc_esc(f, schema->Fields[i].Name);
                    fputc(':', f);
                    if (xi_is_text(&schema->Fields[i])) {
                        const char* t = fields[i].text ? fields[i].text : (fields[i].textOut ? fields[i].textOut : "");
                        xi_putc_esc(f, t);
                    } else if (IsamField_IsFloat(&schema->Fields[i])) {
                        char fb[40];
                        IsamField_FormatFloat(&schema->Fields[i], fields[i].i64, fb, (int)sizeof(fb));
                        fputs(fb, f);
                    } else {
                        fprintf(f, "%lld", (long long)fields[i].i64);
                    }
                }
                fputc('}', f);
            }
            if (Engine_Skip(e, 1) != GMB_DB_OK) break;
        }
    }
    fprintf(f, "]\n}\n");
    fclose(f);
    for (i = 0; i < n; i++) free(fields[i].textOut);
    free(fields);
    Engine_Dispose(e);
    return GMB_DB_OK;
}

static const char* xi_ws(const char* p)
{
    while (p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    return p;
}

static const char* xi_match(const char* p)
{
    int depth = 0;
    char open = *p, close;
    if (open != '{' && open != '[') return p;
    close = (char)(open == '{' ? '}' : ']');
    do {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
            if (*p == '"') p++;
            continue;
        }
        if (*p == open) depth++;
        else if (*p == close) { depth--; if (depth == 0) return p + 1; }
        if (*p) p++;
    } while (*p);
    return p;
}

static const char* xi_find_key(const char* s, const char* e, const char* key)
{
    char pat[128];
    const char* p;
    int klen;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    klen = (int)strlen(pat);
    p = s;
    while (p && p < e) {
        p = strstr(p, pat);
        if (!p || p >= e) return NULL;
        if (p == s || p[-1] == '{' || p[-1] == ',' || p[-1] == '[' || p[-1] == ' ' || p[-1] == '\n') {
            const char* q = xi_ws(p + klen);
            if (*q == ':') return q + 1;
        }
        p += klen;
    }
    return NULL;
}

static int xi_str(const char* s, const char* e, const char* key, char* out, int cap)
{
    const char* p = xi_find_key(s, e, key);
    int n = 0;
    if (!p || !out || cap <= 0) return 0;
    p = xi_ws(p);
    if (*p != '"') return 0;
    p++;
    while (p < e && *p && *p != '"' && n < cap - 1) {
        if (*p == '\\' && p[1]) { p++; out[n++] = *p++; }
        else out[n++] = *p++;
    }
    out[n] = 0;
    return 1;
}

static int xi_i64(const char* s, const char* e, const char* key, int64_t* out)
{
    const char* p = xi_find_key(s, e, key);
    if (!p || !out) return 0;
    *out = (int64_t)strtoll(xi_ws(p), NULL, 10);
    return 1;
}

static int xi_bool(const char* s, const char* e, const char* key)
{
    const char* p = xi_find_key(s, e, key);
    if (!p) return 0;
    p = xi_ws(p);
    return p[0] == 't' || p[0] == 'T';
}

static int xi_parse_index(const char* s, const char* e, IsamIndexDefinition* d)
{
    int64_t kl = 0;
    memset(d, 0, sizeof(*d));
    if (!xi_str(s, e, "name", d->Name, (int)sizeof(d->Name))) return GMB_DB_ERR;
    xi_str(s, e, "keyExpression", d->KeyExpression, (int)sizeof(d->KeyExpression));
    xi_i64(s, e, "keyLength", &kl);
    d->KeyLength = (int)kl;
    d->IsUnique = xi_bool(s, e, "isUnique");
    d->IsUnsafe = xi_bool(s, e, "isUnsafe");
    xi_str(s, e, "partialPredicate", d->PartialPredicate, (int)sizeof(d->PartialPredicate));
    return GMB_DB_OK;
}

int IsamExportImport_ImportJson(const char* destinationIsdb, const char* inputJsonPath)
{
    FILE* f;
    char* json;
    long sz;
    const char *end, *pk0, *pk1, *fields0, *fields1, *recs0, *recs1, *p;
    IsamSchema schema;
    IsamFieldSchema* fields = NULL;
    IsamIndexDefinition* secs = NULL;
    int nFields = 0, nSec = 0, rc = GMB_DB_ERR, i;
    Engine* e = NULL;
    if (!destinationIsdb || !inputJsonPath) return GMB_DB_ARG;
    if (access(destinationIsdb, F_OK) == 0) return GMB_DB_ERR;
    f = fopen(inputJsonPath, "rb");
    if (!f) return GMB_DB_ERR;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    json = (char*)malloc((size_t)sz + 1);
    if (!json) { fclose(f); return GMB_DB_ERR; }
    if ((long)fread(json, 1, (size_t)sz, f) != sz) { fclose(f); free(json); return GMB_DB_ERR; }
    fclose(f);
    json[sz] = 0;
    end = json + sz;
    memset(&schema, 0, sizeof(schema));
    if (!xi_str(json, end, "typeName", schema.TypeName, (int)sizeof(schema.TypeName))) goto done;
    pk0 = xi_ws(xi_find_key(json, end, "primaryKey"));
    if (!pk0 || *pk0 != '{') goto done;
    pk1 = xi_match(pk0);
    if (xi_parse_index(pk0, pk1, &schema.PrimaryKey) != GMB_DB_OK) goto done;
    p = xi_ws(xi_find_key(json, end, "secondaryKeys"));
    if (p && *p == '[') {
        const char* se = xi_match(p);
        p = xi_ws(p + 1);
        while (p < se && *p && *p != ']') {
            const char *o1;
            if (*p == ',') { p = xi_ws(p + 1); continue; }
            if (*p != '{') break;
            o1 = xi_match(p);
            secs = (IsamIndexDefinition*)realloc(secs, (size_t)(nSec + 1) * sizeof(*secs));
            if (!secs) goto done;
            if (xi_parse_index(p, o1, &secs[nSec]) != GMB_DB_OK) goto done;
            nSec++;
            p = xi_ws(o1);
        }
    }
    schema.SecondaryKeys = secs;
    schema.SecondaryKeyCount = nSec;
    fields0 = xi_ws(xi_find_key(json, end, "fields"));
    if (!fields0 || *fields0 != '[') goto done;
    fields1 = xi_match(fields0);
    p = xi_ws(fields0 + 1);
    while (p < fields1 && *p && *p != ']') {
        const char *o1;
        int64_t len = 0;
        if (*p == ',') { p = xi_ws(p + 1); continue; }
        if (*p != '{') break;
        o1 = xi_match(p);
        fields = (IsamFieldSchema*)realloc(fields, (size_t)(nFields + 1) * sizeof(*fields));
        if (!fields) goto done;
        memset(&fields[nFields], 0, sizeof(fields[nFields]));
        if (!xi_str(p, o1, "name", fields[nFields].Name, (int)sizeof(fields[nFields].Name))) goto done;
        if (!xi_str(p, o1, "type", fields[nFields].DataTypeCode, (int)sizeof(fields[nFields].DataTypeCode)))
            goto done;
        xi_i64(p, o1, "length", &len);
        fields[nFields].Length = (int)len;
        nFields++;
        p = xi_ws(o1);
    }
    schema.Fields = fields;
    schema.FieldCount = nFields;
    if (nFields <= 0) goto done;
    if (IsamFileHandle_CreateNew(destinationIsdb, &schema) != GMB_DB_OK) goto done;
    e = Engine_Open(destinationIsdb, &schema);
    if (!e) goto done;
    recs0 = xi_ws(xi_find_key(json, end, "records"));
    if (!recs0 || *recs0 != '[') goto done;
    recs1 = xi_match(recs0);
    p = xi_ws(recs0 + 1);
    while (p < recs1 && *p && *p != ']') {
        const char *o1;
        IsamFieldValue* vals;
        if (*p == ',') { p = xi_ws(p + 1); continue; }
        if (*p != '{') break;
        o1 = xi_match(p);
        vals = (IsamFieldValue*)calloc((size_t)nFields, sizeof(*vals));
        if (!vals) goto done;
        for (i = 0; i < nFields; i++) {
            if (xi_is_text(&fields[i])) {
                char* buf = (char*)malloc(256);
                if (!buf) { free(vals); goto done; }
                buf[0] = 0;
                xi_str(p, o1, fields[i].Name, buf, 256);
                vals[i].text = buf;
                vals[i].textOut = buf;
            } else {
                xi_i64(p, o1, fields[i].Name, &vals[i].i64);
            }
        }
        rc = Engine_Append(e, vals, nFields);
        for (i = 0; i < nFields; i++) free(vals[i].textOut);
        free(vals);
        if (rc != GMB_DB_OK) goto done;
        p = xi_ws(o1);
    }
    rc = GMB_DB_OK;
done:
    if (e) Engine_Dispose(e);
    free(fields);
    free(secs);
    free(json);
    return rc;
}

int IsamExportImport_SelfCheck(void)
{
    char src[] = "native/gmb_db_c/build/isam_xi_src.isdb";
    char json[] = "native/gmb_db_c/build/isam_xi.json";
    char dst[] = "native/gmb_db_c/build/isam_xi_dst.isdb";
    IsamFieldSchema fields[2];
    IsamIndexDefinition sec;
    IsamSchema schema;
    Engine *e, *r;
    IsamFieldValue vals[2];
    char nameOut[32];
    int i;
    unlink(dst);
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    memset(&sec, 0, sizeof(sec));
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
    schema.SecondaryKeys = &sec;
    schema.SecondaryKeyCount = 0;
    if (IsamFileHandle_CreateNew(src, &schema) != GMB_DB_OK) return -250;
    e = Engine_Open(src, &schema);
    if (!e) return -251;
    for (i = 1; i <= 3; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -252; }
    }
    Engine_Dispose(e);
    if (IsamExportImport_ExportJson(src, json, &schema) != GMB_DB_OK) return -253;
    if (IsamExportImport_ImportJson(dst, json) != GMB_DB_OK) return -254;
    r = Engine_Open(dst, &schema);
    if (!r) return -255;
    if (Engine_GetTotalRecordCount(r) != 3) { Engine_Dispose(r); return -256; }
    if (Engine_Go(r, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(r); return -257; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(r, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1) {
        Engine_Dispose(r); return -258;
    }
    Engine_Dispose(r);
    {
        char dbf[] = "native/gmb_db_c/build/isam_xi.dbf";
        char dst2[] = "native/gmb_db_c/build/isam_xi_dbf.isdb";
        DbfEngine* dbfE;
        char idTxt[32], nameTxt[32];
        IsamFieldSchema ifields[XI_MAX_FIELDS];
        IsamIndexDefinition isecs[XI_MAX_SECS];
        IsamSchema ischema;
        Engine* imp;
        unlink(dbf);
        unlink(dst2);
        if (IsamExportImport_ExportDbf(src, dbf, &schema) != GMB_DB_OK) return -259;
        dbfE = DbfEngine_New(dbf, NULL, 0, 1, LockingStrategy_IntraProcess);
        if (!dbfE) return -260;
        DbfEngine_GoTop(dbfE);
        if (DbfEngine_get_Eof(dbfE)) { DbfEngine_Dispose(dbfE); return -261; }
        if (DbfEngine_GetFieldText(dbfE, 0, idTxt, (int)sizeof(idTxt)) != GMB_DB_OK) {
            DbfEngine_Dispose(dbfE); return -262;
        }
        if (DbfEngine_GetFieldText(dbfE, 1, nameTxt, (int)sizeof(nameTxt)) != GMB_DB_OK) {
            DbfEngine_Dispose(dbfE); return -263;
        }
        xi_trim_trail(idTxt);
        xi_trim_trail(nameTxt);
        if (atoi(idTxt) != 1) { DbfEngine_Dispose(dbfE); return -264; }
        if (strcmp(nameTxt, "N1") != 0) { DbfEngine_Dispose(dbfE); return -265; }
        DbfEngine_Dispose(dbfE);
        if (IsamExportImport_ImportDbf(dbf, dst2, "Id", 16) != GMB_DB_OK) return -266;
        memset(&ischema, 0, sizeof(ischema));
        memset(ifields, 0, sizeof(ifields));
        memset(isecs, 0, sizeof(isecs));
        if (xi_load_schema(dst2, &ischema, ifields, XI_MAX_FIELDS, isecs, XI_MAX_SECS) != GMB_DB_OK)
            return -267;
        imp = Engine_Open(dst2, &ischema);
        if (!imp) return -268;
        if (Engine_GetTotalRecordCount(imp) != 3) { Engine_Dispose(imp); return -269; }
        if (Engine_Go(imp, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(imp); return -270; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(imp, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1 ||
            strcmp(nameOut, "N1") != 0) {
            Engine_Dispose(imp); return -271;
        }
        Engine_Dispose(imp);
    }
    return GMB_DB_OK;
}

int IsamExportImport_ExportDbf(const char* sourceIsdb, const char* outputDbfPath, const IsamSchema* schema)
{
    IsamFieldSchema localFields[XI_MAX_FIELDS];
    IsamIndexDefinition localSecs[XI_MAX_SECS];
    IsamSchema localSchema;
    DbfField dbfFields[XI_MAX_FIELDS];
    Engine* e = NULL;
    DbfFile* dbf = NULL;
    IsamFieldValue* vals = NULL;
    uint8_t* payload = NULL;
    const DbfHeader* hdr;
    int i, n, payloadLen, rc = GMB_DB_ERR;
    if (!sourceIsdb || !outputDbfPath) return GMB_DB_ARG;
    if (access(sourceIsdb, F_OK) != 0) return GMB_DB_ERR;
    if (access(outputDbfPath, F_OK) == 0) return GMB_DB_ERR;
    if (!schema) {
        memset(&localSchema, 0, sizeof(localSchema));
        memset(localFields, 0, sizeof(localFields));
        memset(localSecs, 0, sizeof(localSecs));
        if (xi_load_schema(sourceIsdb, &localSchema, localFields, XI_MAX_FIELDS,
                           localSecs, XI_MAX_SECS) != GMB_DB_OK)
            return GMB_DB_ERR;
        schema = &localSchema;
    }
    n = schema->FieldCount;
    if (n <= 0 || n > XI_MAX_FIELDS) return GMB_DB_ARG;
    for (i = 0; i < n; i++) xi_map_isam_to_dbf(&schema->Fields[i], &dbfFields[i]);
    if (DbfFile_CreateNew(outputDbfPath, "utf-8", dbfFields, n) != GMB_DB_OK) return GMB_DB_ERR;
    dbf = DbfFile_New(outputDbfPath, 1, LockingStrategy_IntraProcess);
    if (!dbf) { unlink(outputDbfPath); return GMB_DB_ERR; }
    hdr = DbfFile_get_Header(dbf);
    payloadLen = hdr ? (int)hdr->RecordLength - 1 : 0;
    if (payloadLen <= 0) goto done;
    payload = (uint8_t*)calloc((size_t)payloadLen, 1);
    vals = (IsamFieldValue*)calloc((size_t)n, sizeof(*vals));
    if (!payload || !vals) goto done;
    for (i = 0; i < n; i++) {
        if (!xi_is_text(&schema->Fields[i])) continue;
        vals[i].textOutCap = schema->Fields[i].Length + 1;
        if (vals[i].textOutCap < 2) vals[i].textOutCap = 2;
        vals[i].textOut = (char*)malloc((size_t)vals[i].textOutCap);
        if (!vals[i].textOut) goto done;
    }
    e = Engine_Open(sourceIsdb, schema);
    if (!e) goto done;
    if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) goto done;
    while (!Engine_IsEof(e)) {
        RecordMetadata meta;
        int off = 0;
        memset(&meta, 0, sizeof(meta));
        if (Engine_GetCurrent(e, vals, n, &meta) == GMB_DB_OK &&
            meta.Status != Config_RecordStatusDeleted) {
            memset(payload, 0, (size_t)payloadLen);
            for (i = 0; i < n; i++) {
                char txt[512];
                int flen = dbfFields[i].Length;
                if (off + flen > payloadLen) goto done;
                xi_fmt_value(&schema->Fields[i], &vals[i], txt, (int)sizeof(txt));
                DbfFieldWriter_Write(payload + off, flen, &dbfFields[i], txt, NULL);
                off += flen;
            }
            if (DbfFile_AppendRawRecord(dbf, payload, payloadLen) < 0) goto done;
        }
        if (Engine_Skip(e, 1) != GMB_DB_OK) break;
    }
    rc = GMB_DB_OK;
done:
    if (e) Engine_Dispose(e);
    xi_free_text(vals, n);
    free(vals);
    free(payload);
    if (dbf) DbfFile_Dispose(dbf);
    if (rc != GMB_DB_OK) unlink(outputDbfPath);
    return rc;
}

int IsamExportImport_ImportDbf(const char* sourceDbf, const char* destinationIsdb,
                               const char* primaryKeyFieldName, int primaryKeyLength)
{
    DbfEngine* dbf = NULL;
    Engine* e = NULL;
    const DbfField* dbfFields;
    IsamFieldSchema fields[XI_MAX_FIELDS];
    IsamSchema schema;
    const DbfField* pk;
    int n = 0, i, rc = GMB_DB_ERR;
    if (!sourceDbf || !destinationIsdb) return GMB_DB_ARG;
    if (access(sourceDbf, F_OK) != 0) return GMB_DB_ERR;
    if (access(destinationIsdb, F_OK) == 0) return GMB_DB_ERR;
    if (primaryKeyLength <= 0) primaryKeyLength = 16;
    dbf = DbfEngine_New(sourceDbf, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!dbf) return GMB_DB_ERR;
    dbfFields = DbfEngine_GetFields(dbf, &n);
    if (!dbfFields || n <= 0 || n > XI_MAX_FIELDS) goto done;
    pk = xi_infer_pk(dbfFields, n, primaryKeyFieldName);
    if (!pk) goto done;
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    xi_stem(destinationIsdb, schema.TypeName, (int)sizeof(schema.TypeName));
    if (!schema.TypeName[0]) snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Rec");
    for (i = 0; i < n; i++) xi_map_dbf_to_isam(&dbfFields[i], &fields[i]);
    schema.Fields = fields;
    schema.FieldCount = n;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", pk->Name);
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", pk->Name);
    schema.PrimaryKey.KeyLength = primaryKeyLength;
    schema.PrimaryKey.IsUnique = 1;
    schema.SecondaryKeys = NULL;
    schema.SecondaryKeyCount = 0;
    if (IsamFileHandle_CreateNew(destinationIsdb, &schema) != GMB_DB_OK) goto done;
    e = Engine_Open(destinationIsdb, &schema);
    if (!e) goto done;
    DbfEngine_GoTop(dbf);
    while (!DbfEngine_get_Eof(dbf)) {
        IsamFieldValue vals[XI_MAX_FIELDS];
        char parts[XI_MAX_FIELDS][256];
        if (!DbfEngine_IsCurrentRecordDeleted(dbf)) {
            memset(vals, 0, sizeof(vals));
            for (i = 0; i < n; i++) {
                parts[i][0] = 0;
                DbfEngine_GetFieldText(dbf, i, parts[i], (int)sizeof(parts[i]));
                xi_trim_trail(parts[i]);
                if (xi_is_text(&fields[i]))
                    vals[i].text = parts[i];
                else if (strcmp(fields[i].DataTypeCode, "BitType") == 0)
                    vals[i].i64 = (parts[i][0] == 'T' || parts[i][0] == 't' || parts[i][0] == 'Y' ||
                                   parts[i][0] == 'y' || parts[i][0] == '1');
                else if (IsamField_IsFloat(&fields[i]))
                    vals[i].i64 = IsamField_DoubleToBits(&fields[i], atof(parts[i]));
                else
                    vals[i].i64 = atoll(parts[i]);
            }
            if (Engine_Append(e, vals, n) != GMB_DB_OK) goto done;
        }
        DbfEngine_Skip(dbf, 1);
    }
    rc = GMB_DB_OK;
done:
    if (e) Engine_Dispose(e);
    if (dbf) DbfEngine_Dispose(dbf);
    if (rc != GMB_DB_OK) unlink(destinationIsdb);
    return rc;
}
