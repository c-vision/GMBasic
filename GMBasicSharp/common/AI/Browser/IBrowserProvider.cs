// Browser/IBrowserProvider.cs — ABI del BrowserProvider (Universal Action Runtime, CUA §34)
// Ogni provider (Chromium, Obscura) implementa IBrowserLaunch (per avviare CDP) e
// IBrowserActions (per navigate/click/fill/snapshot/evaluate via CDP).
// L'agente vede solo BROWSER.NAVIGATE/CLICK/FILL — il runtime sceglie il provider
// in base a `AI AGENT SET nome BROWSER "obscura"|"chromium"|"auto"`.
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.Browser;

public interface IBrowserProvider
{
    string Name { get; } // "chromium" | "obscura"
    bool IsAvailable { get; }
    string CacheDir { get; }
    string? BinaryPath { get; }
    Task<BrowserLaunchResult> LaunchAsync(int port, string? profileDir = null, CancellationToken ct = default);
}

public sealed record BrowserLaunchResult(bool Success, string? Error, int Port, string? BinaryPath, string? ProfileDir);

/// <summary>Risultato di un'azione del browser (navigate/snapshot/click/fill/evaluate).</summary>
public sealed record BrowserActionResult(bool Success, string? Error, string? Data);

/// <summary>ABI browser actions — implementata da <see cref="CdpBrowserClient"/> (CDP via WS).</summary>
public interface IBrowserActions
{
    Task<BrowserActionResult> NavigateAsync(string url, CancellationToken ct = default);
    Task<BrowserActionResult> SnapshotAsync(CancellationToken ct = default);
    Task<BrowserActionResult> ClickAsync(string selector, CancellationToken ct = default);
    Task<BrowserActionResult> FillAsync(string selector, string value, CancellationToken ct = default);
    Task<BrowserActionResult> EvaluateAsync(string expression, CancellationToken ct = default);
    Task<BrowserActionResult> ApplyStealthAsync(CancellationToken ct = default);
    Task<BrowserActionResult> AccessibilityTreeAsync(CancellationToken ct = default);
    Task<BrowserActionResult> ScreenshotAsync(string? path = null, CancellationToken ct = default);
    Task<BrowserActionResult> GetCookiesAsync(CancellationToken ct = default);
    Task<BrowserActionResult> SetCookieAsync(string name, string value, string? url = null, CancellationToken ct = default);
    Task<BrowserActionResult> ClearCookiesAsync(CancellationToken ct = default);
    Task<BrowserActionResult> SetHeaderAsync(string name, string value, CancellationToken ct = default);
    Task<BrowserActionResult> SetUserAgentAsync(string userAgent, CancellationToken ct = default);
    Task<BrowserActionResult> HumanClickAsync(string selector, CancellationToken ct = default);
    Task<BrowserActionResult> HumanFillAsync(string selector, string value, CancellationToken ct = default);
    Task<BrowserActionResult> HumanScrollAsync(int dy, CancellationToken ct = default);
    /// <summary>G1: OBSERVE — unico bundle url+title+interactables+console+tabs+downloads.
    /// preset="text" salta screenshot/OCR.</summary>
    Task<BrowserActionResult> ObserveAsync(string? preset = null, CancellationToken ct = default);
    /// <summary>G3: FIND testuale — trova elementi per testo senza snapshot full.</summary>
    Task<BrowserActionResult> FindAsync(string text, bool regex = false, CancellationToken ct = default);
    /// <summary>G5: NETLOG — richieste/re5sponses network bufferizzate (JSON). clear=true svuota.</summary>
    Task<BrowserActionResult> NetLogAsync(bool clear = false, CancellationToken ct = default);
    /// <summary>G5: CONSOLE — messaggi console bufferizzati (JSON). clear=true svuota.</summary>
    Task<BrowserActionResult> ConsoleAsync(bool clear = false, CancellationToken ct = default);
    /// <summary>G2/G5: installa hook console che accumulano errori in window.__gmbConsole.</summary>
    Task<BrowserActionResult> EnableCaptureAsync(CancellationToken ct = default);
}
