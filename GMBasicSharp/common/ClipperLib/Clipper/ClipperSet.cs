using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Replica delle istruzioni SET di Clipper 5.3 esposte come funzioni C#.
/// Ogni funzione è anche un getter: passando <c>null</c> restituisce il valore corrente.
/// </summary>
public static class ClipperSet
{
    private static ClipperState S() => ClipperGlobal.State;

    /// <summary>SET CONFIRM — con ON il READ non esce dall'ultimo campo con Invio.</summary>
    public static bool SetConfirm(bool? on = null)
    {
        if (on.HasValue) S().SetConfirm = on.Value;
        return S().SetConfirm;
    }

    /// <summary>SET WRAP — con ON le frecce destra/sinistra avvolgono il campo corrente.</summary>
    public static bool SetWrap(bool? on = null)
    {
        if (on.HasValue) S().SetWrap = on.Value;
        return S().SetWrap;
    }

    /// <summary>SET DELIMITERS — coppia di caratteri (o stringa di 2) attorno al campo attivo.</summary>
    public static string SetDelimiters(string? value = null)
    {
        if (value != null)
        {
            if (value.Length == 0) S().SetDelimiters = "";
            else if (value.Length == 1) S().SetDelimiters = value + value;
            else S().SetDelimiters = value[..2];
        }
        return S().SetDelimiters;
    }

    /// <summary>SET ESCAPE — con OFF il tasto ESC non termina il READ.</summary>
    public static bool SetEscape(bool? on = null)
    {
        if (on.HasValue) S().SetEscape = on.Value;
        return S().SetEscape;
    }

    /// <summary>SET BELL — con OFF nessun beep sugli errori di campo.</summary>
    public static bool SetBell(bool? on = null)
    {
        if (on.HasValue) S().SetBell = on.Value;
        return S().SetBell;
    }

    /// <summary>SET TYPEAHEAD — dimensione della coda di tastiera in byte.</summary>
    public static int SetTypeahead(int? size = null)
    {
        if (size.HasValue) S().SetTypeahead = Math.Max(0, size.Value);
        return S().SetTypeahead;
    }

    /// <summary>SET INTENSITY — con OFF i campi non attivi usano colori normali.</summary>
    public static bool SetIntensity(bool? on = null)
    {
        if (on.HasValue) S().SetIntensity = on.Value;
        return S().SetIntensity;
    }

    /// <summary>SET DATE FORMAT — formato usato da DTOC()/CTOD() (es. "dd-mm-yyyy").</summary>
    public static string SetDateFormat(string? format = null)
    {
        if (format is { Length: > 0 }) S().SetDateFormat = format;
        return S().SetDateFormat;
    }

    /// <summary>SET CENTURY — con ON DTOC()/CTOD() includono il secolo (yyyy).</summary>
    public static bool SetCentury(bool? on = null)
    {
        if (on.HasValue) S().SetCentury = on.Value;
        return S().SetCentury;
    }

    /// <summary>SET EPOCH — anno di riferimento per la conversione CTOD() a 2 cifre.</summary>
    public static int SetEpoch(int? year = null)
    {
        if (year.HasValue) S().SetEpoch = year.Value;
        return S().SetEpoch;
    }

    /// <summary>SET DECIMALS — numero di decimali per la visualizzazione numerica.</summary>
    public static int SetDecimals(int? n = null)
    {
        if (n.HasValue) S().SetDecimals = Math.Max(0, n.Value);
        return S().SetDecimals;
    }

    /// <summary>SET FIXED — con ON i numeri sono sempre mostrati con SET DECIMALS decimali.</summary>
    public static bool SetFixed(bool? on = null)
    {
        if (on.HasValue) S().SetFixed = on.Value;
        return S().SetFixed;
    }

    /// <summary>
    /// Formattazione numerica centrale: rispetta SET DECIMALS / SET FIXED.
    /// Con SetDecimals &lt; 0 (mai impostato) e FIXED OFF produce il formato legacy
    /// "G15" (invariato rispetto ai golden esistenti). Con SET DECIMALS TO n applica
    /// fino a n decimali (senza zeri in coda); con SET FIXED ON esattamente n.
    /// </summary>
    public static string FormatNumber(double d)
    {
        int n = S().SetDecimals;
        if (n < 0 && !S().SetFixed)
        {
            // Legacy: identico a FormatDoubleValue storico (G15 + segno-spazio).
            return (d >= 0.0 ? " " : "") + d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture);
        }
        n = Math.Max(0, n);
        string body = S().SetFixed
            ? d.ToString("F" + n, System.Globalization.CultureInfo.InvariantCulture)
            : d.ToString("0." + new string('#', n), System.Globalization.CultureInfo.InvariantCulture);
        return (d >= 0.0 ? " " : "") + body;
    }

    /// <summary>SET PRINTER — con ON l'output (QOUT/QQOUT/DEVOUT) va anche alla stampante.</summary>
    public static bool SetPrinter(bool? on = null)
    {
        if (on.HasValue) S().PrinterOn = on.Value;
        return S().PrinterOn;
    }

    /// <summary>SET DEVICE — "PRINTER" instrada l'output solo alla stampante, "SCREEN" al video.</summary>
    public static string SetDevice(string? device = null)
    {
        if (!string.IsNullOrEmpty(device))
            S().PrinterDevice = device.Trim().ToUpperInvariant();
        return S().PrinterDevice;
    }

    /// <summary>SET MESSAGE TO n — riga in cui mostrare il messaggio; 0 lo disabilita.</summary>
    public static int? SetMessage(int? row = null)
    {
        if (row.HasValue) S().MessageRow = row.Value == 0 ? null : row.Value;
        return S().MessageRow;
    }

    /// <summary>MESSAGE() — testo corrente del messaggio/prompt; con argomento imposta il testo.</summary>
    public static string Message(string? text = null)
    {
        if (text != null) S().MessageText = text;
        return S().MessageText;
    }
}
