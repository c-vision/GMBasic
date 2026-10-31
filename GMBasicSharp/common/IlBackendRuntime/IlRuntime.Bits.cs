// FILE: IlRuntime.Bits.cs — namespace BITS (manipolazione bit a 64 bit).
// Controparte gestita (§0bis: STESSA semantica del runtime C gmb_bits.c, che è la
// fonte unica per VM/C, C99Native). Le due implementazioni sono
// indipendenti (ambienti separati, nessun P/Invoke) ma devono restare ALLINEATE:
// indici 0-based, predicati → convenzione Bit (-1 vero / 0 falso), casi limite
// definiti (n fuori [0,63] non altera x; width clampato a 64; lo/hi scambiati se
// invertiti; scan su 0 → -1). Verificato dai MiniTest con oracolo su 4 backend.

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    private static bool BitsInRange(long n) => n >= 0 && n < 64;

    // ── Tier 1 ──────────────────────────────────────────────────────────────
    public static long BitCount(long x) => System.Numerics.BitOperations.PopCount(unchecked((ulong)x));

    public static long BitGet(long x, long n)
        => BitsInRange(n) ? (long)((unchecked((ulong)x) >> (int)n) & 1UL) : 0;

    public static long BitSet(long x, long n)
        => BitsInRange(n) ? unchecked((long)(unchecked((ulong)x) | (1UL << (int)n))) : x;

    public static long BitClear(long x, long n)
        => BitsInRange(n) ? unchecked((long)(unchecked((ulong)x) & ~(1UL << (int)n))) : x;

    public static long BitToggle(long x, long n)
        => BitsInRange(n) ? unchecked((long)(unchecked((ulong)x) ^ (1UL << (int)n))) : x;

    public static bool BitTest(long x, long n)
        => BitsInRange(n) && ((unchecked((ulong)x) >> (int)n) & 1UL) != 0;

    // ── Tier 2 ──────────────────────────────────────────────────────────────
    public static long BitField(long x, long lo, long hi)
    {
        if (lo < 0) lo = 0;
        if (hi > 63) hi = 63;
        if (lo > hi) (lo, hi) = (hi, lo);
        if (lo > hi || hi < 0 || lo > 63) return 0;
        int width = (int)(hi - lo + 1);
        ulong mask = width >= 64 ? ulong.MaxValue : (1UL << width) - 1UL;
        return unchecked((long)((unchecked((ulong)x) >> (int)lo) & mask));
    }

    public static long BitReverse(long x, long width)
    {
        if (width <= 0) return 0;
        if (width > 64) width = 64;
        ulong v = unchecked((ulong)x), r = 0;
        for (long i = 0; i < width; i++) { r = (r << 1) | (v & 1UL); v >>= 1; }
        return unchecked((long)r);
    }

    public static long BitRotateLeft(long x, long n, long width)
    {
        if (width <= 0) return 0;
        if (width > 64) width = 64;
        if (n < 0) n = 0;
        int w = (int)width;
        int k = (int)(n % width);
        ulong mask = w >= 64 ? ulong.MaxValue : (1UL << w) - 1UL;
        ulong v = unchecked((ulong)x) & mask;
        if (k == 0) return unchecked((long)v);
        return unchecked((long)(((v << k) | (v >> (w - k))) & mask));
    }

    public static long BitRotateRight(long x, long n, long width)
    {
        if (width <= 0) return 0;
        if (width > 64) width = 64;
        if (n < 0) n = 0;
        int w = (int)width;
        int k = (int)(n % width);
        ulong mask = w >= 64 ? ulong.MaxValue : (1UL << w) - 1UL;
        ulong v = unchecked((ulong)x) & mask;
        if (k == 0) return unchecked((long)v);
        return unchecked((long)(((v >> k) | (v << (w - k))) & mask));
    }

    public static long GrayCode(long x) => unchecked((long)(unchecked((ulong)x) ^ (unchecked((ulong)x) >> 1)));

    public static long FromGray(long x)
    {
        ulong v = unchecked((ulong)x);
        v ^= v >> 1; v ^= v >> 2; v ^= v >> 4; v ^= v >> 8; v ^= v >> 16; v ^= v >> 32;
        return unchecked((long)v);
    }

    public static long BitParity(long x) => BitCount(x) & 1;

    public static long BitScanFwd(long x)
    {
        ulong v = unchecked((ulong)x);
        if (v == 0) return -1;
        return System.Numerics.BitOperations.TrailingZeroCount(v);
    }

    public static long BitScanRev(long x)
    {
        ulong v = unchecked((ulong)x);
        if (v == 0) return -1;
        return 63 - System.Numerics.BitOperations.LeadingZeroCount(v);
    }

    public static long ByteSwap(long x) => unchecked((long)System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(unchecked((ulong)x)));

    // ── Tier 3 ──────────────────────────────────────────────────────────────
    public static long HammingDist(long a, long b) => BitCount(unchecked(a ^ b));

    public static long BitInterleave(long a, long b)
    {
        ulong ua = unchecked((ulong)a), ub = unchecked((ulong)b), r = 0;
        for (int i = 0; i < 32; i++)
        {
            r |= ((ua >> i) & 1UL) << (2 * i);
            r |= ((ub >> i) & 1UL) << (2 * i + 1);
        }
        return unchecked((long)r);
    }

    public static long BitDeinterleave(long m, long which)
    {
        ulong v = unchecked((ulong)m), r = 0;
        int off = which != 0 ? 1 : 0;
        for (int i = 0; i < 32; i++)
            r |= ((v >> (2 * i + off)) & 1UL) << i;
        return unchecked((long)r);
    }

    public static long NextPow2(long x)
    {
        if (x <= 1) return 1;
        ulong v = unchecked((ulong)(x - 1));
        v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v |= v >> 32;
        return unchecked((long)(v + 1));
    }

    public static bool IsPow2(long x)
    {
        if (x <= 0) return false;
        ulong v = unchecked((ulong)x);
        return (v & (v - 1)) == 0;
    }
}
