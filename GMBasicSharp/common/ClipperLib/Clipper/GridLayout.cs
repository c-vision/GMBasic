using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>Primary split axis of a grid layout.</summary>
public enum SplitStrategy
{
    /// <summary>Split the area into columns first, then each column into rows.</summary>
    Horizontal,
    /// <summary>Split the area into rows first, then each row into columns.</summary>
    Vertical,
}

/// <summary>How space is distributed when column/row sizes do not fill the area.</summary>
public enum DividerStrategy
{
    /// <summary>Distribute the remaining space equally across all tracks.</summary>
    Even,
    /// <summary>Divide space proportionally to the supplied sizes.</summary>
    Proportional,
}

/// <summary>Text trim/truncation strategy (clean-room port of grid_ui's TrimStrategy).</summary>
public enum TrimStrategy
{
    /// <summary>Leave the text unchanged.</summary>
    None,
    /// <summary>Truncate to width, left-aligned (TrimStart).</summary>
    TrimStart,
    /// <summary>Truncate to width, right-aligned (TrimEnd).</summary>
    TrimEnd,
}

/// <summary>
/// Grid layout engine (clean-room port of grid_ui, MIT). Subdivides a
/// <see cref="Rect"/> into a grid of child rects from explicit column/row sizes
/// and a split/divider strategy. Pure, no terminal/OS dependency.
/// </summary>
public class GridLayout
{
    /// <summary>Column sizes (proportional weights or absolute hints).</summary>
    public List<int> Columns { get; set; } = [];

    /// <summary>Row sizes (proportional weights or absolute hints).</summary>
    public List<int> Rows { get; set; } = [];

    /// <summary>Primary split direction.</summary>
    public SplitStrategy Split { get; set; } = SplitStrategy.Horizontal;

    /// <summary>How leftover space is distributed.</summary>
    public DividerStrategy Divider { get; set; } = DividerStrategy.Proportional;

    /// <summary>Computes the child rects for <paramref name="area"/>.</summary>
    public List<Rect> Layout(Rect area)
    {
        var colW = ComputeSizes(Columns, area.W);
        var rowH = ComputeSizes(Rows, area.H);
        var res = new List<Rect>(colW.Count * rowH.Count);
        var x = area.X;
        foreach (var cw in colW)
        {
            var y = area.Y;
            foreach (var rh in rowH)
            {
                res.Add(new Rect(x, y, Math.Max(0, cw), Math.Max(0, rh)));
                y += rh;
            }
            x += cw;
        }
        return res;
    }

    private List<int> ComputeSizes(List<int> sizes, int total)
    {
        var outp = new List<int>(sizes.Count);
        if (sizes.Count == 0)
            return outp;
        var sum = 0;
        foreach (var s in sizes) sum += s;
        if (sum <= 0) sum = 1;
        var used = 0;
        for (var i = 0; i < sizes.Count; i++)
        {
            var sz = Divider == DividerStrategy.Even
                ? total / sizes.Count
                : (int)((long)sizes[i] * total / sum);
            if (i == sizes.Count - 1)
                sz = total - used; // last track absorbs any remainder
            outp.Add(Math.Max(0, sz));
            used += sz;
        }
        return outp;
    }
}
