using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public enum Alignment { Left, Center, Right }

public class Label : Control
{
    public string Text { get; set; } = "";
    public Alignment Align { get; set; } = Alignment.Left;

    public Label WithText(string text) { Text = text; return this; }
    public Label WithAlign(Alignment align) { Align = align; return this; }

    public string RenderLine()
    {
        var w = Width;
        var len = Text.Length;
        if (len >= w) return Text[..w];
        var pad = w - len;
        return Align switch
        {
            Alignment.Right => new string(' ', pad) + Text,
            Alignment.Center => new string(' ', pad / 2) + Text + new string(' ', pad - pad / 2),
            _ => Text + new string(' ', pad),
        };
    }

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.Label, this, text: ApplyCase(Text), sel: (int)Align);
    }

    public override bool HandleKey(int key) => false;
}