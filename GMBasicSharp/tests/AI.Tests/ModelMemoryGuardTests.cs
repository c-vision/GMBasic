// FILE: AI.Tests.ModelMemoryGuardTests
// Guardia di memoria dei caricamenti: il ramo swap (pesi lazy su disco) NON deve chiedere il
// modello intero. Repro del bug "(open) - AI-MLX-Swap-Guardia-FullLoad-Prima-Del-Flag": con
// l'opzione "swap":"true" letta DOPO la guardia, il caso per cui lo swap esiste — modello più
// grande della RAM — veniva rifiutato con un messaggio che negava lo swap.
using Basic.Providers;
using FluentAssertions;

namespace AI.Tests;

/// <summary>Memoria iniettabile (stesso pattern di <see cref="ModelResidencyRegistry"/>): le
/// asserzioni non dipendono dallo stato di RAM della macchina, e il "modello" è solo un numero —
/// nessun caricamento, nessun file da creare, nessun test massivo.</summary>
[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class ModelMemoryGuardTests : IDisposable
{
    private const long Gib = 1L << 30;
    private readonly Func<ulong> _origAvailable = ModelMemoryGuard.AvailableBytes;

    public void Dispose() => ModelMemoryGuard.AvailableBytes = _origAvailable;

    private static void Given(ulong available) => ModelMemoryGuard.AvailableBytes = () => available;

    private const string Provider = "mlx provider 'sonda'";
    private const string Path200Gb = "/modelli/grande";
    private const long ModelBytes = 200L * Gib;
    private const int Ctx = 8192;

    [Fact(DisplayName = "swap attivo: un modello più grande della RAM non è rifiutato (è il caso per cui lo swap esiste)")]
    public void EnsureLoad_LazyWeights_ModelBiggerThanRam_IsNotRefused()
    {
        // 200 GB di pesi su 40 GB di RAM: full-load li chiederebbe tutti, lazy chiede ~25 GB di
        // working set + overhead + KV. Se la guardia non sapesse dello swap, qui si lancerebbe.
        Given(40 * Gib);
        var act = () => ModelMemoryGuard.EnsureLoad(Provider, Path200Gb, ModelBytes, Ctx, lazyWeights: true);
        act.Should().NotThrow("con i pesi lazy il SO paga da disco solo una frazione dei pesi");
    }

    [Fact(DisplayName = "swap NON richiesto: il requisito full-RAM resta (il default non cambia)")]
    public void EnsureLoad_FullWeights_StillRefusesModelBiggerThanRam()
    {
        Given(40 * Gib);
        var act = () => ModelMemoryGuard.EnsureLoad(Provider, Path200Gb, ModelBytes, Ctx, lazyWeights: false);
        act.Should().Throw<InvalidOperationException>()
           .WithMessage("*memoria INSUFFICIENTE*")
           // La frase "nessuno swap" era la disinformazione del bug: il messaggio deve invece NOME
           // L'OPZIONE che sblocca il caso, non mandare l'utente a cercare un modello più piccolo.
           .WithMessage("*\"swap\":\"true\"*")
           .Which.Message.Should().NotContain("nessuno swap");
    }

    [Fact(DisplayName = "swap attivo sotto pressione vera: il rifiuto arriva e non nega lo swap")]
    public void EnsureLoad_LazyWeights_StillRefusedWhenHotSetDoesNotFit()
    {
        // Lo swap non rende il modello gratis: se nemmeno il working set caldo entra, la guardia
        // deve parlare (un rifiuto mancato sarebbe il difetto opposto).
        Given(4 * Gib);
        var act = () => ModelMemoryGuard.EnsureLoad(Provider, Path200Gb, ModelBytes, Ctx, lazyWeights: true);
        act.Should().Throw<InvalidOperationException>()
           .Which.Message.Should().NotContain("nessuno swap")
           .And.Contain("working set");
    }

    [Fact(DisplayName = "memoria non determinabile (0): la guardia non rifiuta in nessuno dei due modi")]
    public void EnsureLoad_UnknownMemory_NeverRefuses()
    {
        Given(0);
        var lazy = () => ModelMemoryGuard.EnsureLoad(Provider, Path200Gb, ModelBytes, Ctx, lazyWeights: true);
        var full = () => ModelMemoryGuard.EnsureLoad(Provider, Path200Gb, ModelBytes, Ctx, lazyWeights: false);
        lazy.Should().NotThrow("0 = misura indisponibile, non 'RAM vuota'");
        full.Should().NotThrow("invariante preesistente: senza misura la guardia tace");
    }

    [Fact(DisplayName = "caricamento mmap (llama.cpp / GGUF): il minimo è overhead+KV, mai i pesi")]
    public void EnsureMinimalLoad_DoesNotCountWeights()
    {
        Given(3 * Gib);
        var act = () => ModelMemoryGuard.EnsureMinimalLoad(Provider, Path200Gb, Ctx);
        act.Should().NotThrow("i pesi restano nel file mappato: il minimo è overhead + KV");

        Given(1 * Gib);
        act.Should().Throw<InvalidOperationException>()
           .Which.Message.Should().Contain("libera memoria");
    }

    [Fact(DisplayName = "working set dei pesi lazy: una sola frazione, non negativa, sotto il modello intero")]
    public void HotSetBytes_IsAFractionOfTheModel()
    {
        ModelMemoryGuard.HotSetBytes(ModelBytes).Should().Be(25 * Gib);
        ModelMemoryGuard.HotSetBytes(ModelBytes).Should().BeLessThan(ModelBytes);
        ModelMemoryGuard.HotSetBytes(-1).Should().Be(0, "un modello di dimensione ignota non deve chiedere nulla");
    }
}
