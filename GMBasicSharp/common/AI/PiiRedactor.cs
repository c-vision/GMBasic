// FILE: Basic.AI.PiiRedactor
// Anonimizzazione automatica di PII (Personal Identifiable Information) su testo
// estratto (es. da AI.OCR). Sostituisce i dati sensibili con segnaposto deterministici
// ([EMAIL], [PHONE], [TAX_ID], ...) mantenendo leggibile la struttura del documento.
namespace Basic.AI;

using System.Text.RegularExpressions;

/// <summary>Redige i dati personali da un testo, usato da AI.OCR(..., redact: TRUE) per
/// anonimizzare automaticamente i documenti scansionati (contratti, fatture, moduli).</summary>
public static class PiiRedactor
{
    // Ordine: le regole più specifiche prima (IBAN/VAT/carta/tax prima di phone, che è greedy).
    private static readonly (string Pattern, string Tag)[] Rules =
    {
        // Secret key: sk-/ghp-/AKIA prefixed tokens (GitHub, AWS, OpenAI-style)
        (@"\b(?:(?:sk|ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{10,}|AKIA[A-Z0-9]{16}|eyJ[A-Za-z0-9_\-]{10,})\b", "[SECRET]"),
        // JWT (starts with eyJ... base64url token)
        (@"\beyJ[A-Za-z0-9_\-]{10,}\.[A-Za-z0-9_\-]{10,}\.[A-Za-z0-9_\-]{10,}\b", "[JWT]"),
        // Cookie session/token: name=value patterns
        (@"\b(session|token|auth|ssid|sid|jsessionid|connect\.sid)[=:][A-Za-z0-9_\-]{8,}\b", "[COOKIE]"),
        // Email
        (@"[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}", "[EMAIL]"),
        // URL
        (@"https?://[A-Za-z0-9./\-_%?&=#:~]+", "[URL]"),
        // IBAN: 2 lettere + 2 cifre + 11-30 alfanumerici
        (@"\b[A-Z]{2}\d{2}[A-Z0-9]{11,30}\b", "[IBAN]"),
        // Partita IVA italiana: "IT" opzionale + 11 cifre
        (@"\b(?:IT)?\d{11}\b", "[VAT_ID]"),
        // Codice fiscale italiano: 16 caratteri alfanumerici (XXXXXX99X99X999X)
        (@"\b[A-Z]{6}\d{2}[A-Z]\d{2}[A-Z]\d{3}[A-Z]\b", "[TAX_ID]"),
        // Carta di credito: 13-19 cifre (con spazi/tratte tra gruppi da 4)
        (@"(?:\d[ \-]?){13,19}", "[CARD]"),
        // Data di nascita / data completa (giorno/mese/anno)
        (@"\b\d{1,2}[/\-. ]\d{1,2}[/\-. ]\d{2,4}\b", "[DATE]"),
        // Indirizzo IP
        (@"\b(?:\d{1,3}\.){3}\d{1,3}\b", "[IP]"),
        // Numero civico con via/strada
        (@"\b(?:via|viale|corso|piazza|strada|street|avenue|road)\s+[A-Za-zÀ-ù' ]+\s*\d{1,5}\b",
            "[ADDRESS]"),
        // Telefono: prefisso internazionale opzionale, 6-15 cifre
        (@"(?<!\d)(?:\+?\d{1,3}[ .\-]?)?(?:\(\d{1,4}\)[ .\-]?)?(?:\d{2,4}[ .\-]?){2,3}\d{2,4}(?!\d)", "[PHONE]"),
    };

    /// <summary>Redige tutte le PII riconosciute nel testo. Il testo restituito mantiene la
    /// struttura originale con i dati sensibili sostituiti da segnaposto deterministici.</summary>
    public static string Redact(string? text)
    {
        if (string.IsNullOrWhiteSpace(text)) return text ?? "";

        // Applica le regole in sequenza sul testo (ognuna sul testo già redatto): l'ordine
        // dei Rules garantisce che le regole più specifiche corrano prima.
        foreach (var (pattern, tag) in Rules)
        {
            var rx = new Regex(pattern, RegexOptions.IgnoreCase | RegexOptions.Compiled);
            text = rx.Replace(text, tag);
        }
        return text;
    }

    /// <summary>G9: redige i valori sensibili dagli argomenti di una tool call.
    /// Se un argomento contiene una chiave sensibile (password, secret, token, apiKey, auth),
    /// lo sostituisce con [REDACTED]. Ritorna una stringa JSON-safe dei parametri.</summary>
    public static string RedactArgs(string argsJson)
    {
        if (string.IsNullOrWhiteSpace(argsJson)) return argsJson;
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(argsJson);
            var sb = new System.Text.StringBuilder();
            sb.Append('{');
            bool first = true;
            foreach (var p in doc.RootElement.EnumerateObject())
            {
                if (!first) sb.Append(',');
                first = false;
                sb.Append('"').Append(p.Name).Append("\":");
                // Chiavi sensibili: redige il valore completamente
                var low = p.Name.ToLowerInvariant();
                if (low is "password" or "secret" or "token" or "apikey" or "auth" or "credential" or "privatekey" or "passphrase")
                {
                    sb.Append("\"[REDACTED]\"");
                }
                else if (p.Value.ValueKind == System.Text.Json.JsonValueKind.String)
                {
                    var val = p.Value.GetString() ?? "";
                    sb.Append('"').Append(Redact(val)).Append('"');
                }
                else
                {
                    sb.Append(p.Value.GetRawText());
                }
            }
            sb.Append('}');
            return sb.ToString();
        }
        catch
        {
            // fallback: redact completo
            return Redact(argsJson);
        }
    }
}
