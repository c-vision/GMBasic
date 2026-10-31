// FILE: Basic.AI.Speculative.NGramDrafter
// N-Gram speculative decoding — draft model senza modello: propone token basati su
// sequenze gia' osservate (Prompt Lookup, vLLM/NVIDIA). 3 livelli di memoria:
//   Context  : hash immediato degli ultimi N token (lookup O(1), per-sessione)
//   Session  : LRU chiave per-tokenizer (ModelId+TokenizerId+Revision+ProjectId+hash)
//   Project  : inverted index su source/doc/tests/generated (complementare a VectorDB:
//              VectorDB = concetto, N-Gram = continuazione letterale)
// Usato da SpeculativeEngine: DRAFT -> TARGET LLM verify -> accept/rollback.
// Puro C# (nessuna dipendenza nativa): testabile senza modello.
namespace Basic.AI.Speculative;

/// <summary>Draft n-gram a 3 livelli per speculative decoding. Non esegue il modello:
/// propone token da sequenze osservate; il target (SpeculativeEngine) verifica.</summary>
public sealed class NGramDrafter
{
    public const int DefaultMaxDraft = 8;
    public const int DefaultMinDraft = 2;
    public const int ContextWindow = 8;          // hash degli ultimi 8 token (spec: N=2..8)
    public const int SessionCapacity = 512;      // LRU cap

    /// <summary>Livello 1 — Context: hash(ultimi token) -> continuazione osservata.
    /// Aggiornato a ogni generazione accettata (UpdateCache).</summary>
    private readonly Dictionary<ulong, int[]> _context = new();

    /// <summary>Livello 2 — Session: LRU chiave per-tokenizer. La chiave isola
    /// ModelId+TokenizerId+Revision+ProjectId (mai collisioni tra modelli).</summary>
    private readonly Dictionary<string, (ulong Hash, int[] Draft)> _session = new();
    private readonly LinkedList<string> _sessionOrder = new();

    /// <summary>Livello 3 — Project: inverted index token -> liste di token successivi
    /// osservati (dalle sorgenti indicizzate). Complementare a VectorDB.</summary>
    private readonly Dictionary<int, Dictionary<int, int>> _project = new(); // token -> (next -> freq)

    /// <summary>Quando true, il drafter usa lo stato del parser (dentro SUB / dopo IMPORT)
    /// per privilegiare token strutturali del linguaggio. Collegato dall'host se attivo.</summary>
    public bool CompilerAware { get; set; }

    /// <summary>Seed di progetto per il livellamento (chiavi session). Cambia la scope
    /// delle chiavi LRU senza svuotare (es. per-tokenizer).</summary>
    public string ProjectId { get; set; } = "default";

    /// <summary>Insegna una sequenza osservata (token gia' generati/accettati) ai 3 livelli.
    /// Chiamato dopo ogni accettazione del target.</summary>
    public void Learn(int[] tokens)
    {
        if (tokens is null || tokens.Length < 2) return;

        // Context: hash di ogni finestra -> token successivo.
        for (var i = 0; i + 1 < tokens.Length; i++)
        {
            var win = tokens[Math.Max(0, i + 1 - ContextWindow)..(i + 1)];
            var h = Hash(win);
            _context[h] = tokens[(i + 1)..Math.Min(i + 1 + DefaultMaxDraft, tokens.Length)];

            // Project: inverted index (token -> successivo, con frequenza).
            var from = tokens[i];
            var to = tokens[i + 1];
            if (!_project.TryGetValue(from, out var nexts))
            {
                nexts = new Dictionary<int, int>();
                _project[from] = nexts;
            }
            nexts[to] = nexts.TryGetValue(to, out var f) ? f + 1 : 1;
        }
    }

    /// <summary>Proposta di draft per il prefisso corrente: cerca la continuazione nei 3
    /// livelli in ordine (Context -> Session -> Project). Ritorna 0..maxDraft token, o
    /// un array vuoto se nessun match (niente speculazione).</summary>
    public int[] Draft(int[] prefix, int maxDraft = DefaultMaxDraft, int minDraft = DefaultMinDraft)
    {
        if (prefix is null || prefix.Length < 2 || maxDraft <= 0) return [];

        // 1. Context: hash immediato degli ultimi token.
        var window = prefix[Math.Max(0, prefix.Length - ContextWindow)..];
        for (var len = window.Length; len >= 2; len--)
        {
            var h = Hash(window[^len..]);
            if (_context.TryGetValue(h, out var cont))
            {
                var d = TakeValid(cont, prefix, maxDraft, minDraft);
                if (d.Length > 0) return d;
            }
        }

        // 2. Session LRU (chiave per-tokenizer): stesso hash, scope progetto.
        var sessionKey = MakeSessionKey(window);
        if (_session.TryGetValue(sessionKey, out var entry))
        {
            TouchSession(sessionKey);
            var d = TakeValid(entry.Draft, prefix, maxDraft, minDraft);
            if (d.Length > 0) return d;
        }

        // 3. Project inverted index: cammina dal token piu' recente.
        var from = prefix[^1];
        var path = new List<int>();
        var cur = from;
        for (var step = 0; step < maxDraft && _project.TryGetValue(cur, out var nexts); step++)
        {
            var best = nexts.OrderByDescending(kv => kv.Value).First().Key;
            if (best == cur) break; // auto-loop
            path.Add(best);
            cur = best;
        }
        if (path.Count >= minDraft) return path.Take(maxDraft).ToArray();

        return [];
    }

    /// <summary>Aggiorna la cache dopo la verifica del target (i token accettati).</summary>
    public void UpdateCache(string sessionKey, int[] accepted)
    {
        if (accepted is null || accepted.Length == 0) return;
        Learn(accepted);
        if (!string.IsNullOrEmpty(sessionKey))
        {
            var h = Hash(accepted[^Math.Min(ContextWindow, accepted.Length)..]);
            _session[sessionKey] = (h, accepted);
            TouchSession(sessionKey);
        }
    }

    /// <summary>Chiave di sessione per-tokenizer (livello 2): isola modelli/tokenizer.</summary>
    public string MakeSessionKey(int[] window) =>
        $"{ProjectId}|{Hash(window)}";

    /// <summary>Seleziona i primi `maxDraft` token validi della continuazione (non
    /// ripetono il prefisso finale, non vuoti, >= minDraft).</summary>
    private static int[] TakeValid(int[] cont, int[] prefix, int maxDraft, int minDraft)
    {
        if (cont is null || cont.Length < minDraft) return [];
        var take = Math.Min(maxDraft, cont.Length);
        var d = cont[..take];
        // Evita un draft che ricomincia col token appena emesso (loop).
        if (d[0] == prefix[^1] && d.Length > 1) d = d[1..];
        return d.Length >= minDraft ? d : [];
    }

    private void TouchSession(string key)
    {
        _sessionOrder.Remove(key);
        _sessionOrder.AddLast(key);
        while (_sessionOrder.Count > SessionCapacity)
        {
            var oldest = _sessionOrder.First!;
            _sessionOrder.RemoveFirst();
            _session.Remove(oldest.Value);
        }
    }

    /// <summary>Hash deterministico stile xxhash-lite (FNV-1a 64bit) dei token.</summary>
    private static ulong Hash(int[] tokens)
    {
        const ulong fnvPrime = 0x100000001b3UL;
        var h = 0xcbf29ce484222325UL;
        foreach (var t in tokens)
        {
            h ^= (ulong)(uint)t;
            h *= fnvPrime;
        }
        return h;
    }
}
