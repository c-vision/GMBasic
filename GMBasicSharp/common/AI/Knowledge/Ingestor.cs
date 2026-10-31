// FILE: Basic.AI.Knowledge.Ingestor
// Second Brain — L6 Ingestion: importa file esterni (PDF/docx/xlsx/pptx/epub/rtf/odt/csv/
// immagini/audio) in un vault come note .md. I file testuali (.md/.txt) si copiano; i binari
// si convertono con un BinaryTextExtractor fornito dal chiamante (pattern RagIndexService:
// common/AI NON referenzia ConversionLib per evitare il ciclo — REPL/CLI passano
// DocumentConverter); le immagini si OCRizzano col provider visione (%OCR); l'audio si
// trascrive col provider audio (%AUDIO). Preserva la struttura delle cartelle.
// Riusa NoteManager per la scrittura nel vault.
namespace Basic.AI.Knowledge;

public sealed record IngestResult(string NotePath, string Kind, bool Ok, string? Error);

public static class Ingestor
{
    /// <summary>Delegate di estrazione testo da binari (fornito dal chiamante REPL/CLI, che
    /// usa DocumentConverter — common/AI non può referenziare ConversionLib).</summary>
    public static Func<string, string>? BinaryTextExtractor { get; set; }

    private static readonly HashSet<string> TextExts = new(StringComparer.OrdinalIgnoreCase)
        { ".md", ".markdown", ".txt" };
    private static readonly HashSet<string> ImageExts = new(StringComparer.OrdinalIgnoreCase)
        { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp" };
    private static readonly HashSet<string> AudioExts = new(StringComparer.OrdinalIgnoreCase)
        { ".wav", ".mp3", ".m4a", ".flac", ".ogg" };

    /// <summary>Importa un file esterno nel vault come nota .md. <paramref name="relPath"/> è il
    /// path relativo nel vault (default: nome del file con .md). Ritorna il risultato.</summary>
    public static IngestResult ImportFile(string vaultName, string sourceFile, string? relPath = null, bool ocr = true, bool transcribe = true)
    {
        var root = NoteManager.VaultRoot(vaultName);
        if (root is null) return new IngestResult("", "", false, "vault non disponibile");
        if (!File.Exists(sourceFile)) return new IngestResult("", "", false, $"file non trovato: {sourceFile}");

        var ext = Path.GetExtension(sourceFile);
        string? content = null;
        string kind;

        try
        {
            if (TextExts.Contains(ext))
            {
                content = File.ReadAllText(sourceFile);
                kind = "text";
            }
            else if (ImageExts.Contains(ext) && ocr)
            {
                content = TryOcr(sourceFile);
                kind = "ocr";
                if (string.IsNullOrWhiteSpace(content)) return new IngestResult("", "", false, "OCR non disponibile (nessun provider %OCR)");
            }
            else if (AudioExts.Contains(ext) && transcribe)
            {
                content = TryTranscribe(sourceFile);
                kind = "audio";
                if (string.IsNullOrWhiteSpace(content)) return new IngestResult("", "", false, "trascrizione non disponibile (nessun provider %AUDIO)");
            }
            else if (BinaryTextExtractor is not null)
            {
                content = BinaryTextExtractor(sourceFile);
                kind = "convert";
                if (string.IsNullOrWhiteSpace(content)) return new IngestResult("", "", false, $"conversione non riuscita: {sourceFile}");
            }
            else
            {
                return new IngestResult("", "", false, $"estensione non supportata (niente BinaryTextExtractor): {ext}");
            }
        }
        catch (Exception ex)
        {
            return new IngestResult("", "", false, ex.Message);
        }

        var target = relPath ?? Path.ChangeExtension(Path.GetFileName(sourceFile), ".md");
        if (NoteManager.CreatePath(vaultName, target, content) is null)
            return new IngestResult("", "", false, "path non valido (esce dalla root del vault)");
        return new IngestResult(target, kind, true, null);
    }

    /// <summary>Importa una cartella intera nel vault (preserva la struttura, relativamente a
    /// <paramref name="srcFolder"/>). Ritorna il numero di file importati.</summary>
    public static int ImportFolder(string vaultName, string srcFolder, bool preserveStructure = true, bool ocr = true, bool transcribe = true)
    {
        if (!Directory.Exists(srcFolder)) return 0;
        var count = 0;
        var root = NoteManager.VaultRoot(vaultName);
        if (root is null) return 0;
        foreach (var file in Directory.GetFiles(srcFolder, "*", SearchOption.AllDirectories))
        {
            var rel = preserveStructure
                ? Path.GetRelativePath(srcFolder, file)
                : Path.GetFileName(file);
            var result = ImportFile(vaultName, file, rel.Replace('\\', '/'), ocr, transcribe);
            if (result.Ok) count++;
        }
        return count;
    }

    /// <summary>Sincronizza una cartella esterna con il vault (bidirezionale o push_only).
    /// Ritorna (importati, aggiornati, conflitti) come stringa "I/U/C".
    /// - push_only: copia i file del vault nella cartella (solo se la sorgente è il vault).
    /// - bidirectional (default): importa nel vault i file più recenti dalla cartella; se il file
    ///   del vault è più recente e direction=bidirectional lo riporta nella cartella (newest_wins).
    /// Ritorna "nImp/nUpd/nConf" — mai stub: cartella mancante → "0/0/0".</summary>
    public static string SyncFolder(string vaultName, string srcFolder, string? direction = "bidirectional", string? conflict = "newest_wins")
    {
        if (!Directory.Exists(srcFolder)) return "0/0/0";
        var root = NoteManager.VaultRoot(vaultName);
        if (root is null) return "0/0/0";
        var dir = (direction ?? "bidirectional").ToLowerInvariant() switch
        {
            "push" or "push_only" => "push",
            _ => "bidirectional",
        };
        var cMode = (conflict ?? "newest_wins").ToLowerInvariant();

        int imported = 0, updated = 0, conflicts = 0;

        // push_only: copia i file del vault (più recenti) nella cartella esterna.
        if (dir == "push")
        {
            foreach (var target in Directory.GetFiles(root, "*", SearchOption.AllDirectories))
            {
                var rel = Path.GetRelativePath(root, target).Replace('\\', '/');
                var dst = Path.Combine(srcFolder, rel.Replace('/', Path.DirectorySeparatorChar));
                try
                {
                    if (!File.Exists(dst))
                    {
                        Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
                        File.Copy(target, dst, overwrite: true);
                        imported++;
                        continue;
                    }
                    var dstTime = File.GetLastWriteTimeUtc(dst);
                    var srcTime = File.GetLastWriteTimeUtc(target);
                    if (srcTime > dstTime.AddSeconds(1))
                    {
                        File.Copy(target, dst, overwrite: true);
                        File.SetLastWriteTimeUtc(dst, srcTime);
                        updated++;
                    }
                }
                catch { conflicts++; }
            }
            return $"{imported}/{updated}/{conflicts}";
        }

        foreach (var file in Directory.GetFiles(srcFolder, "*", SearchOption.AllDirectories))
        {
            var rel = Path.GetRelativePath(srcFolder, file).Replace('\\', '/');
            var target = Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar));
            if (!File.Exists(target))
            {
                var r = ImportFile(vaultName, file, rel);
                if (r.Ok) imported++;
                continue;
            }
            // esiste in entrambi: confronta i mtime (newest_wins)
            var srcTime = File.GetLastWriteTimeUtc(file);
            var dstTime = File.GetLastWriteTimeUtc(target);
            if (srcTime > dstTime.AddSeconds(1))
            {
                var r = ImportFile(vaultName, file, rel);
                if (r.Ok) updated++;
            }
            else if (dstTime > srcTime.AddSeconds(1))
            {
                // il vault è più recente: bidirectional lo riporta in cartella, push_no
                if (dir == "bidirectional" && cMode == "newest_wins")
                {
                    try
                    {
                        var dstDir = Path.GetDirectoryName(file)!;
                        if (!Directory.Exists(dstDir)) Directory.CreateDirectory(dstDir);
                        File.Copy(target, file, overwrite: true);
                        File.SetLastWriteTimeUtc(file, dstTime);
                        updated++;
                    }
                    catch { conflicts++; }
                }
                else conflicts++;
            }
            // mtime uguali (±1s) → nessuna azione
        }
        return $"{imported}/{updated}/{conflicts}";
    }

    /// <summary>WEB.SCRAPE: scarica una pagina (HTTP GET), ne estrae il testo (markdown minimale:
    /// titolo + body senza tag), e la salva come nota .md nel vault. Ritorna il path relativo
    /// della nota, o una stringa d'errore esplicita.</summary>
    public static string WebScrape(string vaultName, string url, string? relPath = null)
    {
        if (string.IsNullOrWhiteSpace(url)) return "Error: url vuota";
        try
        {
            var helper = new CommonLib.Http.HttpClientHelper();
            var result = helper.PerformRequestAsync(url, new CommonLib.Http.HttpOptions { Method = "GET" }, default).GetAwaiter().GetResult();
            if (result.StatusCode >= 400 || result.ErrorMessage is { Length: > 0 })
                return $"Error: {result.StatusCode} {result.ErrorMessage}";
            if (string.IsNullOrWhiteSpace(result.Body))
                return "Error: risposta vuota";

            var target = relPath ?? "web/" + SanitizeHost(url) + ".md";
            var text = ExtractWebText(result.Body, Path.GetFileNameWithoutExtension(target));
            if (NoteManager.CreatePath(vaultName, target, text) is null)
                return "Error: path non valido (esce dalla root del vault)";
            return target;
        }
        catch (Exception ex)
        {
            return $"Error: {ex.Message}";
        }
    }

    /// <summary>Estrae testo leggibile da una pagina HTML: rimuove script/style/tag, decodifica
    /// le entity base, mantiene i titoli come markdown. Best-effort, niente parser completo.</summary>
    private static string ExtractWebText(string html, string title)
    {
        var text = System.Text.RegularExpressions.Regex.Replace(html, "<script[^>]*>.*?</script>", " ", System.Text.RegularExpressions.RegexOptions.Singleline | System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        text = System.Text.RegularExpressions.Regex.Replace(text, "<style[^>]*>.*?</style>", " ", System.Text.RegularExpressions.RegexOptions.Singleline | System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        text = System.Text.RegularExpressions.Regex.Replace(text, "<[^>]+>", "\n");
        text = System.Net.WebUtility.HtmlDecode(text);
        text = System.Text.RegularExpressions.Regex.Replace(text, "[ \t]+", " ");
        text = System.Text.RegularExpressions.Regex.Replace(text, "\n{3,}", "\n\n");
        return $"---\ntitle: {title}\ntype: web\n---\n{text.Trim()}";
    }

    private static string SanitizeHost(string url)
    {
        try { return new Uri(url).Host.Replace('.', '_'); } catch { return "page"; }
    }


    private static string? TryOcr(string file)
    {
        try
        {
            var server = Basic.Providers.ProviderServer.Instance;
            if (string.IsNullOrEmpty(server.GetActiveName("ocr"))) return null;
            var instance = server.GetActiveCompletion("ocr");
            if (instance is not Basic.AI.Rag.IVisionCompleter vision) return null;
            var bytes = File.ReadAllBytes(file);
            var format = Path.GetExtension(file).TrimStart('.').ToLowerInvariant();
            if (format == "jpg") format = "jpeg";
            return vision.DescribeImage(bytes, format, "Extract all text from this image.");
        }
        catch { return null; }
    }

    private static string? TryTranscribe(string file)
    {
        try
        {
            var server = Basic.Providers.ProviderServer.Instance;
            if (string.IsNullOrEmpty(server.GetActiveName("audio"))) return null;
            var instance = server.GetActiveCompletion("audio");
            if (instance is not Basic.AI.Rag.IAudioCompleter audio) return null;
            var bytes = File.ReadAllBytes(file);
            var format = Path.GetExtension(file).TrimStart('.').ToLowerInvariant();
            return audio.TranscribeAudio(bytes, format);
        }
        catch { return null; }
    }
}
