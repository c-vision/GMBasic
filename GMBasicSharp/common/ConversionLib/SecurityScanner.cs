// ConversionLib/SecurityScanner.cs
using System.Text.RegularExpressions;

namespace Basic.ConversionLib;

public sealed record SecurityIssue(string Type, string Severity, string Description, int? Line = null);
public sealed record SecurityResult(bool Clean, List<SecurityIssue> Issues);

public static class SecurityScanner
{
    private static readonly (string Pattern, string Type, string Severity)[] _rules =
    [
        (@"-----BEGIN\s+(RSA|DSA|EC|OPENSSH|PGP)\s+PRIVATE\s+KEY", "PrivateKey", "Critical"),
        (@"AIza[0-9A-Za-z\-_]{35}", "Google API Key", "High"),
        (@"sk-[A-Za-z0-9]{32,}", "OpenAI API Key", "High"),
        (@"AKIA[0-9A-Z]{16}", "AWS Access Key", "High"),
        (@"github_pat_[A-Za-z0-9_]{36,}", "GitHub PAT", "High"),
        (@"ghp_[A-Za-z0-9]{36}", "GitHub Token", "High"),
        (@"eyJ[A-Za-z0-9\-_]+\.eyJ[A-Za-z0-9\-_]+\.[A-Za-z0-9\-_]+", "JWT Token", "Medium"),
        (@"<script\b[^>]*>", "XSS Script", "High"),
        (@"javascript\s*:", "Javascript URL", "Medium"),
        (@"[\x00-\x08\x0B\x0C\x0E-\x1F]", "Control Character", "Low"),
    ];

    public static SecurityResult Scan(string markdown)
    {
        var issues = new List<SecurityIssue>();
        var lines = markdown.Split('\n');

        for (int i = 0; i < lines.Length; i++)
        {
            foreach (var (pattern, type, severity) in _rules)
            {
                if (Regex.IsMatch(lines[i], pattern, RegexOptions.IgnoreCase))
                    issues.Add(new SecurityIssue(type, severity, $"Line {i + 1}: {type}", i + 1));
            }
        }

        return new SecurityResult(issues.Count == 0, issues);
    }
}
