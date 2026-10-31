using Basic.AI.Eval;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class VaultValidatorTests : IDisposable
{
    private readonly string _dir;

    public VaultValidatorTests()
    {
        _dir = Path.Combine(Path.GetTempPath(), "gmb-uat-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_dir);
    }

    public void Dispose()
    {
        try { Directory.Delete(_dir, true); } catch { }
    }

    [Fact]
    public void Validate_ConformingResult_Passes()
    {
        File.WriteAllText(Path.Combine(_dir, "req.md"),
            "# Requisiti\n- Il sistema deve supportare il login utente\n- Deve calcolare il totale degli ordini\n");
        var v = VaultValidator.Validate("il sistema supportare dell utente esegue il login e deve calcolare il totale degli ordini", _dir);
        v.Ok.Should().BeTrue();
        v.Failed.Should().BeEmpty();
        v.Passed.Should().HaveCount(2);
    }

    [Fact]
    public void Validate_MissingRequirement_FailsExplicitly()
    {
        File.WriteAllText(Path.Combine(_dir, "req.md"),
            "# Requisiti\n- Il sistema deve supportare il login utente\n- Deve calcolare il totale degli ordini\n");
        var v = VaultValidator.Validate("il componente deve calcolare il totale degli ordini, ma il flusso resta incompleto", _dir);
        v.Ok.Should().BeFalse();
        v.Failed.Should().ContainSingle(r => r.Contains("login"));
    }

    [Fact]
    public void Validate_EmptyFolder_NotOk()
    {
        var v = VaultValidator.Validate("qualunque cosa", _dir);
        v.Ok.Should().BeFalse();
        v.Failed.Should().BeEmpty();
    }

    [Fact]
    public void ValidateLines_WorksWithBrainRetrievedLines()
    {
        // Seam brain/RAG: le requirement-line arrivano da un retrieval esterno (vault/SecondBrain)
        // e il motore di conformità è lo stesso del percorso file.
        var v = VaultValidator.ValidateLines(
            "il motore supportare il login dell utente e deve calcolare il totale degli ordini",
            ["Il sistema deve supportare il login utente", "Deve calcolare il totale degli ordini"]);
        v.Ok.Should().BeTrue();
        v.Passed.Should().HaveCount(2);
    }

    [Fact]
    public void ToJson_SerializesVerdict()
    {
        var v = VaultValidator.ValidateLines(
            "il sistema deve supportare il login utente",
            ["Il sistema deve supportare il login utente"]);
        var json = VaultValidator.ToJson(v);
        json.Should().Contain("\"ok\":true");
        json.Should().Contain("\"passed\":");
        json.Should().Contain("\"failed\":[]");
    }

    [Fact]
    public void TermsOf_FiltersStopwordsAndShort()
    {
        var t = VaultValidator.TermsOf("Il sistema deve supportare il login");
        t.Should().Contain("login");
        t.Should().NotContain("il").And.NotContain("deve");
    }
}
