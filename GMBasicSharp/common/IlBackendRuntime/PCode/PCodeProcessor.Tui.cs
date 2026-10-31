// FILE: Basic.IlBackend.PCode.PCodeProcessor.Tui.cs
// P-CODE VM: Clipper TUI controls

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

using CommonLib.Consoles.Tui;
using ClipperLib.Clipper;
using ClipperLib.Controls;
using CommonLib.Consoles.Tui;
using CommonLib.Screens;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private void ExecuteTuiSyscall(PCodeTask task, SysCall call, int argCount)
    {
        switch (call)
        {
            case SysCall.TUI_STATUSBAR:   TuiStatusBar(task); break;
            case SysCall.TUI_SPINNER:     TuiSpinner(task); break;
            case SysCall.TUI_CHECKBOX:    TuiCheckBox(task); break;
            case SysCall.TUI_LAST_HANDLE: task.PushRef(IlRuntime.UiLastHandle()); break;
            case SysCall.CONTROL_APPLY_PROPS: ControlApplyProps(task); break;
            case SysCall.TUI_LISTBOX:     TuiListBox(task); break;
            case SysCall.TUI_COMBOBOX:    TuiComboBox(task); break;
            case SysCall.TUI_TOGGLE:      TuiToggle(task); break;
            case SysCall.TUI_RADIOGROUP:  TuiRadioGroup(task); break;
            case SysCall.TUI_SLIDER:      TuiSlider(task); break;
            case SysCall.TUI_NUMERICUPDOWN: TuiNumericUpDown(task); break;
            case SysCall.TUI_STARRATING:  TuiStarRating(task); break;
            case SysCall.TUI_SCROLLBAR:   TuiScrollBar(task); break;
            case SysCall.TUI_SCROLLVIEW:  TuiScrollView(task); break;
            case SysCall.TUI_LABEL:       TuiLabel(task); break;
            case SysCall.TUI_DATALIST:    TuiDataList(task); break;
            case SysCall.TUI_PROPERTYGRID: TuiPropertyGrid(task); break;
            case SysCall.TUI_SPLITTER:    TuiSplitter(task); break;
            case SysCall.TUI_CONTEXTMENU: TuiContextMenu(task); break;
            case SysCall.TUI_LOGVIEW:     TuiLogView(task); break;
            case SysCall.TUI_HEXVIEW:     TuiHexView(task); break;
            case SysCall.TUI_BIGTEXT:     TuiBigText(task); break;
            case SysCall.TUI_CODEAREA:    TuiCodeArea(task); break;
            case SysCall.TUI_REORDERLISTBOX: TuiReorderListBox(task); break;
            case SysCall.TUI_ALERTBOX:    TuiAlertBox(task); break;
            case SysCall.TUI_CONFIRMATIONBOX: TuiConfirmationBox(task); break;
            case SysCall.TUI_FILEEXPLORER: TuiFileExplorer(task); break;
            case SysCall.TUI_DASHBOARD:   TuiDashboard(task); break;
            case SysCall.TUI_TAB:         TuiTab(task); break;
            case SysCall.TUI_GRID:        TuiGrid(task); break;
            case SysCall.TUI_DATEPICKER:  TuiDatePicker(task); break;
            case SysCall.TUI_COLORPICKER: TuiColorPicker(task); break;
            case SysCall.TUI_TREEVIEW:    TuiTreeView(task); break;
            case SysCall.TUI_MENU:        TuiMenu(task); break;
            case SysCall.TUI_FILEDIALOG:  TuiFileDialog(task); break;
            case SysCall.TUI_PRINTER:     TuiPrinterDialog(task); break;
            case SysCall.TUI_TBROWSE:     TuiTBrowse(task); break;
        }
    }

    // ═══ Helpers ═══
    private void EnsureRenderHook()
    {
        ClipperGlobal.State.Buffer = new ScreenBuffer(80, 25);
        // Hook polimorfo: screen grafico → pagina gfx (per-pixel); altrimenti console (celle).
        ClipperGlobal.State.RenderHook = RenderControlBufferPCode;
    }

    /// <summary>Render di un buffer di controllo verso il target attivo: gfx se lo screen
    /// grafico è attivo, altrimenti console. Regola valida per tutti i backend.</summary>
    private static void RenderControlBufferPCode(ScreenBuffer buf)
    {
        try
        {
            var g = IlRuntime.Current.Graphics;
            if (g is not null && PCodeGraphics.Instance.IsScreenOn)
            {
                int w = buf.Width, h = buf.Height;
                if (w > 0 && h > 0)
                {
                    var cells = new TuiCell[w * h];
                    for (int r = 0; r < h; r++)
                        for (int c = 0; c < w; c++)
                        {
                            var cell = buf.Cells[r, c];
                            cells[r * w + c] = new TuiCell(
                                cell.Character == '\0' ? ' ' : cell.Character,
                                PCodeToArgb(cell.ForegroundRgb), PCodeToArgb(cell.BackgroundRgb));
                        }
                    g.RenderTui(cells, w, h);
                    return;
                }
            }
        }
        catch { }
        ConsoleRenderer.Write(buf);
    }

    private static uint PCodeToArgb(CellColor? c)
    {
        if (c is null || c.Value.IsDefault) return 0xFF000000;
        return 0xFF000000u | ((uint)c.Value.R << 16) | ((uint)c.Value.G << 8) | c.Value.B;
    }

    /// <summary>Rende le finestre TUI (WindowManager) sul buffer console (ANSI). Chiamato
    /// dopo ogni modifica al contenuto (APPEND/SET CONTENT/CLEAR/SCROLL/EDIT).</summary>
    private void RenderTuiWindowsForPCode()
    {
        try
        {
            // Non saltare SOLO su WindowCount==0: STATUSBAR senza finestre TUI aperte deve
            // comunque comparire (stesso bug già corretto nel backend IL, RenderTuiWindows).
            if (_tuiWindows is null || (_tuiWindows.WindowCount == 0 && !_tuiWindows.HasStatusText)) return;
            EnsureRenderHook();
            var st = ClipperGlobal.State;
            st.Buffer.Clear();
            _tuiWindows.SyncWorkArea(st.Buffer.Width, st.Buffer.Height);
            _tuiWindows.RenderWm(st.Buffer, ClipperLib.Controls.Theme.Default(), "", "");
            st.RenderHook?.Invoke(st.Buffer);
        }
        catch { }
    }

    /// <summary>GfxWindowManager lazy per le finestre PIXEL (Fase W-Pixel). Work area =
    /// risoluzione logica dello schermo grafico corrente.</summary>
    private CommonGraphics.Graphics.GfxWindowManager EnsureGfxWindows()
    {
        if (_gfxWindows == null)
        {
            _gfxWindows = new CommonGraphics.Graphics.GfxWindowManager();
            try
            {
                var gm = PCodeGraphics.Instance;
                _gfxWindows.ScreenW = gm.ScreenWidth;
                _gfxWindows.ScreenH = gm.ScreenHeight;
            }
            catch { }
        }
        return _gfxWindows;
    }

    /// <summary>Come <see cref="EnsureGfxWindows"/> ma senza creare (solo se già esistente):
    /// per syscall che devono agire sul compositor pixel PRESENTE ma non devono
    /// materializzarlo in un contesto in cui lo schermo grafico non è attivo.</summary>
    private CommonGraphics.Graphics.GfxWindowManager? EnsureGfxWindowsNoCreate() => _gfxWindows;

    /// <summary>Rende le finestre pixel sul buffer della pagina grafica. Il render DEVE
    /// girare sul main thread SDL: accoda via GPM (come il TUI con RenderTui).</summary>
    private void RenderPixelWindowsForPCode()
    {
        try
        {
            // Non saltare SOLO su Windows.Count==0: serve renderizzare sia quando l'ultima
            // finestra è appena stata chiusa (ripristina lo sfondo) sia con sola STATUSBAR e
            // zero finestre (stesso bug già corretto nel backend IL, RenderPixelWindows).
            if (_gfxWindows is null) return;
            var gm = PCodeGraphics.Instance;
            gm.EnqueueGpmOperation(() => { try { _gfxWindows!.Render(); } catch { } return null; }, (_r, _e) => { });
        }
        catch { }
    }

    /// <summary>Finestra TUI attiva (ManagedWindow) o null se lo schermo base — porting
    /// dell'equivalente in IlRuntime.ActiveWindowObj(), stesso schema ma su campo istanza
    /// invece che statico (il backend PCode ha un WindowManager per processore, non uno
    /// globale). Allinea PRINT/LOCATE/CLS/COLOR/STATUSBAR al backend IL: prima scrivevano
    /// sempre sulla console base, ignorando una finestra TUI selezionata con WINDOW SELECT.</summary>
    private ClipperLib.Controls.ManagedWindow? ActiveWindowObjPCode()
    {
        try
        {
            var id = _tuiWindows?.ActiveWindowId;
            return id is int nid ? _tuiWindows?.Get(nid) : null;
        }
        catch { return null; }
    }

    /// <summary>Finestra PIXEL attiva (viewport corrente) o null — porting di
    /// IlRuntime.ActivePixelWindow(), vedi nota su ActiveWindowObjPCode().</summary>
    private CommonGraphics.Graphics.PixelWindow? ActivePixelWindowPCode()
    {
        try
        {
            var id = _gfxWindows?.ActiveId;
            return id is int nid ? _gfxWindows?.Get(nid) : null;
        }
        catch { return null; }
    }
    private static int PopI(PCodeTask task) => (int)task.PopNum();
    private static string PopS(PCodeTask task) => task.PopRef()?.ToString() ?? "";
    private static List<string> PopSL(PCodeTask task)
    {
        var items = new List<string>();
        if (task.PopRef() is System.Collections.IList list)
            foreach (var item in list) items.Add(item?.ToString() ?? "");
        return items;
    }

    // ═══ Display-only ═══
    /// <summary>STATUSBAR polimorfica (allineata al backend IL, IlRuntime.StatusBar): con
    /// schermo grafico attivo il testo va al canale condiviso StatusBar/Taskbar di
    /// GfxWindowManager (fuso in Fase 1 con i chip delle finestre minimizzate); altrimenti
    /// a quello di WindowManager (TUI/SDLConsole). Prima scriveva SEMPRE e solo sul buffer
    /// TUI base via StatusBar.Draw — mai instradata verso GfxWindowManager, quindi
    /// invisibile in modalità grafica pura (nessun output visibile, stesso bug già corretto
    /// nel backend IL).</summary>
    private void TuiStatusBar(PCodeTask task)
    {
        var text = PopS(task);
        try
        {
            if (PCodeGraphics.Instance.IsScreenOn)
            {
                EnsureGfxWindows().SetStatusBar(text);
                RenderPixelWindowsForPCode();
                return;
            }
        }
        catch { }
        try
        {
            EnsureTuiWindows();
            _tuiWindows!.SetStatusBar(text);
            RenderTuiWindowsForPCode();
            return;
        }
        catch { }
        EnsureRenderHook();
        var st = ClipperGlobal.State;
        new StatusBar { X = 0, Y = st.Buffer.Height - 1, Width = st.Buffer.Width, Text = text }.Draw(st.Buffer);
        st.RenderHook!(st.Buffer);
    }
    private void TuiSpinner(PCodeTask task)
    {
        var preset = PopS(task); var col = PopI(task); var row = PopI(task);
        EnsureRenderHook();
        // Registry TUI runtime (come gli altri controlli): creabile/leggibile via CONTROL GET/SET.
        Basic.IlBackend.IlRuntime.UiAddSpinner(row, col, 4, preset);
        task.PushRef(IlRuntime.UiLastHandle());
    }

    // ═══ Simple form controls ═══
    private void ControlApplyProps(PCodeTask task)
    {
        static string? Nz(string s) => string.IsNullOrEmpty(s) ? null : s;
        var colorerr = PopS(task); var colorsel = PopS(task); var colorfocus = PopS(task); var color = PopS(task);
        var style = PopS(task); var focus = task.PopRef();
        var tabstop = PopI(task); var enabled = PopI(task); var visible = PopI(task); var tabindex = PopI(task);
        var tooltip = PopS(task); var tag = PopS(task); var name = PopS(task);
        IlRuntime.ControlApplyProps(new object?[] { name, tag, tooltip, tabindex, visible, enabled, tabstop, focus, Nz(style), Nz(color), Nz(colorfocus), Nz(colorsel), Nz(colorerr) });
    }

    private void TuiCheckBox(PCodeTask task) { var c = PopS(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); /* runtime C non-bloccante (niente modal): registra nel registry */ Basic.IlBackend.IlRuntime.UiAddCheckbox(ro, co, Math.Max(12, (c?.Length ?? 0) + 6), c ?? ""); task.PushRef(0.0); }
    private void TuiListBox(PCodeTask task) { var items = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddListbox(ro, co, w, Math.Max(1, h), string.Join("|", items)); task.PushRef(""); }
    private void TuiComboBox(PCodeTask task) { var items = PopSL(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddCombobox(ro, co, w, string.Join("|", items)); task.PushRef(items.Count > 0 ? items[0] : ""); }
    private void TuiToggle(PCodeTask task) { var cap = PopS(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddToggle(ro, co, Math.Max((cap?.Length ?? 0) + 6, 12), cap ?? ""); task.PushRef(0.0); }
    private void TuiRadioGroup(PCodeTask task) { var items = PopSL(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); int w = items.Count > 0 ? items.Max(x => x.Length) + 5 : 20; Basic.IlBackend.IlRuntime.UiAddRadiogroup(ro, co, w, items.Count, string.Join("|", items)); task.PushRef(items.Count > 0 ? items[0] : ""); }
    private void TuiSlider(PCodeTask task) { var step = PopI(task); var max = PopI(task); var min = PopI(task); var len = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddSlider(ro, co, len, min, max, min); task.PushRef((double)min); }
    private void TuiNumericUpDown(PCodeTask task) { var step = PopI(task); var max = PopI(task); var min = PopI(task); var len = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddNumericUpDown(ro, co, len, min, max, min); task.PushRef((double)min); }
    private void TuiStarRating(PCodeTask task) { var value = PopI(task); var max = PopI(task); var len = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddStarRating(ro, co, len, max, value); task.PushRef((double)value); }
    private void TuiScrollBar(PCodeTask task) { var value = PopI(task); var max = PopI(task); var min = PopI(task); var len = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddScrollBar(ro, co, len, min, max, value); task.PushRef((double)value); }
    private void TuiScrollView(PCodeTask task) { var content = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddScrollView(ro, co, w, h, string.Join("|", content)); task.PushRef(0.0); }
    private void TuiLabel(PCodeTask task) { var text = PopS(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddLabel(ro, co, w, text); task.PushRef(0.0); }
    private void TuiDataList(PCodeTask task) { var items = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddDataList(ro, co, w, h, string.Join("|", items)); task.PushRef(0.0); }
    private void TuiPropertyGrid(PCodeTask task) { var items = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddPropertyGrid(ro, co, w, h, string.Join("|", items)); task.PushRef(0.0); }
    private void TuiSplitter(PCodeTask task) { var orient = PopI(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddSplitter(ro, co, w, h, orient); task.PushRef(IlRuntime.UiLastHandle()); }
    private void TuiContextMenu(PCodeTask task) { var items = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddContextMenu(ro, co, w, h, string.Join("|", items)); task.PushRef(IlRuntime.UiLastHandle()); }
    private void TuiLogView(PCodeTask task) { var records = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddLogView(ro, co, w, h, string.Join("|", records)); task.PushRef(0.0); }
    private void TuiHexView(PCodeTask task) { var data = PopS(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddHexView(ro, co, w, h, data); task.PushRef(0.0); }
    private void TuiBigText(PCodeTask task) { var text = PopS(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddBigText(ro, co, text); task.PushRef(0.0); }
    private void TuiCodeArea(PCodeTask task) { var lines = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddCodeArea(ro, co, w, h, string.Join("|", lines)); task.PushRef(0.0); }
    private void TuiReorderListBox(PCodeTask task) { var items = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddReorderListBox(ro, co, w, h, string.Join("|", items)); task.PushRef(0.0); }
    private void TuiAlertBox(PCodeTask task) { var text = PopS(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddAlertBox(ro, co, w, h, text); task.PushRef(0.0); }
    private void TuiConfirmationBox(PCodeTask task) { var text = PopS(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddConfirmationBox(ro, co, w, h, text); task.PushRef(0.0); }
    private void TuiFileExplorer(PCodeTask task) { var path = PopS(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddFileExplorer(ro, co, w, h, path); task.PushRef(0.0); }
    private void TuiDashboard(PCodeTask task) { var metrics = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddDashboard(ro, co, w, h, string.Join("|", metrics)); task.PushRef(0.0); }
    private void TuiTab(PCodeTask task) { var tabs = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddTab(ro, co, w, string.Join("|", tabs)); task.PushRef(0.0); }
    private void TuiGrid(PCodeTask task) { var data = PopSL(task); var headers = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); var cells = new List<string>(); cells.AddRange(headers); cells.AddRange(data); var cols = headers.Count > 0 ? headers.Count : 1; Basic.IlBackend.IlRuntime.UiAddGrid(ro, co, w, h, cols, string.Join("|", cells)); task.PushRef(-1.0); }

    private void TuiDatePicker(PCodeTask task) { var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddDatepicker(ro, co, 22); task.PushRef(""); }
    private void TuiColorPicker(PCodeTask task) { var cols = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddColorpicker(ro, co, cols, 8); task.PushRef(-1.0); }
    private void TuiTreeView(PCodeTask task) { var raw = PopSL(task); var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddTreeview(ro, co, w, h, string.Join("|", raw)); task.PushRef(""); }

    // ═══ Complex ═══
    private void TuiMenu(PCodeTask task) { var itemsRaw = task.PopRef(); var co = PopI(task); var ro = PopI(task); EnsureRenderHook(); var mi = ResolveMenuItems(itemsRaw); Basic.IlBackend.IlRuntime.UiAddMenu(ro, co, 20, string.Join("|", mi)); task.PushRef(-1.0); }

    private void TuiFileDialog(PCodeTask task) { var title = PopS(task); var mask = PopS(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddFiledialog(title ?? "", mask ?? ""); task.PushRef(""); }

    private void TuiPrinterDialog(PCodeTask task) { var title = PopS(task); EnsureRenderHook(); Basic.IlBackend.IlRuntime.UiAddPrinterdialog(title ?? ""); task.PushRef(""); task.PushRef(0.0); task.PushRef(false); }

    private void TuiTBrowse(PCodeTask task)
    {
        var optsRaw = task.PopRef(); var dataRaw = task.PopRef(); var colsRaw = task.PopRef();
        var h = PopI(task); var w = PopI(task); var co = PopI(task); var ro = PopI(task);
        EnsureRenderHook();
        var columns = new List<string>();
        if (colsRaw is System.Collections.IList cl)
            foreach (var c in cl)
                if (c is System.Collections.IList cd && cd.Count >= 2)
                {
                    var header = cd[0]?.ToString() ?? "";
                    var width = Convert.ToInt32(cd[1] is double dw ? dw : 10);
                    var typeChar = (cd.Count >= 3 ? cd[2]?.ToString()?.ToUpperInvariant() : null) switch { "N" or "I" => "N", "F" => "F", "L" or "B" => "L", "D" => "D", "X" => "X", _ => "T" };
                    var alignChar = (cd.Count >= 4 ? cd[3]?.ToString()?.ToUpperInvariant() : null) switch { "R" => "R", "C" => "C", _ => "L" };
                    columns.Add(header + ":" + width + ":" + typeChar + ":" + alignChar);
                }
        if (columns.Count == 0) columns.Add("Field:10:T:L");
        var rows = new List<List<string>>();
        if (dataRaw is System.Collections.IList dl)
            foreach (var r in dl)
            { var rd = new List<string>(); if (r is System.Collections.IList rl) foreach (var cell in rl) rd.Add(cell?.ToString() ?? ""); rows.Add(rd); }
        var colsCsv = string.Join("|", columns);
        var dataCsv = string.Join(";", rows.ConvertAll(r => string.Join("|", r)));
        Basic.IlBackend.IlRuntime.UiAddTbrowse(ro, co, w, h, colsCsv, dataCsv);
        _ = optsRaw;
        task.PushRef(-1.0);
    }

    private static List<string> ResolveMenuItems(object? val)
    {
        var items = new List<string>();
        if (val is System.Collections.IList list)
            foreach (var item in list)
                if (item is System.Collections.IList sl && sl.Count > 0)
                    items.Add(sl[0]?.ToString() ?? "");
                else items.Add(item?.ToString() ?? "");
        return items;
    }

    /// <summary>
    /// Risolve un valore (int ANSI, stringa hex '#RRGGBB', array [r,g,b]) in CellColor.
    /// </summary>
    internal static CommonLib.Screens.CellColor? ResolveColor(object? val)
    {
        if (val is null) return null;
        if (val is CommonLib.Screens.CellColor cc) return cc;
        if (val is double d)
        {
            int ansi = (int)d;
            return ansi switch
            {
                0 => CommonLib.Screens.CellColor.Black, 1 => CommonLib.Screens.CellColor.DarkBlue,
                2 => CommonLib.Screens.CellColor.DarkGreen, 3 => CommonLib.Screens.CellColor.DarkCyan,
                4 => CommonLib.Screens.CellColor.DarkRed, 5 => CommonLib.Screens.CellColor.DarkMagenta,
                6 => CommonLib.Screens.CellColor.DarkYellow, 7 => CommonLib.Screens.CellColor.Grey,
                8 => CommonLib.Screens.CellColor.Grey, 9 => CommonLib.Screens.CellColor.Blue,
                10 => CommonLib.Screens.CellColor.Green, 11 => CommonLib.Screens.CellColor.Cyan,
                12 => CommonLib.Screens.CellColor.Red, 13 => CommonLib.Screens.CellColor.Magenta,
                14 => CommonLib.Screens.CellColor.Yellow, 15 => CommonLib.Screens.CellColor.White,
                _ => CommonLib.Screens.CellColor.White
            };
        }
        var s = val.ToString()?.TrimStart('#') ?? "";
        if (s.Length == 6 && int.TryParse(s.Substring(0, 2), System.Globalization.NumberStyles.HexNumber, null, out int r) &&
            int.TryParse(s.Substring(2, 2), System.Globalization.NumberStyles.HexNumber, null, out int g) &&
            int.TryParse(s.Substring(4, 2), System.Globalization.NumberStyles.HexNumber, null, out int b))
            return new CommonLib.Screens.CellColor { R = (byte)r, G = (byte)g, B = (byte)b };
        if (val is System.Collections.IDictionary dct)
        {
            int Cv(object? o) => o is null ? 0 : Convert.ToInt32(o is double dd ? dd : 0);
            int rr = Cv(dct.Contains("Red") ? dct["Red"] : null);
            int gg = Cv(dct.Contains("Green") ? dct["Green"] : null);
            int bb = Cv(dct.Contains("Blue") ? dct["Blue"] : null);
            return new CommonLib.Screens.CellColor { R = (byte)Math.Clamp(rr, 0, 255), G = (byte)Math.Clamp(gg, 0, 255), B = (byte)Math.Clamp(bb, 0, 255) };
        }
        if (val is System.Collections.IList list && list.Count >= 3)
        {
            int rr = System.Convert.ToInt32(list[0] is double dr ? dr : 0);
            int gg = System.Convert.ToInt32(list[1] is double dg ? dg : 0);
            int bb = System.Convert.ToInt32(list[2] is double db ? db : 0);
            return new CommonLib.Screens.CellColor { R = (byte)Math.Clamp(rr, 0, 255), G = (byte)Math.Clamp(gg, 0, 255), B = (byte)Math.Clamp(bb, 0, 255) };
        }
        return null;
    }



    /// <summary>SET BELL/CONFIRM/CURSOR/CENTURY/THEME/DATEFORMAT — dispatcher P-CODE.</summary>
    private void ExecuteSetClipper(PCodeTask task)
    {
        var subCode = PopInt(task);
        var st = ClipperLib.Clipper.ClipperGlobal.State;
        switch (subCode)
        {
            case 0: st.SetBell = PopInt(task) != 0; break;          // BELL
            case 1: st.SetConfirm = PopInt(task) != 0; break;       // CONFIRM
            case 2: st.CursorVisible = PopInt(task) != 0; break;    // CURSOR
            case 3: st.SetCentury = PopInt(task) != 0; break;       // CENTURY
            case 4: // THEME
            {
                var name = task.PopRef()?.ToString() ?? "turbo";
                var theme = Theme.Named(name);
                Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetThemeName(name);
                Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetFgBg(theme.Text, theme.DesktopBg);
                ControlStyle.SetTheme(theme);
                RenderTuiWindowsForPCode();
                break;
            }
            case 5: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetDateFormat(task.PopRef()?.ToString() ?? "mm-dd-yyyy"); break; // DATEFORMAT
            // SET legacy Clipper (2026-08-06): WRAP/ESCAPE/DELIMITERS/TYPEAHEAD/INTENSITY/EPOCH/
            // DECIMALS/FIXED/PRINTER/DEVICE/MESSAGE (subCode 6-16) → stato C condiviso.
            case 6: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetWrap(PopInt(task) != 0); break;
            case 7: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetEscape(PopInt(task) != 0); break;
            case 8: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetDelimiters(task.PopRef()?.ToString() ?? ""); break;
            case 9: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetTypeahead(PopInt(task)); break;
            case 10: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetIntensity(PopInt(task) != 0); break;
            case 11: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetEpoch(PopInt(task)); break;
            case 12: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetDecimals(PopInt(task)); break;
            case 13: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetFixed(PopInt(task) != 0); break;
            case 14: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetPrinter(PopInt(task) != 0); break;
            case 15: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetDevice(task.PopRef()?.ToString() ?? ""); break;
            case 16: Basic.IlBackend.IlRuntime.Current.Graphics.ClipperSetMessage(PopInt(task)); break;
            // SET BOLD / UNDERLINE / INVERSE / BLINK (attributi ANSI della console).
            case 17: case 18: case 19: case 20:
            {
                var on = PopInt(task) != 0;
                var code = subCode switch { 17 => on ? "\x1b[1m" : "\x1b[22m", 18 => on ? "\x1b[4m" : "\x1b[24m", 19 => on ? "\x1b[7m" : "\x1b[27m", _ => on ? "\x1b[5m" : "\x1b[25m" };
                lock (_consoleLock) { try { _runtime.Console.Print(code); } catch { } }
                break;
            }
            case 21: // SET SHELL ON|OFF — opt-in per SYSTEM.SHELL/EXEC (IlRuntime, condiviso coi backend)
                IlRuntime.SetShellEnabled(PopInt(task) != 0);
                break;
        }
    }

}