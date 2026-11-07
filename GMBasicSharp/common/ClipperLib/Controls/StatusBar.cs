using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class StatusBar : Control
{
    public string Text { get; set; } = "";

    public StatusBar()
    {
        Height = 1;
    }

    public StatusBar WithText(string text) { Text = text; return this; }

    public string RenderLine()
    {
        var line = Text.Length > Width ? Text[..Width] : Text;
        return line.Length < Width ? line.PadRight(Width) : line;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.StatusBar, this, text: ApplyCase(Text));
    }

    /// <summary>Riconosce un token di tasto (F1..F12 / Alt-X / Ctrl-X / Shift-X) alla posizione i.</summary>
    private static int MatchKeyToken(string s, int i)
    {
        if (i + 1 < s.Length && s[i] == 'F' && char.IsDigit(s[i + 1]))
        {
            var j = i + 1; while (j < s.Length && char.IsDigit(s[j])) j++; return j - i;
        }
        foreach (var p in new[] { "Alt-", "Ctrl-", "Shift-" })
            if (string.Compare(s, i, p, 0, p.Length, true) == 0) { var j = i + p.Length; while (j < s.Length && (char.IsLetterOrDigit(s[j]) || s[j] == '-')) j++; return j - i; }
        return 0;
    }

    public override bool HandleKey(int key) => false;
}