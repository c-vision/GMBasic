using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;
// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
// ReSharper disable MemberCanBePrivate.Global

namespace ClipperLib.Controls;

/// <summary>Allineamento del contenuto di una colonna (stile Clipper TBrowse 5.3).</summary>
public enum BrowseAlign { Left, Center, Right }

/// <summary>Tipo di dato di una colonna: determina il controllo di editing inline.</summary>
public enum BrowseDataType
{
    Text,
    Int,
    Float,
    Bool,
    Date,
    Choice
}

/// <summary>Coppia di colori (sfondo + primo piano).</summary>
public struct BrowseColorPair
{
    public CellColor Fg;
    public CellColor Bg;
    public BrowseColorPair(CellColor fg, CellColor bg) { Fg = fg; Bg = bg; }
    public static readonly BrowseColorPair Default = new(CellColor.White, CellColor.Black);
}

/// <summary>Definizione di una colonna del TBrowse.</summary>
public class BrowseColumn
{
    public string Header { get; set; }
    public int Width { get; set; }
    public BrowseAlign Align { get; set; } = BrowseAlign.Left;
    public bool Editable { get; set; }
    public bool Visible { get; set; } = true;
    public BrowseDataType DataType { get; set; } = BrowseDataType.Text;
    public List<string>? Choices { get; set; }

    public BrowseColumn(string header, int width)
    {
        Header = header;
        Width = width;
    }
    public BrowseColumn WithAlign(BrowseAlign a) { Align = a; return this; }
    public BrowseColumn WithEditable(bool e) { Editable = e; return this; }
    public BrowseColumn WithDataType(BrowseDataType t) { DataType = t; return this; }
    public BrowseColumn WithChoices(List<string> c) { DataType = BrowseDataType.Choice; Choices = c; return this; }
}

/// <summary>DataSource astratto del TBrowse (stile Clipper 5.2 skipBlock).</summary>
public interface IBrowseDataSource
{
    string GetValue(int row, int col);
    int RowCount();
    int Skip(int delta);
    void GoTop();
    void GoBottom();
    int Bookmark();
    void GoTo(int n);
    void SetValue(int row, int col, string val);
}

/// <summary>DataSource semplice basato su matrice di stringhe.</summary>
public class VecDataSource : IBrowseDataSource
{
    public List<List<string>> Data = [];
    public int Current;

    public VecDataSource(List<List<string>> data) { Data = data; }

    public string GetValue(int row, int col)
    {
        if (row < 0 || row >= Data.Count) return "";
        var r = Data[row];
        return col >= 0 && col < r.Count ? r[col] : "";
    }
    public int RowCount() => Data.Count;
    public int Skip(int delta)
    {
        var np = Current + delta;
        var clamped = Math.Clamp(np, 0, Math.Max(0, Data.Count - 1));
        var actual = clamped - Current;
        Current = clamped;
        return actual;
    }
    public void GoTop() => Current = 0;
    public void GoBottom() => Current = Data.Count == 0 ? 0 : Data.Count - 1;
    public int Bookmark() => Current;
    public void GoTo(int n) => Current = Data.Count == 0 ? 0 : Math.Min(n, Data.Count - 1);
    public void SetValue(int row, int col, string val)
    {
        while (row >= Data.Count) Data.Add([]);
        var r = Data[row];
        while (col >= r.Count) r.Add("");
        r[col] = val;
    }
}

/// <summary>Azione risultante dalla navigazione nel TBrowse.</summary>
public enum BrowseAction
{
    None,
    Selected,
    Cancelled,
    Refreshed,
    Edit,
    DoubleClick
}

/// <summary>
/// TBrowse - griglia dati stile CA-Clipper 5.2 / 5.3.
/// Port fedele (TTY-free) del modulo Rust tui_controls::tbrowse.
/// </summary>
public class TBrowse : Control
{
    private struct EditState
    {
        public int Row;
        public int Col;
        public string Buf;
        public int Cursor;
        public BrowseDataType DataType;
        public string Initial;
        public int DateField;
    }

    public List<BrowseColumn> Columns { get; set; } = [];
    public int FrozenCols { get; set; }
    public int CurrentRow { get; set; }
    public int CurrentCol { get; set; }
    public int TopRow { get; set; }
    public int LeftCol { get; set; }
    public bool MouseEnabled { get; set; } = true;
    public bool ShowHeader { get; set; } = true;
    public bool ShowBorder { get; set; } = true;
    public bool HighlightRow { get; set; } = true;
    public BrowseColorPair HeaderColors { get; set; } = new(CellColor.White, CellColor.Blue);
    public BrowseColorPair RowColors { get; set; } = BrowseColorPair.Default;
    public BrowseColorPair HighlightColors { get; set; } = new(CellColor.Black, CellColor.Cyan);
    public string ColSep { get; set; } = "│";
    public string HeadSep { get; set; } = "─";
    public string FootSep { get; set; } = "─";
    public string? FooterText { get; set; }
    public bool AutoLite { get; set; } = true;
    public bool Focused2 { get; set; }

    private IBrowseDataSource _data;
    private bool _stabilized;
    private (int, int)? _lastRenderPos;
    private (int, int)? _lastRenderTop;
    private bool _invalidated;

    private string _colorSpec = "";
    private bool _dispLocked;
    private EditState? _edit;
    private HashSet<(int, int)> _lockedCells = [];
    private Func<int, int, bool>? _cellLock;
    private Func<TBrowse, bool>? _stableBlock;
    private Dictionary<string, Action<TBrowse>> _hooks = new();
    private Dictionary<int, Action<TBrowse>> _keyHandlers = new();
    private Func<int, int, bool>? _preValidator;
    private Func<int, int, bool>? _postValidator;
    private object? _cargo;

    public TBrowse(List<BrowseColumn> columns, IBrowseDataSource data)
    {
        Columns = columns;
        _data = data;
    }
    public void SetDataSource(IBrowseDataSource data) { _data = data; GoTop(); }
    // ---- aiuto: dimensioni ----
    private int InnerWidth => Math.Max(1, Width - (ShowBorder ? 2 : 0));
    public int PageSize()
    {
        var avail = Height;
        if (ShowHeader) avail = Math.Max(0, avail - 2);
        if (ShowBorder) avail = Math.Max(0, avail - 2);
        if (FooterText != null) avail = Math.Max(0, avail - 2);
        return Math.Max(1, avail);
    }
    public int RowCount() => _data.RowCount();
    public string CurrentValue() => _data.GetValue(CurrentRow, CurrentCol);

    // ===================== NAVIGAZIONE =====================
    public void MoveDown()
    {
        var total = RowCount();
        if (total == 0) return;
        if (CurrentRow < total - 1)
        {
            CurrentRow++;
            if (CurrentRow >= TopRow + PageSize()) TopRow++;
        }
    }
    public void MoveUp()
    {
        if (CurrentRow > 0)
        {
            CurrentRow--;
            if (CurrentRow < TopRow) TopRow = CurrentRow;
        }
    }
    public void PageDown()
    {
        var page = PageSize();
        var total = RowCount();
        if (total == 0) return;
        CurrentRow = Math.Min(CurrentRow + page, total - 1);
        TopRow = Math.Min(TopRow + page, Math.Max(0, total - page));
    }
    public void PageUp()
    {
        var page = PageSize();
        CurrentRow = Math.Max(0, CurrentRow - page);
        TopRow = Math.Max(0, TopRow - page);
    }
    public void GoTop()
    {
        _data.GoTop();
        CurrentRow = 0; TopRow = 0;
    }
    public void GoBottom()
    {
        _data.GoBottom();
        var total = RowCount();
        if (total > 0)
        {
            CurrentRow = total - 1;
            TopRow = Math.Max(0, total - PageSize());
        }
    }
    public void MoveRight()
    {
        var vis = Columns.Count(c => c.Visible);
        if (CurrentCol < vis - 1)
        {
            CurrentCol++;
            if (CurrentCol >= LeftCol + VisibleColCount()) LeftCol++;
        }
    }
    public void MoveLeft()
    {
        if (CurrentCol > 0)
        {
            CurrentCol--;
            if (CurrentCol < LeftCol + FrozenCols && LeftCol > 0) LeftCol--;
        }
    }

    private void ProcessClick(int x, int y)
    {
        var headerOff = ShowHeader ? 1 : 0;
        var borderOff = ShowBorder ? 1 : 0;
        var sepOff = ShowHeader ? 1 : 0;
        var dataStartY = Y + borderOff + headerOff + sepOff;
        if (y >= dataStartY)
        {
            var clicked = y - dataStartY + TopRow;
            if (clicked < RowCount()) CurrentRow = clicked;
        }
        var contentX = X + (ShowBorder ? 1 : 0);
        var cx = x - contentX;
        if (cx >= 0)
        {
            var acc = 0;
            foreach (var i in DisplayedColumns())
            {
                var cw = Columns[i].Width;
                if (cx >= acc && cx < acc + cw) { CurrentCol = i; break; }
                acc += cw + 1;
            }
        }
    }

    /// <summary>Naviga con un tasto (codice KeyConstants/Clipper INKEY) e ritorna l'azione.</summary>
    public BrowseAction Navigate(int key)
    {
        // Mouse INKEY codes: converti e gestisci
        if (key < KeyConstants.K_MOUSE_BASE)
        {
            var raw = KeyConstants.K_MOUSE_BASE - key;
            if (Enum.IsDefined(typeof(MouseActionKind), raw))
            {
                var st = ClipperGlobal.State;
                var kind = (MouseActionKind)raw;
                if (kind is MouseActionKind.Click or MouseActionKind.DoubleClick)
                {
                    if (MouseEnabled && st.MouseX >= X && st.MouseX < X + Width && st.MouseY >= Y && st.MouseY < Y + Height)
                    {
                        ProcessClick(st.MouseX, st.MouseY);
                        return kind == MouseActionKind.DoubleClick ? BrowseAction.DoubleClick : BrowseAction.Refreshed;
                    }
                }
                if (kind == MouseActionKind.ScrollUp) { MoveUp(); return BrowseAction.Refreshed; }
                if (kind == MouseActionKind.ScrollDown) { MoveDown(); return BrowseAction.Refreshed; }
            }
            return BrowseAction.None;
        }

        switch (key)
        {
            case KeyConstants.K_UP: MoveUp(); return BrowseAction.Refreshed;
            case KeyConstants.K_DOWN: MoveDown(); return BrowseAction.Refreshed;
            case KeyConstants.K_PGUP: PageUp(); return BrowseAction.Refreshed;
            case KeyConstants.K_PGDN: PageDown(); return BrowseAction.Refreshed;
            case KeyConstants.K_HOME: GoTop(); return BrowseAction.Refreshed;
            case KeyConstants.K_END: GoBottom(); return BrowseAction.Refreshed;
            case KeyConstants.K_LEFT: MoveLeft(); return BrowseAction.Refreshed;
            case KeyConstants.K_RIGHT: MoveRight(); return BrowseAction.Refreshed;
            case KeyConstants.K_ENTER: return BrowseAction.Selected;
            case KeyConstants.K_ESC: return BrowseAction.Cancelled;
            default: return BrowseAction.None;
        }
    }

    // ===================== COLONNE VISIBILI =====================
    private int VisibleColCount()
    {
        int used = 0, count = 0;
        for (var i = 0; i < Columns.Count && i < FrozenCols; i++)
        {
            var c = Columns[i];
            if (c.Visible) { used += c.Width + 1; }
        }
        for (var i = FrozenCols + LeftCol; i < Columns.Count; i++)
        {
            var c = Columns[i];
            if (!c.Visible) continue;
            used += c.Width + 1;
            if (used > InnerWidth) break;
            count++;
        }
        return count + FrozenCols;
    }

    private List<int> DisplayedColumns()
    {
        var outp = new List<int>();
        var max = InnerWidth;
        var width = 0;
        for (var i = 0; i < Columns.Count && i < FrozenCols; i++)
        {
            if (!Columns[i].Visible) continue;
            outp.Add(i); width += Columns[i].Width + 1;
        }
        var start = FrozenCols + LeftCol;
        for (var i = 0; i < Columns.Count; i++)
        {
            if (i < FrozenCols) continue;
            if (i < start) continue;
            var c = Columns[i];
            if (!c.Visible) continue;
            var needed = c.Width + 1;
            if (width + needed > max && outp.Count > 0) break;
            outp.Add(i); width += needed;
        }
        return outp;
    }

    private int MaxLeftCol()
    {
        var scroll = new List<int>();
        for (var i = FrozenCols; i < Columns.Count; i++) if (Columns[i].Visible) scroll.Add(i);
        if (scroll.Count == 0) return 0;
        var frozenW = 0;
        for (var i = 0; i < FrozenCols; i++) if (Columns[i].Visible) frozenW += Columns[i].Width + 1;
        var usable = Math.Max(0, InnerWidth - frozenW);
        int used = 0, fit = 0;
        foreach (var idx in scroll)
        {
            var w = Columns[idx].Width + 1;
            if (used + w > usable) break;
            used += w; fit++;
        }
        fit = Math.Max(1, fit);
        return Math.Max(0, scroll.Count - fit);
    }

    private int ScrollablePage()
    {
        var disp = DisplayedColumns();
        var frozen = disp.Count(d => d < FrozenCols);
        return Math.Max(1, disp.Count - frozen);
    }

    // ===================== CLIPPER 5.3: NAVIGAZIONE =====================
    public void Up(int n) { for (var i = 0; i < n; i++) MoveUp(); }
    public void Down(int n) { for (var i = 0; i < n; i++) MoveDown(); }
    public void Left(int n) { PanLeft(n); }
    public void Right(int n) { PanRight(n); }
    public void PageUpClipper() => PageUp();
    public void PageDownClipper() => PageDown();
    public void PageLeft() => PanLeft(ScrollablePage());
    public void PageRight() => PanRight(ScrollablePage());
    public void Top() => CurrentRow = TopRow;
    public void Bottom()
    {
        var total = RowCount();
        if (total == 0) return;
        var last = Math.Max(0, TopRow + PageSize() - 1);
        CurrentRow = Math.Min(last, total - 1);
    }
    public void GoTo(int row, int col)
    {
        var total = RowCount();
        CurrentRow = total == 0 ? 0 : Math.Min(row, total - 1);
        var vcols = Columns.Count(c => c.Visible);
        CurrentCol = vcols == 0 ? 0 : Math.Min(col, vcols - 1);
    }
    public (int, int) Pos() => (CurrentRow, CurrentCol);
    public int RowPos() => CurrentRow;
    public void SetRowPos(int r) { var t = RowCount(); CurrentRow = t == 0 ? 0 : Math.Min(r, t - 1); }
    public int ColPos() => CurrentCol;
    public void SetColPos(int c) { var v = Columns.Count(x => x.Visible); CurrentCol = v == 0 ? 0 : Math.Min(c, v - 1); }

    public int LeftVisible() => DisplayedColumns().FirstOrDefault();
    public int RightVisible() => DisplayedColumns().LastOrDefault();
    public int TopVisible() => TopRow;
    public int BottomVisible()
    {
        var total = RowCount();
        if (total == 0) return 0;
        return Math.Min(TopRow + PageSize() - 1, total - 1);
    }

    // ===================== CLIPPER 5.3: SCROLL / PAN =====================
    public void PanHome() => LeftCol = 0;
    public void PanEnd() => LeftCol = MaxLeftCol();
    public void PanLeft(int n) => LeftCol = Math.Max(0, LeftCol - n);
    public void PanRight(int n) => LeftCol = Math.Min(LeftCol + n, MaxLeftCol());
    public void HillUp() => Up(Math.Max(1, PageSize() / 2));
    public void HillDown() => Down(Math.Max(1, PageSize() / 2));
    public void HillLeft() => PanLeft(Math.Max(1, ScrollablePage() / 2));
    public void HillRight() => PanRight(Math.Max(1, ScrollablePage() / 2));

    // ===================== CLIPPER 5.3: GESTIONE COLONNE =====================
    public void AddColumn(BrowseColumn col) => Columns.Add(col);
    public void DelColumn(int pos) { if (pos >= 0 && pos < Columns.Count) Columns.RemoveAt(pos); }
    public void InsColumn(int pos, BrowseColumn col) { Columns.Insert(Math.Min(pos, Columns.Count), col); }
    public BrowseColumn? GetColumn(int pos) => pos >= 0 && pos < Columns.Count ? Columns[pos] : null;
    public void SetColumn(int pos, BrowseColumn col) { if (pos >= 0 && pos < Columns.Count) Columns[pos] = col; }
    public int ColCount() => Columns.Count(c => c.Visible);

    // ===================== CELL LOCK =====================
    public void LockCell(int r, int c) => _lockedCells.Add((r, c));
    public void UnlockCell(int r, int c) => _lockedCells.Remove((r, c));
    public bool IsCellLocked(int r, int c) =>
        _lockedCells.Contains((r, c)) || (_cellLock != null && _cellLock(r, c));
    public void SetCellLock(Func<int, int, bool> f) => _cellLock = f;
    public bool CellEditable(int r, int c)
    {
        if (c >= 0 && c < Columns.Count && Columns[c].Editable) return !IsCellLocked(r, c);
        return false;
    }

    // ===================== CLIPPER 5.3: FREEZE / STABILITY =====================
    public int Freeze(int n)
    {
        var max = ColCount();
        FrozenCols = Math.Min(n, max);
        return n;
    }
    public int FreezeCount() => FrozenCols;
    public bool Stable() => _stableBlock?.Invoke(this) ?? _stabilized;
    public void ForceStable() { DisplayedColumns(); _invalidated = false; _stabilized = true; }
    public void SetStableBlock(Func<TBrowse, bool> f) => _stableBlock = f;
    public bool Moved()
    {
        if (_lastRenderPos.HasValue && _lastRenderTop.HasValue)
            return _lastRenderPos != (CurrentRow, CurrentCol) || _lastRenderTop != (TopRow, LeftCol);
        return true;
    }
    public void Invalidate() => _invalidated = true;
    public bool Invalid() => _invalidated;

    // ===================== CLIPPER 5.3: AUTO-LITE =====================
    public bool AutoLiteOn() => AutoLite;
    public void SetAutoLite(bool b) => AutoLite = b;


    // ===================== CLIPPER 5.3: COLOR / SEPARATORI =====================
    public string ColorSpec() => _colorSpec;
    public void SetColorSpec(string spec)
    {
        _colorSpec = spec;
        var parts = spec.Split(',');
        if (parts.Length > 0) HeaderColors = ParseColorPair(parts[0]);
        if (parts.Length > 2) RowColors = ParseColorPair(parts[2]);
        if (parts.Length > 4) HighlightColors = ParseColorPair(parts[4]);
    }
    public void SetColSep(string s) => ColSep = s;
    public void SetHeadSep(string s) => HeadSep = s;
    public void SetFootSep(string s) => FootSep = s;
    public void SetFooter(string? text) => FooterText = text;

    // ===================== CLIPPER 5.3: CELLA / BOOKMARK =====================
    public string CellValue(int r, int c) => _data.GetValue(r, c);
    public string CellValueCurrent() => _data.GetValue(CurrentRow, CurrentCol);
    public void Bookmark(int n) { _data.GoTo(n); CurrentRow = _data.Bookmark(); }
    public void SetCargo(object v) => _cargo = v;
    public object? GetCargo() => _cargo;

    // ===================== CLIPPER 5.3: FOCUS / HOOKS / EDIT =====================
    public void SetFocus() => Focused2 = true;
    public void KillFocus() => Focused2 = false;
    public void Register(string kind, Action<TBrowse> f) => _hooks[kind] = f;
    public void CallHook(string kind) { if (_hooks.TryGetValue(kind, out var h)) h(this); }
    public void SetKey(int k, Action<TBrowse> f) => _keyHandlers[k] = f;
    public BrowseAction ApplyKey(int k)
    {
        if (_keyHandlers.TryGetValue(k, out var h)) { h(this); return BrowseAction.Refreshed; }
        switch (k)
        {
            case KeyConstants.K_UP: MoveUp(); return BrowseAction.Refreshed;
            case KeyConstants.K_DOWN: MoveDown(); return BrowseAction.Refreshed;
            case KeyConstants.K_LEFT: MoveLeft(); return BrowseAction.Refreshed;
            case KeyConstants.K_RIGHT: MoveRight(); return BrowseAction.Refreshed;
            case KeyConstants.K_PGUP: PageUp(); return BrowseAction.Refreshed;
            case KeyConstants.K_PGDN: PageDown(); return BrowseAction.Refreshed;
            case KeyConstants.K_HOME: GoTop(); return BrowseAction.Refreshed;
            case KeyConstants.K_END: GoBottom(); return BrowseAction.Refreshed;
            case KeyConstants.K_ENTER: return BrowseAction.Selected;
            case KeyConstants.K_ESC: return BrowseAction.Cancelled;
            default: return BrowseAction.None;
        }
    }

    // ===================== EDITING INLINE =====================
    private static (string, int) ClipDisplay(string buf, int cursor, int width)
    {
        var chars = buf.ToCharArray();
        var n = chars.Length;
        cursor = Math.Min(cursor, n);
        if (n <= width) return (buf, cursor);
        var offset = Math.Max(0, cursor - (width - 1));
        if (offset + width > n) offset = n - width;
        var s = new string(chars, offset, width);
        return (s, cursor - offset);
    }
    public bool BeginEdit()
    {
        if (!CellEditable(CurrentRow, CurrentCol)) return false;
        int col = CurrentCol, row = CurrentRow;
        var dt = Columns[col].DataType;
        var initial = _data.GetValue(row, col);
        if (dt == BrowseDataType.Date && !ParseDate(initial).HasValue) dt = BrowseDataType.Text;
        _edit = new EditState
        {
            Row = row,
            Col = col,
            Buf = initial,
            Cursor = initial.Length,
            DataType = dt,
            Initial = initial,
            DateField = 2
        };
        return true;
    }
    /// <summary>Processa un tasto durante l'editing. Ritorna l'azione se termina, null se continua.</summary>
    public BrowseAction? ApplyEditKey(int key)
    {
        if (_edit == null) return BrowseAction.None;
        if (key == KeyConstants.K_ESC) { _edit = null; return BrowseAction.Cancelled; }
        switch (key)
        {
            case KeyConstants.K_TAB: return EditMove(0, 1);
            case KeyConstants.K_BTAB: return EditMove(0, -1);
            case KeyConstants.K_ENTER: return CommitEdit();
            case KeyConstants.K_UP:
                if (_edit!.Value.DataType != BrowseDataType.Choice && _edit!.Value.DataType != BrowseDataType.Date)
                    return EditMove(-1, 0);
                EditDate(key); return null;
            case KeyConstants.K_DOWN:
                if (_edit!.Value.DataType != BrowseDataType.Choice && _edit!.Value.DataType != BrowseDataType.Date)
                    return EditMove(1, 0);
                EditDate(key); return null;
            case KeyConstants.K_LEFT:
                if (_edit!.Value.DataType == BrowseDataType.Text || _edit!.Value.DataType == BrowseDataType.Date)
                    EditCursorLeft();
                else return EditMove(0, -1);
                return null;
            case KeyConstants.K_RIGHT:
                if (_edit!.Value.DataType == BrowseDataType.Text || _edit!.Value.DataType == BrowseDataType.Date)
                    EditCursorRight();
                else return EditMove(0, 1);
                return null;
            default:
                EditChar(key);
                return null;
        }
    }

    private BrowseAction? EditMove(int drow, int dcol)
    {
        if (_edit.HasValue)
        {
        var es = _edit!.Value;
            _data.SetValue(es.Row, es.Col, es.Buf);
            PreValidate(es.Row, es.Col);
            PostValidate(es.Row, es.Col);
            CurrentRow = es.Row; CurrentCol = es.Col;
        }
        var next = FindEditable(CurrentRow, CurrentCol, drow, dcol);
        if (next.HasValue) { CurrentRow = next.Value.r; CurrentCol = next.Value.c; BeginEdit(); return null; }
        return BrowseAction.Selected;
    }

    private (int r, int c)? FindEditable(int row, int col, int drow, int dcol)
    {
        int nrows = RowCount(), ncols = Columns.Count;
        if (dcol > 0)
        {
            for (var r = row; r < nrows; r++)
            {
                var cstart = r == row ? col + 1 : 0;
                for (var c = cstart; c < ncols; c++) if (CellEditable(r, c)) return (r, c);
            }
        }
        else if (dcol < 0)
        {
            for (var r = row; r >= 0; r--)
            {
                var cstart = r == row ? col - 1 : ncols - 1;
                for (var c = cstart; c >= 0; c--) if (CellEditable(r, c)) return (r, c);
            }
        }
        else if (drow > 0)
        {
            for (var r = row + 1; r < nrows; r++) if (CellEditable(r, col)) return (r, col);
        }
        else if (drow < 0)
        {
            for (var r = row - 1; r >= 0; r--) if (CellEditable(r, col)) return (r, col);
        }
        return null;
    }
    public BrowseAction CommitEdit()
    {
        if (!_edit.HasValue) return BrowseAction.None;
        var es = _edit!.Value;
        int row = es.Row, col = es.Col;
        _data.SetValue(row, col, es.Buf);
        _edit = null;
        var ok = PreValidate(row, col) && PostValidate(row, col);
        return ok ? BrowseAction.Selected : BrowseAction.Refreshed;
    }

    private void EditChar(int key)
    {
        var es = _edit!.Value;
        if (key == KeyConstants.K_BS)
        {
            if (es.Cursor > 0)
            {
                es.Buf = es.Buf.Remove(es.Cursor - 1, 1);
                es.Cursor--;
            }
        }
        else if (key == KeyConstants.K_DEL)
        {
            if (es.Cursor < es.Buf.Length) es.Buf = es.Buf.Remove(es.Cursor, 1);
        }
        else if (key == KeyConstants.K_HOME) es.Cursor = 0;
        else if (key == KeyConstants.K_END) es.Cursor = es.Buf.Length;
        else if (key is >= 32 and <= 126)
            es.Buf = es.Buf.Insert(es.Cursor, ((char)key).ToString()).Replace("‎", "");
        _edit = es;
    }

    private void EditCursorLeft() { var es = _edit!.Value; es.Cursor = Math.Max(0, es.Cursor - 1); _edit = es; }
    private void EditCursorRight() { var es = _edit!.Value; es.Cursor = Math.Min(es.Cursor + 1, es.Buf.Length); _edit = es; }

    private void EditNumeric(int key, bool isFloat)
    {
        var es = _edit!.Value;
        if (key == KeyConstants.K_UP) NumericStep(ref es, true, isFloat);
        else if (key == KeyConstants.K_DOWN) NumericStep(ref es, false, isFloat);
        else if (key == KeyConstants.K_BS) { if (es.Cursor > 0) { es.Buf = es.Buf.Remove(es.Cursor - 1, 1); es.Cursor--; } }
        else if (key == KeyConstants.K_HOME) es.Cursor = 0;
        else if (key == KeyConstants.K_END) es.Cursor = es.Buf.Length;
        else if (key is >= 32 and <= 126)
        {
            var c = (char)key;
            var ok = isFloat
                ? char.IsDigit(c) || (c == '.' && !es.Buf.Contains('.'))
                : char.IsDigit(c) || (c == '-' && es.Cursor == 0 && !es.Buf.Contains('-'));
            if (ok) { es.Buf = es.Buf.Insert(es.Cursor, c.ToString()); es.Cursor++; }
        }
        _edit = es;
    }

    private static void NumericStep(ref EditState es, bool inc, bool isFloat)
    {
        if (!double.TryParse(es.Buf.Trim(), out var v)) v = 0;
        v = inc ? v + 1 : v - 1;
        es.Buf = isFloat ? $"{v:F2}" : $"{(long)v}";
        es.Cursor = es.Buf.Length;
    }

    private void EditBool(int key)
    {
        var es = _edit!.Value;
        if (key is ' ' or KeyConstants.K_ENTER or KeyConstants.K_UP or KeyConstants.K_DOWN)
        {
            var cur = es.Buf is "Y" or "true";
            es.Buf = cur ? "N" : "Y";
            es.Cursor = es.Buf.Length;
        }
        _edit = es;
    }

    private void EditChoice(int key)
    {
        var es = _edit!.Value;
        if (es.DataType == BrowseDataType.Choice && Columns[es.Col].Choices != null)
        {
            var opts = Columns[es.Col].Choices!;
            if (opts.Count > 0)
            {
                var idx = Math.Max(0, opts.IndexOf(es.Buf));
                var d = key == KeyConstants.K_UP ? -1 : 1;
                idx = ((idx + d) % opts.Count + opts.Count) % opts.Count;
                es.Buf = opts[idx];
                es.Cursor = es.Buf.Length;
            }
        }
        _edit = es;
    }

    private void EditDate(int key)
    {
        var es = _edit!.Value;
        var (y, m, d) = ParseDate(es.Buf) ?? (1970, 1, 1);
        if (key == KeyConstants.K_LEFT) es.DateField = Math.Max(0, es.DateField - 1);
        else if (key == KeyConstants.K_RIGHT) es.DateField = Math.Min(2, es.DateField + 1);
        else if (key == KeyConstants.K_UP)
        {
            if (es.DateField == 0) y++;
            else if (es.DateField == 1) m = m % 12 + 1;
            else d = d % DaysInMonth(y, m) + 1;
        }
        else if (key == KeyConstants.K_DOWN)
        {
            if (es.DateField == 0) y = Math.Max(1, y - 1);
            else if (es.DateField == 1) m = m == 1 ? 12 : m - 1;
            else d = d <= 1 ? DaysInMonth(y, m) : d - 1;
        }
        m = Math.Clamp(m, 1, 12);
        d = Math.Clamp(d, 1, DaysInMonth(y, m));
        es.Buf = $"{y:0000}-{m:00}-{d:00}";
        es.Cursor = es.Buf.Length;
        _edit = es;
    }

    private static int DaysInMonth(int y, int m)
    {
        return m switch
        {
            1 or 3 or 5 or 7 or 8 or 10 or 12 => 31,
            4 or 6 or 9 or 11 => 30,
            2 => (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 29 : 28,
            _ => 30
        };
    }

    private static (int, int, int)? ParseDate(string s)
    {
        var parts = s.Split('-');
        if (parts.Length != 3) return null;
        if (!int.TryParse(parts[0], out var y) || !int.TryParse(parts[1], out var m) || !int.TryParse(parts[2], out var d)) return null;
        if (m < 1 || m > 12 || d < 1 || d > DaysInMonth(y, m)) return null;
        return (y, m, d);
    }

    // ===================== VALIDATORI =====================
    public void SetPreValidator(Func<int, int, bool> f) => _preValidator = f;
    public bool PreValidate(int r, int c) => _preValidator?.Invoke(r, c) ?? true;
    public void SetPostValidator(Func<int, int, bool> f) => _postValidator = f;
    public bool PostValidate(int r, int c) => _postValidator?.Invoke(r, c) ?? true;

    // ===================== RENDER =====================
    public void DispBegin() => _dispLocked = true;
    public void DispEnd() { _dispLocked = false; ForceStable(); }

    private string RenderInner(string sep, Func<int, int, string> cellFn)
    {
        var innerW = InnerWidth;
        var idxs = DisplayedColumns();
        var n = idxs.Count;
        var sb = new System.Text.StringBuilder();
        var x = 0;
        for (var k = 0; k < idxs.Count; k++)
        {
            var i = idxs[k];
            var cw = Columns[i].Width;
            if (k + 1 < n)
            {
                var block = cw + 1;
                if (x + block > innerW)
                {
                    var rem = innerW - x;
                    if (rem > 0) sb.Append(cellFn(i, rem));
                    break;
                }
                sb.Append(cellFn(i, cw));
                sb.Append(sep);
                x += block;
            }
            else
            {
                var rem = innerW - x;
                if (rem > 0) sb.Append(cellFn(i, rem));
                break;
            }
        }
        var len = sb.ToString().Length;
        if (len < innerW) sb.Append(' ', innerW - len);
        else if (len > innerW) return new string(sb.ToString().ToCharArray(), 0, innerW);
        return sb.ToString();
    }

    private string FormatCell(string value, int width, BrowseAlign align)
    {
        var vl = value.Length;
        if (vl >= width) return new string(value.ToCharArray(), 0, width);
        var pad = width - vl;
        return align switch
        {
            BrowseAlign.Right => new string(' ', pad) + value,
            BrowseAlign.Center => new string(' ', pad / 2) + value + new string(' ', pad - pad / 2),
            _ => value + new string(' ', pad)
        };
    }
    public List<string> RenderRows()
    {
        var page = PageSize();
        var total = RowCount();
        var innerW = InnerWidth;
        var rows = new List<string>();
        for (var dr = 0; dr < page; dr++)
        {
            var dataRow = TopRow + dr;
            if (dataRow >= total) { rows.Add(new string(' ', innerW)); continue; }
            rows.Add(RenderInner(ColSep, (i, w) =>
            {
                if (_edit.HasValue && _edit!.Value.Row == dataRow && _edit!.Value.Col == i)
                {
                    var (disp, _) = ClipDisplay(_edit!.Value.Buf, _edit!.Value.Cursor, w);
                    return FormatCell(disp, w, Columns[i].Align);
                }
                return FormatCell(ApplyCase(_data.GetValue(dataRow, i)), w, Columns[i].Align);
            }));
        }
        return rows;
    }
    public string RenderHeader() => RenderInner(ColSep, (i, w) => FormatCell(Columns[i].Header, w, Columns[i].Align));
    public string RenderSeparator() => RenderInner("┼", (_, w) => new string(HeadSep[0], w));
    public string RenderFooterSeparator() => RenderInner("┼", (_, w) => new string(FootSep[0], w));
    public List<string> Render()
    {
        if (_dispLocked) return [];
        var frame = new List<string>();
        var innerW = InnerWidth;
        if (ShowBorder) frame.Add($"┌{new string('─', innerW)}┐");
        if (ShowHeader)
        {
            frame.Add(ShowBorder ? $"│{RenderHeader()}│" : RenderHeader());
            frame.Add(ShowBorder ? $"│{RenderSeparator()}│" : RenderSeparator());
        }
        foreach (var line in RenderRows())
            frame.Add(ShowBorder ? $"│{line}│" : line);
        if (FooterText != null)
        {
            frame.Add(ShowBorder ? $"│{RenderFooterSeparator()}│" : RenderFooterSeparator());
            var f = FormatCell(FooterText, innerW, BrowseAlign.Left);
            frame.Add(ShowBorder ? $"│{f}│" : f);
        }
        if (ShowBorder) frame.Add($"└{new string('─', innerW)}┘");
        return frame;
    }
    public List<string> Refresh()
    {
        _lastRenderPos = (CurrentRow, CurrentCol);
        _lastRenderTop = (TopRow, LeftCol);
        _stabilized = true;
        return Render();
    }
    public List<string> RefreshAll() { _invalidated = false; DisplayedColumns(); return Refresh(); }

    private (int, int)? EditCellView()
    {
        if (!_edit.HasValue) return null;
        var es = _edit!.Value;
        var idxs = DisplayedColumns();
        var n = idxs.Count;
        var x = 0;
        for (var k = 0; k < n; k++)
        {
            var i = idxs[k];
            if (i == es.Col)
            {
                var w = k + 1 == n ? InnerWidth - x : Columns[i].Width;
                var (disp, rel) = ClipDisplay(es.Buf, es.Cursor, w);
                var dlen = disp.Length;
                var pad = Columns[i].Align switch
                {
                    BrowseAlign.Right => Math.Max(0, w - dlen),
                    BrowseAlign.Center => (w - dlen) / 2,
                    _ => 0
                };
                return (x, pad + rel);
            }
            x += Columns[i].Width + 1;
        }
        return null;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        if (_dispLocked) return;
        int ax = X, ay = Y;
        var innerW = InnerWidth;
        var bh = buffer.Height;
        var y = ay;

        (var hfg, var hbg) = (HeaderColors.Fg, HeaderColors.Bg);
        (var rfg, var rbg) = (RowColors.Fg, RowColors.Bg);
        (var sfg, var sbg) = (HighlightColors.Fg, HighlightColors.Bg);

        if (ShowBorder && Width >= 2) { if (y < bh) buffer.SetString(ax, y, $"┌{new string('─', innerW)}┐", hfg, hbg); y++; }
        if (ShowHeader)
        {
            var h = RenderHeader();
            if (y < bh) buffer.SetString(ax, y, ShowBorder ? $"│{h}│" : h, hfg, hbg);
            y++;
            var sep = RenderSeparator();
            if (y < bh) buffer.SetString(ax, y, ShowBorder ? $"│{sep}│" : sep, hfg, hbg);
            y++;
        }

        var rows = RenderRows();
        for (var dr = 0; dr < rows.Count; dr++)
        {
            if (y >= bh) break;
            var dataRow = TopRow + dr;
            var line = rows[dr];
            var isCur = dataRow == CurrentRow && dataRow < RowCount();
            (var fg, var bg) = isCur ? (sfg, sbg) : (rfg, rbg);
            if (ShowBorder)
            {
                buffer.SetChar(ax, y, '│', hfg, hbg);
                buffer.SetString(ax + 1, y, line, fg, bg);
                buffer.SetChar(ax + 1 + innerW, y, '│', hfg, hbg);
            }
            else buffer.SetString(ax, y, line, fg, bg);

            if (_edit.HasValue && _edit!.Value.Row == dataRow)
            {
                var view = EditCellView();
                if (view.HasValue)
                {
                    var contentX = ax + (ShowBorder ? 1 : 0);
                    var cx = contentX + view.Value.Item1 + view.Value.Item2;
                    if (cx < ax + Width)
                    {
                        var ch = _edit!.Value.Cursor < _edit!.Value.Buf.Length ? _edit!.Value.Buf[_edit!.Value.Cursor] : ' ';
                        buffer.SetChar(cx, y, ch, sbg, sfg);
                    }
                }
            }
            y++;
        }

        if (FooterText != null)
        {
            var fsep = RenderFooterSeparator();
            if (y < bh) buffer.SetString(ax, y, ShowBorder ? $"│{fsep}│" : fsep, hfg, hbg);
            y++;
            if (y < bh)
            {
                var f = FormatCell(FooterText, innerW, BrowseAlign.Left);
                buffer.SetString(ax, y, ShowBorder ? $"│{f}│" : f, rfg, rbg);
            }
            y++;
        }
        if (ShowBorder && Width >= 2 && y < bh)
            buffer.SetString(ax, y, $"└{new string('─', innerW)}┘", hfg, hbg);
    }

    public override bool HandleKey(int key)
    {
        if (_edit.HasValue)
        {
            var a = ApplyEditKey(key);
            return a != null;
        }
        return Navigate(key) != BrowseAction.None;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || x < X || x >= X + Width || y < Y || y >= Y + Height) return false;
        if (!MouseEnabled) return false;
        ProcessClick(x, y);
        return true;
    }

    public override bool HandleMouse(MouseAction action)
    {
        if (!Enabled || !MouseEnabled) return false;

        switch (action.Kind)
        {
            case MouseActionKind.Click:
            case MouseActionKind.DoubleClick:
                return HandleClick(action.X, action.Y);
            case MouseActionKind.ScrollUp:
                MoveUp();
                return true;
            case MouseActionKind.ScrollDown:
                MoveDown();
                return true;
            default:
                return false;
        }
    }

    // ===================== COLORI CLIPPER =====================
    private static BrowseColorPair ParseColorPair(string token)
    {
        token = token.Trim();
        var slash = token.IndexOf('/');
        var fg = slash >= 0 ? token[..slash].Trim() : token;
        var bg = slash >= 0 ? token[(slash + 1)..].Trim() : "N";
        return new BrowseColorPair(ParseColor(fg), ParseColor(bg));
    }

    private static CellColor ParseColor(string s)
    {
        var bright = s.Contains('+');
        var t = new string(s.Where(c => c != '+').ToArray()).Trim();
        return t switch
        {
            "N" => bright ? CellColor.Grey : CellColor.Black,
            "B" => bright ? CellColor.Blue : CellColor.DarkBlue,
            "G" => bright ? CellColor.Green : CellColor.DarkGreen,
            "BG" => bright ? CellColor.Cyan : CellColor.DarkCyan,
            "R" => bright ? CellColor.Red : CellColor.DarkRed,
            "RB" => bright ? CellColor.Magenta : CellColor.DarkMagenta,
            "GR" => bright ? CellColor.Yellow : CellColor.DarkYellow,
            "W" => bright ? CellColor.White : CellColor.Grey,
            _ => CellColor.White
        };
    }

    /// <summary>Mappa un tasto KeyConstants nel corrispondente codice INKEY di Clipper.</summary>
    public static int KeyCodeToI32(int key) => key;
}
