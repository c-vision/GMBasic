// FILE: Basic.Agents.FixtureToolCallingCompleter
using Basic.SQLite.Vector;
using Basic.AI.Rag;

namespace Basic.Agents;

/// <summary>Deterministic completer that emits structured tool calls for xUnit/dev.</summary>
/// <remarks>ponytail: magic prompt markers; upgrade path: native provider tool_calls API.</remarks>
public sealed class FixtureToolCallingCompleter : ILlmCompleter
{
    public string ModelName => "fixture-tool-calling-v1";

    public string Complete(string systemPrompt, string userPrompt)
    {
        ArgumentNullException.ThrowIfNull(userPrompt);

        if (userPrompt.Contains("tool:", StringComparison.OrdinalIgnoreCase) &&
            systemPrompt.Contains("add_numbers", StringComparison.OrdinalIgnoreCase) &&
            !userPrompt.Contains("[add_numbers]", StringComparison.Ordinal))
        {
            return """{"tool_calls":[{"id":"1","name":"add_numbers","arguments":{"a":2,"b":2}}]}""";
        }

        if (userPrompt.Contains("[add_numbers]", StringComparison.Ordinal))
            return "The sum is 4.";

        // Tool di un PLUGIN (web-fetch, registrato da PluginRegistry.Enable): round-trip
        // agente→tool→procedura del plugin (criterio sezione 10.6.5 del piano AI-Plugins).
        if (userPrompt.Contains("tool:", StringComparison.OrdinalIgnoreCase) &&
            systemPrompt.Contains("web_fetch", StringComparison.OrdinalIgnoreCase) &&
            !userPrompt.Contains("[web_fetch]", StringComparison.Ordinal))
        {
            return """{"tool_calls":[{"id":"2","name":"web_fetch","arguments":{"url":"https://example.com"}}]}""";
        }

        if (userPrompt.Contains("[web_fetch]", StringComparison.Ordinal))
            return "Page fetched: example.com";


        // Tool D3 (rete/auth): l'agente imposta un cookie (browser_set_cookie) — il risultato
        // conferma il dispatch agente→tool→procedura del plugin browser.
        if (userPrompt.Contains("tool: cookie", StringComparison.OrdinalIgnoreCase) &&
            systemPrompt.Contains("browser_set_cookie", StringComparison.OrdinalIgnoreCase) &&
            !userPrompt.Contains("[browser_set_cookie]", StringComparison.Ordinal))
        {
            return """{"tool_calls":[{"id":"5","name":"browser_set_cookie","arguments":{"name":"session","value":"agent-cookie","url":"http://127.0.0.1:8099","port":9222}}]}""";
        }

        if (userPrompt.Contains("[browser_set_cookie]", StringComparison.Ordinal))
            return "Cookie impostato: session=agent-cookie";

        // Tool del plugin BROWSER (B5 del piano automazione): round-trip agente→browser_navigate
        // →browser_text→risposta basata sul TESTO ESTRATTO dalla pagina reale (CDP). Il sistema
        // prompt del tool-loop elenca i tool registrati; il marker 'tool:' attiva la sequenza.
        if (userPrompt.Contains("tool:", StringComparison.OrdinalIgnoreCase) &&
            systemPrompt.Contains("browser_navigate", StringComparison.OrdinalIgnoreCase) &&
            !userPrompt.Contains("[browser_navigate]", StringComparison.Ordinal))
        {
            return """{"tool_calls":[{"id":"3","name":"browser_navigate","arguments":{"url":"https://example.com","port":9222}}]}""";
        }

        if (userPrompt.Contains("[browser_navigate]", StringComparison.Ordinal) &&
            systemPrompt.Contains("browser_text", StringComparison.OrdinalIgnoreCase) &&
            !userPrompt.Contains("[browser_text]", StringComparison.Ordinal))
        {
            return """{"tool_calls":[{"id":"4","name":"browser_text","arguments":{"port":9222}}]}""";
        }

        if (userPrompt.Contains("[browser_text]", StringComparison.Ordinal))
        {
            // La risposta DEVE contenere il dato estratto dalla pagina reale: legge il risultato
            // del tool browser_text dal transcript (il loop lo inserisce come '[browser_text] <testo>').
            var marker = "[browser_text]";
            var idx = userPrompt.IndexOf(marker, StringComparison.Ordinal);
            var after = idx >= 0 ? userPrompt[(idx + marker.Length)..].Trim() : "";
            var snippet = string.IsNullOrEmpty(after) ? "" : after.Split('\n')[0].Trim();
            if (snippet.Length > 0)
                return "La pagina contiene: " + snippet;
            return "Navigazione completata (testo non estratto).";
        }

        return new FixtureLlmCompleter().Complete(systemPrompt, userPrompt);
    }
}
