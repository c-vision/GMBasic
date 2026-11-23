// FILE: Interpreter.StdLib.Signal.cs
// Namespace SIGNAL — segnali trading (sottoscrizione con filtro strutturato + EMIT locale),
// evento ON SIGNAL.
// Tutte le funzioni del dominio (finestre, convoluzione/correlazione, filtri IIR, FIR,
// picchi, ricampionamento, savgol, welch) sono nella lib FFI `signal`
// (shared/libs/signal + shared/include/signal.inc).

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Signal — namespace 'SIGNAL'
 * 0 funzioni | 1 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (0):
 * COMANDI (1):
 *   SIGNAL
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateSignalNamespace()
    {
        const string namespaceName = "SIGNAL";

        // Comando registrato per NOME (SIGNAL non è una keyword del linguaggio, come CONTROL
        // e i controlli pixel): richiede IMPORT SIGNAL, quindi non collide con variabili o UDF
        // chiamate SIGNAL nei programmi che non importano il namespace.
        RegisterCommandByName("SIGNAL", namespaceName, Parser.ParseSignalStatement);

        // Le procedure del dominio (finestre, filtri, WELCH, SAVGOL, …) sono nel pacchetto FFI
        // `signal`: `shared/libs/signal` + `shared/include/signal.inc` (2026-11-23).
        RegisterBuiltinNamespace(namespaceName, [], ImmutableDictionary<string, DataType>.Empty);
    }
}
