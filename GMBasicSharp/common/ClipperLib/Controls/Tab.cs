using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class Tab : Control
{
    public List<string> Tabs { get; set; } = [];
    public int Active { get; set; }

    public Tab WithTabs(List<string> tabs) { Tabs = tabs; return this; }

    public string RenderLine()
    {
        var result = "";
        for (var i = 0; i < Tabs.Count; i++)
        {
            var tb = $" {Tabs[i]} ";
            if ((result + tb).Length > Width) break;
            result += tb;
        }
        return result.Length < Width ? result.PadRight(Width) : result;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(Focused, HasError);
        var (activeFg, activeBg) = Style.Effective(selected: true);

        var cx = X;
        for (var i = 0; i < Tabs.Count; i++)
        {
            var tb = $" {ApplyCase(Tabs[i])} ";
            var (f, b) = i == Active ? (activeFg, activeBg) : (fg, bg);
            if (cx + tb.Length > X + Width) break;
            buffer.SetString(cx, Y, tb, f, b);
            cx += tb.Length;
        }
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled || Tabs.Count == 0) return false;
        switch (key)
        {
            case KeyConstants.K_LEFT:
                if (Active > 0) Active--;
                return true;
            case KeyConstants.K_RIGHT:
                if (Active < Tabs.Count - 1) Active++;
                return true;
            case KeyConstants.K_HOME: Active = 0; return true;
            case KeyConstants.K_END: Active = Tabs.Count - 1; return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        var cx = X;
        for (var i = 0; i < Tabs.Count; i++)
        {
            var tb = $" {Tabs[i]} ";
            if (cx + tb.Length > X + Width) break;
            if (x >= cx && x < cx + tb.Length) { Active = i; return true; }
            cx += tb.Length;
        }
        return true;
    }
}