// AI/Providers/ProviderConfig.cs — Provider configuration file management

using CommonLib;

namespace Basic.Providers;

public static class ProviderConfig
{
    public static string DefaultPath => GmbiPaths.ProvidersFile;

    /// <summary>Default scheme map: alias di registry -> base URL (pattern npm/pip/docker).
    /// "hf" e' HuggingFace; "ollama"/"openai"/"anthropic" sono i registry API noti. Un nuovo
    /// registry (es. un futuro "hf2") si aggiunge qui o nel config, senza toccare il codice.
    /// I provider salvati con source scheme-based usano questa mappa per risolvere la base URL.</summary>
    public static IReadOnlyDictionary<string, string> DefaultSchemes { get; } =
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["hf"] = "https://huggingface.co",
            ["huggingface"] = "https://huggingface.co",
            ["ollama"] = "http://localhost:11434",
            ["openai"] = "https://api.openai.com/v1",
            ["anthropic"] = "https://api.anthropic.com/v1",
        };

    private static Dictionary<string, string> _schemes = new(DefaultSchemes, StringComparer.OrdinalIgnoreCase);

    /// <summary>Risolve lo scheme (es. "hf", "ollama") nella base URL configurata, o null se
    /// sconosciuto. La mappa può essere estesa dal config file (campo "schemes").</summary>
    public static string? LookupScheme(string scheme) =>
        _schemes.TryGetValue(scheme, out var url) ? url : null;

    /// <summary>Registra o aggiorna uno scheme nel config corrente e lo persiste.</summary>
    public static void SetScheme(string scheme, string baseUrl)
    {
        _schemes[scheme] = baseUrl;
    }

    /// <summary>Default scheme map (used by Save to persist).</summary>
    public static IReadOnlyDictionary<string, string> CurrentSchemes => _schemes;

    /// <summary>Ensure the config file exists with all known providers pre-populated (no API keys).</summary>
    public static void EnsureDefaults()
    {
        var dir = Path.GetDirectoryName(DefaultPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        if (File.Exists(DefaultPath)) return;

        var entries = new List<object>();
        entries.Add(new { role = "completion", name = "openai-default",    kind = "openai",    config = new Dictionary<string, string> { ["model"] = "gpt-4o",    ["baseUrl"] = "https://api.openai.com/v1" } });
        entries.Add(new { role = "completion", name = "anthropic-default", kind = "anthropic", config = new Dictionary<string, string> { ["model"] = "claude-3-5-sonnet-20241022", ["maxTokens"] = "4096" } });
        entries.Add(new { role = "completion", name = "ollama-default",    kind = "ollama",    config = new Dictionary<string, string> { ["model"] = "llama3",  ["baseUrl"] = "http://localhost:11434" } });

        foreach (var (slug, (baseUrl, headers)) in KnownProviders.Map)
            entries.Add(new { role = "completion", name = $"{slug}-default", kind = slug == "anthropic" ? "anthropic" : "openai",
                config = new Dictionary<string, string> { ["model"] = "", ["baseUrl"] = baseUrl }
                    .Concat(headers).ToDictionary(kv => kv.Key, kv => kv.Value) });

        Save(entries);
    }

    /// <summary>Load providers + schemes from the config file into ProviderServer.</summary>
    public static void Load()
    {
        EnsureDefaults();
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(DefaultPath));
            if (doc.RootElement.TryGetProperty("schemes", out var schemes) && schemes.ValueKind == System.Text.Json.JsonValueKind.Object)
            {
                foreach (var p in schemes.EnumerateObject())
                {
                    if (p.Value.ValueKind == System.Text.Json.JsonValueKind.String)
                        _schemes[p.Name] = p.Value.GetString() ?? "";
                }
            }
        }
        catch { /* config schemes non validi: restano i default */ }
        ProviderServer.Instance.Load(DefaultPath);
        if (SyncKnownProvidersFromEnv()) Save();
    }

    /// <summary>Registra automaticamente ogni provider noto (KnownProviders.Map + openai/anthropic)
    /// per cui esiste una API key reale in .env (CommonLib.EnvFile, convenzione
    /// &lt;NOME&gt;_APIKEY — stessa di ProviderRegistryPersistence.ToEnvRef) ma che NON ha ancora
    /// un entry in providers.json — stesso pattern gia' usato manualmente per "deepseek".
    /// Ritorna true se ha registrato almeno un provider (il chiamante deve poi salvare).</summary>
    private static bool SyncKnownProvidersFromEnv()
    {
        var existing = ProviderServer.Instance.CaptureSnapshot().Providers.Keys;
        var added = false;

        void TryRegister(string name, string kind, string baseUrl, IReadOnlyDictionary<string, string>? extraHeaders = null)
        {
            if (existing.Contains(name, StringComparer.OrdinalIgnoreCase)) return;
            var envName = name.Replace('-', '_').ToUpperInvariant() + "_APIKEY";
            var key = CommonLib.EnvFile.Get(envName);
            if (string.IsNullOrWhiteSpace(key)) return;

            var config = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["apiKey"] = key,
                ["baseUrl"] = baseUrl,
                ["model"] = "",
            };
            if (extraHeaders != null)
                foreach (var (k, v) in extraHeaders) config[k] = v;

            ProviderServer.Instance.Register("completion", name, kind, config);
            added = true;
        }

        TryRegister("openai", "openai", DefaultSchemes["openai"]);
        TryRegister("anthropic", "anthropic", DefaultSchemes["anthropic"]);
        foreach (var (slug, (baseUrl, headers)) in KnownProviders.Map)
            TryRegister(slug, slug == "anthropic" ? "anthropic" : "openai", baseUrl, headers);

        return added;
    }

    /// <summary>Save current ProviderServer state to config file.</summary>
    public static void Save()
    {
        ProviderServer.Instance.Save(DefaultPath);
        // Persist schemes nel config -- ricostruisce il json aggiungendo il campo schemes.
        var root = System.Text.Json.JsonSerializer.Deserialize<System.Text.Json.Nodes.JsonObject>(File.ReadAllText(DefaultPath))
                   ?? new System.Text.Json.Nodes.JsonObject();
        var schemesObj = new System.Text.Json.Nodes.JsonObject();
        foreach (var kv in _schemes)
            schemesObj[kv.Key] = kv.Value;
        root["schemes"] = schemesObj;
        File.WriteAllText(DefaultPath, System.Text.Json.JsonSerializer.Serialize(root,
            new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
    }

    private static void Save(List<object> entries)
    {
        var json = System.Text.Json.JsonSerializer.Serialize(new { providers = entries },
            new System.Text.Json.JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(DefaultPath, json);
    }
}
