// AI/ModelDownload/FetchLibsLib.cs — [DEPRECATO 2026-10-28] download del bundle delle librerie
// native per-OS (Resources/libs/<os>/) da c-vision/GMBasicModules, con lo schema VECCHIO del
// manifest (`runtimes.<rid>.archiveUrl`).
//
// SOSTITUITO da CommonLib.LibsResolver (§ grupos support/engine/core/torch, cache ~/.gmbasic/libs/<rid>).
// Resta solo per compatibilità di build: NON usarlo per nuovo codice. Il componente LIBS di
// SetupComponents e CoreLibsBootstrap usano il resolver.
//
// --- (storico) ---
// download/verifica del bundle delle librerie native
// per-OS (Resources/libs/<os>/: SDL, pdf_oxide, vision, torch host, ...) da
// c-vision/GMBasicModules. Manifest Resources/libs-manifest.json. Estrazione diretta in
// Resources/libs/<rid>/ — la cartella che i loader nativi (gmb_torch.c, gmb_doc_pdf.c,
// gmb_doc_ocr.c) scoprono risalendo dalla cwd.

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

public sealed record LibsSetupResult(bool Success, string? Error, string CacheDir, long BytesTotal, IReadOnlyList<string> Files);

public static class LibsCache
{
    /// <summary>Cartella di destinazione: Resources/libs/<rid>/ (risalendo dal binario alla root del repo).</summary>
    public static string Dir(string rid)
    {
        var root = FindRepoRoot();
        return root != null ? Path.Combine(root, "Resources", "libs", rid) : Path.Combine(Directory.GetCurrentDirectory(), "Resources", "libs", rid);
    }

    private static string? FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }

    public static string ResolveRid()
    {
        if (OperatingSystem.IsMacOS()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        if (OperatingSystem.IsLinux()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
        if (OperatingSystem.IsWindows()) return "win-x64";
        return "unknown";
    }
}

public static class FetchLibsLib
{
    private sealed class LibsEntry
    {
        public string? Version { get; set; }
        public string? ArchiveUrl { get; set; }
        public string? Sha256 { get; set; }
        public long Size { get; set; }
    }

    private sealed class Manifest
    {
        public string? Version { get; set; }
        public Dictionary<string, LibsEntry>? Runtimes { get; set; }
    }

    public static string DefaultManifestPath()
    {
        var root = LibsCacheDirRoot();
        return root != null ? Path.Combine(root, "Resources", "libs-manifest.json")
                            : Path.Combine(Directory.GetCurrentDirectory(), "Resources", "libs-manifest.json");
    }

    private static string? LibsCacheDirRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }

    public static bool IsCached(string rid)
    {
        var dir = LibsCache.Dir(rid);
        return Directory.Exists(dir) && Directory.EnumerateFileSystemEntries(dir).Any();
    }

    public static async Task<LibsSetupResult> EnsureAsync(
        string? manifestPath, Action<string>? log, Action<long, long>? progress, CancellationToken ct)
    {
        manifestPath ??= DefaultManifestPath();
        var rid = LibsCache.ResolveRid();
        var manifest = LoadManifest(manifestPath);
        if (manifest?.Runtimes == null || !manifest.Runtimes.TryGetValue(rid, out var entry) || string.IsNullOrEmpty(entry.ArchiveUrl))
            return new LibsSetupResult(false, $"RID '{rid}' non presente nel manifest '{manifestPath}'", LibsCache.Dir(rid), 0, []);

        var dir = LibsCache.Dir(rid);
        Directory.CreateDirectory(dir);

        // Fase 3: download + sha256 + estrazione unificati in CommonLib.ArchiveFetcher.
        log?.Invoke($"[libs] RID={rid} — scarico librerie native (v{entry.Version}, {FormatSize(entry.Size)}) da {entry.ArchiveUrl}");
        using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
        var (ok, err, bytes) = await CommonLib.ArchiveFetcher
            .FetchArchiveAsync(http, entry.ArchiveUrl!, dir, entry.Sha256, progress, ct).ConfigureAwait(false);
        if (!ok)
            return new LibsSetupResult(false, err, dir, entry.Size, []);

        File.WriteAllText(Path.Combine(dir, ".gmb-libs-ok"), entry.Version ?? "");
        log?.Invoke($"[libs] pronti: {dir} (v{entry.Version})");
        return new LibsSetupResult(true, null, dir, bytes, [Path.GetFileName(new Uri(entry.ArchiveUrl!).AbsolutePath)]);
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
