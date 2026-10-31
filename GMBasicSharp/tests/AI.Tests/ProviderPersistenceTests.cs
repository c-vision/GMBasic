// FILE: Sqlite.Tests.Providers.ProviderPersistenceTests
using System.Text.Json;
using Basic.Providers;
using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class ProviderPersistenceTests : IDisposable
{
    private readonly string _envPath = Path.Combine(Path.GetTempPath(), $"env-{Guid.NewGuid():N}.env");

    public ProviderPersistenceTests()
    {
        ProviderServer.ResetForTests();
        // Save_RedactsApiKey_ToEnvReference scrive un segreto reale via RedactSecrets -> EnvFile.Set:
        // MAI verso il .env reale del repo (bug osservato 2026-09-07, prima di questo isolamento).
        CommonLib.EnvFile.OverridePathForTests(_envPath);
    }

    public void Dispose()
    {
        ProviderServer.ResetForTests();
        CommonLib.EnvFile.ResetForTests();
        try { if (File.Exists(_envPath)) File.Delete(_envPath); } catch { /* best effort */ }
    }

    [Fact]
    public void SaveLoad_RoundTrip_PreservesProvidersAndActiveRole()
    {
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var server = ProviderServer.Instance;

        server.Register(ProviderRole.Embedding, "emb-a", "fixture");
        server.Register(ProviderRole.Embedding, "emb-b", "fixture");
        server.SetActive(ProviderRole.Embedding, "emb-b");
        server.Register(ProviderRole.Completion, "llm-a", "fixture");
        server.SetActive(ProviderRole.Completion, "llm-a");

        server.Save(path);
        ProviderServer.ResetForTests();

        ProviderServer.Instance.Load(path);

        var restored = ProviderServer.Instance;
        restored.List(ProviderRole.Embedding).Should().Contain(["emb-a", "emb-b"]);
        restored.GetActiveName(ProviderRole.Embedding).Should().Be("emb-b");
        restored.GetActiveName(ProviderRole.Completion).Should().Be("llm-a");
        restored.GetActiveEmbedding().Should().BeOfType<FixtureEmbeddingProvider>();
    }

    [Fact]
    public void SaveLoad_RoundTrip_PreservesPriorityAndStatus()
    {
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var server = ProviderServer.Instance;

        server.Register(ProviderRole.Embedding, "emb-a", "fixture");
        server.SetPriority("emb-a", 200);
        server.Register(ProviderRole.Embedding, "emb-b", "fixture");
        server.SetStatus("emb-b", ProviderStatus.Blocked);

        server.Save(path);
        ProviderServer.ResetForTests();
        ProviderServer.Instance.Load(path);

        var restored = ProviderServer.Instance;
        restored.GetPriority("emb-a").Should().Be(200);
        restored.GetStatus("emb-a").Should().Be(ProviderStatus.Available);
        restored.GetStatus("emb-b").Should().Be(ProviderStatus.Blocked);
    }

    [Fact]
    public void Load_JsonWithoutPriorityOrStatus_DefaultsToZeroAndAvailable()
    {
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var json = """
                   {
                     "version": 1,
                     "providers": {
                       "legacy-emb": { "role": "embedding", "kind": "fixture", "config": {} }
                     },
                     "roleOrder": { "embedding": ["legacy-emb"] },
                     "activeIndex": { "embedding": 0 }
                   }
                   """;
        File.WriteAllText(path, json);

        ProviderServer.Instance.Load(path);

        ProviderServer.Instance.GetPriority("legacy-emb").Should().Be(0);
        ProviderServer.Instance.GetStatus("legacy-emb").Should().Be(ProviderStatus.Available);
    }

    [Fact]
    public void Save_RedactsApiKey_ToEnvReference()
    {
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var server = ProviderServer.Instance;

        server.Register(ProviderRole.Completion, "openai-demo", "openai",
            new Dictionary<string, string>
            {
                ["apiKey"] = "sk-secret-value",
                ["model"] = "gpt-4o-mini"
            });

        server.Save(path);

        var json = File.ReadAllText(path);
        json.Should().NotContain("sk-secret-value");
        json.Should().Contain("$env:OPENAI_DEMO_APIKEY");

        using var doc = JsonDocument.Parse(json);
        var config = doc.RootElement.GetProperty("providers").GetProperty("openai-demo").GetProperty("config");
        config.GetProperty("apiKey").GetString().Should().Be("$env:OPENAI_DEMO_APIKEY");
        config.GetProperty("model").GetString().Should().Be("gpt-4o-mini");

        // Il valore reale deve finire nel .env isolato del test, mai in questo JSON.
        File.ReadAllText(_envPath).Should().Contain("OPENAI_DEMO_APIKEY=sk-secret-value");
    }

    [Fact]
    public void Load_ResolvesEnvReference_ForSensitiveConfig()
    {
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var envName = $"GMBASIC_OPENAI_DEMO_APIKEY_{Guid.NewGuid():N}";
        var json = $$"""
                     {
                       "version": 1,
                       "providers": {
                         "openai-demo": {
                           "role": "completion",
                           "kind": "openai",
                           "config": {
                             "apiKey": "$env:{{envName}}",
                             "model": "gpt-4o-mini"
                           }
                         }
                       },
                       "roleOrder": { "completion": ["openai-demo"] },
                       "activeIndex": { "completion": 0 }
                     }
                     """;
        File.WriteAllText(path, json);
        Environment.SetEnvironmentVariable(envName, "sk-from-env");

        try
        {
            ProviderServer.Instance.Load(path);
            ProviderServer.Instance.GetActiveName(ProviderRole.Completion).Should().Be("openai-demo");
        }
        finally
        {
            Environment.SetEnvironmentVariable(envName, null);
        }
    }

    [Fact]
    public void Load_MissingEnvVar_DoesNotThrow_PlaceholderPreserved()
    {
        // Il comportamento deliberato (dal commit fb027200, "Load robusto"): una $env var
        // mancante NON fa fallire il caricamento dell'intero registry — il placeholder resta
        // nel config e il provider senza credenziali verrà saltato al momento dell'uso.
        var path = Path.Combine(Path.GetTempPath(), $"providers-{Guid.NewGuid():N}.json");
        var envName = $"GMBASIC_MISSING_VAR_{Guid.NewGuid():N}";
        var json = $$"""
                     {
                       "version": 1,
                       "providers": {
                         "openai-demo": {
                           "role": "completion",
                           "kind": "openai",
                           "config": { "apiKey": "$env:{{envName}}", "model": "gpt-4o-mini" }
                         }
                       },
                       "roleOrder": { "completion": ["openai-demo"] },
                       "activeIndex": { "completion": 0 }
                     }
                     """;
        File.WriteAllText(path, json);

        // Non deve lanciare: l'env var mancante lascia il placeholder, non rompe il Load.
        var act = () => ProviderServer.Instance.Load(path);
        act.Should().NotThrow();
    }
}
