namespace CommonLib;

/// <summary>
/// Core di validazione condiviso (Security D, 2026-09-02) — parità con `gmb_validation_*`
/// nel runtime C. Regole SEMPLICI e deterministiche (niente regex onerose): pensate per
/// costruirci sopra la futura "fluent validation" in BASIC e per i controller HTTP/MVC.
/// </summary>
public static class SecurityValidation
{
    /// <summary>Stringa non vuota (dopo trim).</summary>
    public static bool IsRequired(string? value) => !string.IsNullOrWhiteSpace(value);

    /// <summary>Email minimale: una '@', testo prima e dopo, un '.' dopo la '@', niente spazi.</summary>
    public static bool IsEmail(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return false;
        var s = value.Trim();
        if (s.Contains(' ')) return false;
        var at = s.IndexOf('@');
        if (at <= 0 || at == s.Length - 1) return false;
        var domain = s[(at + 1)..];
        if (!domain.Contains('.')) return false;
        return domain.IndexOf('.') > 0 && domain.IndexOf('.') < domain.Length - 1;
    }

    /// <summary>URL http(s): inizia con http:// o https://, contiene '.', niente spazi.</summary>
    public static bool IsUrl(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return false;
        var s = value.Trim();
        if (s.Contains(' ')) return false;
        if (!(s.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
           || s.StartsWith("https://", StringComparison.OrdinalIgnoreCase))) return false;
        return s.Contains('.');
    }

    /// <summary>Carta di credito (Luhn) su cifre; accetta spazi/'-' come separatori.</summary>
    public static bool IsCreditCard(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return false;
        var digits = value.Where(c => char.IsDigit(c)).Select(c => c - '0').ToList();
        if (digits.Count < 12 || digits.Count > 19) return false;
        if (value.Any(c => char.IsLetter(c))) return false;
        int sum = 0;
        bool alt = false;
        for (int i = digits.Count - 1; i >= 0; i--)
        {
            int d = digits[i];
            if (alt) { d *= 2; if (d > 9) d -= 9; }
            sum += d;
            alt = !alt;
        }
        return sum % 10 == 0;
    }

    /// <summary>Lunghezza della stringa compresa in [min, max] (max &lt; 0 = senza limite alto).</summary>
    public static bool IsLengthBetween(string? value, int min, int max)
    {
        var len = (value ?? "").Length;
        return len >= min && (max < 0 || len <= max);
    }
}
