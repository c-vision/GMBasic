// AI/Decision/ScoringHead.cs
//
// Artefatto `scoring_head.json` del decision model JEV/SystemOne (spec upstream ms-swift).
// Il file porta i parametri di calibrazione a due stadi — bias per slot verbale (24),
// temperatura per kind (noul/choice/score) e temperatura globale — letti dal provider
// locale; il training/fit vive in gmbllm (PUL), il readout nel motore (ABI v8 CAP_DECIDE).
//
// Formato accettato (tutti i campi tranne `bias` sono opzionali):
//   {
//     "bias":          {"false": 0.0, "true": 0.0, "0": 0.0, ..., "A": 0.0}  // oppure [24]
//     "verbalizer_ids": {"false": 123, "true": 456, "A": 789, ...}            // o per-kind
//     "ranges":        {"score": [0, 5]},                                     // informativo
//     "head_T":        1.0,
//     "T_kind":        {"noul": 1.0, "choice": 1.0, "score": 1.0}             // oppure [1,1,1]
//   }
//
// Ordine canonico degli slot (per gli array): NOUL false,true · SCORE 0..5 · CHOICE A..P.
// Se un'etichetta non è uno slot canonico, il bias resta 0: mai un valore inventato.

using System.Globalization;
using System.Text.Json.Nodes;

namespace Basic.Decision;

public sealed class ScoringHead
{
    public const string FileName = "scoring_head.json";

    /// <summary>Ordine canonico dei 24 slot verbali (array `bias` posizionale).</summary>
    public static readonly IReadOnlyList<string> SlotOrder =
    [
        "false", "true",
        "0", "1", "2", "3", "4", "5",
        "A", "B", "C", "D", "E", "F", "G", "H",
        "I", "J", "K", "L", "M", "N", "O", "P",
    ];

    private readonly Dictionary<string, float> _bias;
    private readonly Dictionary<string, int> _tokenIds;
    private readonly float[] _kindTemperature;

    /// <summary>Temperatura globale della testa (`head_T`).</summary>
    public float HeadTemperature { get; }

    /// <summary>true se l'artefatto contiene parametri di calibrazione (default: sì, il file
    /// esiste proprio per calibrare). `"calibrated": false` lo disabilita esplicitamente.</summary>
    public bool IsCalibrated { get; }

    private ScoringHead(Dictionary<string, float> bias, Dictionary<string, int> tokenIds,
        float[] kindTemperature, float headTemperature, bool isCalibrated)
    {
        _bias = bias;
        _tokenIds = tokenIds;
        _kindTemperature = kindTemperature;
        HeadTemperature = headTemperature;
        IsCalibrated = isCalibrated;
    }

    /// <summary>Carica `scoring_head.json` da una cartella modello; null se assente.
    /// Un file presente ma malformato è un errore esplicito (niente calibrazione silenziosa).</summary>
    public static ScoringHead? TryLoad(string? modelDir)
    {
        if (string.IsNullOrWhiteSpace(modelDir))
            return null;
        var path = Path.Combine(modelDir, FileName);
        if (!File.Exists(path))
            return null;
        return TryLoadFile(path);
    }

    /// <summary>Carica un artefatto a PATH ESPLICITO (`decideCalibration` nella config del
    /// provider). File assente o illeggibile = errore esplicito: la calibrazione richiesta non
    /// può degradare in silenzio a identità.</summary>
    public static ScoringHead TryLoadFile(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            throw new DecisionProviderException("scoring_head.json: path vuoto.");
        if (!File.Exists(path))
            throw new DecisionProviderException($"scoring_head.json non trovato ('{path}').");
        try
        {
            return Parse(File.ReadAllText(path));
        }
        catch (Exception ex) when (ex is not DecisionProviderException)
        {
            throw new DecisionProviderException($"scoring_head.json illeggibile ('{path}'): {ex.Message}", ex);
        }
    }

    public static ScoringHead Parse(string json)
    {
        JsonNode? root;
        try
        {
            root = JsonNode.Parse(json);
        }
        catch (Exception ex)
        {
            throw new DecisionProviderException($"scoring_head.json non è JSON valido: {ex.Message}", ex);
        }
        if (root is not JsonObject obj)
            throw new DecisionProviderException("scoring_head.json: atteso un oggetto JSON.");

        var bias = new Dictionary<string, float>(StringComparer.OrdinalIgnoreCase);
        switch (obj["bias"])
        {
            case JsonArray arr:
                for (var i = 0; i < arr.Count && i < SlotOrder.Count; i++)
                {
                    var v = ReadFloat(arr[i]);
                    if (v.HasValue) bias[SlotOrder[i]] = v.Value;
                }
                break;
            case JsonObject bo:
                foreach (var kv in bo)
                {
                    var slot = CanonicalSlot(kv.Key) ?? kv.Key;
                    var v = ReadFloat(kv.Value);
                    if (v.HasValue) bias[slot] = v.Value;
                }
                break;
            case null:
                break;
            default:
                throw new DecisionProviderException("scoring_head.json: 'bias' deve essere un oggetto o un array.");
        }

        var tokenIds = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        CollectVerbalizerIds(obj["verbalizer_ids"], tokenIds);

        var kindTemperature = new[] { 1f, 1f, 1f };
        switch (obj["T_kind"])
        {
            case JsonArray arr:
                for (var i = 0; i < arr.Count && i < 3; i++)
                {
                    var v = ReadFloat(arr[i]);
                    if (v is > 0) kindTemperature[i] = v.Value;
                }
                break;
            case JsonObject to:
                foreach (var kv in to)
                {
                    var v = ReadFloat(kv.Value);
                    if (v is not > 0) continue;
                    var idx = KindIndex(kv.Key);
                    if (idx >= 0) kindTemperature[idx] = v.Value;
                }
                break;
            case null:
                break;
            default:
                throw new DecisionProviderException("scoring_head.json: 'T_kind' deve essere un oggetto o un array.");
        }

        var headTemperature = ReadFloat(obj["head_T"]) is > 0 and var ht ? ht : 1f;
        var isCalibrated = obj["calibrated"] is not JsonValue cv
                           || !cv.TryGetValue<bool>(out var flag) || flag;

        return new ScoringHead(bias, tokenIds, kindTemperature, headTemperature, isCalibrated);
    }

    /// <summary>Bias allenato per l'opzione (0 se lo slot non è canonico).</summary>
    public float BiasFor(DecisionOption option)
    {
        var slot = CanonicalSlot(option.Id) ?? CanonicalSlot(option.Display);
        return slot is not null && _bias.TryGetValue(slot, out var b) ? b : 0f;
    }

    /// <summary>Token id risolto dall'artefatto (se presente) per l'opzione.</summary>
    public int? TokenIdFor(DecisionOption option)
    {
        var slot = CanonicalSlot(option.Id) ?? CanonicalSlot(option.Display);
        return slot is not null && _tokenIds.TryGetValue(slot, out var id) ? id : null;
    }

    public float TemperatureFor(DecisionKind kind) => _kindTemperature[(int)kind] is > 0 and var t ? t : 1f;

    /// <summary>Applica bias/token-id/temperatura dell'artefatto alla richiesta. I valori già
    /// espliciti nell'opzione (bias != 0, temperatura != 1) NON vengono sovrascritti: l'artefatto
    /// è il default calibrato, il chiamante può sempre fare override.</summary>
    public DecisionRequest Apply(DecisionRequest request)
    {
        var options = new List<DecisionOption>(request.Options.Count);
        foreach (var option in request.Options)
        {
            var bias = option.Bias + BiasFor(option);
            var temperature = option.Temperature;
            if (Math.Abs(temperature - 1f) < 1e-6f)
                temperature = TemperatureFor(request.Kind);
            var tokenId = option.TokenId ?? TokenIdFor(option);
            options.Add(option with { Bias = bias, Temperature = temperature, TokenId = tokenId });
        }
        return request with { Options = options };
    }

    /// <summary>Confidenza canonica: `p_top − media(resto)`, clampata in [0,1].
    /// null con meno di due opzioni (non c'è un "resto" da confrontare).</summary>
    public static double? Confidence(IReadOnlyList<double> probabilities, int topIndex)
    {
        if (probabilities.Count < 2 || topIndex < 0 || topIndex >= probabilities.Count)
            return null;
        var top = probabilities[topIndex];
        var sum = 0.0;
        for (var i = 0; i < probabilities.Count; i++)
            if (i != topIndex) sum += probabilities[i];
        var meanRest = sum / (probabilities.Count - 1);
        return Math.Clamp(top - meanRest, 0.0, 1.0);
    }

    /// <summary>Softmax stabile (usata per la probabilità GREZZA, prima della calibrazione).</summary>
    public static double[] Softmax(IReadOnlyList<float> logprobs)
    {
        if (logprobs.Count == 0) return [];
        var max = double.NegativeInfinity;
        foreach (var lp in logprobs)
            if (lp > max) max = lp;
        var exp = new double[logprobs.Count];
        var sum = 0.0;
        for (var i = 0; i < logprobs.Count; i++)
        {
            exp[i] = Math.Exp(logprobs[i] - max);
            sum += exp[i];
        }
        if (sum <= 0 || double.IsNaN(sum)) return new double[logprobs.Count];
        for (var i = 0; i < exp.Length; i++) exp[i] /= sum;
        return exp;
    }

    private static void CollectVerbalizerIds(JsonNode? node, Dictionary<string, int> into)
    {
        switch (node)
        {
            case null:
                return;
            case JsonObject obj:
                foreach (var kv in obj)
                {
                    switch (kv.Value)
                    {
                        case JsonValue v when v.TryGetValue<int>(out var id):
                            into[CanonicalSlot(kv.Key) ?? kv.Key] = id;
                            break;
                        case JsonObject nested:
                            CollectVerbalizerIds(nested, into);
                            break;
                        case JsonArray arr:
                        {
                            var labels = LabelsForKind(kv.Key);
                            if (labels is null) break;
                            for (var i = 0; i < arr.Count && i < labels.Count; i++)
                                if (arr[i] is JsonValue iv && iv.TryGetValue<int>(out var nid))
                                    into[labels[i]] = nid;
                            break;
                        }
                    }
                }
                return;
            case JsonArray flat:
                for (var i = 0; i < flat.Count && i < SlotOrder.Count; i++)
                    if (flat[i] is JsonValue fv && fv.TryGetValue<int>(out var fid))
                        into[SlotOrder[i]] = fid;
                return;
            default:
                throw new DecisionProviderException("scoring_head.json: 'verbalizer_ids' non valido.");
        }
    }

    private static IReadOnlyList<string>? LabelsForKind(string kind) => kind.ToLowerInvariant() switch
    {
        "noul" or "bool" => ["false", "true"],
        "score" => ["0", "1", "2", "3", "4", "5"],
        "choice" => ["A", "B", "C", "D", "E", "F", "G", "H",
                     "I", "J", "K", "L", "M", "N", "O", "P"],
        _ => null,
    };

    private static int KindIndex(string kind) => kind.ToLowerInvariant() switch
    {
        "noul" or "bool" => 0,
        "choice" => 1,
        "score" => 2,
        _ => -1,
    };

    private static string? CanonicalSlot(string? key)
    {
        if (string.IsNullOrWhiteSpace(key)) return null;
        var k = key.Trim();
        foreach (var slot in SlotOrder)
            if (string.Equals(slot, k, StringComparison.OrdinalIgnoreCase))
                return slot;
        return null;
    }

    private static float? ReadFloat(JsonNode? node)
    {
        if (node is not JsonValue value) return null;
        if (value.TryGetValue<float>(out var f)) return f;
        if (value.TryGetValue<double>(out var d)) return (float)d;
        if (value.TryGetValue<string>(out var s)
            && float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
            return parsed;
        return null;
    }
}
