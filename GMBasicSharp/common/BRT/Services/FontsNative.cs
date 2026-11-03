// FILE: Basic.Runtime/FontsNative.cs
// Ponte C# → dylib del runtime FONTS (libgmb_fonts.dylib). L'algoritmo (stb_truetype) NON è
// replicato qui: queste funzioni sono soltanto marshalling verso native/gmb_runtime_c/src/gmb_fonts.c,
// cosi' IL e PCodeVM danno gli stessi numeri di VM/C e C99Native (§0bis: una sola sorgente).
//
// Il resolver per-assembly è già registrato da CGfxNative (delega a GmbBoot.ResolveDllImport,
// che cerca la lib per nome fra libs-runtime, Resources e build native): qui non se ne aggiunge uno.
using System;
using System.Runtime.InteropServices;

namespace Basic.Runtime;

// pubblico: IlBackendRuntime è un assembly separato e deve poter delegare qui.
public static class FontsNative
{
    private const string Lib = "gmb_fonts";

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_fonts_tt_metrics(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        double pixelHeight,
        int codepoint,
        [Out] double[] out8);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_fonts_tt_render(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        double pixelHeight,
        int codepoint,
        [Out] byte[] outBuf,
        int cap,
        out int outW,
        out int outH,
        out int outXoff,
        out int outYoff);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_fonts_tt_glyph_count(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_fonts_tt_glyph_codes(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [Out] int[] outCodes,
        int cap);

    /// <summary>8 metriche del glifo; array vuoto su errore (file assente, font invalido, size &lt;= 0).</summary>
    public static double[] Metrics(string path, double pixelHeight, int codepoint)
    {
        var m = new double[8];
        int rc = gmb_fonts_tt_metrics(path ?? string.Empty, pixelHeight, codepoint, m);
        return rc == 0 ? m : Array.Empty<double>();
    }

    /// <summary>Bitmap alpha (0..255) del glifo; array vuoto se il codepoint non è coperto.</summary>
    public static byte[] Render(string path, double pixelHeight, int codepoint)
    {
        var m = Metrics(path, pixelHeight, codepoint);
        if (m.Length < 2) return Array.Empty<byte>();
        int w = (int)(m[0] + 0.5);
        int h = (int)(m[1] + 0.5);
        if (w <= 0 || h <= 0) return Array.Empty<byte>();
        var buf = new byte[w * h];
        int rc = gmb_fonts_tt_render(path ?? string.Empty, pixelHeight, codepoint, buf, buf.Length,
                                     out int rw, out int rh, out _, out _);
        if (rc != 0) return Array.Empty<byte>();
        if (rw != w || rh != h)
        {
            if (rw <= 0 || rh <= 0) return Array.Empty<byte>();
            Array.Resize(ref buf, rw * rh);
        }
        return buf;
    }

    /// <summary>Codepoint coperti dal cmap, crescenti; array vuoto su errore.</summary>
    public static double[] Glyphs(string path)
    {
        int n = gmb_fonts_tt_glyph_count(path ?? string.Empty);
        if (n <= 0) return Array.Empty<double>();
        var codes = new int[n];
        int got = gmb_fonts_tt_glyph_codes(path ?? string.Empty, codes, n);
        if (got <= 0) return Array.Empty<double>();
        if (got > n) got = n;
        var res = new double[got];
        for (int i = 0; i < got; i++) res[i] = codes[i];
        return res;
    }
}
