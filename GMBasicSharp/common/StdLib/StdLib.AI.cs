// StdLib/StdLib.AI.cs
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: AI — namespace 'AI'
 * 261 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (261):
 *   Active
 *   AgentAskFile
 *   AgentAskSync
 *   AgentCode
 *   AgentEvents
 *   AgentFix
 *   AgentGet
 *   AgentList
 *   AgentPause
 *   AgentQueueClear
 *   AgentQueueStats
 *   AgentReceive
 *   AgentResume
 *   AgentRun
 *   AgentRunCode
 *   AgentRunGraph
 *   AgentRunRag
 *   AgentSend
 *   AgentStatus
 *   AgentStop
 *   AgentSwarm
 *   AnalyzeImage
 *   ArtifactGet
 *   ArtifactLink
 *   ArtifactList
 *   ArtifactPut
 *   ArtifactRemove
 *   Ask
 *   AskChain
 *   AskMulti
 *   Backlinks
 *   BlackboardRead
 *   BlackboardWait
 *   BlackboardWrite
 *   BrainDetach
 *   BrainInfo
 *   BrainList
 *   BrainUse
 *   BrainUsed
 *   BrainVaultBuild
 *   BrainVaultQuery
 *   BrainVaultWrite
 *   CategoryAuto
 *   CategoryFallback
 *   CategoryList
 *   CategoryPick
 *   CategorySet
 *   ChannelCreate
 *   ChannelPublish
 *   ChannelSubscribe
 *   ChannelUnsubscribe
 *   Checkpoint
 *   CheckpointClear
 *   CheckpointList
 *   Configure
 *   Consistency
 *   ConsistencyDetail
 *   ContextBudget
 *   ContextHash
 *   ContextOverflow
 *   ContextUsed
 *   ContextValid
 *   ContextView
 *   ContextViewDebug
 *   Count
 *   Credits
 *   Discover
 *   DocExtract
 *   DocFields
 *   Embed
 *   Enrich
 *   EnsureCollection
 *   Fail
 *   FleetStats
 *   GenTest
 *   GetBase
 *   GetDefault
 *   GetKey
 *   GetModel
 *   GetTemp
 *   GoalAgent
 *   GoalClear
 *   GoalGet
 *   GoalSet
 *   GoalStatus
 *   GoalTick
 *   Handoff
 *   HandoffClear
 *   HandoffComplete
 *   HandoffList
 *   HandoffStatus
 *   HashEdit
 *   HashLine
 *   HashVerify
 *   HealthCheckInterval
 *   HfRepo
 *   HookEvents
 *   HookFire
 *   HookList
 *   HookRegister
 *   HookUnregister
 *   ImageClassify
 *   ImageCompare
 *   ImageConvert
 *   ImageCrop
 *   ImageDetect
 *   ImageEmbed
 *   ImageInfo
 *   ImageResize
 *   IndexMeta
 *   IndexText
 *   Info
 *   IngestWeb
 *   KgAddEdge
 *   KgAddNode
 *   KgQuery
 *   LexicalEntropy
 *   LinkDangling
 *   LinkJson
 *   LinksOut
 *   List
 *   ListCollections
 *   Load
 *   LoadDocuments
 *   McpCall
 *   McpListTools
 *   McpListToolsServer
 *   McpPrompts
 *   McpResources
 *   McpServerList
 *   McpServerMarketplaceList
 *   McpServerStatus
 *   MemoryAsk
 *   MemoryConsolidate
 *   MemoryRank
 *   MetricGet
 *   MetricList
 *   MetricRecord
 *   MetricReset
 *   NoteExists
 *   NoteFolder
 *   NoteGet
 *   NoteLink
 *   NoteSetAlias
 *   NoteTags
 *   NoteTitle
 *   NoteUnlink
 *   NoteWordCount
 *   OCR
 *   Observation
 *   ObservationCount
 *   ObservationFile
 *   ObservationHash
 *   ObservationLatest
 *   ObservationList
 *   Option
 *   OrchLoad
 *   OrchSave
 *   PlanAdd
 *   PlanApprove
 *   PlanClear
 *   PlanDefine
 *   PlanList
 *   PlanRun
 *   PlanStatus
 *   PluginCall
 *   PluginList
 *   PluginsInfo
 *   PluginsList
 *   Priority
 *   Providers
 *   PurgeDeprecated
 *   QaAnswer
 *   RagContext
 *   RagContextMulti
 *   Register
 *   RemoveSource
 *   ReplayClear
 *   ReplayGet
 *   ReplayList
 *   ReplayRecord
 *   ReplayRun
 *   ReplayStart
 *   ReplayStop
 *   Rerank
 *   ResetFailed
 *   ResourceAvailable
 *   ResourceGet
 *   ResourceList
 *   ResourceSet
 *   ResourceSwarmSize
 *   Resume
 *   Rotate
 *   RulesAdd
 *   RulesClear
 *   RulesFor
 *   RulesList
 *   RulesRemove
 *   RunHealthCheck
 *   Save
 *   Search
 *   SearchMulti
 *   SetActive
 *   SetCredits
 *   SetPriority
 *   SetStatus
 *   SetWeight
 *   ShareDecrypt
 *   ShareEncrypt
 *   SignNote
 *   SkillsList
 *   SkillsPaths
 *   SkillsPrompt
 *   StartHealthCheck
 *   StateLoad
 *   Status
 *   StopHealthCheck
 *   SuggestTags
 *   SyncFolder
 *   Synthesize
 *   TaskAssign
 *   TaskClear
 *   TaskDefine
 *   TaskDepends
 *   TaskList
 *   TaskMetrics
 *   TaskRun
 *   TaskStatus
 *   TeamAgents
 *   TeamLatent
 *   TeamLinkInfo
 *   TeamList
 *   TeamOnStream
 *   TeamRecursion
 *   TeamRunStart
 *   TeamRunWait
 *   TeamRuntimeStats
 *   TeamTrainStatus
 *   Test
 *   Transcribe
 *   Unregister
 *   VaultAddFolder
 *   VaultCount
 *   VaultFolders
 *   VaultInfo
 *   VaultList
 *   VaultPolicy
 *   VaultRemoveFolder
 *   VaultSetFolderProps
 *   VaultWatch
 *   VerifyNote
 *   VideoFrame
 *   VideoInfo
 *   VideoSnapshot
 *   Weight
 *   WorkerRescued
 *   WorkerRounds
 *   WorkerSpend
 *   WorkflowList
 *   WorkflowStatus
 *   WorkflowSteps
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateAiNamespace()
    {
        const string ns = "AI";
        var pub = SymbolFlags.IsPublic;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiOcr, "OCR"), [P("path", false), P("model", true), new Parameter(-1, T(TokenKind.Identifier, "redact"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiOcr, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTranscribe, "Transcribe"), [P("path", false)], new StringType(), TokenKind.AiTranscribe, ExecutionKind: NativeExecutionKind.Pure),
            // F3 multimodale (2026-09-11): primitive immagini PNG (solo PNG in decode, come il
            // runtime C gmb_image.c — parità 4 backend; altri formati = errore esplicito).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageInfo, "ImageInfo"), [P("path", false)], new StringType(), TokenKind.AiImageInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageResize, "ImageResize"), [P("input", false), P("output", false), new Parameter(-1, T(TokenKind.Identifier, "width"), new DoubleType(), SymbolFlags.None, null, true), new Parameter(-1, T(TokenKind.Identifier, "height"), new DoubleType(), SymbolFlags.None, null, true)], new LongType(), TokenKind.AiImageResize, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageCrop, "ImageCrop"), [P("input", false), P("output", false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), SymbolFlags.None, null, true), new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), SymbolFlags.None, null, true), new Parameter(-1, T(TokenKind.Identifier, "width"), new DoubleType(), SymbolFlags.None, null, true), new Parameter(-1, T(TokenKind.Identifier, "height"), new DoubleType(), SymbolFlags.None, null, true)], new LongType(), TokenKind.AiImageCrop, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageConvert, "ImageConvert"), [P("input", false), P("output", false)], new LongType(), TokenKind.AiImageConvert, ExecutionKind: NativeExecutionKind.Pure),
            // F2 multimodale (2026-09-11): estrazione strutturata documenti (JSON).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDocFields, "DocFields"), [P("path", false)], new StringType(), TokenKind.AiDocFields, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiDocExtract, "DocExtract"), [P("path", false), P("schema", false)], new StringType(), TokenKind.AiDocExtract, ExecutionKind: NativeExecutionKind.Pure),
            // F4 multimodale (2026-09-11): TTS — sintetizza testo in un file WAV (motore OS).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSynthesize, "Synthesize"), [P("text", false), P("output", false)], new LongType(), TokenKind.AiSynthesize, ExecutionKind: NativeExecutionKind.Pure),
            // F5 multimodale (2026-09-11): embedding percettivo (64), compare 0..100, detect/classify.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageEmbed, "ImageEmbed"), [P("path", false)], new ArrayType(new DoubleType()), TokenKind.AiImageEmbed, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageCompare, "ImageCompare"), [P("path1", false), P("path2", false)], new LongType(), TokenKind.AiImageCompare, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageDetect, "ImageDetect"), [P("path", false)], new StringType(), TokenKind.AiImageDetect, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiImageClassify, "ImageClassify"), [P("path", false), P("labels", false)], new StringType(), TokenKind.AiImageClassify, ExecutionKind: NativeExecutionKind.Pure),
            // F6 multimodale (2026-09-11): agent ask con input multimodale (immagine/audio/documento).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentAskFile, "AgentAskFile"), [P("name", false), P("path", false), P("question", true)], new StringType(), TokenKind.AiAgentAskFile, ExecutionKind: NativeExecutionKind.Pure),
            // F7 multimodale (2026-09-11): video (Y4M nativo).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVideoInfo, "VideoInfo"), [P("path", false)], new StringType(), TokenKind.AiVideoInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVideoFrame, "VideoFrame"), [P("path", false), P("index", false), P("output", false)], new LongType(), TokenKind.AiVideoFrame, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVideoSnapshot, "VideoSnapshot"), [P("path", false), P("output", false)], new LongType(), TokenKind.AiVideoSnapshot, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiKeys, "Providers"), [], new StringType(), TokenKind.AiKeys, ExecutionKind: NativeExecutionKind.Pure),
            // Letture config provider (i Set* sono comandi AI SET KEY ... / AI.SETKEY ...).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGetKey, "GetKey"), [P("name", false)], new StringType(), TokenKind.AiGetKey, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGetModel, "GetModel"), [P("name", false)], new StringType(), TokenKind.AiGetModel, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGetTemp, "GetTemp"), [P("name", false)], new StringType(), TokenKind.AiGetTemp, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGetBase, "GetBase"), [P("name", false)], new StringType(), TokenKind.AiGetBase, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiInfo, "Info"), [P("name", false)], new StringType(), TokenKind.AiInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGetDefault, "GetDefault"), [], new StringType(), TokenKind.AiGetDefault, ExecutionKind: NativeExecutionKind.Pure),            // Layer provider (3 forme: funzione per lettura, comando per scrittura).
            // ── Provider registry UNIFICATO (ex namespace PROVIDER, fuso in AI 2026-09-03):
            // le LETTURE hanno gia' le gemelle sopra (Active/Rotate/List/Status/Priority/Weight/
            // Credits); qui le operazioni di gestione come FUNZIONI qualificate AI.X(...) con le
            // firme EX-PROVIDER (migrazione "PROVIDER. → AI." a parita' di ritorno). I comandi
            // AI ... a spazio restano per chi non usa il ritorno. REGISTERLOCAL eliminata (0
            // sample): la registrazione da path passa da AI REGISTER.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderRegister, "Register"), [P("role", false), P("name", false), P("kind", false), P("configJson", false)], new DoubleType(), TokenKind.ProviderRegister),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSetActive, "SetActive"), [P("role", false), P("name", false)], new DoubleType(), TokenKind.ProviderSetActive),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderConfigure, "Configure"), [P("name", false), P("key", false), P("value", false)], new DoubleType(), TokenKind.ProviderConfigure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderUnregister, "Unregister"), [P("name", false)], new DoubleType(), TokenKind.ProviderUnregister),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSave, "Save"), [P("path", false)], new DoubleType(), TokenKind.ProviderSave),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderLoad, "Load"), [P("path", false)], new DoubleType(), TokenKind.ProviderLoad),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderTest, "Test"), [P("role", false), P("name", false)], new StringType(), TokenKind.ProviderTest),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSetPriority, "SetPriority"), [P("name", false), new Parameter(-1, T(TokenKind.Identifier, "priority"), new DoubleType(), SymbolFlags.None, null, true)], new DoubleType(), TokenKind.ProviderSetPriority),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSetWeight, "SetWeight"), [P("name", false), new Parameter(-1, T(TokenKind.Identifier, "weight"), new DoubleType(), SymbolFlags.None, null, true)], new DoubleType(), TokenKind.ProviderSetWeight),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderDiscover, "Discover"), [P("prefix", false), P("capability", false), P("baseUrl", false), P("apiKey", false), P("soloFree", true)], new ArrayType(new StringType()), TokenKind.ProviderDiscover),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSetCredits, "SetCredits"), [P("name", false), new Parameter(-1, T(TokenKind.Identifier, "credits"), new DoubleType(), SymbolFlags.None, null, true)], new DoubleType(), TokenKind.ProviderSetCredits),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderMarkFailed, "Fail"), [P("name", false)], new DoubleType(), TokenKind.ProviderMarkFailed),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderResetFailed, "ResetFailed"), [P("name", false)], new DoubleType(), TokenKind.ProviderResetFailed),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderSetStatus, "SetStatus"), [P("name", false), P("status", false)], new DoubleType(), TokenKind.ProviderSetStatus),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderStartHealthCheck, "StartHealthCheck"), [P("intervalMinutes", false)], new DoubleType(), TokenKind.ProviderStartHealthCheck),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderStopHealthCheck, "StopHealthCheck"), [], new DoubleType(), TokenKind.ProviderStopHealthCheck),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderHealthCheckInterval, "HealthCheckInterval"), [], new IntegerType(), TokenKind.ProviderHealthCheckInterval),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ProviderRunHealthCheck, "RunHealthCheck"), [], new DoubleType(), TokenKind.ProviderRunHealthCheck),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiActive, "Active"), [P("cap", false)], new StringType(), TokenKind.AiActive, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRotate, "Rotate"), [P("role", false)], new StringType(), TokenKind.AiRotate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCredits, "Credits"), [P("name", false)], new IntegerType(), TokenKind.AiCredits, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPriority, "Priority"), [P("name", false)], new IntegerType(), TokenKind.AiPriority, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiWeight, "Weight"), [P("name", false)], new IntegerType(), TokenKind.AiWeight, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiStatus, "Status"), [P("name", false)], new StringType(), TokenKind.AiStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHfRepo, "HfRepo"), [], new StringType(), TokenKind.AiHfRepo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiOption, "Option"), [P("name", false)], new StringType(), TokenKind.AiOption, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiList, "List"), [P("role", false)], new ArrayType(new StringType()), TokenKind.AiList, ExecutionKind: NativeExecutionKind.Pure),
            // Agenti (funzioni sole: le azioni sono comandi AI AGENT ... / AI.AGENT... senza parentesi).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentAsk, "AgentAskSync"), [P("name", false), P("prompt", false)], new StringType(), TokenKind.AiAgentAsk, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAnalyzeImage, "AnalyzeImage"), [P("path", false), P("prompt", true)], new StringType(), TokenKind.AiAnalyzeImage, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentGet, "AgentGet"), [P("name", false), P("key", false)], new StringType(), TokenKind.AiAgentGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentList, "AgentList"), [], new ArrayType(new StringType()), TokenKind.AiAgentList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentStatus, "AgentStatus"), [P("name", false)], new StringType(), TokenKind.AiAgentStatus, ExecutionKind: NativeExecutionKind.Pure),
            // [HARD MOVE AI->LLM 2026-09-19] CacheStats, StreamStats, PrefillStats, DecodeStats,
            // BenchResult e Finetune{Status,Loss,Hold,Ckpt,Done,Greedy,GreedyAfter,DemoChanged,
            // BaseTrain,BaseHold} spostate nel namespace LLM (StdLib.LLM.cs) — stessi TokenKind.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentEvents, "AgentEvents"), [], new ArrayType(new StringType()), TokenKind.AiAgentEvents, ExecutionKind: NativeExecutionKind.Pure),
            // Queue/ACF L1 (B1): AgentQueueManager + MessageBus
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentSend, "AgentSend"), [P("to", false), P("task", false), P("input", false), new Parameter(-1, T(TokenKind.Identifier, "payment"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new DoubleType(), TokenKind.AiAgentSend, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentReceive, "AgentReceive"), [P("agent", false)], new StringType(), TokenKind.AiAgentReceive, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentPause, "AgentPause"), [P("agent", false)], new DoubleType(), TokenKind.AiAgentPause, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentResume, "AgentResume"), [P("agent", false)], new DoubleType(), TokenKind.AiAgentResume, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentStop, "AgentStop"), [P("agent", false)], new DoubleType(), TokenKind.AiAgentStop, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentQueueStats, "AgentQueueStats"), [P("agent", false)], new StringType(), TokenKind.AiAgentQueueStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentQueueClear, "AgentQueueClear"), [P("agent", false)], new DoubleType(), TokenKind.AiAgentQueueClear, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiChannelCreate, "ChannelCreate"), [P("channel", false)], new DoubleType(), TokenKind.AiChannelCreate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiChannelPublish, "ChannelPublish"), [P("channel", false), P("message", false)], new DoubleType(), TokenKind.AiChannelPublish, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiChannelSubscribe, "ChannelSubscribe"), [P("channel", false), P("handler", false)], new DoubleType(), TokenKind.AiChannelSubscribe, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiChannelUnsubscribe, "ChannelUnsubscribe"), [P("channel", false), P("handler", false)], new DoubleType(), TokenKind.AiChannelUnsubscribe, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentCode, "AgentCode"), [P("task", false), P("outputPath", true)], new StringType(), TokenKind.AiAgentCode, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAgentFix, "AgentFix"), [P("error", false), P("outputPath", true)], new StringType(), TokenKind.AiAgentFix, ExecutionKind: NativeExecutionKind.Pure),
            // B3.5 — KnowledgeGraph mutabile (AI.KGADDNODE/ADDEDGE/QUERY)
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiKgAddNode, "KgAddNode"), [P("node", false)], new DoubleType(), TokenKind.AiKgAddNode, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiKgAddEdge, "KgAddEdge"), [P("from", false), P("to", false)], new DoubleType(), TokenKind.AiKgAddEdge, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiKgQuery, "KgQuery"), [P("query", false)], new StringType(), TokenKind.AiKgQuery, ExecutionKind: NativeExecutionKind.Pure),
            // Vettori (funzioni = letture; le azioni sono comandi AI RAG ... / AI.VECTOR*).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorEmbed, "Embed"), [P("text", false), new Parameter(-1, T(TokenKind.Identifier, "dimensions"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new ArrayType(new SingleType()), TokenKind.AiVectorEmbed, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorCountEmbeddings, "Count"), [P("db", false), P("coll", false)], new LongType(), TokenKind.AiVectorCountEmbeddings, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorSearch, "Search"), [P("db", false), P("coll", false), P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.None, null, true), new Parameter(-1, T(TokenKind.Identifier, "hybrid"), new DoubleType(), SymbolFlags.IsOptional, null, true), P("filter", true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "explain"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiVectorSearch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorRagContext, "RagContext"), [P("db", false), P("coll", false), P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiVectorRagContext, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorAsk, "Ask"), [P("db", false), P("coll", false), P("question", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiVectorAsk, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorListCollections, "ListCollections"), [P("db", false)], new StringType(), TokenKind.AiVectorListCollections, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorRerank, "Rerank"), [P("query", false), P("json", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.None, null, true)], new StringType(), TokenKind.AiVectorRerank, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorEnsureCollection, "EnsureCollection"), [P("db", false), P("coll", false), new Parameter(-1, T(TokenKind.Identifier, "dimensions"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new LongType(), TokenKind.AiVectorEnsureCollection, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorIndexText, "IndexText"), [P("db", false), P("coll", false), P("sourceId", false), P("text", false)], new LongType(), TokenKind.AiVectorIndexText, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorIndexMeta, "IndexMeta"), [P("db", false), P("coll", false), P("sourceId", false), P("text", false), P("metadataJson", false)], new LongType(), TokenKind.AiVectorIndexMeta, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorRemoveSource, "RemoveSource"), [P("db", false), P("coll", false), P("sourceId", false)], new DoubleType(), TokenKind.AiVectorRemoveSource, ExecutionKind: NativeExecutionKind.Pure),
            // DoubleType (impl double, come RemoveSource): con LongType/IntegerType il valore
            // double veniva reinterpretato in bit-pattern (=4607182418800017408) su IL.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorPurgeDeprecated, "PurgeDeprecated"), [P("db", false), P("coll", false)], new DoubleType(), TokenKind.AiVectorPurgeDeprecated, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorLoadDocuments, "LoadDocuments"), [P("db", false), P("coll", false), P("path", false), P("pattern", true), P("metadataJson", true)], new LongType(), TokenKind.AiVectorLoadDocuments, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVectorGenTest, "GenTest"), [P("collection", false), P("n", false), P("types", false)], new StringType(), TokenKind.AiVectorGenTest, ExecutionKind: NativeExecutionKind.Pure),
            // Vault (Second Brain L1: le azioni sono comandi AI VAULT CREATE/INDEX).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultInfo, "VaultInfo"), [P("name", false)], new StringType(), TokenKind.AiVaultInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultFolders, "VaultFolders"), [P("name", false)], new StringType(), TokenKind.AiVaultFolders, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultCount, "VaultCount"), [P("name", false)], new LongType(), TokenKind.AiVaultCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultList, "VaultList"), [], new ArrayType(new StringType()), TokenKind.AiVaultList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultPolicy, "VaultPolicy"), [P("name", false)], new StringType(), TokenKind.AiVaultPolicy, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultAddFolder, "VaultAddFolder"), [P("name", false), P("folder", false), P("indexed", true)], new LongType(), TokenKind.AiVaultAddFolder, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultRemoveFolder, "VaultRemoveFolder"), [P("name", false), P("folder", false)], new LongType(), TokenKind.AiVaultRemoveFolder, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultSetFolderProps, "VaultSetFolderProps"), [P("name", false), P("folder", false), P("indexed", true)], new LongType(), TokenKind.AiVaultSetFolderProps, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiVaultWatch, "VaultWatch"), [P("name", false), P("policy", false)], new LongType(), TokenKind.AiVaultWatch, ExecutionKind: NativeExecutionKind.Pure),
            // Brain (Second Brain L6: le azioni sono comandi AI BRAIN USE/DETACH/INSTALL/UNINSTALL).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBrainList, "BrainList"), [], new ArrayType(new StringType()), TokenKind.AiBrainList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBrainInfo, "BrainInfo"), [P("name", false)], new StringType(), TokenKind.AiBrainInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBrainUsed, "BrainUsed"), [], new StringType(), TokenKind.AiBrainUsed, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBrainUse, "BrainUse"), [P("names", false)], new LongType(), TokenKind.AiBrainUse, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiBrainDetach, "BrainDetach"), [P("name", false)], new DoubleType(), TokenKind.AiBrainDetach, ExecutionKind: NativeExecutionKind.Pure),
            // Note (Second Brain L2: le azioni sono comandi AI NOTE CREATE/UPDATE/APPEND/DELETE/MOVE).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteGet, "NoteGet"), [P("vault", false), P("path", false)], new StringType(), TokenKind.NoteGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteTitle, "NoteTitle"), [P("vault", false), P("path", false)], new StringType(), TokenKind.NoteTitle, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteTags, "NoteTags"), [P("vault", false), P("path", false)], new StringType(), TokenKind.NoteTags, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteWordCount, "NoteWordCount"), [P("vault", false), P("path", false)], new LongType(), TokenKind.NoteWordCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteExists, "NoteExists"), [P("vault", false), P("path", false)], new DoubleType(), TokenKind.NoteExists, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NoteFolder, "NoteFolder"), [P("vault", false), P("path", false)], new StringType(), TokenKind.NoteFolder, ExecutionKind: NativeExecutionKind.Pure),
            // Gap revisione (F7 P3): link manuali / sync / web scrape (funzioni con vault esplicito).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiNoteLink, "NoteLink"), [P("vault", false), P("from", false), P("to", false), P("alias", true)], new LongType(), TokenKind.AiNoteLink, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiNoteUnlink, "NoteUnlink"), [P("vault", false), P("from", false), P("to", false)], new LongType(), TokenKind.AiNoteUnlink, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiNoteSetAlias, "NoteSetAlias"), [P("vault", false), P("from", false), P("to", false), P("alias", false)], new LongType(), TokenKind.AiNoteSetAlias, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSyncFolder, "SyncFolder"), [P("vault", false), P("src", false), P("direction", true), P("conflict", true)], new StringType(), TokenKind.AiSyncFolder, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiIngestWeb, "IngestWeb"), [P("vault", false), P("url", false), P("relPath", true)], new StringType(), TokenKind.AiIngestWeb, ExecutionKind: NativeExecutionKind.Pure),
            // Link (Second Brain L3: wikilink/backlink).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LinksOut, "LinksOut"), [P("vault", false), P("path", false)], new StringType(), TokenKind.LinksOut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.Backlinks, "Backlinks"), [P("vault", false), P("path", false)], new StringType(), TokenKind.Backlinks, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LinkDangling, "LinkDangling"), [P("vault", false)], new LongType(), TokenKind.LinkDangling, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.LinkJson, "LinkJson"), [P("vault", false)], new StringType(), TokenKind.LinkJson, ExecutionKind: NativeExecutionKind.Pure),
            // Brain multi-use (R7): query distribuita sui brain used (merge RRF + provenance brain).
            // Parametri opzionali (P1-2b): reranker (%RERANK_OFF/KEYWORD/ONNX), minEvidence,
            // explain (solo SEARCHMULTI; su RAG/ASK il contesto non espone explain).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSearchMulti, "SearchMulti"), [P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true), P("collection", true), P("brains", true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "explain"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiSearchMulti, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRagContextMulti, "RagContextMulti"), [P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiRagContextMulti, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAskMulti, "AskMulti"), [P("question", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "reranker"), new DoubleType(), SymbolFlags.IsOptional, null, true), new Parameter(-1, T(TokenKind.Identifier, "minEvidence"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AiAskMulti, ExecutionKind: NativeExecutionKind.Pure),
            // Skills (funzioni: lettura indice/lista).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSkillsList, "SkillsList"), [], new ArrayType(new StringType()), TokenKind.AiSkillsList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSkillsPrompt, "SkillsPrompt"), [], new StringType(), TokenKind.AiSkillsPrompt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSkillsPaths, "SkillsPaths"), [], new ArrayType(new StringType()), TokenKind.AiSkillsPaths, ExecutionKind: NativeExecutionKind.Pure),
            // Plugins (funzioni: lettura indice/info + invocazione capacita', namespace AI).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPluginsList, "PluginsList"), [], new ArrayType(new StringType()), TokenKind.AiPluginsList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPluginList, "PluginList"), [], new ArrayType(new StringType()), TokenKind.AiPluginList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPluginInfo, "PluginsInfo"), [P("name", false)], new StringType(), TokenKind.AiPluginInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPluginCall, "PluginCall"), [P("name", false), P("arg", true)], new StringType(), TokenKind.AiPluginCall, ExecutionKind: NativeExecutionKind.Pure),
            // MCP (funzioni di lettura, namespace AI).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpServerList, "McpServerList"), [], new StringType(), TokenKind.McpServerList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpServerStatus, "McpServerStatus"), [P("name", false)], new StringType(), TokenKind.McpServerStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpCall, "McpCall"), [P("server", false), P("tool", false), P("args", false)], new StringType(), TokenKind.McpCall, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpListTools, "McpListTools"), [], new StringType(), TokenKind.McpListTools, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpListToolsServer, "McpListToolsServer"), [P("server", false)], new StringType(), TokenKind.McpListToolsServer, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpServerMarketplaceList, "McpServerMarketplaceList"), [], new StringType(), TokenKind.McpServerMarketplaceList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpResources, "McpResources"), [], new StringType(), TokenKind.McpResources, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.McpPrompts, "McpPrompts"), [], new StringType(), TokenKind.McpPrompts, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MemoryAsk, "MemoryAsk"), [P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.MemoryAsk, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MemoryRank, "MemoryRank"), [P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.MemoryRank, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.MemoryConsolidate, "MemoryConsolidate"), [], new DoubleType(), TokenKind.MemoryConsolidate, ExecutionKind: NativeExecutionKind.Pure),
            // Enrich / QA (§2 Second Brain): AI.ENRICH(text, mode), AI.QAANSWER(question, brain [, window]), AI.SUGGESTTAGS(text).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.Enrich, "Enrich"), [P("text", false), P("mode", false)], new StringType(), TokenKind.Enrich, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.QaAnswer, "QaAnswer"), [P("question", false), P("brain", false), new Parameter(-1, T(TokenKind.Identifier, "window"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.QaAnswer, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.SuggestTags, "SuggestTags"), [P("text", false)], new StringType(), TokenKind.SuggestTags, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.SignNote, "SignNote"), [P("content", false), P("key", false)], new StringType(), TokenKind.SignNote, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.VerifyNote, "VerifyNote"), [P("content", false), P("key", false), P("sig", false)], new DoubleType(), TokenKind.VerifyNote, ExecutionKind: NativeExecutionKind.Pure),
            // Share (F7 P2): AI.SHAREENCRYPT(content, key) / AI.SHAREDECRYPT(cipher, key) — AES-256-GCM.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ShareEncrypt, "ShareEncrypt"), [P("content", false), P("key", false)], new StringType(), TokenKind.ShareEncrypt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ShareDecrypt, "ShareDecrypt"), [P("cipher", false), P("key", false)], new StringType(), TokenKind.ShareDecrypt, ExecutionKind: NativeExecutionKind.Pure),
            // BrainVault (§2): AI.BRAINVAULTBUILD(outputDir), AI.BRAINVAULTQUERY(query[,topK]), AI.BRAINVAULTWRITE(request[,topK]).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrainVaultBuild, "BrainVaultBuild"), [P("outputDir", false)], new DoubleType(), TokenKind.BrainVaultBuild, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrainVaultQuery, "BrainVaultQuery"), [P("query", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.BrainVaultQuery, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrainVaultWrite, "BrainVaultWrite"), [P("request", false), new Parameter(-1, T(TokenKind.Identifier, "topK"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.BrainVaultWrite, ExecutionKind: NativeExecutionKind.Pure),
            // Team (TEAM NON è namespace: comandi AI TEAM ... + funzioni qui sotto, tutto in AI).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamList, "TeamList"), [], new StringType(), TokenKind.TeamList, ExecutionKind: NativeExecutionKind.Pure),
            // AI OBSERVE (P1 todo AI-Runtime consolidato): letture del registry ContextObject.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationLatest, "ObservationLatest"), [], new StringType(), TokenKind.AiObservationLatest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationGet, "Observation"), [P("id", false)], new StringType(), TokenKind.AiObservationGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationHash, "ObservationHash"), [P("id", false)], new StringType(), TokenKind.AiObservationHash, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationCount, "ObservationCount"), [], new DoubleType(), TokenKind.AiObservationCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationList, "ObservationList"), [], new StringType(), TokenKind.AiObservationList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiObservationFile, "ObservationFile"), [P("id", false)], new StringType(), TokenKind.AiObservationFile, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamAgents, "TeamAgents"), [P("name", false)], new StringType(), TokenKind.TeamAgents, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamTrainStatus, "TeamTrainStatus"), [P("name", false)], new StringType(), TokenKind.TeamTrainStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamLinkInfo, "TeamLinkInfo"), [P("name", false)], new StringType(), TokenKind.TeamLinkInfo, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamRecursion, "TeamRecursion"), [P("name", false)], new LongType(), TokenKind.TeamRecursion, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamLatent, "TeamLatent"), [P("name", false)], new StringType(), TokenKind.TeamLatent, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamRuntimeStats, "TeamRuntimeStats"), [P("name", false)], new StringType(), TokenKind.TeamRuntimeStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamRunStart, "TeamRunStart"), [P("team", false), P("task", false)], new StringType(), TokenKind.TeamRunStart, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamRunWait, "TeamRunWait"), [P("handle", false)], new StringType(), TokenKind.TeamRunWait, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.TeamOnStream, "TeamOnStream"), [P("sub", false)], new DoubleType(), TokenKind.TeamOnStream, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BlackboardWrite, "BlackboardWrite"), [P("key", false), P("value", false)], new DoubleType(), TokenKind.BlackboardWrite, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BlackboardRead, "BlackboardRead"), [P("key", false)], new StringType(), TokenKind.BlackboardRead, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BlackboardWait, "BlackboardWait"), [P("key", false), new Parameter(-1, T(TokenKind.Identifier, "timeoutMs"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.BlackboardWait, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentSwarm, "AgentSwarm"), [new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), SymbolFlags.None, null, true), P("agentPrefix", false), P("task", false), new Parameter(-1, T(TokenKind.Identifier, "maxConcurrency"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AgentSwarm, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A1): categorie di task → provider/modello (policy di routing).
            // Runtime condiviso Basic.AI.Providers.CategoryRegistry / gmb_category.c.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCategorySet, "CategorySet"), [P("category", false), P("provider", false)], new DoubleType(), TokenKind.AiCategorySet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCategoryFallback, "CategoryFallback"), [P("category", false), P("provider", false)], new DoubleType(), TokenKind.AiCategoryFallback, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCategoryAuto, "CategoryAuto"), [new Parameter(-1, T(TokenKind.Identifier, "flag"), new DoubleType(), SymbolFlags.None, null, false)], new DoubleType(), TokenKind.AiCategoryAuto, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCategoryPick, "CategoryPick"), [P("category", false)], new StringType(), TokenKind.AiCategoryPick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCategoryList, "CategoryList"), [], new StringType(), TokenKind.AiCategoryList, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (B1): Task Graph con dipendenze (Basic.Agents.TaskGraphRegistry / gmb_taskgraph.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskDefine, "TaskDefine"), [P("id", false), P("desc", false)], new DoubleType(), TokenKind.AiTaskDefine, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskDepends, "TaskDepends"), [P("id", false), P("dep", false)], new DoubleType(), TokenKind.AiTaskDepends, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskAssign, "TaskAssign"), [P("id", false), P("agent", false)], new DoubleType(), TokenKind.AiTaskAssign, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskStatus, "TaskStatus"), [P("id", false)], new StringType(), TokenKind.AiTaskStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskList, "TaskList"), [], new StringType(), TokenKind.AiTaskList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskRun, "TaskRun"), [], new StringType(), TokenKind.AiTaskRun, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A2): Planner — piani con approvazione (Basic.Agents.PlanRegistry / gmb_plan.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanDefine, "PlanDefine"), [P("name", false), P("goal", false)], new DoubleType(), TokenKind.AiPlanDefine, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanAdd, "PlanAdd"), [P("name", false), P("taskId", false)], new DoubleType(), TokenKind.AiPlanAdd, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanApprove, "PlanApprove"), [P("name", false)], new DoubleType(), TokenKind.AiPlanApprove, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanStatus, "PlanStatus"), [P("name", false)], new StringType(), TokenKind.AiPlanStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanList, "PlanList"), [], new StringType(), TokenKind.AiPlanList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanRun, "PlanRun"), [P("name", false)], new StringType(), TokenKind.AiPlanRun, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A3): lifecycle hooks (Basic.Agents.HookRegistry / gmb_hook.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHookRegister, "HookRegister"), [P("event", false), P("handler", false)], new DoubleType(), TokenKind.AiHookRegister, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHookUnregister, "HookUnregister"), [P("event", false), P("handler", false)], new DoubleType(), TokenKind.AiHookUnregister, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHookList, "HookList"), [], new StringType(), TokenKind.AiHookList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHookEvents, "HookEvents"), [], new StringType(), TokenKind.AiHookEvents, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHookFire, "HookFire"), [P("event", false), P("payload", true)], new StringType(), TokenKind.AiHookFire, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A6): Artifact Store (Basic.Agents.ArtifactStore / gmb_artifact.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiArtifactPut, "ArtifactPut"), [P("name", false), P("path", false)], new DoubleType(), TokenKind.AiArtifactPut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiArtifactLink, "ArtifactLink"), [P("name", false), P("uri", false)], new DoubleType(), TokenKind.AiArtifactLink, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiArtifactGet, "ArtifactGet"), [P("name", false)], new StringType(), TokenKind.AiArtifactGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiArtifactList, "ArtifactList"), [], new StringType(), TokenKind.AiArtifactList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiArtifactRemove, "ArtifactRemove"), [P("name", false)], new DoubleType(), TokenKind.AiArtifactRemove, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A7): Resource Manager (Basic.Agents.ResourceRegistry / gmb_resource.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResourceSet, "ResourceSet"), [P("key", false), P("value", false)], new DoubleType(), TokenKind.AiResourceSet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResourceGet, "ResourceGet"), [P("key", false)], new StringType(), TokenKind.AiResourceGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResourceList, "ResourceList"), [], new StringType(), TokenKind.AiResourceList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResourceAvailable, "ResourceAvailable"), [P("key", false)], new DoubleType(), TokenKind.AiResourceAvailable, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResourceSwarmSize, "ResourceSwarmSize"), [new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), SymbolFlags.None, null, false)], new DoubleType(), TokenKind.AiResourceSwarmSize, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A9): Handoff (Basic.Agents.HandoffRegistry / gmb_handoff.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHandoff, "Handoff"), [P("taskId", false), P("toAgent", false)], new DoubleType(), TokenKind.AiHandoff, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHandoffComplete, "HandoffComplete"), [P("taskId", false), P("result", false)], new DoubleType(), TokenKind.AiHandoffComplete, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHandoffStatus, "HandoffStatus"), [P("taskId", false)], new StringType(), TokenKind.AiHandoffStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHandoffList, "HandoffList"), [], new StringType(), TokenKind.AiHandoffList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHandoffClear, "HandoffClear"), [], new DoubleType(), TokenKind.AiHandoffClear, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A5): Rules injection gerarchica (Basic.Agents.RuleRegistry / gmb_rules.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRulesAdd, "RulesAdd"), [P("scope", false), P("content", false)], new DoubleType(), TokenKind.AiRulesAdd, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRulesFor, "RulesFor"), [P("path", false)], new StringType(), TokenKind.AiRulesFor, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRulesList, "RulesList"), [], new StringType(), TokenKind.AiRulesList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRulesRemove, "RulesRemove"), [P("scope", false)], new DoubleType(), TokenKind.AiRulesRemove, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiRulesClear, "RulesClear"), [], new DoubleType(), TokenKind.AiRulesClear, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A4): Goal + idle continuation (Basic.Agents.GoalRegistry / gmb_goal.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalSet, "GoalSet"), [P("objective", false)], new DoubleType(), TokenKind.AiGoalSet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalGet, "GoalGet"), [], new StringType(), TokenKind.AiGoalGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalStatus, "GoalStatus"), [], new StringType(), TokenKind.AiGoalStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalTick, "GoalTick"), [], new DoubleType(), TokenKind.AiGoalTick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalClear, "GoalClear"), [], new DoubleType(), TokenKind.AiGoalClear, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A8): Session Replay (Basic.Agents.ReplayRegistry / gmb_replay.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayStart, "ReplayStart"), [P("sessionId", false)], new DoubleType(), TokenKind.AiReplayStart, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayRecord, "ReplayRecord"), [P("event", false)], new DoubleType(), TokenKind.AiReplayRecord, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayStop, "ReplayStop"), [], new DoubleType(), TokenKind.AiReplayStop, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayGet, "ReplayGet"), [P("sessionId", false)], new StringType(), TokenKind.AiReplayGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayList, "ReplayList"), [], new StringType(), TokenKind.AiReplayList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayClear, "ReplayClear"), [], new DoubleType(), TokenKind.AiReplayClear, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiReplayRun, "ReplayRun"), [P("sessionId", false), P("targetId", false)], new DoubleType(), TokenKind.AiReplayRun, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiGoalAgent, "GoalAgent"), [P("name", false)], new DoubleType(), TokenKind.AiGoalAgent, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiAskChain, "AskChain"), [P("name", false), P("prompt", false), P("prefix", true)], new StringType(), TokenKind.AiAskChain, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (B6): metriche fleet-level (Basic.Agents.MetricsRegistry / gmb_metrics.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiMetricRecord, "MetricRecord"), [P("agent", false), P("key", false), new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), SymbolFlags.None, null, false)], new DoubleType(), TokenKind.AiMetricRecord, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiMetricGet, "MetricGet"), [P("agent", false), P("key", false)], new DoubleType(), TokenKind.AiMetricGet, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiMetricList, "MetricList"), [], new StringType(), TokenKind.AiMetricList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiFleetStats, "FleetStats"), [], new StringType(), TokenKind.AiFleetStats, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiMetricReset, "MetricReset"), [], new DoubleType(), TokenKind.AiMetricReset, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (A10): Hashline (Basic.Agents.HashlineTool / gmb_hashline.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHashLine, "HashLine"), [P("content", false)], new StringType(), TokenKind.AiHashLine, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHashEdit, "HashEdit"), [P("content", false), P("lineRef", false), P("newText", false)], new StringType(), TokenKind.AiHashEdit, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiHashVerify, "HashVerify"), [P("content", false), P("lineRef", false)], new DoubleType(), TokenKind.AiHashVerify, ExecutionKind: NativeExecutionKind.Pure),
            // Orchestrazione agenti (B7): checkpoint/recovery (Basic.Agents.CheckpointRegistry / gmb_checkpoint.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCheckpoint, "Checkpoint"), [P("sessionId", false), P("state", false)], new DoubleType(), TokenKind.AiCheckpoint, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiResume, "Resume"), [P("sessionId", false)], new StringType(), TokenKind.AiResume, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCheckpointList, "CheckpointList"), [], new StringType(), TokenKind.AiCheckpointList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiCheckpointClear, "CheckpointClear"), [], new DoubleType(), TokenKind.AiCheckpointClear, ExecutionKind: NativeExecutionKind.Pure),
            // Persistenza orchestrazione su disco (Basic.Agents.OrchestrationPersistence / gmb_orchestration.c).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiOrchSave, "OrchSave"), [P("path", true)], new DoubleType(), TokenKind.AiOrchSave, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiOrchLoad, "OrchLoad"), [P("path", true)], new DoubleType(), TokenKind.AiOrchLoad, ExecutionKind: NativeExecutionKind.Pure),
            // Reset dei registri (determinismo dei MiniTest).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskClear, "TaskClear"), [], new DoubleType(), TokenKind.AiTaskClear, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiPlanClear, "PlanClear"), [], new DoubleType(), TokenKind.AiPlanClear, ExecutionKind: NativeExecutionKind.Pure),
            // Workflow (ex namespace WORKFLOW fuso in AI 2026-08-28: funzioni sotto AI;
            // i comandi sono AI WORKFLOW ... / AI WORKFLOW STEP|EXECUTE).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WorkflowList, "WorkflowList"), [], new StringType(), TokenKind.WorkflowList, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WorkflowStatus, "WorkflowStatus"), [P("name", false)], new StringType(), TokenKind.WorkflowStatus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WorkflowSteps, "WorkflowSteps"), [P("name", false)], new StringType(), TokenKind.WorkflowSteps, ExecutionKind: NativeExecutionKind.Pure),
            // CUA — loop Brain→Azione NATIVO (modello locale embedding GMBasic): AI.AGENTRUN(name, goal [, rounds]).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentRunCua, "AgentRun"), [P("name", false), P("goal", false), new Parameter(-1, T(TokenKind.Identifier, "rounds"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AgentRunCua, ExecutionKind: NativeExecutionKind.Pure),
            // Graph — l'agente esplora il knowledge graph Second Brain con i tool graph.*:
            // AI.AGENTRUNGRAPH(name, prompt [, rounds]).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentRunGraph, "AgentRunGraph"), [P("name", false), P("prompt", false), new Parameter(-1, T(TokenKind.Identifier, "rounds"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AgentRunGraph, ExecutionKind: NativeExecutionKind.Pure),
            // RAG — l'agente interroga i brain RAG Second Brain con i tool rag.*:
            // AI.AGENTRUNRAG(name, prompt [, rounds]).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentRunRag, "AgentRunRag"), [P("name", false), P("prompt", false), new Parameter(-1, T(TokenKind.Identifier, "rounds"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AgentRunRag, ExecutionKind: NativeExecutionKind.Pure),
            // CODE — l'agente interroga il grafo semantico del CODICE con i tool code.*:
            // AI.AGENTRUNCODE(name, prompt [, rounds]). Loop C# indipendente (SemanticAgentLoop).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentRunCode, "AgentRunCode"), [P("name", false), P("prompt", false), new Parameter(-1, T(TokenKind.Identifier, "rounds"), new DoubleType(), SymbolFlags.IsOptional, null, true)], new StringType(), TokenKind.AgentRunCode, ExecutionKind: NativeExecutionKind.Pure),
            // Self-Consistency / Semantic Entropy (Spanda R_sc) — 2026-09-14.
            // AI.CONSISTENCY(prompt, opts) → risk 0..1 (sampling K× via AskMulti, GPM park-based).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiSelfConsistency, "Consistency"), [P("prompt", false), P("options", true)], new DoubleType(), TokenKind.AiSelfConsistency, ExecutionKind: NativeExecutionKind.Pure),
            // AI.LEXICALENTROPY(responses$) → entropy 0..1 (deterministic, ~µs, pure C runtime).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiLexicalEntropy, "LexicalEntropy"), [P("responses", false)], new DoubleType(), TokenKind.AiLexicalEntropy, ExecutionKind: NativeExecutionKind.Pure),
            // AI.CONSISTENCYDETAIL$() → JSON detail report (k, clusters, entropy, risk, collapse_warning).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiConsistencyDetail, "ConsistencyDetail"), [], new StringType(), TokenKind.AiConsistencyDetail, ExecutionKind: NativeExecutionKind.Pure),
            // P2 (2026-09-15): AI CONTEXT engine — letture (AI.CONTEXT*).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextUsed, "ContextUsed"), [P("agent", false)], new DoubleType(), TokenKind.AiContextUsed, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextBudget, "ContextBudget"), [P("agent", false)], new DoubleType(), TokenKind.AiContextBudget, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextOverflow, "ContextOverflow"), [P("agent", false)], new DoubleType(), TokenKind.AiContextOverflow, ExecutionKind: NativeExecutionKind.Pure),
            // §3.6 PARSEC — cache/version-hash (context-cache ↔ prefix KV).
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextHash, "ContextHash"), [P("agent", false)], new StringType(), TokenKind.AiContextHash, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextValid, "ContextValid"), [P("agent", false), P("revision", false)], new DoubleType(), TokenKind.AiContextValid, ExecutionKind: NativeExecutionKind.Pure),
            // §3.6 PARSEC — metriche TASK (TOKENS/TTFT/TOOL CALLS/LLM CALLS/LATENCY) come JSON.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiTaskMetrics, "TaskMetrics"), [P("task", false)], new StringType(), TokenKind.AiTaskMetrics, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextView, "ContextView"), [P("agent", false)], new StringType(), TokenKind.AiContextView, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiContextViewDebug, "ContextViewDebug"), [P("agent", false)], new StringType(), TokenKind.AiContextViewDebug, ExecutionKind: NativeExecutionKind.Pure),
            // P3 (2026-09-15): AI WORKER + AI STATE — WorkerRuntime registrazioni.
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiWorkerRounds, "WorkerRounds"), [], new DoubleType(), TokenKind.AiWorkerRounds, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiWorkerRescued, "WorkerRescued"), [], new DoubleType(), TokenKind.AiWorkerRescued, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiWorkerSpend, "WorkerSpend"), [], new DoubleType(), TokenKind.AiWorkerSpend, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AiStateLoad, "StateLoad"), [P("worker", false), P("key", false)], new StringType(), TokenKind.AiStateLoad, ExecutionKind: NativeExecutionKind.Pure),
            // §4.10 SEMAPRAX — GMB Semantic Graph: SPOSTATO nel namespace LLM
            // (LLM.SYMBOL/IMPACT/CALLMAP/REVISION/CONTEXT/CHECKREV/PATCH/FINDSYMBOL/REPOMAP).
            // La superficie è una feature di COMPILATORE (lato programma) → asse LLM.
            // Vedi StdLib.LLM.cs (CreateLlmNamespace).
        };

        RegisterBuiltinNamespace(ns, procedures, new Dictionary<string, DataType>());
    }
}
