// FILE: Basic.Executor.GMBasicIde.cs

using Basic.Runtime;
using ClipperLib.App;
using ClipperLib.Clipper;
using ClipperLib.Controls;
using CommonLib.Consoles;
using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;

namespace Basic.Executor;

/// <summary>
/// GMBASIC IDE — editor TUI stile QBasic/QuickVB (Enter­prise Edition).
/// Sostituisce <see cref="GWBasicEditor"/>. Layout QuickVB: TitleBar, MenuBar,
/// CodeArea (gutter con breakpoint/bookmark/curr-line, selezione, scrollbar),
/// Output Panel in fondo, StatusBar con posizione 00001:001 e FunctionKeyBar.
/// Mouse obbligatorio ovunque (menu, gutter, testo, rotate) — vincolo utente.
/// </summary>
public class GMBasicIde : IApp
{
    // ========================================================================
    // Palette QBasic (Fase 1.1 IDE-TUI-TODO)
    // ========================================================================
    private static readonly CellColor BgBlue = new() { R = 0, G = 0, B = 128 };
    private static readonly CellColor BgDeepBlue = new() { R = 0, G = 0, B = 168 };
    private static readonly CellColor BgDarkCyan = new() { R = 0, G = 128, B = 128 };
    private static readonly CellColor BgGray = new() { R = 192, G = 192, B = 192 };
    private static readonly CellColor BgOutputTrack = new() { R = 0, G = 0, B = 100 };
    private static readonly CellColor FgWhite = new() { R = 255, G = 255, B = 255 };
    private static readonly CellColor FgBlack = new() { R = 0, G = 0, B = 0 };
    private static readonly CellColor FgYellow = new() { R = 255, G = 255, B = 0 };
    private static readonly CellColor FgGray = new() { R = 192, G = 192, B = 192 };
    private static readonly CellColor FgGreen = new() { R = 0, G = 255, B = 0 };
    private static readonly CellColor FgCyan = new() { R = 0, G = 255, B = 255 };
    private static readonly CellColor FgMagenta = new() { R = 255, G = 0, B = 255 };
    private static readonly CellColor FgRed = new() { R = 255, G = 0, B = 0 };

    private const int TitleBarY = 0;
    private const int MenuBarY = 1;
    private const int OutputPanelH = 5;

    /// <summary>Righe del body dell'Output Panel (sotto la tab bar). Con la Immediate Window
    /// (Fase 8) 2 righe invece di 4 per fare spazio alla barra `▸` sopra la StatusBar.</summary>
    private int OutputPanelBodyH => _settings.ImmediateWindow ? OutputPanelH - 3 : OutputPanelH - 1;

    /// <summary>Riga superiore del pannello basso (riga dei tab).</summary>
    private int BottomPanelY => _buffer.Height - OutputPanelH;

    /// <summary>Prima riga sotto il pannello basso (esclusiva): con la Immediate Window la barra
    /// `▸` sta a Height-2, altrimenti il pannello arriva fino alla StatusBar.</summary>
    private int BottomPanelEndY => _settings.ImmediateWindow ? _buffer.Height - 2 : _buffer.Height - 1;

    private readonly SortedDictionary<int, string> _programLines;
    private readonly CodeArea _codeArea;
    private readonly Menu _menu;
    private readonly ScreenBuffer _buffer = null!;
    private string _fileName;
    private bool _dirty;
    private bool _exiting;

    // --- IDE settings (Fase 6) ---
    private readonly IdeSettings _settings;
    private DateTime _lastAutoSave = DateTime.MinValue;
    private bool _autoSaveForceSave;

    // --- Output panel ---
    private readonly object _outputLock = new();
    private readonly List<string> _output = [];
    private int _outputScroll;
    private bool _outputVisible = true;
    private bool _outputFocused;

    // --- Run state (Fase 3) ---
    private CancellationTokenSource? _runCts;
    private bool _running;

    // --- Debug state (Fase 4) ---
    // Il debug step-by-step (basato su libgmb_vm_bridge) è stato rimosso:
    // resta il flag _startStep per compatibilità con i menu.
    private bool _startStep;
    private DebugServer? _debugServer;

    // --- Debug panels (Fase 4.9 Watch / 4.10 Call Stack) ---
    private int _bottomTab;                 // 0=Output, 1=Watch, 2=Call Stack
    private int _watchScroll;
    private int _stackScroll;
    private int _stackSelection;
    private readonly List<(string Name, string Value, string Type)> _watchRows = [];
    private readonly List<(string Name, string Value)> _stackRows = [];
    private readonly List<string> _userWatches = [];        // espressioni watch aggiunte manualmente
    private int _tempBpLine = -1;                           // Run-to-Cursor: breakpoint temporaneo

    // --- Undo/Redo (Fase 9) ---
    private readonly List<UndoSnapshot> _undo = [];
    private readonly List<UndoSnapshot> _redo = [];
    private const int UndoLimit = 50;

    private sealed record UndoSnapshot(List<string> Lines, int Line, int Col, int Scroll);

    // --- Breakpoints / Bookmarks (0-based line) ---
    private readonly HashSet<int> _breakpoints = [];
    private readonly HashSet<int> _bookmarks = [];
    private readonly List<int> _bpOrder = [];

    // --- Find/Replace state ---
    private string _findTerm = "";
    private int _lastFindDir = 1;

    // --- Autocomplete (Fase 5) ---
    private List<string>? _completionMatches;
    private int _completionIndex;
    private bool _completionVisible;
    private string _completionToken = "";
    private int _completionStart;
    private HashSet<string> _completionCandidates = new(StringComparer.OrdinalIgnoreCase);

    // --- Immediate Window (Fase 8) ---
    private string _immediateInput = "";
    private bool _immediateFocused;
    private bool _immediateRunning;

    // --- Notebook kernel (Fase 0 Interactive-Notebook-Plan) ---
    // La Immediate Window diventa il veicolo REPL del notebook: ogni riga inviata è una
    // CELLA (JIT P-Code + scope persistente + cache + output per-cell + tempi). Il kernel
    // sostituisce il vecchio percorso "ricompila il programma + VM host esterno" con la
    // pipeline per-cell (piano §2.1/§6bis). Attivazione: modalità notebook ON per default
    // quando presente (retrocompatibilità: %reset riporta lo stato; il percorso legacy
    // resta per input non-BASIC o in caso di fallimento init del kernel).
    private GMBasic.Notebook.NotebookKernel? _notebookKernel;
    private int _notebookCellSeq;

    // Top-level menu accelerators (Alt+lettera → indice voce).
    private static readonly (string Letter, int Index)[] MenuAccel =
    [
        ("F", 0), ("E", 1), ("V", 2), ("S", 3), ("R", 4), ("B", 5), ("D", 6), ("O", 7), ("H", 8),
    ];

    public string Name => "GMBASIC";
    public bool CloseRequested { get; private set; }

    // Caret relativo per il renderer (riga 2 = inizio area editor).
    public (int x, int y) Caret => (_codeArea.CursorCol + 1 + _codeArea.GutterWidth, _codeArea.CursorLine - _codeArea.ScrollOffset + 2);
    public bool CursorVisible => !_menu.IsOpen && !_exiting;

    /// <summary>Registro di inizializzazione: sintassi keyword (Fase 1), condiviso con BuildSyntax.</summary>
    private static readonly string[] BasicKeywords =
        "DIM,REDIM,AS,SUB,END,FUNCTION,GENERATOR,RETURN,IF,THEN,ELSE,ELSEIF,FOR,TO,STEP,NEXT,DO,LOOP,WHILE,WEND,UNTIL,EXIT,CONTINUE,GOTO,GOSUB,ON,ERROR,RESUME,TRY,CATCH,FINALLY,PRINT,INPUT,LINEINPUT,OPEN,CLOSE,LET,CONST,LOCAL,GLOBAL,STATIC,BYREF,BYVAL,OPTIONAL,TYPE,CLASS,INHERITS,IMPLEMENTS,INTERFACE,NEW,DELETE,PUBLIC,PRIVATE,PROTECTED,VIRTUAL,OVERRIDE,IMPORT,REQUIRE,NAMESPACE,DATA,READ,RESTORE,RANDOMIZE,STOP,SWAP,RUN,DECLARE,YIELD,AND,OR,NOT,XOR,MOD,TRUE,FALSE,NULL,SELECT,CASE".Split(',');

    public GMBasicIde(SortedDictionary<int, string> programLines, string fileName)
    {
        _programLines = programLines;
        _fileName = fileName;
        _buffer = new ScreenBuffer(Math.Max(Console.WindowWidth, 80), Math.Max(Console.WindowHeight, 25));
        _settings = IdeSettings.Load();

        var t = Theme.Turbo();
        t.Text = FgGray; t.WindowBg = BgBlue; t.Accent = BgGray; t.ActiveBg = FgWhite;
        ControlStyle.SetTheme(t);

        _output.Add($"GMBASIC IDE — {Path.GetFileName(_fileName)}");

        _codeArea = new CodeArea
        {
            X = 0,
            Y = 2,
            Width = _buffer.Width,
            Height = Math.Max(1, _buffer.Height - OutputPanelH - 2),
            ShowLineNumbers = _settings.ShowLineNumbers,
            GutterColor = FgGray,
            Syntax = _settings.SyntaxHighlighting ? BuildSyntax() : new Syntax(),
            Focused = true,
            ShowScrollBar = true,
            TabWidth = Math.Clamp(_settings.TabWidth, 1, 8),
        };
        _codeArea.OnGutterClick = ToggleBreakpointAt;
        _outputVisible = _settings.ShowOutputPanel;
        if (_settings.ShowWatchPanel) _bottomTab = 1;

        _menu = BuildMenu();
        LoadProgramIntoEditor();
        LoadBreakpoints();
        _codeArea.BreakpointLines = new HashSet<int>(_breakpoints);
        _codeArea.ClearSelection();
        PushUndo();
    }

    private static Syntax BuildSyntax()
    {
        var s = Syntax.FromKeywordsWith(BasicKeywords, FgYellow);
        s.AddWord("REM", FgGreen).AddSymbol('\'', FgGreen);
        s.AddSymbol('"', FgCyan);
        return s;
    }

    /// <summary>Raccoglie gli identificatori definiti dal programma (routine, variabili globali,
    /// TYPE/CLASS/CONST) per i candidati dell'autocomplete.</summary>
    private void CollectCompletionIdentifiers()
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var raw in _codeArea.Lines)
        {
            var t = raw.AsSpan();
            var content = TrimLineNumber(ref t);
            ParseDeclaredIdentifiers(content, set);
            foreach (var kw in new[] { "SUB ", "FUNCTION ", "GENERATOR ", "TYPE ", "CLASS ", "CONST " })
            {
                var p = IndexOfIgnoreCase(content, kw);
                while (p >= 0)
                {
                    var name = ReadIdentifier(content, p + kw.Length);
                    if (name.Length > 0) set.Add(name);
                    var next = IndexOfIgnoreCase(content[(p + kw.Length)..], kw);
                    p = next < 0 ? -1 : p + kw.Length + next;
                }
            }
        }
        foreach (var kw in BasicKeywords) set.Add(kw);
        foreach (var t in BasicCompletionTypes) set.Add(t);
        foreach (var f in Enum.GetNames<global::Compiler.BuiltInFunction>()) set.Add(f);
        _completionCandidates = set;
    }

    private static readonly string[] BasicCompletionTypes =
        "STRING,INTEGER,LONG,DOUBLE,SINGLE,BOOLEAN,BYTE,DATE,MONEY,CURRENCY,OBJECT,VARIANT,UDT".Split(',');

    private static ReadOnlySpan<char> TrimLineNumber(ref ReadOnlySpan<char> t)
    {
        var s = t.TrimStart();
        var n = 0;
        while (n < s.Length && s[n] >= '0' && s[n] <= '9') n++;
        if (n > 0 && n < s.Length && s[n] == ' ') return s[(n + 1)..];
        return s;
    }

    private static void ParseDeclaredIdentifiers(ReadOnlySpan<char> content, HashSet<string> set)
    {
        foreach (var kw in new[] { "DIM ", "LOCAL ", "GLOBAL ", "STATIC " })
        {
            var p = IndexOfIgnoreCase(content, kw);
            while (p >= 0)
            {
                var name = ReadIdentifier(content, p + kw.Length);
                if (name.Length > 0) set.Add(name);
                var next = IndexOfIgnoreCase(content[(p + kw.Length)..], kw);
                p = next < 0 ? -1 : p + kw.Length + next;
            }
        }
    }

    private static int IndexOfIgnoreCase(ReadOnlySpan<char> haystack, string needle)
    {
        if (needle.Length > haystack.Length) return -1;
        for (var i = 0; i <= haystack.Length - needle.Length; i++)
        {
            var match = true;
            for (var j = 0; j < needle.Length; j++)
                if (char.ToUpperInvariant(haystack[i + j]) != needle[j]) { match = false; break; }
            if (match) return i;
        }
        return -1;
    }

    private static string ReadIdentifier(ReadOnlySpan<char> s, int start)
    {
        var i = start;
        while (i < s.Length && (char.IsLetterOrDigit(s[i]) || s[i] == '_')) i++;
        return s[start..i].ToString();
    }

    /// <summary>Identifica la parola alpha-numerica subito a sinistra del cursore.</summary>
    private (string Word, int Start) WordBeforeCursor()
    {
        var line = _codeArea.Lines[_codeArea.CursorLine];
        var col = Math.Min(_codeArea.CursorCol, line.Length);
        var start = col;
        while (start > 0 && (char.IsLetterOrDigit(line[start - 1]) || line[start - 1] == '_' || line[start - 1] == '$' || line[start - 1] == '%' || line[start - 1] == '&' || line[start - 1] == '#' || line[start - 1] == '!')) start--;
        return (line[start..col], start);
    }

    private void MaybeOpenCompletion(bool force)
    {
        if (_running || _outputFocused) return;
        if (_completionVisible)
        {
            RefreshCompletion();
            return;
        }
        var (word, start) = WordBeforeCursor();
        if (word.Length < 1) return;
        if (!force && word.Length < 2) return;
        if (word.All(char.IsDigit)) return;
        CollectCompletionIdentifiers();
        _completionToken = word;
        _completionStart = start;
        _completionMatches = MatchingCandidates(word);
        if (_completionMatches.Count == 0) return;
        _completionVisible = true;
        _completionIndex = 0;
    }

    private void RefreshCompletion()
    {
        if (!_completionVisible) return;
        var (word, start) = WordBeforeCursor();
        if (word.Length < 2 || word.All(char.IsDigit))
        {
            CloseCompletion();
            return;
        }
        _completionToken = word;
        _completionStart = start;
        var matches = MatchingCandidates(word);
        if (matches.Count == 0) { CloseCompletion(); return; }
        var prev = _completionIndex < _completionMatches!.Count ? _completionMatches[_completionIndex] : null;
        _completionMatches = matches;
        _completionIndex = prev == null ? 0 : Math.Max(0, matches.IndexOf(prev));
    }

    private List<string> MatchingCandidates(string word)
    {
        var matches = _completionCandidates
            .Where(c => c.StartsWith(word, StringComparison.OrdinalIgnoreCase))
            .OrderBy(c => string.Equals(c, word, StringComparison.OrdinalIgnoreCase) ? 0 : 1)
            .ThenBy(c => c, StringComparer.OrdinalIgnoreCase)
            .Take(20)
            .ToList();
        return matches;
    }

    private void AcceptCompletion()
    {
        if (!_completionVisible || _completionMatches == null || _completionIndex < 0 || _completionIndex >= _completionMatches.Count) return;
        var chosen = _completionMatches[_completionIndex];
        PushUndo();
        var line = _codeArea.Lines[_codeArea.CursorLine];
        _codeArea.Lines[_codeArea.CursorLine] = line.Remove(_completionStart, _completionToken.Length).Insert(_completionStart, chosen);
        _codeArea.CursorCol = _completionStart + chosen.Length;
        CloseCompletion();
        UpdateDirty();
    }

    private void CloseCompletion()
    {
        _completionVisible = false;
        _completionMatches = null;
        _completionIndex = 0;
    }

    /// <summary>Menu QuickVB-autentici + estensione Calls (Fase 1.2).</summary>
    private Menu BuildMenu()
    {
        var m = new Menu { X = 0, Y = MenuBarY, Width = Math.Max(Console.WindowWidth, 80) };
        m.Items =
        [
            new() { Label = " File ", SubItems =
            [
                new() { Label = "New Program", Hotkey = "Ctrl+N", Action = DoNew },
                new() { Label = "Open Program...", Hotkey = "Ctrl+O", Action = DoOpen },
                new() { Label = "Merge...", Action = DoMerge },
                new() { Label = "Save", Hotkey = "Ctrl+S", Action = SaveAndClean },
                new() { Label = "Save As...", Action = DoSaveAs },
                new() { Label = "Save All", Action = SaveAndClean },
                new() { Label = "", Separator = true },
                new() { Label = "Print...", Action = DoPrint },
                new() { Label = "", Separator = true },
                new() { Label = "Exit", Hotkey = "Esc", Action = DoExit },
            ] },
            new() { Label = " Edit ", SubItems =
            [
                new() { Label = "Undo", Hotkey = "Alt+BS", Action = DoUndo },
                new() { Label = "Redo", Hotkey = "Ctrl+Y", Action = DoRedo },
                new() { Label = "", Separator = true },
                new() { Label = "Cut", Hotkey = "Shift+Del", Action = DoCut },
                new() { Label = "Copy", Hotkey = "Ctrl+Ins", Action = DoCopy },
                new() { Label = "Paste", Hotkey = "Shift+Ins", Action = DoPaste },
                new() { Label = "Clear", Hotkey = "Del", Action = DoClearSelection },
                new() { Label = "", Separator = true },
                new() { Label = "New SUB...", Action = () => NewRoutine("SUB") },
                new() { Label = "New FUNCTION...", Action = () => NewRoutine("FUNCTION") },
                new() { Label = "New GENERATOR...", Action = () => NewRoutine("GENERATOR") },
                new() { Label = "", Separator = true },
                new() { Label = "Comment Block", Hotkey = "Ctrl+/", Action = () => ToggleCommentBlock(true) },
                new() { Label = "Uncomment", Hotkey = "Ctrl+Shift+/", Action = () => ToggleCommentBlock(false) },
                new() { Label = "Select All", Hotkey = "Ctrl+A", Action = DoSelectAll },
                new() { Label = "", Separator = true },
                new() { Label = "Code Completion", Hotkey = "Ctrl+Space", Action = DoCompletion },
                new() { Label = "Go to Definition", Hotkey = "F12", Action = DoGoToDef },
                new() { Label = "Find References", Hotkey = "Shift+F12", Action = DoFindRefs },
                new() { Label = "Rename...", Hotkey = "Ctrl+R", Action = DoRename },
                new() { Label = "Insert Snippet...", Action = DoSnippet },
            ] },
            new() { Label = " View  ", SubItems =
            [
                new() { Label = "SUBs...", Hotkey = "F2", Action = ShowOutline },
                new() { Label = "Next SUB", Hotkey = "Shift+F2", Action = DoNextSub },
                new() { Label = "", Separator = true },
                new() { Label = "Output Screen", Hotkey = "F4", Action = DoToggleOutput },
                new() { Label = "Line Numbers", Hotkey = "Ctrl+L", Action = ToggleLineNumbers },
                new() { Label = "Output Panel", Action = DoToggleOutput },
                new() { Label = "Watch Panel", Action = () => SetBottomTab(1) },
                new() { Label = "Call Stack", Action = () => SetBottomTab(2) },
            ] },
            new() { Label = " Search", SubItems =
            [
                new() { Label = "Find...", Hotkey = "Ctrl+F", Action = DoFind },
                new() { Label = "Selected Text", Hotkey = "Ctrl+\\", Action = () => FindSelected() },
                new() { Label = "Repeat Last Find", Hotkey = "F3", Action = () => FindNext(1) },
                new() { Label = "Change...", Hotkey = "Ctrl+H", Action = DoReplace },
                new() { Label = "Go To Line...", Hotkey = "Ctrl+G", Action = DoGoToLine },
            ] },
            new() { Label = "  Run  ", SubItems =
            [
                new() { Label = "Start", Hotkey = "Shift+F5", Action = DoRun },
                new() { Label = "Restart", Hotkey = "Ctrl+F5", Action = DoRun },
                new() { Label = "Continue", Hotkey = "F5", Action = DoRun },
                new() { Label = "", Separator = true },
                new() { Label = "Modify COMMAND$...", Action = () => Status("Not implemented") },
                new() { Label = "Make EXE File...", Action = DoMakeExe },
                new() { Label = "Make Library...", Action = DoMakeLib },
            ] },
            new() { Label = " Build ", SubItems =
            [
                new() { Label = "Build", Hotkey = "F7", Action = DoBuild },
                new() { Label = "Rebuild", Hotkey = "Ctrl+F7", Action = DoBuild },
                new() { Label = "Clean", Action = DoClean },
                new() { Label = "Error List", Hotkey = "Ctrl+E", Action = DoErrorList },
                new() { Label = "", Separator = true },
                new() { Label = "Target: P-Code", Action = () => Status("Target: P-Code") },
                new() { Label = "Configuration: Debug", Action = () => Status("Configuration: Debug") },
            ] },
            new() { Label = "Debug ", SubItems =
            [
                new() { Label = "Toggle Breakpoint", Hotkey = "F9", Action = ToggleBreakpointAtCursor },
                new() { Label = "Clear All Breakpoints", Action = ClearAllBreakpoints },
                new() { Label = "", Separator = true },
                new() { Label = "Step Into", Hotkey = "F8", Action = DoStepInto },
                new() { Label = "Step Over", Hotkey = "F10", Action = DoStepOver },
                new() { Label = "Step Out", Hotkey = "Shift+F10", Action = DoStepOut },
                new() { Label = "Run to Cursor", Hotkey = "Ctrl+F8", Action = DoRunToCursor },
                new() { Label = "", Separator = true },
                new() { Label = "Add Watch...", Action = DoAddWatch },
                new() { Label = "Delete All Watch", Action = () => { _userWatches.Clear(); RefreshDebugPanels(); Status("All watch deleted"); } },
                new() { Label = "Immediate...", Action = () => { _immediateFocused = true; _codeArea.Focused = false; Status("Immediate: digita l'espressione, Enter valuta, ESC esce"); } },
                new() { Label = "", Separator = true },
                new() { Label = "Watch Panel", Action = () => SetBottomTab(1) },
                new() { Label = "Call Stack", Action = () => SetBottomTab(2) },
            ] },
            new() { Label = "Opts  ", SubItems =
            [
                new() { Label = "Options...", Action = DoOptions },
                new() { Label = "Theme...", Action = DoOptions },
            ] },
            new() { Label = " Help ", SubItems =
            [
                new() { Label = "Index", Hotkey = "F1", Action = () => DoAbout() },
                new() { Label = "Help on Help", Hotkey = "Shift+F1", Action = () => DoAbout() },
                new() { Label = "", Separator = true },
                new() { Label = "About...", Action = DoAbout },
            ] },
        ];
        m.Style = new ControlStyle { Fg = FgWhite, Bg = BgDeepBlue, SelFg = BgGray, SelBg = FgBlack };
        m.SelectedIndex = 0;
        return m;
    }

    // ========================================================================
    // Load / Save sotto forma "num testo" (stesso formato della REPL)
    // ========================================================================
    private void LoadProgramIntoEditor()
    {
        _codeArea.Lines.Clear();
        foreach (var (num, line) in _programLines) _codeArea.Lines.Add($"{num} {line}");
        if (_codeArea.Lines.Count == 0) _codeArea.Lines.Add("");
    }

    private void SaveEditorIntoProgram()
    {
        _programLines.Clear();
        foreach (var raw in _codeArea.Lines)
        {
            var sp = raw.IndexOf(' ');
            if (sp > 0 && int.TryParse(raw[..sp], out var num) && !string.IsNullOrWhiteSpace(raw[(sp + 1)..]))
                _programLines[num] = raw[(sp + 1)..];
            else if (!string.IsNullOrWhiteSpace(raw) && !int.TryParse(raw.Trim(), out _))
            {
                var n = _programLines.Count > 0 ? _programLines.Keys.Max() + 10 : 10;
                _programLines[n] = raw.Trim();
            }
        }
    }

    private string BuildFileText() => string.Join(Environment.NewLine,
        _programLines.Select(kv => $"{kv.Key} {kv.Value}"));

    private void SaveFileClean()
    {
        SaveEditorIntoProgram();
        try
        {
            if (_settings.BackupOnSave && File.Exists(_fileName))
                File.Copy(_fileName, _fileName + ".bak", overwrite: true);
            File.WriteAllText(_fileName, BuildFileText());
            _dirty = false;
            SaveBreakpoints();
            Status($"Saved {Path.GetFileName(_fileName)}" + (_settings.BackupOnSave ? " (+ .bak)" : ""));
        }
        catch (Exception ex) { Status($"Save error: {ex.Message}"); }
    }

    private void SaveAndClean() => SaveFileClean();

    // ========================================================================
    // Feature IDE aggiuntive (Next SUB / Merge / Print / Clean)
    // ========================================================================

    /// <summary>True se la riga (con eventuale numero BASIC in testa) apre una routine.</summary>
    private static bool IsRoutineLine(string raw)
    {
        var s = raw.TrimStart();
        int sp = s.IndexOf(' ');
        if (sp > 0 && int.TryParse(s[..sp], out _)) s = s[(sp + 1)..].TrimStart();
        return s.StartsWith("SUB ", StringComparison.OrdinalIgnoreCase)
            || s.StartsWith("SUB(") || s.StartsWith("SUB (")
            || s.StartsWith("FUNCTION ", StringComparison.OrdinalIgnoreCase)
            || s.StartsWith("GENERATOR ", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Salta alla prossima SUB/FUNCTION/GENERATOR (Shift+F2).</summary>
    private void DoNextSub()
    {
        for (int i = _codeArea.CursorLine + 1; i < _codeArea.Lines.Count; i++)
            if (IsRoutineLine(_codeArea.Lines[i]))
            {
                _codeArea.CursorLine = i;
                _codeArea.CursorCol = 0;
                if (_codeArea.CursorLine < _codeArea.ScrollOffset) _codeArea.ScrollOffset = _codeArea.CursorLine;
                Status($"→ {_codeArea.Lines[i].Trim()}");
                return;
            }
        Status("No next SUB");
    }

    /// <summary>Unisce un altro file .bas nell'editor (accoda le sue righe).</summary>
    private void DoMerge()
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(_fileName)) ?? ProjectDir;
        var files = Directory.GetFiles(dir, "*.bas").Select(Path.GetFileName).Where(f => f != null).Cast<string>().Distinct().ToList();
        if (files.Count == 0) { Status("No .bas files in " + dir); return; }
        var picked = PaginatedMenu.Select(files, "Merge .bas file");
        if (picked == null) return;
        try
        {
            PushUndo();
            var text = File.ReadAllLines(Path.Combine(dir, picked));
            foreach (var l in text) _codeArea.Lines.Add(l);
            if (_codeArea.Lines.Count > 0 && _codeArea.Lines[^1].Length > 0) _codeArea.Lines.Add("");
            MarkDirty();
            Status($"Merged {picked} ({text.Length} lines)");
        }
        catch (Exception ex) { Status($"Merge error: {ex.Message}"); }
    }

    /// <summary>Stampa il programma sul dispositivo (printer virtuale del runtime C).</summary>
    private void DoPrint()
    {
        try
        {
            SaveEditorIntoProgram();
            var g = Basic.IlBackend.IlRuntime.Current.Graphics;
            foreach (var kv in _programLines) g.ClipperPrinterWrite($"{kv.Key} {kv.Value}", true);
            g.ClipperPrinterEject();
            Status($"Printed {_programLines.Count} lines ({g.ClipperPrinterText().Length} byte)");
        }
        catch (Exception ex) { Status($"Print error: {ex.Message}"); }
    }

    /// <summary>Rimuove gli artefatti di build (out/).</summary>
    private void DoClean()
    {
        try
        {
            var bdir = Path.Combine(ProjectDir, "out");
            if (Directory.Exists(bdir)) { Directory.Delete(bdir, recursive: true); Status("Clean: rimossi gli artefatti in out/"); }
            else Status("Clean: nulla da rimuovere");
        }
        catch (Exception ex) { Status($"Clean error: {ex.Message}"); }
    }

    // ========================================================================
    // Feature IDE (nuovo modello C: gmb_ide) — outline/completion/goto/refs/
    // lint/rename/snippet/make. Il codice è analizzato dal runtime C.
    // ========================================================================
    private void SyncIdeToC()
    {
        if (Basic.Runtime.CIdeNative.TabCount() <= 0)
            _activeTab = Basic.Runtime.CIdeNative.TabOpen(Path.GetFileName(_fileName), string.Join('\n', _codeArea.Lines));
        else
            Basic.Runtime.CIdeNative.SetText(_activeTab, string.Join('\n', _codeArea.Lines));
    }

    private void JumpToLine0(int line0)
    {
        if (line0 < 0 || line0 >= _codeArea.Lines.Count) return;
        _codeArea.CursorLine = line0;
        _codeArea.CursorCol = 0;
        if (_codeArea.CursorLine < _codeArea.ScrollOffset) _codeArea.ScrollOffset = _codeArea.CursorLine;
    }

    /// <summary>View → Outline: elenca SUB/FUNCTION/GENERATOR/TYPE e salta a quella scelta.</summary>
    private void DoOutline()
    {
        SyncIdeToC();
        var rows = new List<string>();
        var map = new List<int>();
        foreach (var t in Basic.Runtime.CIdeNative.Outline().Split(';', StringSplitOptions.RemoveEmptyEntries))
        {
            var p = t.Split('|');
            if (p.Length >= 3 && int.TryParse(p[0], out var ln))
            { rows.Add($"{ln + 1,5}  {p[1],-10} {p[2]}"); map.Add(ln); }
        }
        if (rows.Count == 0) { Status("Outline: nessun simbolo"); return; }
        var picked = PaginatedMenu.Select(rows, "Outline (SUBs/Functions)");
        if (picked != null) { var i = rows.IndexOf(picked); if (i >= 0) JumpToLine0(map[i]); }
    }

    // Stato popup completion inline
    private bool _popupActive;
    private int _popupStart;
    private int _popupLine;
    private readonly List<string> _popupCandidates = new();
    private int _popupIdx = -1;

    /// <summary>Edit → Code Completion: POPUP inline (listbox C) al cursore, Enter inserisce, Esc chiude.</summary>
    private void DoCompletion()
    {
        SyncIdeToC();
        var line = _codeArea.Lines[_codeArea.CursorLine];
        var col = Math.Min(_codeArea.CursorCol, line.Length);
        int b = col; while (b > 0 && (char.IsLetterOrDigit(line[b - 1]) || line[b - 1] == '_')) b--;
        var prefix = line[b..col];
        var words = Basic.Runtime.CIdeNative.Complete(prefix).Split(';', StringSplitOptions.RemoveEmptyEntries).ToList();
        if (words.Count == 0) { Status("Completion: nessun suggerimento"); return; }
        _popupCandidates.Clear(); _popupCandidates.AddRange(words);
        _popupActive = true; _popupStart = b; _popupLine = _codeArea.CursorLine;
        var g = Basic.IlBackend.IlRuntime.Current.Graphics;
        int W = ActiveConsoleIO.WindowWidth; int H = Math.Max(ActiveConsoleIO.WindowHeight, 25);
        RegisterIde(g, W, H);
        Status($"Completion: {words.Count} suggerimenti (Enter=inserisci, Esc=chiudi)");
    }

    /// <summary>Inserisce il candidato del popup al posto del prefisso digitato.</summary>
    private void InsertCompletion(string pick)
    {
        if (_popupLine < 0 || _popupLine >= _codeArea.Lines.Count) return;
        var line = _codeArea.Lines[_popupLine];
        int col = Math.Min(_codeArea.CursorCol, line.Length);
        int b = Math.Min(_popupStart, col);
        _codeArea.Lines[_popupLine] = line[..b] + pick + line[col..];
        _codeArea.CursorLine = _popupLine;
        _codeArea.CursorCol = b + pick.Length;
        MarkDirty();
        Status($"Completion: inserito {pick}");
    }

    private void ClosePopup(IGraphicsService g, int W, int H)
    {
        if (!_popupActive) return;
        _popupActive = false;
        _popupIdx = -1;
        RegisterIde(g, W, H);
    }

    /// <summary>Edit → Go to Definition.</summary>
    private void DoGoToDef()
    {
        SyncIdeToC();
        var word = _codeArea.SelectedText?.Trim();
        if (string.IsNullOrEmpty(word)) word = PromptForText("Go to definition (nome):", "");
        if (string.IsNullOrEmpty(word)) return;
        int ln = Basic.Runtime.CIdeNative.GotoDef(word);
        if (ln < 0) { Status($"Definizione di '{word}' non trovata"); return; }
        JumpToLine0(ln);
        Status($"→ definizione di {word} alla riga {ln + 1}");
    }

    /// <summary>Edit → Find References.</summary>
    private void DoFindRefs()
    {
        SyncIdeToC();
        var word = _codeArea.SelectedText?.Trim();
        if (string.IsNullOrEmpty(word)) word = PromptForText("Find references (nome):", "");
        if (string.IsNullOrEmpty(word)) return;
        var refs = Basic.Runtime.CIdeNative.FindRefs(word).Split(';', StringSplitOptions.RemoveEmptyEntries)
            .Select(int.Parse).ToList();
        if (refs.Count == 0) { Status($"Nessun riferimento per '{word}'"); return; }
        var rows = refs.Select(l => $"{l + 1,5}  {_codeArea.Lines[l].Trim()}").ToList();
        var picked = PaginatedMenu.Select(rows, $"References: {word} ({refs.Count})");
        if (picked != null) { var i = rows.IndexOf(picked); if (i >= 0) JumpToLine0(refs[i]); }
    }

    /// <summary>Build → Error List (lint/squiggle).</summary>
    private void DoErrorList()
    {
        SyncIdeToC();
        var rows = new List<string>(); var map = new List<int>();
        foreach (var t in Basic.Runtime.CIdeNative.Lint().Split(';', StringSplitOptions.RemoveEmptyEntries))
        {
            var p = t.Split('|');
            if (p.Length >= 2 && int.TryParse(p[0], out var ln)) { rows.Add($"{(ln >= 0 ? (ln + 1).ToString() : "—"),5}  {p[1]}"); map.Add(ln); }
        }
        if (rows.Count == 0) { Status("Error list: nessun errore"); return; }
        var picked = PaginatedMenu.Select(rows, "Error List");
        if (picked != null) { var i = rows.IndexOf(picked); if (i >= 0 && map[i] >= 0) JumpToLine0(map[i]); }
    }

    /// <summary>Edit → Rename (refactoring sull'identificatore).</summary>
    private void DoRename()
    {
        SyncIdeToC();
        var from = _codeArea.SelectedText?.Trim();
        if (string.IsNullOrEmpty(from)) from = PromptForText("Rename (da):", "");
        if (string.IsNullOrEmpty(from)) return;
        var to = PromptForText("Rename (a):", from);
        if (string.IsNullOrEmpty(to) || to == from) return;
        int n = Basic.Runtime.CIdeNative.Rename(from, to);
        if (n < 0) { Status("Rename rifiutato (keyword)"); return; }
        if (n == 0) { Status($"Rename: nessuna occorrenza di {from}"); return; }
        // rileggi il testo modificato dal runtime C nell'editor
        var text = Basic.Runtime.CIdeNative.DocText(0);
        _codeArea.Lines.Clear();
        _codeArea.Lines.AddRange(text.Replace("\r", "").Split('\n'));
        if (_codeArea.Lines.Count > 0 && _codeArea.Lines[^1].Length == 0) _codeArea.Lines.RemoveAt(_codeArea.Lines.Count - 1);
        if (_codeArea.Lines.Count == 0) _codeArea.Lines.Add("");
        MarkDirty();
        Status($"Rename: {n} sostituzioni");
    }

    /// <summary>Edit → Insert Snippet.</summary>
    private void DoSnippet()
    {
        var kinds = new List<string> { "SUB", "FUNCTION", "GENERATOR", "FOR", "IF", "WHILE", "DO", "SELECT", "CLASS", "TRY" };
        var picked = PaginatedMenu.Select(kinds, "Insert snippet");
        if (picked == null) return;
        var snip = Basic.Runtime.CIdeNative.Snippet(picked);
        if (string.IsNullOrEmpty(snip)) return;
        PushUndo();
        var lines = snip.Replace("\r", "").Split('\n');
        if (lines.Length > 0 && lines[^1].Length == 0) lines = lines[..^1];
        for (int i = 0; i < lines.Length; i++) _codeArea.Lines.Insert(_codeArea.CursorLine + i, lines[i]);
        MarkDirty();
        Status($"Snippet {picked} inserito");
    }

    /// <summary>Run → Make EXE File.</summary>
    private void DoMakeExe()
    {
        try
        {
            SyncIdeToC();
            var outPath = Path.Combine(ProjectDir, Path.GetFileNameWithoutExtension(_fileName) + ".a1");
            int rc = Basic.Runtime.CIdeNative.MakeBas(outPath);
            Status(rc == 0 ? $"Make EXE: {outPath}" : $"Make EXE error {rc}");
        }
        catch (Exception ex) { Status($"Make EXE error: {ex.Message}"); }
    }

    /// <summary>Run → Make Library.</summary>
    private void DoMakeLib()
    {
        try
        {
            SyncIdeToC();
            var outPath = Path.Combine(ProjectDir, Path.GetFileNameWithoutExtension(_fileName) + ".lib.a1");
            int rc = Basic.Runtime.CIdeNative.MakeLib(outPath);
            Status(rc == 0 ? $"Make Library: {outPath}" : rc == -3 ? "Make Library: il modulo ha SUB Main" : $"Make Library error {rc}");
        }
        catch (Exception ex) { Status($"Make Library error: {ex.Message}"); }
    }

    // ========================================================================
    // File operations (Fase 2)
    // ========================================================================
    private void DoNew()
    {
        if (ConfirmSaveChanges() == false) return;
        SaveEditorIntoProgram();
        _codeArea.Lines.Clear();
        _codeArea.Lines.Add("");
        _fileName = "untitled.bas";
        _dirty = false;
        _breakpoints.Clear();
        _bpOrder.Clear();
        _bookmarks.Clear();
        _codeArea.BreakpointLines = new HashSet<int>();
        PushUndo();
        Status("New program");
    }

    private void DoOpen()
    {
        if (ConfirmSaveChanges() == false) return;
        var dir = Path.GetDirectoryName(Path.GetFullPath(_fileName)) ?? ProjectDir;
        var files = Directory.GetFiles(dir, "*.bas").Select(Path.GetFileName).Where(f => f != null).Cast<string>().Distinct().ToList();
        if (files.Count == 0) { Status("No .bas files in " + dir); return; }
        var picked = PaginatedMenu.Select(files, "Open .bas file");
        if (picked == null) return;
        _fileName = Path.Combine(dir, picked);
        LoadProgramIntoEditor();
        _dirty = false;
        _breakpoints.Clear();
        _bpOrder.Clear();
        LoadBreakpoints();
        _codeArea.BreakpointLines = new HashSet<int>(_breakpoints);
        PushUndo();
        Status($"Open {picked}");
    }

    private void DoSaveAs()
    {
        SaveEditorIntoProgram();
        var dir = Path.GetDirectoryName(Path.GetFullPath(_fileName)) ?? ProjectDir;
        var files = Directory.GetFiles(dir, "*.bas").Select(Path.GetFileName).Where(f => f != null).Cast<string>().Distinct().ToList();
        var picked = PaginatedMenu.Select(files, "Save as...");
        if (picked != null)
        {
            _fileName = Path.Combine(dir, picked);
            SaveFileClean();
        }
    }

    private void DoExit() => _exiting = true;

    /// <summary>Rilascia il kernel notebook (se creato). Chiamato sull'uscita dell'IDE
    /// (DoExit/CloseRequested) per pulire lo scope e la cache per-cell.</summary>
    private void ShutdownNotebook()
    {
        try { _notebookKernel?.Reset(); _notebookKernel?.Dispose(); }
        catch { /* best-effort in uscita */ }
        _notebookKernel = null;
    }

    /// <summary>Chiede conferma se ci sono modifiche non salvate. True = prossegui.</summary>
    private bool? ConfirmSaveChanges()
    {
        if (!_dirty && !_codeArea.Modified) return true;
        var r = AlertBox.Show("Unsaved changes",
            $"{Path.GetFileName(_fileName)} has unsaved changes.\n\nSave before continuing?",
            width: 56, buttons: ["Save", "Don't Save", "Cancel"]);
        if (r == AlertResult.OK) { SaveFileClean(); return true; }
        if (r == (AlertResult)2) return true;   // Don't Save
        return false;                           // Cancel
    }

    // ========================================================================
    // Undo / Redo (Fase 9)
    // ========================================================================
    private void PushUndo()
    {
        if (_undo.Count > 0 && _undo[^1].Lines.SequenceEqual(_codeArea.Lines)) return;
        _undo.Add(new UndoSnapshot(new List<string>(_codeArea.Lines), _codeArea.CursorLine, _codeArea.CursorCol, _codeArea.ScrollOffset));
        if (_undo.Count > UndoLimit) _undo.RemoveAt(0);
        _redo.Clear();
    }

    private void DoUndo()
    {
        if (_undo.Count <= 1) return;
        _redo.Insert(0, new UndoSnapshot(new List<string>(_codeArea.Lines), _codeArea.CursorLine, _codeArea.CursorCol, _codeArea.ScrollOffset));
        var snap = _undo[^1];
        _undo.RemoveAt(_undo.Count - 1);
        RestoreSnapshot(snap, staleRollback: false);
        Status("Undo");
    }

    private void DoRedo()
    {
        if (_redo.Count == 0) return;
        var snap = _redo[0];
        _redo.RemoveAt(0);
        _undo.Add(new UndoSnapshot(new List<string>(_codeArea.Lines), _codeArea.CursorLine, _codeArea.CursorCol, _codeArea.ScrollOffset));
        RestoreSnapshot(snap, staleRollback: true);
        Status("Redo");
    }

    private void RestoreSnapshot(UndoSnapshot snap, bool staleRollback)
    {
        _codeArea.Lines.Clear();
        _codeArea.Lines.AddRange(snap.Lines);
        if (_codeArea.Lines.Count == 0) _codeArea.Lines.Add("");
        _codeArea.CursorLine = Math.Min(snap.Line, _codeArea.Lines.Count - 1);
        _codeArea.CursorCol = Math.Min(snap.Col, _codeArea.Lines[_codeArea.CursorLine].Length);
        _codeArea.ScrollOffset = Math.Min(snap.Scroll, Math.Max(0, _codeArea.Lines.Count - _codeArea.Height));
        _codeArea.ClearSelection();
        _codeArea.Modified = true;
        _dirty = true;
    }

    // ========================================================================
    // Clipboard / selezione
    // ========================================================================
    private void DoCut()
    {
        PushUndo();
        var text = _codeArea.SelectedText;
        if (text == null) return;
        TextCopy.ClipboardService.SetText(text);
        _codeArea.ReplaceSelection("");
        MarkDirty();
    }

    private void DoCopy()
    {
        var text = _codeArea.SelectedText;
        if (text != null) TextCopy.ClipboardService.SetText(text);
    }

    private void DoPaste()
    {
        PushUndo();
        var text = TextCopy.ClipboardService.GetText();
        if (string.IsNullOrEmpty(text)) return;
        if (_codeArea.HasSelection) _codeArea.ReplaceSelection(text);
        else
        {
            var lines = text.Split('\n');
            var idx = _codeArea.CursorCol;
            _codeArea.Lines[_codeArea.CursorLine] = _codeArea.Lines[_codeArea.CursorLine].Insert(idx, text);
            _codeArea.CursorCol += text.Length;
            _codeArea.Modified = true;
        }
        MarkDirty();
    }

    private void DoClearSelection()
    {
        if (!_codeArea.HasSelection) return;
        PushUndo();
        _codeArea.ReplaceSelection("");
        MarkDirty();
    }

    private void DoSelectAll()
    {
        if (_codeArea.Lines.Count == 0) return;
        _codeArea.SetSelection((0, 0), (_codeArea.Lines.Count - 1, _codeArea.Lines[^1].Length));
    }

    private void ToggleCommentBlock(bool comment)
    {
        var r = _codeArea.SelectionRange;
        if (!r.HasValue) return;
        PushUndo();
        for (var i = r.Value.from; i <= r.Value.to && i < _codeArea.Lines.Count; i++)
        {
            var line = _codeArea.Lines[i];
            if (comment)
            {
                if (!line.TrimStart().StartsWith("'")) InsertCommentAt(i);
            }
            else if (line.TrimStart().StartsWith("'"))
            {
                var sp = line.IndexOf("'");
                _codeArea.Lines[i] = line.Remove(sp, 1);
            }
        }
        MarkDirty();
    }

    private void InsertCommentAt(int i)
    {
        var line = _codeArea.Lines[i];
        _codeArea.Lines[i] = ' ' + line;
    }

    private void NewRoutine(string kind)
    {
        PushUndo();
        _codeArea.Lines.Add("");
        _codeArea.Lines.Add($"{kind} New{kind}()");
        _codeArea.Lines.Add("");
        _codeArea.Lines.Add($"END {kind}");
        _codeArea.Lines.Add("");
        _codeArea.CursorLine = _codeArea.Lines.Count - 2;
        _codeArea.CursorCol = 0;
        MarkDirty();
        Status($"New {kind.ToLowerInvariant()} added");
    }

    // ========================================================================
    // Find / Replace / GoTo (Fase 2)
    // ========================================================================
    private void DoFind()
    {
        _findTerm = PromptForText("Find...", _codeArea.SelectedText ?? _findTerm);
        if (_findTerm.Length == 0) return;
        FindNext(1);
    }

    private void FindSelected()
    {
        var sel = _codeArea.SelectedText;
        if (string.IsNullOrWhiteSpace(sel)) return;
        _findTerm = sel.Trim();
        FindNext(1);
    }

    private void DoReplace()
    {
        var term = PromptForText("Find...", _findTerm);
        if (term.Length == 0) return;
        var repl = PromptForText("Replace with...", "");
        if (repl.Length == 0) return;
        _findTerm = term;
        var found = FindNext(1);
        if (found)
        {
            if (Confirm($"Replace this occurrence with \"{repl}\"?") == true)
            {
                PushUndo();
                _codeArea.ReplaceSelection(repl);
                MarkDirty();
            }
        }
        else Status("No match for \"" + term + "\"");
    }

    private void DoGoToLine()
    {
        var line = PromptForText("Go to line (1-based)", (_codeArea.CursorLine + 1).ToString());
        if (int.TryParse(line, out var n) && n >= 1 && n <= _codeArea.Lines.Count)
        {
            _codeArea.CursorLine = n - 1;
            _codeArea.CursorCol = 0;
            if (_codeArea.CursorLine < _codeArea.ScrollOffset || _codeArea.CursorLine >= _codeArea.ScrollOffset + _codeArea.Height)
                _codeArea.ScrollOffset = Math.Clamp(_codeArea.CursorLine - _codeArea.Height / 2, 0, Math.Max(0, _codeArea.Lines.Count - _codeArea.Height));
        }
        else Status("Invalid line number");
    }

    /// <summary>Cerca <see cref="_findTerm"/> a partire dal cursore. True se trovato.</summary>
    private bool FindNext(int dir)
    {
        if (_findTerm.Length == 0) return false;
        var startLine = _codeArea.CursorLine;
        var startCol = _codeArea.CursorCol + (dir > 0 ? 1 : 0);
        int total = _codeArea.Lines.Count;

        for (var step = 0; step < total; step++)
        {
            var line = mod(startLine + (dir > 0 ? step : -step), total);
            var col = (step == 0) ? startCol : (dir > 0 ? 0 : _codeArea.Lines[line].Length);
            if (dir > 0)
            {
                var idx = _codeArea.Lines[line].IndexOf(_findTerm, col, StringComparison.OrdinalIgnoreCase);
                if (idx >= 0)
                {
                    JumpTo(line, idx, idx + _findTerm.Length);
                    return true;
                }
            }
            else
            {
                var idx = LastIndexOf(_codeArea.Lines[line], _findTerm, col);
                if (idx >= 0)
                {
                    JumpTo(line, idx, idx + _findTerm.Length);
                    return true;
                }
            }
        }
        Status($"No match for \"{_findTerm}\"");
        return false;
    }

    private static int mod(int a, int m) => ((a % m) + m) % m;

    private static int LastIndexOf(string s, string t, int from)
    {
        if (from > s.Length) from = s.Length;
        for (var i = from - t.Length; i >= 0; i--)
            if (string.Compare(s, i, t, 0, t.Length, StringComparison.OrdinalIgnoreCase) == 0) return i;
        return -1;
    }

    private void JumpTo(int line, int selStart, int selEnd)
    {
        _codeArea.CursorLine = line;
        _codeArea.CursorCol = selStart;
        _codeArea.SetSelection((line, selStart), (line, selEnd));
        if (line < _codeArea.ScrollOffset || line >= _codeArea.ScrollOffset + _codeArea.Height)
            _codeArea.ScrollOffset = Math.Clamp(line - _codeArea.Height / 2, 0, Math.Max(0, _codeArea.Lines.Count - _codeArea.Height));
    }

    // ========================================================================
    // Breakpoints (Fase 4.1) / Bookmarks
    // ========================================================================
    private void ToggleBreakpointAt(int line)
    {
        if (_breakpoints.Contains(line)) { _breakpoints.Remove(line); _bpOrder.Remove(line); }
        else AddBreakpoint(line);
        Status(_breakpoints.Contains(line) ? $"Breakpoint at line {line + 1}" : $"Breakpoint cleared at line {line + 1}");
    }

    private void ToggleBreakpointAtCursor() => ToggleBreakpointAt(_codeArea.CursorLine);

    private void AddBreakpoint(int line)
    {
        _breakpoints.Add(line);
        _bpOrder.Add(line);
        _codeArea.BreakpointLines = new HashSet<int>(_breakpoints);
    }

    private void ClearAllBreakpoints()
    {
        _breakpoints.Clear();
        _bpOrder.Clear();
        _codeArea.BreakpointLines = new HashSet<int>();
        Status("All breakpoints cleared");
    }

    /// <summary>Persistenza breakpoint (Fase 4.11): file `<nome>.gmb.breakpoints` accanto al sorgente,
    /// una riga per breakpoint (linea 1-based). Salvato a ogni save del programma.</summary>
    private string BreakpointsFilePath => Path.ChangeExtension(_fileName, ".gmb.breakpoints");

    private void SaveBreakpoints()
    {
        if (_breakpoints.Count == 0)
        {
            try { if (File.Exists(BreakpointsFilePath)) File.Delete(BreakpointsFilePath); } catch { }
            return;
        }
        try
        {
            File.WriteAllLines(BreakpointsFilePath, _breakpoints.Select(b => (b + 1).ToString()));
        }
        catch (Exception ex) { Status($"Breakpoints save error: {ex.Message}"); }
    }

    /// <summary>Carica i breakpoint persistenti (righe 1-based nel file) e li converte in linee 0-based
    /// interne. Le linee oltre il buffer sorgente vengono scartate.</summary>
    private void LoadBreakpoints()
    {
        try
        {
            if (!File.Exists(BreakpointsFilePath)) return;
            foreach (var raw in File.ReadAllLines(BreakpointsFilePath))
            {
                if (!int.TryParse(raw.Trim(), out var oneBased)) continue;
                var line = oneBased - 1;
                if (line < 0 || line >= _codeArea.Lines.Count) continue;
                if (_breakpoints.Add(line)) _bpOrder.Add(line);
            }
        }
        catch (Exception ex) { Status($"Breakpoints load error: {ex.Message}"); }
    }

    private void ToggleBookmarkAt(int line)
    {
        if (!_bookmarks.Add(line)) _bookmarks.Remove(line);
        _codeArea.BookmarkLines = new HashSet<int>(_bookmarks);
        Status(_bookmarks.Contains(line) ? $"Bookmark at line {line + 1}" : "Bookmark removed");
    }

    // ========================================================================
    // Outline (F2 / SUBs)
    // ========================================================================
    private void ShowOutline()
    {
        var entries = new List<string>();
        for (var i = 0; i < _codeArea.Lines.Count; i++)
        {
            var t = _codeArea.Lines[i].Trim();
            var lineNo = LineNumberAt(i);
            foreach (var kw in new[] { "SUB ", "FUNCTION ", "GENERATOR ", "TYPE ", "CLASS ", "CONST ", "DECLARE " })
            {
                if (t.StartsWith(kw, StringComparison.OrdinalIgnoreCase))
                {
                    entries.Add($"{lineNo,5}  {t}");
                    break;
                }
            }
        }
        if (entries.Count == 0) { Status("No SUB/FUNCTION/TYPE found"); return; }
        var picked = PaginatedMenu.Select(entries, $"SUBs — {entries.Count} routines");
        if (picked == null) return;
        var no = picked.TrimStart().Split(' ')[0];
        if (int.TryParse(no, out var targetNo))
        {
            for (var i = 0; i < _codeArea.Lines.Count; i++)
                if (LineNumberAt(i) == targetNo)
                {
                    JumpTo(i, 0, 0);
                    break;
                }
        }
    }

    private int LineNumberAt(int i)
    {
        var t = _codeArea.Lines[i].TrimStart();
        var sp = t.IndexOf(' ');
        return sp > 0 && int.TryParse(t[..sp], out var n) ? n : i + 1;
    }

    // ========================================================================
    // Renumber (Fase 2.0 — riuso logica REPL RENUM + update GOTO/GOSUB)
    // ========================================================================
    private void DoRenumber()
    {
        PushUndo();
        var remap = new Dictionary<int, int>();
        var newNum = 10;
        var newLines = new List<string>();
        foreach (var raw in _codeArea.Lines)
        {
            var t = raw.TrimStart();
            var sp = t.IndexOf(' ');
            int oldNum;
            if (sp > 0 && int.TryParse(t[..sp], out oldNum)) remap[oldNum] = newNum;
            newNum += 10;
        }
        var baseLine = 10;
        foreach (var raw in _codeArea.Lines)
        {
            string text;
            var t = raw.TrimStart();
            var sp = t.IndexOf(' ');
            if (sp > 0 && int.TryParse(t[..sp], out _)) text = t[(sp + 1)..];
            else text = raw;
            text = RewriteJumpTargets(text, remap);
            newLines.Add($"{baseLine} {text}");
            baseLine += 10;
        }
        _codeArea.Lines = newLines;
        _codeArea.Modified = true;
        MarkDirty();
        Status("Renumbered (GOTO/GOSUB updated)");
    }

    private void InsertLineNumber()
    {
        var next = 10;
        foreach (var raw in _codeArea.Lines)
        {
            var t = raw.TrimStart();
            var sp = t.IndexOf(' ');
            if (sp > 0 && int.TryParse(t[..sp], out var n)) next = Math.Max(next, n + 10);
        }
        _codeArea.Lines.Insert(_codeArea.CursorLine + 1, $"{next} ");
        _codeArea.CursorLine++;
        _codeArea.CursorCol = $"{next} ".Length;
        _codeArea.Modified = true;
        MarkDirty();
    }

    /// <summary>Riscrive i target dei salti (GOTO, GOSUB, THEN, ELSE, RESUME, RESTORE, RUN, RETURN, ON..GOTO) col nuovo numero.</summary>
    private static string RewriteJumpTargets(string text, Dictionary<int, int> remap)
    {
        if (remap.Count == 0) return text;
        return System.Text.RegularExpressions.Regex.Replace(text, @"\b(GOTO|GOSUB|THEN|ELSE|RESUME|RESTORE|RUN|RETURN)\s+(\d+)\b",
            m => $"{m.Groups[1].Value} " + (remap.TryGetValue(int.Parse(m.Groups[2].Value), out var n) ? n : m.Groups[2].Value),
            System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    }

    // ========================================================================
    // Build / Output panel (Fase 3)
    // ========================================================================
    private void DoBuild()
    {
        SaveEditorIntoProgram();
        Output($"Build {Path.GetFileName(_fileName)}...");
        try
        {
            // Compiles the program in memory (no license gate exists: the software runs as-is, PUL).
            var tmp = Path.Combine(Path.GetTempPath(), "gmb_ide_" + Guid.NewGuid().ToString("N") + ".bas");
            File.WriteAllText(tmp, BuildFileText());
            try
            {
                _ = Orchestrator.Compile(tmp, null, verbose: false);
                Output($"Build: OK ({(_breakpoints.Count > 0 ? _breakpoints.Count + " breakpoints set" : "0 errors, 0 warnings")})");
                _outputScroll = 0;
            }
            finally { try { File.Delete(tmp); } catch { } }
        }
        catch (Exception ex)
        {
            Output("Build error: " + ex.Message);
            Status("Build FAILED");
            return;
        }
        Status("Build: OK");
    }

    /// <summary>Esegue il programma dal buffer dell'editor: compila via Orchestrator +
    /// Compiler (P-CODE), poi lancia la VM su un task in background e ne consegna la stdout
    /// nell'Output panel. ESC durante l'esecuzione la interrompe.</summary>
    private void DoRun()
    {
        if (_running)
        {
            if (_debugServer != null && _debugServer.IsPaused)
            {
                _debugServer.Continue();
                Status("Continue");
                return;
            }
            Output("Run: già in esecuzione (premi ESC per interrompere).");
            return;
        }
        SaveEditorIntoProgram();
        Output($"Run {Path.GetFileName(_fileName)}...");
        _outputVisible = true;
        _outputFocused = true;
        _outputScroll = 0;

        _runCts = new CancellationTokenSource();
        var ct = _runCts.Token;
        var source = BuildFileText();
        _running = true;
        Task.Run(() => RunProgramCore(source, ct));
    }

    private void RunProgramCore(string source, CancellationToken ct)
    {
        try
        {
            // Compila dal buffer in memoria: CompileFromString salta il PreProcessProject
            // (la scansione della dir temp fallirebbe su dir quarantine di macOS).
            var program = Orchestrator.CompileFromString(source, _fileName, null, verbose: false);
            if (program?.MainModule == null)
            {
                Output("Compilation failed.");
                return;
            }
            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);

            if (ct.IsCancellationRequested) return;

            // DebugServer unificato: il driver per il backend P-Code/ILBackend usa la
            // PCodeVM interna. L'IDE parla solo con l'interfaccia comune; il driver
            // esegue il chunk in background e si ferma sui breakpoint/step.
            var ds = DebugServer.Create(DebugBackend.PCode);
            ds.Start(chunk, _breakpoints.Select(b => b).ToList());
            if (_tempBpLine >= 0) ds.SetBreakpoint(_tempBpLine + 1);
            if (_startStep) ds.StepIn(); // step iniziale: ferma alla prima riga
            _debugServer = ds;

            try
            {
                // Loop di esecuzione: gira sul thread del driver. Il driver blocca su
                // ogni breakpoint/step; qui si legge output e pannelli e si attende il
                // Continue/Step dell'utente (che rilasciano il task).
                while (_debugServer != null && !ct.IsCancellationRequested)
                {
                    var outChunk = ds.TakeOutput();
                    if (!string.IsNullOrEmpty(outChunk))
                        Output(outChunk);
                    RefreshDebugPanels();
                    if (!ds.IsPaused)
                    {
                        // Non in pausa: se il task è finito, usciamo; altrimenti attendi
                        // che arrivi la prossima pausa (breakpoint/step) o la fine.
                        if (!ds.IsRunning)
                            break;
                    }
                    Thread.Sleep(30);
                }
                // svuota l'output residuo
                var tail = ds.TakeOutput();
                if (!string.IsNullOrEmpty(tail))
                    Output(tail);
            }
            finally
            {
                _debugServer?.Stop();
                _debugServer = null;
                _startStep = false;
                _tempBpLine = -1;
            }

            if (!ct.IsCancellationRequested)
            {
                Output("Run: OK — execution completed.");
            }
        }
        catch (OperationCanceledException)
        {
            Output("Run cancelled.");
        }
        catch (Exception ex)
        {
            Output("Run error: " + ex.Message);
        }
        finally
        {
            _running = false;
        }
    }

    /// <summary>Valuta un'espressione BASIC nella Immediate Window (Fase 8) in background.
    /// Riusa lo stesso flusso del Run (CompileFromString + Compiler + VM/C in-process).</summary>
    private void EvalImmediate()
    {
        var expr = _immediateInput.Trim();
        if (expr.Length == 0) return;
        if (_immediateRunning)
        {
            Output("Immediate: ancora in esecuzione...");
            return;
        }
        _outputVisible = true;
        Output($"▸ {expr}");
        _immediateInput = "";
        _immediateRunning = true;

        // Notebook (Fase 0): la riga è una cella del kernel — JIT P-Code per-cell con scope
        // persistente (niente ricompilazione completa né processo VM esterno). Il risultato
        // (output + eventuale valore + tempi compile/exec) va all'Output panel.
        var kernel = GetOrCreateNotebookKernel();
        if (kernel != null)
        {
            // Kernel path: il sorgente va GREZZO (niente NormalizeImmediate — il kernel
            // riconosce statement e celle-espressione da solo e NE MANTIENE il valore
            // per `_`; "PRINT x" resta stampa, "x" diventa valore).
            var id = "imm" + ++_notebookCellSeq;
            var r = kernel.Execute(id, expr);
            var ms = (r.CompileMs + r.ExecuteMs).TotalMilliseconds;
            if (r.Ok)
            {
                var valueSuffix = r.Value is not null ? $"  = {FormatCell(r.Value)}" : "";
                var cacheSuffix = r.FromCache ? " [cached]" : "";
                if (r.Output.Length > 0) Output(r.Output.TrimEnd());
                Output($"✓ {ms:F1}ms{cacheSuffix}{valueSuffix}");
            }
            else
            {
                Output($"✗ {r.Error}");
            }
            _immediateRunning = false;
            return;
        }

        // Fallback legacy: kernel non disponibile (init fallito) → percorso storico
        // (con NormalizeImmediate: "?" → PRINT ecc.).
        var source = NormalizeImmediate(expr);
        var ct = _runCts?.Token ?? CancellationToken.None;
        Task.Run(() => EvalImmediateCore(source, ct));
    }

    /// <summary>Ottiene il kernel del notebook creandolo al primo uso (lazy). Null se la
    /// creazione fallisce: in quel caso la Immediate Window usa il percorso legacy.</summary>
    private GMBasic.Notebook.NotebookKernel? GetOrCreateNotebookKernel()
    {
        if (_notebookKernel != null) return _notebookKernel;
        try { return _notebookKernel = new GMBasic.Notebook.NotebookKernel(); }
        catch (Exception ex) { Output("Notebook kernel non disponibile: " + ex.Message); return null; }
    }

    private static string FormatCell(object? v) => v switch
    {
        null => "NIL",
        bool b => b ? "TRUE" : "FALSE",
        double d => d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture),
        string s => s,
        _ => v.ToString() ?? ""
    };

    /// <summary>Converte l'input dell'Immediate Window in un programma BASIC valido:
    /// `?` → PRINT, espressione nuda → PRINT, istruzioni (LET/DIM/CALL/...) → come sono.</summary>
    private static string NormalizeImmediate(string input)
    {
        if (input.StartsWith('?')) return "PRINT " + input[1..].TrimStart();
        var first = input.Split([' ', '\t'], 2)[0].ToUpperInvariant();
        var isStatement = first switch
        {
            "PRINT" or "LET" or "DIM" or "REDIM" or "CALL" or "CLS" or "LOCATE" or "COLOR" or
            "BEEP" or "SLEEP" or "DELAY" or "GOTO" or "GOSUB" or "RETURN" or "END" or "STOP" or
            "INPUT" or "READ" or "DATA" or "RESTORE" or "OPEN" or "CLOSE" or "SEEK" or "CLEAR" or
            "RANDOMIZE" or "DRAW" or "BLOAD" or "BSAVE" or "SOUND" or "REM" or "'" => true,
            _ => false,
        };
        return isStatement ? input : "PRINT " + input;
    }

    private void EvalImmediateCore(string source, CancellationToken ct)
    {
        try
        {
            var program = Orchestrator.CompileFromString(source, "immediate.bas", null, verbose: false);
            if (program?.MainModule == null)
            {
                Output("Immediate: compilation failed.");
                return;
            }
            var compiler = new global::Compiler.Compiler();
            var chunk = compiler.Compile(program);
            if (ct.IsCancellationRequested) return;
            var output = new System.Text.StringBuilder();
            var exitCode = GmbVmHostRunner.Run(chunk, ct,
                onOutLine: line => output.AppendLine(line),
                onErrLine: line => output.AppendLine(line));
            Output(output.ToString());
            if (exitCode != 0)
                Output("[IMMEDIATE ERROR] exit code " + exitCode);
        }
        catch (OperationCanceledException)
        {
            Output("Immediate: annullato.");
        }
        catch (Exception ex)
        {
            Output("Immediate error: " + ex.Message);
        }
        finally
        {
            _immediateRunning = false;
        }
    }

    /// <summary>TextWriter che convoglia la stdout del programma / host headless verso l'Output panel,
    /// riga per riga (thread-safe: Output aggancia su _outputLock).</summary>
    private sealed class OutputPanelWriter : TextWriter
    {
        private readonly Action<string> _emit;
        private readonly StringBuilder _sb = new();

        public OutputPanelWriter(Action<string> emit) => _emit = emit;
        public override System.Text.Encoding Encoding => System.Text.Encoding.UTF8;

        public override void Write(char value)
        {
            if (value == '\n')
            {
                var line = _sb.ToString().TrimEnd('\r');
                _sb.Clear();
                if (line.Length > 0) _emit(line);
            }
            else _sb.Append(value);
        }

        public override void Write(string? value)
        {
            if (value == null) return;
            foreach (var c in value) Write(c);
        }

        protected override void Dispose(bool disposing)
        {
            if (_sb.Length > 0)
            {
                _emit(_sb.ToString());
                _sb.Clear();
            }
            base.Dispose(disposing);
        }
    }

    private void Output(string text)
    {
        lock (_outputLock)
        {
            _output.Add(text);
            if (_output.Count > 500) _output.RemoveRange(0, _output.Count - 500);
            _outputScroll = _output.Count;
        }
    }

    private void DoToggleOutput() { _outputVisible = !_outputVisible; Status(_outputVisible ? "Output panel shown" : "Output panel hidden"); }

    private void DoAbout()
    {
        AlertBox.Show("GMBASIC IDE",
            "GMBASIC 1.0.0\nQBasic/QuickVB-style TUI editor\n\nPress Enter or ESC to continue.",
            width: 60, align: AlertAlign.Center);
    }

    // ========================================================================
    // Prompt / Confirm helpers
    // ========================================================================
    private string PromptForText(string title, string initial)
    {
        var result = "";
        var input = new TextInput { Text = initial, Focused = true };
        input.Cursor = initial.Length;

        var boxW = 62;
        var boxH = 7;
        var bx = Math.Max(0, (_buffer.Width - boxW) / 2);
        var by = Math.Max(0, (_buffer.Height - boxH) / 2);
        var label = new Label { X = bx + 2, Y = by + 2, Width = boxW - 4, Text = title };
        input.X = bx + 2; input.Y = by + 4; input.Width = boxW - 4;
        var btnOk = new Button { X = bx + boxW / 2 - 11, Y = by + boxH - 2, Width = 10, Label = "OK" };
        var btnCancel = new Button { X = bx + boxW / 2 + 1, Y = by + boxH - 2, Width = 10, Label = "Cancel" };

        var done = false;
        ClipperInput.EnableMouseMotion();
        Console.CursorVisible = false;
        while (!done)
        {
            _buffer.Clear(FgGray, BgBlue);
            _codeArea.Draw(_buffer);
            _menu.Draw(_buffer);

            // Box disclaimer
            var (tl, th, tr, vl, br, bh, bl, vr) = ('┌', '─', '┐', '│', '┘', '─', '└', '│');
            for (var yy = by; yy < by + boxH; yy++)
                for (var xx = bx; xx < bx + boxW; xx++)
                    _buffer.SetChar(xx, yy, ' ', FgBlack, BgGray);
            for (var xx = bx + 1; xx < bx + boxW - 1; xx++)
            {
                _buffer.SetChar(xx, by, th, FgBlack, BgGray);
                _buffer.SetChar(xx, by + boxH - 1, bh, FgBlack, BgGray);
            }
            for (var yy = by + 1; yy < by + boxH - 1; yy++)
            {
                _buffer.SetChar(bx, yy, vl, FgBlack, BgGray);
                _buffer.SetChar(bx + boxW - 1, yy, vr, FgBlack, BgGray);
            }
            _buffer.SetChar(bx, by, tl, FgBlack, BgGray);
            _buffer.SetChar(bx + boxW - 1, by, tr, FgBlack, BgGray);
            _buffer.SetChar(bx, by + boxH - 1, bl, FgBlack, BgGray);
            _buffer.SetChar(bx + boxW - 1, by + boxH - 1, br, FgBlack, BgGray);

            label.Draw(_buffer);
            input.Draw(_buffer);
            btnOk.Draw(_buffer);
            btnCancel.Draw(_buffer);
            DrawStatusBar(_buffer);

            ClipperGlobal.State.InvokeRenderHook();
            var key = ClipperInput.InKey();
            if (key == 0) continue;
            if (key == KeyConstants.K_ENTER) { result = input.Text; done = true; }
            else if (key == KeyConstants.K_ESC) { result = ""; done = true; }
            else if (key == KeyConstants.K_TAB)
            {
                if (input.Focused) { input.Focused = false; btnOk.Focused = true; }
                else if (btnOk.Focused) { btnOk.Focused = false; btnCancel.Focused = true; }
                else { btnCancel.Focused = false; input.Focused = true; }
            }
            else if (btnOk.Focused && key is ' ') { result = input.Text; done = true; }
            else if (btnCancel.Focused && key is ' ') { result = ""; done = true; }
            else if (key < 0)
            {
                var ma = MouseAction.FromInkeyCode(key);
                if (ma != null)
                {
                    input.HandleClick(ma.X, ma.Y);
                    if (btnOk.Rect.Contains(ma.X, ma.Y)) { result = input.Text; done = true; }
                    else if (btnCancel.Rect.Contains(ma.X, ma.Y)) { result = ""; done = true; }
                }
            }
            else input.HandleKey(key);
        }
        return result;
    }

    private bool? Confirm(string message)
    {
        var r = AlertBox.Show("Confirm", message, width: 60, buttons: ["OK", "Cancel"]);
        return r == AlertResult.OK;
    }

    /// <summary>Dialog Options (Fase 6.3): modifica e salva IdeSettings, applica al volo.</summary>
    private void DoOptions()
    {
        var ctrl = new List<Control>();
        var tabInput = new TextInput { Text = _settings.TabWidth.ToString(), Width = 4, Focused = true };
        var autoInput = new TextInput { Text = _settings.AutoSaveInterval.ToString(), Width = 4 };
        var themeItems = new List<string> { "qbasic", "dark", "light" };
        var themeCombo = new ComboBox { Items = themeItems, Selected = Math.Max(0, themeItems.FindIndex(x => x == _settings.Theme)) };
        var cbLineNo = new CheckBox { Checked = _settings.ShowLineNumbers, Label = "Line Numbers" };
        var cbHighlight = new CheckBox { Checked = _settings.SyntaxHighlighting, Label = "Syntax Highlighting" };
        var cbBackup = new CheckBox { Checked = _settings.BackupOnSave, Label = "Backup on Save (.bak)" };
        var cbOutput = new CheckBox { Checked = _settings.ShowOutputPanel, Label = "Show Output Panel" };
        var cbWatch = new CheckBox { Checked = _settings.ShowWatchPanel, Label = "Show Watch Panel" };

        var boxW = 52;
        var boxH = 19;
        var bx = Math.Max(0, (_buffer.Width - boxW) / 2);
        var by = Math.Max(0, (_buffer.Height - boxH) / 2);
        var title = new Label { X = bx + 2, Y = by + 1, Width = boxW - 4, Text = "Options — ide.settings.json" };

        tabInput.X = bx + 14; tabInput.Y = by + 3;
        autoInput.X = bx + 14; autoInput.Y = by + 4;
        themeCombo.X = bx + 14; themeCombo.Y = by + 5; themeCombo.Width = 10;

        cbLineNo.X = bx + 2; cbLineNo.Y = by + 7; cbLineNo.Width = 20;
        cbHighlight.X = bx + 2; cbHighlight.Y = by + 8; cbHighlight.Width = 20;
        cbBackup.X = bx + 2; cbBackup.Y = by + 9; cbBackup.Width = 24;
        cbOutput.X = bx + 27; cbOutput.Y = by + 7; cbOutput.Width = 22;
        cbWatch.X = bx + 27; cbWatch.Y = by + 8; cbWatch.Width = 22;

        var btnOk = new Button { X = bx + boxW / 2 - 22, Y = by + boxH - 2, Width = 10, Label = "OK" };
        var btnCancel = new Button { X = bx + boxW / 2 - 11, Y = by + boxH - 2, Width = 10, Label = "Cancel" };
        var btnDefaults = new Button { X = bx + boxW / 2 + 0, Y = by + boxH - 2, Width = 12, Label = "Defaults" };

        var done = false;
        ctrl.AddRange([tabInput, autoInput, themeCombo, cbLineNo, cbHighlight, cbBackup, cbOutput, cbWatch, btnOk, btnCancel, btnDefaults]);
        foreach (var c in ctrl) c.Focused = false;
        var focusIdx = 0;
        ctrl[focusIdx].Focused = true;

        void Cycle(int dir)
        {
            ctrl[focusIdx].Focused = false;
            focusIdx = (focusIdx + dir + ctrl.Count) % ctrl.Count;
            ctrl[focusIdx].Focused = true;
        }

        void Apply()
        {
            _settings.TabWidth = Math.Clamp(int.TryParse(tabInput.Text, out var tw) ? tw : 2, 1, 8);
            _settings.AutoSaveInterval = int.TryParse(autoInput.Text, out var ai) ? Math.Max(0, ai) : 0;
            _settings.ShowLineNumbers = cbLineNo.Checked;
            _settings.SyntaxHighlighting = cbHighlight.Checked;
            _settings.BackupOnSave = cbBackup.Checked;
            _settings.ShowOutputPanel = cbOutput.Checked;
            _settings.ShowWatchPanel = cbWatch.Checked;
            if (themeCombo.SelectedItem != null) _settings.Theme = themeCombo.SelectedItem;

            _codeArea.TabWidth = _settings.TabWidth;
            _codeArea.ShowLineNumbers = _settings.ShowLineNumbers;
            _codeArea.Syntax = _settings.SyntaxHighlighting ? BuildSyntax() : new Syntax();
            _outputVisible = _settings.ShowOutputPanel;
            if (_settings.ShowWatchPanel) SetBottomTab(1);
            _settings.Save();
            Status("Options saved");
        }

        void ApplyDefaults()
        {
            tabInput.Text = "2";
            autoInput.Text = "0";
            themeCombo.Selected = 0;
            cbLineNo.Checked = true;
            cbHighlight.Checked = true;
            cbBackup.Checked = true;
            cbOutput.Checked = true;
            cbWatch.Checked = false;
        }

        btnOk.OnClick = () => { Apply(); done = true; };
        btnCancel.OnClick = () => done = true;
        btnDefaults.OnClick = ApplyDefaults;

        ClipperInput.EnableMouseMotion();
        Console.CursorVisible = false;
        while (!done)
        {
            _buffer.Clear(FgGray, BgBlue);
            Draw(_buffer);
            for (var yy = by; yy < by + boxH; yy++)
                for (var xx = bx; xx < bx + boxW; xx++)
                    _buffer.SetChar(xx, yy, ' ', FgBlack, BgGray);
            for (var xx = bx + 1; xx < bx + boxW - 1; xx++)
            {
                _buffer.SetChar(xx, by, '─', FgBlack, BgGray);
                _buffer.SetChar(xx, by + boxH - 1, '─', FgBlack, BgGray);
            }
            for (var yy = by + 1; yy < by + boxH - 1; yy++)
            {
                _buffer.SetChar(bx, yy, '│', FgBlack, BgGray);
                _buffer.SetChar(bx + boxW - 1, yy, '│', FgBlack, BgGray);
            }
            _buffer.SetChar(bx, by, '┌', FgBlack, BgGray);
            _buffer.SetChar(bx + boxW - 1, by, '┐', FgBlack, BgGray);
            _buffer.SetChar(bx, by + boxH - 1, '└', FgBlack, BgGray);
            _buffer.SetChar(bx + boxW - 1, by + boxH - 1, '┘', FgBlack, BgGray);

            title.Draw(_buffer);
            new Label { X = bx + 2, Y = by + 3, Width = 12, Text = "Tab Width:" }.Draw(_buffer);
            new Label { X = bx + 2, Y = by + 4, Width = 12, Text = "AutoSave sec:" }.Draw(_buffer);
            new Label { X = bx + 2, Y = by + 5, Width = 12, Text = "Theme:" }.Draw(_buffer);
            tabInput.Draw(_buffer);
            autoInput.Draw(_buffer);
            themeCombo.Draw(_buffer);
            cbLineNo.Draw(_buffer);
            cbHighlight.Draw(_buffer);
            cbBackup.Draw(_buffer);
            cbOutput.Draw(_buffer);
            cbWatch.Draw(_buffer);
            btnOk.Draw(_buffer);
            btnCancel.Draw(_buffer);
            btnDefaults.Draw(_buffer);
            DrawStatusBar(_buffer);

            ClipperGlobal.State.InvokeRenderHook();
            var key = ClipperInput.InKey();
            if (key == 0) continue;
            if (key == KeyConstants.K_ESC) { done = true; continue; }
            if (key == KeyConstants.K_TAB) { Cycle(1); continue; }
            if (key == KeyConstants.K_ENTER) { ctrl[focusIdx].HandleKey(key); continue; }
            if (key < 0)
            {
                var ma = MouseAction.FromInkeyCode(key);
                if (ma != null)
                {
                    if (btnOk.Rect.Contains(ma.X, ma.Y)) { btnOk.HandleClick(ma.X, ma.Y); continue; }
                    if (btnCancel.Rect.Contains(ma.X, ma.Y)) { btnCancel.HandleClick(ma.X, ma.Y); continue; }
                    if (btnDefaults.Rect.Contains(ma.X, ma.Y)) { btnDefaults.HandleClick(ma.X, ma.Y); continue; }
                    themeCombo.HandleClick(ma.X, ma.Y);
                    for (var i = 0; i < ctrl.Count; i++)
                        if (ctrl[i].Rect.Contains(ma.X, ma.Y))
                        {
                            if (!ReferenceEquals(ctrl[i], themeCombo))
                            {
                                ctrl[focusIdx].Focused = false;
                                focusIdx = i;
                                ctrl[i].Focused = true;
                                ctrl[i].HandleClick(ma.X, ma.Y);
                            }
                            break;
                        }
                    continue;
                }
            }
            ctrl[focusIdx].HandleKey(key);
        }
        ClipperInput.DisableMouseCapture();
        Console.CursorVisible = true;
    }

    private void Status(string msg) { Output(msg); }

    private string ProjectDir
    {
        get
        {
            try { return Path.GetDirectoryName(Path.GetFullPath(Directory.GetCurrentDirectory())) ?? "."; }
            catch { return "."; }
        }
    }

    private void MarkDirty()
    {
        _codeArea.Modified = false;
        _dirty = true;
    }

    // ========================================================================
    // AppRunner entry point
    // ========================================================================
    public void Run(bool showWelcome = false)
    {
        var g = Basic.IlBackend.IlRuntime.Current.Graphics;
        int W = ActiveConsoleIO.WindowWidth;
        int H = Math.Max(ActiveConsoleIO.WindowHeight, 25);
        ActiveConsoleIO.CursorVisible = false;

        RegisterIde(g, W, H);

        while (!ActiveConsoleIO.QuitRequested)
        {
            if (g.UiClosed())
            {
                // ESC: se c'è un popup aperto, chiudi il popup; altrimenti esci dall'IDE.
                if (_popupActive) { ClosePopup(g, W, H); g.UiResetClose(); continue; }
                break;
            }
            g.UiFrame(0); // drain tasti (controllo C) + disegno + render
            while (g.UiPollEvent(out int ev, out int widx))
            {
                // Enter sul popup di completion → inserisci il candidato.
                if (_popupActive && ev == 22 && widx == _popupIdx)
                {
                    var pick = g.UiControlText(_popupIdx);
                    if (!string.IsNullOrEmpty(pick)) InsertCompletion(pick);
                    ClosePopup(g, W, H);
                    break;
                }
                // SELCHANGE (22) sulla tab bar → attiva il tab cliccato.
                if (ev == 22 && widx == TabBarIdx)
                {
                    int ti = g.UiControlValue(TabBarIdx);
                    int n = Basic.Runtime.CIdeNative.TabCount();
                    if (ti >= 0 && ti < n && ti != _activeTab)
                    {
                        Basic.Runtime.CIdeNative.TabActivate(ti);
                        _activeTab = ti;
                        LoadActiveTabIntoEditor();
                        RegisterIde(g, W, H);
                    }
                }
            }
            int hk = g.UiPollKey();
            if (_popupActive && hk != 0) { ClosePopup(g, W, H); continue; } // qualsiasi tasto chiude il popup
            if (hk != 0) HandleHostKey(ToConsoleKey(hk), g, W, H);
        }

        SaveEditorIntoProgram();
        ActiveConsoleIO.CursorVisible = true;
        if (ActiveConsoleIO.HasConsole) ActiveConsoleIO.Clear();
    }

    /// <summary>Registra i controlli C cell dell'IDE (statusbar + tabbar + codearea + statusbar).</summary>
    private void RegisterIde(IGraphicsService g, int W, int H)
    {
        g.UiControlsReset();
        g.UiAddStatusbar(0, 0, W, $"  GMBASIC IDE  |  {System.IO.Path.GetFileName(_fileName)}{(_dirty ? " [*]" : "")}   F1=Help F2=Subs F3=Find F4=Out F5=Run F7=Build F8=Step F9=Break F10=Over F11=Menu F12=Def  Ctrl+S=Save Ctrl+T=New Ctrl+W=Close ESC=Exit");
        // Tab bar: nomi dei tab aperti nel modello C (gmb_ide).
        int tn = Basic.Runtime.CIdeNative.TabCount();
        if (tn <= 0) tn = 1;
        var names = new string[tn];
        for (int i = 0; i < tn; i++) names[i] = Basic.Runtime.CIdeNative.TabName(i);
        g.UiAddTab(0, 1, W, string.Join('|', names));
        g.UiAddCodeArea(0, 2, Math.Max(20, W), Math.Max(4, H - 3), string.Join('\n', _codeArea.Lines));
        // squiggle: marca nel gutter del CodeArea C le righe con errore di lint (gmb_ide_lint).
        SyncIdeToC();
        foreach (var t in Basic.Runtime.CIdeNative.Lint().Split(';', StringSplitOptions.RemoveEmptyEntries))
        {
            var p = t.Split('|');
            if (p.Length >= 2 && int.TryParse(p[0], out var ln) && ln >= 0) g.UiCodeAreaError(CodeAreaIdx, ln, true);
        }
        g.UiAddStatusbar(0, H - 1, W, $"  Line:{_codeArea.CursorLine + 1} Col:{_codeArea.CursorCol + 1}  Tab:{Basic.Runtime.CIdeNative.Ide != IntPtr.Zero}");
        // Popup completion inline (listbox C) accanto al cursore, se attivo.
        if (_popupActive && _popupCandidates.Count > 0)
        {
            int px = Math.Max(0, Math.Min(_popupStart, W - 12));
            int py = Math.Clamp(2 + (_popupLine - _codeArea.ScrollOffset) + 1, 3, Math.Max(3, H - 3));
            int pw = Math.Min(Math.Max(12, _popupCandidates.Max(c => c.Length) + 2), Math.Max(12, W - px));
            int ph = Math.Min(_popupCandidates.Count, Math.Max(1, H - py - 2));
            g.UiAddListbox(px, py, pw, ph, string.Join('|', _popupCandidates));
            _popupIdx = 4; // dopo reset: 0 status, 1 tabbar, 2 codearea, 3 status, 4 popup
        }
    }

    /// <summary>Indice del controllo TAB nel registry (dopo reset: 0 statusbar, 1 tabbar, 2 codearea, 3 statusbar).</summary>
    private const int TabBarIdx = 1;
    private const int CodeAreaIdx = 2;

    /// <summary>Codici tasto del console a celle (GMB_K_F1..GMB_K_F12 = 300..311, vedi gmb_tui.h)
    /// riportati a <see cref="ConsoleKey"/>: senza questa mappatura F1..F12 non arrivavano mai
    /// ai case di HandleHostKey (che confrontano con ConsoleKey.F1 = 112).</summary>
    private static int ToConsoleKey(int k)
        => k >= KeyConstants.K_F1 && k <= KeyConstants.K_F12
             ? (int)ConsoleKey.F1 + (k - KeyConstants.K_F1)
             : k;

    /// <summary>Shortcut host (tasti non consumati dal CodeArea C) → i feature dell'IDE (che operano su _codeArea).</summary>
    private void HandleHostKey(int k, IGraphicsService g, int W, int H)
    {
        // Ctrl+T (20) = nuovo tab; Ctrl+W (23) = chiudi tab. (Ctrl+Tab non distinguibile: la frame
        // usa Tab per il focus; la selezione tab via mouse/click e' gestita dai controlli C.)
        if ((k & 0x1F) == 20) { DoNewTab(g, W, H); return; }
        if ((k & 0x1F) == 23) { DoCloseTab(g, W, H); return; }
        // Ctrl+lettera (parità con l'IDE C: stesso set di scorciatoie)
        if ((k & 0x1F) == 19) { SaveAndClean(); return; }   // Ctrl+S
        if ((k & 0x1F) == 15) { DoOpen(); return; }         // Ctrl+O
        if ((k & 0x1F) == 14) { DoNew(); return; }          // Ctrl+N
        if ((k & 0x1F) == 6)  { DoFind(); return; }         // Ctrl+F
        if ((k & 0x1F) == 8)  { DoReplace(); return; }      // Ctrl+H
        if ((k & 0x1F) == 7)  { DoGoToLine(); return; }     // Ctrl+G

        SyncCodeFromC(g);
        bool changed = false;
        switch (k)
        {
            case (int)ConsoleKey.F1: DoAbout(); break;
            case (int)ConsoleKey.F2: DoOutline(); break;
            case (int)ConsoleKey.F3: FindNext(_lastFindDir); break;   // come l'IDE C (Repeat Last Find)
            case (int)ConsoleKey.F4: DoToggleOutput(); changed = true; break;
            case (int)ConsoleKey.F5: DoRun(); break;
            case (int)ConsoleKey.F7: DoBuild(); break;
            case (int)ConsoleKey.F8: DoStepInto(); break;
            case (int)ConsoleKey.F9: ToggleBreakpointAtCursor(); changed = true; break;
            case (int)ConsoleKey.F10: DoStepOver(); break;
            case (int)ConsoleKey.F11: ShowMenuBar(); changed = true; break;
            case (int)ConsoleKey.F12: DoGoToDef(); break;
        }
        if (changed) RegisterIde(g, W, H);
    }

    private int _activeTab;

    /// <summary>F11 = barra menu: livello 1 = i menu, livello 2 = le voci (stesse azioni dell'IDE C).
    /// Nel console a celle i menu non si disegnano come barra: si usano i selettori paginati.</summary>
    private void ShowMenuBar()
    {
        var tops = _menu.Items.Select(i => i.Label.Trim()).ToList();
        var pick = PaginatedMenu.Select(tops, "Menu");
        if (string.IsNullOrEmpty(pick)) return;
        int mi = tops.IndexOf(pick!);
        if (mi < 0) return;
        var sub = _menu.Items[mi].SubItems.Where(s => !s.Separator).ToList();
        if (sub.Count == 0) { Status($"{pick}: nessuna voce"); return; }
        var labels = sub.Select(s => s.Label).ToList();
        var chosen = PaginatedMenu.Select(labels, pick!);
        if (string.IsNullOrEmpty(chosen)) return;
        int si = labels.IndexOf(chosen!);
        if (si >= 0) sub[si].Action?.Invoke();
    }

    /// <summary>Nuovo tab vuoto + attivazione.</summary>
    private void DoNewTab(IGraphicsService g, int W, int H)
    {
        SyncCodeFromC(g);
        SaveEditorIntoProgram();
        _activeTab = Basic.Runtime.CIdeNative.TabOpen("untitled.bas", "");
        LoadActiveTabIntoEditor();
        RegisterIde(g, W, H);
        Status("New tab");
    }

    /// <summary>Chiude il tab attivo (minimo 1).</summary>
    private void DoCloseTab(IGraphicsService g, int W, int H)
    {
        if (Basic.Runtime.CIdeNative.TabCount() <= 1) { Status("Unico tab"); return; }
        SaveEditorIntoProgram();
        _activeTab = Basic.Runtime.CIdeNative.TabClose(_activeTab);
        LoadActiveTabIntoEditor();
        RegisterIde(g, W, H);
        Status("Tab chiuso");
    }

    /// <summary>Cicla i tab (dir +1/-1).</summary>
    private void DoCycleTab(IGraphicsService g, int W, int H, int dir)
    {
        int n = Basic.Runtime.CIdeNative.TabCount();
        if (n <= 1) return;
        SyncCodeFromC(g);
        int cur = Basic.Runtime.CIdeNative.TabCount() - 1;   // l'ultimo aperto è l'attivo
        int nxt = ((cur + dir) % n + n) % n;
        Basic.Runtime.CIdeNative.TabActivate(nxt);
        LoadActiveTabIntoEditor();
        RegisterIde(g, W, H);
    }

    /// <summary>Carica nell'editor C# il testo del tab attivo (modello C).</summary>
    private void LoadActiveTabIntoEditor()
    {
        int n = Basic.Runtime.CIdeNative.TabCount();
        if (n <= 0) return;
        if (_activeTab < 0 || _activeTab >= n) _activeTab = n - 1;
        var text = Basic.Runtime.CIdeNative.DocText(_activeTab);
        _codeArea.Lines.Clear();
        _codeArea.Lines.AddRange(text.Replace("\r", "").Split('\n'));
        if (_codeArea.Lines.Count > 0 && _codeArea.Lines[^1].Length == 0) _codeArea.Lines.RemoveAt(_codeArea.Lines.Count - 1);
        if (_codeArea.Lines.Count == 0) _codeArea.Lines.Add("");
    }

    /// <summary>Rilegge il testo editato nel CodeArea C (registry idx 1 = il codearea) nel buffer C# `_codeArea`.</summary>
    private void SyncCodeFromC(IGraphicsService g)
    {
        var text = g.UiControlText(1);
        if (string.IsNullOrEmpty(text)) return;
        _codeArea.Lines.Clear();
        _codeArea.Lines.AddRange(text.Replace("\r", "").Split('\n'));
        if (_codeArea.Lines.Count > 0 && _codeArea.Lines[^1].Length == 0) _codeArea.Lines.RemoveAt(_codeArea.Lines.Count - 1);
        if (_codeArea.Lines.Count == 0) _codeArea.Lines.Add("");
    }

    public void Resize(int cols, int rows)
    {
        _buffer.Resize(Math.Max(cols, 80), Math.Max(rows, 25));
        _codeArea.Width = _buffer.Width;
        _codeArea.Height = Math.Max(1, _buffer.Height - OutputPanelH - 2);
    }

    public void HandleEvent(UiEvent evt)
    {
        switch (evt)
        {
            case UiKey key:
                HandleKey(key.Key, key.Modifiers);
                break;
            case UiMouse mouse:
                HandleMouseAction(mouse.Action);
                break;
            case UiResize r:
                Resize(r.Width, r.Height);
                break;
        }
    }

    // ========================================================================
    // Key dispatch (menu, shortcuts QBasic, codice)
    // ========================================================================
    private void HandleKey(int key, KeyModifiers mods)
    {
        var ctrl = (mods & KeyModifiers.Control) != 0;
        var alt = (mods & KeyModifiers.Alt) != 0;
        var shift = (mods & KeyModifiers.Shift) != 0;
        var letter = char.ToUpperInvariant((char)key);

        if (_menu.IsOpen)
        {
            HandleMenuKey(key, alt, shift);
            return;
        }

        // Immediate Window (Fase 8): quando la barra `▸` ha focus, la digitazione va all'espressione.
        if (_immediateFocused)
        {
            if (key == KeyConstants.K_ESC)
            {
                _immediateFocused = false;
                _codeArea.Focused = true;
                return;
            }
            if (key == KeyConstants.K_ENTER)
            {
                EvalImmediate();
                return;
            }
            if (key == KeyConstants.K_BS && _immediateInput.Length > 0)
            {
                _immediateInput = _immediateInput[..^1];
                return;
            }
            if (key >= 32 && key <= 126)
            {
                _immediateInput += (char)key;
                return;
            }
            return; // altri tasti (frecce, F-key) non hanno effetto nella barra
        }

        // Alt+lettera → apri menu (acceleratori Fase 1.3), Alt+F4 → exit
        if (alt && letter >= 'A' && letter <= 'Z')
        {
            foreach (var (l, i) in MenuAccel)
                if (l[0] == letter)
                {
                    _menu.SelectedIndex = i;
                    _menu.SubSelectedIndex = 0;
                    _menu.IsOpen = true;
                    return;
                }
        }

        // Shortcut globali (Ctrl / Shift+F-key / FunZioni)
        if (ctrl && letter == 'N') { DoNew(); return; }
        if (ctrl && letter == 'O') { DoOpen(); return; }
        if (ctrl && letter == 'S') { SaveAndClean(); return; }
        if (ctrl && letter == 'Z') { DoUndo(); return; }
        if (ctrl && letter == 'Y') { DoRedo(); return; }
        if (ctrl && letter == 'A') { DoSelectAll(); return; }
        if (ctrl && letter == 'F') { _menu.Items[3].SubItems[0].Action?.Invoke(); return; }
        if (ctrl && letter == 'H') { _menu.Items[3].SubItems[3].Action?.Invoke(); return; }
        if (ctrl && letter == 'G') { _menu.Items[3].SubItems[4].Action?.Invoke(); return; }
        if (ctrl && key == '/') { ToggleCommentBlock(true); return; }
        if (ctrl && shift && key == '/') { ToggleCommentBlock(false); return; }
        if (ctrl && letter == 'L') { ToggleLineNumbers(); return; }
        if (ctrl && shift && letter == 'L') { DoRenumber(); return; }
        if (ctrl && key == 32) { MaybeOpenCompletion(true); return; }

        // Popup autocomplete: navigazione ↑↓, inserimento Enter/Tab, chiusura ESC.
        if (_completionVisible && !_running && !_outputFocused)
        {
            if (key == KeyConstants.K_UP)
            {
                _completionIndex = Math.Max(0, _completionIndex - 1);
                return;
            }
            if (key == KeyConstants.K_DOWN)
            {
                _completionIndex = Math.Min(_completionMatches!.Count - 1, _completionIndex + 1);
                return;
            }
            if (key == KeyConstants.K_ENTER || key == KeyConstants.K_TAB)
            {
                AcceptCompletion();
                return;
            }
            if (key == KeyConstants.K_ESC)
            {
                CloseCompletion();
                return;
            }
            if (key is KeyConstants.K_LEFT or KeyConstants.K_RIGHT or KeyConstants.K_HOME or KeyConstants.K_END or KeyConstants.K_PGUP or KeyConstants.K_PGDN)
                CloseCompletion();
        }

        // Insert Automatico numero riga (stile QBasic) quando si preme Enter su riga numerata.
        if (key == KeyConstants.K_ENTER && AutoInsertLineNumbers)
        {
            _codeArea.HandleKeyInCurrentLineNumbering();
            return;
        }

        // Copia/incolla/taglia stile QBasic
        if (ctrl && key == KeyConstants.K_INS) { DoCopy(); return; }
        if (shift && key == KeyConstants.K_INS) { DoPaste(); return; }
        if (shift && key == KeyConstants.K_DEL) { DoCut(); return; }

        // F-key bar (QBasic / 2026 revision)
        if (key >= KeyConstants.K_F1 && key <= KeyConstants.K_F10)
        {
            if (ctrl && key == KeyConstants.K_F8) { DoRunToCursor(); return; }
            if (ctrl && key == KeyConstants.K_F5) { DoRun(); return; }
            HandleFunctionKey(key, shift);
            return;
        }
        // F11 = menu (senza Alt), F12 = Go to Definition / Shift+F12 = Find References
        if (key == KeyConstants.K_F11)
        {
            _menu.SelectedIndex = 0;
            _menu.SubSelectedIndex = 0;
            _menu.IsOpen = true;
            return;
        }
        if (key == KeyConstants.K_F12)
        {
            if (shift) DoFindRefs(); else DoGoToDef();
            return;
        }

        if (key == KeyConstants.K_ESC)
        {
            if (_running)
            {
                _runCts?.Cancel();
                _exiting = false;
                return;
            }
            if (_exiting) { ShutdownNotebook(); CloseRequested = true; return; }
            _exiting = true;
            return;
        }

        // Selezione con Shift+arrow
        var hadSelection = _codeArea.HasSelection;
        if (shift && (key is KeyConstants.K_LEFT or KeyConstants.K_RIGHT or KeyConstants.K_UP or KeyConstants.K_DOWN or KeyConstants.K_HOME or KeyConstants.K_END))
        {
            if (!hadSelection) _codeArea.SelectionAnchor = (_codeArea.CursorLine, _codeArea.CursorCol);
            _codeArea.HandleKey(key);
            if (_codeArea.HasSelection) UpdateDirty();
            _codeArea.SelectionEnd = (_codeArea.CursorLine, _codeArea.CursorCol);
            return;
        }

        // Editing: push undo prima di modifiche significative
        if (key is KeyConstants.K_ENTER or KeyConstants.K_BS or KeyConstants.K_DEL or (>= 32 and <= 126))
        {
            PushUndo();
            _codeArea.ClearSelection();
            _codeArea.HandleKey(key);
            UpdateDirty();
            if (key is >= 32 and <= 126) MaybeOpenCompletion(false);
            return;
        }

        // F6 → switch editor/output pane
        if (key == KeyConstants.K_F6)
        {
            _outputFocused = !_outputFocused;
            _codeArea.Focused = !_outputFocused;
            return;
        }

        // Navigazione pannello basso (Watch/Call Stack scroll, ↓/↑ selezione frame)
        if (_outputFocused && _outputVisible)
        {
            if (key == KeyConstants.K_PGDN || (key == KeyConstants.K_DOWN && _bottomTab == 1))
            {
                _watchScroll = Math.Min(_watchRows.Count - (OutputPanelBodyH - 1), _watchScroll + 1);
                if (_watchScroll < 0) _watchScroll = 0;
                return;
            }
            if (key == KeyConstants.K_PGUP || (key == KeyConstants.K_UP && _bottomTab == 1))
            {
                _watchScroll = Math.Max(0, _watchScroll - 1);
                return;
            }
            if (key == KeyConstants.K_UP && _bottomTab == 2)
            {
                _stackSelection = Math.Max(0, _stackSelection - 1);
                _stackScroll = Math.Max(0, Math.Min(_stackRows.Count - OutputPanelBodyH, _stackSelection));
                return;
            }
            if (key == KeyConstants.K_DOWN && _bottomTab == 2)
            {
                _stackSelection = Math.Min(Math.Max(0, _stackRows.Count - 1), _stackSelection + 1);
                _stackScroll = Math.Max(0, Math.Min(_stackRows.Count - OutputPanelBodyH, _stackSelection));
                return;
            }
        }

        _codeArea.HandleKey(key);
    }

    private bool AutoInsertLineNumbers { get; set; } = true;

    private void UpdateDirty()
    {
        if (!_dirty && _codeArea.Modified) _dirty = true;
        _codeArea.Modified = false;
    }

    private void HandleFunctionKey(int key, bool shift)
    {
        var f = key switch
        {
            KeyConstants.K_F1 => 1,
            KeyConstants.K_F2 => 2,
            KeyConstants.K_F3 => 3,
            KeyConstants.K_F4 => 4,
            KeyConstants.K_F5 => 5,
            KeyConstants.K_F6 => 6,
            KeyConstants.K_F7 => 7,
            KeyConstants.K_F8 => 8,
            KeyConstants.K_F9 => 9,
            _ => 10,
        };
        if (shift)
        {
            if (f == 1) { DoAbout(); return; }
            if (f == 2) { DoNextSub(); return; }
            if (f == 5) { DoRun(); return; }
            if (f == 9) { DoAddWatch(); return; }
            if (f == 10) { DoStepOut(); return; }
            return;
        }
        switch (f)
        {
            case 1: DoAbout(); break;
            case 2: ShowOutline(); break;
            case 3: FindNext(_lastFindDir); break;
            case 4: DoToggleOutput(); break;
            case 5: DoRun(); break;
            case 6: _outputFocused = !_outputFocused; _codeArea.Focused = !_outputFocused; break;
            case 7: DoBuild(); break;
            case 8: DoStepInto(); break;
            case 9: ToggleBreakpointAtCursor(); break;
            case 10: DoStepOver(); break;
        }
    }

    // ========================================================================
    // Debug (Fase 4): Step Into/Over/Out, Run to Cursor, Continue
    // ========================================================================
    private void DoStepInto()
    {
        if (_debugServer != null && _debugServer.IsPaused)
        {
            _debugServer.StepIn();
            Status("Step Into");
            return;
        }
        if (!_running)
        {
            _startStep = true;
            DoRun();
        }
    }

    private void DoStepOver()
    {
        if (_debugServer != null && _debugServer.IsPaused)
        {
            _debugServer.StepOver();
            Status("Step Over");
            return;
        }
        if (!_running)
        {
            _startStep = true;
            DoRun();
        }
    }

    private void DoStepOut()
    {
        if (_debugServer != null && _debugServer.IsPaused)
        {
            _debugServer.StepOut();
            Status("Step Out");
        }
    }

    private void DoRunToCursor()
    {
        if (!_running)
        {
            _tempBpLine = _codeArea.CursorLine;
            _startStep = false;
            DoRun();
            return;
        }
        if (_debugServer != null && _debugServer.IsPaused)
        {
            var line = _codeArea.CursorLine;
            _tempBpLine = line;
            _debugServer.SetBreakpoint(line + 1);
            _debugServer.Continue();
            Status($"Run to cursor (line {line + 1})");
        }
    }

    /// <summary>Aggiunge un'espressione alla lista Watch. Quando il debugger è in pausa la valuta
    /// subito tramite la valutazione; altrimenti viene valutata alla prossima pausa.</summary>
    private void DoAddWatch()
    {
        var expr = PromptForText("Add Watch", _userWatches.Count > 0 ? _userWatches[^1] : "");
        if (string.IsNullOrWhiteSpace(expr)) return;
        _userWatches.Add(expr.Trim());
        RefreshDebugPanels();
        SetBottomTab(1);
        Status($"Watch added: {expr}");
    }

    /// <summary>Seleziona il pannello basso (0=Output, 1=Watch, 2=Call Stack) e lo rende visibile.</summary>
    private void SetBottomTab(int tab)
    {
        _bottomTab = Math.Clamp(tab, 0, 2);
        _outputVisible = true;
        _outputFocused = true;
        _codeArea.Focused = false;
        _watchScroll = Math.Max(0, _watchRows.Count - OutputPanelBodyH);
        _stackScroll = 0;
    }

    /// <summary>Rilegge i dati di Watch e Call Stack (debug non più disponibile).
    /// Chiamata a ogni Draw per riflettere i nuovi valori a ogni step/breakpoint.</summary>
    private void RefreshDebugPanels()
    {
        var ds = _debugServer;
        if (ds == null || !ds.IsPaused)
        {
            _watchRows.Clear();
            _stackRows.Clear();
            _stackSelection = 0;
            _watchScroll = 0;
            return;
        }
        // Run-to-Cursor: se la pausa è sul breakpoint temporaneo, lo rimuove (single-shot).
        if (_tempBpLine >= 0 && ds.CurrentLine == _tempBpLine + 1)
        {
            ds.ClearBreakpoints();
            foreach (var line in _breakpoints) ds.SetBreakpoint(line + 1);
            _tempBpLine = -1;
        }
        _watchRows.Clear();
        var vars = ds.GetVariables(0);
        if (vars != null)
        {
            foreach (var v in vars)
                _watchRows.Add((v.Name, v.Value, v.Type));
        }
        foreach (var w in _userWatches)
        {
            var v = ds.Evaluate(w);
            _watchRows.Add((w, v ?? "NULL", "expr"));
        }
        _stackRows.Clear();
        var frames = ds.StackFrames;
        if (frames != null)
        {
            foreach (var f in frames)
                _stackRows.Add((f.Name, $"line {f.Line}"));
        }
        _stackSelection = Math.Clamp(_stackSelection, 0, Math.Max(0, _stackRows.Count - 1));
        _watchScroll = Math.Clamp(_watchScroll, 0, Math.Max(0, _watchRows.Count - OutputPanelBodyH));
    }

    private void HandleMenuKey(int key, bool alt, bool shift)
    {
        if (alt && !shift)
        {
            // Alt+lettera quando menu aperto: naviga alla voce di primo livello.
            _menu.HandleKey(KeyConstants.K_ESC);
            foreach (var (l, i) in MenuAccel)
                if (l[0] == char.ToUpperInvariant((char)key))
                {
                    _menu.SelectedIndex = i;
                    _menu.SubSelectedIndex = 0;
                    _menu.IsOpen = true;
                }
            return;
        }
        if (key == KeyConstants.K_ENTER)
        {
            var sel = _menu.GetSelectedLabel();
            if (sel != null)
            {
                var sub = _menu.Items[_menu.SelectedIndex].SubItems;
                if (_menu.SubSelectedIndex >= 0 && _menu.SubSelectedIndex < sub.Count)
                    sub[_menu.SubSelectedIndex].Action?.Invoke();
            }
            _menu.IsOpen = false;
            return;
        }
        _menu.HandleKey(key);
        if (key == KeyConstants.K_ESC) _exiting = false;
    }

    // ========================================================================
    // Mouse
    // ========================================================================
    private void HandleMouseAction(MouseAction ma)
    {
        if (ma.Kind is MouseActionKind.Click or MouseActionKind.DoubleClick)
        {
            if (_menu.Rect.Contains(ma.X, ma.Y))
            {
                _menu.HandleClick(ma.X, ma.Y);
                _outputFocused = false;
                _codeArea.Focused = true;
                return;
            }
            if (_menu.IsOpen)
            {
                _menu.IsOpen = false;
            }

            var statusY = _buffer.Height - 1;
            if (ma.Y == statusY)
            {
                _outputFocused = false;
                return;
            }

            // Immediate bar (Fase 8): click → focus per digitare un'espressione.
            if (_settings.ImmediateWindow && ma.Y == _buffer.Height - 2)
            {
                _immediateFocused = true;
                _outputFocused = false;
                _codeArea.Focused = false;
                return;
            }

            // Output panel click/scroll
            if (_outputVisible && ma.Y >= BottomPanelY && ma.Y < BottomPanelEndY)
            {
                _outputFocused = true;
                _codeArea.Focused = false;
                // Click sulla riga dei tab (Output | Watch | Call Stack)
                if (ma.Y == BottomPanelY)
                {
                    var cx = 1;
                    string[] tabs = ["Output", "Watch", "Call Stack"];
                    for (var i = 0; i < tabs.Length; i++)
                    {
                        var w = tabs[i].Length + 2;
                        if (ma.X >= cx && ma.X < cx + w) { _bottomTab = i; return; }
                        cx += w + 1;
                    }
                }
                return;
            }

            _outputFocused = false;
            _codeArea.Focused = true;
            _codeArea.HandleMouse(ma);
            return;
        }

        if (ma.Kind == MouseActionKind.ScrollUp)
        {
            if (_outputFocused || (_outputVisible && ma.Y >= BottomPanelY && ma.Y < BottomPanelEndY))
            {
                if (_bottomTab == 1) { _watchScroll = Math.Max(0, _watchScroll - 1); }
                else if (_bottomTab == 2) { _stackScroll = Math.Max(0, _stackScroll - 1); }
                else _outputScroll = Math.Max(0, _outputScroll - 1);
            }
            else _codeArea.HandleMouse(ma);
            return;
        }
        if (ma.Kind == MouseActionKind.ScrollDown)
        {
            if (_outputFocused || (_outputVisible && ma.Y >= BottomPanelY && ma.Y < BottomPanelEndY))
            {
                if (_bottomTab == 1) { _watchScroll = Math.Min(Math.Max(0, _watchRows.Count - OutputPanelBodyH), _watchScroll + 1); }
                else if (_bottomTab == 2) { _stackScroll = Math.Min(Math.Max(0, _stackRows.Count - OutputPanelBodyH), _stackScroll + 1); }
                else _outputScroll = Math.Min(_output.Count, _outputScroll + 1);
            }
            else _codeArea.HandleMouse(ma);
            return;
        }

        if (ma.Kind == MouseActionKind.Drag)
        {
            if (!_outputFocused) _codeArea.HandleMouse(ma);
            return;
        }
    }

    // ========================================================================
    // Draw (TitleBar, MenuBar, CodeArea, Output, StatusBar)
    // ========================================================================
    public void Draw(ScreenBuffer buf)
    {
        UpdateExecutionHighlight();
        RefreshDebugPanels();
        MaybeAutoSave();
        buf.Clear(FgGray, BgBlue);
        DrawTitleBar(buf);
        _menu.Draw(buf);
        _codeArea.Draw(buf);
        if (_outputVisible) DrawBottomPanel(buf);
        if (_settings.ImmediateWindow) DrawImmediateBar(buf);
        DrawStatusBar(buf);
        DrawCompletionPopup(buf);
    }

    /// <summary>Barra Immediate Window (Fase 8): prompt `▸` sopra la StatusBar dove l'utente
    /// digita un'espressione BASIC e la valuta con Enter (PRINT/?/assegnazioni/chiamate).</summary>
    private void DrawImmediateBar(ScreenBuffer buf)
    {
        var y = _buffer.Height - 2;
        var sw = _buffer.Width;
        buf.SetString(0, y, new string(' ', sw), FgWhite, BgDarkCyan);
        var label = _immediateFocused ? "▸ " : "  ";
        buf.SetString(0, y, label, FgYellow, BgDarkCyan);
        var text = _immediateInput;
        var maxLen = sw - label.Length - 1;
        if (text.Length > maxLen) text = text[..maxLen];
        buf.SetString(label.Length, y, text, FgWhite, BgDarkCyan);
        if (_immediateFocused)
        {
            var cx = label.Length + text.Length;
            if (cx < sw) buf.SetChar(cx, y, '_', FgBlack, BgGray);
        }
    }

    /// <summary>Auto-save periodico (Fase 6.2): salva se dirty ogni AutoSaveInterval secondi,
    /// mai durante l'esecuzione. _autoSaveForceSave copre il giro dopo il dialogo Options.</summary>
    private void MaybeAutoSave()
    {
        var interval = _settings.AutoSaveInterval;
        if (interval <= 0 || _running) return;
        if (!_dirty && !_autoSaveForceSave) return;
        var now = DateTime.UtcNow;
        if (_lastAutoSave == DateTime.MinValue || (now - _lastAutoSave).TotalSeconds >= interval || _autoSaveForceSave)
        {
            _lastAutoSave = now;
            _autoSaveForceSave = false;
            SaveFileClean();
        }
    }

    /// <summary>Overlay autocomplete sotto il cursore (Fase 5.2): box con lista candidati,
    /// riga selezionata evidenziata, scroll incrementale.</summary>
    private void DrawCompletionPopup(ScreenBuffer buf)
    {
        if (!_completionVisible || _completionMatches == null || _completionMatches.Count == 0) return;
        var maxVisible = 8;
        var width = Math.Min(40, Math.Max(20, _completionMatches.Max(m => m.Length) + 4));
        var height = Math.Min(_completionMatches.Count, maxVisible) + 2;
        var padding = 2;
        var cx = _codeArea.X + _codeArea.GutterWidth + _codeArea.CursorCol;
        var cy = _codeArea.Y + (_codeArea.CursorLine - _codeArea.ScrollOffset) + 1;
        var bx = Math.Clamp(cx - padding, 0, Math.Max(0, buf.Width - width));
        var by = cy + 1 < buf.Height - 1 ? cy + 1 : Math.Max(0, cy - height - 1);
        if (bx + width > buf.Width) bx = Math.Max(0, buf.Width - width);
        if (by + height > buf.Height) by = Math.Max(0, buf.Height - height);

        for (var yy = by; yy < by + height; yy++)
            for (var xx = bx; xx < bx + width; xx++)
                buf.SetChar(xx, yy, ' ', FgBlack, BgGray);
        buf.SetChar(bx, by, '┌', FgBlack, BgGray);
        buf.SetChar(bx + width - 1, by, '┐', FgBlack, BgGray);
        buf.SetChar(bx, by + height - 1, '└', FgBlack, BgGray);
        buf.SetChar(bx + width - 1, by + height - 1, '┘', FgBlack, BgGray);
        for (var xx = bx + 1; xx < bx + width - 1; xx++)
        {
            buf.SetChar(xx, by, '─', FgBlack, BgGray);
            buf.SetChar(xx, by + height - 1, '─', FgBlack, BgGray);
        }
        for (var yy = by + 1; yy < by + height - 1; yy++)
        {
            buf.SetChar(bx, yy, '│', FgBlack, BgGray);
            buf.SetChar(bx + width - 1, yy, '│', FgBlack, BgGray);
        }

        var first = _completionIndex;
        if (first >= maxVisible) first = _completionIndex - (maxVisible - 1);
        if (first < 0) first = 0;
        for (var i = 0; i < maxVisible && first + i < _completionMatches.Count; i++)
        {
            var text = _completionMatches[first + i];
            var selected = first + i == _completionIndex;
            buf.SetString(bx + 1, by + 1 + i, text, selected ? FgBlack : FgGray, selected ? FgCyan : BgGray);
        }
    }

    /// <summary>Aggiorna l'highlight della riga corrente (debug non più disponibile).idenzia
    /// la riga di esecuzione (CurrentLine è 1-based, il CodeArea è 0-based) e la porta in vista con
    /// lo scroll, come il marker del QBasic; quando il run termina lo azzera.</summary>
    private void UpdateExecutionHighlight()
    {
        var ds = _debugServer;
        if (ds != null && ds.IsPaused)
        {
            var line = ds.CurrentLine - 1;
            if (line >= 0)
            {
                _codeArea.CurrentExecutionLine = line;
                if (line < _codeArea.ScrollOffset || line >= _codeArea.ScrollOffset + _codeArea.Height)
                    _codeArea.ScrollOffset = Math.Clamp(line - _codeArea.Height / 2, 0, Math.Max(0, _codeArea.Lines.Count - _codeArea.Height));
            }
        }
        else if (!_running)
        {
            _codeArea.CurrentExecutionLine = null;
        }
    }

    private void DrawTitleBar(ScreenBuffer buf)
    {
        buf.SetString(0, TitleBarY, new string(' ', buf.Width), FgWhite, BgDarkCyan);
        var title = $"  █ GMBASIC - {Path.GetFileName(_fileName)}{(_dirty ? " [*]" : "")}";
        buf.SetString(1, TitleBarY, title, FgWhite, BgDarkCyan);
        var right = _debugServer != null && _debugServer.IsPaused ? "[Debug: paused]" : _running ? "[Debug: running]" : "[Debug] [P-Code]";
        buf.SetString(Math.Max(0, buf.Width - right.Length - 2), TitleBarY, right, FgWhite, BgDarkCyan);
    }

    private void DrawBottomPanel(ScreenBuffer buf)
    {
        var startY = buf.Height - OutputPanelH;
        var sw = _buffer.Width;
        buf.SetString(0, startY, new string(' ', sw), FgGray, BgDeepBlue);

        // Tab (Output | Watch | Call Stack)
        string[] tabs = ["Output", "Watch", "Call Stack"];
        var cx = 1;
        for (var i = 0; i < tabs.Length; i++)
        {
            var active = i == _bottomTab;
            var label = (active ? "[" : " ") + tabs[i] + (active ? "]" : " ");
            buf.SetString(cx, startY, label, active ? FgBlack : FgGray, active ? BgGray : BgDeepBlue);
            cx += label.Length + 1;
        }
        if (_outputFocused) buf.SetString(cx, startY, "◄", FgYellow, BgDeepBlue);

        switch (_bottomTab)
        {
            case 1: DrawWatchPanel(buf, startY, sw); break;
            case 2: DrawCallStackPanel(buf, startY, sw); break;
            default: DrawOutputPanelBody(buf, startY, sw); break;
        }
    }

    private void DrawOutputPanelBody(ScreenBuffer buf, int startY, int sw)
    {
        var lines = _output;
        var maxTop = Math.Max(0, lines.Count - OutputPanelBodyH);
        var top = Math.Clamp(_outputScroll - 1, 0, maxTop);
        // Snapshot per leggere l'Output panel mentre il task di Run vi scrive in background.
        string[] snap;
        lock (_outputLock) { snap = _output.ToArray(); }
        lines = snap.ToList();
        for (var i = 1; i <= OutputPanelBodyH; i++)
        {
            if (top + i - 1 >= lines.Count) break;
            var text = lines[top + i - 1];
            if (text.Length > sw) text = text[..sw];
            else text = text.PadRight(sw);
            var col = text.StartsWith("Build error", StringComparison.OrdinalIgnoreCase) || text.StartsWith("ERROR")
                ? FgRed : text.StartsWith("Build") ? FgYellow : FgGray;
            buf.SetString(0, startY + i, text, col, BgDeepBlue);
        }

        // Thumb output
        var thumbTotal = Math.Max(1, lines.Count);
        var thumbLen = Math.Max(1, OutputPanelBodyH * OutputPanelBodyH / thumbTotal);
        var thumbY = startY + 1 + Math.Min(OutputPanelBodyH - 1, Math.Max(0, (lines.Count > 0 ? top * (OutputPanelBodyH - 1) / thumbTotal : 0)));
        if (lines.Count > OutputPanelBodyH)
        {
            buf.SetChar(0, thumbY, '▐', FgGray, BgOutputTrack);
        }
    }

    /// <summary>Tab Watch: variabili locali del frame corrente + espressioni watch utente.</summary>
    private void DrawWatchPanel(ScreenBuffer buf, int startY, int sw)
    {
        buf.SetString(1, startY, "", FgGray, BgDeepBlue);
        var maxTop = Math.Max(0, _watchRows.Count - OutputPanelBodyH);
        var top = Math.Clamp(_watchScroll, 0, maxTop);
        for (var i = 1; i <= OutputPanelBodyH; i++)
        {
            var idx = top + i - 1;
            if (idx >= _watchRows.Count) break;
            var (name, value, type) = _watchRows[idx];
            var text = $"{name} = {value}  ({type})";
            if (text.Length > sw) text = text[..sw];
            else text = text.PadRight(sw);
            var col = type == "expr" ? FgCyan : FgGray;
            buf.SetString(0, startY + i, text, col, BgDeepBlue);
        }
        if (_watchRows.Count == 0)
            buf.SetString(1, startY + 1, "(no variables — pausa su breakpoint per popolare)", FgYellow, BgDeepBlue);
    }

    /// <summary>Tab Call Stack: frames con nome procedura e riga. Selezione ↑↓ evidenziata.</summary>
    private void DrawCallStackPanel(ScreenBuffer buf, int startY, int sw)
    {
        var maxTop = Math.Max(0, _stackRows.Count - OutputPanelBodyH);
        var top = Math.Clamp(_stackScroll, 0, maxTop);
        for (var i = 1; i <= OutputPanelBodyH; i++)
        {
            var idx = top + i - 1;
            if (idx >= _stackRows.Count) break;
            var (name, line) = _stackRows[idx];
            var text = $"{name}  {line}";
            if (text.Length > sw) text = text[..sw];
            else text = text.PadRight(sw);
            var sel = idx == _stackSelection;
            buf.SetString(0, startY + i, text, sel ? FgBlack : FgGray, sel ? BgGray : BgDeepBlue);
        }
        if (_stackRows.Count == 0)
            buf.SetString(1, startY + 1, "(stack vuoto — in pausa mostra i frame)", FgYellow, BgDeepBlue);
    }

    private void DrawStatusBar(ScreenBuffer buf)
    {
        var y = buf.Height - 1;
        var sw = buf.Width;
        buf.SetString(0, y, new string(' ', sw), FgWhite, BgDarkCyan);

        var fn = "F1=Help F2=SUBs F3=Find F4=Out  F5=Run  F6=Window F7=Build F8=Step F9=Break F10=Over F12=Def";
        var pos = $"{_codeArea.CursorLine + 1,5}:{_codeArea.CursorCol + 1,3}";
        var total = sw - pos.Length - 1;
        if (fn.Length > total) fn = fn[..total];
        buf.SetString(0, y, fn, FgWhite, BgDarkCyan);
        buf.SetString(sw - pos.Length, y, pos, FgYellow, BgDarkCyan);
    }

    // ========================================================================
    // Extra: toggle numeri riga
    // ========================================================================
    private void ToggleLineNumbers()
    {
        _codeArea.ShowLineNumbers = !_codeArea.ShowLineNumbers;
        Status(_codeArea.ShowLineNumbers ? "Line numbers shown" : "Line numbers hidden");
    }
}

/// <summary>Estensioni locali per il CodeArea (inserimento automatico numero riga alla pressione Invio).</summary>
public static class GMBasicIdeCodeAreaExt
{
    /// <summary>Inserisce la prossima linea numerata (stile AUTO QBasic) quando il cursore è su una riga numerata.</summary>
    public static void HandleKeyInCurrentLineNumbering(this CodeArea area)
    {
        var line = area.CursorLine;
        var cur = area.Lines[line];
        var t = cur.TrimStart();
        var sp = t.IndexOf(' ');
        if (sp <= 0 || !int.TryParse(t[..sp], out var num) || sp + 1 > cur.Length)
        {
            area.HandleKey(KeyConstants.K_ENTER);
            return;
        }
        var next = num + 10;
        var indent = sp + 1;
        var rest = cur.Substring(indent);
        var body = rest.TrimStart() == "" ? "" : rest.TrimStart();
        area.Lines[line] = cur.Substring(0, indent) + body;
        area.Lines.Insert(line + 1, $"{next} ");
        area.CursorLine = line + 1;
        area.CursorCol = $"{next} ".Length;
        area.Modified = true;
    }
}