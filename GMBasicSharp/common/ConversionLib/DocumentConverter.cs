// ConversionLib/DocumentConverter.cs — Unified facade with plugin system
using Basic.ConversionLib.Model;
using Basic.ConversionLib.Render;

using Basic.ConversionLib.Formats;

namespace Basic.ConversionLib;

public static class DocumentConverter
{
    private static readonly ConverterRegistry _registry = new();
    private static readonly MarkdownService _service;

    static DocumentConverter()
    {
        _registry.RegisterPlugin(new BuiltinPlugin());
        _service = new MarkdownService(_registry);
    }

    public static ConverterRegistry Registry => _registry;

    /// <summary>Converts a file to markdown (auto-detect format by extension).</summary>
    public static string Convert(string filePath, CancellationToken ct = default)
        => ConvertAsync(filePath, ct).GetAwaiter().GetResult();

    /// <summary>Converts a file to markdown (async).</summary>
    public static Task<string> ConvertAsync(string filePath, CancellationToken ct = default)
        => LegacyConvert(filePath, ct);

    /// <summary>Converts bytes to the specified output format using extension detection.</summary>
    public static string FromBytes(byte[] bytes, string inputExtension, string? outputExtension = null)
    {
        var doc = LegacyParse(bytes, inputExtension);
        return (outputExtension ?? ".md").ToLowerInvariant() switch
        {
            ".html" or ".htm" => HtmlRenderer.Render(doc),
            ".txt" or ".text" => TextRenderer.Render(doc),
            _ => MarkdownRenderer.Render(doc)
        };
    }

    /// <summary>List supported input formats.</summary>
    public static string[] SupportedFormats => [
        "docx", "pdf", "xlsx", "csv", "pptx", "epub", "html", "rtf", "odt",
        "txt", "md", "json", "xml", "yaml", "jpg", "png", "gif", "bmp", "webp", "svg"
    ];

    /// <summary>Batch convert directory.</summary>
    public static string ConvertBatch(string dir, string fmt, bool recursive)
    {
        var sb = new System.Text.StringBuilder();
        var opt = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
        if (!Directory.Exists(dir)) return $"?Directory not found: {dir}";
        foreach (var f in Directory.GetFiles(dir, "*.*", opt))
        {
            var ext = Path.GetExtension(f).ToLowerInvariant();
            if (ext is ".md" or ".html" or ".txt" or ".exe" or ".dll" or ".pdb") continue;
            try {
                var bytes = File.ReadAllBytes(f);
                var outFile = Path.ChangeExtension(f, "." + fmt);
                File.WriteAllText(outFile, FromBytes(bytes, ext, "." + fmt));
                sb.AppendLine($"[OK] {Path.GetFileName(f)}");
            } catch (Exception ex) { sb.AppendLine($"?{Path.GetFileName(f)}: {ex.Message}"); }
        }
        return sb.ToString();
    }

    /// <summary>Convert URL to markdown. In caso di fetch fallita ritorna un errore
    /// ESPLICITO e DETERMINISTICO (stesso marcatore del runtime C `gmb_doc_web`): il
    /// messaggio della eccezione (HttpClient) non è riproducibile sul backend C (curl),
    /// quindi per parità di risultato si usa un marcatore comune.</summary>
    public static string ConvertUrl(string url)
    {
        try {
            using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("GMBasic/1.0");
            var html = http.GetStringAsync(url).GetAwaiter().GetResult();
            return FromBytes(System.Text.Encoding.UTF8.GetBytes(html), ".html", ".md");
        } catch (Exception) { return "?Error: web fetch failed"; }
    }

    // Legacy fast path (synchronous, backward compatible)
    private static async Task<string> LegacyConvert(string filePath, CancellationToken ct)
    {
        var bytes = await File.ReadAllBytesAsync(filePath, ct);
        return MarkdownRenderer.Render(LegacyParse(bytes, Path.GetExtension(filePath)));
    }

    public static Model.Document LegacyParse(byte[] bytes, string extension) => extension.ToLowerInvariant() switch
    {
        ".docx" => DocxParser.Parse(bytes),
        ".csv" => CsvParser.Parse(bytes),
        ".xlsx" or ".xlsm" => XlsxParser.Parse(bytes),
        ".odt" => OdtParser.Parse(bytes),
        ".epub" => EpubParser.Parse(bytes),
        ".rtf" => RtfParser.Parse(bytes),
        ".pptx" => PptxParser.Parse(bytes),
        ".pdf" => PdfParser.Parse(bytes),
        ".html" or ".htm" => HtmlConverter.Parse(bytes),
        ".json" => DataConverters.ParseJson(bytes),
        ".xml" => DataConverters.ParseXml(bytes),
        ".yaml" or ".yml" => DataConverters.ParseYaml(bytes),
        ".txt" or ".md" or ".log" => TextParser.Parse(bytes),
        _ when ImageConverter.CanHandle(extension) => ImageConverter.Parse(bytes, extension),
        _ => DocxParser.Parse(bytes)
    };

    /// <summary>Plugin-based conversion (async, streaming ready).</summary>
    public static Task<ConversionResult> ConvertPluginAsync(string filePath, CancellationToken ct = default)
        => _service.ConvertAsync(filePath, ct);

    /// <summary>Streaming conversion (for large files).</summary>
    public static IAsyncEnumerable<string> ConvertStreamingAsync(Stream stream, string extension, CancellationToken ct = default)
        => _service.ConvertStreamingAsync(stream, extension, ct);

    // Legacy public API — kept for backward compatibility
    public static string ToMarkdown(Document doc) => MarkdownRenderer.Render(doc);
    public static Document ParseDocx(byte[] bytes) => DocxParser.Parse(bytes);

    internal sealed class BuiltinPlugin : IConverterPlugin
    {
        public string Name => "Builtin";
        public IEnumerable<IMarkdownConverter> GetConverters()
        {
            yield return new GenericConverter(".docx", DocxParser.Parse);
            yield return new GenericConverter(".csv", CsvParser.Parse);
            yield return new GenericConverter(".xlsx", XlsxParser.Parse, ".xlsm");
            yield return new GenericConverter(".odt", OdtParser.Parse);
            yield return new GenericConverter(".epub", EpubParser.Parse);
            yield return new GenericConverter(".rtf", RtfParser.Parse);
            yield return new GenericConverter(".pptx", PptxParser.Parse);
            yield return new PdfChunkedConverter();
            yield return new GenericConverter(".html", HtmlConverter.Parse, ".htm");
            yield return new GenericConverter(".json", DataConverters.ParseJson);
            yield return new GenericConverter(".xml", DataConverters.ParseXml);
            yield return new GenericConverter(".yaml", DataConverters.ParseYaml, ".yml");
            yield return new GenericConverter(".txt", TextParser.Parse, ".md", ".log");
            yield return new ImageGenericConverter();
        }
    }

    private sealed class GenericConverter : IMarkdownConverter
    {
        private readonly HashSet<string> _exts;
        private readonly Func<byte[], Model.Document> _parse;
        public GenericConverter(string primary, Func<byte[], Model.Document> parse, params string[] aliases)
        {
            _exts = [primary, .. aliases];
            _parse = parse;
        }
        public bool CanHandle(string ext) => _exts.Contains(ext);
        public Task<string> ConvertAsync(Stream stream, string ext, CancellationToken ct)
        {
            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            return Task.FromResult(MarkdownRenderer.Render(_parse(ms.ToArray())));
        }
    }

    private sealed class PdfChunkedConverter : ChunkedConverter
    {
        public override bool CanHandle(string ext) => ext == ".pdf";
        public override async IAsyncEnumerable<string> ConvertStreamingAsync(Stream stream, string ext, [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken ct)
        {
            using var ms = new MemoryStream();
            await stream.CopyToAsync(ms, ct);
            var doc = PdfParser.Parse(ms.ToArray());
            var md = MarkdownRenderer.Render(doc);
            // Yield page-by-page chunks: split on ## Page markers
            var parts = md.Split("## Page");
            for (int i = 0; i < parts.Length; i++)
            {
                var chunk = (i == 0 ? parts[0] : "## Page" + parts[i]);
                yield return chunk;
                if (i < parts.Length - 1) yield return "\n\n";
            }
        }
    }

    private sealed class ImageGenericConverter : IMarkdownConverter
    {
        public bool CanHandle(string ext) => ImageConverter.CanHandle(ext);
        public Task<string> ConvertAsync(Stream stream, string ext, CancellationToken ct)
        {
            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            return Task.FromResult(MarkdownRenderer.Render(ImageConverter.Parse(ms.ToArray(), ext)));
        }
    }
}
