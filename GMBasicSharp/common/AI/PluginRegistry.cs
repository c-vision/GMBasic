// FILE: Basic.AI.PluginRegistry
// Registro dei plugin operativi (ciclo di vita e integrazione tool). Un plugin vive in
// plugins/<nome>/PLUGIN.md; Enable carica i tool.json nel McpToolRegistry (così
// AgentToolLoop può invocarli nel round-trip); Disable li deregistra.

namespace Basic.AI;

using System.Text;
using System.Text.Json.Nodes;
using Basic.Mcp;
using CommonLib;

/// <summary>Registro in-process dei plugin (per nome) con ciclo di vita
/// Register/Unregister/Enable/Disable e dispatch della capacità (Call).</summary>
public static class PluginRegistry
{
    private static readonly Dictionary<string, PluginRecord> _records = new(StringComparer.OrdinalIgnoreCase);
    private static readonly object _lock = new();

    /// <summary>Handler di esecuzione della procedura BASIC del plugin, registrato dal
    /// backend IL (IlRuntime, in C#) — il dominio AI non dipende dal backend.</summary>
    public static Func<string, object?[]?, object?>? CallHandler { get; set; }

    public static bool Register(string name)
    {
        var dir = Path.Combine(AiConfig.PluginsDefaultPath, name);
        var pluginFile = Path.Combine(dir, "PLUGIN.md");
        if (!File.Exists(pluginFile)) return false;
        var record = PluginIndex.Scan().FirstOrDefault(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
            ?? new PluginRecord(name, pluginFile, "", "", true, [], [], []);
        lock (_lock) _records[name] = record;
        return true;
    }

    public static bool Unregister(string name)
    {
        lock (_lock) return _records.Remove(name);
    }

    /// <summary>Registra il plugin e carica i suoi tool.json nel McpToolRegistry
    /// (capacità invocabili dall'agente via AgentToolLoop).</summary>
    public static bool Enable(string name)
    {
        if (!Register(name)) return false;
        var dir = Path.Combine(AiConfig.PluginsDefaultPath, name);
        try
        {
            foreach (var tj in Directory.EnumerateFiles(dir, "tool.json", SearchOption.TopDirectoryOnly))
                foreach (var tool in ParseTools(tj))
                    McpToolRegistry.Instance.Register(tool);
        }
        catch { /* best effort */ }
        return true;
    }

    /// <summary>Deregistra i tool del plugin dal McpToolRegistry e rimuove il record.</summary>
    public static bool Disable(string name)
    {
        var record = Get(name);
        if (record != null)
        {
            foreach (var t in record.Tools)
            {
                try { McpToolRegistry.Instance.Unregister(t); } catch { /* best effort */ }
            }
        }
        return Unregister(name);
    }

    public static PluginRecord? Get(string name)
    {
        lock (_lock)
        {
            if (_records.TryGetValue(name, out var r)) return r;
            return PluginIndex.Scan().FirstOrDefault(p =>
                p.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        }
    }

    public static IReadOnlyList<PluginRecord> List()
    {
        lock (_lock) return _records.Values.ToList();
    }

    /// <summary>Esegue la capacità del plugin: trova il plugin, prende la prima procedura
    /// BASIC esposta (procedures del manifest) e la invoca via CallHandler (registrato dal
    /// backend IL → InvokeBasicProcedure). Ritorna il risultato o null.</summary>
    public static object? Call(string name, object?[]? args)
    {
        var record = Get(name);
        if (record == null || CallHandler == null) return null;
        var proc = record.Procedures.FirstOrDefault();
        if (string.IsNullOrEmpty(proc)) return null;
        return CallHandler(proc, args);
    }

    /// <summary>Crea lo scheletro di un plugin (PLUGIN.md + tool.json) — base per la
    /// auto-generazione guidata dall'agente.</summary>
    public static bool Create(string name, string description)
    {
        if (string.IsNullOrWhiteSpace(name)) return false;
        var dir = Path.Combine(AiConfig.PluginsDefaultPath, name);
        Directory.CreateDirectory(dir);
        var manifest =
            $"---\nname: {name}\ndescription: {description}\ntrigger: \nenabled: true\ntools: [{ToolName(name)}]\nrequires: []\nprocedures: [{MakeProcName(name)}]\n---\n" +
            $"# Plugin {name}\n\nIstruzioni operative per l'agente.\n";
        File.WriteAllText(Path.Combine(dir, "PLUGIN.md"), manifest, Encoding.UTF8);
        // Scheletro tool.json (spec §2.3: Create genera PLUGIN.md + tool.json): così il plugin
        // creato è subito abilitabile ed espone il tool all'AgentToolLoop. L'agente riempie poi
        // procedure/inputSchema reali. Supporta il parser di ParseTools (singolo oggetto).
        var tool = new JsonObject
        {
            ["name"] = ToolName(name),
            ["description"] = description,
            ["procedure"] = MakeProcName(name),
            ["inputSchema"] = new JsonObject { ["type"] = "object", ["properties"] = new JsonObject() }
        };
        File.WriteAllText(Path.Combine(dir, "tool.json"), tool.ToJsonString(), Encoding.UTF8);
        return File.Exists(Path.Combine(dir, "PLUGIN.md")) && File.Exists(Path.Combine(dir, "tool.json"));
    }

    /// <summary>Nome del tool MCP a partire dal nome del plugin (trattini → underscore).</summary>
    private static string ToolName(string name) => name.Trim().Replace('-', '_');

    /// <summary>Nome procedura BASIC PascalCase a partire dal nome del plugin (web-fetch → WebFetch).</summary>
    private static string MakeProcName(string name)
    {
        var sb = new StringBuilder();
        foreach (var seg in name.Trim().Split(['-', '_', ' ', '.']))
            if (seg.Length > 0) sb.Append(char.ToUpperInvariant(seg[0])).Append(seg.AsSpan(1));
        return sb.ToString();
    }

    /// <summary>Parsa un tool.json in un McpToolDefinition (nome, descrizione, procedura BASIC,
    /// input schema) — mappa a McpToolRegistry.</summary>
    /// <summary>Parsa tool.json — supporta sia un singolo oggetto tool sia un ARRAY di tool
    /// (`[{...},{...}]`): ogni tool mappa a un McpToolDefinition registrato in McpToolRegistry.</summary>
    private static IEnumerable<McpToolDefinition> ParseTools(string toolJsonPath)
    {
        foreach (var obj in ReadToolObjects(toolJsonPath))
        {
            var name = obj["name"]?.GetValue<string>()?.Trim();
            if (string.IsNullOrWhiteSpace(name)) continue;
            var procedure = obj["procedure"]?.GetValue<string>()?.Trim() ?? "";
            var schema = obj["inputSchema"] as JsonObject ?? new JsonObject();
            yield return new McpToolDefinition
            {
                Name = name,
                Description = obj["description"]?.GetValue<string>() ?? "",
                QualifiedProcedureName = procedure,
                InputSchema = schema
            };
        }
    }

    private static JsonObject[] ReadToolObjects(string toolJsonPath)
    {
        try
        {
            var node = JsonNode.Parse(File.ReadAllText(toolJsonPath, Encoding.UTF8));
            return node switch
            {
                JsonObject o => new[] { o },
                JsonArray arr => arr.OfType<JsonObject>().ToArray(),
                _ => Array.Empty<JsonObject>()
            };
        }
        catch { return Array.Empty<JsonObject>(); }
    }

    /// <summary>Carica i tool del tool.json di un plugin SENZA registrarli (usato per la
    /// risoluzione per-ask guidata dal trigger: l'agente acquisisce la skill se il prompt
    /// la menziona, senza allargare il contesto di default).</summary>
    public static IEnumerable<McpToolDefinition> ToolsFor(string name)
    {
        var toolJson = Path.Combine(AiConfig.PluginsDefaultPath, name, "tool.json");
        if (!File.Exists(toolJson)) return [];
        try { return ParseTools(toolJson).ToList(); }
        catch { return []; }
    }

    /// <summary>Installazione da indice marketplace: clona un repo git (AI PLUGINS MARKETPLACE)
    /// nella destinazione e rileva i plugin (PluginIndex.Refresh). Ritorna 1.0 se il clone è
    /// avvenuto, 0.0 altrimenti.</summary>
    public static double CloneFromIndex(string url, string? dest = null)
    {
        var target = dest;
        if (string.IsNullOrWhiteSpace(target))
            target = Path.Combine(GmbiPaths.PluginsMarketplacesDir, Path.GetFileNameWithoutExtension(url.TrimEnd('/')));
        try
        {
            System.Diagnostics.Process.Start("git", "clone --depth 1 " + url + " \"" + target + "\"")?.WaitForExit();
        }
        catch { return 0.0; }
        PluginIndex.Refresh();
        return Directory.Exists(target) ? 1.0 : 0.0;
    }
}
