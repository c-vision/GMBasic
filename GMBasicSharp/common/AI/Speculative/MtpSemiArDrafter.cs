// FILE: Basic.AI.Speculative.MtpSemiArDrafter
// Drafter SEMI-AUTOREGRESSIVO "MTP" (base per un futuro DSpark): una sessione DRAFT
// PERSISTENTE del modello draft viene tenuta in lockstep col prefisso confermato dal
// target; per proporre un blocco, il modello draft esegue una CATENA di passi greedy
// singoli sulla propria KV (ogni token proposto condiziona i precedenti — la meccanica
// di ricorsione di MTP/DeepSeek). Il target verifica poi il blocco in UN forward batch.
//
// Differenza rispetto al vecchio DraftFromPrefix (che creava una sessione FRESCA e
// ri-prefillava l'intero prefisso a ogni blocco): qui la sessione draft persiste e si
// sincronizza PER DELTA (solo i token confermati nuovi), quindi ogni blocco costa solo
// O(blockSize) decodi singoli sul modello draft, non un re-prefill O(prefix).
//
// Allineamento (corecttezza): a ogni chiamata il drafter riceve il prefisso CONFERMATO
// (committedPrefix, che cresce strettamente). Riallinea la propria sessione col
// riallineamento: se i token confermati nuovi coincidono con un prefisso del blocco che
// aveva appena proposto (full/partial accept), la sessione li contiene gia' (niente
// truncate); altrimenti tronca la sessione (llm_ai_mlx_session_truncate) al prefisso
// confermato e ri-appende i token veri. Ogni token emesso dal target resta authoritative:
// un draft sbagliato non compromette la parita' (il target verifica).
namespace Basic.AI.Speculative;

/// <summary>Primitive native (o mock) per la sessione del modello draft.</summary>
public sealed class MtpDraftSessionOps
{
    public required Func<IntPtr> CreateSession;
    public required Action<IntPtr> DestroySession;
    public required Func<IntPtr, int[], int, int> Append;      // (h, ids, n) -> 0 ok
    public required Func<IntPtr, int, int, int[], int> Generate; // (h, max, eos, out) -> n gen
    public required Func<IntPtr, int, int> Truncate;            // (h, n) -> 0 ok
    /// <summary>DSpark: P(argmax|context) del greedy corrente (confidenza del draft). Null = gate off.</summary>
    public Func<IntPtr, float>? GreedyProb;
}

/// <summary>Drafter semi-autoregressive MTP su sessione draft persistente.</summary>
public sealed class MtpSemiArDrafter : IDisposable
{
    private readonly MtpDraftSessionOps _ops;
    private readonly int _eosTokenId;
    private readonly float _confidenceThreshold; // DSpark: 0 = gate off; altrimenti soglia [0,1]
    private IntPtr _session;
    private int _mirroredLen;        // token confermati del target riflessi nella sessione draft
    private int[] _lastDraft = [];   // ultimo blocco proposto (per evitare truncate inutili)
    private bool _seeded;
    private bool _disposed;

    public MtpSemiArDrafter(MtpDraftSessionOps ops, int eosTokenId, float confidenceThreshold = 0f)
    {
        _ops = ops ?? throw new ArgumentNullException(nameof(ops));
        _eosTokenId = eosTokenId;
        _confidenceThreshold = confidenceThreshold;
    }

    /// <summary>Crea la sessione draft e la semina col prefisso iniziale (stesso prefisso
    /// che il loop speculativo usa come seed). Deve essere chiamata prima di Draft.</summary>
    public bool Seed(int[] prefixTokens)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (prefixTokens is null || prefixTokens.Length == 0) return false;
        DestroySessionLocked();
        _session = _ops.CreateSession();
        if (_session == IntPtr.Zero) return false;
        if (_ops.Append(_session, prefixTokens, prefixTokens.Length) != 0)
        {
            _ops.DestroySession(_session);
            _session = IntPtr.Zero;
            return false;
        }
        _mirroredLen = prefixTokens.Length;
        _lastDraft = [];
        _seeded = true;
        return true;
    }

    /// <summary>Propone un blocco di token (max maxDraft) condizionato sul prefisso confermato.
    /// Il prefisso ricevuto deve essere un superset (strettamente crescente) di quello dell'ultima
    /// chiamata. Ritorna [] se la sessione non e' pronta.</summary>
    public int[] Draft(int[] committedPrefix, int maxDraft)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (!_seeded || _session == IntPtr.Zero || committedPrefix is null || maxDraft <= 0) return [];

        // 1. Riallinea la sessione draft al prefisso confermato.
        var confirmed = committedPrefix.Length;
        var newConfirmed = confirmed - _mirroredLen;
        if (newConfirmed > 0)
        {
            bool alreadyThere = newConfirmed <= _lastDraft.Length
                                && SuffixMatchesDraftPrefix(committedPrefix, _lastDraft, newConfirmed);
            if (!alreadyThere)
            {
                if (_ops.Truncate(_session, _mirroredLen) != 0) return [];
                var delta = new int[newConfirmed];
                Array.Copy(committedPrefix, _mirroredLen, delta, 0, newConfirmed);
                if (_ops.Append(_session, delta, newConfirmed) != 0) return [];
            }
            _mirroredLen = confirmed;
        }

        // 2. Catena semi-autoregressiva: maxDraft passi greedy singoli sulla sessione draft.
        // Ogni token proposto e' appeso (generate avanza la KV) e condiziona il successivo.
        // DSpark confidence-scheduled: se la confidenza del greedy (P(argmax|ctx)) scende sotto
        // la soglia, si tronca il blocco (si scarta il token incerto e i successivi) -> la
        // verifica del target non spreca batch su token che il draft stesso ritiene incerti.
        var block = new List<int>(maxDraft);
        var prob = _confidenceThreshold > 0f ? _ops.GreedyProb : null;
        for (var i = 0; i < maxDraft; i++)
        {
            var outIds = new int[1];
            var n = _ops.Generate(_session, 1, _eosTokenId, outIds);
            if (n <= 0 || n > 1) break;
            if (outIds[0] == _eosTokenId) break; // non proporre EOS
            if (prob is not null && _session != IntPtr.Zero)
            {
                var p = prob(_session);
                if (p < _confidenceThreshold)
                {
                    // Scarta il token appena draftato (incerto): rollback di 1 nella sessione draft.
                    var keep = _mirroredLen + block.Count;
                    _ops.Truncate(_session, keep);
                    break;
                }
            }
            block.Add(outIds[0]);
        }
        _lastDraft = block.ToArray();
        return _lastDraft;
    }

    /// <summary>Lunghezza del prefisso confermato attualmente riflesso nella sessione draft.</summary>
    public int MirroredLen => _mirroredLen;

    private static bool SuffixMatchesDraftPrefix(int[] committedPrefix, int[] lastDraft, int k)
    {
        int start = committedPrefix.Length - k;
        for (int i = 0; i < k; i++)
            if (committedPrefix[start + i] != lastDraft[i]) return false;
        return true;
    }

    private void DestroySessionLocked()
    {
        if (_session != IntPtr.Zero)
        {
            _ops.DestroySession(_session);
            _session = IntPtr.Zero;
        }
        _seeded = false;
        _mirroredLen = 0;
        _lastDraft = [];
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        DestroySessionLocked();
    }
}
