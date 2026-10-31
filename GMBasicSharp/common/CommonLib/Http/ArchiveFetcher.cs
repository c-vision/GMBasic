// CommonLib/Http/ArchiveFetcher.cs — Fase 3: helper UNICO per scaricare, verificare (sha256)
// ed estrarre archivi (.zip/.tar.gz/.tgz/.tar/.gz). Prima questa logica era duplicata in
// FetchLibsLib/FetchChromiumLib/FetchObscuraLib (AI/ModelDownload) e in
// PackageManager.EnsureSourceAsync (sorgente GitHub release dei pacchetti).
//
// Chi lo usa:
//   - FetchLibsLib / FetchChromiumLib / FetchObscuraLib  (gmb setup, bundle da GMBasicModules)
//   - Parser.PackageManager.EnsureSourceAsync             (source: github-release dei pacchetti)

using System.Formats.Tar;
using System.IO.Compression;
using System.Security.Cryptography;

namespace CommonLib;

/// <summary>Download + verifica sha256 + estrazione archivi: una sola implementazione condivisa.</summary>
public static class ArchiveFetcher
{
    /// <summary>Scarica <paramref name="url"/> in <paramref name="destFile"/> (streaming, con progress).</summary>
    public static async Task<string> DownloadAsync(HttpClient http, string url, string destFile,
        Action<long, long>? progress = null, CancellationToken ct = default)
    {
        using var resp = await http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false);
        resp.EnsureSuccessStatusCode();
        var total = resp.Content.Headers.ContentLength ?? -1;
        await using var src = await resp.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        await using var dst = File.Create(destFile);
        var buffer = new byte[81920];
        long done = 0;
        int read;
        while ((read = await src.ReadAsync(buffer, ct).ConfigureAwait(false)) > 0)
        {
            await dst.WriteAsync(buffer.AsMemory(0, read), ct).ConfigureAwait(false);
            done += read;
            progress?.Invoke(done, total >= 0 ? total : done);
        }
        return destFile;
    }

    /// <summary>True se il file esiste, è non vuoto e il suo sha256 coincide con <paramref name="expectedSha"/>
    /// (o se <paramref name="expectedSha"/> è vuoto).</summary>
    public static bool VerifySha256(string path, string? expectedSha)
    {
        if (string.IsNullOrEmpty(expectedSha)) return true;
        using var sha = SHA256.Create();
        using var fs = File.OpenRead(path);
        var hash = Convert.ToHexString(sha.ComputeHash(fs)).ToLowerInvariant();
        return hash.Equals(expectedSha.Trim().ToLowerInvariant(), StringComparison.Ordinal);
    }

    /// <summary>Estrae un archivio in destDir. Formati: .zip, .tar.gz/.tgz, .tar, .gz (singolo file).
    /// Un formato non riconosciuto è trattato come file singolo (copiato in destDir).</summary>
    public static void Extract(string archivePath, string destDir)
    {
        Directory.CreateDirectory(destDir);
        var name = archivePath.ToLowerInvariant();
        if (name.EndsWith(".zip"))
            ZipFile.ExtractToDirectory(archivePath, destDir, overwriteFiles: true);
        else if (name.EndsWith(".tar.gz") || name.EndsWith(".tgz"))
        {
            using var gz = new GZipStream(File.OpenRead(archivePath), CompressionMode.Decompress);
            TarFile.ExtractToDirectory(gz, destDir, overwriteFiles: true);
        }
        else if (name.EndsWith(".tar"))
            TarFile.ExtractToDirectory(archivePath, destDir, overwriteFiles: true);
        else if (name.EndsWith(".gz"))
        {
            using var gz = new GZipStream(File.OpenRead(archivePath), CompressionMode.Decompress);
            using var outFs = File.Create(Path.Combine(destDir, Path.GetFileNameWithoutExtension(archivePath)));
            gz.CopyTo(outFs);
        }
        else
            // Non è un archivio: file singolo (es. eseguibile) copiato nella cache.
            File.Copy(archivePath, Path.Combine(destDir, Path.GetFileName(archivePath)), overwrite: true);
    }

    /// <summary>
    /// Scarica in una cartella temp, verifica lo sha256 (se dato) ed estrae in <paramref name="destDir"/>.
    /// Ritorna (ok, errore, byteScaricati).
    /// </summary>
    public static async Task<(bool Ok, string? Error, long Bytes)> FetchArchiveAsync(
        HttpClient http, string url, string destDir, string? sha256,
        Action<long, long>? progress = null, CancellationToken ct = default)
    {
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_fetch_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            var suffix = url.EndsWith(".tar.gz", StringComparison.OrdinalIgnoreCase)
                ? ".tar.gz" : Path.GetExtension(new Uri(url).AbsolutePath);
            var archivePath = Path.Combine(tmp, "archive" + suffix);
            await DownloadAsync(http, url, archivePath, progress, ct).ConfigureAwait(false);
            var bytes = new FileInfo(archivePath).Length;
            if (!VerifySha256(archivePath, sha256))
                return (false, "sha256 non combacia dopo il download (archivio corrotto o URL cambiato)", bytes);
            Extract(archivePath, destDir);
            return (true, null, bytes);
        }
        finally
        {
            try { Directory.Delete(tmp, recursive: true); } catch { }
        }
    }
}
