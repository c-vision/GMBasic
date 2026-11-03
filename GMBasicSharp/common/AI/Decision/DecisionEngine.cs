// AI/Decision/DecisionEngine.cs
//
// Risoluzione UNICA del provider di decisione per i consumer che vivono nel progetto AI
// (agente/router): provider esplicito (rules/mock/local/typesafe) oppure ruolo %DECIDE attivo.
// La superficie BASIC (IlRuntime) mantiene il proprio percorso di serializzazione JSON, ma la
// semantica di selezione è la stessa.

using Basic.AI.Rag;
using Basic.Providers;

namespace Basic.Decision;

public static class DecisionEngine
{
    public static DecisionResult Decide(DecisionRequest request, string? provider = null)
    {
        request.Validate();
        var p = (provider ?? "").Trim().ToLowerInvariant();
        string? decideName = null;
        if (p.Length == 0)
        {
            decideName = ProviderServer.Instance.GetActiveName(ProviderRole.Decide);
            if (!string.IsNullOrWhiteSpace(decideName))
            {
                var kind = ProviderServer.Instance.GetKind(decideName);
                if (!string.IsNullOrWhiteSpace(kind)) p = kind.Trim().ToLowerInvariant();
            }
        }

        if (p == "local")
        {
            ILlmCompleter? completer = null;
            try { completer = ProviderServer.Instance.GetActiveCompletion(); }
            catch { /* nessun provider attivo */ }
            if (completer is not IDecisionCompleter decisionCompleter)
                throw new DecisionProviderException(
                    "local: local provider non disponibile: nessun modello MLX attivo");
            var results = decisionCompleter.Decide(request);
            if (results.Count == 0)
                throw new DecisionProviderException("local: nessun risultato");
            return results.OrderByDescending(r => r.Probability).First();
        }

        if (p == "typesafe")
        {
            decideName ??= ProviderServer.Instance.GetActiveName(ProviderRole.Decide);
            if (string.IsNullOrWhiteSpace(decideName))
                throw new DecisionProviderException(
                    "typesafe: nessun provider %DECIDE attivo con config baseUrl.");
            var cfg = ProviderServer.Instance.GetConfigMap(decideName);
            cfg.TryGetValue("baseUrl", out var baseUrl);
            cfg.TryGetValue("apiKey", out var apiKey);
            cfg.TryGetValue("model", out var model);
            var timeout = TimeSpan.FromSeconds(30);
            if (cfg.TryGetValue("timeoutMs", out var timeoutText)
                && int.TryParse(timeoutText, out var timeoutMs) && timeoutMs > 0)
                timeout = TimeSpan.FromMilliseconds(timeoutMs);
            return new TypesafeDecisionProvider(baseUrl ?? "", apiKey, model, timeout)
                .Evaluate(request)[0];
        }

        ISystemOneProvider providerImpl = p switch
        {
            "" or "rules" => new RuleDecisionProvider(),
            "mock" => new MockDecisionProvider(),
            _ => throw new DecisionProviderException(
                $"provider '{provider}' non valido (rules/mock/local/typesafe)"),
        };
        return providerImpl.Evaluate(request)[0];
    }
}
