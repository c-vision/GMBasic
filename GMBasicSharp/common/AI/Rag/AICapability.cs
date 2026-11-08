// FILE: Basic.AI.Rag.AICapability
namespace Basic.AI.Rag;

/// <summary>Capability di un provider AI -- la dimensione ORTOGONALE al trasporto (vedi
/// Native-LLM-Provider-Plan / AI-Provider-Register-Unified-Plan). Un modello può essere
/// chat, ocr o audio a prescindere da come viene raggiunto (remoto API, ollama, embedded
/// llamacpp/mlx). Esiste per far coesistere piu' modelli: un provider attivo per capability,
/// non uno globale. Mappa sui ruoli del registry (ProviderRole): "completion" per chat,
/// "ocr" e "audio" per le altre due.</summary>
public enum AiCapability
{
    Chat,
    Ocr,
    Audio,
}

public static class AiCapabilityExtensions
{
    /// <summary>Il ruolo ProviderServer corrispondente. Chat mappa sul ruolo storico
    /// "completion" per retro-compatibilita' con i comandi esistenti (AIAGENT, etc.).</summary>
    public static string ToRole(this AiCapability capability) => capability switch
    {
        AiCapability.Chat => "completion",
        AiCapability.Ocr => "ocr",
        AiCapability.Audio => "audio",
        _ => "completion",
    };

    public static AiCapability FromRole(string role) => role.ToLowerInvariant() switch
    {
        "completion" or "chat" => AiCapability.Chat,
        "ocr" => AiCapability.Ocr,
        "audio" => AiCapability.Audio,
        _ => AiCapability.Chat,
    };

    public static AiCapability FromConfig(string? value)
    {
        var v = value?.Trim().TrimStart('%').ToLowerInvariant();
        return v switch
        {
            "ocr" or "1" => AiCapability.Ocr,
            "audio" or "2" => AiCapability.Audio,
            _ => AiCapability.Chat,
        };
    }

    public static string ToLabel(this AiCapability capability) => capability switch
    {
        AiCapability.Chat => "chat",
        AiCapability.Ocr => "ocr",
        AiCapability.Audio => "audio",
        _ => "chat",
    };
}
