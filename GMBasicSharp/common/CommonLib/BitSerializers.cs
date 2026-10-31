// FILE: CommonLib.BitSerializers.cs

using System.Buffers.Binary;
using System.Runtime.InteropServices;
using System.Text;

/* 
 * BitSerializers.cs: serializzazione/deserializzazione little-endian ad alte prestazioni di tipi primitivi, stringhe e struct
 * 
 * FUNZIONI:
 * - ToUInt16/ToUInt32/ToUInt64/ToInt16/ToInt32/ToInt64/ToSingle/ToDouble(data, offset): da byte array a tipo
 * - ToString(data, offset, length, encoding): da byte array a stringa a lunghezza fissa
 * - ToLengthPrefixedString(data, offset, encoding): da byte array a stringa con prefisso lunghezza
 * - ToStruct<T>(data, offset): da byte array a struct unmanaged
 * - FromUInt16/FromUInt32/.../FromDouble(value): da tipo a byte array
 * - FromString(value, fixedLength, encoding): da stringa a byte array a lunghezza fissa
 * - FromLengthPrefixedString(value, encoding): da stringa a byte array con prefisso lunghezza
 * - FromStruct<T>(value): da struct unmanaged a byte array
 * - TryWriteBytes(destination, value): TryWrite per tipi primitivi su Span<byte>
 * - TryWriteString/TryWriteLengthPrefixedString: TryWrite per stringhe
 * - TryWriteBytes<T>(destination, value): TryWrite generico per struct
 */

namespace CommonLib;

/// <summary>
/// Fornisce metodi ad alte prestazioni per serializzare e deserializzare tipi di dato
/// da e verso sequenze di byte in ordine little-endian. Sfrutta le API ottimizzate
/// del runtime .NET (BinaryPrimitives, MemoryMarshal) per operazioni zero-copy.
/// Supporta tipi primitivi, stringhe (fixed-length e length-prefixed) e struct unmanaged.
/// </summary>
public static class BitSerializers
{
    // --- To (da byte a tipo) ---

    public static ushort ToUInt16(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadUInt16LittleEndian(data[offset..]);

    public static uint ToUInt32(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]);

    public static ulong ToUInt64(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadUInt64LittleEndian(data[offset..]);

    public static short ToInt16(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadInt16LittleEndian(data[offset..]);

    public static int ToInt32(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadInt32LittleEndian(data[offset..]);

    public static long ToInt64(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadInt64LittleEndian(data[offset..]);

    public static float ToSingle(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadSingleLittleEndian(data[offset..]);

    public static double ToDouble(ReadOnlySpan<byte> data, int offset = 0) =>
        BinaryPrimitives.ReadDoubleLittleEndian(data[offset..]);

    public static ushort ToUInt16(byte[] data, int offset = 0) => ToUInt16(data.AsSpan(), offset);
    public static uint ToUInt32(byte[] data, int offset = 0) => ToUInt32(data.AsSpan(), offset);
    public static ulong ToUInt64(byte[] data, int offset = 0) => ToUInt64(data.AsSpan(), offset);
    public static short ToInt16(byte[] data, int offset = 0) => ToInt16(data.AsSpan(), offset);
    public static int ToInt32(byte[] data, int offset = 0) => ToInt32(data.AsSpan(), offset);
    public static long ToInt64(byte[] data, int offset = 0) => ToInt64(data.AsSpan(), offset);
    public static float ToSingle(byte[] data, int offset = 0) => ToSingle(data.AsSpan(), offset);
    public static double ToDouble(byte[] data, int offset = 0) => ToDouble(data.AsSpan(), offset);

    public static string ToString(ReadOnlySpan<byte> data, int offset, int length, Encoding encoding) =>
        encoding.GetString(data.Slice(offset, length));

    public static string ToLengthPrefixedString(ReadOnlySpan<byte> data, int offset, Encoding encoding)
    {
        var length = ToInt32(data, offset);
        return ToString(data, offset + sizeof(int), length, encoding);
    }

    public static T ToStruct<T>(ReadOnlySpan<byte> data, int offset = 0) where T : unmanaged
        => MemoryMarshal.Read<T>(data[offset..]);

    // --- From (da tipo a byte) ---

    public static byte[] FromUInt16(ushort value) { var b = new byte[sizeof(ushort)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromUInt32(uint value) { var b = new byte[sizeof(uint)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromUInt64(ulong value) { var b = new byte[sizeof(ulong)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromInt16(short value) { var b = new byte[sizeof(short)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromInt32(int value) { var b = new byte[sizeof(int)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromInt64(long value) { var b = new byte[sizeof(long)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromSingle(float value) { var b = new byte[sizeof(float)]; TryWriteBytes(b, value); return b; }
    public static byte[] FromDouble(double value) { var b = new byte[sizeof(double)]; TryWriteBytes(b, value); return b; }

    public static byte[] FromString(string value, int fixedLength, Encoding encoding)
    {
        var buffer = new byte[fixedLength];
        encoding.GetBytes(value, buffer);
        return buffer;
    }

    public static byte[] FromLengthPrefixedString(string value, Encoding encoding)
    {
        var byteCount = encoding.GetByteCount(value);
        var buffer = new byte[sizeof(int) + byteCount];
        TryWriteBytes(buffer, byteCount);
        encoding.GetBytes(value, buffer.AsSpan(sizeof(int)));
        return buffer;
    }

    public static byte[] FromStruct<T>(in T value) where T : unmanaged
    {
        var buffer = new byte[Marshal.SizeOf<T>()];
        TryWriteBytes(buffer, in value);
        return buffer;
    }

    // --- TryWrite ---

    public static bool TryWriteBytes(Span<byte> destination, ushort value) =>
        BinaryPrimitives.TryWriteUInt16LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, uint value) =>
        BinaryPrimitives.TryWriteUInt32LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, ulong value) =>
        BinaryPrimitives.TryWriteUInt64LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, short value) =>
        BinaryPrimitives.TryWriteInt16LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, int value) =>
        BinaryPrimitives.TryWriteInt32LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, long value) =>
        BinaryPrimitives.TryWriteInt64LittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, float value) =>
        BinaryPrimitives.TryWriteSingleLittleEndian(destination, value);

    public static bool TryWriteBytes(Span<byte> destination, double value) =>
        BinaryPrimitives.TryWriteDoubleLittleEndian(destination, value);

    public static bool TryWriteString(Span<byte> destination, string value, Encoding encoding) =>
        encoding.GetBytes(value, destination) <= destination.Length;

    public static bool TryWriteLengthPrefixedString(Span<byte> destination, string value, Encoding encoding)
    {
        var byteCount = encoding.GetByteCount(value);
        if (destination.Length < sizeof(int) + byteCount) return false;
        TryWriteBytes(destination, byteCount);
        return TryWriteString(destination[sizeof(int)..], value, encoding);
    }

    public static bool TryWriteBytes<T>(Span<byte> destination, in T value) where T : unmanaged =>
        MemoryMarshal.TryWrite(destination, in value);
}
