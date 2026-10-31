using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Renderizza la <see cref="Emulator"/> in un <see cref="ScreenBuffer"/>.
/// Le celle di continuazione (wide char) vengono saltate; il reverse video
/// scambia fg/bg; se <c>drawCursor</c> e attivo, la cella del cursore
/// viene evidenziata (colori invertiti).
/// </summary>
public static class EmuRender
{
    /// <summary>Copia lo schermo dell'emulatore nel buffer (cursore disegnato).</summary>
    public static void ToBuffer(this Emulator emu, ScreenBuffer buf)
        => ToBuffer(emu, buf, true, 0, 0);

    /// <summary>Copia lo schermo dell'emulatore nel buffer.</summary>
    public static void ToBuffer(this Emulator emu, ScreenBuffer buf, bool drawCursor)
        => ToBuffer(emu, buf, drawCursor, 0, 0);

    /// <summary>Copia lo schermo dell'emulatore nel buffer con un offset (x,y).</summary>
    public static void ToBuffer(this Emulator emu, ScreenBuffer buf, int offX, int offY)
        => ToBuffer(emu, buf, true, offX, offY);

    private static void ToBuffer(Emulator emu, ScreenBuffer buf, bool drawCursor, int offX, int offY)
    {
        var grid = emu.Grid;
        var w = Math.Min(grid.Cols, buf.Width - offX);
        var h = Math.Min(grid.Rows, buf.Height - offY);
        var cur = emu.Cursor();
        var drawCur = drawCursor && grid.CursorVisible;

        for (var y = 0; y < h; y++)
        {
            for (var x = 0; x < w; x++)
            {
                var cell = grid.CellAt(x, y);
                if (cell.IsContinuation) continue;

                var fg = cell.Style.Fg ?? CellColor.Default;
                var bg = cell.Style.Bg ?? CellColor.Default;
                if (cell.Style.Reverse) (fg, bg) = (bg, fg);
                if (drawCur && x == cur.X && y == cur.Y)
                    (fg, bg) = (bg, fg);

                buf.SetChar(x + offX, y + offY, cell.Char, fg, bg);
            }
        }
    }
}
