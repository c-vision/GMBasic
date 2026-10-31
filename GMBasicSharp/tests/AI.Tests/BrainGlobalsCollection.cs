// FILE: AI.Tests.BrainGlobalsCollection
// Le suite che toccano lo stato PROCESS-WIDE del second brain (AiVectorRuntime.SqlDataSession,
// EmbeddingProviderResolver.Default/ActiveDimensions, AiConfig.{Brains,Rag,Graphs}DefaultDir,
// BrainRegistry) NON possono girare in parallelo fra loro: si scambiano la sessione SQLite e il
// provider di embedding e i risultati diventano vuoti/non deterministici (osservato 2026-09-16:
// MemoryBrainTests falliva solo nella suite completa, non da sola né in coppia).
using Xunit;

namespace AI.Tests;

[CollectionDefinition("BrainGlobals", DisableParallelization = true)]
public sealed class BrainGlobalsCollection;
