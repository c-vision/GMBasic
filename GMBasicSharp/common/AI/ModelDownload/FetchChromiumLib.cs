// AI/ModelDownload/FetchChromiumLib.cs — download/verifica del runtime Chromium
// (Chrome for Testing) in libs-runtime/chromium/<rid>/ (GmbiPaths.ChromiumDir), da cui il plugin
// browser (plugins/browser/) e launch_chromium.sh la scoprono. NON legato a Torch/ML:
// manifesto dedicato config/chromium-manifest.json (per RID: URL + sha256 + size).
// SHA256 sempre verificato; nessun file corrotto viene mai usato.

using System;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading;
using CommonLib;
using System.Threading.Tasks;

namespace Basic.AI.ModelDownload;

public sealed record ChromiumSetupResult(bool Success, string? Error, string CacheDir, long BytesTotal, IReadOnlyList<string> Files);

public static class ChromiumCache
{
    /// <summary>Cache componente BROWSER: GmbiPaths.ChromiumDir(rid) =
    /// &lt;root&gt;/libs-runtime/chromium/&lt;rid&gt;/ (default, configurabile in system.settings.json).</summary>
    public static string Dir(string rid) => GmbiPaths.ChromiumDir(rid);

    /// <summary>RID come in MlCache / resources: osx-arm64, osx-x64, linux-x64, linux-arm64, win-x64.</summary>
    public static string ResolveRid()
    {
        if (OperatingSystem.IsMacOS()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        if (OperatingSystem.IsLinux()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
        if (OperatingSystem.IsWindows()) return "win-x64";
        return "unknown";
    }
}

public static class FetchChromiumLib
{
    private sealed class ChromiumEntry
    {
        public string? Version { get; set; }
        public string? ArchiveUrl { get; set; }
        public string? Sha256 { get; set; }
        public long Size { get; set; }
        // Nome della cartella/binario dentro l'archivio (es. 'Chromium.app' su macOS).
        public string? BinName { get; set; }
    }

    private sealed class Manifest
    {
        public string? Version { get; set; }
        public Dictionary<string, ChromiumEntry>? Runtimes { get; set; }
    }

    public static string DefaultManifestPath()
    {
        var here = AppContext.BaseDirectory;
        var root = CommonLib.GmbiPaths.RepoRoot;              // monorepo-aware
        foreach (var cand in new[]
        {
            Path.Combine(CommonLib.GmbiPaths.SharedConfigDir, "chromium-manifest.json"), // CONFIG comune
            Path.Combine(here, "config", "chromium-manifest.json"),                      // distribuzione
            Path.Combine(here, "Resources", "chromium-manifest.json"),                   // legacy
        })
            if (File.Exists(cand)) return Path.GetFullPath(cand);
        return Path.Combine(here, "config", "chromium-manifest.json");
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
        var dir = ChromiumCache.Dir(rid);
        if (!Directory.Exists(dir)) return false;
        var manifest = LoadManifest(DefaultManifestPath());
        var entry = manifest?.Runtimes?[rid];
        if (entry == null) return false;
        // Considera cached se esiste un binario eseguibile (o l'app) nella cache.
        return HasBinary(dir, entry);
    }

    private static bool HasBinary(string dir, ChromiumEntry entry)
    {
        if (Directory.Exists(dir) && Directory.EnumerateFileSystemEntries(dir).Any()) return true;
        return false;
    }

    public static async Task<ChromiumSetupResult> EnsureAsync(
        string? manifestPath, Action<string>? log, Action<long, long>? progress, CancellationToken ct)
    {
        manifestPath ??= DefaultManifestPath();
        var rid = ChromiumCache.ResolveRid();
        var manifest = LoadManifest(manifestPath);
        if (manifest?.Runtimes == null || !manifest.Runtimes.TryGetValue(rid, out var entry) || string.IsNullOrEmpty(entry.ArchiveUrl))
            return new ChromiumSetupResult(false, $"RID '{rid}' non presente nel manifest '{manifestPath}'", ChromiumCache.Dir(rid), 0, []);

        var dir = ChromiumCache.Dir(rid);
        Directory.CreateDirectory(dir);

        // Fase 3: download + sha256 + estrazione unificati in CommonLib.ArchiveFetcher.
        log?.Invoke($"[browser] RID={rid} — scarico Chromium (v{entry.Version}, {FormatSize(entry.Size)}) da {entry.ArchiveUrl}");
        using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(20) };
        var (ok, err, bytes) = await CommonLib.ArchiveFetcher
            .FetchArchiveAsync(http, entry.ArchiveUrl!, dir, entry.Sha256, progress, ct).ConfigureAwait(false);
        if (!ok)
            return new ChromiumSetupResult(false, err, dir, entry.Size, []);

        File.WriteAllText(Path.Combine(dir, ".gmb-chromium-ok"), entry.Version ?? "");
        log?.Invoke($"[browser] cache pronta: {dir} (v{entry.Version})");
        return new ChromiumSetupResult(true, null, dir, bytes, [Path.GetFileName(new Uri(entry.ArchiveUrl!).AbsolutePath)]);
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
