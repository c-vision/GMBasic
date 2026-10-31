// FILE: CommonLib.VideoUtils.cs

/*
 * VideoUtils.cs: primitive VIDEO pure-managed (Y4M nativo) condivise dai backend C#.
 *
 * F7 multimodale (2026-09-11): Y4M (YUV4MPEG2) — formato video semplice e deterministico,
 * parsato senza dipendenze esterne (parità col runtime C gmb_video.c). Altri formati
 * (mp4/mov/...) richiedono ffmpeg (fallback esterno, non gestito qui).
 *   - TryReadY4mInfo: "width,height,fps,frames".
 *   - TryReadY4mFrame: frame `index` → RGBA.
 */

namespace CommonLib;

public static class VideoUtils
{
    public static bool HasY4mExt(string? path) =>
        !string.IsNullOrEmpty(path) && path.EndsWith(".y4m", StringComparison.OrdinalIgnoreCase);

    private static string? ReadLine(Stream s)
    {
        var buf = new System.Collections.Generic.List<byte>(128);
        int c;
        while ((c = s.ReadByte()) != -1)
        {
            if (c == '\n') return System.Text.Encoding.ASCII.GetString(buf.ToArray());
            buf.Add((byte)c);
        }
        return buf.Count > 0 ? System.Text.Encoding.ASCII.GetString(buf.ToArray()) : null;
    }

    private static bool ParseHeader(string line, out int w, out int h, out int fps)
    {
        w = 0; h = 0; fps = 0;
        if (!line.StartsWith("YUV4MPEG2", StringComparison.Ordinal)) return false;
        int fn = 0, fd = 0;
        foreach (var p in line.Substring(9).Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            if (p.StartsWith('W')) int.TryParse(p.AsSpan(1), out w);
            else if (p.StartsWith('H')) int.TryParse(p.AsSpan(1), out h);
            else if (p.StartsWith('F'))
            {
                var c = p.Substring(1).Split(':');
                int.TryParse(c[0], out fn);
                if (c.Length > 1) int.TryParse(c[1], out fd);
            }
        }
        fps = fd > 0 ? fn / fd : fn;
        return w > 0 && h > 0;
    }

    private static int FrameSize(int w, int h) => w * h + ((w + 1) / 2) * ((h + 1) / 2) * 2;

    private static byte[] ReadExact(Stream s, int n)
    {
        var b = new byte[n];
        int off = 0;
        while (off < n)
        {
            int r = s.Read(b, off, n - off);
            if (r <= 0) break;
            off += r;
        }
        if (off != n) throw new EndOfStreamException();
        return b;
    }

    public static bool TryReadY4mInfo(string path, out int w, out int h, out int fps, out int frames, out string error)
    {
        w = 0; h = 0; fps = 0; frames = 0; error = "";
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) { error = "[AI Video: file not found]"; return false; }
        if (!HasY4mExt(path)) { error = "[AI Video: formato non supportato (Y4M nativo; altri formati richiedono ffmpeg)]"; return false; }
        try
        {
            using var s = File.OpenRead(path);
            var hdr = ReadLine(s);
            if (hdr is null || !ParseHeader(hdr, out w, out h, out fps)) { error = "[AI Video: header Y4M non valido]"; return false; }
            int fsz = FrameSize(w, h);
            string? line;
            while ((line = ReadLine(s)) is not null)
            {
                if (line.StartsWith("FRAME", StringComparison.Ordinal))
                {
                    frames++;
                    if (s.Seek(fsz, SeekOrigin.Current) > s.Length) break;
                }
            }
            return true;
        }
        catch (Exception ex) { error = $"[AI Video: {ex.Message}]"; return false; }
    }

    public static bool TryReadY4mFrame(string path, int index, out int w, out int h, out byte[] rgba, out string error)
    {
        w = 0; h = 0; rgba = Array.Empty<byte>(); error = "";
        if (index < 0) { error = "[AI Video: indice frame negativo]"; return false; }
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) { error = "[AI Video: file not found]"; return false; }
        if (!HasY4mExt(path)) { error = "[AI Video: formato non supportato (Y4M nativo; altri formati richiedono ffmpeg)]"; return false; }
        try
        {
            using var s = File.OpenRead(path);
            var hdr = ReadLine(s);
            if (hdr is null || !ParseHeader(hdr, out w, out h, out _)) { error = "[AI Video: header Y4M non valido]"; return false; }
            int fsz = FrameSize(w, h);
            int i = 0;
            string? line;
            byte[]? frm = null;
            while ((line = ReadLine(s)) is not null)
            {
                if (line.StartsWith("FRAME", StringComparison.Ordinal))
                {
                    if (i == index) { frm = ReadExact(s, fsz); break; }
                    i++;
                    if (s.Seek(fsz, SeekOrigin.Current) > s.Length) break;
                }
            }
            if (frm is null) { error = "[AI Video: indice frame fuori range]"; return false; }
            rgba = Yuv420ToRgba(frm, w, h);
            return true;
        }
        catch (Exception ex) { error = $"[AI Video: {ex.Message}]"; return false; }
    }

    private static byte[] Yuv420ToRgba(byte[] frm, int w, int h)
    {
        var px = new byte[w * h * 4];
        int ysz = w * h;
        int usz = ((w + 1) / 2) * ((h + 1) / 2);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                int ci = (y >> 1) * ((w + 1) / 2) + (x >> 1);
                int c = frm[y * w + x] - 16;
                int d = frm[ysz + ci] - 128;
                int e = frm[ysz + usz + ci] - 128;
                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;
                if (r < 0) r = 0; if (r > 255) r = 255;
                if (g < 0) g = 0; if (g > 255) g = 255;
                if (b < 0) b = 0; if (b > 255) b = 255;
                int o = (y * w + x) * 4;
                px[o] = (byte)r; px[o + 1] = (byte)g; px[o + 2] = (byte)b; px[o + 3] = 255;
            }
        return px;
    }
}
