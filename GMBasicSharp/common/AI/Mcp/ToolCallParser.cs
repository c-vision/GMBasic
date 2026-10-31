// FILE: Basic.Mcp.ToolCallParser
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Basic.Mcp;

/// <summary>Parse structured tool-call responses from LLM output.</summary>
public static class ToolCallParser
{
    private static readonly JsonSerializerOptions JsonOptions = new() { PropertyNameCaseInsensitive = true };

    /// <summary>Try to extract tool calls from assistant text (JSON object or fenced JSON).</summary>
    public static IReadOnlyList<ToolCallRequest>? TryParse(string assistantText)
    {
        ArgumentNullException.ThrowIfNull(assistantText);
        var trimmed = assistantText.Trim();
        if (trimmed.StartsWith("```", StringComparison.Ordinal))
        {
            var start = trimmed.IndexOf('\n');
            var end = trimmed.LastIndexOf("```", StringComparison.Ordinal);
            if (start >= 0 && end > start)
                trimmed = trimmed[(start + 1)..end].Trim();
        }

        if (!trimmed.StartsWith("{", StringComparison.Ordinal))
            return null;

        try
        {
            var node = JsonNode.Parse(trimmed) as JsonObject;
            if (node?.TryGetPropertyValue("tool_calls", out var callsNode) != true || callsNode is not JsonArray calls)
                return null;

            var result = new List<ToolCallRequest>();
            foreach (var call in calls)
            {
                if (call is not JsonObject obj)
                    continue;

                // Formato OpenAI/Anthropic moderno: {"function":{"name":...,"arguments":"..."}}
                // Formato legacy: {"name":..., "arguments":...} oppure {"function":"nome", "arguments":...}
                var name = obj["name"]?.GetValue<string>();
                var argsNode = obj["arguments"];
                if (string.IsNullOrWhiteSpace(name) && obj.TryGetPropertyValue("function", out var fn))
                {
                    if (fn is JsonObject fnObj)
                    {
                        name = fnObj["name"]?.GetValue<string>();
                        argsNode ??= fnObj["arguments"];
                    }
                    else
                    {
                        name = fn!.GetValue<string>();
                    }
                }
                if (string.IsNullOrWhiteSpace(name))
                    continue;

                var id = obj["id"]?.GetValue<string>() ?? Guid.NewGuid().ToString("N");
                var args = argsNode?.ToJsonString() ?? "{}";
                result.Add(new ToolCallRequest(id, name, args));
            }

            return result.Count > 0 ? result : null;
        }
        catch (JsonException)
        {
            return null;
        }
    }
}

/// <summary>Single tool invocation requested by the model.</summary>
public sealed record ToolCallRequest(string Id, string Name, string ArgumentsJson);
