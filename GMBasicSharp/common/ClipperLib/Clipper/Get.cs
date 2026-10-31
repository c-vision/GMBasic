using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

public class Get
{
    public int Row { get; set; }
    public int Col { get; set; }
    public string Value { get; set; } = "";
    public string? Picture { get; set; }
    public string? Color { get; set; }
    public string? FocusColor { get; set; }
    public string? ErrorColor { get; set; }
    public string? Message { get; set; }
    public (double lo, double hi)? Range { get; set; }
    public Func<string, bool>? Valid { get; set; }
    /// <summary>Blocca il tasto nel keypress: il carattere viene scartato (con beep) se il predicato è false.</summary>
    public Func<char, bool>? AcceptKey { get; set; }
    /// <summary>Callback invocato quando un carattere viene rifiutato nel keypress (template PICTURE, @A, AcceptKey).</summary>
    public Action<char>? Rejected { get; set; }
    public Func<bool>? When { get; set; }
    public bool Changed { get; set; }
    public bool Updated { get; set; }
    public int Cur { get; set; }
    public int Sel { get; set; }
    public Action<string> Apply { get; set; } = _ => { };
    public int? Width { get; set; }
    public char Fill { get; set; } = ' ';

    // --- Controlli TYPE (Spinner/ComboBox/CheckBox/DatePicker/ListBox) ---
    /// <summary>Tipo di controllo: "TextBox" (default), "Spinner", "ComboBox", "CheckBox", "DatePicker", "ListBox".</summary>
    public string? ControlType { get; set; }
    public List<string>? Items { get; set; }
    public string? Caption { get; set; }
    public double? Min { get; set; }
    public double? Max { get; set; }
    public double? Step { get; set; }
    public int? Height { get; set; }

    /// <summary>Indirizzo P-Code della subroutine del predicato VALID (0 = nessuno). La VM la
    /// chiama rivalutando l'espressione col valore corrente del campo (variabile già applicata).</summary>
    public int? ValidAddress { get; set; }
    public int ScrollPos { get; set; }

    internal PictureSpec? Pic => _pic ??= Picture != null ? FormatPicture.Parse(Picture) : null;
    private PictureSpec? _pic;
}

public class GetBuilder
{
    private Get _get;

    public GetBuilder(int row, int col, string value, Action<string> apply)
    {
        _get = new Get { Row = row, Col = col, Value = value, Apply = apply };
    }

    public GetBuilder Picture(string p) { _get.Picture = p; return this; }
    public GetBuilder Color(string c) { _get.Color = c; return this; }
    public GetBuilder FocusColor(string c) { _get.FocusColor = c; return this; }
    public GetBuilder ErrorColor(string c) { _get.ErrorColor = c; return this; }
    public GetBuilder Message(string m) { _get.Message = m; return this; }
    public GetBuilder Valid(Func<string, bool> f) { _get.Valid = f; return this; }
    public GetBuilder AcceptKey(Func<char, bool> f) { _get.AcceptKey = f; return this; }
    public GetBuilder Rejected(Action<char> f) { _get.Rejected = f; return this; }
    public GetBuilder When(Func<bool> f) { _get.When = f; return this; }
    public GetBuilder Range(double lo, double hi) { _get.Range = (lo, hi); return this; }
    public GetBuilder Width(int w) { _get.Width = w; return this; }
    public GetBuilder Fill(char ch) { _get.Fill = ch; return this; }
    public Get Build() => _get;

    public static GetBuilder ForValue(int row, int col, ref int var)
    {
        var v = var.ToString();
        var captured = var;
        Action<string> apply = s =>
        {
            if (int.TryParse(s, out var parsed))
                captured = parsed;
        };
        return new GetBuilder(row, col, v, apply);
    }

    public static GetBuilder ForString(int row, int col, ref string var)
    {
        var v = var;
        var captured = var;
        Action<string> apply = s => captured = s;
        return new GetBuilder(row, col, v, apply);
    }
}
