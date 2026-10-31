using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Globalization;
using System.Text;

namespace ClipperLib.Clipper;

/// <summary>
/// Funzioni stringa di Clipper 5.3: AT/RAT, SUBSTR/STUFF/STRTRAN, LEFT/RIGHT,
/// UPPER/LOWER, ISALPHA/ISDIGIT/ISLOWER/ISUPPER, LEN, TRIM/LTRIM/RTRIM/ALLTRIM,
/// PADL/PADC/PADR, REPLICATE, SPACE, TRANSFORM, VAL, STR, STRZERO, TYPE, VALTYPE,
/// EMPTY, ASC, CHR. Gli indici di posizione sono a base 1 come in Clipper.
/// </summary>
public static class ClipperStrings
{
    /// <summary>AT(cSearch, cTarget) — posizione (1-based) della prima occorrenza; 0 se assente.</summary>
    public static int At(string search, string target)
    {
        if (string.IsNullOrEmpty(search)) return 0;
        var idx = target.IndexOf(search, StringComparison.Ordinal);
        return idx < 0 ? 0 : idx + 1;
    }

    /// <summary>RAT(cSearch, cTarget) — posizione (1-based) dell'ultima occorrenza; 0 se assente.</summary>
    public static int Rat(string search, string target)
    {
        if (string.IsNullOrEmpty(search)) return 0;
        var idx = target.LastIndexOf(search, StringComparison.Ordinal);
        return idx < 0 ? 0 : idx + 1;
    }

    /// <summary>SUBSTR(cString, nStart, nCount) — sottostringa a base 1; nStart&lt;=0 e nCount&lt;=0 gestiti.</summary>
    public static string Substr(string s, int start, int count)
    {
        if (string.IsNullOrEmpty(s) || count <= 0) return "";
        var len = s.Length;
        if (start > len) return "";
        if (start < 1)
        {
            count += start - 1;
            start = 1;
            if (count <= 0) return "";
        }
        if (start + count - 1 > len) count = len - start + 1;
        return s.Substring(start - 1, count);
    }

    /// <summary>STUFF(cString, nStart, nDelete, cInsert) — sostituisce nDelete caratteri da nStart con cInsert.</summary>
    public static string Stuff(string s, int start, int delete, string insert)
    {
        insert ??= "";
        if (string.IsNullOrEmpty(s))
            return start <= 1 ? insert : insert;
        if (start < 1) start = 1;
        if (delete < 0) delete = 0;
        if (start > s.Length + 1) return s + insert;
        var idx = start - 1;
        var tail = Math.Min(delete, s.Length - idx);
        return s[..idx] + insert + s[(idx + tail)..];
    }

    /// <summary>STRTRAN(cString, cSearch, cReplace) — sostituisce tutte le occorrenze.</summary>
    public static string StrTran(string s, string search, string replace)
    {
        replace ??= "";
        if (string.IsNullOrEmpty(s) || string.IsNullOrEmpty(search)) return s;
        return s.Replace(search, replace);
    }

    /// <summary>STRTRAN(cString, cSearch, cReplace, nStart, nCount) — sostituzioni limitate.</summary>
    public static string StrTran(string s, string search, string replace, int start, int count)
    {
        if (string.IsNullOrEmpty(s) || string.IsNullOrEmpty(search) || count == 0) return s;
        replace ??= "";
        var idx = s.IndexOf(search, start - 1, StringComparison.Ordinal);
        if (idx < 0) return s;
        var sb = new StringBuilder(s);
        var replaced = 0;
        var pos = idx;
        while (pos >= 0 && replaced < count)
        {
            sb.Remove(pos, search.Length);
            sb.Insert(pos, replace);
            replaced++;
            pos = sb.ToString().IndexOf(search, pos + replace.Length, StringComparison.Ordinal);
        }
        return sb.ToString();
    }

    /// <summary>LEFT(cString, n) — primi n caratteri.</summary>
    public static string Left(string s, int n)
    {
        if (string.IsNullOrEmpty(s) || n <= 0) return "";
        return n >= s.Length ? s : s[..n];
    }

    /// <summary>RIGHT(cString, n) — ultimi n caratteri.</summary>
    public static string Right(string s, int n)
    {
        if (string.IsNullOrEmpty(s) || n <= 0) return "";
        return n >= s.Length ? s : s[^n..];
    }

    /// <summary>UPPER(cString) — maiuscolo.</summary>
    public static string Upper(string s) => s.ToUpperInvariant();

    /// <summary>LOWER(cString) — minuscolo.</summary>
    public static string Lower(string s) => s.ToLowerInvariant();

    /// <summary>ISALPHA(c) — true se è una lettera.</summary>
    public static bool IsAlpha(char c) => char.IsLetter(c);

    /// <summary>ISDIGIT(c) — true se è una cifra.</summary>
    public static bool IsDigit(char c) => char.IsDigit(c);

    /// <summary>ISLOWER(c) — true se è una lettera minuscola.</summary>
    public static bool IsLower(char c) => char.IsLower(c);

    /// <summary>ISUPPER(c) — true se è una lettera maiuscola.</summary>
    public static bool IsUpper(char c) => char.IsUpper(c);

    /// <summary>LEN(cString) — numero di caratteri.</summary>
    public static int Len(string s) => s.Length;

    /// <summary>TRIM(cString) / RTRIM — rimuove gli spazi finali.</summary>
    public static string Trim(string s) => s.TrimEnd();

    /// <summary>RTRIM(cString) — come TRIM.</summary>
    public static string RTrim(string s) => s.TrimEnd();

    /// <summary>LTRIM(cString) — rimuove gli spazi iniziali.</summary>
    public static string LTrim(string s) => s.TrimStart();

    /// <summary>ALLTRIM(cString) — rimuove gli spazi a entrambe le estremità.</summary>
    public static string AllTrim(string s) => s.Trim();

    /// <summary>PADL(cString, nLength, cFill=' ') — giustifica a destra riempiendo a sinistra.</summary>
    public static string PadL(string s, int length, char fill = ' ') =>
        Pad(s, length, fill, left: true);

    /// <summary>PADC(cString, nLength, cFill=' ') — centra.</summary>
    public static string PadC(string s, int length, char fill = ' ')
    {
        if (s.Length >= length) return s;
        var total = length - s.Length;
        var l = total / 2;
        return new string(fill, l) + s + new string(fill, total - l);
    }

    /// <summary>PADR(cString, nLength, cFill=' ') — giustifica a sinistra riempiendo a destra.</summary>
    public static string PadR(string s, int length, char fill = ' ') =>
        Pad(s, length, fill, left: false);

    private static string Pad(string s, int length, char fill, bool left)
    {
        if (s.Length >= length) return s;
        var pad = new string(fill, length - s.Length);
        return left ? pad + s : s + pad;
    }

    /// <summary>REPLICATE(cExpression, nCount) — ripete la stringa n volte.</summary>
    public static string Replicate(string s, int n)
    {
        if (n <= 0 || string.IsNullOrEmpty(s)) return "";
        return string.Concat(Enumerable.Repeat(s, n));
    }

    /// <summary>REPLICATE(cChar, nCount) — ripete il carattere n volte.</summary>
    public static string Replicate(char c, int n) =>
        n <= 0 ? "" : new string(c, n);

    /// <summary>SPACE(n) — n spazi.</summary>
    public static string Space(int n) => n <= 0 ? "" : new string(' ', n);

    /// <summary>TRANSFORM(expr, cPicture) — formatta un valore (stringa/numero/data) con un PICTURE.</summary>
    public static string Transform(object value, string picture) =>
        FormatPicture.Apply(value, picture);

    /// <summary>TRANSFORM(cString, cPicture) — formatta una stringa con un PICTURE.</summary>
    public static string Transform(string value, string picture) =>
        FormatPicture.Apply(value, picture);

    /// <summary>TRANSFORM(nValue, cPicture) — formatta un numero con un PICTURE.</summary>
    public static string Transform(double value, string picture) =>
        FormatPicture.Apply(value, picture);

    /// <summary>TRANSFORM(dDate, cPicture) — formatta una data con un PICTURE.</summary>
    public static string Transform(DateTime value, string picture) =>
        FormatPicture.Apply(value, picture);

    /// <summary>
    /// VAL(cString) — converte il numero iniziale della stringa in double (0.0 se assente).
    /// Si ferma al primo carattere non numerico dopo segno e separatore decimale.
    /// </summary>
    public static double Val(string s)
    {
        if (string.IsNullOrEmpty(s)) return 0.0;
        var i = 0;
        while (i < s.Length && char.IsWhiteSpace(s[i])) i++;
        var sb = new StringBuilder();
        if (i < s.Length && (s[i] == '+' || s[i] == '-')) { sb.Append(s[i]); i++; }
        var digits = false;
        while (i < s.Length && (char.IsDigit(s[i]) || s[i] == '.'))
        {
            if (s[i] == '.')
            {
                if (sb.ToString().Contains('.')) break;
                sb.Append('.');
            }
            else sb.Append(s[i]);
            digits = true;
            i++;
        }
        if (!digits) return 0.0;
        return double.TryParse(sb.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out var v)
            ? v : 0.0;
    }

    /// <summary>STR(nValue, nLength=10, nDecimals=0) — numero in stringa giustificata a destra; '*' se non ci sta.</summary>
    public static string Str(double value, int length = 10, int decimals = 0)
    {
        if (length < 1) return "";
        var s = value.ToString("F" + Math.Max(0, decimals), CultureInfo.InvariantCulture);
        if (s.Length > length) return new string('*', length);
        return s.PadLeft(length);
    }

    /// <summary>STRZERO(nValue, nLength, nDecimals=0) — come STR ma riempie con zeri.</summary>
    public static string StrZero(double value, int length, int decimals = 0)
    {
        if (length < 1) return "";
        var s = value.ToString("F" + Math.Max(0, decimals), CultureInfo.InvariantCulture);
        var neg = s.StartsWith("-");
        var body = neg ? s[1..] : s;
        if (body.Length > length) return new string('*', length);
        body = body.PadLeft(length, '0');
        return neg ? "-" + body[1..] : body;
    }

    /// <summary>ASC(cChar) — codice del primo carattere.</summary>
    public static int Asc(char c) => c;

    /// <summary>CHR(n) — carattere dal codice.</summary>
    public static char Chr(int n) => (char)(n & 0xFFFF);

    /// <summary>EMPTY(expr) — true se null, stringa vuota, 0, false, data vuota o array vuoto.</summary>
    public static bool Empty(object? v) => v switch
    {
        null => true,
        string s => s.Length == 0,
        char c => c == '\0',
        byte b => b == 0,
        short n => n == 0,
        int n => n == 0,
        long n => n == 0,
        float f => f == 0f,
        double d => d == 0d,
        decimal m => m == 0m,
        bool b2 => !b2,
        DateTime dt => dt == DateTime.MinValue || dt == default,
        System.Collections.ICollection c2 => c2.Count == 0,
        _ => false,
    };

    /// <summary>VALTYPE(expr) — lettera di tipo: C, N, D, L, U, A, O.</summary>
    public static string ValType(object? v) => v switch
    {
        null => "U",
        string => "C",
        char => "C",
        bool => "L",
        DateTime => "D",
        byte or short or int or long or float or double or decimal => "N",
        // Istanza UDT/CLASS (dict con descriptor __class__): è un OGGETTO, non un array — anche
        // se la rappresentazione boxed è un IDictionary (che è ICollection). Parità con il
        // backend IL (struct → "O") e con Clipper (gli oggetti sono "O").
        System.Collections.IDictionary d when d.Contains("__class__") => "O",
        System.Collections.ICollection => "A",
        _ => "O",
    };

    /// <summary>
    /// TYPE(cExpression) — lettera di tipo di una costante letterale (C/N/D/L), "U" se sconosciuto.
    /// </summary>
    public static string Type(string expr)
    {
        var s = expr?.Trim() ?? "";
        if (s.Length == 0) return "U";
        if (s is ".T." or ".F.") return "L";
        if (s.StartsWith('{') && s.EndsWith('}')) return "D";
        if ((s.StartsWith('"') && s.EndsWith('"')) || (s.StartsWith('\'') && s.EndsWith('\''))) return "C";
        var i = s[0] is '+' or '-' ? 1 : 0;
        if (i < s.Length && char.IsDigit(s[i]))
        {
            var n = 0;
            while (n < s.Length && (char.IsDigit(s[n]) || s[n] == '.' || s[n] == '+' || s[n] == '-')) n++;
            return n == s.Length ? "N" : "C";
        }
        return "U";
    }
}
