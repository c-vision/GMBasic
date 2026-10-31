/* FILE: native/gmb_db_c/src/ISAM/RecordMarshaller.c */
/* Port 1:1 di modules/Database/ISAM/RecordMarshaller.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.RecordMarshaller.cs
//
//using System.Buffers;
//using System.Text;
//using System.Text.Json.Nodes;
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//// ReSharper disable RedundantNameQualifier
//
//namespace Basic.ISAM;
//
//internal static class RecordMarshaller
//{
//    /// <summary>
//    /// Serializza una istanza di UDT in un buffer di byte, utilizzando una struttura di metadati predefinita.
//    /// Questo metodo è il punto di ingresso principale per la serializzazione di un record completo.
//    /// </summary>
//    /// <param name="schema">Lo schema che definisce la struttura del record.</param>
//    /// <param name="udtInstance">L'istanza UDT contenente i dati da scrivere.</param>
//    /// <param name="metadata">La struttura di metadati completa (stato, versione, txId, etc.) da scrivere.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per operazioni sul LOB Heap (es. JSON)
//    /// e per ottenere l'encoding del file.</param>
//    /// <returns>Un array di byte che rappresenta il record completo (metadati + dati).</returns>
//    /// <exception cref="InvalidDataException">Se la dimensione del record serializzato non corrisponde allo schema.</exception>
//    public static byte[] SerializeRecordWithMetadata(Config.IsamSchema schema, UdtInstance udtInstance,
//        RecordMetadata metadata, IsamFileHandle fileHandle)
//    {
//        var recordSize = SchemaManager.CalculateRecordSize(schema);
//        var buffer = new byte[recordSize];
//        SerializeRecordWithMetadataToBuffer(schema, udtInstance, metadata, fileHandle, buffer);
//        return buffer;
//    }
//
//    /// <summary>
//    /// Serializza un record completo in un buffer pre-allocato (zero ToArray intermedio).
//    /// </summary>
//    internal static void SerializeRecordWithMetadataToBuffer(
//        Config.IsamSchema schema,
//        UdtInstance udtInstance,
//        RecordMetadata metadata,
//        IsamFileHandle fileHandle,
//        byte[] buffer,
//        int offset = 0)
//    {
//        var recordSize = SchemaManager.CalculateRecordSize(schema);
//        if (buffer.Length - offset < recordSize)
//        {
//            throw new ArgumentException(
//                $"Buffer record troppo piccolo: {buffer.Length - offset} < {recordSize}.", nameof(buffer));
//        }
//
//        using var stream = new MemoryStream(buffer, offset, recordSize, writable: true, publiclyVisible: true);
//        stream.SetLength(0);
//        MarshalHelper.WriteStructure(stream, metadata);
//        using var writer = new BinaryWriter(stream, fileHandle.FileEncoding, leaveOpen: true);
//        WriteUdtData(writer, schema, udtInstance, fileHandle);
//
//        if (stream.Position != recordSize)
//        {
//            throw new InvalidDataException("La dimensione del record serializzato non corrisponde allo schema.");
//        }
//    }
//
//    /// <summary>
//    /// Serializza e scrive un record su disco usando un buffer pooled (path caldo Append/AppendMany).
//    /// </summary>
//    internal static void WriteRecordWithMetadata(
//        IsamFileHandle fileHandle,
//        long offset,
//        Config.IsamSchema schema,
//        UdtInstance udtInstance,
//        RecordMetadata metadata)
//    {
//        var recordSize = SchemaManager.CalculateRecordSize(schema);
//        var buffer = ArrayPool<byte>.Shared.Rent(recordSize);
//        try
//        {
//            SerializeRecordWithMetadataToBuffer(schema, udtInstance, metadata, fileHandle, buffer);
//            fileHandle.WriteRecordFromBuffer(offset, buffer, recordSize);
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(buffer);
//        }
//    }
//
//    /// <summary>
//    /// Deserializza un buffer di byte in metadati e una istanza UDT.
//    /// Questo metodo è il punto di ingresso principale per la deserializzazione di un record completo.
//    /// </summary>
//    /// <param name="schema">Lo schema che definisce la struttura del record.</param>
//    /// <param name="buffer">L'array di byte contenente i dati del record.</param>
//    /// <param name="udtDefinition">La definizione AST del tipo UDT.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per operazioni sul LOB Heap (es. JSON)
//    /// e per ottenere l'encoding del file.</param>
//    /// <returns>Una tupla contenente i metadati del record e l'istanza UDT popolata.</returns>
//    public static (RecordMetadata metadata, UdtInstance instance) DeserializeRecord(
//        Config.IsamSchema schema,
//        byte[] buffer,
//        UserDefinedType udtDefinition,
//        IsamFileHandle fileHandle)
//    {
//        using var stream = new MemoryStream(buffer);
//        using var reader = new BinaryReader(stream, fileHandle.FileEncoding); // Usa FileEncoding dal handle
//
//        // 1. Legge i metadati usando MarshalHelper.
//        var metadata = MarshalHelper.ReadStructure<RecordMetadata>(stream);
//
//        // 2. Legge i dati dell'UDT.
//        var instance = ReadUdtData(reader, schema, udtDefinition, fileHandle);
//
//        return (metadata, instance);
//    }
//
//    /// <summary>
//    /// Serializza una struttura <see cref="RecordMetadata"/> nella sua rappresentazione binaria esatta (un array di byte),
//    /// pronta per essere scritta su disco.
//    /// </summary>
//    /// <param name="metadata">L'istanza della struttura <see cref="RecordMetadata"/> contenente lo stato e la versione del record da serializzare.</param>
//    /// <returns>
//    /// Un array di byte la cui lunghezza e il cui layout corrispondono esattamente alla definizione <c>[StructLayout]</c>
//    /// della struttura <see cref="RecordMetadata"/>.
//    /// </returns>
//    /// <remarks>
//    /// Questo metodo è un wrapper che utilizza <see cref="MarshalHelper.WriteStructure{T}(Stream, T)"/> per produrre
//    /// un array di byte contenente solo i metadati, facilitando l'uso in contesti dove è richiesto un `byte[]` per i metadati (es. WAL).
//    /// </remarks>
//    public static byte[] SerializeMetadata(RecordMetadata metadata)
//    {
//        using var stream = new MemoryStream();
//        MarshalHelper.WriteStructure(stream, metadata);
//        return stream.ToArray();
//    }
//
//    /// <summary>
//    /// Legge solo la porzione di metadati (stato, versione, TransactionId e PreviousVersionOffset) da un buffer di record.
//    /// Questa operazione è veloce e non richiede la deserializzazione dell'intero record.
//    /// </summary>
//    /// <param name="recordBuffer">Il buffer di byte contenente l'intero record.</param>
//    /// <param name="fileEncoding">L'encoding del file, necessario per il BinaryReader.</param>
//    /// <returns>La struttura <see cref="RecordMetadata"/> letta dal buffer.</returns>
//    public static RecordMetadata ReadMetadata(byte[] recordBuffer, Encoding fileEncoding)
//    {
//        using var stream = new MemoryStream(recordBuffer);
//        // L'encoding è richiesto dal costruttore di BinaryReader, anche se per una struct non testuale non è critico.
//        using var reader = new BinaryReader(stream, fileEncoding);
//        return MarshalHelper.ReadStructure<RecordMetadata>(stream);
//    }
//
//    /// <summary>
//    /// Scrive i dati di una istanza UDT (<see cref="UdtInstance"/>) su uno stream binario,
//    /// seguendo l'ordine e il formato definiti dallo schema ISAM. Gestisce anche lo storage off-row per JSON.
//    /// </summary>
//    /// <param name="writer">Il <see cref="BinaryWriter"/> su cui scrivere i dati serializzati.</param>
//    /// <param name="schema">L'oggetto <see cref="Config.IsamSchema"/> che definisce i campi, i loro tipi e le loro lunghezze.</param>
//    /// <param name="udtInstance">L'istanza UDT i cui valori dei campi devono essere scritti.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se un campo definito nello schema non viene trovato nel dizionario dei campi dell'istanza UDT,
//    /// indicando un disallineamento tra lo schema e i dati.
//    /// </exception>
//    private static void WriteUdtData(BinaryWriter writer, Config.IsamSchema schema, UdtInstance udtInstance,
//        IsamFileHandle fileHandle)
//    {
//        // 1. Crea una mappa di lookup una-tantum per tradurre i nomi dei campi in SymbolId.
//        var fieldIdMap = udtInstance.Definition.Fields.ToDictionary(
//            f => f.Name.Lexeme,
//            f => f.SymbolId,
//            StringComparer.OrdinalIgnoreCase);
//        foreach (var field in schema.Fields)
//        {
//            // Usa la mappa per trovare il SymbolId corrispondente al nome del campo.
//            if (!fieldIdMap.TryGetValue(field.Name, out var symbolId))
//                throw new InvalidOperationException(
//                    $"Disallineamento schema: Campo '{field.Name}' definito nello schema ISAM ma non trovato nel TYPE '{udtInstance.Definition.OriginalName.Lexeme}'.");
//
//            // Usa il SymbolId per recuperare il valore dall'istanza UDT.
//            if (!udtInstance.Fields.TryGetValue(symbolId, out var value))
//                throw new InvalidOperationException(
//                    $"Dato mancante: Valore per il campo '{field.Name}' (ID: {symbolId}) non trovato nell'istanza UDT.");
//
//            WriteField(writer, value, field.DataType, field.Length, fileHandle);
//        }
//    }
//
//    /// <summary>
//    /// Legge i dati di un record da uno stream binario e li deserializza in una nuova istanza UDT
//    /// (<see cref="UdtInstance"/>), seguendo l'ordine e il formato definiti dallo schema ISAM.
//    /// Gestisce anche il recupero di dati off-row per JSON.
//    /// </summary>
//    /// <param name="reader">Il <see cref="BinaryReader"/> da cui leggere i dati grezzi del record.</param>
//    /// <param name="schema">L'oggetto <see cref="Config.IsamSchema"/> che definisce i campi, i loro tipi e le loro lunghezze.</param>
//    /// <param name="udtDefinition">La definizione del tipo (<see cref="UserDefinedType"/>) necessaria per costruire
//    /// l'istanza UDT risultante.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
//    /// <returns>Una nuova istanza di <see cref="UdtInstance"/> popolata con i dati letti dallo stream.</returns>
//    /// <exception cref="System.Text.Json.JsonException">Se il JSON letto dal LOB Heap non è valido.</exception>
//    private static UdtInstance ReadUdtData(BinaryReader reader, Config.IsamSchema schema,
//        UserDefinedType udtDefinition, IsamFileHandle fileHandle)
//    {
//        // 1. Leggi i dati in un dizionario temporaneo string-keyed, come prima.
//        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        foreach (var field in schema.Fields)
//        {
//            stringKeyedFields[field.Name] = ReadField(reader, field.DataType, field.Length, fileHandle);
//        }
//
//        // 2. Crea la mappa di traduzione Nome -> SymbolId.
//        var fieldIdMap = udtDefinition.Fields.ToDictionary(
//            f => f.Name.Lexeme, 
//            f => f.SymbolId, 
//            StringComparer.OrdinalIgnoreCase);
//            
//        // 3. Esegui il "marshalling": converti il dizionario string-keyed in un dizionario int-keyed.
//        var intKeyedFields = new Dictionary<int, object?>();
//        foreach (var (fieldName, value) in stringKeyedFields)
//        {
//            if (fieldIdMap.TryGetValue(fieldName, out var symbolId))
//            {
//                intKeyedFields[symbolId] = value;
//            }
//            // Se un campo letto dal file non esiste nell'UDT, viene semplicemente ignorato.
//        }
//
//        // 4. Crea l'UdtInstance con il dizionario corretto.
//        return new UdtInstance(udtDefinition, intKeyedFields);
//        
//    }
//
//    /// <summary>
//    /// Scrive un singolo valore su uno stream binario, convertendolo nel formato corretto in base al suo tipo di dato
//    /// e alla lunghezza definita dallo schema. Gestisce lo storage off-row per i tipi JSON.
//    /// </summary>
//    /// <param name="writer">Il <see cref="BinaryWriter"/> su cui scrivere i dati serializzati.</param>
//    /// <param name="value">Il valore dell'oggetto da scrivere.</param>
//    /// <param name="dataType">Il <see cref="DataType"/> del campo, che determina il formato di serializzazione.</param>
//    /// <param name="length">La lunghezza fissa in byte del campo, usata principalmente per le stringhe.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
//    internal static void WriteField(BinaryWriter writer, object? value, DataType dataType, int length,
//        IsamFileHandle fileHandle)
//    {
//        var coercedValue = CoerceValue(value, dataType);
//
//        switch (dataType)
//        {
//            case StringType:
//                var str = (string)coercedValue;
//                var buffer = new byte[length];
//                Array.Fill(buffer, (byte)' ');
//                if (!string.IsNullOrEmpty(str))
//                {
//                    var encoded = fileHandle.FileEncoding.GetBytes(str);
//                    Buffer.BlockCopy(encoded, 0, buffer, 0, Math.Min(encoded.Length, length));
//                }
//
//                writer.Write(buffer);
//                break;
//
//            case BitType: writer.Write((bool)coercedValue); break;
//            case ByteType: writer.Write((byte)coercedValue); break;
//            case SByteType: writer.Write((sbyte)coercedValue); break;
//            case IntegerType: writer.Write((short)coercedValue); break;
//            case UIntegerType: writer.Write((ushort)coercedValue); break;
//            case LongType: writer.Write((int)coercedValue); break;
//            case ULongType: writer.Write((uint)coercedValue); break;
//            case QuadType: writer.Write((long)coercedValue); break;
//            case UQuadType: writer.Write((ulong)coercedValue); break;
//            case SingleType: writer.Write((float)coercedValue); break;
//            case DoubleType: writer.Write((double)coercedValue); break;
//            case CurrencyType: writer.Write((decimal)coercedValue); break;
//            case DateType: writer.Write(((DateOnly)coercedValue).ToDateTime(TimeOnly.MinValue).Ticks); break;
//            case TimeType: writer.Write(((TimeOnly)coercedValue).Ticks); break;
//            case DateTimeType: writer.Write(((DateTime)coercedValue).Ticks); break;
//            case TimeSpanType: writer.Write(((TimeSpan)coercedValue).Ticks); break;
//
//            case JsonType:
//                // Tratta il valore come una stringa. La conversione da JsonWrapper a stringa
//                // è ora responsabilità dell'Executor.
//                var jsonString = (string)CoerceValue(value, new StringType());
//                var jsonBytes = fileHandle.FileEncoding.GetBytes(jsonString);
//                var lobOffset = fileHandle.WriteLob(jsonBytes);
//                writer.Write(lobOffset); // Scrive il puntatore.
//                break;
//
//            default:
//                // Per tipi non supportati direttamente o sconosciuti, scrive un blocco di byte vuoti se la lunghezza è > 0.
//                if (length > 0) writer.Write(new byte[length]);
//                break;
//        }
//    }
//
//    /// <summary>
//    /// Legge un singolo valore da uno stream binario, interpretando i byte in base al tipo di dato
//    /// e alla lunghezza specificati dallo schema. Gestisce il recupero di dati off-row per i tipi JSON.
//    /// </summary>
//    /// <param name="reader">Il <see cref="BinaryReader"/> da cui leggere i dati grezzi.</param>
//    /// <param name="dataType">Il <see cref="DataType"/> del campo, che determina come interpretare i byte.</param>
//    /// <param name="length">La lunghezza fissa in byte del campo, usata per i tipi a lunghezza fissa come le stringhe.</param>
//    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
//    /// <returns>L'oggetto .NET deserializzato.</returns>
//    /// <exception cref="System.Text.Json.JsonException">Se il JSON letto dal LOB Heap non è valido.</exception>
//    internal static object ReadField(BinaryReader reader, DataType dataType, int length, IsamFileHandle fileHandle)
//    {
//        return dataType switch
//        {
//            StringType => fileHandle.FileEncoding.GetString(reader.ReadBytes(length)).TrimEnd('\0', ' '),
//            BitType => reader.ReadBoolean(),
//            ByteType => reader.ReadByte(),
//            SByteType => reader.ReadSByte(),
//            IntegerType => reader.ReadInt16(),
//            UIntegerType => reader.ReadUInt16(),
//            LongType => reader.ReadInt32(),
//            ULongType => reader.ReadUInt32(),
//            QuadType => reader.ReadInt64(),
//            UQuadType => reader.ReadUInt64(),
//            SingleType => reader.ReadSingle(),
//            DoubleType => reader.ReadDouble(),
//            CurrencyType => reader.ReadDecimal(),
//            DateType => DateOnly.FromDateTime(new DateTime(reader.ReadInt64())),
//            TimeType => new TimeOnly(reader.ReadInt64()),
//            DateTimeType => new DateTime(reader.ReadInt64()),
//            TimeSpanType => new TimeSpan(reader.ReadInt64()),
//
//            // Per JsonType, legge il puntatore, recupera i byte JSON dal LOB Heap e li parsifica.
//            JsonType =>
//                // Legge il puntatore, recupera i byte dal LOB Heap e restituisce una stringa.
//                // La conversione in JsonWrapper è responsabilità dell'Executor.
//                fileHandle.FileEncoding.GetString(fileHandle.ReadLob(reader.ReadInt64())),
//
//            _ => GetDefaultValue(dataType)
//        };
//    }
//
//    /// <summary>
//    /// Restituisce il valore di default per un dato tipo di dato.
//    /// </summary>
//    /// <param name="dataType">Il tipo di dato per cui recuperare il valore di default.</param>
//    /// <returns>Un oggetto che rappresenta il valore di default per il tipo specificato.</returns>
//    private static object GetDefaultValue(DataType dataType)
//    {
//        return dataType switch
//        {
//            StringType { FixedLength: null } => string.Empty,
//            StringType { FixedLength: { } len } => new string('\0', len),
//            BitType => false,
//            ByteType => (byte)0,
//            SByteType => (sbyte)0,
//            IntegerType => (short)0,
//            UIntegerType => (ushort)0,
//            LongType => 0,
//            ULongType => 0U,
//            QuadType => 0L,
//            UQuadType => 0UL,
//            SingleType => 0.0f,
//            DoubleType => 0.0d,
//            CurrencyType => 0.0m,
//            DateType => DateOnly.MinValue,
//            TimeType => TimeOnly.MinValue,
//            DateTimeType => DateTime.MinValue,
//            TimeSpanType => TimeSpan.Zero,
//            JsonType => new JsonWrapper(new JsonObject()),
//            _ => null! // Per i tipi non serializzabili o sconosciuti, restituiamo null.
//        };
//    }
//
//    /// <summary>
//    /// Converte un valore generico nel tipo .NET esatto richiesto per la serializzazione,
//    /// gestendo conversioni numeriche, booleane e la coercizione a tipi ISAM.
//    /// </summary>
//    /// <param name="value">Il valore da convertire.</param>
//    /// <param name="targetType">Il tipo di dato ISAM di destinazione.</param>
//    /// <returns>Il valore convertito, tipizzato correttamente per il campo ISAM.</returns>
//    /// <exception cref="InvalidOperationException">Se la conversione non è possibile o causa un overflow.</exception>
//    private static object CoerceValue(object? value, DataType targetType)
//    {
//        // Gestisce la conversione standard dei booleani BASIC (-1/0) a bool .NET se il target non è BitType.
//        if (value is bool b && targetType is not BitType)
//            value = b ? -1 : 0;
//
//        try
//        {
//            return targetType switch
//            {
//                StringType => Convert.ToString(value) ?? string.Empty,
//                BitType => Convert.ToBoolean(value),
//                ByteType => Convert.ToByte(value),
//                SByteType => Convert.ToSByte(value),
//                IntegerType => Convert.ToInt16(value),
//                UIntegerType => Convert.ToUInt16(value),
//                LongType => Convert.ToInt32(value),
//                ULongType => Convert.ToUInt32(value),
//                QuadType => Convert.ToInt64(value),
//                UQuadType => Convert.ToUInt64(value),
//                SingleType => Convert.ToSingle(value),
//                DoubleType => Convert.ToDouble(value),
//                CurrencyType => Convert.ToDecimal(value),
//                // Un campo DATE/TIME/TIMESPAN mai assegnato esplicitamente (es. "UltimoOrdine
//                // AS DATE" in un record dove solo alcuni campi vengono impostati prima di
//                // ISAM APPEND) arriva qui col default BASIC generico per un campo non-stringa:
//                // un double 0.0 (vedi DefaultValueForFieldType, PCodeVm/PCodeProcessor.Helpers.cs)
//                // — MAI un DateTime/null. Il cast diretto "(DateOnly)(double)" non è definito in
//                // C# e lanciava sempre InvalidCastException, che ISAM_APPEND inghiottiva in un
//                // catch silenzioso: qualunque record con un campo data/ora non esplicitamente
//                // valorizzato non veniva MAI scritto (0 record appesi, navigazione/GET
//                // successivi vedevano un file vuoto — scoperto verificando DbIsamFullTests.bas
//                // con TYPE Cliente che include "UltimoOrdine AS DATE"). Il double è trattato
//                // come OADate (stessa convenzione di DateValue/GbDate in tutto il resto della
//                // VM): 0.0 → 30/12/1899, il classico "zero date" di BASIC/OLE Automation.
//                // Il fallback finale usa Convert.ToDateTime (via IConvertible), non un cast
//                // diretto: DATETIME.DATEVALUE(...) e simili builtin di PCodeVm restituiscono un
//                // wrapper interno (PCodeVm.DateValue, non visibile da questo progetto, Databases
//                // sta SOTTO PCodeVm nel grafo di dipendenze) che implementa IConvertible.
//                // ToDateTime() — un cast diretto "(DateOnly)value" su quell'oggetto lanciava
//                // InvalidCastException (mai un vero DateOnly), ISAM_APPEND la inghiottiva in un
//                // catch silenzioso: qualunque record con un campo DATE valorizzato tramite
//                // DATEVALUE/NOW/TODAY ecc. (non un raro caso limite: la normale via per
//                // assegnare una data) non veniva MAI scritto.
//                DateType => value switch
//                {
//                    DateTime dt => DateOnly.FromDateTime(dt),
//                    DateOnly d => d,
//                    double oaDate => DateOnly.FromDateTime(DateTime.FromOADate(oaDate)),
//                    null => DateOnly.MinValue,
//                    _ => DateOnly.FromDateTime(Convert.ToDateTime(value))
//                },
//                TimeType => value switch
//                {
//                    DateTime dtt => TimeOnly.FromDateTime(dtt),
//                    TimeOnly t => t,
//                    double oaTime => TimeOnly.FromDateTime(DateTime.FromOADate(oaTime)),
//                    null => TimeOnly.MinValue,
//                    _ => TimeOnly.FromDateTime(Convert.ToDateTime(value))
//                },
//                TimeSpanType => value switch
//                {
//                    DateTime dts => dts.TimeOfDay,
//                    TimeSpan ts => ts,
//                    double oaSpan => TimeSpan.FromDays(oaSpan),
//                    null => TimeSpan.Zero,
//                    _ => Convert.ToDateTime(value).TimeOfDay
//                },
//                // Stesso ragionamento di DateType/TimeType sopra: Convert.ToDateTime non
//                // accetta un double (OADate) — serve FromOADate esplicito.
//                DateTimeType => value switch
//                {
//                    DateTime dtv => dtv,
//                    double oaDt => DateTime.FromOADate(oaDt),
//                    null => DateTime.MinValue,
//                    _ => Convert.ToDateTime(value)
//                },
//                JsonType when value is JsonWrapper jw => jw,
//                JsonType when value is string s => new JsonWrapper(
//                    JsonNode.Parse(s) ?? new JsonObject()),
//                // Se il tipo di destinazione non è riconosciuto, lancia un'eccezione.
//                _ => throw new InvalidCastException()
//            };
//        }
//        catch (Exception ex) when (ex is FormatException or InvalidCastException or OverflowException)
//        {
//            throw new InvalidOperationException(
//                $"Impossibile convertire il valore '{value}' nel tipo ISAM '{targetType}'. Dettagli: {ex.Message}", ex);
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static int rm_is_numeric(const char* typeCode)
{
    return typeCode && (
        strcmp(typeCode, "IntegerType") == 0 || strcmp(typeCode, "UIntegerType") == 0 ||
        strcmp(typeCode, "LongType") == 0 || strcmp(typeCode, "ULongType") == 0 ||
        strcmp(typeCode, "QuadType") == 0 || strcmp(typeCode, "UQuadType") == 0 ||
        strcmp(typeCode, "ByteType") == 0 || strcmp(typeCode, "SByteType") == 0 ||
        strcmp(typeCode, "BitType") == 0);
}

static void rm_wr_i16(uint8_t* p, int16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static int16_t rm_rd_i16(const uint8_t* p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void rm_wr_i32(uint8_t* p, int32_t v)
{
    int i;
    for (i = 0; i < 4; i++) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static int32_t rm_rd_i32(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void rm_wr_i64(uint8_t* p, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++) p[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static int64_t rm_rd_i64(const uint8_t* p)
{
    int i;
    uint64_t u = 0;
    for (i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

static int rm_write_field(uint8_t* dst, int cap, const IsamFieldSchema* field, const IsamFieldValue* val)
{
    int sz = SchemaManager_GetFieldSize(field->DataTypeCode, field->Length);
    if (sz < 0 || sz > cap) return -1;
    memset(dst, 0, (size_t)sz);
    if (strcmp(field->DataTypeCode, "StringType") == 0) {
        memset(dst, ' ', (size_t)sz);
        if (val->text) {
            int n = (int)strlen(val->text);
            if (n > sz) n = sz;
            memcpy(dst, val->text, (size_t)n);
        }
        return sz;
    }
    if (strcmp(field->DataTypeCode, "BitType") == 0 || strcmp(field->DataTypeCode, "ByteType") == 0
        || strcmp(field->DataTypeCode, "SByteType") == 0) {
        dst[0] = (uint8_t)val->i64;
        return sz;
    }
    if (strcmp(field->DataTypeCode, "IntegerType") == 0 || strcmp(field->DataTypeCode, "UIntegerType") == 0) {
        rm_wr_i16(dst, (int16_t)val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "LongType") == 0 || strcmp(field->DataTypeCode, "ULongType") == 0) {
        rm_wr_i32(dst, (int32_t)val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "QuadType") == 0 || strcmp(field->DataTypeCode, "UQuadType") == 0
        || strcmp(field->DataTypeCode, "DateTimeType") == 0 || strcmp(field->DataTypeCode, "DateType") == 0
        || strcmp(field->DataTypeCode, "TimeType") == 0 || strcmp(field->DataTypeCode, "TimeSpanType") == 0
        || strcmp(field->DataTypeCode, "JsonType") == 0) {
        rm_wr_i64(dst, val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "DoubleType") == 0) {
        /* IEEE-754 little-endian (BitConverter.GetBytes(double)): i64 porta gia' i bit. */
        rm_wr_i64(dst, val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "SingleType") == 0) {
        rm_wr_i32(dst, (int32_t)(uint32_t)val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "CurrencyType") == 0) {
        /* .NET decimal (16 byte LE): lo|mid|hi|flags(scale<<16|sign<<31). Valore intero
         * scale 0 (parità Reader.ReadDecimal C#: 96-bit int + flags). Prima il campo cadeva
         * nel default → 16 byte di zeri → Saldo letto 0 (DbIsamFullTests). */
        /* .NET decimal: la mantissa è il VALORE ASSOLUTO, il segno è nel flag. Scrivere il
         * complemento a due E il flag (come prima) raddoppiava la negazione → +120 da -120. */
        uint64_t v = (val->i64 < 0) ? (uint64_t)(-(int64_t)val->i64) : (uint64_t)val->i64;
        rm_wr_i32(dst + 0, (int32_t)(uint32_t)(v & 0xFFFFFFFFu));
        rm_wr_i32(dst + 4, (int32_t)(uint32_t)((v >> 32) & 0xFFFFFFFFu));
        rm_wr_i32(dst + 8, 0);
        /* flags: scale=4 (il valore i64 è già scalato ×10^4) + segno. */
        rm_wr_i32(dst + 12, (int32_t)(uint32_t)(((uint32_t)4 << 16) | ((val->i64 < 0) ? 0x80000000u : 0u)));
        return sz;
    }
    return sz;
}

static int rm_read_field(const uint8_t* src, const IsamFieldSchema* field, IsamFieldValue* val)
{
    int sz = SchemaManager_GetFieldSize(field->DataTypeCode, field->Length);
    int n;
    if (sz < 0) return -1;
    val->i64 = 0;
    if (strcmp(field->DataTypeCode, "StringType") == 0) {
        n = sz;
        while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\0')) n--;
        if (val->textOut && val->textOutCap > 0) {
            if (n >= val->textOutCap) n = val->textOutCap - 1;
            memcpy(val->textOut, src, (size_t)n);
            val->textOut[n] = 0;
        }
        return sz;
    }
    if (strcmp(field->DataTypeCode, "BitType") == 0) {
        /* booleano BASIC: -1 (true) / 0 (false). Prima `src[0]` restituiva 255 per TRUE
         * (byte di -1) → i filtri `Attivo = -1` non matchavano mai. */
        val->i64 = src[0] ? -1 : 0;
        return sz;
    }
    if (strcmp(field->DataTypeCode, "ByteType") == 0
        || strcmp(field->DataTypeCode, "SByteType") == 0) {
        val->i64 = src[0];
        return sz;
    }
    if (strcmp(field->DataTypeCode, "IntegerType") == 0 || strcmp(field->DataTypeCode, "UIntegerType") == 0) {
        val->i64 = rm_rd_i16(src);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "LongType") == 0 || strcmp(field->DataTypeCode, "ULongType") == 0) {
        val->i64 = rm_rd_i32(src);
        return sz;
    }
    if (rm_is_numeric(field->DataTypeCode) || strcmp(field->DataTypeCode, "JsonType") == 0
        || strcmp(field->DataTypeCode, "DateTimeType") == 0 || strcmp(field->DataTypeCode, "DateType") == 0
        || strcmp(field->DataTypeCode, "TimeType") == 0 || strcmp(field->DataTypeCode, "TimeSpanType") == 0) {
        val->i64 = rm_rd_i64(src);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "DoubleType") == 0) {
        val->i64 = rm_rd_i64(src);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "SingleType") == 0) {
        val->i64 = (int64_t)(uint32_t)rm_rd_i32(src);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "CurrencyType") == 0) {
        /* .NET decimal: mantissa 96-bit + scale/sign. Valore → i64 (dopo la scala). */
        uint64_t lo = (uint32_t)rm_rd_i32(src + 0);
        uint64_t mid = (uint32_t)rm_rd_i32(src + 4);
        uint32_t flags = (uint32_t)rm_rd_i32(src + 12);
        uint64_t v = lo | (mid << 32);
        /* Il valore è già scalato ×10^4: NON dividere (la formattazione decimale è a valle,
         * gmb_db_isam_read). */
        (void)flags;
        val->i64 = (flags & 0x80000000u) ? -(int64_t)v : (int64_t)v;
        return sz;
    }
    return sz;
}

int RecordMarshaller_SerializeToBuffer(const IsamSchema* schema, const RecordMetadata* meta,
                                       const IsamFieldValue* fields, int fieldCount,
                                       uint8_t* buffer, int bufferLen)
{
    int recSize, i, pos;
    if (!schema || !meta || !buffer) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(schema);
    if (recSize < 0 || bufferLen < recSize || fieldCount != schema->FieldCount) return GMB_DB_ERR;
    memset(buffer, 0, (size_t)recSize);
    memcpy(buffer, meta, RecordMetadata_Size);
    pos = RecordMetadata_Size;
    for (i = 0; i < schema->FieldCount; i++) {
        int n = rm_write_field(buffer + pos, recSize - pos, &schema->Fields[i], &fields[i]);
        if (n < 0) return GMB_DB_ERR;
        pos += n;
    }
    return pos == recSize ? GMB_DB_OK : GMB_DB_ERR;
}

int RecordMarshaller_DeserializeFromBuffer(const IsamSchema* schema, const uint8_t* buffer, int bufferLen,
                                           RecordMetadata* meta, IsamFieldValue* fields, int fieldCount)
{
    int recSize, i, pos;
    if (!schema || !buffer || !meta || !fields) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(schema);
    if (recSize < 0 || bufferLen < recSize || fieldCount != schema->FieldCount) return GMB_DB_ERR;
    memcpy(meta, buffer, RecordMetadata_Size);
    pos = RecordMetadata_Size;
    for (i = 0; i < schema->FieldCount; i++) {
        int n = rm_read_field(buffer + pos, &schema->Fields[i], &fields[i]);
        if (n < 0) return GMB_DB_ERR;
        pos += n;
    }
    return GMB_DB_OK;
}

int RecordMarshaller_WriteRecordWithMetadata(IsamFileHandle* fileHandle, int64_t offset,
                                             const IsamFieldValue* fields, int fieldCount,
                                             const RecordMetadata* meta)
{
    int recSize;
    uint8_t* buf;
    int rc;
    IsamFieldValue* local = NULL;
    int i, jsonPass = 0;
    if (!fileHandle || !fileHandle->Schema || !meta) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(fileHandle->Schema);
    if (recSize < 0) return GMB_DB_ERR;
    /* JSON pre-pass (port LOB heap): per ogni campo JsonType con valore testuale, scrivi il
     * payload nel LOB heap e sostituisci il valore con l'offset 8-byte (il formato del record). */
    for (i = 0; i < fieldCount && i < fileHandle->Schema->FieldCount; i++) {
        if (strcmp(fileHandle->Schema->Fields[i].DataTypeCode, "JsonType") == 0
            && fields[i].text) {
            jsonPass = 1;
            break;
        }
    }
    if (jsonPass) {
        local = (IsamFieldValue*)calloc((size_t)fieldCount, sizeof(*local));
        if (!local) return GMB_DB_ERR;
        memcpy(local, fields, (size_t)fieldCount * sizeof(*local));
        for (i = 0; i < fieldCount && i < fileHandle->Schema->FieldCount; i++) {
            if (strcmp(fileHandle->Schema->Fields[i].DataTypeCode, "JsonType") == 0
                && local[i].text) {
                local[i].i64 = IsamFileHandle_WriteLob(fileHandle,
                    (const uint8_t*)local[i].text, (int)strlen(local[i].text));
                if (local[i].i64 < 0) { free(local); return GMB_DB_ERR; }
                local[i].text = NULL;
            }
        }
        fields = local;
    }
    buf = (uint8_t*)malloc((size_t)recSize);
    if (!buf) { free(local); return GMB_DB_ERR; }
    rc = RecordMarshaller_SerializeToBuffer(fileHandle->Schema, meta, fields, fieldCount, buf, recSize);
    if (rc == GMB_DB_OK)
        rc = IsamFileHandle_WriteRecordFromBuffer(fileHandle, offset, buf, recSize);
    free(buf);
    free(local);
    return rc;
}
