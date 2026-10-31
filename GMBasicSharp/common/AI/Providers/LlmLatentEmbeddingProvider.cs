// FILE: Basic.Providers.LlmLatentEmbeddingProvider
// Adapter che espone un completer locale con latent/embedding (llamacpp/mlx, che implementano
// ILatentCompleter.EmbedText) come IEmbeddingProvider per il RAG. Riusa il motore già caricato:
// zero nuovo caricamento modello, stessa sessione KV del completer.
using Basic.AI.Rag;
using Basic.SQLite.Vector;

namespace Basic.Providers;

/// <summary>Avvolge un completer locale che espone embedding latenti (llamacpp/mlx) come
/// <see cref="IEmbeddingProvider"/>. L'embedding viene dal motore del completer (gmb_ai_*
/// embed), non da un secondo modello.</summary>
public sealed class LlmLatentEmbeddingProvider : IEmbeddingProvider
{
    private readonly ILatentCompleter _latent;

    public LlmLatentEmbeddingProvider(ILatentCompleter latent, string? modelVersion = null)
    {
        _latent = latent;
        ModelVersion = modelVersion ?? "llm-latent";
    }

    public string ModelVersion { get; }

    public float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        var v = _latent.EmbedText(text);
        if (v is null || v.Length == 0) return new float[dimensions];
        // Adatta alla dimensione richiesta: tronca o zero-pad (il completer può avere dim diversa).
        if (v.Length == dimensions) return v;
        var outv = new float[dimensions];
        Array.Copy(v, outv, Math.Min(v.Length, dimensions));
        return outv;
    }
}
