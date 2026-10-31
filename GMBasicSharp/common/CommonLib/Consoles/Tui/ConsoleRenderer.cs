using CommonLib.Screens;
using System.Text;

namespace CommonLib.Consoles.Tui;

public static class ConsoleRenderer
{
    private static readonly StringBuilder Sb = new();

    public static void Write(ScreenBuffer buf)
    {
        // Console attiva (Sdl/Standard/System): copia le celle nel buffer della console e
        // presenta — NIENTE ANSI su System.Console (la console non è fissa).
        if (ActiveConsoleIO.Console is { } con)
        {
            int cw = Math.Min(buf.Width, con.BufferWidth);
            int chh = Math.Min(buf.Height, con.BufferHeight);
            if (cw > 0 && chh > 0)
            {
                var cells = new ScreenCell[chh, cw];
                for (var y = 0; y < chh; y++)
                    for (var x = 0; x < cw; x++)
                        cells[y, x] = buf.Cells[y, x];
                try { con.Put(0, 0, cells); } catch { }
            }
            try { con.Present(); } catch { }
            return;
        }

        int termH; int termW;
        try { termH = Console.WindowHeight; termW = Console.WindowWidth; }
        catch { termH = -1; termW = -1; }
        if (termH <= 0) termH = buf.Height;
        if (termW <= 0) termW = buf.Width;
        var h = Math.Min(buf.Height, termH);
        var w = Math.Min(buf.Width, termW);
        try { Console.SetCursorPosition(0, 0); } catch { }
        var prevFg = CellColor.Default;
        var prevBg = CellColor.Default;
        for (var y = 0; y < h; y++)
        {
            try { Console.SetCursorPosition(0, y); } catch { continue; }
            Sb.Clear();
            for (var x = 0; x < w; x++)
            {
                var cell = buf.Cells[y, x];
                var fg = cell.ForegroundRgb ?? CellColor.White;
                var bg = cell.BackgroundRgb ?? CellColor.Black;
                if (!fg.Equals(prevFg))
                {
                    Sb.Append(AnsiFg(fg));
                    prevFg = fg;
                }
                if (!bg.Equals(prevBg))
                {
                    Sb.Append(AnsiBg(bg));
                    prevBg = bg;
                }
                Sb.Append(cell.Character == '\0' ? ' ' : cell.Character);
            }
            if (w < termW)
            {
                if (!prevBg.IsDefault)
                {
                    Sb.Append("\x1b[49m");
                    prevBg = CellColor.Default;
                }
                if (!prevFg.IsDefault)
                {
                    Sb.Append("\x1b[39m");
                    prevFg = CellColor.Default;
                }
                Sb.Append(' ', termW - w);
            }
            try { Console.Write(Sb.ToString()); } catch { }
        }
        for (var y = h; y < termH; y++)
        {
            try
            {
                Console.SetCursorPosition(0, y);
                Console.Write("\x1b[0m" + new string(' ', termW));
            }
            catch { }
        }
        try { Console.Write("\x1b[0m"); } catch { }
    }

    /// <summary>
    /// Posiziona il cursore di sistema dopo un frame e lo mostra/nasconde secondo
    /// <paramref name="visible"/> (usato per le <see cref="IApp"/> che non disegnano
    /// il cursore a cella).
    /// </summary>
    public static void PositionCursor(int x, int y, bool visible)
    {
        if (ActiveConsoleIO.HasConsole)
        {
            ActiveConsoleIO.CursorVisible = visible;
            if (visible) ActiveConsoleIO.SetCursorPosition(Math.Max(0, x), Math.Max(0, y));
            ActiveConsoleIO.Present();
            return;
        }
        try
        {
            if (!visible)
            {
                Console.Write("\x1b[?25l");
                return;
            }
            Console.Write("\x1b[?25h");
            Console.SetCursorPosition(Math.Max(0, x), Math.Max(0, y));
        }
        catch { }
    }

    private static string AnsiFg(CellColor c) =>
        c.IsDefault ? "\x1b[39m" : $"\x1b[38;2;{c.R};{c.G};{c.B}m";

    private static string AnsiBg(CellColor c) =>
        c.IsDefault ? "\x1b[49m" : $"\x1b[48;2;{c.R};{c.G};{c.B}m";
}
