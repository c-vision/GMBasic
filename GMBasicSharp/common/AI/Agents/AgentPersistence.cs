// FILE: Basic.Agents.AgentPersistence
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.Agents;

/// <summary>JSON persistence for <see cref="AgentServer"/> agent state.</summary>
/// <remarks>ponytail: JSON file (not ISAM); upgrade path: ISAM transactional agent store.</remarks>
internal static class AgentPersistence
{
    internal const int CurrentVersion = 1;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    internal sealed record Snapshot(
        int Version,
        string Name,
        string Id,
        string Model,
        double Temperature,
        int MaxTokens,
        string SystemPrompt,
        string? ProviderName,
        List<string> ToolNames,
        List<AgentChatMessage> History,
        bool SkillsEnabled = false,
        bool SessionStreamingEnabled = false,
        bool ClientToolsEnabled = false,
        bool MemoryEnabled = false,
        bool PluginsEnabled = false,
        AgentPermissionLevel PermissionLevel = AgentPermissionLevel.YOLO,
        Dictionary<string, string>? ToolPolicies = null,
        string? PublicKey = null,
        string? PrivateKey = null,
        string? Brains = null);

    internal static Snapshot Capture(AgentRecord agent) =>
        new(
            CurrentVersion,
            agent.Name,
            agent.Id,
            agent.Model,
            agent.Temperature,
            agent.MaxTokens,
            agent.SystemPrompt,
            agent.ProviderName,
            [..agent.ToolNames],
            [..agent.History],
            agent.SkillsEnabled,
            agent.SessionStreamingEnabled,
            agent.ClientToolsEnabled,
            agent.MemoryEnabled,
            agent.PluginsEnabled,
            agent.PermissionLevel,
            new Dictionary<string, string>(agent.ToolPolicies, StringComparer.OrdinalIgnoreCase),
            agent.PublicKey,
            agent.PrivateKey,
            agent.Brains);

    internal static AgentRecord Restore(Snapshot snapshot)
    {
        var record = new AgentRecord
        {
            Name = snapshot.Name,
            Id = snapshot.Id,
            Model = snapshot.Model,
            Temperature = snapshot.Temperature,
            MaxTokens = snapshot.MaxTokens,
            SystemPrompt = snapshot.SystemPrompt,
            ProviderName = snapshot.ProviderName,
            SkillsEnabled = snapshot.SkillsEnabled,
            SessionStreamingEnabled = snapshot.SessionStreamingEnabled,
            ClientToolsEnabled = snapshot.ClientToolsEnabled,
            MemoryEnabled = snapshot.MemoryEnabled,
            PluginsEnabled = snapshot.PluginsEnabled,
            PermissionLevel = snapshot.PermissionLevel,
            PublicKey = snapshot.PublicKey,
            PrivateKey = snapshot.PrivateKey,
            // Compat: snapshot precedenti a F2 non hanno il campo → nessuna dichiarazione (Inherit).
            Brains = snapshot.Brains ?? string.Empty
        };
        if (snapshot.ToolPolicies is not null)
            foreach (var kv in snapshot.ToolPolicies)
                record.ToolPolicies[kv.Key] = kv.Value;
        record.ToolNames.UnionWith(snapshot.ToolNames ?? []);
        record.History.AddRange(snapshot.History);
        return record;
    }

    internal static void SaveToFile(string path, Snapshot snapshot)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        File.WriteAllText(path, JsonSerializer.Serialize(snapshot, JsonOptions));
    }

    internal static Snapshot LoadFromFile(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        if (!File.Exists(path))
            throw new FileNotFoundException($"Agent file not found: '{path}'.", path);

        var snapshot = JsonSerializer.Deserialize<Snapshot>(File.ReadAllText(path), JsonOptions)
                       ?? throw new InvalidDataException($"Agent file '{path}' is empty or invalid.");

        if (snapshot.Version != CurrentVersion)
            throw new InvalidDataException(
                $"Agent file version {snapshot.Version} is not supported (expected {CurrentVersion}).");

        return snapshot;
    }

}
