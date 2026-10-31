// AI/ModelDownload/FetchObscuraLib.cs — download/verifica del runtime Obscura
// (Rust+V8, headless, CDP) in libs-runtime/obscura/<rid>/ (GmbiPaths.ObscuraDir),
// da cui il BrowserProvider (ChromiumProvider | ObscuraProvider) lo scopre.
// Clone parziale di FetchChromiumLib.cs — stesso pattern: manifesto dedicato
// Resources/obscura-manifest.json (per RID: URL + sha256 + size), SHA256
// sempre verificato, nessun file corrotto viene mai usato.

using System;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.ModelDownload;

using CommonLib;

public sealed record ObscuraSetupResult(bool Success, string? Error, string CacheDir, long BytesTotal, IReadOnlyList<string> Files);

public static class ObscuraCache
{
    /// <summary>Cache componente BROWSER/OBSCURA: GmbiPaths.ObscuraDir(rid) =
    /// &lt;root&gt;/libs-runtime/obscura/&lt;rid&gt;/ (default, configurabile in system.settings.json).</summary>
    public static string Dir(string rid) => GmbiPaths.ObscuraDir(rid);

    /// <summary>RID come in MlCache / chromium: osx-arm64, osx-x64, linux-x64, linux-arm64, win-x64.</summary>
    public static string ResolveRid()
    {
        if (OperatingSystem.IsMacOS()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        if (OperatingSystem.IsLinux()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
        if (OperatingSystem.IsWindows()) return "win-x64";
        return "unknown";
    }
}

public static class FetchObscuraLib
{
    private sealed class ObscuraEntry
    {
        public string? Version { get; set; }
        public string? ArchiveUrl { get; set; }
        public string? Sha256 { get; set; }
        public long Size { get; set; }
        public string? BinName { get; set; }
    }

    private sealed class Manifest
    {
        public string? Version { get; set; }
        public Dictionary<string, ObscuraEntry>? Runtimes { get; set; }
    }

    public static string DefaultManifestPath()
    {
        var here = AppContext.BaseDirectory;
        var root = CommonLib.GmbiPaths.RepoRoot;              // monorepo-aware
        foreach (var cand in new[]
        {
            Path.Combine(CommonLib.GmbiPaths.SharedConfigDir, "obscura-manifest.json"),  // CONFIG comune
            Path.Combine(here, "config", "obscura-manifest.json"),
            Path.Combine(here, "Resources", "obscura-manifest.json"),                    // legacy
        })
            if (File.Exists(cand)) return Path.GetFullPath(cand);
        return Path.Combine(here, "Resources", "obscura-manifest.json");
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    public static bool IsCached(string rid)
    {
        var dir = ObscuraCache.Dir(rid);
        if (!Directory.Exists(dir)) return false;
        var manifest = LoadManifest(DefaultManifestPath());
        var entry = manifest?.Runtimes?[rid];
        if (entry == null) return false;
        return HasBinary(dir, entry);
    }

    private static bool HasBinary(string dir, ObscuraEntry entry)
    {
        if (Directory.Exists(dir) && Directory.EnumerateFileSystemEntries(dir).Any()) return true;
        return false;
    }

    public static async Task<ObscuraSetupResult> EnsureAsync(
        string? manifestPath, Action<string>? log, Action<long, long>? progress, CancellationToken ct)
    {
        manifestPath ??= DefaultManifestPath();
        var rid = ObscuraCache.ResolveRid();
        var manifest = LoadManifest(manifestPath);
        if (manifest?.Runtimes == null || !manifest.Runtimes.TryGetValue(rid, out var entry) || string.IsNullOrEmpty(entry.ArchiveUrl))
            return new ObscuraSetupResult(false, $"RID '{rid}' non presente nel manifest '{manifestPath}'", ObscuraCache.Dir(rid), 0, []);

        var dir = ObscuraCache.Dir(rid);
        Directory.CreateDirectory(dir);

        // Fase 3: download + sha256 + estrazione unificati in CommonLib.ArchiveFetcher.
        log?.Invoke($"[obscura] RID={rid} — scarico Obscura (v{entry.Version}, {FormatSize(entry.Size)}) da {entry.ArchiveUrl}");
        using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(20) };
        var (ok, err, bytes) = await CommonLib.ArchiveFetcher
            .FetchArchiveAsync(http, entry.ArchiveUrl!, dir, entry.Sha256, progress, ct).ConfigureAwait(false);
        if (!ok)
            return new ObscuraSetupResult(false, err, dir, entry.Size, []);

        File.WriteAllText(Path.Combine(dir, ".gmb-obscura-ok"), entry.Version ?? "");
        log?.Invoke($"[obscura] cache pronta: {dir} (v{entry.Version})");
        return new ObscuraSetupResult(true, null, dir, bytes, [Path.GetFileName(new Uri(entry.ArchiveUrl!).AbsolutePath)]);
    }

    private static Manifest? LoadManifest(string path)
    {
        try
        {
            var opts = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
            return JsonSerializer.Deserialize<Manifest>(File.ReadAllText(path), opts);
        }
        catch { return null; }
    }

    private static string FormatSize(long bytes) =>
        bytes >= 1_000_000_000 ? $"{bytes / 1_000_000_000.0:F1} GB"
        : bytes >= 1_000_000 ? $"{bytes / 1_000_000.0:F1} MB"
        : bytes >= 1_000 ? $"{bytes / 1_000.0:F1} KB"
        : $"{bytes} B";
}
