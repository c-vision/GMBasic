// StdLib/StdLib.LLM.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: LLM — namespace 'LLM'
 * 61 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (61):
 *   Arch
 *   BenchResult
 *   CacheStats
 *   Callmap
 *   CheckRev
 *   Commit
 *   Context
 *   DatasetStats
 *   DecodeStats
 *   Detokenize
 *   DistillCkpt
 *   DistillDone
 *   DistillEvalAccept
 *   DistillEvalPass
 *   DistillEvalTok
 *   DistillHold
 *   DistillLoss
 *   DistillSampleDone
 *   DistillSampleFail
 *   DistillSampleN
 *   DistillStatus
 *   EvalGate
 *   FileOutline
 *   FindSymbol
 *   FinetuneBaseHold
 *   FinetuneBaseTrain
 *   FinetuneCkpt
 *   FinetuneDemoChanged
 *   FinetuneDone
 *   FinetuneGreedy
 *   FinetuneGreedyAfter
 *   FinetuneHold
 *   FinetuneLoss
 *   FinetuneStatus
 *   Impact
 *   Inspect
 *   Kernel
 *   Kv
 *   LoraList
 *   LoraLoad
 *   LoraSave
 *   Module
 *   Patch
 *   Policy
 *   PrefillStats
 *   RepoMap
 *   Revision
 *   RunStop
 *   RunsClear
 *   RunsCount
 *   RunsItem
 *   RunsJson
 *   StreamStats
 *   Symbol
 *   Tensor
 *   TokenCount
 *   Tokens
 *   TrainConfig
 *   TrainConfigSet
 *   TrainExport
 *   Verify
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

/* Namespace LLM (hard move AI->LLM, 2026-09-19) — layer motore/modello/compiler/serving.
 * STAGING: le funzioni sono COPIE di quelle di AI (stesso TokenKind → stesso handler), così
 * `LLM.X(...)` e `AI.X(...)` COESISTONO e la build non si rompe. Dopo la migrazione dei sample/
 * minitest si rimuoveranno le copie da StdLib.AI.cs. Vedi Todo/(open) - Namespace-LLM-Hard-Move.md.
 * Comandi (MODEL/FINETUNE/LORA/BENCHMARK/PREFILL/DECODE/CACHE/SERVER): il namespace LLM è già
 * accettato dal parser (C# e C) — non sono definiti nella StdLib. */
public sealed partial class StdLib
{
    private void CreateLlmNamespace()
    {
        const string ns = "LLM";
        var pub = SymbolFlags.IsPublic;

        var procedures = new ProcedureDefinition[]
        {
            // Statistiche motore/serving (KV/stream/prefill/decode/bench).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCacheStats, "CacheStats"), [P("agent", false)], new StringType(), TokenKind.AiCacheStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiStreamStats, "StreamStats"), [P("model", false)], new StringType(), TokenKind.AiStreamStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPrefillStats, "PrefillStats"), [P("agent", false)], new StringType(), TokenKind.AiPrefillStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDecodeStats, "DecodeStats"), [P("agent", false)], new StringType(), TokenKind.AiDecodeStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBenchResult, "BenchResult"), [P("agent", false)], new StringType(), TokenKind.AiBenchResult, ExecutionKind: NativeExecutionKind.Pure),
            // Fine-tune LoRA su modelli MLX locali (letture di stato).
            // Stream D: AI DISTILL (fase D1) — stato del BUILD.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillStatus, "DistillStatus"), [], new StringType(), TokenKind.AiDistillStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillSampleN, "DistillSampleN"), [], new DoubleType(), TokenKind.AiDistillSampleN, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillSampleFail, "DistillSampleFail"), [], new DoubleType(), TokenKind.AiDistillSampleFail, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillSampleDone, "DistillSampleDone"), [], new DoubleType(), TokenKind.AiDistillSampleDone, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillEvalTok, "DistillEvalTok"), [P("idx", false)], new DoubleType(), TokenKind.AiDistillEvalTok, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillEvalPass, "DistillEvalPass"), [P("idx", false)], new DoubleType(), TokenKind.AiDistillEvalPass, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillEvalAcc, "DistillEvalAccept"), [P("idx", false)], new DoubleType(), TokenKind.AiDistillEvalAcc, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillLoss, "DistillLoss"), [], new DoubleType(), TokenKind.AiDistillLoss, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillHold, "DistillHold"), [], new DoubleType(), TokenKind.AiDistillHold, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillCkpt, "DistillCkpt"), [], new StringType(), TokenKind.AiDistillCkpt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDistillDone, "DistillDone"), [], new DoubleType(), TokenKind.AiDistillDone, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneStatus, "FinetuneStatus"), [], new StringType(), TokenKind.AiFinetuneStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneLoss, "FinetuneLoss"), [], new DoubleType(), TokenKind.AiFinetuneLoss, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneHold, "FinetuneHold"), [], new DoubleType(), TokenKind.AiFinetuneHold, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneCkpt, "FinetuneCkpt"), [], new StringType(), TokenKind.AiFinetuneCkpt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneDone, "FinetuneDone"), [], new DoubleType(), TokenKind.AiFinetuneDone, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneGreedy, "FinetuneGreedy"), [], new DoubleType(), TokenKind.AiFinetuneGreedy, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneGreedyAfter, "FinetuneGreedyAfter"), [], new DoubleType(), TokenKind.AiFinetuneGreedyAfter, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneDemoChanged, "FinetuneDemoChanged"), [], new DoubleType(), TokenKind.AiFinetuneDemoChanged, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneBaseTrain, "FinetuneBaseTrain"), [], new DoubleType(), TokenKind.AiFinetuneBaseTrain, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFinetuneBaseHold, "FinetuneBaseHold"), [], new DoubleType(), TokenKind.AiFinetuneBaseHold, ExecutionKind: NativeExecutionKind.Pure),
            // Registro run training/apply (superficie TRAIN RUNS, F1): query del job registry.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmRunsJson, "RunsJson"), [], new StringType(), TokenKind.LlmRunsJson, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmRunsCount, "RunsCount"), [], new LongType(), TokenKind.LlmRunsCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmRunsClear, "RunsClear"), [], new LongType(), TokenKind.LlmRunsClear, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTrainConfig, "TrainConfig"), [], new StringType(), TokenKind.LlmTrainConfig, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTrainConfigSet, "TrainConfigSet"), [P("key", false), P("value", false)], new StringType(), TokenKind.LlmTrainConfigSet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmRunsItem, "RunsItem"), [P("id", false)], new StringType(), TokenKind.LlmRunsItem, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmDatasetStats, "DatasetStats"), [P("path", false)], new StringType(), TokenKind.LlmDatasetStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmLoraList, "LoraList"), [], new StringType(), TokenKind.LlmLoraList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmLoraSave, "LoraSave"), [P("ckpt", false), P("out", false)], new LongType(), TokenKind.LlmLoraSave, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmLoraLoad, "LoraLoad"), [P("adapter", false), P("out", false)], new LongType(), TokenKind.LlmLoraLoad, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTrainExport, "TrainExport"), [P("out", false), P("run", true)], new LongType(), TokenKind.LlmTrainExport, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmRunStop, "RunStop"), [P("id", false)], new LongType(), TokenKind.LlmRunStop, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmEvalGate, "EvalGate"), [P("ref", false), P("cand", false)], new StringType(), TokenKind.LlmEvalGate, ExecutionKind: NativeExecutionKind.Pure),
            // §4.10 SEMAPRAX — GMB Semantic Graph (lato PROGRAMMA), spostato da AI → LLM.
            // Funzioni (ritornano valori), risolte a COMPILE/LINK-time dal grafo del programma
            // (C# common/SemanticGraph; C gmb_semgraph) -> costante stringa.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSymbol, "Symbol"), [P("name", false)], new StringType(), TokenKind.AiSymbol, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImpact, "Impact"), [P("name", false), P("depth", true)], new StringType(), TokenKind.AiImpact, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCallmap, "Callmap"), [P("name", false)], new StringType(), TokenKind.AiCallmap, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRevision, "Revision"), [], new StringType(), TokenKind.AiRevision, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContext, "Context"), [P("name", false), P("depth", true)], new StringType(), TokenKind.AiContext, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCheckrev, "CheckRev"), [P("revision", false)], new StringType(), TokenKind.AiCheckrev, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPatch, "Patch"), [P("target", false)], new StringType(), TokenKind.AiPatch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFindsymbol, "FindSymbol"), [P("prefix", false)], new StringType(), TokenKind.AiFindsymbol, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRepomap, "RepoMap"), [], new StringType(), TokenKind.AiRepomap, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFileoutline, "FileOutline"), [P("file", false)], new StringType(), TokenKind.AiFileoutline, ExecutionKind: NativeExecutionKind.Pure),
            // §4.10 write-side lifecycle (read-only receipts): VERIFY/COMMIT del piano patch.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmVerify, "Verify"), [], new StringType(), TokenKind.LlmVerify, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmCommit, "Commit"), [], new StringType(), TokenKind.LlmCommit, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmInspect, "Inspect"), [P("name", false)], new StringType(), TokenKind.LlmInspect, ExecutionKind: NativeExecutionKind.Pure),
            // B2 — LLM.TOKENCOUNT(model, testo): numero di token del testo (superficie tokenizer).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTokenCount, "TokenCount"), [P("model", false), P("text", false)], new DoubleType(), TokenKind.LlmTokenCount, ExecutionKind: NativeExecutionKind.Pure),
            // B2 (2026-10-30) — i verbi del tokenizer come FUNZIONI (il comando stampa, la funzione
            // restituisce il testo): `ids$ = LLM.TOKENS(m, t)`, `txt$ = LLM.DETOKENIZE(m, ids)`.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTokens, "Tokens"), [P("model", false), P("text", false)], new StringType(), TokenKind.LlmTokens, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmDetokenize, "Detokenize"), [P("model", false), P("ids", false)], new StringType(), TokenKind.LlmDetokenize, ExecutionKind: NativeExecutionKind.Pure),
            // Versione FUNZIONE di qualunque sotto-verbo `LLM TENSOR <sub>`: restituisce l'output.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmTensorValue, "Tensor"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmTensorValue, ExecutionKind: NativeExecutionKind.Pure),
            // Versioni-FUNZIONE delle altre famiglie (il comando omonimo stampa: qui si restituisce).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmPolicyOut, "Policy"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmPolicyOut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmArchOut, "Arch"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmArchOut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmModuleOut, "Module"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmModuleOut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmKernelOut, "Kernel"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmKernelOut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LlmKvOut, "Kv"), [P("sub", false), P("a", true), P("b", true), P("c", true)], new StringType(), TokenKind.LlmKvOut, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(ns, procedures, new Dictionary<string, DataType>());
    }
}
