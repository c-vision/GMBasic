// AI/Rag/IMultimodalCompleter.cs

namespace Basic.AI.Rag;

public interface IVisionCompleter : ILlmCompleter
{
    string DescribeImage(byte[] imageBytes, string format, string prompt);
}

public interface IAudioCompleter : ILlmCompleter
{
    string TranscribeAudio(byte[] audioBytes, string format);
}
