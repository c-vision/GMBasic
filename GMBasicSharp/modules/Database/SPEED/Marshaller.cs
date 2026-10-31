// FILE: Basic.Databases.Speed.Marshaller.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Text;

namespace Basic.Databases.Speed;

/// <summary>
/// Fornisce metodi statici per serializzare e deserializzare i tipi di dato del linguaggio BASIC
/// nel formato binario richiesto dal motore di storage Speed (LSM-Tree).
/// Questa classe è completamente disaccoppiata dall'Executor.
/// </summary>
internal static class SpeedMarshaller
{
    /// <summary>
    /// Serializza un valore BASIC (primitivo o UDT rappresentato da un dizionario) in un array di byte.
    /// </summary>
    /// <param name="value">Il valore da serializzare.</param>
    /// <param name="dataType">Il tipo di dato del valore, che guida la serializzazione.</param>
    /// <param name="schema">Lo schema completo, necessario per serializzare i tipi UDT.</param>
    /// <returns>La rappresentazione binaria del valore.</returns>
    /// <exception cref="NotSupportedException">Se il tipo di dato non è serializzabile.</exception>
    public static byte[] ToBytes(object? value, DataType dataType, Config.SpeedSchema? schema = null)
    {
        if (value == null) return [];

        switch (dataType)
        {
            case StringType: return Encoding.UTF8.GetBytes(Convert.ToString(value) ?? string.Empty);
            case BitType: return BitConverter.GetBytes(Convert.ToBoolean(value));
            case ByteType: return [Convert.ToByte(value)];
            case SByteType: return [(byte)Convert.ToSByte(value)];
            case IntegerType: return BitConverter.GetBytes(Convert.ToInt16(value));
            case UIntegerType: return BitConverter.GetBytes(Convert.ToUInt16(value));
            // LONG/ULONG in questo backend sono a 64 bit (vedi DataTypeMapper: LongType →
            // System.Int64, ULongType → System.UInt64) — non 32 bit. Con ToInt32/UInt32 qui,
            // il round-trip PUT→GET su un campo LONG (es. `ID AS LONG`, la chiave primaria
            // tipica di ogni tabella SPEED/ISAM) produceva un valore boxato Int32 mentre lo
            // struct UDT generato ha un campo Int64 → InvalidCastException in
            // EmitCopyObjectArrayToTarget (Unbox_Any su Int64 con un box Int32 in mano).
            case LongType: return BitConverter.GetBytes(Convert.ToInt64(value));
            case ULongType: return BitConverter.GetBytes(Convert.ToUInt64(value));
            case QuadType: return BitConverter.GetBytes(Convert.ToInt64(value));
            case UQuadType: return BitConverter.GetBytes(Convert.ToUInt64(value));
            case SingleType: return BitConverter.GetBytes(Convert.ToSingle(value));
            case DoubleType: return BitConverter.GetBytes(Convert.ToDouble(value));
            case CurrencyType: 
                var bits = decimal.GetBits(Convert.ToDecimal(value));
                using (var ms = new MemoryStream(16))
                {
                    using var writer = new BinaryWriter(ms);
                    writer.Write(bits[0]);
                    writer.Write(bits[1]);
                    writer.Write(bits[2]);
                    writer.Write(bits[3]);
                    return ms.ToArray();
                }
            case DateType: return BitConverter.GetBytes(((DateOnly)value).ToDateTime(TimeOnly.MinValue).Ticks);
            case DateTimeType: return BitConverter.GetBytes(((DateTime)value).Ticks);
            case JsonType: return Encoding.UTF8.GetBytes(Convert.ToString(value) ?? "{}");

            case UserDefinedType:
                if (value is not Dictionary<string, object?> udtFields)
                    throw new ArgumentException("Il valore per un UDT deve essere un Dictionary<string, object?>.", nameof(value));
                if (schema is null)
                    throw new ArgumentNullException(nameof(schema), "Lo schema è necessario per serializzare un UDT.");
                return SerializeUdt(udtFields, schema);

            default:
                throw new NotSupportedException($"La serializzazione per il tipo '{dataType.GetType().Name}' non è supportata dal motore Speed.");
        }
    }
    
    // NUOVO OVERLOAD PER SEMPLIFICARE LA SERIALIZZAZIONE DI UDT
    public static byte[] ToBytes(IReadOnlyDictionary<string, object?> udtFields, Config.SpeedSchema schema)
    {
        return SerializeUdt(udtFields, schema);
    }
    

    /// <summary>
    /// Deserializza un array di byte in un valore BASIC.
    /// </summary>
    /// <param name="buffer">L'array di byte da deserializzare.</param>
    /// <param name="targetType">Il tipo di dato di destinazione.</param>
    /// <param name="schema">Lo schema completo, necessario per deserializzare i tipi UDT.</param>
    /// <returns>Il valore BASIC deserializzato (un tipo primitivo o un Dictionary per UDT).</returns>
    /// <exception cref="NotSupportedException">Se il tipo di dato non è deserializzabile.</exception>
    public static object? FromBytes(byte[] buffer, DataType targetType, Config.SpeedSchema? schema = null)
    {
        if (buffer.Length == 0) return GetDefaultValue(targetType);

        switch (targetType)
        {
            case StringType: return Encoding.UTF8.GetString(buffer);
            case BitType: return BitConverter.ToBoolean(buffer, 0);
            case ByteType: return buffer[0];
            case SByteType: return (sbyte)buffer[0];
            case IntegerType: return BitConverter.ToInt16(buffer, 0);
            case UIntegerType: return BitConverter.ToUInt16(buffer, 0);
            case LongType: return BitConverter.ToInt64(buffer, 0);
            case ULongType: return BitConverter.ToUInt64(buffer, 0);
            case QuadType: return BitConverter.ToInt64(buffer, 0);
            case UQuadType: return BitConverter.ToUInt64(buffer, 0);
            case SingleType: return BitConverter.ToSingle(buffer, 0);
            case DoubleType: return BitConverter.ToDouble(buffer, 0);
            case CurrencyType:
                var bits = new int[4];
                bits[0] = BitConverter.ToInt32(buffer, 0);
                bits[1] = BitConverter.ToInt32(buffer, 4);
                bits[2] = BitConverter.ToInt32(buffer, 8);
                bits[3] = BitConverter.ToInt32(buffer, 12);
                return new decimal(bits);
            case DateType: return DateOnly.FromDateTime(new DateTime(BitConverter.ToInt64(buffer, 0)));
            case DateTimeType: return new DateTime(BitConverter.ToInt64(buffer, 0));
            case JsonType: return Encoding.UTF8.GetString(buffer);

            case UserDefinedType:
                if (schema is null)
                    throw new ArgumentNullException(nameof(schema), "Lo schema è necessario per deserializzare un UDT.");
                return DeserializeUdt(buffer, schema);
                
            default:
                throw new NotSupportedException($"La deserializzazione per il tipo '{targetType.GetType().Name}' non è supportata dal motore Speed.");
        }
    }
    
    // NUOVO OVERLOAD PER SEMPLIFICARE LA DESERIALIZZAZIONE DI UDT
    public static object FromBytes(byte[] buffer, Config.SpeedSchema schema)
    {
        return DeserializeUdt(buffer, schema);
    }
    
    /// <summary>
    /// Restituisce il valore di default per un dato tipo di dato del linguaggio,
    /// che corrisponde allo stato "vuoto" di un campo.
    /// </summary>
    /// <param name="dataType">Il tipo di dato per cui recuperare il valore di default.</param>
    /// <returns>Un oggetto che rappresenta il valore di default per il tipo specificato.</returns>
    private static object? GetDefaultValue(DataType dataType)
    {
        // Se il tipo è definito, usa il GetClrType
        {
            var clrType = dataType.GetClrType();
            if (clrType.IsValueType)
            {
                return Activator.CreateInstance(clrType);
            }
        }

        return dataType switch
        {
            StringType => string.Empty,
            JsonType => "{}",
            UserDefinedType => new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase),
            _ => null
        };
    }

    /// <summary>
    /// Serializza i campi di un UDT (rappresentato come dizionario) in un array di byte compatto.
    /// </summary>
    /// <param name="udtFields">Il dizionario di coppie nome-campo/valore che rappresenta l'istanza UDT.</param>
    /// <param name="schema">Lo schema della tabella, che definisce l'ordine e il tipo di ogni campo da serializzare.</param>
    /// <returns>Un array di byte che rappresenta l'intero record UDT serializzato.</returns>
    private static byte[] SerializeUdt(IReadOnlyDictionary<string, object?> udtFields, Config.SpeedSchema schema)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.UTF8);

        foreach (var field in schema.Fields)
        {
            var value = udtFields.GetValueOrDefault(field.Name);
            WriteFieldValue(writer, value, field.DataType, schema);
        }
        return stream.ToArray();
    }

    /// <summary>
    /// Deserializza un array di byte in un dizionario che rappresenta i campi di un UDT.
    /// </summary>
    /// <param name="buffer">L'array di byte contenente il record UDT serializzato.</param>
    /// <param name="schema">Lo schema della tabella, essenziale per interpretare correttamente i dati nel buffer.</param>
    /// <returns>Un dizionario di coppie nome-campo/valore che rappresenta l'istanza UDT ricostruita.</returns>
    private static Dictionary<string, object?> DeserializeUdt(byte[] buffer, Config.SpeedSchema schema)
    {
        using var stream = new MemoryStream(buffer);
        using var reader = new BinaryReader(stream, Encoding.UTF8);
        
        var fields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in schema.Fields)
        {
            fields[field.Name] = ReadFieldValue(reader, field.DataType, schema);
        }
        return fields;
    }
    
    /// <summary>
    /// Scrive un singolo valore su un BinaryWriter, precedendolo con la sua lunghezza in byte.
    /// </summary>
    /// <param name="writer">Il BinaryWriter su cui scrivere i dati.</param>
    /// <param name="value">Il valore dell'oggetto da scrivere.</param>
    /// <param name="dataType">Il tipo di dato del valore, che guida la serializzazione.</param>
    /// <param name="schema">Lo schema completo della tabella, necessario per la serializzazione ricorsiva di UDT annidati.</param>
    private static void WriteFieldValue(BinaryWriter writer, object? value, DataType dataType, Config.SpeedSchema schema)
    {
        var bytes = ToBytes(value, dataType, schema);
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    /// <summary>
    /// Legge un singolo valore da un BinaryReader, aspettandosi il formato lunghezza + dati.
    /// </summary>
    /// <param name="reader">Il BinaryReader da cui leggere i dati.</param>
    /// <param name="dataType">Il tipo di dato di destinazione in cui deserializzare i byte.</param>
    /// <param name="schema">Lo schema completo della tabella, necessario per la deserializzazione ricorsiva di UDT annidati.</param>
    /// <returns>L'oggetto deserializzato, o il valore di default del tipo se la lunghezza letta è zero.</returns>
    private static object? ReadFieldValue(BinaryReader reader, DataType dataType, Config.SpeedSchema schema)
    {
        var length = reader.ReadInt32();
        if (length == 0) return GetDefaultValue(dataType);
        var bytes = reader.ReadBytes(length);
        return FromBytes(bytes, dataType, schema);
    }
}