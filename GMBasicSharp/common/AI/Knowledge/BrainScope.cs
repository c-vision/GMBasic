// FILE: Basic.AI.Knowledge.BrainScope
// Scope dei cervelli (Second Brain) — i brain hanno uno SCOPE come le variabili.
// Design: 
// Impatto:  (F1)
//
// Catena (dal più interno al più esterno):
//     CALL  → TASK  → AGENT → SESSION → PROCESS
// Regole (identiche allo scope delle variabili):
//   1. «innermost wins»: il primo livello che dichiara il nome lo risolve;
//   2. «vuoto ≠ assente»: un livello che dichiara ESPLICITAMENTE il set vuoto (Declared non-null e
//      vuoto, oppure IsPrivate senza dichiarazioni) INTERROMPE la catena — nessun fallback silenzioso;
//   3. INHERIT (default) = `Declared == null` → si eredita dal livello esterno; PRIVATE = non si eredita;
//   4. il set effettivo è l'UNIONE dei livelli attraversati (dedup per nome, vince il più interno);
//   5. un alias risolve DENTRO il livello che lo dichiara;
//   6. `Without` sottrae nomi/alias ereditati;
//   7. IMMUTABILE e read-only: uno scope non scrive mai il registry globale (livello PROCESS).
//
// F1: questo file è di sola risoluzione — NON cambia i default dei consumer (il cablaggio di
// AgentRecord/agent-tool-loop/task è F2/F3, il cambio di default dei consumer è F4).

using CommonLib;

namespace Basic.AI.Knowledge;

/// <summary>Livello della catena di scope. L'ordine dell'enum è dall'esterno (Process) all'interno (Call).</summary>
public enum BrainScopeLevel
{
    Process = 0,
    Session = 1,
    Agent = 2,
    Task = 3,
    Call = 4,
}

/// <summary>Riferimento a un cervello in uno scope: nome registrato, eventuale alias locale e root.</summary>
public sealed record BrainRef(string Name, string? Alias = null, string? BrainRoot = null)
{
    /// <summary>True se il riferimento risponde a <paramref name="nameOrAlias"/> (nome o alias, case-insensitive).</summary>
    public bool Matches(string nameOrAlias) =>
        !string.IsNullOrWhiteSpace(nameOrAlias)
        && (string.Equals(Name, nameOrAlias, StringComparison.OrdinalIgnoreCase)
            || (Alias is { Length: > 0 } a && string.Equals(a, nameOrAlias, StringComparison.OrdinalIgnoreCase)));

    public override string ToString() => Alias is { Length: > 0 } ? $"{Name} AS {Alias}" : Name;
}

/// <summary>Esito della risoluzione di un nome/alias: riferimento trovato + livello che l'ha risolto.</summary>
public sealed record BrainResolution(BrainRef Ref, BrainScopeLevel Level, int Depth)
{
    public bool Found => true;
}

/// <summary>
/// Catena di scope dei cervelli. Ogni istanza dichiara i riferimenti di UN livello e punta al livello
/// esterno (<see cref="Parent"/>); il livello PROCESS ha <c>Parent == null</c> e i brain attivati
/// globalmente (vedi <see cref="ForProcess"/>).
/// </summary>
public sealed class BrainScope
{
    /// <summary>Livello esterno (null = questo è il livello PROCESS).</summary>
    public BrainScope? Parent { get; }

    /// <summary>Livello di questa istanza.</summary>
    public BrainScopeLevel Level { get; }

    /// <summary>
    /// Riferimenti dichiarati da QUESTO livello. <c>null</c> = nessuna dichiarazione (INHERIT: si eredita
    /// dal livello esterno); <c>[]</c> = dichiarazione ESPLICITA del set vuoto (interrompe la catena).
    /// </summary>
    public IReadOnlyList<BrainRef>? Declared { get; }

    /// <summary>True = livello PRIVATE: non eredita nulla dall'esterno (le proprie <see cref="Declared"/> restano).</summary>
    public bool IsPrivate { get; }

    /// <summary>
    /// Nomi/alias SOTTRATTI da questo livello: nasconde ai livelli ESTERNI i riferimenti il cui NOME o
    /// ALIAS coincide (match testuale, case-insensitive). Non tocca le proprie <see cref="Declared"/>.
    /// Nota: la sottrazione è per identificatore, non per «identità del brain» — se un livello esterno
    /// dichiara lo STESSO brain con un identificatore diverso (es. senza alias), quel riferimento resta
    /// visibile: è una scelta di predicibilità (nessuna risoluzione implicita dentro la risoluzione).
    /// </summary>
    public IReadOnlyList<string> Without { get; }

    public BrainScope(
        BrainScope? parent,
        IReadOnlyList<BrainRef>? declared = null,
        bool isPrivate = false,
        IReadOnlyList<string>? without = null,
        BrainScopeLevel? level = null)
    {
        Parent = parent;
        Declared = declared;
        IsPrivate = isPrivate;
        Without = without ?? [];
        // Senza livello esplicito: PROCESS se è la radice; altrimenti il livello immediatamente più
        // interno (uso avanzato/test — i livelli reali si creano con Session()/Agent()/Task()/Call()).
        Level = level ?? (parent is null ? BrainScopeLevel.Process : NextInner(parent.Level));
    }

    private static BrainScopeLevel NextInner(BrainScopeLevel parent) => parent switch
    {
        BrainScopeLevel.Process => BrainScopeLevel.Session,
        BrainScopeLevel.Session => BrainScopeLevel.Agent,
        BrainScopeLevel.Agent => BrainScopeLevel.Task,
        _ => BrainScopeLevel.Call,
    };

    /// <summary>True se questo livello interrompe la catena (dichiarazione esplicita del vuoto o PRIVATE).</summary>
    private bool StopsChain => IsPrivate || Declared is { Count: 0 };

    /// <summary>
    /// Livelli attraversati dalla risoluzione (dal più interno al più esterno), fermandosi al primo che
    /// interrompe la catena.
    /// </summary>
    private IEnumerable<BrainScope> Chain()
    {
        for (var s = this; s is not null; s = s.Parent)
        {
            yield return s;
            if (s.StopsChain) yield break;
        }
    }

    /// <summary>
    /// Risolve un nome/alias con la regola «innermost wins». Ritorna <c>null</c> se nessun livello lo
    /// dichiara (il chiamante DEVE produrre un errore esplicito, mai un fallback silenzioso).
    /// </summary>
    public BrainResolution? Resolve(string nameOrAlias)
    {
        if (string.IsNullOrWhiteSpace(nameOrAlias)) return null;
        var hidden = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var depth = 0;
        foreach (var s in Chain())               // dal più interno al più esterno
        {
            foreach (var r in s.Declared ?? [])
                if (!IsHidden(r, hidden) && r.Matches(nameOrAlias))
                    return new BrainResolution(r, s.Level, depth);
            // `Without` di questo livello nasconde i nomi/alias ai livelli ESTERNI (non ai propri)
            foreach (var w in s.Without) hidden.Add(w);
            depth++;
        }
        return null;
    }

    /// <summary>True se il riferimento è nascosto da un `Without` di un livello più interno (nome O alias).</summary>
    private static bool IsHidden(BrainRef r, HashSet<string> hidden) =>
        hidden.Count > 0 && (hidden.Contains(r.Name) || (r.Alias is { Length: > 0 } a && hidden.Contains(a)));

    /// <summary>
    /// Set effettivo (unione dei livelli attraversati, dedup per nome, vince il più interno), applicando
    /// i filtri <see cref="Without"/> di ogni livello.
    /// </summary>
    public IReadOnlyList<BrainRef> Effective()
    {
        var result = new List<BrainRef>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var hidden = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var s in Chain())
        {
            foreach (var r in s.Declared ?? [])
            {
                if (IsHidden(r, hidden)) continue;
                if (!seen.Add(r.Name)) continue;   // il livello più interno vince
                result.Add(r);
            }
            foreach (var w in s.Without) hidden.Add(w);
        }
        return result;
    }

    /// <summary>Spiegazione leggibile della catena (per `BRAIN RESOLVE --explain`).</summary>
    public IReadOnlyList<string> Explain()
    {
        var lines = new List<string>();
        var depth = 0;
        foreach (var s in Chain())
        {
            var declared = s.Declared is null ? "(nessuna dichiarazione → eredita)"
                : s.Declared.Count == 0 ? "(set VUOTO dichiarato → interrompe la catena)"
                : string.Join(", ", s.Declared.Select(r => r.ToString()));
            var flags = new List<string>();
            if (s.IsPrivate) flags.Add("PRIVATE");
            if (s.Without.Count > 0) flags.Add($"WITHOUT {string.Join(",", s.Without)}");
            lines.Add($"  [depth {depth}] {s.Level}" + (flags.Count > 0 ? $" [{string.Join(" ", flags)}]" : "") + $" → {declared}");
            depth++;
            if (s.StopsChain) break;
        }
        var eff = Effective();
        lines.Add($"  effettivo: {(eff.Count == 0 ? "(vuoto)" : string.Join(", ", eff.Select(r => r.ToString())))}");
        return lines;
    }

    // ── Factory dei livelli ────────────────────────────────────────────────

    /// <summary>
    /// Livello PROCESS (root della catena) dai brain attivati GLOBALMENTE: SOLO LETTURA del registry —
    /// non attiva nulla. Nome esplicito (non `ForProcess()`): un overload «senza argomenti» che legge il
    /// registry della macchina è una trappola per i test (leak di stato utente).
    /// </summary>
    public static BrainScope FromRegistry() =>
        new(parent: null, declared: BrainRegistry.Used().Select(n => new BrainRef(n)).ToArray(),
            level: BrainScopeLevel.Process);

    /// <summary>Livello PROCESS con dichiarazione ESPLICITA (test, dump, diagnostica): nessun accesso al registry.</summary>
    public static BrainScope Process(params string[] names) =>
        new(parent: null, declared: names.Select(n => new BrainRef(n)).ToArray(), level: BrainScopeLevel.Process);

    /// <summary>Livello PROCESS con riferimenti ESPLICITI (alias inclusi) — test/diagnostica.
    /// Nome distinto da `Process(params string[])`: con due overload `params` la chiamata senza argomenti
    /// sarebbe ambigua (e `Process()` senza argomenti è il caso "set vuoto esplicito" usato nei test).</summary>
    public static BrainScope ProcessRefs(params BrainRef[] refs) =>
        new(parent: null, declared: refs, level: BrainScopeLevel.Process);

    /// <summary>Livello SESSION innestato in questo scope (i livelli NON attraversati semplicemente non esistono).</summary>
    public BrainScope Session(IReadOnlyList<BrainRef>? declared = null, bool isPrivate = false, IReadOnlyList<string>? without = null)
        => new(this, declared, isPrivate, without, BrainScopeLevel.Session);

    /// <summary>Livello AGENT (brain dichiarati per agente — persistente, come STATIC).</summary>
    public BrainScope Agent(IReadOnlyList<BrainRef>? declared = null, bool isPrivate = false, IReadOnlyList<string>? without = null)
        => new(this, declared, isPrivate, without, BrainScopeLevel.Agent);

    /// <summary>Livello TASK (scope del task GPM — transiente, come LOCAL).</summary>
    public BrainScope Task(IReadOnlyList<BrainRef>? declared = null, bool isPrivate = false, IReadOnlyList<string>? without = null)
        => new(this, declared, isPrivate, without, BrainScopeLevel.Task);

    /// <summary>Livello CALL (argomento esplicito della singola query).</summary>
    public BrainScope Call(IReadOnlyList<BrainRef>? declared = null, bool isPrivate = false, IReadOnlyList<string>? without = null)
        => new(this, declared, isPrivate, without, BrainScopeLevel.Call);

    /// <summary>Nuovo livello innestato con livello ESPLICITO (uso avanzato/test).</summary>
    public BrainScope WithLevel(BrainScopeLevel level,
        IReadOnlyList<BrainRef>? declared = null, bool isPrivate = false, IReadOnlyList<string>? without = null) =>
        new(this, declared, isPrivate, without, level);
}
