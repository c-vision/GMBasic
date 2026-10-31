// FILE: Basic.AI.PluginIndex
// Scanner dei plugin operativi nel percorso configurato + generatore dell'indice
// (PLUGINS_PROMPT.md). Modello di SkillIndex.cs. Un plugin è una cartella
// plugins/<nome>/PLUGIN.md (manifest YAML in frontmatter).

namespace Basic.AI;

using System.Text;
using System.Text.RegularExpressions;

/// <summary>Rileva i plugin (PLUGIN.md) nel percorso configurato (AiConfig.PluginsDefaultPath)
/// e genera l'indice riassuntivo (prompt per l'agente). Usato da AI PLUGINS REFRESH.</summary>
public static class PluginIndex
{
    /// <summary>Scansiona il percorso configurato e restituisce un PluginRecord per ogni
    /// plugin trovato (una cartella contenente PLUGIN.md).</summary>
    public static List<PluginRecord> Scan()
    {
        var result = new List<PluginRecord>();
        var dir = AiConfig.PluginsDefaultPath;
        if (!Directory.Exists(dir)) return result;
        try
        {
            foreach (var pluginFile in Directory.EnumerateFiles(dir, "PLUGIN.md", SearchOption.AllDirectories))
            {
                var name = Path.GetFileName(Path.GetDirectoryName(pluginFile)!) ?? Path.GetFileName(pluginFile);
                result.Add(BuildRecord(name, pluginFile));
            }
        }
        catch { /* best effort per percorso */ }
        return result.OrderBy(x => x.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    /// <summary>Plugin NOTI il cui trigger matcha il prompt (per l'acquisizione automatica
    /// della skill per-ask: il tool-loop parte + i tool del plugin entrano solo se il task li
    /// menziona, senza gonfiare il contesto di default).</summary>
    public static IEnumerable<PluginRecord> MatchByTrigger(string prompt)
    {
        if (string.IsNullOrWhiteSpace(prompt)) yield break;
        foreach (var plugin in Scan())
        {
            if (string.IsNullOrWhiteSpace(plugin.Trigger)) continue;
            foreach (var phrase in plugin.Trigger.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                if (prompt.Contains(phrase, StringComparison.OrdinalIgnoreCase))
                {
                    yield return plugin;
                    break;
                }
                if (phrase.Length >= 4 && !phrase.Contains(' ', StringComparison.Ordinal))
                    foreach (var w in phrase.Split(' ', StringSplitOptions.RemoveEmptyEntries))
                        if (w.Length >= 4 && prompt.Contains(w, StringComparison.OrdinalIgnoreCase))
                        {
                            yield return plugin;
                            break;
                        }
            }
        }
    }

    /// <summary>Genera il prompt di riepilogo da tutti i plugin e lo scrive nel percorso
    /// indice configurato (AiConfig.PluginsPromptPath). Ritorna il contenuto.</summary>
    public static string Refresh()
    {
        var plugins = Scan();
        var indexPath = AiConfig.PluginsPromptPath;
        var dir = Path.GetDirectoryName(indexPath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        var sb = new StringBuilder();
        sb.AppendLine("# GMBASIC — Archivio Plugin Agente");
        sb.AppendLine();
        sb.AppendLine($"> **Percorso configurato**: {AiConfig.PluginsDefaultPath}");
        sb.AppendLine($"> **Percorso indice**: `{indexPath}`");
        sb.AppendLine($"> {plugins.Count} plugin rilevati.\n");
        sb.AppendLine("## Istruzioni per l'agente");
        sb.AppendLine("Quando ricevi una richiesta che corrisponde a un plugin sotto, leggi il file");
        sb.AppendLine("`PLUGIN.md` del plugin rilevante (percorso: `plugins/<nome>/PLUGIN.md`) e segui");
        sb.AppendLine("le sue istruzioni. Un plugin è una **capacità operativa**: può registrare tool");
        sb.AppendLine("invocabili e procedure BASIC. Se più plugin sono rilevanti, leggili tutti e combinali.");
        sb.AppendLine();

        foreach (var p in plugins)
        {
            var state = p.Enabled ? "attivo" : "disattivo";
            var trigger = string.IsNullOrWhiteSpace(p.Trigger) ? "" : $" · trigger: {p.Trigger}";
            sb.AppendLine($"- **`{p.Name}`** — {p.Description} · stato: {state}{trigger}");
        }

        // Best effort (parità SkillIndex.Refresh): il testo è il valore di ritorno, la scrittura
        // è solo una cache su disco.
        try { File.WriteAllText(indexPath, sb.ToString(), Encoding.UTF8); }
        catch { /* cartella non scrivibile: si prosegue col testo in memoria */ }
        return sb.ToString();
    }

    /// <summary>Costruisce il PluginRecord dal frontmatter YAML di un PLUGIN.md.</summary>
    private static PluginRecord BuildRecord(string name, string pluginFile)
    {
        string description = "", trigger = "";
        var enabled = true;
        string[] tools = [], requires = [], procedures = [];
        try
        {
            var text = File.ReadAllText(pluginFile, Encoding.UTF8);
            var m = Regex.Match(text, @"^---\s*\n(.*?)\n---\s*$", RegexOptions.Singleline | RegexOptions.Multiline);
            if (m.Success)
            {
                var fm = m.Groups[1].Value;
                description = ReadField(fm, "description");
                trigger = ReadField(fm, "trigger");
                var en = ReadField(fm, "enabled");
                enabled = string.IsNullOrWhiteSpace(en) || !en.Equals("false", StringComparison.OrdinalIgnoreCase);
                tools = ReadList(fm, "tools");
                requires = ReadList(fm, "requires");
                procedures = ReadList(fm, "procedures");
            }
        }
        catch { /* best effort */ }
        return new PluginRecord(name, pluginFile, description, trigger, enabled, tools, requires, procedures);
    }

    /// <summary>Legge un campo scalare dal frontmatter YAML (fino al campo successivo).</summary>
    private static string ReadField(string fm, string key)
    {
        var m = Regex.Match(fm,
            @"^\s*" + Regex.Escape(key) + @":\s*[""]?(.+?)(?:\n\s*\w+:|[""]\s*$)",
            RegexOptions.Multiline | RegexOptions.Singleline);
        return m.Success ? m.Groups[1].Value.Trim().Replace('\n', ' ') : "";
    }

    /// <summary>Legge una lista YAML (es. `tools: [a, b]`) dal frontmatter.</summary>
    private static string[] ReadList(string fm, string key)
    {
        var m = Regex.Match(fm,
            @"^\s*" + Regex.Escape(key) + @":\s*\[([^\]]*)\]",
            RegexOptions.Multiline);
        if (!m.Success) return [];
        return m.Groups[1].Value.Split(',')
            .Select(x => x.Trim().Trim('"').Trim('\''))
            .Where(x => x.Length > 0)
            .ToArray();
    }
}
