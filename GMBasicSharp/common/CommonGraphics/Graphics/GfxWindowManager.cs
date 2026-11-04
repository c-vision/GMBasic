// FILE: CommonGraphics.Graphics.GfxWindowManager.cs
//
// Window manager PIXEL — <b>proxy P/Invoke</b> sul runtime C.
//
// Lo stato (finestre, z-order, focus, id, desktop, drag/resize, colori, contenuto) vive
// SOLO in C: `gmb_window_manager_static()` (rect/ordine/stato) + il modello `gmb_winstmt_*`
// (DEFINE, compositor, hit-test, mouse). Questa classe non tiene nessuna lista né uno stato
// duplicato: la vecchia implementazione C# (777 righe con stato e renderer propri) è
// archiviata in GMBasicMonoRepo-Deprecated/CommonGraphics/Graphics/GfxWindowManager.cs.
//
// La geometria è in PIXEL LOGICI (title 24, padding client 6, pulsanti a destra, bordo
// resize 4 px) come nel vecchio compositor: le API C espongono esattamente questi valori.

using Basic;
using SDL2;

namespace CommonGraphics.Graphics;

/// <summary>Stato di una finestra pixel.</summary>
public enum GfxWindowState
{
    Floating,
    SnappedLeft,
    SnappedRight,
    Maximized,
    Tiled,
    Minimized
}

/// <summary>Zona di snap pixel: metà sinistra o destra.</summary>
public enum GfxSnapZone
{
    Left,
    Right
}

/// <summary>Finestra pixel: binding sottile sulla finestra C (gmb_managed_window_t).</summary>
public sealed class PixelWindow
{
    // 0 = istanza STACCATA (snapshot locale, es. SAVESCREEN). != 0 → tutte le proprietà
    // leggono/scrivono la finestra C.
    internal IntPtr C;

    private int _id, _x, _y, _w, _h, _z, _state, _desktop = 1, _scroll, _curLine, _curCol;
    private int _rx, _ry, _rw, _rh, _radius = 6;
    private bool _minimized, _focused, _closable = true, _draggable = true, _resizable = true, _shadow = true;
    private uint _bodyColor = 0xFF14161C, _fgColor = 0xFFD0D6E0, _titleColor = 0xFFD6DCE6;
    private string _title = "", _name = "";
    private List<string> _localLines = [];

    // id dei campi del binding C (gmb_tui_windowmanager.c: enum GMB_MWF_*)
    private const int F_X = 0, F_Y = 1, F_W = 2, F_H = 3, F_Z = 4, F_STATE = 5,
        F_SHADOW = 8, F_FOCUSED = 9, F_MINIMIZED = 10, F_DESKTOP = 11,
        F_CURSOR_LINE = 12, F_CURSOR_COL = 13, F_SCROLL = 14, F_CLOSABLE = 16,
        F_DRAGGABLE = 17, F_ID = 18, F_RX = 19, F_RY = 20, F_RW = 21, F_RH = 22,
        F_EDITABLE = 23, F_BODY_COLOR = 24, F_FG_COLOR = 25, F_TITLE_COLOR = 26,
        F_PIXEL = 27, F_RADIUS = 28, F_RESIZABLE = 29;

    private int Get(int f, int local) => C != IntPtr.Zero ? GfxWmNative.GetInt(C, f) : local;
    private void Set(int f, int v) { if (C != IntPtr.Zero) GfxWmNative.SetInt(C, f, v); }

    public int Id { get => Get(F_ID, _id); set => _id = value; }
    public string Name
    {
        get => C != IntPtr.Zero ? GfxWmNative.GetStr(C, 1) : _name;
        set { _name = value ?? ""; if (C != IntPtr.Zero) GfxWmNative.SetStr(C, 1, _name); }
    }
    public string Title
    {
        get => C != IntPtr.Zero ? GfxWmNative.GetStr(C, 0) : _title;
        set { _title = value ?? ""; if (C != IntPtr.Zero) GfxWmNative.SetStr(C, 0, _title); }
    }
    public int X { get => Get(F_X, _x); set { _x = value; Set(F_X, value); } }
    public int Y { get => Get(F_Y, _y); set { _y = value; Set(F_Y, value); } }
    public int W { get => Get(F_W, _w); set { _w = value; Set(F_W, value); } }
    public int H { get => Get(F_H, _h); set { _h = value; Set(F_H, value); } }
    public int Z { get => Get(F_Z, _z); set { _z = value; Set(F_Z, value); } }

    /// <summary>Stato: mappato fra GfxWindowState (C#) e stati C (0 floating, 1 maximized,
    /// 2 minimized, 3 snapped-left, 4 snapped-right, 5 tiled).</summary>
    public GfxWindowState State
    {
        get
        {
            if (Get(F_MINIMIZED, _minimized ? 1 : 0) != 0) return GfxWindowState.Minimized;
            return Get(F_STATE, _state) switch
            {
                1 => GfxWindowState.Maximized,
                3 => GfxWindowState.SnappedLeft,
                4 => GfxWindowState.SnappedRight,
                5 => GfxWindowState.Tiled,
                _ => GfxWindowState.Floating,
            };
        }
        set
        {
            _state = (int)value;
            if (C == IntPtr.Zero) { _minimized = value == GfxWindowState.Minimized; return; }
            Set(F_MINIMIZED, value == GfxWindowState.Minimized ? 1 : 0);
            Set(F_STATE, value switch
            {
                GfxWindowState.Maximized => 1,
                GfxWindowState.SnappedLeft => 3,
                GfxWindowState.SnappedRight => 4,
                GfxWindowState.Tiled => 5,
                _ => 0,
            });
        }
    }

    public int RestoreX { get => Get(F_RX, _rx); set { _rx = value; Set(F_RX, value); } }
    public int RestoreY { get => Get(F_RY, _ry); set { _ry = value; Set(F_RY, value); } }
    public int RestoreW { get => Get(F_RW, _rw); set { _rw = value; Set(F_RW, value); } }
    public int RestoreH { get => Get(F_RH, _rh); set { _rh = value; Set(F_RH, value); } }
    public bool Focused { get => Get(F_FOCUSED, _focused ? 1 : 0) != 0; set { _focused = value; Set(F_FOCUSED, value ? 1 : 0); } }
    public bool Shadow { get => Get(F_SHADOW, _shadow ? 1 : 0) != 0; set { _shadow = value; Set(F_SHADOW, value ? 1 : 0); } }
    public int BorderRadius { get => Get(F_RADIUS, _radius); set { _radius = value; Set(F_RADIUS, value); } }
    public bool Closable { get => Get(F_CLOSABLE, _closable ? 1 : 0) != 0; set { _closable = value; Set(F_CLOSABLE, value ? 1 : 0); } }
    public bool Draggable { get => Get(F_DRAGGABLE, _draggable ? 1 : 0) != 0; set { _draggable = value; Set(F_DRAGGABLE, value ? 1 : 0); } }
    public bool Resizable { get => Get(F_RESIZABLE, _resizable ? 1 : 0) != 0; set { _resizable = value; Set(F_RESIZABLE, value ? 1 : 0); } }
    public int Desktop { get => Get(F_DESKTOP, _desktop); set { _desktop = value; Set(F_DESKTOP, value); } }

    /// <summary>Colori per-finestra (ARGB). 0 = colore del tema.</summary>
    public uint BodyColor { get => (uint)Get(F_BODY_COLOR, (int)_bodyColor); set { _bodyColor = value; Set(F_BODY_COLOR, (int)value); } }
    public uint FgColor { get => (uint)Get(F_FG_COLOR, (int)_fgColor); set { _fgColor = value; Set(F_FG_COLOR, (int)value); } }
    public uint TitleColor { get => (uint)Get(F_TITLE_COLOR, (int)_titleColor); set { _titleColor = value; Set(F_TITLE_COLOR, (int)value); } }

    public List<string> Lines
    {
        get
        {
            if (C == IntPtr.Zero) return _localLines;
            var n = GfxWmNative.LinesCount(C);
            var list = new List<string>(n);
            for (var i = 0; i < n; i++) list.Add(GfxWmNative.GetLine(C, i));
            return list;
        }
        set
        {
            _localLines = value?.ToList() ?? [];
            if (C == IntPtr.Zero) return;
            GfxWmNative.LinesClear(C);
            foreach (var line in _localLines) GfxWmNative.LineInsert(C, GfxWmNative.LinesCount(C), line);
        }
    }

    public int ScrollOffset { get => Get(F_SCROLL, _scroll); set { _scroll = value; Set(F_SCROLL, value); } }
    public bool Editable { get => Get(F_EDITABLE, _editable ? 1 : 0) != 0; set { _editable = value; Set(F_EDITABLE, value ? 1 : 0); } }
    private bool _editable;
    public int CursorLine { get => Get(F_CURSOR_LINE, _curLine); set { _curLine = value; Set(F_CURSOR_LINE, value); } }
    public int CursorCol { get => Get(F_CURSOR_COL, _curCol); set { _curCol = value; Set(F_CURSOR_COL, value); } }

    public int TitleHeight => 24;
    public int ClientPadding => 6;

    /// <summary>Rettangolo client in pixel logici (title 24, padding 6): dal C.</summary>
    public (int x, int y, int w, int h) ClientRect
    {
        get
        {
            if (C != IntPtr.Zero)
            {
                var r = new int[4];
                GfxWmNative.gmb_managed_window_client_rect_px(C, r);
                return (r[0], r[1], r[2], r[3]);
            }
            return (X + ClientPadding, Y + TitleHeight, W - 2 * ClientPadding, H - TitleHeight - ClientPadding);
        }
    }

    public void AppendLine(string line)
    {
        var cr = ClientRect;
        var rowH = Math.Max(1, GfxWindowManager.CharHeight());
        var wasAtBottom = ScrollOffset + cr.h >= Lines.Count * rowH || Lines.Count == 0;
        if (C != IntPtr.Zero) GfxWmNative.LineInsert(C, GfxWmNative.LinesCount(C), line ?? "");
        else _localLines.Add(line ?? "");
        if (wasAtBottom && Lines.Count > Math.Max(1, cr.h / rowH))
            ScrollOffset = Lines.Count - Math.Max(1, cr.h / rowH);
    }

    public string SayAt(int row, int col, string text)
    {
        var cr = ClientRect;
        var charW = Math.Max(1, GfxWindowManager.CharWidth());
        var charH = Math.Max(1, GfxWindowManager.CharHeight());
        var maxCols = Math.Max(1, cr.w / charW);
        var absRow = ScrollOffset + row;
        var lines = Lines;
        while (lines.Count <= absRow) lines.Add("");
        var line = lines[absRow];
        var start = Math.Max(0, Math.Min(col, maxCols - 1));
        if (line.Length < start) line = line.PadRight(start);
        var n = Math.Min(text?.Length ?? 0, maxCols - start);
        var piece = (text ?? "")[..n];
        line = line.Remove(start, Math.Min(piece.Length, line.Length - start)).Insert(start, piece);
        lines[absRow] = line;
        if (C != IntPtr.Zero) GfxWmNative.LineSet(C, absRow, line);
        else _localLines = lines;
        return line;
    }

    public void BoxAt(int top, int left, int bottom, int right, string? border)
    {
        var b = (border ?? "single") switch { "double" => "╔═╗║║╚═╝", "none" => "", _ => "┌─┐││└─┘" };
        if (b.Length < 8) return;
        for (var x = left; x <= right; x++)
        {
            SayAt(top, x, b[1].ToString());
            SayAt(bottom, x, b[6].ToString());
        }
        for (var y = top; y <= bottom; y++)
        {
            SayAt(y, left, b[3].ToString());
            SayAt(y, right, b[4].ToString());
        }
        SayAt(top, left, b[0].ToString());
        SayAt(top, right, b[2].ToString());
        SayAt(bottom, left, b[5].ToString());
        SayAt(bottom, right, b[7].ToString());
    }

    /// <summary>Snapshot STACCATO (SAVESCREEN sulla finestra pixel).</summary>
    public PixelWindow Snapshot() => new()
    {
        Id = Id, Name = Name, Title = Title, X = X, Y = Y, W = W, H = H, Z = Z,
        State = State, RestoreX = RestoreX, RestoreY = RestoreY, RestoreW = RestoreW, RestoreH = RestoreH,
        Focused = Focused, Shadow = Shadow, BorderRadius = BorderRadius,
        Closable = Closable, Draggable = Draggable, Resizable = Resizable,
        Lines = [.. Lines], ScrollOffset = ScrollOffset,
        Editable = Editable, CursorLine = CursorLine, CursorCol = CursorCol,
    };

    public void RestoreFrom(PixelWindow snap)
    {
        Lines = [.. snap.Lines];
        ScrollOffset = snap.ScrollOffset;
        Editable = snap.Editable;
        CursorLine = snap.CursorLine;
        CursorCol = snap.CursorCol;
    }
}

/// <summary>Compositor di finestre pixel: proxy sul runtime C (nessuno stato locale).</summary>
public sealed class GfxWindowManager
{
    private static IntPtr M => GfxWmNative.gmb_window_manager_static();

    public GfxWindowManager()
    {
        // Finestre pixel e TUI convivono nel manager C: si distinguono per il flag `pixel`.
    }

    public int ScreenW
    {
        get { var r = new int[4]; GfxWmNative.gmb_window_manager_work_area4(M, r); return r[2]; }
        set => GfxWmNative.SyncWorkArea(Math.Max(1, value), ScreenH);
    }

    public int ScreenH
    {
        get { var r = new int[4]; GfxWmNative.gmb_window_manager_work_area4(M, r); return r[3]; }
        set => GfxWmNative.SyncWorkArea(ScreenW, Math.Max(1, value));
    }

    public int CurrentDesktop => GfxWmNative.gmb_window_manager_current_desktop(M);

    public void SwitchDesktop(int desktop)
    {
        var d = Math.Max(1, desktop);
        GfxWmNative.gmb_window_manager_switch_desktop(M, d);
        GfxWmNative.gmb_winstmt_desktop(d);
    }

    public void MoveWindowToDesktop(int id, int desktop)
        => GfxWmNative.gmb_window_manager_move_to_desktop(M, id, Math.Max(1, desktop));

    /// <summary>Finestre PIXEL (tutte, come la vecchia lista `windows`).</summary>
    public IReadOnlyList<PixelWindow> Windows
    {
        get
        {
            var list = new List<PixelWindow>();
            var total = GfxWmNative.gmb_window_manager_window_count(M, 0);
            for (var i = 0; i < total; i++)
            {
                var id = GfxWmNative.gmb_window_manager_window_id_at(M, i);
                var w = Get(id);
                if (w is not null) list.Add(w);
            }
            return list;
        }
    }

    public int? ActiveId => FocusedId;

    public int AddWindow(string name, string title, int x, int y, int w, int h, bool shadow = true)
    {
        var id = GfxWmNative.Define(name, title, x, y, Math.Max(16, w), Math.Max(24, h), shadow);
        if (id < 0) return -1;
        if (Get(id) is { } win)
        {
            win.Closable = true;
            win.Draggable = true;
            win.Resizable = true;
            win.BorderRadius = 6;
        }
        GfxWmNative.gmb_window_manager_raise(M, id);
        return id;
    }

    public PixelWindow? Get(int id)
    {
        var ptr = GfxWmNative.gmb_window_manager_get(M, id);
        if (ptr == IntPtr.Zero) return null;
        if (GfxWmNative.GetInt(ptr, 27 /* F_PIXEL */) == 0) return null;   // finestra TUI
        return new PixelWindow { C = ptr };
    }

    public PixelWindow? Find(string name)
    {
        if (string.IsNullOrEmpty(name)) return null;
        var id = GfxWmNative.Id(name);
        var w = id >= 0 ? Get(id) : null;
        if (w is not null) return w;
        // fallback per titolo
        foreach (var p in Windows)
            if (string.Equals(p.Title, name, StringComparison.OrdinalIgnoreCase)) return p;
        return null;
    }

    public int? FocusedId
    {
        get
        {
            var id = GfxWmNative.gmb_window_manager_focused(M);
            if (id < 0) return null;
            return GfxWmNative.GetInt(GfxWmNative.gmb_window_manager_get(M, id), 27) != 0 ? id : null;
        }
    }

    public List<PixelWindow> ZOrdered =>
        [.. Windows.Where(w => w.Desktop == CurrentDesktop).OrderBy(w => w.Z)];

    public int WindowCount => Windows.Count(w => w.Desktop == CurrentDesktop);

    public PixelWindow? TopmostAt(int x, int y)
    {
        var id = GfxWmNative.gmb_window_manager_topmost_at(M, x, y);
        return id >= 0 ? Get(id) : null;
    }

    public void Raise(int id) => GfxWmNative.gmb_window_manager_raise(M, id);

    public void Select(int? id)
    {
        if (id is int nid) GfxWmNative.gmb_window_manager_select(M, nid);
        else GfxWmNative.gmb_window_manager_clear_focus(M);
    }

    public void Close(int id)
    {
        var wasFocused = FocusedId == id;
        GfxWmNative.gmb_window_manager_remove(M, id);
        if (wasFocused) { /* il C azzera il focus: nessuna finestra attiva */ }
    }

    public void MoveTo(int id, int x, int y) => GfxWmNative.gmb_window_manager_move_to(M, id, x, y);

    public void ResizeTo(int id, int wNew, int hNew)
    {
        var w = Get(id);
        if (w is null) return;
        GfxWmNative.gmb_window_manager_resize_by(M, id, Math.Max(16, wNew) - w.W, Math.Max(24, hNew) - w.H);
    }

    public void Snap(int id, GfxSnapZone zone)
        => GfxWmNative.gmb_window_manager_snap_zone(M, id, zone == GfxSnapZone.Left ? 0 : 1);

    public void MaximizeToggle(int id) => GfxWmNative.gmb_window_manager_maximize_toggle(M, id);

    public void Minimize(int id) => GfxWmNative.gmb_window_manager_minimize(M, id);

    public void Restore(int id)
    {
        var w = Get(id);
        if (w is null || w.State != GfxWindowState.Minimized) return;
        GfxWmNative.gmb_window_manager_restore(M, id);
        GfxWmNative.gmb_window_manager_unminimize(M, id);
        Raise(id);
    }

    public bool IsDragging => GfxWmNative.gmb_window_manager_is_dragging(M) != 0;

    public void StartDrag(int id, int x, int y) => GfxWmNative.gmb_window_manager_start_drag(M, id, x, y);
    public void UpdateDrag(int x, int y) => GfxWmNative.gmb_window_manager_update_drag(M, x, y);
    public void EndDrag() => GfxWmNative.gmb_window_manager_end_drag(M);

    public void StartResize(int id, int x, int y, string edge)
    {
        var w = Get(id);
        if (w is null || !w.Resizable || w.State != GfxWindowState.Floating) return;
        GfxWmNative.StartResize(M, id, x, y, edge);
    }

    public void UpdateResize(int x, int y) => GfxWmNative.gmb_window_manager_update_resize(M, x, y);
    public void EndResize() => GfxWmNative.gmb_window_manager_end_resize(M);

    /// <summary>Controllo titlebar sotto il punto: "close" / "maximize" / "minimize" / null (C).</summary>
    public string? ControlAt(PixelWindow w, int x, int y) =>
        GfxWmNative.ControlAt(w.Name, x, y) switch
        {
            1 => "close",
            2 => "minimize",
            3 => "maximize",
            _ => null
        };

    /// <summary>Bordo di resize sotto il cursore: "n"/"s"/"e"/"w"/"ne"/... o null (C).</summary>
    public string? ResizeEdgeAt(PixelWindow w, int x, int y)
    {
        if (!w.Resizable || w.State != GfxWindowState.Floating) return null;
        return GfxWmNative.ResizeEdgeAt(w.Name, x, y) switch
        {
            1 => "n", 2 => "s", 4 => "e", 8 => "w",
            5 => "ne", 9 => "nw", 6 => "se", 10 => "sw",
            _ => null
        };
    }

    /// <summary>Gestisce un evento mouse (coordinate LOGICHE). True se consumato (C).</summary>
    public bool HandleMouse(int kind, int x, int y) => GfxWmNative.gmb_winstmt_pixel_handle_mouse(kind, x, y) != 0;

    /// <summary>Composita tutte le finestre pixel (desktop + taskbar) sul buffer grafico (C).</summary>
    public void Render() => GfxWmNative.gmb_winstmt_render_all();

    public void SetStatusBar(string text) => GfxWmNative.SetStatusBar(M, text);

    public bool HasStatusText => GfxWmNative.HasStatus(M);

    /// <summary>Larghezza carattere corrente (per layout colonne nel client area).</summary>
    public static int CharWidth()
    {
        try { return (int)GraphicsManager.Instance.TextWidthInternal(" "); }
        catch { return 8; }
    }

    /// <summary>Altezza riga corrente (per layout righe nel client area).</summary>
    public static int CharHeight()
    {
        try { return (int)GraphicsManager.Instance.TextHeightInternal(); }
        catch { return 16; }
    }
}
