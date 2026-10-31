// FILE: SemanticGraph/Model.cs — §4.10 GMB Semantic Graph: data model.
// Nodi = simboli (SUB/FUNCTION/var/tipi/…) E espressioni/statement; edge = relazioni; + snapshot.

using System.Text.Json.Serialization;

namespace GMBasic.SemanticGraph;

/// <summary>Categoria di un nodo-simbolo.</summary>
public enum SemanticSymbolKind
{
    Namespace, Module, Procedure, Function, Method, Variable, Parameter,
    Constant, Type, Class, Interface, Enum, Field, Property, Operator, Unknown
}

/// <summary>Tipo di relazione (edge) tra nodi/rifermi.</summary>
public enum SemanticEdgeKind
{
    Defines,     // container -> simbolo definito
    Calls,       // call-site -> procedura
    Uses,        // riferimento generico -> simbolo
    Reads,       // lettura di variabile
    Writes,      // scrittura di variabile
    Returns,     // procedura -> tipo/espressione restituita
    Inherits,    // classe -> base
    Implements,  // classe -> interfaccia
    Imports,     // modulo -> namespace/membro importato
    Contains,    // simbolo -> espressione/statement contenuto (granularità fine)
    DependsOn    // dipendenza generica (es. tipo di parametro)
}

/// <summary>Nodo-simbolo del grafo. <see cref="Id"/> è STABILE tra revisioni (non l'id di
/// compilazione): namespace + kind + nome + firma.</summary>
public sealed record SymbolNode(
    string Id,
    SemanticSymbolKind Kind,
    string Name,
    string? Namespace,
    string? ContainerId,
    string File,
    int Line,
    int Column,
    string? TypeName = null,
    string? Signature = null,
    bool IsExported = false)
{
    /// <summary>Etichetta leggibile (namespace.nome).</summary>
    public string Label => string.IsNullOrEmpty(Namespace) ? Name : $"{Namespace}.{Name}";
}

/// <summary>Nodo-riferimento: un'espressione/statement (call-site, accesso, assegnazione).
/// Serve per la granularità fine (rename/refactor) e per localizzare gli usi.</summary>
public sealed record ReferenceNode(
    string Id,
    string NodeKind,          // es. "Call", "VarRef", "Assignment", "Expr"
    string Text,              // resa testuale breve del nodo sorgente
    string File,
    int Line,
    int Column,
    string? EnclosingSymbolId = null);

/// <summary>Relazione orientata.</summary>
public sealed record SemanticEdge(
    string FromId,
    string ToId,
    SemanticEdgeKind Kind,
    string File,
    int Line,
    int Column);

/// <summary>Snapshot serializzabile di un grafo (per il diff revision-bound).</summary>
public sealed record GraphSnapshot(
    int Version,
    string Label,
    IReadOnlyList<SymbolNode> Symbols,
    IReadOnlyList<ReferenceNode> References,
    IReadOnlyList<SemanticEdge> Edges);

/// <summary>Grafo semantico in memoria con gli indici per le query.</summary>
public sealed class SemanticGraph
{
    private readonly Dictionary<string, SymbolNode> _symbols = new(StringComparer.Ordinal);
    private readonly Dictionary<string, ReferenceNode> _refs = new(StringComparer.Ordinal);
    private readonly List<SemanticEdge> _edges = [];
    private readonly Dictionary<string, List<SemanticEdge>> _out = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<SemanticEdge>> _in = new(StringComparer.Ordinal);

    public IReadOnlyCollection<SymbolNode> Symbols => _symbols.Values;
    public IReadOnlyCollection<ReferenceNode> References => _refs.Values;
    public IReadOnlyList<SemanticEdge> Edges => _edges;

    public void AddSymbol(SymbolNode n) => _symbols[n.Id] = n;
    public void AddReference(ReferenceNode r) => _refs[r.Id] = r;

    public void AddEdge(SemanticEdge e)
    {
        _edges.Add(e);
        if (!_out.TryGetValue(e.FromId, out var o)) _out[e.FromId] = o = [];
        o.Add(e);
        if (!_in.TryGetValue(e.ToId, out var i)) _in[e.ToId] = i = [];
        i.Add(e);
    }

    public SymbolNode? GetSymbol(string id) => _symbols.TryGetValue(id, out var n) ? n : null;
    public ReferenceNode? GetReference(string id) => _refs.TryGetValue(id, out var r) ? r : null;

    /// <summary>Edge uscenti da un nodo (opz. filtrati per tipo).</summary>
    public IReadOnlyList<SemanticEdge> OutEdges(string id, SemanticEdgeKind? kind = null)
        => _out.TryGetValue(id, out var l) ? (kind is null ? l : l.Where(e => e.Kind == kind).ToList()) : [];

    /// <summary>Edge entranti in un nodo (opz. filtrati per tipo).</summary>
    public IReadOnlyList<SemanticEdge> InEdges(string id, SemanticEdgeKind? kind = null)
        => _in.TryGetValue(id, out var l) ? (kind is null ? l : l.Where(e => e.Kind == kind).ToList()) : [];

    public GraphSnapshot ToSnapshot(string label) =>
        new(1, label, [.. _symbols.Values], [.. _refs.Values], [.. _edges]);
}
