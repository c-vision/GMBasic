// FILE: Interpreter.Parser.Advisories.cs
// Advisory DB: recupero del JSON dal registry (GET /api/advisories).
// HTTP, audit (match versione ↔ range) e deserializzazione sono DELEGATI al C
// (gmb_net_http_request / gmb_pkg_audit): qui non resta alcuna logica C#.

namespace Basic;

/// <summary>Recupero del DB advisory dal registry (HTTP + audit nel runtime C).</summary>
public static class AdvisoryDb
{
    /// <summary>Scarica gli advisory dal registry come JSON grezzo (delega al C). Null se irraggiungibile.</summary>
    public static string? LoadRegistryJson(string registryUrl)
        => NativePkg.RegistryGet($"{registryUrl.TrimEnd('/')}/api/advisories");
}
