// FILE: Basic.ISAM.IsamExportImport

using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.DBF;
using Basic.Tools.Isam;
using Basic.Databases;
using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Export/import JSON round-trip per tooling offline (Phase G3).
/// </summary>
public static class IsamExportImport
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    /// <summary>Esporta schema ISCH + record attivi in JSON.</summary>
    public static void ExportJson(string sourceIsdb, string outputJsonPath, KeyExpressionEvaluatorCallback evaluator)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputJsonPath);
        ArgumentNullException.ThrowIfNull(evaluator);

        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
        var records = new JsonArray();
        using (var engine = IsamOfflineToolkit.OpenEngine(sourceIsdb, evaluator))
        {
            engine.Go(Config.CursorMoveOperation.Top);
            while (!engine.IsEof())
            {
                var record = engine.GetCurrentRecord();
                if (record is not null)
                {
                    records.Add(RecordToJson(snapshot.Schema, record));
                }

                engine.Skip();
            }
        }

        var payload = new JsonObject
        {
            ["fileFormatVersion"] = snapshot.Header.Version,
            ["exportFormatVersion"] = 1,
            ["typeName"] = snapshot.Schema.TypeName,
            ["primaryKey"] = IndexToJson(snapshot.Schema.PrimaryKey),
            ["secondaryKeys"] = new JsonArray(snapshot.Schema.SecondaryKeys.Select(IndexToJson).ToArray()),
            ["fields"] = new JsonArray(snapshot.Schema.Fields.Select(FieldToJson).ToArray()),
            ["records"] = records,
        };

        var json = payload.ToJsonString(JsonOptions);
        File.WriteAllText(outputJsonPath, json, Encoding.UTF8);
    }

    /// <summary>Crea un nuovo <c>.isdb</c> da export JSON.</summary>
    public static void ImportJson(string destinationIsdb, string inputJsonPath, KeyExpressionEvaluatorCallback evaluator)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(destinationIsdb);
        ArgumentException.ThrowIfNullOrWhiteSpace(inputJsonPath);
        ArgumentNullException.ThrowIfNull(evaluator);

        if (!File.Exists(inputJsonPath))
        {
            throw new FileNotFoundException("JSON export not found.", inputJsonPath);
        }

        if (File.Exists(destinationIsdb))
        {
            throw new InvalidOperationException($"Destination already exists: {destinationIsdb}");
        }

        var root = JsonNode.Parse(File.ReadAllText(inputJsonPath, Encoding.UTF8))
                   ?? throw new InvalidDataException("Invalid JSON export.");

        var schema = ParseSchema(root);
        IsamFileHandle.CreateNew(destinationIsdb, schema, Encoding.UTF8);

        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
        using var handle = new IsamFileHandle(destinationIsdb, schema, udt, LockingStrategy.IntraProcess);
        using var engine = new Engine(handle) { Evaluator = evaluator };

        var records = root["records"]?.AsArray()
                      ?? throw new InvalidDataException("JSON export missing 'records' array.");

        foreach (var node in records)
        {
            if (node is not JsonObject row)
            {
                continue;
            }

            engine.Append(ParseRecord(schema, udt, row), evaluator);
        }

        handle.Commit();
    }

    /// <summary>Esporta record attivi in un file DBF Visual FoxPro (Phase G3).</summary>
    public static void ExportDbf(string sourceIsdb, string outputDbfPath, KeyExpressionEvaluatorCallback evaluator)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputDbfPath);
        ArgumentNullException.ThrowIfNull(evaluator);

        if (File.Exists(outputDbfPath))
        {
            throw new InvalidOperationException($"Destination already exists: {outputDbfPath}");
        }

        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
        var dbfFields = TranslateIsamSchemaToDbf(snapshot.Schema);
        var encoding = Encoding.UTF8;
        DbfFile.CreateNew(outputDbfPath, encoding, dbfFields);

        using (var dbfFile = new DbfFile(outputDbfPath, exclusive: true))
        using (var isamEngine = IsamOfflineToolkit.OpenEngine(sourceIsdb, evaluator))
        {
            var payload = new byte[dbfFile.Header.RecordLength - 1];
            isamEngine.Go(Config.CursorMoveOperation.Top);
            while (!isamEngine.IsEof())
            {
                var record = isamEngine.GetCurrentRecord();
                if (record is not null)
                {
                    WriteDbfRecordPayload(dbfFields, RecordToDbfValues(snapshot.Schema, record), payload, encoding);
                    dbfFile.AppendRawRecord(payload);
                }

                isamEngine.Skip();
            }
        }
    }

    /// <summary>Importa un file DBF in un nuovo <c>.isdb</c> (Phase G3 round-trip).</summary>
    /// <param name="typeName">Nome del TYPE BASIC dichiarato nella clausola `TYPE <UDT>` di
    /// `CONVERT DBF ... TO ISAM ... TYPE <UDT>` (es. "DbfTestType"). Se null/vuoto, ricade sul
    /// nome del file destinazione (comportamento storico) — ma questo produce un TypeName nello
    /// schema del file che NON corrisponde al TYPE del codice BASIC, facendo fallire
    /// SchemaManager.AreSchemasCompatible al successivo `ISAM OPEN ... OF <UDT>` (il file aperto
    /// resta senza schema compatibile, GET/lettura restituiscono record vuoti) — scoperto dal
    /// vivo su DbIsamFullTests.bas sez. 11.</param>
    /// <param name="fieldOverridesInOrder">Nome REALE + codice tipo ISAM (short-code:
    /// "STRING"/"LONG"/..., stesso formato di IlProgramCompiler.BuildIsamSchemaJson/
    /// IsamTypeCode) di ogni campo del TYPE BASIC di destinazione, nell'ORDINE di
    /// dichiarazione. Quando forniti, sostituiscono nome e tipo tradotti da
    /// <see cref="Engine.TranslateDbfSchemaToIsam"/> per POSIZIONE (i campi DBF/UDT sono
    /// nello stesso ordine, per costruzione: il DBF è stato creato FROM lo stesso TYPE).
    /// Senza: (a) un campo "Codice AS LONG" veniva scritto nello schema ISAM come DoubleType
    /// (TranslateDbfSchemaToIsam semplifica OGNI DBF Numeric a Double a prescindere dal vero
    /// tipo); (b) un campo "Descrizione" (11 caratteri) veniva scritto come "Descrizion"
    /// (DBF tronca i nomi campo a 10 caratteri) — entrambi mai compatibili col TYPE reale al
    /// successivo `ISAM OPEN ... OF <UDT>` (stessa classe di bug del TypeName sbagliato, vedi
    /// <paramref name="typeName"/> — tutti e tre scoperti dal vivo su DbIsamFullTests.bas
    /// sez. 11, uno via l'altro mano a mano che si risolvevano).</param>
    public static void ImportDbf(
        string sourceDbf,
        string destinationIsdb,
        KeyExpressionEvaluatorCallback evaluator,
        string? primaryKeyFieldName = null,
        int primaryKeyLength = 16,
        string? typeName = null,
        IReadOnlyList<(string Name, string TypeCode)>? fieldOverridesInOrder = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceDbf);
        ArgumentException.ThrowIfNullOrWhiteSpace(destinationIsdb);
        ArgumentNullException.ThrowIfNull(evaluator);

        if (!File.Exists(sourceDbf))
        {
            throw new FileNotFoundException("DBF file not found.", sourceDbf);
        }

        if (File.Exists(destinationIsdb))
        {
            throw new InvalidOperationException($"Destination already exists: {destinationIsdb}");
        }

        using var dbfEngine = new DbfEngine(sourceDbf, null, false, LockingStrategy.IntraProcess);
        var dbfFields = dbfEngine.GetFields();
        if (dbfFields.Count == 0)
        {
            throw new InvalidDataException("DBF file has no fields.");
        }

        var pkName = primaryKeyFieldName ?? InferPrimaryKeyField(dbfFields);
        var resolvedTypeName = string.IsNullOrWhiteSpace(typeName) ? Path.GetFileNameWithoutExtension(destinationIsdb) : typeName;
        var schema = Engine.TranslateDbfSchemaToIsam(dbfFields, pkName, primaryKeyLength, resolvedTypeName);
        if (fieldOverridesInOrder is { Count: > 0 })
        {
            var overriddenFields = new List<Config.IsamFieldSchema>(schema.Fields.Count);
            for (var i = 0; i < schema.Fields.Count; i++)
            {
                var f = schema.Fields[i];
                if (i >= fieldOverridesInOrder.Count) { overriddenFields.Add(f); continue; }
                var (name, typeCode) = fieldOverridesInOrder[i];
                var dt = ShortCodeToDataType(typeCode) ?? f.DataType;
                var realName = string.IsNullOrEmpty(name) ? f.Name : name;
                // Se il nome del campo cambia (es. tronco DBF "Descrizion" -> reale
                // "Descrizione") e QUESTO campo era la chiave primaria, aggiorna anche
                // pkName così ConvertFromDbf/PrimaryKeyDef usano il nome corretto.
                if (f.Name.Equals(pkName, StringComparison.OrdinalIgnoreCase) && !realName.Equals(pkName, StringComparison.OrdinalIgnoreCase))
                    pkName = realName;
                overriddenFields.Add(f with { Name = realName, DataType = dt });
            }
            schema = schema with { Fields = overriddenFields };
        }
        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
        Engine.ConvertFromDbf(sourceDbf, destinationIsdb, pkName, primaryKeyLength, udt, evaluator);
    }

    /// <summary>Stesso formato short-code di IlProgramCompiler.IsamTypeCode/
    /// DatabaseService.Commercial.ParseIsamDataType — duplicato qui (non condivisibile:
    /// modules/Database non referenzia common/BRT) perché serve anche lato import/export
    /// offline, non solo al dispatcher schema JSON di ISAM OPEN/SPEED OPEN.</summary>
    private static DataType? ShortCodeToDataType(string code) => code.ToUpperInvariant() switch
    {
        "STRING" => new StringType(),
        "LONG" => new LongType(),
        "INTEGER" => new IntegerType(),
        "DOUBLE" => new DoubleType(),
        "CURRENCY" => new CurrencyType(),
        "BIT" => new BitType(),
        "BYTE" => new ByteType(),
        "SINGLE" => new SingleType(),
        "DATE" => new DateType(),
        "DATETIME" => new DateTimeType(),
        _ => null,
    };

    private static JsonObject IndexToJson(Config.IndexDefinition index) => new()
    {
        ["name"] = index.Name,
        ["keyExpression"] = index.KeyExpression,
        ["keyLength"] = index.KeyLength,
        ["isUnique"] = index.IsUnique,
        ["isUnsafe"] = index.IsUnsafe,
        ["partialPredicate"] = index.PartialPredicate ?? string.Empty,
    };

    private static JsonObject FieldToJson(Config.IsamFieldSchema field) => new()
    {
        ["name"] = field.Name,
        ["type"] = field.DataType.GetType().Name,
        ["length"] = field.Length,
    };

    private static JsonObject RecordToJson(Config.IsamSchema schema, UdtInstance record)
    {
        var obj = new JsonObject();
        foreach (var field in schema.Fields)
        {
            var udtField = record.Definition.Fields.First(f =>
                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
            var value = record.Fields[udtField.SymbolId];
            obj[field.Name] = ValueToJson(value);
        }

        return obj;
    }

    private static JsonNode? ValueToJson(object? value) => value switch
    {
        null => null,
        string s => JsonValue.Create(s),
        bool b => JsonValue.Create(b),
        long l => JsonValue.Create(l),
        int i => JsonValue.Create(i),
        double d => JsonValue.Create(d),
        float f => JsonValue.Create(f),
        decimal m => JsonValue.Create(m),
        _ => JsonValue.Create(value.ToString()),
    };

    private static Config.IsamSchema ParseSchema(JsonNode root)
    {
        var typeName = root["typeName"]?.GetValue<string>()
                       ?? throw new InvalidDataException("JSON export missing typeName.");
        var primaryKey = ParseIndex(root["primaryKey"]
                                    ?? throw new InvalidDataException("JSON export missing primaryKey."));
        var secondaryKeys = root["secondaryKeys"]?.AsArray() ?? [];
        var secondaries = secondaryKeys
            .Select(n => ParseIndex(n ?? throw new InvalidDataException("Null secondary key.")))
            .ToList();
        var fieldsNode = root["fields"]?.AsArray()
                         ?? throw new InvalidDataException("JSON export missing fields.");
        var fields = fieldsNode
            .Select(n => ParseField(n ?? throw new InvalidDataException("Null field.")))
            .ToList();
        return new Config.IsamSchema(typeName, fields, primaryKey, secondaries);
    }

    private static Config.IndexDefinition ParseIndex(JsonNode node) => new(
        node["name"]?.GetValue<string>() ?? throw new InvalidDataException("Index missing name."),
        node["keyExpression"]?.GetValue<string>() ?? string.Empty,
        node["keyLength"]?.GetValue<int>() ?? 0,
        node["isUnique"]?.GetValue<bool>() ?? false,
        node["isUnsafe"]?.GetValue<bool>() ?? false,
        ParseOptionalString(node["partialPredicate"]));

    private static Config.IsamFieldSchema ParseField(JsonNode node)
    {
        var name = node["name"]?.GetValue<string>() ?? throw new InvalidDataException("Field missing name.");
        var typeCode = node["type"]?.GetValue<string>() ?? throw new InvalidDataException("Field missing type.");
        var length = node["length"]?.GetValue<int>() ?? 0;
        return new Config.IsamFieldSchema(name, ParseDataType(typeCode, length), length);
    }

    private static DataType ParseDataType(string typeCode, int length) => typeCode switch
    {
        nameof(StringType) => new StringType(length > 0 ? length : null),
        nameof(JsonType) => new JsonType(),
        nameof(BitType) => new BitType(),
        nameof(ByteType) => new ByteType(),
        nameof(SByteType) => new SByteType(),
        nameof(IntegerType) => new IntegerType(),
        nameof(UIntegerType) => new UIntegerType(),
        nameof(LongType) => new LongType(),
        nameof(ULongType) => new ULongType(),
        nameof(QuadType) => new QuadType(),
        nameof(UQuadType) => new UQuadType(),
        nameof(SingleType) => new SingleType(),
        nameof(DoubleType) => new DoubleType(),
        nameof(CurrencyType) => new CurrencyType(),
        nameof(DateType) => new DateType(),
        nameof(TimeType) => new TimeType(),
        nameof(TimeSpanType) => new TimeSpanType(),
        nameof(DateTimeType) => new DateTimeType(),
        _ => throw new NotSupportedException($"Unsupported field type in JSON export: {typeCode}."),
    };

    private static string? ParseOptionalString(JsonNode? node)
    {
        if (node is null)
        {
            return null;
        }

        var text = node.GetValue<string>();
        return string.IsNullOrWhiteSpace(text) ? null : text;
    }

    private static UdtInstance ParseRecord(Config.IsamSchema schema, UserDefinedType udt, JsonObject row)
    {
        var fields = new Dictionary<int, object?>();
        foreach (var field in schema.Fields)
        {
            var udtField = udt.Fields.First(f =>
                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
            row.TryGetPropertyValue(field.Name, out var jsonValue);
            fields[udtField.SymbolId] = ParseFieldValue(field, jsonValue);
        }

        return new UdtInstance(udt, fields);
    }

    private static object? ParseFieldValue(Config.IsamFieldSchema field, JsonNode? jsonValue)
    {
        if (jsonValue is null)
        {
            return null;
        }

        return field.DataType switch
        {
            LongType => jsonValue.GetValue<long>(),
            IntegerType => jsonValue.GetValue<int>(),
            UIntegerType => (uint)jsonValue.GetValue<long>(),
            ULongType => (ulong)jsonValue.GetValue<long>(),
            StringType => jsonValue.GetValue<string>() ?? string.Empty,
            JsonType => jsonValue.ToJsonString(),
            BitType => jsonValue.GetValue<bool>(),
            ByteType => (byte)jsonValue.GetValue<int>(),
            SByteType => (sbyte)jsonValue.GetValue<int>(),
            SingleType => (float)jsonValue.GetValue<double>(),
            DoubleType => jsonValue.GetValue<double>(),
            CurrencyType => jsonValue.GetValue<decimal>(),
            DateType => jsonValue.GetValue<DateOnly>(),
            TimeType => jsonValue.GetValue<TimeOnly>(),
            DateTimeType => jsonValue.GetValue<DateTime>(),
            TimeSpanType => jsonValue.GetValue<TimeSpan>(),
            _ => jsonValue.GetValue<string>(),
        };
    }

    private static List<DbfField> TranslateIsamSchemaToDbf(Config.IsamSchema schema)
    {
        var fields = new List<DbfField>(schema.Fields.Count);
        foreach (var field in schema.Fields)
        {
            var (type, length, decimals) = MapIsamFieldToDbf(field);
            fields.Add(DbfField.Create(field.Name, type, length, decimals));
        }

        return fields;
    }

    private static (DbfFieldType Type, byte Length, byte Decimals) MapIsamFieldToDbf(Config.IsamFieldSchema field) =>
        field.DataType switch
        {
            StringType => (DbfFieldType.Character, (byte)Math.Clamp(field.Length > 0 ? field.Length : 32, 1, 254), 0),
            LongType or IntegerType => (DbfFieldType.Integer, 4, 0),
            UIntegerType or ULongType => (DbfFieldType.Numeric, 18, 0),
            BitType => (DbfFieldType.Logical, 1, 0),
            DateType => (DbfFieldType.Date, 8, 0),
            DateTimeType => (DbfFieldType.DateTime, 8, 0),
            SingleType => (DbfFieldType.Float, 20, 6),
            DoubleType => (DbfFieldType.Double, 8, 0),
            CurrencyType => (DbfFieldType.Currency, 8, 0),
            JsonType => (DbfFieldType.Character, 254, 0), // ponytail: JSON as memo-less C(254) for export
            _ => (DbfFieldType.Character, (byte)Math.Clamp(field.Length > 0 ? field.Length : 32, 1, 254), 0),
        };

    private static Dictionary<string, object?> RecordToDbfValues(Config.IsamSchema schema, UdtInstance record)
    {
        var values = new Dictionary<string, object?>(schema.Fields.Count, StringComparer.OrdinalIgnoreCase);
        foreach (var field in schema.Fields)
        {
            var udtField = record.Definition.Fields.First(f =>
                f.Name.Lexeme.Equals(field.Name, StringComparison.OrdinalIgnoreCase));
            values[field.Name] = record.Fields[udtField.SymbolId];
        }

        return values;
    }

    private static void WriteDbfRecordPayload(
        IReadOnlyList<DbfField> fields,
        IReadOnlyDictionary<string, object?> values,
        Span<byte> payload,
        Encoding encoding)
    {
        payload.Clear();
        var offset = 0;
        foreach (var field in fields)
        {
            values.TryGetValue(field.Name, out var value);
            DbfFieldWriter.Write(payload.Slice(offset, field.Length), field, value, encoding);
            offset += field.Length;
        }
    }

    private static string InferPrimaryKeyField(IReadOnlyList<DbfField> fields)
    {
        foreach (var field in fields)
        {
            if (field.Name.Equals("Id", StringComparison.OrdinalIgnoreCase))
            {
                return field.Name;
            }
        }

        return fields[0].Name;
    }
}
