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
    /// <summary>Converte gli span C# nella struct del runtime C (testo troncato a 255).</summary>
    internal static NativeControlDraw.RichSpanC[] ToNative(IReadOnlyList<RichSpan> spans)
    {
        var arr = new NativeControlDraw.RichSpanC[spans.Count];
        for (var i = 0; i < spans.Count; i++)
        {
            var s = spans[i];
            var text = s.Text ?? "";
            if (text.Length > 255) text = text[..255];
            var e = new NativeControlDraw.RichSpanC { Text = text, Bold = s.Bold, Italic = s.Italic, Underline = s.Underline };
            if (s.Fg is CellColor f) { e.HasFg = true; e.Fg = new NativeControlDraw.RichColor { R = f.R, G = f.G, B = f.B, IsDefault = f.IsDefault }; }
            if (s.Bg is CellColor b) { e.HasBg = true; e.Bg = new NativeControlDraw.RichColor { R = b.R, G = b.G, B = b.B, IsDefault = b.IsDefault }; }
            arr[i] = e;
        }
        return arr;
    }

    /// <summary>Parse markup into a flat list of styled spans (parser nel runtime C).</summary>
    public static List<RichSpan> Parse(string markup)
    {
        var native = new NativeControlDraw.RichSpanC[64];
        var n = NativeControlDraw.gmb_richtext_parse(markup ?? "", native, native.Length);
        var spans = new List<RichSpan>(n);
        for (var i = 0; i < n; i++)
        {
            var c = native[i];
            spans.Add(new RichSpan
            {
                Text = c.Text ?? "",
                Fg = c.HasFg ? new CellColor { R = c.Fg.R, G = c.Fg.G, B = c.Fg.B, IsDefault = c.Fg.IsDefault } : null,
                Bg = c.HasBg ? new CellColor { R = c.Bg.R, G = c.Bg.G, B = c.Bg.B, IsDefault = c.Bg.IsDefault } : null,
                Bold = c.Bold,
                Italic = c.Italic,
                Underline = c.Underline
            });
        }
        return spans;
    }

    /// <summary>Draw spans left-to-right starting at (x,y), stopping at maxWidth (render C).</summary>
    public static void RenderToBuffer(ScreenBuffer buf, int x, int y, List<RichSpan> spans, int maxWidth)
    {
        if (spans == null || spans.Count == 0 || maxWidth <= 0 || buf.Handle == IntPtr.Zero) return;
        var arr = ToNative(spans);
        NativeControlDraw.gmb_richtext_render_to_buffer(buf.Handle, x, y, arr, arr.Length, maxWidth);
    }

    /// <summary>Resolve a color token: <c>#rrggbb</c> hex or a CSS-style name (parser C).</summary>
    public static CellColor? ParseColor(string tok)
    {
        if (string.IsNullOrEmpty(tok)) return null;
        var c = new NativeControlDraw.RichColor();
        if (NativeControlDraw.gmb_richtext_parse_color(tok, ref c) == 0) return null;
        return new CellColor { R = c.R, G = c.G, B = c.B, IsDefault = c.IsDefault };
    }
}
