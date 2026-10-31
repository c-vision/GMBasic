// FILE: Basic.Databases.KeyExpressionPCodeHook.cs
// Hook di iniezione verso la pipeline unificata Compiler -> P-Code -> PCodeVm interno
// (DbExprPCodeEvaluator, common/IlBackend/) per le espressioni CHIAVE (SEEK/INDEX ON) — stesso
// pattern di RuntimeExpressionEvaluator.PCodeEvaluatorFactory (vedi quel file per il perché:
// questo progetto è sotto IlBackend nel grafo delle dipendenze, non può referenziare PCodeVm
// direttamente). IlBackend imposta CompileFactory una volta (vedi DbFilterHookInit).

namespace Basic.Databases;

public static class KeyExpressionPCodeHook
{
    /// <summary>Data una stringa espressione chiave, ritorna una funzione compilata che la
    /// valuta contro un contesto campi del record corrente — null se non impostato (IlBackend
    /// non caricato) o se l'espressione non è compilabile.</summary>
    public static Func<string, Func<IReadOnlyDictionary<string, object?>?, object?>?>? CompileFactory { get; set; }
}
