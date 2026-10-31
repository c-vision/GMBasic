// FILE: Basic.AI.PluginRecord
// Record di un plugin operativo (capacita' per l'agente). Un plugin vive in
// plugins/<nome>/PLUGIN.md (manifest + istruzioni) con tool.json opzionale
// (definizione tool MCP) e procedure BASIC opzionali (capacita' eseguibile).

namespace Basic.AI;

/// <summary>Descrizione di un plugin dell'archivio plugins/.</summary>
public sealed record PluginRecord(
    string Name,
    string Path,                 // percorso assoluto di PLUGIN.md
    string Description,
    string Trigger,
    bool Enabled,
    string[] Tools,              // nomi dei tool.json registrati in McpToolRegistry
    string[] Requires,           // namespace/IMPORT richiesti
    string[] Procedures);        // procedure BASIC esposte (QualifiedProcedureName)
