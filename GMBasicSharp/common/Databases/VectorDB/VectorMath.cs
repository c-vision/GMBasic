// FILE: Basic.SQLite.Vector.VectorMath
using System.Buffers.Binary;
using System.Runtime.InteropServices;

namespace Basic.SQLite.Vector;

/// <summary>Float32 vector math for brute-force search.</summary>
public static class VectorMath
{
    /// <summary>Encodes floats as little-endian float32 BLOB.</summary>
    public static byte[] ToBlob(ReadOnlySpan<float> vector)
    {
        var bytes = new byte[vector.Length * sizeof(float)];
        for (var i = 0; i < vector.Length; i++)
            BinaryPrimitives.WriteSingleLittleEndian(bytes.AsSpan(i * sizeof(float)), vector[i]);
        return bytes;
    }

    /// <summary>Decodes little-endian float32 BLOB.</summary>
    public static float[] FromBlob(ReadOnlySpan<byte> blob)
    {
        if (blob.Length % sizeof(float) != 0)
            throw new ArgumentException("Embedding BLOB length must be a multiple of 4.", nameof(blob));

        var count = blob.Length / sizeof(float);
        var vector = new float[count];
        for (var i = 0; i < count; i++)
            vector[i] = BinaryPrimitives.ReadSingleLittleEndian(blob.Slice(i * sizeof(float)));
        return vector;
    }

    /// <summary>Ranking score — higher is better for both metrics.</summary>
    public static float Score(VectorMetric metric, ReadOnlySpan<float> a, ReadOnlySpan<float> b)
    {
        if (a.Length != b.Length)
            throw new ArgumentException($"Dimension mismatch: {a.Length} vs {b.Length}.");

        return metric switch
        {
            VectorMetric.Cosine => CosineSimilarity(a, b),
            VectorMetric.L2 => -L2Distance(a, b),
            _ => throw new ArgumentOutOfRangeException(nameof(metric))
        };
    }

    public static float CosineSimilarity(ReadOnlySpan<float> a, ReadOnlySpan<float> b)
    {
        var dot = 0.0;
        var na = 0.0;
        var nb = 0.0;
        for (var i = 0; i < a.Length; i++)
        {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }

        if (na == 0 || nb == 0) return 0f;
        return (float)(dot / (Math.Sqrt(na) * Math.Sqrt(nb)));
    }

    public static float L2Distance(ReadOnlySpan<float> a, ReadOnlySpan<float> b)
    {
        var sum = 0.0;
        for (var i = 0; i < a.Length; i++)
        {
            var d = a[i] - b[i];
            sum += d * d;
        }

        return (float)Math.Sqrt(sum);
    }

    public static void Normalize(Span<float> vector)
    {
        var norm = 0.0;
        for (var i = 0; i < vector.Length; i++)
            norm += vector[i] * vector[i];
        if (norm == 0) return;
        var inv = 1.0 / Math.Sqrt(norm);
        for (var i = 0; i < vector.Length; i++)
            vector[i] = (float)(vector[i] * inv);
    }

    // ==== SQ8 (unica semantica con gmb_vecquant.c §0bis: scala simmetrica max/127,
    // arrotondamento half-away, clamp [-127,127]; scoring SEMPRE nel dominio int8) ====

    /// <summary>Layout blob SQ8: [scale:float32 LE][n int8] — len = 4+n (mai uguale a 4*n).</summary>
    public static byte[] ToSq8Blob(ReadOnlySpan<float> vector)
    {
        var (q, scale) = Sq8Encode(vector);
        var bytes = new byte[4 + vector.Length];
        BinaryPrimitives.WriteSingleLittleEndian(bytes.AsSpan(0, 4), scale);
        for (var i = 0; i < q.Length; i++)
            bytes[4 + i] = (byte)q[i];
        return bytes;
    }

    /// <summary>Rileva layout SQ8 per lunghezza (DB vecchi senza colonna format restano FP32).</summary>
    public static bool IsSq8Blob(ReadOnlySpan<byte> blob, int dims) =>
        dims > 0 && blob.Length == 4 + dims;

    /// <summary>Quantizza una query una volta sola (il loop di scoring resta int8).</summary>
    public static (sbyte[] Q, float Scale) Sq8EncodeQuery(ReadOnlySpan<float> vector) =>
        Sq8Encode(vector);

    public static (sbyte[] Q, float Scale) Sq8Encode(ReadOnlySpan<float> vector)
    {
        var m = 0f;
        for (var i = 0; i < vector.Length; i++)
        {
            var a = Math.Abs(vector[i]);
            if (a > m) m = a;
        }
        var scale = m == 0f ? 0f : m / 127f;
        var q = new sbyte[vector.Length];
        if (scale != 0f)
            for (var i = 0; i < vector.Length; i++)
            {
                var v = vector[i] / scale;
                var r = (int)(v + (v >= 0f ? 0.5f : -0.5f));
                q[i] = (sbyte)Math.Max(-127, Math.Min(127, r));
            }
        return (q, scale);
    }

    public static int Sq8Dot(ReadOnlySpan<sbyte> a, ReadOnlySpan<sbyte> b)
    {
        var d = 0;
        for (var i = 0; i < a.Length; i++)
            d += a[i] * b[i];
        return d;
    }

    /// <summary>Coseno nel dominio int8 (le scale si elidono nel rapporto, come nel C).</summary>
    public static float Sq8Cosine(ReadOnlySpan<sbyte> qa, ReadOnlySpan<sbyte> qb)
    {
        double dot = 0, na = 0, nb = 0;
        for (var i = 0; i < qa.Length; i++)
        {
            dot += (double)qa[i] * qb[i];
            na += (double)qa[i] * qa[i];
            nb += (double)qb[i] * qb[i];
        }
        if (na == 0 || nb == 0) return 0f;
        return (float)(dot / (Math.Sqrt(na) * Math.Sqrt(nb)));
    }

    public static float Sq8L2(ReadOnlySpan<sbyte> qa, float sa, ReadOnlySpan<sbyte> qb, float sb)
    {
        double sum = 0;
        for (var i = 0; i < qa.Length; i++)
        {
            var d = (double)qa[i] * sa - (double)qb[i] * sb;
            sum += d * d;
        }
        return (float)Math.Sqrt(sum);
    }

    /// <summary>Legge scala+payload da blob SQ8 (il chiamante ha verificato IsSq8Blob).</summary>
    public static (sbyte[] Q, float Scale) Sq8SplitBlob(ReadOnlySpan<byte> blob, int dims)
    {
        var scale = BinaryPrimitives.ReadSingleLittleEndian(blob[..4]);
        var q = new sbyte[dims];
        for (var i = 0; i < dims; i++)
            q[i] = (sbyte)blob[4 + i];
        return (q, scale);
    }

    /// <summary>Decodes BLOB in entrambi i layout (FP32 legacy o SQ8) — per HNSW
    /// (navigazione resta FP32) e compatibilità mista. Lo scoring usa ScoreSq8.</summary>
    public static float[] FromBlobAny(ReadOnlySpan<byte> blob, int dims)
    {
        if (IsSq8Blob(blob, dims))
        {
            var (q, scale) = Sq8SplitBlob(blob, dims);
            var v = new float[dims];
            for (var i = 0; i < dims; i++)
                v[i] = q[i] * scale;
            return v;
        }
        return FromBlob(blob);
    }

    /// <summary>Score con query pre-quantizzata su blob SQ8 (ranking int8-domain).</summary>
    public static float ScoreSq8(VectorMetric metric, ReadOnlySpan<sbyte> queryQ, float queryScale, ReadOnlySpan<byte> blob, int dims)
    {
        var (bq, bs) = Sq8SplitBlob(blob, dims);
        return metric switch
        {
            // Le scale si elidono nel coseno (come in gmb_sq8_cosine).
            VectorMetric.Cosine => Sq8Cosine(queryQ, bq),
            VectorMetric.L2 => Sq8L2(queryQ, queryScale, bq, bs),
            _ => throw new ArgumentOutOfRangeException(nameof(metric))
        };
    }
}
