// FILE: CommonLib.Numerics.MatrixScipyLinalg.cs
// Completamento scipy.linalg: funzioni matriciali (cosm/sinm/tanm/signm/fractional
// power), decomposizioni SVD-based (orth, subspace_angles, polar, cond, nuclear
// norm), costruttori (toeplitz/hankel/circulant), solve_triangular e
// autovalori/autovettori generali + real Schur form.
//
// TUTTE le operazioni di questo file eseguono il calcolo nei KERNEL C del runtime
// (`libgmb_matrix_flat`, ponte `MatrixNativeKernels`): una sola fonte numerica,
// condivisa con VM/C e C99Native. Non esiste un'interpretazione gestita di riserva:
// se la lib non è caricabile la chiamata solleva `NotSupportedException` con il
// nome del kernel mancante.
//
// Le reference numeriche sono scipy.linalg; il runtime C segue la struttura
// classica di JAMA (orthes/hqr2) per EIG/SCHUR.

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

public static partial class Numerics
{
    // ========================================================================
    // Funzioni matriciali: il calcolo è SOLO nel runtime C (kernel condivisi con
    // VM/C e C99Native). Nessun fallback gestito: se la lib manca l'errore è esplicito.
    // ========================================================================

    /// <summary>scipy.linalg.cosm — coseno di matrice (kernel C `gmb_matrix_cosm`).</summary>
    public static Matrix<double> MatrixCos(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Cos, out var r) ? r
           : throw new NotSupportedException("LINALG.COSM: kernel C `gmb_matrix_cosm` non disponibile (libgmb_matrix_flat).");

    /// <summary>scipy.linalg.sinm — seno di matrice (kernel C `gmb_matrix_sinm`).</summary>
    public static Matrix<double> MatrixSin(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Sin, out var r) ? r
           : throw new NotSupportedException("LINALG.SINM: kernel C `gmb_matrix_sinm` non disponibile (libgmb_matrix_flat).");

    /// <summary>scipy.linalg.tanm — tangente di matrice (kernel C `gmb_matrix_tanm`).</summary>
    public static Matrix<double> MatrixTan(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Tan, out var r) ? r
           : throw new NotSupportedException("LINALG.TANM: kernel C `gmb_matrix_tanm` non disponibile (libgmb_matrix_flat).");

    /// <summary>scipy.linalg.signm — segno di matrice (via EVD simmetrica).</summary>
    public static Matrix<double> MatrixSignm(Matrix<double> a)
        => MatrixNativeKernels.TrySignm(a, out var r) ? r
           : throw new NotSupportedException("LINALG.SIGNM: kernel C `gmb_matrix_signm` non disponibile (libgmb_matrix_flat). L'implementazione gestita è stata rimossa: la fonte è una sola (C).");

    /// <summary>scipy.linalg.fractional_matrix_power — potenza frazionaria di matrice.</summary>
    public static Matrix<double> MatrixFractionalPower(Matrix<double> a, double p)
        => MatrixNativeKernels.TryFractionalPower(a, p, out var r) ? r
           : throw new NotSupportedException("LINALG.FRACTIONALPOWER: kernel C `gmb_matrix_fractional_power` non disponibile (libgmb_matrix_flat).");

    // ========================================================================
    // SVD-based: orth, subspace_angles, polar, cond, nuclear norm
    // ========================================================================

    /// <summary>scipy.linalg.orth — base ortonormale del range di A (kernel C `gmb_matrix_orth`).</summary>
    public static Matrix<double> Orth(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Orth, out var r) ? r
           : throw new NotSupportedException("LINALG.ORTH: kernel C `gmb_matrix_orth` non disponibile (libgmb_matrix_flat).");

    /// <summary>scipy.linalg.subspace_angles — angoli tra due sottospazi.</summary>
    public static double[] SubspaceAngles(Matrix<double> a, Matrix<double> b)
        => MatrixNativeKernels.TrySubspaceAngles(a, b, out var r) ? r
           : throw new NotSupportedException("LINALG.SUBSPACEANGLES: kernel C `gmb_matrix_subspace_angles` non disponibile (libgmb_matrix_flat).");

    /// <summary>Risultato della decomposizione polare (A = U·P).</summary>
    public sealed class PolarDecomposition
    {
        public Matrix<double> U { get; }
        public Matrix<double> P { get; }

        public PolarDecomposition(Matrix<double> u, Matrix<double> p)
        {
            U = u;
            P = p;
        }
    }

    /// <summary>scipy.linalg.polar — decomposizione polare A = U·P (U ortogonale, P semidefinita positiva).</summary>
    public static PolarDecomposition Polar(Matrix<double> a)
    {
        if (MatrixNativeKernels.TryPolar(a, out var u, out var p)) return new PolarDecomposition(u, p);
        throw new NotSupportedException("LINALG.POLAR: kernel C `gmb_matrix_polar` non disponibile (libgmb_matrix_flat).");
    }

    /// <summary>scipy.linalg.cond — numero di condizionamento (norma 2: s_max/s_min).</summary>
    public static double ConditionNumber(Matrix<double> a)
        => MatrixNativeKernels.TryCond(a, out var r) ? r
           : throw new NotSupportedException("LINALG.COND: kernel C `gmb_matrix_cond` non disponibile (libgmb_matrix_flat).");

    /// <summary>scipy.linalg.norm(ord='nuc') — norma nucleare (somma dei valori singolari).</summary>
    public static double NuclearNorm(Matrix<double> a)
        => MatrixNativeKernels.TryNuclearNorm(a, out var r) ? r
           : throw new NotSupportedException("LINALG.NUCLEARNORM: kernel C `gmb_matrix_nuclear_norm` non disponibile (libgmb_matrix_flat).");

    // ========================================================================
    // Costruttori: toeplitz, hankel, circulant
    // ========================================================================

    /// <summary>scipy.linalg.toeplitz — matrice di Toeplitz (c su/di sotto la diagonale, r sopra).</summary>
    public static Matrix<double> Toeplitz(double[] c, double[] r)
    {
        if (MatrixNativeKernels.TryToeplitz(c, r, out var k)) return k;
        throw new NotSupportedException("LINALG.TOEPLITZ: kernel C `gmb_matrix_toeplitz` non disponibile (libgmb_matrix_flat).");
    }

    /// <summary>scipy.linalg.hankel — matrice di Hankel (costanti lungo le anti-diagonali).</summary>
    public static Matrix<double> Hankel(double[] c, double[] r)
    {
        if (MatrixNativeKernels.TryHankel(c, r, out var k)) return k;
        throw new NotSupportedException("LINALG.HANKEL: kernel C `gmb_matrix_hankel` non disponibile (libgmb_matrix_flat).");
    }

    /// <summary>scipy.linalg.circulant — matrice circolante (riga i = c shiftato a destra di i).</summary>
    public static Matrix<double> Circulant(double[] c)
    {
        if (MatrixNativeKernels.TryCirculant(c, out var k)) return k;
        throw new NotSupportedException("LINALG.CIRCULANT: kernel C `gmb_matrix_circulant` non disponibile (libgmb_matrix_flat).");
    }

    // ========================================================================
    // solve_triangular
    // ========================================================================

    /// <summary>scipy.linalg.solve_triangular — sostituzione forward/backward su matrice triangolare.</summary>
    public static Vector<double> SolveTriangular(Matrix<double> a, Vector<double> b, bool lower)
    {
        var n = a.RowCount;
        if (n != a.ColumnCount || n != b.Count)
            throw new ArgumentException("solve_triangular richiede matrice quadrata e vettore compatibile.");
        if (MatrixNativeKernels.TrySolveTriangular(a, b.ToArray(), lower, out var x)) return DenseVector.OfArray(x);
        throw new NotSupportedException("LINALG.SOLVETRIANGULAR: kernel C `gmb_matrix_solve_triangular` non disponibile (libgmb_matrix_flat).");
    }

    // ========================================================================
    // ========================================================================
    // EIG generale (non simmetrica) e real Schur form.
    // Riduzione di Hessenberg (orthes) + iterazione QR di Francis a doppio shift
    // (hqr2) — struttura JAMA, adattata a double[,]. La base di Schur Z viene
    // preservata; gli autovettori sono ricavati separatamente per back-substitution
    // sulla forma quasi-triangolare (W) seguita da W = Z·W.
    // ========================================================================

    /// <summary>Decomposizione agli autovalori per matrici reali generali.</summary>
    public sealed class GeneralEigenDecomposition
    {
        private readonly int _n;
        private readonly double[,] _t;
        private readonly double[,] _z;
        private readonly double[] _d;
        private readonly double[] _e;
        private readonly double[,] _vr;
        private readonly double[,] _vi;

        internal GeneralEigenDecomposition(int n, double[,] t, double[,] z, double[] d, double[] e, double[,] vr, double[,] vi)
        {
            _n = n;
            _t = t;
            _z = z;
            _d = d;
            _e = e;
            _vr = vr;
            _vi = vi;
        }

        /// <summary>Matrice di Schur reale (quasi-triangolare superiore).</summary>
        public Matrix<double> RealSchurForm => DenseMatrix.OfArray(_t);

        /// <summary>Trasformazione ortogonale Z con T = Z^T A Z.</summary>
        public Matrix<double> SchurVectors => DenseMatrix.OfArray(_z);

        /// <summary>Parti reali degli autovalori.</summary>
        public double[] EigenvaluesReal => (double[])_d.Clone();

        /// <summary>Parti immaginarie degli autovalori.</summary>
        public double[] EigenvaluesImag => (double[])_e.Clone();

        /// <summary>Parti reali degli autovettori destri (colonna i).</summary>
        public Matrix<double> EigenvectorsReal => _vr is null ? DenseMatrix.Create(_n, _n, 0.0) : DenseMatrix.OfArray(_vr);

        /// <summary>Parti immaginarie degli autovettori destri (colonna i).</summary>
        public Matrix<double> EigenvectorsImag => _vi is null ? DenseMatrix.Create(_n, _n, 0.0) : DenseMatrix.OfArray(_vi);
    }

    /// <summary>scipy.linalg.eig (matrici reali) — autovalori e autovettori generali.</summary>
    public static GeneralEigenDecomposition GeneralEigen(Matrix<double> a)
    {
        var n = a.RowCount;
        if (n != a.ColumnCount) throw new ArgumentException("eig richiede una matrice quadrata.");
        if (MatrixNativeKernels.TryEig(a, withVectors: true, out var r))
            return new GeneralEigenDecomposition(n, r.T, r.Z, r.D, r.E, r.VR!, r.VI!);
        throw new NotSupportedException("LINALG.EIG: kernel C `gmb_matrix_eig` non disponibile (libgmb_matrix_flat).");
    }

    /// <summary>scipy.linalg.schur — real Schur decomposition (fattori T e Z nella decomp).</summary>
    public static GeneralEigenDecomposition RealSchur(Matrix<double> a)
    {
        var n = a.RowCount;
        if (n != a.ColumnCount) throw new ArgumentException("schur richiede una matrice quadrata.");
        if (MatrixNativeKernels.TryEig(a, withVectors: false, out var r))
            return new GeneralEigenDecomposition(n, r.T, r.Z, r.D, r.E, null!, null!);
        throw new NotSupportedException("LINALG.SCHUR: kernel C `gmb_matrix_schur` non disponibile (libgmb_matrix_flat).");
    }

}
