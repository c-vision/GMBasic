// FILE: Interpreter.StdLib.Brains.cs
// Namespace BRAINS — funzioni qualificate per i grafi dei knowledge base (brain).
// Ex namespace GRAPH (rinominato 2026-11-04): il nome GRAPH è riservato alla TEORIA DEI GRAFI
// (pacchetto FFI gmbgraph / graph.inc). Le funzioni wiki-only (ByTag/ByFolder/QueryMulti)
// restano qui: interrogano il grafo di un cervello, non sono funzioni AI.
// I comandi legacy (BRAINS BUILD/UPDATE/STATUS) restano in REPL/CLI (non BASIC) —
// questo file registra solo le FUNZIONI qualificate.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Brains — namespace 'BRAINS'
 * 25 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (25):
 *   Betweenness
 *   Bfs
 *   Bipartite
 *   ByFolder
 *   ByTag
 *   Clustering
 *   Connected
 *   Dfs
 *   Diameter
 *   Dijkstra
 *   Dot
 *   Eccentricity
 *   HasCycle
 *   IsValid
 *   Louvain
 *   MaxFlow
 *   Mst
 *   Neighbors
 *   PageRank
 *   Path
 *   QueryMulti
 *   Stats
 *   StronglyConnected
 *   Subgraph
 *   TopoSort
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
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphQueryMulti, "QueryMulti"),
                [P("query", false), P("brains", true)], new StringType(),
                TokenKind.GraphQueryMulti, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphNeighbors, "Neighbors"),
                [P("brain", false), P("node", false)], new StringType(),
                TokenKind.GraphNeighbors, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphPath, "Path"),
                [P("brain", false), P("from", false), P("to", false)], new StringType(),
                TokenKind.GraphPath, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphPageRank, "PageRank"),
                [P("brain", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new IntegerType(), SymbolFlags.IsOptional, null, false)], new StringType(),
                TokenKind.GraphPageRank, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphConnected, "Connected"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphConnected, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphStats, "Stats"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphStronglyConnected, "StronglyConnected"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphStronglyConnected, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphTopoSort, "TopoSort"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphTopoSort, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphBipartite, "Bipartite"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphBipartite, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphBetweenness, "Betweenness"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphBetweenness, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphClustering, "Clustering"),
                [P("brain", false), P("node", false)], new StringType(),
                TokenKind.GraphClustering, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphDiameter, "Diameter"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphDiameter, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphMst, "Mst"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphMst, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphLouvain, "Louvain"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphLouvain, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphMaxFlow, "MaxFlow"),
                [P("brain", false), P("from", false), P("to", false)], new StringType(),
                TokenKind.GraphMaxFlow, ExecutionKind: NativeExecutionKind.Pure),
            // Algoritmi avanzati (2026-09-07): delegano ai motori dei due mondi (CommonLib C# per
            // IL/PCode; gmb_graph.c per VM/C/C99Native) — mai stub.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphDijkstra, "Dijkstra"),
                [P("brain", false), P("from", false), P("to", false)], new StringType(),
                TokenKind.GraphDijkstra, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphHasCycle, "HasCycle"),
                [P("brain", false)], new DoubleType(),
                TokenKind.GraphHasCycle, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphEccentricity, "Eccentricity"),
                [P("brain", false), P("node", false)], new StringType(),
                TokenKind.GraphEccentricity, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphDfs, "Dfs"),
                [P("brain", false), P("root", false)], new StringType(),
                TokenKind.GraphDfs, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphBfs, "Bfs"),
                [P("brain", false), P("root", false)], new StringType(),
                TokenKind.GraphBfs, ExecutionKind: NativeExecutionKind.Pure),
            // Export estesi (F7 P1): dot / subgraph / isValid.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphDot, "Dot"),
                [P("brain", false)], new StringType(),
                TokenKind.GraphDot, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphSubgraph, "Subgraph"),
                [P("brain", false), P("root", false), new Parameter(-1, T(TokenKind.Identifier, "depth"), new IntegerType(), SymbolFlags.IsOptional, null, false)], new StringType(),
                TokenKind.GraphSubgraph, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphByTag, "ByTag"),
                [P("brain", false), P("tag", false)], new StringType(),
                TokenKind.GraphByTag, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphByFolder, "ByFolder"),
                [P("brain", false), P("folder", false)], new StringType(),
                TokenKind.GraphByFolder, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GraphIsValid, "IsValid"),
                [P("brain", false)], new DoubleType(),
                TokenKind.GraphIsValid, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
