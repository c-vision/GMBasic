// FILE: Basic.AI.SkillIndex
// Scanner delle skills nei percorsi configurati + generatore dell'indice (SKILLS_PROMPT.md).
// L'indice è UNO SOLO: il prompt per l'agente che riassume le skills disponibili e i percorsi.
namespace Basic.AI;

using System.Text;
using System.Text.RegularExpressions;

/// <summary>Rileva le skills (SKILL.md o *.md) in un insieme di percorsi e genera l'indice
/// riassuntivo (prompt per l'agente). Usato da AI SKILLS REFRESH e dalle operazioni di
/// aggiunta/rimozione per mantenere aggiornato SKILLS_PROMPT.md.</summary>
public static class SkillIndex
{
    /// <summary>Scansiona i percorsi configurati (AiConfig.SkillPaths) e restituisce
    /// (nome, percorso-assoluto, descrizione) per ogni skill trovata.</summary>
    public static List<(string Name, string Path, string Description)> Scan()
    {
        var result = new List<(string, string, string)>();
        foreach (var root in AiConfig.SkillPaths)
        {
            var dir = PathExpansion(root);
            if (!Directory.Exists(dir)) continue;
            try
            {
                // Scansione RICORSIVA: una skill è una cartella che contiene SKILL.md (es.
                // skills/<nome>/SKILL.md oppure <marketplace>/repo/skills/<nome>/SKILL.md).
                foreach (var skillFile in Directory.EnumerateFiles(dir, "SKILL.md", SearchOption.AllDirectories))
                {
                    var name = Path.GetFileName(Path.GetDirectoryName(skillFile)!) ?? Path.GetFileName(skillFile);
                    result.Add((name, skillFile, ReadDescription(skillFile)));
                }
            }
            catch { /* best effort per percorso */ }
        }
        return result.OrderBy(x => x.Item1, StringComparer.OrdinalIgnoreCase).ToList();
    }

    /// <summary>Genera il prompt di riepilogo da tutte le skills scansionate e lo scrive
    /// nel percorso indice configurato (AiConfig.SkillsIndexPath). Ritorna il contenuto.</summary>
    public static string Refresh()
    {
        var skills = Scan();
        var indexPath = AiConfig.SkillsIndexPath;
        var dir = Path.GetDirectoryName(indexPath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        var sb = new StringBuilder();
        sb.AppendLine("# GMBASIC — Archivio Skills Agente");
        sb.AppendLine();
        sb.AppendLine($"> **Percorsi configurati**: {string.Join("; ", AiConfig.SkillPaths)}");
        sb.AppendLine($"> **Percorso indice**: `{indexPath}`");
        sb.AppendLine($"> {skills.Count} skill rilevate.\n");
        sb.AppendLine("## Istruzioni per l'agente");
        sb.AppendLine("Quando ricevi una richiesta che corrisponde a una delle skill sotto, leggi il file");
        sb.AppendLine("`SKILL.md` della skill rilevante (percorso: `skills/<nome>/SKILL.md`) e segui");
        sb.AppendLine("le sue istruzioni. Il riassunto qui sotto e' solo un indice: il dettaglio operativo e' nel");
        sb.AppendLine("singolo file. Se piu' skill sono rilevanti, leggile tutte e combinale.");
        sb.AppendLine();

        foreach (var s in skills)
            sb.AppendLine($"- **`{s.Item1}`** — {s.Item3}");

        // Best effort: l'indice è un OUTPUT (il valore di ritorno è la fonte per l'agente: il
        // chiamante lo passa al SystemPrompt). Una cartella non scrivibile non deve far fallire
        // AI.SKILLSPROMPT — parità con il C, dove `fopen` che fallisce lascia comunque il testo.
        try { File.WriteAllText(indexPath, sb.ToString(), Encoding.UTF8); }
        catch { /* cartella non scrivibile (app installata): si prosegue col testo in memoria */ }
        return sb.ToString();
    }

    /// <summary>Legge name/description dal frontmatter di una SKILL.md.</summary>
    private static string ReadDescription(string skillFile)
    {
        try
        {
            var text = File.ReadAllText(skillFile, Encoding.UTF8);
            var m = Regex.Match(text, @"^---\s*\n(.*?)\n---\s*$", RegexOptions.Singleline | RegexOptions.Multiline);
            if (m.Success)
            {
                var dm = Regex.Match(m.Groups[1].Value,
                    @"^\s*description:\s*[""]?(.+?)(?:\n\s*\w+:|[""]\s*$)", RegexOptions.Multiline | RegexOptions.Singleline);
                if (dm.Success)
                    return dm.Groups[1].Value.Trim().Replace('\n', ' ');
            }
        }
        catch { /* best effort */ }
        return "";
    }

    private static string PathExpansion(string path) =>
        Basic.ModelDownload.PathExpansion.Expand(path);
}
