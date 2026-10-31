using System.Net;

namespace CommonLib.Http;

/// <summary>
/// Helper condivisi per la sicurezza del server HTTP (HTTP CONFIG, 2026-09-02):
/// - IP allow/block list con CIDR IPv4 (e IP singoli)
/// - header di sicurezza automatici (HSTS/CSP/X-Frame-Options/X-Content-Type-Options/…)
/// Implementazione pura, senza stato — usata dal backend IL (HttpService) e dal PCode
/// (PCodeHttpServer); il VM/C userà le corrispondenti gmb_* nel runtime C.
/// </summary>
public static class HttpSecurity
{
    /// <summary>Header di sicurezza applicati di default quando HTTP CONFIG .SecurityHeaders = TRUE.</summary>
    public static IReadOnlyDictionary<string, string> DefaultSecurityHeaders()
        => new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains",
            ["Content-Security-Policy"] = "default-src 'self'",
            ["X-Frame-Options"] = "SAMEORIGIN",
            ["X-Content-Type-Options"] = "nosniff",
            ["Referrer-Policy"] = "strict-origin-when-cross-origin",
            ["X-Permitted-Cross-Domain-Policies"] = "none",
        };

    /// <summary>True se l'IP remoto è ammesso secondo le liste (configurate in HTTP CONFIG):
    /// lista allow non vuota → deve combaciare; lista block → mai combaciare. Le liste sono
    /// separate da virgola; elementi = IP singolo o CIDR IPv4 (es. 10.0.0.0/8).</summary>
    public static bool IpAllowed(string remoteIp, string allowedList, string blockedList)
    {
        var ip = ParseIp(remoteIp);
        if (ip == null) return true; // IP non risolvibile → non bloccare (fail-open sul parse)

        if (!string.IsNullOrWhiteSpace(allowedList) && !MatchesAny(allowedList, ip.Value)) return false;
        if (MatchesAny(blockedList, ip.Value)) return false;
        return true;
    }

    private static bool MatchesAny(string csv, uint ip)
    {
        if (string.IsNullOrWhiteSpace(csv)) return false;
        foreach (var part in csv.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            if (MatchEntry(part, ip)) return true;
        return false;
    }

    private static bool MatchEntry(string entry, uint ip)
    {
        if (entry.Contains('/'))
        {
            var slash = entry.IndexOf('/');
            if (!uint.TryParse(entry[(slash + 1)..], out var bits) || bits > 32) return false;
            var addr = ParseIp(entry[..slash].Trim());
            if (addr == null) return false;
            uint mask = bits == 0 ? 0 : (uint.MaxValue << (int)(32 - bits));
            return (ip & mask) == (addr.Value & mask);
        }
        var single = ParseIp(entry);
        return single == ip;
    }

    private static uint? ParseIp(string ip)
    {
        if (string.IsNullOrWhiteSpace(ip)) return null;
        if (IPAddress.TryParse(ip.Trim(), out var addr) && addr.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
        {
            var b = addr.GetAddressBytes();
            return ((uint)b[0] << 24) | ((uint)b[1] << 16) | ((uint)b[2] << 8) | b[3];
        }
        return null;
    }
}
