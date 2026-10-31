// ConversionLib/PluginSystem.cs — Plugin architecture, streaming, security, evaluation

namespace Basic.ConversionLib;

/// <summary>Contract for document-to-markdown converters.</summary>
public interface IMarkdownConverter
{
    bool CanHandle(string fileExtension);
    Task<string> ConvertAsync(Stream fileStream, string fileExtension, CancellationToken ct = default);
}

/// <summary>Extended contract for streaming (chunk-by-chunk) converters.</summary>
public interface IStreamingConverter : IMarkdownConverter
{
    IAsyncEnumerable<string> ConvertStreamingAsync(Stream fileStream, string fileExtension, CancellationToken ct = default);
}

/// <summary>Plugin that bundles one or more converters.</summary>
public interface IConverterPlugin
{
    string Name { get; }
    IEnumerable<IMarkdownConverter> GetConverters();
}

/// <summary>Resolves converters by file extension.</summary>
public class ConverterRegistry
{
    private readonly List<IMarkdownConverter> _converters = [];

    public void Register(IMarkdownConverter c) => _converters.Add(c);
    public void RegisterPlugin(IConverterPlugin p) { foreach (var c in p.GetConverters()) Register(c); }

    public IMarkdownConverter? Resolve(string extension)
    {
        foreach (var c in _converters)
            if (c.CanHandle(extension)) return c;
        return null;
    }

    public IReadOnlyList<IMarkdownConverter> All => _converters.AsReadOnly();
}

/// <summary>Conversion result with metadata.</summary>
public sealed record ConversionResult(string Markdown, string SourceFormat, bool Success, string? ErrorMessage = null);

/// <summary>High-level conversion service.</summary>
public class MarkdownService
{
    private readonly ConverterRegistry _registry;

    public MarkdownService(ConverterRegistry registry) => _registry = registry;

    public async Task<ConversionResult> ConvertAsync(string filePath, CancellationToken ct = default)
    {
        var ext = Path.GetExtension(filePath);
        var converter = _registry.Resolve(ext);
        if (converter == null) return new("", ext, false, $"No converter for '{ext}'");
        try
        {
            using var fs = File.OpenRead(filePath);
            var md = await converter.ConvertAsync(fs, ext, ct);
            return new(md, ext, true);
        }
        catch (Exception ex) { return new("", ext, false, ex.Message); }
    }

    public async Task<ConversionResult> ConvertAsync(Stream stream, string extension, CancellationToken ct = default)
    {
        var converter = _registry.Resolve(extension);
        if (converter == null) return new("", extension, false, $"No converter for '{extension}'");
        try
        {
            var md = await converter.ConvertAsync(stream, extension, ct);
            return new(md, extension, true);
        }
        catch (Exception ex) { return new("", extension, false, ex.Message); }
    }

    public IAsyncEnumerable<string> ConvertStreamingAsync(Stream stream, string extension, CancellationToken ct = default)
    {
        var c = _registry.Resolve(extension);
        if (c is IStreamingConverter sc) return sc.ConvertStreamingAsync(stream, extension, ct);
        return FallbackStreamAsync(c, stream, extension, ct);
    }

    private static async IAsyncEnumerable<string> FallbackStreamAsync(
        IMarkdownConverter? c, Stream stream, string extension, [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken ct)
    {
        if (c != null)
        {
            var md = await c.ConvertAsync(stream, extension, ct);
            yield return md;
        }
    }
}

/// <summary>Streaming converter that splits content into chunks (e.g., PDF page-by-page).</summary>
public abstract class ChunkedConverter : IStreamingConverter
{
    public abstract bool CanHandle(string fileExtension);
    public abstract IAsyncEnumerable<string> ConvertStreamingAsync(Stream fileStream, string fileExtension, CancellationToken ct);

    public async Task<string> ConvertAsync(Stream fileStream, string fileExtension, CancellationToken ct = default)
    {
        var sb = new System.Text.StringBuilder();
        await foreach (var chunk in ConvertStreamingAsync(fileStream, fileExtension, ct))
            sb.Append(chunk);
        return sb.ToString();
    }
}
