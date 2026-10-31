// Browser/ChromiumBrowserProvider.cs — provider Chromium (esistente)
// Wrappa FetchChromiumLib + launch_chromium.sh (già in plugins/browser/).
using Basic.AI.ModelDownload;
using CommonLib;

namespace Basic.AI.Browser;

public sealed class ChromiumBrowserProvider : IBrowserProvider
{
    public string Name => "chromium";
    public bool IsAvailable => FetchChromiumLib.IsCached(ChromiumCache.ResolveRid());
    public string CacheDir => ChromiumCache.Dir(ChromiumCache.ResolveRid());
    public string? BinaryPath
    {
        get
        {
            // Cache CANONICA del downloader (~/.gmbasic/libs/chromium/<rid>) e, se non c'è, la copia
            // nel checkout (<repo>/libs-runtime/chromium/<rid>, stessa che usa launch_chromium.sh):
            // senza il fallback il binario del checkout era invisibile al provider (che quindi
            // riportava "non disponibile" pur avendo il browser).
            var dir = CacheDir;
            if (!Directory.Exists(dir))
            {
                var alt = Path.Combine(GmbiPaths.RepoRoot, "libs-runtime", "chromium", ChromiumCache.ResolveRid());
                if (Directory.Exists(alt)) dir = alt; else return null;
            }
            // Cerca il binario dentro la cache (chrome / chrome.exe / Chromium.app / Google Chrome for Testing).
            // Su macOS il binario è <...>.app/Contents/MacOS/<nome-app> (es. "Google Chrome for Testing"),
            // con eventuali "Helper"/"Framework" annidati: esclude quelli e preferisce il path più corto
            // (il binario principale dell'app).
            var files = Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories)
                .Where(f =>
                {
                    var fn = Path.GetFileName(f);
                    return fn.EndsWith("chrome", StringComparison.OrdinalIgnoreCase)
                        || fn.EndsWith("chrome.exe", StringComparison.OrdinalIgnoreCase)
                        || fn.Equals("Chromium", StringComparison.OrdinalIgnoreCase)
                        || fn.Equals("Google Chrome for Testing", StringComparison.OrdinalIgnoreCase);
                })
                .Where(f => !f.Contains("Helper", StringComparison.OrdinalIgnoreCase)
                         && !f.Contains("Framework", StringComparison.OrdinalIgnoreCase))
                .OrderBy(f => f.Length)
                .FirstOrDefault();
            return files;
        }
    }

    public Task<BrowserLaunchResult> LaunchAsync(int port, string? profileDir = null, CancellationToken ct = default)
    {
        // Il browser deve esserci in cache; lo SCRIPT è solo un dettaglio di come lo si lancia
        // (in distribuzione può non esserci: BrowserStarter avvia il binario direttamente).
        // Prima si pretendeva lo script nel checkout → su una macchina senza sorgenti il
        // provider riportava "launch_chromium.sh non trovato" anche col browser già scaricato.
        var bin = BinaryPath;
        var script = Path.Combine(AppContext.BaseDirectory, "plugins", "browser", "launch_chromium.sh");
        if (string.IsNullOrEmpty(bin) && !File.Exists(script) &&
            !File.Exists(Path.Combine(GmbiPaths.RepoRoot, "plugins", "browser", "launch_chromium.sh")))
            return Task.FromResult(new BrowserLaunchResult(false,
                $"chromium non trovato (cache {CacheDir}, nessuno script di lancio) — scaricalo con: gmb setup BROWSER", port, null, profileDir));
        return Task.FromResult(new BrowserLaunchResult(true, null, port, bin, profileDir));
    }
}
