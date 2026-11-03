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
    /// <summary>AI: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> AIBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIBASE,
        BuiltInFunction.AIDEF,
        BuiltInFunction.AIHFREPO,
        BuiltInFunction.AIINFO,
        BuiltInFunction.AIKEY,
        BuiltInFunction.AIKEYS,
        BuiltInFunction.AILOAD,
        BuiltInFunction.AILOADCONFIG,
        BuiltInFunction.AILOGPATH,
        BuiltInFunction.AILOGWRITE,
        BuiltInFunction.AILOGWRITELINE,
        BuiltInFunction.AIMODEL,
        BuiltInFunction.AIOCR,
        BuiltInFunction.AIOFF,
        BuiltInFunction.AION,
        BuiltInFunction.AISAVE,
        BuiltInFunction.AISAVECONFIG,
        BuiltInFunction.AISETHFREPO,
        BuiltInFunction.AISETHFTOKEN,
        BuiltInFunction.AISETOPTION,
        BuiltInFunction.AIOPTION,
        BuiltInFunction.AITEMP,
        BuiltInFunction.AITRANSCRIBE,
        BuiltInFunction.AIVERIFY,
        // AI OBSERVE (P1 todo AI-Runtime consolidato): azioni + letture registry.
        BuiltInFunction.OBSERVEFILE, BuiltInFunction.OBSERVELOG, BuiltInFunction.OBSERVESCREEN,
        BuiltInFunction.OBSERVATIONLATEST, BuiltInFunction.OBSERVATION, BuiltInFunction.OBSERVATIONHASH,
        BuiltInFunction.OBSERVATIONCOUNT, BuiltInFunction.OBSERVATIONLIST, BuiltInFunction.OBSERVATIONFILE,
        BuiltInFunction.AIEVALVALIDATE,
        BuiltInFunction.AIDECIDE,
        BuiltInFunction.AIASKCHAIN,
        // F3 multimodale (2026-09-11).
        BuiltInFunction.AIIMAGEINFO,
        BuiltInFunction.AIIMAGERESIZE,
        BuiltInFunction.AIIMAGECROP,
        BuiltInFunction.AIIMAGECONVERT,
        // F2 multimodale (2026-09-11).
        BuiltInFunction.AIDOCFIELDS,
        BuiltInFunction.AIDOCEXTRACT,
        // F4 multimodale (2026-09-11).
        BuiltInFunction.AISYNTHESIZE,
        // F5 multimodale (2026-09-11).
        BuiltInFunction.AIIMAGEEMBED,
        BuiltInFunction.AIIMAGECOMPARE,
        BuiltInFunction.AIIMAGEDETECT,
        BuiltInFunction.AIIMAGECLASSIFY,
        // F6 multimodale (2026-09-11).
        BuiltInFunction.AIAGENTASKFILE,
        // F7 multimodale (2026-09-11).
        BuiltInFunction.AIVIDEOINFO,
        BuiltInFunction.AIVIDEOFRAME,
        BuiltInFunction.AIVIDEOSNAPSHOT,
        // P2 (2026-09-15): AI CONTEXT engine — letture (AI.CONTEXT*).
        BuiltInFunction.AICONTEXTUSED,
        BuiltInFunction.AICONTEXTBUDGET,
        BuiltInFunction.AICONTEXTOVERFLOW,
        BuiltInFunction.AICONTEXTVIEW,
        BuiltInFunction.AICONTEXTVIEWDEBUG,
        BuiltInFunction.AICONTEXTSET,
        BuiltInFunction.AICONTEXTVIEWCMD,
        // P2 residuo (2026-09-16): AI CONTEXT SET POLICY|SELECTOR.
        BuiltInFunction.AICONTEXTSETPOLICY,
        BuiltInFunction.AICONTEXTSETSELECTOR,
        // §3.6 PARSEC: AI CONTEXT builder a SEGMENTI (agente).
        BuiltInFunction.AICONTEXTCREATE,
        BuiltInFunction.AICONTEXTADD,
        BuiltInFunction.AICONTEXTMAP,
        BuiltInFunction.AICONTEXTTRIM,
        BuiltInFunction.AICONTEXTGC,
        BuiltInFunction.AICONTEXTCOMMIT,
        BuiltInFunction.AICONTEXTHASH,
        BuiltInFunction.AICONTEXTVALID,
        BuiltInFunction.AITASKMETRICS,
        // P3 (2026-09-15): AI WORKER + AI STATE — WorkerRuntime.
        BuiltInFunction.AIWORKERNEW,
        BuiltInFunction.AIWORKERRUN,
        BuiltInFunction.AIWORKERINVOKE,
        BuiltInFunction.AIWORKERLOOP,
        BuiltInFunction.AIWORKERMANAGE,
        BuiltInFunction.AIREPLAYAUTO,
        BuiltInFunction.AIOBSERVETEST,
        BuiltInFunction.AIOBSERVEDATABASE,
        BuiltInFunction.AIOBSERVEPROCESS,
        BuiltInFunction.AIOBSERVEBROWSER,
        BuiltInFunction.AISTATESAVE,
        BuiltInFunction.AISTATELOAD,
        BuiltInFunction.AIWORKERROUNDS,
        BuiltInFunction.AIWORKERRESCUED,
        BuiltInFunction.AIWORKERSPEND,
    };

    /// <summary>Ai: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _aiDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> AIDispatch
        => _aiDispatch ??= BuildAiDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildAiDispatch() => new()
    {
        [BuiltInFunction.AIASKCHAIN] = (task, args) =>
        {
            // AI ASKCHAIN name, prompt, prefix (T1.2): anello concatenato (Blackboard), bloccante
            // (LLM) → ParkBlocking fuori dal worker. Ritorna la risposta dell'anello.
            var name = ArgOr(args, 0)?.ToString() ?? "";
            var prompt = ArgOr(args, 1)?.ToString() ?? "";
            var prefix = ArgOr(args, 2)?.ToString() ?? "";
            ParkBlocking(task,
                () => Basic.IlBackend.IlRuntime.AiAskChain(name, prompt, prefix),
                res => task.PushRef(res));
            return ParkedBuiltinResult;
        },
        [BuiltInFunction.AIEVALVALIDATE] = (task, args) =>
        {
            // AI EVAL VALIDATE <result> AGAINST VAULT <folder>: UAT via vault (T4.1). File
            // scan veloce, sincrono (nessun park necessario). Ritorna il verdetto JSON.
            var result = ArgOr(args, 0)?.ToString() ?? "";
            var folder = ArgOr(args, 1)?.ToString() ?? "";
            return Basic.IlBackend.IlRuntime.AiEvalValidate(result, folder);
        },
        [BuiltInFunction.AIDECIDE] = (task, args) =>
        {
            // AI DECIDE <state>, "NOUL|SCORE|CHOICE" (v0, provider Rules, JSON): sincrono,
            // nessun modello/rete. Stessa forma del runtime C (gmb_ai_decide_run_json).
            if (args.Length >= 6)
                return Basic.IlBackend.IlRuntime.AiDecideVerify(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgOr(args, 1)?.ToString() ?? "",
                    ArgOr(args, 2)?.ToString() ?? "");
            if (args.Length >= 5)
                return Basic.IlBackend.IlRuntime.AiDecideGroup(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgOr(args, 1)?.ToString() ?? "",
                    ArgOr(args, 2)?.ToString() ?? "",
                    ArgOr(args, 3)?.ToString() ?? "",
                    ArgOr(args, 4)?.ToString() ?? "");
            var state = ArgOr(args, 0)?.ToString() ?? "";
            var kind = ArgOr(args, 1)?.ToString() ?? "";
            var provider = ArgOr(args, 2)?.ToString() ?? "";
            var fallback = ArgOr(args, 3)?.ToString() ?? "";
            return fallback.Length > 0
                ? Basic.IlBackend.IlRuntime.AiDecide(state, kind, provider, fallback)
                : Basic.IlBackend.IlRuntime.AiDecide(state, kind, provider);
        },
        [BuiltInFunction.AIVERIFY] = (task, args) =>
        {
            // AI VERIFY <action> REQUIRE <require> (comando b): verifier/retry con REQUIRE
            // valutato sulle LOCALI del chiamante + RESULT$ (parità S_eval). Bloccante
            // (compile su file) → ParkBlocking fuori dal worker, mai Task.Wait in worker.
            var action = ArgOr(args, 0)?.ToString() ?? "";
            var require = ArgOr(args, 1)?.ToString() ?? "";
            var hostLocals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
            var localNames = ResolveProcLocalNames(task.Ip);
            int vlen = System.Math.Min(localNames?.Length ?? 0, hostLocals.Length);
            var names = new string[vlen];
            var values = new object?[vlen];
            for (int i = 0; i < vlen; i++) { names[i] = localNames![i] ?? ""; values[i] = hostLocals[i]; }
            ParkBlocking(task,
                () => Basic.IlBackend.IlRuntime.AiVerify(action, require, 3.0, names, values),
                res => task.PushRef(res));
            return ParkedBuiltinResult;
        },
        [BuiltInFunction.AILOADCONFIG] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiLoadConfig();
        },
        [BuiltInFunction.AISAVECONFIG] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSaveConfig();
            
                // ── AI LOG ───────────────────────────────────────────────────────
        },
        [BuiltInFunction.AILOGPATH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiLogPath(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AILOGWRITE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiLogWrite(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AILOGWRITELINE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiLogWriteLine(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
            
                // ── AI PATH MODEL/SKILL/PLUGIN ADD|REMOVE ───────────────────────
                // --- AIPATH: delegato a ExecuteBuiltInAipath (PCodeProcessor.Builtins.Aipath.cs) ---
        },
        [BuiltInFunction.AIOCR] = (task, args) =>
        {
                // Runtime comune (IlRuntime): supporta prompt opzionale e redact (PiiRedactor),
                // stessa logica del backend IL — unica implementazione.
                return Basic.IlBackend.IlRuntime.AiOcrFull(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "Extract all text from this image." : "Extract all text from this image.",
                args.Length >= 3 ? SafeDouble(args[2]) : 0);
        },
        [BuiltInFunction.AITRANSCRIBE] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica del backend IL — unica implementazione.
                return Basic.IlBackend.IlRuntime.AiTranscribe(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        // F3 multimodale (2026-09-11): runtime comune IlRuntime (stessa logica del backend IL).
        [BuiltInFunction.AIIMAGEINFO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageInfo(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIIMAGERESIZE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageResize(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "",
                args.Length >= 3 ? SafeDouble(args[2]) : 0,
                args.Length >= 4 ? SafeDouble(args[3]) : 0);
        },
        [BuiltInFunction.AIIMAGECROP] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageCrop(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "",
                args.Length >= 3 ? SafeDouble(args[2]) : 0,
                args.Length >= 4 ? SafeDouble(args[3]) : 0,
                args.Length >= 5 ? SafeDouble(args[4]) : 0,
                args.Length >= 6 ? SafeDouble(args[5]) : 0);
        },
        [BuiltInFunction.AIIMAGECONVERT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageConvert(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        // F2 multimodale (2026-09-11): runtime comune IlRuntime (stessa logica del backend IL).
        [BuiltInFunction.AIDOCFIELDS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiDocFields(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIDOCEXTRACT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiDocExtract(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        // F4 multimodale (2026-09-11): TTS.
        [BuiltInFunction.AISYNTHESIZE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSynthesize(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        // F5 multimodale (2026-09-11).
        [BuiltInFunction.AIIMAGEEMBED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageEmbed(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIIMAGECOMPARE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageCompare(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIIMAGEDETECT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageDetect(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIIMAGECLASSIFY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiImageClassify(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        // F6 multimodale (2026-09-11).
        [BuiltInFunction.AIAGENTASKFILE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiAgentAskFile(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "",
                args.Length >= 3 ? args[2]?.ToString() ?? "" : "");
        },
        // F7 multimodale (2026-09-11).
        [BuiltInFunction.AIVIDEOINFO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiVideoInfo(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIVIDEOFRAME] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiVideoFrame(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? SafeDouble(args[1]) : 0,
                args.Length >= 3 ? args[2]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIVIDEOSNAPSHOT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiVideoSnapshot(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        // P2 (2026-09-15): AI CONTEXT engine — letture (AI.CONTEXT*), runtime comune IlRuntime.
        [BuiltInFunction.AICONTEXTUSED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextUsed(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AICONTEXTBUDGET] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextBudget(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AICONTEXTOVERFLOW] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextOverflow(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AICONTEXTVIEW] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextView(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AICONTEXTVIEWDEBUG] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextViewDebug(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AICONTEXTSET] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextSet(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AICONTEXTSETPOLICY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextSetPolicy(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AICONTEXTSETSELECTOR] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextSetSelector(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AICONTEXTVIEWCMD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiContextViewCmd(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        // §3.6 PARSEC: AI CONTEXT builder a SEGMENTI (agente) — CREATE/ADD/MAP/TRIM/GC/COMMIT.
        [BuiltInFunction.AICONTEXTCREATE] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextCreate((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTADD] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextAdd(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "",
                (args.Length >= 2 ? args[1]?.ToString() : "") ?? "",
                (args.Length >= 3 ? args[2]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTMAP] = (task, args) =>
        {
                // Report sul canale PRINT della VM (PendingPrints), NON su Console.WriteLine:
                // il delegato IlRuntime.AiContextMap stampa sul console del runtime IL, che in
                // PCode non è quello catturato → output vuoto (divergenza MiniTest parity).
                PCodePrintText(task, Basic.AI.Context.AiContextEngine.Map(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? ""));
                return 1.0;
        },
        [BuiltInFunction.AICONTEXTTRIM] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextTrim((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTGC] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextGc((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTCOMMIT] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextCommit((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTHASH] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextHash((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AICONTEXTVALID] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiContextValid(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "",
                (args.Length >= 2 ? args[1]?.ToString() : "") ?? ""),
        [BuiltInFunction.AITASKMETRICS] = (task, args) =>
                Basic.IlBackend.IlRuntime.AiTaskMetrics((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        // P3 (2026-09-15): AI WORKER + AI STATE — WorkerRuntime dispatch.
        [BuiltInFunction.AIWORKERNEW] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerNew(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIOBSERVETEST] = (task, args) =>
            Basic.IlBackend.IlRuntime.AiObserveTest((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AIOBSERVEDATABASE] = (task, args) =>
            Basic.IlBackend.IlRuntime.AiObserveDatabase((args.Length >= 1 ? args[0]?.ToString() : "") ?? "",
                (args.Length >= 2 ? args[1]?.ToString() : "") ?? ""),
        [BuiltInFunction.AIOBSERVEPROCESS] = (task, args) =>
            Basic.IlBackend.IlRuntime.AiObserveProcess((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AIOBSERVEBROWSER] = (task, args) =>
            Basic.IlBackend.IlRuntime.AiObserveBrowser((args.Length >= 1 ? args[0]?.ToString() : "") ?? ""),
        [BuiltInFunction.AIREPLAYAUTO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiReplayAuto(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIWORKERMANAGE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerManage(
                args.Length >= 1 && double.TryParse(args[0]?.ToString(), out var cy) ? cy : 0,
                (args.Length >= 2 ? args[1]?.ToString() : "") ?? "",
                (args.Length >= 3 ? args[2]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIWORKERLOOP] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerLoop(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "",
                args.Length >= 2 && double.TryParse(args[1]?.ToString(), out var r) ? r : 0,
                (args.Length >= 3 ? args[2]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIWORKERINVOKE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerInvoke(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIWORKERRUN] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerRun(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AISTATESAVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiStateSave(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "",
                (args.Length >= 3 ? args[2]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AISTATELOAD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiStateLoad(
                (args.Length >= 1 ? args[0]?.ToString() : "") ?? "", (args.Length >= 2 ? args[1]?.ToString() : "") ?? "");
        },
        [BuiltInFunction.AIWORKERROUNDS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerRounds();
        },
        [BuiltInFunction.AIWORKERRESCUED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerRescued();
        },
        [BuiltInFunction.AIWORKERSPEND] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiWorkerSpend();
        },
        [BuiltInFunction.AIKEYS] = (task, args) =>
        {
                // Runtime comune (IlRuntime): providers.json — unica implementazione.
                return Basic.IlBackend.IlRuntime.AiProviders();
        },
        [BuiltInFunction.AIKEY] = (task, args) =>
        {
                // AI.GETKEY(name): legge l'apiKey.
                return Basic.IlBackend.IlRuntime.AiGetKey(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AISETHFTOKEN] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSetHfToken(args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AISETHFREPO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSetHfRepo(args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AISETOPTION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSetOption(
                    args.Length >= 1 ? args[0]?.ToString() ?? "" : "",
                    args.Length >= 2 ? args[1]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIOPTION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiOption(args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIHFREPO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiHfRepo();
        },
        [BuiltInFunction.AIMODEL] = (task, args) =>
        {
                // AI.GETMODEL(name): legge il modello.
                return Basic.IlBackend.IlRuntime.AiGetModel(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AITEMP] = (task, args) =>
        {
                // AI.GETTEMP(name): legge la temperatura.
                return Basic.IlBackend.IlRuntime.AiGetTemp(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIBASE] = (task, args) =>
        {
                // AI.GETBASE(name): legge la baseUrl (name vuoto → provider attivo).
                return Basic.IlBackend.IlRuntime.AiGetBase(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIINFO] = (task, args) =>
        {
                // Runtime comune (IlRuntime): name vuoto → provider attivo.
                return Basic.IlBackend.IlRuntime.AiInfo(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AISAVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSaveConfig();
        },
        [BuiltInFunction.AILOAD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiLoadConfig();
        },
        [BuiltInFunction.AIDEF] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiGetDefault();
        },
        [BuiltInFunction.AION] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiEnable(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        [BuiltInFunction.AIOFF] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiDisable(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "");
        },
        // AI OBSERVE (P1 todo AI-Runtime consolidato): azioni del comando + letture registry.
        [BuiltInFunction.OBSERVEFILE] = (task, args) => Basic.IlBackend.IlRuntime.AiObserveFile(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.OBSERVELOG] = (task, args) => Basic.IlBackend.IlRuntime.AiObserveLog(args.Length > 0 ? args[0]?.ToString() ?? "" : "", Num(args, 1)),
        [BuiltInFunction.OBSERVESCREEN] = (task, args) => Basic.IlBackend.IlRuntime.AiObserveScreen(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        [BuiltInFunction.OBSERVATIONLATEST] = (task, args) => Basic.IlBackend.IlRuntime.ObservationLatest(),
        [BuiltInFunction.OBSERVATION] = (task, args) => Basic.IlBackend.IlRuntime.Observation(Num(args, 0)),
        [BuiltInFunction.OBSERVATIONHASH] = (task, args) => Basic.IlBackend.IlRuntime.ObservationHash(Num(args, 0)),
        [BuiltInFunction.OBSERVATIONCOUNT] = (task, args) => Basic.IlBackend.IlRuntime.ObservationCount(),
        [BuiltInFunction.OBSERVATIONLIST] = (task, args) => Basic.IlBackend.IlRuntime.ObservationList(),
        [BuiltInFunction.OBSERVATIONFILE] = (task, args) => Basic.IlBackend.IlRuntime.ObservationFile(Num(args, 0)),
    };

    private object? ExecuteBuiltInAi(PCodeTask task, BuiltInFunction fn, object?[] args)
        => AIDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInAi (fn={fn}).");

    /// <summary>Accoda un testo multi-riga al canale PRINT della VM (PendingPrints, kind 1 =
    /// riga chiusa). I report dei builtin (AI CONTEXT MAP, …) devono usare lo STESSO canale di
    /// PRINT: con System.Console.WriteLine il testo non passa dal console del runtime e i
    /// MiniTest di parità vedono output vuoto su PCode (IL lo cattura), oltre a bypassare il
    /// routing grafico/Clipper.</summary>
    private static void PCodePrintText(PCodeTask task, string? text)
    {
        if (string.IsNullOrEmpty(text)) return;
        var only = Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterOnly();
        var routing = Basic.IlBackend.IlRuntime.Current.Graphics.ClipperPrinterRouting();
        foreach (var line in text!.Replace("\r\n", "\n").Split('\n'))
            (task.PendingPrints ??= new()).Add((1, line, only, routing));
    }

}
