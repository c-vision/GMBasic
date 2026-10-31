// FILE: Basic.SQLite.Vector.VectorSettings
using System.Text.Json;

namespace Basic.SQLite.Vector;

/// <summary>Optional ONNX embedding + reranker settings from <c>app.settings.json</c>.</summary>
public sealed record VectorSettings(
    string? OnnxModelPath,
    string? OnnxVocabPath,
    int? Dimensions,
    string? ModelVersion,
    string? RerankerModelPath = null,
    string? RerankerVocabPath = null,
    string? RerankerModelVersion = null)
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true
    };

    /// <summary>Loads <c>vector</c> section; returns null if file or section missing.</summary>
    public static VectorSettings? TryLoadFromFile(string settingsPath)
    {
        if (!File.Exists(settingsPath))
            return null;

        try
        {
            using var stream = File.OpenRead(settingsPath);
            using var doc = JsonDocument.Parse(stream);
            if (!doc.RootElement.TryGetProperty("vector", out var vector))
                return null;

            return new VectorSettings(
                vector.TryGetProperty("onnxModel", out var m) ? m.GetString() : null,
                vector.TryGetProperty("onnxVocab", out var v) ? v.GetString() : null,
                vector.TryGetProperty("dimensions", out var d) && d.TryGetInt32(out var dim) ? dim : null,
                vector.TryGetProperty("modelVersion", out var mv) ? mv.GetString() : null,
                vector.TryGetProperty("rerankerModel", out var rm) ? rm.GetString() : null,
                vector.TryGetProperty("rerankerVocab", out var rv) ? rv.GetString() : null,
                vector.TryGetProperty("rerankerModelVersion", out var rmv) ? rmv.GetString() : null);
        }
        catch (JsonException)
        {
            return null;
        }
    }
}
