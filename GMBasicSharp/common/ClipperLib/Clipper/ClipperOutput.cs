using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

public static class ClipperOutput
{
    private static ClipperState S() => ClipperGlobal.State;

    public static void SetColor(string spec)
    {
        var st = S();
        st.ColorSpec = spec;
        var pair = spec.Split(',').FirstOrDefault()?.Trim() ?? "W/N";
        var (fg, bg) = ColorParser.ParsePair(pair);
        st.Fg = fg;
        st.Bg = bg;
    }

    public static void SetColorTo(string spec) => SetColor(spec);

    public static int Row() => S().Row;
    public static int Col() => S().Col;

    public static void SetPos(int row, int col)
    {
        var st = S();
        st.Row = row;
        st.Col = col;
    }

    public static void DevPos(int row, int col) => SetPos(row, col);

    private static void PutChar(ClipperState st, int x, int y, char ch)
    {
        st.Buffer.SetChar(x, y, ch, st.Fg, st.Bg);
    }

    private static void FillRect(ClipperState st, int top, int left, int bottom, int right, char ch)
    {
        st.Buffer.FillRect(top, left, bottom, right, ch, st.Fg, st.Bg);
    }

    public static void Say(int row, int col, string expr)
    {
        var st = S();
        var nx = st.Buffer.SetStringN(col, row, expr, int.MaxValue, st.Fg, st.Bg);
        st.Row = row;
        st.Col = nx;
    }

    public static void SayPic(int row, int col, string expr, string picture)
    {
        var formatted = FormatPicture.Apply(expr, picture);
        Say(row, col, formatted);
    }

    public static void SayColor(int row, int col, string expr, string color)
    {
        var (fg, bg) = ColorParser.ParsePair(color);
        var st = S();
        var nx = st.Buffer.SetStringN(col, row, expr, int.MaxValue, fg, bg);
        st.Row = row;
        st.Col = nx;
    }

    public static void DevOut(string s)
    {
        var st = S();
        if (!ClipperPrinter.PrinterOnly())
        {
            var nx = st.Buffer.SetStringN(st.Col, st.Row, s, int.MaxValue, st.Fg, st.Bg);
            st.Col = nx;
        }
        ClipperPrinter.Write(s, newLine: false);
    }

    public static void DevOutPic(string s, string pic)
    {
        var formatted = FormatPicture.Apply(s, pic);
        DevOut(formatted);
    }

    public static void DispOut(string s) => DevOut(s);

    public static void QOut(string s)
    {
        var st = S();
        if (!ClipperPrinter.PrinterOnly())
        {
            st.Buffer.SetStringN(st.Col, st.Row, s, int.MaxValue, st.Fg, st.Bg);
            if (st.Row + 1 < st.Buffer.Height)
                st.Row++;
            st.Col = 0;
        }
        ClipperPrinter.Write(s, newLine: true);
    }

    public static void QQOut(string s) => DevOut(s);

    public static void TextBlock(string s)
    {
        foreach (var line in s.Split('\n'))
            QOut(line);
    }

    private static (char tl, char th, char tr, char vl, char br, char bh, char bl, char vr, char fill) ParseBorder(string border)
    {
        if (border.Length == 1)
        {
            var c = border[0];
            return (c, c, c, c, c, c, c, c, ' ');
        }
        if (border.Length >= 9)
            return (border[0], border[1], border[2], border[3], border[4],
                    border[5], border[6], border[7], border[8]);

        var def = KeyConstants.BORDER_SINGLE;
        return (def[0], def[1], def[2], def[3], def[4], def[5], def[6], def[7], def[8]);
    }

    private static void DrawBoxImpl(ClipperState st, int top, int left, int bottom, int right, string border)
    {
        var (tl, th, tr, vl, br, bh, bl, vr, fill) = ParseBorder(border);
        PutChar(st, left, top, tl);
        PutChar(st, right, top, tr);
        PutChar(st, left, bottom, bl);
        PutChar(st, right, bottom, br);
        for (var x = left + 1; x < right; x++)
        {
            PutChar(st, x, top, th);
            PutChar(st, x, bottom, bh);
        }
        for (var y = top + 1; y < bottom; y++)
        {
            PutChar(st, left, y, vl);
            PutChar(st, right, y, vr);
        }
        var fiTop = Math.Max(top + 1, 0);
        var fiLeft = Math.Max(left + 1, 0);
        var fiBottom = Math.Max(bottom - 1, 0);
        var fiRight = Math.Max(right - 1, 0);
        if (fiTop <= fiBottom && fiLeft <= fiRight)
            FillRect(st, fiTop, fiLeft, fiBottom, fiRight, fill);
    }

    public static void Box(int top, int left, int bottom, int right, string style)
    {
        var border = string.IsNullOrEmpty(style) ? KeyConstants.BORDER_SINGLE : style;
        var st = S();
        DrawBoxImpl(st, top, left, bottom, right, border);
    }

    public static void DispBox(int top, int left, int bottom, int right, string spec)
    {
        var border = string.IsNullOrEmpty(spec) ? KeyConstants.BORDER_SINGLE : spec;
        var st = S();
        DrawBoxImpl(st, top, left, bottom, right, border);
    }

    public static void ClearScreen()
    {
        var st = S();
        st.Buffer.Clear(st.Fg, st.Bg);
    }

    public static void ClearRegion(int top, int left, int bottom, int right)
    {
        var st = S();
        FillRect(st, top, left, bottom, right, ' ');
    }

    public static void Beep()
    {
        // Console.Beep() è supportato solo su Windows: su altri OS la teniamo silenziosa.
        if (S().RenderHook == null) return;
        try { Console.Beep(); } catch { }
    }

    public static void SetCursor(bool on) => S().CursorVisible = on;

    public static bool GetCursor() => S().CursorVisible;

    public static void Render()
    {
        S().InvokeRenderHook();
    }

    public static ScreenBuffer RenderToBuffer() =>
        S().Buffer.Clone();
}
