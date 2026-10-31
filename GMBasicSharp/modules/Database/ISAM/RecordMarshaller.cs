// FILE: Basic.ISAM.RecordMarshaller.cs

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json.Nodes;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace
// ReSharper disable RedundantNameQualifier

namespace Basic.ISAM;

internal static class RecordMarshaller
{
    /// <summary>
    /// Serializza una istanza di UDT in un buffer di byte, utilizzando una struttura di metadati predefinita.
    /// Questo metodo è il punto di ingresso principale per la serializzazione di un record completo.
    /// </summary>
    /// <param name="schema">Lo schema che definisce la struttura del record.</param>
    /// <param name="udtInstance">L'istanza UDT contenente i dati da scrivere.</param>
    /// <param name="metadata">La struttura di metadati completa (stato, versione, txId, etc.) da scrivere.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per operazioni sul LOB Heap (es. JSON)
    /// e per ottenere l'encoding del file.</param>
    /// <returns>Un array di byte che rappresenta il record completo (metadati + dati).</returns>
    /// <exception cref="InvalidDataException">Se la dimensione del record serializzato non corrisponde allo schema.</exception>
    public static byte[] SerializeRecordWithMetadata(Config.IsamSchema schema, UdtInstance udtInstance,
        RecordMetadata metadata, IsamFileHandle fileHandle)
    {
        var recordSize = SchemaManager.CalculateRecordSize(schema);
        var buffer = new byte[recordSize];
        SerializeRecordWithMetadataToBuffer(schema, udtInstance, metadata, fileHandle, buffer);
        return buffer;
    }

    /// <summary>
    /// Serializza un record completo in un buffer pre-allocato (zero ToArray intermedio).
    /// </summary>
    internal static void SerializeRecordWithMetadataToBuffer(
        Config.IsamSchema schema,
        UdtInstance udtInstance,
        RecordMetadata metadata,
        IsamFileHandle fileHandle,
        byte[] buffer,
        int offset = 0)
    {
        var recordSize = SchemaManager.CalculateRecordSize(schema);
        if (buffer.Length - offset < recordSize)
        {
            throw new ArgumentException(
                $"Buffer record troppo piccolo: {buffer.Length - offset} < {recordSize}.", nameof(buffer));
        }

        using var stream = new MemoryStream(buffer, offset, recordSize, writable: true, publiclyVisible: true);
        stream.SetLength(0);
        MarshalHelper.WriteStructure(stream, metadata);
        using var writer = new BinaryWriter(stream, fileHandle.FileEncoding, leaveOpen: true);
        WriteUdtData(writer, schema, udtInstance, fileHandle);

        if (stream.Position != recordSize)
        {
            throw new InvalidDataException("La dimensione del record serializzato non corrisponde allo schema.");
        }
    }

    /// <summary>
    /// Serializza e scrive un record su disco usando un buffer pooled (path caldo Append/AppendMany).
    /// </summary>
    internal static void WriteRecordWithMetadata(
        IsamFileHandle fileHandle,
        long offset,
        Config.IsamSchema schema,
        UdtInstance udtInstance,
        RecordMetadata metadata)
    {
        var recordSize = SchemaManager.CalculateRecordSize(schema);
        var buffer = ArrayPool<byte>.Shared.Rent(recordSize);
        try
        {
            SerializeRecordWithMetadataToBuffer(schema, udtInstance, metadata, fileHandle, buffer);
            fileHandle.WriteRecordFromBuffer(offset, buffer, recordSize);
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(buffer);
        }
    }

    /// <summary>
    /// Deserializza un buffer di byte in metadati e una istanza UDT.
    /// Questo metodo è il punto di ingresso principale per la deserializzazione di un record completo.
    /// </summary>
    /// <param name="schema">Lo schema che definisce la struttura del record.</param>
    /// <param name="buffer">L'array di byte contenente i dati del record.</param>
    /// <param name="udtDefinition">La definizione AST del tipo UDT.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per operazioni sul LOB Heap (es. JSON)
    /// e per ottenere l'encoding del file.</param>
    /// <param name="length">Numero di byte validi in <paramref name="buffer"/>. <c>-1</c> (default)
    /// = usa <c>buffer.Length</c>, cioe' il comportamento storico per i caller che passano un array
    /// di dimensione esatta. Va passato esplicitamente quando <paramref name="buffer"/> e' piu'
    /// grande del record (es. un buffer preso da <c>ArrayPool</c>): senza, il
    /// <c>MemoryStream</c> avrebbe per lunghezza l'array intero e un campo che dichiara una
    /// lunghezza oltre la fine del record leggerebbe la coda spazzatura invece di lanciare
    /// <see cref="EndOfStreamException"/> — silenziosamente e con dati non deterministici
    /// (resti di un record precedente nel buffer riusato).</param>
    /// <returns>Una tupla contenente i metadati del record e l'istanza UDT popolata.</returns>
    public static (RecordMetadata metadata, UdtInstance instance) DeserializeRecord(
        Config.IsamSchema schema,
        byte[] buffer,
        UserDefinedType udtDefinition,
        IsamFileHandle fileHandle,
        int length = -1)
    {
        /* Lo stream valido e' il tratto [0, length): con writable=false + publiclyVisible=false
         * MemoryStream rifiuta l'offset iniziale su un buffer gia' materializzato solo se
         * writable=true; index 0 + count = length imposta Length al solo tratto valido.
         * NOTA E1.5.2: lo stream qui sotto e' solo un ponte verso lo span — MemoryStream e
         * BinaryReader per record sono eliminati, la lettura avviene su IsamSpanReader. */
        ReadOnlySpan<byte> bytes = length < 0 ? buffer : buffer.AsSpan(0, length);
        var reader = new IsamSpanReader(bytes);

        // 1. Legge i metadati senza allocazioni (era MarshalHelper.ReadStructure: new byte[]
        // + AllocHGlobal/FreeHGlobal per chiamata). I campi sono letti a mano nell'ordine
        // dichiarato dalla struct ([Sequential, Pack=1]: 1 + 8 + 8 + 8 = 25 byte), identico
        // al layout che PtrToStructure interpreta.
        var metadata = reader.ReadMetadata();

        // 2. Legge i dati dell'UDT.
        var instance = ReadUdtData(ref reader, schema, udtDefinition, fileHandle);

        return (metadata, instance);
    }

    /* Lettore posizionale su span, senza allocazioni (E1.5.2). Sostituisce il
     * MemoryStream+BinaryReader per record in DeserializeRecord. ref struct: vive solo sullo
     * stack, zero heap. Contratto identico a BinaryReader, verificato contro
     * BinaryReader/BinaryWriter (little-endian in entrambi):
     * - i tipi a lunghezza fissa lanciano EndOfStreamException se i byte non bastano;
     * - ReadBytes restituisce il tratto disponibile senza lanciare (come BinaryReader.ReadBytes);
     * - ReadDecimal legge 4 Int32 LE e usa il ctor decimal(ReadOnlySpan<int>), identico ordine.
     * internal (non private) perche' ReadField e' internal e lo prende per ref. */
    internal ref struct IsamSpanReader
    {
        private readonly ReadOnlySpan<byte> _span;
        private int _pos;

        public IsamSpanReader(ReadOnlySpan<byte> span) { _span = span; _pos = 0; }

        public ReadOnlySpan<byte> ReadBytes(int count)
        {
            int avail = Math.Min(count, _span.Length - _pos);
            if (avail <= 0) return ReadOnlySpan<byte>.Empty;
            var s = _span.Slice(_pos, avail);
            _pos += avail;
            return s;
        }

        private ReadOnlySpan<byte> Take(int count)
        {
            if (_span.Length - _pos < count) throw new EndOfStreamException();
            var s = _span.Slice(_pos, count);
            _pos += count;
            return s;
        }

        public byte ReadByte() => Take(1)[0];
        public sbyte ReadSByte() => (sbyte)Take(1)[0];
        public bool ReadBoolean() => Take(1)[0] != 0;
        public short ReadInt16() => BinaryPrimitives.ReadInt16LittleEndian(Take(2));
        public ushort ReadUInt16() => BinaryPrimitives.ReadUInt16LittleEndian(Take(2));
        public int ReadInt32() => BinaryPrimitives.ReadInt32LittleEndian(Take(4));
        public uint ReadUInt32() => BinaryPrimitives.ReadUInt32LittleEndian(Take(4));
        public long ReadInt64() => BinaryPrimitives.ReadInt64LittleEndian(Take(8));
        public ulong ReadUInt64() => BinaryPrimitives.ReadUInt64LittleEndian(Take(8));
        public float ReadSingle() => BinaryPrimitives.ReadSingleLittleEndian(Take(4));
        public double ReadDouble() => BinaryPrimitives.ReadDoubleLittleEndian(Take(8));
        public decimal ReadDecimal()
        {
            Span<int> bits = stackalloc int[4];
            bits[0] = ReadInt32(); bits[1] = ReadInt32(); bits[2] = ReadInt32(); bits[3] = ReadInt32();
            return new decimal(bits);
        }
        public RecordMetadata ReadMetadata()
        {
            /* Layout su disco (RecordMetadata, [Sequential, Pack=1]): Status byte + Version i64
             * + TransactionId i64 + PreviousVersionOffset i64 = 25 byte. Letti a mano invece che
             * con MemoryMarshal.Read (non referenziato in questo progetto): nessuna assunzione
             * oltre l'ordine dei campi dichiarati nella struct. */
            if (_span.Length - _pos < 25) throw new EndOfStreamException();
            var m = new RecordMetadata
            {
                Status = _span[_pos],
                Version = BinaryPrimitives.ReadInt64LittleEndian(_span.Slice(_pos + 1, 8)),
                TransactionId = BinaryPrimitives.ReadInt64LittleEndian(_span.Slice(_pos + 9, 8)),
                PreviousVersionOffset = BinaryPrimitives.ReadInt64LittleEndian(_span.Slice(_pos + 17, 8)),
            };
            _pos += 25;
            return m;
        }
    }

    /// <summary>
    /// Serializza una struttura <see cref="RecordMetadata"/> nella sua rappresentazione binaria esatta (un array di byte),
    /// pronta per essere scritta su disco.
    /// </summary>
    /// <param name="metadata">L'istanza della struttura <see cref="RecordMetadata"/> contenente lo stato e la versione del record da serializzare.</param>
    /// <returns>
    /// Un array di byte la cui lunghezza e il cui layout corrispondono esattamente alla definizione <c>[StructLayout]</c>
    /// della struttura <see cref="RecordMetadata"/>.
    /// </returns>
    /// <remarks>
    /// Questo metodo è un wrapper che utilizza <see cref="MarshalHelper.WriteStructure{T}(Stream, T)"/> per produrre
    /// un array di byte contenente solo i metadati, facilitando l'uso in contesti dove è richiesto un `byte[]` per i metadati (es. WAL).
    /// </remarks>
    public static byte[] SerializeMetadata(RecordMetadata metadata)
    {
        using var stream = new MemoryStream();
        MarshalHelper.WriteStructure(stream, metadata);
        return stream.ToArray();
    }

    /// <summary>
    /// Legge solo la porzione di metadati (stato, versione, TransactionId e PreviousVersionOffset) da un buffer di record.
    /// Questa operazione è veloce e non richiede la deserializzazione dell'intero record.
    /// </summary>
    /// <param name="recordBuffer">Il buffer di byte contenente l'intero record.</param>
    /// <param name="fileEncoding">Parametro storico, non piu' usato (tenuto per compatibilita'
    /// di firma): la decodifica non passa piu' da BinaryReader. E1.5.2.</param>
    /// <returns>La struttura <see cref="RecordMetadata"/> letta dal buffer.</returns>
    public static RecordMetadata ReadMetadata(byte[] recordBuffer, Encoding fileEncoding)
    {
        // Zero allocazioni (era: MemoryStream + BinaryReader + Marshal byte[] + AllocHGlobal).
        var reader = new IsamSpanReader(recordBuffer);
        return reader.ReadMetadata();
    }

    /// <summary>
    /// Scrive i dati di una istanza UDT (<see cref="UdtInstance"/>) su uno stream binario,
    /// seguendo l'ordine e il formato definiti dallo schema ISAM. Gestisce anche lo storage off-row per JSON.
    /// </summary>
    /// <param name="writer">Il <see cref="BinaryWriter"/> su cui scrivere i dati serializzati.</param>
    /// <param name="schema">L'oggetto <see cref="Config.IsamSchema"/> che definisce i campi, i loro tipi e le loro lunghezze.</param>
    /// <param name="udtInstance">L'istanza UDT i cui valori dei campi devono essere scritti.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se un campo definito nello schema non viene trovato nel dizionario dei campi dell'istanza UDT,
    /// indicando un disallineamento tra lo schema e i dati.
    /// </exception>
    private static void WriteUdtData(BinaryWriter writer, Config.IsamSchema schema, UdtInstance udtInstance,
        IsamFileHandle fileHandle)
    {
        // 1. Crea una mappa di lookup una-tantum per tradurre i nomi dei campi in SymbolId.
        var fieldIdMap = udtInstance.Definition.Fields.ToDictionary(
            f => f.Name.Lexeme,
            f => f.SymbolId,
            StringComparer.OrdinalIgnoreCase);
        foreach (var field in schema.Fields)
        {
            // Usa la mappa per trovare il SymbolId corrispondente al nome del campo.
            if (!fieldIdMap.TryGetValue(field.Name, out var symbolId))
                throw new InvalidOperationException(
                    $"Disallineamento schema: Campo '{field.Name}' definito nello schema ISAM ma non trovato nel TYPE '{udtInstance.Definition.OriginalName.Lexeme}'.");

            // Usa il SymbolId per recuperare il valore dall'istanza UDT.
            if (!udtInstance.Fields.TryGetValue(symbolId, out var value))
                throw new InvalidOperationException(
                    $"Dato mancante: Valore per il campo '{field.Name}' (ID: {symbolId}) non trovato nell'istanza UDT.");

            WriteField(writer, value, field.DataType, field.Length, fileHandle);
        }
    }

    /// <summary>
    /// Legge i dati di un record da uno stream binario e li deserializza in una nuova istanza UDT
    /// (<see cref="UdtInstance"/>), seguendo l'ordine e il formato definiti dallo schema ISAM.
    /// Gestisce anche il recupero di dati off-row per JSON.
    /// </summary>
    /// <param name="reader">Il lettore posizionale su span da cui leggere i dati grezzi
    /// del record (E1.5.2: niente BinaryReader per record).</param>
    /// <param name="schema">L'oggetto <see cref="Config.IsamSchema"/> che definisce i campi, i loro tipi e le loro lunghezze.</param>
    /// <param name="udtDefinition">La definizione del tipo (<see cref="UserDefinedType"/>) necessaria per costruire
    /// l'istanza UDT risultante.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
    /// <returns>Una nuova istanza di <see cref="UdtInstance"/> popolata con i dati letti dallo stream.</returns>
    /// <exception cref="System.Text.Json.JsonException">Se il JSON letto dal LOB Heap non è valido.</exception>
    /* Mappa Nome -> SymbolId per UDT, costruita UNA VOLTA per UDT invece che a ogni record
     * (E1.5.1). Prima di questo cambiamento ReadUdtData ricostruiva la mappa con ToDictionary
     * + un dizionario string-keyed intermedio a OGNI lettura di record. ConditionalWeakTable:
     * thread-safe senza lock da gestire, e senza leak (chiave debole: il valore muore con
     * l'UDT). Comportamento identico: se l'UDT avesse Lexeme duplicati, ToDictionary
     * lancerebbe comunque — prima a ogni record, ora al primo (la factory che lancia non
     * mette nulla in cache, quindi il record successivo rilancia come prima). Misurato (E1b):
     * -35% alloc per GetCurrentRecord (1232,3 -> 801,8 byte/record su 400 letture). */
    private static readonly ConditionalWeakTable<UserDefinedType, Dictionary<string, int>> s_nameToSymbolId = new();

    private static UdtInstance ReadUdtData(ref IsamSpanReader reader, Config.IsamSchema schema,
        UserDefinedType udtDefinition, IsamFileHandle fileHandle)
    {
        // 1. Leggi i campi DIRETTAMENTE nel dizionario int-keyed: niente dizionario
        // string-keyed intermedio, niente ToDictionary per record. Lo stream avanza
        // esattamente come prima (la mappa non tocca lo stream), quindi la posizione dopo
        // il ciclo e' identica; in caso di eccezione in ReadField, entrambi i dizionari
        // parziali vengono comunque scartati con la propagazione.
        var nameToId = s_nameToSymbolId.GetValue(udtDefinition, udt => udt.Fields.ToDictionary(
            f => f.Name.Lexeme,
            f => f.SymbolId,
            StringComparer.OrdinalIgnoreCase));

        var intKeyedFields = new Dictionary<int, object?>();
        foreach (var field in schema.Fields)
        {
            var value = ReadField(ref reader, field.DataType, field.Length, fileHandle);
            if (nameToId.TryGetValue(field.Name, out var symbolId))
            {
                intKeyedFields[symbolId] = value;
            }
            // Se un campo letto dal file non esiste nell'UDT, viene semplicemente ignorato.
        }

        // 2. Crea l'UdtInstance con il dizionario corretto.
        return new UdtInstance(udtDefinition, intKeyedFields);
    }

    /// <summary>
    /// Scrive un singolo valore su uno stream binario, convertendolo nel formato corretto in base al suo tipo di dato
    /// e alla lunghezza definita dallo schema. Gestisce lo storage off-row per i tipi JSON.
    /// </summary>
    /// <param name="writer">Il <see cref="BinaryWriter"/> su cui scrivere i dati serializzati.</param>
    /// <param name="value">Il valore dell'oggetto da scrivere.</param>
    /// <param name="dataType">Il <see cref="DataType"/> del campo, che determina il formato di serializzazione.</param>
    /// <param name="length">La lunghezza fissa in byte del campo, usata principalmente per le stringhe.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
    internal static void WriteField(BinaryWriter writer, object? value, DataType dataType, int length,
        IsamFileHandle fileHandle)
    {
        var coercedValue = CoerceValue(value, dataType);

        switch (dataType)
        {
            case StringType:
                var str = (string)coercedValue;
                var buffer = new byte[length];
                Array.Fill(buffer, (byte)' ');
                if (!string.IsNullOrEmpty(str))
                {
                    var encoded = fileHandle.FileEncoding.GetBytes(str);
                    Buffer.BlockCopy(encoded, 0, buffer, 0, Math.Min(encoded.Length, length));
                }

                writer.Write(buffer);
                break;

            case BitType: writer.Write((bool)coercedValue); break;
            case ByteType: writer.Write((byte)coercedValue); break;
            case SByteType: writer.Write((sbyte)coercedValue); break;
            case IntegerType: writer.Write((short)coercedValue); break;
            case UIntegerType: writer.Write((ushort)coercedValue); break;
            case LongType: writer.Write((int)coercedValue); break;
            case ULongType: writer.Write((uint)coercedValue); break;
            case QuadType: writer.Write((long)coercedValue); break;
            case UQuadType: writer.Write((ulong)coercedValue); break;
            case SingleType: writer.Write((float)coercedValue); break;
            case DoubleType: writer.Write((double)coercedValue); break;
            case CurrencyType: writer.Write((decimal)coercedValue); break;
            case DateType: writer.Write(((DateOnly)coercedValue).ToDateTime(TimeOnly.MinValue).Ticks); break;
            case TimeType: writer.Write(((TimeOnly)coercedValue).Ticks); break;
            case DateTimeType: writer.Write(((DateTime)coercedValue).Ticks); break;
            case TimeSpanType: writer.Write(((TimeSpan)coercedValue).Ticks); break;

            case JsonType:
                // Tratta il valore come una stringa. La conversione da JsonWrapper a stringa
                // è ora responsabilità dell'Executor.
                var jsonString = (string)CoerceValue(value, new StringType());
                var jsonBytes = fileHandle.FileEncoding.GetBytes(jsonString);
                var lobOffset = fileHandle.WriteLob(jsonBytes);
                writer.Write(lobOffset); // Scrive il puntatore.
                break;

            default:
                // Per tipi non supportati direttamente o sconosciuti, scrive un blocco di byte vuoti se la lunghezza è > 0.
                if (length > 0) writer.Write(new byte[length]);
                break;
        }
    }

    /// <summary>
    /// Legge un singolo valore da uno stream binario, interpretando i byte in base al tipo di dato
    /// e alla lunghezza specificati dallo schema. Gestisce il recupero di dati off-row per i tipi JSON.
    /// </summary>
    /// <param name="reader">Il lettore posizionale su span da cui leggere i dati grezzi
    /// (E1.5.2: niente BinaryReader, niente byte[] intermedio per le stringhe —
    /// Encoding.GetString accetta direttamente lo span).</param>
    /// <param name="dataType">Il <see cref="DataType"/> del campo, che determina come interpretare i byte.</param>
    /// <param name="length">La lunghezza fissa in byte del campo, usata per i tipi a lunghezza fissa come le stringhe.</param>
    /// <param name="fileHandle">L'handle del file ISAM, necessario per le operazioni sul LOB Heap per JSON.</param>
    /// <returns>L'oggetto .NET deserializzato.</returns>
    /// <exception cref="System.Text.Json.JsonException">Se il JSON letto dal LOB Heap non è valido.</exception>
    internal static object ReadField(ref IsamSpanReader reader, DataType dataType, int length, IsamFileHandle fileHandle)
    {
        return dataType switch
        {
            StringType => fileHandle.FileEncoding.GetString(reader.ReadBytes(length)).TrimEnd('\0', ' '),
            BitType => reader.ReadBoolean(),
            ByteType => reader.ReadByte(),
            SByteType => reader.ReadSByte(),
            IntegerType => reader.ReadInt16(),
            UIntegerType => reader.ReadUInt16(),
            LongType => reader.ReadInt32(),
            ULongType => reader.ReadUInt32(),
            QuadType => reader.ReadInt64(),
            UQuadType => reader.ReadUInt64(),
            SingleType => reader.ReadSingle(),
            DoubleType => reader.ReadDouble(),
            CurrencyType => reader.ReadDecimal(),
            DateType => DateOnly.FromDateTime(new DateTime(reader.ReadInt64())),
            TimeType => new TimeOnly(reader.ReadInt64()),
            DateTimeType => new DateTime(reader.ReadInt64()),
            TimeSpanType => new TimeSpan(reader.ReadInt64()),

            // Per JsonType, legge il puntatore, recupera i byte JSON dal LOB Heap e li parsifica.
            JsonType =>
                // Legge il puntatore, recupera i byte dal LOB Heap e restituisce una stringa.
                // La conversione in JsonWrapper è responsabilità dell'Executor.
                fileHandle.FileEncoding.GetString(fileHandle.ReadLob(reader.ReadInt64())),

            _ => GetDefaultValue(dataType)
        };
    }

    /// <summary>
    /// Restituisce il valore di default per un dato tipo di dato.
    /// </summary>
    /// <param name="dataType">Il tipo di dato per cui recuperare il valore di default.</param>
    /// <returns>Un oggetto che rappresenta il valore di default per il tipo specificato.</returns>
    private static object GetDefaultValue(DataType dataType)
    {
        return dataType switch
        {
            StringType { FixedLength: null } => string.Empty,
            StringType { FixedLength: { } len } => new string('\0', len),
            BitType => false,
            ByteType => (byte)0,
            SByteType => (sbyte)0,
            IntegerType => (short)0,
            UIntegerType => (ushort)0,
            LongType => 0,
            ULongType => 0U,
            QuadType => 0L,
            UQuadType => 0UL,
            SingleType => 0.0f,
            DoubleType => 0.0d,
            CurrencyType => 0.0m,
            DateType => DateOnly.MinValue,
            TimeType => TimeOnly.MinValue,
            DateTimeType => DateTime.MinValue,
            TimeSpanType => TimeSpan.Zero,
            JsonType => new JsonWrapper(new JsonObject()),
            _ => null! // Per i tipi non serializzabili o sconosciuti, restituiamo null.
        };
    }

    /// <summary>
    /// Converte un valore generico nel tipo .NET esatto richiesto per la serializzazione,
    /// gestendo conversioni numeriche, booleane e la coercizione a tipi ISAM.
    /// </summary>
    /// <param name="value">Il valore da convertire.</param>
    /// <param name="targetType">Il tipo di dato ISAM di destinazione.</param>
    /// <returns>Il valore convertito, tipizzato correttamente per il campo ISAM.</returns>
    /// <exception cref="InvalidOperationException">Se la conversione non è possibile o causa un overflow.</exception>
    private static object CoerceValue(object? value, DataType targetType)
    {
        // Gestisce la conversione standard dei booleani BASIC (-1/0) a bool .NET se il target non è BitType.
        if (value is bool b && targetType is not BitType)
            value = b ? -1 : 0;

        try
        {
            return targetType switch
            {
                StringType => Convert.ToString(value) ?? string.Empty,
                BitType => Convert.ToBoolean(value),
                ByteType => Convert.ToByte(value),
                SByteType => Convert.ToSByte(value),
                IntegerType => Convert.ToInt16(value),
                UIntegerType => Convert.ToUInt16(value),
                LongType => Convert.ToInt32(value),
                ULongType => Convert.ToUInt32(value),
                QuadType => Convert.ToInt64(value),
                UQuadType => Convert.ToUInt64(value),
                SingleType => Convert.ToSingle(value),
                DoubleType => Convert.ToDouble(value),
                CurrencyType => Convert.ToDecimal(value),
                // Un campo DATE/TIME/TIMESPAN mai assegnato esplicitamente (es. "UltimoOrdine
                // AS DATE" in un record dove solo alcuni campi vengono impostati prima di
                // ISAM APPEND) arriva qui col default BASIC generico per un campo non-stringa:
                // un double 0.0 (vedi DefaultValueForFieldType, PCodeVm/PCodeProcessor.Helpers.cs)
                // — MAI un DateTime/null. Il cast diretto "(DateOnly)(double)" non è definito in
                // C# e lanciava sempre InvalidCastException, che ISAM_APPEND inghiottiva in un
                // catch silenzioso: qualunque record con un campo data/ora non esplicitamente
                // valorizzato non veniva MAI scritto (0 record appesi, navigazione/GET
                // successivi vedevano un file vuoto — scoperto verificando DbIsamFullTests.bas
                // con TYPE Cliente che include "UltimoOrdine AS DATE"). Il double è trattato
                // come OADate (stessa convenzione di DateValue/GbDate in tutto il resto della
                // VM): 0.0 → 30/12/1899, il classico "zero date" di BASIC/OLE Automation.
                // Il fallback finale usa Convert.ToDateTime (via IConvertible), non un cast
                // diretto: DATETIME.DATEVALUE(...) e simili builtin di PCodeVm restituiscono un
                // wrapper interno (PCodeVm.DateValue, non visibile da questo progetto, Databases
                // sta SOTTO PCodeVm nel grafo di dipendenze) che implementa IConvertible.
                // ToDateTime() — un cast diretto "(DateOnly)value" su quell'oggetto lanciava
                // InvalidCastException (mai un vero DateOnly), ISAM_APPEND la inghiottiva in un
                // catch silenzioso: qualunque record con un campo DATE valorizzato tramite
                // DATEVALUE/NOW/TODAY ecc. (non un raro caso limite: la normale via per
                // assegnare una data) non veniva MAI scritto.
                DateType => value switch
                {
                    DateTime dt => DateOnly.FromDateTime(dt),
                    DateOnly d => d,
                    double oaDate => DateOnly.FromDateTime(DateTime.FromOADate(oaDate)),
                    null => DateOnly.MinValue,
                    _ => DateOnly.FromDateTime(Convert.ToDateTime(value))
                },
                TimeType => value switch
                {
                    DateTime dtt => TimeOnly.FromDateTime(dtt),
                    TimeOnly t => t,
                    double oaTime => TimeOnly.FromDateTime(DateTime.FromOADate(oaTime)),
                    null => TimeOnly.MinValue,
                    _ => TimeOnly.FromDateTime(Convert.ToDateTime(value))
                },
                TimeSpanType => value switch
                {
                    DateTime dts => dts.TimeOfDay,
                    TimeSpan ts => ts,
                    double oaSpan => TimeSpan.FromDays(oaSpan),
                    null => TimeSpan.Zero,
                    _ => Convert.ToDateTime(value).TimeOfDay
                },
                // Stesso ragionamento di DateType/TimeType sopra: Convert.ToDateTime non
                // accetta un double (OADate) — serve FromOADate esplicito.
                DateTimeType => value switch
                {
                    DateTime dtv => dtv,
                    double oaDt => DateTime.FromOADate(oaDt),
                    null => DateTime.MinValue,
                    _ => Convert.ToDateTime(value)
                },
                JsonType when value is JsonWrapper jw => jw,
                JsonType when value is string s => new JsonWrapper(
                    JsonNode.Parse(s) ?? new JsonObject()),
                // Se il tipo di destinazione non è riconosciuto, lancia un'eccezione.
                _ => throw new InvalidCastException()
            };
        }
        catch (Exception ex) when (ex is FormatException or InvalidCastException or OverflowException)
        {
            throw new InvalidOperationException(
                $"Impossibile convertire il valore '{value}' nel tipo ISAM '{targetType}'. Dettagli: {ex.Message}", ex);
        }
    }
}