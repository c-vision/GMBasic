// FILE: Basic.Agents.AgentBrains
// Livello AGENT dello scope dei cervelli (F2 di Knowledge/Brain-Scope-Modello.md):
// ogni agente può DICHIARARE i brain che può usare (`AI AGENT SET <agente>, brains, "finance AS fin"`).
//
// Regola di F2 (nessun cambio di default):
//  - se l'agente NON dichiara nulla → si restituisce `null` e il consumer continua col proprio default
//    (oggi: `BrainRegistry.Used()`, cioè i brain attivati globalmente) — comportamento INVARIATO;
//  - se l'agente dichiara → si restituisce il set effettivo dello scope (con alias/`WITHOUT` risolti).
// Il livello TASK (scope per-task GPM) è F3; qui c'è solo AGENT sopra PROCESS.

using Basic.AI.Knowledge;

namespace Basic.Agents;

/// <summary>Scope dei cervelli dichiarato da un agente (livello AGENT) + risoluzione per i tool.</summary>
public static class AgentBrains
{
    /// <summary>Separatore della dichiarazione: virgola. Un riferimento può avere un alias: `finance AS fin`.</summary>
    private const string AliasKeyword = " AS ";

    /// <summary>Parsa una dichiarazione CSV ("finance AS fin, airline") in <see cref="BrainRef"/>.
    /// Ritorna <c>null</c> per una dichiarazione ASSENTE/vuota (≠ set vuoto: vedi <see cref="ParseDeclared"/>).</summary>
    public static IReadOnlyList<BrainRef>? Parse(string? csv)
        => ParseDeclared(csv);

    /// <summary>
    /// Parsa la dichiarazione. `null`/vuoto = nessuna dichiarazione (INHERIT); una lista di riferimenti
    /// (eventualmente vuota se la stringa è solo separatori) = dichiarazione esplicita.
    /// </summary>
    public static IReadOnlyList<BrainRef>? ParseDeclared(string? csv)
    {
        if (string.IsNullOrWhiteSpace(csv)) return null;
        var refs = new List<BrainRef>();
        foreach (var raw in csv.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            var item = raw.Trim().Trim('"');
            if (item.Length == 0) continue;
            var idx = item.IndexOf(AliasKeyword, StringComparison.OrdinalIgnoreCase);
            if (idx > 0)
            {
                var name = item[..idx].Trim();
                var alias = item[(idx + AliasKeyword.Length)..].Trim();
                if (name.Length > 0) refs.Add(new BrainRef(name, alias.Length > 0 ? alias : null));
            }
            else refs.Add(new BrainRef(item));
        }
        return refs;
    }

    /// <summary>
    /// Scope dell'agente: livello PROCESS (brain attivati globalmente) + livello AGENT.
    ///
    /// **Semantica del livello AGENT** (decisione F4, coerente con `ResolveForTool` e con l'esempio del
    /// modello): se l'agente **dichiara** i brain, la dichiarazione è un **GRANT** — il livello è PRIVATE e
    /// NON eredita i brain globali (l'agente vede solo i suoi). Se NON dichiara nulla, il livello è INHERIT
    /// e si comporta come oggi (brain attivati con `AI BRAIN USE`).
    /// </summary>
    public static BrainScope ScopeFor(AgentRecord agent)
    {
        var declared = ParseDeclared(agent.Brains);
        return BrainScope.FromRegistry().Agent(declared, isPrivate: declared is { Count: > 0 });
    }

    /// <summary>
    /// Set di brain per un tool, o <c>null</c> se non c'è alcuna dichiarazione (→ consumer al proprio
    /// default, comportamento invariato).
    ///
    /// Precedenza (dal più interno al più esterno, come la catena di scope): **argomento esplicito del
    /// tool** → **scope del TASK** (`taskCsv`, F3: il set passato al task che sta eseguendo) →
    /// **dichiarazione dell'AGENTE** (`agent.Brains`, F2) → `null` (default del consumer, invariato).
    /// </summary>
    public static string[]? ResolveForTool(AgentRecord? agent, string? explicitCsv, string? taskCsv = null)
    {
        var explicitRefs = ParseDeclared(explicitCsv);
        if (explicitRefs is { Count: > 0 }) return explicitRefs.Select(r => r.Name).ToArray();
        var taskRefs = ParseDeclared(taskCsv);
        if (taskRefs is { Count: > 0 }) return taskRefs.Select(r => r.Name).ToArray();
        var declared = ParseDeclared(agent?.Brains);
        if (declared is { Count: > 0 }) return declared.Select(r => r.Name).ToArray();
        return null;   // nessuna dichiarazione: NON si tocca il default del consumer
    }

    /// <summary>
    /// Risoluzione per un tool CON ENFORCEMENT del grant (F5): se l'agente DICHIARA i brain, una richiesta
    /// esplicita del tool (`brain=…`) è ammessa **solo** dentro il grant (per nome o alias); un brain fuori
    /// dal grant produce un **errore esplicito** (mai una query silenziosa). Un agente che non dichiara nulla
    /// resta senza restrizioni (comportamento storico: può interrogare i brain attivati).
    /// </summary>
    public static bool TryResolveForTool(
        AgentRecord? agent, string? callCsv, string? taskCsv, out string[]? brains, out string? error)
    {
        brains = null;
        error = null;
        var callRefs = ParseDeclared(callCsv);
        var granted = ParseDeclared(agent?.Brains);
        if (granted is { Count: > 0 } && callRefs is { Count: > 0 })
        {
            var outOfGrant = callRefs.Where(r => !granted.Any(g => g.Matches(r.Name))).ToList();
            if (outOfGrant.Count > 0)
            {
                error = $"brain non concesso all'agente '{(agent?.Name ?? "?")}': "
                      + $"{string.Join(", ", outOfGrant.Select(r => r.Name))} — scope concesso: "
                      + $"{string.Join(", ", granted.Select(g => g.ToString()))}";
                return false;
            }
        }
        brains = ResolveForTool(agent, callCsv, taskCsv);
        return true;
    }

    /// <summary>Descrizione leggibile della dichiarazione (per il prompt di sistema e `AI AGENT GET`).</summary>
    public static string Describe(string? csv)
    {
        var refs = ParseDeclared(csv);
        if (refs is null) return "";
        return refs.Count == 0 ? "(set vuoto)" : string.Join(", ", refs.Select(r => r.ToString()));
    }
}
