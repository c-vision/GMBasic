// FILE: AI.Tests.Ds4MlxRoutingTests
// Routing GGUF→motore MLX embedded (DS4/DeepSeek V4 Flash) e chat template DeepSeek V4.
using Basic.ModelDownload;
using Basic.Providers;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class Ds4MlxRoutingTests
{
    // Nessun path personale: il modello si indica con GMB_DS4_GGUF, altrimenti i test si saltano.
    private static string Ds4Gguf =>
        Environment.GetEnvironmentVariable("GMB_DS4_GGUF") ?? "";

    [Fact(DisplayName = "DetectConfig instrada il GGUF DS4 (arch deepseek4) al motore MLX, non a llama.cpp")]
    public void DetectConfig_Ds4Gguf_RoutesToMlx()
    {
        // Skip morbido: il modello (o la lib nativa aggiornata) può non essere presente in CI.
        if (string.IsNullOrEmpty(Ds4Gguf) || !File.Exists(Ds4Gguf)) return;
        int supported;
        try { supported = MlxEngineNative.llm_ai_mlx_gguf_supported(Ds4Gguf); }
        catch { return; }
        if (supported != 1) return;

        var cfg = ModelScanner.DetectConfig(Ds4Gguf);

        cfg.Kind.Should().Be("mlx");
        cfg.Config["modelDir"].Should().Be(Ds4Gguf);
    }

    [Fact(DisplayName = "Chat template DeepSeek V4 (DS4): BOS + <｜User｜>/<｜Assistant｜>, EOS dedicato")]
    public void ChatTranscripts_BuildDeepSeek_Format()
    {
        var t = ChatTranscripts.Resolve("deepseek_v4");
        t.DefaultEosToken.Should().Be("<｜end▁of▁sentence｜>");

        var s = t.Build([("user", "Ciao")], addAssistantOpener: true);
        s.Should().StartWith("<｜begin▁of▁sentence｜><｜User｜>Ciao<｜Assistant｜>");

        // un turno assistant chiude con <｜end▁of▁sentence｜>
        var s2 = t.Build([("user", "Ciao"), ("assistant", "Ehi")], addAssistantOpener: false);
        s2.Should().Be("<｜begin▁of▁sentence｜><｜User｜>Ciao<｜Assistant｜>Ehi<｜end▁of▁sentence｜>");
    }
}
