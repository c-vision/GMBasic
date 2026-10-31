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
public class WindowManager
{
    private ScreenRect work;
    // C autoritativo: struct C persistente (gmb_window_manager_t). SyncAll() la riallinea
    // dalle finestre C#; gli op delegano a *_(_c); il finalizer la libera.
    private readonly IntPtr _c;
    private readonly List<ManagedWindow> windows = [];
    private int? focus;
    private int nextId = 1;
    private int nextZ = 1;

    // Drag delegato al C (lo stato offset vive in _c): qui solo l'id in corso.
    private int? _dragId;
    private int? _resizeId;
    /// <summary>Testo impostato da STATUSBAR "text" — condiviso con la riga della taskbar
    /// (prima erano due canali separati: STATUSBAR scriveva su System.Console fuori da una
    /// finestra, mai collegato a RenderWm/RenderStatus).</summary>
    private string _statusText = "";
    public void SetStatusBar(string text) => _statusText = text ?? "";
    public bool HasStatusText => !string.IsNullOrEmpty(_statusText);
    /// <summary>Hitbox dei chip taskbar (finestre minimizzate) sull'ultimo render — l'unico
    /// modo per farle ricomparire con un click, oltre al comando esplicito RESTORE.</summary>
    private readonly List<(int id, int x0, int x1, int y)> _taskbarChipHitboxes = [];

    public WindowManager(ScreenRect workArea)
    {
        work = workArea;
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

    private WindowEventArgs Args(ManagedWindow w) =>
        new(w.Id) { Title = w.Title, Rect = w.Rect, State = w.State };

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

    public ScreenRect WorkArea
    {
        get => work;
        set => work = value;
    }

    /// <summary>Sincronizza l'area di lavoro con le dimensioni REALI del buffer di render
    /// (console/terminale): il work nasce 80x24 ma se il terminale è più grande le finestre
    /// maximiz/tile su WorkArea restano confinate in un angolo, con il desktop scoperto.</summary>
    public void SyncWorkArea(int width, int height)
    {
        var w = Math.Max(1, width);
        var h = Math.Max(1, height);
        if (work.W != w || work.H != h)
            work = new ScreenRect(work.X, work.Y, w, h);
    }

    // ── Virtual Desktops ──────────────────────────────────────────

    /// <summary>Desktop virtuale attivo (1-based, default 1).</summary>
    public int CurrentDesktop { get; private set; } = 1;

    /// <summary>Passa a un altro desktop virtuale.</summary>
    public void SwitchDesktop(int desktop)
    {
        CurrentDesktop = Math.Max(1, desktop);
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_switch_desktop(_c, CurrentDesktop);
        focus = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_focused(_c);
        if (focus is int ff && ff < 0) focus = null;
        foreach (var w in windows)
            w.Focused = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_focused(_c, w.Id) != 0;
        ActiveWindowId = focus;
    }

    /// <summary>Sposta una finestra su un desktop virtuale.</summary>
    public void MoveWindowToDesktop(int windowId, int desktop)
    {
        var w = Get(windowId);
        if (w == null) return;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_to_desktop(_c, windowId, Math.Max(1, desktop));
        w.Desktop = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_desktop(_c, windowId);
        focus = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_focused(_c);
        if (focus is int ff && ff < 0) focus = null;
        foreach (var win in windows)
            win.Focused = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_focused(_c, win.Id) != 0;
        ActiveWindowId = focus;
    }

    /// <summary>Sposta una finestra per nome/titolo su un desktop virtuale.</summary>
    public void MoveWindowToDesktop(string name, int desktop)
    {
        if (FindByTitle(name) is int id)
            MoveWindowToDesktop(id, desktop);
    }

    public int WindowCount => windows.Count(w => w.Desktop == CurrentDesktop);
    public int TotalWindowCount => windows.Count;

    // ── Window CRUD ────────────────────────────────────────────────

    public int AddWindow(string title, ScreenRect rect)
    {
        var ev = new WindowEventArgs(nextId) { Title = title, Rect = rect, State = WindowState.Floating };
        OnWindowOpening(ev);
        if (ev.Cancel) return -1;

        var id = nextId++;
        var z = nextZ++;
        var win = new ManagedWindow
        {
            Id = id,
            Title = title,
            Rect = rect,
            Z = z,
            State = WindowState.Floating,
            RestoreRect = rect,
            Desktop = CurrentDesktop,
        };
        windows.Add(win);
        // C autoritativo: crea subito la finestra in C e lega il proxy (nessuna sync dopo).
        CommonLib.Consoles.Tui.ClipperWmNative.Push(_c, string.IsNullOrEmpty(title) ? win.Name : title, id,
            rect.X, rect.Y, rect.W, rect.H, z, 0, 0, (int)win.Border, 1, CurrentDesktop, win.Closable ? 1 : 0);
        win.C = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_get(_c, id);
        win.RestoreRect = rect;
        var wasFocused = focus;
        focus = id;
        OnWindowOpened(Args(win));
        if (wasFocused != id)
            OnWindowFocused(Args(win));
        return id;
    }

    public ManagedWindow? Get(int id)
    {
        var w = windows.Find(x => x.Id == id);
        if (w != null && w.C == IntPtr.Zero)
            w.C = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_get(_c, id);
        return w;
    }

    public int? Focused => focus;

    /// <summary>Finestre del desktop attivo ordinate per z (dalla più bassa alla più alta).</summary>
    public List<ManagedWindow> ZOrdered =>
        [.. windows.Where(w => w.Desktop == CurrentDesktop).OrderBy(w => w.Z)];

    /// <summary>Finestra visibile più in alto nel desktop corrente in un punto, o null.</summary>
    public int? TopmostAt(int x, int y) =>
        windows
            .Where(w => w.Desktop == CurrentDesktop && !w.Minimized && w.Rect.Contains(x, y))
            .OrderByDescending(w => w.Z)
            .FirstOrDefault()?.Id;

    public void Raise(int id)
    {
        var w = Get(id);
        if (w == null) return;
        var wasFocused = focus;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_raise(_c, id);
        w.Z = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_z(_c, id);
        w.Focused = true;
        focus = id;
        ActiveWindowId = id; // il click/raise rende la finestra la viewport attiva
        if (wasFocused != id)
            OnWindowFocused(Args(w));
    }

    public void RemoveWindow(int id) => Close(id);

    /// <summary>Rimuove il focus dalla finestra (WINDOW DEACTIVATE) — il focus torna null
    /// se la finestra indicata era quella focalizzata.</summary>
    public void Deactivate(int id)
    {
        if (focus != id) return;
        var w = Get(id);
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_deactivate(_c, id);
        if (w != null) { w.Focused = false; OnWindowFocused(Args(w)); }
        focus = null;
    }

    /// <summary>Cerca l'ID della finestra per titolo O nome (case-insensitive) — usato dai
    /// comandi WINDOW name ACTION e WINDOW SELECT per mappare il nome/titolo all'ID.</summary>
    public int? FindByTitle(string title)
    {
        if (string.IsNullOrEmpty(title)) return null;
        return windows.FirstOrDefault(w => w.Title.Equals(title, StringComparison.OrdinalIgnoreCase)
            || (!string.IsNullOrEmpty(w.Name) && w.Name.Equals(title, StringComparison.OrdinalIgnoreCase)))?.Id;
    }

    /// <summary>Imposta il titolo della finestra focalizzata (WINDOW SET TITLE). Ritorna
    /// l'id della finestra modificata, o null se non c'è una finestra con focus.</summary>
    public int? SetFocusedTitle(string title)
    {
        if (focus is not int fid) return null;
        var w = Get(fid);
        if (w == null) return null;
        w.Title = title;
        return fid;
    }

    /// <summary>Id della finestra attiva (viewport di output corrente), o null se lo schermo
    /// base. Sincronizzato con `focus`.</summary>
    public int? ActiveWindowId { get; private set; }

    /// <summary>Seleziona la finestra attiva (viewport corrente): id o nome. null → schermo
    /// base (nessuna finestra attiva). Non cambia lo z-order.</summary>
    public int? Select(int? id)
    {
        if (id is int nid && Get(nid) != null)
        {
            SyncAll();
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_select(_c, nid);
            focus = nid;
            var w = Get(nid);
            if (w != null) { w.Focused = true; OnWindowFocused(Args(w)); }
        }
        ActiveWindowId = id;
        return ActiveWindowId;
    }
    public int? Select(string? title)
    {
        if (string.IsNullOrEmpty(title)) return Select((int?)null);
        return Select(FindByTitle(title));
    }

    // ── Move / Resize ──────────────────────────────────────────────

    public void MoveBy(int id, int dx, int dy)
    {
        var w = Get(id);
        if (w == null) return;
        var old = w.Rect;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_by(_c, id, dx, dy);
        ReadRect(id, w);
        if (w.State != WindowState.Floating)
            w.State = WindowState.Floating;
        if (w.Rect.X != old.X || w.Rect.Y != old.Y)
            OnWindowMoved(Args(w));
    }

    public void MoveTo(int id, int x, int y)
    {
        var w = Get(id);
        if (w == null) return;
        var old = w.Rect;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_move_to(_c, id, x, y);
        ReadRect(id, w);
        w.State = WindowState.Floating;
        if (w.Rect.X != old.X || w.Rect.Y != old.Y)
            OnWindowMoved(Args(w));
    }

    public void ResizeTo(int id, int wNew, int hNew)
    {
        var win = Get(id);
        if (win == null) return;
        var old = win.Rect;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_resize_by(_c, id, wNew - old.W, hNew - old.H);
        ReadRect(id, win);
        win.State = WindowState.Floating;
        if (win.Rect.W != old.W || win.Rect.H != old.H)
            OnWindowResized(Args(win));
    }

    /// <summary>Read-back del rect della finestra id dalla struct C in <paramref name="w"/>.</summary>
    private void ReadRect(int id, ManagedWindow w)
    {
        var r = new int[4];
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_rect(_c, id, r);
        w.Rect = new ScreenRect(r[0], r[1], r[2], r[3]);
    }

    // ── Snap / Tile / Maximize / Minimize ─────────────────────────

    public void Snap(int id, SnapZone zone)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State != WindowState.Floating)
            w.RestoreRect = w.Rect;
        // Geometria delegata al C (gmb_window_manager_snap_zone); lo stato (Snapped*) è C#.
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_snap_zone(_c, id,
            zone == SnapZone.Left ? 0 : zone == SnapZone.Right ? 1 : -1);
        ReadRect(id, w);
        w.State = zone switch
        {
            SnapZone.Left => WindowState.SnappedLeft,
            SnapZone.Right => WindowState.SnappedRight,
            _ => WindowState.Floating,
        };
        OnWindowStateChanged(Args(w));
    }

    public void SendToCell(int id, TileGrid grid, int row, int col, int gap)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State != WindowState.Floating)
            w.RestoreRect = w.Rect;
        // Geometria delegata al runtime C (gmb_window_manager_send_to_cell) + read-back.
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_send_to_cell(_c, id, grid.Rows, grid.Cols, row, col, gap);
        var r = new int[4];
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_rect(_c, id, r);
        w.Rect = new ScreenRect(r[0], r[1], r[2], r[3]);
        w.State = WindowState.Tiled;
        OnWindowStateChanged(Args(w));
    }

    public void TileAll(TileGrid grid, int gap)
    {
        var visible = windows.Where(w => !w.Minimized).OrderBy(w => w.Z).ToList();
        if (visible.Count == 0) return;
        // Geometria delegata al runtime C (gmb_window_manager_tile_all) + read-back + eventi.
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_tile_all(_c, grid.Rows, grid.Cols, gap);
        var r = new int[4];
        foreach (var w in visible)
        {
            if (w.State != WindowState.Floating) w.RestoreRect = w.Rect;
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_rect(_c, w.Id, r);
            w.Rect = new ScreenRect(r[0], r[1], r[2], r[3]);
            w.State = WindowState.Tiled;
            OnWindowStateChanged(Args(w));
        }
    }

    public void MaximizeToggle(int id)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State == WindowState.Maximized)
        {
            w.Rect = w.RestoreRect;
            w.State = WindowState.Floating;
        }
        else
        {
            if (w.State == WindowState.Floating)
                w.RestoreRect = w.Rect;
            w.Rect = work;
            w.State = WindowState.Maximized;
        }
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_maximize_toggle(_c, id);
        ReadRect(id, w);
        OnWindowStateChanged(Args(w));
    }

    public void Minimize(int id)
    {
        var w = Get(id);
        if (w == null) return;
        var wasFocused = focus;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_minimize(_c, id);
        w.Minimized = true;
        w.Focused = false;
        if (focus == id)
        {
            focus = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_focused(_c);
            if (focus is int ff && ff < 0) focus = null;
        }
        OnWindowStateChanged(Args(w));
        OnWindowMinimized(Args(w));
        if (wasFocused == id && focus is int nid && Get(nid) is ManagedWindow nw)
            OnWindowFocused(Args(nw));
    }

    public void Unminimize(int id)
    {
        var w = Get(id);
        if (w == null) return;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_unminimize(_c, id);
        w.Minimized = false;
        OnWindowRestored(Args(w));
        OnWindowStateChanged(Args(w));
        Raise(id);
    }

    public void Restore(int id)
    {
        var w = Get(id);
        if (w == null) return;
        if (!w.Minimized) return;
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_restore(_c, id);
        w.Minimized = false;
        OnWindowStateChanged(Args(w));
        OnWindowRestored(Args(w));
    }

    public void Close(int id)
    {
        var w = Get(id);
        if (w == null) return;
        var ev = Args(w);
        OnWindowClosing(ev);
        if (ev.Cancel) return;
        windows.RemoveAll(w => w.Id == id);
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_remove(_c, id);
        OnWindowClosed(Args(w));
        if (focus == id)
        {
            focus = windows.Where(x => !x.Minimized).OrderByDescending(x => x.Z)
                .FirstOrDefault()?.Id;
            if (focus is int nid && Get(nid) is ManagedWindow nw)
                OnWindowFocused(Args(nw));
        }
    }

    public bool IsMinimized(int id) => Get(id)?.Minimized ?? false;

    // ── Drag ───────────────────────────────────────────────────────

    public bool IsDragging => _dragId.HasValue;

    public void StartDrag(int id, int x, int y)
    {
        var w = Get(id);
        if (w is { Draggable: true, State: WindowState.Floating })
        {
            SyncAll();
            CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_start_drag(_c, id, x, y);
            _dragId = id;
        }
    }

    public bool UpdateDrag(int x, int y)
    {
        if (_dragId is not int id) return false;
        var w = Get(id);
        if (w == null) { _dragId = null; return false; }
        var old = w.Rect;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_update_drag(_c, x, y);
        ReadRect(id, w);
        if (w.Rect.X != old.X || w.Rect.Y != old.Y)
            OnWindowMoved(Args(w));
        return true;
    }

    public int? EndDrag()
    {
        if (_dragId is not int id) return null;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_end_drag(_c);
        _dragId = null;
        return id;
    }

    /// <summary>Trova la finestra sotto la titlebar in un punto (delegato al C).</summary>
    public int? WindowAtTitleBar(int x, int y)
    {
        SyncAll();
        int id = CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_window_at_titlebar(_c, x, y);
        return id >= 0 ? id : (int?)null;
    }

    /// <summary>Avvia resize lato/corner (delegato al C: edge-string N/S/E/W).</summary>
    public bool StartResize(int id, int x, int y, string edge)
    {
        var w = Get(id);
        if (w == null || w.State != WindowState.Floating) return false;
        SyncAll();
        if (CommonLib.Consoles.Tui.ClipperWmNative.StartResize(_c, id, x, y, edge ?? "") == 0) return false;
        _resizeId = id;
        return true;
    }

    public bool UpdateResize(int x, int y)
    {
        if (_resizeId is not int id) return false;
        var w = Get(id);
        if (w == null) { _resizeId = null; return false; }
        var old = w.Rect;
        if (CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_update_resize(_c, x, y) == 0) return false;
        ReadRect(id, w);
        w.State = WindowState.Floating;
        if (w.Rect.X != old.X || w.Rect.Y != old.Y || w.Rect.W != old.W || w.Rect.H != old.H)
            OnWindowResized(Args(w));
        return true;
    }

    public void EndResize()
    {
        if (_resizeId is not int id) return;
        CommonLib.Consoles.Tui.ClipperWmNative.gmb_window_manager_end_resize(_c);
        _resizeId = null;
        Raise(id);
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

    /// <summary>Fornisce titolo e rect per la nuova finestra creata con 'N'.</summary>
    public Func<(string Title, ScreenRect Rect)>? NewWindowFactory { get; set; }

    /// <summary>Servizio clipboard per Ctrl+C/V/X sul contenuto delle finestre. Iniettato dal
    /// runtime host (il WindowManager non deve crearlo). Null → clipboard disabilitata.</summary>
    public IClipboardService? ClipboardService { get; set; }

    /// <summary>Sposta il focus alla finestra successiva/precedente in ordine z (escluso minimized).</summary>
    public int? FocusNext(bool forward)
    {
        var list = windows.Where(x => !x.Minimized).OrderBy(x => x.Z).Select(x => x.Id).ToList();
        if (list.Count == 0) return null;
        var idx = focus.HasValue ? list.IndexOf(focus.Value) : -1;
        var next = forward
            ? (idx < 0 ? 0 : (idx + 1) % list.Count)
            : (idx < 0 ? list.Count - 1 : (idx - 1 + list.Count) % list.Count);
        var id = list[next];
        Raise(id);
        return id;
    }

    /// <summary>
    /// Gestisce un <see cref="UiEvent"/> per la gestione finestre: tasti (Tab/Shift+Tab
    /// ciclano, N crea, DEL chiude, M maximizza) e mouse (focus, titlebar, drag, resize).
    /// True se l'evento è stato consumato.
    /// </summary>
    public bool HandleEvent(UiEvent evt)
    {
        switch (evt)
        {
            case UiKey(var key, var mods):
                // Clipboard (Ctrl+C / Ctrl+V / Ctrl+X) sulla finestra focalizzata.
                if ((mods & KeyModifiers.Control) != 0)
                {
                    if (HandleClipboardKey(key)) return true;
                }
                // Editing (se la finestra focalizzata è editable) — riusa la logica TextArea.
                if (HandleEditKey(key, mods)) return true;
                switch (key)
                {
                    case KeyConstants.K_TAB: return FocusNext(true) != null;
                    case KeyConstants.K_BTAB: return FocusNext(false) != null;
                    case KeyConstants.K_F6: return FocusNext(true) != null;
                    case KeyConstants.K_PGUP:
                        if (focus is int puId && Get(puId) is { } puw) { puw.ScrollBy(-Math.Max(1, puw.ContentRect.H)); return true; }
                        return false;
                    case KeyConstants.K_PGDN:
                        if (focus is int pdId && Get(pdId) is { } pdw) { pdw.ScrollBy(Math.Max(1, pdw.ContentRect.H)); return true; }
                        return false;
                    case 'n' or 'N':
                        if (NewWindowFactory == null) return false;
                        var (title, rect) = NewWindowFactory();
                        AddWindow(title, rect);
                        return true;
                    case KeyConstants.K_DEL:
                        if (focus is int delId) { Close(delId); return true; }
                        return false;
                    case 'm' or 'M':
                        if (focus is int mid) { MaximizeToggle(mid); return true; }
                        return false;
                }
                return false;

            case UiMouse(var ma):
                return HandleMouse(ma);

            default:
                return false;
        }
    }

    /// <summary>Clipboard su finestra focalizzata: Ctrl+C copia, Ctrl+V incolla (se editable),
    /// Ctrl+X taglia (se editable). Usa l'IClipboardService iniettato.</summary>
    /// <summary>Processa un tasto sull'ActiveWindow (editing/clipboard). Ritorna true se
    /// consumato. Usato da INKEY/WAITKEY quando la finestra attiva è editable.</summary>
    public bool ProcessKeyOnActive(int key, KeyModifiers mods)
    {
        if ((mods & KeyModifiers.Control) != 0 && HandleClipboardKey(key)) return true;
        return HandleEditKey(key, mods);
    }

    private bool HandleClipboardKey(int key)
    {
        if (focus is not int cid || Get(cid) is not { } cw) return false;
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

    /// <summary>Editing da tastiera sulla finestra focalizzata se Editable (riusa la semantica
    /// TextArea: frecce/cursore/Enter/Backspace/caratteri).</summary>
    private bool HandleEditKey(int key, KeyModifiers mods)
    {
        if (focus is not int eid || Get(eid) is not { } ew) return false;
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
                // Taskbar sempre in primo piano: un chip minimizzato intercetta il click
                // prima di qualunque finestra sottostante.
                if (TaskbarChipAt(x, y) is int chipId) { Restore(chipId); return true; }
                var topId = TopmostAt(x, y);
                if (topId is not int id) return false;
                Raise(id);
                var w = Get(id);
                if (w == null) return false;

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
                if (_resizeId.HasValue) return UpdateResize(x, y);
                return false;

            case MouseActionKind.Release:
                var ended = false;
                if (IsDragging) { EndDrag(); ended = true; }
                if (_resizeId.HasValue) { EndResize(); ended = true; }
                return ended;

            case MouseActionKind.ScrollUp:
            case MouseActionKind.ScrollDown:
            {
                // Rotella: scroll della finestra sotto il cursore (se ha contenuto).
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


    /// <summary>Id della finestra il cui chip taskbar si trova in (x,y), o null.</summary>
    public int? TaskbarChipAt(int x, int y)
    {
        foreach (var (id, x0, x1, cy) in _taskbarChipHitboxes)
            if (y == cy && x >= x0 && x < x1) return id;
        return null;
    }

    /// <summary>Calcola gli hitbox dei chip delle finestre minimizzate (layout identico al
    /// render C: label dal fondo, larghezza = lunghezza label). Usato dal click sulla taskbar.</summary>
    private void ComputeTaskbarChips(int width, int y)
    {
        _taskbarChipHitboxes.Clear();
        var minimized = windows.Where(w => w.Desktop == CurrentDesktop && w.Minimized).OrderBy(w => w.Z).ToList();
        var x2 = width;
        foreach (var w in minimized)
        {
            var label = " " + (string.IsNullOrEmpty(w.Title) ? w.Name : w.Title) + " ";
            if (label.Length > 16) label = " " + label[1..14] + "… ";
            var x0 = x2 - label.Length;
            if (x0 < 1) break;
            _taskbarChipHitboxes.Add((w.Id, x0, x2, y));
            x2 = x0 - 1;
        }
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

    /// <summary>Composita l'intero WindowManager in un buffer: sfondo + finestre + status.</summary>
    public void RenderWm(ScreenBuffer buf, Theme theme, string header, string status)
    {
        ControlStyle.SetTheme(theme);
        if (!string.IsNullOrEmpty(status)) _statusText = status;
        // Composito (desktop+cornici+titlebar+statusbar) reso dal runtime C.
        SyncAll();
        CommonLib.Consoles.Tui.ClipperWmNative.SetStatusBar(_c, _statusText);
        CommonLib.Consoles.Tui.ClipperWmNative.RenderNamed(_c, buf.Handle, theme.Name, string.IsNullOrEmpty(header) ? null : header);
        ComputeTaskbarChips(buf.Width, buf.Height - 1);
        // Contenuto (Lines) delle finestre: disegnato dal C# (split render).
        foreach (var win in windows)
        {
            if (win.Minimized || win.Desktop != CurrentDesktop) continue;
            DrawWindowContent(buf, win, theme);
        }
    }

    /// <summary>Riallinea la struct C persistente (gmb_window_manager_t, _c) dalle finestre C#
    /// (stessi id C#). Chiamata prima degli op delegati al runtime C.</summary>
    private void SyncAll() { /* La finestra C ' la sorgente: nessun push dal C#. */ }

    /// <summary>Disegna SOLO il contenuto (Lines + scrollbar + cursore) nella ContentRect.
    /// Cornici/titlebar/statusbar sono rese dal runtime C (`gmb_window_manager_render_named`).</summary>
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
