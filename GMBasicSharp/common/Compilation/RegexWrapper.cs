// FILE: Basic.Compilation.RegexWrapper
// Wrapper REGEX nativo (parità semantica: System.Text.RegularExpressions — la P-CODE VM
// gestisce RegexType come no-op, qui l'implementazione è reale). In COMPILATION perché è
// condiviso tra Linker (inferenza dei tipi dei metodi) e IlBackend (emissione).
using System.Text.RegularExpressions;

namespace Basic.RegexNative;

public sealed class RegexWrapper
{
    private Regex? _regex;
    public string Pattern
    {
        get => _regex?.ToString() ?? "";
        set => _regex = new Regex(value ?? "");
    }

    private Regex Ensure() => _regex ??= new Regex("");

    public double IsMatch(string? input) => Ensure().IsMatch(input ?? "") ? -1 : 0;
    public RegexMatchValue Match(string? input)
    {
        var m = Ensure().Match(input ?? "");
        return new RegexMatchValue(m.Success ? m.Value : "", m.Index, m.Length, m.Success ? -1 : 0);
    }
    public List<RegexMatchValue> Matches(string? input)
    {
        var result = new List<RegexMatchValue>();
        foreach (Match m in Ensure().Matches(input ?? ""))
            result.Add(new RegexMatchValue(m.Value, m.Index, m.Length, -1));
        return result;
    }
    public string Replace(string? input, string? replacement) => Ensure().Replace(input ?? "", replacement ?? "");
    public string[] Split(string? input) => Ensure().Split(input ?? "");
}

public sealed record RegexMatchValue(string Value, int Index, int Length, double Success);
