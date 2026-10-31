using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class TextInput : Control
{
    public string Text { get; set; } = "";
    public int Cursor { get; set; }
    public int MaxLength { get; set; } = 50;
    public string? Picture { get; set; }
    public Func<string, bool>? Validator { get; set; }
    public bool PasswordMode { get; set; }
    public bool ReadOnly { get; set; }

    public TextInput WithText(string text) { Text = text; Cursor = text.Length; return this; }
    public TextInput WithMaxLength(int max) { MaxLength = max; return this; }
    public TextInput WithPasswordMode(bool pw) { PasswordMode = pw; return this; }
    public TextInput WithValidator(Func<string, bool>? validator) { Validator = validator; return this; }

    public bool IsValid => Validator == null || Validator(Text);

    public string RenderLine()
    {
        var shown = PasswordMode ? new string('*', Text.Length) : ApplyCase(Text);
        if (shown.Length > Width) shown = shown[..Width];
        else if (shown.Length < Width) shown = shown.PadRight(Width);
        return shown;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var error = HasError || !IsValid;
        var (fg, bg) = Style.Effective(Focused, error);
        var fill = new string(Style.FillChar, Width);
        buffer.SetString(X, Y, fill, fg, bg);

        var shown = PasswordMode ? new string('*', Text.Length) : ApplyCase(Text);
        if (Picture != null) shown = FormatPicture.Apply(Text, Picture);
        for (var i = 0; i < Math.Min(shown.Length, Width); i++)
            buffer.SetChar(X + i, Y, shown[i], fg, bg);

        if (Focused && Cursor < Width && BlinkOn)
        {
            var c = Cursor < shown.Length ? shown[Cursor] : Style.FillChar;
            buffer.SetChar(X + Cursor, Y, c, bg, fg);
        }
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled || ReadOnly) return false;

        if (key is >= 32 and <= 126)
        {
            if (Text.Length < MaxLength)
            {
                Text = Text.Insert(Cursor, ((char)key).ToString());
                Cursor++;
            }
            return true;
        }

        switch (key)
        {
            case KeyConstants.K_BS:
                if (Cursor > 0)
                {
                    Text = Text.Remove(Cursor - 1, 1);
                    Cursor--;
                }
                return true;
            case KeyConstants.K_DEL:
                if (Cursor < Text.Length)
                    Text = Text.Remove(Cursor, 1);
                return true;
            case KeyConstants.K_LEFT:
                if (Cursor > 0) Cursor--;
                return true;
            case KeyConstants.K_RIGHT:
                if (Cursor < Text.Length) Cursor++;
                return true;
            case KeyConstants.K_HOME: Cursor = 0; return true;
            case KeyConstants.K_END: Cursor = Text.Length; return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        var clickCol = x - X;
        if (clickCol >= 0) Cursor = Math.Min(clickCol, Text.Length);
        return true;
    }
}