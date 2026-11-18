// FILE: Linker.ModuleInit.cs
//
// Inizializzazione dei moduli importati. Un file BASIC con `NAMESPACE Foo` può contenere
// codice a livello di modulo (assegnazioni a variabili `PUBLIC DIM`, PRINT, ecc.): quel
// codice deve girare PRIMA dell'entry point, come per il modulo di ingresso e come fa
// C99Native (che compila i file in un'unica unità e li esegue in ordine di dipendenza).
//
// IL e PCode/VM-C compilano soltanto il `MainModule`: le righe di modulo degli altri
// moduli non venivano né emesse né eseguite, quindi `PUBLIC DIM x` restava al default
// (bug `Namespace-Accesso-Qualificato-Fra-Moduli`). Qui i loro statement NON numerati
// (le righe senza numero di riga) vengono fusi in testa al `MainModule`, in ordine
// topologico delle dipendenze: una sola implementazione, condivisa da IL e bytecode,
// perché entrambi compilano `MainModule.ExecutionOrder`.
//
// Le righe NUMERATE legacy non possono esistere in un modulo importato: dentro `NAMESPACE`
// il parser le rifiuta (stesso messaggio su parser C# e C; MiniTest
// `constructs/namespace_numeri_riga.bas`), quindi l'inizializzazione di un modulo è fatta
// solo di righe senza numero. Il filtro `lineNumber < 1_000_000` resta come guardia
// difensiva (righe numerate di un file raggiunto senza NAMESPACE).

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>Base delle chiavi sintetiche usata dal parser per le righe non numerate
    /// (<c>Parser.Statements.SyntheticLineBase</c>): da qui in poi una riga è "moderna" e
    /// viene eseguita prima di <c>SUB Main</c>.</summary>
    private const int ModuleInitSyntheticBase = 1_000_000;

    /// <summary>
    /// Porta in testa al <see cref="BasicProgram.MainModule"/> gli statement di modulo dei
    /// moduli importati (dipendenze prima, entry per ultima), rinumerando le chiavi sintetiche
    /// per non collidere. Idempotente rispetto all'ordine: va chiamata una sola volta, dopo
    /// tutte le passate di linking e prima della codegen.
    /// </summary>
    public static void InlineModuleInitializers(BasicProgram program)
    {
        var main = program.MainModule;
        if (main is null) return;

        var order = ComputeInitOrder(program, main);
        if (order.Count == 0) return;

        /* 1. Prelude: solo statement non numerati (chiavi >= SyntheticLineBase). Le righe
         *    numerate legacy restano escluse (vedi commento di testa). Le chiavi nuove stanno
         *    in [1_000_000, ...): "moderne", quindi eseguite all'avvio prima di Main. */
        var prelude = new List<(List<Stmt> Statements, int Key)>();
        var preludeKey = ModuleInitSyntheticBase;
        foreach (var module in order)
        {
            foreach (var lineNumber in module.ExecutionOrder)
            {
                if (lineNumber < ModuleInitSyntheticBase) continue;
                if (!module.Statements.TryGetValue(lineNumber, out var statements) || statements.Count == 0) continue;
                prelude.Add((statements, preludeKey++));
            }
        }

        if (prelude.Count == 0) return;

        /* 2. Entry module: le chiavi numerate restano invariate (GOTO numerico e
         *    `lineNumber < 1_000_000` continuano a funzionare); le chiavi sintetiche sono
         *    spostate in una banda superiore al prelude, così l'ordine nei due dizionari non
         *    si mescola se qualcuno ordina per chiave. */
        var entryBase = Math.Max(10_000_000, preludeKey + 1_000_000);
        var merged = new Dictionary<int, List<Stmt>>();
        var execution = new List<int>(prelude.Count + main.ExecutionOrder.Length);

        foreach (var (statements, key) in prelude)
        {
            merged[key] = statements;
            execution.Add(key);
        }

        var keyMap = new Dictionary<int, int>();
        foreach (var lineNumber in main.ExecutionOrder)
        {
            if (!keyMap.TryGetValue(lineNumber, out var key))
            {
                key = lineNumber < ModuleInitSyntheticBase
                    ? lineNumber
                    : entryBase + keyMap.Count;
                keyMap[lineNumber] = key;
            }

            execution.Add(key);
        }

        foreach (var (original, remapped) in keyMap)
        {
            if (main.Statements.TryGetValue(original, out var statements))
                merged[remapped] = statements;
        }

        main.Statements = merged;
        main.ExecutionOrder = [.. execution];
        program.MainModule = main;
    }

    /// <summary>Ordine topologico dei moduli da inizializzare: prima le dipendenze (IMPORT),
    /// poi chi le importa; il modulo di ingresso è escluso (i suoi statement sono già in testa
    /// all'ExecutionOrder originale).</summary>
    private static List<BasicModule> ComputeInitOrder(BasicProgram program, BasicModule main)
    {
        var byPath = new Dictionary<string, BasicModule>(StringComparer.OrdinalIgnoreCase);
        foreach (var (path, module) in program.LoadedModules)
            byPath[Path.GetFullPath(path)] = module;

        /* namespace → moduli: la dichiarazione `NAMESPACE X` è già timbrata su ogni nodo di
         * primo livello dal parser (DeclaredNamespace), quindi non serve ri-scandire i file
         * (e il Linker non dipende dal progetto Pipeline). */
        var namespaceToModules = new Dictionary<string, List<BasicModule>>(StringComparer.OrdinalIgnoreCase);
        foreach (var module in byPath.Values)
        {
            foreach (var ns in DeclaredNamespaces(module))
            {
                if (!namespaceToModules.TryGetValue(ns, out var list))
                    namespaceToModules[ns] = list = [];
                if (!list.Contains(module)) list.Add(module);
            }
        }

        var result = new List<BasicModule>();
        var done = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var visiting = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        void Visit(BasicModule module)
        {
            var path = Path.GetFullPath(module.FilePath);
            if (done.Contains(path) || !visiting.Add(path)) return;   // cicli: si spezza qui

            foreach (var import in module.Imports)
            {
                if (!namespaceToModules.TryGetValue(import.NamespaceName.Lexeme, out var dependencies)) continue;
                foreach (var dependency in dependencies)
                {
                    if (ReferenceEquals(dependency, module)) continue;
                    Visit(dependency);
                }
            }

            visiting.Remove(path);
            done.Add(path);
            if (!ReferenceEquals(module, main)) result.Add(module);
        }

        Visit(main);

        /* Moduli non raggiungibili dagli IMPORT del main (es. file di pacchetto già
         * tokenizzati): si inizializzano comunque, dopo le dipendenze note. */
        foreach (var (_, module) in program.LoadedModules)
            Visit(module);

        return result;
    }

    /// <summary>Namespace dichiarati dal modulo (un file può contenerne più di uno).</summary>
    private static IEnumerable<string> DeclaredNamespaces(BasicModule module)
    {
        foreach (var node in module.Declarations)
        {
            switch (node)
            {
                case Decl decl when !string.IsNullOrEmpty(decl.DeclaredNamespace):
                    yield return decl.DeclaredNamespace!;
                    break;
                case DeclarationStmt ds when ds.Declarations.FirstOrDefault()?.DeclaredNamespace is { Length: > 0 } ns:
                    yield return ns;
                    break;
                case ConstStmt cs when cs.Declarations.FirstOrDefault()?.DeclaredNamespace is { Length: > 0 } ns:
                    yield return ns;
                    break;
            }
        }
    }
}
