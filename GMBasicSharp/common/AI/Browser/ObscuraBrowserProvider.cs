// Browser/ObscuraBrowserProvider.cs — provider Obscura (Rust+V8, ~70MB/~30MB)
// Alternativa leggera a Chromium per agenti (CUA §10, ASTRA §1). Stessa ABI
// di ChromiumProvider, ma binario "obscura" headless con CDP su --remote-debugging-port.
using Basic.AI.ModelDownload;
using CommonLib;

namespace Basic.AI.Browser;

public sealed class ObscuraBrowserProvider : IBrowserProvider
{
    public string Name => "obscura";
    public bool IsAvailable => FetchObscuraLib.IsCached(ObscuraCache.ResolveRid());
    public string CacheDir => ObscuraCache.Dir(ObscuraCache.ResolveRid());
    public string? BinaryPath
    {
        get
        {
            var dir = CacheDir;
            if (!Directory.Exists(dir)) return null;
            var files = Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories)
                .FirstOrDefault(f => Path.GetFileName(f).Equals("obscura", StringComparison.OrdinalIgnoreCase) || f.EndsWith("obscura.exe", StringComparison.OrdinalIgnoreCase));
            return files;
        }
    }

    public Task<BrowserLaunchResult> LaunchAsync(int port, string? profileDir = null, CancellationToken ct = default)
    {
        var bin = BinaryPath;
        if (bin == null || !File.Exists(bin))
            return Task.FromResult(new BrowserLaunchResult(false, $"Obscura non in cache: {CacheDir} (gmb setup OBSCURA)", port, null, profileDir));
        // Obscura espone CDP su --remote-debugging-port, compatibile con connectOverCDP()
        return Task.FromResult(new BrowserLaunchResult(true, null, port, bin, profileDir));
    }
}
