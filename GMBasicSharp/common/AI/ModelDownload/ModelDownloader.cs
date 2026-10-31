// AI/ModelDownload/ModelDownloader.cs — curl-based model downloader for the HF/MLX REPL commands
//
// Ports the parts of Rapid-MLX-NextGen's vllm_mlx/tools/model_downloader.py that proved stable
// in production: curl as the transport (resumable via -C -, its own internal retry loop via
// --retry), a stall watchdog that kills and restarts a transfer if the destination file stops
// growing, and post-download integrity verification against the HF repo manifest (SHA256 for
// LFS-tracked files, size otherwise). Deliberately does NOT invent a second transport: --no-curl
// is rejected with a clear error rather than silently ignored, and --workers/--no-xet are
// documented as either genuinely used (parallel file downloads) or honest no-ops.
using System.Security.Cryptography;

namespace Basic.ModelDownload;

public static class ModelDownloader
{
    private static readonly TimeSpan StallPollInterval = TimeSpan.FromSeconds(2);

    /// <summary>Two folders HF/MLX fall back to when AIPATH has no entries, used by the
    /// LLM command as its own fallback so 'where would a fresh HF/MLX download land' and
    /// 'where does LLM look' never drift apart.</summary>
    public static IReadOnlyList<string> DefaultAiPathFallbacks() =>
    [
        Path.Combine(ResolveRepoRoot(), "samples", "ai", "models", "gguf"),
        Path.Combine(ResolveRepoRoot(), "samples", "ai", "models", "mlx"),
    ];

    /// <summary>Progresso di un download (usato da PROVIDER.REGISTER con hf: e dal REPL).
    /// BytesDone è la somma dei byte scaricati finora (polling della cartella di destinazione),
    /// BytesTotal la somma delle dimensioni dei file dal manifest HF. Percent è 0..100.
    /// CurrentFile indica il file in corso di trasferimento.</summary>
    public sealed record DownloadProgress(long BytesDone, long BytesTotal, double Percent, string? CurrentFile);

    public static async Task<ModelDownloadResult> DownloadAsync(
        ModelDownloadOptions opts, HttpClient http, Action<string>? log, CancellationToken ct)
        => await DownloadAsync(opts, http, log, progress: null, ct).ConfigureAwait(false);

    public static async Task<ModelDownloadResult> DownloadAsync(
        ModelDownloadOptions opts, HttpClient http, Action<string>? log,
        Action<DownloadProgress>? progress, CancellationToken ct)
    {
        void Log(string msg) { if (!opts.Quiet) log?.Invoke(msg); }

        if (opts.NoCurl)
            return Fail("Solo il trasporto curl e' implementato in questa porta nativa (nessun fallback huggingface_hub); --no-curl non e' supportato.");

        if (FindCurlExecutable() is not { } curlPath)
            return Fail("Eseguibile 'curl' non trovato nel PATH. E' richiesto per i comandi HF/MLX.");

        if (opts.NoXet)
            Log("[info] --no-xet e' irrilevante con il trasporto curl (scarica sempre via HTTP classico dall'endpoint 'resolve', non usa mai Xet).");

        string? token = opts.NoToken ? null : ResolveHfToken();

        HfRepoManifest manifest;
        try { manifest = await HfRepoManifestClient.FetchAsync(http, opts.RepoId, token, ct).ConfigureAwait(false); }
        catch (Exception ex) { return Fail($"Impossibile leggere il manifest del repo '{opts.RepoId}': {ex.Message}"); }

        if (manifest.Files.Count == 0)
            return Fail($"Il repo '{opts.RepoId}' non contiene file (o non esiste).");

        List<HfFileEntry> toDownload;
        string destDir;
        if (opts.Kind == ModelDownloadKind.Mlx)
        {
            toDownload = manifest.Files.ToList();
            destDir = Path.Combine(ResolveDestRoot(opts, "mlx"), LastSegment(opts.RepoId));
        }
        else
        {
            toDownload = SelectHfFiles(manifest, opts.File, out var selectionError);
            if (selectionError is not null) return Fail(selectionError);
            destDir = ResolveDestRoot(opts, "gguf");
        }

        Directory.CreateDirectory(destDir);
        Log($"[dest] {destDir}");

        var results = new string[toDownload.Count];
        var errors = new string?[toDownload.Count];
        var totalBytes = toDownload.Sum(f => f.Size);
        var doneBytes = 0L;
        using var gate = new SemaphoreSlim(Math.Max(1, opts.Workers));
        var tasks = toDownload.Select(async (file, i) =>
        {
            await gate.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                var destPath = Path.Combine(destDir, file.RFilename.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(destPath)!);
                var url = HfRepoManifestClient.ResolveUrl(opts.RepoId, file.RFilename);
                var ok = await DownloadFileWithStallWatchdogAsync(curlPath, url, destPath, file, opts, token, Log,
                    fileBytes =>
                    {
                        if (progress is null) return;
                        var globalDone = Interlocked.Read(ref doneBytes) + fileBytes;
                        progress(new DownloadProgress(globalDone, totalBytes,
                            totalBytes > 0 ? (double)globalDone / totalBytes * 100.0 : 0, file.RFilename));
                    }, ct).ConfigureAwait(false);
                if (ok) results[i] = destPath; else errors[i] = $"{file.RFilename}: download fallito (stall/errore/integrita').";
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { errors[i] = $"{file.RFilename}: {ex.Message}"; }
            finally
            {
                // File completato: accumula il suo intero peso nei byte fatti.
                var fSize = file.Size;
                Interlocked.Add(ref doneBytes, fSize);
                progress?.Invoke(new DownloadProgress(Interlocked.Read(ref doneBytes), totalBytes,
                    totalBytes > 0 ? (double)Interlocked.Read(ref doneBytes) / totalBytes * 100.0 : 0, file.RFilename));
                gate.Release();
            }
        });
        await Task.WhenAll(tasks).ConfigureAwait(false);

        var failed = errors.Where(e => e is not null).Select(e => e!).ToList();
        if (failed.Count > 0)
            return new ModelDownloadResult(false, destDir, results.Where(r => r is not null).Select(r => r!).ToList(), string.Join("; ", failed));

        Log($"[ok] {results.Length} file scaricati in {destDir}");
        return new ModelDownloadResult(true, destDir, results!, null);
    }

    /// <summary>For HF downloads: picks which manifest files to fetch. Exact match on
    /// <paramref name="filePattern"/> wins; otherwise '*' is treated as a wildcard, otherwise
    /// it's a substring match. With no pattern, auto-selects the repo's single .gguf file, or
    /// fails listing every .gguf candidate if there's more than one (never guesses).</summary>
    private static List<HfFileEntry> SelectHfFiles(HfRepoManifest manifest, string? filePattern, out string? error)
    {
        error = null;
        if (!string.IsNullOrWhiteSpace(filePattern))
        {
            var exact = manifest.Files.Where(f => f.RFilename.Equals(filePattern, StringComparison.OrdinalIgnoreCase)).ToList();
            if (exact.Count > 0) return exact;

            var matched = filePattern.Contains('*')
                ? manifest.Files.Where(f => WildcardMatch(f.RFilename, filePattern)).ToList()
                : manifest.Files.Where(f => f.RFilename.Contains(filePattern, StringComparison.OrdinalIgnoreCase)).ToList();

            if (matched.Count == 0)
            {
                error = $"Nessun file corrisponde a --file '{filePattern}' nel repo. File disponibili:\n" +
                         string.Join('\n', manifest.Files.Select(f => $"  {f.RFilename} ({FormatSize(f.Size)})"));
                return [];
            }
            return matched;
        }

        var ggufFiles = manifest.Files.Where(f => f.RFilename.EndsWith(".gguf", StringComparison.OrdinalIgnoreCase)).ToList();
        if (ggufFiles.Count == 1) return ggufFiles;
        if (ggufFiles.Count == 0)
        {
            error = "Nessun file .gguf trovato nel repo. Specificare --file <nome> per scegliere un file esplicitamente. File disponibili:\n" +
                    string.Join('\n', manifest.Files.Select(f => $"  {f.RFilename} ({FormatSize(f.Size)})"));
            return [];
        }
        error = $"Il repo contiene {ggufFiles.Count} file .gguf: serve --file <nome> per scegliere quale scaricare (evita download accidentali dell'intero repo). Candidati:\n" +
                string.Join('\n', ggufFiles.Select(f => $"  {f.RFilename} ({FormatSize(f.Size)})"));
        return [];
    }

    private static bool WildcardMatch(string text, string pattern)
    {
        var regex = "^" + string.Join(".*", pattern.Split('*').Select(System.Text.RegularExpressions.Regex.Escape)) + "$";
        return System.Text.RegularExpressions.Regex.IsMatch(text, regex, System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    }

    private static async Task<bool> DownloadFileWithStallWatchdogAsync(
        string curlPath, string url, string destPath, HfFileEntry file, ModelDownloadOptions opts,
        string? token, Action<string> log, Action<long>? fileProgress, CancellationToken ct)
    {
        for (var attempt = 0; attempt <= opts.StallRetries; attempt++)
        {
            var outcome = await RunCurlOnceAsync(curlPath, url, destPath, opts, token, log, fileProgress, ct).ConfigureAwait(false);
            if (outcome == CurlOutcome.Success)
            {
                if (VerifyIntegrity(destPath, file, log)) return true;
                log($"[integrity] {Path.GetFileName(destPath)}: verifica fallita, rimuovo e riprovo.");
                TryDelete(destPath);
                continue;
            }
            if (outcome == CurlOutcome.HardFailure) return false; // curl itself gave up (bad URL, 404, auth...) -- retrying won't help
            log($"[stall] {Path.GetFileName(destPath)}: nessuna crescita per {opts.StallTimeoutMinutes} min, tentativo {attempt + 1}/{opts.StallRetries + 1} killato, riprendo con resume.");
        }
        return false;
    }

    private enum CurlOutcome { Success, Stalled, HardFailure }

    private static async Task<CurlOutcome> RunCurlOnceAsync(
        string curlPath, string url, string destPath, ModelDownloadOptions opts, string? token,
        Action<string> log, Action<long>? fileProgress, CancellationToken ct)
    {
        var psi = new System.Diagnostics.ProcessStartInfo
        {
            FileName = curlPath,
            UseShellExecute = false,
            RedirectStandardOutput = false,
            RedirectStandardError = opts.Quiet,
        };
        psi.ArgumentList.Add("-fL");
        psi.ArgumentList.Add("-C"); psi.ArgumentList.Add("-");
        psi.ArgumentList.Add("--retry"); psi.ArgumentList.Add("50");
        psi.ArgumentList.Add("--retry-delay"); psi.ArgumentList.Add("3");
        psi.ArgumentList.Add("--retry-max-time"); psi.ArgumentList.Add("0");
        psi.ArgumentList.Add(opts.Quiet ? "-s" : "--progress-bar");
        if (!string.IsNullOrWhiteSpace(token))
        {
            psi.ArgumentList.Add("-H");
            psi.ArgumentList.Add($"Authorization: Bearer {token}");
        }
        psi.ArgumentList.Add("-o"); psi.ArgumentList.Add(destPath);
        psi.ArgumentList.Add(url);

        using var process = new System.Diagnostics.Process { StartInfo = psi };
        process.Start();

        var lastSize = SafeFileLength(destPath);
        var lastGrowth = DateTime.UtcNow;
        var stallLimit = TimeSpan.FromMinutes(Math.Max(1, opts.StallTimeoutMinutes));
        var killedForStall = false;

        try
        {
            while (!process.HasExited)
            {
                await Task.Delay(StallPollInterval, ct).ConfigureAwait(false);
                var size = SafeFileLength(destPath);
                if (size > lastSize) { lastSize = size; lastGrowth = DateTime.UtcNow; }
                else if (DateTime.UtcNow - lastGrowth > stallLimit)
                {
                    killedForStall = true;
                    try { process.Kill(entireProcessTree: true); } catch { /* best effort */ }
                    break;
                }
                // Progress del file corrente (byte scaricati). Il livello superiore combina
                // questo con i byte già completati per il progress globale.
                fileProgress?.Invoke(size);
            }
        }
        catch (OperationCanceledException)
        {
            try { process.Kill(entireProcessTree: true); } catch { /* best effort */ }
            throw;
        }

        if (!process.HasExited) await process.WaitForExitAsync(ct).ConfigureAwait(false);
        if (killedForStall) return CurlOutcome.Stalled;
        return process.ExitCode == 0 ? CurlOutcome.Success : CurlOutcome.HardFailure;
    }

    private static bool VerifyIntegrity(string path, HfFileEntry file, Action<string> log)
    {
        if (!File.Exists(path)) return false;
        var actualSize = new FileInfo(path).Length;
        if (file.Size > 0 && actualSize != file.Size)
        {
            log($"[integrity] {Path.GetFileName(path)}: dimensione attesa {file.Size}, ottenuta {actualSize}.");
            return false;
        }
        if (string.IsNullOrEmpty(file.Sha256)) return true;

        using var sha = SHA256.Create();
        using var stream = File.OpenRead(path);
        var hash = Convert.ToHexStringLower(sha.ComputeHash(stream));
        if (!hash.Equals(file.Sha256, StringComparison.OrdinalIgnoreCase))
        {
            log($"[integrity] {Path.GetFileName(path)}: sha256 atteso {file.Sha256}, ottenuto {hash}.");
            return false;
        }
        return true;
    }

    private static long SafeFileLength(string path) { try { return new FileInfo(path).Length; } catch { return 0; } }
    private static void TryDelete(string path) { try { File.Delete(path); } catch { /* best effort */ } }

    private static string? ResolveHfToken()
    {
        var env = Environment.GetEnvironmentVariable("HF_TOKEN");
        if (!string.IsNullOrWhiteSpace(env)) return env;
        try
        {
            var tokenFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".cache", "huggingface", "token");
            if (File.Exists(tokenFile))
            {
                var t = File.ReadAllText(tokenFile).Trim();
                if (t.Length > 0) return t;
            }
        }
        catch { /* best effort */ }
        return null;
    }

    private static string? FindCurlExecutable()
    {
        var candidates = OperatingSystem.IsWindows() ? new[] { "curl.exe" } : new[] { "/usr/bin/curl", "/usr/local/bin/curl", "curl" };
        foreach (var c in candidates)
        {
            if (Path.IsPathRooted(c)) { if (File.Exists(c)) return c; continue; }
            var pathVar = Environment.GetEnvironmentVariable("PATH") ?? "";
            foreach (var dir in pathVar.Split(Path.PathSeparator))
            {
                var full = Path.Combine(dir, c);
                if (File.Exists(full)) return full;
            }
        }
        return null;
    }

    private static string ResolveDestRoot(ModelDownloadOptions opts, string modelTypeFolder)
    {
        if (!string.IsNullOrWhiteSpace(opts.Dest)) return PathExpansion.Expand(opts.Dest);
        var modelPaths = Basic.AI.AiConfig.ModelPaths;
        if (modelPaths.Count > 0) return PathExpansion.Expand(modelPaths[0]);
        return Path.Combine(ResolveRepoRoot(), "samples", "ai", "models", modelTypeFolder);
    }

    private static string ResolveRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (var i = 0; i < 10 && dir != null; i++, dir = dir.Parent)
        {
            if (dir.GetFiles("*.sln").Length > 0) return dir.FullName;
        }
        return Directory.GetCurrentDirectory();
    }

    private static string LastSegment(string repoId) => repoId.Contains('/') ? repoId[(repoId.LastIndexOf('/') + 1)..] : repoId;

    private static string FormatSize(long bytes)
    {
        string[] units = ["B", "KB", "MB", "GB", "TB"];
        double size = bytes; var u = 0;
        while (size >= 1024 && u < units.Length - 1) { size /= 1024; u++; }
        return $"{size:0.#} {units[u]}";
    }

    private static ModelDownloadResult Fail(string message) => new(false, "", [], message);
}
