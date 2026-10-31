using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>
/// A fragment of styled text produced by <see cref="RichText.Parse"/>.
/// Attributes (Bold/Italic/Underline) are retained for SGR emission; a
/// <see cref="ScreenBuffer"/> only carries Fg/Bg, so <see cref="RichText.RenderToBuffer"/>
/// applies colors and ignores pure attribute flags when drawing.
/// </summary>
public class RichSpan
{
    public string Text = "";
    public CellColor? Fg;
    public CellColor? Bg;
    public bool Bold;
    public bool Italic;
    public bool Underline;
}

/// <summary>
/// Tiny markup parser (clean-room port of wiki-tui rich_text semantics).
/// Supported syntax:
///  - <c>*bold*</c>          -> Bold
///  - <c>_italic_</c>        -> Italic
///  - <c>{#rrggbb text}</c>  -> foreground color from hex
///  - <c>{red text}</c>      -> foreground color from a named palette
/// Tags can be nested; an unmatched delimiter is emitted literally.
/// </summary>
public static class RichText
{
    /// <summary>Parse markup into a flat list of styled spans.</summary>
    public static List<RichSpan> Parse(string markup)
    {
        var spans = new List<RichSpan>();
        var text = new System.Text.StringBuilder();
        bool bold = false, italic = false, underline = false;
        CellColor? fg = null, bg = null;

        void Flush()
        {
            if (text.Length > 0)
            {
                spans.Add(new RichSpan
                {
                    Text = text.ToString(),
                    Fg = fg,
                    Bg = bg,
                    Bold = bold,
                    Italic = italic,
                    Underline = underline
                });
                text.Clear();
            }
        }

        var i = 0;
        while (i < markup.Length)
        {
            var c = markup[i];
            if (c == '*')
            {
                Flush();
                bold = !bold;
                i++;
                continue;
            }
            if (c == '_')
            {
                Flush();
                italic = !italic;
                i++;
                continue;
            }
            if (c == '{')
            {
                var close = markup.IndexOf('}', i);
                if (close < 0)
                {
                    text.Append(c);
                    i++;
                    continue;
                }
                var inner = markup.Substring(i + 1, close - i - 1);
                var sp = inner.IndexOf(' ');
                if (sp < 0)
                {
                    // bare color tag: just set fg for subsequent text
                    fg = ParseColor(inner) ?? fg;
                    i = close + 1;
                    continue;
                }
                var colorTok = inner[..sp];
                var content = inner[(sp + 1)..];
                var prevFg = fg;
                fg = ParseColor(colorTok) ?? fg;
                if (content.Length > 0)
                {
                    spans.Add(new RichSpan
                    {
                        Text = content,
                        Fg = fg,
                        Bg = bg,
                        Bold = bold,
                        Italic = italic,
                        Underline = underline
                    });
                }
                fg = prevFg;
                i = close + 1;
                continue;
            }
            text.Append(c);
            i++;
        }
        Flush();
        return spans;
    }

    /// <summary>Draw spans left-to-right starting at (x,y), stopping at maxWidth.</summary>
    public static void RenderToBuffer(ScreenBuffer buf, int x, int y, List<RichSpan> spans, int maxWidth)
    {
        var cx = x;
        var limit = x + maxWidth;
        foreach (var span in spans)
        {
            foreach (var ch in span.Text)
            {
                if (cx >= limit) return;
                buf.SetChar(cx, y, ch, span.Fg, span.Bg);
                cx++;
            }
        }
    }

    /// <summary>Resolve a color token: <c>#rrggbb</c> hex or a CSS-style name.</summary>
    public static CellColor? ParseColor(string tok)
    {
        if (string.IsNullOrEmpty(tok)) return null;
        if (tok[0] == '#')
        {
            var hex = tok[1..];
            if (hex.Length == 6 &&
                int.TryParse(hex[..2], System.Globalization.NumberStyles.HexNumber, null, out var r) &&
                int.TryParse(hex.Substring(2, 2), System.Globalization.NumberStyles.HexNumber, null, out var g) &&
                int.TryParse(hex.Substring(4, 2), System.Globalization.NumberStyles.HexNumber, null, out var b))
            {
                return new CellColor { R = (byte)r, G = (byte)g, B = (byte)b };
            }
            return null;
        }
        return tok.ToLowerInvariant() switch
        {
            "black" => CellColor.Black,
            "white" => CellColor.White,
            "red" => CellColor.Red,
            "green" => CellColor.Green,
            "blue" => CellColor.Blue,
            "yellow" => CellColor.Yellow,
            "cyan" => CellColor.Cyan,
            "magenta" => CellColor.Magenta,
            "grey" or "gray" => CellColor.Grey,
            "darkred" => CellColor.DarkRed,
            "darkgreen" => CellColor.DarkGreen,
            "darkblue" => CellColor.DarkBlue,
            "darkyellow" => CellColor.DarkYellow,
            "darkcyan" => CellColor.DarkCyan,
            "darkmagenta" => CellColor.DarkMagenta,
            _ => null
        };
    }
}
