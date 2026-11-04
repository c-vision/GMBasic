using CommonLib.Consoles.Tui;
using System.Runtime.InteropServices;
using CommonLib.Screens;
using CommonLib.Clipboard;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>Zona di snap sinistra/destra.</summary>
public enum SnapZone { Left, Right }

/// <summary>Stato di una finestra gestita.</summary>
public enum WindowState
{
    Floating,
    SnappedLeft,
    SnappedRight,
    Maximized,
    Tiled
}

/// <summary>Stile del bordo finestra.</summary>
public enum FrameStyle
{
    Single,
    Double,
    Rounded
}

/// <summary>Griglia per tile layout.</summary>
public class TileGrid
{
    public int Rows { get; }
    public int Cols { get; }

    public TileGrid(int rows, int cols)
    {
        Rows = Math.Max(1, rows);
        Cols = Math.Max(1, cols);
    }

    public int Cells => Rows * Cols;

    public (int row, int col) RowCol(int index)
    {
        var r = index / Cols;
        var c = index % Cols;
        return (r, c);
    }
}

/// <summary>Finestra gestita dal WindowManager.</summary>
public class ManagedWindow
{
    // Handle alla struct C (gmb_managed_window_t). 0 = snapshot STACCATO (solo locale).
    // Quando C != 0 TUTTE le proprieta' leggono/scrivono la finestra C: il modello C e'
    // l'unica sorgente; questa classe e' un binding sottile (P/Invoke).
    internal IntPtr C;

    private int _id, _x, _y, _w, _h, _z, _state, _desktop = 1, _scroll, _curLine, _curCol;
    private int _rx, _ry, _rw, _rh;
    private bool _minimized, _focused = false, _closable = true, _draggable = true, _editable, _shadow = true;
    private int _border = (int)PanelBorder.Double, _frame = (int)FrameStyle.Double;
    private string _title = "", _name = "";
    private List<string> _localLines = [];
    private CWindowLines? _proxyLines;

    private const int F_X = 0, F_Y = 1, F_W = 2, F_H = 3, F_Z = 4, F_STATE = 5, F_BORDER = 6,
        F_FRAME = 7, F_SHADOW = 8, F_FOCUSED = 9, F_MINIMIZED = 10, F_DESKTOP = 11,
        F_CURSOR_LINE = 12, F_CURSOR_COL = 13, F_SCROLL = 14, F_LINES_COUNT = 15,
        F_CLOSABLE = 16, F_DRAGGABLE = 17, F_ID = 18, F_RX = 19, F_RY = 20, F_RW = 21,
        F_RH = 22, F_EDITABLE = 23;

    private int Get(int f, int local) => C != IntPtr.Zero ? CommonLib.Consoles.Tui.ClipperWmNative.GetInt(C, f) : local;
    private void Set(int f, int v) { if (C != IntPtr.Zero) CommonLib.Consoles.Tui.ClipperWmNative.SetInt(C, f, v); }
    private string GetS(int f, string local) => C != IntPtr.Zero ? CommonLib.Consoles.Tui.ClipperWmNative.GetStr(C, f) : local;
    private void SetS2(int f, string v) { if (C != IntPtr.Zero) CommonLib.Consoles.Tui.ClipperWmNative.SetStr(C, f, v); }

    public int Id { get => Get(F_ID, _id); set { _id = value; } }
    public string Title { get => GetS(0, _title); set { _title = value ?? ""; SetS2(0, _title); } }
    /// <summary>Nome della finestra (dal DEFINE, es. "Edit") — distinto dal Title (es. "Editor").</summary>
    public string Name { get => GetS(1, _name); set { _name = value ?? ""; SetS2(1, _name); } }

    public ScreenRect Rect
    {
        get => new(Get(F_X, _x), Get(F_Y, _y), Get(F_W, _w), Get(F_H, _h));
        set { _x = value.X; _y = value.Y; _w = value.W; _h = value.H; Set(F_X, _x); Set(F_Y, _y); Set(F_W, _w); Set(F_H, _h); }
    }
    public int Z { get => Get(F_Z, _z); set { _z = value; Set(F_Z, value); } }
    public WindowState State { get => (WindowState)Get(F_STATE, _state); set { _state = (int)value; Set(F_STATE, _state); } }
    public ScreenRect RestoreRect
    {
        get => new(Get(F_RX, _rx), Get(F_RY, _ry), Get(F_RW, _rw), Get(F_RH, _rh));
        set { _rx = value.X; _ry = value.Y; _rw = value.W; _rh = value.H; Set(F_RX, _rx); Set(F_RY, _ry); Set(F_RW, _rw); Set(F_RH, _rh); }
    }
    public bool Minimized { get => Get(F_MINIMIZED, _minimized ? 1 : 0) != 0; set { _minimized = value; Set(F_MINIMIZED, value ? 1 : 0); } }
    public PanelBorder Border { get => (PanelBorder)Get(F_BORDER, _border); set { _border = (int)value; Set(F_BORDER, _border); } }
    public bool Shadow { get => Get(F_SHADOW, _shadow ? 1 : 0) != 0; set { _shadow = value; Set(F_SHADOW, value ? 1 : 0); } }
    public FrameStyle FrameStyle { get => (FrameStyle)Get(F_FRAME, _frame); set { _frame = (int)value; Set(F_FRAME, _frame); } }
    public int Desktop { get => Get(F_DESKTOP, _desktop); set { _desktop = value; Set(F_DESKTOP, value); } }
    public bool Focused { get => Get(F_FOCUSED, _focused ? 1 : 0) != 0; set { _focused = value; Set(F_FOCUSED, value ? 1 : 0); } }
    public bool Closable { get => Get(F_CLOSABLE, _closable ? 1 : 0) != 0; set { _closable = value; Set(F_CLOSABLE, value ? 1 : 0); } }
    public bool Draggable { get => Get(F_DRAGGABLE, _draggable ? 1 : 0) != 0; set { _draggable = value; Set(F_DRAGGABLE, value ? 1 : 0); } }

    // ── Contenuto della finestra (buffer + scroll) ──
    /// <summary>Righe del contenuto. Con C != 0 e' una vista sul buffer C.</summary>
    public IList<string> Lines
    {
        get => C != IntPtr.Zero ? (_proxyLines ??= new CWindowLines(this)) : _localLines;
        set
        {
            _localLines = value?.ToList() ?? [];
            if (C != IntPtr.Zero) CommonLib.Consoles.Tui.ClipperWmNative.SetLines(C, _localLines);
        }
    }
    public int ScrollOffset { get => Get(F_SCROLL, _scroll); set { _scroll = value; Set(F_SCROLL, value); } }
    /// <summary>Se true il contenuto accetta editing da tastiera.</summary>
    public bool Editable { get => Get(F_EDITABLE, _editable ? 1 : 0) != 0; set { _editable = value; Set(F_EDITABLE, value ? 1 : 0); } }
    public int CursorLine { get => Get(F_CURSOR_LINE, _curLine); set { _curLine = value; Set(F_CURSOR_LINE, value); } }
    public int CursorCol { get => Get(F_CURSOR_COL, _curCol); set { _curCol = value; Set(F_CURSOR_COL, value); } }

    /// <summary>Aggiunge una riga al buffer e scrolla in fondo se eravamo in coda.</summary>
    public void AppendLine(string line)
    {
        var wasAtBottom = ScrollOffset + ContentRect.H >= Lines.Count || Lines.Count == 0;
        Lines.Add(line ?? "");
        if (wasAtBottom && Lines.Count > ContentRect.H)
            ScrollOffset = Lines.Count - ContentRect.H;
    }

    /// <summary>Imposta il buffer (array di righe) e riporta lo scroll in cima.</summary>
    public void SetLines(IEnumerable<string> lines)
    {
        Lines = lines?.ToList() ?? [];
        ScrollOffset = 0;
        CursorLine = 0;
        CursorCol = 0;
    }

    /// <summary>Scroll verticale vincolato ai limiti del buffer.</summary>
    public void ScrollTo(int offset)
    {
        var max = Math.Max(0, Lines.Count - Math.Max(1, ContentRect.H));
        ScrollOffset = Math.Clamp(offset, 0, max);
    }
    public void ScrollBy(int delta) => ScrollTo(ScrollOffset + delta);

    /// <summary>Scrive un testo alla riga relativa `row` e colonna relativa `col` della
    /// viewport. Ritorna la riga scritta.</summary>
    public string SayAt(int row, int col, string text)
    {
        if (C != IntPtr.Zero) return CommonLib.Consoles.Tui.ClipperWmNative.SayAt(C, row, col, text);
        var absRow = ScrollOffset + row;
        while (_localLines.Count <= absRow) _localLines.Add("");
        var line = _localLines[absRow];
        if (line.Length < col) line = line.PadRight(col);
        line = line.Remove(col, Math.Min(text.Length, line.Length - col)).Insert(col, text);
        _localLines[absRow] = line;
        return line;
    }

    /// <summary>Disegna un box (bordi) alle coordinate relative della viewport.</summary>
    public void BoxAt(int top, int left, int bottom, int right, string? border)
    {
        if (C != IntPtr.Zero) { CommonLib.Consoles.Tui.ClipperWmNative.BoxAt(C, top, left, bottom, right, border); return; }
        var b = (border ?? "single") switch { "double" => "╔═╗║║╚═╝", "none" => "", _ => "┌─┐││└─┘" };
        if (b.Length < 8) return;
        for (int x = left; x <= right; x++) { SayAt(top, x, b[1].ToString()); SayAt(bottom, x, b[6].ToString()); }
        for (int y = top; y <= bottom; y++) { SayAt(y, left, b[3].ToString()); SayAt(y, right, b[4].ToString()); }
        SayAt(top, left, b[0].ToString()); SayAt(top, right, b[2].ToString());
        SayAt(bottom, left, b[5].ToString()); SayAt(bottom, right, b[7].ToString());
    }

    /// <summary>Snapshot del buffer (SAVESCREEN sulla finestra): copia righe+scroll+cursore.</summary>
    public ManagedWindow Snapshot()
    {
        return new ManagedWindow
        {
            Id = Id, Title = Title, Name = Name, Rect = Rect, Z = Z, State = State,
            RestoreRect = RestoreRect, Minimized = Minimized, Border = Border, Shadow = Shadow,
            FrameStyle = FrameStyle, Desktop = Desktop, Focused = Focused, Closable = Closable,
            Draggable = Draggable, Lines = [.. Lines], ScrollOffset = ScrollOffset,
            Editable = Editable, CursorLine = CursorLine, CursorCol = CursorCol,
        };
    }

    /// <summary>Ripristina lo snapshot del buffer (RESTORESCREEN sulla finestra).</summary>
    public void RestoreFrom(ManagedWindow snap)
    {
        Lines = [.. snap.Lines];
        ScrollOffset = snap.ScrollOffset;
        Editable = snap.Editable;
        CursorLine = snap.CursorLine;
        CursorCol = snap.CursorCol;
    }

    /// <summary>Rettangolo interno del contenuto (esclude titolo e bordi).</summary>
    public ScreenRect ContentRect
    {
        get
        {
            if (C != IntPtr.Zero) { var r = CommonLib.Consoles.Tui.ClipperWmNative.gmb_managed_window_content_rect(C); return new(r.x, r.y, r.w, r.h); }
            var rr = Rect;
            return new(rr.X + 1, rr.Y + 1, Math.Max(0, rr.W - 2), Math.Max(0, rr.H - 2));
        }
    }

    /// <summary>Rettangolo della titlebar (riga superiore di Rect).</summary>
    public ScreenRect TitlebarRect
    {
        get
        {
            if (C != IntPtr.Zero) { var r = CommonLib.Consoles.Tui.ClipperWmNative.gmb_managed_window_titlebar_rect(C); return new(r.x, r.y, r.w, r.h); }
            var rr = Rect;
            return new(rr.X, rr.Y, rr.W, 1);
        }
    }

    /// <summary>Colonne dei pulsanti titlebar in stile Turbo Vision: (close, min, max) offset da Rect.X.</summary>
    public (int close, int min, int max) ControlColumns
    {
        get
        {
            var w = Rect.W;
            return (2, w - 8, w - 5);
        }
    }

    /// <summary>Quale controllo titlebar si trova nel punto globale (x,y), se presente.</summary>
    public WinControl? ControlAt(int x, int y)
    {
        if (C != IntPtr.Zero)
            return CommonLib.Consoles.Tui.ClipperWmNative.gmb_managed_window_control_at(C, x, y) switch
            { 1 => WinControl.Close, 2 => WinControl.Minimize, 3 => WinControl.Maximize, _ => (WinControl?)null };
        var rr = Rect;
        if (y != rr.Y) return null;
        var lx = x - rr.X;
        var w = rr.W;
        if (Closable && w >= 7 && lx >= 2 && lx <= 4) return WinControl.Close;
        if (w >= 12 && lx >= w - 8 && lx <= w - 6) return WinControl.Minimize;
        if (w >= 10 && lx >= w - 5 && lx <= w - 3) return WinControl.Maximize;
        return null;
    }

    /// <summary>Bordo di resize sotto il cursore, se presente: null / "s" / "e" / "w" / "ne" / "nw" / "se" / "sw".</summary>
    public string? ResizeEdgeAt(int x, int y)
    {
        if (C != IntPtr.Zero)
            return CommonLib.Consoles.Tui.ClipperWmNative.gmb_managed_window_resize_edge_at(C, x, y) switch
            { 2 => "s", 4 => "e", 8 => "w", 5 => "ne", 9 => "nw", 6 => "se", 10 => "sw", _ => (string?)null };
        var rr = Rect;
        const int edge = 1;
        if (x < rr.X - edge || x >= rr.X + rr.W + edge) return null;
        if (y < rr.Y - edge || y >= rr.Y + rr.H + edge) return null;
        var onN = y == rr.Y;
        var onS = y == rr.Y + rr.H - 1;
        var onE = x == rr.X + rr.W - 1;
        var onW = x == rr.X;
        if (onN && onE) return "ne";
        if (onN && onW) return "nw";
        if (onS && onE) return "se";
        if (onS && onW) return "sw";
        if (onS) return "s";
        if (onE) return "e";
        if (onW) return "w";
        return null;
    }

    /// <summary>Vista IList&lt;string&gt; sul buffer C (line_set/insert/remove).</summary>
    private sealed class CWindowLines(ManagedWindow w) : IList<string>
    {
        public string this[int index]
        {
            get => CommonLib.Consoles.Tui.ClipperWmNative.GetLine(w.C, index);
            set => CommonLib.Consoles.Tui.ClipperWmNative.LineSet(w.C, index, value);
        }
        public int Count => CommonLib.Consoles.Tui.ClipperWmNative.LinesCount(w.C);
        public bool IsReadOnly => false;
        public void Add(string item) => CommonLib.Consoles.Tui.ClipperWmNative.LineInsert(w.C, Count, item);
        public void Clear() => CommonLib.Consoles.Tui.ClipperWmNative.SetLines(w.C, Array.Empty<string>());
        public bool Contains(string item) { for (int i = 0; i < Count; i++) if (this[i] == item) return true; return false; }
        public void CopyTo(string[] array, int arrayIndex) { for (int i = 0; i < Count; i++) array[arrayIndex + i] = this[i]; }
        public IEnumerator<string> GetEnumerator() { for (int i = 0; i < Count; i++) yield return this[i]; }
        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator() => GetEnumerator();
        public int IndexOf(string item) { for (int i = 0; i < Count; i++) if (this[i] == item) return i; return -1; }
        public void Insert(int index, string item) => CommonLib.Consoles.Tui.ClipperWmNative.LineInsert(w.C, index, item);
        public bool Remove(string item) { int i = IndexOf(item); if (i < 0) return false; RemoveAt(i); return true; }
        public void RemoveAt(int index) => CommonLib.Consoles.Tui.ClipperWmNative.LineRemove(w.C, index);
    }
}

/// <summary>Argomenti degli eventi di finestra.</summary>
public class WindowEventArgs : EventArgs
{
    public WindowEventArgs(int id) => Id = id;

    public int Id { get; }
    public string Title { get; init; } = "";
    public ScreenRect Rect { get; init; }
    public WindowState State { get; init; }
    public bool Handled { get; set; }
    public bool Cancel { get; set; }
}

/// <summary>Window Manager: gestisce finestre multiple con z-order, focus, e layout.</summary>
/// <summary>
/// Window Manager — <b>proxy P/Invoke</b> sul runtime C (<c>gmb_window_manager_t</c>).
///
/// Lo stato (finestre, z-order, focus, id, desktop, drag/resize, statusbar, contenuto)
/// vive <b>solo in C</b>: questa classe non tiene nessuna copia. Ogni operazione delega a
/// <c>gmb_window_manager_*</c> e ogni lettura torna dal C; gli eventi (<c>WindowOpened</c>,
/// <c>WindowClosed</c>, …) sono emessi qui come esito dell'operazione, non da uno stato
/// duplicato. La vecchia implementazione C# è archiviata in
/// <c>GMBasicMonoRepo-Deprecated/ClipperLib/Controls/WindowManager.cs</c>.
/// </summary>
public class WindowManager
{
    private readonly IntPtr _c;

    public WindowManager(ScreenRect workArea)
    {
        _c = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_new(workArea.X, workArea.Y, workArea.W, workArea.H);
    }

    ~WindowManager()
    {
        try { CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_free(_c); } catch { }
    }

    // ── Eventi ────────────────────────────────────────────────────

    public event EventHandler<WindowEventArgs>? WindowOpening;
    public event EventHandler<WindowEventArgs>? WindowOpened;
    public event EventHandler<WindowEventArgs>? WindowClosing;
    public event EventHandler<WindowEventArgs>? WindowClosed;
    public event EventHandler<WindowEventArgs>? WindowMoved;
    public event EventHandler<WindowEventArgs>? WindowResized;
    public event EventHandler<WindowEventArgs>? WindowStateChanged;
    public event EventHandler<WindowEventArgs>? WindowFocused;
    public event EventHandler<WindowEventArgs>? WindowMinimized;
    public event EventHandler<WindowEventArgs>? WindowRestored;

    private WindowEventArgs Args(int id)
    {
        var w = Get(id);
        return new WindowEventArgs(id)
        {
            Title = w?.Title ?? "",
            Rect = w?.Rect ?? default,
            State = w?.State ?? WindowState.Floating,
        };
    }

    protected virtual void OnWindowOpening(WindowEventArgs e) => WindowOpening?.Invoke(this, e);
    protected virtual void OnWindowOpened(WindowEventArgs e) => WindowOpened?.Invoke(this, e);
    protected virtual void OnWindowClosing(WindowEventArgs e) => WindowClosing?.Invoke(this, e);
    protected virtual void OnWindowClosed(WindowEventArgs e) => WindowClosed?.Invoke(this, e);
    protected virtual void OnWindowMoved(WindowEventArgs e) => WindowMoved?.Invoke(this, e);
    protected virtual void OnWindowResized(WindowEventArgs e) => WindowResized?.Invoke(this, e);
    protected virtual void OnWindowStateChanged(WindowEventArgs e) => WindowStateChanged?.Invoke(this, e);
    protected virtual void OnWindowFocused(WindowEventArgs e) => WindowFocused?.Invoke(this, e);
    protected virtual void OnWindowMinimized(WindowEventArgs e) => WindowMinimized?.Invoke(this, e);
    protected virtual void OnWindowRestored(WindowEventArgs e) => WindowRestored?.Invoke(this, e);

    // ── Work area / statusbar ─────────────────────────────────────

    public ScreenRect WorkArea
    {
        get { var r = new int[4]; CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_work_area4(_c, r); return new(r[0], r[1], r[2], r[3]); }
        set => CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_sync_workarea(_c, Math.Max(1, value.W), Math.Max(1, value.H));
    }

    /// <summary>Sincronizza l'area di lavoro con le dimensioni REALI del buffer di render.</summary>
    public void SyncWorkArea(int width, int height)
    {
        var r = new int[4]; CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_work_area4(_c, r);
        var w = Math.Max(1, width);
        var h = Math.Max(1, height);
        if (r[2] != w || r[3] != h)
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_sync_workarea(_c, w, h);
    }

    public void SetStatusBar(string text)
    {
        var p = Marshal.StringToHGlobalAnsi(text ?? "");
        try { CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_set_statusbar(_c, p); }
        finally { Marshal.FreeHGlobal(p); }
    }

    public bool HasStatusText => CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_has_status(_c) != 0;

    // ── Virtual Desktops ──────────────────────────────────────────

    public int CurrentDesktop => CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_current_desktop(_c);

    public void SwitchDesktop(int desktop)
    {
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_switch_desktop(_c, Math.Max(1, desktop));
    }

    public void MoveWindowToDesktop(int windowId, int desktop)
    {
        if (Get(windowId) is null) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_to_desktop(_c, windowId, Math.Max(1, desktop));
    }

    public void MoveWindowToDesktop(string name, int desktop)
    {
        if (FindByTitle(name) is int id) MoveWindowToDesktop(id, desktop);
    }

    public int WindowCount =>
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_count(_c,
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_current_desktop(_c));

    public int TotalWindowCount => CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_count(_c, 0);

    // ── Window CRUD ────────────────────────────────────────────────

    public int AddWindow(string title, ScreenRect rect)
    {
        var nextId = 1;
        for (var i = 0; i < TotalWindowCount; i++)
        {
            var wid = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_id_at(_c, i);
            if (wid >= nextId) nextId = wid + 1;
        }

        var ev = new WindowEventArgs(nextId) { Title = title, Rect = rect, State = WindowState.Floating };
        OnWindowOpening(ev);
        if (ev.Cancel) return -1;

        var id = CommonLib.Consoles.Tui.ClipperWmNative.AddWindowEx(_c, title, title, rect.X, rect.Y, rect.W, rect.H,
            (int)PanelBorder.Double, (int)FrameStyle.Double, 1,
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_current_desktop(_c));
        if (id < 0) return -1;

        if (Get(id) is { } w)
        {
            w.Closable = true;
            w.Draggable = true;
        }
        OnWindowOpened(Args(id));
        return id;
    }

    /// <summary>Binding alla finestra C (null se l'id non esiste). Ogni chiamata crea il proxy:
    /// non c'è nessuna lista lato C#.</summary>
    public ManagedWindow? Get(int id)
    {
        var ptr = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_get(_c, id);
        return ptr == IntPtr.Zero ? null : new ManagedWindow { C = ptr };
    }

    public int? Focused
    {
        get
        {
            var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_focused(_c);
            return id >= 0 ? id : null;
        }
    }

    /// <summary>Finestre del desktop attivo ordinate per z (dalla più bassa alla più alta).</summary>
    public List<ManagedWindow> ZOrdered
    {
        get
        {
            var list = new List<ManagedWindow>();
            var desktop = CurrentDesktop;
            var total = TotalWindowCount;
            for (var i = 0; i < total; i++)
            {
                var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_id_at(_c, i);
                var w = Get(id);
                if (w is not null && w.Desktop == desktop) list.Add(w);
            }
            return [.. list.OrderBy(w => w.Z)];
        }
    }

    /// <summary>Finestra visibile più in alto nel desktop corrente in un punto, o null.</summary>
    public int? TopmostAt(int x, int y)
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_topmost_at(_c, x, y);
        return id >= 0 ? id : null;
    }

    public void Raise(int id)
    {
        if (Get(id) is null) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_raise(_c, id);
        /* FOCUS a ogni raise (parità runtime C: `WINDOW … ACTIVATE`/click innalzano sempre
         * "FOCUS" per la finestra, non solo al cambio di focus). */
        OnWindowFocused(Args(id));
    }

    public void RemoveWindow(int id) => Close(id);

    /// <summary>Rimuove il focus dalla finestra (WINDOW DEACTIVATE).</summary>
    public void Deactivate(int id)
    {
        if (Focused != id) return;
        if (Get(id) is not { } w) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_deactivate(_c, id);
        OnWindowFocused(Args(w.Id));
    }

    /// <summary>Cerca l'ID della finestra per titolo O nome (case-insensitive).</summary>
    public int? FindByTitle(string title)
    {
        if (string.IsNullOrEmpty(title)) return null;
        var id = CommonLib.Consoles.Tui.ClipperWmNative.FindByTitle(_c, title);
        return id >= 0 ? id : null;
    }

    /// <summary>Cerca l'ID della finestra per NOME BASIC (`WINDOW DEFINE nome ...`),
    /// distinto dal titolo. Serve al re-DEFINE: la finestra si aggiorna, non si duplica.</summary>
    public int? FindByName(string name)
    {
        if (string.IsNullOrEmpty(name)) return null;
        var id = CommonLib.Consoles.Tui.ClipperWmNative.FindByName(_c, name);
        return id >= 0 ? id : null;
    }

    /// <summary>Imposta il titolo della finestra focalizzata (WINDOW SET TITLE).</summary>
    public int? SetFocusedTitle(string title)
    {
        if (Focused is not int fid || Get(fid) is null) return null;
        var p = Marshal.StringToHGlobalAnsi(title ?? "");
        try { CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_set_title(_c, fid, p); }
        finally { Marshal.FreeHGlobal(p); }
        return fid;
    }

    /// <summary>Id della finestra attiva (viewport di output corrente), o null se lo schermo base.</summary>
    public int? ActiveWindowId => Focused;

    /// <summary>Seleziona la finestra attiva (viewport corrente): id o nome. null → schermo base.</summary>
    public int? Select(int? id)
    {
        if (id is int nid)
        {
            if (Get(nid) is null) return null;
            var wasFocused = Focused;
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_select(_c, nid);
            if (wasFocused != nid) OnWindowFocused(Args(nid));
            return nid;
        }
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_clear_focus(_c);
        return null;
    }

    public int? Select(string? title)
    {
        if (string.IsNullOrEmpty(title)) return Select((int?)null);
        return Select(FindByTitle(title));
    }

    // ── Move / Resize ──────────────────────────────────────────────

    public void MoveBy(int id, int dx, int dy)
    {
        if (Get(id) is not { } w) return;
        var old = w.Rect;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_by(_c, id, dx, dy);
        if (w.State != WindowState.Floating) w.State = WindowState.Floating;
        if (w.Rect.X != old.X || w.Rect.Y != old.Y)
            OnWindowMoved(Args(id));
    }

    public void MoveTo(int id, int x, int y)
    {
        if (Get(id) is not { } w) return;
        var old = w.Rect;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_to(_c, id, x, y);
        if (w.State != WindowState.Floating) w.State = WindowState.Floating;
        if (w.Rect.X != old.X || w.Rect.Y != old.Y)
            OnWindowMoved(Args(id));
    }

    public void ResizeTo(int id, int wNew, int hNew)
    {
        if (Get(id) is not { } win) return;
        var old = win.Rect;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_resize_by(_c, id, wNew - old.W, hNew - old.H);
        win.State = WindowState.Floating;
        if (win.Rect.W != old.W || win.Rect.H != old.H)
            OnWindowResized(Args(id));
    }

    // ── Snap / Tile / Maximize / Minimize ─────────────────────────

    public void Snap(int id, SnapZone zone)
    {
        if (Get(id) is not { } w) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_snap_zone(_c, id,
            zone == SnapZone.Left ? 0 : zone == SnapZone.Right ? 1 : -1);
        w.State = zone switch
        {
            SnapZone.Left => WindowState.SnappedLeft,
            SnapZone.Right => WindowState.SnappedRight,
            _ => WindowState.Floating,
        };
        OnWindowStateChanged(Args(id));
    }

    public void SendToCell(int id, TileGrid grid, int row, int col, int gap)
    {
        if (Get(id) is not { } w) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_send_to_cell(_c, id, grid.Rows, grid.Cols, row, col, gap);
        w.State = WindowState.Tiled;
        OnWindowStateChanged(Args(id));
    }

    public void TileAll(TileGrid grid, int gap)
    {
        var visible = ZOrdered.Where(w => !w.Minimized).ToList();
        if (visible.Count == 0) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_tile_all(_c, grid.Rows, grid.Cols, gap);
        foreach (var w in visible)
        {
            w.State = WindowState.Tiled;
            OnWindowStateChanged(Args(w.Id));
        }
    }

    public void MaximizeToggle(int id)
    {
        if (Get(id) is not { } w) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_maximize_toggle(_c, id);
        OnWindowStateChanged(Args(w.Id));
    }

    public void Minimize(int id)
    {
        if (Get(id) is not { } w) return;
        var wasFocused = Focused;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_minimize(_c, id);
        OnWindowStateChanged(Args(id));
        OnWindowMinimized(Args(id));
        if (wasFocused == id && Focused is int nid)
            OnWindowFocused(Args(nid));
    }

    public void Unminimize(int id)
    {
        if (Get(id) is not { } w) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_unminimize(_c, id);
        OnWindowRestored(Args(id));
        OnWindowStateChanged(Args(id));
        Raise(id);
    }

    public void Restore(int id)
    {
        if (Get(id) is not { } w) return;
        if (!w.Minimized) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_restore(_c, id);
        OnWindowStateChanged(Args(id));
        OnWindowRestored(Args(id));
    }

    public void Close(int id)
    {
        if (Get(id) is not { } w) return;
        var ev = Args(id);
        OnWindowClosing(ev);
        if (ev.Cancel) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_remove(_c, id);
        OnWindowClosed(Args(id));
        /* Nessun focus-transfer alla chiusura (semantica C: focus = -1). */
    }

    public bool IsMinimized(int id) => Get(id)?.Minimized ?? false;

    // ── Drag / Resize ──────────────────────────────────────────────

    public bool IsDragging => CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_is_dragging(_c) != 0;

    public void StartDrag(int id, int x, int y)
    {
        if (Get(id) is { Draggable: true, State: WindowState.Floating })
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_start_drag(_c, id, x, y);
    }

    public bool UpdateDrag(int x, int y)
    {
        if (!IsDragging) return false;
        var dragging = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_drag_id(_c);
        if (dragging < 0) return false;
        var old = Get(dragging)?.Rect ?? default;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_update_drag(_c, x, y);
        var now = Get(dragging)?.Rect ?? default;
        if (now.X != old.X || now.Y != old.Y) OnWindowMoved(Args(dragging));
        return true;
    }

    public int? EndDrag()
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_end_drag(_c);
        return id >= 0 ? id : null;
    }

    /// <summary>Trova la finestra sotto la titlebar in un punto (C).</summary>
    public int? WindowAtTitleBar(int x, int y)
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_at_titlebar(_c, x, y);
        return id >= 0 ? id : null;
    }

    public bool StartResize(int id, int x, int y, string edge)
    {
        if (Get(id) is not { State: WindowState.Floating }) return false;
        return CommonLib.Consoles.Tui.ClipperWmNative.StartResize(_c, id, x, y, edge ?? "") != 0;
    }

    public bool UpdateResize(int x, int y)
    {
        if (CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_is_resizing(_c) == 0) return false;
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_resize_id(_c);
        if (id < 0) return false;
        var old = Get(id)?.Rect ?? default;
        if (CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_update_resize(_c, x, y) == 0) return false;
        var w = Get(id);
        if (w is not null) w.State = WindowState.Floating;
        var now = w?.Rect ?? default;
        if (now.X != old.X || now.Y != old.Y || now.W != old.W || now.H != old.H)
            OnWindowResized(Args(id));
        return true;
    }

    public void EndResize()
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_resize_id(_c);
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_end_resize(_c);
        if (id >= 0) Raise(id);
    }

    /// <summary>Applica un'azione di controllo titlebar (min/max/close).</summary>
    public void ControlAction(int id, WinControl ctrl)
    {
        switch (ctrl)
        {
            case WinControl.Minimize: Minimize(id); break;
            case WinControl.Maximize: MaximizeToggle(id); break;
            case WinControl.Close: Close(id); break;
        }
    }

    // ── Input event-driven ─────────────────────────────────────────

    public Func<(string Title, ScreenRect Rect)>? NewWindowFactory { get; set; }

    /// <summary>Servizio clipboard per Ctrl+C/V/X sul contenuto delle finestre.</summary>
    public IClipboardService? ClipboardService { get; set; }

    /// <summary>Sposta il focus alla finestra successiva/precedente in ordine z (C).</summary>
    public int? FocusNext(bool forward)
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_focus_next(_c, forward ? 1 : 0);
        if (id < 0) return null;
        OnWindowFocused(Args(id));
        return id;
    }

    /// <summary>Gestisce un UiEvent per la gestione finestre. True se consumato.</summary>
    public bool HandleEvent(UiEvent evt)
    {
        switch (evt)
        {
            case UiKey(var key, var mods):
                if ((mods & KeyModifiers.Control) != 0 && HandleClipboardKey(key)) return true;
                if (HandleEditKey(key, mods)) return true;
                switch (key)
                {
                    case KeyConstants.K_TAB: return FocusNext(true) != null;
                    case KeyConstants.K_BTAB: return FocusNext(false) != null;
                    case KeyConstants.K_F6: return FocusNext(true) != null;
                    case KeyConstants.K_PGUP:
                        if (Focused is int puId && Get(puId) is { } puw) { puw.ScrollBy(-Math.Max(1, puw.ContentRect.H)); return true; }
                        return false;
                    case KeyConstants.K_PGDN:
                        if (Focused is int pdId && Get(pdId) is { } pdw) { pdw.ScrollBy(Math.Max(1, pdw.ContentRect.H)); return true; }
                        return false;
                    case 'n' or 'N':
                        if (NewWindowFactory == null) return false;
                        var (title, rect) = NewWindowFactory();
                        AddWindow(title, rect);
                        return true;
                    case KeyConstants.K_DEL:
                        if (Focused is int delId) { Close(delId); return true; }
                        return false;
                    case 'm' or 'M':
                        if (Focused is int mid) { MaximizeToggle(mid); return true; }
                        return false;
                }
                return false;

            case UiMouse(var ma):
                return HandleMouse(ma);

            default:
                return false;
        }
    }

    /// <summary>Processa un tasto sull'ActiveWindow (editing/clipboard).</summary>
    public bool ProcessKeyOnActive(int key, KeyModifiers mods)
    {
        if ((mods & KeyModifiers.Control) != 0 && HandleClipboardKey(key)) return true;
        return HandleEditKey(key, mods);
    }

    private bool HandleClipboardKey(int key)
    {
        if (Focused is not int cid || Get(cid) is not { } cw) return false;
        var upper = (char)(key >= 'a' && key <= 'z' ? key - 32 : key);
        switch (upper)
        {
            case 'C':
            {
                var text = string.Join(Environment.NewLine, cw.Lines);
                try { ClipboardService?.SetText(text); } catch { }
                return true;
            }
            case 'V':
                if (!cw.Editable) return false;
                try
                {
                    var pasted = ClipboardService?.GetText() ?? "";
                    foreach (var line in pasted.Split('\n'))
                        cw.AppendLine(line.TrimEnd('\r'));
                }
                catch { }
                return true;
            case 'X':
                if (!cw.Editable) return false;
                try { ClipboardService?.SetText(string.Join(Environment.NewLine, cw.Lines)); } catch { }
                cw.Lines.Clear();
                cw.ScrollOffset = 0;
                return true;
        }
        return false;
    }

    /// <summary>Editing da tastiera sulla finestra focalizzata se Editable.</summary>
    private bool HandleEditKey(int key, KeyModifiers mods)
    {
        if (Focused is not int eid || Get(eid) is not { } ew) return false;
        if (!ew.Editable) return false;
        var cr = ew.ContentRect;
        switch (key)
        {
            case KeyConstants.K_UP:
                if (ew.CursorLine > 0) ew.CursorLine--;
                ew.CursorCol = Math.Min(ew.CursorCol, ew.Lines.Count > 0 && ew.CursorLine < ew.Lines.Count ? ew.Lines[ew.CursorLine].Length : 0);
                if (ew.CursorLine < ew.ScrollOffset) ew.ScrollOffset = ew.CursorLine;
                return true;
            case KeyConstants.K_DOWN:
                if (ew.CursorLine < ew.Lines.Count - 1) ew.CursorLine++;
                ew.CursorCol = Math.Min(ew.CursorCol, ew.Lines.Count > 0 && ew.CursorLine < ew.Lines.Count ? ew.Lines[ew.CursorLine].Length : 0);
                if (ew.CursorLine >= ew.ScrollOffset + cr.H) ew.ScrollOffset = ew.CursorLine - cr.H + 1;
                return true;
            case KeyConstants.K_LEFT:
                if (ew.CursorCol > 0) ew.CursorCol--;
                return true;
            case KeyConstants.K_RIGHT:
                if (ew.Lines.Count > 0 && ew.CursorLine < ew.Lines.Count && ew.CursorCol < ew.Lines[ew.CursorLine].Length) ew.CursorCol++;
                return true;
            case KeyConstants.K_ENTER:
                if (ew.Lines.Count == 0) ew.Lines.Add("");
                var rest = ew.Lines[ew.CursorLine][ew.CursorCol..];
                ew.Lines[ew.CursorLine] = ew.Lines[ew.CursorLine][..ew.CursorCol];
                ew.Lines.Insert(ew.CursorLine + 1, rest);
                ew.CursorLine++;
                ew.CursorCol = 0;
                if (ew.CursorLine >= ew.ScrollOffset + cr.H) ew.ScrollOffset = ew.CursorLine - cr.H + 1;
                return true;
            case KeyConstants.K_BS:
                if (ew.Lines.Count == 0) return true;
                if (ew.CursorCol > 0)
                {
                    ew.Lines[ew.CursorLine] = ew.Lines[ew.CursorLine].Remove(ew.CursorCol - 1, 1);
                    ew.CursorCol--;
                }
                else if (ew.CursorLine > 0)
                {
                    var prevLen = ew.Lines[ew.CursorLine - 1].Length;
                    ew.Lines[ew.CursorLine - 1] += ew.Lines[ew.CursorLine];
                    ew.Lines.RemoveAt(ew.CursorLine);
                    ew.CursorLine--;
                    ew.CursorCol = prevLen;
                }
                return true;
            case KeyConstants.K_HOME: ew.CursorCol = 0; return true;
            case KeyConstants.K_END:
                if (ew.Lines.Count > 0 && ew.CursorLine < ew.Lines.Count) ew.CursorCol = ew.Lines[ew.CursorLine].Length;
                return true;
            default:
                if (key is >= 32 and <= 126)
                {
                    if (ew.Lines.Count == 0) ew.Lines.Add("");
                    ew.Lines[ew.CursorLine] = ew.Lines[ew.CursorLine].Insert(ew.CursorCol, ((char)key).ToString());
                    ew.CursorCol++;
                    return true;
                }
                break;
        }
        return false;
    }

    private bool HandleMouse(MouseAction ma)
    {
        var x = ma.X;
        var y = ma.Y;

        switch (ma.Kind)
        {
            case MouseActionKind.Click:
            case MouseActionKind.DoubleClick:
            {
                if (TaskbarChipAt(x, y) is int chipId) { Restore(chipId); return true; }
                var topId = TopmostAt(x, y);
                if (topId is not int id) return false;
                Raise(id);
                if (Get(id) is not { } w) return false;

                var ctrl = w.ControlAt(x, y);
                if (ctrl != null)
                {
                    ControlAction(id, ctrl.Value);
                    return true;
                }

                if (y == w.Rect.Y && w.State == WindowState.Floating && w.Draggable)
                {
                    var e = w.ResizeEdgeAt(x, y);
                    if (e is "ne" or "nw")
                        return StartResize(id, x, y, e);
                    StartDrag(id, x, y);
                    return true;
                }

                var edge = w.ResizeEdgeAt(x, y);
                if (edge != null && w.State == WindowState.Floating)
                    return StartResize(id, x, y, edge);
                return true;
            }

            case MouseActionKind.Drag:
                if (IsDragging) return UpdateDrag(x, y);
                if (CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_is_resizing(_c) != 0) return UpdateResize(x, y);
                return false;

            case MouseActionKind.Release:
                var ended = false;
                if (IsDragging) { EndDrag(); ended = true; }
                if (CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_is_resizing(_c) != 0) { EndResize(); ended = true; }
                return ended;

            case MouseActionKind.ScrollUp:
            case MouseActionKind.ScrollDown:
            {
                var id = TopmostAt(x, y);
                if (id is int scId && Get(scId) is { } sw2 && sw2.Lines.Count > 0)
                {
                    sw2.ScrollBy(ma.Kind == MouseActionKind.ScrollUp ? -3 : 3);
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    // ── Rendering ──────────────────────────────────────────────────

    /// <summary>Id della finestra il cui chip taskbar si trova in (x,y), o null (C).</summary>
    public int? TaskbarChipAt(int x, int y)
    {
        var id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_taskbar_chip_at(_c, x, y);
        return id >= 0 ? id : null;
    }

    private void DrawContentWithScroll(ScreenBuffer buf, ScreenRect cr, ManagedWindow win, Theme theme)
    {
        var showScroll = win.Lines.Count > cr.H;
        var contentWidth = showScroll ? Math.Max(1, cr.W - 1) : cr.W;
        for (var row = 0; row < cr.H; row++)
        {
            var idx = win.ScrollOffset + row;
            if (idx >= win.Lines.Count) break;
            var line = win.Lines[idx];
            var clip = line.Length > contentWidth ? line[..contentWidth] : line;
            for (var c = 0; c < clip.Length; c++)
                buf.SetChar(cr.X + c, cr.Y + row, clip[c], theme.Text, theme.WindowBg);
        }

        if (showScroll)
        {
            var barX = cr.X + cr.W - 1;
            var page = Math.Max(1, cr.H);
            var max = Math.Max(0, win.Lines.Count - page);
            var thumb = Math.Max(1, (int)Math.Round(cr.H * (double)page / Math.Max(1, win.Lines.Count)));
            var startThumb = max > 0 ? (int)Math.Round(win.ScrollOffset * (double)(cr.H - thumb) / max) : 0;

            buf.SetChar(barX, cr.Y, '▲', theme.Accent, theme.WindowBg);
            buf.SetChar(barX, cr.Y + cr.H - 1, '▼', theme.Accent, theme.WindowBg);

            for (var row = 1; row < cr.H - 1; row++)
            {
                var isThumb = row > startThumb && row <= startThumb + thumb - 1;
                buf.SetChar(barX, cr.Y + row, isThumb ? '█' : '░',
                    isThumb ? theme.Accent : theme.Dim, theme.WindowBg);
            }
        }
    }

    private static int contentW(ScreenRect cr, ManagedWindow win)
    {
        var showScroll = win.Lines.Count > cr.H;
        return Math.Max(0, (showScroll ? cr.W - 1 : cr.W) - 1);
    }

    /// <summary>Composita l'intero WindowManager in un buffer: cornici/desktop/status dal C,
    /// contenuto (Lines) delle finestre dal binding C.</summary>
    public void RenderWm(ScreenBuffer buf, Theme theme, string header, string status)
    {
        ControlStyle.SetTheme(theme);
        if (!string.IsNullOrEmpty(status)) SetStatusBar(status);
        CommonLib.Consoles.Tui.ClipperWmNative.RenderNamed(_c, buf.Handle, theme.Name, string.IsNullOrEmpty(header) ? null : header);
        foreach (var win in ZOrdered)
        {
            if (win.Minimized) continue;
            DrawWindowContent(buf, win, theme);
        }
    }

    /// <summary>Disegna SOLO il contenuto (Lines + scrollbar + cursore) nella ContentRect.</summary>
    private void DrawWindowContent(ScreenBuffer buf, ManagedWindow win, Theme theme)
    {
        var cr = win.ContentRect;
        if (win.Lines.Count > 0) DrawContentWithScroll(buf, cr, win, theme);
        if (win.Editable && win.Focused && win.Lines.Count > 0)
        {
            var visY = win.CursorLine - win.ScrollOffset;
            if (visY >= 0 && visY < cr.H)
            {
                var lineLen = win.CursorLine < win.Lines.Count ? win.Lines[win.CursorLine].Length : 0;
                var ch = win.CursorCol < lineLen ? win.Lines[win.CursorLine][win.CursorCol] : ' ';
                buf.SetChar(cr.X + Math.Min(win.CursorCol, contentW(cr, win)), cr.Y + visY, ch, theme.Accent, theme.Text);
            }
        }
    }
}
