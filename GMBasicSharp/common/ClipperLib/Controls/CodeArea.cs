using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>
/// Keyword/symbol → color map for syntax highlighting (pure, TTY-free).
/// Faithful port of the Rust <c>Syntax</c> struct from CodeArea.
/// </summary>
public class Syntax
{
    private readonly Dictionary<string, CellColor> words = new();
    private readonly Dictionary<char, CellColor> symbols = new();

    public Syntax() { }

    public static Syntax New() => new();

    /// <summary>Build a syntax from a list of keywords, all the same color.</summary>
    public static Syntax FromKeywords(IEnumerable<string> keywords) => FromKeywordsWith(keywords, CellColor.Yellow);

    /// <summary>Build a syntax from a list of keywords with an explicit color.</summary>
    public static Syntax FromKeywordsWith(IEnumerable<string> keywords, CellColor color)
    {
        var s = new Syntax();
        foreach (var w in keywords) s.words[w] = color;
        return s;
    }

    public Syntax AddWord(string word, CellColor color) { words[word] = color; return this; }
    public Syntax AddWords(IEnumerable<string> ws, CellColor color) { foreach (var w in ws) words[w] = color; return this; }
    public Syntax AddSymbol(char symbol, CellColor color) { symbols[symbol] = color; return this; }

    /// <summary>Color associated with a word, if any.</summary>
    public CellColor? WordColor(string word) => words.TryGetValue(word, out var c) ? c : null;

    /// <summary>Registered keywords (references).</summary>
    public IEnumerable<string> Words() => words.Keys;

    /// <summary>Color associated with a symbol, if any.</summary>
    public CellColor? SymbolColor(char symbol) => symbols.TryGetValue(symbol, out var c) ? c : null;

    /// <summary>Compute a per-character color override for a line (null = use base color).</summary>
    public CellColor?[] Colorize(string line)
    {
        var colors = new CellColor?[line.Length];
        var i = 0;
        while (i < line.Length)
        {
            var c = line[i];
            if (char.IsLetterOrDigit(c) || c == '_')
            {
                var start = i;
                while (i < line.Length && (char.IsLetterOrDigit(line[i]) || line[i] == '_')) i++;
                if (words.TryGetValue(line[start..i], out var col))
                    for (var k = start; k < i; k++) colors[k] = col;
            }
            else
            {
                if (symbols.TryGetValue(c, out var col)) colors[i] = col;
                i++;
            }
        }
        return colors;
    }
}

/// <summary>
/// A syntax-highlighted code editor with a line-number gutter, breakpoint/bookmark
/// markers, current-execution-line highlight, text selection and a vertical scrollbar.
/// TTY-free: never writes to the console. Editing/scrolling logic is inherited from
/// <see cref="TextArea"/>; this class adds the gutter and per-token coloring.
/// </summary>
public class CodeArea : TextArea
{
    public Syntax Syntax { get; set; } = new();
    public bool ShowLineNumbers { get; set; } = true;
    public CellColor GutterColor { get; set; } = CellColor.Grey;

    /// <summary>Linee con breakpoint (0-based). Marker rosso ● nella gutter.</summary>
    public HashSet<int> BreakpointLines { get; set; } = [];

    /// <summary>Linee con bookmark (0-based). Marker ciano ◆ nella gutter.</summary>
    public HashSet<int> BookmarkLines { get; set; } = [];

    /// <summary>Linea corrente di esecuzione (0-based), evidenziata in giallo (null = off).</summary>
    public int? CurrentExecutionLine { get; set; }

    /// <summary>Invocato quando l'utente clicca la gutter (argomento = indice riga 0-based).</summary>
    public Action<int>? OnGutterClick { get; set; }

    /// <summary>Mostra una scrollbar verticale sul bordo destro.</summary>
    public bool ShowScrollBar { get; set; } = true;

    /// <summary>Larghezza di un TAB in spazi (Fase 6 — configurabile da IdeSettings).</summary>
    public int TabWidth { get; set; } = 2;

    // ------------------------------------------------------------------
    // Selezione testo
    // ------------------------------------------------------------------
    public (int line, int col) SelectionAnchor;
    public (int line, int col)? SelectionEnd;
    public bool HasSelection => SelectionEnd.HasValue && SelectionAnchor != SelectionEnd.Value;

    public void SetSelection((int line, int col) anchor, (int line, int col) end)
    {
        SelectionAnchor = anchor;
        SelectionEnd = end;
    }

    public void ClearSelection() { SelectionAnchor = (CursorLine, CursorCol); SelectionEnd = null; }

    /// <summary>Righe attualmente selezionate (intervallo ordinato), o null.</summary>
    public (int from, int to)? SelectionRange
    {
        get
        {
            if (!SelectionEnd.HasValue) return null;
            var a = SelectionAnchor;
            var b = SelectionEnd.Value;
            return a.CompareTo(b) <= 0 ? (a.line, b.line) : (b.line, a.line);
        }
    }

    /// <summary>Testo della selezione corrente (righe separate da \n), o null.</summary>
    public string? SelectedText
    {
        get
        {
            if (!HasSelection) return null;
            var (from, to) = SelectionRange!.Value;
            var a = SelectionAnchor;
            var b = SelectionEnd!.Value;
            var start = a.CompareTo(b) <= 0 ? a : b;
            var end = a.CompareTo(b) <= 0 ? b : a;
            var sb = new System.Text.StringBuilder();
            for (var l = start.line; l <= end.line; l++)
            {
                if (l >= Lines.Count) break;
                var line = Lines[l];
                var fc = l == start.line ? Math.Min(start.col, line.Length) : 0;
                var tc = l == end.line ? Math.Min(end.col, line.Length) : line.Length;
                if (fc < tc) sb.Append(line[fc..tc]);
                if (l < end.line) sb.Append('\n');
            }
            return sb.ToString();
        }
    }

    /// <summary>Selezione singola riga che parte dalla colonna 0 fino alla fine.</summary>
    public void SelectLine(int line)
    {
        if (line < 0 || line >= Lines.Count) return;
        SetSelection((line, 0), (line, Lines[line].Length));
    }

    /// <summary>Sostituisce la selezione corrente con il testo dato, curando l'indice.</summary>
    public void ReplaceSelection(string text)
    {
        var (from, to) = SelectionRange ?? (CursorLine, CursorLine);
        var a = SelectionAnchor;
        var b = SelectionEnd ?? (CursorLine, CursorCol);
        var start = a.CompareTo(b) <= 0 ? a : b;
        var end = a.CompareTo(b) <= 0 ? b : a;
        var (fLine, fCol) = start;
        var (tLine, tCol) = end;

        var newLines = text.Split('\n');
        var head = fCol > 0 && fLine < Lines.Count ? Lines[fLine][..fCol] : "";
        var tail = tLine < Lines.Count ? Lines[tLine][tCol..] : "";
        var removeCount = Math.Min(tLine - fLine, Math.Max(0, Lines.Count - fLine - 1));

        if (removeCount > 0) Lines.RemoveRange(fLine + 1, removeCount);
        if (newLines.Length == 1)
        {
            Lines[fLine] = head + newLines[0] + tail;
            CursorLine = fLine;
            CursorCol = head.Length + newLines[0].Length;
        }
        else
        {
            Lines[fLine] = head + newLines[0];
            for (var i = 1; i < newLines.Length; i++) Lines.Insert(fLine + i, newLines[i]);
            Lines[fLine + newLines.Length - 1] += tail;
            CursorLine = fLine + newLines.Length - 1;
            CursorCol = newLines[^1].Length;
        }
        ClearSelection();
        Modified = true;
    }

    /// <summary>Width (in cells) of the line-number gutter, including markers and separator.</summary>
    public int GutterWidth
    {
        get
        {
            if (!ShowLineNumbers) return 0;
            return GutterInnerWidth + 1; // +1 for the '|' separator
        }
    }

    /// <summary>Larghezza numero riga (senza separatore).</summary>
    public int GutterInnerWidth => Math.Max(2, Lines.Count.ToString().Length);

    private static readonly CellColor MarkerBreakpoint = new() { R = 255, G = 0, B = 0 };
    private static readonly CellColor MarkerBookmark = new() { R = 0, G = 255, B = 255 };
    private static readonly CellColor MarkerCurrentLine = new() { R = 255, G = 255, B = 0 };
    private static readonly CellColor SelRowBg = new() { R = 0, G = 0, B = 168 };
    private static readonly CellColor ScrollTrack = new() { R = 0, G = 0, B = 100 };
    private static readonly CellColor ScrollThumb = new() { R = 192, G = 192, B = 192 };

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.CodeArea, this,
                        text: string.Join("\n", Lines),
                        sel: ScrollOffset, min: CursorLine, max: CursorCol);
    }

    /// <summary>True se la cella (riga, colonna) cade dentro l'intervallo di selezione.</summary>
    private bool CurrentSelectionCovers(int idx, int j, (int from, int to)? selRange)
    {
        if (!selRange.HasValue) return false;
        var a = SelectionAnchor;
        var b = SelectionEnd!.Value;
        var start = a.CompareTo(b) <= 0 ? a : b;
        var end = a.CompareTo(b) <= 0 ? b : a;
        if (idx < start.line || idx > end.line) return false;
        var line = Lines[idx];
        if (idx > start.line && idx < end.line) return true;
        if (idx == start.line && idx == end.line)
            return j >= start.col && j < end.col;
        if (idx == start.line) return j >= start.col;
        if (idx == end.line) return j < end.col;
        return false;
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        switch (key)
        {
            case KeyConstants.K_TAB:
                Modified = true;
                Lines[CursorLine] = Lines[CursorLine].Insert(Math.Min(CursorCol, Lines[CursorLine].Length), new string(' ', TabWidth));
                CursorCol += TabWidth;
                return true;
            case KeyConstants.K_DEL:
                if (CursorCol < Lines[CursorLine].Length)
                {
                    Modified = true;
                    Lines[CursorLine] = Lines[CursorLine].Remove(CursorCol, 1);
                }
                else if (CursorLine < Lines.Count - 1)
                {
                    Modified = true;
                    Lines[CursorLine] += Lines[CursorLine + 1];
                    Lines.RemoveAt(CursorLine + 1);
                }
                return true;
            case KeyConstants.K_PGUP:
                ScrollOffset = Math.Max(0, ScrollOffset - Height);
                CursorLine = Math.Max(0, CursorLine - Height);
                return true;
            case KeyConstants.K_PGDN:
                ScrollOffset = Math.Min(Math.Max(0, Lines.Count - Height), ScrollOffset + Height);
                CursorLine = Math.Min(Lines.Count - 1, CursorLine + Height);
                return true;
            default:
                return base.HandleKey(key);
        }
    }

    /// <summary>True mentre si trascina la scrollbar (per lo scroll continuo).</summary>
    private bool _scrollBarDragging;

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        var gutter = GutterWidth;
        var codeX = X + gutter;
        var sbX = X + Width - 1;
        var idx = y - Y + ScrollOffset;
        if (idx < 0 || idx >= Lines.Count)
        {
            ClearSelection();
            return true;
        }
        if (ShowScrollBar && x == sbX)
        {
            _scrollBarDragging = true;
            ScrollToRatio(y);
            return true;
        }
        if (x < codeX)
        {
            CursorLine = idx;
            OnGutterClick?.Invoke(idx);
            return true;
        }
        CursorLine = idx;
        CursorCol = Math.Clamp(x - codeX, 0, Lines[idx].Length);
        ClearSelection();
        return true;
    }

    /// <summary>Scroll proporzionale alla posizione verticale y (per la scrollbar).</summary>
    private void ScrollToRatio(int y)
    {
        var total = Math.Max(1, Lines.Count);
        var page = Math.Max(1, Height);
        var maxPos = Math.Max(0, total - page);
        if (maxPos <= 0) return;
        var clamped = Math.Clamp(y - Y, 0, Height - 1);
        ScrollOffset = Math.Min(maxPos, clamped * maxPos / Math.Max(1, Height - 1));
    }

    public override bool HandleMouse(MouseAction action)
    {
        if (!Enabled) return false;
        switch (action.Kind)
        {
            case MouseActionKind.ScrollUp:
                ScrollUp();
                return true;
            case MouseActionKind.ScrollDown:
                ScrollDown();
                return true;
            case MouseActionKind.Click or MouseActionKind.DoubleClick or MouseActionKind.TripleClick or MouseActionKind.Drag or MouseActionKind.Release:
                var inside = action.X >= X && action.X < X + Width && action.Y >= Y && action.Y < Y + Height;
                if (!inside) return false;
                if (action.Kind == MouseActionKind.Drag)
                {
                    if (_scrollBarDragging)
                    {
                        ScrollToRatio(action.Y);
                        return true;
                    }
                    var idx = action.Y - Y + ScrollOffset;
                    if (idx >= 0 && idx < Lines.Count)
                    {
                        if (CursorLine >= 0)
                            SetSelection(SelectionAnchor, (idx, Math.Clamp(action.X - (X + GutterWidth), 0, Lines[idx].Length)));
                        CursorLine = idx;
                    }
                    return true;
                }
                if (action.Kind == MouseActionKind.Release)
                {
                    _scrollBarDragging = false;
                    return true;
                }
                if (action.Kind == MouseActionKind.Click)
                {
                    var shift = (action.Modifiers & KeyModifiers.Shift) != 0;
                    var keepAnchor = shift && SelectionEnd.HasValue;
                    var anchor = SelectionAnchor;
                    HandleClick(action.X, action.Y);
                    if (keepAnchor) SetSelection(anchor, (CursorLine, CursorCol));
                    else SelectionAnchor = (CursorLine, CursorCol);
                    return true;
                }
                if (action.Kind == MouseActionKind.DoubleClick)
                {
                    HandleClick(action.X, action.Y);
                    var ci = Math.Clamp(action.X - (X + GutterWidth), 0, Lines[CursorLine].Length);
                    var line = Lines[CursorLine];
                    var ws = 0; while (ws < line.Length && line[ws] == ' ') ws++;
                    int s = ci, e = ci;
                    while (s > ws && char.IsLetterOrDigit(line[s - 1])) s--;
                    while (e < line.Length && char.IsLetterOrDigit(line[e])) e++;
                    if (e > s) SetSelection((CursorLine, s), (CursorLine, e));
                    return true;
                }
                if (action.Kind == MouseActionKind.TripleClick)
                {
                    HandleClick(action.X, action.Y);
                    SelectLine(CursorLine);
                    return true;
                }
                return true;
        }
        return false;
    }

    /// <summary>
    /// Format a 1-based line number right-justified on <paramref name="width"/> characters,
    /// followed by a '|' separator. Mirrors the Rust <c>format_line_number</c>:
    /// <c>FormatLineNumber(3, 5)</c> → <c>"    3|"</c>.
    /// </summary>
    public static string FormatLineNumber(int n, int width)
    {
        var s = n + "|";
        while (s.Length - 1 < width) s = " " + s;
        return s;
    }
}