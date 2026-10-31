// ContextEnginePolicyTests.cs — P2 residuo (asse B): la policy del Context Engine è EFFETTIVA.
// Prima: budget/policy erano memorizzati ma nessuno li applicava (SetView salvava il testo grezzo,
// il tool loop ignorava la view). Ora SetView normalizza subito e AgentToolLoop usa la view curata.
using System;
using Basic.AI.Context;
using Xunit;

namespace AI.Tests;

public sealed class ContextEnginePolicyTests : IDisposable
{
    public ContextEnginePolicyTests() => AiContextEngine.Reset();
    public void Dispose() => AiContextEngine.Reset();

    [Fact]
    public void NonConfigurato_NonEUnContextEngineOptIn()
    {
        Assert.False(AiContextEngine.IsConfigured("ag"));
        AiContextEngine.SetBudget("ag", 100);
        Assert.True(AiContextEngine.IsConfigured("ag"));
    }

    [Fact]
    public void ViewEntroBudget_RestaInvariata_EOverflowZero()
    {
        AiContextEngine.SetBudget("ag", 100);
        AiContextEngine.SetView("ag", "ciao");
        Assert.Equal("ciao", AiContextEngine.GetView("ag"));
        Assert.Equal(4, AiContextEngine.GetUsed("ag"));
        Assert.Equal(0, AiContextEngine.GetOverflow("ag"));
        Assert.False(AiContextEngine.WasCompacted("ag"));
    }

    [Fact]
    public void PolicyTruncate_TagliaEBudget_EOverflowReale()
    {
        AiContextEngine.SetBudget("ag", 10);
        AiContextEngine.SetPolicy("ag", "truncate");
        Assert.True(AiContextEngine.SetView("ag", "1234567890123456"));
        Assert.Equal("1234567890", AiContextEngine.GetView("ag"));
        Assert.Equal(10, AiContextEngine.GetUsed("ag"));
        Assert.Equal(6, AiContextEngine.GetOverflow("ag"));   // overflow del testo ORIGINALE
    }

    [Fact]
    public void PolicyCompact_TestaMarcatoreCoda_DiLunghezzaEsattaBudget()
    {
        AiContextEngine.SetBudget("ag", 40);
        AiContextEngine.SetPolicy("ag", "compact");
        var text = new string('a', 108);
        Assert.True(AiContextEngine.SetView("ag", text));
        var view = AiContextEngine.GetView("ag");
        Assert.Equal(40, view.Length);
        Assert.Equal(40, AiContextEngine.GetUsed("ag"));
        Assert.Equal(68, AiContextEngine.GetOverflow("ag"));
        Assert.Contains("compattato", view);
        Assert.StartsWith("aaaaaaaaaaaaaaa", view);   // testa 15 = (40-14)*60%
        Assert.EndsWith("aaaaaaaaaaa", view);         // coda 11
        Assert.True(AiContextEngine.WasCompacted("ag"));
    }

    [Fact]
    public void PolicyError_RifiutaLaView_NonTagliaInSilenzio()
    {
        AiContextEngine.SetBudget("ag", 10);
        AiContextEngine.SetPolicy("ag", "error");
        Assert.False(AiContextEngine.SetView("ag", "1234567890123456"));
        Assert.Equal("", AiContextEngine.GetView("ag"));      // vista NON aggiornata
        Assert.Equal(0, AiContextEngine.GetUsed("ag"));
    }

    [Fact]
    public void BuildView_PolicyError_LanciaEsplicito()
    {
        AiContextEngine.SetBudget("ag", 4);
        AiContextEngine.SetPolicy("ag", "error");
        var ex = Assert.Throws<InvalidOperationException>(() => AiContextEngine.BuildView("ag", "1234567890"));
        Assert.Contains("budget 4", ex.Message);
    }

    [Fact]
    public void BuildView_SenzaViewUsaFallback_SenzaConfigurazioneRestaLegacy()
    {
        AiContextEngine.SetBudget("ag", 5);
        var view = AiContextEngine.BuildView("ag", "1234567890");
        Assert.Equal(5, view.TotalChars);
        Assert.True(view.Truncated);
        // agente non configurato: il loop resta sul transcript legacy (nessuna normalizzazione)
        var legacy = AiContextEngine.BuildView("altro", "1234567890");
        Assert.Equal(10, legacy.TotalChars);
        Assert.False(legacy.Truncated);
    }

    [Fact]
    public void Selector_AccettatoEConservato()
    {
        AiContextEngine.SetSelector("ag", "task,deps,freshness");
        Assert.True(AiContextEngine.IsConfigured("ag"));
    }
}
