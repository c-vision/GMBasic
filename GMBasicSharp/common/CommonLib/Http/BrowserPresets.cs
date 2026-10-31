// FILE: CommonLib.Http.BrowserPresets.cs

namespace CommonLib.Http;

/// <summary>
/// Preset di header realistici (nome + valore + ORDINE) per simulare una richiesta inviata da un
/// browser reale, usati da <see cref="HttpOptions.BrowserPreset"/>. Ogni preset è la sequenza di
/// header che quel browser invia davvero per una GET di navigazione verso una pagina HTML — non
/// un elenco esaustivo/parametrico di tutte le varianti possibili (niente Client Hints dinamici
/// per versione OS reale, niente negoziazione HTTP/2 pseudo-header): l'obiettivo è "sembrare
/// plausibilmente un browser" a un endpoint che ispeziona User-Agent/Accept/ordine header, non
/// un fingerprint bit-perfect (che richiederebbe controllo a livello di stack TLS/HTTP2, fuori
/// portata di HttpClient/SocketsHttpHandler senza riscrivere il livello di trasporto).
/// </summary>
public static class BrowserPresets
{
    /// <summary>
    /// Restituisce la lista ordinata (nome, valore) di header per il preset indicato
    /// (case-insensitive: "chrome", "firefox", "safari"), o lista vuota se il nome non è
    /// riconosciuto (nessun preset applicato, comportamento invariato).
    /// </summary>
    public static IReadOnlyList<(string Name, string Value)> GetHeaders(string? preset) => preset?.Trim().ToLowerInvariant() switch
    {
        "chrome" => Chrome,
        "firefox" => Firefox,
        "safari" => Safari,
        _ => []
    };

    private static readonly (string, string)[] Chrome =
    [
        ("sec-ch-ua", "\"Not_A Brand\";v=\"8\", \"Chromium\";v=\"120\", \"Google Chrome\";v=\"120\""),
        ("sec-ch-ua-mobile", "?0"),
        ("sec-ch-ua-platform", "\"Windows\""),
        ("Upgrade-Insecure-Requests", "1"),
        ("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
        ("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"),
        ("Sec-Fetch-Site", "none"),
        ("Sec-Fetch-Mode", "navigate"),
        ("Sec-Fetch-User", "?1"),
        ("Sec-Fetch-Dest", "document"),
        ("Accept-Encoding", "gzip, deflate, br"),
        ("Accept-Language", "en-US,en;q=0.9"),
    ];

    private static readonly (string, string)[] Firefox =
    [
        ("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0"),
        ("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"),
        ("Accept-Language", "en-US,en;q=0.5"),
        ("Accept-Encoding", "gzip, deflate, br"),
        ("Upgrade-Insecure-Requests", "1"),
        ("Sec-Fetch-Dest", "document"),
        ("Sec-Fetch-Mode", "navigate"),
        ("Sec-Fetch-Site", "none"),
        ("Sec-Fetch-User", "?1"),
    ];

    private static readonly (string, string)[] Safari =
    [
        ("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Safari/605.1.15"),
        ("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"),
        ("Accept-Language", "en-US,en;q=0.9"),
        ("Accept-Encoding", "gzip, deflate, br"),
    ];
}
