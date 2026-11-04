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
    /// AiConfig.BrainsDefaultDir). Rollback completo se la validazione fallisce.
    ///
    /// <para>Con <paramref name="destDir"/> nullo (produzione) il TRASFERIMENTO e l'ESTRAZIONE li
    /// fa il runtime C (<c>gmb_brains_install_source</c>: gmbfetch con resume+sha, zip-slip
    /// safe, cache <c>~/.gmbasic/brains/&lt;nome&gt;/&lt;versione&gt;</c>), così VM/C e C99Native
    /// installano lo stesso cervello con lo stesso codice. Qui restano la VALIDAZIONE forte
    /// (format, nome, checksum per-file) sui file installati e la registrazione.</para>
    /// <para><paramref name="destDir"/> esplicito = percorso storico in C#, usato dai test e
    /// dall'installazione localizzata.</para></summary>
    public static BrainInstallResult Install(string name, string source, string? destDir = null, bool activate = false)
    {
        if (destDir is null) return InstallViaNative(name, source, activate);

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

    /// <summary>Installazione "di produzione": trasferimento + estrazione nel runtime C
    /// (<c>gmb_brains_install_source</c>), poi VALIDAZIONE forte e registrazione qui. Un cervello
    /// che non supera la validazione NON resta installato (viene rimosso dalla cache).
    /// Sorgente vuota = installazione dal MANIFEST (`brains-manifest.json`).</summary>
    private static BrainInstallResult InstallViaNative(string name, string source, bool activate)
    {
        bool ok; string root; string error;
        try
        {
            (ok, root, error) = string.IsNullOrWhiteSpace(source)
                ? CommonLib.NativeBrains.Install(name)
                : CommonLib.NativeBrains.InstallSource(name, source);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"?AI BRAIN INSTALL {name}: {ex.GetBaseException().Message}");
            return new BrainInstallResult(name, "", [], false, ex.GetBaseException().Message);
        }
        if (!ok || string.IsNullOrEmpty(root))
        {
            var msg = string.IsNullOrEmpty(error) ? "installazione fallita" : error;
            Console.Error.WriteLine($"?AI BRAIN INSTALL {name}: {msg}");
            return new BrainInstallResult(name, "", [], false, msg);
        }

        BrainManifest? manifest = null;
        string? invalid = null;
        var manifestPath = Path.Combine(root, "brain.manifest.json");
        if (!File.Exists(manifestPath))
        {
            invalid = "non è un cervello GMBasic valido: manca brain.manifest.json";
        }
        else
        {
            try
            {
                manifest = JsonSerializer.Deserialize<BrainManifest>(File.ReadAllText(manifestPath),
                    new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
            }
            catch (Exception ex) { invalid = $"manifest corrotto: {ex.Message}"; }
        }

        if (invalid is null && manifest is not null)
        {
            if (!string.Equals(manifest.Format, BrainManifest.FormatName, StringComparison.Ordinal))
                invalid = $"non è un cervello GMBasic valido: format '{manifest.Format}' (atteso '{BrainManifest.FormatName}')";
            else if (!string.IsNullOrWhiteSpace(manifest.Name) && !string.Equals(manifest.Name, name, StringComparison.OrdinalIgnoreCase))
                invalid = $"il manifest dichiara il nome '{manifest.Name}', ma è stato richiesto '{name}'";
            /* checksum e per-file sono OPZIONALI: un cervello artigianale (o di terze parti) ha
             * spesso solo rag/<nome>.db. La garanzia d'integrità di un cervello DISTRIBUITO è lo
             * sha256 dell'ASSET nel manifest dei cervelli, verificato da gmbfetch prima di
             * estrarre; il manifest interno è una firma in più, e quando è presente si verifica. */
            else if (manifest.Checksum is not null && !manifest.VerifySelfChecksum())
                invalid = "checksum del manifest non valido (zip manomesso/corrotto)";
            else
            {
                foreach (var (rel, hash) in manifest.Files)
                {
                    var full = Path.GetFullPath(Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar)));
                    if (!full.StartsWith(Path.GetFullPath(root), StringComparison.OrdinalIgnoreCase))
                    { invalid = $"zip-slip: entry '{rel}' fuori dalla root"; break; }
                    if (!File.Exists(full)) { invalid = $"file mancante nello zip: {rel}"; break; }
                    if (!string.Equals(BrainManifest.HashFile(full), hash, StringComparison.Ordinal))
                    { invalid = $"checksum non valido per {rel} (file corrotto)"; break; }
                }
            }
        }

        if (invalid is null && BrainRegistry.AvailableModes(name).Length == 0)
            invalid = "il cervello non contiene né rag/ né graphs/ (niente da usare)";

        if (invalid is not null)
        {
            CommonLib.NativeBrains.Remove(name);   // un cervello non valido non resta installato
            Console.Error.WriteLine($"?AI BRAIN INSTALL {name}: {invalid}");
            return new BrainInstallResult(name, "", [], false, invalid);
        }

        var m = manifest!;
        var available = m.Available.Count > 0 ? new List<string>(m.Available) : [.. BrainRegistry.AvailableModes(name)];
        BrainRegistry.Register(name, source: source, vaultPath: root, mode: m.Mode ?? "",
            checksum: m.Checksum, version: m.Version, docs: m.Docs,
            parsers: m.Parsers, probe: m.Probe, signedBy: m.SignedBy);
        if (activate) BrainRegistry.Use(name);
        return new BrainInstallResult(name, root, available, true, null);
    }

    private static void TryRollback(string dir)
    {
        try { if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true); } catch { }
    }
}
