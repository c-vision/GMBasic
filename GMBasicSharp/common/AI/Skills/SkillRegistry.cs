// FILE: Basic.AI.Skills.SkillRegistry
// B3.2 — SkillRegistry: REQUIRE SKILL github:user/repo, SKILL.SEARCH/PUBLISH + DB SKILLS/AGENTS

using System.Collections.Concurrent;

namespace Basic.AI.Skills;

public sealed record SkillRecord(string Name, string Repo, string Description, DateTime Created, int Reputation = 0);

public static class SkillRegistry
{
    private static readonly ConcurrentDictionary<string, SkillRecord> _skills = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, string> _requires = new(StringComparer.OrdinalIgnoreCase);

    public static bool RequireSkill(string spec)
    {
        if (string.IsNullOrWhiteSpace(spec)) return false;
        // spec like "github:user/repo" or "user/repo"
        var name = spec.Trim();
        if (name.StartsWith("github:", StringComparison.OrdinalIgnoreCase)) name = name.Substring(7);
        _requires[spec] = name;
        // Se non esiste già, crea un record placeholder
        _skills.TryAdd(name, new SkillRecord(name, spec, $"Skill {name} via {spec}", DateTime.UtcNow));
        return true;
    }

    public static bool Publish(string name, string description, string repo = "")
    {
        if (string.IsNullOrWhiteSpace(name)) return false;
        var rec = new SkillRecord(name, string.IsNullOrWhiteSpace(repo) ? $"github:{name}" : repo, description, DateTime.UtcNow);
        _skills[name] = rec;
        return true;
    }

    public static List<SkillRecord> Search(string query, int topK = 5)
    {
        if (string.IsNullOrWhiteSpace(query)) return _skills.Values.Take(topK).ToList();
        var q = query.ToLowerInvariant();
        return _skills.Values
            .Where(s => s.Name.ToLowerInvariant().Contains(q) || s.Description.ToLowerInvariant().Contains(q) || s.Repo.ToLowerInvariant().Contains(q))
            .OrderByDescending(s => s.Reputation)
            .Take(topK)
            .ToList();
    }

    public static string SearchJson(string query, int topK = 5)
    {
        var list = Search(query, topK);
        return System.Text.Json.JsonSerializer.Serialize(new { query, count = list.Count, hits = list });
    }

    public static List<SkillRecord> List() => _skills.Values.OrderBy(s => s.Name).ToList();
    public static bool Exists(string name) => _skills.ContainsKey(name);
    public static void Clear() { _skills.Clear(); _requires.Clear(); }
}
