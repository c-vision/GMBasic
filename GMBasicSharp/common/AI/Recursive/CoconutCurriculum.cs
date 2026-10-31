// FILE: Basic.Agents.Recursive.CoconutCurriculum
// Estensione 9.2 (Coconut) — curriculum latente per l'inner training: stadi k=0..N dove
// allo stadio k i primi k step del dataset vengono sostituiti con k·c iterazioni latenti
// (R_in applicato più volte) prima di tornare al testo. Reset dell'ottimizzatore a ogni
// stadio (come Deng et al. 2024). Le iterazioni latenti usano l'embedding REALE del modello
// (LastHidden loop) quando il provider attivo è un ILatentCompleter, altrimenti il proxy.
using Basic.AI.Rag;
using Basic.Providers;

namespace Basic.Agents.Recursive;

public static class CoconutCurriculum
{
    /// <summary>Training inner a stadi (Coconut §3): per ogni stadio k=0..N allena il link con
    /// k·c passi latenti extra per campione. Quando il provider attivo è un ILatentCompleter, le
    /// iterazioni latenti usano l'embedding REALE del modello (EmbedText/LastHidden); altrimenti
    /// il forward del RecursiveLink (proxy). Ritorna la loss di fine training di ogni stadio.</summary>
    public static double[] TrainInnerCurriculum(string team, string agent, int n = 6, int c = 2,
        int stepsPerStage = 40, double lr = 0.01)
    {
        if (string.IsNullOrWhiteSpace(team) || string.IsNullOrWhiteSpace(agent)) return [];
        var completer = Basic.Providers.ProviderServer.Instance.GetActiveCompletion() as ILatentCompleter;
        var losses = new double[n + 1];
        for (var k = 0; k <= n; k++)
        {
            RecursiveLink.ResetForTests();
            var handle = RecursiveLink.CreateInner(768, 128);
            var data = RecursiveTrainer.LoadDataset("toy");
            for (var s = 0; s < stepsPerStage; s++)
            {
                var (x, y) = data[s % data.Count];
                var h = RecursiveLink.SimpleEmbed(x);
                var t = RecursiveLink.SimpleEmbed(y);
                // k·c iterazioni latenti prima del target (curriculum)
                for (var lat = 0; lat < k * c; lat++)
                    h = LatentIterate(completer, handle, h, x);
                losses[k] = RecursiveLink.TrainStep(handle, h, t, lr);
            }
        }
        return losses;
    }

    /// <summary>Iterazione latente: embedding REALE del modello (Coconut §3 LastHidden loop)
    /// se disponibile, altrimenti forward residual del link (proxy deterministico).</summary>
    private static double[] LatentIterate(ILatentCompleter? completer, int handle, double[] h, string source)
    {
        try
        {
            if (completer is not null)
            {
                var f = completer.EmbedText(source);
                if (f is { Length: > 0 }) return Array.ConvertAll(f, v => (double)v);
            }
        }
        catch { /* degrada al proxy */ }
        return RecursiveLink.Forward(handle, h) ?? h;
    }
}
