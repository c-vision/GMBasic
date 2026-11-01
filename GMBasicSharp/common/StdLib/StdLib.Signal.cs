// FILE: Interpreter.StdLib.Signal.cs
// Namespace SIGNAL — segnali trading (sottoscrizione con filtro strutturato + EMIT locale),
// evento ON SIGNAL e FUNZIONI di elaborazione dei segnali (scipy.signal).
// Le funzioni sono quelle canoniche del catalogo (SIGNAL.HANN/CONVOLVE/BUTTER/...) e hanno
// l'implementazione nelle gambe: IlRuntime (IL/PCode) e gmb_builtin (VM/C) / emit_bf_signal
// (C99Native). Qui c'è solo la FIRMA visibile al linker.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Signal — namespace 'SIGNAL'
 * 18 funzioni | 1 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (18):
 *   BLACKMAN, BUTTER, CHEBY1, CHEBY2, CONVOLVE, CORRELATE, DECIMATE, ELLIP, FILTFILT,
 *   FINDPEAKS, FIRWIN, FREQZ, HAMMING, HANN, LFILTER, RESAMPLE, SAVGOLFILTER, WELCH
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
        var signalKernel = new Parameter(-1, T(TokenKind.Identifier, "k"), arrayOfDouble, noFlags, null, false);
        var signalN = new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false);
        var signalWn = new Parameter(-1, T(TokenKind.Identifier, "wn"), arrayOfDouble, noFlags, null, false);
        var signalBtype = new Parameter(-1, T(TokenKind.Identifier, "btype"), new StringType(), noFlags, null, false);
        var signalFilter = new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false);

        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            // Finestre (scipy.signal.windows.*)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalHann, "HANN"),
                [signalN], arrayOfDouble, TokenKind.SignalHann, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalHamming, "HAMMING"),
                [signalN], arrayOfDouble, TokenKind.SignalHamming, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalBlackman, "BLACKMAN"),
                [signalN], arrayOfDouble, TokenKind.SignalBlackman, ExecutionKind: NativeExecutionKind.Pure),

            // Convoluzione / correlazione
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalConvolve, "CONVOLVE"),
                [signalArray, signalKernel], arrayOfDouble, TokenKind.SignalConvolve, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCorrelate, "CORRELATE"),
                [signalArray, signalKernel], arrayOfDouble, TokenKind.SignalCorrelate, ExecutionKind: NativeExecutionKind.Pure),

            // Ricampionamento / decimazione
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalDecimate, "DECIMATE"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "q"), new LongType(), noFlags, null, false)
                ], arrayOfDouble, TokenKind.SignalDecimate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalResample, "RESAMPLE"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "num"), new LongType(), noFlags, null, false)
                ], arrayOfDouble, TokenKind.SignalResample, ExecutionKind: NativeExecutionKind.Pure),

            // Design dei filtri IIR (handle) + applicazione
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalButter, "BUTTER"),
                [signalN, signalWn, signalBtype], new LongType(), TokenKind.SignalButter, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCheby1, "CHEBY1"),
                [
                    signalN,
                    new Parameter(-1, T(TokenKind.Identifier, "rp"), new DoubleType(), noFlags, null, false),
                    signalWn, signalBtype
                ], new LongType(), TokenKind.SignalCheby1, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCheby2, "CHEBY2"),
                [
                    signalN,
                    new Parameter(-1, T(TokenKind.Identifier, "rs"), new DoubleType(), noFlags, null, false),
                    signalWn, signalBtype
                ], new LongType(), TokenKind.SignalCheby2, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalEllip, "ELLIP"),
                [
                    signalN,
                    new Parameter(-1, T(TokenKind.Identifier, "rp"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "rs"), new DoubleType(), noFlags, null, false),
                    signalWn, signalBtype
                ], new LongType(), TokenKind.SignalEllip, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalLfilter, "LFILTER"),
                [signalFilter, signalArray], arrayOfDouble, TokenKind.SignalLfilter, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFiltfilt, "FILTFILT"),
                [signalFilter, signalArray], arrayOfDouble, TokenKind.SignalFiltfilt, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFreqz, "FREQZ"),
                [signalFilter, signalN], arrayOfDouble, TokenKind.SignalFreqz, ExecutionKind: NativeExecutionKind.Pure),

            // FIR + picchi
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFirwin, "FIRWIN"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "numtaps"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "cutoff"), arrayOfDouble, noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "btype"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "window"), new StringType(), noFlags, null, false)
                ], arrayOfDouble, TokenKind.SignalFirwin, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFindPeaks, "FINDPEAKS"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "distance"), new DoubleType(), noFlags, null, false)
                ], arrayOfDouble, TokenKind.SignalFindPeaks, ExecutionKind: NativeExecutionKind.Pure),

            // Spettro (Welch) e filtro di Savitzky-Golay
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalWelch, "WELCH"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "nperseg"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "fs"), new DoubleType(), noFlags, null, false)
                ], new LongType(), TokenKind.SignalWelch, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalSavgolFilter, "SAVGOLFILTER"),
                [
                    signalArray,
                    new Parameter(-1, T(TokenKind.Identifier, "window_length"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "polyorder"), new LongType(), noFlags, null, false)
                ], arrayOfDouble, TokenKind.SignalSavgolFilter, ExecutionKind: NativeExecutionKind.Pure));

        RegisterBuiltinNamespace(namespaceName, procedures,
            ImmutableDictionary<string, DataType>.Empty);
    }
}
