// FILE: Interpreter.StdLib.Brains.cs
// Namespace BRAINS — funzioni qualificate per i grafi dei knowledge base (brain).
// Ex namespace GRAPH (rinominato 2026-11-04): il nome GRAPH è riservato alla TEORIA DEI GRAFI
// (pacchetto FFI gmbgraph / graph.inc).
//
// REGOLA DI FRONTIERA (2026-11-06): qui sta solo l'ACCESSO AI DATI del cervello
// (query wiki/rag, ispezione, export, navigazione). La MATEMATICA dei grafi NON si duplica
// qui: vive una volta sola nei pacchetti FFI. Mappa delle 17 funzioni ritirate:
//
//   BRAINS.Betweenness            -> GRAPH.BETWEENNESS
//   BRAINS.Bfs                    -> GRAPH.BFS
//   BRAINS.Bipartite              -> GRAPH.ISBIPARTITE
//   BRAINS.Clustering             -> GRAPH.CLUSTERING
//   BRAINS.Connected              -> GRAPH.COMPONENTS
//   BRAINS.Diameter               -> GRAPH.DIAMETER
//   BRAINS.Dijkstra               -> GRAPH.DIJKSTRA
//   BRAINS.Dfs                    -> GRAPH.DFS
//   BRAINS.Eccentricity           -> GRAPH.ECCENTRICITY
//   BRAINS.HasCycle               -> GRAPH.HASCYCLE
//   BRAINS.Louvain                -> GRAPH.LOUVAIN
//   BRAINS.MaxFlow                -> FLOW.MAXFLOW
//   BRAINS.Mst                    -> GRAPH.MSTKRUSKAL
//   BRAINS.PageRank               -> GRAPH.PAGERANK
//   BRAINS.StronglyConnected      -> GRAPH.SCC
//   BRAINS.Subgraph               -> GRAPH.SUBGRAPH
//   BRAINS.TopoSort               -> GRAPH.TOPOSORT
//
// Le funzioni GRAPH/FLOW lavorano su matrici esplicite (adiacenza/pesi, n dichiarato):
// il grafo del cervello va materializzato dal chiamante (nodi + matrice). In cambio una sola
// implementazione per algoritmo, in C, usabile da tutti e 4 i backend.
// I comandi legacy (BRAINS BUILD/UPDATE/FIND PATH/RDD) restano in REPL/CLI (non BASIC) —
// questo file registra solo le FUNZIONI qualificate.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Brains — namespace 'BRAINS'
 * 8 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (8):
 *   ByFolder
 *   ByTag
 *   Dot
 *   IsValid
 *   Neighbors
 *   Path
 *   QueryMulti
 *   Stats
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateBrainsNamespace()
    {
        const string namespaceName = "BRAINS";
        const SymbolFlags pub = SymbolFlags.IsPublic | SymbolFlags.IsFunction;

        var procedures = new ProcedureDefinition[]
        {
            // ── query sul contenuto del cervello (wiki/rag) ──────────────────────────────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphQueryMulti, "QueryMulti"),
                [P("query", false), P("brains", true)], new StringType(),
                TokenKind.GraphQueryMulti, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphByTag, "ByTag"),
                [P("brain", false), P("tag", false)], new StringType(),
                TokenKind.GraphByTag, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphByFolder, "ByFolder"),
                [P("brain", false), P("folder", false)], new StringType(),
                TokenKind.GraphByFolder, ExecutionKind: NativeExecutionKind.Pure),
            // ── accesso alla struttura (nessun algoritmo: sola lettura del grafo) ───────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphNeighbors, "Neighbors"),
                [P("brain", false), P("node", false)], new StringType(),
                TokenKind.GraphNeighbors, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphPath, "Path"),
                [P("brain", false), P("from", false), P("to", false)], new StringType(),
                TokenKind.GraphPath, ExecutionKind: NativeExecutionKind.Pure),
            // ── ispezione / validazione / export ────────────────────────────────────────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphStats, "Stats"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphIsValid, "IsValid"),
                [P("brain", false)], new DoubleType(),
                TokenKind.GraphIsValid, ExecutionKind: NativeExecutionKind.Pure),
            // Export estesi (F7 P1).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphDot, "Dot"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphDot, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
