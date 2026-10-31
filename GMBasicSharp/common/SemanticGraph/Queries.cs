// FILE: SemanticGraph/Queries.cs — §4.10: query sul grafo (definition/references/impact).

namespace GMBasic.SemanticGraph;

/// <summary>Risultato di una query di impatto: un simbolo raggiunto (inverso) + la profondità.</summary>
public sealed record ImpactNode(string SymbolId, int Depth, SemanticEdgeKind Via);

public static class SemanticGraphQueries
{
    /// <summary>Trova i simboli con un dato nome (case-insensitive).</summary>
    public static IReadOnlyList<SymbolNode> FindByName(this SemanticGraph g, string name, SemanticSymbolKind? kind = null)
        => g.Symbols.Where(s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase)
                             && (kind is null || s.Kind == kind))
                    .OrderBy(s => s.Line).ToList();

    /// <summary>Definition: il simbolo con quel nome (il primo per posizione), opz. del kind dato.</summary>
    public static SymbolNode? Definition(this SemanticGraph g, string name, SemanticSymbolKind? kind = null)
        => g.FindByName(name, kind).FirstOrDefault();

    /// <summary>Riferimenti diretti a un simbolo (edge entranti).</summary>
    public static IReadOnlyList<SemanticEdge> References(this SemanticGraph g, string symbolId)
        => g.InEdges(symbolId);

    /// <summary>Chiamanti diretti di una procedura (edge Calls entranti).</summary>
    public static IReadOnlyList<SemanticEdge> Callers(this SemanticGraph g, string symbolId)
        => g.InEdges(symbolId, SemanticEdgeKind.Calls);

    /// <summary>Chiamati diretti (edge Calls uscenti).</summary>
    public static IReadOnlyList<SemanticEdge> Callees(this SemanticGraph g, string symbolId)
        => g.OutEdges(symbolId, SemanticEdgeKind.Calls);

    /// <summary>Impact: chiusura transitiva ALL'INDIETRO (chi è impattato da un cambiamento di
    /// <paramref name="symbolId"/>), seguendo le relazioni che propagano (Calls/Uses/Defines/
    /// Reads/Writes/DependsOn/Inherits/Implements). Ritorna i nodi raggiunti (senza il seme).</summary>
    public static IReadOnlyList<ImpactNode> Impact(this SemanticGraph g, string symbolId, int maxDepth = 32)
    {
        var propagating = new HashSet<SemanticEdgeKind>
        {
            SemanticEdgeKind.Calls, SemanticEdgeKind.Uses, SemanticEdgeKind.Defines,
            SemanticEdgeKind.Reads, SemanticEdgeKind.Writes, SemanticEdgeKind.DependsOn,
            SemanticEdgeKind.Inherits, SemanticEdgeKind.Implements, SemanticEdgeKind.Returns
        };
        var seen = new Dictionary<string, ImpactNode>(StringComparer.Ordinal) { [symbolId] = new(symbolId, 0, SemanticEdgeKind.DependsOn) };
        var q = new Queue<string>(); q.Enqueue(symbolId);
        var result = new List<ImpactNode>();
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            var depth = seen[cur].Depth;
            if (depth >= maxDepth) continue;
            foreach (var e in g.InEdges(cur))
            {
                if (!propagating.Contains(e.Kind)) continue;
                // Il chiamante/riferimento è "From" (Call-site o simbolo contenitore); risaliamo
                // al simbolo contenitore del riferimento per l'impatto a livello di simbolo.
                var fromSym = ContainingSymbol(g, e.FromId);
                if (fromSym is null || seen.ContainsKey(fromSym)) continue;
                seen[fromSym] = new ImpactNode(fromSym, depth + 1, e.Kind);
                result.Add(seen[fromSym]);
                q.Enqueue(fromSym);
            }
        }
        return result;
    }

    /// <summary>Risale dal nodo (riferimento o simbolo) al simbolo contenitore.</summary>
    private static string? ContainingSymbol(SemanticGraph g, string nodeId)
    {
        if (g.GetSymbol(nodeId) is not null) return nodeId;
        if (g.GetReference(nodeId) is { } r) return r.EnclosingSymbolId;
        return null;
    }
}
