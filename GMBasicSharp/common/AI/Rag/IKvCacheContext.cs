// FILE: Basic.AI.Rag.IKvCacheContext
// Cache context KV — astrazione risolvibile per NOME, condivisa da tutte le funzioni
// AI.*STATS / AI.*INFO (AI.CACHESTATS, AI.MODELINFO, AI.PREFILLSTATS, ...).
//
// Perché esiste: il piano LLM definisce le funzioni su <alias> (alias modello/cache), ma il
// runtime reale ha sessioni KV PER-AGENTE (AgentRecord.SessionId). Un provider può avere piu'
// agenti, quindi "alias modello" non identifica una singola KV-cache. Questo risolutore
// unifica la semantica: oggi risolve per nome AGENTE (la sua sessione KV), in futuro anche per
// alias di cache first-class (AI CACHE CREATE <alias>) quando quel registry esistera'.
//
// Ordine di risoluzione (primo match vince):
//   1. alias di cache first-class  (futuro — RegistryCache se esiste)
//   2. agente registrato            (oggi — AgentServer, sessione KV via IKvCacheManager)
//   3. errore esplicito (mai null silenzioso: il chiamante decide il JSON d'errore)
namespace Basic.AI.Rag;

/// <summary>Snapshot delle statistiche della cache KV di un contesto.</summary>
public interface IKvCacheContext
{
    /// <summary>Nome del contesto (l'agente o l'alias risolto).</summary>
    string Name { get; }

    /// <summary>Statistiche correnti della cache KV. null se il completer non supporta
    /// IKvCacheManager (es. fixture) — mai eccezione.</summary>
    IKvCacheManager.CacheStats? Stats();

    /// <summary>Svuota la cache KV del contesto.</summary>
    void Clear();

    /// <summary>Rimuove i primi N token (eviction dei piu' vecchi).</summary>
    void Trim(int dropTokens);
}

/// <summary>Risolve un nome in un <see cref="IKvCacheContext"/>. Implementazione unica:
/// prima cache alias (futuro), poi agente. Per l'MVP il resolver agente e' quello reale.</summary>
public static class KvCacheContextResolver
{
    /// <summary>Risolve <paramref name="name"/> in un contesto KV. Ritorna null se il nome
    /// non e' un agente registrato (e non esiste ancora un registry di cache alias).</summary>
    public static IKvCacheContext? Resolve(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);

        // 1. Alias di cache first-class (futuro): quando esistera' il registry di AI CACHE
        //    CREATE <alias>, si cerca qui per primo. Oggi non esiste -> si passa all'agente.
        //    Punto di inserimento: var cache = CacheRegistry.TryGet(name); if (cache != null) ...

        // 2. Agente registrato (oggi): la sessione KV dell'agente via AgentServer.
        return AgentContext.TryResolve(name);
    }
}

/// <summary>Contesto KV di un AGENTE (oggi l'unica sorgente di sessioni KV reali).</summary>
public sealed class AgentContext : IKvCacheContext
{
    private readonly Func<IKvCacheManager.CacheStats?> _stats;
    private readonly Action _clear;
    private readonly Action<int> _trim;

    public string Name { get; }

    public AgentContext(string name, Func<IKvCacheManager.CacheStats?> stats, Action clear, Action<int> trim)
    {
        Name = name;
        _stats = stats;
        _clear = clear;
        _trim = trim;
    }

    public IKvCacheManager.CacheStats? Stats() => _stats();
    public void Clear() => _clear();
    public void Trim(int dropTokens) => _trim(dropTokens);

    /// <summary>Crea il contesto dalla sessione dell'agente (via AgentServer), se l'agente
    /// esiste e il suo completer e' IKvCacheManager. Ritorna null altrimenti.</summary>
    public static AgentContext? TryResolve(string name)
    {
        // Evita dipendenza circolare: il resolver chiama AgentServer tramite un delegato
        // injectato dal runtime (IlRuntime) per non accoppiare i namespace.
        return ResolverHook?.Invoke(name);
    }

    /// <summary>Hook impostato dal runtime (IlRuntime.SetupAgentCacheContextResolver) per
    /// accedere ad AgentServer senza dipendenza statica AI->Agents qui. Se null, il resolver
    /// agente non e' disponibile (prima dell'init) e Resolve ritorna null.</summary>
    public static Func<string, AgentContext?>? ResolverHook { get; set; }
}
