using System.Globalization;

/* 
 * DateUtils.cs: utilità statiche per aritmetica data/ora, conversioni e query del clock di sistema
 * 
 * FUNZIONI:
 * - CoreDateAdd(interval, number, date): aggiunge un intervallo a una data
 * - CoreWeekday(dateValue): restituisce il giorno della settimana (1-7)
 * - CoreEomonth(dateValue, monthsToAdd): restituisce l'ultimo giorno del mese
 * - CoreDateDiff(interval, date1, date2): calcola la differenza tra due date
 * - CoreDateSerial(year, month, day): costruisce un DateOnly da componenti
 * - CoreTimeSerial(hour, minute, second): costruisce un TimeOnly da componenti
 * - CoreDateValue(dateString): parsifica una stringa in DateOnly
 * - CoreNow(): restituisce la data/ora corrente
 * - CoreToday(): restituisce la data corrente
 * - CoreTimeOnly(): restituisce l'ora corrente
 * - CoreTimeString(): restituisce l'ora come stringa "HH:mm:ss"
 * - CoreDtos(dateValue): converte in formato "yyyyMMdd"
 * - CoreStod(dateString): converte da "yyyyMMdd" a DateOnly
 * - CoreTiString(): restituisce l'ora come "HH:mm:ss"
 * - CoreDaysInMonth(dateValue): numero di giorni nel mese
 * - CoreIsLeapYear(dateValue): verifica se anno bisestile
 * - CoreFirstDayOfMonth(dateValue): primo giorno del mese
 * - CoreIsoWeekOfYear(dateValue): numero ISO 8601 della settimana
 * - CoreDateFromUnixTimestamp(unixTimestamp): converte da Unix timestamp a DateOnly
 * - CoreUnixTimestamp(dateValue): converte DateTime in Unix timestamp
 */

namespace CommonLib;

/// <summary>
/// Pure static utility methods for date/time arithmetic, conversion, and system clock queries.
/// No instance state — safe for any execution context.
/// </summary>
public static class DateUtils
{
    /// <summary>
    /// Adds a specified interval to a date. Supports days ("d"), hours ("h"), minutes ("n"),
    /// seconds ("s"), months ("m"), and years ("yyyy").
    /// </summary>
    /// <param name="interval">The interval type: "d" (days), "h" (hours), "n" (minutes), "s" (seconds), "m" (months), "yyyy" (years).</param>
    /// <param name="number">The number of intervals to add (can be fractional; truncated to integer for months/years).</param>
    /// <param name="date">The starting date/time.</param>
    /// <returns>The resulting <see cref="DateTime"/> after adding the specified interval.</returns>
    /// <exception cref="ArgumentException">Thrown if <paramref name="interval"/> is not a recognized value.</exception>
    public static DateTime CoreDateAdd(string interval, decimal number, DateTime date)
    {
        var numberInt = (int)Math.Truncate(number);

        return interval.ToLowerInvariant() switch
        {
            "d" => date.AddDays((double)number),
            "h" => date.AddHours((double)number),
            "n" => date.AddMinutes((double)number),
            "s" => date.AddSeconds((double)number),
            "m" => date.AddMonths(numberInt),
            // ANNO: il runtime C (gmb_dt_dateadd, case 'y') accetta qualunque grafia che inizi
            // per 'y' ("y","yy","yyy","yyyy"); la VM C# accettava solo "yyyy" → DATEADD("Y",..)
            // lanciava "Invalid interval" (P-CodeVM), divergenza col C (bug campione
            // ClipperDbfFullTests/DbfIndexCompound/DbfMemoCrud). Allineato.
            "y" or "yy" or "yyy" or "yyyy" => date.AddYears(numberInt),
            _ => throw new ArgumentException($"Invalid interval for DATEADD: '{interval}'.")
        };
    }

    /// <summary>
    /// Returns the day of the week as a number (Sunday=1, Monday=2, ..., Saturday=7).
    /// </summary>
    /// <param name="dateValue">The date to evaluate.</param>
    /// <returns>A long integer from 1 (Sunday) to 7 (Saturday).</returns>
    public static long CoreWeekday(DateTime dateValue)
    {
        return (long)dateValue.DayOfWeek + 1;
    }

    // --- Funzioni stile Clipper/dBASE (CDOW/CMONTH/DOW/CTOD/DTOC/SECONDS) ---
    // Semantica allineata a ClipperLib/Clipper/ClipperDates.cs

    /// <summary>DOW(d) — numero del giorno della settimana (1 = domenica .. 7 = sabato).</summary>
    public static int CoreDow(DateTime d) => (int)d.DayOfWeek + 1;

    /// <summary>CDOW(d) — nome inglese del giorno della settimana.</summary>
    public static string CoreCDow(DateTime d) => d.DayOfWeek.ToString();

    /// <summary>CMONTH(d) — nome inglese del mese.</summary>
    public static string CoreCMonth(DateTime d) => d.ToString("MMMM", CultureInfo.InvariantCulture);

    /// <summary>DTOC(d) — data come stringa nel formato di SET DATE FORMAT (default MM/DD/YYYY).</summary>
    public static string CoreDtoc(DateTime d) => d.ToString("MM/dd/yyyy", CultureInfo.InvariantCulture);

    /// <summary>CTOD(s) — stringa data (formato MM/DD/YYYY) in DateTime serial; 0 se non valida.</summary>
    public static double CoreCtod(string s)
    {
        if (string.IsNullOrWhiteSpace(s)) return 0;
        if (DateTime.TryParse(s, CultureInfo.InvariantCulture, DateTimeStyles.None, out var parsed))
            return parsed.Ticks;
        return 0;
    }

    /// <summary>SECONDS() — secondi trascorsi dall'ultima mezzanotte.</summary>
    public static long CoreSeconds() => (long)DateTime.Now.TimeOfDay.TotalSeconds;

    /// <summary>
    /// Returns the last day of the month for the specified date, after adding a number of months.
    /// </summary>
    /// <param name="dateValue">The starting date.</param>
    /// <param name="monthsToAdd">The number of months to add (can be negative).</param>
    /// <returns>A <see cref="DateOnly"/> representing the last day of the resulting month.</returns>
    public static DateOnly CoreEomonth(DateTime dateValue, int monthsToAdd)
    {
        var date = dateValue.AddMonths(monthsToAdd);
        var lastDay = DateTime.DaysInMonth(date.Year, date.Month);
        return new DateOnly(date.Year, date.Month, lastDay);
    }

    /// <summary>
    /// Calculates the difference between two dates according to the specified interval.
    /// </summary>
    /// <param name="interval">The interval type: "d" (days), "h" (hours), "n" (minutes), "s" (seconds), "m" (months), "yyyy" (years).</param>
    /// <param name="date1">The first date (earlier).</param>
    /// <param name="date2">The second date (later).</param>
    /// <returns>For "d"/"h"/"n"/"s": a <see cref="TimeSpan"/>; for "m": total months as <see cref="long"/>; for "yyyy": total years as <see cref="long"/>.</returns>
    /// <exception cref="ArgumentException">Thrown if <paramref name="interval"/> is not a recognized value.</exception>
    public static object CoreDateDiff(string interval, DateTime date1, DateTime date2)
    {
        return interval.ToLowerInvariant() switch
        {
            "d" or "h" or "n" or "s" => date2 - date1,
            "m" => (long)((date2.Year - date1.Year) * 12 + date2.Month - date1.Month),
            "yyyy" => (long)(date2.Year - date1.Year),
            _ => throw new ArgumentException($"Invalid interval for DATEDIFF: '{interval}'.")
        };
    }

    /// <summary>
    /// Constructs a <see cref="DateOnly"/> from year, month, and day components.
    /// </summary>
    /// <param name="year">The year.</param>
    /// <param name="month">The month (1–12).</param>
    /// <param name="day">The day (1–31).</param>
    /// <returns>A <see cref="DateOnly"/> representing the specified date.</returns>
    public static DateOnly CoreDateSerial(int year, int month, int day)
    {
        return new DateOnly(year, month, day);
    }

    /// <summary>
    /// Constructs a <see cref="TimeOnly"/> from hour, minute, and second components.
    /// </summary>
    /// <param name="hour">The hour (0–23).</param>
    /// <param name="minute">The minute (0–59).</param>
    /// <param name="second">The second (0–59).</param>
    /// <returns>A <see cref="TimeOnly"/> representing the specified time.</returns>
    public static TimeOnly CoreTimeSerial(int hour, int minute, int second)
    {
        return new TimeOnly(hour, minute, second);
    }

    /// <summary>
    /// Parses a date string into a <see cref="DateOnly"/> using invariant culture conventions.
    /// </summary>
    /// <param name="dateString">The date string to parse.</param>
    /// <returns>A <see cref="DateOnly"/> parsed from the string.</returns>
    /// <exception cref="FormatException">Thrown if the string cannot be parsed as a date.</exception>
    public static DateOnly CoreDateValue(string dateString)
    {
        return DateOnly.Parse(dateString, CultureInfo.InvariantCulture);
    }

    /// <summary>
    /// Returns the current system date and time.
    /// </summary>
    /// <returns>The current <see cref="DateTime"/>.</returns>
    public static DateTime CoreNow() => DateTime.Now;

    /// <summary>
    /// Returns the current system date (without time).
    /// </summary>
    /// <returns>A <see cref="DateOnly"/> representing today's date.</returns>
    public static DateOnly CoreToday() => DateOnly.FromDateTime(DateTime.Today);

    /// <summary>
    /// Returns the current system time.
    /// </summary>
    /// <returns>A <see cref="TimeOnly"/> representing the current time.</returns>
    public static TimeOnly CoreTimeOnly() => TimeOnly.FromDateTime(DateTime.Now);

    /// <summary>
    /// Returns the current system time as a formatted string "HH:mm:ss".
    /// </summary>
    /// <returns>The current time in "HH:mm:ss" format.</returns>
    public static string CoreTimeString() => DateTime.UtcNow.ToString("HH:mm:ss", CultureInfo.InvariantCulture);

    /// <summary>
    /// Canone data/ora (UTC) — TIME(): seriale data+ora corrente (OLE/Excel, es. 46302.9034027778).
    /// Unica definizione per le 4 gambe: C runtime `gmb_dt_now()`.
    /// </summary>
    public static double CoreTimeSerial() => DateTime.UtcNow.ToOADate();

    /// <summary>Canone data/ora (UTC) — DATE(): seriale del solo giorno corrente (es. 46302).</summary>
    public static double CoreDaySerial() => Math.Floor(DateTime.UtcNow.ToOADate());

    /// <summary>Canone data/ora (UTC) — STRDATE()/DATE$: data corrente "dd/MM/yyyy".</summary>
    public static string CoreDateString() => DateTime.UtcNow.ToString("dd/MM/yyyy", CultureInfo.InvariantCulture);

    /// <summary>
    /// Converts a date to a sortable "yyyyMMdd" string.
    /// </summary>
    /// <param name="dateValue">The date to convert.</param>
    /// <returns>The date formatted as "yyyyMMdd".</returns>
    public static string CoreDtos(DateTime dateValue)
    {
        return dateValue.ToString("yyyyMMdd", CultureInfo.InvariantCulture);
    }

    /// <summary>
    /// Converts a string in "yyyyMMdd" format to a <see cref="DateOnly"/>.
    /// Returns <see cref="DateOnly.MinValue"/> if the string is invalid or not exactly 8 characters.
    /// </summary>
    /// <param name="dateString">The date string in "yyyyMMdd" format (exactly 8 characters).</param>
    /// <returns>A <see cref="DateOnly"/> parsed from the string, or <see cref="DateOnly.MinValue"/> on failure.</returns>
    public static DateOnly CoreStod(string dateString)
    {
        if (string.IsNullOrEmpty(dateString) || dateString.Trim().Length != 8)
            return DateOnly.MinValue;

        return DateOnly.TryParseExact(dateString, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None,
            out var result)
            ? result
            : DateOnly.MinValue;
    }

    /// <summary>
    /// Returns the current system time as a "HH:mm:ss" string (canone unico 2026-09-30:
    /// stesso formato di VM/C `gmb_time_string()`, C99Native; prima "HHmmss").
    /// </summary>
    /// <returns>The current time in "HH:mm:ss" format.</returns>
    public static string CoreTiString() => CoreTimeString();

    /// <summary>
    /// Returns the number of days in the month of the specified date.
    /// </summary>
    /// <param name="dateValue">The date whose month to examine.</param>
    /// <returns>The number of days in the month (28–31).</returns>
    public static int CoreDaysInMonth(DateTime dateValue)
    {
        return DateTime.DaysInMonth(dateValue.Year, dateValue.Month);
    }

    /// <summary>
    /// Determines whether the year of the specified date is a leap year.
    /// </summary>
    /// <param name="dateValue">The date whose year to check.</param>
    /// <returns><c>true</c> if the year is a leap year; otherwise <c>false</c>.</returns>
    public static bool CoreIsLeapYear(DateTime dateValue)
    {
        return DateTime.IsLeapYear(dateValue.Year);
    }

    /// <summary>
    /// Returns the first day of the month for the specified date.
    /// </summary>
    /// <param name="dateValue">The date to evaluate.</param>
    /// <returns>A <see cref="DateOnly"/> representing the first day of the month.</returns>
    public static DateOnly CoreFirstDayOfMonth(DateTime dateValue)
    {
        return new DateOnly(dateValue.Year, dateValue.Month, 1);
    }

    /// <summary>
    /// Returns the ISO 8601 week number of the year for the specified date.
    /// </summary>
    /// <param name="dateValue">The date to evaluate.</param>
    /// <returns>The ISO 8601 week number (1–53).</returns>
    public static int CoreIsoWeekOfYear(DateTime dateValue)
    {
        return ISOWeek.GetWeekOfYear(dateValue);
    }

    /// <summary>
    /// Converts a Unix timestamp (seconds since 1970-01-01) to a <see cref="DateOnly"/>.
    /// </summary>
    /// <param name="unixTimestamp">The Unix timestamp in seconds.</param>
    /// <returns>A <see cref="DateOnly"/> representing the UTC date of the timestamp.</returns>
    public static DateOnly CoreDateFromUnixTimestamp(long unixTimestamp)
    {
        var dateTime = DateTimeOffset.FromUnixTimeSeconds(unixTimestamp).DateTime;
        return DateOnly.FromDateTime(dateTime);
    }

    /// <summary>
    /// Converts a <see cref="DateTime"/> to a Unix timestamp (seconds since 1970-01-01).
    /// </summary>
    /// <param name="dateValue">The date/time to convert.</param>
    /// <returns>The Unix timestamp in seconds.</returns>
    public static long CoreUnixTimestamp(DateTime dateValue)
    {
        return new DateTimeOffset(dateValue).ToUnixTimeSeconds();
    }
}
