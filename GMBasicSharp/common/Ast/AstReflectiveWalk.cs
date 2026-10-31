// FILE: Ast/AstReflectiveWalk.cs
// Helper condiviso per i walk RIFLESSIVI sull'AST (linker, DCE, compiler, emitter).
//
// Perché esiste (bug 2026-09-16, `Bugs/(open) - Linker-FindExplicitCallTo-StackOverflow-Su-Nodi-UDT`):
// i walk riflessivi generici scorrono TUTTE le proprietà pubbliche di un nodo e ricorrono in ogni
// `AstNode`/`IEnumerable<AstNode>`. I nodi UDT hanno una **back-reference al genitore**
// (`UserDefinedType.Parent`, `common/Ast/DataTypes.UdtClasses.cs:268`) che NON è un figlio
// strutturale: seguirla fa risalire l'albero all'infinito → `Stack overflow` del processo, che
// ABORTA l'intero sweep (gate emitter C# full-suite e Conformance) su qualunque sample con
// classi/UDT.
//
// Regola: ogni walk riflessivo DEVE usare <see cref="ChildNodes"/> (che salta le back-reference e
// le `ImmutableArray` mai inizializzate) e, quando ricorre su sottografi condivisi, tenere un
// `HashSet&lt;AstNode&gt;(ReferenceEqualityComparer.Instance)` dei nodi già visitati.

using System.Collections;
using System.Collections.Immutable;
using System.Reflection;

namespace Basic;

/// <summary>Traversal riflessivo sicuro dei figli di un nodo AST.</summary>
public static class AstReflectiveWalk
{
    /// <summary>Nomi delle proprietà che sono <b>back-reference al genitore</b>, non figli
    /// strutturali. Seguirle fa ciclare il walk (v. commento di testa).</summary>
    public static readonly HashSet<string> BackReferencePropertyNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "Parent", "Owner", "Container"
    };

    private static readonly Dictionary<Type, PropertyInfo[]> PropertyCache = new();

    private static PropertyInfo[] GetWalkableProperties(Type type)
    {
        if (PropertyCache.TryGetValue(type, out var cached)) return cached;

        var props = type.GetProperties(BindingFlags.Public | BindingFlags.Instance)
            .Where(p => p.GetIndexParameters().Length == 0)
            .Where(p => !BackReferencePropertyNames.Contains(p.Name))
            .ToArray();

        PropertyCache[type] = props;
        return props;
    }

    /// <summary>Figli AST strutturali del nodo (una proprietà = un figlio; una collezione =
    /// i suoi item `AstNode`). Salta back-reference, indicizzatori, stringhe, tipi non-AST e
    /// `ImmutableArray&lt;T&gt;` mai inizializzate (iterarle lancia `InvalidOperationException`).</summary>
    public static IEnumerable<AstNode> ChildNodes(AstNode node)
    {
        foreach (var prop in GetWalkableProperties(node.GetType()))
        {
            object? val;
            try { val = prop.GetValue(node); }
            catch { continue; }

            switch (val)
            {
                case AstNode child:
                    yield return child;
                    break;
                case null:
                case string:
                    break;
                case IEnumerable en:
                    if (IsDefaultImmutableArray(val)) break;
                    foreach (var item in en)
                        if (item is AstNode itemNode)
                            yield return itemNode;
                    break;
            }
        }
    }

    /// <summary>True se <paramref name="val"/> è una <c>ImmutableArray&lt;T&gt;</c> mai
    /// inizializzata (`IsDefault`): iterarla lancia, quindi il walk la salta.</summary>
    public static bool IsDefaultImmutableArray(object val)
    {
        var t = val.GetType();
        return t.IsGenericType &&
               t.GetGenericTypeDefinition() == typeof(ImmutableArray<>) &&
               (bool)t.GetProperty("IsDefault")!.GetValue(val)!;
    }
}
