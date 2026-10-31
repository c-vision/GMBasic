// FILE: Basic.Agents.AgentBundle
// C2 — .BAGENT bundle: esporta/importa un agente (snapshot + skills + keypair) in un file
// JSON `.bagent`. Riusa AgentPersistence.Capture/Restore (stesso assembly). Mai stub.

using System.Text.Json;

namespace Basic.Agents;

public static class AgentBundle
{
    public const string Format = "gmb-bagent";
    public const int Version = 1;

    /// <summary>Esporta l'agente in un bundle .bagent (JSON). Ritorna il path o errore esplicito.</summary>
    public static string Export(string name, string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var agent = AgentServer.Instance.List().FirstOrDefault(a => a.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
            ?? throw new KeyNotFoundException($"Agent '{name}' non trovato");
        lock (agent.StateLock)
        {
            var snapshot = AgentPersistence.Capture(agent);
            var skills = agent.ToolNames.Where(t => Basic.AI.Skills.SkillRegistry.Exists(t)).ToList();
            var bundle = new
            {
                format = Format,
                version = Version,
                exportedAt = DateTime.UtcNow,
                snapshot,
                skills,
                hasKeypair = !string.IsNullOrWhiteSpace(agent.PublicKey)
            };
            var dir = Path.GetDirectoryName(Path.GetFullPath(path));
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(path, JsonSerializer.Serialize(bundle, new JsonSerializerOptions { WriteIndented = true }));
            return path;
        }
    }

    /// <summary>
    /// Importa un bundle .bagent: ripristina l'agente (sovrascrive se esiste) e ri-registra le skill del
    /// bundle. Ritorna il nome dell'agente importato.
    ///
    /// **F5 — import CONSERVATIVO dello scope cervelli**: il bundle può contenere `brains`, ma per default
    /// la dichiarazione viene **scartata** (un pacchetto di terze parti non deve ereditare automaticamente
    /// l'accesso ai cervelli di chi lo importa: il grant si dà esplicitamente con `keepBrains: true` o dopo
    /// con `AI AGENT SET &lt;agente&gt;, brains, "…"`).
    /// </summary>
    public static string Import(string path, string? newName = null, bool keepBrains = false)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        if (!File.Exists(path)) throw new FileNotFoundException($"Bundle non trovato: {path}", path);
        var json = File.ReadAllText(path);
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        if (!root.TryGetProperty("format", out var fmt) || fmt.GetString() != Format)
            throw new InvalidDataException($"'{path}' non è un bundle .bagent (format atteso '{Format}')");
        var snapshot = JsonSerializer.Deserialize<AgentPersistence.Snapshot>(root.GetProperty("snapshot").GetRawText())
            ?? throw new InvalidDataException($"Bundle '{path}' senza snapshot valido");
        var name = newName ?? snapshot.Name;
        var effective = keepBrains ? snapshot : snapshot with { Brains = null };
        var agent = AgentPersistence.Restore(effective with { Name = name });

        if (root.TryGetProperty("skills", out var skills) && skills.ValueKind == JsonValueKind.Array)
            foreach (var s in skills.EnumerateArray())
                if (s.GetString() is { } skillName)
                    Basic.AI.Skills.SkillRegistry.RequireSkill(skillName);

        AgentServer.Instance.Upsert(agent);
        return name;
    }
}
