using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Truecolor → ANSI palette quantization.
///
/// Faithful port of the Rust <c>colorize::quantize</c> (chargrid's
/// <c>ansi_colour_codes.rs</c>, MIT/Apache-2.0). Snaps an arbitrary 24-bit
/// <see cref="CellColor"/> to the nearest standard ANSI color: the 16 named
/// colors, the 216-color cube (codes 16..=231), or the 24-step greyscale ramp
/// (codes 232..=255). TTY-free: no console writes.
/// </summary>
public static class Quantize
{
    private const int RgbStart = 16;
    private const int RgbMaxField = 5;
    private const int RgbFieldRange = RgbMaxField + 1; // 6
    private const int RgbCount = RgbFieldRange * RgbFieldRange * RgbFieldRange; // 216
    private const int RgbEnd = RgbStart + RgbCount - 1; // 231
    private const int GreyScaleStart = RgbEnd + 1; // 232
    private const int GreyScaleMaxLevel = 23;

    private const int Step = 255 / RgbMaxField; // 51

    /// <summary>Standard xterm ANSI 16 palette (indices 0..15).</summary>
    public static readonly CellColor[] Ansi16 =
    [
        new() { R = 0,   G = 0,   B = 0   }, // 0  black
        new() { R = 205, G = 0,   B = 0   }, // 1  red
        new() { R = 0,   G = 205, B = 0   }, // 2  green
        new() { R = 205, G = 205, B = 0   }, // 3  yellow
        new() { R = 0,   G = 0,   B = 205 }, // 4  blue
        new() { R = 205, G = 0,   B = 205 }, // 5  magenta
        new() { R = 0,   G = 205, B = 205 }, // 6  cyan
        new() { R = 205, G = 205, B = 205 }, // 7  white (light gray)
        new() { R = 127, G = 127, B = 127 }, // 8  bright black (gray)
        new() { R = 255, G = 0,   B = 0   }, // 9  bright red
        new() { R = 0,   G = 255, B = 0   }, // 10 bright green
        new() { R = 255, G = 255, B = 0   }, // 11 bright yellow
        new() { R = 0,   G = 0,   B = 255 }, // 12 bright blue
        new() { R = 255, G = 0,   B = 255 }, // 13 bright magenta
        new() { R = 0,   G = 255, B = 255 }, // 14 bright cyan
        new() { R = 255, G = 255, B = 255 } // 15 bright white
    ];

    /// <summary>xterm 256-color palette: Ansi16 (0..15) + 216 cube (16..231) + 24 greyscale (232..255).</summary>
    public static readonly CellColor[] Ansi256 =
        BuildAnsi256();

    private static CellColor[] BuildAnsi256()
    {
        var arr = new CellColor[256];
        for (var i = 0; i < 16; i++)
            arr[i] = Ansi16[i];
        for (var code = RgbStart; code <= RgbEnd; code++)
            arr[code] = PaletteRgb((byte)code);
        for (var code = GreyScaleStart; code <= 255; code++)
            arr[code] = GreyscaleRgb((byte)code);
        return arr;
    }

    /// <summary>Maps a color to the nearest 216-color cube entry (ANSI 16..231).</summary>
    public static int QuantizeToPalette(CellColor c)
    {
        var r = c.R * RgbMaxField / 255;
        var g = c.G * RgbMaxField / 255;
        var b = c.B * RgbMaxField / 255;
        return RgbStart + RgbFieldRange * RgbFieldRange * r + RgbFieldRange * g + b;
    }

    /// <summary>Maps a color to the nearest 24-step greyscale entry (ANSI 232..255).</summary>
    public static int QuantizeToGreyscale(CellColor c)
    {
        var offset = (c.R + c.G + c.B) / (GreyScaleMaxLevel * 3);
        return GreyScaleStart + offset;
    }

    /// <summary>Maps a color to the nearest of the 16 standard ANSI colors (0..15), using chargrid's coarse 85-step buckets.</summary>
    public static int QuantizeTo16(CellColor c)
    {
        const int third = 255 / 3; // 85
        var cr = c.R / third;
        var cg = c.G / third;
        var cb = c.B / third;

        if (cr == 0 && cg == 0 && cb == 0) return 0;
        if (cr == 1 && cg == 0 && cb == 0) return 1;
        if (cr == 0 && cg == 1 && cb == 0) return 2;
        if (cr == 0 && cg == 0 && cb == 1) return 4;
        if (cr == 1 && cg == 1 && cb == 0) return 3;
        if (cr == 0 && cg == 1 && cb == 1) return 6;
        if (cr == 1 && cg == 0 && cb == 1) return 5;
        if (cr == 1 && cg == 1 && cb == 1) return 8;

        bool hi(int v) => v is 2 or 3;
        bool lo(int v) => v is 0 or 1;

        if (hi(cr) && hi(cg) && hi(cb))
            return cr == 2 && cg == 2 && cb == 2 ? 7 : 15;
        if (hi(cr) && lo(cg) && lo(cb)) return 9;
        if (lo(cr) && hi(cg) && lo(cb)) return 10;
        if (lo(cr) && lo(cg) && hi(cb)) return 12;
        if (hi(cr) && hi(cg) && lo(cb)) return 11;
        if (lo(cr) && hi(cg) && hi(cb)) return 14;
        if (hi(cr) && lo(cg) && hi(cb)) return 13;

        // Unreachable given the 4-bucket partitioning.
        return 0;
    }

    /// <summary>Maps a color to the nearest ANSI 256 entry (16..255), choosing whichever of the cube or greyscale ramp is closer.</summary>
    public static int QuantizeTo256(CellColor c)
    {
        var p = QuantizeToPalette(c);
        var g = QuantizeToGreyscale(c);
        return SquaredDistance(c, Ansi256[p]) <= SquaredDistance(c, Ansi256[g]) ? p : g;
    }

    // SGR helpers ----------------------------------------------------------

    /// <summary>Foreground SGR for the nearest ANSI 16 color.</summary>
    public static string FgAnsi16(CellColor c) => Ansi.Fg256(QuantizeTo16(c));

    /// <summary>Background SGR for the nearest ANSI 16 color.</summary>
    public static string BgAnsi16(CellColor c) => Ansi.Bg256(QuantizeTo16(c));

    /// <summary>Foreground SGR for the nearest ANSI 256 color.</summary>
    public static string FgAnsi256(CellColor c) => Ansi.Fg256(QuantizeTo256(c));

    /// <summary>Background SGR for the nearest ANSI 256 color.</summary>
    public static string BgAnsi256(CellColor c) => Ansi.Bg256(QuantizeTo256(c));

    // Internals ------------------------------------------------------------

    private static CellColor PaletteRgb(byte code)
    {
        var n = code - RgbStart;
        var r = n / (RgbFieldRange * RgbFieldRange);
        var g = n / RgbFieldRange % RgbFieldRange;
        var b = n % RgbFieldRange;
        return new() { R = (byte)(r * Step), G = (byte)(g * Step), B = (byte)(b * Step) };
    }

    private static CellColor GreyscaleRgb(byte code)
    {
        var n = code - GreyScaleStart;
        var v = (byte)(n * 255 / GreyScaleMaxLevel);
        return new() { R = v, G = v, B = v };
    }

    private static uint SquaredDistance(CellColor a, CellColor b)
    {
        var r = a.R - b.R;
        var g = a.G - b.G;
        var bl = a.B - b.B;
        return (uint)(r * r + g * g + bl * bl);
    }
}
