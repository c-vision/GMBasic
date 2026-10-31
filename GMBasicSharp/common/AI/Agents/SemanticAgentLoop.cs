// Agents/SemanticAgentLoop.cs — §4.10: tool `code.*` per gli agenti sul SEMANTIC GRAPH del codice.
// Speculare a RagAgentLoop (rag.*) e GraphAgentLoop (graph.*): qui l'agente interroga la STRUTTURA
// del codice BASIC (simboli, chiamate, impatto) di un file `.bas`.
//
// Implementazione C# INDIPENDENTE (precedente PARSEC): riusa la logica del grafo C# già usata dalla
// superficie BASIC `LLM.SYMBOL/IMPACT/CALLMAP/…` (SemanticQueryHook via Linker.SemanticQueryFolder:
// UNA sola sorgente del grafo C#). Il gemello C (VM/C, C99Native) usa `gmb_semgraph` in
// `gmb_cua_execute` — nessun P/Invoke, nessuna chiamata C#↔C.
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.Mcp;
using Basic.Pipeline;
using GMBasic.SemanticGraph;

namespace Basic.Agents;

/// <summary>Strumenti code.* esposti agli agenti (query sul grafo semantico di un file BASIC).</summary>
public static class SemanticAgentLoop
{
    public static readonly (string Name, string Description)[] Tools =
    {
        ("code.symbol", "Definizione di un simbolo del file: '<kind> <label>' (es. 'procedure Foo')."),
        ("code.callmap", "Callers/callees DIRETTI di un simbolo: 'callers=[...] callees=[...]'."),
        ("code.impact", "Impatto transitivo (chi è impattato da una modifica del simbolo), fino a 'depth': 'nome@depth,...'."),
        ("code.context", "Contesto del simbolo: esso + i vicini (callers/callees) fino a 'depth'."),
        ("code.findsymbol", "Cerca i simboli il cui nome contiene 'query' (case-insensitive)."),
        ("code.fileoutline", "Outline del file: simboli DEFINITI, in ordine di linea ('kind label;...')."),
        ("code.repomap", "Mappa completa delle chiamate del file: 'caller->callee;...'."),
    };

    /// <summary>Definizioni McpTool degli strumenti code (procedura = nome tool).</summary>
    public static IEnumerable<McpToolDefinition> Definitions
    {
        get
        {
            foreach (var (n, d) in Tools)
            {
                var props = new JsonObject
                {
                    ["path"] = new JsonObject { ["type"] = "string", ["description"] = "File .bas da analizzare" },
                };
                if (n is "code.symbol" or "code.callmap" or "code.impact" or "code.context")
                    props["name"] = new JsonObject { ["type"] = "string", ["description"] = "Nome del simbolo" };
                if (n == "code.findsymbol")
                    props["query"] = new JsonObject { ["type"] = "string", ["description"] = "Sottostringa del nome" };
                if (n is "code.impact" or "code.context")
                    props["depth"] = new JsonObject { ["type"] = "integer", ["description"] = "Profondità (default 32)" };
                yield return new McpToolDefinition
                {
                    Name = n, Description = d, QualifiedProcedureName = n,
                    InputSchema = new JsonObject { ["type"] = "object", ["properties"] = props, ["required"] = new JsonArray("path") },
                };
            }
        }
    }

    /// <summary>Registra gli strumenti code.* nel registry (dedup per nome).</summary>
    public static void Register()
    {
        foreach (var t in Definitions)
            if (McpToolRegistry.Instance.Get(t.Name) is null)
                McpToolRegistry.Instance.Register(t);
    }

    /// <summary>Istruzioni per il modello: elenco strumenti + formato JSON tool_calls.</summary>
    public static string ToolInstructions()
    {
        var sb = new StringBuilder();
        sb.AppendLine("Strumenti code.* per interrogare il grafo semantico di un file BASIC (parametro 'path' = file .bas).");
        sb.AppendLine("Per usarne uno rispondi SOLO con il JSON {\"tool_calls\":[{\"name\":\"<tool>\",\"arguments\":{...}}]}, altrimenti rispondi con la risposta finale in testo libero.");
        foreach (var (n, d) in Tools)
            sb.AppendLine($"- {n}: {d}");
        sb.AppendLine("Esempio: {\"tool_calls\":[{\"name\":\"code.impact\",\"arguments\":{\"path\":\"main.bas\",\"name\":\"Calcola\",\"depth\":4}}]}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>Dispatcher MCP che esegue i tool code.* lato C#.</summary>
    public sealed class Dispatcher : IMcpToolDispatcher
    {
        public Task<McpToolCallResult> DispatchAsync(string qualifiedProcedureName, string argumentsJson, CancellationToken cancellationToken)
        {
            var text = Execute(qualifiedProcedureName, argumentsJson);
            return Task.FromResult(new McpToolCallResult(text.StartsWith("error:", StringComparison.Ordinal), text));
        }
    }

    /// <summary>Esegue un tool code.*: compila il file BASIC, ne estrae il grafo semantico e interroga.</summary>
    internal static string Execute(string name, string argsJson)
    {
        string? Arg(string key)
        {
            try
            {
                using var doc = JsonDocument.Parse(argsJson);
                return doc.RootElement.TryGetProperty(key, out var el) && el.ValueKind == JsonValueKind.String ? el.GetString() : null;
            }
            catch { return null; }
        }
        int ArgInt(string key, int def)
        {
            try
            {
                using var doc = JsonDocument.Parse(argsJson);
                if (doc.RootElement.TryGetProperty(key, out var el) && el.ValueKind == JsonValueKind.Number)
                    return el.GetInt32() > 0 ? el.GetInt32() : def;
            }
            catch { }
            return def;
        }

        var path = Arg("path") ?? Arg("file") ?? "";
        if (path.Length == 0) return "error: manca 'path' (file .bas)";
        if (!File.Exists(path)) return $"error: file non trovato '{path}'";

        TokenKind kind; string arg = "";
        switch (name)
        {
            case "code.symbol":      kind = TokenKind.AiSymbol;      arg = Arg("name") ?? ""; if (arg.Length == 0) return "error: manca 'name'"; break;
            case "code.callmap":     kind = TokenKind.AiCallmap;     arg = Arg("name") ?? ""; if (arg.Length == 0) return "error: manca 'name'"; break;
            case "code.impact":      kind = TokenKind.AiImpact;      arg = Arg("name") ?? ""; if (arg.Length == 0) return "error: manca 'name'"; break;
            case "code.context":     kind = TokenKind.AiContext;     arg = Arg("name") ?? ""; if (arg.Length == 0) return "error: manca 'name'"; break;
            case "code.findsymbol":  kind = TokenKind.AiFindsymbol;  arg = Arg("query") ?? Arg("name") ?? ""; if (arg.Length == 0) return "error: manca 'query'"; break;
            case "code.fileoutline": kind = TokenKind.AiFileoutline; arg = Path.GetFileName(path); break;
            case "code.repomap":     kind = TokenKind.AiRepomap;     break;
            default: return $"error: tool code '{name}' non riconosciuto";
        }
        int depth = ArgInt("depth", 32);

        try
        {
            var text = File.ReadAllText(path);
            SemanticSurface.EnsureRegistered();   // installa Linker.SemanticQueryFolder (logica grafo C#)
            var (_, program) = SourcePipeline.CompileFromStringWithRegistry(
                text, Path.GetFileName(path), Basic.StdLib.Instance, SourcePipeline.DefaultNamespacesStandard);
            var folder = Basic.Linker.SemanticQueryFolder;
            var res = folder?.Invoke(program, kind, arg, depth) ?? "";
            if (res.Length == 0) return "error: nessun risultato" + (name == "code.symbol" ? $" per il simbolo '{arg}'" : "");
            return res;
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }
}
