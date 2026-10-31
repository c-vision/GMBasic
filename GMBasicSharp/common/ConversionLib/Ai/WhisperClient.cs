// ConversionLib/Ai/WhisperClient.cs — Local audio transcription (requires ONNX Runtime)

namespace Basic.ConversionLib.Ai;

/// <summary>Transcribes audio to text locally using Whisper ONNX model.</summary>
public interface IWhisperClient
{
    Task<string> TranscribeAsync(byte[] audioBytes, string format, CancellationToken ct = default);
}

/// <summary>Stub implementation — requires ONNX Runtime + Whisper model to work.</summary>
public sealed class WhisperStub : IWhisperClient
{
    public Task<string> TranscribeAsync(byte[] audioBytes, string format, CancellationToken ct = default)
        => Task.FromResult("[Whisper: ONNX Runtime + model not configured]");
}

/// <summary>
/// Audio formats supported.
/// Real implementation needs:
///   dotnet add package Microsoft.ML.OnnxRuntime
///   dotnet add package Microsoft.ML.OnnxRuntime.Managed
///   Download whisper model (~75MB) from https://huggingface.co/ggerganov/whisper.cpp
///   Convert to ONNX via https://github.com/microsoft/onnxruntime
/// </summary>
public static class WhisperFactory
{
    public static IWhisperClient Create() => new WhisperStub();
}
