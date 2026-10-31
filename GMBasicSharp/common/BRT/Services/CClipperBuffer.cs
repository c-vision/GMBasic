// FILE: Basic.Runtime.CClipperBuffer.cs
// Ponte C# → buffer del runtime C (`gmb_screen_buffer_t`, libgmb_gfx). È la FONDAZIONE
// per migrare i widget C# (ClipperLib.Controls / Clipper.Form*) ai controlli C: i widget
// C disegnano su questo buffer, renderizzabile via gmb_render_screen (ANSI) o copiabile
// in uno ScreenBuffer C# per i renderer esistenti. Direzione unica C# → C.
using System;
using System.Runtime.InteropServices;
using CommonLib.Consoles.Tui;
using CommonLib.Screens;

namespace Basic.Runtime;

/// <summary>Screen buffer C-backed (wrapper di <c>gmb_screen_buffer_t</c>).</summary>
public sealed class CClipperBuffer : IDisposable
{
    private const string Lib = "gmb_screen";   // buffer di schermo: lib dedicata (niente SDL)

    [DllImport(Lib)] private static extern IntPtr gmb_screen_new(int w, int h);
    [DllImport(Lib)] private static extern void gmb_screen_free(IntPtr b);
    [DllImport(Lib)] private static extern int gmb_screen_width(IntPtr b);
    [DllImport(Lib)] private static extern int gmb_screen_height(IntPtr b);
    [DllImport(Lib)] private static extern void gmb_screen_clear(IntPtr b, GColor fg, GColor bg);
    [DllImport(Lib)] private static extern void gmb_screen_setchar(IntPtr b, int x, int y, uint ch, GColor fg, GColor bg);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern int gmb_screen_setstring(IntPtr b, int x, int y, string s, GColor fg, GColor bg);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern int gmb_screen_setstring_n(IntPtr b, int x, int y, string s, int maxChars, GColor fg, GColor bg);
    [DllImport(Lib)] private static extern void gmb_screen_fillrect(IntPtr b, int top, int left, int bottom, int right, uint ch, GColor fg, GColor bg);
    [DllImport(Lib)] private static extern void gmb_screen_resize(IntPtr b, int w, int h);
    [DllImport(Lib)] private static extern void gmb_screen_get_cell(IntPtr b, int x, int y, int[] out10);

    // gmb_cell_color_t { uint8 r,g,b; bool is_default; } = 4 byte.
    [StructLayout(LayoutKind.Sequential)]
    private struct GColor { public byte r, g, b, is_default; }

    private IntPtr _b;

    public CClipperBuffer(int width, int height)
    {
        _b = gmb_screen_new(width, height);
    }

    public IntPtr Handle => _b;
    public int Width => _b == IntPtr.Zero ? 0 : gmb_screen_width(_b);
    public int Height => _b == IntPtr.Zero ? 0 : gmb_screen_height(_b);
    public bool IsValid => _b != IntPtr.Zero;

    private static GColor C(CellColor c) => new() { r = c.R, g = c.G, b = c.B, is_default = (byte)(c.IsDefault ? 1 : 0) };

    public void Clear(CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_clear(_b, C(fg ?? CellColor.White), C(bg ?? CellColor.Black));

    public void SetChar(int x, int y, char ch, CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_setchar(_b, x, y, ch, C(fg ?? CellColor.White), C(bg ?? CellColor.Black));

    public void SetString(int x, int y, string s, CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_setstring(_b, x, y, s ?? "", C(fg ?? CellColor.White), C(bg ?? CellColor.Black));

    public int SetStringN(int x, int y, string s, int maxLen, CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_setstring_n(_b, x, y, s ?? "", maxLen, C(fg ?? CellColor.White), C(bg ?? CellColor.Black));

    public void FillRect(int top, int left, int bottom, int right, char ch, CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_fillrect(_b, top, left, bottom, right, ch, C(fg ?? CellColor.White), C(bg ?? CellColor.Black));

    public void Resize(int w, int h) => gmb_screen_resize(_b, w, h);

    /// <summary>Copia il buffer C in uno ScreenBuffer C# (per i renderer esistenti).</summary>
    public ScreenBuffer ToManaged()
    {
        var sb = new ScreenBuffer(Math.Max(1, Width), Math.Max(1, Height));
        var cell = new int[10];
        for (int y = 0; y < Height; y++)
            for (int x = 0; x < Width; x++)
            {
                gmb_screen_get_cell(_b, x, y, cell);
                var fg = new CellColor { R = (byte)cell[1], G = (byte)cell[2], B = (byte)cell[3], IsDefault = cell[4] != 0 };
                var bg = new CellColor { R = (byte)cell[5], G = (byte)cell[6], B = (byte)cell[7], IsDefault = cell[8] != 0 };
                sb.Cells[y, x] = new ScreenCell(cell[0] == 0 ? ' ' : (char)cell[0], fg, bg);
            }
        return sb;
    }

    public void Dispose()
    {
        if (_b != IntPtr.Zero) { gmb_screen_free(_b); _b = IntPtr.Zero; }
    }
}
