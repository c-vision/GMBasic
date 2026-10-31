// Agents/GraphAgentLoop.cs — tool grafo per gli agenti (Second Brain).
// Espone le funzioni del knowledge graph (GRAPH.*: vicini, path, visita, metriche, query
// multi-brain) come tool `graph.*` invocabili da un agente BASIC o da un client MCP. Il
// dispatch è NATIVO C# verso Basic.AI.Graph.GraphMultiQuery (nessuna procedura BASIC
// richiesta, nessun passaggio dal GPM: è sola lettura del grafo, non codice BASIC).
//
// Differenziale rispetto a graphify (tool Python standalone): qui un AGENTE del linguaggio
// (AI AGENT) può interrogare il knowledge graph del Second Brain durante un task e usare il
// risultato per decidere l'azione successiva — il grafo non è un artefatto statico ma una
// sorgente interrogabile nel loop agente.
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.Mcp;

namespace Basic.Agents;

/// <summary>Strumenti graph.* esposti agli agenti (query/nodi/vicini/visite/metriche del
/// knowledge graph Second Brain). Ogni tool ritorna JSON (o "1"/"0" per i predicati).</summary>
public static class GraphAgentLoop
{
    public static readonly (string Name, string Description)[] Tools =
    {
        ("graph.query", "Cerca nel grafo i nodi il cui path/title contiene la query (multi-brain) e ritorna il loro vicinato 1-hop"),
        ("graph.neighbors", "Vicini diretti (in+out) di un nodo in un brain"),
        ("graph.path", "Percorso più corto (BFS) tra due nodi di un brain"),
        ("graph.dijkstra", "Path a costo minimo tra due nodi (archi pesati)"),
        ("graph.bfs", "Visita in ampiezza dal nodo radice"),
        ("graph.dfs", "Visita in profondità dal nodo radice"),
        ("graph.hubs", "Nodi più centrali (grado) di un brain"),
        ("graph.stats", "Metriche del brain (nodi/edge/community/pageRank top)"),
    };

    /// <summary>Definizioni McpTool degli strumenti graph (procedura = nome tool).</summary>
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

    /// <summary>Registra gli strumenti graph nel registry (dedup per nome).</summary>
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
        sb.AppendLine("Strumenti graph.* per interrogare il knowledge graph del Second Brain (brain = nome del vault, es. 'graph-vault' o 'AiFinanceWiki').");
        sb.AppendLine("Per usarne uno rispondi SOLO con il JSON {\"tool_calls\":[{\"name\":\"<tool>\",\"arguments\":{...}}]}, altrimenti rispondi con la risposta finale in testo libero.");
        foreach (var (n, d) in Tools)
            sb.AppendLine($"- {n}: {d}");
        sb.AppendLine("Esempio: {\"tool_calls\":[{\"name\":\"graph.path\",\"arguments\":{\"brain\":\"graph-vault\",\"from\":\"index.md\",\"to\":\"trading/momentum.md\"}}]}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>Dispatcher MCP che esegue i tool graph lato C#.</summary>
    public sealed class Dispatcher : IMcpToolDispatcher
    {
        public Task<McpToolCallResult> DispatchAsync(string qualifiedProcedureName, string argumentsJson, CancellationToken cancellationToken)
        {
            var text = Execute(qualifiedProcedureName, argumentsJson);
            return Task.FromResult(new McpToolCallResult(text.StartsWith("error:", StringComparison.Ordinal), text));
        }
    }

    /// <summary>
    /// Esegue un tool graph.*. <paramref name="scopeBrains"/> è il set dello scope (livello AGENT/TASK):
    /// se <c>null</c> il default NON cambia; l'argomento esplicito `brain` del tool ha precedenza.
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

        // Precedenza: argomento esplicito del tool → scope → default (invariato se scopeBrains è null).
        string? ScopeBrain() => scopeBrains is { Length: > 0 } sb ? sb[0] : null;
        string[]? ScopeBrains() => scopeBrains is { Length: > 0 } sb ? sb : null;

        try
        {
            switch (name)
            {
                case "graph.query":
                {
                    var q = Arg("query") ?? "";
                    if (q.Length == 0) return "error: manca 'query'";
                    var brains = Arg("brain") is { Length: > 0 } single ? new[] { single } : ScopeBrains();
                    return Basic.AI.Graph.GraphMultiQuery.QueryMulti(q, brains);
                }
                case "graph.neighbors":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? ""; var node = Arg("node") ?? "";
                    if (brain.Length == 0 || node.Length == 0) return "error: mancano 'brain' e 'node'";
                    return Basic.AI.Graph.GraphMultiQuery.Neighbors(brain, node);
                }
                case "graph.path":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? ""; var from = Arg("from") ?? ""; var to = Arg("to") ?? "";
                    if (brain.Length == 0 || from.Length == 0 || to.Length == 0) return "error: mancano 'brain','from','to'";
                    return Basic.AI.Graph.GraphMultiQuery.Path(brain, from, to);
                }
                case "graph.dijkstra":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? ""; var from = Arg("from") ?? ""; var to = Arg("to") ?? "";
                    if (brain.Length == 0 || from.Length == 0 || to.Length == 0) return "error: mancano 'brain','from','to'";
                    return Basic.AI.Graph.GraphMultiQuery.Dijkstra(brain, from, to);
                }
                case "graph.bfs":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? ""; var root = Arg("root") ?? "";
                    if (brain.Length == 0 || root.Length == 0) return "error: mancano 'brain' e 'root'";
                    return Basic.AI.Graph.GraphMultiQuery.Bfs(brain, root);
                }
                case "graph.dfs":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? ""; var root = Arg("root") ?? "";
                    if (brain.Length == 0 || root.Length == 0) return "error: mancano 'brain' e 'root'";
                    return Basic.AI.Graph.GraphMultiQuery.Dfs(brain, root);
                }
                case "graph.hubs":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? "";
                    if (brain.Length == 0) return "error: manca 'brain'";
                    return Basic.AI.Graph.GraphMultiQuery.Stats(brain);   // stats includono topHubs/pageRankTop
                }
                case "graph.stats":
                {
                    var brain = Arg("brain") is { Length: > 0 } b ? b : ScopeBrain() ?? "";
                    if (brain.Length == 0) return "error: manca 'brain'";
                    return Basic.AI.Graph.GraphMultiQuery.Stats(brain);
                }
                default:
                    return $"error: tool graph '{name}' non riconosciuto";
            }
        }
        catch (Exception ex) { return "error: " + ex.Message; }
    }
}
