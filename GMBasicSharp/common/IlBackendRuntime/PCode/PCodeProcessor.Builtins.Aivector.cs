// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>AIVECTOR: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AIVECTORBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIVECTOREMBED,
        BuiltInFunction.AIVECTORENSURECOLLECTION,
        BuiltInFunction.AIVECTORINDEXTEXT,
        BuiltInFunction.AIVECTORINDEXMETA,
        BuiltInFunction.AIVECTORREMOVESOURCE,
        BuiltInFunction.AIVECTORPURGEDEPRECATED,
        BuiltInFunction.AIVECTORCOUNT,
        BuiltInFunction.AIVECTORSEARCH,
        BuiltInFunction.AIVECTORRAGCONTEXT,
        BuiltInFunction.AIVECTORASK,
        BuiltInFunction.AIVECTORLOADDOCUMENTS,
        BuiltInFunction.AIVECTORLISTCOLLECTIONS,
        BuiltInFunction.AIVECTORRERANK,
        // GenTest (2026-09-15)
        BuiltInFunction.AIVECTORGENTEST,
        // RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione (R2).
        BuiltInFunction.VECTORDBPRECISIONAT,
        BuiltInFunction.VECTORDBRECALLAT,
        BuiltInFunction.VECTORDBMRR,
        BuiltInFunction.VECTORDBNDCGAT,
        BuiltInFunction.VECTORDBFAITHFULNESS,
        BuiltInFunction.VECTORDBRELEVANCE,
        BuiltInFunction.VECTORDBHALLUCINATION,
        BuiltInFunction.VECTORDBCOMPLETENESS,
        // RAG eval avanzato (2026-09-15): diagnostica/calibrazione/drift/A-B test.
        BuiltInFunction.VECTORDBDIAGNOSE,
        BuiltInFunction.VECTORDBDIAGNOSEREPORT,
        BuiltInFunction.VECTORDBCALIBRATE,
        BuiltInFunction.VECTORDBEVALGATE,
        BuiltInFunction.VECTORDBEVALDIFF,
        BuiltInFunction.VECTORDBBASELINE,
        BuiltInFunction.VECTORDBDRIFT,
        BuiltInFunction.VECTORDBABTEST,
        BuiltInFunction.VECTORDBCOMPARE,
        // Self-Consistency / Semantic Entropy (2026-09-14): i 3 func_code (3538-3540) avevano
        // l'handler in AIVECTORDispatch ma NON erano nel set di routing → su PCode il dispatch
        // cadeva nel default "builtin non implementato" e il programma falliva (IL invece
        // girava: divergenza reale, trovata dal MiniTest mcp_uncertainty).
        BuiltInFunction.AICONSISTENCY,
        BuiltInFunction.AILEXICALENTROPY,
        BuiltInFunction.AICONSISTENCYDETAIL,
    };

    /// <summary>Aivector: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aivectorDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AIVECTORDispatch
        => _aivectorDispatch ??= BuildAivectorDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAivectorDispatch() => new()
    {
        [BuiltInFunction.AIVECTOREMBED] = (task, args) =>
        {
                                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                                return Basic.IlBackend.IlRuntime.AiVectorEmbed(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions);
        },
        [BuiltInFunction.AIVECTORENSURECOLLECTION] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorEnsureCollection(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 0,
                                    args.Length > 3 ? ArgOr(args, 3)?.ToString() : null);
        },
        [BuiltInFunction.AIVECTORINDEXTEXT] = (task, args) =>
        {
                                return (double)Basic.IlBackend.IlRuntime.AiVectorIndexText(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "",
                                    ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVECTORINDEXMETA] = (task, args) =>
        {
                                return (double)Basic.IlBackend.IlRuntime.AiVectorIndexMeta(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "",
                                    ArgOr(args, 3)?.ToString() ?? "",
                                    args.Length > 4 ? ArgOr(args, 4)?.ToString() : null);
        },
        [BuiltInFunction.AIVECTORREMOVESOURCE] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorRemoveSource(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVECTORPURGEDEPRECATED] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorPurgeDeprecated(
                                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVECTORCOUNT] = (task, args) =>
        {
                                return (double)Basic.IlBackend.IlRuntime.AiVectorCountEmbeddings(
                                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVECTORSEARCH] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorSearch(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "",
                                    args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : 5,
                                    args.Length <= 4 ? 1 : SafeDouble(ArgOr(args, 4)),
                                    args.Length > 5 ? ArgOr(args, 5)?.ToString() : null,
                                    args.Length > 6 ? SafeDouble(ArgOr(args, 6)) : 1,
                                    args.Length > 7 ? SafeDouble(ArgOr(args, 7)) : double.NaN,
                                    args.Length > 8 ? SafeDouble(ArgOr(args, 8)) : 0);
        },
        [BuiltInFunction.AIVECTORRAGCONTEXT] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorRagContext(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "",
                                    args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : 5,
                                    args.Length > 4 ? SafeDouble(ArgOr(args, 4)) : 1,
                                    args.Length > 5 ? SafeDouble(ArgOr(args, 5)) : double.NaN);
        },
        [BuiltInFunction.AIVECTORASK] = (task, args) =>
        {
                                // RAG query (embedding + retrieval, potenzialmente lenta) → blocking pool.
                            {
                                var db = ArgOr(args, 0)?.ToString() ?? "";
                                var coll = ArgOr(args, 1)?.ToString() ?? "";
                                var question = ArgOr(args, 2)?.ToString() ?? "";
                                var topK = args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : 5;
                                var reranker = args.Length > 4 ? SafeDouble(ArgOr(args, 4)) : 1;
                                var minEv = args.Length > 5 ? SafeDouble(ArgOr(args, 5)) : double.NaN;
                                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiVectorAsk(db, coll, question, topK, reranker, minEv),
                                    result => task.PushRef(result));
                                return ParkedBuiltinResult;
                            }
        },
        [BuiltInFunction.AIVECTORLOADDOCUMENTS] = (task, args) =>
        {
                                // Load + chunking + embedding di documenti (molto lento) → blocking pool.
                            {
                                var db = ArgOr(args, 0)?.ToString() ?? "";
                                var coll = ArgOr(args, 1)?.ToString() ?? "";
                                var path = ArgOr(args, 2)?.ToString() ?? "";
                                var glob = args.Length > 3 ? ArgOr(args, 3)?.ToString() : null;
                                var loader = args.Length > 4 ? ArgOr(args, 4)?.ToString() : null;
                                ParkBlocking(task, () => (object?)(double)Basic.IlBackend.IlRuntime.AiVectorLoadDocuments(db, coll, path, glob, loader),
                                    result => task.PushRef(result));
                                return ParkedBuiltinResult;
                            }
        },
        [BuiltInFunction.AIVECTORLISTCOLLECTIONS] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorListCollections(
                                    ArgOr(args, 0)?.ToString() ?? "");
        },
[BuiltInFunction.AIVECTORRERANK] = (task, args) =>
        {
                                // Runtime comune (IlRuntime): il PCodeVM non duplica la logica di rerank —
                                // stessa implementazione del backend IL (come VM/C e C99 condividono gmb_runtime_c).
                                return Basic.IlBackend.IlRuntime.AiVectorRerank(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 5);

                            // === AI PLUGINS (ciclo di vita + dispatch capacità) — delegato a IlRuntime ===
        },
        [BuiltInFunction.AIVECTORGENTEST] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.AiVectorGenTest(
                                    ArgOr(args, 0)?.ToString() ?? "",
                                    ArgOr(args, 1)?.ToString() ?? "",
                                    ArgOr(args, 2)?.ToString() ?? "");
        },
        // RAG eval (2026-09-14): metriche retrieval (R1) + giudizi generazione (R2) — pure math,
        // nessun park. Canone = stesse formule di IlRuntime (delega).
        [BuiltInFunction.VECTORDBPRECISIONAT] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbPrecisionAt(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 0),
        [BuiltInFunction.VECTORDBRECALLAT] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbRecallAt(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 0),
        [BuiltInFunction.VECTORDBMRR] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbMrr(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBNDCGAT] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbNdcgAt(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 0),
        [BuiltInFunction.VECTORDBFAITHFULNESS] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbFaithfulness(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBRELEVANCE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbRelevance(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBHALLUCINATION] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbHallucination(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBCOMPLETENESS] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbCompleteness(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        // RAG eval avanzato (2026-09-15): diagnostica/calibrazione/drift/A-B test.
        [BuiltInFunction.VECTORDBDIAGNOSE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbDiagResultJson(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBDIAGNOSEREPORT] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbDiagReport(),
        [BuiltInFunction.VECTORDBCALIBRATE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbCalibrate(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBEVALGATE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbEvalGate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBEVALDIFF] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbEvalDiff(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBBASELINE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbBaseline(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBDRIFT] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbDrift(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBABTEST] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbAbTest(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.VECTORDBCOMPARE] = (task, args) =>
            Basic.IlBackend.IlRuntime.VectorDbCompare(ArgOr(args, 0)?.ToString() ?? ""),
        // Self-Consistency / Semantic Entropy (2026-09-14) — AI namespace
        [BuiltInFunction.AICONSISTENCY] = (task, args) => {
            string prompt = ArgOr(args, 0)?.ToString() ?? "";
            object? options = args.Length > 1 ? args[1] : null;
            return Basic.IlBackend.IlRuntime.AiConsistency(prompt, options);
        },
        [BuiltInFunction.AILEXICALENTROPY] = (task, args) => {
            // Una stringa delimitata: lo split è nel runtime (C) / SemanticEntropy (C#),
            // così il comportamento è identico fra i due mondi (prima splittava qui e
            // passava un array a un metodo con firma string → divergenza dal C).
            string responses = ArgOr(args, 0)?.ToString() ?? "";
            return Basic.IlBackend.IlRuntime.AiLexicalEntropy(responses);
        },
        [BuiltInFunction.AICONSISTENCYDETAIL] = (task, args) => {
            return Basic.IlBackend.IlRuntime.AiConsistencyDetail();
        },
    };

    private object? ExecuteBuiltInAivector(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AIVECTORDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAivector (fn={fn}).");

}
