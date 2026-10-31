// FILE: CommonLib.RandomRecordCodec.cs
// Codec dei RECORD di un file RANDOM (PUT/GET #n, rec, <UDT>) per i backend gestiti (IL, PCodeVM).
// Gemello byte-per-byte di gmb_file_put_record / gmb_file_get_record (native/gmb_runtime_c/src/gmb_file.c),
// usato da VM/C, C99Native: i file prodotti da un backend sono leggibili da tutti gli altri.
//
// LAYOUT CANONICO (stesso canone di SIZEOF/PACKED TYPE, common/Ast/TypeLayout.cs): campi in ordine
// di dichiarazione, SENZA padding, little-endian. Un token per campo:
//   t BIT (1 byte, 0/1; riletto -1/0)   y BYTE (u8)   Y SBYTE (i8)   i INTEGER (i16)   I UINT (u16)
//   f SINGLE (f32)   l LONG/QUAD/ULONG/UQUAD (i64)   d DOUBLE (f64)
//   c CURRENCY (16 byte: int64 valore*10000 arrotondato + 8 byte a zero)
//   sN STRING * N (N byte: testo UTF-8 troncato/riempito con spazi, poi 1 terminatore NUL)
// Il record occupa LEN byte (OPEN … LEN = n, default 128); layout > LEN = errore 59 "Bad record length";
// l'avanzo e' riempito di zeri. PUT #n, rec → offset (rec-1)*LEN; rec assente (<= 0) = posizione
// corrente. GET oltre la fine del file restituisce zeri / stringhe vuote.
using System.Runtime.CompilerServices;
using System.Text;

namespace CommonLib;

public static class RandomRecordCodec
{
    public const int DefaultRecordLength = 128;

    /// <summary>Errore BASIC 59 "Bad record length": il layout dell'UDT supera LEN.</summary>
    public const int BadRecordLengthError = 59;
    public const string BadRecordLengthMessage = "Bad record length";

    private static readonly ConditionalWeakTable<Stream, StrongBox<int>> RecordLengths = new();

    /// <summary>Marca lo stream come file RANDOM con record da <paramref name="recordLength"/> byte
    /// (&lt;= 0 → default 128). Chiamato da OPEN … FOR RANDOM.</summary>
    public static void MarkRandom(Stream stream, long recordLength)
    {
        var len = recordLength > 0 ? (int)Math.Min(recordLength, int.MaxValue) : DefaultRecordLength;
        RecordLengths.AddOrUpdate(stream, new StrongBox<int>(len));
    }

    public static bool IsRandom(Stream? stream) => stream is not null && RecordLengths.TryGetValue(stream, out _);

    /// <summary>Scompone il descrittore "nome:token;nome:token" in nomi e token concatenati.</summary>
    public static (string[] Names, string Tokens) ParseDescriptor(string descriptor)
    {
        var names = new List<string>();
        var tokens = new StringBuilder();
        foreach (var part in descriptor.Split(';', StringSplitOptions.RemoveEmptyEntries))
        {
            var colon = part.LastIndexOf(':');
            if (colon <= 0) continue;
            names.Add(part[..colon]);
            tokens.Append(part[(colon + 1)..]);
        }
        return (names.ToArray(), tokens.ToString());
    }

    private static int NextToken(string tokens, ref int pos, out char kind, out int n)
    {
        kind = tokens[pos++];
        n = 0;
        switch (kind)
        {
            case 't': case 'y': case 'Y': return 1;
            case 'i': case 'I': return 2;
            case 'f': return 4;
            case 'l': case 'd': return 8;
            case 'c': return 16;
            case 's':
                while (pos < tokens.Length && tokens[pos] is >= '0' and <= '9') n = n * 10 + (tokens[pos++] - '0');
                return n + 1;
            default: return 0;
        }
    }

    /// <summary>Larghezza in byte del layout descritto da <paramref name="tokens"/>.</summary>
    public static int RecordSize(string tokens)
    {
        var total = 0;
        var pos = 0;
        while (pos < tokens.Length)
        {
            var w = NextToken(tokens, ref pos, out _, out _);
            if (w == 0) break;
            total += w;
        }
        return total;
    }

    private static double ToDouble(object? v) => v switch
    {
        null => 0.0,
        double d => d,
        bool b => b ? -1.0 : 0.0,
        string s => double.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var r) ? r : 0.0,
        _ => Convert.ToDouble(v, System.Globalization.CultureInfo.InvariantCulture),
    };

    private static void PutLe(byte[] buf, int off, ulong value, int bytes)
    {
        for (var i = 0; i < bytes; i++) buf[off + i] = (byte)((value >> (8 * i)) & 0xFF);
    }

    private static ulong GetLe(byte[] buf, int off, int bytes)
    {
        ulong v = 0;
        for (var i = 0; i < bytes; i++) v |= (ulong)buf[off + i] << (8 * i);
        return v;
    }

    /// <summary>Codifica i valori nel buffer di un record da <paramref name="recordLength"/> byte.</summary>
    public static byte[] Encode(string tokens, IReadOnlyList<object?> values, int recordLength)
    {
        var buf = new byte[recordLength];
        var off = 0;
        var pos = 0;
        var idx = 0;
        while (pos < tokens.Length)
        {
            var w = NextToken(tokens, ref pos, out var kind, out var n);
            if (w == 0) break;
            var v = idx < values.Count ? values[idx] : null;
            idx++;
            switch (kind)
            {
                case 't': buf[off] = (byte)(ToDouble(v) != 0.0 ? 1 : 0); break;
                case 'y': case 'Y': buf[off] = (byte)((long)ToDouble(v) & 0xFF); break;
                case 'i': case 'I': PutLe(buf, off, (ulong)(long)ToDouble(v), 2); break;
                case 'f': PutLe(buf, off, (uint)BitConverter.SingleToInt32Bits((float)ToDouble(v)), 4); break;
                case 'l': PutLe(buf, off, v is long lv ? (ulong)lv : (ulong)(long)ToDouble(v), 8); break;
                case 'd': PutLe(buf, off, (ulong)BitConverter.DoubleToInt64Bits(ToDouble(v)), 8); break;
                case 'c': PutLe(buf, off, (ulong)(long)Math.Round(ToDouble(v) * 10000.0, MidpointRounding.AwayFromZero), 8); break;
                case 's':
                {
                    var bytes = Encoding.UTF8.GetBytes(v?.ToString() ?? "");
                    var len = Math.Min(bytes.Length, n);
                    Array.Copy(bytes, 0, buf, off, len);
                    for (var k = len; k < n; k++) buf[off + k] = (byte)' ';
                    break;
                }
            }
            off += w;
        }
        return buf;
    }

    /// <summary>Decodifica un record: numeri come <c>double</c> (LONG come <c>long</c>), STRING * N come <c>string</c>.</summary>
    public static object?[] Decode(string tokens, byte[] rec)
    {
        var result = new List<object?>();
        var off = 0;
        var pos = 0;
        while (pos < tokens.Length)
        {
            var w = NextToken(tokens, ref pos, out var kind, out var n);
            if (w == 0) break;
            switch (kind)
            {
                case 't': result.Add(rec[off] != 0 ? -1.0 : 0.0); break;
                case 'y': result.Add((double)rec[off]); break;
                case 'Y': result.Add((double)(sbyte)rec[off]); break;
                case 'i': result.Add((double)(short)GetLe(rec, off, 2)); break;
                case 'I': result.Add((double)(ushort)GetLe(rec, off, 2)); break;
                case 'f': result.Add((double)BitConverter.Int32BitsToSingle((int)GetLe(rec, off, 4))); break;
                case 'l': result.Add((long)GetLe(rec, off, 8)); break;
                case 'd': result.Add(BitConverter.Int64BitsToDouble((long)GetLe(rec, off, 8))); break;
                case 'c': result.Add((double)(long)GetLe(rec, off, 8) / 10000.0); break;
                case 's':
                {
                    var len = 0;
                    while (len < n && rec[off + len] != 0) len++;
                    result.Add(Encoding.UTF8.GetString(rec, off, len).PadRight(n));
                    break;
                }
            }
            off += w;
        }
        return result.ToArray();
    }

    /// <summary>PUT: scrive il record. Ritorna 0 ok, -1 stream non RANDOM, -2 layout &gt; LEN.</summary>
    public static int Put(Stream stream, long record, string tokens, IReadOnlyList<object?> values)
    {
        if (!RecordLengths.TryGetValue(stream, out var box)) return -1;
        var recLen = box.Value;
        if (RecordSize(tokens) > recLen) return -2;
        var buf = Encode(tokens, values, recLen);
        if (record > 0) stream.Seek((record - 1) * recLen, SeekOrigin.Begin);
        stream.Write(buf, 0, buf.Length);
        stream.Flush();
        return 0;
    }

    /// <summary>GET: legge il record. Ritorna 0 ok, -1 stream non RANDOM, -2 layout &gt; LEN.</summary>
    public static int Get(Stream stream, long record, string tokens, out object?[] values)
    {
        values = Array.Empty<object?>();
        if (!RecordLengths.TryGetValue(stream, out var box)) return -1;
        var recLen = box.Value;
        if (RecordSize(tokens) > recLen) return -2;
        if (record > 0) stream.Seek((record - 1) * recLen, SeekOrigin.Begin);
        var buf = new byte[recLen];
        var got = 0;
        while (got < recLen)
        {
            var r = stream.Read(buf, got, recLen - got);
            if (r <= 0) break;
            got += r;
        }
        values = Decode(tokens, buf);
        return 0;
    }
}
