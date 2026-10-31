// ConversionLib/Formats/ImageConverter.cs

namespace Basic.ConversionLib.Formats;

public static class ImageConverter
{
    private static readonly HashSet<string> ImageExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg", ".ico", ".tiff", ".tif"
    };

    public static bool CanHandle(string extension) => ImageExtensions.Contains(extension);

    public static Model.Document Parse(byte[] bytes, string extension)
    {
        var size = bytes.Length;
        var sizeStr = size switch
        {
            < 1024 => $"{size} B",
            < 1024 * 1024 => $"{size / 1024.0:F1} KB",
            _ => $"{size / (1024.0 * 1024.0):F1} MB"
        };

        return new Model.Document(
        [
            new Model.Paragraph([
                new Model.TextInline($"[Image: {extension.TrimStart('.')}, {sizeStr}]", Model.InlineStyle.None)
            ])
        ], []);
    }
}
