// FILE: SemanticGraph/Diff.cs — §4.10: diff del grafo tra due snapshot (semantic patch
// revision-bound). "Cosa è cambiato e chi ne è impattato".

namespace GMBasic.SemanticGraph;

/// <summary>Variazione di un simbolo tra due snapshot (per ID stabile).</summary>
public enum SymbolChangeKind { Added, Removed, Moved, TypeChanged, SignatureChanged, VisibilityChanged, Updated }

public sealed record SymbolChange(string SymbolId, SymbolChangeKind Kind, SymbolNode? Before, SymbolNode? After);

/// <summary>Risultato del diff: cambi + impatto (chi è toccato dalle rimozioni/modifiche).</summary>
public sealed record GraphDiffResult(
    string FromLabel,
    string ToLabel,
    IReadOnlyList<SymbolChange> SymbolChanges,
    IReadOnlyList<SemanticEdge> AddedEdges,
    IReadOnlyList<SemanticEdge> RemovedEdges,
    IReadOnlyList<string> ImpactedSymbols)
{
    public int AddedCount => SymbolChanges.Count(c => c.Kind == SymbolChangeKind.Added);
    public int RemovedCount => SymbolChanges.Count(c => c.Kind == SymbolChangeKind.Removed);
    public int ChangedCount => SymbolChanges.Count(c => c.Kind is not (SymbolChangeKind.Added or SymbolChangeKind.Removed));
}

public static class GraphDiff
{
    /// <summary>Diff semantico tra due snapshot. <paramref name="impactGraph"/> (lo snapshot
    /// "dopo", ri-materializzato) serve per calcolare chi è impattato dalle rimozioni/modifiche.</summary>
    public static GraphDiffResult Diff(GraphSnapshot from, GraphSnapshot to, SemanticGraph? impactGraph = null)
    {
        var before = from.Symbols.ToDictionary(s => s.Id, StringComparer.Ordinal);
        var after = to.Symbols.ToDictionary(s => s.Id, StringComparer.Ordinal);

        var changes = new List<SymbolChange>();
        foreach (var (id, a) in after)
        {
            if (!before.TryGetValue(id, out var b)) { changes.Add(new(id, SymbolChangeKind.Added, null, a)); continue; }
            var kind = ClassifyChange(b, a);
            if (kind != SymbolChangeKind.Updated) changes.Add(new(id, kind, b, a));
        }
        foreach (var (id, b) in before)
            if (!after.ContainsKey(id)) changes.Add(new(id, SymbolChangeKind.Removed, b, null));

        var edgeKey = (SemanticEdge e) => $"{e.FromId}\u0001{e.ToId}\u0001{e.Kind}";
        var beforeEdges = from.Edges.ToDictionary(edgeKey, e => e);
        var afterEdges = to.Edges.ToDictionary(edgeKey, e => e);
        var addedEdges = afterEdges.Where(kv => !beforeEdges.ContainsKey(kv.Key)).Select(kv => kv.Value).ToList();
        var removedEdges = beforeEdges.Where(kv => !afterEdges.ContainsKey(kv.Key)).Select(kv => kv.Value).ToList();

        // impatto: per ogni simbolo rimosso/modificato, chi lo referenziava (nel grafo "dopo")
        var impacted = new SortedSet<string>(StringComparer.Ordinal);
        if (impactGraph is not null)
        {
            foreach (var c in changes.Where(c => c.Kind != SymbolChangeKind.Added))
            {
                foreach (var e in impactGraph.InEdges(c.SymbolId))
                {
                    if (e.Kind is SemanticEdgeKind.Calls or SemanticEdgeKind.Uses or SemanticEdgeKind.Defines
                        or SemanticEdgeKind.DependsOn or SemanticEdgeKind.Inherits or SemanticEdgeKind.Implements)
                        impacted.Add(e.FromId);
                }
            }
        }
        return new GraphDiffResult(from.Label, to.Label, changes, addedEdges, removedEdges, [.. impacted]);
    }

    private static SymbolChangeKind ClassifyChange(SymbolNode b, SymbolNode a)
    {
        if (!string.Equals(b.Signature, a.Signature, StringComparison.Ordinal)) return SymbolChangeKind.SignatureChanged;
        if (!string.Equals(b.TypeName, a.TypeName, StringComparison.Ordinal)) return SymbolChangeKind.TypeChanged;
        if (b.IsExported != a.IsExported) return SymbolChangeKind.VisibilityChanged;
        if (b.Line != a.Line || b.Column != a.Column) return SymbolChangeKind.Moved;
        return SymbolChangeKind.Updated;
    }

    /// <summary>Materializza uno snapshot in un grafo (per le query di impatto).</summary>
    public static SemanticGraph Materialize(GraphSnapshot snap)
    {
        var g = new SemanticGraph();
        foreach (var s in snap.Symbols) g.AddSymbol(s);
        foreach (var r in snap.References) g.AddReference(r);
        foreach (var e in snap.Edges) g.AddEdge(e);
        return g;
    }
}
