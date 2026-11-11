// FILE: CommonLib.Numerics.MatrixNativeKernels.cs
// Kernel matriciali CONDIVISI con i backend C via P/Invoke (C# → C, direzione unica).
//
// PERCHÉ. SINM/TANM/ORTH erano implementati due volte: MathNet/CommonLib nel managed
// (IL/PCodeVM) e `gmb_matrix_sinm/tanm/orth` nel runtime C (VM/C e C99Native). Le due
// implementazioni differiscono di pochi ulp; quando un sample stampa 15 cifre (parity4
// confronta il testo esatto) la differenza emerge (`MatrixFunctionsSciPyParity2`).
// Qui il managed chiama la STESSA funzione delle gambe C: una sola sorgente numerica,
// risultati bit-identici sulle 4 gambe.
//
// La lib `libgmb_matrix_flat` espone un'ABI a BUFFER PIATTI (nessun handle che attraversa
// il confine → nessun doppio registry). Se la lib non è disponibile il chiamante ricade
// sull'implementazione gestita: i backend managed restano autosufficienti.

using System;
using System.Runtime.InteropServices;
using System.Threading;
using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

internal static class MatrixNativeKernels
{
    /// <summary>Operazioni esposte da `gmb_matrix_flat_op` (gmb_matrix_flat.h).</summary>
    internal enum Op
    {
        Sin = 1,
        Tan = 2,
        Orth = 3,
    }

    private const string Lib = "gmb_matrix_flat";

    [DllImport(Lib, EntryPoint = "gmb_matrix_flat_op", CallingConvention = CallingConvention.Cdecl)]
    private static extern int FlatOp(double[] a, long rows, long cols, int op,
                                     double[] outBuf, long outCap,
                                     out long outRows, out long outCols);

    /// <summary>0 = non ancora provata, 1 = disponibile, -1 = assente/non caricabile.</summary>
    private static int _state;

    private static void MarkUnavailable() => Volatile.Write(ref _state, -1);

    /// <summary>
    /// Applica il kernel nativo. Ritorna false se la libreria non c'è (fallback gestito) o se
    /// il kernel rifiuta l'input (es. SINM/TANM su matrice non quadrata).
    /// </summary>
    internal static bool TryApply(Matrix<double> a, Op op, out Matrix<double> result)
    {
        result = null!;
        if (a == null) return false;
        if (Volatile.Read(ref _state) == -1) return false;

        int rows = a.RowCount;
        int cols = a.ColumnCount;
        if (rows <= 0 || cols <= 0) return false;

        // Il kernel nativo è row-major; MathNet è column-major: si appiattisce esplicitamente.
        var flat = new double[(long)rows * cols];
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                flat[(long)i * cols + j] = a[i, j];

        // ORTH può restituire n×rank (rank ≤ min(rows, cols)); SINM/TANM sono n×n.
        // La capacità rows*cols copre entrambi i casi.
        var outBuf = new double[flat.Length];
        long outRows, outCols;
        int ok;
        try
        {
            ok = FlatOp(flat, rows, cols, (int)op, outBuf, outBuf.Length, out outRows, out outCols);
            if (Volatile.Read(ref _state) == 0) Volatile.Write(ref _state, 1);
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }

        if (ok != 1 || outRows <= 0 || outCols <= 0) return false;
        int or = (int)outRows, oc = (int)outCols;
        if ((long)or * oc > outBuf.Length) return false;
        var m = new double[or, oc];
        for (int i = 0; i < or; i++)
            for (int j = 0; j < oc; j++)
                m[i, j] = outBuf[(long)i * oc + j];
        result = Matrix<double>.Build.DenseOfArray(m);
        return true;
    }
}
