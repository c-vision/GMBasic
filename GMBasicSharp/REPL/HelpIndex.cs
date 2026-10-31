using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace Basic.Executor;

public class HelpIndex
{
    private readonly List<HelpEntry> _entries = [];
    private readonly Dictionary<string, List<int>> _invertedIndex = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, HelpEntry> _byFullName = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _knownNamespaces = new(StringComparer.OrdinalIgnoreCase);

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    private static readonly Regex CmdLineRegex = new(
        @"^\│(?<name>[^│]+)\│(?<syntax>[^│]+)\│(?<desc>[^│]+)\│",
        RegexOptions.Compiled);

    private static readonly Regex NamespaceRegex = new(
        @"Namespace\s+([A-Za-z_][A-Za-z0-9_.]*)",
        RegexOptions.Compiled | RegexOptions.IgnoreCase);

    public int Count => _entries.Count;

    public static HelpIndex Load(string helpsDir, string commandsPath)
    {
        var index = new HelpIndex();
        index.LoadJsonFiles(helpsDir);
        index.LoadCommandsTxt(commandsPath);
        index.BuildSearchIndex();
        return index;
    }

    private void LoadJsonFiles(string helpsDir)
    {
        if (!Directory.Exists(helpsDir)) return;

        foreach (var file in Directory.GetFiles(helpsDir, "*.json"))
        {
            try
            {
                var json = File.ReadAllText(file);
                var entries = JsonSerializer.Deserialize<List<HelpEntry>>(json, JsonOpts);
                if (entries == null) continue;

                var fileName = Path.GetFileNameWithoutExtension(file);

                foreach (var entry in entries)
                {
                    if (string.IsNullOrWhiteSpace(entry.Name)) continue;

                    if (string.IsNullOrEmpty(entry.Namespace))
                        entry.Namespace = fileName.ToUpperInvariant();

                    if (string.IsNullOrEmpty(entry.Category))
                        entry.Category = entry.Namespace;

                    if (_byFullName.ContainsKey(entry.FullName))
                        continue;

                    _entries.Add(entry);
                    _byFullName[entry.FullName] = entry;
                    _knownNamespaces.Add(entry.Namespace);
                }
            }
            catch { }
        }
    }

    private void LoadCommandsTxt(string path)
    {
        if (!File.Exists(path)) return;

        var lines = File.ReadAllLines(path);
        string? currentNs = null;

        foreach (var line in lines)
        {
            var nsMatch = NamespaceRegex.Match(line);
            if (nsMatch.Success)
            {
                currentNs = ExtractNamespaceFromLine(line);
                continue;
            }

            var cmdMatch = CmdLineRegex.Match(line);
            if (!cmdMatch.Success) continue;

            var name = cmdMatch.Groups["name"].Value.Trim();
            var syntax = cmdMatch.Groups["syntax"].Value.Trim();
            var desc = cmdMatch.Groups["desc"].Value.Trim();

            if (name is "NOME" or "├─" or "└─") continue;
            if (name.StartsWith("TIPO:") || name.StartsWith("COSTANTE:")) continue;

            if (string.IsNullOrEmpty(name) || string.IsNullOrWhiteSpace(name)) continue;

            var ns = string.IsNullOrEmpty(currentNs) ? "" : currentNs;

            var fullName = string.IsNullOrEmpty(ns) ? name : $"{ns}.{name}";

            if (_byFullName.ContainsKey(fullName))
                continue;

            var entry = new HelpEntry
            {
                Name = name,
                Namespace = ns,
                Category = ns,
                Description = desc.EndsWith('.') ? desc : desc + ".",
                Syntax = [CleanSyntax(syntax)],
                ReturnType = ExtractReturnType(desc),
                IsCommand = IsCommandSyntax(syntax, desc)
            };

            _entries.Add(entry);
            _byFullName[fullName] = entry;
            if (!string.IsNullOrEmpty(ns))
                _knownNamespaces.Add(ns);
        }
    }

    private void BuildSearchIndex()
    {
        _invertedIndex.Clear();
        var stopWords = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            { "il", "lo", "la", "i", "gli", "le", "un", "una", "di", "a", "da", "in", "con",
              "su", "per", "tra", "fra", "e", "che", "non", "si", "del", "della", "dei", "delle",
              "al", "alla", "ai", "alle", "dal", "dalla", "dai", "dalle", "nel", "nella", "nei",
              "nelle", "sul", "sulla", "sui", "sulle", "the", "a", "an", "and", "or", "of", "to",
              "is", "in", "it", "on", "for", "with", "by", "as", "at", "be", "from" };

        for (int i = 0; i < _entries.Count; i++)
        {
            var text = _entries[i].SearchText;
            var tokens = Tokenize(text);

            foreach (var token in tokens)
            {
                if (token.Length < 2) continue;
                if (stopWords.Contains(token)) continue;

                if (!_invertedIndex.TryGetValue(token, out var list))
                {
                    list = [];
                    _invertedIndex[token] = list;
                }
                if (!list.Contains(i))
                    list.Add(i);
            }
        }
    }

    public SearchResult Search(string query, int maxResults = 25)
    {
        if (string.IsNullOrWhiteSpace(query))
            return new SearchResult { Results = [] };

        var tokens = Tokenize(query).Where(t => t.Length >= 2).ToList();

        if (tokens.Count == 0)
            return new SearchResult { Results = [] };

        var scores = new Dictionary<int, double>();

        foreach (var token in tokens)
        {
            var scoredEntries = ScoreForToken(token);
            foreach (var (idx, score) in scoredEntries)
            {
                if (!scores.ContainsKey(idx))
                    scores[idx] = 0;
                scores[idx] += score;
            }
        }

        var sorted = scores
            .Select(kv => new ScoredEntry
            {
                Entry = _entries[kv.Key],
                Score = kv.Value,
                MatchType = DetermineMatchType(_entries[kv.Key], tokens)
            })
            .OrderByDescending(x => x.Score)
            .ThenBy(x => x.Entry.Name)
            .Take(maxResults)
            .ToList();

        var namespaces = sorted
            .Where(x => !string.IsNullOrEmpty(x.Entry.Namespace))
            .GroupBy(x => x.Entry.Namespace)
            .ToDictionary(g => g.Key, g => g.Select(x => x.Entry).ToList());

        return new SearchResult
        {
            Results = sorted,
            Namespaces = namespaces
        };
    }

    public HelpEntry? FindExact(string name, string? ns = null)
    {
        var fullName = ns != null ? $"{ns}.{name}" : name;
        return _byFullName.GetValueOrDefault(fullName);
    }

    public List<HelpEntry> GetByNamespace(string ns)
    {
        return _entries
            .Where(e => string.Equals(e.Namespace, ns, StringComparison.OrdinalIgnoreCase))
            .OrderBy(e => e.Name)
            .ToList();
    }

    public List<string> GetNamespaces()
    {
        return _knownNamespaces.OrderBy(n => n).ToList();
    }

    private List<(int index, double score)> ScoreForToken(string token)
    {
        var results = new List<(int, double)>();

        if (_invertedIndex.TryGetValue(token, out var exactMatches))
        {
            foreach (var idx in exactMatches)
                results.Add((idx, 1.0));
        }

        foreach (var kw in _invertedIndex.Keys)
        {
            if (string.Equals(kw, token, StringComparison.OrdinalIgnoreCase))
                continue;

            if (kw.StartsWith(token, StringComparison.OrdinalIgnoreCase))
            {
                foreach (var idx in _invertedIndex[kw])
                    results.Add((idx, 0.7));
            }
            else if (FuzzyMatch(kw, token, 2))
            {
                foreach (var idx in _invertedIndex[kw])
                    results.Add((idx, 0.3));
            }
        }

        return results;
    }

    private static MatchType DetermineMatchType(HelpEntry entry, List<string> tokens)
    {
        var nameTokens = Tokenize(entry.Name).ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var t in tokens)
            if (nameTokens.Contains(t))
                return MatchType.ExactName;

        foreach (var t in tokens)
            if (entry.Name.StartsWith(t, StringComparison.OrdinalIgnoreCase))
                return MatchType.PrefixName;

        foreach (var t in tokens)
            if (FuzzyMatch(entry.Name, t, 2))
                return MatchType.FuzzyName;

        return MatchType.Description;
    }

    private static bool FuzzyMatch(string text, string pattern, int maxDistance)
    {
        if (string.IsNullOrEmpty(text) || string.IsNullOrEmpty(pattern)) return false;
        if (text.Length < pattern.Length) return false;

        for (int i = 0; i <= text.Length - pattern.Length; i++)
        {
            var slice = text.Substring(i, Math.Min(pattern.Length, text.Length - i));
            if (LevenshteinDistance(slice, pattern) <= maxDistance)
                return true;
        }

        return string.Equals(text, pattern, StringComparison.OrdinalIgnoreCase);
    }

    private static int LevenshteinDistance(string a, string b)
    {
        var lenA = a.Length;
        var lenB = b.Length;
        if (lenA == 0) return lenB;
        if (lenB == 0) return lenA;

        var prev = new int[lenB + 1];
        var curr = new int[lenB + 1];

        for (int j = 0; j <= lenB; j++) prev[j] = j;

        for (int i = 0; i < lenA; i++)
        {
            curr[0] = i + 1;
            for (int j = 0; j < lenB; j++)
            {
                var cost = char.ToLowerInvariant(a[i]) == char.ToLowerInvariant(b[j]) ? 0 : 1;
                curr[j + 1] = Math.Min(Math.Min(curr[j] + 1, prev[j + 1] + 1), prev[j] + cost);
            }
            var tmp = prev;
            prev = curr;
            curr = tmp;
        }
        return prev[lenB];
    }

    private static List<string> Tokenize(string text)
    {
        var result = new List<string>();
        var current = "";

        foreach (var ch in text)
        {
            if (char.IsLetterOrDigit(ch) || ch == '_')
            {
                current += char.ToLowerInvariant(ch);
            }
            else
            {
                if (current.Length > 0)
                {
                    result.Add(current);
                    current = "";
                }
            }
        }
        if (current.Length > 0)
            result.Add(current);

        return result;
    }

    private static string ExtractNamespaceFromLine(string line)
    {
        var match = NamespaceRegex.Match(line);
        if (match.Success)
            return match.Groups[1].Value.Trim().ToUpperInvariant();
        return "";
    }

    private static string CleanSyntax(string syntax)
    {
        return syntax.Trim().TrimEnd(',').TrimEnd(';');
    }

    private static string ExtractReturnType(string desc)
    {
        var match = Regex.Match(desc, @"->\s*([A-Z][A-Z\s]+)(?:[\.\)]|$)",
            RegexOptions.IgnoreCase);
        if (match.Success)
            return match.Groups[1].Value.Trim();

        if (Regex.IsMatch(desc, @"\bbit\b", RegexOptions.IgnoreCase))
            return "BIT";
        if (Regex.IsMatch(desc, @"\bstringa?\b", RegexOptions.IgnoreCase))
            return "STRING";
        if (Regex.IsMatch(desc, @"\bdouble\b", RegexOptions.IgnoreCase))
            return "DOUBLE";
        if (Regex.IsMatch(desc, @"\blong\b", RegexOptions.IgnoreCase))
            return "LONG";
        if (Regex.IsMatch(desc, @"\binteger\b", RegexOptions.IgnoreCase))
            return "INTEGER";
        if (Regex.IsMatch(desc, @"\barray\s+di\s+byte\b", RegexOptions.IgnoreCase) ||
            Regex.IsMatch(desc, @"\bbyte\s+array\b", RegexOptions.IgnoreCase))
            return "ARRAY OF BYTE";
        if (Regex.IsMatch(desc, @"\barray\b", RegexOptions.IgnoreCase))
            return "ARRAY";

        return "";
    }

    private static bool IsCommandSyntax(string syntax, string desc)
    {
        if (Regex.IsMatch(desc, @"\bstatement\b", RegexOptions.IgnoreCase))
            return true;
        if (Regex.IsMatch(desc, @"\bcomando\b", RegexOptions.IgnoreCase))
            return true;
        return syntax.Contains(" TO ", StringComparison.OrdinalIgnoreCase)
            || syntax.Contains(" AS ", StringComparison.OrdinalIgnoreCase)
            || syntax.Contains(" #", StringComparison.OrdinalIgnoreCase);
    }
}

public enum MatchType
{
    ExactName,
    PrefixName,
    FuzzyName,
    Namespace,
    Description
}

public class ScoredEntry
{
    public HelpEntry Entry { get; set; } = null!;
    public double Score { get; set; }
    public MatchType MatchType { get; set; }
}

public class SearchResult
{
    public List<ScoredEntry> Results { get; set; } = [];
    public Dictionary<string, List<HelpEntry>> Namespaces { get; set; } = [];
}
