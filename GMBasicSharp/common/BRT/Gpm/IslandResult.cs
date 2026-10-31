// FILE: Basic.Runtime.Gpm.IslandResult.cs
// ISLAND CONTRACT — esito di un'isola nativa (GPM⇄IL bridge).
// Un'isola nativa è un leaf compilato in IL (IlBackend) che il PCodeProcessor esegue
// al posto del bytecode P-CODE. L'isola gira SUL THREAD del worker (mai su un thread
// proprio), legge/scrive lo stato del PCodeTask passato e restituisce un esito.

namespace Basic.Runtime.Gpm;

/// <summary>
/// Esito di <c>RunIsland(PCodeTask)</c>. Il valore di ritorno è LOCALE sullo stack del
/// thread chiamante — mai un campo condiviso sul task (stessa disciplina di
/// <see cref="PCodeProcessor.ExecuteTaskTick"/>: un campo scrivibile da un altro thread
/// che esegua un tick successivo dello stesso task è esattamente la classe di bug di
/// doppia esecuzione §16.4).
/// </summary>
public enum IslandResult
{
    /// <summary>L'isola ha completato un'unità atomica senza bloccare. Il driver può
    /// proseguire il tick del task normalmente (es. incrementando Ip oltre l'isola).</summary>
    Continue,

    /// <summary>L'isola ha parcheggiato il task su una struttura d'attesa (canale, waitgroup,
    /// future, sleep) con la disciplina §16.3/§16.4. Il driver NON deve riaccodare il task —
    /// verrà risvegliato dalla struttura d'attesa via <c>WakeTask</c> e rientrerà nel
    /// checkpoint della sub-isola salvata.</summary>
    Blocked,
}
