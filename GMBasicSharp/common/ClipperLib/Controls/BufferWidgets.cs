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

    /// <summary>Draw a single-line box (┌─┐│└─┘) with optional title.
    /// Render nativo (runtime C): stessa geometria dei pannelli.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0) return;
        NativeDraw.Show(buf, NativeDraw.Panel, area.X, area.Y, area.W, area.H, text: Title);
    }
}

/// <summary>
/// Word-wrapped paragraph of <see cref="RichSpan"/>s.
/// </summary>
public class Paragraph
{
    /// <summary>Styled content spans.</summary>
    public List<RichSpan> Spans = [];

    /// <summary>Draw spans word-wrapped inside <paramref name="area"/> (render nel runtime C).</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0 || Spans.Count == 0 || buf.Handle == IntPtr.Zero) return;
        var arr = RichText.ToNative(Spans);
        NativeControlDraw.gmb_richtext_render_paragraph(buf.Handle, area.X, area.Y, area.W, area.H, arr, arr.Length, 1);
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

    /// <summary>Draw the list, scrolling so <see cref="Selected"/> stays visible.
    /// Render nativo (runtime C): stesso ListBox dei controlli.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0 || Items.Count == 0) return;
        NativeDraw.Show(buf, NativeDraw.ListBox, area.X, area.Y, area.W, area.H,
                        text: string.Join("|", Items), sel: Selected);
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

    /// <summary>Draw tab titles separated by spaces, highlighting <see cref="Selected"/>.
    /// Render nativo (runtime C): stessa barra Tab dei controlli.</summary>
    public void Draw(ScreenBuffer buf, Rect area)
    {
        if (area.W <= 0 || area.H <= 0 || Titles.Count == 0) return;
        NativeDraw.Show(buf, NativeDraw.Tab, area.X, area.Y, area.W, 1,
                        text: string.Join("|", Titles), sel: Selected);
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

        var colW = Math.Max(2, (area.W - 1) / cols);
        var hdrs = new List<string>();
        for (var c = 0; c < cols; c++)
            hdrs.Add($"{(c < Headers.Count ? Headers[c] : "")}:{colW}");

        var sb = new System.Text.StringBuilder();
        for (var r = 0; r < Rows.Count && r < 64; r++)
        {
            if (r > 0) sb.Append(';');
            for (var c = 0; c < cols; c++)
            {
                if (c > 0) sb.Append('|');
                sb.Append(c < Rows[r].Count ? Rows[r][c] : "");
            }
        }
        // Render nativo (runtime C): stessa griglia del TBrowse.
        NativeDraw.Show(buf, NativeDraw.TBrowse, area.X, area.Y, area.W, area.H,
                        text: string.Join("|", hdrs), extra: sb.ToString());
    }
}
