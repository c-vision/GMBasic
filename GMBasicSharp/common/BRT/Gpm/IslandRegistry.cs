// FILE: Basic.Runtime.Gpm.IslandRegistry.cs
// ISLAND REGISTRY — ponte GPM⇄IL a RUNTIME.
//
// Dipendenze: BRT è l'unico assembly condiviso da IlBackend (che REGISTRA) e PCodeVm
// (che CERCA). Nessun ciclo: BRT dipende solo da CommonLib, IlBackend e PCodeVm
// dipendono entrambi da BRT.
//
// A compile-time IlBackend compila le procedure native-eligible (IslandEligibility)
// in "isole" — metodi IL nativi con firma <c>IslandResult Run(PCodeTask)</c> — e le
// registra qui per nome. A dispatch-time la P-CODE VM, prima di saltare al bytecode di
// una procedura (OP_CALL), consulta questo registro: se per quel nome esiste un'isola,
// la esegue nativamente invece di interpretare il P-Code (fast path non bloccante).
//
// L'isola compila SENZA punti di pre-empt: se il corpo promette di non bloccare mai,
// l'isola può girare in un unico passo atomico e ritornare <see cref="IslandResult.Continue"/>,
// e il driver prosegue il tick normalmente. Se un'isola finisce per dover parcheggiare,
// ritorna <see cref="IslandResult.Blocked"/> DOPO aver usato IslandRuntime per preparare lo
// stato di retry (§16.3/§16.4) — il driver NON ri-accoda il task.

using System.Collections.Concurrent;

namespace Basic.Runtime.Gpm;

/// <summary>
/// Firma di un'isola nativa (leaf IL compilato dal IlBackend).
/// Stesso profilo del dispatcher: evita scambio di frame; l'isola scrive/esce
/// direttamente sullo stato del <see cref="PCodeTask"/> passato.
/// </summary>
public delegate IslandResult IslandRun(PCodeTask task);

/// <summary>
/// Registro process-console-globale delle isole native compilate da IlBackend, chiavato
/// per nome procedura (StringComparer.OrdinalIgnoreCase). Popolato UNA VOLTA a program-load
/// (before any task starts), letto in dispatch-time dal driver: la VM chiede
/// <see cref="TryGet(PCodeTask, string, out IslandRun)"/> prima di eseguire il bytecode di
/// una procedura. Immutabile dopo init (no locking sul path di lettura caldo).
/// </summary>
public static class IslandRegistry
{
    private static readonly ConcurrentDictionary<string, IslandRun> _islands =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Versione del registro, incrementata a ogni mutazione. Il driver del P-CODE VM la usa
    /// per invalidare la sua cache indirizzo→isola (rebuilt lazily al primo dispatch dopo un
    /// cambio). Evita di leggere il registry per nome a ogni OP_CALL (hot path) mantenendo però
    /// il registro come UNICA fonte di verità: se un'isola viene registrata DOPO la costruzione
    /// del PCodeProcessor (tipico: registrazione a program-load, VM costruita per-run prima),
    /// la cache si ricostruisce da sola al dispatch successivo.
    /// </summary>
    private static int _versionField;

    /// <summary>Valore corrente di <see cref="_versionField"/> (read atomica).</summary>
    public static int Version => Volatile.Read(ref _versionField);

    /// <summary>Registra un'isola nativa per una procedura. Sovrascrive una precedente
    /// omonima (ultima scrittura vince). Da chiamare UNA VOLTA per tipo di programma, prima
    /// che qualsiasi task parta (compile-time / widow dell'entry point).</summary>
    public static void Register(string procedureName, IslandRun run)
    {
        if (string.IsNullOrEmpty(procedureName)) return;
        _islands[procedureName] = run;
        Interlocked.Increment(ref _versionField);
    }

    /// <summary>True se esiste un'isola registrata per <paramref name="procedureName"/>.</summary>
    public static bool IsRegistered(string procedureName)
        => _islands.ContainsKey(procedureName);

    /// <summary>Cerca un'isola per nome procedura. True se presente (in <paramref name="run"/>).</summary>
    public static bool TryGet(string procedureName, out IslandRun run)
        => _islands.TryGetValue(procedureName ?? string.Empty, out run!);

    /// <summary>Rimuove un'isola (es. per ricompilazione o reset della sessione REPL).</summary>
    public static void Unregister(string procedureName)
    {
        if (_islands.TryRemove(procedureName, out _))
            Interlocked.Increment(ref _versionField);
    }

    /// <summary>Azzera il registro (nuovo programma / nuova compilazione).</summary>
    public static void Clear()
    {
        _islands.Clear();
        Interlocked.Increment(ref _versionField);
    }
}