// ConversionLib/Ai/AiImageConverter.cs
using Basic.AI.Rag;

namespace Basic.ConversionLib.Ai;

public static class AiImageConverter
{
    public static async Task<string> ConvertAsync(
        byte[] imageBytes, string format, IVisionCompleter vision,
        string prompt = "Describe this image in detail.", CancellationToken ct = default)
    {
        var desc = await Task.Run(() => vision.DescribeImage(imageBytes, format, prompt), ct);
        return $"[Image Description]\n\n{desc}";
    }
}
