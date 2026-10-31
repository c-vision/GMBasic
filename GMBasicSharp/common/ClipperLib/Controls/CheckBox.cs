using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class CheckBox : Control
{
    public bool Checked { get; set; }
    public string Label { get; set; } = "";

    public CheckBox WithChecked(bool c) { Checked = c; return this; }
    public CheckBox WithLabel(string label) { Label = label; return this; }

    public string RenderLine()
    {
        var mark = Checked ? "[X]" : "[ ]";
        var s = $"{mark} {Label}";
        if (s.Length > Width) s = s[..Width];
        else if (s.Length < Width) s = s.PadRight(Width);
        return s;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(Focused, HasError);
        buffer.SetString(X, Y, ApplyCase(RenderLine()), fg, bg);
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        if (key is ' ' or KeyConstants.K_ENTER)
        {
            Checked = !Checked;
            return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        Checked = !Checked;
        return true;
    }
}