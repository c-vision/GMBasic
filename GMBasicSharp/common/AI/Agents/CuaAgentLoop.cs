// Agents/CuaAgentLoop.cs — tool CUA + dispatcher per il loop Brain→Azione NATIVO C#.
// Le azioni browser.*/tool.search sono REALI (CDP via BrowserRuntime); ui.*/computer.*
// danno errore esplicito (richiedono AGENT.RUN su VM/C). Riusa AgentToolLoop.AskWithTools
// (sessione KV persistente per i completer session-aware, es. MLX locale).
using System.Text.Json.Nodes;
using Basic.AI.Browser;
using Basic.Mcp;

namespace Basic.Agents;

public static class CuaAgentLoop
{
    public static readonly (string Name, string Description)[] Tools =
    {
        ("browser.navigate", "Navigate the browser to a URL"),
        ("browser.snapshot", "Snapshot (DOM JSON) of the current page"),
        ("browser.evaluate", "Evaluate a JavaScript expression in the page"),
        ("browser.click", "Click an element by CSS selector"),
        ("browser.fill", "Fill a form field by CSS selector"),
        ("tool.search", "Search available tools by keyword"),
        ("ui.find", "Find a native UI element by text (richiede VM/C: AX)"),
        ("computer.screenshot", "Take a screenshot (richiede VM/C)"),
    };

    /// <summary>Definizioni McpTool degli strumenti CUA (procedura = nome tool).</summary>
    public static IEnumerable<McpToolDefinition> Definitions
    {
        get
        {
            foreach (var (n, d) in Tools)
            {
                var schema = new JsonObject { ["type"] = "object", ["properties"] = new JsonObject(), ["additionalProperties"] = false };
                yield return new McpToolDefinition { Name = n, Description = d, QualifiedProcedureName = n, InputSchema = schema };
            }
        }
    }

    /// <summary>Registra gli strumenti CUA nel registry (dedup per nome).</summary>
    public static void Register()
    {
        foreach (var t in Definitions)
            if (McpToolRegistry.Instance.Get(t.Name) is null)
                McpToolRegistry.Instance.Register(t);
    }

    /// <summary>Istruzioni per il modello: elenco strumenti + formato JSON tool_calls.</summary>
    public static string ToolInstructions()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("Strumenti disponibili. Per usarne uno rispondi SOLO con il JSON {\"tool_calls\":[{\"name\":\"<tool>\",\"arguments\":{...}}]}, altrimenti rispondi con la risposta finale in testo libero.");
        foreach (var (n, d) in Tools)
            sb.AppendLine($"- {n}: {d}");
        sb.AppendLine("Esempio: {\"tool_calls\":[{\"name\":\"browser.navigate\",\"arguments\":{\"url\":\"https://example.com\"}}]}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>Dispatcher che esegue le azioni CUA lato C# (reali per browser/tool.search).</summary>
    public sealed class Dispatcher : IMcpToolDispatcher
    {
        public Task<McpToolCallResult> DispatchAsync(string qualifiedProcedureName, string argumentsJson, CancellationToken cancellationToken)
        {
            var text = Execute(qualifiedProcedureName, argumentsJson);
            return Task.FromResult(new McpToolCallResult(text.StartsWith("error:", StringComparison.Ordinal), text));
        }
    }

    internal static string Execute(string name, string argsJson)
    {
        string? Arg(string key)
        {
            try
            {
                using var doc = System.Text.Json.JsonDocument.Parse(argsJson);
                return doc.RootElement.TryGetProperty(key, out var el) ? el.ToString() : null;
            }
            catch { return null; }
        }

        try
        {
            switch (name)
            {
                case "browser.navigate":
                {
                    var r = BrowserRuntime.NavigateAsync(Arg("url") ?? "").GetAwaiter().GetResult();
                    return r.Success ? r.Data ?? "navigated" : "error: " + r.Error;
                }
                case "browser.snapshot":
                {
                    var r = BrowserRuntime.SnapshotAsync().GetAwaiter().GetResult();
                    return r.Success ? (r.Data ?? "").Length.ToString() + " bytes DOM" : "error: " + r.Error;
                }
                case "browser.evaluate":
                {
                    var r = BrowserRuntime.EvaluateAsync(Arg("expression") ?? "").GetAwaiter().GetResult();
                    return r.Success ? r.Data ?? "null" : "error: " + r.Error;
                }
                case "browser.click":
                {
                    var r = BrowserRuntime.ClickAsync(Arg("selector") ?? "").GetAwaiter().GetResult();
                    return r.Success ? "clicked" : "error: " + r.Error;
                }
                case "browser.fill":
                {
                    var r = BrowserRuntime.FillAsync(Arg("selector") ?? "", Arg("value") ?? "").GetAwaiter().GetResult();
                    return r.Success ? "filled" : "error: " + r.Error;
                }
                case "tool.search":
                    return ToolSearchNative.Search(Arg("query") ?? "");
                default:
                    return $"error: tool '{name}' non disponibile su questo host C# (usa AGENT.RUN su VM/C per ui.*/computer.*)";
            }
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }
}
