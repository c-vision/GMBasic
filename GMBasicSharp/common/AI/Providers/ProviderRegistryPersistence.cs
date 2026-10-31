// FILE: Basic.Providers.ProviderRegistryPersistence
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.Providers;

/// <summary>JSON persistence for <see cref="ProviderServer"/> registry state.</summary>
/// <remarks>
/// ponytail: JSON file (not ISAM/SQLite table); secrets stored as <c>$env:VAR</c> on save.
/// </remarks>
internal static class ProviderRegistryPersistence
{
    private const int CurrentVersion = 1;
    private const string EnvPrefix = "$env:";

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    private static readonly string[] SensitiveKeys = ["apikey", "token", "secret", "password"];

    internal sealed record Snapshot(
        int Version,
        Dictionary<string, ProviderEntry> Providers,
        Dictionary<string, List<string>> RoleOrder,
        Dictionary<string, int> ActiveIndex);

    internal sealed record ProviderEntry(
        string Role, string Kind, Dictionary<string, string> Config,
        int Priority = 0, string Status = "Available", int Weight = 1,
        bool IsFree = false, decimal CostInputPer1m = 0m, decimal CostOutputPer1m = 0m,
        int CreditsRemaining = -1);

    internal static Snapshot Capture(ProviderServer server) => server.CaptureSnapshot();

    internal static void Restore(ProviderServer server, Snapshot snapshot) => server.RestoreSnapshot(snapshot);

    internal static void SaveToFile(string path, Snapshot snapshot)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        var redacted = RedactSecrets(snapshot);
        var json = JsonSerializer.Serialize(redacted, JsonOptions);
        File.WriteAllText(path, json);
    }

    internal static Snapshot LoadFromFile(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        if (!File.Exists(path))
            throw new FileNotFoundException($"Provider registry file not found: '{path}'.", path);

        var json = File.ReadAllText(path);
        var snapshot = JsonSerializer.Deserialize<Snapshot>(json, JsonOptions)
                       ?? throw new InvalidDataException($"Provider registry file '{path}' is empty or invalid.");

        if (snapshot.Version != CurrentVersion)
            throw new InvalidDataException(
                $"Provider registry version {snapshot.Version} is not supported (expected {CurrentVersion}).");

        ResolveSecrets(snapshot);
        return snapshot;
    }

    internal static Snapshot CreateSnapshot(
        IReadOnlyDictionary<string, ProviderDescriptor> providers,
        IReadOnlyDictionary<string, List<string>> roleOrder,
        IReadOnlyDictionary<string, int> activeIndex)
    {
        var entries = new Dictionary<string, ProviderEntry>(StringComparer.OrdinalIgnoreCase);
        foreach (var (name, descriptor) in providers)
        {
            entries[name] = new ProviderEntry(
                descriptor.Role,
                descriptor.Kind,
                new Dictionary<string, string>(descriptor.Config, StringComparer.OrdinalIgnoreCase),
                descriptor.Priority,
                descriptor.Status.ToString(),
                descriptor.Weight,
                descriptor.IsFree,
                descriptor.CostInputPer1m,
                descriptor.CostOutputPer1m,
                descriptor.CreditsRemaining);
        }

        var order = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var (role, names) in roleOrder)
            order[role] = [..names];

        var active = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var (role, index) in activeIndex)
            active[role] = index;

        return new Snapshot(CurrentVersion, entries, order, active);
    }

    /// <summary>Redige i segreti prima di scrivere il file. Un valore GIA' in forma <c>$env:X</c>
    /// (o vuoto) resta invariato; un valore REALE viene persistito in <c>.env</c> (CommonLib.EnvFile
    /// — mai in questo file JSON, versionato dal 2026-09-07) e sostituito col riferimento.</summary>
    private static Snapshot RedactSecrets(Snapshot snapshot)
    {
        var providers = new Dictionary<string, ProviderEntry>(StringComparer.OrdinalIgnoreCase);
        foreach (var (name, entry) in snapshot.Providers)
        {
            var config = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var (key, value) in entry.Config)
            {
                if (IsSensitiveKey(key) && !string.IsNullOrEmpty(value) &&
                    !value.StartsWith(EnvPrefix, StringComparison.Ordinal))
                {
                    var envRef = ToEnvRef(name, key);
                    CommonLib.EnvFile.Set(envRef[EnvPrefix.Length..], value);
                    config[key] = envRef;
                }
                else
                {
                    config[key] = value;
                }
            }

            providers[name] = entry with { Config = config };
        }

        return snapshot with { Providers = providers };
    }

    private static void ResolveSecrets(Snapshot snapshot)
    {
        foreach (var entry in snapshot.Providers.Values)
        {
            foreach (var key in entry.Config.Keys.ToList())
            {
                var value = entry.Config[key];
                if (!value.StartsWith(EnvPrefix, StringComparison.Ordinal))
                    continue;

                var envName = value[EnvPrefix.Length..];
                var resolved = Environment.GetEnvironmentVariable(envName);
                if (string.IsNullOrEmpty(resolved))
                {
                    // Una singola env var mancante NON deve far fallire il caricamento dell'intero
                    // registry: lascia il placeholder e lo skip del provider avverrà al momento
                    // dell'uso (GetActive* salterà il provider che non ha credenziali risolte).
                    continue;
                }

                entry.Config[key] = resolved;
            }
        }
    }

    private static bool IsSensitiveKey(string key) =>
        SensitiveKeys.Any(s => key.Contains(s, StringComparison.OrdinalIgnoreCase))
        // "maxTokens"/"contextTokens" ecc. sono limiti numerici (plurale), MAI credenziali
        // (quelle sono sempre singolari: "token"/"apiKey"/"authToken") — false positive reale
        // osservato su llamacpp-chat.maxTokens (2026-09-07).
        && !key.EndsWith("Tokens", StringComparison.OrdinalIgnoreCase);

    private static string ToEnvRef(string providerName, string key)
    {
        var slug = providerName.Replace('-', '_').ToUpperInvariant();
        return $"{EnvPrefix}{slug}_{key.ToUpperInvariant()}";
    }
}
