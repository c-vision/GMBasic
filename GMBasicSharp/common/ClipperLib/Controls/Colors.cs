using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>RGB color with floating-point components in [0, 1].</summary>
public readonly struct Rgb
{
    public double R { get; }
    public double G { get; }
    public double B { get; }

    public Rgb(double r, double g, double b)
    {
        R = r;
        G = g;
        B = b;
    }

    public static Rgb FromCellColor(CellColor c)
    {
        if (c.IsDefault)
            return new Rgb(0, 0, 0);
        return new Rgb(c.R / 255.0, c.G / 255.0, c.B / 255.0);
    }

    public CellColor ToCellColor()
    {
        var (r, g, b) = ToU8();
        return new CellColor { R = r, G = g, B = b };
    }

    public static Rgb FromU8(byte r, byte g, byte b) =>
        new(r / 255.0, g / 255.0, b / 255.0);

    public (byte r, byte g, byte b) ToU8()
    {
        byte Clamp255(double v)
        {
            var clamped = Math.Clamp(v, 0.0, 1.0);
            if (clamped <= 0.0) return 0;
            if (clamped >= 1.0) return 255;
            return (byte)(clamped * 255.0 + 0.5);
        }
        return (Clamp255(R), Clamp255(G), Clamp255(B));
    }
}

/// <summary>Interpolation mode for gradient stops.</summary>
public enum BlendMode
{
    Rgb,
    Hsl
}

/// <summary>Color gradient sampleable at any t in [0, 1].</summary>
public class Gradient
{
    private readonly (double pos, Rgb color)[] _stops;
    private readonly BlendMode _mode;

    private Gradient((double, Rgb)[] stops, BlendMode mode)
    {
        _stops = stops;
        _mode = mode;
    }

    private static Gradient FromU8Stops((double pos, byte r, byte g, byte b)[] stops, BlendMode mode)
    {
        var s = new (double, Rgb)[stops.Length];
        for (var i = 0; i < stops.Length; i++)
            s[i] = (stops[i].pos, Rgb.FromU8(stops[i].r, stops[i].g, stops[i].b));
        return new Gradient(s, mode);
    }

    public static Gradient FromColors(CellColor[] colors, BlendMode mode)
    {
        if (colors is null || colors.Length == 0)
            return new Gradient([], mode);
        double n = colors.Length;
        var stops = new (double, Rgb)[colors.Length];
        for (var i = 0; i < colors.Length; i++)
            stops[i] = (n > 1 ? i / (n - 1.0) : 0.0, Rgb.FromCellColor(colors[i]));
        return new Gradient(stops, mode);
    }

    public static Gradient WithStops((double pos, CellColor color)[] stops, BlendMode mode)
    {
        if (stops is null || stops.Length == 0)
            return new Gradient([], mode);
        var s = new (double, Rgb)[stops.Length];
        for (var i = 0; i < stops.Length; i++)
            s[i] = (stops[i].pos, Rgb.FromCellColor(stops[i].color));
        return new Gradient(s, mode);
    }

    public CellColor At(double t)
    {
        if (_stops.Length == 0)
            return CellColor.Default;

        var first = _stops[0];
        var last = _stops[^1];

        if (t <= first.pos) return first.color.ToCellColor();
        if (t >= last.pos) return last.color.ToCellColor();

        int lo = 0, hi = _stops.Length;
        while (lo < hi)
        {
            var mid = (lo + hi) / 2;
            if (_stops[mid].pos < t)
                lo = mid + 1;
            else
                hi = mid;
        }

        var (pa, ca) = _stops[lo - 1];
        var (pb, cb) = _stops[lo];

        var local = Math.Abs(pb - pa) < double.Epsilon
            ? 0.0
            : (t - pa) / (pb - pa);

        var result = _mode switch
        {
            BlendMode.Rgb => ColorMath.Mix(ca, cb, local),
            BlendMode.Hsl => InterpolateHsl(ca, cb, local),
            _ => ColorMath.Mix(ca, cb, local)
        };

        return result.ToCellColor();
    }

    private static Rgb InterpolateHsl(Rgb a, Rgb b, double t)
    {
        var (ha, sa, la) = ColorMath.RgbToHsl(a);
        var (hb, sb, lb) = ColorMath.RgbToHsl(b);
        var dh = ShortestHueDelta(ha, hb);
        return ColorMath.HslToRgb(ha + dh * t, sa + (sb - sa) * t, la + (lb - la) * t);
    }

    private static double ShortestHueDelta(double a, double b)
    {
        var d = ((b - a) % 360.0 + 360.0) % 360.0;
        return d > 180.0 ? d - 360.0 : d;
    }

    public CellColor[] Samples(int n)
    {
        if (n <= 0) return [];
        var results = new CellColor[n];
        for (var i = 0; i < n; i++)
            results[i] = At(i / (double)(n - 1));
        return results;
    }

    public static Gradient Rainbow() => FromU8Stops(
    [
        (0.0, 255, 0, 0),
            (0.166, 255, 255, 0),
            (0.333, 0, 255, 0),
            (0.5, 0, 255, 255),
            (0.666, 0, 0, 255),
            (0.833, 255, 0, 255),
            (1.0, 255, 0, 0)
    ], BlendMode.Hsl);

    public static Gradient Turbo() => FromU8Stops(
    [
        (0.0, 48, 18, 59),
            (0.13, 70, 107, 227),
            (0.25, 40, 174, 229),
            (0.38, 53, 224, 153),
            (0.5, 147, 232, 58),
            (0.62, 221, 203, 54),
            (0.75, 249, 133, 38),
            (0.88, 209, 54, 21),
            (1.0, 122, 4, 3)
    ], BlendMode.Rgb);

    public static Gradient Viridis() => FromU8Stops(
    [
        (0.0, 68, 1, 84),
            (0.25, 59, 82, 139),
            (0.5, 33, 145, 140),
            (0.75, 94, 201, 98),
            (1.0, 253, 231, 37)
    ], BlendMode.Rgb);

    public static Gradient Plasma() => FromU8Stops(
    [
        (0.0, 13, 8, 135),
            (0.25, 126, 3, 168),
            (0.5, 204, 71, 120),
            (0.75, 248, 149, 64),
            (1.0, 240, 249, 33)
    ], BlendMode.Rgb);

    public static Gradient Heat() => FromU8Stops(
    [
        (0.0, 0, 0, 0),
            (0.33, 178, 34, 34),
            (0.66, 255, 165, 0),
            (1.0, 255, 255, 255)
    ], BlendMode.Rgb);

    public static Gradient Blues() => FromU8Stops(
    [
        (0.0, 8, 29, 88),
            (0.5, 43, 131, 186),
            (1.0, 199, 233, 180)
    ], BlendMode.Rgb);

    public static Gradient Grayscale() => FromU8Stops(
    [
        (0.0, 0, 0, 0),
            (1.0, 255, 255, 255)
    ], BlendMode.Rgb);
}

/// <summary>Color math utilities: conversions and manipulation.</summary>
public static class ColorMath
{
    public static (double h, double s, double l) RgbToHsl(Rgb c)
    {
        var min = Math.Min(c.R, Math.Min(c.G, c.B));
        var max = Math.Max(c.R, Math.Max(c.G, c.B));
        var chroma = max - min;
        var l = (max + min) / 2.0;

        if (chroma < double.Epsilon)
            return (0.0, 0.0, l);

        double h;
        if (Math.Abs(max - c.R) < double.Epsilon)
            h = (c.G - c.B) / chroma % 6.0;
        else if (Math.Abs(max - c.G) < double.Epsilon)
            h = (c.B - c.R) / chroma + 2.0;
        else
            h = (c.R - c.G) / chroma + 4.0;

        h *= 60.0;
        if (h < 0.0) h += 360.0;

        var s = l < 1.0
            ? chroma / (1.0 - Math.Abs(2.0 * l - 1.0))
            : 0.0;

        return (h, s, l);
    }

    public static Rgb HslToRgb(double h, double s, double l)
    {
        h = (h % 360.0 + 360.0) % 360.0;
        var c = (1.0 - Math.Abs(2.0 * l - 1.0)) * s;
        var x = c * (1.0 - Math.Abs(h / 60.0 % 2.0 - 1.0));
        var m = l - c / 2.0;

        double r, g, b;
        if (h < 60.0) { r = c; g = x; b = 0.0; }
        else if (h < 120.0) { r = x; g = c; b = 0.0; }
        else if (h < 180.0) { r = 0.0; g = c; b = x; }
        else if (h < 240.0) { r = 0.0; g = x; b = c; }
        else if (h < 300.0) { r = x; g = 0.0; b = c; }
        else { r = c; g = 0.0; b = x; }

        return new Rgb(r + m, g + m, b + m);
    }

    public static (double h, double s, double v) RgbToHsv(Rgb c)
    {
        var min = Math.Min(c.R, Math.Min(c.G, c.B));
        var max = Math.Max(c.R, Math.Max(c.G, c.B));
        var delta = max - min;
        var v = max;

        var s = max > 1e-6 ? delta / max : 0.0;

        var h = 0.0;
        if (delta > double.Epsilon)
        {
            if (Math.Abs(max - c.R) < double.Epsilon)
                h = (c.G - c.B) / delta;
            else if (Math.Abs(max - c.G) < double.Epsilon)
                h = 2.0 + (c.B - c.R) / delta;
            else
                h = 4.0 + (c.R - c.G) / delta;

            h *= 60.0;
        }

        if (h < 0.0) h += 360.0;
        return (h, s, v);
    }

    public static Rgb HsvToRgb(double h, double s, double v)
    {
        h = (h % 360.0 + 360.0) % 360.0;
        var c = v * s;
        var x = c * (1.0 - Math.Abs(h / 60.0 % 2.0 - 1.0));
        var m = v - c;

        double r, g, b;
        if (h < 60.0) { r = c; g = x; b = 0.0; }
        else if (h < 120.0) { r = x; g = c; b = 0.0; }
        else if (h < 180.0) { r = 0.0; g = c; b = x; }
        else if (h < 240.0) { r = 0.0; g = x; b = c; }
        else if (h < 300.0) { r = x; g = 0.0; b = c; }
        else { r = c; g = 0.0; b = x; }

        return new Rgb(r + m, g + m, b + m);
    }

    public static (double c, double m, double y, double k) RgbToCmyk(Rgb rgb)
    {
        var k = 1.0 - Math.Max(rgb.R, Math.Max(rgb.G, rgb.B));
        if (k >= 1.0)
            return (0.0, 0.0, 0.0, 1.0);
        var c = (1.0 - rgb.R - k) / (1.0 - k);
        var m = (1.0 - rgb.G - k) / (1.0 - k);
        var y = (1.0 - rgb.B - k) / (1.0 - k);
        return (c, m, y, k);
    }

    public static Rgb CmykToRgb(double c, double m, double y, double k)
    {
        var r = (1.0 - c) * (1.0 - k);
        var g = (1.0 - m) * (1.0 - k);
        var b = (1.0 - y) * (1.0 - k);
        return new Rgb(r, g, b);
    }

    public static Rgb Mix(Rgb a, Rgb b, double t)
    {
        t = Math.Clamp(t, 0.0, 1.0);
        return new Rgb(
            a.R + t * (b.R - a.R),
            a.G + t * (b.G - a.G),
            a.B + t * (b.B - a.B)
        );
    }

    public static Rgb Lighten(Rgb c, double amt)
    {
        var (h, s, l) = RgbToHsl(c);
        return HslToRgb(h, s, Math.Clamp(l + amt, 0.0, 1.0));
    }

    public static Rgb Darken(Rgb c, double amt) => Lighten(c, -amt);

    public static Rgb Saturate(Rgb c, double amt)
    {
        var (h, s, l) = RgbToHsl(c);
        return HslToRgb(h, Math.Clamp(s + amt, 0.0, 1.0), l);
    }
}

/// <summary>CSS/X11 named color lookup.</summary>
public static class NamedColors
{
    private static readonly Dictionary<string, (byte r, byte g, byte b)> _table;

    static NamedColors()
    {
        _table = new Dictionary<string, (byte, byte, byte)>(StringComparer.OrdinalIgnoreCase)
        {
            ["aliceblue"] = (240, 248, 255),
            ["antiquewhite"] = (250, 235, 215),
            ["aqua"] = (0, 255, 255),
            ["aquamarine"] = (127, 255, 212),
            ["azure"] = (240, 255, 255),
            ["beige"] = (245, 245, 220),
            ["bisque"] = (255, 228, 196),
            ["black"] = (0, 0, 0),
            ["blanchedalmond"] = (255, 235, 205),
            ["blue"] = (0, 0, 255),
            ["blueviolet"] = (138, 43, 226),
            ["brown"] = (165, 42, 42),
            ["burlywood"] = (222, 184, 135),
            ["cadetblue"] = (95, 158, 160),
            ["chartreuse"] = (127, 255, 0),
            ["chocolate"] = (210, 105, 30),
            ["coral"] = (255, 127, 80),
            ["cornflowerblue"] = (100, 149, 237),
            ["cornsilk"] = (255, 248, 220),
            ["crimson"] = (220, 20, 60),
            ["cyan"] = (0, 255, 255),
            ["darkblue"] = (0, 0, 139),
            ["darkcyan"] = (0, 139, 139),
            ["darkgoldenrod"] = (184, 134, 11),
            ["darkgray"] = (169, 169, 169),
            ["darkgreen"] = (0, 100, 0),
            ["darkgrey"] = (169, 169, 169),
            ["darkkhaki"] = (189, 183, 107),
            ["darkmagenta"] = (139, 0, 139),
            ["darkolivegreen"] = (85, 107, 47),
            ["darkorange"] = (255, 140, 0),
            ["darkorchid"] = (153, 50, 204),
            ["darkred"] = (139, 0, 0),
            ["darksalmon"] = (233, 150, 122),
            ["darkseagreen"] = (143, 188, 143),
            ["darkslateblue"] = (72, 61, 139),
            ["darkslategray"] = (47, 79, 79),
            ["darkslategrey"] = (47, 79, 79),
            ["darkturquoise"] = (0, 206, 209),
            ["darkviolet"] = (148, 0, 211),
            ["deeppink"] = (255, 20, 147),
            ["deepskyblue"] = (0, 191, 255),
            ["dimgray"] = (105, 105, 105),
            ["dimgrey"] = (105, 105, 105),
            ["dodgerblue"] = (30, 144, 255),
            ["firebrick"] = (178, 34, 34),
            ["floralwhite"] = (255, 250, 240),
            ["forestgreen"] = (34, 139, 34),
            ["fuchsia"] = (255, 0, 255),
            ["gainsboro"] = (220, 220, 220),
            ["ghostwhite"] = (248, 248, 255),
            ["gold"] = (255, 215, 0),
            ["goldenrod"] = (218, 165, 32),
            ["gray"] = (128, 128, 128),
            ["green"] = (0, 128, 0),
            ["greenyellow"] = (173, 255, 47),
            ["grey"] = (128, 128, 128),
            ["honeydew"] = (240, 255, 240),
            ["hotpink"] = (255, 105, 180),
            ["indianred"] = (205, 92, 92),
            ["indigo"] = (75, 0, 130),
            ["ivory"] = (255, 255, 240),
            ["khaki"] = (240, 230, 140),
            ["lavender"] = (230, 230, 250),
            ["lavenderblush"] = (255, 240, 245),
            ["lawngreen"] = (124, 252, 0),
            ["lemonchiffon"] = (255, 250, 205),
            ["lightblue"] = (173, 216, 230),
            ["lightcoral"] = (240, 128, 128),
            ["lightcyan"] = (224, 255, 255),
            ["lightgoldenrodyellow"] = (250, 250, 210),
            ["lightgray"] = (211, 211, 211),
            ["lightgreen"] = (144, 238, 144),
            ["lightgrey"] = (211, 211, 211),
            ["lightpink"] = (255, 182, 193),
            ["lightsalmon"] = (255, 160, 122),
            ["lightseagreen"] = (32, 178, 170),
            ["lightskyblue"] = (135, 206, 250),
            ["lightslategray"] = (119, 136, 153),
            ["lightslategrey"] = (119, 136, 153),
            ["lightsteelblue"] = (176, 196, 222),
            ["lightyellow"] = (255, 255, 224),
            ["lime"] = (0, 255, 0),
            ["limegreen"] = (50, 205, 50),
            ["linen"] = (250, 240, 230),
            ["magenta"] = (255, 0, 255),
            ["maroon"] = (128, 0, 0),
            ["mediumaquamarine"] = (102, 205, 170),
            ["mediumblue"] = (0, 0, 205),
            ["mediumorchid"] = (186, 85, 211),
            ["mediumpurple"] = (147, 112, 219),
            ["mediumseagreen"] = (60, 179, 113),
            ["mediumslateblue"] = (123, 104, 238),
            ["mediumspringgreen"] = (0, 250, 154),
            ["mediumturquoise"] = (72, 209, 204),
            ["mediumvioletred"] = (199, 21, 133),
            ["midnightblue"] = (25, 25, 112),
            ["mintcream"] = (245, 255, 250),
            ["mistyrose"] = (255, 228, 225),
            ["moccasin"] = (255, 228, 181),
            ["navajowhite"] = (255, 222, 173),
            ["navy"] = (0, 0, 128),
            ["oldlace"] = (253, 245, 230),
            ["olive"] = (128, 128, 0),
            ["olivedrab"] = (107, 142, 35),
            ["orange"] = (255, 165, 0),
            ["orangered"] = (255, 69, 0),
            ["orchid"] = (218, 112, 214),
            ["palegoldenrod"] = (238, 232, 170),
            ["palegreen"] = (152, 251, 152),
            ["paleturquoise"] = (175, 238, 238),
            ["palevioletred"] = (219, 112, 147),
            ["papayawhip"] = (255, 239, 213),
            ["peachpuff"] = (255, 218, 185),
            ["peru"] = (205, 133, 63),
            ["pink"] = (255, 192, 203),
            ["plum"] = (221, 160, 221),
            ["powderblue"] = (176, 224, 230),
            ["purple"] = (128, 0, 128),
            ["rebeccapurple"] = (102, 51, 153),
            ["red"] = (255, 0, 0),
            ["rosybrown"] = (188, 143, 143),
            ["royalblue"] = (65, 105, 225),
            ["saddlebrown"] = (139, 69, 19),
            ["salmon"] = (250, 128, 114),
            ["sandybrown"] = (244, 164, 96),
            ["seagreen"] = (46, 139, 87),
            ["seashell"] = (255, 245, 238),
            ["sienna"] = (160, 82, 45),
            ["silver"] = (192, 192, 192),
            ["skyblue"] = (135, 206, 235),
            ["slateblue"] = (106, 90, 205),
            ["slategray"] = (112, 128, 144),
            ["slategrey"] = (112, 128, 144),
            ["snow"] = (255, 250, 250),
            ["springgreen"] = (0, 255, 127),
            ["steelblue"] = (70, 130, 180),
            ["tan"] = (210, 180, 140),
            ["teal"] = (0, 128, 128),
            ["thistle"] = (216, 191, 216),
            ["tomato"] = (255, 99, 71),
            ["turquoise"] = (64, 224, 208),
            ["violet"] = (238, 130, 238),
            ["wheat"] = (245, 222, 179),
            ["white"] = (255, 255, 255),
            ["whitesmoke"] = (245, 245, 245),
            ["yellow"] = (255, 255, 0),
            ["yellowgreen"] = (154, 205, 50)
        };
    }

    public static CellColor? Get(string name)
    {
        if (name is null || !_table.TryGetValue(name, out var rgb))
            return null;
        return new CellColor { R = rgb.r, G = rgb.g, B = rgb.b };
    }

    public static Rgb? GetRgb(string name)
    {
        if (name is null || !_table.TryGetValue(name, out var rgb))
            return null;
        return Rgb.FromU8(rgb.r, rgb.g, rgb.b);
    }
}

/// <summary>Color capability level supported by the terminal.</summary>
public enum ColorLevel
{
    None,
    Ansi16,
    Ansi256,
    TrueColor
}

/// <summary>Detect terminal color support from environment.</summary>
public static class ColorDetection
{
    public static ColorLevel FromEnv(string? term, string? colorterm, bool noColor, int forceColor)
    {
        if (noColor && forceColor == 0)
            return ColorLevel.None;
        if (forceColor >= 3)
            return ColorLevel.TrueColor;
        if (colorterm is not null)
        {
            if (colorterm is "truecolor" or "24bit")
                return ColorLevel.TrueColor;
        }
        if (term is not null)
        {
            if (term.EndsWith("direct") || term.EndsWith("truecolor"))
                return ColorLevel.TrueColor;
            if (term.Contains("256color") || colorterm is not null)
                return ColorLevel.Ansi256;
            if (term.Contains("color"))
                return ColorLevel.Ansi16;
        }
        if (colorterm is not null)
            return ColorLevel.Ansi256;
        if (forceColor > 0)
            return ColorLevel.Ansi16;
        return ColorLevel.None;
    }

    public static ColorLevel Detect()
    {
        var term = Environment.GetEnvironmentVariable("TERM");
        var colorterm = Environment.GetEnvironmentVariable("COLORTERM");
        var noColor = !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("NO_COLOR"));
        var forceColor = 0;
        var fc = Environment.GetEnvironmentVariable("FORCE_COLOR");
        if (int.TryParse(fc, out var parsed))
            forceColor = parsed;
        return FromEnv(term, colorterm, noColor, forceColor);
    }

    public static bool SupportsTrueColor() => Detect() == ColorLevel.TrueColor;
}
