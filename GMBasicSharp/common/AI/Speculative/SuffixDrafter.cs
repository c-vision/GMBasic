// FILE: Basic.AI.Speculative.SuffixDrafter
// SuffixDecoding (NeurIPS 2025, https://arxiv.org/abs/2411.04975) — drafter
// speculativo senza modello: indice a suffissi sul prompt + token generati; a ogni
// passo propone un draft di lunghezza variabile guardando TUTTE le occorrenze
// precedenti del suffisso corrente e votando la continuazione per frequenza,
// troncando al primo punto sotto la soglia di confidenza.
// Port di `vllm_mlx/speculative/suffix_decoding.py` (rapidMLX). Puro C# (nessuna
// dipendenza nativa): testabile senza modello.
//
// A differenza di NGramDrafter (lookup hash della continuazione immediata), il
// SuffixDrafter considera tutte le posizioni di match (non solo la prima), usa un
// suffisso più lungo possibile (più discriminante) e tronca adattivamente il draft
// dove l'indice è ambiguo.
namespace Basic.AI.Speculative;

/// <summary>Drafter a indice di suffissi con voto maggioritario e troncamento per
/// confidenza. Usato da SpeculativeDecode: propone token; il target verifica.</summary>
public sealed class SuffixDrafter
{
    public const int DefaultMaxDraft = 8;
    public const int DefaultMinDraft = 2;
    public const int DefaultMaxSuffixLen = 4;
    public const double DefaultMinConfidence = 0.3;
    public const int DefaultMaxHistory = 32000;

    private readonly List<int> _tokens = new();
    private int _shift;
    private readonly Dictionary<(int K, ulong Hash), List<int>> _suffixIndex = new();

    /// <summary>Lunghezza massima del suffisso indicizzato (k-gram). 4 copre le
    /// frasi ripetute tipiche senza far esplodere l'indice.</summary>
    public int MaxSuffixLen { get; init; } = DefaultMaxSuffixLen;

    /// <summary>Soglia di confidenza: frazione minima di match che devono concordare
    /// sul token successivo. Sotto questa soglia il draft si tronca.</summary>
    public double MinConfidence { get; init; } = DefaultMinConfidence;

    /// <summary>Cap massimo della storia indicizzata (drop dei token vecchi).</summary>
    public int MaxHistory { get; init; } = DefaultMaxHistory;

    /// <summary>Indicizza il prompt prima della generazione.</summary>
    public void AddPromptTokens(int[] tokens)
    {
        if (tokens is null) return;
        foreach (var t in tokens) _AddOne(t);
    }

    /// <summary>Indicizza un token appena generato/accettato.</summary>
    public void AddGeneratedToken(int token) => _AddOne(token);

    private void _AddOne(int token)
    {
        _tokens.Add(token);
        var absPos = _shift + _tokens.Count - 1;
        for (var k = 1; k <= MaxSuffixLen; k++)
        {
            var startLocal = _tokens.Count - k;
            if (startLocal < 0) continue;
            var key = (k, Hash(_tokens, startLocal, k));
            if (!_suffixIndex.TryGetValue(key, out var ends))
            {
                ends = new List<int>();
                _suffixIndex[key] = ends;
            }
            ends.Add(absPos);
        }
        // Trim testa se oltre MaxHistory: le voci stantie vanno rimosse (le
        // continuazioni di posizioni assolute vecchie risolverebbero a token diversi
        // nella finestra shiftata -> draft sbagliati).
        if (MaxHistory > 0 && _tokens.Count > MaxHistory)
        {
            var drop = _tokens.Count - MaxHistory;
            _tokens.RemoveRange(0, drop);
            _shift += drop;
            var stale = new List<(int, ulong)>();
            foreach (var kv in _suffixIndex)
            {
                var (k, _) = kv.Key;
                var threshold = _shift + k - 1;
                var fresh = new List<int>();
                foreach (var e in kv.Value)
                {
                    if (e >= threshold) fresh.Add(e);
                }
                if (fresh.Count > 0) _suffixIndex[kv.Key] = fresh;
                else stale.Add(kv.Key);
            }
            foreach (var key in stale) _suffixIndex.Remove(key);
        }
    }

    /// <summary>Proposta di draft per il prefisso corrente. Sincronizza la propria
    /// storia col prefisso (se piu' lungo) e interroga l'indice dai suffissi piu'
    /// lunghi ai piu' corti; ritorna la corsa piu' lunga sopra la soglia di
    /// confidenza, o un array vuoto (niente speculazione).</summary>
    public int[] Draft(int[] prefix, int maxDraft = DefaultMaxDraft, int minDraft = DefaultMinDraft)
    {
        if (prefix is null || maxDraft <= 0) return [];
        Synchronize(prefix);

        // Suffissi dal piu' lungo al piu' corto: un match lungo e' piu' discriminante.
        var maxK = Math.Min(MaxSuffixLen, _tokens.Count);
        for (var k = maxK; k >= 1; k--)
        {
            var key = (k, Hash(_tokens, _tokens.Count - k, k));
            if (!_suffixIndex.TryGetValue(key, out var positions) || positions.Count == 0) continue;

            var currentEndAbs = _shift + _tokens.Count - 1;
            var draft = BuildDraftFromPositions(positions, currentEndAbs, maxDraft);
            if (draft.Count >= minDraft) return draft.ToArray();
        }
        return [];
    }

    private void Synchronize(int[] prefix)
    {
        // La storia interna deve coprire il prefisso (il primo giro la semina, i
        // successivi aggiungono i token committati). Se il prefisso e' piu' lungo,
        // aggiungi i token mancanti.
        if (_tokens.Count < prefix.Length)
        {
            for (var i = _tokens.Count; i < prefix.Length; i++) _AddOne(prefix[i]);
        }
    }

    private List<int> BuildDraftFromPositions(List<int> matchEndPositions, int currentEndAbs, int maxDraft)
    {
        var draft = new List<int>();
        for (var offset = 0; offset < maxDraft; offset++)
        {
            var counter = new Dictionary<int, int>();
            foreach (var endAbs in matchEndPositions)
            {
                if (endAbs == currentEndAbs) continue; // non predire noi stessi
                var contLocal = endAbs + 1 + offset - _shift;
                if (contLocal >= 0 && contLocal < _tokens.Count)
                {
                    var t = _tokens[contLocal];
                    counter[t] = counter.TryGetValue(t, out var f) ? f + 1 : 1;
                }
            }
            if (counter.Count == 0) break;

            var best = -1;
            var bestCount = -1;
            var total = 0;
            foreach (var kv in counter)
            {
                total += kv.Value;
                if (kv.Value > bestCount) { bestCount = kv.Value; best = kv.Key; }
            }
            if (best < 0 || (double)bestCount / total < MinConfidence) break;

            draft.Add(best);
            // Restringi il pool ai soli match che hanno prodotto il vincitore: i
            // match lunghi monotoni restano confidenti, gli ambigui crollano.
            var kept = new List<int>();
            foreach (var endAbs in matchEndPositions)
            {
                if (endAbs == currentEndAbs) continue;
                var contLocal = endAbs + 1 + offset - _shift;
                if (contLocal >= 0 && contLocal < _tokens.Count && _tokens[contLocal] == best)
                    kept.Add(endAbs);
            }
            matchEndPositions = kept;
            if (matchEndPositions.Count == 0) break;
        }
        return draft;
    }

    /// <summary>Hash FNV-1a 64bit dei token in [start, start+len).</summary>
    private static ulong Hash(IReadOnlyList<int> tokens, int start, int len)
    {
        const ulong fnvPrime = 0x100000001b3UL;
        var h = 0xcbf29ce484222325UL;
        for (var i = start; i < start + len; i++)
        {
            h ^= (uint)tokens[i];
            h *= fnvPrime;
        }
        return h;
    }
}
