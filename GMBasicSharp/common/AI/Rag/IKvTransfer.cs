// FILE: Basic.AI.Rag.IKvTransfer
// Estensione 9.1 (LatentMAS) — KV-cache transfer IN-RAM tra sessioni dello stesso modello.
// Chi lo implementa (LlamaCppLlmCompleter) copia lo stato KV+token da una sessione all'altra
// via llama_state_get_data/set_data, senza I/O su disco. Valido solo tra sessioni con lo
// stesso modello e stessa config ctx.
namespace Basic.AI.Rag;

public interface IKvTransfer
{
    /// <summary>Copia lo stato KV+token della sessione <paramref name="srcSessionId"/> nella
    /// sessione <paramref name="dstSessionId"/> (in-RAM). Ritorna true se il trasferimento è
    /// avvenuto.</summary>
    bool TransferKv(int srcSessionId, int dstSessionId);
}
