// FILE: AI.Tests.AiBenchmarkTests
using Basic.Agents;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Verifica di AI BENCHMARK + AI.BENCHRESULT sul lato runtime
/// (AgentServer.Benchmark / GetBenchResult). Copre i percorsi deterministici che NON
/// richiedono un modello locale: gli errori espliciti (mai crash, mai risultato
/// silenzioso — regola anti-stub). I percorsi di successo richiedono un modello locale
/// session-aware+cache-manager (llamacpp/MLX) e sono verificati end-to-end dal sample
/// samples/Community/Ai/AiBenchmark.bas e dai test env-gated.</summary>
[Trait("Category", "Fast")]
public sealed class AiBenchmarkTests : IDisposable
{
    public AiBenchmarkTests() => AgentServer.ResetForTests();
    public void Dispose() => AgentServer.ResetForTests();

    [Fact]
    public void Benchmark_UnknownAgent_ReturnsExplicitError()
    {
        var res = AgentServer.Instance.Benchmark("nessun-agente", "PREFILL", 1, "x");
        res.Should().Contain("error");
        res.Should().Contain("non risolve a un agente con sessione KV");
    }

    [Fact]
    public void Benchmark_InvalidMode_ReturnsExplicitError()
    {
        AgentServer.Instance.Create("bench");
        var res = AgentServer.Instance.Benchmark("bench", "FAST", 1, "x");
        res.Should().Contain("error");
        res.Should().Contain("PREFILL|DECODE|ALL");
    }

    [Fact]
    public void Benchmark_FixtureAgent_ReturnsSessionError()
    {
        // Un agente creato con il fixture (completer NON session-aware) non puo' avere
        // una sessione KV: Benchmark deve ritornare un errore esplicito, mai uno stub.
        AgentServer.Instance.Create("bench");
        var res = AgentServer.Instance.Benchmark("bench", "PREFILL", 1, "x");
        res.Should().Contain("error");
        res.Should().Contain("non risolve a un agente con sessione KV");
    }

    [Fact]
    public void GetBenchResult_NoBenchmarkYet_ReturnsExplicitError()
    {
        AgentServer.Instance.Create("bench");
        var res = AgentServer.Instance.GetBenchResult("bench");
        res.Should().Contain("error");
        res.Should().Contain("non ha ancora un benchmark");
    }

    [Fact]
    public void GetBenchResult_UnknownAgent_ReturnsExplicitError()
    {
        var res = AgentServer.Instance.GetBenchResult("nessun-agente");
        res.Should().Contain("error");
    }
}
