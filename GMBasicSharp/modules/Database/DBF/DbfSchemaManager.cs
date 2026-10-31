// FILE: Basic.DBF.DbfSchemaManager.cs

using System.Runtime.Serialization.Formatters.Binary;
using Basic.DBF;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.DBF
{
    /// <summary>
    /// Rappresenta la definizione logica di un singolo campo all'interno dello schema "arricchito" di un DBF.
    /// Contiene il tipo di dato specifico dell'interprete.
    /// </summary>
    [Serializable]
    public sealed record DbfFieldSchema(
        string Name,
        DataType DataType, // Questo fa riferimento a Basic.DataType, che è un tipo comune del linguaggio
        int Length
    )
    {
        public bool Equals(DbfFieldSchema? other)
        {
            if (other is null) return false;
            if (!string.Equals(Name, other.Name, StringComparison.Ordinal)) return false;
            if (Length != other.Length) return false;
            // Confronta DataType per tipo strutturale, ignorando SourceToken
            if (DataType.GetType() != other.DataType.GetType()) return false;
            if (DataType is StringType st && other.DataType is StringType st2)
                return st.FixedLength == st2.FixedLength;
            return true;
        }

        public override int GetHashCode()
        {
            var hc = new HashCode();
            hc.Add(Name, StringComparer.Ordinal);
            hc.Add(Length);
            hc.Add(DataType.GetType());
            if (DataType is StringType st) hc.Add(st.FixedLength);
            return hc.ToHashCode();
        }
    }

    /// <summary>
    /// Rappresenta la definizione logica "arricchita" dello schema di un file DBF,
    /// come memorizzata nel file satellite .gms. Contiene informazioni sui tipi di dato
    /// specifici dell'interprete che non possono essere dedotte dal solo header .dbf.
    /// </summary>
    public sealed record DbfSchemaInfo(
        // Il nome del TYPE del linguaggio BASIC associato a questo schema.
        string TypeName,
        // La lista dei campi, con i tipi di dato specifici dell'interprete.
        List<DbfFieldSchema> Fields
    )
    {
        public bool Equals(DbfSchemaInfo? other)
        {
            if (other is null) return false;
            if (!string.Equals(TypeName, other.TypeName, StringComparison.Ordinal)) return false;
            if (Fields.Count != other.Fields.Count) return false;
            for (var i = 0; i < Fields.Count; i++)
            {
                if (!Fields[i].Equals(other.Fields[i])) return false;
            }
            return true;
        }

        public override int GetHashCode()
        {
            var hc = new HashCode();
            hc.Add(TypeName, StringComparer.Ordinal);
            foreach (var f in Fields) hc.Add(f);
            return hc.ToHashCode();
        }
    }

    /// <summary>
    /// Fornisce metodi statici per gestire la validazione dello schema di un file DBF,
    /// utilizzando un file satellite .gms per una validazione forte quando disponibile.
    /// </summary>
    internal static class DbfSchemaManager
    {
        /// <summary>
        /// Ottiene il percorso del file di schema satellite (.gms) per un dato file DBF.
        /// </summary>
        internal static string GetSchemaSidecarPath(string dbfFilePath) =>
            Path.ChangeExtension(dbfFilePath, ".gms");

        private static string GetSchemaFilePath(string dbfFilePath) => GetSchemaSidecarPath(dbfFilePath);

        /// <summary>
        /// Scrive le informazioni di schema arricchite in un file satellite .gms.
        /// </summary>
        /// <param name="dbfFilePath">Il percorso del file DBF a cui associare lo schema.</param>
        /// <param name="schemaInfo">L'oggetto DbfSchemaInfo da serializzare e scrivere.</param>
        public static void WriteSchemaInfo(string dbfFilePath, DbfSchemaInfo schemaInfo)
        {
            var schemaFilePath = GetSchemaFilePath(dbfFilePath);
            byte[] bytes;
            try
            {
                bytes = SerializeWithBinaryFormatter(schemaInfo);
            }
            catch (PlatformNotSupportedException)
            {
                // ponytail: .NET 9+ blocks BinaryFormatter — GMS1 is opaque bytes for replication too.
                bytes = SerializeSchemaInfoGms1(schemaInfo);
            }

            File.WriteAllBytes(schemaFilePath, bytes);
        }

        /// <summary>Reads raw .gms bytes when present (null if missing or BinaryFormatter blocked).</summary>
        internal static byte[]? ReadSchemaBytes(string dbfFilePath)
        {
            var schemaFilePath = GetSchemaFilePath(dbfFilePath);
            if (!File.Exists(schemaFilePath))
            {
                return null;
            }

            return File.ReadAllBytes(schemaFilePath);
        }

        /// <summary>Writes raw .gms bytes for replication apply (ponytail: opaque copy).</summary>
        internal static void WriteSchemaBytes(string dbfFilePath, byte[] bytes)
        {
            ArgumentNullException.ThrowIfNull(bytes);
            File.WriteAllBytes(GetSchemaFilePath(dbfFilePath), bytes);
        }

        private static void TryDeleteSchemaFile(string schemaFilePath)
        {
            try
            {
                if (File.Exists(schemaFilePath))
                {
                    File.Delete(schemaFilePath);
                }
            }
            catch
            {
                /* best-effort */
            }
        }

        private static byte[] SerializeWithBinaryFormatter(DbfSchemaInfo schemaInfo)
        {
#pragma warning disable SYSLIB0011
            using var stream = new MemoryStream();
            var formatter = new BinaryFormatter();
            formatter.Serialize(stream, schemaInfo);
            return stream.ToArray();
#pragma warning restore SYSLIB0011
        }

        private static bool IsGms1Payload(byte[] bytes) =>
            bytes.Length >= 4 &&
            bytes[0] == (byte)'G' &&
            bytes[1] == (byte)'M' &&
            bytes[2] == (byte)'S' &&
            bytes[3] == (byte)'1';

        private static byte[] SerializeSchemaInfoGms1(DbfSchemaInfo schemaInfo)
        {
            using var stream = new MemoryStream();
            using var writer = new BinaryWriter(stream, System.Text.Encoding.UTF8, leaveOpen: true);
            writer.Write((byte)'G');
            writer.Write((byte)'M');
            writer.Write((byte)'S');
            writer.Write((byte)'1');
            writer.Write(schemaInfo.TypeName);
            writer.Write(schemaInfo.Fields.Count);
            foreach (var field in schemaInfo.Fields)
            {
                writer.Write(field.Name);
                writer.Write(field.Length);
                writer.Write(MapDataTypeTag(field.DataType));
                var stringLength = field.DataType is StringType st ? st.FixedLength ?? field.Length : field.Length;
                writer.Write(stringLength);
            }

            return stream.ToArray();
        }

        private static DbfSchemaInfo DeserializeSchemaInfoGms1(byte[] bytes)
        {
            using var stream = new MemoryStream(bytes);
            using var reader = new BinaryReader(stream, System.Text.Encoding.UTF8, leaveOpen: true);
            if (reader.ReadByte() != (byte)'G' ||
                reader.ReadByte() != (byte)'M' ||
                reader.ReadByte() != (byte)'S' ||
                reader.ReadByte() != (byte)'1')
            {
                throw new InvalidDataException("Invalid GMS1 schema payload.");
            }

            var typeName = reader.ReadString();
            var fieldCount = reader.ReadInt32();
            var fields = new List<DbfFieldSchema>(fieldCount);
            for (var i = 0; i < fieldCount; i++)
            {
                var name = reader.ReadString();
                var length = reader.ReadInt32();
                var typeTag = reader.ReadByte();
                var typeLength = reader.ReadInt32();
                fields.Add(new DbfFieldSchema(name, MapDataTypeFromTag(typeTag, typeLength, length), length));
            }

            return new DbfSchemaInfo(typeName, fields);
        }

        private static byte MapDataTypeTag(DataType dataType) =>
            dataType switch
            {
                StringType => 1,
                IntegerType => 2,
                LongType => 3,
                DoubleType => 4,
                DateType => 5,
                DateTimeType => 6,
                BitType => 7,
                CurrencyType => 8,
                _ => throw new NotSupportedException($"GMS1 schema cannot encode type '{dataType.GetType().Name}'."),
            };

        private static DataType MapDataTypeFromTag(byte tag, int typeLength, int fieldLength) =>
            tag switch
            {
                1 => new StringType(typeLength > 0 ? typeLength : fieldLength),
                2 => new IntegerType(),
                3 => new LongType(),
                4 => new DoubleType(),
                5 => new DateType(),
                6 => new DateTimeType(),
                7 => new BitType(),
                8 => new CurrencyType(),
                _ => throw new NotSupportedException($"GMS1 schema type tag {tag} is not supported."),
            };

        /// <summary>
        /// Verifica la compatibilità tra un tipo di campo fisico DBF e un tipo di dato logico dell'interprete.
        /// </summary>
        private static bool AreTypesCompatible(DbfField dbfField, DataType codeType, bool isStrict)
        {
            switch (dbfField.Type)
            {
                case DbfFieldType.Character:
                    return codeType is StringType;
                case DbfFieldType.Date:
                    return codeType is DateType;
                case DbfFieldType.DateTime:
                    return codeType is DateTimeType;
                case DbfFieldType.Logical:
                    return codeType is BitType;

                case DbfFieldType.Integer:
                    return codeType is IntegerType or LongType or UIntegerType or ULongType or QuadType or UQuadType;

                case DbfFieldType.Double:
                    return codeType is DoubleType or SingleType;

                case DbfFieldType.Currency:
                    return codeType is CurrencyType or DoubleType or SingleType;

                case DbfFieldType.Memo:
                case DbfFieldType.General:
                    return codeType is StringType;

                case DbfFieldType.Numeric:
                case DbfFieldType.Float: // Trattiamo Numeric e Float in modo simile
                    if (isStrict)
                    {
                        // Mappatura stretta 1-a-1
                        if (dbfField.DecimalCount > 0)
                        {
                            // Se ha decimali, può essere Double, Single o Currency
                            return codeType is DoubleType or SingleType or CurrencyType;
                        }
                        else
                        {
                            // Se è intero, mappiamo in base alla lunghezza
                            return dbfField.Length switch
                            {
                                <= 5 => codeType is IntegerType, // short
                                <= 10 => codeType is LongType, // int
                                _ => codeType is QuadType // long
                            };
                        }
                    }
                    else
                    {
                        // Mappatura flessibile: qualsiasi tipo numerico va bene.
                        return codeType is IntegerType or LongType or QuadType or
                            SingleType or DoubleType or CurrencyType;
                    }

                default:
                    return false; // Tipi non mappati (es. Memo, General) non sono validati a questo livello.
            }
        }

        /// <summary>
        /// Legge e deserializza le informazioni di schema da un file satellite .gms.
        /// </summary>
        /// <param name="dbfFilePath">Il percorso del file DBF principale.</param>
        /// <returns>L'oggetto DbfSchemaInfo deserializzato, o null se il file .gms non esiste.</returns>
        public static DbfSchemaInfo? ReadSchemaInfo(string dbfFilePath)
        {
            var schemaFilePath = GetSchemaFilePath(dbfFilePath);
            if (!File.Exists(schemaFilePath))
            {
                return null;
            }

            var bytes = File.ReadAllBytes(schemaFilePath);
            if (IsGms1Payload(bytes))
            {
                return DeserializeSchemaInfoGms1(bytes);
            }

            try
            {
#pragma warning disable SYSLIB0011
                using var stream = new MemoryStream(bytes);
                var formatter = new BinaryFormatter();
                return (DbfSchemaInfo)formatter.Deserialize(stream);
#pragma warning restore SYSLIB0011
            }
            catch (PlatformNotSupportedException)
            {
                return null;
            }
        }

        /// <summary>
        /// Esegue la validazione dello schema di un file DBF contro la definizione di un TYPE del codice.
        /// </summary>
        /// <param name="dbfFile">L'handle del file DBF a basso livello, già aperto.</param>
        /// <param name="codeSchema">Lo schema "arricchito" derivato dal TYPE nel codice.</param>
        /// <param name="isSchemaStrict">Flag che indica se eseguire una validazione stretta per i file legacy.</param>
        public static void Validate(DbfFile dbfFile, DbfSchemaInfo codeSchema, bool isSchemaStrict)
        {
            Validate(dbfFile, codeSchema, fieldMappings: null, isSchemaStrict);
        }

        /// <summary>
        /// Validates schema compatibility when TYPE field names differ from physical DBF columns.
        /// <paramref name="fieldMappings"/> maps TYPE field name to DBF column name.
        /// </summary>
        public static void Validate(
            DbfFile dbfFile,
            DbfSchemaInfo codeSchema,
            IReadOnlyDictionary<string, string>? fieldMappings,
            bool isSchemaStrict)
        {
            if (fieldMappings is { Count: > 0 })
            {
                ValidateWithFieldMapping(dbfFile, codeSchema, fieldMappings, isSchemaStrict);
                return;
            }

            ValidateCore(dbfFile, codeSchema, isSchemaStrict);
        }

        private static void ValidateCore(DbfFile dbfFile, DbfSchemaInfo codeSchema, bool isSchemaStrict)
        {
            var gmsSchema = ReadSchemaInfo(dbfFile.FilePath);

            if (gmsSchema != null)
            {
                // -- SCENARIO A: FILE .GMS ESISTE (VALIDAZIONE FORTE) --

                // 1. Controlla che lo schema del .gms corrisponda a quello del codice.
                if (!gmsSchema.Equals(codeSchema))
                    throw new InvalidOperationException(
                        $"La struttura del TYPE '{codeSchema.TypeName}' nel codice non corrisponde a quella memorizzata nel file di schema '{GetSchemaFilePath(dbfFile.FilePath)}'.");

                // 2. Controlla che l'header del .dbf non sia stato manomesso.
                if (!IsHeaderCompatibleWithSchema(dbfFile.Header, dbfFile.Fields, gmsSchema))
                    throw new InvalidOperationException(
                        $"Il file '{dbfFile.FilePath}' è stato modificato da un programma esterno. La sua struttura non è più coerente con lo schema '{GetSchemaFilePath(dbfFile.FilePath)}'.");
            }
            else
            {
                // -- SCENARIO B: FILE .GMS NON ESISTE (FILE LEGACY) --

                if (isSchemaStrict)
                {
                    // Validazione Stretta
                    if (!IsHeaderCompatibleWithSchema(dbfFile.Header, dbfFile.Fields, codeSchema, strict: true))
                        throw new InvalidOperationException(
                            $"La struttura del file legacy '{dbfFile.FilePath}' non corrisponde esattamente alla definizione del TYPE '{codeSchema.TypeName}' (modalità STRICT).");
                }
                else
                {
                    // Validazione Flessibile
                    if (!IsHeaderCompatibleWithSchema(dbfFile.Header, dbfFile.Fields, codeSchema, strict: false))
                        throw new InvalidOperationException(
                            $"La struttura del file legacy '{dbfFile.FilePath}' non è compatibile con la definizione del TYPE '{codeSchema.TypeName}'.");
                }
            }
        }

        /// <summary>
        /// Metodo helper per confrontare la struttura fisica di un DBF (letta dall'header)
        /// con uno schema "arricchito" (dal codice o da un file .gms).
        /// </summary>
        /// <param name="header">L'header del file .dbf.</param>
        /// <param name="dbfFields">La lista dei campi letta dal file .dbf.</param>
        /// <param name="schema">Lo schema "arricchito" di riferimento.</param>
        /// <param name="strict">Se true, esegue una validazione dei tipi 1-a-1 esatta. Se false, esegue una validazione flessibile.</param>
        /// <returns>True se la struttura è compatibile, altrimenti False.</returns>
        private static bool IsHeaderCompatibleWithSchema(DbfHeader header, IReadOnlyList<DbfField> dbfFields, DbfSchemaInfo schema, bool strict = false)
        {
            if (dbfFields.Count != schema.Fields.Count) return false;

            for (var i = 0; i < dbfFields.Count; i++)
            {
                var dbfField = dbfFields[i];
                var schemaField = schema.Fields[i];

                // DBF field names are limited to 10 characters; truncate schema name for comparison.
                var schemaNameTruncated = schemaField.Name.Length > 10
                    ? schemaField.Name[..10]
                    : schemaField.Name;
                if (!dbfField.Name.Equals(schemaNameTruncated, StringComparison.OrdinalIgnoreCase))
                    return false;
                
                // Per i tipi stringa, la lunghezza deve sempre corrispondere.
                if (schemaField.DataType is StringType && dbfField.Length != schemaField.Length)
                    return false;

                if (!AreTypesCompatible(dbfField, schemaField.DataType, strict))
                    return false;
            }
            return true;
        }

        private static void ValidateWithFieldMapping(
            DbfFile dbfFile,
            DbfSchemaInfo codeSchema,
            IReadOnlyDictionary<string, string> fieldMappings,
            bool isSchemaStrict)
        {
            var gmsSchema = ReadSchemaInfo(dbfFile.FilePath);
            if (gmsSchema != null)
            {
                throw new InvalidOperationException(
                    $"MAP field mapping is not supported when a .gms schema file exists for '{dbfFile.FilePath}'.");
            }

            var dbfFields = dbfFile.Fields;
            foreach (var schemaField in codeSchema.Fields)
            {
                var dbfName = fieldMappings.GetValueOrDefault(schemaField.Name, schemaField.Name);
                var dbfField = dbfFields.FirstOrDefault(f =>
                    f.Name.Equals(dbfName, StringComparison.OrdinalIgnoreCase));
                if (dbfField.Equals(default(DbfField)))
                {
                    throw new InvalidOperationException(
                        $"DBF field '{dbfName}' mapped from TYPE field '{schemaField.Name}' was not found in '{dbfFile.FilePath}'.");
                }

                if (schemaField.DataType is StringType && dbfField.Length != schemaField.Length)
                {
                    throw new InvalidOperationException(
                        $"Length mismatch for mapped field '{dbfName}' (TYPE '{schemaField.Name}').");
                }

                if (!AreTypesCompatible(dbfField, schemaField.DataType, isSchemaStrict))
                {
                    throw new InvalidOperationException(
                        $"Type mismatch for mapped field '{dbfName}' (TYPE '{schemaField.Name}').");
                }
            }
        }

    }
}