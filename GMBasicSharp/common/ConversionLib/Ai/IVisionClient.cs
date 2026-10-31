// ConversionLib/Ai/IVisionClient.cs

namespace Basic.ConversionLib.Ai;

public interface IVisionClient
{
    Task<string> DescribeAsync(byte[] imageBytes, string imageFormat, string prompt, CancellationToken ct = default);
}

public interface IAudioClient
{
    Task<string> TranscribeAsync(byte[] audioBytes, string audioFormat, CancellationToken ct = default);
}
