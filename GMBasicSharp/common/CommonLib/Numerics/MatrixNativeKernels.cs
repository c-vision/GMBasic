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
        Cos = 4,   /* matrix cosine, kernel `gmb_matrix_cosm` (2026-11-19) */
    }

    private const string Lib = "gmb_matrix_flat";

    [DllImport(Lib, EntryPoint = "gmb_matrix_solve_lin", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MatrixSolveLin(long n, double[] a, double[] b, double[] x, double tol);

    /// <summary>
    /// Risolve A·x = b (A n×n row-major) con l'UNICO solver del runtime (`gmb_solve_lin_tol`,
    /// eliminazione di Gauss a pivot parziale). `tol` è la soglia di singolarità.
    /// A e b non vengono modificati (il kernel ne fa una copia).
    /// </summary>
    internal static bool SolveLinear(double[] a, double[] b, double[] x, double tol)
        => MatrixSolveLin(x.Length, a, b, x, tol) != 0;

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
    [DllImport(Lib, EntryPoint = "gmb_matrix_solve_triangular", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeSolveTriangular(long a, double[] b, int nb, int lower);
    [DllImport(Lib, EntryPoint = "gmb_matrix_polar", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativePolar(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_decomp_get_factor", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeDecompGetFactor(long decomp, int idx);
    [DllImport(Lib, EntryPoint = "gmb_matrix_decomp_get_factor_ex", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeDecompGetFactorEx(long decomp, int idx, out int owned);
    [DllImport(Lib, EntryPoint = "gmb_matrix_eig", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeEig(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_schur", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeSchur(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_lu", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeLu(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_qr", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeQr(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_svd", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeSvd(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_evd", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeEvd(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_cholesky", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeCholesky(long a);
    [DllImport(Lib, EntryPoint = "gmb_matrix_decomp_get_factor_named_ex", CallingConvention = CallingConvention.Cdecl)]
    private static extern long NativeDecompGetFactorNamedEx(long decomp, string name, out int owned);
    [DllImport(Lib, EntryPoint = "gmb_matrix_decomp_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeDecompFree(long decomp);

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
    /// <summary>Legge un handle del runtime SENZA liberarlo (fattori posseduti da una decomposizione).</summary>
    private static Matrix<double>? ReadDense(long h)
    {
        if (h <= 0) return null;
        var rows = MatrixRows(h); var cols = MatrixCols(h);
        if (rows <= 0 || cols <= 0) return null;
        var m = new double[rows, cols];
        for (var i = 0; i < rows; i++)
            for (var j = 0; j < cols; j++)
                m[i, j] = MatrixGet(h, i, j);
        return Matrix<double>.Build.DenseOfArray(m);
    }

    private static Matrix<double>? FromNative(long h)
    {
        if (h <= 0) return null;
        try { return ReadDense(h); }
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

    /// <summary>scipy.linalg.solve_triangular — sostituzione in avanti/indietro (kernel C N×1).</summary>
    internal static bool TrySolveTriangular(Matrix<double> a, double[] b, bool lower, out double[] result)
    {
        result = [];
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var m = FromNative(NativeSolveTriangular(h, b, b.Length, lower ? 1 : 0));
            if (m is null) return false;
            if (m.ColumnCount == 1)
            {
                var v = new double[m.RowCount];
                for (var i = 0; i < v.Length; i++) v[i] = m[i, 0];
                result = v;
            }
            else
            {
                var v = new double[m.RowCount * m.ColumnCount];
                for (var i = 0; i < m.RowCount; i++)
                    for (var j = 0; j < m.ColumnCount; j++) v[i * m.ColumnCount + j] = m[i, j];
                result = v;
            }
            return true;
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    /// <summary>
    /// scipy.linalg.polar — A = U·P. Il kernel C ritorna una DECOMPOSIZIONE (tipo 5: fattori
    /// U = idx 0, P = idx 1); i fattori sono posseduti dalla decomposizione (non vanno liberati
    /// separatamente: si libera solo l'handle della decomposizione).
    /// </summary>
    internal static bool TryPolar(Matrix<double> a, out Matrix<double> u, out Matrix<double> p)
    {
        u = null!; p = null!;
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var dec = NativePolar(h);
            if (dec <= 0) return false;
            try
            {
                var um = ReadDense(NativeDecompGetFactor(dec, 0));
                var pm = ReadDense(NativeDecompGetFactor(dec, 1));
                if (um is null || pm is null) return false;
                u = um; p = pm;
                return true;
            }
            finally { NativeDecompFree(dec); }
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    /// <summary>Fattori di EIG/SCHUR restituiti dal runtime C (matrici già materializzate).</summary>
    internal sealed class EigResult
    {
        internal double[,] T = null!;   // forma di Schur reale (quasi-triangolare)
        internal double[,] Z = null!;   // trasformazione ortogonale (T = Z^T A Z)
        internal double[] D = [];       // parti reali degli autovalori
        internal double[] E = [];       // parti immaginarie degli autovalori
        internal double[,]? VR;         // autovettori reali (null se non richiesti)
        internal double[,]? VI;         // autovettori immaginari (null se non richiesti)
    }

    /// <summary>
    /// EIG/SCHUR: forma di Schur (T, Z) + autovalori e, se richiesti, autovettori destri.
    /// Il runtime C espone due decomposizioni: `gmb_matrix_schur` (tipo 7: T, Z) e
    /// `gmb_matrix_eig` (tipo 6: DREAL, DIMAG, VRREAL, VRIMAG). I fattori DREAL/DIMAG
    /// sono handle NUOVI (vanno liberati), gli altri sono posseduti dalla decomposizione.
    /// </summary>
    internal static bool TryEig(Matrix<double> a, bool withVectors, out EigResult result)
    {
        result = null!;
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var sdec = NativeSchur(h);
            if (sdec <= 0) return false;
            double[,] t;
            double[,] z;
            try
            {
                var tm = ReadDense(NativeDecompGetFactor(sdec, 0));
                var zm = ReadDense(NativeDecompGetFactor(sdec, 1));
                if (tm is null || zm is null) return false;
                t = tm.ToArray();
                z = zm.ToArray();
            }
            finally { NativeDecompFree(sdec); }

            var edec = NativeEig(h);
            if (edec <= 0) return false;
            try
            {
                var dh = NativeDecompGetFactorEx(edec, 0, out var ownedD);
                var ih = NativeDecompGetFactorEx(edec, 1, out var ownedI);
                try
                {
                    var dm = ReadDense(dh);
                    var im = ReadDense(ih);
                    if (dm is null || im is null) return false;
                    var n = dm.RowCount;
                    var d = new double[n];
                    var e = new double[n];
                    for (var i = 0; i < n; i++) { d[i] = dm[i, 0]; e[i] = im[i, 0]; }

                    double[,]? vr = null, vi = null;
                    if (withVectors)
                    {
                        var vrm = ReadDense(NativeDecompGetFactor(edec, 2));
                        var vim = ReadDense(NativeDecompGetFactor(edec, 3));
                        if (vrm is null || vim is null) return false;
                        vr = vrm.ToArray();
                        vi = vim.ToArray();
                    }

                    result = new EigResult { T = t, Z = z, D = d, E = e, VR = vr, VI = vi };
                    return true;
                }
                finally
                {
                    if (ownedD != 0 && dh > 0) MatrixFree(dh);
                    if (ownedI != 0 && ih > 0) MatrixFree(ih);
                }
            }
            finally { NativeDecompFree(edec); }
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

    /// <summary>Legge un fattore per NOME (liberando l'handle se il kernel ne ha creato uno nuovo).</summary>
    private static bool CopyFactorMatrix(long dec, string name, NativeDecomposition nd)
    {
        var fh = NativeDecompGetFactorNamedEx(dec, name, out var owned);
        try
        {
            if (fh <= 0) return false;
            var m = ReadDense(fh);
            if (m is null) return false;
            nd.AddMatrix(name, m);
            return true;
        }
        finally { if (owned != 0 && fh > 0) MatrixFree(fh); }
    }

    /// <summary>Come <see cref="CopyFactorMatrix"/> ma per i fattori che sono VETTORI (N×1).</summary>
    private static bool CopyFactorVector(long dec, string name, NativeDecomposition nd)
    {
        var fh = NativeDecompGetFactorNamedEx(dec, name, out var owned);
        try
        {
            if (fh <= 0) return false;
            var m = ReadDense(fh);
            if (m is null) return false;
            var v = Vector<double>.Build.Dense(m.RowCount * m.ColumnCount);
            var k = 0;
            for (var i = 0; i < m.RowCount; i++)
                for (var j = 0; j < m.ColumnCount; j++) v[k++] = m[i, j];
            nd.AddVector(name, v);
            return true;
        }
        finally { if (owned != 0 && fh > 0) MatrixFree(fh); }
    }

    /// <summary>
    /// LU/QR/SVD/EVD/Cholesky con i kernel C: i fattori sono materializzati subito e
    /// l'handle della decomposizione viene rilasciato (nessun handle C resta vivo).
    /// `false` = lib non caricabile oppure decomposizione impossibile (es. Cholesky su
    /// matrice non definita positiva, che il kernel rifiuta esplicitamente).
    /// </summary>
    internal static bool TryDecompose(Matrix<double> a, NativeDecompKind kind, out NativeDecomposition result)
    {
        result = null!;
        if (!TryToNative(a, out var h)) return false;
        try
        {
            var dec = kind switch
            {
                NativeDecompKind.Lu => NativeLu(h),
                NativeDecompKind.Qr => NativeQr(h),
                NativeDecompKind.Svd => NativeSvd(h),
                NativeDecompKind.Evd => NativeEvd(h),
                _ => NativeCholesky(h)
            };
            if (dec <= 0) return false;
            try
            {
                var nd = new NativeDecomposition();
                var ok = kind switch
                {
                    NativeDecompKind.Lu => CopyFactorMatrix(dec, "L", nd) && CopyFactorMatrix(dec, "U", nd),
                    NativeDecompKind.Qr => CopyFactorMatrix(dec, "Q", nd) && CopyFactorMatrix(dec, "R", nd),
                    NativeDecompKind.Svd => CopyFactorMatrix(dec, "U", nd) && CopyFactorMatrix(dec, "S", nd)
                                            && CopyFactorMatrix(dec, "VT", nd) && CopyFactorVector(dec, "SVECTOR", nd),
                    NativeDecompKind.Evd => CopyFactorMatrix(dec, "D", nd) && CopyFactorMatrix(dec, "V", nd)
                                            && CopyFactorVector(dec, "DREAL", nd) && CopyFactorVector(dec, "DIMAG", nd),
                    _ => CopyFactorMatrix(dec, "L", nd)
                };
                if (!ok) return false;
                result = nd;
                return true;
            }
            finally { NativeDecompFree(dec); }
        }
        catch (DllNotFoundException) { MarkUnavailable(); return false; }
        catch (EntryPointNotFoundException) { MarkUnavailable(); return false; }
        catch (BadImageFormatException) { MarkUnavailable(); return false; }
        finally { MatrixFree(h); }
    }

}
