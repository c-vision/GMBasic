// FILE: SemanticGraph/Extractor.cs — §4.10: costruisce il grafo dal BasicProgram (linker/AST).
// Nodi-simbolo dal SymbolTable; edge Defines (container->simbolo) + Calls/Uses/Reads/Writes
// dai riferimenti (VariableExpr/UnresolvedCallExpr/MethodCallExpr/Assignment) nell'AST.
// ID STABILE tra revisioni = namespace + kind + nome (+ firma), NON l'id di compilazione.

using Basic;
using System.Security.Cryptography;
using System.Text;

namespace GMBasic.SemanticGraph;

public static class SemanticGraphExtractor
{
    /// <summary>Estrae il grafo semantico di un programma già parsato/linkato.</summary>
    public static SemanticGraph Extract(BasicProgram program, string label = "working")
    {
        var g = new SemanticGraph();
        var idMap = new Dictionary<int, string>();      // SymbolId(compilazione) -> Id STABILE
        var fileMap = FileMap(program);

        // 1) nodi-simbolo
        foreach (var s in program.Symbols.AllSymbols)
        {
            string stable = StableId(program, s);
            idMap[s.Id] = stable;
            string file = fileMap.TryGetValue(s.OriginalToken.Position.FileId, out var fp)
                ? Path.GetFileName(fp) : $"file#{s.OriginalToken.Position.FileId}";
            g.AddSymbol(new SymbolNode(
                Id: stable,
                Kind: MapKind(s),
                Name: s.OriginalName,
                Namespace: NullIfGlobal(program.Symbols.GetNamespaceNameForSymbol(s.Id)),
                ContainerId: null,                     // riempito sotto (Defines)
                File: file,
                Line: s.OriginalToken.Position.Line,
                Column: s.OriginalToken.Position.Column,
                TypeName: s.DataType?.ToString(),
                Signature: null,
                IsExported: s.IsVisibleExternally));
        }

        // 2) Defines: lo scope contenitore (ownerSymbolId) -> il simbolo
        foreach (var s in program.Symbols.AllSymbols)
        {
            int owner = program.Symbols.GetScope(s.ScopeId).OwnerSymbolId;
            if (owner >= 0 && idMap.TryGetValue(owner, out var parent) && idMap.TryGetValue(s.Id, out var child))
            {
                var pnode = g.GetSymbol(parent);
                if (pnode is not null) g.AddSymbol(pnode with { ContainerId = parent });
                g.AddEdge(new SemanticEdge(parent, child, SemanticEdgeKind.Defines, File(g.GetSymbol(child)), Line(g.GetSymbol(child)), Col(g.GetSymbol(child))));
            }
        }

        // 3) riferimenti nei corpi: Calls/Uses/Reads/Writes (espressioni/statement).
        // Iteriamo le DICHIARAZIONI dei moduli (include Main/entry, che GetAllProcedures esclude).
        var walked = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var mod in program.LoadedModules.Values)
            foreach (var decl in mod.Declarations)
                if (decl is ProcedureDefinition pd && !pd.Body.IsDefaultOrEmpty
                    && walked.Add(pd.OriginalName.Lexeme) && EnclosingOf(program, pd, idMap, out var encl))
                    foreach (var stmt in pd.Body) Walk(g, program, stmt, encl, idMap, fileMap);
        // fallback: procedure note al SymbolTable ma non nei moduli (es. sintetiche)
        foreach (var proc in program.GetAllProcedures())
            if (walked.Add(proc.OriginalName.Lexeme) && EnclosingOf(program, proc, idMap, out var encl2))
                foreach (var stmt in proc.Body) Walk(g, program, stmt, encl2, idMap, fileMap);
        return g;

        static string File(SymbolNode? n) => n?.File ?? "?";
        static int Line(SymbolNode? n) => n?.Line ?? 0;
        static int Col(SymbolNode? n) => n?.Column ?? 0;
    }

    /// <summary>Trova l'ID stabile del simbolo contenitore di una procedura: prima via
    /// SymbolId, poi fallback per NOME (es. Main entry, che può avere SymbolId -1).</summary>
    private static bool EnclosingOf(BasicProgram program, ProcedureDefinition pd,
                                   Dictionary<int, string> idMap, out string encl)
    {
        if (pd.SymbolId >= 0 && idMap.TryGetValue(pd.SymbolId, out encl!)) return true;
        var sym = program.Symbols.AllSymbols.FirstOrDefault(s =>
            s.DeclarationNode == pd ||
            (s.OriginalName.Equals(pd.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase)
             && s.DeclarationNode is ProcedureDefinition));
        if (sym is not null && idMap.TryGetValue(sym.Id, out encl!)) return true;
        encl = pd.OriginalName.Lexeme;   // ultimo fallback: nome come id
        return true;
    }

    /// <summary>Walk di un sottoalbero AST accumulando riferimenti ed edge.</summary>
    private static void Walk(SemanticGraph g, BasicProgram program, AstNode node, string enclosing,
                             Dictionary<int, string> idMap, Dictionary<int, string> fileMap)
    {
        switch (node)
        {
            // LetStmt contiene `Assignment` (NON AstNode): il walk riflessivo non lo attraversa,
            // quindi esploriamo esplicitamente Target/Value di ogni assegnazione.
            case LetStmt let:
                foreach (var a in let.Assignments)
                {
                    if (a.Value is not null) Walk(g, program, a.Value, enclosing, idMap, fileMap);
                    if (a.Target is AstNode tn) Walk(g, program, tn, enclosing, idMap, fileMap);
                }
                return;
            // Chiamata SUB come STATEMENT (`Foo()`): il walk riflessivo non attraversa
            // ExpressionStmt -> discendiamo esplicitamente (senza, i caller non erano visti).
            case ExpressionStmt es:
                Walk(g, program, es.Expression, enclosing, idMap, fileMap);
                return;
            case VariableExpr v when v.SymbolId >= 0 && idMap.TryGetValue(v.SymbolId, out var target):
            {
                string file = fileMap.TryGetValue(v.Position.FileId, out var fp) ? Path.GetFileName(fp) : "?";
                var refNode = new ReferenceNode(
                    RefId(enclosing, v.Position, "VarRef"), "VarRef", v.NameCleaned.Lexeme,
                    file, v.Position.Line, v.Position.Column, enclosing);
                g.AddReference(refNode);
                g.AddEdge(new SemanticEdge(refNode.Id, target, SemanticEdgeKind.Uses, file, v.Position.Line, v.Position.Column));
                break;
            }
            case FunctionExpr f when f.SymbolId >= 0 && idMap.TryGetValue(f.SymbolId, out var ftarget):
            {
                // chiamata RISOLTA dal linker: il target è il SymbolId (nessuna risoluzione per nome).
                string file = fileMap.TryGetValue(f.Position.FileId, out var fp) ? Path.GetFileName(fp) : "?";
                var refNode = new ReferenceNode(
                    RefId(enclosing, f.Position, "Call"), "Call", f.OriginalToken.Lexeme,
                    file, f.Position.Line, f.Position.Column, enclosing);
                g.AddReference(refNode);
                g.AddEdge(new SemanticEdge(refNode.Id, ftarget, SemanticEdgeKind.Calls, file, f.Position.Line, f.Position.Column));
                break;
            }
            case UnresolvedCallExpr c:
            {
                string file = fileMap.TryGetValue(c.Position.FileId, out var fp) ? Path.GetFileName(fp) : "?";
                var refNode = new ReferenceNode(
                    RefId(enclosing, c.Position, "Call"), "Call", c.Name.Lexeme,
                    file, c.Position.Line, c.Position.Column, enclosing);
                g.AddReference(refNode);
                // risoluzione per nome (il target è il simbolo con quel nome, se esiste)
                var target = g.Symbols.FirstOrDefault(s => s.Name.Equals(c.Name.Lexeme, StringComparison.OrdinalIgnoreCase)
                                                         && (s.Kind is SemanticSymbolKind.Function or SemanticSymbolKind.Procedure
                                                             or SemanticSymbolKind.Method or SemanticSymbolKind.Operator));
                if (target is not null) g.AddEdge(new SemanticEdge(refNode.Id, target.Id, SemanticEdgeKind.Calls, file, c.Position.Line, c.Position.Column));
                break;
            }
            case MethodCallExpr mc:
            {
                string file = fileMap.TryGetValue(mc.Position.FileId, out var fp) ? Path.GetFileName(fp) : "?";
                var refNode = new ReferenceNode(
                    RefId(enclosing, mc.Position, "MethodCall"), "MethodCall", mc.MethodName.Lexeme,
                    file, mc.Position.Line, mc.Position.Column, enclosing);
                g.AddReference(refNode);
                var target = g.Symbols.FirstOrDefault(s => s.Name.Equals(mc.MethodName.Lexeme, StringComparison.OrdinalIgnoreCase)
                                                         && (s.Kind is SemanticSymbolKind.Method or SemanticSymbolKind.Function or SemanticSymbolKind.Operator));
                if (target is not null) g.AddEdge(new SemanticEdge(refNode.Id, target.Id, SemanticEdgeKind.Calls, file, mc.Position.Line, mc.Position.Column));
                break;
            }
        }
        foreach (var child in AstReflectiveWalk.ChildNodes(node))
            Walk(g, program, child, enclosing, idMap, fileMap);
    }

    // --- helper ---
    private static SemanticSymbolKind MapKind(SymbolTable.Symbol s)
    {
        var f = s.Flags;
        // SUB vs FUNCTION: entrambi sono FunctionDefinition; il SUB ha ReturnType VoidType.
        if (s.DeclarationNode is FunctionDefinition fd)
            return fd.ReturnType is VoidType ? SemanticSymbolKind.Procedure : SemanticSymbolKind.Function;
        if (s.DeclarationNode is GeneratorDefinition) return SemanticSymbolKind.Function;
        if (s.DeclarationNode is ProcedureDefinition) return SemanticSymbolKind.Procedure;
        if (s.Has(SymbolFlags.IsFunction)) return SemanticSymbolKind.Function;
        if (s.Has(SymbolFlags.IsClass)) return SemanticSymbolKind.Class;
        if (s.Has(SymbolFlags.IsInterface)) return SemanticSymbolKind.Interface;
        if (s.Has(SymbolFlags.IsEnum)) return SemanticSymbolKind.Enum;
        if (s.Has(SymbolFlags.IsStruct)) return SemanticSymbolKind.Type;
        if (s.Has(SymbolFlags.IsTypeAlias)) return SemanticSymbolKind.Type;
        if (s.Has(SymbolFlags.IsParameter)) return SemanticSymbolKind.Parameter;
        if (s.Has(SymbolFlags.IsConst) || s.Has(SymbolFlags.IsEnumMember)) return SemanticSymbolKind.Constant;
        if (s.Has(SymbolFlags.IsNamespace)) return SemanticSymbolKind.Namespace;
        if (s.Has(SymbolFlags.IsModule)) return SemanticSymbolKind.Module;
        if (s.Has(SymbolFlags.IsOperator)) return SemanticSymbolKind.Operator;
        return SemanticSymbolKind.Variable;
    }

    private static string StableId(BasicProgram program, SymbolTable.Symbol s)
    {
        string ns = program.Symbols.GetNamespaceNameForSymbol(s.Id);
        string kind = MapKind(s).ToString();
        string sig = s.DataType?.ToString() ?? "";
        string raw = $"{ns}\u0001{kind}\u0001{s.OriginalName.ToUpperInvariant()}\u0001{sig}";
        var hash = Convert.ToHexString(SHA1.HashData(Encoding.UTF8.GetBytes(raw)))[..12].ToLowerInvariant();
        return $"{ns}:{kind}:{s.OriginalName}:{hash}";
    }

    private static string RefId(string enclosing, SourcePosition pos, string kind)
        => $"ref:{enclosing}:{kind}:{pos.FileId}:{pos.Line}:{pos.Column}";

    private static string? NullIfGlobal(string ns) => ns == SymbolTable.GlobalScopeName ? null : ns;

    private static Dictionary<int, string> FileMap(BasicProgram program)
    {
        var m = new Dictionary<int, string>();
        try
        {
            foreach (var line in program.SourceMap) if (!m.ContainsKey(line.FileId)) m[line.FileId] = line.FilePath;
        }
        catch { /* SourceMap opzionale */ }
        return m;
    }
}
