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

    // ── ABI handle-based del runtime (`gmb_matrix_*`) ────────────────────────────────────────
    // Le operazioni che non hanno una forma `flat_op` (costruttori, norme scalari, elementwise
    // via autovettori) esistono nel runtime con handle. Il managed crea l'handle, scrive i dati,
    // chiama l'operazione e rilegge: il **calcolo resta solo in C** (una sola fonte, 2026-11-19).
    [DllImport(Lib, EntryPoint = "gmb_matrix_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern long MatrixCreate(int rows, int cols);
    [DllImport(Lib, EntryPoint = "gmb_matrix_set", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MatrixSet(long h, int r, int c, double v);
    [DllImport(Lib, EntryPoint = "gmb_matrix_get", CallingConvention = CallingConvention.Cdecl)]
    private static extern double MatrixGet(long h, int r, int c);
    [DllImport(Lib, EntryPoint = "gmb_matrix_rows", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MatrixRows(long h);
    [DllImport(Lib, EntryPoint = "gmb_matrix_cols", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MatrixCols(long h);
    [DllImport(Lib, EntryPoint = "gmb_matrix_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MatrixFree(long h);

    [DllImport(Lib, EntryPoint = "gmb_matrix_toeplitz", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeToeplitz(double[] c, double[] r, int n, int m);
    [DllImport(Lib, EntryPoint = "gmb_matrix_hankel", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeHankel(double[] c, double[] r, int n, int m);
    [DllImport(Lib, EntryPoint = "gmb_matrix_circulant", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeCirculant(double[] c, int n);
    [DllImport(Lib, EntryPoint = "gmb_matrix_cond", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeCond(long h);
    [DllImport(Lib, EntryPoint = "gmb_matrix_nuclear_norm", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeNuclearNorm(long h);
    [DllImport(Lib, EntryPoint = "gmb_matrix_signm", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeSignm(long h);
    [DllImport(Lib, EntryPoint = "gmb_matrix_fractional_power", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeFractionalPower(long h, double p);
    /* il buffer degli angoli è malloc'd dal kernel: va liberato con `free` */
    [DllImport("libc", EntryPoint = "free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void libc_free(IntPtr p);

    [DllImport(Lib, EntryPoint = "gmb_matrix_subspace_angles", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSubspaceAngles(long a, long b, out IntPtr outBuf);

    /// <summary>Matrice gestita → handle del runtime. false se la lib non è caricabile.</summary>
    private static bool TryToNative(Matrix<double> a, out long h)
    {
        h = -1;
        if (a is null) return false;
        try
        {
            h = MatrixCreate(a.RowCount, a.ColumnCount);
            if (h <= 0) { h = -1; return false; }
            for (var i = 0; i < a.RowCount; i++)
                for (var j = 0; j < a.ColumnCount; j++)
                    MatrixSet(h, i, j, a[i, j]);
            if (Volatile.Read(ref _state) == 0) Volatile.Write(ref _state, 1);
            return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); h = -1; return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); h = -1; return false; }
        catch (BadImageFormatException) { MarkUnavailable(); h = -1; return false; }
    }

    /// <summary>Handle del runtime → matrice gestita (poi libera l'handle).</summary>
    private static Matrix<double>? FromNative(long h)
    {
        if (h <= 0) return null;
        try
        {
            var rows = MatrixRows(h); var cols = MatrixCols(h);
            if (rows <= 0 || cols <= 0) return null;
            var m = new double[rows, cols];
            for (var i = 0; i < rows; i++)
                for (var j = 0; j < cols; j++)
                    m[i, j] = MatrixGet(h, i, j);
            return Matrix<double>.Build.DenseOfArray(m);
        }
        finally { MatrixFree(h); }
    }

    private static bool TryWrapF2(Matrix<double> a, Func<long, long> op, out Matrix<double> result)
    {
        result = null!;
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var outH = op(h);
            var m = FromNative(outH);
            if (m is null) return false;
            result = m;
            return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    internal static bool TrySignm(Matrix<double> a, out Matrix<double> result)
        => TryWrapF2(a, NativeSignm, out result);

    internal static bool TryFractionalPower(Matrix<double> a, double p, out Matrix<double> result)
    {
        result = null!;
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var m = FromNative(NativeFractionalPower(h, p));
            if (m is null) return false;
            result = m;
            return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    internal static bool TryCond(Matrix<double> a, out double result)
    {
        result = 0;
        if (!TryToNative(a, out var h)) return false;
        try { result = NativeCond(h); return true; }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    internal static bool TryNuclearNorm(Matrix<double> a, out double result)
    {
        result = 0;
        if (!TryToNative(a, out var h)) return false;
        try { result = NativeNuclearNorm(h); return true; }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    internal static bool TryToeplitz(double[] c, double[] r, out Matrix<double> result)
    {
        result = null!;
        try
        {
            var m = FromNative(NativeToeplitz(c, r, c.Length, r.Length));
            if (m is null) return false;
            result = m; return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
    }

    internal static bool TryHankel(double[] c, double[] r, out Matrix<double> result)
    {
        result = null!;
        try
        {
            var m = FromNative(NativeHankel(c, r, c.Length, r.Length));
            if (m is null) return false;
            result = m; return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
    }

    internal static bool TryCirculant(double[] c, out Matrix<double> result)
    {
        result = null!;
        try
        {
            var m = FromNative(NativeCirculant(c, c.Length));
            if (m is null) return false;
            result = m; return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
    }

    internal static bool TrySubspaceAngles(Matrix<double> a, Matrix<double> b, out double[] result)
    {
        result = [];
        if (!TryToNative(a, out var ha)) return false;
        try
        {
            if (!TryToNative(b, out var hb)) return false;
            try
            {
                /* il kernel ritorna il NUMERO di angoli (non 0) e un buffer malloc'd:
                 * va copiato e liberato (prima il `!= 0` lo trattava come errore). */
                var n = NativeSubspaceAngles(ha, hb, out var p);
                if (n <= 0 || p == IntPtr.Zero) return false;
                try
                {
                    var arr = new double[n];
                    Marshal.Copy(p, arr, 0, n);
                    result = arr;
                    return true;
                }
                finally { try { libc_free(p); } catch { /* piattaforma senza libc: buffer perso, non fatale */ } }
            }
            finally { MatrixFree(hb); }
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(ha); }
    }

}
