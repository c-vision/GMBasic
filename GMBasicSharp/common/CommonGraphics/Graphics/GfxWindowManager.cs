// FILE: CommonGraphics.Graphics.GfxWindowManager.cs
// Fase F2 del piano Window-Pixel-Based: compositor di finestre PIXEL su schermo grafico.
// Ogni finestra è un PixelWindow con rect in PIXEL LOGICI (x,y,w,h), titlebar, bordo
// arrotondato, ombra, z-order, stato (Floating/Snapped/Maximized/Minimized). La macchina
// a stati (focus, drag, resize, snap, min/max/close) replica ClipperLib.WindowManager ma
// in pixel-space. Il rendering avviene sul buffer della pagina attiva via i primitivi
// Gfx* (GfxWindowCanvas) — tutto headless-safe (renderer software incluso).
//
// NOTA architetturale: il WindowManager cell-based (ClipperLib) resta il default; questo
// è il manager SEPARATO usato dalle finestre con clausola PIXEL in WINDOW DEFINE.

using System.Runtime.InteropServices;
using Basic;
using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

/// <summary>Stato di una finestra pixel.</summary>
public enum GfxWindowState
{
    Floating,
    SnappedLeft,
    SnappedRight,
    Maximized,
    Minimized,
}

/// <summary>Zona di snap per una finestra pixel.</summary>
public enum GfxSnapZone
{
    Left,
    Right,
}

/// <summary>Finestra pixel gestita dal GfxWindowManager.</summary>
public sealed class PixelWindow
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public string Title { get; set; } = "";
    public int X { get; set; }
    public int Y { get; set; }
    public int W { get; set; }
    public int H { get; set; }
    public int Z { get; set; }
    public GfxWindowState State { get; set; } = GfxWindowState.Floating;
    /// <summary>Desktop virtuale di appartenenza (1-based, default 1). Parità col
    /// WindowManager TUI (ClipperLib.Controls): le finestre di altri desktop non vengono
    /// renderizzate né colpite da hit-test finché SwitchDesktop non ci porta lì.</summary>
    public int Desktop { get; set; } = 1;
    public int RestoreX, RestoreY, RestoreW, RestoreH;
    public bool Focused { get; set; }
    public bool Shadow { get; set; } = true;
    public int BorderRadius { get; set; } = 6;
    public bool Closable { get; set; } = true;
    public bool Draggable { get; set; } = true;
    public bool Resizable { get; set; } = true;

    // ── Colori (ARGB). Default = tema scuro attuale; COLOR fg,bg con finestra attiva li cambia. ──
    /// <summary>Colore di sfondo del corpo della finestra (0xAARRGGBB).</summary>
    public uint BodyColor { get; set; } = 0xFF14161C;
    /// <summary>Colore del testo del contenuto (0xAARRGGBB).</summary>
    public uint FgColor { get; set; } = 0xFFD0D6E0;
    /// <summary>Colore del titolo nella titlebar (0xAARRGGBB).</summary>
    public uint TitleColor { get; set; } = 0xFFD6DCE6;

    // ── Contenuto (buffer testo, come ManagedWindow.Lines) ──
    public List<string> Lines { get; set; } = [];
    public int ScrollOffset { get; set; }
    public bool Editable { get; set; }
    public int CursorLine { get; set; }
    public int CursorCol { get; set; }

    /// <summary>Altezza titlebar in pixel logici.</summary>
    public int TitleHeight => 24;

    /// <summary>Margine interno (padding) del client area in pixel logici.</summary>
    public int ClientPadding => 6;

    /// <summary>Rettangolo client (esclusi titlebar e bordo).</summary>
    public (int x, int y, int w, int h) ClientRect
    {
        get
        {
            var pad = ClientPadding;
            return (X + pad, Y + TitleHeight, W - 2 * pad, H - TitleHeight - pad);
        }
    }

    public void AppendLine(string line)
    {
        var cr = ClientRect;
        var wasAtBottom = ScrollOffset + cr.h >= Lines.Count || Lines.Count == 0;
        Lines.Add(line ?? "");
        if (wasAtBottom && Lines.Count > cr.h / 16)
            ScrollOffset = Lines.Count - cr.h / 16;
    }

    /// <summary>Scrive un testo alla riga/colonna RELATIVA del client area (0-based).
    /// Colonna e riga sono in caratteri (layout colonne/righe nel client area).</summary>
    public string SayAt(int row, int col, string text)
    {
        var cr = ClientRect;
        var charW = GfxWindowManager.CharWidth();
        var charH = GfxWindowManager.CharHeight();
        if (charW <= 0) charW = 8;
        if (charH <= 0) charH = 16;
        var maxCols = Math.Max(1, cr.w / charW);
        var maxRows = Math.Max(1, cr.h / charH);
        var absRow = ScrollOffset + row;
        while (Lines.Count <= absRow) Lines.Add("");
        var line = Lines[absRow];
        var start = Math.Max(0, Math.Min(col, maxCols - 1));
        if (line.Length < start) line = line.PadRight(start);
        var n = Math.Min(text?.Length ?? 0, maxCols - start);
        var piece = (text ?? "")[..n];
        line = line.Remove(start, Math.Min(piece.Length, line.Length - start)).Insert(start, piece);
        Lines[absRow] = line;
        return line;
    }

    /// <summary>Disegna un box (bordi) nel buffer con coordinate relative al client area.</summary>
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

    /// <summary>Snapshot del buffer (SAVESCREEN sulla finestra pixel).</summary>
    public PixelWindow Snapshot() => new()
    {
        Id = Id, Name = Name, Title = Title, X = X, Y = Y, W = W, H = H, Z = Z,
        State = State, RestoreX = RestoreX, RestoreY = RestoreY, RestoreW = RestoreW, RestoreH = RestoreH,
        Focused = Focused, Shadow = Shadow, BorderRadius = BorderRadius,
        Closable = Closable, Draggable = Draggable, Resizable = Resizable,
        Lines = [.. Lines], ScrollOffset = ScrollOffset,
        Editable = Editable, CursorLine = CursorLine, CursorCol = CursorCol,
    };

    /// <summary>Ripristina lo snapshot del buffer (RESTORESCREEN sulla finestra pixel).</summary>
    public void RestoreFrom(PixelWindow snap)
    {
        Lines = [.. snap.Lines];
        ScrollOffset = snap.ScrollOffset;
        Editable = snap.Editable;
        CursorLine = snap.CursorLine;
        CursorCol = snap.CursorCol;
    }
}

/// <summary>Compositor di finestre pixel sul buffer della pagina grafica attiva.</summary>
public sealed class GfxWindowManager
{
    private readonly List<PixelWindow> windows = [];
    private int? focus;
    private int? active;
    private int nextId = 1;
    private int nextZ = 1;
    private int workW = 640;
    private int workH = 480;
    private (int id, int offX, int offY)? dragState;
    private (int id, int sx, int sy, int sw, int sh, string edge)? resizeState;

    public int ScreenW { get => workW; set => workW = Math.Max(1, value); }
    public int ScreenH { get => workH; set => workH = Math.Max(1, value); }

    // ── Virtual Desktops ─────────────────────────────────────────
    /// <summary>Desktop virtuale attivo (1-based, default 1). Parità TUI.</summary>
    public int CurrentDesktop { get; private set; } = 1;

    /// <summary>Cambia desktop attivo: le window filtrate ricadono sul nuovo desktop e il
    /// focus/active si riallineano alla finestra visibile più alta (se il vecchio focus
    /// non è sul desktop nuovo).</summary>
    public void SwitchDesktop(int desktop)
    {
        CurrentDesktop = Math.Max(1, desktop);
        if (focus is int fid && Get(fid) is { } fw && fw.Desktop != CurrentDesktop)
        {
            focus = windows.Where(w => w.Desktop == CurrentDesktop && w.State != GfxWindowState.Minimized)
                           .OrderByDescending(w => w.Z).FirstOrDefault()?.Id;
            active = focus;
            foreach (var w in windows)
                w.Focused = w.Id == focus;
        }
    }

    /// <summary>Sposta una finestra su un desktop virtuale (se non è sul desktop attivo,
    /// perde il focus/active come nel TUI).</summary>
    public void MoveWindowToDesktop(int id, int desktop)
    {
        if (Get(id) is not { } w) return;
        w.Desktop = Math.Max(1, desktop);
        if (w.Desktop != CurrentDesktop && focus == id)
        {
            focus = windows.Where(x => x.Desktop == CurrentDesktop && x.State != GfxWindowState.Minimized)
                           .OrderByDescending(x => x.Z).FirstOrDefault()?.Id;
            active = focus;
            foreach (var x in windows)
                x.Focused = x.Id == focus;
        }
    }

    public IReadOnlyList<PixelWindow> Windows => windows;

    /// <summary>Id della finestra attiva (viewport output corrente), o null.</summary>
    public int? ActiveId => active;

    // ── CRUD ─────────────────────────────────────────────────────

    public int AddWindow(string name, string title, int x, int y, int w, int h, bool shadow = true)
    {
        var id = nextId++;
        var win = new PixelWindow
        {
            Id = id,
            Name = name,
            Title = title,
            X = x,
            Y = y,
            W = Math.Max(16, w),
            H = Math.Max(24, h),
            Z = nextZ++,
            State = GfxWindowState.Floating,
            RestoreX = x,
            RestoreY = y,
            RestoreW = Math.Max(16, w),
            RestoreH = Math.Max(24, h),
            Shadow = shadow,
        };
        windows.Add(win);
        focus = id;
        active = id;
        win.Focused = true;
        return id;
    }

    public PixelWindow? Get(int id) => windows.Find(w => w.Id == id);
    public PixelWindow? Find(string name) => windows.Find(w =>
        w.Name.Equals(name, StringComparison.OrdinalIgnoreCase)
        || w.Title.Equals(name, StringComparison.OrdinalIgnoreCase));

    public int? FocusedId => focus;

    /// <summary>Finestre ordinate per z (dalla più bassa alla più alta), SOLO quelle del
    /// desktop corrente (finestre di altri desktop non sono visibili).</summary>
    public List<PixelWindow> ZOrdered => [.. windows.Where(w => w.Desktop == CurrentDesktop).OrderBy(w => w.Z)];

    /// <summary>Il desktop corrente ha finestre non minimizzate (per far mostrare la
    /// taskbar anche senza minimizzate, se un desktop è vuoto).</summary>
    public int WindowCount => windows.Count(w => w.Desktop == CurrentDesktop);

    public PixelWindow? TopmostAt(int x, int y) =>
        windows
            .Where(w => w.Desktop == CurrentDesktop && w.State != GfxWindowState.Minimized && x >= w.X && x < w.X + w.W && y >= w.Y && y < w.Y + w.H)
            .OrderByDescending(w => w.Z)
            .FirstOrDefault();

    public void Raise(int id)
    {
        var w = Get(id);
        if (w == null) return;
        w.Z = nextZ++;
        w.Focused = true;
        focus = id;
        active = id;
    }

    public void Select(int? id)
    {
        active = id;
        if (id is int nid && Get(nid) is { } w)
        {
            focus = nid;
            w.Focused = true;
        }
    }

    public void Close(int id)
    {
        windows.RemoveAll(w => w.Id == id);
        if (focus == id)
        {
            focus = windows.Count > 0 ? windows.MaxBy(w => w.Z)?.Id : null;
            active = focus;
        }
    }

    // ── Azioni stato ──────────────────────────────────────────────

    public void MoveTo(int id, int x, int y)
    {
        var w = Get(id);
        if (w == null) return;
        w.X = Math.Clamp(x, 0, Math.Max(0, workW - w.W));
        w.Y = Math.Clamp(y, 0, Math.Max(0, workH - w.H));
        if (w.State == GfxWindowState.Floating)
        {
            w.RestoreX = w.X;
            w.RestoreY = w.Y;
        }
    }

    public void ResizeTo(int id, int wNew, int hNew)
    {
        var w = Get(id);
        if (w == null) return;
        w.W = Math.Max(16, wNew);
        w.H = Math.Max(24, hNew);
    }

    public void Snap(int id, GfxSnapZone zone)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State != GfxWindowState.Floating)
        {
            w.RestoreX = w.X; w.RestoreY = w.Y; w.RestoreW = w.W; w.RestoreH = w.H;
        }
        var half = workW / 2;
        if (zone == GfxSnapZone.Left)
        {
            w.X = 0; w.Y = 0; w.W = half; w.H = workH;
            w.State = GfxWindowState.SnappedLeft;
        }
        else
        {
            w.X = half; w.Y = 0; w.W = workW - half; w.H = workH;
            w.State = GfxWindowState.SnappedRight;
        }
    }

    public void MaximizeToggle(int id)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State == GfxWindowState.Maximized)
        {
            w.X = w.RestoreX; w.Y = w.RestoreY; w.W = w.RestoreW; w.H = w.RestoreH;
            w.State = GfxWindowState.Floating;
        }
        else
        {
            w.RestoreX = w.X; w.RestoreY = w.Y; w.RestoreW = w.W; w.RestoreH = w.H;
            w.X = 0; w.Y = 0; w.W = workW; w.H = workH;
            w.State = GfxWindowState.Maximized;
        }
    }

    public void Minimize(int id)
    {
        var w = Get(id);
        if (w == null) return;
        w.State = GfxWindowState.Minimized;
        w.Focused = false;
        if (focus == id)
        {
            focus = windows.Where(x => x.State != GfxWindowState.Minimized).MaxBy(x => x.Z)?.Id;
            active = focus;
        }
    }

    public void Restore(int id)
    {
        var w = Get(id);
        if (w == null) return;
        if (w.State == GfxWindowState.Minimized)
        {
            w.State = GfxWindowState.Floating;
            Raise(id);
        }
    }

    // ── Drag / Resize ─────────────────────────────────────────────

    public bool IsDragging => dragState.HasValue;

    public void StartDrag(int id, int x, int y)
    {
        var w = Get(id);
        if (w is { Draggable: true, State: GfxWindowState.Floating })
            dragState = (id, x - w.X, y - w.Y);
    }

    public void UpdateDrag(int x, int y)
    {
        if (dragState is not { } ds) return;
        var w = Get(ds.id);
        if (w == null) { dragState = null; return; }
        w.X = Math.Clamp(x - ds.offX, 0, Math.Max(0, workW - w.W));
        w.Y = Math.Clamp(y - ds.offY, 0, Math.Max(0, workH - w.H));
    }

    public void EndDrag() => dragState = null;

    public void StartResize(int id, int x, int y, string edge)
    {
        var w = Get(id);
        if (w == null || !w.Resizable || w.State != GfxWindowState.Floating) return;
        resizeState = (id, x, y, w.W, w.H, edge);
    }

    public void UpdateResize(int x, int y)
    {
        if (resizeState is not { } rs) return;
        var w = Get(rs.id);
        if (w == null) { resizeState = null; return; }
        var dx = x - rs.sx;
        var dy = y - rs.sy;
        var nw = rs.sw;
        var nh = rs.sh;
        if (rs.edge.Contains('e')) nw = Math.Max(16, rs.sw + dx);
        if (rs.edge.Contains('s')) nh = Math.Max(24, rs.sh + dy);
        if (rs.edge.Contains('w')) { nw = Math.Max(16, rs.sw - dx); w.X = w.X + (rs.sw - nw); }
        if (rs.edge.Contains('n')) { nh = Math.Max(24, rs.sh - dy); w.Y = w.Y + (rs.sh - nh); }
        w.X = Math.Clamp(w.X, 0, Math.Max(0, workW - w.W));
        w.Y = Math.Clamp(w.Y, 0, Math.Max(0, workH - w.H));
        w.W = nw;
        w.H = nh;
        w.State = GfxWindowState.Floating;
    }

    public void EndResize() => resizeState = null;

    /// <summary>Quale controllo titlebar si trova in (x,y): null / Minimize / Maximize / Close.</summary>
    public string? ControlAt(PixelWindow w, int x, int y)
    {
        if (y < w.Y || y >= w.Y + w.TitleHeight || w.W < 40) return null;
        var closeX = w.X + w.W - 24;
        var maxX = closeX - 24;
        var minX = maxX - 24;
        if (x >= closeX && x < closeX + 20) return "close";
        if (x >= maxX && x < maxX + 20) return "maximize";
        if (x >= minX && x < minX + 20) return "minimize";
        return null;
    }

    /// <summary>Bordo di resize sotto il cursore: null / "e" / "s" / "w" / "n" / "se" / ...</summary>
    public string? ResizeEdgeAt(PixelWindow w, int x, int y)
    {
        if (!w.Resizable || w.State != GfxWindowState.Floating) return null;
        const int edge = 4;
        var onE = Math.Abs(x - (w.X + w.W)) <= edge;
        var onS = Math.Abs(y - (w.Y + w.H)) <= edge;
        var onW = Math.Abs(x - w.X) <= edge;
        var onN = Math.Abs(y - w.Y) <= edge && y < w.Y + w.TitleHeight;
        if (onN && onE) return "ne";
        if (onN && onW) return "nw";
        if (onS && onE) return "se";
        if (onS && onW) return "sw";
        if (onS) return "s";
        if (onE) return "e";
        if (onW) return "w";
        return null;
    }

    /// <summary>Gestisce un evento mouse (coordinate LOGICHE schermo). True se consumato.</summary>
    public bool HandleMouse(int kind, int x, int y)
    {
        switch (kind)
        {
            case 0: // down (left)
            {
                // Taskbar sempre in primo piano: un chip minimizzato intercetta il click
                // prima di qualunque finestra sottostante.
                if (TaskbarChipAt(x, y) is { } chipId) { Restore(chipId); return true; }
                var w = TopmostAt(x, y);
                if (w == null) { active = null; return false; }
                Raise(w.Id);
                var ctrl = ControlAt(w, x, y);
                if (ctrl == "close") { Close(w.Id); return true; }
                if (ctrl == "maximize") { MaximizeToggle(w.Id); return true; }
                if (ctrl == "minimize") { Minimize(w.Id); return true; }
                if (y < w.Y + w.TitleHeight && w.Draggable && w.State == GfxWindowState.Floating)
                {
                    StartDrag(w.Id, x, y);
                    return true;
                }
                var edge = ResizeEdgeAt(w, x, y);
                if (edge != null) { StartResize(w.Id, x, y, edge); return true; }
                return true;
            }
            case 1: // release
                EndDrag();
                EndResize();
                return false;
            case 2: // move / drag
                if (IsDragging) { UpdateDrag(x, y); return true; }
                if (resizeState.HasValue) { UpdateResize(x, y); return true; }
                return false;
            default:
                return false;
        }
    }

    // ── Rendering ─────────────────────────────────────────────────

    private const uint CWindowBg = 0xFF14161C;
    private const uint CTitleTop = 0xFF23262F;
    private const uint CTitleBot = 0xFF16181F;
    private const uint CTitleTopFocus = 0xFF2B3350;
    private const uint CTitleBotFocus = 0xFF1A1E2E;
    private const uint CBorder = 0xFF3A4258;
    private const uint CTitleText = 0xFFD6DCE6;
    private const uint CText = 0xFFD0D6E0;
    private const uint CClose = 0xFFFF5F57;
    private const uint CMinimizeDot = 0xFFFFBD2E;
    private const uint CMaximizeDot = 0xFF28C940;
    private const uint CShadow = 0x8A000000;
    private const uint CAccent = 0xFF6CB6FF;
    private const uint CScrollTrack = 0xFF1E2129;
    private const uint CScrollThumb = 0xFF3A4258;
    private const uint CTaskbarBg = 0xFF0E1016;
    private const uint CTaskbarChip = 0xFF262A35;
    private const uint CTaskbarChipFocus = 0xFF33394A;
    private const uint CTaskbarText = 0xFFD0D6E0;

    private const int TaskbarHeight = 28;
    private const int TaskbarChipWidth = 130;
    private const int TaskbarChipGap = 6;

    /// <summary>Composita tutte le finestre (z-order) sul buffer della pagina attiva.
    /// DEVE essere chiamata sul main thread SDL (via coda GPM). Ripristina PRIMA lo sfondo
    /// pulito (catturato una volta alla prima chiamata): senza questo, spostare/chiudere
    /// una finestra lasciava i vecchi pixel impressi sullo schermo (nessun redraw completo,
    /// ogni frame si accumulava sul precedente).</summary>
    public void Render()
    {
        var g = GraphicsManager.Instance;
        g.GfxCaptureBackgroundIfNeeded();
        g.GfxRestoreBackground();
        foreach (var w in ZOrdered)
            RenderWindow(w);
        RenderTaskbar(g);
    }

    /// <summary>Testo impostato da STATUSBAR "text" quando nessuna finestra è attiva —
    /// condiviso con la riga della taskbar (stesso schema di ClipperLib.Controls.
    /// WindowManager.SetStatusBar: un solo canale invece di due che non si conoscono).</summary>
    private string _statusText = "";
    public void SetStatusBar(string text) => _statusText = text ?? "";
    public bool HasStatusText => !string.IsNullOrEmpty(_statusText);

    /// <summary>Barra in basso: testo STATUSBAR a sinistra + un "chip" per ogni finestra
    /// minimizzata a destra (unico modo per farla ricomparire con un click, oltre al comando
    /// WINDOW ... RESTORE). Disegnata PER ULTIMA: resta sempre sopra le finestre, come una
    /// vera taskbar OS.</summary>
    private void RenderTaskbar(GraphicsManager g)
    {
        var minimized = windows.Where(w => w.State == GfxWindowState.Minimized).OrderBy(w => w.Z).ToList();
        // Indicatore desktop in alto a destra della barra — parità
        // WindowManager.RenderStatusBar del TUI ([D1*] [D2]). Completo a destra,
        // i chip finiscono a sinistra (layout deterministico: niente sovrapposizione).
        var maxDesktop = Math.Max(CurrentDesktop,
            windows.Count > 0 ? windows.Max(w => w.Desktop) : 1);
        string dtStr = "";
        if (maxDesktop > 1)
        {
            for (var d = 1; d <= maxDesktop; d++)
                dtStr += d == CurrentDesktop ? $"[D{d}*] " : $"[D{d}] ";
        }
        var status = _statusText;
        if (!string.IsNullOrEmpty(dtStr)) status = dtStr + status;
        if (minimized.Count == 0 && string.IsNullOrEmpty(status)) return;
        var barY = workH - TaskbarHeight;
        g.GfxRoundedRect(0, barY, workW, TaskbarHeight, 0, CTaskbarBg, filled: true);
        var x = TaskbarChipGap;
        foreach (var w in minimized)
        {
            var isFocus = w.Focused;
            g.GfxRoundedRect(x, barY + 3, TaskbarChipWidth, TaskbarHeight - 6, 4,
                isFocus ? CTaskbarChipFocus : CTaskbarChip, filled: true);
            var title = string.IsNullOrEmpty(w.Title) ? w.Name : w.Title;
            if (title.Length > 18) title = title[..18] + "…";
            g.DrawWindowText(title, x + 8, barY + 7, CTaskbarText);
            x += TaskbarChipWidth + TaskbarChipGap;
        }
        if (!string.IsNullOrEmpty(status))
            g.DrawWindowText(status, x + 6, barY + 7, CTaskbarText);
    }

    /// <summary>Id della finestra il cui chip in taskbar si trova in (x,y), o null. Stesso
    /// layout deterministico di RenderTaskbar (nessuno stato extra da tenere sincronizzato).</summary>
    private int? TaskbarChipAt(int x, int y)
    {
        var barY = workH - TaskbarHeight;
        if (y < barY || y >= workH) return null;
        var minimized = windows.Where(w => w.State == GfxWindowState.Minimized).OrderBy(w => w.Z).ToList();
        var cx = TaskbarChipGap;
        foreach (var w in minimized)
        {
            if (x >= cx && x < cx + TaskbarChipWidth) return w.Id;
            cx += TaskbarChipWidth + TaskbarChipGap;
        }
        return null;
    }

    private void RenderWindow(PixelWindow w)
    {
        if (w.State == GfxWindowState.Minimized) return;
        var g = GraphicsManager.Instance;
        var isFocus = w.Focused;

        // Ombra
        if (w.Shadow)
            g.GfxDropShadow(w.X, w.Y, w.W, w.H, 3, CShadow, 10);

        // Corpo finestra (colore configurabile via COLOR fg,bg con finestra attiva)
        g.GfxRoundedRect(w.X, w.Y, w.W, w.H, w.BorderRadius, w.BodyColor, filled: true);

        // Titlebar: gradiente
        g.GfxGradientRect(w.X, w.Y, w.W, w.TitleHeight, isFocus ? CTitleTopFocus : CTitleTop,
            isFocus ? CTitleBotFocus : CTitleBot, vertical: true);

        // Bordo (copre anche il perimetro della titlebar, angoli arrotondati inclusi).
        g.GfxRoundedRect(w.X, w.Y, w.W, w.H, w.BorderRadius, isFocus ? CBorder : (CBorder & 0x00FFFFFFu) | 0x88000000u, filled: false);

        // Focus ring: contorno esterno evidenziato (doppio bordo) per la finestra focalizzata
        // — parità col bordo doppio delle finestre TUI attive (WindowManager: DOUBLE border).
        if (isFocus)
            g.GfxRoundedRect(w.X - 2, w.Y - 2, w.W + 4, w.H + 4, w.BorderRadius + 2,
                (CBorder & 0x00FFFFFFu) | 0x40000000u, filled: false);

        // Titolo (centrato nella titlebar) — colore configurabile via COLOR fg,bg
        var titleY = w.Y + w.TitleHeight / 2;
        if (!string.IsNullOrEmpty(w.Title))
        {
            var tw = GfxTextWidth(w.Title);
            var cx = w.X + (w.W - tw) / 2;
            g.DrawWindowText(w.Title, Math.Max(w.X + 4, cx), titleY - 8, w.TitleColor);
        }

        // Pulsanti titlebar (forme vettoriali)
        RenderTitleControls(g, w, isFocus);

        // Contenuto (buffer testo)
        RenderContent(g, w);

        // Cursore editing
        if (w.Editable && w.Focused && w.Lines.Count > 0)
        {
            var cr = w.ClientRect;
            var lineH = GfxTextHeight();
            var visY = w.CursorLine - w.ScrollOffset;
            if (visY >= 0 && visY < cr.h / Math.Max(1, lineH))
            {
                var cy = cr.y + visY * lineH;
                var cx = cr.x + Math.Min(w.CursorCol, 40) * GfxCharWidth();
                g.DrawWindowText("_", cx, cy + lineH - 4, CAccent);
            }
        }
    }

    /// <summary>Pulsanti titlebar in stile "traffic light" macOS: 3 cerchi pieni
    /// anti-aliasing (chiuso/rosso, minimizza/giallo, massimizza/verde) con il simbolo
    /// (×/−/+) SEMPRE visibile sopra (non solo in hover come nel vero macOS — scelta
    /// esplicita per leggibilità, dato che qui non c'è tracciamento del mouse per finestra).</summary>
    private void RenderTitleControls(GraphicsManager g, PixelWindow w, bool isFocus)
    {
        var y = w.Y + w.TitleHeight / 2;
        var closeX = w.X + w.W - 24;
        var maxX = closeX - 24;
        var minX = maxX - 24;
        const int d = 12;
        var alphaFactor = isFocus ? 1.0 : 0.35;
        var iconColor = ApplyAlphaFactor(0xFF000000u, alphaFactor * 0.5);

        g.GfxFilledCircle(minX + 7, y, d, ApplyAlphaFactor(CMinimizeDot, alphaFactor));
        g.GfxRoundedRect(minX + 4, y - 1, 6, 2, 1, iconColor, filled: true);

        g.GfxFilledCircle(maxX + 7, y, d, ApplyAlphaFactor(CMaximizeDot, alphaFactor));
        g.GfxRoundedRect(maxX + 4, y - 1, 6, 2, 1, iconColor, filled: true);
        g.GfxRoundedRect(maxX + 6, y - 3, 2, 6, 1, iconColor, filled: true);

        g.GfxFilledCircle(closeX + 7, y, d, ApplyAlphaFactor(CClose, alphaFactor));
        DrawCross(g, closeX + 7, y, 3, iconColor);
    }

    /// <summary>Disegna una "×" vettoriale (due segmenti diagonali) come sequenza di piccoli
    /// cerchi pieni anti-aliasing lungo le due diagonali — evita di dipendere dal font per
    /// un glifo leggibile a 12px, e non serve un primitivo di linea ruotata.</summary>
    private static void DrawCross(GraphicsManager g, int cx, int cy, int half, uint color)
    {
        const int steps = 5;
        for (var i = -steps; i <= steps; i++)
        {
            var t = i / (double)steps;
            var dx = (int)Math.Round(t * half);
            g.GfxFilledCircle(cx + dx, cy + dx, 2, color);
            g.GfxFilledCircle(cx + dx, cy - dx, 2, color);
        }
    }

    /// <summary>Scala il canale alpha di un colore ARGB per un fattore 0..1 (es. attenuare i
    /// pulsanti titlebar quando la finestra non ha il focus).</summary>
    private static uint ApplyAlphaFactor(uint rgba, double factor)
    {
        var a = (byte)Math.Clamp((int)Math.Round(((rgba >> 24) & 0xFF) * factor), 0, 255);
        return ((uint)a << 24) | (rgba & 0x00FFFFFFu);
    }

    private void RenderContent(GraphicsManager g, PixelWindow w)
    {
        var cr = w.ClientRect;
        if (cr.w <= 0 || cr.h <= 0) return;
        var lineH = GfxTextHeight();
        var charW = GfxCharWidth();
        if (lineH <= 0) lineH = 16;
        if (charW <= 0) charW = 8;
        var maxRows = cr.h / lineH;
        var maxCols = cr.w / charW;
        if (maxRows <= 0 || maxCols <= 0) return;

        for (var row = 0; row < maxRows; row++)
        {
            var idx = w.ScrollOffset + row;
            if (idx >= w.Lines.Count) break;
            var line = w.Lines[idx] ?? "";
            if (line.Length > maxCols) line = line[..maxCols];
            if (line.Length == 0) continue;
            g.DrawWindowText(line, cr.x, cr.y + row * lineH, w.FgColor);
        }

        // Scrollbar verticale se il buffer è più alto della viewport
        if (w.Lines.Count > maxRows)
        {
            var barX = cr.x + cr.w - 4;
            var barH = cr.h;
            g.GfxRoundedRect(barX, cr.y, 3, barH, 2, CScrollTrack, filled: true);
            var page = maxRows;
            var maxOff = Math.Max(0, w.Lines.Count - page);
            var thumbH = Math.Max(6, (int)Math.Round(barH * (double)page / w.Lines.Count));
            var start = maxOff > 0 ? (int)Math.Round(w.ScrollOffset * (double)(barH - thumbH) / maxOff) : 0;
            g.GfxRoundedRect(barX, cr.y + start, 3, thumbH, 2, CScrollThumb, filled: true);
        }
    }

    /// <summary>Larghezza testo logica (font corrente).</summary>
    private static int GfxTextWidth(string text)
    {
        try { return (int)GraphicsManager.Instance.TextWidthInternal(text); }
        catch { return text.Length * 8; }
    }

    private static int GfxTextHeight()
    {
        try { return (int)GraphicsManager.Instance.TextHeightInternal(); }
        catch { return 16; }
    }

    private static int GfxCharWidth()
    {
        try { return (int)GraphicsManager.Instance.TextWidthInternal(" "); }
        catch { return 8; }
    }

    /// <summary>Larghezza carattere corrente (per layout colonne nel client area).</summary>
    public static int CharWidth() => GfxCharWidth();

    /// <summary>Altezza riga corrente (per layout righe nel client area).</summary>
    public static int CharHeight() => GfxTextHeight();
}
