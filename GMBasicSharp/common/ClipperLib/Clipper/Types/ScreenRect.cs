using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

public struct ScreenRect
{
    public int X { get; set; }
    public int Y { get; set; }
    public int W { get; set; }
    public int H { get; set; }

    public ScreenRect(int x, int y, int w, int h)
    {
        X = x; Y = y; W = w; H = h;
    }

    public int Right => X + W - 1;
    public int Bottom => Y + H - 1;

    public bool Contains(int x, int y) =>
        x >= X && x < X + W && y >= Y && y < Y + H;

    public ScreenRect? Intersect(ScreenRect o)
    {
        var x = Math.Max(X, o.X);
        var y = Math.Max(Y, o.Y);
        var r = Math.Min(Right, o.Right);
        var b = Math.Min(Bottom, o.Bottom);
        if (r < x || b < y) return null;
        return new ScreenRect(x, y, r - x + 1, b - y + 1);
    }
}
