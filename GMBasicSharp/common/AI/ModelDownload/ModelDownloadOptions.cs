// AI/ModelDownload/ModelDownloadOptions.cs — parameters for the HF/MLX REPL download commands
namespace Basic.ModelDownload;

public enum ModelDownloadKind { Hf, Mlx }

/// <summary>Mirrors the parameter set of Rapid-MLX-NextGen's <c>download_model()</c>
/// (vllm_mlx/tools/model_downloader.py), ported to what this C# curl-only port actually
/// implements. See <see cref="ModelDownloader"/> for what each flag does and which ones
/// are honest no-ops (no equivalent transport exists in this port).</summary>
public sealed record ModelDownloadOptions
{
    public required string RepoId { get; init; }
    public required ModelDownloadKind Kind { get; init; }

    /// <summary>HF only. Exact filename, or a '*' wildcard pattern, selecting which file(s)
    /// to download out of a multi-file repo (e.g. a multi-quant GGUF repo). Required when the
    /// repo contains more than one .gguf file.</summary>
    public string? File { get; init; }

    /// <summary>Destination folder override. Falls back to the first configured AIPATH entry,
    /// then to "&lt;repo-root&gt;/samples/ai/models/&lt;gguf|mlx&gt;/".</summary>
    public string? Dest { get; init; }

    public int StallTimeoutMinutes { get; init; } = 5;
    public int StallRetries { get; init; } = 1;
    public int Workers { get; init; } = 8;
    public bool NoXet { get; init; }
    public bool NoToken { get; init; }
    public bool NoCurl { get; init; }
    public bool Quiet { get; init; }
}

public sealed record ModelDownloadResult(bool Success, string DestDir, IReadOnlyList<string> Files, string? Error);
