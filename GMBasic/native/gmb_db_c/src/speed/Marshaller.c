/* FILE: native/gmb_db_c/src/SPEED/Marshaller.c */
/* Port 1:1 di modules/Database/SPEED/Marshaller.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Marshaller.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Text;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Fornisce metodi statici per serializzare e deserializzare i tipi di dato del linguaggio BASIC
///// nel formato binario richiesto dal motore di storage Speed (LSM-Tree).
///// Questa classe è completamente disaccoppiata dall'Executor.
///// </summary>
//internal static class SpeedMarshaller
//{
//    /// <summary>
//    /// Serializza un valore BASIC (primitivo o UDT rappresentato da un dizionario) in un array di byte.
//    /// </summary>
//    /// <param name="value">Il valore da serializzare.</param>
//    /// <param name="dataType">Il tipo di dato del valore, che guida la serializzazione.</param>
//    /// <param name="schema">Lo schema completo, necessario per serializzare i tipi UDT.</param>
//    /// <returns>La rappresentazione binaria del valore.</returns>
//    /// <exception cref="NotSupportedException">Se il tipo di dato non è serializzabile.</exception>
//    public static byte[] ToBytes(object? value, DataType dataType, Config.SpeedSchema? schema = null)
//    {
//        if (value == null) return [];
//
//        switch (dataType)
//        {
//            case StringType: return Encoding.UTF8.GetBytes(Convert.ToString(value) ?? string.Empty);
//            case BitType: return BitConverter.GetBytes(Convert.ToBoolean(value));
//            case ByteType: return [Convert.ToByte(value)];
//            case SByteType: return [(byte)Convert.ToSByte(value)];
//            case IntegerType: return BitConverter.GetBytes(Convert.ToInt16(value));
//            case UIntegerType: return BitConverter.GetBytes(Convert.ToUInt16(value));
//            case LongType: return BitConverter.GetBytes(Convert.ToInt32(value));
//            case ULongType: return BitConverter.GetBytes(Convert.ToUInt32(value));
//            case QuadType: return BitConverter.GetBytes(Convert.ToInt64(value));
//            case UQuadType: return BitConverter.GetBytes(Convert.ToUInt64(value));
//            case SingleType: return BitConverter.GetBytes(Convert.ToSingle(value));
//            case DoubleType: return BitConverter.GetBytes(Convert.ToDouble(value));
//            case CurrencyType: 
//                var bits = decimal.GetBits(Convert.ToDecimal(value));
//                using (var ms = new MemoryStream(16))
//                {
//                    using var writer = new BinaryWriter(ms);
//                    writer.Write(bits[0]);
//                    writer.Write(bits[1]);
//                    writer.Write(bits[2]);
//                    writer.Write(bits[3]);
//                    return ms.ToArray();
//                }
//            case DateType: return BitConverter.GetBytes(((DateOnly)value).ToDateTime(TimeOnly.MinValue).Ticks);
//            case DateTimeType: return BitConverter.GetBytes(((DateTime)value).Ticks);
//            case JsonType: return Encoding.UTF8.GetBytes(Convert.ToString(value) ?? "{}");
//
//            case UserDefinedType:
//                if (value is not Dictionary<string, object?> udtFields)
//                    throw new ArgumentException("Il valore per un UDT deve essere un Dictionary<string, object?>.", nameof(value));
//                if (schema is null)
//                    throw new ArgumentNullException(nameof(schema), "Lo schema è necessario per serializzare un UDT.");
//                return SerializeUdt(udtFields, schema);
//
//            default:
//                throw new NotSupportedException($"La serializzazione per il tipo '{dataType.GetType().Name}' non è supportata dal motore Speed.");
//        }
//    }
//    
//    // NUOVO OVERLOAD PER SEMPLIFICARE LA SERIALIZZAZIONE DI UDT
//    public static byte[] ToBytes(IReadOnlyDictionary<string, object?> udtFields, Config.SpeedSchema schema)
//    {
//        return SerializeUdt(udtFields, schema);
//    }
//    
//
//    /// <summary>
//    /// Deserializza un array di byte in un valore BASIC.
//    /// </summary>
//    /// <param name="buffer">L'array di byte da deserializzare.</param>
//    /// <param name="targetType">Il tipo di dato di destinazione.</param>
//    /// <param name="schema">Lo schema completo, necessario per deserializzare i tipi UDT.</param>
//    /// <returns>Il valore BASIC deserializzato (un tipo primitivo o un Dictionary per UDT).</returns>
//    /// <exception cref="NotSupportedException">Se il tipo di dato non è deserializzabile.</exception>
//    public static object? FromBytes(byte[] buffer, DataType targetType, Config.SpeedSchema? schema = null)
//    {
//        if (buffer.Length == 0) return GetDefaultValue(targetType);
//
//        switch (targetType)
//        {
//            case StringType: return Encoding.UTF8.GetString(buffer);
//            case BitType: return BitConverter.ToBoolean(buffer, 0);
//            case ByteType: return buffer[0];
//            case SByteType: return (sbyte)buffer[0];
//            case IntegerType: return BitConverter.ToInt16(buffer, 0);
//            case UIntegerType: return BitConverter.ToUInt16(buffer, 0);
//            case LongType: return BitConverter.ToInt32(buffer, 0);
//            case ULongType: return BitConverter.ToUInt32(buffer, 0);
//            case QuadType: return BitConverter.ToInt64(buffer, 0);
//            case UQuadType: return BitConverter.ToUInt64(buffer, 0);
//            case SingleType: return BitConverter.ToSingle(buffer, 0);
//            case DoubleType: return BitConverter.ToDouble(buffer, 0);
//            case CurrencyType:
//                var bits = new int[4];
//                bits[0] = BitConverter.ToInt32(buffer, 0);
//                bits[1] = BitConverter.ToInt32(buffer, 4);
//                bits[2] = BitConverter.ToInt32(buffer, 8);
//                bits[3] = BitConverter.ToInt32(buffer, 12);
//                return new decimal(bits);
//            case DateType: return DateOnly.FromDateTime(new DateTime(BitConverter.ToInt64(buffer, 0)));
//            case DateTimeType: return new DateTime(BitConverter.ToInt64(buffer, 0));
//            case JsonType: return Encoding.UTF8.GetString(buffer);
//
//            case UserDefinedType:
//                if (schema is null)
//                    throw new ArgumentNullException(nameof(schema), "Lo schema è necessario per deserializzare un UDT.");
//                return DeserializeUdt(buffer, schema);
//                
//            default:
//                throw new NotSupportedException($"La deserializzazione per il tipo '{targetType.GetType().Name}' non è supportata dal motore Speed.");
//        }
//    }
//    
//    // NUOVO OVERLOAD PER SEMPLIFICARE LA DESERIALIZZAZIONE DI UDT
//    public static object FromBytes(byte[] buffer, Config.SpeedSchema schema)
//    {
//        return DeserializeUdt(buffer, schema);
//    }
//    
//    /// <summary>
//    /// Restituisce il valore di default per un dato tipo di dato del linguaggio,
//    /// che corrisponde allo stato "vuoto" di un campo.
//    /// </summary>
//    /// <param name="dataType">Il tipo di dato per cui recuperare il valore di default.</param>
//    /// <returns>Un oggetto che rappresenta il valore di default per il tipo specificato.</returns>
//    private static object? GetDefaultValue(DataType dataType)
//    {
//        // Se il tipo è definito, usa il GetClrType
//        {
//            var clrType = dataType.GetClrType();
//            if (clrType.IsValueType)
//            {
//                return Activator.CreateInstance(clrType);
//            }
//        }
//
//        return dataType switch
//        {
//            StringType => string.Empty,
//            JsonType => "{}",
//            UserDefinedType => new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase),
//            _ => null
//        };
//    }
//
//    /// <summary>
//    /// Serializza i campi di un UDT (rappresentato come dizionario) in un array di byte compatto.
//    /// </summary>
//    /// <param name="udtFields">Il dizionario di coppie nome-campo/valore che rappresenta l'istanza UDT.</param>
//    /// <param name="schema">Lo schema della tabella, che definisce l'ordine e il tipo di ogni campo da serializzare.</param>
//    /// <returns>Un array di byte che rappresenta l'intero record UDT serializzato.</returns>
//    private static byte[] SerializeUdt(IReadOnlyDictionary<string, object?> udtFields, Config.SpeedSchema schema)
//    {
//        using var stream = new MemoryStream();
//        using var writer = new BinaryWriter(stream, Encoding.UTF8);
//
//        foreach (var field in schema.Fields)
//        {
//            var value = udtFields.GetValueOrDefault(field.Name);
//            WriteFieldValue(writer, value, field.DataType, schema);
//        }
//        return stream.ToArray();
//    }
//
//    /// <summary>
//    /// Deserializza un array di byte in un dizionario che rappresenta i campi di un UDT.
//    /// </summary>
//    /// <param name="buffer">L'array di byte contenente il record UDT serializzato.</param>
//    /// <param name="schema">Lo schema della tabella, essenziale per interpretare correttamente i dati nel buffer.</param>
//    /// <returns>Un dizionario di coppie nome-campo/valore che rappresenta l'istanza UDT ricostruita.</returns>
//    private static Dictionary<string, object?> DeserializeUdt(byte[] buffer, Config.SpeedSchema schema)
//    {
//        using var stream = new MemoryStream(buffer);
//        using var reader = new BinaryReader(stream, Encoding.UTF8);
//        
//        var fields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        foreach (var field in schema.Fields)
//        {
//            fields[field.Name] = ReadFieldValue(reader, field.DataType, schema);
//        }
//        return fields;
//    }
//    
//    /// <summary>
//    /// Scrive un singolo valore su un BinaryWriter, precedendolo con la sua lunghezza in byte.
//    /// </summary>
//    /// <param name="writer">Il BinaryWriter su cui scrivere i dati.</param>
//    /// <param name="value">Il valore dell'oggetto da scrivere.</param>
//    /// <param name="dataType">Il tipo di dato del valore, che guida la serializzazione.</param>
//    /// <param name="schema">Lo schema completo della tabella, necessario per la serializzazione ricorsiva di UDT annidati.</param>
//    private static void WriteFieldValue(BinaryWriter writer, object? value, DataType dataType, Config.SpeedSchema schema)
//    {
//        var bytes = ToBytes(value, dataType, schema);
//        writer.Write(bytes.Length);
//        writer.Write(bytes);
//    }
//
//    /// <summary>
//    /// Legge un singolo valore da un BinaryReader, aspettandosi il formato lunghezza + dati.
//    /// </summary>
//    /// <param name="reader">Il BinaryReader da cui leggere i dati.</param>
//    /// <param name="dataType">Il tipo di dato di destinazione in cui deserializzare i byte.</param>
//    /// <param name="schema">Lo schema completo della tabella, necessario per la deserializzazione ricorsiva di UDT annidati.</param>
//    /// <returns>L'oggetto deserializzato, o il valore di default del tipo se la lunghezza letta è zero.</returns>
//    private static object? ReadFieldValue(BinaryReader reader, DataType dataType, Config.SpeedSchema schema)
//    {
//        var length = reader.ReadInt32();
//        if (length == 0) return GetDefaultValue(dataType);
//        var bytes = reader.ReadBytes(length);
//        return FromBytes(bytes, dataType, schema);
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

static int sm_copy(const uint8_t* src, int srcLen, uint8_t** out, int* outLen)
{
    uint8_t* p;
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (srcLen < 0) srcLen = 0;
    p = (uint8_t*)malloc((size_t)srcLen + 1);
    if (!p) return 0;
    if (srcLen > 0 && src) memcpy(p, src, (size_t)srcLen);
    p[srcLen] = 0;
    if (out) *out = p;
    if (outLen) *outLen = srcLen;
    return 1;
}

static int32_t sm_rd_le32(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void sm_wr_le32(uint8_t* p, int32_t v)
{
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)(u & 0xffu);
    p[1] = (uint8_t)((u >> 8) & 0xffu);
    p[2] = (uint8_t)((u >> 16) & 0xffu);
    p[3] = (uint8_t)((u >> 24) & 0xffu);
}

static int sm_fixed_size(SpeedDataTypeKind kind)
{
    switch (kind) {
        case SpeedDataType_Bit: case SpeedDataType_Byte: case SpeedDataType_SByte: return 1;
        case SpeedDataType_Integer: case SpeedDataType_UInteger: return 2;
        case SpeedDataType_Long: case SpeedDataType_ULong: case SpeedDataType_Single: return 4;
        case SpeedDataType_Quad: case SpeedDataType_UQuad: case SpeedDataType_Double:
        case SpeedDataType_Date: case SpeedDataType_DateTime: return 8;
        case SpeedDataType_Currency: return 16;
        default: return 0;
    }
}

SpeedDataTypeKind SpeedDataType_Parse(const char* typeName)
{
    char buf[64];
    size_t n;
    if (!typeName || !typeName[0]) return SpeedDataType_Unknown;
    snprintf(buf, sizeof(buf), "%s", typeName);
    n = strlen(buf);
    if (n < 4 || gmb_strcasecmp(buf + n - 4, "Type") != 0) {
        if (n + 4 < sizeof(buf)) strcat(buf, "Type");
    }
    if (gmb_strcasecmp(buf, "StringType") == 0) return SpeedDataType_String;
    if (gmb_strcasecmp(buf, "BitType") == 0) return SpeedDataType_Bit;
    if (gmb_strcasecmp(buf, "ByteType") == 0) return SpeedDataType_Byte;
    if (gmb_strcasecmp(buf, "SByteType") == 0) return SpeedDataType_SByte;
    if (gmb_strcasecmp(buf, "IntegerType") == 0) return SpeedDataType_Integer;
    if (gmb_strcasecmp(buf, "UIntegerType") == 0) return SpeedDataType_UInteger;
    if (gmb_strcasecmp(buf, "LongType") == 0) return SpeedDataType_Long;
    if (gmb_strcasecmp(buf, "ULongType") == 0) return SpeedDataType_ULong;
    if (gmb_strcasecmp(buf, "QuadType") == 0) return SpeedDataType_Quad;
    if (gmb_strcasecmp(buf, "UQuadType") == 0) return SpeedDataType_UQuad;
    if (gmb_strcasecmp(buf, "SingleType") == 0) return SpeedDataType_Single;
    if (gmb_strcasecmp(buf, "DoubleType") == 0) return SpeedDataType_Double;
    if (gmb_strcasecmp(buf, "CurrencyType") == 0) return SpeedDataType_Currency;
    if (gmb_strcasecmp(buf, "DateType") == 0) return SpeedDataType_Date;
    if (gmb_strcasecmp(buf, "DateTimeType") == 0) return SpeedDataType_DateTime;
    if (gmb_strcasecmp(buf, "JsonType") == 0) return SpeedDataType_Json;
    if (gmb_strcasecmp(buf, "UserDefinedType") == 0) return SpeedDataType_UserDefined;
    return SpeedDataType_Unknown;
}

const char* SpeedDataType_Name(SpeedDataTypeKind kind)
{
    switch (kind) {
        case SpeedDataType_String: return "StringType";
        case SpeedDataType_Bit: return "BitType";
        case SpeedDataType_Byte: return "ByteType";
        case SpeedDataType_SByte: return "SByteType";
        case SpeedDataType_Integer: return "IntegerType";
        case SpeedDataType_UInteger: return "UIntegerType";
        case SpeedDataType_Long: return "LongType";
        case SpeedDataType_ULong: return "ULongType";
        case SpeedDataType_Quad: return "QuadType";
        case SpeedDataType_UQuad: return "UQuadType";
        case SpeedDataType_Single: return "SingleType";
        case SpeedDataType_Double: return "DoubleType";
        case SpeedDataType_Currency: return "CurrencyType";
        case SpeedDataType_Date: return "DateType";
        case SpeedDataType_DateTime: return "DateTimeType";
        case SpeedDataType_Json: return "JsonType";
        case SpeedDataType_UserDefined: return "UserDefinedType";
        default: return "StringType";
    }
}

int SpeedDataType_IsNumeric(SpeedDataTypeKind kind)
{
    switch (kind) {
        case SpeedDataType_Byte: case SpeedDataType_SByte:
        case SpeedDataType_Integer: case SpeedDataType_UInteger:
        case SpeedDataType_Long: case SpeedDataType_ULong:
        case SpeedDataType_Quad: case SpeedDataType_UQuad:
        case SpeedDataType_Single: case SpeedDataType_Double:
        case SpeedDataType_Currency:
            return 1;
        default:
            return 0;
    }
}

int SpeedMarshaller_ToBytes(SpeedDataTypeKind kind, const uint8_t* src, int srcLen,
                            uint8_t** out, int* outLen)
{
    int fixed;
    if (!src) srcLen = 0;
    if (srcLen < 0) srcLen = 0;
    if (kind == SpeedDataType_Unknown) kind = SpeedDataType_String;
    if (srcLen == 0) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof(zeros));
        if (kind == SpeedDataType_String || kind == SpeedDataType_Json || kind == SpeedDataType_UserDefined)
            return sm_copy(NULL, 0, out, outLen);
        fixed = sm_fixed_size(kind);
        return sm_copy(zeros, fixed, out, outLen);
    }
    return sm_copy(src, srcLen, out, outLen);
}

int SpeedMarshaller_FromBytes(SpeedDataTypeKind kind, const uint8_t* buf, int bufLen,
                              uint8_t** out, int* outLen)
{
    (void)kind;
    if (!buf || bufLen <= 0) return sm_copy(NULL, 0, out, outLen);
    return sm_copy(buf, bufLen, out, outLen);
}

int SpeedMarshaller_SerializeUdt(const Manifest* man, int tableId,
                                 const uint8_t* const* fieldValues, const int* fieldLens,
                                 uint8_t** out, int* outLen)
{
    int n, i, total = 0;
    uint8_t* buf;
    int off = 0;
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (!man) return 0;
    n = Manifest_FieldCount(man, tableId);
    if (n <= 0) return 0;
    for (i = 0; i < n; i++) {
        int fl = (fieldLens && i < n) ? fieldLens[i] : 0;
        if (fl < 0) fl = 0;
        total += 4 + fl;
    }
    buf = (uint8_t*)malloc((size_t)total);
    if (!buf) return 0;
    for (i = 0; i < n; i++) {
        int fl = (fieldLens && i < n) ? fieldLens[i] : 0;
        const uint8_t* fv = (fieldValues && fieldValues[i]) ? fieldValues[i] : NULL;
        if (fl < 0) fl = 0;
        sm_wr_le32(buf + off, fl);
        off += 4;
        if (fl > 0 && fv) memcpy(buf + off, fv, (size_t)fl);
        off += fl;
    }
    if (out) *out = buf;
    if (outLen) *outLen = total;
    return 1;
}

int SpeedMarshaller_ExtractField(const Manifest* man, int tableId,
                                 const uint8_t* udt, int udtLen, const char* fieldName,
                                 uint8_t** out, int* outLen)
{
    int n, i, off = 0;
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (!man || !udt || udtLen < 0 || !fieldName) return 0;
    n = Manifest_FieldCount(man, tableId);
    for (i = 0; i < n; i++) {
        char name[64];
        int fl;
        if (!Manifest_GetField(man, tableId, i, name, (int)sizeof(name), NULL, 0, NULL))
            return 0;
        if (off + 4 > udtLen) return 0;
        fl = sm_rd_le32(udt + off);
        off += 4;
        if (fl < 0 || off + fl > udtLen) return 0;
        if (gmb_strcasecmp(name, fieldName) == 0)
            return sm_copy(udt + off, fl, out, outLen);
        off += fl;
    }
    return 0;
}

int SpeedMarshaller_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/speedsch_XXXXXX";
    Database* db;
    Manifest* man;
    int tableId, idxId, n, len, found, vlen;
    char fname[64], tname[32];
    uint8_t *udt = NULL, *extracted = NULL, *val = NULL;
    const uint8_t* fields[2];
    int flens[2];
    uint8_t age[4];
    FILE* f;
    char path[1088], line[256];
    int sawType = 0;

    if (SpeedDataType_Parse("String") != SpeedDataType_String) return -100;
    if (SpeedDataType_Parse("LongType") != SpeedDataType_Long) return -101;
    if (!SpeedDataType_IsNumeric(SpeedDataType_Long) || SpeedDataType_IsNumeric(SpeedDataType_String))
        return -102;

    if (!mkdtemp(dir)) return -103;
    db = Database_Open(dir);
    if (!db) return -104;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -105; }
    man = Database_GetManifest(db);
    if (Manifest_AddField(man, tableId, "Name", "StringType", 0) != GMB_DB_OK) {
        Database_Dispose(db); return -106;
    }
    if (Manifest_AddField(man, tableId, "Age", "Long", 0) != GMB_DB_OK) {
        Database_Dispose(db); return -107;
    }
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -108;
    man = Database_GetManifest(db);
    if (!Manifest_TryGetTableByName(man, "Person", &tableId)) { Database_Dispose(db); return -109; }
    n = Manifest_FieldCount(man, tableId);
    if (n != 2 || !Manifest_GetField(man, tableId, 0, fname, (int)sizeof(fname), tname, (int)sizeof(tname), &len) ||
        strcmp(fname, "Name") != 0 || strcmp(tname, "StringType") != 0) {
        Database_Dispose(db); return -110;
    }
    if (!Manifest_GetField(man, tableId, 1, fname, (int)sizeof(fname), tname, (int)sizeof(tname), &len) ||
        strcmp(fname, "Age") != 0 || strcmp(tname, "LongType") != 0) {
        Database_Dispose(db); return -111;
    }
    snprintf(path, sizeof(path), "%s/manifest.json", dir);
    f = fopen(path, "rb");
    if (!f) { Database_Dispose(db); return -112; }
    while (fgets(line, (int)sizeof(line), f)) {
        if (strstr(line, "\"$type\": \"StringType\"")) sawType = 1;
    }
    fclose(f);
    if (!sawType) { Database_Dispose(db); return -113; }

    fields[0] = (const uint8_t*)"alice";
    flens[0] = 5;
    age[0] = 42; age[1] = 0; age[2] = 0; age[3] = 0;
    fields[1] = age;
    flens[1] = 4;
    if (!SpeedMarshaller_SerializeUdt(man, tableId, fields, flens, &udt, &n) || n != 17) {
        free(udt); Database_Dispose(db); return -114;
    }
    if (!SpeedMarshaller_ExtractField(man, tableId, udt, n, "Name", &extracted, &len) ||
        len != 5 || memcmp(extracted, "alice", 5) != 0) {
        free(extracted); free(udt); Database_Dispose(db); return -115;
    }
    free(extracted);
    if (Database_Set(db, tableId, (const uint8_t*)"k001", 4, udt, n) != GMB_DB_OK) {
        free(udt); Database_Dispose(db); return -116;
    }
    idxId = Database_CreateSecondaryIndex(db, tableId, "ByName", "Name", 0);
    if (idxId <= 0) { free(udt); Database_Dispose(db); return -117; }
    found = Database_Get(db, idxId, (const uint8_t*)"alice", 5, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "k001", 4) != 0) {
        free(val); free(udt); Database_Dispose(db); return -118;
    }
    free(val);
    free(udt);
    Database_Dispose(db);
    return GMB_DB_OK;
}
