using System.Text;

namespace CommonLib;

/// <summary>
/// Core di encoding condiviso (parità byte-per-byte con il runtime C `gmb_html_encode`/
/// `gmb_url_encode` in gmb_runtime_c). Set canonico definito qui — NON WebUtility (che
/// numeric-encoda anche il range Latin-1 0xA0-0xFF): il C deve riprodurre ESATTAMENTE
/// queste due funzioni. I backend chiamano i core condivisi, mai codice inline.
/// </summary>
public static class WebEncoding
{
    /// <summary>HTML-encode (OWASP minimal set): &amp; &lt; &gt; &quot; &#39;. Tutti gli altri
    /// caratteri passano invariati (inclusi i non-ASCII UTF-8). Stringa input vuota/null → "".</summary>
    public static string CoreHtmlEncode(string value)
    {
        if (string.IsNullOrEmpty(value)) return value ?? string.Empty;
        var sb = new StringBuilder(value.Length);
        foreach (var ch in value)
        {
            switch (ch)
            {
                case '&': sb.Append("&amp;"); break;
                case '<': sb.Append("&lt;"); break;
                case '>': sb.Append("&gt;"); break;
                case '"': sb.Append("&quot;"); break;
                case '\'': sb.Append("&#39;"); break;
                default: sb.Append(ch); break;
            }
        }
        return sb.ToString();
    }

    /// <summary>URL percent-encode RFC 3986 (unreserved A-Za-z0-9-._~ conservati, %HH maiuscolo,
    /// input UTF-8) — parità con Uri.EscapeDataString e gmb_url_encode.</summary>
    public static string CoreUrlEncode(string value)
        => Uri.EscapeDataString(value ?? string.Empty);

    /// <summary>JS string-encode per literal a doppi apici: escape di \ " ' e controlli C0
    /// (\b \t \n \f \r, altri C0 come \u00xx minuscolo) + U+2028/U+2029 → \u2028/\u2029.
    /// Non-ASCII invariati — parità con gmb_js_encode (runtime C).</summary>
    public static string CoreJsEncode(string value)
    {
        if (string.IsNullOrEmpty(value)) return value ?? string.Empty;
        var sb = new StringBuilder(value.Length + 8);
        foreach (var ch in value)
        {
            switch (ch)
            {
                case '\\': sb.Append("\\\\"); break;
                case '"': sb.Append("\\\""); break;
                case '\'': sb.Append("\\'"); break;
                case '\b': sb.Append("\\b"); break;
                case '\t': sb.Append("\\t"); break;
                case '\n': sb.Append("\\n"); break;
                case '\f': sb.Append("\\f"); break;
                case '\r': sb.Append("\\r"); break;
                case '\u2028': sb.Append("\\u2028"); break;
                case '\u2029': sb.Append("\\u2029"); break;
                default:
                    if (ch < 0x20) sb.Append("\\u").Append(((int)ch).ToString("x4"));
                    else sb.Append(ch);
                    break;
            }
        }
        return sb.ToString();
    }
}
