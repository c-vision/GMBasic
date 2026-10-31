// FILE: CommonLib.ConversionUtils.cs

using System.Globalization;
using System.Text;

/* 
 * ConversionUtils.cs: utility per conversioni tra tipi BASIC e stringhe numeriche (VAL, CSTR$, HEX$, OCT$, BIN$)
 * 
 * FUNZIONI:
 * - Val(source): estrae un valore numerico dall'inizio di una stringa (VAL)
 * - CStr(value): converte un valore in stringa con formattazione BASIC (CSTR$)
 * - Hex(value): converte un intero in stringa esadecimale (HEX$)
 * - Oct(value): converte un intero in stringa ottale (OCT$)
 * - Bin(value): converte un intero in stringa binaria (BIN$)
 * - ToInteger(arg): converte un argomento generico in Int32
 * - PadOrTruncate(value, length): adatta una stringa a una lunghezza fissa
 * - AppendFormattedValue(sb, value): formatta un valore per output BASIC
 * - AppendFormattedGfxValue(sb, value): formatta un valore per output grafico
 */

namespace CommonLib;

/// <summary>
/// Utility per conversioni tra tipi BASIC e stringhe numeriche.
/// Include VAL (estrazione numero da stringa), CSTR (conversione a stringa BASIC),
/// HEX$, OCT$ e BIN$. Ogni funzione è stateless e thread-safe.
/// Le convenzioni di formattazione seguono lo standard BASIC storico
/// (spazio iniziale per numeri positivi in CSTR, formati invariant culture).
/// </summary>
public static class ConversionUtils
{
    /// <summary>
    /// Estrae un valore numerico dall'inizio di una stringa, ignorando i caratteri
    /// non numerici successivi. Ricalca la funzione VAL del BASIC.
    /// Supporta segno opzionale (+/-), punto decimale e notazione scientifica (E+nn).
    /// </summary>
    /// <param name="source">Stringa da cui estrarre il numero.</param>
    /// <returns>Double parsato, o 0.0 se non viene trovato nessun numero valido.</returns>
    public static double Val(string? source)
    {
        if (string.IsNullOrWhiteSpace(source))
            return 0.0;

        var span = source.AsSpan().TrimStart();
        var end = 0;

        if (span.Length > 0 && (span[0] == '-' || span[0] == '+'))
            end = 1;

        while (end < span.Length && (char.IsDigit(span[end]) || span[end] == '.'))
            end++;

        if (end < span.Length - 1 && char.ToUpperInvariant(span[end]) == 'E' && (span[end + 1] == '+' || span[end + 1] == '-'))
        {
            end += 2;
            while (end < span.Length && char.IsDigit(span[end]))
                end++;
        }

        return double.TryParse(span[..end], CultureInfo.InvariantCulture, out var n) ? n : 0.0;
    }

    /// <summary>
    /// Converte un valore in stringa con formattazione BASIC: aggiunge uno spazio
    /// iniziale per i numeri positivi. Usato da CSTR$. I booleani restituiscono
    /// "True"/"False" senza spazio iniziale.
    /// </summary>
    /// <param name="value">Il valore da convertire (numerico, booleano, o altro).</param>
    /// <returns>Rappresentazione stringa del valore in formato BASIC.</returns>
    public static string CStr(object? value)
    {
        string result;

        switch (value)
        {
            case float f: result = f.ToString(CultureInfo.InvariantCulture); break;
            case double d: result = d.ToString("G15", CultureInfo.InvariantCulture); break;
            case decimal dec: result = dec.ToString(CultureInfo.InvariantCulture); break;
            case bool b: return b ? "True" : "False";
            default: result = value?.ToString() ?? string.Empty; break;
        }

        if (value is IConvertible && !value.GetType().IsEnum && value is not string
            && decimal.TryParse(Convert.ToString(value, CultureInfo.InvariantCulture), NumberStyles.Any, CultureInfo.InvariantCulture, out var decVal) && decVal >= 0)
            return " " + result;

        return result;
    }

    /// <summary>Converte un intero in stringa esadecimale (HEX$).</summary>
    public static string Hex(long value) => value.ToString("X");

    /// <summary>Converte un intero in stringa ottale (OCT$).</summary>
    public static string Oct(long value) => Convert.ToString(value, 8);

    /// <summary>Converte un intero in stringa binaria (BIN$).</summary>
    public static string Bin(long value) => Convert.ToString(value, 2);

    /// <summary>Converte un argomento generico in un intero Int32.</summary>
    public static int ToInteger(object? arg) => Convert.ToInt32(arg);

    /// <summary>Adatta una stringa a una lunghezza fissa, troncandola o aggiungendo spazi.</summary>
    public static string PadOrTruncate(string value, int length)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(length);
        if (value.Length > length) return value[..length];
        if (value.Length < length) return value.PadRight(length);
        return value;
    }

    /// <summary>Formatta un valore per output BASIC appendendolo a uno StringBuilder.</summary>
    public static void AppendFormattedValue(StringBuilder sb, object? value)
    {
        switch (value)
        {
            case string s: sb.Append(s); break;
            case float f:
                if (f >= 0.0f) sb.Append(' ');
                sb.Append(f.ToString("G9", CultureInfo.InvariantCulture));
                sb.Append(' ');
                break;
            case double d:
                if (d >= 0.0) sb.Append(' ');
                sb.Append(d.ToString("G15", CultureInfo.InvariantCulture));
                sb.Append(' ');
                break;
            case decimal dec:
                if (dec >= 0.0m) sb.Append(' ');
                sb.Append(dec.ToString("G", CultureInfo.InvariantCulture));
                sb.Append(' ');
                break;
            case bool b: sb.Append(b ? "True" : "False"); break;
            case char ch:
                sb.Append(ch);
                break;
            case Array arr:
                sb.Append('[');
                for (var i = 0; i < arr.Length; i++)
                {
                    if (i > 0) sb.Append(", ");
                    AppendFormattedValue(sb, arr.GetValue(i));
                }
                sb.Append(']');
                break;
            case System.Collections.IDictionary dict:
                sb.Append('{');
                var first = true;
                foreach (System.Collections.DictionaryEntry de in dict)
                {
                    if (!first) sb.Append(", ");
                    first = false;
                    AppendFormattedValue(sb, de.Key);
                    sb.Append(": ");
                    AppendFormattedValue(sb, de.Value);
                }
                sb.Append('}');
                break;
            case System.Collections.IEnumerable enumerable:
                sb.Append('[');
                first = true;
                foreach (var item in enumerable)
                {
                    if (!first) sb.Append(", ");
                    first = false;
                    AppendFormattedValue(sb, item);
                }
                sb.Append(']');
                break;
            case IConvertible conv:
                if (conv is DateTime) { sb.Append(conv.ToString()); break; }
                if (Convert.ToDouble(conv) >= 0.0) sb.Append(' ');
                sb.Append(conv.ToString(CultureInfo.InvariantCulture));
                sb.Append(' ');
                break;
            default:
                if (value is not null) sb.Append(value);
                break;
        }
    }

    public static void AppendFormattedGfxValue(StringBuilder sb, object? value)
    {
        switch (value)
        {
            case string s: sb.Append(s); break;
            case float f: sb.Append(f.ToString("G9", CultureInfo.InvariantCulture)); break;
            case double d: sb.Append(d.ToString("G15", CultureInfo.InvariantCulture)); break;
            case decimal dec: sb.Append(dec.ToString("G", CultureInfo.InvariantCulture)); break;
            case bool b: sb.Append(b ? "True" : "False"); break;
            case DateTime dt: sb.Append(dt.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture)); break;
            case DateOnly d: sb.Append(d.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture)); break;
            case TimeOnly t: sb.Append(t.ToString("HH:mm:ss", CultureInfo.InvariantCulture)); break;
            case TimeSpan ts: sb.Append(ts.ToString()); break;
            case IConvertible conv: sb.Append(conv.ToString(CultureInfo.InvariantCulture)); break;
            default:
                if (value is not null) sb.Append(value);
                break;
        }
    }
}
