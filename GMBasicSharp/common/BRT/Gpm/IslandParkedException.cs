// FILE: Basic.Runtime.Gpm.IslandParkedException.cs
// ISLAND PARKED EXCEPTION — meccanismo di unwinding per parcheggiare un'isola nativa.
//
// Quando un'isola nativa (eseguita come IL in-process) incontra un'operazione bloccante
// (SEND/RECV/WAITGROUP/FUTURE/HTTP/FILE I/O), il helper IslandRuntime lancia questa
// eccezione per annullare lo stack CIL e tornare immediatamente al driver della P-CODE VM,
// che riceve IslandResult.Blocked e non riaccoda il task.
//
// Il task è stato già parcheggiato (State=Blocked, registrato sulla struttura d'attesa)
// DAL helper stesso, atomicamente sotto lock — prima del throw. Il catch nello shim
// (IlIslandShimCompiler) si limita a propagare IslandResult.Blocked.
//
// Al risveglio, la P-CODE VM ri-esegue l'isola dall'inizio (il frame è ancora valido,
// gli argomenti sono ancora sullo stack) e l'operazione bloccante ritenta — o riesce,
// o rilancia l'eccezione per parcheggiare di nuovo.

namespace Basic.Runtime.Gpm;

public sealed class IslandParkedException : Exception
{
    public IslandParkedException() : base("Island parked: the task has been registered on a waiting structure.") { }
}
