// FILE: GMBasicSharp/tests/GMBASIC.IlBackend.Tests/BuildFreshnessTests.cs
// Regola ANTI-STALE visibile nel report dei test (la guardia vera gira nel ModuleInitializer
// di BuildFreshness.cs, così vale anche per i run filtrati come il gate di parità).

using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class BuildFreshnessTests
{
    /// <summary>Gli assembly del compilatore caricati dal test project devono essere più
    /// recenti dei loro sorgenti: altrimenti il gate di parità misura codice vecchio
    /// (falso verde/rosso). Fallisce con l'istruzione `dotnet build` da eseguire.</summary>
    [Fact]
    public void Assembly_Non_Stale() => BuildFreshness.AssertFresh();
}
