// FILE: CommonLib.FormatUtils.cs

using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

/* 
 * FormatUtils.cs: logica di business pura per la funzione FORMAT$ (formattazione numerica, data/ora, booleana)
 * 
 * FUNZIONI:
 * - FormatValue(value, format): formatta un valore secondo la sintassi BASIC
 * - ContainsDateTimeTokens(format): verifica se il formato contiene token di data/ora
 * - IsBooleanFormat(format): verifica se il formato è per valori booleani
 * - FormatNumeric(number, format): smista la formattazione numerica
 * - FormatNumericDirect(number, format): formatta un numero non scientifico
 * - FormatNumberWithMask(number, mask): applica una maschera numerica
 * - FormatScientificDirect(number, format): formatta in notazione scientifica
 * - FormatDateTime(dt, format, culture): formatta una data/ora
 * - GetNextDateTimeToken(format, start): estrae il prossimo token di data/ora
 * - FormatSingleDateTimeToken(dt, token, culture): formatta un singolo token di data/ora
 * - FormatBoolean(value, format): formatta un valore booleano
 */

namespace CommonLib;

/// <summary>
/// Logica di business pura per la funzione FORMAT$. Contiene metodi statici
/// per la formattazione di valori numerici, data/ora e booleani secondo la sintassi BASIC.
/// </summary>
public static class FormatUtils
{
    private static readonly CultureInfo DefaultCulture = CultureInfo.InvariantCulture;

    /// <summary>Cultura per i NOMI di giorni/mesi in FORMAT: invariante, cosi' l'output non dipende dalla macchina ed e'
    /// identico al runtime C (VM/C, C99Native) che non ha tabelle di locale.</summary>
    private static readonly CultureInfo DateNamesCulture = CultureInfo.InvariantCulture;

    /// <summary>
    /// Logica di business pura per formattare un valore qualsiasi secondo la sintassi BASIC.
    /// Smista la formattazione al gestore appropriato in base al tipo e al formato.
    /// </summary>
    public static string FormatValue(object? value, string format)
    {
        if (string.IsNullOrEmpty(format))
            return value?.ToString() ?? string.Empty;

        if (value is bool || IsBooleanFormat(format))
            return FormatBoolean(value, format);
            
        var hasNumericTokens = format.Any(c => "#0".Contains(c));   // come gmb_format.c: solo # e 0 rendono il formato numerico

        /* Data/ora: se la maschera è NUMERICA ('#'/'0') il valore si formatta come SERIALE,
         * come le gambe C (gmb_format: il ramo "custom numeric" precede i token data/ora).
         * Prima un DateTime prendeva SEMPRE il ramo data → `FORMAT(d, "0.000000")` dava
         * "0.000000" su IL mentre PCode/VM-C/C99 davano "46024.000000" (bug aperto
         * 2026-11-01, vedi Bugs/(open) - Variabile-Con-Suffisso-E-Precisione-SINGLE §3). */
        if (value is DateTime dt)
            return hasNumericTokens
                ? FormatNumeric(dt.ToOADate(), format)
                : FormatDateTime(dt, format, DateNamesCulture);
        if (value is DateOnly dateOnly)
        {
            var oaDate = dateOnly.ToDateTime(TimeOnly.MinValue).ToOADate();
            return hasNumericTokens
                ? FormatNumeric(oaDate, format)
                : FormatDateTime(dateOnly.ToDateTime(TimeOnly.MinValue), format, DateNamesCulture);
        }
        if (value is TimeOnly timeOnly)
            // TIME = frazione di giornata (0.xxx), come gmb_dt_timeserial delle gambe C.
            return hasNumericTokens
                ? FormatNumeric(timeOnly.ToTimeSpan().TotalDays, format)
                : FormatDateTime(DateTime.Today.Add(timeOnly.ToTimeSpan()), format, DateNamesCulture);
        if (value is TimeSpan ts)
            return hasNumericTokens
                ? FormatNumeric(ts.TotalDays, format)
                : FormatDateTime(DateTime.Today.Add(ts), format, DateNamesCulture);

        if (global::CommonLib.Numerics.IsNumeric(value))
        {
            var numValue = Convert.ToDouble(value, CultureInfo.InvariantCulture);

            if (!hasNumericTokens && ContainsDateTimeTokens(format))
            {
                if (numValue is >= -657434.0 and <= 2958465.99999999)
                {
                    try {
                        var oleDate = DateTime.FromOADate(numValue);
                        return FormatDateTime(oleDate, format, DateNamesCulture);
                    } catch { /* Fallback a formattazione numerica */ }
                }
            }
            return FormatNumeric(numValue, format);
        }

        if (value is string str)
        {
            if (ContainsDateTimeTokens(format) && DateTime.TryParse(str, CultureInfo.InvariantCulture, DateTimeStyles.None, out var parsedDate))
                return FormatDateTime(parsedDate, format, DateNamesCulture);
        }

        return value?.ToString() ?? string.Empty;
    }
    
    /// <summary>
    /// Determina se il formato contiene token di data/ora (es. d, m, y, h) al di fuori di stringhe letterali.
    /// </summary>
    public static bool ContainsDateTimeTokens(string format)
    {
        var inLiteral = false;
        var quoteChar = '\0';
        for (var i = 0; i < format.Length; i++)
        {
            var c = format[i];
            if (c == '\\') { i++; continue; }
            if (!inLiteral && c is '\'' or '"') { inLiteral = true; quoteChar = c; continue; }
            if (inLiteral) {
                if (c == quoteChar) {
                    if (i + 1 < format.Length && format[i + 1] == quoteChar) i++; else inLiteral = false;
                }
                continue;
            }
            if ("yMdHhmnst".Contains(c)) return true;   // token case-sensitive (M=mese, m=minuti), come gmb_format.c
        }
        return false;
    }

    /// <summary>
    /// Verifica se il formato è destinato a valori booleani.
    /// </summary>
    public static bool IsBooleanFormat(string format) => format.Contains(';');

    /// <summary>
    /// Smista la formattazione numerica: percentuale, scientifica o standard.
    /// </summary>
    public static string FormatNumeric(double number, string format)
    {
        if (format.Contains('%'))
        {
            var percentValue = number * 100;
            var numFormat = format.Replace("%", "");
            return FormatNumeric(percentValue, numFormat) + "%";
        }

        if (format.Contains("E+", StringComparison.OrdinalIgnoreCase) || format.Contains("E-", StringComparison.OrdinalIgnoreCase))
            return FormatScientificDirect(number, format);

        return FormatNumericDirect(number, format);
    }
    
    /// <summary>
    /// Formatta un numero standard (non scientifico) analizzando la stringa di formato completa.
    /// </summary>
    public static string FormatNumericDirect(double number, string format)
    {
        var finalResult = new StringBuilder();
        var numberPartFormatted = false;
        var i = 0;
        while (i < format.Length)
        {
            var c = format[i];
            if (c == '\\' && i + 1 < format.Length) { finalResult.Append(format[i + 1]); i += 2; continue; }
            if (c is '\'' or '"') {
                var quoteChar = c; i++; var literalStart = i;
                while (i < format.Length && format[i] != quoteChar) i++;
                finalResult.Append(format.AsSpan(literalStart, i - literalStart));
                if (i < format.Length) i++; continue;
            }
            if (!numberPartFormatted && "#0.,+-".Contains(c)) {
                var maskStartIndex = i;
                while (i < format.Length && "#0.,+-".Contains(format[i])) i++;
                var numericMask = format.Substring(maskStartIndex, i - maskStartIndex);
                finalResult.Append(FormatNumberWithMask(number, numericMask));
                numberPartFormatted = true; continue;
            }
            if (c != '%') finalResult.Append(c);
            i++;
        }
        return finalResult.ToString();
    }
    
    /// <summary>
    /// Applica una maschera puramente numerica a un valore.
    /// </summary>
    public static string FormatNumberWithMask(double number, string mask)
    {
        var showPlusSign = mask.Contains('+');
        var useSeparators = mask.Contains(',');
        var decimalPos = mask.IndexOf('.');
        var integerMask = (decimalPos == -1 ? mask : mask[..decimalPos]).Replace(",", "").Replace("+", "").Replace("-", "");
        var decimalMask = decimalPos == -1 ? "" : mask[(decimalPos + 1)..];
        var roundedNumber = Math.Round(Math.Abs(number), decimalMask.Length);
        var numberParts = roundedNumber.ToString("F" + decimalMask.Length, CultureInfo.InvariantCulture).Split('.');
        var integerDigits = numberParts[0];
        var decimalDigits = numberParts.Length > 1 ? numberParts[1] : "";
        var integerResult = new StringBuilder();
        var digitIndex = integerDigits.Length - 1;

        for (var i = integerMask.Length - 1; i >= 0; i--) {
            if (digitIndex >= 0) integerResult.Insert(0, integerDigits[digitIndex--]);
            else integerResult.Insert(0, integerMask[i] == '0' ? '0' : ' ');
        }
        while (digitIndex >= 0) integerResult.Insert(0, integerDigits[digitIndex--]);

        var tempResult = integerResult.ToString();
        var firstDigitIndex = tempResult.AsSpan().IndexOfAny("0123456789");

        if (useSeparators && firstDigitIndex != -1) {
            var integerPart = new StringBuilder(tempResult[firstDigitIndex..]);
            for (var i = integerPart.Length - 3; i > 0; i -= 3) integerPart.Insert(i, ',');
            integerResult = new StringBuilder(tempResult[..firstDigitIndex]).Append(integerPart);
        }

        var decimalResult = new StringBuilder();
        for (var i = 0; i < decimalMask.Length; i++) {
            if (i < decimalDigits.Length) decimalResult.Append(decimalDigits[i]);
            else if (decimalMask[i] == '0') decimalResult.Append('0');
        }
        if (decimalResult.Length > 0) integerResult.Append('.').Append(decimalResult);

        var finalString = integerResult.ToString();
        if (number < 0) return "-" + finalString.TrimStart();
        if (showPlusSign) return "+" + finalString.TrimStart();
        
        return finalString;
    }
    
    /// <summary>
    /// Formatta un numero in notazione scientifica secondo un pattern BASIC.
    /// </summary>
    public static string FormatScientificDirect(double number, string format)
    {
        var isNegative = number < 0;
        number = Math.Abs(number);
        var match = Regex.Match(format, @"([#0\.]+)[Ee]([+-])([0]+)", RegexOptions.IgnoreCase);
        if (!match.Success) return FormatNumericDirect(number * (isNegative ? -1 : 1), format);

        var mantissaMask = match.Groups[1].Value;
        var signSpecifier = match.Groups[2].Value;
        var exponentDigits = match.Groups[3].Value.Length;

        if (number == 0.0) {
            var zeroMantissa = FormatNumberWithMask(0, mantissaMask);
            var zeroExponent = new string('0', exponentDigits);
            return $"{zeroMantissa}E+{zeroExponent}";
        }

        var exponent = (int)Math.Floor(Math.Log10(number));
        var mantissa = number / Math.Pow(10, exponent);
        var formattedMantissa = FormatNumberWithMask(mantissa, mantissaMask);
        var exponentSign = exponent < 0 ? "-" : (signSpecifier == "+" ? "+" : "");
        var formattedExponent = Math.Abs(exponent).ToString().PadLeft(exponentDigits, '0');
        var result = $"{formattedMantissa}E{exponentSign}{formattedExponent}";

        return isNegative ? "-" + result : result;
    }

    /// <summary>
    /// Formatta una data/ora secondo i token specificati in un formato BASIC.
    /// </summary>
    public static string FormatDateTime(DateTime dt, string format, CultureInfo culture)
    {
        var result = new StringBuilder();
        var i = 0;
        while (i < format.Length) {
            var c = format[i];
            if (c == '\\' && i + 1 < format.Length) { result.Append(format[i + 1]); i += 2; continue; }
            if (c is '\'' or '"') {
                var quoteChar = c; i++;
                while (i < format.Length) {
                    if (format[i] == quoteChar) {
                        if (i + 1 < format.Length && format[i + 1] == quoteChar) { result.Append(quoteChar); i++; } 
                        else { break; }
                    } else { result.Append(format[i]); }
                    i++;
                }
                i++; continue;
            }
            if (string.Compare(format, i, "AM/PM", 0, 5, StringComparison.OrdinalIgnoreCase) == 0 && i + 5 <= format.Length)
            { result.Append(dt.Hour < 12 ? "AM" : "PM"); i += 5; continue; }
            var token = GetNextDateTimeToken(format, i);
            result.Append(FormatSingleDateTimeToken(dt, token, culture));
            i += token.Length;
        }
        return result.ToString();
    }

    /// <summary>
    /// Logica di business pura per estrarre il prossimo token di data/ora.
    /// </summary>
    public static string GetNextDateTimeToken(string format, int start)
    {
        // Token CASE-SENSITIVE (come gmb_format.c): una sequenza di caratteri IDENTICI. 'M' (mese) e 'm' (minuti) sono distinti.
        var firstChar = format[start];
        var len = 1;
        while (start + len < format.Length && format[start + len] == firstChar) len++;
        return format.Substring(start, len);
    }
    
    /// <summary>
    /// Logica di business pura per formattare un singolo token di data/ora.
    /// Canone (Strings/StringsFormat.bas): d/dd/ddd/dddd giorno, M/MM/MMM/MMMM mese, yy/yyyy anno, h/hh ora 12h,
    /// H/HH ora 24h, m/mm e n/nn minuti, s/ss secondi, t/tt designatore AM/PM. Altro = letterale.
    /// </summary>
    public static string FormatSingleDateTimeToken(DateTime dt, string token, CultureInfo culture)
    {
        var hour12 = dt.Hour % 12 == 0 ? 12 : dt.Hour % 12;
        var run = token.Length;
        switch (token[0]) {
            case 'd':
                if (run >= 4) return culture.DateTimeFormat.GetDayName(dt.DayOfWeek);
                if (run == 3) return culture.DateTimeFormat.GetAbbreviatedDayName(dt.DayOfWeek);
                return run == 2 ? dt.Day.ToString("00") : dt.Day.ToString();
            case 'M':
                if (run >= 4) return culture.DateTimeFormat.GetMonthName(dt.Month);
                if (run == 3) return culture.DateTimeFormat.GetAbbreviatedMonthName(dt.Month);
                return run == 2 ? dt.Month.ToString("00") : dt.Month.ToString();
            case 'y':
                if (run >= 4) return dt.Year.ToString("0000");
                return run >= 2 ? (dt.Year % 100).ToString("00") : (dt.Year % 10).ToString();
            case 'h': return run == 1 ? hour12.ToString() : hour12.ToString("00");
            case 'H': return run == 1 ? dt.Hour.ToString() : dt.Hour.ToString("00");
            case 'm':
            case 'n': return run == 1 ? dt.Minute.ToString() : dt.Minute.ToString("00");
            case 's': return run == 1 ? dt.Second.ToString() : dt.Second.ToString("00");
            case 't': return run >= 2 ? (dt.Hour < 12 ? "AM" : "PM") : (dt.Hour < 12 ? "A" : "P");
            default: return token;
        }
    }
    
    /// <summary>
    /// Formatta un valore booleano secondo la sintassi "True;False".
    /// </summary>
    public static string FormatBoolean(object? value, string format)
    {
        var boolValue = value is not null && Convert.ToDouble(value, CultureInfo.InvariantCulture) != 0;
        var parts = format.Split(';');
        // Gli spazi del formato vengono RISPETTATI (canone Strings/StringsFormat.bas, come gmb_format.c): niente Trim.
        return PlainFormatText(boolValue ? parts[0] : (parts.Length > 1 ? parts[1] : string.Empty));
    }

    /// <summary>Testo di una sezione non numerica: toglie gli apici (', ") e risolve l'escape '\x' — come gmb_fmt_plain in gmb_format.c.</summary>
    private static string PlainFormatText(string s)
    {
        var sb = new StringBuilder(s.Length);
        for (var i = 0; i < s.Length; i++) {
            var c = s[i];
            if (c == '\\' && i + 1 < s.Length) { sb.Append(s[++i]); continue; }
            if (c is '\'' or '"') continue;
            sb.Append(c);
        }
        return sb.ToString();
    }
}
