// FILE: Basic.Providers.ProviderRole
namespace Basic.Providers;

/// <summary>Logical provider roles for rotation and active selection. I ruoli sono
/// raggiungibili dalle costanti globali pubbliche %CHAT/%OCR/%AUDIO/%EMBEDDING/%RERANKER/%TTS/
/// %DECIDE (CONSTANTS namespace, valori 0..6) OPPURE dal nome di ruolo storico stringa.</summary>
public static class ProviderRole
{
    public const string Embedding = "embedding";
    public const string Completion = "completion";
    public const string Reranker = "reranker";
    public const string Ocr = "ocr";
    public const string Audio = "audio";
    public const string Tts = "tts";
    public const string Decide = "decide";

    public static bool IsKnown(string role) =>
        string.Equals(role, Embedding, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Completion, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Reranker, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Ocr, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Audio, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Tts, StringComparison.OrdinalIgnoreCase)
        || string.Equals(role, Decide, StringComparison.OrdinalIgnoreCase);

    internal static string Normalize(string role) => role.ToLowerInvariant();

    /// <summary>Risolutore UNICO costante/ruolo → ruolo canonico del registry. Accetta:
    /// costante numerica (0=chat/completion, 1=ocr, 2=audio, 3=embedding, 4=reranker,
    /// 5=tts, 6=decide),
    /// nome di ruolo storico (completion/ocr/audio/embedding/reranker) o label capability
    /// (chat/ocr/audio). Usato da RegisterSource/NormalizeRole/FromConfig — MAI stringhe
    /// libere diverse da questi insiemi.</summary>
    public static string FromCapabilityOrRole(string? value)
    {
        var v = value?.Trim().TrimStart('%');
        if (string.IsNullOrEmpty(v)) return Completion;
        var lower = v.ToLowerInvariant();
        return lower switch
        {
            "0" or "chat" or "completion" => Completion,
            "1" or "ocr" => Ocr,
            "2" or "audio" => Audio,
            "3" or "embedding" => Embedding,
            "4" or "reranker" => Reranker,
            "5" or "tts" => Tts,
            "6" or "decide" => Decide,
            _ => Completion,
        };
    }
}
