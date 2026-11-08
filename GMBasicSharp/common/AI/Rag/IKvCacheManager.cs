// FILE: Basic.AI.Rag.IKvCacheManager
// KV-cache manager (P0, 2026-08-31) — primitive base sulla memoria del contesto del
// provider llama.cpp (llama_get_memory + llama_memory_*). Interfaccia opzionale: chi la
// implementa (LlamaCppLlmCompleter) espone clear/trim/stats della KV-cache della sessione.
namespace Basic.AI.Rag;

public interface IKvCacheManager
{
    /// <summary>Statistiche della KV-cache di una sessione.</summary>
    sealed record CacheStats(int NCtx, int NTokens, long Bytes, int PosMin, int PosMax)
    {
        public double UsedPercent => NCtx > 0 ? 100.0 * NTokens / NCtx : 0;
    }

    /// <summary>Svuota completamente la KV-cache della sessione (lo stato libro resta).</summary>
    void ClearCache(int sessionId);

    /// <summary>Rimuove i primi <paramref name="dropTokens"/> token (eviction dei piu'
    /// vecchi), mantenendo i piu' recenti.</summary>
    void TrimCache(int sessionId, int dropTokens);

    /// <summary>Statistiche correnti della KV-cache della sessione.</summary>
    CacheStats GetCacheStats(int sessionId);
}
