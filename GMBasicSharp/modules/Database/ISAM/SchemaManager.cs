// FILE: Basic.ISAM.SchemaManager.cs

using System.Text;

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace Basic.ISAM;

/// <summary>
/// Fornisce metodi statici per gestire la definizione, serializzazione, validazione e interpretazione
/// dello schema logico di un file ISAM.
/// </summary>
public static class SchemaManager
{
    private static readonly byte[] SchemaMagic = "ISCH"u8.ToArray();
    private const int SchemaFormatVersion = 2;
    private const int SchemaFormatVersionLegacy = 1;
    /// <summary>
    /// Calcola la dimensione fissa in byte di un singolo record dati, basandosi sullo schema fornito.
    /// La dimensione totale include lo spazio per i metadati del record (Stato e Versione).
    /// </summary>
    /// <param name="schema">Lo schema del record di cui calcolare la dimensione.</param>
    /// <returns>La dimensione totale del record in byte.</returns>
    public static int CalculateRecordSize(Config.IsamSchema schema)
    {
        var metadataSize = System.Runtime.InteropServices.Marshal.SizeOf<RecordMetadata>();
        return metadataSize + schema.Fields.Sum(field => GetFieldSize(field.DataType, field.Length));
    }

    /// <summary>
    /// Serializza una mappa di nomi di indici e offset delle loro radici in un array di byte.
    /// </summary>
    /// <param name="map">Il dizionario da serializzare.</param>
    /// <returns>La rappresentazione binaria della mappa.</returns>
    public static byte[] SerializeIndexMap(Dictionary<string, long> map)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write(map.Count);
        foreach (var kvp in map)
        {
            writer.Write(kvp.Key);
            writer.Write(kvp.Value);
        }

        return stream.ToArray();
    }

    /// <summary>
    /// Deserializza un array di byte in una mappa di nomi di indici e offset delle loro radici.
    /// </summary>
    /// <param name="buffer">Il buffer contenente la mappa serializzata.</param>
    /// <returns>Il dizionario ricostruito.</returns>
    public static Dictionary<string, long> DeserializeIndexMap(byte[] buffer)
    {
        var map = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        using var stream = new MemoryStream(buffer);
        using var reader = new BinaryReader(stream);
        var count = reader.ReadInt32();
        for (var i = 0; i < count; i++)
        {
            map[reader.ReadString()] = reader.ReadInt64();
        }

        return map;
    }

    /// <summary>
    /// Serializza un oggetto <see cref="Config.IsamSchema"/> in un array di byte per la persistenza su disco.
    /// </summary>
    /// <param name="schema">L'oggetto schema da serializzare.</param>
    /// <returns>La rappresentazione binaria dello schema.</returns>
    public static byte[] SerializeSchema(Config.IsamSchema schema)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
        writer.Write(SchemaMagic);
        writer.Write(SchemaFormatVersion);
        writer.Write(schema.TypeName);
        WriteIndexDefinition(writer, schema.PrimaryKey);
        writer.Write(schema.SecondaryKeys.Count);
        foreach (var secondary in schema.SecondaryKeys)
        {
            WriteIndexDefinition(writer, secondary);
        }

        writer.Write(schema.Fields.Count);
        foreach (var field in schema.Fields)
        {
            WriteFieldSchema(writer, field);
        }

        return stream.ToArray();
    }

    /// <summary>
    /// Deserializza un array di byte in un oggetto <see cref="Config.IsamSchema"/>.
    /// </summary>
    /// <param name="buffer">Il buffer contenente lo schema serializzato.</param>
    /// <returns>L'oggetto <see cref="Config.IsamSchema"/> ricostruito.</returns>
    public static Config.IsamSchema DeserializeSchema(byte[] buffer)
    {
        using var stream = new MemoryStream(buffer);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);

        var magic = reader.ReadBytes(SchemaMagic.Length);
        if (!magic.AsSpan().SequenceEqual(SchemaMagic))
        {
            throw new InvalidDataException("Blocco schema ISAM non valido o formato legacy non supportato.");
        }

        var version = reader.ReadInt32();
        if (version is not (SchemaFormatVersion or SchemaFormatVersionLegacy))
        {
            throw new InvalidDataException($"Versione schema ISAM non supportata: {version}.");
        }

        var typeName = reader.ReadString();
        var primaryKey = ReadIndexDefinition(reader, version);
        var secondaryCount = reader.ReadInt32();
        var secondaryKeys = new List<Config.IndexDefinition>(secondaryCount);
        for (var i = 0; i < secondaryCount; i++)
        {
            secondaryKeys.Add(ReadIndexDefinition(reader, version));
        }

        var fieldCount = reader.ReadInt32();
        var fields = new List<Config.IsamFieldSchema>(fieldCount);
        for (var i = 0; i < fieldCount; i++)
        {
            fields.Add(ReadFieldSchema(reader));
        }

        return new Config.IsamSchema(typeName, fields, primaryKey, secondaryKeys);
    }

    private static void WriteIndexDefinition(BinaryWriter writer, Config.IndexDefinition definition)
    {
        writer.Write(definition.Name);
        writer.Write(definition.KeyExpression);
        writer.Write(definition.KeyLength);
        writer.Write(definition.IsUnique);
        writer.Write(definition.IsUnsafe);
        writer.Write(definition.PartialPredicate ?? string.Empty);
    }

    private static Config.IndexDefinition ReadIndexDefinition(BinaryReader reader, int schemaVersion)
    {
        var name = reader.ReadString();
        var keyExpression = reader.ReadString();
        var keyLength = reader.ReadInt32();
        var isUnique = reader.ReadBoolean();
        var isUnsafe = reader.ReadBoolean();
        string? partialPredicate = null;
        if (schemaVersion >= SchemaFormatVersion)
        {
            var raw = reader.ReadString();
            partialPredicate = string.IsNullOrWhiteSpace(raw) ? null : raw;
        }

        return new Config.IndexDefinition(name, keyExpression, keyLength, isUnique, isUnsafe, partialPredicate);
    }

    private static void WriteFieldSchema(BinaryWriter writer, Config.IsamFieldSchema field)
    {
        writer.Write(field.Name);
        writer.Write(GetDataTypeCode(field.DataType));
        writer.Write(field.Length);
    }

    private static Config.IsamFieldSchema ReadFieldSchema(BinaryReader reader)
    {
        var name = reader.ReadString();
        var typeCode = reader.ReadString();
        var length = reader.ReadInt32();
        var dataType = CreateDataType(typeCode, length);
        return new Config.IsamFieldSchema(name, dataType, length);
    }

    private static string GetDataTypeCode(DataType dataType) => dataType switch
    {
        StringType => nameof(StringType),
        JsonType => nameof(JsonType),
        BitType => nameof(BitType),
        ByteType => nameof(ByteType),
        SByteType => nameof(SByteType),
        IntegerType => nameof(IntegerType),
        UIntegerType => nameof(UIntegerType),
        LongType => nameof(LongType),
        ULongType => nameof(ULongType),
        QuadType => nameof(QuadType),
        UQuadType => nameof(UQuadType),
        SingleType => nameof(SingleType),
        DoubleType => nameof(DoubleType),
        CurrencyType => nameof(CurrencyType),
        DateType => nameof(DateType),
        TimeType => nameof(TimeType),
        TimeSpanType => nameof(TimeSpanType),
        DateTimeType => nameof(DateTimeType),
        _ => throw new NotSupportedException($"Tipo di dato '{dataType.GetType().Name}' non serializzabile nello schema ISAM.")
    };

    private static DataType CreateDataType(string typeCode, int length) => typeCode switch
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
        _ => throw new NotSupportedException($"Codice tipo schema ISAM sconosciuto: '{typeCode}'.")
    };

    /// <summary>
    /// Valida la compatibilità strutturale tra lo schema definito nel codice e quello letto dal file.
    /// La compatibilità richiede una corrispondenza esatta di nome del tipo, numero di campi, e per ogni
    /// campo: nome, tipo e lunghezza. Vengono verificate anche le definizioni degli indici.
    /// </summary>
    /// <param name="codeSchema">Lo schema derivato dal `TYPE` nel codice BASIC.</param>
    /// <param name="fileSchema">Lo schema letto dall'header del file .isdb.</param>
    /// <returns>True se gli schemi sono compatibili, altrimenti False.</returns>
    public static bool AreSchemasCompatible(Config.IsamSchema codeSchema, Config.IsamSchema fileSchema)
    {
        if (!codeSchema.TypeName.Equals(fileSchema.TypeName, StringComparison.OrdinalIgnoreCase))
        {
            Console.Error.WriteLine($"[SCHEMA] TypeName mismatch: '{codeSchema.TypeName}' vs '{fileSchema.TypeName}'");
            return false;
        }

        if (codeSchema.Fields.Count != fileSchema.Fields.Count)
        {
            Console.Error.WriteLine($"[SCHEMA] Field count: {codeSchema.Fields.Count} vs {fileSchema.Fields.Count}");
            return false;
        }

        for (var i = 0; i < codeSchema.Fields.Count; i++)
        {
            var codeField = codeSchema.Fields[i];
            var fileField = fileSchema.Fields[i];

            if (!codeField.Name.Equals(fileField.Name, StringComparison.OrdinalIgnoreCase))
            {
                Console.Error.WriteLine($"[SCHEMA] Field[{i}] name: '{codeField.Name}' vs '{fileField.Name}'");
                return false;
            }
            if (codeField.DataType.GetType() != fileField.DataType.GetType())
            {
                Console.Error.WriteLine($"[SCHEMA] Field[{i}] type: {codeField.DataType.GetType().Name} vs {fileField.DataType.GetType().Name}");
                return false;
            }
            if (codeField.Length != fileField.Length)
            {
                Console.Error.WriteLine($"[SCHEMA] Field[{i}] length: {codeField.Length} vs {fileField.Length}");
                return false;
            }
        }

        // KeyLength della chiave primaria confrontato con TOLLERANZA: `ISAM OPEN ... INDEXED
        // BY <campo>` non ha modo di esprimere una lunghezza chiave esplicita (solo il NOME
        // del campo, vedi IlProgramCompiler.BuildIsamSchemaJson/Compiler.Isam.cs
        // VisitIsamUseStatement) — per un campo NON-stringa il lato codice ricade sempre su un
        // default fisso (20), mentre `ISAM CONVERT DBF ... KEY <pk> LEN <n>` permette di
        // scegliere una lunghezza diversa (es. LEN 8) per il file appena creato. Un file
        // convertito con LEN != 20 falliva quindi SEMPRE questo confronto (con TypeName/nomi/
        // tipi campo già corretti — vedi IlRuntime.IsamConvertDbf), anche se la lunghezza
        // chiave non influisce sulla LETTURA dei record (byte-layout dei dati, invariato) — al
        // più sul SEEK indicizzato. Qui si confronta tutto TRANNE KeyLength; una differenza
        // viene solo loggata (scoperto dal vivo su DbIsamFullTests.bas sez. 11, ultimo di
        // quattro mismatch a cascata sullo stesso CONVERT+OPEN).
        var pkCompatibleIgnoringLength =
            codeSchema.PrimaryKey.Name.Equals(fileSchema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase) &&
            codeSchema.PrimaryKey.KeyExpression.Equals(fileSchema.PrimaryKey.KeyExpression, StringComparison.OrdinalIgnoreCase) &&
            codeSchema.PrimaryKey.IsUnique == fileSchema.PrimaryKey.IsUnique &&
            codeSchema.PrimaryKey.IsUnsafe == fileSchema.PrimaryKey.IsUnsafe &&
            codeSchema.PrimaryKey.PartialPredicate == fileSchema.PrimaryKey.PartialPredicate;
        if (!pkCompatibleIgnoringLength)
        {
            Console.Error.WriteLine($"[SCHEMA] PK mismatch: {codeSchema.PrimaryKey} vs {fileSchema.PrimaryKey}");
            return false;
        }
        if (codeSchema.PrimaryKey.KeyLength != fileSchema.PrimaryKey.KeyLength)
        {
            Console.Error.WriteLine($"[SCHEMA] PK KeyLength differs (tollerato): {codeSchema.PrimaryKey.KeyLength} vs {fileSchema.PrimaryKey.KeyLength}");
        }
        // Stessa tolleranza di KeyLength applicata sopra alla chiave primaria: confronta gli
        // indici secondari ignorando KeyLength (vedi commento sopra sul PK per il motivo).
        if (codeSchema.SecondaryKeys.Count != fileSchema.SecondaryKeys.Count)
        {
            Console.Error.WriteLine($"[SCHEMA] SecondaryKeys count: {codeSchema.SecondaryKeys.Count} vs {fileSchema.SecondaryKeys.Count}");
            return false;
        }
        for (var i = 0; i < codeSchema.SecondaryKeys.Count; i++)
        {
            var codeKey = codeSchema.SecondaryKeys[i];
            var fileKey = fileSchema.SecondaryKeys[i];
            var secondaryCompatibleIgnoringLength =
                codeKey.Name.Equals(fileKey.Name, StringComparison.OrdinalIgnoreCase) &&
                codeKey.KeyExpression.Equals(fileKey.KeyExpression, StringComparison.OrdinalIgnoreCase) &&
                codeKey.IsUnique == fileKey.IsUnique &&
                codeKey.IsUnsafe == fileKey.IsUnsafe &&
                codeKey.PartialPredicate == fileKey.PartialPredicate;
            if (!secondaryCompatibleIgnoringLength)
            {
                Console.Error.WriteLine($"[SCHEMA] SecondaryKeys[{i}] mismatch: {codeKey} vs {fileKey}");
                return false;
            }
            if (codeKey.KeyLength != fileKey.KeyLength)
            {
                Console.Error.WriteLine($"[SCHEMA] SecondaryKeys[{i}] KeyLength differs (tollerato): {codeKey.KeyLength} vs {fileKey.KeyLength}");
            }
        }

        return true;
    }

    /// <summary>
    /// Serializza un valore di chiave (già calcolato) nella sua rappresentazione binaria.
    /// Il valore viene convertito in una stringa maiuscola, poi in byte, e infine
    /// troncato o riempito (padding con spazi) per adattarlo alla lunghezza fissa definita dall'indice.
    /// </summary>
    /// <param name="value">Il valore della chiave da serializzare (es. una stringa o un numero).</param>
    /// <param name="indexDef">La definizione dell'indice, usata per ottenere la lunghezza della chiave.</param>
    /// <returns>La rappresentazione binaria della chiave, normalizzata alla lunghezza corretta.</returns>
    public static byte[] SerializeKeyValue(object? value, Config.IndexDefinition indexDef)
    {
        var strValue = FormatKeyString(value, indexDef.KeyLength);
        var bytes = Encoding.UTF8.GetBytes(strValue);

        var keyBuffer = new byte[indexDef.KeyLength];
        Array.Fill(keyBuffer, (byte)' ');
        Buffer.BlockCopy(bytes, 0, keyBuffer, 0, Math.Min(bytes.Length, keyBuffer.Length));

        return keyBuffer;
    }

    private static string FormatKeyString(object? value, int keyLength)
    {
        if (value is null)
        {
            return string.Empty;
        }

        // ponytail: zero-pad numerici così l'ordinamento lessicografico byte[] == ordinamento numerico
        return value switch
        {
            byte or sbyte or short or ushort or int or uint or long or ulong
                => Convert.ToInt64(value, System.Globalization.CultureInfo.InvariantCulture)
                    .ToString(System.Globalization.CultureInfo.InvariantCulture)
                    .PadLeft(keyLength, '0'),
            float or double or decimal
                => Convert.ToDouble(value, System.Globalization.CultureInfo.InvariantCulture)
                    .ToString("G17", System.Globalization.CultureInfo.InvariantCulture)
                    .PadLeft(keyLength, '0'),
            _ => (value.ToString() ?? string.Empty).ToUpperInvariant(),
        };
    }

    /// <summary>
    /// Deserializza una mappa di nomi di indici e offset delle loro radici direttamente da uno stream.
    /// </summary>
    /// <param name="stream">Lo stream da cui leggere i dati, posizionato all'inizio della mappa.</param>
    /// <returns>Il dizionario ricostruito.</returns>
    public static Dictionary<string, long> DeserializeIndexMapFromStream(Stream stream)
    {
        var map = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
        var count = reader.ReadInt32();
        for (var i = 0; i < count; i++)
        {
            map[reader.ReadString()] = reader.ReadInt64();
        }

        return map;
    }

    /// <summary>
    /// Helper privato per ottenere la dimensione fissa in byte di un singolo tipo di dato.
    /// </summary>
    /// <param name="dataType">Il tipo di dato da misurare.</param>
    /// <param name="length">La lunghezza specifica per i tipi a lunghezza variabile (es. String).</param>
    /// <returns>La dimensione in byte.</returns>
    private static int GetFieldSize(DataType dataType, int length) => dataType switch
    {
        StringType => length,
        JsonType => sizeof(long),
        BitType => sizeof(bool),
        ByteType => sizeof(byte),
        SByteType => sizeof(sbyte),
        IntegerType => sizeof(short),
        UIntegerType => sizeof(ushort),
        LongType => sizeof(int),
        ULongType => sizeof(uint),
        QuadType => sizeof(long),
        UQuadType => sizeof(ulong),
        SingleType => sizeof(float),
        DoubleType => sizeof(double),
        CurrencyType => sizeof(decimal),
        DateType => sizeof(long),
        TimeType => sizeof(long),
        TimeSpanType => sizeof(long),
        DateTimeType => sizeof(long),
        _ => throw new NotSupportedException($"Tipo di dato '{dataType.GetType().Name}' non supportato nei file ISAM.")
    };

    internal static int GetFieldStorageSize(DataType dataType, int length) => GetFieldSize(dataType, length);
}