// FILE: Basic.SQLite.Vector.FixtureLlmCompleter
namespace Basic.AI.Rag;

/// <summary>Deterministic RAG completer for dev/xUnit — no network.</summary>
/// <remarks>ponytail: echoes top attribution snippet; upgrade path: OpenAI/Ollama ILlmCompleter.</remarks>
public sealed class FixtureLlmCompleter : ILlmCompleter, IStreamingLlmCompleter
{
    public string ModelName => "fixture-llm-v1";

    public string Complete(string systemPrompt, string userPrompt)
    {
        ArgumentNullException.ThrowIfNull(userPrompt);

        // Semantic extraction del GRAFO (GraphSemanticExtractor): il prompt chiede di dedurre
        // relazioni tra note "## titolo (path.md)". La fixture produce un JSON deterministico:
        // un edge "applies" tra le prime due note distinte trovate (per test offline).
        if (userPrompt.Contains("RELAZIONI SEMANTICHE", StringComparison.Ordinal))
        {
            var paths = System.Text.RegularExpressions.Regex.Matches(userPrompt, @"## .*?\(([^)]+\.md)\)")
                .Select(m => m.Groups[1].Value).Distinct().Take(2).ToList();
            if (paths.Count < 2) return "{\"edges\":[]}";
            return "{\"edges\":[{\"from\":\"" + paths[0].Replace("\"", "\\\"") +
                   "\",\"to\":\"" + paths[1].Replace("\"", "\\\"") +
                   "\",\"relation\":\"applies\",\"confidence\":0.85}]}";
        }

        // C7 (estrazione STRUTTURATA): il prompt contiene 'EXTRACT STRUCTURED SCHEMA: <json>'.
        // Il completer fixture produce un JSON con i campi dello schema — per la verifica
        // deterministica usa i valori reali della pagina quando riconoscibili (es. il titolo),
        // altrimenti un valore per campo in base al tipo dichiarato.
        var extractIdx = userPrompt.IndexOf("EXTRACT STRUCTURED", StringComparison.Ordinal);
        if (extractIdx >= 0)
            return ProduceStructuredJson(userPrompt);

        const string questionMarker = "Question: ";
        var question = userPrompt.Contains(questionMarker, StringComparison.Ordinal)
            ? userPrompt[(userPrompt.IndexOf(questionMarker, StringComparison.Ordinal) + questionMarker.Length)..]
                .Trim()
            : userPrompt.Trim();

        var snippet = ExtractFirstSnippet(userPrompt);
        return string.IsNullOrEmpty(snippet)
            ? $"[{question}] (insufficient context)"
            : $"[{question}] {snippet}";
    }

    /// <summary>C7: produce un JSON strutturato con i campi dello SCHEMA, popolato dal testo
    /// della pagina quando i valori sono riconoscibili (es. il titolo reale), altrimenti un
    /// valore per tipo (stringa: prima riga del testo; numero: conteggio righe).</summary>
    private static string ProduceStructuredJson(string prompt)
    {
        var schemaStart = prompt.IndexOf("SCHEMA: ", StringComparison.Ordinal);
        string schema = "";
        if (schemaStart >= 0)
        {
            var start = schemaStart + "SCHEMA: ".Length;
            var end = prompt.IndexOf(" INSTRUCTION:", start, StringComparison.Ordinal);
            if (end < 0) end = prompt.Length;
            schema = prompt[start..end].Trim();
        }
        var pageStart = prompt.IndexOf("PAGE: ", StringComparison.Ordinal);
        string pageText = pageStart >= 0 ? prompt[(pageStart + "PAGE: ".Length)..].Trim() : "";
        var firstLine = pageText.Split('\n').FirstOrDefault()?.Trim() ?? "";
        if (firstLine.Length > 60) firstLine = firstLine[..60];

        var sb = new System.Text.StringBuilder("{");
        var first = true;
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(schema);
            foreach (var prop in doc.RootElement.EnumerateObject())
            {
                if (!first) sb.Append(',');
                first = false;
                sb.Append(System.Text.Json.JsonSerializer.Serialize(prop.Name)).Append(':');
                var type = prop.Value.GetString() ?? "string";
                switch (type.ToLowerInvariant())
                {
                    case "number" or "integer" or "double":
                        sb.Append(pageText.Split('\n').Length);
                        break;
                    case "bool" or "boolean":
                        sb.Append(pageText.Length > 0 ? "true" : "false");
                        break;
                    case "object" or "array":
                        sb.Append(type == "array" ? "[]" : "{}");
                        break;
                    default:
                        sb.Append(System.Text.Json.JsonSerializer.Serialize(firstLine));
                        break;
                }
            }
        }
        catch
        {
            sb.Append("\"error\":\"schema non valido\"");
        }
        sb.Append('}');
        return sb.ToString();
    }

    public IEnumerable<string> CompleteStream(string systemPrompt, string userPrompt)
    {
        var answer = Complete(systemPrompt, userPrompt);
        foreach (var word in answer.Split(' '))
            yield return word + " ";
    }

    private static string ExtractFirstSnippet(string userPrompt)
    {
        const string scoreMarker = ") ";
        var idx = userPrompt.IndexOf(scoreMarker, StringComparison.Ordinal);
        if (idx < 0)
            return string.Empty;

        var start = idx + scoreMarker.Length;
        var end = userPrompt.IndexOf('\n', start);
        if (end < 0)
            end = userPrompt.Length;

        return userPrompt[start..end].Trim();
    }
}
