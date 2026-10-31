// SemanticGraph/SemanticQueryHook.cs — §4.10(f) Opzione A: registra il folder che risolve
// AI.SYMBOL/IMPACT/CALLMAP a LINK-time usando il grafo del programma.
// Il Linker NON referenzia il grafo (ciclo SemanticGraph→Compilation): il folder e' INIETTATO qui.
using System.Runtime.CompilerServices;
using System.Text;
using Basic;                    // Linker, TokenKind (via Ast)

namespace GMBasic.SemanticGraph;

/// <summary>§4.10(f) — superficie pubblica per forzare la registrazione del folder semantico nel
/// Linker. I backend (IL/PCode) la chiamano prima di compilare: garantisce il load
/// dell'assembly (e quindi l'esecuzione del ModuleInitializer) + l'idempotenza.</summary>
public static class SemanticSurface
{
    private static bool _done;
    /// <summary>Registra (una volta) il folder AI.SYMBOL/IMPACT/CALLMAP nel Linker.</summary>
    public static void EnsureRegistered() { if (_done) return; _done = true; SemanticQueryHook.Init(); }
}

internal static class SemanticQueryHook
{
    [ModuleInitializer]
    internal static void Init()
    {
        Linker.SemanticQueryFolder = static (program, kind, name, depth) =>
        {
            var g = SemanticGraphExtractor.Extract(program);
            bool noName = kind is TokenKind.AiRevision or TokenKind.AiCheckrev or TokenKind.AiRepomap or TokenKind.AiFindsymbol or TokenKind.AiFileoutline or TokenKind.LlmVerify or TokenKind.LlmCommit;
            var s = noName ? null : g.Definition(name);
            if (!noName && s is null) return "";
            // Mini-walk dell'AST per la call graph (nome-chiamante, nome-chiamato): robusto,
            // non dipende dai ReferenceNode/edge dell'estrattore. Pari al C (name-based).
            int bceDepth = depth;
            var callPairs = new List<(string Caller, string Callee)>();
            // §4.10 write-side: i semantic patch presenti (per i receipt LLM.VERIFY()/LLM.COMMIT()).
            var patches = new List<Basic.SemanticPatchStmt>();
            string CallGraphHash()
            {
                var prs = callPairs.Select(c => c.Caller + "->" + c.Callee).Distinct().OrderBy(x => x, StringComparer.Ordinal);
                ulong h = 1469598103934665603UL;
                foreach (var x in prs) foreach (var ch in x) { h ^= (byte)ch; h *= 1099511628211UL; }
                return h.ToString("x16");
            }

            void W(AstNode n, string encl)
            {
                if (n is Basic.ProcedureDefinition) return;
                if (n is Basic.SemanticPatchStmt sp2) { patches.Add(sp2); return; }
                if (n is Basic.FunctionExpr fe && fe.OriginalToken is not null) callPairs.Add((encl, fe.OriginalToken.Lexeme));
                else if (n is Basic.UnresolvedCallExpr ue) callPairs.Add((encl, ue.Name.Lexeme));
                else if (n is Basic.MethodCallExpr me) callPairs.Add((encl, me.MethodName.Lexeme));
                // LetStmt: `Assignment` non e' AstNode -> esplicito.
                if (n is Basic.LetStmt let)
                {
                    foreach (var a in let.Assignments)
                    {
                        if (a.Value is not null) W(a.Value, encl);
                        if (a.Target is AstNode tn) W(tn, encl);
                    }
                    return;
                }
                // PRINT/CPRINT: `PrintArg` e' struct -> esplicito.
                if (n is Basic.PrintStmt ps) { foreach (var a in ps.Arguments) if (a.Expression is not null) W(a.Expression, encl); return; }
                if (n is Basic.CPrintStmt cps) { foreach (var a in cps.Arguments) if (a.Expression is not null) W(a.Expression, encl); return; }
                foreach (var c in Basic.AstReflectiveWalk.ChildNodes(n)) W(c, encl);
            }
            var seenPd = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var walkedNames = new List<string>();
            foreach (var mod in program.LoadedModules.Values)
                foreach (var decl in mod.Declarations)
                    if (decl is Basic.ProcedureDefinition pd && !pd.Body.IsDefaultOrEmpty && seenPd.Add(pd.OriginalName.Lexeme))
                        { walkedNames.Add(pd.OriginalName.Lexeme + "(B" + pd.Body.Length + ")"); foreach (var st in pd.Body) W(st, pd.OriginalName.Lexeme); }
            foreach (var pr in program.GetAllProcedures())
                if (seenPd.Add(pr.OriginalName.Lexeme))
                    foreach (var st in pr.Body) W(st, pr.OriginalName.Lexeme);
            switch (kind)
            {
                case TokenKind.AiSymbol:
                    return $"{KindName(s.Kind)} {s.Label}";
                case TokenKind.AiCallmap:
                {
                    var callers = callPairs.Where(c => c.Callee == s.Name).Select(c => c.Caller).Distinct();
                    var callees = callPairs.Where(c => c.Caller == s.Name).Select(c => c.Callee).Distinct();
                    return $"callers=[{string.Join(",", callers)}] callees=[{string.Join(",", callees)}]";
                }
                case TokenKind.AiImpact:
                {
                    var seen = new HashSet<string>(StringComparer.Ordinal) { s.Name };
                    var outList = new List<string>();
                    var cur = new List<string> { s.Name };
                    for (int d = 0; d < depth && cur.Count > 0; d++)
                    {
                        var nxt = new List<string>();
                        foreach (var to in cur)
                            foreach (var c in callPairs.Where(x => x.Callee == to))
                                if (seen.Add(c.Caller)) { outList.Add($"{c.Caller}@{d + 1}"); nxt.Add(c.Caller); }
                        cur = nxt;
                    }
                    return string.Join(",", outList);
                }
                case TokenKind.AiRevision:
                    return "rev=" + CallGraphHash();
                case TokenKind.AiCheckrev:
                    return (name == "rev=" + CallGraphHash()) ? "match" : "mismatch";
                case TokenKind.AiFindsymbol:
                {
                    var names = callPairs.SelectMany(c => new[] { c.Caller, c.Callee })
                        .Where(x => x.Contains(name, StringComparison.OrdinalIgnoreCase))
                        .Distinct().OrderBy(x => x, StringComparer.Ordinal);
                    return $"find {name}: [{string.Join(",", names)}]";
                }
                case TokenKind.AiRepomap:
                {
                    var all = callPairs.Select(c => c.Caller + "->" + c.Callee).Distinct().OrderBy(x => x, StringComparer.Ordinal);
                    return $"repomap: [{string.Join(";", all)}]";
                }
                case TokenKind.AiFileoutline:
                {
                    // §3.6 PARSEC — outline per-file: i simboli DEFINITI nel file `name` (match per
                    // basename o suffix), in ordine di linea, resi come "kind label" separati da ";".
                    string Fn(string f) => System.IO.Path.GetFileName(f);
                    var syms = g.Symbols
                        .Where(x => !string.IsNullOrEmpty(x.File) &&
                                    (string.Equals(Fn(x.File), name, StringComparison.OrdinalIgnoreCase) ||
                                     x.File!.EndsWith(name, StringComparison.OrdinalIgnoreCase)))
                        .OrderBy(x => x.Line).ThenBy(x => x.Name, StringComparer.Ordinal)
                        .Select(x => KindName(x.Kind) + " " + x.Label);
                    return string.Join(";", syms);
                }
                case TokenKind.AiPatch:
                {
                    // Semantic patch (receipt): target + impatto (chi lo chiama, transitivo). Il gate
                    // di revisione e' AI.CHECKREV. decision=APPLY se il target esiste.
                    var impacted = new List<string>();
                    var seenP = new HashSet<string>(StringComparer.Ordinal) { s.Name };
                    var fr2 = new List<string> { s.Name };
                    for (int lvl = 0; lvl < 32 && fr2.Count > 0; lvl++)
                    {
                        var nx = new List<string>();
                        foreach (var n in fr2) foreach (var c in callPairs) if (c.Callee == n && seenP.Add(c.Caller)) { impacted.Add($"{c.Caller}@{lvl + 1}"); nx.Add(c.Caller); }
                        fr2 = nx;
                    }
                    return $"patch target={s.Name} impact=[{string.Join(",", impacted)}] decision=APPLY";
                }
                case TokenKind.AiContext:
                {
                    // Contesto limitato: il simbolo + i vicini (callers/callees) fino a `depth`.
                    var seenC = new HashSet<string>(StringComparer.Ordinal) { s.Name };
                    var frontier = new List<string> { s.Name };
                    int d = bceDepth;
                    for (int lvl = 0; lvl < d && frontier.Count > 0; lvl++)
                    {
                        var nxt = new List<string>();
                        foreach (var n in frontier)
                        {
                            foreach (var c in callPairs) { if (c.Caller == n && seenC.Add(c.Callee)) nxt.Add(c.Callee); if (c.Callee == n && seenC.Add(c.Caller)) nxt.Add(c.Caller); }
                        }
                        frontier = nxt;
                    }
                    var names = seenC.OrderBy(x => x, StringComparer.Ordinal);
                    return $"context {s.Name}: [{string.Join(",", names)}]";
                }
                case TokenKind.LlmVerify:
                {
                    // Receipt del gate revision-bound: ok se TUTTI i patch hanno rev == corrente.
                    if (patches.Count == 0) return "verify: none";
                    var rev = "rev=" + CallGraphHash();
                    bool allOk = patches.All(p => string.Equals(p.ExpectedRev, rev, StringComparison.Ordinal));
                    return "verify: " + (allOk ? "ok" : "rejected");
                }
                case TokenKind.LlmCommit:
                {
                    // Receipt del commit: applied se TUTTI i patch sono verificati (rev == corrente).
                    if (patches.Count == 0) return "commit: none";
                    var rev = "rev=" + CallGraphHash();
                    bool allOk = patches.All(p => string.Equals(p.ExpectedRev, rev, StringComparison.Ordinal));
                    return "commit: " + (allOk ? "applied" : "rejected");
                }
                case TokenKind.LlmInspect:
                {
                    // Hover: kind+label + callmap immediato (callers/callees). Tutti campi parity-safe.
                    var callers = callPairs.Where(c => c.Callee == s.Name).Select(c => c.Caller).Distinct();
                    var callees = callPairs.Where(c => c.Caller == s.Name).Select(c => c.Callee).Distinct();
                    return $"{KindName(s.Kind)} {s.Label} callers=[{string.Join(",", callers)}] callees=[{string.Join(",", callees)}]";
                }
                default:
                    return "";
            }
        };
    }

    /// <summary>Nome del simbolo con un dato Id (o null se e' un ref-id / non-simbolo).</summary>
    private static string? NameOf(SemanticGraph g, string id)
        => g.Symbols.FirstOrDefault(s => s.Id == id)?.Name;

    /// <summary>Nome del kind in minuscolo (pari a `gmb_sym_kind_name` C: "procedure"/"function"/…).</summary>
    private static string KindName(SemanticSymbolKind k) =>
        k switch
        {
            SemanticSymbolKind.Namespace => "namespace",
            SemanticSymbolKind.Module => "module",
            SemanticSymbolKind.Procedure => "procedure",
            SemanticSymbolKind.Function => "function",
            SemanticSymbolKind.Method => "method",
            SemanticSymbolKind.Variable => "variable",
            SemanticSymbolKind.Parameter => "parameter",
            SemanticSymbolKind.Constant => "constant",
            SemanticSymbolKind.Type => "type",
            SemanticSymbolKind.Class => "class",
            SemanticSymbolKind.Interface => "interface",
            SemanticSymbolKind.Enum => "enum",
            SemanticSymbolKind.Field => "field",
            SemanticSymbolKind.Property => "property",
            SemanticSymbolKind.Operator => "operator",
            _ => "unknown",
        };
}
