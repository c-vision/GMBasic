using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Minimal pure YAML config loader (no external YAML crate). Supports the subset used by
/// the TUI form config: key: value maps, nested maps, lists, and scalar
/// strings/numbers/booleans. TTY-free: parsing never performs console I/O.
/// </summary>
public static class YamlConfig
{
    // ---- Public entry points ----

    /// <summary>Loads a <see cref="FormConfig"/> from a YAML string. TTY-free.</summary>
    public static FormConfig LoadFormFromString(string yaml)
    {
        var root = Parse(yaml);
        if (root.Kind != YamlNodeKind.Map)
            throw new YamlConfigException("Root YAML node must be a map");
        return FormConfig.FromNode(root);
    }

    /// <summary>Loads a <see cref="FormConfig"/> from a YAML file. TTY-free (file read only).</summary>
    public static FormConfig LoadFormFromFile(string path)
    {
        return LoadFormFromString(File.ReadAllText(path));
    }

    /// <summary>Counts the number of editable input fields (text/currency/date/masked), excluding grid and list.</summary>
    public static int CountInputFields(FormConfig config)
    {
        var n = 0;
        foreach (var f in config.Fields)
            if (f is TextField or CurrencyField or DateField or MaskedField) n++;
        return n;
    }

    /// <summary>Returns the names of all fields defined in the form.</summary>
    public static List<string> FieldNames(FormConfig config)
    {
        var names = new List<string>();
        foreach (var f in config.Fields) names.Add(f.Name);
        return names;
    }

    // ---- Parser ----

    public static YamlNode Parse(string yaml)
    {
        var lines = new List<LineInfo>();
        foreach (var raw in yaml.Replace("\r\n", "\n").Split('\n'))
        {
            var line = raw.TrimEnd('\r');
            var trimmed = line.Trim();
            if (trimmed.Length == 0) continue;
            if (trimmed.StartsWith("#")) continue;
            var indent = line.Length - line.TrimStart().Length;
            lines.Add(new LineInfo(indent, line.TrimStart()));
        }
        var i = 0;
        if (lines.Count == 0)
            return new YamlNode(new Dictionary<string, YamlNode>());
        return ParseBlock(lines, ref i, 0);
    }

    private static YamlNode ParseBlock(List<LineInfo> lines, ref int i, int indent)
    {
        if (lines[i].Text.StartsWith("- "))
            return ParseList(lines, ref i, indent);
        return ParseMap(lines, ref i, indent);
    }

    private static YamlNode ParseMap(List<LineInfo> lines, ref int i, int indent)
    {
        var map = new Dictionary<string, YamlNode>();
        while (i < lines.Count && lines[i].Indent == indent && !lines[i].Text.StartsWith("- "))
        {
            var (key, rest) = SplitKey(lines[i].Text);
            if (rest == null)
            {
                i++;
                if (i < lines.Count && lines[i].Indent > indent)
                    map[key] = ParseBlock(lines, ref i, lines[i].Indent);
                else
                    map[key] = new YamlNode(new Dictionary<string, YamlNode>());
            }
            else
            {
                map[key] = YamlNode.FromScalar(rest);
                i++;
            }
        }
        return new YamlNode(map);
    }

    private static YamlNode ParseList(List<LineInfo> lines, ref int i, int indent)
    {
        var list = new List<YamlNode>();
        while (i < lines.Count && lines[i].Indent == indent && lines[i].Text.StartsWith("- "))
        {
            var content = lines[i].Text[2..].TrimStart();
            if (IsMapStart(content))
            {
                lines[i] = new LineInfo(indent + 2, content);
                list.Add(ParseMap(lines, ref i, indent + 2));
            }
            else
            {
                list.Add(YamlNode.FromScalar(content));
                i++;
            }
        }
        return new YamlNode(list);
    }

    private static (string key, string? rest) SplitKey(string text)
    {
        var idx = text.IndexOf(':');
        if (idx < 0) return (text.Trim(), text.Trim());
        var key = text[..idx].Trim();
        var after = text[(idx + 1)..].Trim();
        return after.Length == 0 ? (key, null) : (key, after);
    }

    private static bool IsMapStart(string content)
    {
        var t = content.TrimStart();
        if (t.StartsWith("\"") || t.StartsWith("'")) return false;
        return content.Contains(": ") || content.EndsWith(":");
    }

    private sealed record LineInfo(int Indent, string Text);
}

/// <summary>Error raised while loading or parsing a YAML config.</summary>
public class YamlConfigException : Exception
{
    public YamlConfigException(string message) : base(message) { }
}

public enum YamlNodeKind { Scalar, Map, List }

/// <summary>A parsed YAML value: a scalar string, a string-keyed map, or a list.</summary>
public class YamlNode
{
    public YamlNodeKind Kind { get; }
    public string? Scalar { get; }
    public Dictionary<string, YamlNode>? Map { get; }
    public List<YamlNode>? List { get; }

    public YamlNode(string scalar) { Kind = YamlNodeKind.Scalar; Scalar = Unquote(scalar); }
    public YamlNode(Dictionary<string, YamlNode> map) { Kind = YamlNodeKind.Map; Map = map; }
    public YamlNode(List<YamlNode> list) { Kind = YamlNodeKind.List; List = list; }

    public static YamlNode FromScalar(string s) => new(s);

    public YamlNode? Get(string key) => Map != null && Map.TryGetValue(key, out var v) ? v : null;
    public YamlNode this[int index] => List![index];

    public string AsString() => Scalar ?? string.Empty;
    public bool AsBool() => string.Equals(Scalar, "true", StringComparison.OrdinalIgnoreCase);
    public int AsInt()
    {
        if (int.TryParse(Scalar, out var i)) return i;
        if (double.TryParse(Scalar, System.Globalization.NumberStyles.Any, null, out var d)) return (int)d;
        return 0;
    }
    public double AsDouble()
    {
        if (double.TryParse(Scalar, System.Globalization.NumberStyles.Any, null, out var d)) return d;
        return 0.0;
    }

    private static string Unquote(string s)
    {
        s = s.Trim();
        if (s.Length >= 2 && ((s[0] == '"' && s[^1] == '"') || (s[0] == '\'' && s[^1] == '\'')))
            return s.Substring(1, s.Length - 2);
        return s;
    }
}

// ---- Config model (faithful to the Rust structs) ----

/// <summary>Style of a window border.</summary>
public enum BorderStyle { None, Single, Double, Rounded }

public class FormConfig
{
    public string Title { get; set; } = string.Empty;
    public WindowConfig Window { get; set; } = new();
    public List<FieldConfig> Fields { get; set; } = [];
    public MouseConfig? Mouse { get; set; }

    public static FormConfig FromNode(YamlNode node)
    {
        var cfg = new FormConfig();
        cfg.Title = node.Get("title")?.AsString() ?? string.Empty;
        if (node.Get("window") is { } w) cfg.Window = WindowConfig.FromNode(w);
        if (node.Get("fields") is { Kind: YamlNodeKind.List } fl)
            foreach (var f in fl.List!) cfg.Fields.Add(FieldConfig.FromNode(f));
        if (node.Get("mouse") is { } m) cfg.Mouse = MouseConfig.FromNode(m);
        return cfg;
    }
}

public class WindowConfig
{
    public int X { get; set; }
    public int Y { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }
    public BorderStyle Border { get; set; } = BorderStyle.Rounded;
    public bool Shadow { get; set; }

    public static WindowConfig FromNode(YamlNode node)
    {
        var w = new WindowConfig();
        w.X = node.Get("x")?.AsInt() ?? 0;
        w.Y = node.Get("y")?.AsInt() ?? 0;
        w.Width = node.Get("width")?.AsInt() ?? 0;
        w.Height = node.Get("height")?.AsInt() ?? 0;
        w.Border = ParseBorder(node.Get("border")?.AsString() ?? "rounded");
        w.Shadow = node.Get("shadow")?.AsBool() ?? false;
        return w;
    }

    private static BorderStyle ParseBorder(string s) => s.ToLower() switch
    {
        "none" => BorderStyle.None,
        "single" => BorderStyle.Single,
        "double" => BorderStyle.Double,
        _ => BorderStyle.Rounded,
    };
}

public class MouseConfig
{
    public bool Enabled { get; set; } = true;
    public bool ClickFocus { get; set; } = true;
    public bool DragWindows { get; set; }
    public bool ScrollInGrid { get; set; } = true;

    public static MouseConfig FromNode(YamlNode node)
    {
        return new MouseConfig
        {
            Enabled = node.Get("enabled")?.AsBool() ?? true,
            ClickFocus = node.Get("click_focus")?.AsBool() ?? true,
            DragWindows = node.Get("drag_windows")?.AsBool() ?? false,
            ScrollInGrid = node.Get("scroll_in_grid")?.AsBool() ?? true,
        };
    }
}

public class GridColumnConfig
{
    public string Header { get; set; } = string.Empty;
    public int Width { get; set; }
    public string Align { get; set; } = "left";
    public string? Format { get; set; }
    public bool Editable { get; set; }

    public static GridColumnConfig FromNode(YamlNode node)
    {
        return new GridColumnConfig
        {
            Header = node.Get("header")?.AsString() ?? string.Empty,
            Width = node.Get("width")?.AsInt() ?? 0,
            Align = node.Get("align")?.AsString() ?? "left",
            Format = node.Get("format")?.AsString(),
            Editable = node.Get("editable")?.AsBool() ?? false,
        };
    }
}

public abstract class FieldConfig
{
    public abstract string Type { get; }
    public string Name { get; set; } = string.Empty;
    public string Label { get; set; } = string.Empty;
    public int X { get; set; }
    public int Y { get; set; }

    public static FieldConfig FromNode(YamlNode node)
    {
        var type = node.Get("type")?.AsString() ?? string.Empty;
        FieldConfig f = type switch
        {
            "text" => new TextField(),
            "currency" => new CurrencyField(),
            "date" => new DateField(),
            "masked" => new MaskedField(),
            "grid" => new GridField(),
            "list" => new ListField(),
            _ => throw new YamlConfigException($"Unknown field type: {type}"),
        };
        f.Name = node.Get("name")?.AsString() ?? string.Empty;
        f.Label = node.Get("label")?.AsString() ?? string.Empty;
        f.X = node.Get("x")?.AsInt() ?? 0;
        f.Y = node.Get("y")?.AsInt() ?? 0;
        f.ApplyExtras(node);
        return f;
    }

    protected virtual void ApplyExtras(YamlNode node) { }
}

public class TextField : FieldConfig
{
    public override string Type => "text";
    public int MaxLen { get; set; }
    public int MinLen { get; set; }
    public string? Case { get; set; }
    public string? Allowed { get; set; }
    protected override void ApplyExtras(YamlNode node)
    {
        MaxLen = node.Get("max_len")?.AsInt() ?? 0;
        MinLen = node.Get("min_len")?.AsInt() ?? 0;
        Case = node.Get("case")?.AsString();
        Allowed = node.Get("allowed")?.AsString();
    }
}

public class CurrencyField : FieldConfig
{
    public override string Type => "currency";
    public int Decimals { get; set; }
    public double Min { get; set; } = double.MinValue;
    public double Max { get; set; } = double.MaxValue;
    protected override void ApplyExtras(YamlNode node)
    {
        Decimals = node.Get("decimals")?.AsInt() ?? 0;
        Min = node.Get("min")?.AsDouble() ?? double.MinValue;
        Max = node.Get("max")?.AsDouble() ?? double.MaxValue;
    }
}

public class DateField : FieldConfig
{
    public override string Type => "date";
    public string Format { get; set; } = string.Empty;
    public bool Century { get; set; } = true;
    protected override void ApplyExtras(YamlNode node)
    {
        Format = node.Get("format")?.AsString() ?? string.Empty;
        Century = node.Get("century")?.AsBool() ?? true;
    }
}

public class MaskedField : FieldConfig
{
    public override string Type => "masked";
    public string Mask { get; set; } = string.Empty;
    protected override void ApplyExtras(YamlNode node) => Mask = node.Get("mask")?.AsString() ?? string.Empty;
}

public class GridField : FieldConfig
{
    public override string Type => "grid";
    public int Width { get; set; }
    public int Height { get; set; }
    public List<GridColumnConfig> Columns { get; set; } = [];
    public int Frozen { get; set; }
    public bool MouseScroll { get; set; } = true;
    protected override void ApplyExtras(YamlNode node)
    {
        Width = node.Get("width")?.AsInt() ?? 0;
        Height = node.Get("height")?.AsInt() ?? 0;
        Frozen = node.Get("frozen")?.AsInt() ?? 0;
        MouseScroll = node.Get("mouse_scroll")?.AsBool() ?? true;
        if (node.Get("columns") is { Kind: YamlNodeKind.List } cl)
            foreach (var c in cl.List!) Columns.Add(GridColumnConfig.FromNode(c));
    }
}

public class ListField : FieldConfig
{
    public override string Type => "list";
    public int Width { get; set; }
    public int Height { get; set; }
    public bool MultiSelect { get; set; }
    public bool Searchable { get; set; } = true;
    public List<string>? Items { get; set; }
    protected override void ApplyExtras(YamlNode node)
    {
        Width = node.Get("width")?.AsInt() ?? 0;
        Height = node.Get("height")?.AsInt() ?? 0;
        MultiSelect = node.Get("multi_select")?.AsBool() ?? false;
        Searchable = node.Get("searchable")?.AsBool() ?? true;
        if (node.Get("items") is { Kind: YamlNodeKind.List } il)
            Items = il.List!.ConvertAll(n => n.AsString());
    }
}
