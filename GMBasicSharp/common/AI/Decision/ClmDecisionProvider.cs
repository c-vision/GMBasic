// AI/Decision/ClmDecisionProvider.cs
//
// Provider `clm` (Contrastive Language Models) del primitivo `AI DECIDE`: stesso contratto
// SystemOne di TypeSafe/Jev ma wire `POST /v1/systemone` di CLM:
//
//   richiesta  {"state": <stringa|oggetto|array>,
//               "questions": {"q": {"type":"noul|choice|score","instructions":"…","criteria":…}}}
//   risposta   {"type":"choice","choice":"auth","confidence":0.6,"probabilities":{…}}
//              {"type":"noul","noul":0.83}   {"type":"score","score":…,"probabilities":{…}}
//
// Mirror managed di GMBasic/native/gmb_runtime_c/src/gmb_decision_clm.c (parità IL/PCode ↔ VM/C ↔
// C99Native). Il percorso LOCALE (encoder MLX last-token + head) vive solo nel runtime C e non è
// ancora cablato: senza `baseUrl` l'errore è ESPLICITO, mai un fallback silenzioso.

using System.Net.Http;
using System.Text;
using System.Text.Json.Nodes;

namespace Basic.Decision;

public sealed class ClmDecisionProvider : ISystemOneProvider
{
    private readonly HttpClient _http;
    private readonly string _url;
    private readonly string? _apiKey;
    private readonly string? _instructions;

    public ClmDecisionProvider(string baseUrl, string? apiKey = null, string? instructions = null,
        TimeSpan? timeout = null)
    {
        if (string.IsNullOrWhiteSpace(baseUrl))
            throw new DecisionProviderException("clm: baseUrl mancante nel config del provider %DECIDE");
        if (!Uri.TryCreate(baseUrl, UriKind.Absolute, out var uri)
            || (uri.Scheme != Uri.UriSchemeHttp && uri.Scheme != Uri.UriSchemeHttps))
            throw new DecisionProviderException($"clm: baseUrl non valido ('{baseUrl}').");
        _url = baseUrl.Contains("/v1/systemone", StringComparison.Ordinal)
            ? baseUrl
            : baseUrl.TrimEnd('/') + "/v1/systemone";
        _apiKey = string.IsNullOrWhiteSpace(apiKey) ? null : apiKey;
        _instructions = string.IsNullOrWhiteSpace(instructions) ? null : instructions;
        _http = new HttpClient { Timeout = timeout ?? TimeSpan.FromSeconds(15) };
    }

    public string Name => "clm";

    public DecisionCapabilities Capabilities =>
        DecisionCapabilities.Noul | DecisionCapabilities.Choice | DecisionCapabilities.Score |
        DecisionCapabilities.Probability | DecisionCapabilities.Confidence;

    // ── Prosa (to_text / state_text): le head sono addestrate su prosa, non su JSON ──────────

    /// <summary>Oggetto JSON → righe `chiave: valore`, array → `- elemento`, annidati indentati di 2
    /// spazi; una stringa non-JSON (o JSON malformato) torna invariata. Senza newline finale.</summary>
    public static string ToText(string state)
    {
        var t = (state ?? "").TrimStart();
        if (t.Length == 0 || (t[0] != '{' && t[0] != '[')) return state ?? "";
        JsonNode? root;
        try { root = JsonNode.Parse(state!); } catch { return state!; }
        if (root is not (JsonObject or JsonArray)) return state!;
        var sb = new StringBuilder();
        Render(root, 0, sb);
        if (sb.Length > 0 && sb[^1] == '\n') sb.Length--;
        return sb.ToString();
    }

    public static string StateText(string state, string? instructions)
    {
        var s = ToText(state);
        var i = instructions ?? "";
        return s.Length == 0 ? i : i.Length == 0 ? s : s + "\n\n" + i;
    }

    private static bool IsContainer(JsonNode? n) => n is JsonObject or JsonArray;

    private static string Scalar(JsonNode? n) =>
        n is null ? "null"
        : n is JsonValue v && v.TryGetValue<string>(out var s) ? s
        : n.ToJsonString();

    private static void Render(JsonNode node, int indent, StringBuilder sb)
    {
        var pad = new string(' ', indent);
        if (node is JsonObject obj)
        {
            foreach (var (k, v) in obj)
            {
                sb.Append(pad).Append(k);
                if (IsContainer(v)) { sb.Append(":\n"); Render(v!, indent + 2, sb); }
                else sb.Append(": ").Append(Scalar(v)).Append('\n');
            }
        }
        else if (node is JsonArray arr)
        {
            foreach (var v in arr)
            {
                if (IsContainer(v)) { sb.Append(pad).Append("-\n"); Render(v!, indent + 2, sb); }
                else sb.Append(pad).Append("- ").Append(Scalar(v)).Append('\n');
            }
        }
    }

    // ── Wire ────────────────────────────────────────────────────────────────────────────────

    /// <summary>Corpo della richiesta per UNA domanda (id "q").</summary>
    public static string BuildRequest(DecisionRequest request, string? instructions)
    {
        JsonNode stateNode;
        var t = request.State.TrimStart();
        JsonNode? parsed = null;
        if (t.Length > 0 && (t[0] == '{' || t[0] == '['))
        {
            try { parsed = JsonNode.Parse(request.State); } catch { parsed = null; }
        }
        stateNode = parsed is JsonObject or JsonArray ? parsed : JsonValue.Create(request.State)!;

        var q = new JsonObject
        {
            ["type"] = request.Kind switch
            {
                DecisionKind.Choice => "choice",
                DecisionKind.Score => "score",
                _ => "noul",
            },
            ["instructions"] = instructions ?? "",
        };
        if (request.Kind == DecisionKind.Choice)
        {
            var c = new JsonObject();
            foreach (var o in request.Options)
                c[o.Id] = string.Equals(o.Id, o.Display, StringComparison.Ordinal) ? "" : o.Display;
            q["criteria"] = c;
        }
        else if (request.Kind == DecisionKind.Score)
        {
            var a = new JsonArray();
            foreach (var o in request.Options) a.Add(o.Display);
            q["criteria"] = a;
        }
        var root = new JsonObject
        {
            ["state"] = stateNode,
            ["questions"] = new JsonObject { ["q"] = q },
        };
        return root.ToJsonString();
    }

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request,
        CancellationToken cancellationToken = default)
    {
        request.Validate();
        var payload = BuildRequest(request, _instructions ?? request.Instructions);
        string body;
        try
        {
            using var msg = new HttpRequestMessage(HttpMethod.Post, _url)
            {
                Content = new StringContent(payload, Encoding.UTF8, "application/json"),
            };
            if (_apiKey is not null)
                msg.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", _apiKey);
            using var response = _http.Send(msg, HttpCompletionOption.ResponseContentRead, cancellationToken);
            body = response.Content.ReadAsStringAsync(cancellationToken).GetAwaiter().GetResult();
            if (!response.IsSuccessStatusCode)
                throw new DecisionProviderException($"clm: HTTP {(int)response.StatusCode}");
        }
        catch (DecisionProviderException) { throw; }
        catch (Exception ex)
        {
            throw new DecisionProviderException($"clm: HTTP fallito: {ex.Message}", ex);
        }
        return [ParseResponse(body, request)];
    }

    /// <summary>Risposta → risultato. Accetta la risposta della domanda sia al livello superiore sia
    /// sotto l'id "q". Errore ESPLICITO su tipo incoerente, opzione fuori dominio, probabilità mancanti.</summary>
    public static DecisionResult ParseResponse(string body, DecisionRequest request)
    {
        if (string.IsNullOrWhiteSpace(body)) throw new DecisionProviderException("clm: risposta vuota");
        JsonNode? root;
        try { root = JsonNode.Parse(body); }
        catch (Exception ex) { throw new DecisionProviderException("clm: risposta non JSON", ex); }
        var ans = FindAnswer(root) ?? throw new DecisionProviderException("clm: risposta senza oggetto 'type'");
        var type = (ans["type"] as JsonValue)?.TryGetValue<string>(out var ts) == true ? ts : null;
        var want = request.Kind switch { DecisionKind.Noul => "noul", DecisionKind.Score => "score", _ => "choice" };
        if (type is null) throw new DecisionProviderException("clm: campo 'type' non valido");
        if (!type.Equals(want, StringComparison.OrdinalIgnoreCase))
            throw new DecisionProviderException($"clm: type '{type}' incoerente (atteso {want})");

        if (request.Kind == DecisionKind.Noul)
        {
            var pt = Num(ans["noul"]);
            if (pt is null || pt < 0 || pt > 1)
                throw new DecisionProviderException("clm: noul assente o fuori [0,1]");
            var opt = request.Options.FirstOrDefault(o => o.Id.Equals(pt >= 0.5 ? "true" : "false", StringComparison.OrdinalIgnoreCase))
                      ?? throw new DecisionProviderException("clm: dominio NOUL senza true/false");
            var top = pt >= 0.5 ? pt.Value : 1.0 - pt.Value;
            return DecisionResult.FromOption(request, opt, top, confidence: Math.Clamp(2 * top - 1, 0, 1));
        }

        if (ans["probabilities"] is not JsonObject probs)
            throw new DecisionProviderException("clm: risposta senza 'probabilities'");
        double? P(DecisionOption o) => Num(probs[o.Id]) ?? Num(probs[o.Display]);
        DecisionOption? best = null;
        double bp = -1;
        if (request.Kind == DecisionKind.Choice)
        {
            var ck = (ans["choice"] as JsonValue)?.TryGetValue<string>(out var cs) == true ? cs : null;
            if (ck is null) throw new DecisionProviderException("clm: risposta senza 'choice'");
            best = request.Options.FirstOrDefault(o => o.Id.Equals(ck, StringComparison.OrdinalIgnoreCase))
                   ?? request.Options.FirstOrDefault(o => o.Display.Equals(ck, StringComparison.OrdinalIgnoreCase))
                   ?? throw new DecisionProviderException($"clm: option non nel dominio: {ck}");
            bp = P(best) ?? -1;
        }
        else
        {
            foreach (var o in request.Options)
            {
                var p = P(o) ?? -1;
                if (p > bp) { bp = p; best = o; }
            }
        }
        if (best is null || bp < 0 || bp > 1)
            throw new DecisionProviderException("clm: probabilities incomplete o fuori [0,1]");
        var conf = Num(ans["confidence"]);
        if (conf is null or < 0)
        {
            var ps = request.Options.Select(o => Math.Max(0, P(o) ?? 0)).ToArray();
            conf = Confidence(ps);
        }
        return DecisionResult.FromOption(request, best, bp, confidence: conf);
    }

    /// <summary>confidence = max(0, min(1, p_top − media(resto))); 1.0 con meno di 2 opzioni.</summary>
    public static double Confidence(IReadOnlyList<double> p)
    {
        if (p.Count < 2) return 1.0;
        var top = 0;
        for (var i = 1; i < p.Count; i++) if (p[i] > p[top]) top = i;
        double rest = 0;
        for (var i = 0; i < p.Count; i++) if (i != top) rest += p[i];
        return Math.Clamp(p[top] - rest / (p.Count - 1), 0, 1);
    }

    private static JsonObject? FindAnswer(JsonNode? root)
    {
        if (root is not JsonObject o) return null;
        if (o.ContainsKey("type")) return o;
        foreach (var (_, v) in o)
            if (v is JsonObject inner && inner.ContainsKey("type")) return inner;
        return null;
    }

    private static double? Num(JsonNode? n)
    {
        if (n is not JsonValue v) return null;
        if (v.TryGetValue<double>(out var d)) return d;
        if (v.TryGetValue<string>(out var s)
            && double.TryParse(s, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var parsed)) return parsed;
        return null;
    }
}
