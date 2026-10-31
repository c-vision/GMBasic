using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Globalization;
using System.Text;

namespace ClipperLib.Clipper;

/// <summary>Flag e template di un PICTURE Clipper 5.3, parsati da <see cref="FormatPicture.Parse"/>.</summary>
public sealed class PictureSpec
{
    // @-functions (GET e SAY)
    public bool Alpha;                // @A — solo alfabetico (GET)
    public bool LeftJustifyNumbers;   // @B — numeri allineati a sinistra
    public bool CrAfterPositive;      // @C — "CR" dopo i positivi
    public bool DateSetFormat;        // @D — data in SET DATE FORMAT
    public bool DateBritish;          // @E — data britannica (dd/mm/yy)
    public bool European;             // @F — formato numerico europeo (virgola decimale)
    public bool InsertMode;           // @I — modalità inserimento (GET)
    public bool JustifyRight;         // @J — testo allineato a destra
    public bool Smart;                // @K — cursore intelligente (GET)
    public bool LiteralInsert;        // @R — i caratteri letterali non fanno parte del valore
    public bool Scroll;               // @S<n> — campo a scorrimento (GET)
    public int ScrollWidth;
    public bool Trim;                 // @T — taglia spazi iniziali/finali
    public bool DbAfterNegatives;     // @X — "DB" dopo i negativi
    public bool ZeroBlank;            // @Z — zero come spazi
    public bool ParensWithLeading;    // @( — negativi tra parentesi con spazio iniziale
    public bool ParensNoLeading;      // @) — negativi tra parentesi senza spazio
    public bool Currency;             // @$ — simbolo dollaro flottante
    public bool Upper;                // @! — maiuscolo
    public char FillChar = ' ';       // @L<c> — carattere di riempimento numerico

    public string Template = "";      // template dopo le @-funzioni
    public bool IsNumeric;
    public bool IsDate;
    public int Decimals;
    public bool HasThousands;
    public char ThousandsSep = ',';
    public char DecimalSep = '.';
    public char FloatingFill;         // '$', '*' o '\0'

    /// <summary>True se il PICTURE inizia con '@' e ha almeno una funzione.</summary>
    public bool HasFunctions { get; internal set; }
}

/// <summary>
/// PICTURE Clipper 5.3 completo: tutte le @-funzioni (A B C D E F I J K L&lt;c&gt; N R S&lt;n&gt; T X Z ( ) $ !)
/// e i simboli di template (A N X 9 # L Y ! $ * . , 0). Usato da SAY/@...SAY, TRANSFORM() e dal
/// sistema GET per l'editing.
/// </summary>
public static class FormatPicture
{
    private const string FunctionChars = "ABCDEFIJKLNRSTXZ()$!";

    /// <summary>Analizza un PICTURE in <see cref="PictureSpec"/>.</summary>
    public static PictureSpec Parse(string picture)
    {
        var p = new PictureSpec();
        if (string.IsNullOrEmpty(picture))
        {
            p.Template = "";
            return p;
        }

        var idx = 0;
        if (picture[0] == '@')
        {
            p.HasFunctions = true;
            idx = 1;
            while (idx < picture.Length)
            {
                var c = char.ToUpperInvariant(picture[idx]);
                // Uno spazio separa le @-funzioni dal template; si prosegue solo se
                // dopo gli spazi c'è ancora un '@' (es. "@R @S5 @K"). In "@R (999)"
                // la '(' è un letterale del template, non una @-funzione.
                if (c == '@')
                {
                    idx++;
                    continue;
                }
                if (c == ' ')
                {
                    var j = idx;
                    while (j < picture.Length && picture[j] == ' ') j++;
                    if (j < picture.Length && picture[j] == '@')
                    {
                        idx = j;
                        continue;
                    }
                    break;
                }
                if (c == 'S' && idx + 1 < picture.Length && char.IsDigit(picture[idx + 1]))
                {
                    p.Scroll = true;
                    var j = idx + 1;
                    while (j < picture.Length && char.IsDigit(picture[j])) j++;
                    p.ScrollWidth = int.Parse(picture[(idx + 1)..j]);
                    idx = j;
                    continue;
                }
                if (c == 'L' && idx + 1 < picture.Length)
                {
                    p.FillChar = picture[idx + 1];
                    idx += 2;
                    continue;
                }
                if (FunctionChars.Contains(c))
                {
                    ApplyFunction(p, c);
                    idx++;
                    continue;
                }
                break;
            }
            if (idx < picture.Length && picture[idx] == ' ')
                idx++;
        }

        p.Template = picture[idx..];

        var t = p.Template;
        p.IsNumeric = t.Length > 0 && t.All(ch => "9#$*., 0()".Contains(ch)) && t.Any(ch => "9#".Contains(ch));
        p.IsDate = (p.DateSetFormat || p.DateBritish)
                   || (t.Length >= 6 && t.All(ch => char.IsDigit(ch) || "/-.: ".Contains(ch))
                       && t.Any(ch => "/-.:".Contains(ch)));

        if (p.IsNumeric)
        {
            var decSep = p.European ? ',' : '.';
            var thSep = p.European ? '.' : ',';
            p.DecimalSep = decSep;
            p.ThousandsSep = thSep;

            // separatore decimale: se ci sono entrambi i separatori è il più a destra;
            // se c'è solo uno, è decimale solo se ha cifre dopo.
            var hasDecSep = t.Contains(decSep);
            var hasThSep = t.Contains(thSep);
            int decIdx;
            if (hasDecSep && hasThSep)
            {
                decIdx = Math.Max(t.LastIndexOf(decSep), t.LastIndexOf(thSep));
            }
            else if (hasDecSep)
            {
                var i = t.LastIndexOf(decSep);
                decIdx = t[(i + 1)..].Any(ch => "9#0".Contains(ch)) ? i : -1;
            }
            else
            {
                decIdx = -1;
            }

            if (decIdx >= 0)
            {
                p.Decimals = t[(decIdx + 1)..].Count(ch => "9#0".Contains(ch));
                p.HasThousands = t[..decIdx].Contains(thSep);
            }
            else
            {
                p.Decimals = 0;
                p.HasThousands = t.Contains(thSep);
            }

            if (p.Currency) p.FloatingFill = '$';
            else if (t.Length > 0 && t[0] == '*') p.FloatingFill = '*';
        }
        else if (p.IsDate && (p.DateSetFormat || p.DateBritish) && string.IsNullOrEmpty(t))
        {
            p.Template = p.DateBritish ? "dd/mm/yy" : ClipperSet.SetDateFormat();
        }

        return p;
    }

    private static void ApplyFunction(PictureSpec p, char fn)
    {
        switch (fn)
        {
            case 'A': p.Alpha = true; break;
            case 'B': p.LeftJustifyNumbers = true; break;
            case 'C': p.CrAfterPositive = true; break;
            case 'D': p.DateSetFormat = true; break;
            case 'E': p.DateBritish = true; break;
            case 'F': p.European = true; break;
            case 'I': p.InsertMode = true; break;
            case 'J': p.JustifyRight = true; break;
            case 'K': p.Smart = true; break;
            case 'N': p.JustifyRight = true; break;
            case 'R': p.LiteralInsert = true; break;
            case 'T': p.Trim = true; break;
            case 'X': p.DbAfterNegatives = true; break;
            case 'Z': p.ZeroBlank = true; break;
            case '(': p.ParensWithLeading = true; break;
            case ')': p.ParensNoLeading = true; break;
            case '$': p.Currency = true; break;
            case '!': p.Upper = true; break;
        }
    }

    /// <summary>Applica il PICTURE a un valore qualsiasi (stringa, numero, data, bool).</summary>
    public static string Apply(object value, string picture)
    {
        var p = Parse(picture);
        return value switch
        {
            DateTime d => FormatDate(d, p),
            bool b => b ? "T" : "F",
            int i => FormatNumber(i, p),
            long l => FormatNumber(l, p),
            double d2 => FormatNumber(d2, p),
            decimal m => FormatNumber((double)m, p),
            float f => FormatNumber(f, p),
            _ => FormatText(value?.ToString() ?? "", p),
        };
    }

    /// <summary>Applica il PICTURE a una stringa.</summary>
    public static string Apply(string value, string picture)
    {
        var p = Parse(picture);
        if (p.IsNumeric)
        {
            if (p.LiteralInsert ||
                !double.TryParse(value.Trim(), NumberStyles.Any, CultureInfo.InvariantCulture, out var n))
                return FormatPlaceholders(value ?? "", p);
            return FormatNumber(n, p);
        }
        return FormatText(value ?? "", p);
    }

    /// <summary>Applica il PICTURE a un numero.</summary>
    public static string Apply(double value, string picture) => FormatNumber(value, Parse(picture));

    /// <summary>Applica il PICTURE a un numero decimale.</summary>
    public static string Apply(decimal value, string picture) => FormatNumber((double)value, Parse(picture));

    /// <summary>Applica il PICTURE a un intero.</summary>
    public static string Apply(int value, string picture) => FormatNumber(value, Parse(picture));

    /// <summary>Applica il PICTURE a una data.</summary>
    public static string Apply(DateTime value, string picture) => FormatDate(value, Parse(picture));

    // ---- internals ----

    private static string FormatNumber(double value, PictureSpec p)
    {
        if (p.IsDate) return FormatDate(DateTime.Today, p);
        var width = Math.Max(p.Template.Length, 1);
        if (p.ZeroBlank && value == 0)
            return new string(' ', width);

        var dec = Math.Clamp(p.Decimals, 0, 15);
        var s = Math.Abs(value).ToString("F" + dec, CultureInfo.InvariantCulture);
        var dot = s.IndexOf('.');
        var intPart = dot < 0 ? s : s[..dot];
        var frac = dot < 0 ? "" : s[(dot + 1)..];

        if (p.HasThousands && intPart.Length > 3)
            intPart = AddThousands(intPart, p.ThousandsSep);

        var body = dec > 0 ? intPart + p.DecimalSep + frac : intPart;
        var neg = value < 0;
        var fill = p.FillChar == ' ' ? ' ' : p.FillChar;

        var basePadded = p.LeftJustifyNumbers ? body.PadRight(width, fill) : body.PadLeft(width, fill);

        // $ / * flottante: riempie gli spazi prima della prima cifra
        if (p.FloatingFill != '\0')
        {
            var fd = FindFirstDigit(basePadded);
            var arr = basePadded.ToCharArray();
            for (var i = 0; i < fd && (arr[i] == ' ' || arr[i] == fill); i++)
                arr[i] = p.FloatingFill;
            basePadded = new string(arr);
        }

        string signed;
        if (neg)
        {
            if (p.ParensWithLeading) signed = "(" + basePadded + " )";
            else if (p.ParensNoLeading) signed = "(" + basePadded + ")";
            else if (p.DbAfterNegatives) signed = basePadded + " DB";
            else signed = InsertMinus(basePadded, fill);
        }
        else if (p.CrAfterPositive)
        {
            signed = basePadded + " CR";
        }
        else
        {
            signed = basePadded;
        }

        return signed;
    }

    private static int FindFirstDigit(string s)
    {
        for (var i = 0; i < s.Length; i++)
            if (char.IsDigit(s[i])) return i;
        return s.Length;
    }

    private static string InsertMinus(string s, char fill)
    {
        var first = FindFirstDigit(s);
        if (first == 0) return "-" + s;
        if (first < s.Length && s[first - 1] is ' ' or '$' or '*')
        {
            var arr = s.ToCharArray();
            arr[first - 1] = '-';
            return new string(arr);
        }
        return s.Insert(first, "-");
    }

    private static string AddThousands(string intPart, char sep)
    {
        var neg = intPart.StartsWith('-');
        var digits = neg ? intPart[1..] : intPart;
        var sb = new StringBuilder();
        var count = 0;
        for (var i = digits.Length - 1; i >= 0; i--)
        {
            if (count > 0 && count % 3 == 0) sb.Insert(0, sep);
            sb.Insert(0, digits[i]);
            count++;
        }
        return neg ? "-" + sb : sb.ToString();
    }

    private static string FormatDate(DateTime d, PictureSpec p)
    {
        if (p.DateSetFormat)
            return ClipperDates.Dtoc(d);
        if (p.DateBritish)
            return ClipperSet.SetCentury()
                ? d.ToString("dd/MM/yyyy", CultureInfo.InvariantCulture)
                : d.ToString("dd/MM/yy", CultureInfo.InvariantCulture);
        if (p.IsDate && p.Template.Length >= 6)
            return FormatDateTemplate(d, p.Template);
        return d.ToShortDateString();
    }

    /// <summary>Compila un template data tipo "99/99/9999" o "99-99-99".</summary>
    private static string FormatDateTemplate(DateTime d, string template)
    {
        var sb = new StringBuilder();
        var groups = new List<int>();
        var cur = 0;
        foreach (var c in template)
        {
            if (char.IsDigit(c)) cur++;
            else
            {
                if (cur > 0) { groups.Add(cur); cur = 0; }
                sb.Append(c);
            }
        }
        if (cur > 0) groups.Add(cur);
        if (groups.Count < 3) return d.ToShortDateString();

        var parts = new[] { d.Day.ToString("00"), d.Month.ToString("00"),
                            groups[2] >= 4 ? d.Year.ToString("0000") : (d.Year % 100).ToString("00") };
        var outIdx = 0;
        var gIdx = 0;
        var result = new StringBuilder();
        foreach (var c in template)
        {
            if (char.IsDigit(c))
            {
                if (gIdx < 3 && outIdx < parts[gIdx].Length)
                    result.Append(parts[gIdx][outIdx]);
                else
                    result.Append(' ');
                outIdx++;
                if (outIdx >= (groups.Count > gIdx ? groups[gIdx] : 0))
                {
                    outIdx = 0;
                    gIdx++;
                }
            }
            else
            {
                result.Append(c);
            }
        }
        return result.ToString();
    }

    private static string FormatText(string value, PictureSpec p)
    {
        if (p.Upper) value = value.ToUpperInvariant();
        if (p.Trim) value = value.Trim();

        var template = p.Template;
        if (string.IsNullOrEmpty(template))
        {
            if (p.JustifyRight) return value.TrimStart().PadLeft(Math.Max(value.Length, 1));
            return value;
        }

        // @J: il testo è allineato a destra dentro il template (riempimento da destra).
        if (p.JustifyRight)
        {
            var sb = new StringBuilder();
            var v = value;
            var vi = v.Length - 1;
            for (var i = template.Length - 1; i >= 0; i--)
            {
                var t = template[i];
                if (!IsPlaceholder(t))
                {
                    sb.Insert(0, t);
                    continue;
                }
                var c = vi >= 0 ? v[vi--] : ' ';
                if (p.Upper && char.IsLetter(c)) c = char.ToUpperInvariant(c);
                sb.Insert(0, MatchTemplate(c, t) ? c : ' ');
            }
            var jr = sb.ToString();
            return p.Trim ? jr.Trim() : jr;
        }

        var sb2 = new StringBuilder();
        var vi2 = 0;
        foreach (var t in template)
        {
            if (IsPlaceholder(t))
            {
                var c = vi2 < value.Length ? value[vi2++] : ' ';
                var ok = MatchTemplate(c, t);
                if (p.Upper && char.IsLetter(c)) c = char.ToUpperInvariant(c);
                sb2.Append(ok ? c : ' ');
            }
            else
            {
                sb2.Append(t);
            }
        }

        var result = sb2.ToString();
        if (p.Trim) result = result.Trim();
        return result;
    }

    /// <summary>
    /// Riempe i placeholder del template da destra a sinistra mantenendo i letterali
    /// (es. "@R (999) 999" con "123456" → "(123) 456"). Usato da @R e dai template
    /// numerici con valore non numerico.
    /// </summary>
    private static string FormatPlaceholders(string value, PictureSpec p)
    {
        if (p.Upper) value = value.ToUpperInvariant();
        var template = p.Template;
        var slots = new List<int>();
        for (var i = 0; i < template.Length; i++)
            if (IsPlaceholder(template[i])) slots.Add(i);

        var chars = value.ToCharArray();
        var arr = template.ToCharArray();
        var vi = chars.Length - 1;
        for (var i = slots.Count - 1; i >= 0; i--)
        {
            if (vi >= 0)
            {
                var c = chars[vi--];
                if (p.Upper && char.IsLetter(c)) c = char.ToUpperInvariant(c);
                arr[slots[i]] = c;
            }
            else
            {
                arr[slots[i]] = ' ';
            }
        }
        return new string(arr);
    }

    internal static int PlaceholderCount(PictureSpec p) => p.Template.Count(IsPlaceholder);

    /// <summary>True se il template contiene almeno un placeholder riconosciuto (A N X 9 # L Y ! 0).</summary>
    internal static bool HasPlaceholders(PictureSpec p) => p.Template.Any(IsPlaceholder);

    /// <summary>Carattere template del placeholder di indice <paramref name="rawIdx"/> (0-based, solo i placeholder, esclusi i letterali).</summary>
    internal static char PlaceholderTypeAt(PictureSpec p, int rawIdx)
    {
        var n = 0;
        for (var i = 0; i < p.Template.Length; i++)
        {
            if (!IsPlaceholder(p.Template[i])) continue;
            if (n == rawIdx) return p.Template[i];
            n++;
        }
        return '\0';
    }

    /// <summary>
    /// Verifica che <paramref name="ch"/> sia accettato dal placeholder alla posizione
    /// grezza <paramref name="rawIdx"/>: rispetta il PICTURE NEL keypress, non a
    /// posteriori. Un template senza placeholder riconoscibili (es. @D con data) non
    /// impone vincoli; oltre l'ultimo slot il carattere viene rifiutato.
    /// </summary>
    internal static bool TemplateAccepts(PictureSpec p, char ch, int rawIdx)
    {
        var t = PlaceholderTypeAt(p, rawIdx);
        if (t == '\0') return !HasPlaceholders(p);
        return t switch
        {
            'A' => char.IsLetter(ch),
            'N' => char.IsLetterOrDigit(ch),
            'X' or '!' => true,
            '9' => char.IsDigit(ch),
            '0' => char.IsDigit(ch),
            '#' => char.IsDigit(ch) || ch is '-' or '+',
            'L' => char.ToUpperInvariant(ch) is 'T' or 'F',
            'Y' => char.ToUpperInvariant(ch) is 'Y' or 'N',
            _ => true,
        };
    }

    /// <summary>Posizione nel template formattato del carattere grezzo di indice <paramref name="rawIdx"/> (editing @R, sinistra→destra).</summary>
    internal static int MapToDisplay(int rawIdx, PictureSpec p)
    {
        var n = -1;
        for (var i = 0; i < p.Template.Length; i++)
        {
            if (IsPlaceholder(p.Template[i]))
            {
                n++;
                if (n == rawIdx) return i;
            }
        }
        return p.Template.Length;
    }

    private static bool IsPlaceholder(char t) => t is 'A' or 'N' or 'X' or '9' or '#' or 'L' or 'Y' or '!' or '0';

    private static bool MatchTemplate(char c, char t) => t switch
    {
        'A' => char.IsLetter(c),
        'N' => char.IsLetterOrDigit(c),
        'X' => true,
        '9' => char.IsDigit(c) || c == ' ',
        '#' => char.IsDigit(c) || c == ' ' || c == '-',
        'L' => c is 'T' or 'F' or ' ',
        'Y' => c is 'Y' or 'N' or ' ',
        '!' => true,
        '0' => char.IsDigit(c) || c == ' ',
        _ => true,
    };
}
