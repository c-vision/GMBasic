// I test che eseguono stepper/driver GPM toccano lo stato GLOBALE del runtime IL
// (IlRuntime.Current è un singleton di processo, e Console.Out è globale). Se girano in
// parallelo, un test sovrascrive l'host/output di un altro → output vuoto / golden flaky.
// Questa collection li serializza. Le classi di sola ANALISI (MayParkAnalysisTests,
// GoRoutinesMinimalAnalysisTests) NON la usano e restano parallele.

using Xunit;

namespace Basic.IlBackend.Tests;

[CollectionDefinition("GpmRuntimeState", DisableParallelization = true)]
public class GpmRuntimeStateCollection
{
}
