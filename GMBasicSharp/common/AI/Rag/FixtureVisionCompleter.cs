// FILE: Basic.AI.Rag.FixtureVisionCompleter
namespace Basic.AI.Rag;

/// <summary>Vision completer deterministico per dev/xUnit/headless — nessun modello reale.</summary>
/// <remarks>Descrive l'immagine in base al prompt: se il prompt contiene 'REAL TEXT' estrae
/// la stringa richiesta, altrimenti una risposta fissa per il loop computer-use (D1).</remarks>
public sealed class FixtureVisionCompleter : IVisionCompleter
{
    public string ModelName => "fixture-vision-v1";

    public string Complete(string systemPrompt, string userPrompt)
    {
        // Reindirizza a DescribeImage per univocità di comportamento nel fixture.
        return $"[vision: {userPrompt}] (fixture)";
    }

    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {
        ArgumentNullException.ThrowIfNull(imageBytes);
        // D1 (loop computer-use): il prompt chiede l'azione successiva; il fixture risponde
        // deterministicamente con il prossimo step atteso dal sample.
        if (prompt.Contains("NEXT STEP", StringComparison.Ordinal))
            return "click #btn";
        if (prompt.Contains("REAL TEXT", StringComparison.Ordinal))
            return "fixture-vision";
        // E8 (Visual QA): il prompt ASSERT chiede un verdetto booleano. "ASSERT:FALSE" →
        // falso deterministico (per testare il ramo FALLITO), altrimenti vero.
        if (prompt.Contains("ASSERT", StringComparison.Ordinal))
            return prompt.Contains("ASSERT:FALSE", StringComparison.Ordinal) ? "false" : "true";
        return $"[vision {format} {imageBytes.Length}b: {prompt}] (fixture)";
    }
}
