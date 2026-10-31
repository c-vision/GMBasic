// Agents/RagAgentLoop.cs — tool RAG per gli agenti (Second Brain).
// Espone il retrieval vettoriale multi-brain (MultiBrainSearch: fuse RRF con provenance
// brain) come tool `rag.*` invocabili da un agente BASIC o da un client MCP. Il dispatch è
// NATIVO C# verso Basic.AI.Rag.MultiBrainSearch (nessuna procedura BASIC richiesta, nessun
// passaggio dal GPM: è sola lettura dell'indice, non codice BASIC).
//
// Complementare a GraphAgentLoop (grafo): qui l'agente interroga il CONTENUTO (chunk
// vettoriali dei brain usati), lì la STRUTTURA (nodi/edge). Insieme chiudono il Second Brain
// al loop agente: retrieval semantico per la risposta, grafo per le relazioni.
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.Mcp;

namespace Basic.Agents;

/// <summary>Strumenti rag.* esposti agli agenti (search/context/ask sul retrieval vettoriale
/// multi-brain del Second Brain). Ogni tool ritorna JSON o testo grounded.</summary>
public static class RagAgentLoop
{
    public static readonly (string Name, string Description)[] Tools =
    {
        ("rag.search", "Cerca chunk rilevanti nei brain RAG collegati (multi-brain, fuse RRF) e ritorna JSON {status,hits:[{sourceId,text,score,brain,rrf}]}"),
        ("rag.context", "Contesto grounded aggregato (testi + attribuzioni sourceId/brain) per una query — pronto da iniettare nel prompt"),
        ("rag.ask", "Risposta grounded alla domanda: retrieval sui brain + completamento LLM sul contesto (richiede un completer %CHAT attivo)"),
    };

    /// <summary>Definizioni McpTool degli strumenti rag (procedura = nome tool).</summary>
    public static IEnumerable<McpToolDefinition> Definitions
    {
        get
        {
            foreach (var (n, d) in Tools)
            {
                var schema = new JsonObject
                {
                    ["type"] = "object",
                    ["properties"] = new JsonObject(),
                    ["additionalProperties"] = false,
                };
                yield return new McpToolDefinition { Name = n, Description = d, QualifiedProcedureName = n, InputSchema = schema };
            }
        }
    }

    /// <summary>Registra gli strumenti rag nel registry (dedup per nome).</summary>
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
        sb.AppendLine("Strumenti rag.* per interrogare i brain RAG del Second Brain (collega i brain con AI BRAIN USE; la ricerca è multi-brain con provenance).");
        sb.AppendLine("Per usarne uno rispondi SOLO con il JSON {\"tool_calls\":[{\"name\":\"<tool>\",\"arguments\":{...}}]}, altrimenti rispondi con la risposta finale in testo libero.");
        foreach (var (n, d) in Tools)
            sb.AppendLine($"- {n}: {d}");
        sb.AppendLine("Esempio: {\"tool_calls\":[{\"name\":\"rag.search\",\"arguments\":{\"query\":\"risk parity\",\"topK\":5}}]}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>Dispatcher MCP che esegue i tool rag lato C#.</summary>
    public sealed class Dispatcher : IMcpToolDispatcher
    {
        public Task<McpToolCallResult> DispatchAsync(string qualifiedProcedureName, string argumentsJson, CancellationToken cancellationToken)
        {
            var text = Execute(qualifiedProcedureName, argumentsJson);
            return Task.FromResult(new McpToolCallResult(text.StartsWith("error:", StringComparison.Ordinal), text));
        }
    }

    /// <summary>
    /// Esegue un tool rag.*. <paramref name="scopeBrains"/> è il set del livello AGENT/TASK dello scope
    /// (F2/F3): se <c>null</c> NON viene toccato il default (tutti i brain attivati globalmente).
    /// L'argomento esplicito `brain` del tool ha precedenza sullo scope.
    /// </summary>
    internal static string Execute(string name, string argsJson, string[]? scopeBrains = null)
    {
        string? Arg(string key)
        {
            try
            {
                using var doc = JsonDocument.Parse(argsJson);
                return doc.RootElement.TryGetProperty(key, out var el) ? el.GetString() : null;
            }
            catch { return null; }
        }
        int ArgTopK(string key, int def)
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
        string[]? Brains(string? brain) => brain is { Length: > 0 } b ? b.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries) : null;
        // Precedenza: argomento esplicito del tool → scope (agente/task) → null (default del consumer).
        string[]? EffectiveBrains(string? brain) => Brains(brain) ?? (scopeBrains is { Length: > 0 } sb ? sb : null);

        try
        {
            switch (name)
            {
                case "rag.search":
                {
                    var q = Arg("query") ?? "";
                    if (q.Length == 0) return "error: manca 'query'";
                    var topK = ArgTopK("topK", 5);
                    return Basic.AI.Rag.MultiBrainSearch.SearchMulti(q, topK, "wiki", EffectiveBrains(Arg("brain")));
                }
                case "rag.context":
                {
                    var q = Arg("query") ?? "";
                    if (q.Length == 0) return "error: manca 'query'";
                    var topK = ArgTopK("topK", 5);
                    var bundle = Basic.AI.Rag.MultiBrainSearch.RagContextMulti(q, topK, brains: EffectiveBrains(Arg("brain")));
                    if (string.IsNullOrEmpty(bundle.Context)) return "error: nessun contesto recuperato (brain RAG collegati? AI BRAIN USE)";
                    return bundle.Context;
                }
                case "rag.ask":
                {
                    var q = Arg("question") ?? Arg("query") ?? "";
                    if (q.Length == 0) return "error: manca 'question'";
                    var topK = ArgTopK("topK", 5);
                    return Basic.AI.Rag.MultiBrainSearch.AskMulti(q, topK, brains: EffectiveBrains(Arg("brain")));
                }
                default:
                    return $"error: tool rag '{name}' non riconosciuto";
            }
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }
}
