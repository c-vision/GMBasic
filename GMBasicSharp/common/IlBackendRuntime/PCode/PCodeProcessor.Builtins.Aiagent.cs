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
    /// <summary>AIAGENT: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AIAGENTBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIAGENTASK,
        BuiltInFunction.AIAGENTCREATE,
        BuiltInFunction.AIAGENTDELETE,
        BuiltInFunction.AIAGENTEVENTS,
        BuiltInFunction.AIAGENTGET,
        BuiltInFunction.AIAGENTLIST,
        BuiltInFunction.AIAGENTLOAD,
        BuiltInFunction.AIAGENTREGISTERTOOL,
        BuiltInFunction.AIAGENTSAVE,
        BuiltInFunction.AIAGENTSERVE,
        BuiltInFunction.AIAGENTSEND,
        BuiltInFunction.AIAGENTRECEIVE,
        BuiltInFunction.AIAGENTPAUSE,
        BuiltInFunction.AIAGENTRESUME,
        BuiltInFunction.AIAGENTSTOP,
        BuiltInFunction.AIAGENTQUEUE_STATS,
        BuiltInFunction.AIAGENTQUEUE_CLEAR,
        BuiltInFunction.AICHANNEL_CREATE,
        BuiltInFunction.AICHANNEL_PUBLISH,
        BuiltInFunction.AICHANNEL_SUBSCRIBE,
        BuiltInFunction.AICHANNEL_UNSUBSCRIBE,
        BuiltInFunction.AIAGENTCODE,
        BuiltInFunction.AIAGENTFIX,
        BuiltInFunction.AIKGADDNODE,
        BuiltInFunction.AIKGADDEDGE,
        BuiltInFunction.AIKGQUERY,
        BuiltInFunction.AIAGENTSET,
        BuiltInFunction.AIAGENTSTATUS,
        BuiltInFunction.AIAGENTSTOPSERVER,
        BuiltInFunction.AIAGENTTRACE,
        BuiltInFunction.AICACHESTATS,
        BuiltInFunction.AIPREFILL,
        BuiltInFunction.AIDECODE,
        BuiltInFunction.AIPREFILLSTATS,
        BuiltInFunction.AIDECODESTATS,
        BuiltInFunction.AICACHECLEAR,
        BuiltInFunction.AICACHETRIM,
        BuiltInFunction.AIBENCHMARK,
        BuiltInFunction.AIBENCHRESULT,
        BuiltInFunction.AIFINETUNE,
        BuiltInFunction.AIDISTILLBUILD,
        BuiltInFunction.AIDISTILLSTATUS,
        BuiltInFunction.AIDISTILLSAMPLE,
        BuiltInFunction.AIDISTILLSAMPLEN,
        BuiltInFunction.AIDISTILLSAMPLEFAIL,
        BuiltInFunction.AIDISTILLSAMPLEDONE,
        BuiltInFunction.AIDISTILLEVALACC,
        BuiltInFunction.AIDISTILLEVAL,
        BuiltInFunction.AIDISTILLEVALTOK,
        BuiltInFunction.AIDISTILLEVALPASS,
        BuiltInFunction.AIDISTILLAPPLY,
        BuiltInFunction.AIDISTILLRESTORE,
        BuiltInFunction.AIDISTILLTRAIN,
        BuiltInFunction.AIDISTILLLOSS,
        BuiltInFunction.AIDISTILLHOLD,
        BuiltInFunction.AIDISTILLCKPT,
        BuiltInFunction.AIDISTILLDONE,
        BuiltInFunction.AILORAAPPLY,
        BuiltInFunction.AIFINETUNESTATUS,
        BuiltInFunction.AIFINETUNELOSS,
        BuiltInFunction.AIFINETUNEHOLD,
        BuiltInFunction.AIFINETUNECKPT,
        BuiltInFunction.AIFINETUNEDONE,
        BuiltInFunction.AIFINETUNEGREEDY,
        BuiltInFunction.AIFINETUNEGREEDYAFTER,
        BuiltInFunction.AIFINETUNEDEMOCHANGED,
        BuiltInFunction.AIFINETUNEBASETRAIN,
        BuiltInFunction.AIFINETUNEBASEHOLD,
        BuiltInFunction.LLMRUNSJSON,
        BuiltInFunction.LLMRUNSCOUNT,
        BuiltInFunction.LLMRUNSCLEAR,
        BuiltInFunction.LLMTRAINCONFIG,
        BuiltInFunction.LLMTRAINCONFIGSET,
        BuiltInFunction.LLMRUNSITEM,
        BuiltInFunction.LLMDATASETSTATS,
        BuiltInFunction.LLMLORALIST,
        BuiltInFunction.LLMLORASAVE,
        BuiltInFunction.LLMLORALOAD,
        BuiltInFunction.AIDPO,
        BuiltInFunction.AIGRPO,
        BuiltInFunction.LLMTRAINEXPORT,
        BuiltInFunction.LLMRUNSTOP,
        BuiltInFunction.LLMEVALGATE,
        BuiltInFunction.AISERVERSTART,
        BuiltInFunction.AISERVERSTOP,
        BuiltInFunction.AISERVERSTATUS,
        BuiltInFunction.AISERVERMODEL,
        BuiltInFunction.AISERVERUNLOAD,
        BuiltInFunction.AIMODELLOAD,
        BuiltInFunction.AIMODELUNLOAD,
        BuiltInFunction.AIMODELCACHE,
        BuiltInFunction.AIMODELPREFETCH,
        BuiltInFunction.AIMODELDEVICE,
        BuiltInFunction.AIMODELCONVERT,
        BuiltInFunction.AIMODELPROMOTE,
        BuiltInFunction.AIMODELDEMOTE,
        BuiltInFunction.AIMODELQUANTIZE,
        BuiltInFunction.AIMODELINSPECT,
        BuiltInFunction.AIMODELREMOVE,
        BuiltInFunction.AIMODELSAVE,
        BuiltInFunction.AIMODELABLATE,
        BuiltInFunction.AIMODELDIFF,
        BuiltInFunction.AIMODELCHECKPOINT,
        BuiltInFunction.AIMODELROLLBACK,
        BuiltInFunction.AIMODELADD,
        BuiltInFunction.AIMODELADAPTER,
        BuiltInFunction.AIMODELMODULE,
        BuiltInFunction.AIMODELMODALITY,
        BuiltInFunction.AIMODELVALIDATE,
        BuiltInFunction.AIMODELTRACE,
        BuiltInFunction.AISTREAMSTATS,
        BuiltInFunction.AIAGENTASKSTREAM,
        // MODEL COMPILER LLM (2026-09-23): verbi LLM → IlRuntime.Llm* (P/Invoke al runtime C
        // condiviso, dylib gmb_ai_llm). Prima erano solo in C99Native; ora tutti i backend.
        BuiltInFunction.LLMPLAN, BuiltInFunction.LLMPOLICY, BuiltInFunction.LLMQUANT,
        BuiltInFunction.LLMMERGE, BuiltInFunction.LLMPRUNE, BuiltInFunction.LLMEXTRACT,
        BuiltInFunction.LLMPROFILE, BuiltInFunction.LLMARCH,
        // Superficie .gmbarch (2026-09-24): TENSOR/KERNEL/MODULE/KV → IlRuntime.Llm*.
        BuiltInFunction.LLMTENSOR, BuiltInFunction.LLMKERNEL,
        BuiltInFunction.LLMCOMPILE,
        BuiltInFunction.LLMQUALITY,
        BuiltInFunction.LLMMODULE, BuiltInFunction.LLMKV,
        BuiltInFunction.LLMTOKENCOUNT,
        BuiltInFunction.LLMTOKENS, BuiltInFunction.LLMDETOKENIZE, BuiltInFunction.LLMTENSORVALUE,
        BuiltInFunction.LLMPOLICYOUT, BuiltInFunction.LLMARCHOUT, BuiltInFunction.LLMMODULEOUT, BuiltInFunction.LLMKERNELOUT, BuiltInFunction.LLMKVOUT,
        BuiltInFunction.ASRTRANSCRIBE,
        // GMB MODULE — registry di moduli esterni (model compiler §4.3).
        BuiltInFunction.GMBMODULE,
    };

    /// <summary>Aiagent: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aiagentDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AIAGENTDispatch
        => _aiagentDispatch ??= BuildAiagentDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAiagentDispatch() => new()
    {
        [BuiltInFunction.AIAGENTCREATE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return Basic.IlBackend.IlRuntime.AiAgentCreate(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTDELETE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentDelete(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTASK] = (task, args) =>
        {
                // Inferenza LLM sincrona (può durare secondi) → blocking pool, mai sul worker.
                {
                var name = ArgOr(args, 0)?.ToString() ?? "";
                var prompt = ArgOr(args, 1)?.ToString() ?? "";
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiAgentAsk(name, prompt),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIAGENTSET] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentSet(
                ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTGET] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentGet(
                ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTSAVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentSave(
                ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIAGENTLOAD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentLoad(
                ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIAGENTREGISTERTOOL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentRegisterTool(
                ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTSERVE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica (HttpListener + handler /chat) —
                // unica implementazione.
                    return Basic.IlBackend.IlRuntime.AiAgentServe(SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.AIAGENTSTOPSERVER] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentStopServer();
        },
        [BuiltInFunction.AIAGENTLIST] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentList();
        },
        [BuiltInFunction.AIAGENTSTATUS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentStatus(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AICACHESTATS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiCacheStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPREFILLSTATS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPrefillStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIDECODESTATS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiDecodeStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIPREFILL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiPrefill(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        args.Length > 2 ? args[2]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIDECODE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiDecode(
                        ArgOr(args, 0)?.ToString() ?? "",
                        args.Length > 1 ? (int)SafeDouble(args[1]) : 0);
        },
        [BuiltInFunction.AICACHECLEAR] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiCacheClear(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AICACHETRIM] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiCacheTrim(
                        ArgOr(args, 0)?.ToString() ?? "",
                        SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.AISERVERSTART] = (task, args) =>
        {
                    string dir = ArgOr(args, 0)?.ToString() ?? "";
                    return args.Length >= 2
                        ? Basic.IlBackend.IlRuntime.AiServerStartPort(dir, SafeDouble(ArgOr(args, 1)))
                        : Basic.IlBackend.IlRuntime.AiServerStart(dir);
        },
        [BuiltInFunction.AISERVERSTOP] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiServerStop();
        },
        [BuiltInFunction.AISERVERSTATUS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiServerStatus();
        },
        [BuiltInFunction.AISERVERMODEL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiServerModel(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISERVERUNLOAD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiServerUnload();
        },
        [BuiltInFunction.AIMODELLOAD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelLoad(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        args.Length > 2 ? ArgOr(args, 2)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIMODELUNLOAD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelUnload(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELPROMOTE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelPromote(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELDEMOTE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelDemote(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELCACHE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelCache(
                        ArgOr(args, 0)?.ToString() ?? "", SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.AIMODELPREFETCH] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelPrefetch(
                        ArgOr(args, 0)?.ToString() ?? "", SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.AIMODELDEVICE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelDevice(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AISTREAMSTATS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiStreamStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELCONVERT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelConvert(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        ArgOr(args, 2)?.ToString() ?? "GGUF");
        },
        [BuiltInFunction.AIMODELQUANTIZE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelQuantize(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        SafeDouble(ArgOr(args, 2)),
                        SafeDouble(ArgOr(args, 3)),
                        ArgOr(args, 4)?.ToString() ?? "",
                        SafeDouble(ArgOr(args, 5)));
        },
        [BuiltInFunction.AIMODELINSPECT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelInspect(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELSAVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelSave(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELABLATE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelAblate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELREMOVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelRemove(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELDIFF] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelDiff(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELCHECKPOINT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelCheckpoint(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELROLLBACK] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelRollback(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELADD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelAdd(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELADAPTER] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelAdapter(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "1");
        },
        [BuiltInFunction.AIMODELMODULE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelModule(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELMODALITY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelModality(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELVALIDATE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelValidate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIMODELTRACE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiModelTrace(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        // MODEL COMPILER LLM (2026-09-23): delega a IlRuntime.Llm* → runtime CONDIVISO C
        // (dylib gmb_ai_llm via P/Invoke). Stessa logica di C99Native/VM-C.
        [BuiltInFunction.LLMPLAN] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmPlan(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMPOLICY] = (task, args) =>
        {
                    // Livello B (mondo gestito, PCodeVM): CALLBACK <Proc> -> la GPM PCode itera il
                    // piano (nativo) e invoca la SUB (closure) via CallClosureSync, nel task corrente.
                    if ((ArgOr(args, 0)?.ToString() ?? "") == "CALLBACK")
                    {
                        var cd = ArgOr(args, 1) as ClosureData;
                        if (cd == null) throw new InvalidOperationException("LLM POLICY CALLBACK: callback non valido");
                        int n = Basic.Providers.LlmModelNative.llmmodelio_policy_item_count();
                        var buf = new byte[1024]; int applied = 0;
                        for (int i = 0; i < n; i++)
                        {
                            System.Array.Clear(buf, 0, buf.Length);
                            if (Basic.Providers.LlmModelNative.llmmodelio_policy_item_to(i, buf, buf.Length) != 0) continue;
                            int len = System.Array.IndexOf(buf, (byte)0); if (len < 0) len = buf.Length;
                            var parts = System.Text.Encoding.UTF8.GetString(buf, 0, len).Split('\t');
                            if (parts.Length < 3) continue;
                            var fmt = CallClosureSync(task, cd, parts[0], parts[1], parts[2])?.ToString() ?? "";
                            if (fmt.Length > 0) { Basic.Providers.LlmModelNative.llmmodelio_policy_set_item(parts[0], fmt); applied++; }
                        }
                        return (double)applied;
                    }
                    return Basic.IlBackend.IlRuntime.LlmPolicy(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.GMBMODULE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.GmbModule(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMQUANT] = (task, args) =>
        {
                    if (args.Length >= 3)
                        return Basic.IlBackend.IlRuntime.LlmQuantAuto(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", SafeDouble(ArgOr(args, 2)));
                    return Basic.IlBackend.IlRuntime.LlmQuant(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMMERGE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmMerge(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMPRUNE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmPrune(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.LLMEXTRACT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmExtract(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
        },
        [BuiltInFunction.LLMPROFILE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmProfile(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMARCH] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmArch(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMTENSOR] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmTensor(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMTOKENCOUNT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmTokenCount(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMTOKENS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmTokens(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMDETOKENIZE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmDetokenize(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMTENSORVALUE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmTensorValue(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMPOLICYOUT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmPolicyOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMARCHOUT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmArchOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMMODULEOUT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmModuleOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMKERNELOUT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmKernelOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMKVOUT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmKvOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.ASRTRANSCRIBE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AsrTranscribe(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMKERNEL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmKernel(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMMODULE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmModule(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMCOMPILE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmCompile(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMQUALITY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmQuality(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.LLMKV] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.LlmKv(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBENCHMARK] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiBenchmark(
                        ArgOr(args, 0)?.ToString() ?? "",
                        args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "",
                        args.Length > 2 ? args[2]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIBENCHRESULT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiBenchResult(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIFINETUNE] = (task, args) =>
        {
                // Training LoRA: processo esterno che può durare minuti → blocking pool,
                // mai sul worker GPM (AGENTS §0ter). Stesso pattern di AIAGENTASK.
                {
                var model = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiFinetune(model, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AILORAAPPLY] = (task, args) =>
        {
                // Deploy/verifica adapter: processo esterno bloccante → blocking pool (§0ter).
                {
                var ckpt = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiLoraApply(ckpt, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLBUILD] = (task, args) =>
        {
                // BUILD può toccare disco + tokenizer: blocking pool, mai sul worker GPM (§0ter).
                {
                var jsonl = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillBuild(jsonl, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLSTATUS] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillStatus(),
        [BuiltInFunction.AIDISTILLSAMPLE] = (task, args) =>
        {
                // SAMPLE genera col modello (pesante): blocking pool, mai sul worker GPM (§0ter).
                {
                var prompts = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillSample(prompts, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLSAMPLEN] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillSampleN(),
        [BuiltInFunction.AIDISTILLSAMPLEFAIL] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillSampleFail(),
        [BuiltInFunction.AIDISTILLSAMPLEDONE] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillSampleDone(),
        [BuiltInFunction.AIDISTILLEVAL] = (task, args) =>
        {
                // EVAL genera col modello (pesante): blocking pool, mai sul worker GPM (§0ter).
                {
                var prompts = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillEval(prompts, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLEVALTOK] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillEvalTok(ArgOr(args, 0)),
        [BuiltInFunction.AIDISTILLEVALPASS] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillEvalPass(ArgOr(args, 0)),
        [BuiltInFunction.AIDISTILLEVALACC] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillEvalAcc(ArgOr(args, 0)),
        [BuiltInFunction.AIDISTILLAPPLY] = (task, args) =>
        {
                {
                var ckpt = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillApply(ckpt, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLRESTORE] = (task, args) =>
        {
                {
                var backup = ArgOr(args, 0)?.ToString() ?? "";
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillRestore(backup),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLTRAIN] = (task, args) =>
        {
                // TRAIN addestra (pesante): blocking pool, mai sul worker GPM (§0ter).
                {
                var dataset = ArgOr(args, 0)?.ToString() ?? "";
                object? opts = args.Length > 1 ? args[1] : null;
                ParkBlocking(task, () => Basic.IlBackend.IlRuntime.AiDistillTrain(dataset, opts),
                result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.AIDISTILLLOSS] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillLoss(),
        [BuiltInFunction.AIDISTILLHOLD] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillHold(),
        [BuiltInFunction.AIDISTILLCKPT] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillCkpt(),
        [BuiltInFunction.AIDISTILLDONE] = (task, args) => Basic.IlBackend.IlRuntime.AiDistillDone(),
        [BuiltInFunction.AIFINETUNESTATUS] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneStatus(),
        [BuiltInFunction.AIFINETUNELOSS] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneLoss(),
        [BuiltInFunction.AIFINETUNEHOLD] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneHold(),
        [BuiltInFunction.AIFINETUNECKPT] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneCkpt(),
        [BuiltInFunction.AIFINETUNEDONE] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneDone(),
        [BuiltInFunction.AIFINETUNEGREEDY] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneGreedy(),
        [BuiltInFunction.AIFINETUNEGREEDYAFTER] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneGreedyAfter(),
        [BuiltInFunction.AIFINETUNEDEMOCHANGED] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneDemoChanged(),
        [BuiltInFunction.AIFINETUNEBASETRAIN] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneBaseTrain(),
        [BuiltInFunction.AIFINETUNEBASEHOLD] = (task, args) => Basic.IlBackend.IlRuntime.AiFinetuneBaseHold(),
        [BuiltInFunction.LLMRUNSJSON] = (task, args) => Basic.IlBackend.IlRuntime.RunsJson(),
        [BuiltInFunction.LLMRUNSCOUNT] = (task, args) => (double)Basic.IlBackend.IlRuntime.RunsCount(),
        [BuiltInFunction.LLMRUNSCLEAR] = (task, args) => (double)Basic.IlBackend.IlRuntime.RunsClear(),
        [BuiltInFunction.LLMTRAINCONFIG] = (task, args) => Basic.IlBackend.IlRuntime.TrainConfig(),
        [BuiltInFunction.LLMTRAINCONFIGSET] = (task, args) => Basic.IlBackend.IlRuntime.TrainConfigSet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.LLMRUNSITEM] = (task, args) => Basic.IlBackend.IlRuntime.RunsItem(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.LLMDATASETSTATS] = (task, args) => Basic.IlBackend.IlRuntime.DatasetStats(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.LLMLORALIST] = (task, args) => Basic.IlBackend.IlRuntime.LoraList(),
        [BuiltInFunction.LLMLORASAVE] = (task, args) => (double)Basic.IlBackend.IlRuntime.LoraSave(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.LLMLORALOAD] = (task, args) => (double)Basic.IlBackend.IlRuntime.LoraLoad(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.AIDPO] = (task, args) => Basic.IlBackend.IlRuntime.AiDpo(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)),
        [BuiltInFunction.AIGRPO] = (task, args) => Basic.IlBackend.IlRuntime.AiGrpo(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)),
        [BuiltInFunction.LLMTRAINEXPORT] = (task, args) => (double)Basic.IlBackend.IlRuntime.TrainExport(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.LLMRUNSTOP] = (task, args) => (double)Basic.IlBackend.IlRuntime.RunsStop(ArgOr(args, 0)?.ToString() ?? ""),
        [BuiltInFunction.LLMEVALGATE] = (task, args) => Basic.IlBackend.IlRuntime.LlmEvalGate(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.AIAGENTTRACE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentTrace(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTEVENTS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentEvents();
        },
        [BuiltInFunction.AIAGENTSEND] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentSend(
                ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "", args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : 0);
        },
        [BuiltInFunction.AIAGENTRECEIVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentReceive(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTPAUSE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentPause(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTRESUME] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentResume(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTSTOP] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentStop(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTQUEUE_STATS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentQueueStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTQUEUE_CLEAR] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentQueueClear(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AICHANNEL_CREATE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiChannelCreate(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AICHANNEL_PUBLISH] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiChannelPublish(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AICHANNEL_SUBSCRIBE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiChannelSubscribe(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AICHANNEL_UNSUBSCRIBE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiChannelUnsubscribe(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTCODE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentCode(ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIAGENTFIX] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiAgentFix(ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIKGADDNODE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiKgAddNode(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIKGADDEDGE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiKgAddEdge(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIKGQUERY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.AiKgQuery(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIAGENTASKSTREAM] = (task, args) =>
        {
                    var cb = args.Length > 2 ? args[2] : null;
                    // Il compiler PCode emette il NOME della SUB callback (StringExpr); il path IL
                    // emette lo symbol id (int). Distinguere: con nome -> invoke-by-name PCode.
                    if (cb is string cbName)
                        return Basic.IlBackend.IlRuntime.AiAgentAskStreamByName(
                            args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                            args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                            cbName);
                    return Basic.IlBackend.IlRuntime.AiAgentAskStream(
                    args.Length > 0 ? args[0]?.ToString() ?? "" : "",
                    args.Length > 1 ? args[1]?.ToString() ?? "" : "",
                    args.Length > 2 ? (int)SafeDouble(args[2]) : 0);
            
        },
    };

    private object? ExecuteBuiltInAiagent(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AIAGENTDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAiagent (fn={fn}).");

}
