// FILE: Basic.Mcp.McpConfig
// Lista dei server MCP consumabili come CLIENT (config/mcp.json — configDir configurabile). Il programmatore
// GMBASIC conosce i propri server MCP (endpoint HTTP/SSE o comando stdio locale già
// presente sul PATH) e alimenta la lista: ingrandisce con MCP.SERVERADD, restringe con
// MCP.SERVERREMOVE, abilita/disabilita con MCP.SERVERENABLE/DISABLE. Nessun pacchetto
// esterno: http = URL JSON-RPC/SSE, stdio = eseguibile già installato.
using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib;

namespace Basic.Mcp;

/// <summary>Definizione di un server MCP consumabile. <see cref="Kind"/> è
/// <c>"http"</c> (endpoint remoto, <see cref="Url"/>) o <c>"stdio"</c> (processo locale
/// già presente, <see cref="Command"/> + <see cref="Args"/>).</summary>
public sealed record McpServerEntry(
    string Name,
    string Kind,
    string? Url = null,
    string? Command = null,
    List<string>? Args = null,
    Dictionary<string, string>? Env = null,
    bool Enabled = true);

/// <summary>Persistenza della lista dei server MCP client. Un solo file JSON
/// (<c>config/mcp.json</c>), stesso pattern di <see cref="Basic.AI.AiConfig"/>.</summary>
public static class McpConfig
{
    public static string DefaultPath => GmbiPaths.McpFile;

    public sealed record Settings(List<McpServerEntry> Servers)
    {
        /// <summary>Chiavi JSON non mappate: conservate al salvataggio (stessa classe del bug
        /// `AiConfig-Roundtrip-Perdita-Chiavi-JSON`, 2026-09-16 — latente qui, oggi il file ha solo
        /// `servers`).</summary>
        [JsonExtensionData]
        public Dictionary<string, JsonElement> Extra { get; init; } = new();

        public static Settings Empty() => new([]);
    }

    private static Settings? _cache;

    public static Settings Load()
    {
        if (_cache is not null) return _cache;
        if (!File.Exists(DefaultPath))
        {
            // Prima esecuzione: configurazione di default con i server MCP piu' comuni
            // (catalogo marketplace) gia' abilitati — l'utente puo' usarli subito.
            var defaults = McpMarketplace.Catalog.ToList();
            _cache = new Settings(defaults);
            try { Save(_cache); } catch { /* best-effort: fallisce solo se la cartella non e' scrivibile */ }
            return _cache;
        }
        try
        {
            var raw = File.ReadAllText(DefaultPath);
            _cache = JsonSerializer.Deserialize<Settings>(raw, JsonOpts) ?? Settings.Empty();
        }
        catch
        {
            _cache = Settings.Empty();
        }
        return _cache;
    }

    public static void Save(Settings settings)
    {
        _cache = settings;
        var dir = Path.GetDirectoryName(DefaultPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var json = JsonSerializer.Serialize(settings, JsonOpts);
        try { if (File.Exists(DefaultPath) && File.ReadAllText(DefaultPath) == json) return; } catch { }
        File.WriteAllText(DefaultPath, json);
    }

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    /// <summary>Server configurati (ordine di inserimento).</summary>
    public static IReadOnlyList<McpServerEntry> Servers => Load().Servers;

    public static McpServerEntry? Get(string name) =>
        Load().Servers.FirstOrDefault(s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    /// <summary>Aggiunge (o sostituisce) un server alla lista. Ritorna false se il nome
    /// esiste già con definizione diversa (usa SetEnabled per togglarlo).</summary>
    public static bool AddServer(McpServerEntry entry)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(entry.Name);
        var s = Load();
        var existing = s.Servers.FirstOrDefault(x => x.Name.Equals(entry.Name, StringComparison.OrdinalIgnoreCase));
        if (existing is not null)
            return existing == entry;
        Save(s with { Servers = [.. s.Servers, entry] });
        return true;
    }

    /// <summary>Rimuove un server dalla lista. Ritorna true se esisteva.</summary>
    public static bool RemoveServer(string name)
    {
        var s = Load();
        var filtered = s.Servers.Where(x => !x.Name.Equals(name, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count == s.Servers.Count) return false;
        Save(s with { Servers = filtered });
        return true;
    }

    /// <summary>Abilita/disabilita un server (ingrandire/restringere senza rimuovere).</summary>
    public static bool SetEnabled(string name, bool enabled)
    {
        var s = Load();
        var idx = s.Servers.FindIndex(x => x.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        if (idx < 0) return false;
        var updated = s.Servers.ToList();
        updated[idx] = updated[idx] with { Enabled = enabled };
        Save(s with { Servers = updated });
        return true;
    }

    internal static void ResetForTests() => _cache = null;
}
