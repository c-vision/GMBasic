// Browser/BrowserProviderRegistry.cs — registro BrowserProvider (Chromium | Obscura)
// L'agente non conosce il provider: vede solo browser.navigate/click/... — il runtime
// sceglie in base a `AI AGENT SET nome, browser, "obscura|chromium|auto"` o al default
// globale (system.settings.json → browser.provider).
using CommonLib;

namespace Basic.AI.Browser;

public static class BrowserProviderRegistry
{
    private static readonly IBrowserProvider Chromium = new ChromiumBrowserProvider();
    private static readonly IBrowserProvider Obscura = new ObscuraBrowserProvider();

    /// <summary>Provider disponibili (sempre entrambi, IsAvailable dice se cached).</summary>
    public static IReadOnlyList<IBrowserProvider> All => [Chromium, Obscura];

    /// <summary>Default globale (da system.settings.json: browser.provider | BROWSER_PROVIDER env).</summary>
    public static string DefaultProviderName
    {
        get
        {
            var env = Environment.GetEnvironmentVariable("BROWSER_PROVIDER");
            if (!string.IsNullOrWhiteSpace(env)) return Normalize(env);
            // Leggi da system.settings.json se presente (sezione "browser": {"provider":"obscura"})
            try
            {
                var file = GmbiPaths.SettingsFile;
                if (File.Exists(file))
                {
                    var json = File.ReadAllText(file);
                    using var doc = System.Text.Json.JsonDocument.Parse(json);
                    if (doc.RootElement.TryGetProperty("browser", out var b) && b.TryGetProperty("provider", out var p))
                    {
                        var s = p.GetString();
                        if (!string.IsNullOrWhiteSpace(s)) return Normalize(s);
                    }
                }
            }
            catch { }
            return "auto"; // auto = scegli il primo disponibile (obscura preferito se cached)
        }
    }

    public static IBrowserProvider Resolve(string? requested)
    {
        var name = Normalize(requested ?? DefaultProviderName);
        if (name == "chromium") return Chromium;
        if (name == "obscura") return Obscura;
        // auto: preferisci obscura se disponibile (leggero, agente), altrimenti chromium se disponibile
        if (Obscura.IsAvailable) return Obscura;
        if (Chromium.IsAvailable) return Chromium;
        // Se nessuno è cached, ritorna chromium come fallback (con IsAvailable=false → errore esplicito al launch)
        return Chromium;
    }

    public static string Normalize(string s) => s.Trim().ToLowerInvariant() switch
    {
        "chrome" => "chromium",
        "chromium" => "chromium",
        "obscura" => "obscura",
        "auto" => "auto",
        _ => "auto"
    };

    /// <summary>Nome motore da codice numerico `%AUTO/%CHROMIUM/%OBSCURA` (0 AUTO, 1 CHROMIUM, 2 OBSCURA).
    /// Unico punto di traduzione codice→stringa per la superficie BASIC (mai stringhe nel
    /// linguaggio per identificare il motore). Valori fuori range → "auto" (default).</summary>
    public static string EngineName(double engine)
    {
        var code = (int)Math.Round(engine);
        return code switch
        {
            1 => "chromium",
            2 => "obscura",
            _ => "auto"
        };
    }

    public static IReadOnlyList<string> AvailableNames => All.Where(p => p.IsAvailable).Select(p => p.Name).ToList();
}
