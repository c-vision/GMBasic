// FILE: Basic.AI.Rag.ILatentCompleter
// R5 — latent nativo: accesso agli hidden states del modello per la modalita'
// 'TEAM SET LATENT native'. Implementato dai provider embedded (LlamaCppLlmCompleter
// via llama_get_embeddings; MLX previsto). Se il provider attivo non lo implementa,
// la modalita' native degrada a embed (proxy SimpleEmbed) — mai errore.

namespace Basic.AI.Rag;

public interface ILatentCompleter : ISessionAwareLlmCompleter
{
    /// <summary>Dimensione hidden d_h (n_embd).</summary>
    int EmbDim { get; }

    /// <summary>Hidden finale della sessione (last token) — array di lunghezza EmbDim.</summary>
    float[] LastHidden(int sessionId);

    /// <summary>Embedding di un testo (tokenize + decode + last hidden).</summary>
    float[] EmbedText(string text);
}
