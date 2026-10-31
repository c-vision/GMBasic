using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// 2D coordinate (TTY-free, matches Rust geometry::Point).
/// </summary>
public struct Point : IEquatable<Point>
{
    public int X;
    public int Y;

    public Point(int x, int y)
    {
        X = x;
        Y = y;
    }

    public static Point New(int x, int y) => new Point(x, y);

    public readonly bool Equals(Point other) => X == other.X && Y == other.Y;
    public override readonly bool Equals(object? obj) => obj is Point p && Equals(p);
    public override readonly int GetHashCode() => HashCode.Combine(X, Y);
    public override readonly string ToString() => $"Point({X},{Y})";
}

/// <summary>
/// Rectangle with x,y,w,h (TTY-free, matches Rust geometry::Rect).
/// Distinct from ScreenRect; implicit conversions provided for interop.
/// </summary>
public struct Rect : IEquatable<Rect>
{
    public int X;
    public int Y;
    public int W;
    public int H;

    public Rect(int x, int y, int w, int h)
    {
        X = x; Y = y; W = w; H = h;
    }

    public static Rect New(int x, int y, int w, int h) => new Rect(x, y, w, h);

    public static Rect FromU16(ushort x, ushort y, ushort w, ushort h) =>
        new Rect(x, y, w, h);

    public readonly ushort Width => (ushort)W;
    public readonly ushort Height => (ushort)H;
    public readonly int Right => X + W - 1;
    public readonly int Bottom => Y + H - 1;

    public readonly Point Center => Point.New(X + W / 2, Y + H / 2);

    public readonly bool Contains(ushort x, ushort y)
    {
        int xi = x, yi = y;
        return xi >= X && xi < X + W && yi >= Y && yi < Y + H;
    }

    public readonly bool ContainsPoint(int x, int y) =>
        x >= X && x < X + W && y >= Y && y < Y + H;

    public readonly Rect? Intersect(Rect o)
    {
        var x = Math.Max(X, o.X);
        var y = Math.Max(Y, o.Y);
        var r = Math.Min(Right, o.Right);
        var b = Math.Min(Bottom, o.Bottom);
        if (r < x || b < y) return null;
        return new Rect(x, y, r - x + 1, b - y + 1);
    }

    public readonly Rect Union(Rect other)
    {
        var x1 = Math.Min(X, other.X);
        var y1 = Math.Min(Y, other.Y);
        var x2 = Math.Max(Right, other.Right);
        var y2 = Math.Max(Bottom, other.Bottom);
        return new Rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    }

    public static implicit operator ScreenRect(Rect r) => new ScreenRect(r.X, r.Y, r.W, r.H);
    public static implicit operator Rect(ScreenRect r) => new Rect(r.X, r.Y, r.W, r.H);

    public readonly bool Equals(Rect other) =>
        X == other.X && Y == other.Y && W == other.W && H == other.H;
    public override readonly bool Equals(object? obj) => obj is Rect r && Equals(r);
    public override readonly int GetHashCode() => HashCode.Combine(X, Y, W, H);
    public override readonly string ToString() => $"Rect({X},{Y},{W},{H})";
}

/// <summary>
/// Snap target for a dragged window (matches Rust geometry::SnapZone).
/// </summary>
public enum SnapZone
{
    Left,
    Right,
}

public static class Geometry
{
    public static SnapZone? SnapZone(Point p, Rect screen, int threshold)
    {
        if (p.X < screen.X + threshold) return Clipper.SnapZone.Left;
        if (p.X > screen.Right - threshold) return Clipper.SnapZone.Right;
        return null;
    }

    public static Rect SnappedRect(Clipper.SnapZone zone, Rect work)
    {
        var half = work.W / 2;
        return zone == Clipper.SnapZone.Left
            ? new Rect(work.X, work.Y, half, work.H)
            : new Rect(work.X + half, work.Y, work.W - half, work.H);
    }
}

/// <summary>
/// Tiled layout grid (matches Rust geometry::Grid).
/// </summary>
public struct Grid : IEquatable<Grid>
{
    public byte Rows;
    public byte Cols;

    public Grid(byte rows, byte cols)
    {
        Rows = rows;
        Cols = cols;
    }

    public static Grid New(byte rows, byte cols) => new Grid(rows, cols);

    private readonly int RowsI => Math.Max(1, (int)Rows);
    private readonly int ColsI => Math.Max(1, (int)Cols);

    public readonly byte Cells => (byte)(RowsI * ColsI);

    public readonly Rect CellRect(Rect work, byte row, byte col, int gap)
    {
        int rows = RowsI, cols = ColsI;
        int r = Math.Min(row, (byte)(rows - 1));
        int c = Math.Min(col, (byte)(cols - 1));
        int rI = r, cI = c;
        var cw = Math.Max(cols, work.W - gap * (cols - 1)) / cols;
        var ch = Math.Max(rows, work.H - gap * (rows - 1)) / rows;
        var x = work.X + cI * (cw + gap);
        var y = work.Y + rI * (ch + gap);
        var w = cI == cols - 1 ? work.X + work.W - x : cw;
        var h = rI == rows - 1 ? work.Y + work.H - y : ch;
        return new Rect(x, y, Math.Max(1, w), Math.Max(1, h));
    }

    public readonly (byte Row, byte Col) CellAt(Rect work, Point p)
    {
        int rows = RowsI, cols = ColsI;
        var dx = Math.Clamp(p.X - work.X, 0, work.W - 1);
        var dy = Math.Clamp(p.Y - work.Y, 0, work.H - 1);
        var col = Math.Clamp(dx * cols / Math.Max(1, work.W), 0, cols - 1);
        var row = Math.Clamp(dy * rows / Math.Max(1, work.H), 0, rows - 1);
        return ((byte)row, (byte)col);
    }

    public readonly byte IndexOf(byte row, byte col) => (byte)(row * ColsI + col);

    public readonly (byte Row, byte Col) RowCol(byte index)
    {
        var cols = ColsI;
        return ((byte)(index / cols), (byte)(index % cols));
    }

    public readonly bool Equals(Grid other) => Rows == other.Rows && Cols == other.Cols;
    public override readonly bool Equals(object? obj) => obj is Grid g && Equals(g);
    public override readonly int GetHashCode() => HashCode.Combine(Rows, Cols);
    public override readonly string ToString() => $"Grid({Rows}x{Cols})";
}
