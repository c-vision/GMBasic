// FILE: Basic.AI.Knowledge.BrainInstaller
// Second Brain — INSTALL (AI BRAIN INSTALL, §6.1): importa un cervello GIÀ COSTRUITO e
// impacchettato in uno ZIP .gmbrain (remoto da URL http(s)://...zip o locale da path .zip).
// La costruzione on-demand NON è compito di questo modulo (blocco ore) — REPL/CLI la fanno con
// RAG BUILD/GRAPH BUILD + BRAIN EXPORT. Qui: scarica/estrai/valida (manifest + checksum,
// §6.1ex)/installa in AiConfig.BrainsDefaultDir/<name>/ con rollback su errore.
using System.Net.Http;
using System.Text.Json;
using CommonLib;

namespace Basic.AI.Knowledge;

public sealed record BrainInstallResult(string Name, string InstallRoot, List<string> Available, bool Ok, string? Error);

public static class BrainInstaller
{
    /// <summary>Scarica uno zip remoto nella cache temporanea (riuso del pattern
    /// stall-watchdog: curl con --retry, un solo file). Ritorna il path dello zip locale.</summary>
    public static async Task<string?> DownloadZipAsync(string url, CancellationToken ct = default)
    {
        var cacheDir = Path.Combine(Path.GetTempPath(), "gmbrain-download");
        Directory.CreateDirectory(cacheDir);
        var dest = Path.Combine(cacheDir, Path.GetFileName(new Uri(url).AbsolutePath) ?? "brain.zip");
        if (string.IsNullOrWhiteSpace(Path.GetExtension(dest))) dest += ".zip";

        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
            var bytes = await http.GetByteArrayAsync(url, ct).ConfigureAwait(false);
            await File.WriteAllBytesAsync(dest, bytes, ct).ConfigureAwait(false);
            return dest;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"?download fallito: {ex.Message}");
            return null;
        }
    }

    /// <summary>Importa un cervello da zip (remoto URL o locale path). Valida brain.manifest.json
    /// (format/name/checksum/files sha256) e installa in destDir (default
    /// AiConfig.BrainsDefaultDir). Rollback completo se la validazione fallisce.</summary>
    public static BrainInstallResult Install(string name, string source, string? destDir = null, bool activate = false)
    {
        string? localZip = source;
        var isRemote = source.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
            || source.StartsWith("https://", StringComparison.OrdinalIgnoreCase);
        if (isRemote)
        {
            var dl = DownloadZipAsync(source).GetAwaiter().GetResult();
            if (dl is null || !File.Exists(dl))
                return new BrainInstallResult(name, "", [], false, "download dello zip fallito");
            localZip = dl;
        }

        if (!File.Exists(localZip))
            return new BrainInstallResult(name, "", [], false, $"zip non trovato: {localZip}");

        var installRoot = destDir ?? Path.Combine(AiConfig.BrainsDefaultDir, name);
        var staging = installRoot + ".staging-" + Guid.NewGuid().ToString("N");
        try
        {
            Directory.CreateDirectory(staging);
            CompressionUtils.ExtractZip(localZip, staging);

            // valida
            var manifestPath = Path.Combine(staging, "brain.manifest.json");
            if (!File.Exists(manifestPath))
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false,
                    "non è un cervello GMBasic valido: manca brain.manifest.json");
            }

            BrainManifest manifest;
            try
            {
                manifest = JsonSerializer.Deserialize<BrainManifest>(File.ReadAllText(manifestPath),
                    new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase })
                    ?? throw new InvalidDataException("manifest non decodificabile");
            }
            catch (Exception ex)
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false, $"manifest corrotto: {ex.Message}");
            }

            if (!string.Equals(manifest.Format, BrainManifest.FormatName, StringComparison.Ordinal))
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false,
                    $"non è un cervello GMBasic valido: format '{manifest.Format}' (atteso '{BrainManifest.FormatName}')");
            }
            if (!string.IsNullOrWhiteSpace(manifest.Name) && !string.Equals(manifest.Name, name, StringComparison.OrdinalIgnoreCase))
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false,
                    $"il manifest dichiara il nome '{manifest.Name}', ma è stato richiesto '{name}'");
            }
            if (manifest.Files.Count == 0)
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false, "manifest senza files: zip vuoto o incompleto");
            }
            if (!manifest.VerifySelfChecksum())
            {
                TryRollback(staging);
                return new BrainInstallResult(name, "", [], false, "checksum del manifest non valido (zip manomesso/corrotto)");
            }

            // verifica ogni file dichiarato: esiste e sha256 combacia
            foreach (var (rel, hash) in manifest.Files)
            {
                var full = Path.GetFullPath(Path.Combine(staging, rel.Replace('/', Path.DirectorySeparatorChar)));
                if (!full.StartsWith(Path.GetFullPath(staging), StringComparison.OrdinalIgnoreCase))
                {
                    TryRollback(staging);
                    return new BrainInstallResult(name, "", [], false, $"zip-slip: entry '{rel}' fuori dalla root");
                }
                if (!File.Exists(full))
                {
                    TryRollback(staging);
                    return new BrainInstallResult(name, "", [], false, $"file mancante nello zip: {rel}");
                }
                var actual = BrainManifest.HashFile(full);
                if (!string.Equals(actual, hash, StringComparison.Ordinal))
                {
                    TryRollback(staging);
                    return new BrainInstallResult(name, "", [], false, $"checksum non valido per {rel} (file corrotto)");
                }
            }

            // installa: svuota la destinazione precedente, poi muovi lo staging
            if (Directory.Exists(installRoot))
            {
                var backup = installRoot + ".old-" + Guid.NewGuid().ToString("N");
                try { Directory.Move(installRoot, backup); Directory.Delete(backup, recursive: true); }
                catch { /* ignora */ }
            }
            Directory.CreateDirectory(installRoot);
            foreach (var entry in Directory.GetFileSystemEntries(staging))
            {
                var target = Path.Combine(installRoot, Path.GetFileName(entry));
                if (Directory.Exists(entry)) Directory.Move(entry, target);
                else File.Move(entry, target);
            }

            List<string> available;
            if (manifest.Available.Count > 0) available = [.. manifest.Available];
            else available = [.. BrainRegistry.AvailableModes(name)];
            BrainRegistry.Register(name, source: source, vaultPath: installRoot, mode: manifest.Mode,
                checksum: manifest.Checksum, version: manifest.Version, docs: manifest.Docs,
                parsers: manifest.Parsers, probe: manifest.Probe, signedBy: manifest.SignedBy);
            if (activate) BrainRegistry.Use(name);

            return new BrainInstallResult(name, installRoot, available, true, null);
        }
        catch (Exception ex)
        {
            TryRollback(staging);
            return new BrainInstallResult(name, "", [], false, ex.Message);
        }
        finally
        {
            TryRollback(staging);
        }
    }

    private static void TryRollback(string dir)
    {
        try { if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true); } catch { }
    }
}
