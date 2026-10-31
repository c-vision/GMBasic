// AI/ModelDownload/FetchTorchLib.cs — download/verifica delle librerie runtime ML
// (host libgmb_torch + libtorch) in libs-runtime/ml/<rid>/ (GmbiPaths.MlDir), da cui il
// runtime C (gmb_torch.c) e il resolver P/Invoke (MlNativeEngine) le scoprono da soli.
// Manifest: Resources/ml-manifest.json (versione, per RID i file con sha256).
// Fonti: "build" (host, da Resources/libs/<rid>), "local" (libtorch da un torch locale:
// GMB_TORCH_LOCAL o venv ~/ai), "url" (download ufficiale, da popolare). SHA256 sempre
// verificato; nessun file corrotto viene mai usato.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading;
using CommonLib;
using System.Threading.Tasks;

namespace Basic.AI.ModelDownload;

using Basic.ModelDownload;

public sealed record MlSetupResult(bool Success, string? Error, string CacheDir, long BytesTotal, IReadOnlyList<string> Files);

public static class MlCache
{
    /// <summary>Cache componente TORCH (scoperta da loader C# e C): GmbiPaths.MlDir(rid)
    /// = &lt;root&gt;/libs-runtime/ml/&lt;rid&gt;/ (default, configurabile in system.settings.json).</summary>
    public static string Dir(string rid) => GmbiPaths.MlDir(rid);

    /// <summary>RID come in gmb_torch.c / resources: osx-arm64, osx-x64, linux-x64, linux-arm64,
    /// win-x64, win-arm64 (stessi RID del manifest librerie).</summary>
    public static string ResolveRid()
    {
        if (OperatingSystem.IsMacOS()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        if (OperatingSystem.IsLinux()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
        if (OperatingSystem.IsWindows()) return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "win-arm64" : "win-x64";
        return "unknown";
    }
}

public static class FetchTorchLib
{
    private sealed class ManifestFile
    {
        public string? Name { get; set; }
        public string? Sha256 { get; set; }
        public long Size { get; set; }
        public string? Source { get; set; }
        /// <summary>source="url": download diretto di un nostro binario (es. wrapper libgmb_torch
        /// pubblicato su GMBasicModules, che non esiste nell'archivio pytorch).</summary>
        public string? Url { get; set; }
    }
    /// <summary>Archivio ufficiale (libtorch da download.pytorch.org) da cui estrarre i file
    /// con source="archive". LibDir = cartella interna (default "libtorch/lib").</summary>
    private sealed class ArchiveEntry
    {
        public string? Url { get; set; }
        public string? Sha256 { get; set; }
        public string? LibDir { get; set; }
        /// <summary>Estrae TUTTA libDir nella cache (per RID i cui nomi-file non sono enumerati).</summary>
        public bool ExtractAll { get; set; }
    }
    private sealed class RuntimeEntry
    {
        public string? TorchVersion { get; set; }
        public string? Device { get; set; }
        public ArchiveEntry? Archive { get; set; }
        public List<ManifestFile>? Files { get; set; }
    }
    private sealed class Manifest
    {
        public string? Version { get; set; }
        public Dictionary<string, RuntimeEntry>? Runtimes { get; set; }
    }

    public static string DefaultManifestPath()
    {
        // In dev gira dalla root del repo; in distribuzione Resources è copiato nell'output.
        var here = AppContext.BaseDirectory;
        var root = CommonLib.GmbiPaths.RepoRoot;          // monorepo-aware (GMBASIC.sln / GMBasicSharp.slnx)
        foreach (var cand in new[]
        {
            Path.Combine(CommonLib.GmbiPaths.SharedConfigDir, "ml-manifest.json"),  // CONFIG comune
            Path.Combine(here, "config", "ml-manifest.json"),
            Path.Combine(here, "Resources", "ml-manifest.json"),                    // legacy
        })
            if (File.Exists(cand)) return Path.GetFullPath(cand);
        return Path.Combine(here, "config", "ml-manifest.json");
    }

    private static string FindRepoRoot()
    {
        var dir = AppContext.BaseDirectory;
        for (var i = 0; i < 8 && dir != null; i++, dir = Path.GetDirectoryName(dir))
            if (File.Exists(Path.Combine(dir, "GMBASIC.sln"))) return dir;
        return AppContext.BaseDirectory;
    }

    /// <summary>Verifica che i file del manifest per il RID siano nella cache e validi (sha256).</summary>
    public static bool IsCached(string rid)
    {
        var manifest = LoadManifest();
        if (manifest?.Runtimes == null || !manifest.Runtimes.TryGetValue(rid, out var rt) || rt.Files == null) return false;
        var dir = MlCache.Dir(rid);
        if (rt.Archive?.ExtractAll == true && !File.Exists(Path.Combine(dir, TorchMarker))) return false;
        return rt.Files.All(f => IsValid(Path.Combine(dir, f.Name ?? ""), f.Sha256, f.Size));
    }

    private static bool IsValid(string path, string? sha, long size)
    {
        if (!File.Exists(path)) return false;
        try
        {
            if (size > 0 && new FileInfo(path).Length != size) return false;
            // Fase 3: verifica sha256 unificata in CommonLib.ArchiveFetcher.
            return CommonLib.ArchiveFetcher.VerifySha256(path, sha);
        }
        catch { return false; }
    }

    public static async Task<MlSetupResult> EnsureAsync(
        string? manifestPath, Action<string>? log, Action<ModelDownloader.DownloadProgress>? progress, CancellationToken ct)
    {
        manifestPath ??= DefaultManifestPath();
        var rid = MlCache.ResolveRid();
        var manifest = LoadManifest(manifestPath);
        if (manifest?.Runtimes == null || !manifest.Runtimes.TryGetValue(rid, out var rt) || rt.Files == null)
            return new MlSetupResult(false, $"RID '{rid}' non presente nel manifest '{manifestPath}'", MlCache.Dir(rid), 0, []);

        var dir = MlCache.Dir(rid);
        Directory.CreateDirectory(dir);
        var localTorch = ResolveLocalTorch();
        var copied = new List<string>();
        long bytes = 0;
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_torch_" + Guid.NewGuid().ToString("N"));
        try
        {
            // libtorch ufficiale (source="archive"): scarica UNA volta ed estrae i file richiesti.
            var archiveFiles = rt.Files.Where(f => f.Source == "archive").ToList();
            var needArchive = archiveFiles.Any(f => !IsValid(Path.Combine(dir, f.Name ?? ""), f.Sha256, f.Size))
                || (rt.Archive?.ExtractAll == true && !File.Exists(Path.Combine(dir, TorchMarker)));
            string? archiveLibDir = null;
            if (needArchive)
            {
                if (string.IsNullOrEmpty(rt.Archive?.Url))
                    return new MlSetupResult(false,
                        $"{rid}: file con source='archive' ma nessun archive.url nel manifest", dir, bytes, copied);
                Directory.CreateDirectory(tmp);
                log?.Invoke($"[ml] {rid}: scarico libtorch ufficiale {rt.Archive!.Url}");
                using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
                var (ok, err, _) = await ArchiveFetcher
                    .FetchArchiveAsync(http, rt.Archive.Url, tmp, rt.Archive.Sha256, null, ct).ConfigureAwait(false);
                if (!ok) return new MlSetupResult(false, $"libtorch ufficiale: {err}", dir, bytes, copied);
                var libRel = (rt.Archive.LibDir ?? "libtorch/lib").Replace('/', Path.DirectorySeparatorChar);
                archiveLibDir = Directory.Exists(Path.Combine(tmp, libRel)) ? Path.Combine(tmp, libRel) : tmp;
                if (rt.Archive.ExtractAll)
                {
                    foreach (var f in Directory.EnumerateFiles(archiveLibDir))
                    {
                        var t = Path.Combine(dir, Path.GetFileName(f));
                        File.Copy(f, t, overwrite: true);
                        copied.Add(Path.GetFileName(f)); bytes += new FileInfo(f).Length;
                    }
                    File.WriteAllText(Path.Combine(dir, TorchMarker), rt.Archive.Url);
                }
            }

            foreach (var f in rt.Files)
            {
                ct.ThrowIfCancellationRequested();
                var target = Path.Combine(dir, f.Name ?? "");
                if (IsValid(target, f.Sha256, f.Size)) { bytes += f.Size; copied.Add(f.Name!); continue; }

                string? src;
                if (f.Source == "url")
                {
                    if (string.IsNullOrEmpty(f.Url))
                        return new MlSetupResult(false, $"{f.Name}: source='url' ma url assente nel manifest", dir, bytes, copied);
                    Directory.CreateDirectory(tmp);
                    var dl = Path.Combine(tmp, "dl_" + Guid.NewGuid().ToString("N"));
                    log?.Invoke($"[ml] {f.Name} ({FormatSize(f.Size)}) ← {f.Url}");
                    using (var http = new HttpClient { Timeout = TimeSpan.FromMinutes(15) })
                    using (var resp = await http.GetAsync(f.Url, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false))
                    {
                        resp.EnsureSuccessStatusCode();
                        using (var fs = File.Create(dl))
                            await resp.Content.CopyToAsync(fs, ct).ConfigureAwait(false);
                    }
                    src = dl;
                }
                else if (f.Source == "archive")
                {
                    src = archiveLibDir != null && File.Exists(Path.Combine(archiveLibDir, f.Name ?? ""))
                        ? Path.Combine(archiveLibDir, f.Name ?? "") : null;
                }
                else
                {
                    src = ResolveSource(f, localTorch);
                }
                if (src == null)
                    return new MlSetupResult(false,
                        $"{f.Name}: fonte non disponibile (archive ufficiale / build host / local torch)", dir, bytes, copied);

                log?.Invoke($"[ml] {f.Name} ({FormatSize(f.Size)}) ← {src}");
                progress?.Invoke(new ModelDownloader.DownloadProgress(0, f.Size, 0, f.Name));
                File.Copy(src, target, overwrite: true);
                progress?.Invoke(new ModelDownloader.DownloadProgress(f.Size, f.Size, 100, f.Name));

                if (!IsValid(target, f.Sha256, f.Size))
                    return new MlSetupResult(false, $"{f.Name}: sha256 non combacia dopo la copia", dir, bytes, copied);
                bytes += f.Size;
                copied.Add(f.Name!);
            }
            log?.Invoke($"[ml] cache pronta: {dir} ({FormatSize(bytes)})");
            return new MlSetupResult(true, null, dir, bytes, copied);
        }
        catch (OperationCanceledException) { return new MlSetupResult(false, "interrotto", dir, bytes, copied); }
        catch (Exception ex) { return new MlSetupResult(false, ex.Message, dir, bytes, copied); }
        finally { try { if (Directory.Exists(tmp)) Directory.Delete(tmp, true); } catch { } }
    }

    /// <summary>Fonte di un file: build → Resources/libs/&lt;rid&gt;/; local → GMB_TORCH_LOCAL o venv ~/ai.
    /// (source="archive" è gestito in EnsureAsync.)</summary>
    private static string? ResolveSource(ManifestFile f, string? localTorch)
    {
        if (f.Source == "build")
        {
            var rid = MlCache.ResolveRid();
            foreach (var baseDir in new[] { "libs", "lib" })   // Resources/libs/<rid>/ (runtime) o Resources/lib/<rid>/
            {
                var cand = Path.Combine(FindRepoRoot(), "Resources", baseDir, rid, f.Name ?? "");
                if (File.Exists(cand)) return cand;
            }
            // Unica posizione reale delle librerie ora: la cache condivisa ~/.gmbasic/libs/<rid>/
            // (il wrapper libgmb_torch vi arriva con il gruppo `runtime`; Resources/libs non esiste più).
            var cached = Path.Combine(CommonLib.GmbiPaths.LibsDir, rid, f.Name ?? "");
            if (File.Exists(cached)) return cached;
            return null;
        }
        if (f.Source == "local" && localTorch != null)
        {
            var cand = Path.Combine(localTorch, f.Name ?? "");
            return File.Exists(cand) ? cand : null;
        }
        return null;
    }

    private const string TorchMarker = ".gmb-torch-ok";

    private static string? ResolveLocalTorch()
    {
        var env = Basic.Providers.GmbEnv.Get("GMB_TORCH_LOCAL");
        if (!string.IsNullOrEmpty(env) && Directory.Exists(env)) return env;
        // venv di sviluppo: ~/ai/env*/lib/python*/site-packages/torch/lib (ricerca ricorsiva)
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var ai = Path.Combine(home, "ai");
        if (!Directory.Exists(ai)) return null;
        foreach (var envDir in Directory.EnumerateDirectories(ai, "env*"))
        {
            foreach (var torchDir in Directory.EnumerateDirectories(envDir, "torch", SearchOption.AllDirectories))
            {
                var lib = Path.Combine(torchDir, "lib");
                if (File.Exists(Path.Combine(lib, "libtorch_cpu.dylib")) || File.Exists(Path.Combine(lib, "libtorch.so")))
                    return lib;
            }
        }
        return null;
    }

    private static readonly JsonSerializerOptions _jsonOpts = new() { PropertyNameCaseInsensitive = true };

    private static Manifest? LoadManifest(string? path = null)
    {
        path ??= DefaultManifestPath();
        if (!File.Exists(path)) return null;
        try { return JsonSerializer.Deserialize<Manifest>(File.ReadAllText(path), _jsonOpts); }
        catch { return null; }
    }

    private static string FormatSize(long b) => b >= 1 << 20 ? $"{b / (double)(1 << 20):F0} MB" : $"{b} B";
}
