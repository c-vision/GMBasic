// FILE: CommonLib.Http.BuiltInFunctions.cs

using System.Collections;
using System.Globalization;

/* 
 * BuiltInFunctions.cs: implementazioni C# pure di funzioni built-in GM-BASIC per template engine (stringhe, formattazione, math, date)
 * 
 * FUNZIONI:
 * - Len(obj): lunghezza stringa o conteggio collezione
 * - UCase/LCase: conversione maiuscolo/minuscolo
 * - Mid/Left/Right: estrazione sottostringa
 * - Trim/LTrim/RTrim: rimozione spazi
 * - Replace/Reverse: manipolazione stringhe
 * - Space/String: generazione caratteri
 * - ToTitleCase: conversione title case
 * - CStr/Val/Format: conversione e formattazione
 * - Abs/Sgn/Sqr/Log/Exp/Sin/Cos/Tan/Atn/Round/Min/Max: funzioni matematiche
 * - Now/Today/Time/TimeString/DateAdd/DateDiff/Year/Month/Day/Hour/Minute/Second/Weekday: funzioni data/ora
 * - Iif: condizionale
 */

namespace CommonLib.Http;

/// <summary>
/// Contiene implementazioni C# pure e autonome per un sottoinsieme
/// di funzioni built-in di GM-BASIC. Queste funzioni sono "safe" per essere
/// chiamate da contesti esterni come il template engine, poiché non dipendono
/// dallo stato dell'Executor.
/// </summary>
public static class BuiltInFunctions
{
    private static readonly TextInfo TextInfo = new CultureInfo("en-US", false).TextInfo;

    // ========================================================================
    // --- Funzioni di Stringa ---
    // ========================================================================

    /// <summary>Restituisce la lunghezza di una stringa o il conteggio di una collezione.</summary>
    /// <param name="obj">Stringa o collezione.</param>
    /// <returns>Lunghezza o conteggio.</returns>
    public static int Len(object obj)
    {
        if (obj is string s) return s.Length;
        if (obj is ICollection c) return c.Count;
        return 0;
    }

    /// <summary>Converte una stringa in maiuscolo (UCASE$).</summary>
    public static string UCase(string s) => s.ToUpperInvariant();
    /// <summary>Converte una stringa in minuscolo (LCASE$).</summary>
    public static string LCase(string s) => s.ToLowerInvariant();
    /// <summary>Estrae una sottostringa da una posizione con lunghezza (MID$). Posizione 1-based.</summary>
    public static string Mid(string s, int start, int length)
    {
        start--;
        if (start < 0 || start >= s.Length || length <= 0) return string.Empty;
        length = Math.Min(length, s.Length - start);
        return s.Substring(start, length);
    }
    /// <summary>Estrae i primi N caratteri da sinistra (LEFT$).</summary>
    public static string Left(string s, int len) => len < 0 ? "" : (s.Length > len ? s[..len] : s);
    /// <summary>Estrae gli ultimi N caratteri da destra (RIGHT$).</summary>
    public static string Right(string s, int len) => len < 0 ? "" : (s.Length > len ? s[^len..] : s);
    /// <summary>Rimuove spazi iniziali e finali (TRIM$).</summary>
    public static string Trim(string s) => s.Trim();
    /// <summary>Rimuove spazi iniziali (LTRIM$).</summary>
    public static string LTrim(string s) => s.TrimStart();
    /// <summary>Rimuove spazi finali (RTRIM$).</summary>
    public static string RTrim(string s) => s.TrimEnd();
    /// <summary>Sostituisce tutte le occorrenze di una stringa con un'altra (REPLACE$).</summary>
    public static string Replace(string s, string find, string rep) => s.Replace(find, rep);
    /// <summary>Inverte l'ordine dei caratteri in una stringa (REVERSE$).</summary>
    public static string Reverse(string s)
    {
        var charArray = s.ToCharArray();
        Array.Reverse(charArray);
        return new string(charArray);
    }
    /// <summary>Genera una stringa di N spazi (SPACE$).</summary>
    public static string Space(int count) => count > 0 ? new string(' ', count) : string.Empty;
    /// <summary>Genera una stringa di N caratteri ripetuti (STRING$).</summary>
    public static string String(int count, object character)
    {
        var ch = character switch
        {
            string { Length: >= 1 } s => s[0],
            _ => (char)Convert.ToInt32(character, CultureInfo.InvariantCulture)
        };
        if (count < 0) count = 0;
        return new string(ch, count);
    }
    /// <summary>Converte una stringa in Title Case.</summary>
    public static string ToTitleCase(string s) => TextInfo.ToTitleCase(s.ToLowerInvariant());
    
    // ========================================================================
    // --- Funzioni di Formattazione e Conversione ---
    // ========================================================================

    /// <summary>Converte un valore in stringa con formattazione BASIC (CSTR$).</summary>
    public static string CStr(object arg)
    {
        var result = arg switch
        {
            float f => f.ToString("G9", CultureInfo.InvariantCulture),
            double d => d.ToString("G15", CultureInfo.InvariantCulture),
            _ => arg.ToString() ?? string.Empty
        };

        if (arg is IConvertible and not bool && Convert.ToDecimal(arg, CultureInfo.InvariantCulture) >= 0)
        {
            return " " + result;
        }
        return result;
    }

    /// <summary>Estrae un valore numerico dall'inizio di una stringa (VAL).</summary>
    public static double Val(string arg)
    {
        var span = arg.AsSpan().TrimStart();
        var end = 0;
        if (span.Length > 0 && (span[0] == '-' || span[0] == '+')) end = 1;
        while (end < span.Length && (char.IsDigit(span[end]) || span[end] == '.')) end++;
        if (end < span.Length - 1 && char.ToUpperInvariant(span[end]) == 'E' && (span[end + 1] == '+' || span[end + 1] == '-'))
        {
            end += 2;
            while (end < span.Length && char.IsDigit(span[end])) end++;
        }
        return double.TryParse(span[..end], CultureInfo.InvariantCulture, out var n) ? n : 0.0;
    }

    /// <summary>Formatta un valore secondo un formato specificato (FORMAT$).</summary>
    public static string Format(object value, string format)
    {
        if (value is IFormattable formattable)
            return formattable.ToString(format, CultureInfo.InvariantCulture);
        return value.ToString() ?? "";
    }
    
    // ========================================================================
    // --- Funzioni Matematiche ---
    // ========================================================================

    /// <summary>Valore assoluto (ABS).</summary>
    public static double Abs(double n) => Math.Abs(n);
    /// <summary>Segno del numero (SGN).</summary>
    public static double Sgn(double n) => Math.Sign(n);
    /// <summary>Radice quadrata (SQR).</summary>
    public static double Sqr(double n) => n >= 0 ? Math.Sqrt(n) : double.NaN;
    /// <summary>Logaritmo naturale (LOG).</summary>
    public static double Log(double n) => n > 0 ? Math.Log(n) : double.NaN;
    /// <summary>Esponenziale e^n (EXP).</summary>
    public static double Exp(double n) => Math.Exp(n);
    /// <summary>Seno (SIN).</summary>
    public static double Sin(double n) => Math.Sin(n);
    /// <summary>Coseno (COS).</summary>
    public static double Cos(double n) => Math.Cos(n);
    /// <summary>Tangente (TAN).</summary>
    public static double Tan(double n) => Math.Tan(n);
    /// <summary>Arcotangente (ATN).</summary>
    public static double Atn(double n) => Math.Atan(n);
    /// <summary>Arrotonda un valore a un numero specifico di decimali.</summary>
    public static double Round(double val, int digits) => Math.Round(val, digits, MidpointRounding.AwayFromZero);
    /// <summary>Minimo tra due valori.</summary>
    public static double Min(double a, double b) => Math.Min(a, b);
    /// <summary>Massimo tra due valori.</summary>
    public static double Max(double a, double b) => Math.Max(a, b);
    
    // ========================================================================
    // --- Funzioni di Data e Ora ---
    // ========================================================================
    
    /// <summary>Restituisce la data e ora corrente del sistema (NOW).</summary>
    public static DateTime Now() => DateTime.Now;
    /// <summary>Restituisce la data corrente (TODAY).</summary>
    public static DateOnly Today() => DateOnly.FromDateTime(DateTime.Today);
    /// <summary>Restituisce l'ora corrente (TIME).</summary>
    public static TimeOnly Time() => TimeOnly.FromDateTime(DateTime.Now);
    /// <summary>Restituisce l'ora corrente come stringa "HH:mm:ss" (TIME$).</summary>
    public static string TimeString() => DateTime.Now.ToString("HH:mm:ss");

    /// <summary>Aggiunge un intervallo a una data (DATEADD).</summary>
    public static DateTime DateAdd(string interval, double number, DateTime date)
    {
        var intNumber = (int)number;
        return interval.ToLowerInvariant() switch
        {
            "d" => date.AddDays(number),
            "h" => date.AddHours(number),
            "n" => date.AddMinutes(number),
            "s" => date.AddSeconds(number),
            "m" => date.AddMonths(intNumber),
            "y" or "yy" or "yyy" or "yyyy" => date.AddYears(intNumber),
            _ => throw new ArgumentException("Invalid interval for DATEADD.")
        };
    }

    /// <summary>Calcola la differenza tra due date (DATEDIFF).</summary>
    public static long DateDiff(string interval, DateTime date1, DateTime date2)
    {
        return interval.ToLowerInvariant() switch
        {
            "d" => (long)(date2 - date1).TotalDays,
            "h" => (long)(date2 - date1).TotalHours,
            "n" => (long)(date2 - date1).TotalMinutes,
            "s" => (long)(date2 - date1).TotalSeconds,
            "m" => (date2.Year - date1.Year) * 12 + date2.Month - date1.Month,
            "yyyy" => date2.Year - date1.Year,
            _ => throw new ArgumentException("Invalid interval for DATEDIFF.")
        };
    }

    /// <summary>Restituisce l'anno di una data (YEAR).</summary>
    public static int Year(DateTime date) => date.Year;
    /// <summary>Restituisce il mese di una data (MONTH).</summary>
    public static int Month(DateTime date) => date.Month;
    /// <summary>Restituisce il giorno di una data (DAY).</summary>
    public static int Day(DateTime date) => date.Day;
    /// <summary>Restituisce l'ora di una data (HOUR).</summary>
    public static int Hour(DateTime date) => date.Hour;
    /// <summary>Restituisce il minuto di una data (MINUTE).</summary>
    public static int Minute(DateTime date) => date.Minute;
    /// <summary>Restituisce il secondo di una data (SECOND).</summary>
    public static int Second(DateTime date) => date.Second;
    /// <summary>Restituisce il giorno della settimana (1=domenica, 7=sabato).</summary>
    public static int Weekday(DateTime date) => (int)date.DayOfWeek + 1;
    
    // ========================================================================
    // --- Funzioni Logiche e di Utilità ---
    // ========================================================================
    
    /// <summary>Restituisce un valore in base a una condizione (IIF).</summary>
    public static object Iif(bool condition, object trueVal, object falseVal) => condition ? trueVal : falseVal;
}
