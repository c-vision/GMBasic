// AI/ModelDownload/HfRepoManifest.cs — HuggingFace repo file listing + integrity metadata
using System.Text.Json;

namespace Basic.ModelDownload;

public sealed record HfFileEntry(string RFilename, long Size, string? Sha256);

public sealed record HfRepoManifest(string RepoId, IReadOnlyList<HfFileEntry> Files);

/// <summary>Fetches the file manifest for a HuggingFace repo via the public models API
/// (<c>GET https://huggingface.co/api/models/{id}?blobs=true</c>), used both for integrity
/// verification (LFS sha256) and for listing files when an HF download needs <c>--file</c>
/// disambiguation.</summary>
public static class HfRepoManifestClient
{
    private const string ApiBase = "https://huggingface.co/api/models";

    public static async Task<HfRepoManifest> FetchAsync(HttpClient http, string repoId, string? token, CancellationToken ct)
    {
        using var req = new HttpRequestMessage(HttpMethod.Get, $"{ApiBase}/{repoId}?blobs=true");
        if (!string.IsNullOrWhiteSpace(token))
            req.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", token);

        using var resp = await http.SendAsync(req, ct).ConfigureAwait(false);
        if (!resp.IsSuccessStatusCode)
        {
            var body = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
            throw new InvalidOperationException($"HuggingFace API error {(int)resp.StatusCode} for '{repoId}': {body}");
        }

        var json = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
        using var doc = JsonDocument.Parse(json);
        var files = new List<HfFileEntry>();
        if (doc.RootElement.TryGetProperty("siblings", out var siblings) && siblings.ValueKind == JsonValueKind.Array)
        {
            foreach (var s in siblings.EnumerateArray())
            {
                var name = s.TryGetProperty("rfilename", out var rf) ? rf.GetString() ?? "" : "";
                if (name.Length == 0) continue;
                var size = s.TryGetProperty("size", out var sz) && sz.ValueKind == JsonValueKind.Number ? sz.GetInt64() : 0L;
                string? sha256 = null;
                if (s.TryGetProperty("lfs", out var lfs) && lfs.ValueKind == JsonValueKind.Object &&
                    lfs.TryGetProperty("sha256", out var sh) && sh.ValueKind == JsonValueKind.String)
                    sha256 = sh.GetString();
                files.Add(new HfFileEntry(name, size, sha256));
            }
        }
        return new HfRepoManifest(repoId, files);
    }

    /// <summary>Direct download URL for a single file in a repo (HF's resolve endpoint;
    /// curl follows the redirect to the CDN with <c>-fL</c>).</summary>
    public static string ResolveUrl(string repoId, string filename) =>
        $"https://huggingface.co/{repoId}/resolve/main/{filename}";
}
