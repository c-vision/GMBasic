// FILE: Basic.Databases.Speed.DataTypeExtensions.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Fornisce metodi di estensione per la classe base DataType.
/// </summary>
internal static class DataTypeExtensions
{
    /// <summary>
    /// Restituisce il tipo CLR (.NET) sottostante per un dato DataType.
    /// </summary>
    /// <param name="dataType">Il tipo di dato del linguaggio da analizzare.</param>
    /// <returns>Il <see cref="System.Type"/> corrispondente.</returns>
    public static Type GetClrType(this DataType dataType) => dataType switch
    {
        StringType => typeof(string),
        BitType => typeof(bool),
        ByteType => typeof(byte),
        SByteType => typeof(sbyte),
        IntegerType => typeof(short),
        UIntegerType => typeof(ushort),
        LongType => typeof(int),
        ULongType => typeof(uint),
        QuadType => typeof(long),
        UQuadType => typeof(ulong),
        SingleType => typeof(float),
        DoubleType => typeof(double),
        CurrencyType => typeof(decimal),
        DateType => typeof(DateOnly),
        DateTimeType => typeof(DateTime),
        JsonType => typeof(string), // Il motore tratta il JSON come stringa internamente
        UserDefinedType => typeof(Dictionary<string, object?>),
        _ => typeof(object)
    };

    /// <summary>
    /// Determina se un DataType rappresenta un tipo numerico.
    /// </summary>
    /// <param name="dataType">Il tipo di dato da controllare.</param>
    /// <returns>True se il tipo è numerico, altrimenti False.</returns>
    public static bool IsNumeric(this DataType dataType) => dataType is
        ByteType or SByteType or IntegerType or UIntegerType or LongType or ULongType or
        QuadType or UQuadType or SingleType or DoubleType or CurrencyType;
}
