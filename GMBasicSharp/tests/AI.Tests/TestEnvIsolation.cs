// FILE: AI.Tests.TestEnvIsolation
// Isolamento GLOBALE del .env reale per l'intero assembly di test: qualunque test che
// registra/salva un provider con una config "sensibile" (apiKey/token/secret/password) fa
// scattare ProviderRegistryPersistence.RedactSecrets -> CommonLib.EnvFile.Set, che senza
// questo isolamento scriverebbe nel .env REALE del repo (bug osservato 2026-09-07 con
// RagBuildTests.ResolveSecrets_MissingEnv_DoesNotThrow, che salva un provider fittizio con
// una apiKey letterale). Un [ModuleInitializer] gira una sola volta, PRIMA di qualunque test,
// e reindirizza EnvFile a un file temp per l'intera durata del run — nessuna classe di test
// deve occuparsene singolarmente.
using System.Runtime.CompilerServices;

namespace AI.Tests;

internal static class TestEnvIsolation
{
    [ModuleInitializer]
    public static void Init()
    {
        var path = Path.Combine(Path.GetTempPath(), $"ai-tests-{Guid.NewGuid():N}.env");
        CommonLib.EnvFile.OverridePathForTests(path);
    }
}
