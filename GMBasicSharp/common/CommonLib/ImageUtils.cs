// FILE: CommonLib.ImageUtils.cs

/*
 * ImageUtils.cs: primitive immagini pure-managed (PNG-only) condivise dai backend C#.
 *
 * F3 multimodale (2026-09-11): IL/PCodeVM operano su queste (stessa semantica del runtime C
 * gmb_image.c: decode PNG, resize/crop nearest-neighbor, encode PNG). Solo PNG in decode
 * e encode — altri formati = errore esplicito, mai stub. Nessuna dipendenza nativa/grafica
 * (funziona headless in CI): inflate via DeflateStream, encode con wrapper zlib manuale
 * (stesso pattern di NativeDocPdf.WritePng).
 *
 * FUNZIONI:
 * - TryReadPng(path, w, h, rgba, error): decodifica PNG 8-bit (tipi 0/2/3/4/6, non interlacciato) → RGBA.
 * - TryWritePng(path, rgba, w, h, error): scrive PNG RGBA 8-bit.
 * - ResizeNearest(src, sw, sh, dw, dh): ridimensiona RGBA nearest-neighbor.
 * - TryCrop(src, sw, sh, x, y, w, h, dst, cw, ch): ritaglia con clamp (false se fuori del tutto).
 */

namespace CommonLib;

public static class ImageUtils
{
    public const int MaxDim = 16384;

    private static readonly byte[] PngSig = { 137, 80, 78, 71, 13, 10, 26, 10 };

    public static bool HasPngExt(string? path)
    {
        if (string.IsNullOrEmpty(path) || path.Length < 4) return false;
        return path.EndsWith(".png", StringComparison.OrdinalIgnoreCase);
    }

    private static uint ReadU32BE(byte[] d, int o) =>
        ((uint)d[o] << 24) | ((uint)d[o + 1] << 16) | ((uint)d[o + 2] << 8) | d[o + 3];

    private static readonly uint[] CrcTable = BuildCrcTable();
    private static uint[] BuildCrcTable()
    {
        var t = new uint[256];
        for (uint n = 0; n < 256; n++)
        {
            uint c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }

    private static uint Crc(byte[] type, byte[] data, int len)
    {
        uint c = 0xFFFFFFFFu;
        foreach (var b in type) c = CrcTable[(c ^ b) & 0xFF] ^ (c >> 8);
        for (int i = 0; i < len; i++) c = CrcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }

    private static void WriteChunk(Stream s, string type, byte[] data)
    {
        var t = System.Text.Encoding.ASCII.GetBytes(type);
        var lb = BitConverter.GetBytes(data.Length);
        if (BitConverter.IsLittleEndian) Array.Reverse(lb);
        s.Write(lb, 0, 4);
        s.Write(t, 0, 4);
        if (data.Length > 0) s.Write(data, 0, data.Length);
        var crc = Crc(t, data, data.Length);
        var cb = BitConverter.GetBytes(crc);
        if (BitConverter.IsLittleEndian) Array.Reverse(cb);
        s.Write(cb, 0, 4);
    }

    /// <summary>Encode RGBA → byte PNG (8-bit, filtro 0). Usato anche per scrivere file.</summary>
    public static byte[] EncodePng(byte[] rgba, int w, int h)
    {
        using var ms = new MemoryStream();
        ms.Write(PngSig, 0, 8);
        var ihdr = new byte[13];
        ihdr[0] = (byte)(w >> 24); ihdr[1] = (byte)(w >> 16); ihdr[2] = (byte)(w >> 8); ihdr[3] = (byte)w;
        ihdr[4] = (byte)(h >> 24); ihdr[5] = (byte)(h >> 16); ihdr[6] = (byte)(h >> 8); ihdr[7] = (byte)h;
        ihdr[8] = 8; ihdr[9] = 6;
        WriteChunk(ms, "IHDR", ihdr);
        int row = w * 4;
        var raw = new byte[row * h + h];
        for (int y = 0; y < h; y++)
        {
            raw[y * (row + 1)] = 0;
            Buffer.BlockCopy(rgba, y * row, raw, y * (row + 1) + 1, row);
        }
        byte[] deflated;
        using (var cms = new MemoryStream())
        {
            using (var ds = new System.IO.Compression.DeflateStream(cms, System.IO.Compression.CompressionLevel.Optimal))
                ds.Write(raw, 0, raw.Length);
            deflated = cms.ToArray();
        }
        var idat = new byte[2 + deflated.Length + 4];
        idat[0] = 0x78; idat[1] = 0x9C;
        Buffer.BlockCopy(deflated, 0, idat, 2, deflated.Length);
        uint a1 = 1, a2 = 0;
        foreach (byte b in raw) { a1 = (a1 + b) % 65521; a2 = (a2 + a1) % 65521; }
        uint adler = (a2 << 16) | a1;
        idat[idat.Length - 4] = (byte)(adler >> 24);
        idat[idat.Length - 3] = (byte)(adler >> 16);
        idat[idat.Length - 2] = (byte)(adler >> 8);
        idat[idat.Length - 1] = (byte)adler;
        WriteChunk(ms, "IDAT", idat);
        WriteChunk(ms, "IEND", Array.Empty<byte>());
        return ms.ToArray();
    }

    public static bool TryWritePng(string path, byte[] rgba, int w, int h, out string error)
    {
        error = "";
        try
        {
            File.WriteAllBytes(path, EncodePng(rgba, w, h));
            return true;
        }
        catch (Exception ex) { error = $"[AI Image: scrittura fallita: {ex.Message}]"; return false; }
    }

    private static int Paeth(int a, int b, int c)
    {
        int p = a + b - c, pa = Math.Abs(p - a), pb = Math.Abs(p - b), pc = Math.Abs(p - c);
        return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
    }

    /// <summary>Decodifica PNG 8-bit non interlacciato (tipi 0/2/3/4/6) → RGBA.</summary>
    public static bool TryReadPng(string path, out int w, out int h, out byte[] rgba, out string error)
    {
        w = 0; h = 0; rgba = Array.Empty<byte>(); error = "";
        if (!File.Exists(path)) { error = "[AI Image: file not found]"; return false; }
        if (!HasPngExt(path)) { error = "[AI Image: formato non supportato (solo PNG)]"; return false; }
        try
        {
            var d = File.ReadAllBytes(path);
            if (d.Length < 8 || !PngSig.SequenceEqual(d.Take(8))) { error = "[AI Image: decode PNG fallito]"; return false; }
            int off = 8, width = 0, height = 0, bitDepth = 0, colorType = -1;
            byte[][]? palette = null;
            var idat = new MemoryStream();
            bool haveIhdr = false;
            while (off + 8 <= d.Length)
            {
                int clen = (int)ReadU32BE(d, off);
                string ctype = System.Text.Encoding.ASCII.GetString(d, off + 4, 4);
                if (off + 8 + clen > d.Length) { error = "[AI Image: decode PNG fallito]"; return false; }
                if (ctype == "IHDR" && clen >= 13)
                {
                    width = (int)ReadU32BE(d, off + 8); height = (int)ReadU32BE(d, off + 12);
                    bitDepth = d[off + 16]; colorType = d[off + 17];
                    if (d[off + 20] != 0) { error = "[AI Image: PNG interlacciato non supportato]"; return false; }
                    haveIhdr = true;
                }
                else if (ctype == "PLTE")
                {
                    int n = clen / 3;
                    palette = new byte[n][];
                    for (int i = 0; i < n; i++) palette[i] = new byte[] { d[off + 8 + i * 3], d[off + 8 + i * 3 + 1], d[off + 8 + i * 3 + 2], 255 };
                }
                else if (ctype == "IDAT") idat.Write(d, off + 8, clen);
                else if (ctype == "IEND") break;
                off += 8 + clen + 4;
            }
            if (!haveIhdr || width <= 0 || height <= 0) { error = "[AI Image: decode PNG fallito]"; return false; }
            if (bitDepth != 8 || (colorType != 0 && colorType != 2 && colorType != 3 && colorType != 4 && colorType != 6))
            { error = "[AI Image: profondità/tipo PNG non supportato (solo 8-bit)]"; return false; }
            int bpp = colorType == 0 || colorType == 3 ? 1 : colorType == 4 ? 2 : colorType == 2 ? 3 : 4;
            var zlib = idat.ToArray();
            if (zlib.Length < 6) { error = "[AI Image: decode PNG fallito]"; return false; }
            byte[] raw;
            using (var cms = new MemoryStream(zlib, 2, zlib.Length - 6))
            using (var ds = new System.IO.Compression.DeflateStream(cms, System.IO.Compression.CompressionMode.Decompress))
            using (var out_ = new MemoryStream())
            {
                ds.CopyTo(out_);
                raw = out_.ToArray();
            }
            int stride = width * bpp;
            if (raw.Length < (stride + 1) * height) { error = "[AI Image: decode PNG fallito]"; return false; }
            var px = new byte[width * height * 4];
            var prev = new byte[stride];
            for (int y = 0; y < height; y++)
            {
                int ro = y * (stride + 1);
                int f = raw[ro];
                var row = new byte[stride];
                Buffer.BlockCopy(raw, ro + 1, row, 0, stride);
                for (int x = 0; x < stride; x++)
                {
                    int a = x >= bpp ? row[x - bpp] : 0;
                    int b = y > 0 ? prev[x] : 0;
                    int c = (x >= bpp && y > 0) ? prev[x - bpp] : 0;
                    int v = row[x];
                    v += f switch { 1 => a, 2 => b, 3 => (a + b) / 2, 4 => Paeth(a, b, c), _ => 0 };
                    row[x] = (byte)(v & 0xFF);
                }
                Buffer.BlockCopy(row, 0, prev, 0, stride);
                for (int x = 0; x < width; x++)
                {
                    int r, g, bl, al;
                    if (colorType == 0) { r = g = bl = row[x]; al = 255; }
                    else if (colorType == 2) { r = row[x * 3]; g = row[x * 3 + 1]; bl = row[x * 3 + 2]; al = 255; }
                    else if (colorType == 3)
                    {
                        int idx = row[x];
                        if (palette != null && idx < palette.Length) { r = palette[idx][0]; g = palette[idx][1]; bl = palette[idx][2]; al = 255; }
                        else { r = g = bl = idx; al = 255; }
                    }
                    else if (colorType == 4) { r = g = bl = row[x * 2]; al = row[x * 2 + 1]; }
                    else { r = row[x * 4]; g = row[x * 4 + 1]; bl = row[x * 4 + 2]; al = row[x * 4 + 3]; }
                    int o = (y * width + x) * 4;
                    px[o] = (byte)r; px[o + 1] = (byte)g; px[o + 2] = (byte)bl; px[o + 3] = (byte)al;
                }
            }
            w = width; h = height; rgba = px;
            return true;
        }
        catch (Exception ex) { error = $"[AI Image: decode PNG fallito: {ex.Message}]"; return false; }
    }

    public static byte[] ResizeNearest(byte[] src, int sw, int sh, int dw, int dh)
    {
        var dst = new byte[dw * dh * 4];
        for (int y = 0; y < dh; y++)
        {
            int sy = Math.Min(sh - 1, (int)((long)y * sh / dh));
            for (int x = 0; x < dw; x++)
            {
                int sx = Math.Min(sw - 1, (int)((long)x * sw / dw));
                Buffer.BlockCopy(src, (sy * sw + sx) * 4, dst, (y * dw + x) * 4, 4);
            }
        }
        return dst;
    }

    /// <summary>Ritaglio con clamp; false se il rettangolo è del tutto fuori immagine.</summary>
    public static bool TryCrop(byte[] src, int sw, int sh, int x, int y, int w, int h,
        out byte[] dst, out int cw, out int ch)
    {
        dst = Array.Empty<byte>(); cw = 0; ch = 0;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x >= sw || y >= sh || w <= 0 || h <= 0) return false;
        if (x + w > sw) w = sw - x;
        if (y + h > sh) h = sh - y;
        dst = new byte[w * h * 4];
        for (int dy = 0; dy < h; dy++)
            Buffer.BlockCopy(src, ((y + dy) * sw + x) * 4, dst, dy * w * 4, w * 4);
        cw = w; ch = h;
        return true;
    }

    // F5 multimodale (2026-09-11): embedding percettivo deterministico (8x8=64 double
    // unitari) — luminanza BT.601 intera al centro di ogni cella. Stessa formula del
    // runtime C (gmb_image.c img_embed_from_px) → parità 4 backend. Modello-free.
    public const int EmbedGrid = 8;

    public static double[] PerceptualEmbed(byte[] rgba, int w, int h)
    {
        const int G = 8;
        var e = new double[G * G];
        for (int gy = 0; gy < G; gy++)
        {
            int sy = (int)(((double)gy + 0.5) * h / G);
            if (sy >= h) sy = h - 1;
            if (sy < 0) sy = 0;
            for (int gx = 0; gx < G; gx++)
            {
                int sx = (int)(((double)gx + 0.5) * w / G);
                if (sx >= w) sx = w - 1;
                if (sx < 0) sx = 0;
                int o = (sy * w + sx) * 4;
                int r = rgba[o], g = rgba[o + 1], b = rgba[o + 2];
                e[gy * G + gx] = (r * 299 + g * 587 + b * 114) / 1000;
            }
        }
        double n = 0.0;
        foreach (var v in e) n += v * v;
        n = Math.Sqrt(n);
        if (n > 1e-12) for (int i = 0; i < e.Length; i++) e[i] /= n;
        return e;
    }
}
