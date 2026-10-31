// Test della sezione "compiler" dei settings: default GLOBALE (system.settings.json)
// sovrascrivibile dall'APPLICAZIONE (app.settings.json), con semantica di sostituzione
// per chiave. Il punto delicato — e il motivo di questi test — è che "chiave assente" e
// "chiave dichiarata col suo default" devono restare DISTINGUIBILI: un app.settings.json
// che dichiara solo `cflags` non deve perdere un `cc` globale custom.

using CommonLib;
using Xunit;

namespace Basic.IlBackend.Tests;

public class CompilerSettingsTests
{
    private static CompilerSettings Sys(string cc = "cc", string[]? cflags = null,
        string[]? ldflags = null, string[]? defines = null)
        => new()
        {
            Cc = cc,
            CFlags = cflags is null ? ["-std=c99", "-O2", "-w"] : [.. cflags],
            LdFlags = ldflags is null ? [] : [.. ldflags],
            Defines = defines is null ? [] : [.. defines]
        };

    [Fact]
    public void NessunOverride_IlDefaultGlobaleValeIntero()
    {
        var r = CompilerSettings.Resolve(Sys(cflags: ["-O3"]), null);
        Assert.Equal("cc", r.Cc);
        Assert.Equal(["-O3"], r.CFlags);
        Assert.Empty(r.LdFlags);
        Assert.Empty(r.Defines);
    }

    [Fact]
    public void AppAssente_UsaIDefaultDelRecord()
    {
        var r = CompilerSettings.Resolve(null, null);
        Assert.Equal("cc", r.Cc);
        Assert.Equal(["-std=c99", "-O2", "-w"], r.CFlags);
    }

    [Fact]
    public void OverrideSoloCflags_ConservaIlCcGlobale()
    {
        // Il caso che motiverebbe un bug silenzioso: `cc` globale custom + app che tocca
        // solo i cflags. Se l'override fosse per-oggetto, `cc` tornerebbe a "cc".
        var r = CompilerSettings.Resolve(
            Sys(cc: "/opt/llvm/bin/clang", cflags: ["-O2"], ldflags: ["-flto"]),
            new CompilerSettingsOverride { CFlags = ["-O3", "-march=native"] });

        Assert.Equal("/opt/llvm/bin/clang", r.Cc);   // conservato
        Assert.Equal(["-O3", "-march=native"], r.CFlags);         // sostituito, NON unito
        Assert.Equal(["-flto"], r.LdFlags);                        // conservato
    }

    [Fact]
    public void ListaVuota_DisattivaIFlagGlobali()
    {
        var r = CompilerSettings.Resolve(Sys(cflags: ["-O2", "-w"]), new CompilerSettingsOverride { CFlags = [] });
        Assert.Empty(r.CFlags);
    }

    [Fact]
    public void CcCustom_NelSoloOverride()
    {
        var r = CompilerSettings.Resolve(Sys(), new CompilerSettingsOverride { Cc = "  gcc-14  " });
        Assert.Equal("gcc-14", r.Cc);   // trimmed
    }

    [Fact]
    public void CcVuoto_NelSoloOverride_ConservaIlGlobale()
    {
        var r = CompilerSettings.Resolve(Sys(cc: "clang-18"), new CompilerSettingsOverride { Cc = "   " });
        Assert.Equal("clang-18", r.Cc);
    }

    [Fact]
    public void Defines_HannoLaStessaSemanticaDiCflags()
    {
        var r = CompilerSettings.Resolve(
            Sys(defines: ["-DHAVE_OPENSSL"]),
            new CompilerSettingsOverride { Defines = ["-DNDEBUG"], LdFlags = ["-flto"] });
        Assert.Equal(["-DNDEBUG"], r.Defines);
        Assert.Equal(["-flto"], r.LdFlags);
    }

    [Fact]
    public void Resolve_NonMutaIlDefaultGlobale()
    {
        var s = Sys(cflags: ["-O2"]);
        var r = CompilerSettings.Resolve(s, new CompilerSettingsOverride { CFlags = ["-O0"] });
        r.CFlags.Add("-flto");                       // il risultato è una copia
        Assert.Equal(["-O2"], s.CFlags);
    }

    [Fact]
    public void LineePerCommandLine_HannoIlSpazioDavantiERestanoVuoteSeNonCiSono()
    {
        var r = CompilerSettings.Resolve(Sys(cflags: ["-O3"], ldflags: [], defines: ["-DX=1"]), null);
        Assert.Equal(" -O3", r.CFlagsLine);
        Assert.Equal("", r.LdFlagsLine);
        Assert.Equal(" -DX=1", r.DefinesLine);
    }

    [Fact]
    public void SystemSettingsAssenti_UsaIDefaultDelRecord()
    {
        var s = new SystemSettings();
        var r = CompilerSettings.Resolve(s.Compiler, null);
        Assert.Equal(["-std=c99", "-O2", "-w"], r.CFlags);
    }

    // ── Livello 3: override one-shot da riga di comando (vince sui settings) ────────────

    [Fact]
    public void CliVinceSullAppEGlobalaleETollApp()
    {
        var r = CompilerSettings.Resolve(
            Sys(cc: "clang-globale", cflags: ["-O2"], ldflags: ["-flto-globale"]),
            new CompilerSettingsOverride { CFlags = ["-O1"] },
            new CompilerSettingsOverride { CFlags = ["-O3", "-march=native"] });

        Assert.Equal(["-O3", "-march=native"], r.CFlags);   // la CLI vince
        Assert.Equal("clang-globale", r.Cc);                // non dichiarato in CLI → conservato
        Assert.Equal(["-flto-globale"], r.LdFlags);
    }

    [Fact]
    public void CliToccaUnaChiaveLeAltreRestano()
    {
        // Il caso d'uso reale del gate: provare -O3 senza toccare il cc né i ldflags dei settings.
        var r = CompilerSettings.Resolve(
            Sys(cc: "/opt/llvm/bin/clang", ldflags: ["-flto"]),
            null,
            new CompilerSettingsOverride { CFlags = ["-O0", "-g"] });

        Assert.Equal("/opt/llvm/bin/clang", r.Cc);
        Assert.Equal(["-flto"], r.LdFlags);
        Assert.Equal(["-O0", "-g"], r.CFlags);
    }

    [Fact]
    public void CliENull_SiFallaSuiSettings()
    {
        var r = CompilerSettings.Resolve(
            Sys(cflags: ["-O2"]),
            new CompilerSettingsOverride { CFlags = ["-O1"] },
            cli: null);
        Assert.Equal(["-O1"], r.CFlags);
    }

    [Fact]
    public void CliConListaVuota_DisattivaAncheQuelloDellApp()
    {
        var r = CompilerSettings.Resolve(
            Sys(cflags: ["-O2", "-w"]),
            new CompilerSettingsOverride { CFlags = ["-O1"] },
            new CompilerSettingsOverride { CFlags = [] });
        Assert.Empty(r.CFlags);
    }

    [Fact]
    public void ApplyNonMutaIlProfiloDiBase()
    {
        var b = Sys(cflags: ["-O2"]);
        var r = CompilerSettings.Apply(b, new CompilerSettingsOverride { CFlags = ["-O0"] });
        r.CFlags.Add("-flto");
        Assert.Equal(["-O2"], b.CFlags);
    }
}
