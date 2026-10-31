using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>
/// Buffer widgets (clean-room port of buffers/crossterm_window block+widgets).
/// Each widget draws itself into a <see cref="ScreenBuffer"/> inside a <see cref="Rect"/>.
/// TTY-free: no console writes; colors come from <see cref="CellColor"/>.
/// </summary>
public class BorderBox
{
    /// <summary>Border color (default Cyan).</summary>
    public CellColor BorderColor = CellColor.Cyan;
    /// <summary>Optional title drawn on the top border.</summary>
    public string? Title;

    /// <summary>Draw a single-line box (┌─┐│└─┘) with optional centered title.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0) return;
        var left = area.X;
        var right = area.X + area.W - 1;
        var top = area.Y;
        var bottom = area.Y + area.H - 1;

        for (var x = left; x <= right; x++)
        {
            buf.SetChar(x, top, '─', BorderColor);
            buf.SetChar(x, bottom, '─', BorderColor);
        }
        for (var y = top; y <= bottom; y++)
        {
            buf.SetChar(left, y, '│', BorderColor);
            buf.SetChar(right, y, '│', BorderColor);
        }
        buf.SetChar(left, top, '┌', BorderColor);
        buf.SetChar(right, top, '┐', BorderColor);
        buf.SetChar(left, bottom, '└', BorderColor);
        buf.SetChar(right, bottom, '┘', BorderColor);

        if (!string.IsNullOrEmpty(Title) && area.W > 2)
        {
            var title = Title!;
            var avail = area.W - 2;
            if (title.Length > avail) title = title[..avail];
            var tx = left + 1 + Math.Max(0, (avail - title.Length) / 2);
            buf.SetString(tx, top, title, BorderColor);
        }
    }
}

/// <summary>
/// Word-wrapped paragraph of <see cref="RichSpan"/>s.
/// </summary>
public class Paragraph
{
    /// <summary>Styled content spans.</summary>
    public List<RichSpan> Spans = [];

    /// <summary>Draw spans word-wrapped inside <paramref name="area"/>.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0) return;

        var y = area.Y;
        var limit = area.X + area.W;
        var cx = area.X;

        foreach (var span in Spans)
        {
            // split into words keeping spaces; wrap at word boundaries
            var words = span.Text.Split(' ');
            for (var w = 0; w < words.Length; w++)
            {
                var word = words[w];
                var wordW = word.Length;
                if (cx + wordW > limit && cx > area.X)
                {
                    y++;
                    cx = area.X;
                    if (y > area.Y + area.H - 1) return;
                }
                // draw word char by char
                for (var k = 0; k < word.Length; k++)
                {
                    if (cx >= limit) break;
                    buf.SetChar(cx, y, word[k], span.Fg, span.Bg);
                    cx++;
                }
                // trailing space if not last word
                if (w < words.Length - 1)
                {
                    if (cx >= limit)
                    {
                        y++;
                        cx = area.X;
                        if (y > area.Y + area.H - 1) return;
                    }
                    else
                    {
                        buf.SetChar(cx, y, ' ', span.Fg, span.Bg);
                        cx++;
                    }
                }
            }
        }
    }
}

/// <summary>
/// Scrollable list with a highlighted selected row.
/// </summary>
public class List
{
    /// <summary>Items to display.</summary>
    public List<string> Items = [];
    /// <summary>Index of the selected item (-1 for none).</summary>
    public int Selected = -1;
    /// <summary>Foreground color of the selected row (default Black).</summary>
    public CellColor SelFg = CellColor.Black;
    /// <summary>Background color of the selected row (default Cyan).</summary>
    public CellColor SelBg = CellColor.Cyan;

    /// <summary>Draw the list, scrolling so <see cref="Selected"/> stays visible.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0 || Items.Count == 0) return;

        var height = area.H;
        var offset = 0;
        if (Selected >= 0)
        {
            if (Selected < offset) offset = Selected;
            else if (Selected >= offset + height) offset = Selected + 1 - height;
        }

        for (var row = 0; row < height; row++)
        {
            var idx = offset + row;
            var y = area.Y + row;
            if (idx >= Items.Count) break;
            var sel = idx == Selected;
            var s = Items[idx];
            if (s.Length > area.W) s = s[..area.W];
            if (sel)
            {
                buf.FillRect(y, area.X, y, area.X + area.W - 1, ' ', SelFg, SelBg);
                buf.SetString(area.X, y, s, SelFg, SelBg);
            }
            else
            {
                buf.SetString(area.X, y, s);
            }
        }
    }
}

/// <summary>
/// Horizontal tab bar; the selected tab is highlighted.
/// </summary>
public class Tabs
{
    /// <summary>Tab titles.</summary>
    public List<string> Titles = [];
    /// <summary>Index of the selected tab.</summary>
    public int Selected;
    /// <summary>Foreground color of the selected tab (default Black).</summary>
    public CellColor SelFg = CellColor.Black;
    /// <summary>Background color of the selected tab (default Cyan).</summary>
    public CellColor SelBg = CellColor.Cyan;

    /// <summary>Draw tab titles separated by spaces, highlighting <see cref="Selected"/>.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0 || Titles.Count == 0) return;
        var x = area.X;
        var limit = area.X + area.W;
        for (var i = 0; i < Titles.Count; i++)
        {
            if (x >= limit) break;
            var t = Titles[i];
            var sel = i == Selected;
            var fg = sel ? SelFg : CellColor.White;
            var bg = sel ? SelBg : CellColor.Black;
            for (var k = 0; k < t.Length && x < limit; k++)
            {
                buf.SetChar(x, area.Y, t[k], fg, bg);
                x++;
            }
            if (i + 1 < Titles.Count && x < limit)
            {
                buf.SetChar(x, area.Y, ' ', CellColor.White, CellColor.Black);
                x++;
            }
        }
    }
}

/// <summary>
/// Simple bordered table with a header row.
/// </summary>
public class Table
{
    /// <summary>Column header labels.</summary>
    public List<string> Headers = [];
    /// <summary>Row cells (one inner list per row, aligned to column count).</summary>
    public List<List<string>> Rows = [];
    /// <summary>Border color (default Cyan).</summary>
    public CellColor BorderColor = CellColor.Cyan;

    /// <summary>Draw the table with column separators and a header underline.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0) return;
        var cols = Math.Max(Headers.Count, Rows.Count > 0 ? Rows[0].Count : 0);
        if (cols == 0) return;

        var innerW = area.W - 1; // account for left border
        var colW = Math.Max(1, innerW / cols);
        var x0 = area.X;
        var y = area.Y;

        // header
        DrawRow(buf, area, Headers, x0, y, colW, BorderColor);
        y++;
        if (y > area.Y + area.H - 1) return;
        // header separator
        for (var c = 0; c < cols; c++)
        {
            var cx = x0 + 1 + c * colW;
            buf.SetChar(cx, y, '─', BorderColor);
            if (c < cols - 1)
            {
                var cxEnd = x0 + 1 + (c + 1) * colW - 1;
                for (var xx = cx + 1; xx < cxEnd; xx++)
                    buf.SetChar(xx, y, '─', BorderColor);
                buf.SetChar(cxEnd, y, '┼', BorderColor);
            }
        }
        buf.SetChar(x0, y, '├', BorderColor);
        buf.SetChar(x0 + 1 + cols * colW - 1, y, '┤', BorderColor);
        y++;

        // data rows
        for (var r = 0; r < Rows.Count; r++)
        {
            if (y > area.Y + area.H - 1) break;
            DrawRow(buf, area, Rows[r], x0, y, colW, BorderColor);
            y++;
        }
    }

    private static void DrawRow(ScreenBuffer buf, Rect area, List<string> cells, int x0, int y, int colW, CellColor border)
    {
        buf.SetChar(x0, y, '│', border);
        for (var c = 0; c < cells.Count; c++)
        {
            var cx = x0 + 1 + c * colW;
            var s = cells[c];
            if (s.Length > colW - 1) s = s[..(colW - 1)];
            buf.SetString(cx, y, s);
            var cxEnd = x0 + 1 + (c + 1) * colW - 1;
            buf.SetChar(cxEnd, y, '│', border);
        }
    }
}
