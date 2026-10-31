// FILE: CommonLib.Consoles.Tui.ScreenBuffer.cs — C-backed (gmb_screen_buffer_t, libgmb_gfx)
// FONDAMENTO A2: il canvas dei widget C# È il buffer del runtime C, così i widget C
// (gmb_form_*_draw) e i widget C# condividono lo stesso buffer. Direzione unica C#→C.
using System;
using System.Runtime.InteropServices;
using CommonLib.Screens;

namespace CommonLib.Consoles.Tui;

public class ScreenBuffer : IDisposable
{
    private const string Lib = "gmb_screen";   // buffer di schermo: lib dedicata (niente SDL)

    [DllImport(Lib)] private static extern IntPtr gmb_screen_new(int w, int h);
    [DllImport(Lib)] private static extern void gmb_screen_free(IntPtr b);
    [DllImport(Lib)] private static extern int gmb_screen_width(IntPtr b);
    [DllImport(Lib)] private static extern int gmb_screen_height(IntPtr b);
    [DllImport(Lib)] private static extern void gmb_screen_clear(IntPtr b, GColor fg, GColor bg);
    [DllImport(Lib)] private static extern void gmb_screen_setchar(IntPtr b, int x, int y, uint ch, GColor fg, GColor bg);
    [DllImport(Lib)] private static extern void gmb_screen_resize(IntPtr b, int w, int h);
    [DllImport(Lib)] private static extern IntPtr gmb_screen_clone(IntPtr b);
    [DllImport(Lib)] private static extern void gmb_screen_get_cell(IntPtr b, int x, int y, int[] out10);
    [DllImport(Lib)] private static extern void gmb_screen_set_cell(IntPtr b, int x, int y, int ch, int attrs,
        int fgr, int fgg, int fgb, int fgd, int bgr, int bgg, int bgb, int bgd);

    // gmb_cell_color_t { uint8 r,g,b; bool is_default; } = 4 byte.
    [StructLayout(LayoutKind.Sequential)]
    private struct GColor { public byte r, g, b, is_default; }
    private static GColor ToG(CellColor c) => new() { r = c.R, g = c.G, b = c.B, is_default = (byte)(c.IsDefault ? 1 : 0) };

    private IntPtr _b;
    public int Width { get; private set; }
    public int Height { get; private set; }

    /// <summary>Handle al <c>gmb_screen_buffer_t</c> nativo (per le API C che lo richiedono).</summary>
    public IntPtr Handle => _b;

    /// <summary>Vista a celle del buffer C (sintassi <c>buf.Cells[y,x]</c>).</summary>
    public CellsView Cells { get; }

    public ScreenBuffer(int width, int height)
    {
        Width = width; Height = height;
        _b = gmb_screen_new(width, height);
        Cells = new CellsView(this);
    }

    private ScreenBuffer(IntPtr c)
    {
        _b = c;
        Width = c == IntPtr.Zero ? 0 : gmb_screen_width(c);
        Height = c == IntPtr.Zero ? 0 : gmb_screen_height(c);
        Cells = new CellsView(this);
    }

    public void Resize(int width, int height)
    {
        if (_b == IntPtr.Zero) return;
        gmb_screen_resize(_b, width, height);
        Width = width; Height = height;
    }

    public void Clear(CellColor? fg = null, CellColor? bg = null)
        => gmb_screen_clear(_b, ToG(fg ?? CellColor.White), ToG(bg ?? CellColor.Black));

    internal ScreenCell GetCell(int x, int y)
    {
        var buf = new int[10];
        gmb_screen_get_cell(_b, x, y, buf);
        var fg = new CellColor { R = (byte)buf[1], G = (byte)buf[2], B = (byte)buf[3], IsDefault = buf[4] != 0 };
        var bg = new CellColor { R = (byte)buf[5], G = (byte)buf[6], B = (byte)buf[7], IsDefault = buf[8] != 0 };
        return new ScreenCell(buf[0] == 0 ? ' ' : (char)buf[0], fg, bg);
    }

    internal void SetCell(int x, int y, ScreenCell cell)
    {
        var fg = cell.ForegroundRgb ?? CellColor.White;
        var bg = cell.BackgroundRgb ?? CellColor.Black;
        gmb_screen_set_cell(_b, x, y, cell.Character, 0, fg.R, fg.G, fg.B, fg.IsDefault ? 1 : 0, bg.R, bg.G, bg.B, bg.IsDefault ? 1 : 0);
    }

    public void SetChar(int x, int y, char ch, CellColor? fg = null, CellColor? bg = null)
    {
        if (x < 0 || x >= Width || y < 0 || y >= Height) return;
        ScreenCell cur = default;
        if (fg is null || bg is null) cur = GetCell(x, y);
        var f = fg ?? cur.ForegroundRgb ?? CellColor.White;
        var b = bg ?? cur.BackgroundRgb ?? CellColor.Black;
        gmb_screen_setchar(_b, x, y, ch, ToG(f), ToG(b));
    }

    public void SetString(int x, int y, string s, CellColor? fg = null, CellColor? bg = null)
    {
        for (var i = 0; i < s.Length; i++)
            SetChar(x + i, y, s[i], fg, bg);
    }

    public void FillRect(int top, int left, int bottom, int right, char ch,
                         CellColor? fg = null, CellColor? bg = null)
    {
        for (var y = top; y <= bottom && y < Height; y++)
            for (var x = left; x <= right && x < Width; x++)
                SetChar(x, y, ch, fg, bg);
    }

    public int SetStringN(int x, int y, string s, int maxLen, CellColor? fg = null, CellColor? bg = null)
    {
        var n = Math.Min(s.Length, maxLen);
        for (var i = 0; i < n; i++)
            SetChar(x + i, y, s[i], fg, bg);
        return x + n;
    }

    public ScreenBuffer Clone()
    {
        if (_b == IntPtr.Zero) return new ScreenBuffer(Width, Height);
        return new ScreenBuffer(gmb_screen_clone(_b));
    }

    public void Dispose()
    {
        if (_b != IntPtr.Zero) { gmb_screen_free(_b); _b = IntPtr.Zero; }
        GC.SuppressFinalize(this);
    }

    ~ScreenBuffer() { if (_b != IntPtr.Zero) gmb_screen_free(_b); }

    /// <summary>Vista a celle (indexer su buffer C).</summary>
    public sealed class CellsView
    {
        private readonly ScreenBuffer _b;
        internal CellsView(ScreenBuffer b) => _b = b;
        public ScreenCell this[int y, int x]
        {
            get => _b.GetCell(x, y);
            set => _b.SetCell(x, y, value);
        }
        public int Length => _b.Width * _b.Height;
        public int GetLength(int dim) => dim == 0 ? _b.Height : _b.Width;
    }
}
