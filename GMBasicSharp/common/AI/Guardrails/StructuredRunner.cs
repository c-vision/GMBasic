// FILE: Basic.AI.Guardrails.StructuredRunner
// B3.3 — StructuredRunner: RUNSTRUCTURED(prompt, schema) validato con JSON Schema (Pydantic style)

using System.Text.Json;
using Basic.AI.Rag;

namespace Basic.AI.Guardrails;

public static class StructuredRunner
{
    /// <summary>Esegue prompt con schema JSON Schema e valida la risposta del LLM.
    /// Ritorna (ok, json, error). Se ok=false, json è il testo grezzo del LLM.</summary>
    public static (bool ok, string json, string? error) RunStructured(string prompt, string schemaJson, ILlmCompleter? completer = null)
    {
        if (string.IsNullOrWhiteSpace(prompt)) return (false, "", "Prompt vuoto");
        if (string.IsNullOrWhiteSpace(schemaJson)) return (false, "", "Schema vuoto");

        JsonDocument schema;
        try { schema = JsonDocument.Parse(schemaJson); }
        catch (Exception ex) { return (false, "", $"Schema JSON invalido: {ex.Message}"); }

        completer ??= LlmCompleterResolver.Default;
        if (completer == null) return (false, "", "Nessun provider LLM configurato (AI SET ACTIVE)");

        var system = "Sei un generatore JSON strutturato. Rispondi SOLO con un JSON valido che rispetta esattamente lo schema fornito. Niente testo fuori dal JSON.\n\nSchema:\n" + schemaJson;
        string raw;
        try { raw = completer.Complete(system, prompt); }
        catch (Exception ex) { return (false, "", $"LLM error: {ex.Message}"); }

        // Estrai JSON dal markdown se necessario
        var jsonStr = ExtractJson(raw);
        JsonDocument doc;
        try { doc = JsonDocument.Parse(jsonStr); }
        catch (Exception ex) { return (false, raw, $"Risposta non è JSON valido: {ex.Message}"); }

        var (valid, err) = Validate(doc.RootElement, schema.RootElement);
        if (!valid) return (false, jsonStr, err);
        return (true, jsonStr, null);
    }

    private static string ExtractJson(string raw)
    {
        var s = raw.Trim();
        var start = s.IndexOf('{');
        var end = s.LastIndexOf('}');
        if (start >= 0 && end > start) return s.Substring(start, end - start + 1);
        start = s.IndexOf('[');
        end = s.LastIndexOf(']');
        if (start >= 0 && end > start) return s.Substring(start, end - start + 1);
        return s;
    }

    private static (bool ok, string? error) Validate(JsonElement data, JsonElement schema)
    {
        // Validazione minimale JSON Schema (type, properties, required, enum)
        if (schema.TryGetProperty("type", out var typeEl) && typeEl.ValueKind == JsonValueKind.String)
        {
            var expected = typeEl.GetString();
            var actual = data.ValueKind switch
            {
                JsonValueKind.Object => "object",
                JsonValueKind.Array => "array",
                JsonValueKind.String => "string",
                JsonValueKind.Number => data.TryGetInt64(out _) ? "integer" : "number",
                JsonValueKind.True or JsonValueKind.False => "boolean",
                _ => "null"
            };
            if (!string.Equals(expected, actual, StringComparison.OrdinalIgnoreCase) && expected != "integer" && actual != "integer")
            {
                // integer è sottotipo di number
                if (!(expected == "number" && actual == "integer"))
                    return (false, $"Type mismatch: atteso {expected}, got {actual}");
            }
        }

        if (schema.TryGetProperty("required", out var reqEl) && reqEl.ValueKind == JsonValueKind.Array && data.ValueKind == JsonValueKind.Object)
        {
            foreach (var req in reqEl.EnumerateArray())
            {
                var prop = req.GetString();
                if (prop != null && !data.TryGetProperty(prop, out _))
                    return (false, $"Missing required property '{prop}'");
            }
        }

        if (schema.TryGetProperty("properties", out var propsEl) && propsEl.ValueKind == JsonValueKind.Object && data.ValueKind == JsonValueKind.Object)
        {
            foreach (var prop in propsEl.EnumerateObject())
            {
                if (data.TryGetProperty(prop.Name, out var val))
                {
                    var (ok, err) = Validate(val, prop.Value);
                    if (!ok) return (false, $"Property '{prop.Name}': {err}");
                }
            }
        }

        if (schema.TryGetProperty("enum", out var enumEl) && enumEl.ValueKind == JsonValueKind.Array)
        {
            var found = false;
            foreach (var ev in enumEl.EnumerateArray())
            {
                if (ev.GetRawText() == data.GetRawText()) { found = true; break; }
                if (ev.ValueKind == JsonValueKind.String && data.ValueKind == JsonValueKind.String && ev.GetString() == data.GetString()) { found = true; break; }
            }
            if (!found) return (false, $"Value {data.GetRawText()} not in enum {enumEl.GetRawText()}");
        }

        return (true, null);
    }
}
