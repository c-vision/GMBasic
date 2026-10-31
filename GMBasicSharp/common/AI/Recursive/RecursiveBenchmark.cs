// FILE: Basic.Agents.Recursive.RecursiveBenchmark
// Harness gate §7 (18-23): misura accuracy/token/time delle modalità latent su un dataset
// etichettato (samples/Fixtures/ai/*.jsonl). Model-agnostico: usa AgentServer.Ask con il
// provider attivo (fixture nei test senza modello, llama.cpp/MLX quando caricati). Il team
// viene RIUSATO tra i problemi (sessioni persistenti, niente create/destroy per-problema —
// pattern sicuro con i modelli nativi). Accuracy proxy: estrazione dell'ULTIMO numero della
// reference vs dell'output del modello (funziona per MATH-500 \boxed{...} e GSM8K #### NNN).

using System.Collections.Concurrent;
using System.Text.RegularExpressions;

namespace Basic.Agents.Recursive;

public sealed record BenchmarkProblem(string X, string Y, string Output, bool Correct, long TimeMs, int Tokens);

public sealed record BenchmarkReport(string Team, string Mode, int Rounds, int Problems, int Correct, double Accuracy,
    long TotalTimeMs, long TotalTokens, int Asks)
{
    public override string ToString() =>
        $"mode={Mode,-7} r={Rounds} acc={Accuracy:P1} ({Correct}/{Problems}) time={TotalTimeMs}ms tok={TotalTokens} asks={Asks}";
}

public static class RecursiveBenchmark
{
    private static readonly Regex NumberRegex = new(@"-?\d+(?:\.\d+)?", RegexOptions.Compiled);

    /// <summary>Ultimo numero nel testo (intero o decimale), o null.</summary>
    public static double? ExtractLastNumber(string text)
    {
        if (string.IsNullOrEmpty(text)) return null;
        var matches = NumberRegex.Matches(text);
        if (matches.Count == 0) return null;
        var last = matches[^1].Value;
        return double.TryParse(last, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var v) ? v : null;
    }

    /// <summary>Confronto proxy: l'ultimo numero dell'output del modello == quello della reference
    /// (tolleranza relativa 1e-6 per i decimali).</summary>
    public static bool IsCorrect(string reference, string output)
    {
        var a = ExtractLastNumber(reference);
        var b = ExtractLastNumber(output);
        if (a is null || b is null) return false;
        return Math.Abs(a.Value - b.Value) < 1e-6 * Math.Max(1.0, Math.Abs(a.Value));
    }

    /// <summary>Esegue il benchmark su un team ricorrente (RIUSATO tra i problemi).
    /// datasetPath: righe x|y, .json o .jsonl (MATH-500/GSM8K). members: agenti del team.</summary>
    public static BenchmarkReport Run(string datasetPath, string teamName, string mode,
        int rounds = 2, int maxProblems = 20, int members = 2,
        string? innerLinkConfig = null, string? outerLinkConfig = null)
    {
        if (members < 1) members = 1;
        var data = RecursiveTrainer.LoadDataset(datasetPath).Take(maxProblems).ToList();
        if (data.Count == 0) throw new ArgumentException($"Dataset vuoto o non caricabile: {datasetPath}");

        var server = Basic.Agents.AgentServer.Instance;
        var teams = Basic.Agents.TeamServer.Instance;
        for (var i = 0; i < members; i++) server.Create($"{teamName}-{i}");
        teams.Create(teamName);
        for (var i = 0; i < members; i++) teams.AddAgent(teamName, $"{teamName}-{i}");
        teams.SetStrategy(teamName, Basic.Agents.TeamStrategy.Sequential);
        teams.SetRecursion(teamName, rounds);
        teams.SetLatent(teamName, mode);
        if (innerLinkConfig is not null) teams.SetLink(teamName, "inner", innerLinkConfig);
        if (outerLinkConfig is not null) teams.SetLink(teamName, "outer", outerLinkConfig);

        try
        {
            var problems = new List<BenchmarkProblem>();
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var asksBefore = teams.Get(teamName)!.Conversation.Count;
            long totalTokens = 0;
            foreach (var (x, y) in data)
            {
                var psw = System.Diagnostics.Stopwatch.StartNew();
                var team = teams.Get(teamName)!;
                var output = Basic.Agents.TeamOrchestrator.Run(team, x);
                psw.Stop();
                totalTokens += output.Length / 4;
                problems.Add(new BenchmarkProblem(x, y, output, IsCorrect(y, output), psw.ElapsedMilliseconds, output.Length / 4));
            }
            sw.Stop();
            var asks = teams.Get(teamName)!.Conversation.Count - asksBefore;
            var correct = problems.Count(p => p.Correct);
            return new BenchmarkReport(teamName, mode, rounds, problems.Count, correct,
                problems.Count == 0 ? 0 : (double)correct / problems.Count,
                sw.ElapsedMilliseconds, totalTokens, asks);
        }
        finally
        {
            for (var i = 0; i < members; i++) server.Delete($"{teamName}-{i}");
            teams.Delete(teamName);
        }
    }

    /// <summary>Esegue le tre modalità sullo stesso dataset (stesso seed/ordine) e ritorna i report.
    /// link config attivo per embed/native; text senza link. Ordine: text → embed → native.</summary>
    public static List<BenchmarkReport> RunAllModes(string datasetPath, string teamPrefix,
        int rounds = 2, int maxProblems = 20, int members = 2, string linkHidden = "64")
    {
        var cfg = $"{{\"hidden\":{linkHidden}}}";
        return
        [
            Run(datasetPath, teamPrefix + "-text", "text", rounds, maxProblems, members),
            Run(datasetPath, teamPrefix + "-embed", "embed", rounds, maxProblems, members, cfg, cfg),
            Run(datasetPath, teamPrefix + "-native", "native", rounds, maxProblems, members, cfg, cfg),
        ];
    }
}
