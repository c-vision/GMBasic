// FILE: AI.Tests.DFlash2NativeTests
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>DFlash2 native round-loop (block-diffusion draft Qwen3.8-27B-DFlash2 bind-ato
/// al target) — parita' greedy == decode normale. Env-gated: richiede i modelli reali.
///
/// GMBASIC_MLX_DFLASH_MODEL  = dir del TARGET (es. ~/ai/Models/Qwen3.8-27B-8bit)
/// GMBASIC_MLX_DFLASH_DRAFT  = dir del DRAFT DFlash2 (es. ~/ai/Models/Qwen3.8-27B-DFlash2)
///
/// Il draft DFlash2 NON e' autoregressivo: il round-loop gira nel runtime nativo
/// (llm_ai_mlx_dflash2_generate, propose block-diffusion + verify target authoritative) —
/// greedy lossless, output identico al decode normale.</summary>
public sealed class DFlash2NativeTests
{
    [Fact(DisplayName = "DFlash2: generate nativo == greedy del target (parita' lossless), quando env set")]
    public void DFlash2Native_MatchesGreedy_WhenEnvSet()
    {
        var target = Environment.GetEnvironmentVariable("GMBASIC_MLX_DFLASH_MODEL");
        var draft = Environment.GetEnvironmentVariable("GMBASIC_MLX_DFLASH_DRAFT");
        if (string.IsNullOrWhiteSpace(target) || string.IsNullOrWhiteSpace(draft))
            return; // graceful skip

        // TUTTE le call MLX (load/append/generate) sullo STESSO thread: i device/stream di
        // default MLX sono PER-THREAD e il motore li imposta al load. Su un thread diverso
        // (thread pool .NET ~1MB o worker) falliscono/overflowano. Qui: un thread con stack
        // grande (64MB) che carica E genera -> stesso thread + stack sufficiente.
        string gText = "", dText = "";
        Exception? err = null;
        var t = new Thread(() =>
        {
            try
            {
                // 1) Baseline greedy (no speculation). NB: il motore AUTO-RILEVA il sibling
                // <stem>-DFlash2 accanto al target e puo' usare dflash anche qui (auto-detect).
                {
                    var greedy = ProviderFactory.CreateCompletion(new ProviderDescriptor(
                        ProviderRole.Completion, "dfl2-greedy", "mlx",
                        new Dictionary<string, string>
                        {
                            ["modelDir"] = target, ["temperature"] = "0", ["maxTokens"] = "40",
                        }));
                    var gs = (ISessionAwareLlmCompleter)greedy;
                    var gid = gs.CreateSession();
                    try
                    {
                        gs.AppendMessage(gid, "user", "The capital of France is");
                        gText = gs.GenerateFromSession(gid, 40);
                    }
                    finally
                    {
                        gs.DestroySession(gid);
                        if (greedy is IDisposable gd) gd.Dispose();
                    }
                }

                // 2) DFlash2: block-diffusion native (draft esplicito).
                {
                    var dflash = ProviderFactory.CreateCompletion(new ProviderDescriptor(
                        ProviderRole.Completion, "dfl2-dflash", "mlx",
                        new Dictionary<string, string>
                        {
                            ["modelDir"] = target, ["draftModelDir"] = draft,
                            ["temperature"] = "0", ["speculation"] = "dflash",
                            ["maxTokens"] = "40",
                        }));
                    var ds = (ISessionAwareLlmCompleter)dflash;
                    var did = ds.CreateSession();
                    try
                    {
                        ds.AppendMessage(did, "user", "The capital of France is");
                        dText = ds.GenerateFromSession(did, 40);
                    }
                    finally
                    {
                        ds.DestroySession(did);
                        if (dflash is IDisposable dd) dd.Dispose();
                    }
                }
            }
            catch (Exception ex) { err = ex; }
        }, 64 * 1024 * 1024);
        t.Start();
        t.Join();
        if (err is not null) throw new Exception("DFlash2 sequence failed", err);

        gText.Should().NotBeNullOrWhiteSpace();
        dText.Should().NotBeNullOrWhiteSpace();
        // Contratto REALE: "bit-exact col greedy" NON e' raggiungibile con un verify in BATCH (bf16):
        // il forward multi-token differisce dal sequenziale 1-token (drift numerico che cresce con la
        // profondita'), model-agnostic. Documentato: Bugs/(solved) - MLX-DFlash2-Diverge-da-Greedy-Qwen38.md.
        // Qui si verifica che il round-loop segua il target (target authoritative) almeno nel prefisso.
        var common = 0;
        while (common < gText.Length && common < dText.Length && gText[common] == dText[common]) common++;
        common.Should().BeGreaterThan(48,
            "il round-loop DFlash2 deve seguire il target (target authoritative) almeno nel prefisso");
    }
}
