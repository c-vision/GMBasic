using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>
/// Result of a navigation action, mirroring the Rust FileExplorerAction.
/// </summary>
public enum FileExplorerAction
{
    None,
    Selected,
    ChangedDir,
}

/// <summary>
/// File system explorer. Lists directory entries (dirs first, then files, sorted),
/// allows descending into directories (Enter) and ascending (Backspace / ".."),
/// optional incremental fuzzy filter (type '/' to search), mouse click selection,
/// and renders each entry with a FileIcons icon + color. TTY-free: draws into a
/// ScreenBuffer only.
/// </summary>
public class FileExplorer : Control
{
    public string CurrentPath { get; set; }
    public bool ShowHidden { get; set; }
    public bool ShowIcons { get; set; } = true;

    /// <summary>Last navigation outcome, set by HandleKey / HandleClick.</summary>
    public FileExplorerAction LastAction { get; private set; } = FileExplorerAction.None;

    /// <summary>Path affected by the last action (entered dir or selected file).</summary>
    public string? SelectedPath { get; private set; }

    private class Entry
    {
        public string Name = "";
        public bool IsDir;
        public string Path = "";
    }

    private readonly List<Entry> _entries = [];
    private int _selected;
    private int _offset;
    private bool _searchMode;
    private string _searchQuery = "";

    public FileExplorer()
    {
        Height = 10;
        CurrentPath = Directory.GetCurrentDirectory();
        Refresh();
    }

    public FileExplorer WithPath(string path)
    {
        CurrentPath = path;
        Refresh();
        return this;
    }

    public FileExplorer WithShowIcons(bool on = true) { ShowIcons = on; return this; }
    public FileExplorer WithShowHidden(bool on = true) { ShowHidden = on; return this; }

    // --- public query helpers (mirror Rust API) ---------------------------

    public string? GetSelected() => _selected < _entries.Count ? _entries[_selected].Path : null;
    public int SelectedIndex => _selected;
    public int Offset => _offset;
    public int EntryCount => _entries.Count;

    /// <summary>Fuzzy-filter entries by query, best-first. Empty query returns all.</summary>
    public List<EntryView> FilterEntries(string query)
    {
        if (string.IsNullOrEmpty(query))
            return _entries.Select(e => new EntryView(e.Name, e.IsDir, e.Path)).ToList();
        var names = _entries.Select(e => e.Name).ToList();
        var scored = FuzzyPattern.Filter(query, names);
        return scored.Select(s => _entries[s.Index])
                    .Select(e => new EntryView(e.Name, e.IsDir, e.Path))
                    .ToList();
    }

    public class EntryView
    {
        public string Name { get; }
        public bool IsDir { get; }
        public string Path { get; }
        public EntryView(string name, bool isDir, string path)
        {
            Name = name; IsDir = isDir; Path = path;
        }
    }

    // --- internals --------------------------------------------------------

    private static string? ParentOf(string path)
    {
        try
        {
            var p = Directory.GetParent(path);
            return p?.FullName;
        }
        catch { return null; }
    }

    private void Refresh()
    {
        _entries.Clear();
        _selected = 0;
        _offset = 0;
        var parent = ParentOf(CurrentPath);
        if (parent != null)
            _entries.Add(new Entry { Name = "..", IsDir = true, Path = parent });

        try
        {
            var paths = Directory.GetFileSystemEntries(CurrentPath);
            var dirs = new List<Entry>();
            var files = new List<Entry>();
            foreach (var p in paths)
            {
                var name = Path.GetFileName(p) ?? p;
                if (!ShowHidden && name.StartsWith("."))
                    continue;
                bool isDir;
                try { isDir = (File.GetAttributes(p) & FileAttributes.Directory) != 0; }
                catch { isDir = false; }
                var e = new Entry { Name = name, IsDir = isDir, Path = p };
                if (isDir) dirs.Add(e); else files.Add(e);
            }
            dirs.Sort((a, b) => string.Compare(a.Name, b.Name, System.StringComparison.OrdinalIgnoreCase));
            files.Sort((a, b) => string.Compare(a.Name, b.Name, System.StringComparison.OrdinalIgnoreCase));
            _entries.AddRange(dirs);
            _entries.AddRange(files);
        }
        catch { }
    }

    private List<int> VisibleIndices()
    {
        if (string.IsNullOrEmpty(_searchQuery))
            return Enumerable.Range(0, _entries.Count).ToList();
        var names = _entries.Select(e => e.Name).ToList();
        return FuzzyPattern.Filter(_searchQuery, names).Select(s => s.Index).ToList();
    }

    private int BodyRows => System.Math.Max(0, Height - 1);

    private void Clamp()
    {
        var vlen = VisibleIndices().Count;
        if (_selected >= vlen) _selected = System.Math.Max(0, vlen - 1);
        if (_selected < _offset) _offset = _selected;
        else if (_selected >= _offset + BodyRows && BodyRows > 0) _offset = _selected + 1 - BodyRows;
        if (_offset < 0) _offset = 0;
    }

    private void Activate()
    {
        var visible = VisibleIndices();
        if (visible.Count == 0 || _selected >= visible.Count) return;
        var e = _entries[visible[_selected]];
        if (e.IsDir)
        {
            CurrentPath = e.Path;
            Refresh();
            LastAction = FileExplorerAction.ChangedDir;
            SelectedPath = e.Path;
        }
        else
        {
            LastAction = FileExplorerAction.Selected;
            SelectedPath = e.Path;
        }
    }

    private string BuildLine(Entry e, bool selected)
    {
        var marker = selected ? ">" : " ";
        var name = e.Name;
        if (ShowIcons)
        {
            var info = FileTheme.IconForPath(e.Path);
            name = info.Icon + FileTheme.IconSeparator + name;
        }
        var prefix = e.IsDir ? "D " : "  ";
        var line = marker + prefix + name;
        var w = System.Math.Max(0, Width);
        if (line.Length < w) line = line.PadRight(w);
        else if (line.Length > w) line = line[..w];
        return ApplyCase(line);
    }

    // --- rendering --------------------------------------------------------

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.FileExplorer, this,
                        extra: _searchMode ? "/" + _searchQuery : CurrentPath,
                        sel: _selected, min: _offset);
    }

    // --- input ------------------------------------------------------------

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        LastAction = FileExplorerAction.None;
        SelectedPath = null;
        if (_searchMode) return HandleSearchKey(key);

        switch (key)
        {
            case KeyConstants.K_UP:
                if (_selected > 0) { _selected--; Clamp(); }
                return true;
            case KeyConstants.K_DOWN:
                var v1 = VisibleIndices().Count;
                if (v1 > 0 && _selected + 1 < v1) { _selected++; Clamp(); }
                return true;
            case KeyConstants.K_PGUP:
                _selected = System.Math.Max(0, _selected - BodyRows);
                Clamp();
                return true;
            case KeyConstants.K_PGDN:
                var v2 = VisibleIndices().Count;
                if (v2 > 0) { _selected = System.Math.Min(v2 - 1, _selected + BodyRows); Clamp(); }
                return true;
            case KeyConstants.K_HOME:
                _selected = 0; Clamp();
                return true;
            case KeyConstants.K_END:
                var v3 = VisibleIndices().Count;
                if (v3 > 0) { _selected = v3 - 1; Clamp(); }
                return true;
            case '/':
                _searchMode = true;
                return true;
            case KeyConstants.K_ENTER:
                Activate();
                return true;
            case KeyConstants.K_BS:
                var parent = ParentOf(CurrentPath);
                if (parent != null)
                {
                    CurrentPath = parent;
                    Refresh();
                    LastAction = FileExplorerAction.ChangedDir;
                    SelectedPath = parent;
                }
                return true;
            default:
                return false;
        }
    }

    private bool HandleSearchKey(int key)
    {
        switch (key)
        {
            case KeyConstants.K_ESC:
                _searchQuery = ""; _searchMode = false; _selected = 0; _offset = 0;
                return true;
            case KeyConstants.K_ENTER:
                _searchMode = false; _selected = 0; _offset = 0;
                return true;
            case KeyConstants.K_BS:
                if (_searchQuery.Length > 0) _searchQuery = _searchQuery[..^1];
                _selected = 0; _offset = 0;
                return true;
            case KeyConstants.K_UP:
                if (_selected > 0) { _selected--; Clamp(); }
                return true;
            case KeyConstants.K_DOWN:
                var v = VisibleIndices().Count;
                if (v > 0 && _selected + 1 < v) { _selected++; Clamp(); }
                return true;
            default:
                if (key is >= 32 and < 127) { _searchQuery += (char)key; _selected = 0; _offset = 0; }
                return true;
        }
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        var row = y - Y - 1;
        if (row < 0) return true;
        var visible = VisibleIndices();
        var idx = _offset + row;
        if (idx >= 0 && idx < visible.Count)
        {
            _selected = idx;
            Clamp();
            Activate();
        }
        return true;
    }

    public override bool HandleMouse(MouseAction action)
    {
        if (!Enabled) return false;
        var maxOffset = Math.Max(0, VisibleIndices().Count - (Height - 2));

        switch (action.Kind)
        {
            case MouseActionKind.ScrollUp:
                if (_offset > 0) { _offset--; }
                return true;
            case MouseActionKind.ScrollDown:
                if (_offset < maxOffset) { _offset++; }
                return true;
            default:
                return base.HandleMouse(action);
        }
    }
}
