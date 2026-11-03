// FILE: Ast.CollMembers.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-COLLMEMBERS-BEGIN ====
 * F1 piano Incongruenze — risoluzione della GRAFIA del membro di collection nel
 * TokenKind dedicato (Coll*), al punto di creazione del MethodCallExpr (Parser).
 * Regola d'oro (AGENTS §12): il TokenKind è la chiave canonica univoca, il lexeme è
 * solo la grafia sorgente. La tabella è l'UNICA sorgente del mapping membro→kind per
 * il frontend C# (IL, PCodeVM dispatchano per MethodName.Kind con
 * fallback lexeme per i membri fuori tabella — HTTP/WS/SQL/metodi utente).
    ==== GMB-COLLMEMBERS-END ==== */

using System.Collections.Immutable;
using System.Collections.Generic;
using System.Linq;

namespace Basic;

/// <summary>
/// Tabella membro-di-collection → TokenKind dedicato (Coll*). Il lexeme resta invariato
/// sul token (grafia sorgente); cambia solo il Kind. Case-insensitive (BASIC).
/// </summary>
public static class CollMembers
{
    private static readonly ImmutableDictionary<string, TokenKind> Map = ImmutableDictionary.CreateRange(
        StringComparer.OrdinalIgnoreCase, new Dictionary<string, TokenKind>
        {            ["Count"] = TokenKind.CollCount,
            ["Add"] = TokenKind.CollAdd,
            ["AddUnique"] = TokenKind.CollAddUnique,
            ["AddRange"] = TokenKind.CollAddRange,
            ["Insert"] = TokenKind.CollInsert,
            ["RemoveAt"] = TokenKind.CollRemoveAt,
            ["Remove"] = TokenKind.CollRemove,
            ["Clear"] = TokenKind.CollClear,
            ["Contains"] = TokenKind.CollContains,
            ["IndexOf"] = TokenKind.CollIndexOf,
            ["Join"] = TokenKind.CollJoin,
            ["Sort"] = TokenKind.CollSort,
            ["Reverse"] = TokenKind.CollReverse,
            ["Max"] = TokenKind.CollMax,
            ["Min"] = TokenKind.CollMin,
            ["Sum"] = TokenKind.CollSum,
            ["Union"] = TokenKind.CollUnion,
            ["Intersect"] = TokenKind.CollIntersect,
            ["Except"] = TokenKind.CollExcept,
            ["ToArray"] = TokenKind.CollToArray,
            ["ToList"] = TokenKind.CollToList,
            ["Push"] = TokenKind.CollPush,
            ["Pop"] = TokenKind.CollPop,
            ["Peek"] = TokenKind.CollPeek,
            ["Enqueue"] = TokenKind.CollEnqueue,
            ["Dequeue"] = TokenKind.CollDequeue,
            ["First"] = TokenKind.CollFirst,
            ["Last"] = TokenKind.CollLast,
            ["AddAfter"] = TokenKind.CollAddAfter,
            ["AddBefore"] = TokenKind.CollAddBefore,
            ["AddFirst"] = TokenKind.CollAddFirst,
            ["AddLast"] = TokenKind.CollAddLast,
            ["RemoveFirst"] = TokenKind.CollRemoveFirst,
            ["RemoveLast"] = TokenKind.CollRemoveLast,
            ["UniqueCount"] = TokenKind.CollUniqueCount,
            ["UniqueItems"] = TokenKind.CollUniqueItems,
            ["HasItem"] = TokenKind.CollHasItem,
            ["StartsWith"] = TokenKind.CollStartsWith,
            ["FindAll"] = TokenKind.CollFindAll,
            ["Capacity"] = TokenKind.CollCapacity,
            ["Keys"] = TokenKind.CollKeys,
            ["Values"] = TokenKind.CollValues,
            ["Set"] = TokenKind.CollSet,
            ["ContainsKey"] = TokenKind.CollContainsKey,
            ["Get"] = TokenKind.CollGet,
            ["TryGetValue"] = TokenKind.CollTryGetValue,
            ["TryGet"] = TokenKind.CollTryGet,
            ["ToDict"] = TokenKind.CollToDict,
            ["MinPair"] = TokenKind.CollMinPair,
            ["MaxPair"] = TokenKind.CollMaxPair,
            ["ExtractMin"] = TokenKind.CollExtractMin,
            ["ExtractMax"] = TokenKind.CollExtractMax,
            ["Rotate"] = TokenKind.CollRotate,
            ["Current"] = TokenKind.CollCurrent,
            ["Update"] = TokenKind.CollUpdate,
            ["Query"] = TokenKind.CollQuery,
            ["QueryRange"] = TokenKind.CollQueryRange,
            ["SetKeyValue"] = TokenKind.CollSetKeyValue,
            ["AddNode"] = TokenKind.CollAddNode,
            ["AddEdge"] = TokenKind.CollAddEdge,
            ["HasNode"] = TokenKind.CollHasNode,
            ["HasEdge"] = TokenKind.CollHasEdge,
            ["RemoveNode"] = TokenKind.CollRemoveNode,
            ["RemoveEdge"] = TokenKind.CollRemoveEdge,
            ["GetEdgeWeight"] = TokenKind.CollGetEdgeWeight,
            ["GetInDegree"] = TokenKind.CollGetInDegree,
            ["NodeCount"] = TokenKind.CollNodeCount,
            ["GetNeighbors"] = TokenKind.CollGetNeighbors,
            ["BFS"] = TokenKind.CollBFS,
            ["DFS"] = TokenKind.CollDFS,
            ["Dijkstra"] = TokenKind.CollDijkstra,
            ["Nodes"] = TokenKind.CollNodes,
            ["GetRange"] = TokenKind.CollGetRange,
            ["ToJSON"] = TokenKind.CollToJSON,
            ["Front"] = TokenKind.CollFront,
            ["Back"] = TokenKind.CollBack,
            ["IsEmpty"] = TokenKind.CollIsEmpty,
            ["AddNodeConsistent"] = TokenKind.CollAddNodeConsistent,
        });

    /// <summary>
    /// Ritorna il token con Kind = Coll* dedicato se il lexeme è un membro di collection;
    /// altrimenti il token originale (membri non-Coll* — HTTP/WS/SQL/utente — restano
    /// immutati). Il lexeme e la posizione NON cambiano mai.
    /// </summary>
    public static Token Remap(Token member)
    {
        if (Map.TryGetValue(member.Lexeme, out var kind) && member.Kind != kind)
            return new Token(kind, member.Lexeme, member.Literal, member.Position);
        return member;
    }

    /// <summary>Mappa inversa TokenKind → nome runtime (grafia canonica upper-case),
    /// derivata dalla tabella — unica sorgente per il runtime dispatch per kind
    /// (IlRuntime.CollectionInvokeByKind) e per l'emettitore C99 (chiave prima del
    /// fallback lexeme).</summary>
    public static readonly ImmutableDictionary<TokenKind, string> ByKind =
        ImmutableDictionary.CreateRange(
            Map.Select(kv => new KeyValuePair<TokenKind, string>(kv.Value, kv.Key.ToUpperInvariant())));

    /// <summary>Mappa inversa TokenKind → grafia canonica CamelCase ("Count", "Add", ...)
    /// — la forma che il chunk OP_CALL_MEMBER trasporta e su cui VM/C dispatcha
    /// (strcmp). BASIC è case-insensitive: il lexeme sorgente può essere ".count",
    /// ".Count" o ".COUNT" — il chunk deve portare UNA sola forma stabile.</summary>
    public static readonly ImmutableDictionary<TokenKind, string> CanonicalByKind =
        ImmutableDictionary.CreateRange(
            Map.Select(kv => new KeyValuePair<TokenKind, string>(kv.Value, kv.Key)));

    /// <summary>True se il TokenKind è un membro di collection (Coll*).</summary>
    public static bool IsCollMember(TokenKind kind) => ByKind.ContainsKey(kind);

    /// <summary>True se il token è il membro <paramref name="kind"/>: usa il TokenKind
    /// risolto dal parser quando presente, altrimenti la grafia canonica del kind (fallback
    /// per i token non rimappati, es. FieldAccessExpr o costruzioni del linker). Il lexeme
    /// non è più la chiave di dispatch: è solo il fallback della frontiera.</summary>
    public static bool Is(Token member, TokenKind kind)
    {
        if (member.Kind == kind) return true;
        if (IsCollMember(member.Kind)) return false;
        var canon = Canonical(kind);
        return canon is not null && string.Equals(member.Lexeme, canon, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Nome runtime canonico (UPPER) del membro per TokenKind; null se non è
    /// un membro di collection (chiave di dispatch per Kind con fallback lexeme).</summary>
    public static string? RuntimeName(TokenKind kind) => ByKind.TryGetValue(kind, out var n) ? n : null;

    /// <summary>Grafia canonica (CamelCase) del membro per TokenKind; null se non è un
    /// membro di collection — per la normalizzazione del nome nel chunk OP_CALL_MEMBER.</summary>
    public static string? Canonical(TokenKind kind) => CanonicalByKind.TryGetValue(kind, out var n) ? n : null;
}
