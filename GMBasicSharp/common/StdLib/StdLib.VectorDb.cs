// StdLib/StdLib.VectorDb.cs
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: VectorDb — namespace 'VECTORDB'
 * 28 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (28):
 *   AbTest
 *   Baseline
 *   Calibrate
 *   Compare
 *   Completeness
 *   Count
 *   Diagnose
 *   DiagnoseReport
 *   Drift
 *   Embed
 *   EnsureCollection
 *   EvalDiff
 *   EvalGate
 *   Faithfulness
 *   GenTest
 *   Hallucination
 *   IndexMeta
 *   IndexText
 *   ListCollections
 *   LoadDocuments
 *   Mrr
 *   NdcgAt
 *   PrecisionAt
 *   PurgeDeprecated
 *   RecallAt
 *   Relevance
 *   RemoveSource
 *   Search
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Namespace VECTORDB — database vettoriale autonoma su SQLite.
/// Modulo DATABASE a sé stante (come DBF/ISAM/SPEED/SQL): gli AI lo USANO,
/// non lo sostituiscono. Funzioni qualificate con parentesi; i comandi sono
/// legacy a spazio (Parser.Stmt.VectorDb.cs).
/// </summary>
public sealed partial class StdLib
{
    private void CreateVectorDbNamespace()
    {
        const string ns = "VECTORDB";
        var pub = SymbolFlags.IsPublic;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorEmbed, "Embed"), [P("text", false), P("dimensions", true)], new ArrayType(new SingleType()), TokenKind.AiVectorEmbed, ExecutionKind: NativeExecutionKind.Pure),
            // EnsureCollection/RemoveSource ritornano DOUBLE sui 4 backend (-1.0 = errore):
            // erano dichiarati LongType/IntegerType, quindi un `DIM e = VECTORDB.ENSURECOLLECTION(...)`
            // creava un locale INTEGER e lo stloc di un double in int64 produceva valori
            // spazzatura nel percorso IL NON ottimizzato (-4.6e18 invece di -1: ILOptim mascherava
            // il difetto). Firme runtime: IlRuntime.AiVectorEnsureCollection → double,
            // IlRuntime.AiVectorRemoveSource → double; PCode/VM/C/C99 identici.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorEnsureCollection, "EnsureCollection"), [P("db", false), P("coll", false), P("dims", true), P("format", true)], new DoubleType(), TokenKind.AiVectorEnsureCollection, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorIndexText, "IndexText"), [P("db", false), P("coll", false), P("docId", false), P("text", false)], new LongType(), TokenKind.AiVectorIndexText, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorIndexMeta, "IndexMeta"), [P("db", false), P("coll", false), P("docId", false), P("text", false), P("meta", true)], new LongType(), TokenKind.AiVectorIndexMeta, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorRemoveSource, "RemoveSource"), [P("db", false), P("coll", false), P("sourceId", false)], new DoubleType(), TokenKind.AiVectorRemoveSource, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorPurgeDeprecated, "PurgeDeprecated"), [P("db", false), P("coll", false)], new DoubleType(), TokenKind.AiVectorPurgeDeprecated, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorCountEmbeddings, "Count"), [P("db", false), P("coll", false)], new LongType(), TokenKind.AiVectorCountEmbeddings, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorSearch, "Search"), [P("db", false), P("coll", false), P("query", false), P("topK", false), P("hybrid", true), P("filter", true), P("reranker", true), P("minEvidence", true), P("explain", true)], new StringType(), TokenKind.AiVectorSearch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorListCollections, "ListCollections"), [P("db", false)], new StringType(), TokenKind.AiVectorListCollections, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorLoadDocuments, "LoadDocuments"), [P("db", false), P("coll", false), P("path", false), P("pattern", true), P("loader", true)], new IntegerType(), TokenKind.AiVectorLoadDocuments, ExecutionKind: NativeExecutionKind.Pure),
            // RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione tier-1 (R2).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbPrecisionAt, "PrecisionAt"), [P("retrieved", false), P("ideal", false), P("k", false)], new DoubleType(), TokenKind.VectorDbPrecisionAt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbRecallAt, "RecallAt"), [P("retrieved", false), P("ideal", false), P("k", false)], new DoubleType(), TokenKind.VectorDbRecallAt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbMrr, "Mrr"), [P("retrieved", false), P("ideal", false)], new DoubleType(), TokenKind.VectorDbMrr, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbNdcgAt, "NdcgAt"), [P("retrieved", false), P("ideal", false), P("k", false)], new DoubleType(), TokenKind.VectorDbNdcgAt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbFaithfulness, "Faithfulness"), [P("context", false), P("answer", false)], new DoubleType(), TokenKind.VectorDbFaithfulness, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbRelevance, "Relevance"), [P("question", false), P("answer", false)], new DoubleType(), TokenKind.VectorDbRelevance, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbHallucination, "Hallucination"), [P("context", false), P("answer", false)], new DoubleType(), TokenKind.VectorDbHallucination, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbCompleteness, "Completeness"), [P("question", false), P("answer", false)], new DoubleType(), TokenKind.VectorDbCompleteness, ExecutionKind: NativeExecutionKind.Pure),
            // GenTest (2026-09-15): genera testset sintetici da collection vettoriale (R3).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorGenTest, "GenTest"), [P("collection", false), P("n", false), P("types", false)], new StringType(), TokenKind.AiVectorGenTest, ExecutionKind: NativeExecutionKind.Pure),
            // RAG eval avanzato (2026-09-15): 9 funzioni diagnostica/calibrazione/drift/A-B test.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbDiagnose, "Diagnose"), [P("result", false)], new StringType(), TokenKind.VectorDbDiagnose, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbDiagnoseReport, "DiagnoseReport"), [], new StringType(), TokenKind.VectorDbDiagnoseReport, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbCalibrate, "Calibrate"), [P("cases", false)], new StringType(), TokenKind.VectorDbCalibrate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbEvalGate, "EvalGate"), [P("baseline", false), P("current", false)], new StringType(), TokenKind.VectorDbEvalGate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbEvalDiff, "EvalDiff"), [P("baseline", false), P("current", false)], new StringType(), TokenKind.VectorDbEvalDiff, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbBaseline, "Baseline"), [P("sample", false)], new StringType(), TokenKind.VectorDbBaseline, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbDrift, "Drift"), [P("baseline", false), P("window", false)], new DoubleType(), TokenKind.VectorDbDrift, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbAbTest, "AbTest"), [P("variantA", false), P("variantB", false)], new StringType(), TokenKind.VectorDbAbTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VectorDbCompare, "Compare"), [P("configs", false)], new StringType(), TokenKind.VectorDbCompare, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, new Dictionary<string, DataType>());
    }
}
