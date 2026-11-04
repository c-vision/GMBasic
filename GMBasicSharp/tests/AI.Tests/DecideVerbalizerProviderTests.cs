using Basic.Decision;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Provider `%DECIDE` distinguibile (T8): `AI REGISTER "jev", %DECIDE, "verbalizer", {...}`.
/// Senza modello reale: risoluzione dell'alias e profilo di decisione.</summary>
[Trait("Category", "Fast")]
public sealed class DecideVerbalizerProviderTests : IDisposable
{
    public DecideVerbalizerProviderTests() { ProviderServer.ResetForTests(); }
    public void Dispose() { ProviderServer.ResetForTests(); }

    [Fact]
    public void Alias_OfRulesDecideProvider_IsUsableAsExplicitProvider()
    {
        ProviderServer.Instance.Register("decide", "myrules", "rules");
        var viaAlias = Basic.IlBackend.IlRuntime.AiDecide("sì", "NOUL", "myrules");
        var viaKind = Basic.IlBackend.IlRuntime.AiDecide("sì", "NOUL", "rules");
        viaAlias.Should().Be(viaKind);
    }

    [Fact]
    public void Alias_OfCompletionProvider_IsNotTreatedAsDecider()
    {
        ProviderServer.Instance.Register("completion", "notdecide", "fixture");
        var r = Basic.IlBackend.IlRuntime.AiDecide("sì", "NOUL", "notdecide");
        r.Should().Contain("non valido");
    }

    [Fact]
    public void Verbalizer_WithoutActiveModel_FailsExplicitly()
    {
        ProviderServer.Instance.Register("decide", "jev", "verbalizer",
            new Dictionary<string, string> { ["decide"] = "on" });
        var r = Basic.IlBackend.IlRuntime.AiDecide("sì", "NOUL", "jev");
        r.Should().Contain("\"error\"").And.Contain("local");
    }

    [Fact]
    public void Profile_FromConfig_ReadsOnlyDecideKeys()
    {
        var cfg = new Dictionary<string, string>
        {
            ["decide"] = "off",
            ["decideTemplate"] = "T {state} {options}",
            ["decideCalibration"] = "/tmp/head.json",
            ["modelDir"] = "/ignored",
        };
        var p = DecideProfile.FromConfig(cfg);
        p.Mode.Should().Be("off");
        p.Template.Should().Be("T {state} {options}");
        p.Calibration.Should().Be("/tmp/head.json");
    }

    [Fact]
    public void Profile_Push_IsScopedAndRestoresPrevious()
    {
        DecideProfile.Current.Should().BeNull();
        using (DecideProfile.Push(new DecideProfile("on", null, null)))
        {
            DecideProfile.Current!.Mode.Should().Be("on");
            using (DecideProfile.Push(new DecideProfile("off", null, null)))
                DecideProfile.Current!.Mode.Should().Be("off");
            DecideProfile.Current!.Mode.Should().Be("on");
        }
        DecideProfile.Current.Should().BeNull();
    }
}
