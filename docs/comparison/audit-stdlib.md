# GMBASIC — Audit StdLib per namespace (comandi, funzioni, costanti, tipi)

_Generato da `tools/basic/recon_stdlib.bas` dal codice `common/StdLib/StdLib.*.cs`._
**Chiave:** (NAMESPACE, NOME). I nomi polimorfici sono elencati in OGNI namespace che li espone.

## Namespace AGENT

### Comandi (0)

| Comando |
|---|---|

### Funzioni (1)

| Funzione | Ritorno |
|---|---|
| `Run` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace AI

### Comandi (1)

| Comando |
|---|---|
| `CONNECTOR` |

### Funzioni (272)

| Funzione | Ritorno |
|---|---|
| `Active` | StringType |
| `AgentAskFile` | StringType |
| `AgentAskSync` | StringType |
| `AgentCode` | StringType |
| `AgentEvents` | ArrayType |
| `AgentFix` | StringType |
| `AgentGet` | StringType |
| `AgentList` | ArrayType |
| `AgentPause` | DoubleType |
| `AgentQueueClear` | DoubleType |
| `AgentQueueStats` | StringType |
| `AgentReceive` | StringType |
| `AgentResume` | DoubleType |
| `AgentRun` | StringType |
| `AgentRunCode` | StringType |
| `AgentRunGraph` | StringType |
| `AgentRunRag` | StringType |
| `AgentSend` | DoubleType |
| `AgentStatus` | StringType |
| `AgentStop` | DoubleType |
| `AgentSwarm` | StringType |
| `AnalyzeImage` | StringType |
| `ArtifactGet` | StringType |
| `ArtifactLink` | DoubleType |
| `ArtifactList` | StringType |
| `ArtifactPut` | DoubleType |
| `ArtifactRemove` | DoubleType |
| `Ask` | StringType |
| `AskChain` | StringType |
| `AskMulti` | StringType |
| `Backlinks` | StringType |
| `BlackboardRead` | StringType |
| `BlackboardWait` | StringType |
| `BlackboardWrite` | DoubleType |
| `BrainDetach` | DoubleType |
| `BrainInfo` | StringType |
| `BrainList` | ArrayType |
| `BrainUse` | LongType |
| `BrainUsed` | StringType |
| `BrainVaultBuild` | DoubleType |
| `BrainVaultQuery` | StringType |
| `BrainVaultWrite` | StringType |
| `CONNECTORCALL` | StringType |
| `CONNECTORLIST` | StringType |
| `CONNECTORMARKETPLACEGET` | StringType |
| `CONNECTORMARKETPLACELIST` | StringType |
| `CONNECTORPOLICYGET` | StringType |
| `CONNECTORPROMPTS` | StringType |
| `CONNECTORRESOURCES` | StringType |
| `CONNECTORSEARCH` | StringType |
| `CONNECTORSECRETGET` | StringType |
| `CONNECTORSTATUS` | StringType |
| `CONNECTORTOOLS` | StringType |
| `CategoryAuto` | DoubleType |
| `CategoryFallback` | DoubleType |
| `CategoryList` | StringType |
| `CategoryPick` | StringType |
| `CategorySet` | DoubleType |
| `ChannelCreate` | DoubleType |
| `ChannelPublish` | DoubleType |
| `ChannelSubscribe` | DoubleType |
| `ChannelUnsubscribe` | DoubleType |
| `Checkpoint` | DoubleType |
| `CheckpointClear` | DoubleType |
| `CheckpointList` | StringType |
| `Configure` | DoubleType |
| `Consistency` | DoubleType |
| `ConsistencyDetail` | StringType |
| `ContextBudget` | DoubleType |
| `ContextHash` | StringType |
| `ContextOverflow` | DoubleType |
| `ContextUsed` | DoubleType |
| `ContextValid` | DoubleType |
| `ContextView` | StringType |
| `ContextViewDebug` | StringType |
| `Count` | LongType |
| `Credits` | IntegerType |
| `Discover` | ArrayType |
| `DocExtract` | StringType |
| `DocFields` | StringType |
| `Embed` | ArrayType |
| `Enrich` | StringType |
| `EnsureCollection` | LongType |
| `Fail` | DoubleType |
| `FleetStats` | StringType |
| `GenTest` | StringType |
| `GetBase` | StringType |
| `GetDefault` | StringType |
| `GetKey` | StringType |
| `GetModel` | StringType |
| `GetTemp` | StringType |
| `GoalAgent` | DoubleType |
| `GoalClear` | DoubleType |
| `GoalGet` | StringType |
| `GoalSet` | DoubleType |
| `GoalStatus` | StringType |
| `GoalTick` | DoubleType |
| `Handoff` | DoubleType |
| `HandoffClear` | DoubleType |
| `HandoffComplete` | DoubleType |
| `HandoffList` | StringType |
| `HandoffStatus` | StringType |
| `HashEdit` | StringType |
| `HashLine` | StringType |
| `HashVerify` | DoubleType |
| `HealthCheckInterval` | IntegerType |
| `HfRepo` | StringType |
| `HookEvents` | StringType |
| `HookFire` | StringType |
| `HookList` | StringType |
| `HookRegister` | DoubleType |
| `HookUnregister` | DoubleType |
| `ImageClassify` | StringType |
| `ImageCompare` | LongType |
| `ImageConvert` | LongType |
| `ImageCrop` | LongType |
| `ImageDetect` | StringType |
| `ImageEmbed` | ArrayType |
| `ImageInfo` | StringType |
| `ImageResize` | LongType |
| `IndexMeta` | LongType |
| `IndexText` | LongType |
| `Info` | StringType |
| `IngestWeb` | StringType |
| `KgAddEdge` | DoubleType |
| `KgAddNode` | DoubleType |
| `KgQuery` | StringType |
| `LexicalEntropy` | DoubleType |
| `LinkDangling` | LongType |
| `LinkJson` | StringType |
| `LinksOut` | StringType |
| `List` | ArrayType |
| `ListCollections` | StringType |
| `Load` | DoubleType |
| `LoadDocuments` | LongType |
| `McpCall` | StringType |
| `McpListTools` | StringType |
| `McpListToolsServer` | StringType |
| `McpPrompts` | StringType |
| `McpResources` | StringType |
| `McpServerList` | StringType |
| `McpServerMarketplaceList` | StringType |
| `McpServerStatus` | StringType |
| `MemoryAsk` | StringType |
| `MemoryConsolidate` | DoubleType |
| `MemoryRank` | StringType |
| `MetricGet` | DoubleType |
| `MetricList` | StringType |
| `MetricRecord` | DoubleType |
| `MetricReset` | DoubleType |
| `NoteExists` | DoubleType |
| `NoteFolder` | StringType |
| `NoteGet` | StringType |
| `NoteLink` | LongType |
| `NoteSetAlias` | LongType |
| `NoteTags` | StringType |
| `NoteTitle` | StringType |
| `NoteUnlink` | LongType |
| `NoteWordCount` | LongType |
| `OCR` | StringType |
| `Observation` | StringType |
| `ObservationCount` | DoubleType |
| `ObservationFile` | StringType |
| `ObservationHash` | StringType |
| `ObservationLatest` | StringType |
| `ObservationList` | StringType |
| `Option` | StringType |
| `OrchLoad` | DoubleType |
| `OrchSave` | DoubleType |
| `PlanAdd` | DoubleType |
| `PlanApprove` | DoubleType |
| `PlanClear` | DoubleType |
| `PlanDefine` | DoubleType |
| `PlanList` | StringType |
| `PlanRun` | StringType |
| `PlanStatus` | StringType |
| `PluginCall` | StringType |
| `PluginList` | ArrayType |
| `PluginsInfo` | StringType |
| `PluginsList` | ArrayType |
| `Priority` | IntegerType |
| `Providers` | StringType |
| `PurgeDeprecated` | DoubleType |
| `QaAnswer` | StringType |
| `RagContext` | StringType |
| `RagContextMulti` | StringType |
| `Register` | DoubleType |
| `RemoveSource` | DoubleType |
| `ReplayClear` | DoubleType |
| `ReplayGet` | StringType |
| `ReplayList` | StringType |
| `ReplayRecord` | DoubleType |
| `ReplayRun` | DoubleType |
| `ReplayStart` | DoubleType |
| `ReplayStop` | DoubleType |
| `Rerank` | StringType |
| `ResetFailed` | DoubleType |
| `ResourceAvailable` | DoubleType |
| `ResourceGet` | StringType |
| `ResourceList` | StringType |
| `ResourceSet` | DoubleType |
| `ResourceSwarmSize` | DoubleType |
| `Resume` | StringType |
| `Rotate` | StringType |
| `RulesAdd` | DoubleType |
| `RulesClear` | DoubleType |
| `RulesFor` | StringType |
| `RulesList` | StringType |
| `RulesRemove` | DoubleType |
| `RunHealthCheck` | DoubleType |
| `Save` | DoubleType |
| `Search` | StringType |
| `SearchMulti` | StringType |
| `SetActive` | DoubleType |
| `SetCredits` | DoubleType |
| `SetPriority` | DoubleType |
| `SetStatus` | DoubleType |
| `SetWeight` | DoubleType |
| `ShareDecrypt` | StringType |
| `ShareEncrypt` | StringType |
| `SignNote` | StringType |
| `SkillsList` | ArrayType |
| `SkillsPaths` | ArrayType |
| `SkillsPrompt` | StringType |
| `StartHealthCheck` | DoubleType |
| `StateLoad` | StringType |
| `Status` | StringType |
| `StopHealthCheck` | DoubleType |
| `SuggestTags` | StringType |
| `SyncFolder` | StringType |
| `Synthesize` | LongType |
| `TaskAssign` | DoubleType |
| `TaskClear` | DoubleType |
| `TaskDefine` | DoubleType |
| `TaskDepends` | DoubleType |
| `TaskList` | StringType |
| `TaskMetrics` | StringType |
| `TaskRun` | StringType |
| `TaskStatus` | StringType |
| `TeamAgents` | StringType |
| `TeamLatent` | StringType |
| `TeamLinkInfo` | StringType |
| `TeamList` | StringType |
| `TeamOnStream` | DoubleType |
| `TeamRecursion` | LongType |
| `TeamRunStart` | StringType |
| `TeamRunWait` | StringType |
| `TeamRuntimeStats` | StringType |
| `TeamTrainStatus` | StringType |
| `Test` | StringType |
| `Transcribe` | StringType |
| `Unregister` | DoubleType |
| `VaultAddFolder` | LongType |
| `VaultCount` | LongType |
| `VaultFolders` | StringType |
| `VaultInfo` | StringType |
| `VaultList` | ArrayType |
| `VaultPolicy` | StringType |
| `VaultRemoveFolder` | LongType |
| `VaultSetFolderProps` | LongType |
| `VaultWatch` | LongType |
| `VerifyNote` | DoubleType |
| `VideoFrame` | LongType |
| `VideoInfo` | StringType |
| `VideoSnapshot` | LongType |
| `Weight` | IntegerType |
| `WorkerRescued` | DoubleType |
| `WorkerRounds` | DoubleType |
| `WorkerSpend` | DoubleType |
| `WorkflowList` | StringType |
| `WorkflowStatus` | StringType |
| `WorkflowSteps` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace ANALYSIS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (44)

| Funzione | Ritorno |
|---|---|
| `AdfTest` | AdfTestResultType |
| `Anova` | AnovaResultType |
| `Arima` | ArimaResultType |
| `Correlation` | DoubleType |
| `CubicSpline` | DoubleType |
| `CumTrapezoid` | ArrayType |
| `Differentiate` | DoubleType |
| `EuclideanDistanceSquared` | DoubleType |
| `Fft` | ArrayType |
| `FftComplex` | ArrayType |
| `FftFreq` | ArrayType |
| `FftShift` | ArrayType |
| `HoltWinters` | HoltWintersResultType |
| `Ifft` | ArrayType |
| `IfftComplex` | ArrayType |
| `IfftShift` | ArrayType |
| `Integrate` | DoubleType |
| `InterpolateClamp` | DoubleType |
| `InterpolateGeneric` | DoubleType |
| `InterpolateLinear` | DoubleType |
| `InterpolatePchip` | DoubleType |
| `InterpolatePolynomial` | DoubleType |
| `InterpolateSpline` | DoubleType |
| `InterpolateStep` | DoubleType |
| `Irfft` | ArrayType |
| `KMeans` | KMeansResultType |
| `Kurtosis` | DoubleType |
| `MaCoefficients` | ArrayType |
| `Mad` | DoubleType |
| `Mean` | DoubleType |
| `Median` | DoubleType |
| `Pca` | PcaResultType |
| `Regress` | RegressionResultType |
| `Rfft` | LongType |
| `Simpson` | DoubleType |
| `Skewness` | DoubleType |
| `SolveIvp` | LongType |
| `SolveOde` | ArrayType |
| `Splev` | ArrayType |
| `Splrep` | LongType |
| `StdDev` | DoubleType |
| `Trapezoid` | DoubleType |
| `Variance` | DoubleType |
| `YuleWalker` | ArrayType |

### Tipi (7)

| Tipo |
|---|---|
| `AdfTestResultType` |
| `AnovaResultType` |
| `ArimaResultType` |
| `HoltWintersResultType` |
| `KMeansResultType` |
| `PcaResultType` |
| `RegressionResultType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace ARRAYS

### Comandi (8)

| Comando |
|---|---|
| `AADD` |
| `ACOPY` |
| `ADEL` |
| `AEVAL` |
| `AFILL` |
| `AINS` |
| `ASHUFFLE` |
| `ASORT` |

### Funzioni (26)

| Funzione | Ritorno |
|---|---|
| `AADD` | ArrayType |
| `ABINARYSEARCH` | LongType |
| `ACHUNK` | ArrayType |
| `ACLONE` | ArrayType |
| `ACOPY` | ArrayType |
| `ACOUNT` | LongType |
| `ADEL` | ArrayType |
| `AEVAL` | ArrayType |
| `AFILL` | ArrayType |
| `AFILTER` | ArrayType |
| `AFLAT` | ArrayType |
| `AINS` | ArrayType |
| `AJOIN` | StringType |
| `AMAP` | ArrayType |
| `AMAX` | DoubleType |
| `AMIN` | DoubleType |
| `APAD` | ArrayType |
| `AREDUCE` | InferredType |
| `AREVERSE` | ArrayType |
| `ASCAN` | LongType |
| `ASHUFFLE` | ArrayType |
| `ASORT` | ArrayType |
| `ASUM` | DoubleType |
| `ATOKENS` | ArrayType |
| `AUNIQUE` | ArrayType |
| `AZIP` | ArrayType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace ASR

### Comandi (0)

| Comando |
|---|---|

### Funzioni (1)

| Funzione | Ritorno |
|---|---|
| `RECOGNIZE` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace ATOMIC

### Comandi (0)

| Comando |
|---|---|

### Funzioni (5)

| Funzione | Ritorno |
|---|---|
| `COMPARESWAP` | ? |
| `FETCHVALUE` | ? |
| `INCREMENT` | ? |
| `STORE` | ? |
| `SWAP` | ? |

### Tipi (1)

| Tipo |
|---|---|
| `ATOMIC` |

### Costanti (0)

| Costante |
|---|---|

## Namespace AUTH

### Comandi (0)

| Comando |
|---|---|

### Funzioni (3)

| Funzione | Ritorno |
|---|---|
| `JWTCREATE` | StringType |
| `JWTPAYLOAD` | StringType |
| `JWTVERIFY` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace BITS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (21)

| Funzione | Ritorno |
|---|---|
| `BITCLEAR` | LongType |
| `BITCOUNT` | LongType |
| `BITDEINTERLEAVE` | LongType |
| `BITFIELD` | LongType |
| `BITGET` | LongType |
| `BITINTERLEAVE` | LongType |
| `BITREVERSE` | LongType |
| `BITROTATEL` | LongType |
| `BITROTATER` | LongType |
| `BITSCANFWD` | LongType |
| `BITSCANREV` | LongType |
| `BITSET` | LongType |
| `BITTEST` | BitType |
| `BITTOGGLE` | LongType |
| `BYTESWAP` | LongType |
| `FROMGRAY` | LongType |
| `GRAYCODE` | LongType |
| `HAMMINGDIST` | LongType |
| `ISPOW2` | BitType |
| `NEXTPOW2` | LongType |
| `PARITY` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace BLOCKCHAIN

### Comandi (0)

| Comando |
|---|---|

### Funzioni (24)

| Funzione | Ritorno |
|---|---|
| `ABIDECODE` | ArrayType |
| `ABIENCODE` | StringType |
| `BASE32DECODE` | ? |
| `BASE32ENCODE` | StringType |
| `BASE58DECODE` | ? |
| `BASE58ENCODE` | StringType |
| `BLAKE2B256` | ? |
| `BLOCKDIFFICULTY` | DoubleType |
| `BTCADDRFROMPUB` | StringType |
| `CONTRACT` | InferredType |
| `DERIVEKEY` | DictType |
| `ETHADDRCHECKSUM` | StringType |
| `ETHADDRFROMPUB` | StringType |
| `GENERATEMNEMONIC` | StringType |
| `GETPUBLICKEY` | ? |
| `KECCAK256` | ? |
| `MNEMONICTOSEED` | ? |
| `RIPEMD160` | ? |
| `RLPENCODE` | ? |
| `SECP256K1SIGN` | ? |
| `STRKECCAK256` | StringType |
| `STRRIPEMD160` | StringType |
| `VARINTDECODE` | LongType |
| `VARINTENCODE` | ? |

### Tipi (1)

| Tipo |
|---|---|
| `IERC20` |

### Costanti (0)

| Costante |
|---|---|

## Namespace BROWSER

### Comandi (0)

| Comando |
|---|---|

### Funzioni (21)

| Funzione | Ritorno |
|---|---|
| `AccessTree` | StringType |
| `Auth` | StringType |
| `ClearCookies` | StringType |
| `Click` | StringType |
| `Console` | StringType |
| `Evaluate` | StringType |
| `Fill` | StringType |
| `Find` | StringType |
| `GetCookies` | StringType |
| `HumanClick` | StringType |
| `HumanFill` | StringType |
| `HumanScroll` | StringType |
| `Navigate` | StringType |
| `NetLog` | StringType |
| `Observe` | StringType |
| `Screenshot` | StringType |
| `SetCookie` | StringType |
| `SetHeader` | StringType |
| `SetUserAgent` | StringType |
| `Snapshot` | StringType |
| `Start` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace CHART

### Comandi (6)

| Comando |
|---|---|
| `CLEAR` |
| `CLOSE` |
| `FINANCIAL` |
| `PLOT` |
| `SAVE` |
| `SHOW` |

### Funzioni (1)

| Funzione | Ritorno |
|---|---|
| `CREATE` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace COLLECTIONS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (0)

| Funzione | Ritorno |
|---|---|

### Tipi (30)

| Tipo |
|---|---|
| `AVLTreeType` |
| `BITREE` |
| `BPlusTreeType` |
| `BagType` |
| `BinaryHeapType` |
| `BinaryIndexedTreeType` |
| `CIRCULAR` |
| `CacheType` |
| `CircularLinkedListType` |
| `DictType` |
| `DoublyLinkedListType` |
| `GraphType` |
| `HEAP` |
| `HashRingType` |
| `HashSetType` |
| `KeyValuePairType` |
| `LinkedListType` |
| `ListType` |
| `MAXHEAP` |
| `QueueType` |
| `REDBLACK` |
| `RedBlackTreeType` |
| `RingBufferType` |
| `SORTED` |
| `SortedDictType` |
| `SparseSetType` |
| `StackType` |
| `TimelineType` |
| `TreeType` |
| `TrieType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace COMPRESSION

### Comandi (2)

| Comando |
|---|---|
| `UNZIP` |
| `ZIP` |

### Funzioni (21)

| Funzione | Ritorno |
|---|---|
| `ALGORITHMS` | StringType |
| `BROTLI` | StringType |
| `BZIP2` | StringType |
| `COMPRESS` | StringType |
| `DECOMPRESS` | StringType |
| `DEFLATE` | StringType |
| `GZIP` | StringType |
| `LZ4` | StringType |
| `UNBROTLI` | StringType |
| `UNBZIP2` | StringType |
| `UNDEFLATE` | StringType |
| `UNGZIP` | StringType |
| `UNLZ4` | StringType |
| `UNXZ` | StringType |
| `UNZIP` | BitType |
| `UNZLIB` | StringType |
| `UNZSTD` | StringType |
| `XZ` | StringType |
| `ZIP` | BitType |
| `ZLIB` | StringType |
| `ZSTD` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace COMPUTER

### Comandi (0)

| Comando |
|---|---|

### Funzioni (12)

| Funzione | Ritorno |
|---|---|
| `Click` | DoubleType |
| `DoubleClick` | DoubleType |
| `HotKey` | DoubleType |
| `Key` | DoubleType |
| `Move` | DoubleType |
| `ScreenRecord` | StringType |
| `ScreenRecordStart` | StringType |
| `ScreenRecordStatus` | StringType |
| `ScreenRecordStop` | StringType |
| `Screenshot` | StringType |
| `Type` | DoubleType |
| `Windows` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace CONCURRENCY

### Comandi (11)

| Comando |
|---|---|
| `CHANNEL` |
| `COND` |
| `DEFER` |
| `DONE` |
| `GO` |
| `MUTEX` |
| `ONCE` |
| `RECEIVE` |
| `SEND` |
| `SLEEP` |
| `WAIT` |

### Funzioni (3)

| Funzione | Ritorno |
|---|---|
| `SleepAsync` | ? |
| `TRYLOCK` | BitType |
| `TaskYield` | ? |

### Tipi (3)

| Tipo |
|---|---|
| `CHANNEL` |
| `ONCE` |
| `WAITGROUP` |

### Costanti (0)

| Costante |
|---|---|

## Namespace CONSOLE

### Comandi (18)

| Comando |
|---|---|
| `CAPTION` |
| `CLIP` |
| `CLS` |
| `COLOR` |
| `CURSOR` |
| `DOEVENTS` |
| `GET` |
| `INKEY` |
| `INPUT` |
| `LINE INPUT` |
| `LOCATE` |
| `PRINT` |
| `RESTORESCREEN` |
| `SAVESCREEN` |
| `SCROLLMODE` |
| `SENDKEY` |
| `SET` |
| `WAITKEY` |

### Funzioni (14)

| Funzione | Ritorno |
|---|---|
| `ALERT` | LongType |
| `BGRGB` | StringType |
| `CELL` | LongType |
| `CELLSTR` | StringType |
| `CONFIRM` | LongType |
| `FGRGB` | StringType |
| `GET` | StringType |
| `GETCURSORPOS` | ArrayType |
| `GETPALETTECOLOR` | ArrayType |
| `GETTEXTMETRICS` | ArrayType |
| `INKEY` | StringType |
| `MAKE` | LongType |
| `MESSAGE` | StringType |
| `Point` | DoubleType |

### Tipi (1)

| Tipo |
|---|---|
| `KEY` |

### Costanti (0)

| Costante |
|---|---|

## Namespace CONSTANTS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (0)

| Funzione | Ritorno |
|---|---|

### Tipi (0)

| Tipo |
|---|---|

### Costanti (284)

| Costante |
|---|---|
| `ABORT` |
| `ABORTRETRYIGNORE` |
| `ANSI` |
| `AQUA` |
| `AQUAMARINE1` |
| `AQUAMARINE1_2` |
| `AQUAMARINE3` |
| `AUDIO` |
| `AUTO` |
| `BLACK` |
| `BLUE` |
| `BLUE1` |
| `BLUE3` |
| `BLUE3_2` |
| `BLUE_VIOLET` |
| `C64` |
| `CADET_BLUE` |
| `CADET_BLUE_2` |
| `CANCEL` |
| `CENTER` |
| `CHARTREUSE1` |
| `CHARTREUSE2` |
| `CHARTREUSE2_2` |
| `CHARTREUSE3` |
| `CHARTREUSE3_2` |
| `CHARTREUSE4` |
| `CHAT` |
| `CHCLOSED` |
| `CHROMIUM` |
| `CLOSED` |
| `CORNFLOWER_BLUE` |
| `CYAN1` |
| `CYAN2` |
| `CYAN3` |
| `DARK_BLUE` |
| `DARK_CYAN` |
| `DARK_GREEN` |
| `DARK_MAGENTA` |
| `DARK_MAGENTA_2` |
| `DARK_OLIVE_GREEN3` |
| `DARK_OLIVE_GREEN3_2` |
| `DARK_RED` |
| `DARK_RED_2` |
| `DARK_SEA_GREEN` |
| `DARK_SEA_GREEN3` |
| `DARK_SEA_GREEN4` |
| `DARK_SEA_GREEN4_2` |
| `DARK_SLATE_GRAY1` |
| `DARK_SLATE_GRAY2` |
| `DARK_SLATE_GRAY3` |
| `DARK_TURQUOISE` |
| `DARK_VIOLET` |
| `DEEP_PINK4` |
| `DEEP_PINK4_2` |
| `DEEP_SKY_BLUE1` |
| `DEEP_SKY_BLUE2` |
| `DEEP_SKY_BLUE3` |
| `DEEP_SKY_BLUE3_2` |
| `DEEP_SKY_BLUE4` |
| `DEEP_SKY_BLUE4_2` |
| `DEEP_SKY_BLUE4_3` |
| `DODGER_BLUE1` |
| `DODGER_BLUE2` |
| `DODGER_BLUE3` |
| `DOUBLE` |
| `EMBEDDING` |
| `FAST` |
| `FILE_CHANGED` |
| `FILE_CREATED` |
| `FILE_DELETED` |
| `FILE_RENAMED` |
| `FUCHSIA` |
| `GOVERNED` |
| `GRAY` |
| `GRAY0` |
| `GRAY11` |
| `GRAY15` |
| `GRAY19` |
| `GRAY2` |
| `GRAY23` |
| `GRAY27` |
| `GRAY3` |
| `GRAY30` |
| `GRAY35` |
| `GRAY37` |
| `GRAY39` |
| `GRAY42` |
| `GRAY46` |
| `GRAY50` |
| `GRAY53` |
| `GRAY54` |
| `GRAY58` |
| `GRAY62` |
| `GRAY66` |
| `GRAY7` |
| `GRAY70` |
| `GRAY74` |
| `GRAY78` |
| `GRAY82` |
| `GRAY85` |
| `GRAY89` |
| `GREEN` |
| `GREEN1` |
| `GREEN3` |
| `GREEN3_2` |
| `GREEN4` |
| `IGNORE` |
| `JOYBUTTON2` |
| `JOYDOWN` |
| `JOYFIRE` |
| `JOYLEFT` |
| `JOYRIGHT` |
| `JOYUP` |
| `K_0` |
| `K_1` |
| `K_2` |
| `K_3` |
| `K_4` |
| `K_5` |
| `K_6` |
| `K_7` |
| `K_8` |
| `K_9` |
| `K_A` |
| `K_ALT` |
| `K_B` |
| `K_BS` |
| `K_C` |
| `K_CTRL` |
| `K_CTRL_BRK` |
| `K_CTRL_C` |
| `K_D` |
| `K_DEL` |
| `K_DOWN` |
| `K_E` |
| `K_END` |
| `K_ENTER` |
| `K_ESC` |
| `K_F` |
| `K_F1` |
| `K_F10` |
| `K_F11` |
| `K_F12` |
| `K_F2` |
| `K_F3` |
| `K_F4` |
| `K_F5` |
| `K_F6` |
| `K_F7` |
| `K_F8` |
| `K_F9` |
| `K_G` |
| `K_H` |
| `K_HOME` |
| `K_I` |
| `K_INS` |
| `K_J` |
| `K_K` |
| `K_L` |
| `K_LEFT` |
| `K_M` |
| `K_N` |
| `K_N0` |
| `K_N1` |
| `K_N2` |
| `K_N3` |
| `K_N4` |
| `K_N5` |
| `K_N6` |
| `K_N7` |
| `K_N8` |
| `K_N9` |
| `K_NADD` |
| `K_NDEC` |
| `K_NDIV` |
| `K_NMUL` |
| `K_NSUB` |
| `K_O` |
| `K_P` |
| `K_PAUSE` |
| `K_PGDN` |
| `K_PGUP` |
| `K_PRTSC` |
| `K_Q` |
| `K_R` |
| `K_RIGHT` |
| `K_S` |
| `K_SHIFT` |
| `K_SHIFT_TAB` |
| `K_SPC` |
| `K_T` |
| `K_TAB` |
| `K_U` |
| `K_UP` |
| `K_V` |
| `K_W` |
| `K_X` |
| `K_Y` |
| `K_Z` |
| `LEFT` |
| `LIGHT_GREEN` |
| `LIGHT_GREEN_2` |
| `LIGHT_PINK4` |
| `LIGHT_SEA_GREEN` |
| `LIGHT_SKY_BLUE3` |
| `LIGHT_SKY_BLUE3_2` |
| `LIGHT_SLATE_BLUE` |
| `LIGHT_SLATE_GRAY` |
| `LIME` |
| `MAROON` |
| `MEDIUM_PURPLE` |
| `MEDIUM_PURPLE3` |
| `MEDIUM_PURPLE3_2` |
| `MEDIUM_PURPLE4` |
| `MEDIUM_SPRING_GREEN` |
| `MEDIUM_TURQUOISE` |
| `NAVY` |
| `NAVY_BLUE` |
| `NO` |
| `NONE` |
| `OBSCURA` |
| `OCR` |
| `OK` |
| `OKCANCEL` |
| `OLIVE` |
| `ORANGE4` |
| `ORANGE4_2` |
| `PALE_GREEN1` |
| `PALE_GREEN3` |
| `PALE_GREEN3_2` |
| `PALE_TURQUOISE4` |
| `PLUM4` |
| `PURPLE` |
| `PURPLE1` |
| `PURPLE3` |
| `PURPLE4` |
| `PURPLE4_2` |
| `RECEIVED` |
| `RED` |
| `RERANKER` |
| `RERANK_KEYWORD` |
| `RERANK_OFF` |
| `RERANK_ONNX` |
| `RETRY` |
| `RETRYCANCEL` |
| `RIGHT` |
| `ROUNDED` |
| `ROYAL_BLUE1` |
| `SDL` |
| `SEA_GREEN1` |
| `SEA_GREEN1_2` |
| `SEA_GREEN2` |
| `SEA_GREEN3` |
| `SILVER` |
| `SINGLE` |
| `SKY_BLUE1` |
| `SKY_BLUE2` |
| `SKY_BLUE3` |
| `SLATE_BLUE1` |
| `SLATE_BLUE3` |
| `SLATE_BLUE3_2` |
| `SPRING_GREEN1` |
| `SPRING_GREEN2` |
| `SPRING_GREEN2_2` |
| `SPRING_GREEN3` |
| `SPRING_GREEN3_2` |
| `SPRING_GREEN4` |
| `STEEL_BLUE` |
| `STEEL_BLUE1` |
| `STEEL_BLUE1_2` |
| `STEEL_BLUE3` |
| `SYSTEM` |
| `TEAL` |
| `TTS` |
| `TURQUOISE2` |
| `TURQUOISE4` |
| `WHEAT4` |
| `WHITE` |
| `YELLOW` |
| `YELLOW4` |
| `YELLOW4_2` |
| `YES` |
| `YESNO` |
| `YESNOCANCEL` |

## Namespace CONTEXT

### Comandi (0)

| Comando |
|---|---|

### Funzioni (5)

| Funzione | Ritorno |
|---|---|
| `BACKGROUND` | ? |
| `CANCEL` | ? |
| `CANCELLED` | ? |
| `DONE` | ? |
| `WITHCANCEL` | ? |

### Tipi (1)

| Tipo |
|---|---|
| `CONTEXT` |

### Costanti (0)

| Costante |
|---|---|

## Namespace CONTROLS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (3)

| Funzione | Ritorno |
|---|---|
| `CTRLGETPROP` | DoubleType |
| `CTRLGETPROPS` | StringType |
| `handle` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace CONVERSIONS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (11)

| Funzione | Ritorno |
|---|---|
| `Bin` | StringType |
| `CByte` | ByteType |
| `CCur` | CurrencyType |
| `CDbl` | DoubleType |
| `CInt` | IntegerType |
| `CLng` | LongType |
| `CSng` | SingleType |
| `CStr` | StringType |
| `Hex` | StringType |
| `Oct` | StringType |
| `Val` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace CONVERT

### Comandi (4)

| Comando |
|---|---|
| `CONVBAT` |
| `EXTRACT` |
| `RENDER` |
| `WEB2MD` |

### Funzioni (31)

| Funzione | Ritorno |
|---|---|
| `Batch` | StringType |
| `Close` | DoubleType |
| `CodeBlocks` | ListType |
| `Extract` | StringType |
| `ExtractEngine` | StringType |
| `Format` | StringType |
| `Formats` | StringType |
| `Headings` | ListType |
| `Html` | StringType |
| `Images` | DoubleType |
| `Links` | ListType |
| `Markdown` | StringType |
| `MdImages` | ListType |
| `Metadata` | StringType |
| `Open` | DoubleType |
| `PageCount` | DoubleType |
| `Parse` | JsonType |
| `Query` | ListType |
| `Render` | DoubleType |
| `RenderHtml` | StringType |
| `RenderText` | StringType |
| `Search` | StringType |
| `Strip` | StringType |
| `Tables` | ListType |
| `Text` | StringType |
| `ToHtml` | StringType |
| `ToMd` | StringType |
| `ToText` | StringType |
| `Tree` | StringType |
| `Validate` | StringType |
| `Web` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace CRYPTOGRAPHY

### Comandi (2)

| Comando |
|---|---|
| `KEYCLOSE` |
| `KEYGEN` |

### Funzioni (40)

| Funzione | Ritorno |
|---|---|
| `AESDECRYPT` | ? |
| `AESENCRYPT` | ? |
| `CHACHA20DECRYPT` | ? |
| `CHACHA20ENCRYPT` | ? |
| `CHACHA20POLY1305DECRYPT` | ? |
| `CHACHA20POLY1305ENCRYPT` | DictType |
| `DECRYPTASYM` | ? |
| `ENCRYPTASYM` | ? |
| `FROMBASE64` | ? |
| `FROMBASE64URL` | ? |
| `FROMHEX` | ? |
| `HASHFILE` | ? |
| `HMACSHA256` | StringType |
| `HMACSHA384` | StringType |
| `HMACSHA512` | StringType |
| `KEYEXPORT` | StringType |
| `KEYGEN` | ArrayType |
| `KEYIMPORT` | LongType |
| `MD5` | ? |
| `PASSWORDHASH` | StringType |
| `PASSWORDVERIFY` | BitType |
| `RANDOMBYTES` | ? |
| `RANDOMCRYPTOBYTES` | ? |
| `RANDOMDOUBLE` | DoubleType |
| `RANDOMINT` | IntegerType |
| `RANDOMNORMAL` | DoubleType |
| `SHA1` | ? |
| `SHA256` | ? |
| `SHA512` | ? |
| `SIGN` | ? |
| `STRHASHFILE` | StringType |
| `STRMD5` | StringType |
| `STRSHA1` | StringType |
| `STRSHA256` | StringType |
| `STRSHA512` | StringType |
| `TOBASE64` | StringType |
| `TOBASE64URL` | StringType |
| `TOHEX` | StringType |
| `VERIFY` | BitType |
| `VERIFYRAW` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace DATETIME

### Comandi (0)

| Comando |
|---|---|

### Funzioni (36)

| Funzione | Ritorno |
|---|---|
| `CDOW` | StringType |
| `CMONTH` | StringType |
| `CTOD` | DoubleType |
| `DATEADD` | DateTimeType |
| `DATEDIFF` | DoubleType |
| `DATEFROMUNIX` | DateType |
| `DATESERIAL` | DateType |
| `DATEVALUE` | DateType |
| `DAY` | LongType |
| `DAYSINMONTH` | IntegerType |
| `DOW` | IntegerType |
| `DTOC` | StringType |
| `DTOS` | StringType |
| `Date` | DoubleType |
| `EOMONTH` | DateType |
| `FIRSTDAYOFMONTH` | DateType |
| `HOUR` | LongType |
| `ISDATE` | BitType |
| `ISLEAPYEAR` | BitType |
| `ISOWEEKOFYEAR` | IntegerType |
| `MINUTE` | LongType |
| `MONTH` | LongType |
| `NOW` | DateTimeType |
| `SECOND` | LongType |
| `SECONDS` | DoubleType |
| `STOD` | DateType |
| `STRDATE` | StringType |
| `STRTI` | StringType |
| `STRTIME` | StringType |
| `TI` | LongType |
| `TIMESERIAL` | TimeType |
| `TODAY` | DateType |
| `Time` | DoubleType |
| `UNIXTIMESTAMP` | LongType |
| `WEEKDAY` | LongType |
| `YEAR` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace DBF

### Comandi (0)

| Comando |
|---|---|

### Funzioni (30)

| Funzione | Ritorno |
|---|---|
| `ALIAS` | StringType |
| `APPENDMANY` | LongType |
| `APPLYREPLICATION` | LongType |
| `BEGINGROUPCOMMIT` | BitType |
| `BOF` | BitType |
| `DBFILTER` | StringType |
| `DBRELATION` | StringType |
| `DELETED` | BitType |
| `ENDGROUPCOMMIT` | BitType |
| `EOF` | BitType |
| `FIELDNAME` | StringType |
| `FLOCK` | BitType |
| `FLUSHDURABILITY` | BitType |
| `FOLLOWREPLICATION` | LongType |
| `FOUND` | BitType |
| `INDEXKEY` | StringType |
| `INDEXORD` | LongType |
| `LASTREC` | LongType |
| `LOOKUP` | InferredType |
| `LUPDATE` | StringType |
| `NETERR` | BitType |
| `RECNO` | LongType |
| `RECSIZE` | LongType |
| `REPLICATIONRECV` | LongType |
| `REPLICATIONSHIP` | LongType |
| `RLOCK` | BitType |
| `SELECTED` | LongType |
| `SETDURABILITY` | BitType |
| `SETREPLICATION` | StringType |
| `USED` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace FINANCE

### Comandi (0)

| Comando |
|---|---|

### Funzioni (23)

| Funzione | Ritorno |
|---|---|
| `CUMIPMT` | DoubleType |
| `CUMPRINC` | DoubleType |
| `DAYS360` | DoubleType |
| `DB` | DoubleType |
| `DDB` | DoubleType |
| `FV` | DoubleType |
| `FVEXT` | DoubleType |
| `IRR` | DoubleType |
| `MIRR` | DoubleType |
| `MNPV` | DoubleType |
| `NPER` | DoubleType |
| `NPEREXT` | DoubleType |
| `NPV` | DoubleType |
| `PMT` | DoubleType |
| `PMTEXT` | DoubleType |
| `PV` | DoubleType |
| `PVEXT` | DoubleType |
| `RATE` | DoubleType |
| `SLN` | DoubleType |
| `SYD` | DoubleType |
| `XIRR` | DoubleType |
| `XNPV` | DoubleType |
| `YEARFRAC` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace GRAPH

### Comandi (0)

| Comando |
|---|---|

### Funzioni (25)

| Funzione | Ritorno |
|---|---|
| `Betweenness` | StringType |
| `Bfs` | StringType |
| `Bipartite` | StringType |
| `ByFolder` | StringType |
| `ByTag` | StringType |
| `Clustering` | StringType |
| `Connected` | StringType |
| `Dfs` | StringType |
| `Diameter` | StringType |
| `Dijkstra` | StringType |
| `Dot` | StringType |
| `Eccentricity` | StringType |
| `HasCycle` | DoubleType |
| `IsValid` | DoubleType |
| `Louvain` | StringType |
| `MaxFlow` | StringType |
| `Mst` | StringType |
| `Neighbors` | StringType |
| `PageRank` | StringType |
| `Path` | StringType |
| `QueryMulti` | StringType |
| `Stats` | StringType |
| `StronglyConnected` | StringType |
| `Subgraph` | StringType |
| `TopoSort` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace GRAPHICS

### Comandi (40)

| Comando |
|---|---|
| `BLOAD` |
| `BSAVE` |
| `CCLS` |
| `CIRCLE` |
| `CLEAR` |
| `CLOCATE` |
| `CLS` |
| `COLOR` |
| `CPALETTE` |
| `CPRINT` |
| `CRESTORESCREEN` |
| `CSAVESCREEN` |
| `DOEVENTS` |
| `DRAW` |
| `FONT` |
| `IMAGEGET` |
| `IMAGEPUT` |
| `IMAGEPUTS` |
| `LINE` |
| `LOAD` |
| `LOCATE` |
| `PAINT` |
| `PALETTE` |
| `PCOPY` |
| `PIXEL` |
| `PLAY` |
| `POINT` |
| `POLYGON` |
| `PRESET` |
| `PRINT` |
| `PSET` |
| `SCREEN` |
| `SCREENSET` |
| `SOUND` |
| `SPRITE` |
| `SPRITEDEF` |
| `SYNC` |
| `TEXTMODE` |
| `TILEMAP` |
| `VIEW` |

### Funzioni (15)

| Funzione | Ritorno |
|---|---|
| `C2PX` | DoubleType |
| `C2PY` | DoubleType |
| `CELL` | LongType |
| `CELLSTR` | StringType |
| `COLLIDE` | InferredType |
| `COLLIDEBATCH` | ArrayType |
| `P2CX` | DoubleType |
| `P2CY` | DoubleType |
| `Point` | LongType |
| `RGB` | RgbaColorType |
| `SCREEN` | DoubleType |
| `ScreenInfo` | DisplayModeType |
| `Screens` | InferredType |
| `TextHeight` | DoubleType |
| `TextWidth` | DoubleType |

### Tipi (4)

| Tipo |
|---|---|
| `DisplayModeType` |
| `RGBA` |
| `RgbaColorType` |
| `SPRITEINFO` |

### Costanti (0)

| Costante |
|---|---|

## Namespace GREP

### Comandi (0)

| Comando |
|---|---|

### Funzioni (3)

| Funzione | Ritorno |
|---|---|
| `COUNT` | LongType |
| `FILES` | ArrayType |
| `MATCHES` | ArrayType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace HTTP

### Comandi (6)

| Comando |
|---|---|
| `CONFIG` |
| `FETCH` |
| `ROUTE` |
| `SERVER` |
| `STATIC` |
| `USE` |

### Funzioni (8)

| Funzione | Ritorno |
|---|---|
| `DELETE` | StringType |
| `DETECTCONTENTTYPE` | StringType |
| `FETCH` | FetchResultType |
| `GET` | StringType |
| `POST` | StringType |
| `PUT` | StringType |
| `SAFEFILENAME` | StringType |
| `SESSIONCREATE` | StringType |

### Tipi (12)

| Tipo |
|---|---|
| `CookieJarType` |
| `FetchResultType` |
| `FileFieldType` |
| `FormFieldType` |
| `HttpConfigType` |
| `HttpHeadersType` |
| `HttpOptionsType` |
| `HttpPostedDataType` |
| `HttpRequestType` |
| `HttpResponseType` |
| `NextHandler` |
| `WebSocketType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace IO

### Comandi (25)

| Comando |
|---|---|
| `CHDIR` |
| `CLOSE` |
| `COPY` |
| `ERASE` |
| `FREAD` |
| `GET` |
| `INPUT` |
| `KILL` |
| `LINE INPUT` |
| `LOCK` |
| `MEMORYMAP` |
| `MKDIR` |
| `MMAPPOKE` |
| `MMAPWRITEBYTES` |
| `MOVE` |
| `OPEN` |
| `PUT` |
| `RENAME` |
| `RMDIR` |
| `SEEK` |
| `UNLOCK` |
| `UNWATCH` |
| `WATCH` |
| `WRITE` |
| `XCOPY` |

### Funzioni (35)

| Funzione | Ritorno |
|---|---|
| `AppPath` | StringType |
| `BinPath` | StringType |
| `CountLines` | LongType |
| `Cwd` | StringType |
| `DIR` | ArrayType |
| `DirExists` | BitType |
| `EOF` | BitType |
| `FClose` | LongType |
| `FCreate` | LongType |
| `FError` | LongType |
| `FINPUT` | StringType |
| `FOpen` | LongType |
| `FRead` | StringType |
| `FSeek` | LongType |
| `FWrite` | LongType |
| `FileExists` | BitType |
| `FolderSize` | QuadType |
| `FreeFile` | LongType |
| `GETTEMPDIR` | ? |
| `GETTEMPFILE` | ? |
| `GetFolderPath` | StringType |
| `LOF` | LongType |
| `MEMORYMAP` | LongType |
| `MMAPPEEK` | ByteType |
| `MMAPREADBYTES` | ArrayType |
| `NormalizePath` | StringType |
| `ParentDir` | StringType |
| `ProjectFolder` | StringType |
| `ReadBytes` | ArrayType |
| `ReadFile` | StringType |
| `ReadLines` | ArrayType |
| `ResolveFilePath` | StringType |
| `SEEK` | LongType |
| `TempDir` | StringType |
| `TempFile` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace ISAM

### Comandi (0)

| Comando |
|---|---|

### Funzioni (24)

| Funzione | Ritorno |
|---|---|
| `APPENDMANY` | LongType |
| `APPLYREPLICATION` | LongType |
| `BOF` | BitType |
| `DBFILTER` | StringType |
| `DBRELATION` | StringType |
| `DELETED` | BitType |
| `EOF` | BitType |
| `FLOCK` | BitType |
| `FOLLOWREPLICATION` | LongType |
| `FOUND` | BitType |
| `INDEXKEY` | StringType |
| `INDEXORD` | LongType |
| `INFO` | JsonType |
| `INTEGRITY` | JsonType |
| `LASTREC` | LongType |
| `LOG` | StringType |
| `RECNO` | LongType |
| `RECSIZE` | LongType |
| `REPLICATIONRECV` | LongType |
| `REPLICATIONSHIP` | LongType |
| `RLOCK` | BitType |
| `SELECTED` | LongType |
| `SETREPLICATION` | StringType |
| `USED` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace JOY

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `AVAILABLE` | BitType |
| `MASK` | LongType |
| `NAME` | StringType |
| `PORTS` | IntegerType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace JSON

### Comandi (4)

| Comando |
|---|---|
| `OPEN` |
| `SAVE` |
| `WRITESECTION` |
| `WRITEVALUE` |

### Funzioni (9)

| Funzione | Ritorno |
|---|---|
| `Extract` | InferredType |
| `KEYS` | ArrayType |
| `Load` | JsonType |
| `Parse` | JsonType |
| `ReadSection` | JsonType |
| `ReadValue` | InferredType |
| `Stringify` | StringType |
| `ToJson` | JsonType |
| `ToObject` | InferredType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace KDTREE

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `CREATE` | LongType |
| `QUERY` | ? |
| `QUERYBALL` | ? |
| `QUERYDIST` | ? |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace LEDGER

### Comandi (0)

| Comando |
|---|---|

### Funzioni (17)

| Funzione | Ritorno |
|---|---|
| `ACCOUNTS` | ArrayType |
| `ADD` | IntegerType |
| `BALANCE` | ArrayType |
| `BOF` | BitType |
| `COUNT` | LongType |
| `EDIT` | IntegerType |
| `EOF` | BitType |
| `FOUND` | BitType |
| `GET` | ArrayType |
| `IMPORT` | IntegerType |
| `LASTREC` | LongType |
| `OPEN` | IntegerType |
| `PLOT` | IntegerType |
| `RECNO` | LongType |
| `REGISTER` | ArrayType |
| `VALIDATE` | BitType |
| `VARIANCE` | DoubleType |

### Tipi (1)

| Tipo |
|---|---|
| `LEDGERENTRY` |

### Costanti (0)

| Costante |
|---|---|

## Namespace LLM

### Comandi (0)

| Comando |
|---|---|

### Funzioni (61)

| Funzione | Ritorno |
|---|---|
| `Arch` | StringType |
| `BenchResult` | StringType |
| `CacheStats` | StringType |
| `Callmap` | StringType |
| `CheckRev` | StringType |
| `Commit` | StringType |
| `Context` | StringType |
| `DatasetStats` | StringType |
| `DecodeStats` | StringType |
| `Detokenize` | StringType |
| `DistillCkpt` | StringType |
| `DistillDone` | DoubleType |
| `DistillEvalAccept` | DoubleType |
| `DistillEvalPass` | DoubleType |
| `DistillEvalTok` | DoubleType |
| `DistillHold` | DoubleType |
| `DistillLoss` | DoubleType |
| `DistillSampleDone` | DoubleType |
| `DistillSampleFail` | DoubleType |
| `DistillSampleN` | DoubleType |
| `DistillStatus` | StringType |
| `EvalGate` | StringType |
| `FileOutline` | StringType |
| `FindSymbol` | StringType |
| `FinetuneBaseHold` | DoubleType |
| `FinetuneBaseTrain` | DoubleType |
| `FinetuneCkpt` | StringType |
| `FinetuneDemoChanged` | DoubleType |
| `FinetuneDone` | DoubleType |
| `FinetuneGreedy` | DoubleType |
| `FinetuneGreedyAfter` | DoubleType |
| `FinetuneHold` | DoubleType |
| `FinetuneLoss` | DoubleType |
| `FinetuneStatus` | StringType |
| `Impact` | StringType |
| `Inspect` | StringType |
| `Kernel` | StringType |
| `Kv` | StringType |
| `LoraList` | StringType |
| `LoraLoad` | LongType |
| `LoraSave` | LongType |
| `Module` | StringType |
| `Patch` | StringType |
| `Policy` | StringType |
| `PrefillStats` | StringType |
| `RepoMap` | StringType |
| `Revision` | StringType |
| `RunStop` | LongType |
| `RunsClear` | LongType |
| `RunsCount` | LongType |
| `RunsItem` | StringType |
| `RunsJson` | StringType |
| `StreamStats` | StringType |
| `Symbol` | StringType |
| `Tensor` | StringType |
| `TokenCount` | DoubleType |
| `Tokens` | StringType |
| `TrainConfig` | StringType |
| `TrainConfigSet` | StringType |
| `TrainExport` | LongType |
| `Verify` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace MATH

### Comandi (0)

| Comando |
|---|---|

### Funzioni (42)

| Funzione | Ritorno |
|---|---|
| `Abs` | InferredType |
| `Acos` | DoubleType |
| `Angle2d` | DoubleType |
| `Asin` | DoubleType |
| `Atn` | DoubleType |
| `Avg` | InferredType |
| `COSH` | DoubleType |
| `Ceiling` | DoubleType |
| `Clamp` | InferredType |
| `Comb` | DoubleType |
| `Cos` | DoubleType |
| `Deg2Rad` | DoubleType |
| `Dist2d` | DoubleType |
| `Dist3d` | DoubleType |
| `Exp` | DoubleType |
| `Fact` | UQuadType |
| `Fix` | DoubleType |
| `Floor` | DoubleType |
| `Hypot` | DoubleType |
| `Int` | DoubleType |
| `IsNumeric` | BitType |
| `Log` | DoubleType |
| `Log10` | DoubleType |
| `Max` | InferredType |
| `Min` | InferredType |
| `POLAR_X` | DoubleType |
| `POLAR_Y` | DoubleType |
| `Perm` | DoubleType |
| `Pi` | DoubleType |
| `Pythagoras` | DoubleType |
| `Rad2Deg` | DoubleType |
| `Range` | ArrayType |
| `Rnd` | DoubleType |
| `Round` | DoubleType |
| `SINH` | DoubleType |
| `Sgn` | LongType |
| `Sin` | DoubleType |
| `Sqr` | DoubleType |
| `Sum` | InferredType |
| `TANH` | DoubleType |
| `Tan` | DoubleType |
| `Trunc` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace MATRIX

### Comandi (10)

| Comando |
|---|---|
| `IDENTITY` |
| `MULTIPLY` |
| `POP` |
| `PROJECTION` |
| `PUSH` |
| `ROTATE` |
| `SCALE` |
| `TRANSLATE` |
| `VIEW` |
| `WORLD` |

### Funzioni (75)

| Funzione | Ritorno |
|---|---|
| `ABOVEMASK` | ArrayType |
| `ABOVEMATRIX` | ArrayType |
| `ADDCOLVECTOR` | LongType |
| `ADDROWVECTOR` | LongType |
| `ARGMAX` | ArrayType |
| `ARGMIN` | ArrayType |
| `ASCSV` | StringType |
| `ASJSON` | StringType |
| `BLOCKDIAG` | LongType |
| `CHOLESKY` | CholeskyDecompositionType |
| `CIRCULANT` | LongType |
| `CLONEMATRIX` | LongType |
| `COLMEANS` | ArrayType |
| `COLSUMS` | ArrayType |
| `COLUMNCOUNT` | LongType |
| `COLVARIANCES` | ArrayType |
| `CONCATCOLS` | LongType |
| `CONCATROWS` | LongType |
| `COND` | DoubleType |
| `COSM` | LongType |
| `CREATE` | LongType |
| `DETERMINANT` | DoubleType |
| `EIG` | LongType |
| `EVD` | EvdDecompositionType |
| `EXPM` | LongType |
| `FACTOR` | LongType |
| `FRACTIONALPOWER` | LongType |
| `FROBENIUSNORM` | DoubleType |
| `GETVALUE` | DoubleType |
| `HANKEL` | LongType |
| `HESSENBERG` | LongType |
| `INFINITYNORM` | DoubleType |
| `INVERSE` | LongType |
| `ISLOWERTRIANGULAR` | BitType |
| `ISORTHOGONAL` | BitType |
| `ISSYMMETRIC` | BitType |
| `ISUPPERTRIANGULAR` | BitType |
| `KRONECKER` | LongType |
| `LOADCSV` | LongType |
| `LOADJSON` | LongType |
| `LOGM` | LongType |
| `LU` | LuDecompositionType |
| `MAXVALUE` | DoubleType |
| `MINVALUE` | DoubleType |
| `MULTIPLY` | LongType |
| `MULTIPLYLEFTVECTOR` | ArrayType |
| `MULTIPLYVECTOR` | ArrayType |
| `NORM` | DoubleType |
| `NUCLEARNORM` | DoubleType |
| `NULLSPACE` | LongType |
| `ONENORM` | DoubleType |
| `ORTH` | LongType |
| `PINVERSE` | LongType |
| `POLAR` | LongType |
| `QR` | QrDecompositionType |
| `RANK` | LongType |
| `RESHAPE` | LongType |
| `ROWCOUNT` | LongType |
| `ROWMEANS` | ArrayType |
| `ROWSUMS` | ArrayType |
| `ROWVARIANCES` | ArrayType |
| `SCHUR` | LongType |
| `SETVALUE` | LongType |
| `SIGNM` | LongType |
| `SINM` | LongType |
| `SLICE` | LongType |
| `SOLVE` | ArrayType |
| `SOLVETRIANGULAR` | ArrayType |
| `SQRTM` | LongType |
| `SUBSPACEANGLES` | ArrayType |
| `SVD` | SvdDecompositionType |
| `TANM` | LongType |
| `TOEPLITZ` | LongType |
| `TRACE` | DoubleType |
| `TRANSPOSE` | LongType |

### Tipi (5)

| Tipo |
|---|---|
| `CholeskyDecomposition` |
| `EvdDecomposition` |
| `LuDecomposition` |
| `QrDecomposition` |
| `SvdDecomposition` |

### Costanti (0)

| Costante |
|---|---|

## Namespace MEMORY

### Comandi (0)

| Comando |
|---|---|

### Funzioni (5)

| Funzione | Ritorno |
|---|---|
| `DELETE` | LongType |
| `LIST` | StringType |
| `LOAD` | StringType |
| `SEARCH` | StringType |
| `STORE` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace MESSAGE

### Comandi (1)

| Comando |
|---|---|
| `MESSAGE` |

### Funzioni (1)

| Funzione | Ritorno |
|---|---|
| `DELIVERY` | ? |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace MOUSE

### Comandi (9)

| Comando |
|---|---|
| `CURSOR` |
| `HIDE` |
| `LIMIT` |
| `OFF` |
| `ON` |
| `SET` |
| `SHOW` |
| `UNLIMIT` |
| `WAIT` |

### Funzioni (11)

| Funzione | Ritorno |
|---|---|
| `BUTTON` | IntegerType |
| `CLICK` | BitType |
| `DBLCLICK` | BitType |
| `LEFT` | BitType |
| `MIDDLE` | BitType |
| `MOVE` | BitType |
| `PRESENT` | BitType |
| `RIGHT` | BitType |
| `WHEEL` | IntegerType |
| `X` | IntegerType |
| `Y` | IntegerType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace NET

### Comandi (2)

| Comando |
|---|---|
| `SETOPTION` |
| `SHUTDOWN` |

### Funzioni (8)

| Funzione | Ritorno |
|---|---|
| `CONN` | BitType |
| `GETHOSTIP` | StringType |
| `LOCALHOST` | StringType |
| `LOCALPORT` | LongType |
| `PEEK` | StringType |
| `RECV` | LongType |
| `REMOTEIP` | StringType |
| `REMOTEPORT` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace OPTIMIZE

### Comandi (0)

| Comando |
|---|---|

### Funzioni (34)

| Funzione | Ritorno |
|---|---|
| `BasinHopping` | ? |
| `Bfgs` | ? |
| `Bisection` | DoubleType |
| `Brent` | DoubleType |
| `BrentMin` | DoubleType |
| `Brenth` | DoubleType |
| `Brentq` | DoubleType |
| `Broyden` | ? |
| `Brute` | ? |
| `CholeskySolve` | ? |
| `Cobyla` | ? |
| `ConjugateGradient` | ? |
| `CurveFit` | ? |
| `DifferentialEvolution` | ? |
| `DualAnnealing` | ? |
| `Fsolve` | ? |
| `GoldenSection` | DoubleType |
| `LBfgs` | ? |
| `LeastSquares` | ? |
| `LeastSquaresFit` | ? |
| `LevenbergMarquardt` | ? |
| `LevenbergMarquardtVec` | ? |
| `Linprog` | ? |
| `Minimize` | ? |
| `MinimizeScalar` | DoubleType |
| `NelderMead` | ? |
| `Newton` | DoubleType |
| `Powell` | ? |
| `Ridder` | DoubleType |
| `Secant` | DoubleType |
| `Simplex` | ? |
| `Slsqp` | ? |
| `Tnc` | ? |
| `Toms748` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace POOL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (2)

| Funzione | Ritorno |
|---|---|
| `GET` | ? |
| `PUT` | ? |

### Tipi (1)

| Tipo |
|---|---|
| `POOL` |

### Costanti (0)

| Costante |
|---|---|

## Namespace RDD

### Comandi (0)

| Comando |
|---|---|

### Funzioni (22)

| Funzione | Ritorno |
|---|---|
| `BOF` | BitType |
| `BUFFER` | BitType |
| `CTOD` | DateType |
| `CURVAL` | DoubleType |
| `DELETE` | LongType |
| `DTOC` | AnyType |
| `EOF` | BitType |
| `FLOAT` | DoubleType |
| `FLOCK` | BitType |
| `FOUND` | BitType |
| `GETFIELD` | AnyType |
| `GETFLDSTATE` | LongType |
| `OLDVAL` | AnyType |
| `RECALL` | LongType |
| `RECNO` | LongType |
| `REPLACE` | LongType |
| `RLOCK` | BitType |
| `SETDELETED` | BitType |
| `TABLEREVERT` | BitType |
| `TABLEUPDATE` | BitType |
| `TODAY` | DateType |
| `VALTYPE` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace SIGNAL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (18)

| Funzione | Ritorno |
|---|---|
| `BLACKMAN` | ? |
| `BUTTER` | LongType |
| `CHEBY1` | LongType |
| `CHEBY2` | LongType |
| `CONVOLVE` | ? |
| `CORRELATE` | ? |
| `DECIMATE` | ? |
| `ELLIP` | LongType |
| `FILTFILT` | ? |
| `FINDPEAKS` | ? |
| `FIRWIN` | ? |
| `FREQZ` | ? |
| `HAMMING` | ? |
| `HANN` | ? |
| `LFILTER` | ? |
| `RESAMPLE` | ? |
| `SAVGOLFILTER` | ? |
| `WELCH` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace SPATIAL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `CDIST` | LongType |
| `CONVEXHULL` | ? |
| `DISTANCEMATRIX` | LongType |
| `PDIST` | ? |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace SPECIAL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (19)

| Funzione | Ritorno |
|---|---|
| `BESSELI` | DoubleType |
| `BESSELJ` | DoubleType |
| `BESSELK` | DoubleType |
| `BESSELY` | DoubleType |
| `BETA` | DoubleType |
| `BETAINC` | DoubleType |
| `BETAINCINV` | DoubleType |
| `BETALN` | DoubleType |
| `DIGAMMA` | DoubleType |
| `ERF` | DoubleType |
| `ERFC` | DoubleType |
| `ERFCINV` | DoubleType |
| `ERFINV` | DoubleType |
| `GAMMA` | DoubleType |
| `GAMMAINC` | DoubleType |
| `GAMMAINCC` | DoubleType |
| `GAMMAINCCINV` | DoubleType |
| `GAMMAINCINV` | DoubleType |
| `LGAMMA` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace SPEED

### Comandi (0)

| Comando |
|---|---|

### Funzioni (9)

| Funzione | Ritorno |
|---|---|
| `BEGINGROUPCOMMIT` | BitType |
| `ENDGROUPCOMMIT` | BitType |
| `EOF` | BitType |
| `FLUSHDURABILITY` | BitType |
| `INFO` | StringType |
| `SELECTED` | InferredType |
| `SETDURABILITY` | StringType |
| `SETMANY` | LongType |
| `VALIDATE` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace SQL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (8)

| Funzione | Ritorno |
|---|---|
| `CHANGES` | LongType |
| `COL` | ArrayType |
| `DFSCHEMA` | StringType |
| `EXEC` | LongType |
| `FETCH` | BitType |
| `FROMCSV` | LongType |
| `LASTINSERTROWID` | LongType |
| `TOCSV` | LongType |

### Tipi (1)

| Tipo |
|---|---|
| `SqlResultSetType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace STATS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (83)

| Funzione | Ritorno |
|---|---|
| `Anova` | AnyType |
| `BernoulliPmf` | DoubleType |
| `BetaCdf` | DoubleType |
| `BetaPdf` | DoubleType |
| `BetaPpf` | DoubleType |
| `BinomialPmf` | DoubleType |
| `Chi2` | AnyType |
| `Chi2Con` | AnyType |
| `ChiCdf` | DoubleType |
| `ChiPdf` | DoubleType |
| `ChiPpf` | DoubleType |
| `ChiSquaredCdf` | DoubleType |
| `ChiSquaredIsf` | DoubleType |
| `ChiSquaredPdf` | DoubleType |
| `ChiSquaredPpf` | DoubleType |
| `Correlation` | DoubleType |
| `Describe` | AnyType |
| `ExponentialCdf` | DoubleType |
| `ExponentialIsf` | DoubleType |
| `ExponentialPdf` | DoubleType |
| `ExponentialPpf` | DoubleType |
| `FCdf` | DoubleType |
| `FIsf` | DoubleType |
| `FPdf` | DoubleType |
| `FPpf` | DoubleType |
| `Fisher` | AnyType |
| `GammaCdf` | DoubleType |
| `GammaIsf` | DoubleType |
| `GammaPdf` | DoubleType |
| `GammaPpf` | DoubleType |
| `GeometricPmf` | DoubleType |
| `HypergeometricPmf` | DoubleType |
| `Iqr` | DoubleType |
| `JarqueBera` | AnyType |
| `Ks` | AnyType |
| `Kurtosis` | DoubleType |
| `KurtosisTest` | AnyType |
| `LinRegress` | AnyType |
| `LogisticPdf` | DoubleType |
| `LogisticPpf` | DoubleType |
| `LognormalCdf` | DoubleType |
| `LognormalPdf` | DoubleType |
| `LognormalPpf` | DoubleType |
| `Mad` | DoubleType |
| `MannWhitney` | AnyType |
| `Mean` | DoubleType |
| `Median` | DoubleType |
| `Mode` | DoubleType |
| `Moment` | DoubleType |
| `NegativeBinomialPmf` | DoubleType |
| `NormalCdf` | DoubleType |
| `NormalIsf` | DoubleType |
| `NormalPdf` | DoubleType |
| `NormalPpf` | DoubleType |
| `NormalTest` | AnyType |
| `ParetoPdf` | DoubleType |
| `ParetoPpf` | DoubleType |
| `PearsonR` | AnyType |
| `Percentile` | DoubleType |
| `PoissonPmf` | DoubleType |
| `Shapiro` | AnyType |
| `SkewTest` | AnyType |
| `SkewedNormalPdf` | DoubleType |
| `Skewness` | DoubleType |
| `SpearmanR` | AnyType |
| `StdDev` | DoubleType |
| `StudentTCdf` | DoubleType |
| `StudentTIsf` | DoubleType |
| `StudentTPdf` | DoubleType |
| `StudentTPpf` | DoubleType |
| `TTest` | AnyType |
| `TTestOneSample` | AnyType |
| `TTestPaired` | AnyType |
| `TriangularPdf` | DoubleType |
| `TriangularPpf` | DoubleType |
| `UniformPdf` | DoubleType |
| `UniformPpf` | DoubleType |
| `Variance` | DoubleType |
| `WeibullCdf` | DoubleType |
| `WeibullPdf` | DoubleType |
| `WeibullPpf` | DoubleType |
| `Wilcoxon` | AnyType |
| `ZTest` | AnyType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace STRINGS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (47)

| Funzione | Ritorno |
|---|---|
| `Asc` | LongType |
| `Camel` | StringType |
| `Chr` | StringType |
| `Contains` | BitType |
| `Dup` | StringType |
| `EndsWith` | BitType |
| `Format` | StringType |
| `HTMLENCODE` | StringType |
| `ISCREDITCARD` | BitType |
| `ISEMAIL` | BitType |
| `ISLENGTH` | BitType |
| `ISREQUIRED` | BitType |
| `ISURL` | BitType |
| `Instr` | LongType |
| `JSENCODE` | StringType |
| `Join` | StringType |
| `Kebab` | StringType |
| `LCASE` | ? |
| `LTrim` | StringType |
| `Left` | StringType |
| `Len` | LongType |
| `Lower` | StringType |
| `Mid` | StringType |
| `PadC` | StringType |
| `PadL` | StringType |
| `PadR` | StringType |
| `Pascal` | StringType |
| `Petscii` | StringType |
| `RTrim` | StringType |
| `Remove` | StringType |
| `Repeat` | StringType |
| `Replace` | StringType |
| `Reverse` | StringType |
| `Right` | StringType |
| `Search` | BitType |
| `Sentence` | StringType |
| `Snake` | StringType |
| `Space` | StringType |
| `Split` | ArrayType |
| `StartsWith` | BitType |
| `Str` | StringType |
| `Title` | StringType |
| `Trim` | StringType |
| `UCASE` | ? |
| `URLENCODE` | StringType |
| `Upper` | StringType |
| `WRAP` | StringType |

### Tipi (3)

| Tipo |
|---|---|
| `RegexMatchType` |
| `RegexType` |
| `StringBuilderType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace SYNCMAP

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `DELETE` | ? |
| `LOAD` | ? |
| `LOADORSTORE` | ? |
| `STORE` | ? |

### Tipi (1)

| Tipo |
|---|---|
| `CONCURRENTMAP` |

### Costanti (0)

| Costante |
|---|---|

## Namespace SYSTEM

### Comandi (2)

| Comando |
|---|---|
| `RANDOMIZE` |
| `SHELL` |

### Funzioni (26)

| Funzione | Ritorno |
|---|---|
| `CHOOSE` | InferredType |
| `COMMAND` | StringType |
| `COMMANDCOUNT` | IntegerType |
| `CTYPE` | AnyType |
| `Count` | LongType |
| `EMPTY` | BitType |
| `GOMAXPROCS` | LongType |
| `IIF` | InferredType |
| `LBOUND` | LongType |
| `NUMCPU` | LongType |
| `NUMGOROUTINE` | LongType |
| `Next` | InferredType |
| `SHELLCODE` | LongType |
| `SHELLOUT` | StringType |
| `SIZEOF` | LongType |
| `STRPTR` | LongType |
| `STRTI` | StringType |
| `SYS` | StringType |
| `TAB` | StringType |
| `TI` | LongType |
| `TYPENAME` | StringType |
| `UBOUND` | LongType |
| `UDTALIGNED` | DoubleType |
| `UDTPACKED` | DoubleType |
| `VALTYPE` | StringType |
| `VARPTR` | LongType |

### Tipi (1)

| Tipo |
|---|---|
| `ErrType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace TIMESERIES

### Comandi (0)

| Comando |
|---|---|

### Funzioni (32)

| Funzione | Ritorno |
|---|---|
| `TSAUTOCORR` | DoubleType |
| `TSBANDPASS` | ? |
| `TSBANDSTOP` | ? |
| `TSCOVARIANCEMATRIX` | ? |
| `TSDECOMPOSE` | ? |
| `TSDESEASONALIZE` | ? |
| `TSDIFFERENCE` | ? |
| `TSEMA` | ? |
| `TSHIGHPASS` | ? |
| `TSLOWPASS` | ? |
| `TSMA` | ? |
| `TSMINMAX` | ? |
| `TSOUTLIERS` | ? |
| `TSRETURNS` | ? |
| `TSROLLAUTOCORR` | ? |
| `TSROLLCORR` | ? |
| `TSROLLCOVARIANCE` | ? |
| `TSROLLDELTA` | ? |
| `TSROLLKURTOSIS` | ? |
| `TSROLLMAD` | ? |
| `TSROLLMEAN` | ? |
| `TSROLLMEDIAN` | ? |
| `TSROLLMINMAX` | ? |
| `TSROLLQUANTILE` | ? |
| `TSROLLREGRESSION` | ? |
| `TSROLLREGRESSIONSINGLE` | ? |
| `TSROLLSHARPE` | ? |
| `TSROLLSKEWNESS` | ? |
| `TSROLLSORTINO` | ? |
| `TSROLLSTDDEV` | ? |
| `TSROLLZSCORE` | ? |
| `TSVOLATILITY` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TOOL

### Comandi (0)

| Comando |
|---|---|

### Funzioni (2)

| Funzione | Ritorno |
|---|---|
| `Register` | BitType |
| `Search` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TORCH

### Comandi (0)

| Comando |
|---|---|

### Funzioni (76)

| Funzione | Ritorno |
|---|---|
| `ABS` | ? |
| `ACCURACY` | ? |
| `ADD` | ? |
| `ALLCLOSE` | ? |
| `ARGMAX` | ? |
| `ARGSORT` | ? |
| `AVAILABLE` | ? |
| `BLEU` | ? |
| `CLIP` | ? |
| `CONCAT` | ? |
| `CREATE` | ? |
| `DATASETLOAD` | ? |
| `DEVICE` | ? |
| `DEVICEOF` | ? |
| `DIV` | ? |
| `DTYPE` | ? |
| `EXP` | ? |
| `EYE` | ? |
| `FREE` | ? |
| `FROM` | ? |
| `GELU` | ? |
| `GET` | ? |
| `IMATRIXLOAD` | ? |
| `IMATRIXRANKS` | ? |
| `LIBPATH` | ? |
| `LOADED` | ? |
| `LOG` | ? |
| `LOSSBINARY` | ? |
| `LOSSCROSSENTROPY` | ? |
| `LOSSMSE` | ? |
| `MATMUL` | ? |
| `MAX` | ? |
| `MEAN` | ? |
| `MIN` | ? |
| `MODELCREATE` | ? |
| `MODELDPOSTEP` | ? |
| `MODELEXPORT` | ? |
| `MODELFORWARD` | ? |
| `MODELFREE` | ? |
| `MODELINPUT` | ? |
| `MODELLOSS` | ? |
| `MODELOUTPUT` | ? |
| `MODELPARAMETERS` | ? |
| `MODELRESET` | ? |
| `MODELSAVE` | ? |
| `MODELSTATE` | ? |
| `MODELTRAINSTEP` | ? |
| `MUL` | ? |
| `NORMALIZE` | ? |
| `ONEHOT` | ? |
| `ONES` | ? |
| `PERMUTE` | ? |
| `POW` | ? |
| `RANDN` | ? |
| `RELU` | ? |
| `RESHAPE` | ? |
| `ROUGE` | ? |
| `SET` | ? |
| `SHAPE` | ArrayType |
| `SIGMOID` | ? |
| `SIMILARITY` | ? |
| `SLICE` | ? |
| `SOFTMAX` | ? |
| `SPLIT` | ? |
| `SQRT` | ? |
| `STACK` | ? |
| `STANDARDIZE` | ? |
| `SUB` | ? |
| `SUM` | ? |
| `TANH` | ? |
| `TEXTDECODE` | ? |
| `TEXTENCODE` | ? |
| `TOARRAY` | ? |
| `TOPK` | ? |
| `TRANSPOSE` | ? |
| `ZEROS` | ? |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING

### Comandi (0)

| Comando |
|---|---|

### Funzioni (12)

| Funzione | Ritorno |
|---|---|
| `BidAskSpread` | DoubleType |
| `CAPM` | DoubleType |
| `CapitalGainsYield` | DoubleType |
| `GeometricMeanReturn` | DoubleType |
| `GrowthRate` | DoubleType |
| `HoldingPeriodReturn` | DoubleType |
| `PreferredStockValue` | DoubleType |
| `RiskPremium` | DoubleType |
| `StockValueConstGrowth` | DoubleType |
| `StockValueZeroGrowth` | DoubleType |
| `TaxEquivalentYield` | DoubleType |
| `TotalStockReturn` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING.BACKTESTS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (1)

| Funzione | Ritorno |
|---|---|
| `RUN` | BacktestResultType |

### Tipi (2)

| Tipo |
|---|---|
| `BacktestResultType` |
| `RebalanceStrategyType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING.BONDS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (9)

| Funzione | Ritorno |
|---|---|
| `BondEquivalentYield` | DoubleType |
| `BondPrice` | DoubleType |
| `BondYield` | DoubleType |
| `CurrentYield` | DoubleType |
| `DURATION` | DoubleType |
| `PRICESIMPLE` | DoubleType |
| `YieldToMaturity` | DoubleType |
| `ZeroCouponBondValue` | DoubleType |
| `ZeroCouponBondYield` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING.METRICS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (32)

| Funzione | Ritorno |
|---|---|
| `Alpha` | DoubleType |
| `AnnualizedDownsideDeviation` | DoubleType |
| `AnnualizedReturn` | DoubleType |
| `AnnualizedTrackingError` | DoubleType |
| `AnnualizedVolatility` | DoubleType |
| `Beta` | DoubleType |
| `BookValuePerShare` | DoubleType |
| `CalmarRatio` | DoubleType |
| `ConditionalValueAtRisk` | DoubleType |
| `DPS` | DoubleType |
| `DilutedEPS` | DoubleType |
| `DividendPayoutRatio` | DoubleType |
| `DividendYield` | DoubleType |
| `DownsideDeviation` | DoubleType |
| `EPS` | DoubleType |
| `EquityMultiplier` | DoubleType |
| `InformationRatio` | DoubleType |
| `JarqueBeraTest` | ArrayType |
| `MaxDrawdown` | DoubleType |
| `NAV` | DoubleType |
| `OmegaRatio` | DoubleType |
| `PBRatio` | DoubleType |
| `PERatio` | DoubleType |
| `PSRatio` | DoubleType |
| `ReturnsKurtosis` | DoubleType |
| `ReturnsSkewness` | DoubleType |
| `SHARPERATIO` | DoubleType |
| `SortinoRatio` | DoubleType |
| `TotalReturn` | DoubleType |
| `TreynorRatio` | DoubleType |
| `UlcerIndex` | DoubleType |
| `ValueAtRisk` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING.OPTIONS

### Comandi (0)

| Comando |
|---|---|

### Funzioni (16)

| Funzione | Ritorno |
|---|---|
| `BinomialOption` | DoubleType |
| `BsCall` | DoubleType |
| `BsPut` | DoubleType |
| `Charm` | DoubleType |
| `Delta` | DoubleType |
| `DeltaSpeed` | DoubleType |
| `Gamma` | DoubleType |
| `GammaColor` | DoubleType |
| `MertonCall` | DoubleType |
| `MertonPut` | DoubleType |
| `Rho` | DoubleType |
| `Theta` | DoubleType |
| `Vanna` | DoubleType |
| `Vega` | DoubleType |
| `Volga` | DoubleType |
| `Zomma` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TRADING.PORTFOLIO

### Comandi (0)

| Comando |
|---|---|

### Funzioni (19)

| Funzione | Ritorno |
|---|---|
| `BLACKLITTERMAN` | PortfolioResultType |
| `CVAR` | DoubleType |
| `EQUALLYWEIGHTED` | PortfolioResultType |
| `EVOLVE` | PortfolioResultType |
| `EXPECTEDRETURN` | DoubleType |
| `ISLONGONLY` | BitType |
| `KELLYCRITERION` | PortfolioResultType |
| `MAXRETURN` | PortfolioResultType |
| `MAXSHARPE` | PortfolioResultType |
| `MEANVARIANCE` | PortfolioResultType |
| `MINCDAR` | PortfolioResultType |
| `MINCVAR` | PortfolioResultType |
| `MINVOLATILITY` | PortfolioResultType |
| `PORTFOLIOMAD` | DoubleType |
| `PORTFOLIOSEMIMAD` | DoubleType |
| `PVAR` | DoubleType |
| `RISKPARITY` | PortfolioResultType |
| `SHARPERATIO` | DoubleType |
| `STDEV` | DoubleType |

### Tipi (3)

| Tipo |
|---|---|
| `GaParametersType` |
| `PortfolioConstraintsType` |
| `PortfolioResultType` |

### Costanti (0)

| Costante |
|---|---|

## Namespace TRAY

### Comandi (0)

| Comando |
|---|---|

### Funzioni (10)

| Funzione | Ritorno |
|---|---|
| `TRAYAVAILABLE` | LongType |
| `TRAYBACKEND` | StringType |
| `TRAYCREATE` | LongType |
| `TRAYITEM` | LongType |
| `TRAYLAST` | LongType |
| `TRAYPOLL` | LongType |
| `TRAYREMOVE` | LongType |
| `TRAYSELFTEST` | LongType |
| `TRAYSEP` | LongType |
| `TRAYTOOLTIP` | LongType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace TYPEINFO

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `HAS` | DoubleType |
| `METHODINFO` | StringType |
| `PROPERTYINFO` | StringType |
| `TYPEINFO` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace UI

### Comandi (0)

| Comando |
|---|---|

### Funzioni (4)

| Funzione | Ritorno |
|---|---|
| `Click` | BitType |
| `Find` | StringType |
| `FindAll` | ArrayType |
| `Type` | BitType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace VECTOR

### Comandi (9)

| Comando |
|---|---|
| `ADD` |
| `ADD WRAP` |
| `BOUNCE2D` |
| `DIV` |
| `FILL RANDOM` |
| `FUNCTION` |
| `MUL` |
| `SET` |
| `WRAP` |

### Funzioni (7)

| Funzione | Ritorno |
|---|---|
| `CHEBYSHEV` | DoubleType |
| `COSINE` | DoubleType |
| `CREATE` | LongType |
| `EUCLIDEA` | DoubleType |
| `MANHATTAN` | DoubleType |
| `MINKOWSKI` | DoubleType |
| `PRODUCT` | DoubleType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace VECTORDB

### Comandi (0)

| Comando |
|---|---|

### Funzioni (28)

| Funzione | Ritorno |
|---|---|
| `AbTest` | StringType |
| `Baseline` | StringType |
| `Calibrate` | StringType |
| `Compare` | StringType |
| `Completeness` | DoubleType |
| `Count` | LongType |
| `Diagnose` | StringType |
| `DiagnoseReport` | StringType |
| `Drift` | DoubleType |
| `Embed` | ArrayType |
| `EnsureCollection` | DoubleType |
| `EvalDiff` | StringType |
| `EvalGate` | StringType |
| `Faithfulness` | DoubleType |
| `GenTest` | StringType |
| `Hallucination` | DoubleType |
| `IndexMeta` | LongType |
| `IndexText` | LongType |
| `ListCollections` | StringType |
| `LoadDocuments` | IntegerType |
| `Mrr` | DoubleType |
| `NdcgAt` | DoubleType |
| `PrecisionAt` | DoubleType |
| `PurgeDeprecated` | DoubleType |
| `RecallAt` | DoubleType |
| `Relevance` | DoubleType |
| `RemoveSource` | DoubleType |
| `Search` | StringType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

## Namespace WINDOWS

### Comandi (14)

| Comando |
|---|---|
| `CLEAR` |
| `DEFINE` |
| `DESKTOP` |
| `EDIT` |
| `FRAME` |
| `PRINT` |
| `SCROLL` |
| `SELECT` |
| `SET` |
| `SHADOW` |
| `SHOW` |
| `TEXT` |
| `THEME` |
| `VIEWPORT` |

### Funzioni (7)

| Funzione | Ritorno |
|---|---|
| `DESKTOP` | IntegerType |
| `HEIGHT` | IntegerType |
| `THEME` | StringType |
| `WIDTH` | IntegerType |
| `WINDOWCLOSED` | IntegerType |
| `X` | IntegerType |
| `Y` | IntegerType |

### Tipi (0)

| Tipo |
|---|---|

### Costanti (0)

| Costante |
|---|---|

