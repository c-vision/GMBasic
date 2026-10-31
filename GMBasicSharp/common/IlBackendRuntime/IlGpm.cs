// FILE: Basic.IlBackend.IlGpm.cs
// IL GPM — driver chunk-resumeIp per il backend IL (vedi AiWiki
// GM-Basic-ILBackend-GPM-Chunk-State-Machine §9-§17, §26).
//
// Il backend IL NON interpreta bytecode: genera metodi CIL nativi. Il problema è che un
// metodo CIL non può essere sospeso a metà (stack managed + GC, §6). La soluzione: ogni
// procedura che può parcheggiare (may-park, §9.2/§13-2) viene compilata come uno STEPPER
// che a ogni chiamata esegue UN chunk (un blocco di statement senza punti di blocco) e
// ritorna. Il punto da cui riprendere è un resumeIp = task.Ip (indice di chunk), salvato
// nel PCodeCallFrame insieme ai locali. Questo file contiene:
//   - IlGpmStepResult           : esito di un tick (Continue/Blocked/Finished).
//   - IlGpmStep / IlGpmTaskEntry: firma degli stepper e catcher per la creazione task.
//   - IlGpmDispatcher           : connessione frame.OwnerProcedure → stepper.
//   - IlGpmDriver               : worker loop su PCodeScheduler + switch esito.
//
// Il driver NON conosce i singoli stepper (vivono nell'assembly generato): riceve un
// delegato `stepFor` da chi lo costruisce (IlBackend runtime) e lo usa per dispatch.
// Nessun accoppiamento BRT→IlBackend: BRT fornisce solo PCodeScheduler/PCodeTask.

using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

/// <summary>Esito di un tick dello stepper (una chiamata = un chunk eseguito).</summary>
public enum IlGpmStepResult
{
    /// <summary>Il chunk è terminato senza bloccare: il driver riaccoda il task (round-robin).</summary>
    Continue,
    /// <summary>Il chunk ha parcheggiato il task su una struttura d'attesa: il driver NON
    /// riaccoda; sarà WakeTask della struttura (canale/waitgroup/future/timer) a farlo.</summary>
    Blocked,
    /// <summary>Il task ha completato l'ultimo chunk (End/GO EXIT/ret del livello top):
    /// il driver risolve il future se GO FUNCTION, non riaccoda.</summary>
    Finished,
}
