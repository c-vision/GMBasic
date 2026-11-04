// FILE: CommonLib/Collections/HyperLogLog.cs
// HyperLogLog — stima della cardinalità (numero di elementi DISTINTI) con memoria fissa.
//
// Rispecchia ESATTAMENTE il runtime C (gmb_hll.c, gmb_hll.h): stesso payload canonico,
// stesso xxHash64, stessa formula di stima (HLL classico + linear counting), stessa
// serializzazione. Il MiniTest di parità (`minitests/collections/hll_parity.bas`)
// verifica che le 4 gambe stampino gli STESSI valori.
//
// Registri a 8 bit (0..q+1), m = 2^p (default p = 14 → 16 KiB), Count() arrotondato
// all'intero (la cardinalità è un intero: output deterministico e identico fra gambe).

using System;
using System.Collections.Generic;
using System.Text;

namespace CommonLib.Collections;

public sealed class HyperLogLog
{
    public const int DefaultPrecision = 14;
    public const int MinPrecision = 4;
    public const int MaxPrecision = 18;
    public const int HashId = 1;
    private const string SerialMagic = "GMBHLL1";
    private const int SerialHeaderLen = 9;

    private readonly int _p;
    private readonly int _q;
    private readonly int _m;
    private readonly byte[] _regs;
    private long _nnz;

    public HyperLogLog(int p = DefaultPrecision)
    {
        if (p < MinPrecision) p = MinPrecision;
        if (p > MaxPrecision) p = MaxPrecision;
        _p = p;
        _q = 64 - p;
        _m = 1 << p;
        _regs = new byte[_m];
        _nnz = 0;
    }

    public int Precision() => _p;

    public double Error() => 1.04 / Math.Sqrt(_m);

    public void Clear()
    {
        Array.Clear(_regs, 0, _regs.Length);
        _nnz = 0;
    }

    public void Add(object? value) => AddHash(HashVal(value));

    public void AddHash(ulong hash)
    {
        var idx = (int)(hash >> (64 - _p));
        var w = hash << _p;
        int rho;
        if (w == 0) rho = _q + 1;
        else
        {
            rho = 1;
            while ((w & (1UL << 63)) == 0) { rho++; w <<= 1; }
        }
        if (rho > _q + 1) rho = _q + 1;
        if (rho > _regs[idx])
        {
            if (_regs[idx] == 0) _nnz++;
            _regs[idx] = (byte)rho;
        }
    }

    public double Count()
    {
        if (_nnz == 0) return 0.0;
        double sum = 0.0;
        int zeros = 0;
        for (var i = 0; i < _m; i++)
        {
            var r = _regs[i];
            if (r == 0) { zeros++; sum += 1.0; }
            else sum += 1.0 / (double)(1UL << r);
        }
        if (sum <= 0.0) return 0.0;
        var alpha = 0.7213 / (1.0 + 1.079 / (double)_m);
        var raw = alpha * (double)_m * (double)_m / sum;
        double est;
        if (raw <= 2.5 * (double)_m && zeros > 0)
        {
            est = (double)_m * Math.Log((double)_m / (double)zeros);
        }
        else
        {
            const double two64 = 18446744073709551616.0;
            est = raw;
            if (est > two64 / 30.0) est = -two64 * Math.Log(1.0 - est / two64);
        }
        if (est < 0.0) est = 0.0;
        return Math.Floor(est + 0.5);
    }

    public void Merge(HyperLogLog other)
    {
        if (other is null) return;
        if (other._p != _p)
            throw new ArgumentException(
                $"HLL.Merge: precisioni diverse ({_p} vs {other._p}). Servono sketch con lo stesso p.");
        for (var i = 0; i < _m; i++)
        {
            if (other._regs[i] > _regs[i])
            {
                if (_regs[i] == 0) _nnz++;
                _regs[i] = other._regs[i];
            }
        }
    }

    /// <summary>Serializzazione binaria (una 'char' per byte, Latin-1) — stesso formato del C.</summary>
    public string Serialize()
    {
        var sb = new StringBuilder(SerialHeaderLen + _m);
        sb.Append(SerialMagic);
        sb.Append((char)_p);
        sb.Append((char)HashId);
        foreach (var b in _regs) sb.Append((char)b);
        return sb.ToString();
    }

    /// <summary>Ricostruisce lo sketch da <see cref="Serialize"/> (formato non valido → throw).</summary>
    public void Load(string data)
    {
        if (data is null || data.Length < SerialHeaderLen)
            throw new ArgumentException("HLL.Load: dati troppo corti o non validi.");
        if (string.CompareOrdinal(data, 0, SerialMagic, 0, SerialMagic.Length) != 0)
            throw new ArgumentException("HLL.Load: magic non valido.");
        var p = (int)data[SerialMagic.Length];
        if (data[SerialMagic.Length + 1] != (char)HashId)
            throw new ArgumentException("HLL.Load: hash id diverso (formato incompatibile).");
        if (p < MinPrecision || p > MaxPrecision || p != _p)
            throw new ArgumentException($"HLL.Load: precisione {p} diversa da quella dello sketch ({_p}).");
        if (data.Length < SerialHeaderLen + _m)
            throw new ArgumentException("HLL.Load: registri mancanti.");
        for (var i = 0; i < _m; i++) _regs[i] = (byte)data[SerialHeaderLen + i];
        _nnz = 0;
        for (var i = 0; i < _m; i++) if (_regs[i] != 0) _nnz++;
    }

    // ---- Contratto di hashing (identico a gmb_hll.c) ----
    public static ulong HashVal(object? value)
    {
        switch (value)
        {
            case null:
                return HashBytes(new byte[] { 0x00 });
            case bool b:
                return HashBytes(new byte[] { 0x04, (byte)(b ? 1 : 0) });
            case string s:
            {
                var bytes = Encoding.UTF8.GetBytes(s);
                var buf = new byte[5 + bytes.Length];
                buf[0] = 0x03;
                WriteU32(buf, 1, (uint)bytes.Length);
                Buffer.BlockCopy(bytes, 0, buf, 5, bytes.Length);
                return HashBytes(buf);
            }
            case sbyte or byte or short or ushort or int or uint or long or ulong or float or double or decimal:
            {
                var d = Convert.ToDouble(value);
                var buf = new byte[9];
                if (d == Math.Floor(d) && Math.Abs(d) <= 9007199254740992.0)
                {
                    buf[0] = 0x01;
                    var iv = (long)d;
                    WriteI64(buf, 1, iv);
                }
                else
                {
                    buf[0] = 0x02;
                    WriteI64(buf, 1, BitConverter.DoubleToInt64Bits(d));
                }
                return HashBytes(buf);
            }
            default:
            {
                var json = System.Text.Json.JsonSerializer.Serialize(value);
                var bytes = Encoding.UTF8.GetBytes(json);
                var buf = new byte[5 + bytes.Length];
                buf[0] = 0x05;
                WriteU32(buf, 1, (uint)bytes.Length);
                Buffer.BlockCopy(bytes, 0, buf, 5, bytes.Length);
                return HashBytes(buf);
            }
        }
    }

    private static void WriteU32(byte[] buf, int off, uint v)
    {
        buf[off] = (byte)v;
        buf[off + 1] = (byte)(v >> 8);
        buf[off + 2] = (byte)(v >> 16);
        buf[off + 3] = (byte)(v >> 24);
    }

    private static void WriteI64(byte[] buf, int off, long v)
    {
        for (var i = 0; i < 8; i++) buf[off + i] = (byte)(v >> (8 * i));
    }

    public static ulong HashBytes(byte[] data)
    {
        const ulong P1 = 11400714785074694791UL;
        const ulong P2 = 14029467366897019727UL;
        const ulong P3 = 1609587929392839161UL;
        const ulong P4 = 9650029242287828579UL;
        const ulong P5 = 2870177450012600261UL;

        static ulong Rotl(ulong x, int r) => (x << r) | (x >> (64 - r));
        static ulong Read64(byte[] b, int o) => BitConverter.ToUInt64(b, o);
        static ulong Round(ulong acc, ulong input)
        {
            acc += input * P2;
            acc = Rotl(acc, 31);
            acc *= P1;
            return acc;
        }
        static ulong MergeAcc(ulong acc, ulong val)
        {
            val = Round(0, val);
            acc ^= val;
            acc = acc * P1 + P4;
            return acc;
        }

        var p = 0;
        var len = data.Length;
        var end = len;
        ulong h;
        if (len >= 32)
        {
            var v1 = unchecked(P1 + P2);
            var v2 = P2;
            var v3 = 0UL;
            var v4 = unchecked(0UL - P1);
            var limit = end - 32;
            do
            {
                v1 = Round(v1, Read64(data, p)); p += 8;
                v2 = Round(v2, Read64(data, p)); p += 8;
                v3 = Round(v3, Read64(data, p)); p += 8;
                v4 = Round(v4, Read64(data, p)); p += 8;
            } while (p <= limit);
            h = Rotl(v1, 1) + Rotl(v2, 7) + Rotl(v3, 12) + Rotl(v4, 18);
            h = MergeAcc(h, v1);
            h = MergeAcc(h, v2);
            h = MergeAcc(h, v3);
            h = MergeAcc(h, v4);
        }
        else h = P5;
        h += (ulong)len;

        while (p + 8 <= end)
        {
            var k1 = Round(0, Read64(data, p));
            h ^= k1;
            h = Rotl(h, 27) * P1 + P4;
            p += 8;
        }
        if (p + 4 <= end)
        {
            h ^= (ulong)BitConverter.ToUInt32(data, p) * P1;
            h = Rotl(h, 23) * P2 + P3;
            p += 4;
        }
        while (p < end)
        {
            h ^= (ulong)data[p] * P5;
            h = Rotl(h, 11) * P1;
            p++;
        }
        h ^= h >> 33;
        h *= P2;
        h ^= h >> 29;
        h *= P3;
        h ^= h >> 32;
        return h;
    }
}
