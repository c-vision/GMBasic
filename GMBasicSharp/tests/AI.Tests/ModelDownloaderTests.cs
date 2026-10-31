// FILE: AI.Tests.ModelDownloaderTests
using Basic.ModelDownload;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class ModelDownloaderTests
{
    /// <summary>Opt-in, real-network test against a genuinely tiny (1.1MB) public GGUF repo
    /// (llama.cpp's own stories260K test model) -- exercises the real curl subprocess, manifest
    /// fetch, and sha256 integrity verification end to end. Same graceful-skip-via-env-var
    /// convention as <see cref="MlxProviderTests"/>/<see cref="LlamaCppProviderTests"/>; not run
    /// by default so `dotnet test` stays hermetic.</summary>
    [Fact(DisplayName = "HF download: tiny single-file GGUF repo, real network + real curl + sha256 verification")]
    public async Task DownloadAsync_TinyHfRepo_DownloadsAndVerifies()
    {
        if (Environment.GetEnvironmentVariable("GMBASIC_MODEL_DOWNLOAD_LIVE_TEST") != "1")
            return; // ponytail: graceful skip -- opt-in only, hits the real network

        var tempDir = Path.Combine(Path.GetTempPath(), "gmbasic-hf-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            using var http = new HttpClient();
            var opts = new ModelDownloadOptions
            {
                RepoId = "ggml-org/test-model-stories260K",
                Kind = ModelDownloadKind.Hf,
                Dest = tempDir,
                StallTimeoutMinutes = 2,
                StallRetries = 1,
            };

            var result = await ModelDownloader.DownloadAsync(opts, http, msg => Console.WriteLine(msg), CancellationToken.None);

            result.Success.Should().BeTrue(result.Error ?? "");
            result.Files.Should().HaveCount(1);
            File.Exists(result.Files[0]).Should().BeTrue();
            new FileInfo(result.Files[0]).Length.Should().Be(1_185_376);
        }
        finally
        {
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, recursive: true);
        }
    }

    [Fact(DisplayName = "HF download: multi-file repo without --file lists candidates instead of guessing")]
    public async Task DownloadAsync_MultiGgufRepoWithoutFile_FailsWithFileList()
    {
        if (Environment.GetEnvironmentVariable("GMBASIC_MODEL_DOWNLOAD_LIVE_TEST") != "1")
            return; // ponytail: graceful skip -- opt-in only, hits the real network

        using var http = new HttpClient();
        var opts = new ModelDownloadOptions
        {
            RepoId = "bartowski/Llama-3.2-1B-Instruct-GGUF",
            Kind = ModelDownloadKind.Hf,
            Dest = Path.Combine(Path.GetTempPath(), "gmbasic-hf-test-should-not-be-created"),
        };

        var result = await ModelDownloader.DownloadAsync(opts, http, msg => Console.WriteLine(msg), CancellationToken.None);

        result.Success.Should().BeFalse();
        result.Error.Should().Contain(".gguf");
        Directory.Exists(opts.Dest).Should().BeFalse();
    }

    [Fact(DisplayName = "MLX download: whole-repo multi-file download into <dest>/<repo>/, real network + parallel curl")]
    public async Task DownloadAsync_TinyMlxRepo_DownloadsAllFilesIntoRepoSubfolder()
    {
        if (Environment.GetEnvironmentVariable("GMBASIC_MODEL_DOWNLOAD_LIVE_TEST") != "1")
            return; // ponytail: graceful skip -- opt-in only, hits the real network (~48MB, smallest real mlx-community repo found)

        var tempDir = Path.Combine(Path.GetTempPath(), "gmbasic-mlx-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            using var http = new HttpClient();
            var opts = new ModelDownloadOptions
            {
                RepoId = "mlx-community/whisper-tiny-mlx-q4",
                Kind = ModelDownloadKind.Mlx,
                Dest = tempDir,
                StallTimeoutMinutes = 2,
                StallRetries = 1,
                Workers = 4,
            };

            var result = await ModelDownloader.DownloadAsync(opts, http, msg => Console.WriteLine(msg), CancellationToken.None);

            result.Success.Should().BeTrue(result.Error ?? "");
            result.DestDir.Should().Be(Path.Combine(tempDir, "whisper-tiny-mlx-q4"));
            result.Files.Should().Contain(f => f.EndsWith("config.json"));
            result.Files.Should().Contain(f => f.EndsWith("weights.npz"));
            foreach (var f in result.Files) File.Exists(f).Should().BeTrue();
        }
        finally
        {
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, recursive: true);
        }
    }
}
