using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

// Port of buffers/crossterm_window/text.rs — the Line/Span/Text styled-text model.
// TTY-free, pure logic. Colors map to CellColor (nullable = "no override").

/// <summary>Text emphasis flags (port of Rust `Modifier`).</summary>
[Flags]
public enum Modifier : ushort
{
    None        = 0,
    Bold        = 0b0000_0000_0001,
    Dim         = 0b0000_0000_0010,
    Italic      = 0b0000_0000_0100,
    Underlined  = 0b0000_0000_1000,
    SlowBlink   = 0b0000_0001_0000,
    RapidBlink  = 0b0000_0010_0000,
    Reversed    = 0b0000_0100_0000,
    Hidden      = 0b0000_1000_0000,
    CrossedOut  = 0b0001_0000_0000,
}

/// <summary>
/// A style: optional fg/bg colors plus add/sub modifier sets (port of Rust `Style`).
/// A null color means "leave unchanged" when patched.
/// </summary>
public struct Style : IEquatable<Style>
{
    public CellColor? Fg;
    public CellColor? Bg;
    public Modifier AddModifier;
    public Modifier SubModifier;

    public static Style New() => new Style
    {
        Fg = null,
        Bg = null,
        AddModifier = Modifier.None,
        SubModifier = Modifier.None,
    };

    public static Style Default => New();

    /// <summary>A style resetting all properties.</summary>
    public static Style Reset() => new Style
    {
        Fg = CellColor.Default,
        Bg = CellColor.Default,
        AddModifier = Modifier.None,
        SubModifier = (Modifier)0b1_1111_1111,
    };

    public readonly Style WithFg(CellColor color)
    {
        var s = this;
        s.Fg = color;
        return s;
    }

    public readonly Style WithBg(CellColor color)
    {
        var s = this;
        s.Bg = color;
        return s;
    }

    /// <summary>Add the given modifier to the style modifiers.</summary>
    public readonly Style WithAddModifier(Modifier modifier)
    {
        var s = this;
        s.SubModifier &= ~modifier;
        s.AddModifier |= modifier;
        return s;
    }

    /// <summary>Remove the given modifier from the style modifiers.</summary>
    public readonly Style WithRemoveModifier(Modifier modifier)
    {
        var s = this;
        s.AddModifier &= ~modifier;
        s.SubModifier |= modifier;
        return s;
    }

    /// <summary>Combine two styles: `other` takes precedence where it specifies values.</summary>
    public readonly Style Patch(Style other)
    {
        var s = this;
        s.Fg = other.Fg ?? s.Fg;
        s.Bg = other.Bg ?? s.Bg;

        s.AddModifier &= ~other.SubModifier;
        s.AddModifier |= other.AddModifier;
        s.SubModifier &= ~other.AddModifier;
        s.SubModifier |= other.SubModifier;
        return s;
    }

    public readonly bool Equals(Style o) =>
        Fg == o.Fg && Bg == o.Bg && AddModifier == o.AddModifier && SubModifier == o.SubModifier;
    public override readonly bool Equals(object? obj) => obj is Style s && Equals(s);
    public override readonly int GetHashCode() => HashCode.Combine(Fg, Bg, AddModifier, SubModifier);
    public static bool operator ==(Style a, Style b) => a.Equals(b);
    public static bool operator !=(Style a, Style b) => !a.Equals(b);
}

/// <summary>A fragment of text with an associated style (port of Rust `Span`).</summary>
public class Span
{
    public string Content;
    public Style Style;

    public Span(string content, Style style)
    {
        Content = content ?? "";
        Style = style;
    }

    /// <summary>Raw span with default (unset) style.</summary>
    public static Span Raw(string content) => new Span(content, Style.Default);

    /// <summary>Styled span.</summary>
    public static Span Styled(string content, Style style) => new Span(content, style);

    /// <summary>Display width (chars; TTY-free, no unicode-width dependency).</summary>
    public int Width => Content.Length;

    public static implicit operator Span(string s) => Raw(s);
}

/// <summary>A line composed of one or more <see cref="Span"/> (port of Rust `Line`).</summary>
public class Line
{
    public List<Span> Spans;
    public Style Style;

    public Line()
    {
        Spans = [];
        Style = Style.Default;
    }

    public Line(IEnumerable<Span> spans)
    {
        Spans = new List<Span>(spans);
        Style = Style.Default;
    }

    public static Line Raw(string content) => new Line([Span.Raw(content)]);

    public static Line Styled(string content, Style style) =>
        new Line([Span.Styled(content, style)]);

    public static Line FromSpan(Span span) => new Line([span]);

    /// <summary>Total display width (sum of spans).</summary>
    public int Width
    {
        get
        {
            var w = 0;
            foreach (var s in Spans) w += s.Width;
            return w;
        }
    }

    /// <summary>Full flattened text of the line.</summary>
    public string Text
    {
        get
        {
            var sb = new System.Text.StringBuilder();
            foreach (var s in Spans) sb.Append(s.Content);
            return sb.ToString();
        }
    }

    /// <summary>
    /// Per-column style lookup: returns the (line-patched) style of the span
    /// covering the given column, or null if the column is past the line's width.
    /// </summary>
    public Style? StyleAt(int column)
    {
        if (column < 0) return null;
        var col = 0;
        foreach (var s in Spans)
        {
            var w = s.Width;
            if (column < col + w)
                return Style.Patch(s.Style);
            col += w;
        }
        return null;
    }

    public static implicit operator Line(string s) => Raw(s);
    public static implicit operator Line(Span span) => FromSpan(span);
}

/// <summary>Multi-line styled text (port of the ratatui-style `Text` aggregate).</summary>
public class Text
{
    public List<Line> Lines;
    public Style Style;

    public Text()
    {
        Lines = [];
        Style = Style.Default;
    }

    public Text(IEnumerable<Line> lines)
    {
        Lines = new List<Line>(lines);
        Style = Style.Default;
    }

    /// <summary>Build a Text from a raw string, splitting on newlines.</summary>
    public static Text Raw(string content)
    {
        var t = new Text();
        foreach (var l in (content ?? "").Split('\n'))
            t.Lines.Add(Line.Raw(l));
        return t;
    }

    public static Text Styled(string content, Style style)
    {
        var t = new Text();
        foreach (var l in (content ?? "").Split('\n'))
            t.Lines.Add(Line.Styled(l, style));
        return t;
    }

    /// <summary>Append a line to the text.</summary>
    public void Push(Line line) => Lines.Add(line);

    /// <summary>Append another Text's lines onto this one.</summary>
    public void Append(Text other) => Lines.AddRange(other.Lines);

    /// <summary>Number of lines.</summary>
    public int Height => Lines.Count;

    /// <summary>Widest line width (the styled width of the block).</summary>
    public int Width
    {
        get
        {
            var max = 0;
            foreach (var l in Lines)
                if (l.Width > max) max = l.Width;
            return max;
        }
    }

    public static implicit operator Text(string s) => Raw(s);
    public static implicit operator Text(Line l) => new Text([l]);
}
