using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;
using ClipperLib.Clipper;
// ReSharper disable UnusedMember.Global
// ReSharper disable InconsistentNaming
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable UnusedAutoPropertyAccessor.Global

namespace ClipperLib.Inputs;

/// <summary>
/// Kind of data a mask template is editing/formatting. Mirrors the Rust
/// <c>ParsedDataType</c> used by the maxxlib mask machinery.
/// </summary>
public enum MaskDataType
{
    Text,
    Integer,
    Float,
    Date,
    Time,
    DateTime,
    Bool
}

/// <summary>
/// A single position of a parsed mask: either a literal character that is
/// displayed verbatim (and auto-inserted as the user types) or an editable
/// placeholder slot that validates/transforms input.
/// </summary>
public sealed class MaskSlot
{
    public bool IsLiteral;
    public char Literal;
    public char Symbol;
    public bool ForceUpper;
}

/// <summary>
/// A Clipper-style input mask. Parses a template string that may contain a
/// PICTURE function part (prefixed with <c>@</c>, e.g. <c>@!</c>, <c>@R</c>)
/// and a template part made of literal characters plus placeholder symbols
/// (<c>9</c> digit, <c>A</c> alpha, <c>X</c> any, <c>N</c> alnum, <c>L</c> T/F,
/// <c>Y</c> Y/N, <c>0</c> digit, <c>#</c> digit/sign/space, <c>!</c> uppercase).
/// Pure, TTY-free.
/// </summary>
public sealed class Mask
{
    // Editable placeholder symbols. Everything else (' ', '.', ',', '$', '*',
    // '/', '-', ':' ...) is treated as a literal that is auto-inserted.
    private static readonly HashSet<char> Editable = ['A', 'N', 'X', '9', '#', '0', 'L', 'Y', '!'];

    // Function identifiers that may follow '@'.
    private const string FuncIds = "AKSBCDEFLRXZ()$!";

    public string RawMask { get; }
    public string Template { get; private set; } = "";
    public string Picture { get; private set; } = "";

    public bool ForceUppercase { get; private set; }
    public bool InsertLiterals { get; private set; }
    public bool LeftJustified { get; private set; }
    public bool ZeroAsBlank { get; private set; }
    public bool EuropeanNumbers { get; private set; }
    public int VisibleSize { get; private set; }
    public char FillChar { get; private set; } = ' ';
    public bool AlphaOnly { get; private set; }

    public MaskDataType DataType { get; private set; } = MaskDataType.Text;
    public List<MaskSlot> Slots { get; } = [];

    public Mask(string mask) : this(mask, false) { }

    public Mask(string mask, bool clipper)
    {
        RawMask = mask;
        if (clipper) ParseClipper(RawMask);
        else ParseExtended(RawMask);
        BuildSlots();
        DetectDataType();
    }

    // ---- parsing -----------------------------------------------------------

    private void ParseExtended(string mask)
    {
        var pic = new StringBuilder();
        var tmpl = new StringBuilder();
        var i = 0;
        while (i < mask.Length)
        {
            if (mask[i] == '@' && i + 1 < mask.Length && FuncIds.Contains(mask[i + 1]))
            {
                var fn = mask[i + 1];
                pic.Append('@');
                pic.Append(fn);
                i += 2;
                if (fn == 'S')
                {
                    var digs = new string(mask.Skip(i).TakeWhile(char.IsDigit).ToArray());
                    if (digs.Length > 0) { VisibleSize = int.Parse(digs); i += digs.Length; }
                }
                else if (fn == 'L')
                {
                    if (i < mask.Length) { FillChar = mask[i]; i++; }
                }
            }
            else
            {
                tmpl.Append(mask[i]);
                i++;
            }
        }
        Picture = pic.ToString();
        Template = tmpl.ToString();
        ApplyFlags();
    }

    private void ParseClipper(string mask)
    {
        var input = mask.TrimStart();
        var picture = "";
        var templateBody = input;
        if (input.Length > 1 && input[0] == '@' && FuncIds.Contains(input[1]))
        {
            picture = input[..2];
            var consumed = 2;
            if (input.Length > 2)
            {
                if (picture == "@S")
                {
                    var digs = new string(input.Skip(2).TakeWhile(char.IsDigit).ToArray());
                    if (digs.Length > 0) { VisibleSize = int.Parse(digs); consumed = 2 + digs.Length; }
                    else picture = "";
                }
                else if (picture == "@L")
                {
                    FillChar = input[2];
                    consumed = 3;
                }
            }
            templateBody = input[consumed..];
            if (templateBody.Length > 0 && templateBody[0] != ' ')
            {
                picture = "";
                templateBody = input;
            }
            else if (templateBody.Length > 0)
            {
                templateBody = templateBody[1..];
            }
        }
        Picture = picture;
        Template = templateBody;
        ApplyFlags();
    }

    private void ApplyFlags()
    {
        ForceUppercase = Picture.Contains("!");
        InsertLiterals = Picture.Contains("R");
        LeftJustified = Picture.Contains("B");
        ZeroAsBlank = Picture.Contains("Z");
        EuropeanNumbers = Picture.Contains("F");
        AlphaOnly = Picture == "@A";
    }

    private void BuildSlots()
    {
        Slots.Clear();
        foreach (var c in Template)
        {
            Slots.Add(Editable.Contains(c)
                ? new MaskSlot { IsLiteral = false, Symbol = c, ForceUpper = c == '!' }
                : new MaskSlot { IsLiteral = true, Literal = c, Symbol = c });
        }
    }

    private void DetectDataType()
    {
        if (string.IsNullOrEmpty(Template)) { DataType = MaskDataType.Text; return; }
        if (Template is "L" or "Y") { DataType = MaskDataType.Bool; return; }

        var allNum = Template.All(c => "09#$*,.() -".Contains(c));
        if (allNum)
        {
            DataType = Template.Contains('.') || Template.Contains(',') ? MaskDataType.Float : MaskDataType.Integer;
            return;
        }
        var allDate = Template.All(c => "0/-.: ".Contains(c));
        if (allDate)
        {
            if (Template.Contains(':'))
                DataType = Template.Contains(' ') ? MaskDataType.DateTime : MaskDataType.Time;
            else
                DataType = MaskDataType.Date;
            return;
        }
        DataType = MaskDataType.Text;
    }

    // ---- validation / transform -------------------------------------------

    private static bool Validate(char c, char sym, MaskDataType dt)
    {
        switch (sym)
        {
            case 'A': return char.IsLetter(c);
            case 'N': return char.IsLetterOrDigit(c);
            case 'X': return true;
            case 'L': return c is 'T' or 'F';
            case 'Y': return c is 'Y' or 'N';
            case '0': return char.IsDigit(c);
            case '9': return char.IsDigit(c) || c == '-';
            case '#': return char.IsDigit(c) || c == '-' || char.IsWhiteSpace(c);
            case '!': return char.IsLetterOrDigit(c);
            default: return false;
        }
    }

    internal char Transform(char c, int pos)
    {
        if (pos < 0 || pos >= Slots.Count) return c;
        var s = Slots[pos];
        if ((ForceUppercase || s.ForceUpper) && char.IsLetter(c))
            return char.ToUpperInvariant(c);
        return c;
    }

    /// <summary>True if <paramref name="c"/> may be placed at slot <paramref name="pos"/>.</summary>
    public bool IsValidChar(char c, int pos)
    {
        if (pos < 0 || pos >= Slots.Count) return false;
        var s = Slots[pos];
        if (s.IsLiteral) return false;
        if (AlphaOnly && !char.IsLetter(c)) return false;
        return Validate(c, s.Symbol, DataType);
    }

    /// <summary>
    /// Apply <paramref name="input"/> to the mask: literals are auto-inserted,
    /// input characters are validated against their slot and case-transformed.
    /// Invalid characters are skipped; unfilled slots are padded with blanks.
    /// </summary>
    public string Apply(string input)
    {
        input = input ?? "";
        var sb = new StringBuilder();
        var si = 0;
        var vi = 0;
        while (si < Slots.Count)
        {
            var s = Slots[si];
            if (s.IsLiteral) { sb.Append(s.Literal); si++; continue; }
            if (vi < input.Length)
            {
                var c = input[vi];
                if (Validate(c, s.Symbol, DataType)) { sb.Append(Transform(c, si)); vi++; si++; continue; }
                vi++; // reject char, keep trying remaining input at this slot
                continue;
            }
            sb.Append(' ');
            si++;
        }
        return sb.ToString();
    }

    /// <summary>
    /// Format <paramref name="value"/> using the full Clipper PICTURE (function
    /// part + template) by delegating to <see cref="FormatPicture"/>.
    /// </summary>
    public string Format(string value)
    {
        var pic = string.IsNullOrEmpty(Template)
            ? Picture
            : (Picture + " " + Template).Trim();
        if (string.IsNullOrWhiteSpace(pic)) return value;
        return FormatPicture.Apply(value ?? "", pic);
    }

    /// <summary>Empty (fully masked but unfilled) representation.</summary>
    public string Empty() => Apply("");
}

/// <summary>
/// Stateful helper that drives a masked edit field: given the current displayed
/// text, the cursor position and a typed key it computes the next text and
/// cursor. Handles printable characters and backspace (<c>'\b'</c>).
/// Literal slots are skipped automatically.
/// </summary>
public static class MaskedInput
{
    public static (string text, int cursor) ProcessKey(string text, int cursor, char key, Mask mask)
    {
        if (mask == null) return (text, cursor);
        var n = mask.Slots.Count;

        if (key == '\b')
        {
            var p = cursor - 1;
            while (p >= 0 && mask.Slots[p].IsLiteral) p--;
            if (p < 0) return (text, cursor);
            var arr = (text ?? "").ToCharArray();
            if (p < arr.Length) arr[p] = ' ';
            return (new string(arr), p);
        }

        var pos = cursor;
        while (pos < n && mask.Slots[pos].IsLiteral) pos++;
        if (pos >= n) return (text, cursor);
        if (!mask.IsValidChar(key, pos)) return (text, cursor);

        var a = (text ?? mask.Empty()).ToCharArray();
        if (pos < a.Length) a[pos] = mask.Transform(key, pos);

        var nc = pos + 1;
        while (nc < n && mask.Slots[nc].IsLiteral) nc++;
        return (new string(a), nc);
    }
}
