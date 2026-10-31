// FILE: Basic.Agents.GpmWorkerGuard — hook di sicurezza per il bug (open, 2026-09-08)
// "AgentBatch-AiAgentSwarm-Conflitto-GPM": AgentBatch/AgentDag/Blackboard.Wait bloccano il
// thread chiamante (Task.WaitAll/Task.Wait) — vietato dalla regola GPM "un worker non può mai
// bloccarsi" quando chiamati da dentro una goroutine (GO SUB/GO FUNCTION).
//
// AI.csproj (questo file) non referenzia IlBackendRuntime.csproj (dove vive la GPM IL) — e non
// deve, altrimenti si crea un riferimento circolare (IlBackendRuntime -> AI è la direzione
// esistente). Per questo il controllo "sto girando su un worker GPM?" è un hook a inversione di
// controllo: il backend IL lo registra a startup (vedi IlGpmRuntime), questo file resta
// agnostico rispetto a QUALE backend gira sopra.
//
// Fix scelto: guardia esplicita fail-fast, non la riscrittura park-based completa (che
// richiederebbe far restituire un PCodeFutureValue a queste chiamate e integrarle nel
// meccanismo GO FUNCTION...INTO/TryReadFuture — refactor profondo, esplicitamente rimandato
// come follow-on nel TODO GMBasic-Agent-Runtime-Async-DAG-Verifier-Blackboard). Fallire subito
// con un errore chiaro è preferibile allo stallo silenzioso di un worker (stesso principio già
// applicato altrove nel backend IL, vedi IlGpmRuntime.ThrowIfWorker).
using System;

namespace Basic.Agents;

public static class GpmWorkerGuard
{
    /// <summary>Registrato dal backend IL attivo (IlGpmRuntime) a startup. Null = nessun
    /// backend GPM-aware registrato (es. test unitari diretti su AgentBatch/Blackboard) —
    /// in quel caso il controllo è un no-op, coerente col comportamento pre-esistente.</summary>
    public static Func<bool>? IsOnGpmWorker;

    /// <summary>Lancia se il chiamante gira su un worker GPM: <paramref name="op"/> userebbe
    /// un'attesa sincrona (Task.WaitAll/Task.Wait) che bloccherebbe il worker invece di
    /// parcheggiare, con rischio di stallo degli altri task multiplexati sullo stesso worker.</summary>
    public static void Check(string op)
    {
        if (IsOnGpmWorker?.Invoke() == true)
            throw new InvalidOperationException(
                $"{op}: non può essere chiamata da dentro una goroutine GPM (GO SUB/GO FUNCTION IN wg) — " +
                "l'implementazione attuale attende in modo sincrono (Task.WaitAll/Task.Wait) e bloccherebbe " +
                "il worker GPM. Chiamarla solo dal flusso principale sincrono (fuori da GO SUB), finché non " +
                "esiste una versione park-based (vedi Bugs/(open) - AgentBatch-AiAgentSwarm-Conflitto-GPM.md).");
    }
}
