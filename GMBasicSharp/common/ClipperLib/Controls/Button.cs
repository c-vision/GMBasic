using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class Button : Control
{
    public string Label { get; set; } = "OK";
    public Action? OnClick { get; set; }
    /// <summary>Bottone di default (Borland: sfondo verde).</summary>
    public bool IsDefault { get; set; }

    public Button()
    {
        Width = 10;
        Height = 1;
    }

    public Button WithLabel(string label) { Label = label; return this; }
    public Button WithOnClick(Action onClick) { OnClick = onClick; return this; }
    public Button WithDefault(bool d = true) { IsDefault = d; return this; }

    public string RenderLine()
    {
        var inner = Math.Max(0, Width - 2);
        var text = Label.Length > inner ? Label[..inner] : Label;
        var pad = inner - text.Length;
        var leftPad = pad / 2;
        var rightPad = pad - leftPad;
        return "[" + new string(' ', leftPad) + text + new string(' ', rightPad) + "]";
    }

    public override void Draw(ScreenBuffer buffer)
    {
        // Render nativo (runtime C): unica fonte del disegno dei controlli.
        NativeDraw.Show(buffer, NativeDraw.Button, this, text: ApplyCase(Label));
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        if (key is KeyConstants.K_ENTER or ' ')
        {
            OnClick?.Invoke();
            return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        OnClick?.Invoke();
        return true;
    }
}