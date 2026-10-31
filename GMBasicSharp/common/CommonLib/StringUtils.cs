// FILE: CommonLib.StringUtils.cs

using System.Globalization;
using System.Linq;
using System.Text;

/* 
 * StringUtils.cs: utilità per la manipolazione di stringhe in stile BASIC (LEFT$, RIGHT$, MID$, STR$, CHR$, INSTR, ecc.)
 * 
 * FUNZIONI:
 * - Left(source, length): estrae i primi N caratteri da sinistra (LEFT$)
 * - Right(source, length): estrae gli ultimi N caratteri da destra (RIGHT$)
 * - Mid(source, start, length): estrae una sottostringa a partire da una posizione (MID$)
 * - Str(value): converte un valore numerico in stringa (STR$)
 * - Chr(charCode): converte un codice ASCII in carattere (CHR$)
 * - Instr(source, substring, start): cerca una sottostringa in una stringa (INSTR)
 * - Title(source): converte in Title Case
 * - Sentence(source): converte in Sentence case
 * - Duplicate(count, character): ripete un carattere N volte
 * - Repeat(source, count, separator): ripete una stringa N volte con separatore opzionale
 * - Pascal(source): converte in PascalCase
 * - Camel(source): converte in camelCase
 * - Wrap(text, width, alignment): word wrap con allineamento (0=sinistra, 1=destra, 2=giustificato)
 * - Count(source, value): conta le occorrenze non sovrapposte di una sottostringa
 * - PadCenter(text, totalWidth, padChar): centra un testo in un campo di larghezza fissa (PADC$)
 */

namespace CommonLib;

/// <summary>
/// Utility per la manipolazione di stringhe in stile BASIC. Tutti i metodi sono statici,
/// stateless (eccetto una cache TextInfo per il case conversion). Include funzioni classiche
/// BASIC (LEFT$, RIGHT$, MID$, STR$, CHR$, INSTR), formattazione testo (TITLE, SENTENCE,
/// PASCAL, CAMEL case), manipolazione (DUPLICATE, REPEAT, COUNT, PADCENTER) e word wrap
/// con allineamento sinistro, destro o giustificato.
/// </summary>
public static class StringUtils
{
    /// <summary>Cache Thread-safe per TextInfo us-Specific usata da Title/Sentence/Pascal/Camel case.</summary>
    private static readonly TextInfo TextInfo = new CultureInfo("en-US", false).TextInfo;

    /// <summary>Estrae i primi N caratteri da sinistra (LEFT$). N = CARATTERI (code point).</summary>
    public static string Left(string? source, int length)
    {
        var str = source ?? string.Empty;
        if (length < 0) length = 0;
        if (length == 0) return string.Empty;
        var sb = new StringBuilder();
        var taken = 0;
        foreach (var rune in str.EnumerateRunes())
        {
            if (taken >= length) break;
            sb.Append(rune.ToString());
            taken++;
        }
        return sb.ToString();
    }

    /// <summary>Numero di CARATTERI (code point Unicode) della stringa (LEN).</summary>
    public static int CharCount(string? source)
    {
        var str = source ?? string.Empty;
        var n = 0;
        foreach (var _ in str.EnumerateRunes()) n++;
        return n;
    }

    /// <summary>Numero di BYTE della codifica UTF-8 (LENB).</summary>
    public static int ByteCount(string? source) => Encoding.UTF8.GetByteCount(source ?? string.Empty);

    /// <summary>Indice UTF-16 del carattere (code point) numero <paramref name="charIndex"/> (0-based).</summary>
    private static int CharIndexToUtf16(string str, int charIndex)
    {
        if (charIndex <= 0) return 0;
        var n = 0;
        var i = 0;
        while (i < str.Length)
        {
            if (n >= charIndex) return i;
            i += char.IsHighSurrogate(str[i]) && i + 1 < str.Length && char.IsLowSurrogate(str[i + 1]) ? 2 : 1;
            n++;
        }
        return str.Length;
    }

    /// <summary>Estrae gli ultimi N caratteri da destra (RIGHT$). N = CARATTERI.</summary>
    public static string Right(string? source, int length)
    {
        var str = source ?? string.Empty;
        if (length < 0) length = 0;
        var chars = CharCount(str);
        if (length >= chars) return str;
        var from = CharIndexToUtf16(str, chars - length);
        return str[from..];
    }

    /// <summary>Estrae una sottostringa a partire da una posizione (MID$). Indici in CARATTERI, 1-based.</summary>
    public static string Mid(string? source, int start, int? length)
    {
        var str = source ?? string.Empty;
        var chars = CharCount(str);
        start--;
        if (start < 0 || start >= chars || length is <= 0) return string.Empty;
        var from = CharIndexToUtf16(str, start);
        var take = length ?? (chars - start);
        var to = CharIndexToUtf16(str, start + Math.Min(take, chars - start));
        return str[from..to];
    }

    /// <summary>Converte un valore numerico in stringa (STR$). Aggiunge spazio per i positivi.</summary>
    public static string Str(IConvertible value)
    {
        var decValue = value.ToDecimal(CultureInfo.InvariantCulture);
        var strValue = value.ToString(CultureInfo.InvariantCulture);
        return decValue >= 0 ? " " + strValue : strValue;
    }

    /// <summary>Converte un codice ASCII in carattere (CHR$).</summary>
    public static string Chr(int charCode) => ((char)charCode).ToString();

    /// <summary>Cerca una sottostringa in una stringa (INSTR). Posizione 1-based, 0 se non trovata.</summary>
    /// <param name="source">Stringa in cui cercare.</param>
    /// <param name="substring">Sottostringa da trovare.</param>
    /// <param name="start">Posizione 1-based di inizio ricerca.</param>
    public static long Instr(string? source, string? substring, int start)
    {
        var str = source ?? string.Empty;
        var sub = substring ?? string.Empty;
        var chars = CharCount(str);
        start--;
        if (start < 0 || start >= chars)
            return 0L;
        // Case-insensitive (OrdinalIgnoreCase): INSTR in Clipper/BASIC non distingue
        // maiuscole/minuscole — la PCodeVM usava già OrdinalIgnoreCase.
        // `start` è un indice in CARATTERI: si converte in indice UTF-16 e si riconverte il
        // risultato in caratteri (prima erano entrambi indici UTF-16 → divergenza con LEN).
        var fromUtf16 = CharIndexToUtf16(str, start);
        var idx = str.IndexOf(sub, fromUtf16, StringComparison.OrdinalIgnoreCase);
        return idx == -1 ? 0L : CharIndexFromUtf16(str, idx) + 1;
    }

    /// <summary>Numero di caratteri (code point) prima dell'indice UTF-16 <paramref name="utf16Index"/>.</summary>
    private static int CharIndexFromUtf16(string str, int utf16Index)
    {
        var n = 0;
        var i = 0;
        while (i < utf16Index && i < str.Length)
        {
            i += char.IsHighSurrogate(str[i]) && i + 1 < str.Length && char.IsLowSurrogate(str[i + 1]) ? 2 : 1;
            n++;
        }
        return n;
    }

    /// <summary>Converte in Title Case (prima lettera di ogni parola maiuscola).</summary>
    public static string Title(string? source)
    {
        var str = source ?? string.Empty;
        return TextInfo.ToTitleCase(str.ToLowerInvariant());
    }

    /// <summary>Converte in Sentence case (prima lettera della frase maiuscola, resto minuscolo).</summary>
    public static string Sentence(string? source)
    {
        var str = source ?? string.Empty;
        if (string.IsNullOrEmpty(str)) return string.Empty;
        return char.ToUpperInvariant(str[0]) + str[1..].ToLowerInvariant();
    }

    /// <summary>Ripete un carattere N volte.</summary>
    public static string Duplicate(int count, char character)
    {
        if (count < 0) count = 0;
        return new string(character, count);
    }

    /// <summary>Ripete N volte il PRIMO CARATTERE della stringa (DUP con argomento stringa).</summary>
    public static string DuplicateFirst(int count, string? value)
    {
        if (count < 0) count = 0;
        var str = value ?? string.Empty;
        if (str.Length == 0) return string.Empty;
        // un carattere = un code point: niente surrogati spezzati
        var first = str.EnumerateRunes().First().ToString();
        var sb = new StringBuilder(first.Length * count);
        for (var i = 0; i < count; i++) sb.Append(first);
        return sb.ToString();
    }

    /// <summary>Ripete una stringa N volte con separatore opzionale.</summary>
    public static string Repeat(string? source, int count, string? separator)
    {
        var str = source ?? string.Empty;
        if (count <= 0) return string.Empty;
        if (separator is null)
            return string.Concat(Enumerable.Repeat(str, count));
        return string.Join(separator, Enumerable.Repeat(str, count));
    }

    /// <summary>Converte in PascalCase (ogni parola con iniziale maiuscola, senza spazi).</summary>
    public static string Pascal(string? source)
    {
        var str = source ?? string.Empty;
        var parts = str.Split([' ', '_'], StringSplitOptions.RemoveEmptyEntries);
        var sb = new StringBuilder();
        foreach (var part in parts)
        {
            if (part.Length > 0)
            {
                sb.Append(char.ToUpperInvariant(part[0]));
                sb.Append(part[1..].ToLowerInvariant());
            }
        }
        return sb.ToString();
    }

    /// <summary>Converte in camelCase (prima parola minuscola, successive maiuscole).</summary>
    public static string Camel(string? source)
    {
        var str = source ?? string.Empty;
        var parts = str.Split([' ', '_'], StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 0) return string.Empty;

        var sb = new StringBuilder(parts[0].ToLowerInvariant());
        for (var i = 1; i < parts.Length; i++)
        {
            var part = parts[i];
            if (part.Length > 0)
            {
                sb.Append(char.ToUpperInvariant(part[0]));
                sb.Append(part[1..].ToLowerInvariant());
            }
        }
        return sb.ToString();
    }

    /// <summary>Word wrap con allineamento (0=sinistra, 1=destra, 2=giustificato).</summary>
    public static string Wrap(string? text, int width, int alignment)
    {
        if (width <= 0 || string.IsNullOrEmpty(text)) return string.Empty;

        var words = text.Split([' '], StringSplitOptions.RemoveEmptyEntries);
        if (words.Length == 0) return string.Empty;

        var resultLines = new List<string>();
        var currentLine = new StringBuilder();

        foreach (var word in words)
        {
            if (word.Length > width)
            {
                if (currentLine.Length > 0)
                {
                    resultLines.Add(currentLine.ToString());
                    currentLine.Clear();
                }
                var remainingWord = word;
                while (remainingWord.Length > width)
                {
                    resultLines.Add(remainingWord[..width]);
                    remainingWord = remainingWord[width..];
                }
                if (remainingWord.Length > 0)
                    currentLine.Append(remainingWord);
            }
            else
            {
                var spaceNeeded = currentLine.Length > 0 ? 1 : 0;
                if (currentLine.Length + spaceNeeded + word.Length > width && currentLine.Length > 0)
                {
                    resultLines.Add(currentLine.ToString());
                    currentLine.Clear();
                    currentLine.Append(word);
                }
                else
                {
                    if (currentLine.Length > 0) currentLine.Append(' ');
                    currentLine.Append(word);
                }
            }
        }
        if (currentLine.Length > 0) resultLines.Add(currentLine.ToString());

        var finalResult = new StringBuilder();
        for (var i = 0; i < resultLines.Count; i++)
        {
            var formattedLine = FormatLine(resultLines[i], width, alignment, i == resultLines.Count - 1);
            finalResult.Append(formattedLine);
            if (i < resultLines.Count - 1) finalResult.AppendLine();
        }
        return finalResult.ToString();
    }

    /// <summary>Applica allineamento a una singola riga di testo per il word wrap.</summary>
    private static string FormatLine(string line, int width, int alignment, bool isLastLine)
    {
        if (line.Length >= width) return line[..width];
        return alignment switch
        {
            1 => line.PadLeft(width),
            2 when !isLastLine && line.Contains(' ') => JustifyLine(line.Split(' '), width),
            _ => line.PadRight(width),
        };
    }

    /// <summary>Giustifica una riga distribuendo gli spazi extra tra le parole.</summary>
    private static string JustifyLine(string[] words, int width)
    {
        if (words.Length <= 1) return string.Join(" ", words).PadRight(width);
        var totalWordLength = words.Sum(w => w.Length);
        var totalSpaces = width - totalWordLength;
        var gaps = words.Length - 1;
        if (gaps == 0) return words[0].PadRight(width);

        var baseSpaces = totalSpaces / gaps;
        var extraSpaces = totalSpaces % gaps;

        var result = new StringBuilder();
        for (var i = 0; i < words.Length - 1; i++)
        {
            result.Append(words[i]);
            result.Append(' ', baseSpaces + (extraSpaces-- > 0 ? 1 : 0));
        }
        result.Append(words.Last());
        return result.ToString();
    }

    /// <summary>Conta le occorrenze non sovrapposte di una sottostringa in una stringa.</summary>
    public static long Count(string? source, string? value)
    {
        var src = source ?? string.Empty;
        var val = value ?? string.Empty;
        if (string.IsNullOrEmpty(val)) return 0;
        return (src.Length - src.Replace(val, string.Empty, StringComparison.Ordinal).Length) / val.Length;
    }

    /// <summary>Centra un testo in un campo di larghezza fissa con carattere di padding (PADC$).</summary>
    public static string PadCenter(string? text, int totalWidth, char padChar)
    {
        var str = text ?? string.Empty;
        var padAmount = totalWidth - str.Length;
        if (padAmount <= 0) return str.Length > totalWidth ? str[..totalWidth] : str;

        var padLeft = padAmount / 2;
        var padRight = padAmount - padLeft;

        return new string(padChar, padLeft) + str + new string(padChar, padRight);
    }
}
