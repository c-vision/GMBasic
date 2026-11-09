// FILE: Interpreter.StdLib.Signal.cs
// Namespace SIGNAL — segnali trading (sottoscrizione con filtro strutturato + EMIT locale),
// evento ON SIGNAL e FUNZIONE di densità spettrale (scipy.signal).
// Le altre 17 funzioni del dominio (finestre, convoluzione/correlazione, filtri IIR, FIR,
// picchi, ricampionamento, savgol) sono migrate alla lib FFI `signal`
// (shared/libs/signal + shared/include/signal.inc): qui resta solo WELCH, che ritorna un
// handle di matrice e quindi dipende dal registro delle matrici del runtime.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Signal — namespace 'SIGNAL'
 * 1 funzioni | 1 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   WELCH
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

        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayOfDouble = new ArrayType(new DoubleType());
        var signalArray = new Parameter(-1, T(TokenKind.Identifier, "x"), arrayOfDouble, noFlags, null, false);
        var signalN = new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // Densità spettrale di potenza (Welch): handle di matrice (nfreq × 2).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalWelch, "WELCH"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "nperseg"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "fs"), new DoubleType(), noFlags, null, false)
                ], new LongType(), TokenKind.SignalWelch, ExecutionKind: NativeExecutionKind.Pure));

        RegisterBuiltinNamespace(namespaceName, procedures,
            ImmutableDictionary<string, DataType>.Empty);
    }
}
