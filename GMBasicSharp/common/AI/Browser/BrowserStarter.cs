// Browser/BrowserStarter.cs — BROWSER.START: lancia il browser (Chromium via
// launch_chromium.sh, Obscura via `obscura serve`) se non già su CDP, poi connette.
// Cross-platform: usa il manifest del provider per trovare il binario in cache.
using System.Diagnostics;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Basic.AI.ModelDownload;

namespace Basic.AI.Browser;

public static class BrowserStarter
{
    public static async Task<string> StartAndConnectAsync(string provider, int port, string? profile = null, CancellationToken ct = default)
    {
        // 1) già attivo?
        var probe = await ProbeCdpAsync(port);
        if (probe) return await ConnectAsync(provider, port, "already running");

        var effective = provider.ToLowerInvariant();
        if (effective is not ("obscura" or "chromium" or "auto")) effective = "auto";
        if (effective == "auto")
            effective = FetchObscuraLib.IsCached(ObscuraCache.ResolveRid()) ? "obscura" : "chromium";

        string? launched = null;
        if (effective == "obscura")
            launched = LaunchObscura(port);
        else
            launched = LaunchChromium(port, profile);
        if (launched != null) return launched;

        // attende il CDP (max ~20s) poi connette
        for (var i = 0; i < 40; i++)
        {
            if (await ProbeCdpAsync(port)) break;
            await Task.Delay(500, ct);
        }
        if (!await ProbeCdpAsync(port))
            return $"error: {effective} non è partito su 127.0.0.1:{port} (gmb setup {effective.ToUpperInvariant()})";
        return await ConnectAsync(effective, port, null);
    }

    private static async Task<string> ConnectAsync(string provider, int port, string? note)
    {
        var r = await BrowserRuntime.EnsureConnectedAsync(provider, port);
        return r.Success ? $"connected: {provider} on {port}" + (note == null ? "" : " (" + note + ")") : "error: " + r.Error;
    }

    private static async Task<bool> ProbeCdpAsync(int port)
    {
        foreach (var host in new[] { "127.0.0.1", "[::1]" })
        {
            try
            {
                using var http = new HttpClient { Timeout = TimeSpan.FromMilliseconds(600) };
                var s = await http.GetStringAsync($"http://{host}:{port}/json/version");
                if (s.Contains("webSocketDebuggerUrl")) return true;
            }
            catch { }
        }
        return false;
    }

    /// <summary>Trova `launch_chromium.sh`: PRIMA accanto all'app (distribuzione: finisce
    /// nell'output del build), poi nel checkout. String.Empty se non c'è in nessuno dei due.</summary>
    private static string FindLaunchScript()
    {
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "plugins", "browser", "launch_chromium.sh"),
            Path.Combine(CommonLib.GmbiPaths.RepoRoot, "plugins", "browser", "launch_chromium.sh"),
        };
        foreach (var c in candidates) if (File.Exists(c)) return c;
        return string.Empty;
    }

    /// <summary>Avvio DIRETTO del browser (niente script): serve su una macchina dove il
    /// checkout non esiste (programma compilato/distribuito). Gli stessi flag dello script —
    /// headless nuovo, CDP sulla porta, profilo dedicato, niente first-run/popup.</summary>
    private static string? LaunchChromiumDirect(int port, string? profile = null)
    {
        var bin = new ChromiumBrowserProvider().BinaryPath;
        if (string.IsNullOrEmpty(bin)) return "error: chromium non in cache (gmb setup BROWSER)";
        try
        {
            var userData = Path.Combine(CommonLib.GmbiPaths.LibsDir, "chromium", "profiles",
                string.IsNullOrWhiteSpace(profile) ? "default" : profile);
            Directory.CreateDirectory(userData);
            var args = $"--headless=new --disable-gpu --disable-extensions --disable-component-update " +
                       $"--disable-notifications --disable-blink-features=AutomationControlled " +
                       $"--remote-debugging-port={port} --user-data-dir=\"{userData}\" " +
                       $"--no-first-run --no-default-browser-check about:blank";
            var psi = new ProcessStartInfo(bin, args) { UseShellExecute = false };
            if (!string.IsNullOrWhiteSpace(profile)) psi.Environment["GMBASIC_CHROMIUM_PROFILE"] = profile;
            Process.Start(psi);
            return null;
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }

    private static string? LaunchChromium(int port, string? profile = null)
    {
        var script = FindLaunchScript();
        if (string.IsNullOrEmpty(script)) return LaunchChromiumDirect(port, profile);   // nessun checkout
        try
        {
            // nohup + log su file: il figlio NON deve ereditare stdout/stderr, altrimenti
            // l'harness (test_ilrun/shell) resta appeso fino alla sua uscita.
            var psi = new ProcessStartInfo("bash", $"-c \"nohup bash '{script}' {port} >/tmp/gmb-browser-{port}.log 2>&1 &\"")
            { UseShellExecute = false };
            // isolamento sessione per-utente (residuo B): profilo nominativo -> env letto da launch_chromium.sh
            if (!string.IsNullOrWhiteSpace(profile))
                psi.Environment["GMBASIC_CHROMIUM_PROFILE"] = profile;
            Process.Start(psi);
            return null;
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }

    private static string? LaunchObscura(int port)
    {
        var bin = new ObscuraBrowserProvider().BinaryPath;
        if (bin == null) return "error: Obscura non in cache (gmb setup OBSCURA)";
        try
        {
            var psi = new ProcessStartInfo("bash", $"-c \"nohup '{bin}' serve --port {port} >/tmp/gmb-obscura-{port}.log 2>&1 &\"")
            { UseShellExecute = false };
            Process.Start(psi);
            return null;
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }
}
