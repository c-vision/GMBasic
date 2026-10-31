// Browser/BrowserRuntime.cs — facade process-wide per il browser attivo (Universal Action Runtime).
// G7: sessioni multiple (fork), lock per-sessione (CdpBrowserClient._gate).
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.Browser;

public static class BrowserRuntime
{
    private static CdpBrowserClient? _client;
    private static int _port;
    private static string _providerName = "auto";
    private static readonly SemaphoreSlim _gate = new(1, 1);
    // G7: sessioni forkate (dict nome→client CDP)
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, CdpBrowserClient> _sessions = new();
    private static string _activeSession = "default";

    public static int DefaultPort =>
        int.TryParse(System.Environment.GetEnvironmentVariable("GMBASIC_CDP_PORT"), out var p) && p > 0 ? p : 9222;

    /// <summary>Fork: clona lo stato auth in una nuova sessione CDP (nuovo page target).
    /// Ritorna messaggio ok/errore. src="default" se omesso.</summary>
    public static async Task<string> ForkSessionAsync(string dst, string? src = null, CancellationToken ct = default)
    {
        src ??= "default";
        if (string.IsNullOrWhiteSpace(dst)) return "error: nome sessione dst vuoto";
        var srcClient = src == "default" ? _client : _sessions.GetValueOrDefault(src);
        if (srcClient == null) return $"error: sessione sorgente '{src}' non trovata";

        // Crea nuovo page target via CDP
        using var tempMs = new System.IO.MemoryStream();
        // Non possiamo chiamare CdpBrowserClient internamente senza ws — dobbiamo usare il client esistente
        // e CDP Target.createTarget tramite SendAsync.
        // Invece, connetti alla stessa porta e crea un nuovo client
        var newClient = await CdpBrowserClient.ConnectAsync(_port, ct);
        if (newClient == null) return "error: fork — impossibile connettere nuova sessione CDP";

        // Copia cookies dalla sorgente
        var cookiesResult = await srcClient.GetCookiesAsync(ct);
        if (cookiesResult.Success && cookiesResult.Data is { } json && json.Length > 2)
        {
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            if (doc.RootElement.TryGetProperty("cookies", out var arr) || doc.RootElement.TryGetProperty("result", out arr))
            {
                foreach (var c in arr.EnumerateArray())
                {
                    string n = c.TryGetProperty("name", out var nv) ? nv.GetString() ?? "" : "";
                    string v = c.TryGetProperty("value", out var vv) ? vv.GetString() ?? "" : "";
                    if (!string.IsNullOrEmpty(n)) await newClient.SetCookieAsync(n, v, ct: ct);
                }
            }
        }

        _sessions[dst] = newClient;
        return $"ok: sessione '{dst}' forkata da '{src}'";
    }

    public static async Task<BrowserActionResult> EnsureConnectedAsync(string? provider, int? port, CancellationToken ct = default)
    {
        var want = port ?? DefaultPort;
        await _gate.WaitAsync(ct);
        try
        {
            if (_client != null && _port == want) return new BrowserActionResult(true, null, "already connected");
            if (_client != null) { try { await _client.DisposeAsync(); } catch {} _client = null; }
            _port = want;
            _providerName = provider ?? _providerName;
            var c = await CdpBrowserClient.ConnectAsync(want, ct);
            if (c == null)
                return new BrowserActionResult(false, $"CDP connect fallita su 127.0.0.1:{want} (provider={_providerName}) — avvia Chromium/Obscura (launch_chromium.sh 9222)", null);
            _client = c;
            _sessions["default"] = c;
            try { await _client.ApplyStealthAsync(ct); } catch { }
            return new BrowserActionResult(true, null, $"connected to {_providerName} on {want}");
        }
        finally { _gate.Release(); }
    }

    private static async Task<BrowserActionResult> EnsureLazyAsync(CancellationToken ct)
    {
        if (_client == null) return await EnsureConnectedAsync(null, null, ct);
        return new BrowserActionResult(true, null, "already connected");
    }

    public static async Task<BrowserActionResult> NavigateAsync(string url, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.NavigateAsync(url, ct);
    }
    public static async Task<BrowserActionResult> SnapshotAsync(CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.SnapshotAsync(ct);
    }
    public static async Task<BrowserActionResult> ClickAsync(string selector, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.ClickAsync(selector, ct);
    }
    public static async Task<BrowserActionResult> FillAsync(string selector, string value, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.FillAsync(selector, value, ct);
    }
    public static async Task<BrowserActionResult> EvaluateAsync(string expression, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.EvaluateAsync(expression, ct);
    }

    public static async Task<BrowserActionResult> AccessibilityTreeAsync(CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.AccessibilityTreeAsync(ct);
    }

    public static async Task<BrowserActionResult> ScreenshotAsync(string? path = null, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.ScreenshotAsync(path, ct);
    }
    public static async Task<BrowserActionResult> PrintToPdfAsync(string? path = null, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.PrintToPdfAsync(path, ct);
    }

    public static async Task<BrowserActionResult> GetCookiesAsync(CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.GetCookiesAsync(ct);
    }
    public static async Task<BrowserActionResult> SetCookieAsync(string name, string value, string? url = null, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.SetCookieAsync(name, value, url, ct);
    }
    public static async Task<BrowserActionResult> ClearCookiesAsync(CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.ClearCookiesAsync(ct);
    }
    public static async Task<BrowserActionResult> SetHeaderAsync(string name, string value, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.SetHeaderAsync(name, value, ct);
    }
    public static async Task<BrowserActionResult> SetUserAgentAsync(string userAgent, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.SetUserAgentAsync(userAgent, ct);
    }
    public static async Task<BrowserActionResult> HumanClickAsync(string selector, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.HumanClickAsync(selector, ct);
    }
    public static async Task<BrowserActionResult> HumanFillAsync(string selector, string value, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.HumanFillAsync(selector, value, ct);
    }
    public static async Task<BrowserActionResult> HumanScrollAsync(int dy, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.HumanScrollAsync(dy, ct);
    }

    public static async Task<BrowserActionResult> ObserveAsync(string? preset = null, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.ObserveAsync(preset, ct);
    }

    public static async Task<BrowserActionResult> FindAsync(string text, bool regex = false, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.FindAsync(text, regex, ct);
    }

    public static async Task<BrowserActionResult> NetLogAsync(bool clear = false, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.NetLogAsync(clear, ct);
    }

    public static async Task<BrowserActionResult> ConsoleAsync(bool clear = false, CancellationToken ct = default)
    {
        var c = await EnsureLazyAsync(ct);
        if (!c.Success) return c;
        return await _client!.ConsoleAsync(clear, ct);
    }

    public static void Disconnect()
    {
        if (_client != null) { try { _client.DisposeAsync().AsTask().Wait(1000); } catch {} _client = null; }
    }
}
