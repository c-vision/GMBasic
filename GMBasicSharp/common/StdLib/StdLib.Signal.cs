// FILE: Interpreter.StdLib.Signal.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Signal — namespace 'SIGNAL'
 * 18 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (18):
 *   BLACKMAN
 *   BUTTER
 *   CHEBY1
 *   CHEBY2
 *   CONVOLVE
 *   CORRELATE
 *   DECIMATE
 *   ELLIP
 *   FILTFILT
 *   FINDPEAKS
 *   FIRWIN
 *   FREQZ
 *   HAMMING
 *   HANN
 *   LFILTER
 *   RESAMPLE
 *   SAVGOLFILTER
 *   WELCH
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SIGNAL NAMESPACE (nucleo scipy.signal) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra le definizioni per il namespace 'SIGNAL': convoluzione,
    /// correlazione 1D e finestre classiche. Implementazione C#:
    /// CommonLib.Numerics (SignalConvolve/SignalCorrelate/SignalHann/...).
    /// </summary>
    private void CreateSignalNamespace()
    {
        const string namespaceName = "SIGNAL";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new ArrayType(new DoubleType()), noFlags, null, false);
        var kernelArrayParam = new Parameter(-1, T(TokenKind.Identifier, "kernel"), new ArrayType(new DoubleType()), noFlags, null, false);
        var nParam = new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(), noFlags, null, false);
        var wnArrayParam = new Parameter(-1, T(TokenKind.Identifier, "wn"), new ArrayType(new DoubleType()), noFlags, null, false);
        var btypeParam = new Parameter(-1, T(TokenKind.Identifier, "btype"), new StringType(), noFlags, null, false);
        var handleParam = new Parameter(-1, T(TokenKind.Identifier, "h"), new LongType(), noFlags, null, false);
        var doubleArray = new ArrayType(new DoubleType());

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalConvolve, "CONVOLVE"),
                [dataArrayParam, kernelArrayParam], doubleArray, TokenKind.SignalConvolve),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCorrelate, "CORRELATE"),
                [dataArrayParam, kernelArrayParam], doubleArray, TokenKind.SignalCorrelate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalHann, "HANN"),
                [nParam], doubleArray, TokenKind.SignalHann),
            // scipy.signal P0: FIRWIN (design FIR) e FINDPEAKS.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFirwin, "FIRWIN"),
                [new Parameter(-1, T(TokenKind.Identifier, "numtaps"), new IntegerType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "cutoff"), new ArrayType(new DoubleType()), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "btype"), new StringType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "window"), new StringType(), noFlags, null, false)],
                doubleArray, TokenKind.SignalFirwin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFindPeaks, "FINDPEAKS"),
                [dataArrayParam,
                 new Parameter(-1, T(TokenKind.Identifier, "distance"), new DoubleType(), noFlags, null, false)],
                doubleArray, TokenKind.SignalFindPeaks),
            // scipy.signal P1: welch, savgol_filter, resample, decimate.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalWelch, "WELCH"),
                [dataArrayParam,
                 new Parameter(-1, T(TokenKind.Identifier, "nperseg"), new IntegerType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "fs"), new DoubleType(), noFlags, null, false)],
                new LongType(), TokenKind.SignalWelch),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalSavgolFilter, "SAVGOLFILTER"),
                [dataArrayParam,
                 new Parameter(-1, T(TokenKind.Identifier, "window_length"), new IntegerType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "polyorder"), new IntegerType(), noFlags, null, false)],
                doubleArray, TokenKind.SignalSavgolFilter),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalResample, "RESAMPLE"),
                [dataArrayParam,
                 new Parameter(-1, T(TokenKind.Identifier, "num"), new IntegerType(), noFlags, null, false)],
                doubleArray, TokenKind.SignalResample),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalDecimate, "DECIMATE"),
                [dataArrayParam,
                 new Parameter(-1, T(TokenKind.Identifier, "q"), new IntegerType(), noFlags, null, false)],
                doubleArray, TokenKind.SignalDecimate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalHamming, "HAMMING"),
                [nParam], doubleArray, TokenKind.SignalHamming),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalBlackman, "BLACKMAN"),
                [nParam], doubleArray, TokenKind.SignalBlackman),

            // --- Filtri IIR (scipy.signal): design → handle; LFILTER/FILTFILT/FREQZ applicano ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalButter, "BUTTER"),
                [nParam, wnArrayParam, btypeParam], new LongType(), TokenKind.SignalButter),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCheby1, "CHEBY1"),
                [nParam,
                 new Parameter(-1, T(TokenKind.Identifier, "rp"), new DoubleType(), noFlags, null, false),
                 wnArrayParam, btypeParam], new LongType(), TokenKind.SignalCheby1),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalCheby2, "CHEBY2"),
                [nParam,
                 new Parameter(-1, T(TokenKind.Identifier, "rs"), new DoubleType(), noFlags, null, false),
                 wnArrayParam, btypeParam], new LongType(), TokenKind.SignalCheby2),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalEllip, "ELLIP"),
                [nParam,
                 new Parameter(-1, T(TokenKind.Identifier, "rp"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "rs"), new DoubleType(), noFlags, null, false),
                 wnArrayParam, btypeParam], new LongType(), TokenKind.SignalEllip),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalLfilter, "LFILTER"),
                [handleParam, dataArrayParam], doubleArray, TokenKind.SignalLfilter),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFiltfilt, "FILTFILT"),
                [handleParam, dataArrayParam], doubleArray, TokenKind.SignalFiltfilt),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SignalFreqz, "FREQZ"),
                [handleParam, nParam], doubleArray, TokenKind.SignalFreqz),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
