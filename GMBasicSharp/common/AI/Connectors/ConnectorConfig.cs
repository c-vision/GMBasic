// FILE: Basic.Connectors.ConnectorConfig
// Persistenza dei connettori (config/connectors.json), pattern McpConfig/AiConfig.
// Un solo file JSON per tutti i connettori + cache in-memory.

using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib;

namespace Basic.Connectors;

/// <summary>Persistenza della lista connettori (config/connectors.json).</summary>
public static class ConnectorConfig
{
    public static string DefaultPath => Path.Combine(Path.GetDirectoryName(GmbiPaths.McpFile) ?? "config", "connectors.json");

    public sealed record Settings(List<ConnectorDefinition> Connectors)
    {
        /// <summary>Chiavi JSON non mappate: conservate al salvataggio (stessa classe del bug
        /// `AiConfig-Roundtrip-Perdita-Chiavi-JSON`, 2026-09-16 — latente qui, oggi il file ha solo
        /// `connectors`).</summary>
        [JsonExtensionData]
        public Dictionary<string, JsonElement> Extra { get; init; } = new();

        public static Settings Empty() => new([]);
    }

    private static Settings? _cache;

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    public static Settings Load()
    {
        if (_cache is not null) return _cache;
        if (!File.Exists(DefaultPath))
        {
            var defaults = ConnectorMarketplace.Catalog.Select(c => new ConnectorDefinition(
                Name: c.Name, Kind: c.Kind, DisplayName: c.DisplayName, Version: c.Version,
                AuthKind: c.AuthKind, Capabilities: c.Capabilities.ToList(), Tools: c.Tools.ToList(),
                Description: c.Description, Url: c.Url, Command: c.Command, Args: c.Args?.ToList(),
                Enabled: c.Enabled)).ToList();
            _cache = new Settings(defaults);
            try { Save(_cache); } catch { }
            return _cache;
        }
        try
        {
            var raw = File.ReadAllText(DefaultPath);
            _cache = JsonSerializer.Deserialize<Settings>(raw, JsonOpts) ?? Settings.Empty();
        }
        catch { _cache = Settings.Empty(); }
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

    public static IReadOnlyList<ConnectorDefinition> Connectors => Load().Connectors;

    public static ConnectorDefinition? Get(string name) =>
        Load().Connectors.FirstOrDefault(c => c.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    public static bool Add(ConnectorDefinition def)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(def.Name);
        var s = Load();
        if (s.Connectors.Any(c => c.Name.Equals(def.Name, StringComparison.OrdinalIgnoreCase)))
            return false;
        Save(s with { Connectors = [.. s.Connectors, def with { CreatedAt = DateTimeOffset.Now, UpdatedAt = DateTimeOffset.Now }] });
        return true;
    }

    public static bool Update(ConnectorDefinition def)
    {
        var s = Load();
        var idx = s.Connectors.FindIndex(c => c.Name.Equals(def.Name, StringComparison.OrdinalIgnoreCase));
        if (idx < 0) return false;
        var list = s.Connectors.ToList();
        list[idx] = def with { UpdatedAt = DateTimeOffset.Now };
        Save(s with { Connectors = list });
        return true;
    }

    public static bool Remove(string name)
    {
        var s = Load();
        var filtered = s.Connectors.Where(c => !c.Name.Equals(name, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count == s.Connectors.Count) return false;
        Save(s with { Connectors = filtered });
        return true;
    }

    public static bool SetEnabled(string name, bool enabled)
    {
        var s = Load();
        var idx = s.Connectors.FindIndex(c => c.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        if (idx < 0) return false;
        var list = s.Connectors.ToList();
        list[idx] = list[idx] with { Enabled = enabled, UpdatedAt = DateTimeOffset.Now };
        Save(s with { Connectors = list });
        return true;
    }

    public static bool SetPolicy(string connectorName, string tool, string policy)
    {
        var s = Load();
        var idx = s.Connectors.FindIndex(c => c.Name.Equals(connectorName, StringComparison.OrdinalIgnoreCase));
        if (idx < 0) return false;
        var list = s.Connectors.ToList();
        var def = list[idx];
        var policies = new Dictionary<string, string>(def.ToolPolicies ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase), StringComparer.OrdinalIgnoreCase)
        { [tool] = policy };
        list[idx] = def with { ToolPolicies = policies, UpdatedAt = DateTimeOffset.Now };
        Save(s with { Connectors = list });
        return true;
    }

    public static bool SetSecret(string connectorName, string key, string value)
    {
        var s = Load();
        var idx = s.Connectors.FindIndex(c => c.Name.Equals(connectorName, StringComparison.OrdinalIgnoreCase));
        if (idx < 0) return false;
        var list = s.Connectors.ToList();
        var def = list[idx];
        var secrets = new Dictionary<string, string>(def.Secrets ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase), StringComparer.OrdinalIgnoreCase)
        { [key] = value };
        list[idx] = def with { Secrets = secrets, UpdatedAt = DateTimeOffset.Now };
        Save(s with { Connectors = list });
        return true;
    }

    internal static void ResetForTests() => _cache = null;
}
