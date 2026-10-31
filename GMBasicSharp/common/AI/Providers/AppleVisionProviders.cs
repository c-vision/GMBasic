// FILE: Basic.Providers.AppleVisionProviders
using System.Runtime.InteropServices;
using Basic.AI.Rag;

namespace Basic.Providers;

/// <summary>P/Invoke surface over native/gmb_host_ai/apple_vision/llm_vision_apple.m
/// (built as libllm_vision_apple.dylib, macOS-only). Thin C API over Apple's
/// Vision framework (VNRecognizeTextRequest) -- zero-download system OCR.</summary>
internal static partial class AppleVisionNative
{
    private const string LibName = "llm_vision_apple";

    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial IntPtr llm_vision_apple_recognize_text(string imagePath);

    [LibraryImport(LibName)]
    internal static partial void llm_vision_apple_free_string(IntPtr s);
}

/// <summary>FASE 5 Parte B, opzionale: OCR macOS-only via Apple Vision framework
/// (VNRecognizeTextRequest) -- implementa <see cref="IVisionCompleter"/>, quindi
/// <c>AI.OCR</c> funziona contro questo provider. Complementa mtmd: nessun download
/// di modello, il framework è di sistema. NON è cross-platform (solo macOS) -- mtmd
/// resta il percorso portabile. Complete() lancia NotSupportedException (vision-only,
/// anti-stub). Questo provider non carica alcun modello: il costruttore è vuoto.</summary>
internal sealed class AppleVisionCompleter : IVisionCompleter, IDisposable
{
    private readonly string _modelName;
    private bool _disposed;

    internal AppleVisionCompleter(ProviderDescriptor descriptor)
    {
        _modelName = descriptor.GetConfig("model") ?? "apple-vision";
    }

    public string ModelName => _modelName;

    public string Complete(string systemPrompt, string userPrompt) =>
        throw new NotSupportedException(
            $"apple-vision provider '{_modelName}' is OCR-only (Vision framework) — use AI.OCR, not AIAGENT.ASK.");

    /// <summary>Riconosce il testo nei byte immagine tramite VNRecognizeTextRequest.
    /// Salva i byte in un file temporaneo e lo passa allo shim (Vision legge da file).</summary>
    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        var ext = string.IsNullOrWhiteSpace(format) ? ".png" : format.StartsWith('.') ? format : "." + format;
        var tmpPath = Path.Combine(Path.GetTempPath(), "gmb-vision-" + Guid.NewGuid().ToString("N") + ext);
        try
        {
            File.WriteAllBytes(tmpPath, imageBytes);
            var result = AppleVisionNative.llm_vision_apple_recognize_text(tmpPath);
            if (result == IntPtr.Zero)
                return "";
            try
            {
                return Marshal.PtrToStringUTF8(result) ?? "";
            }
            finally
            {
                AppleVisionNative.llm_vision_apple_free_string(result);
            }
        }
        finally
        {
            try { File.Delete(tmpPath); } catch { /* best effort */ }
        }
    }

    public void Dispose()
    {
        _disposed = true;
    }
}
