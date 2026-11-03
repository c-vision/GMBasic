// AI/Decision/TypesafeDecisionProvider.cs
//
// Provider REMOTO TypeSafe (JEV): POST JSON verso un endpoint HTTP che risponde con la
// decisione già calcolata. Contratto (stesso dei provider locali, §6 del piano JEV):
//
//   richiesta  {"state":"…","kind":"NOUL|CHOICE|SCORE",
//               "options":[{"id":"…","label":"…"}, …],
//               "instructions":"…", "model":"…"}
//   risposta   {"kind":"NOUL","option":"true","probability":0.93,
//               "confidence":0.8,"calibrated":0.9}
//
// `option` può essere l'id o la label di una delle opzioni inviate; `confidence`/`calibrated`
// sono opzionali/null. Qualunque errore HTTP, status non-2xx o JSON invalido è un errore
// ESPLICITO (niente fallback silenzioso). La policy resta applicativa.

using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Basic.Decision;

public sealed class TypesafeDecisionProvider : ISystemOneProvider
{
    private readonly HttpClient _http;
    private readonly string _url;
    private readonly string? _apiKey;
    private readonly string? _model;

    public TypesafeDecisionProvider(string baseUrl, string? apiKey = null, string? model = null,
        TimeSpan? timeout = null)
    {
        if (string.IsNullOrWhiteSpace(baseUrl))
            throw new DecisionProviderException("typesafe: baseUrl mancante.");
        if (!Uri.TryCreate(baseUrl, UriKind.Absolute, out var uri)
            || (uri.Scheme != Uri.UriSchemeHttp && uri.Scheme != Uri.UriSchemeHttps))
            throw new DecisionProviderException($"typesafe: baseUrl non valido ('{baseUrl}').");
        _url = baseUrl;
        _apiKey = string.IsNullOrWhiteSpace(apiKey) ? null : apiKey;
        _model = string.IsNullOrWhiteSpace(model) ? null : model;
        _http = new HttpClient { Timeout = timeout ?? TimeSpan.FromSeconds(30) };
    }

    public string Name => "typesafe";

    public DecisionCapabilities Capabilities =>
        DecisionCapabilities.Noul | DecisionCapabilities.Choice | DecisionCapabilities.Score |
        DecisionCapabilities.Probability | DecisionCapabilities.Confidence |
        DecisionCapabilities.Calibration;

    public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request,
        CancellationToken cancellationToken = default)
    {
        request.Validate();
        var options = new JsonArray();
        foreach (var o in request.Options)
            options.Add(new JsonObject { ["id"] = o.Id, ["label"] = o.Display });
        var payload = new JsonObject
        {
            ["state"] = request.State,
            ["kind"] = request.Kind switch
            {
                DecisionKind.Noul => "NOUL",
                DecisionKind.Score => "SCORE",
                _ => "CHOICE",
            },
            ["options"] = options,
        };
        if (!string.IsNullOrWhiteSpace(request.Instructions))
            payload["instructions"] = request.Instructions;
        if (_model is not null) payload["model"] = _model;

        string body;
        try
        {
            using var msg = new HttpRequestMessage(HttpMethod.Post, _url)
            {
                Content = new StringContent(payload.ToJsonString(), Encoding.UTF8, "application/json"),
            };
            if (_apiKey is not null)
                msg.Headers.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", _apiKey);
            using var response = _http.Send(msg, HttpCompletionOption.ResponseContentRead, cancellationToken);
            body = response.Content.ReadAsStringAsync(cancellationToken).GetAwaiter().GetResult();
            if (!response.IsSuccessStatusCode)
                throw new DecisionProviderException(
                    $"typesafe: HTTP {(int)response.StatusCode} {response.ReasonPhrase}: {Trim(body)}");
        }
        catch (DecisionProviderException) { throw; }
        catch (Exception ex)
        {
            throw new DecisionProviderException($"typesafe: richiesta HTTP fallita: {ex.Message}", ex);
        }

        JsonNode? root;
        try { root = JsonNode.Parse(body); }
        catch (JsonException ex)
        {
            throw new DecisionProviderException($"typesafe: risposta non JSON: {Trim(body)}", ex);
        }
        if (root is not JsonObject obj)
            throw new DecisionProviderException($"typesafe: risposta non è un oggetto JSON: {Trim(body)}");

        var optionText = obj["option"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(optionText))
            throw new DecisionProviderException($"typesafe: risposta senza 'option': {Trim(body)}");
        var probability = ReadDouble(obj["probability"])
            ?? throw new DecisionProviderException($"typesafe: risposta senza 'probability': {Trim(body)}");
        if (probability < 0 || probability > 1)
            throw new DecisionProviderException($"typesafe: probability fuori [0,1]: {probability}");

        var option = request.Options.FirstOrDefault(o =>
                         string.Equals(o.Id, optionText, StringComparison.OrdinalIgnoreCase))
                     ?? request.Options.FirstOrDefault(o =>
                         string.Equals(o.Display, optionText, StringComparison.OrdinalIgnoreCase));
        if (option is null)
            throw new DecisionProviderException(
                $"typesafe: option '{optionText}' non è nel dominio della richiesta.");

        if (obj["kind"]?.GetValue<string>() is { } responseKind)
        {
            var expected = request.Kind switch
            {
                DecisionKind.Noul => "NOUL",
                DecisionKind.Score => "SCORE",
                _ => "CHOICE",
            };
            if (!responseKind.Equals(expected, StringComparison.OrdinalIgnoreCase))
                throw new DecisionProviderException(
                    $"typesafe: kind '{responseKind}' incoerente con '{expected}'.");
        }

        var result = DecisionResult.FromOption(request, option, probability,
            confidence: ReadDouble(obj["confidence"]),
            calibrated: ReadDouble(obj["calibrated"]),
            raw: "typesafe:" + Trim(body));
        return [result];
    }

    private static double? ReadDouble(JsonNode? node)
    {
        if (node is not JsonValue value) return null;
        if (value.TryGetValue<double>(out var d)) return d;
        if (value.TryGetValue<string>(out var s)
            && double.TryParse(s, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var parsed))
            return parsed;
        return null;
    }

    private static string Trim(string s) => s.Length <= 300 ? s : s[..300] + "…";
}
