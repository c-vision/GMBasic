using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// SGR string-builder helpers plus ANSI 256-color / truecolor conversions.
/// Pure and TTY-free: all escapes are produced only via <see cref="Ansi"/> constants.
/// </summary>
public static class Colorize
{
    private const int RgbStart = 16;
    private const int RgbMaxField = 5;
    private const int RgbFieldRange = RgbMaxField + 1;
    private const int RgbEnd = RgbStart + RgbFieldRange * RgbFieldRange * RgbFieldRange - 1; // 231
    private const int GreyScaleStart = RgbEnd + 1; // 232
    private const int GreyScaleMaxLevel = 23;

    /// <summary>Resets all SGR attributes (reuses <see cref="Ansi.Reset"/>).</summary>
    public static string Reset => Ansi.Reset;

    /// <summary>
    /// Builds a single SGR escape string from the given attributes and optional
    /// 24-bit foreground/background colors. Color codes are emitted with
    /// <see cref="Ansi.FgRgb"/> / <see cref="Ansi.BgRgb"/>; attribute codes reuse
    /// <see cref="Ansi.CSI"/> as the introducer.
    /// </summary>
    public static string Sgr(
        bool bold = false,
        bool dim = false,
        bool italic = false,
        bool underline = false,
        CellColor? fg = null,
        CellColor? bg = null)
    {
        var codes = new System.Collections.Generic.List<string>();
        if (bold) codes.Add("1");
        if (dim) codes.Add("2");
        if (italic) codes.Add("3");
        if (underline) codes.Add("4");

        var fc = fg ?? default;
        if (fg is not null && !fc.IsDefault)
            codes.Add($"38;2;{fc.R};{fc.G};{fc.B}");

        var bc = bg ?? default;
        if (bg is not null && !bc.IsDefault)
            codes.Add($"48;2;{bc.R};{bc.G};{bc.B}");

        if (codes.Count == 0)
            return Ansi.Reset;

        return Ansi.CSI + string.Join(";", codes) + "m";
    }

    /// <summary>
    /// Maps a 24-bit color to the nearest xterm 256-color index, choosing
    /// whichever of the 6x6x6 cube (16..=231) or the greyscale ramp (232..=255)
    /// is closer in squared RGB distance.
    /// </summary>
    public static int RgbToAnsi256(CellColor c)
    {
        var pr = c.R * RgbMaxField / 255;
        var pg = c.G * RgbMaxField / 255;
        var pb = c.B * RgbMaxField / 255;
        var palette = RgbStart + RgbFieldRange * RgbFieldRange * pr + RgbFieldRange * pg + pb;

        var grey = GreyScaleStart + (c.R + c.G + c.B) / (GreyScaleMaxLevel * 3);

        return SquaredDistance(c, Ansi256ToRgb(palette)) <= SquaredDistance(c, Ansi256ToRgb(grey))
            ? palette
            : grey;
    }

    /// <summary>Inverse of <see cref="RgbToAnsi256"/>: returns the RGB color for an xterm 256 index.</summary>
    public static CellColor Ansi256ToRgb(int idx)
    {
        if (idx >= GreyScaleStart)
        {
            var level = idx - GreyScaleStart;
            var v = (byte)((level * 255 + GreyScaleMaxLevel / 2) / GreyScaleMaxLevel);
            return new CellColor { R = v, G = v, B = v };
        }

        var n = idx - RgbStart;
        var step = 255 / RgbMaxField;
        var r = n / (RgbFieldRange * RgbFieldRange);
        var g = n / RgbFieldRange % RgbFieldRange;
        var b = n % RgbFieldRange;
        return new CellColor
        {
            R = (byte)(r * step),
            G = (byte)(g * step),
            B = (byte)(b * step)
        };
    }

    /// <summary>Foreground SGR for an xterm 256-color index (reuses <see cref="Ansi.Fg256"/>).</summary>
    public static string Fg256(int idx) => Ansi.Fg256(idx);

    /// <summary>Background SGR for an xterm 256-color index (reuses <see cref="Ansi.Bg256"/>).</summary>
    public static string Bg256(int idx) => Ansi.Bg256(idx);

    private static uint SquaredDistance(CellColor a, CellColor b)
    {
        var r = a.R - b.R;
        var g = a.G - b.G;
        var bl = a.B - b.B;
        return (uint)(r * r + g * g + bl * bl);
    }
}
