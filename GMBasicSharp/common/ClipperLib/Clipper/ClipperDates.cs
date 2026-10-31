using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Globalization;

namespace ClipperLib.Clipper;

/// <summary>
/// Funzioni data/ora di Clipper 5.3: DATE, TIME, YEAR, MONTH, DAY, DOW, CDOW,
/// CMONTH, DTOC, DTOS, CTOD, SECONDS. Rispettano SET DATE FORMAT, SET CENTURY e SET EPOCH.
/// </summary>
public static class ClipperDates
{
    private static ClipperState S() => ClipperGlobal.State;

    /// <summary>DATE() — data odierna.</summary>
    public static DateTime Date() => DateTime.Today;

    /// <summary>TIME() — ora corrente come stringa "HH:MM:SS".</summary>
    public static string Time() => DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture);

    /// <summary>SECONDS() — secondi trascorsi dalla mezzanotte.</summary>
    public static int Seconds() => (int)DateTime.Now.TimeOfDay.TotalSeconds;

    /// <summary>YEAR(d) — anno (int).</summary>
    public static int Year(DateTime d) => d.Year;

    /// <summary>MONTH(d) — mese 1-12.</summary>
    public static int Month(DateTime d) => d.Month;

    /// <summary>DAY(d) — giorno del mese.</summary>
    public static int Day(DateTime d) => d.Day;

    /// <summary>DOW(d) — giorno della settimana (1=domenica … 7=sabato).</summary>
    public static int Dow(DateTime d) => (int)d.DayOfWeek + 1;

    /// <summary>CDOW(d) — nome del giorno della settimana in inglese.</summary>
    public static string CDow(DateTime d) => d.DayOfWeek.ToString();

    /// <summary>CMONTH(d) — nome del mese in inglese.</summary>
    public static string CMonth(DateTime d) => d.ToString("MMMM", CultureInfo.InvariantCulture);

    /// <summary>DTOC(d) — data come stringa nel formato di SET DATE FORMAT.</summary>
    public static string Dtoc(DateTime d) => FormatDate(d, S().SetDateFormat, S().SetCentury);

    /// <summary>DTOS(d) — data come stringa "yyyymmdd" (per indici, indipendente da SET DATE FORMAT).</summary>
    public static string Dtos(DateTime d) => d.ToString("yyyyMMdd", CultureInfo.InvariantCulture);

    /// <summary>CTOD(s) — stringa data (formato SET DATE FORMAT) in DateTime; null se non valida.</summary>
    public static DateTime? Ctod(string s)
    {
        if (string.IsNullOrWhiteSpace(s)) return null;
        var format = S().SetDateFormat;
        var tokens = Tokenize(format);
        var digitRuns = DigitRuns(s);
        var digitTokens = tokens.Where(t => t.kind != "lit").ToList();
        if (digitRuns.Count < digitTokens.Count) return null;

        int mm = 1, dd = 1, yyyy = 0;
        var hasYear = false;
        for (var i = 0; i < digitTokens.Count; i++)
        {
            var (kind, _) = digitTokens[i];
            var digits = digitRuns[i];
            switch (kind)
            {
                case "mm":
                    if (digits.Length != 2 || !int.TryParse(digits, out mm)) return null;
                    break;
                case "dd":
                    if (digits.Length != 2 || !int.TryParse(digits, out dd)) return null;
                    break;
                case "yyyy":
                    hasYear = true;
                    if (digits.Length != 4 || !int.TryParse(digits, out yyyy)) return null;
                    break;
                case "yy":
                {
                    hasYear = true;
                    if (digits.Length != 2 || !int.TryParse(digits, out var yy)) return null;
                    var epoch = S().SetEpoch;
                    yyyy = epoch - (epoch % 100) + yy;
                    if (yyyy < epoch) yyyy += 100;
                    break;
                }
            }
        }

        if (!hasYear) yyyy = DateTime.Today.Year;
        if (mm < 1 || mm > 12 || dd < 1 || dd > DateTime.DaysInMonth(yyyy, mm)) return null;
        return new DateTime(yyyy, mm, dd);
    }

    /// <summary>
    /// Formatta la data secondo un formato Clipper (mm, dd, yy, yyyy + separatori letterali).
    /// Se century è false il token "yy" produce 2 cifre, altrimenti 4.
    /// </summary>
    private static string FormatDate(DateTime d, string format, bool century)
    {
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < format.Length;)
        {
            var c = char.ToLowerInvariant(format[i]);
            if (c is 'm' or 'd' or 'y')
            {
                var j = i;
                while (j < format.Length && char.ToLowerInvariant(format[j]) == c) j++;
                var len = j - i;
                switch (c)
                {
                    case 'm':
                        if (len >= 3) sb.Append(d.ToString("MMM", CultureInfo.InvariantCulture));
                        else sb.Append(d.Month.ToString("00", CultureInfo.InvariantCulture));
                        break;
                    case 'd':
                        if (len >= 3) sb.Append(d.ToString("ddd", CultureInfo.InvariantCulture));
                        else sb.Append(d.Day.ToString("00", CultureInfo.InvariantCulture));
                        break;
                    case 'y':
                        if (len >= 4) sb.Append(d.Year.ToString("0000", CultureInfo.InvariantCulture));
                        else if (century) sb.Append(d.Year.ToString("0000", CultureInfo.InvariantCulture));
                        else sb.Append((d.Year % 100).ToString("00", CultureInfo.InvariantCulture));
                        break;
                }
                i = j;
            }
            else
            {
                sb.Append(format[i]);
                i++;
            }
        }
        return sb.ToString();
    }

    private static List<(string kind, string literal)> Tokenize(string format)
    {
        var tokens = new List<(string, string)>();
        for (var i = 0; i < format.Length;)
        {
            var c = char.ToLowerInvariant(format[i]);
            if (c is 'm' or 'd' or 'y')
            {
                var j = i;
                while (j < format.Length && char.ToLowerInvariant(format[j]) == c) j++;
                var len = j - i;
                tokens.Add((c == 'y' && len >= 4 ? "yyyy" : c == 'y' ? "yy" : c == 'm' ? "mm" : "dd", ""));
                i = j;
            }
            else
            {
                tokens.Add(("lit", format[i].ToString()));
                i++;
            }
        }
        return tokens;
    }

    private static List<string> DigitRuns(string s)
    {
        var runs = new List<string>();
        var cur = new System.Text.StringBuilder();
        foreach (var c in s)
        {
            if (char.IsDigit(c)) cur.Append(c);
            else if (cur.Length > 0)
            {
                runs.Add(cur.ToString());
                cur.Clear();
            }
        }
        if (cur.Length > 0) runs.Add(cur.ToString());
        return runs;
    }
}
