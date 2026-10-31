// ConversionLib/Evaluator.cs

namespace Basic.ConversionLib;

public sealed record EvalIssue(string Category, string Severity, string Detail);
public sealed record EvalResult(int Score, string Grade, List<EvalIssue> Issues, TimeSpan Latency, long OutputBytes);

public static class Evaluator
{
    public static EvalResult Evaluate(string markdown, string sourceFormat, TimeSpan latency)
    {
        var issues = new List<EvalIssue>();
        int score = 100;

        if (string.IsNullOrWhiteSpace(markdown))
        {
            issues.Add(new("Content", "Critical", "Empty output"));
            return new(0, "F", issues, latency, 0);
        }

        // Heuristic checks
        if (markdown.Length < 10)
        { score -= 40; issues.Add(new("Content", "High", "Output too short (< 10 chars)")); }

        if (markdown.Contains("�") || markdown.Contains("□"))
        { score -= 20; issues.Add(new("Encoding", "Medium", "Encoding replacement chars detected")); }

        if (CountChar(markdown, '\0') > 0)
        { score -= 30; issues.Add(new("Content", "High", "Null bytes in output")); }

        // Too many consecutive whitespace lines
        if (markdown.Split('\n').Count(l => string.IsNullOrWhiteSpace(l)) > markdown.Split('\n').Length * 0.6)
        { score -= 15; issues.Add(new("Structure", "Low", ">60% blank lines - possible extraction issue")); }

        // Check for common formatting
        bool hasHeaders = markdown.Contains("\n#") || markdown.StartsWith("#");
        bool hasLinks = markdown.Contains("](");
        bool hasTables = markdown.Contains("| ---") || markdown.Contains("|---");

        var grade = score switch { >= 90 => "A", >= 75 => "B", >= 50 => "C", >= 25 => "D", _ => "F" };

        return new EvalResult(Math.Max(0, score), grade, issues, latency, markdown.Length);
    }

    private static int CountChar(string s, char c) => s.Count(ch => ch == c);
}
