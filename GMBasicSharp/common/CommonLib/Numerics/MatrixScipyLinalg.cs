// FILE: CommonLib.Numerics.MatrixScipyLinalg.cs
// Completamento scipy.linalg: funzioni matriciali (cosm/sinm/tanm/signm/fractional
// power), decomposizioni SVD-based (orth, subspace_angles, polar, cond, nuclear
// norm), costruttori (toeplitz/hankel/circulant), solve_triangular e — la parte
// pesante — autovalori/autovettori GENERALI non simmetrici e real Schur form
// (riduzione di Hessenberg + iterazione QR di Francis a doppio shift).
// Le reference numeriche sono scipy.linalg; gli algoritmi seguono la struttura
// classica di JAMA (orthes/hqr2) adattata a double[,].

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

public static partial class Numerics
{
    // ========================================================================
    // Funzioni matriciali via EVD (simmetrica): f(A) = V f(D) V^T
    // ========================================================================

    private static Matrix<double> ApplyEigenwise(Matrix<double> a, Func<double, double> f)
    {
        var evd = a.Evd();
        var v = evd.EigenVectors;
        var d = evd.D;
        var n = a.RowCount;
        var r = DenseMatrix.Create(n, n, 0.0);
        for (var i = 0; i < n; i++)
            for (var j = 0; j < n; j++)
            {
                double s = 0;
                for (var k = 0; k < n; k++)
                    s += v[i, k] * f(d[k, k]) * v[j, k];
                r[i, j] = s;
            }
        return r;
    }

    /// <summary>scipy.linalg.cosm — coseno di matrice (via EVD simmetrica).</summary>
    public static Matrix<double> MatrixCos(Matrix<double> a) => ApplyEigenwise(a, Math.Cos);

    /// <summary>scipy.linalg.sinm — seno di matrice (via EVD simmetrica).</summary>
    public static Matrix<double> MatrixSin(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Sin, out var r)
            ? r : ApplyEigenwise(a, Math.Sin);

    /// <summary>scipy.linalg.tanm — tangente di matrice (via EVD simmetrica).</summary>
    public static Matrix<double> MatrixTan(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Tan, out var r)
            ? r : ApplyEigenwise(a, Math.Tan);

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

    private const double MachineEps = 2.220446049250313e-16;

    /// <summary>scipy.linalg.orth — base ortonormale del range di A (SVD).</summary>
    public static Matrix<double> Orth(Matrix<double> a)
        => MatrixNativeKernels.TryApply(a, MatrixNativeKernels.Op.Orth, out var r)
            ? r : OrthManaged(a);

    /// <summary>Implementazione gestita di ORTH (fallback quando `libgmb_matrix_flat` non è
    /// disponibile). Stessa semantica e stessa convenzione di segno del runtime C.</summary>
    internal static Matrix<double> OrthManaged(Matrix<double> a)
    {
        var svd = a.Svd();
        var tol = Math.Max(a.RowCount, a.ColumnCount) * MachineEps * svd.S[0];
        var rank = 0;
        for (var i = 0; i < svd.S.Count; i++)
            if (svd.S[i] > tol) rank++;
        if (rank == 0) return DenseMatrix.Create(a.RowCount, 0, 0.0);
        var q = svd.U.SubMatrix(0, a.RowCount, 0, rank);
        // Convenzione di SEGNO condivisa col runtime C (gmb_matrix_orth): il primo elemento
        // di modulo massimo di ogni colonna è POSITIVO. Senza, il segno delle colonne di U
        // dipende dall'implementazione SVD (LAPACK del runtime C vs MathNet) e le gambe
        // divergono su ORTH (es. -0.7882 vs 0.7882 nel sample MatrixDecompositions).
        CanonicalizeColumnSigns(q);
        return q;
    }

    /// <summary>Rende positivo il primo elemento di modulo massimo di ogni colonna (convenzione
    /// di segno stabile per basi ortonormali tra implementazioni SVD diverse).</summary>
    internal static void CanonicalizeColumnSigns(Matrix<double> q)
    {
        for (var c = 0; c < q.ColumnCount; c++)
        {
            var pivotRow = 0;
            var pivotAbs = 0.0;
            for (var r = 0; r < q.RowCount; r++)
            {
                var v = Math.Abs(q[r, c]);
                if (v > pivotAbs) { pivotAbs = v; pivotRow = r; }
            }
            if (pivotAbs > 0 && q[pivotRow, c] < 0)
                for (var r = 0; r < q.RowCount; r++) q[r, c] = -q[r, c];
        }
    }

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
        var svd = a.Svd();
        var v = svd.VT.Transpose();
        var u = svd.U.Multiply(svd.VT);
        var p = v.Multiply(svd.W).Multiply(v.Transpose());
        return new PolarDecomposition(u, p);
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
        var x = new double[n];
        if (lower)
        {
            for (var i = 0; i < n; i++)
            {
                double s = 0;
                for (var j = 0; j < i; j++) s += a[i, j] * x[j];
                x[i] = (b[i] - s) / a[i, i];
            }
        }
        else
        {
            for (var i = n - 1; i >= 0; i--)
            {
                double s = 0;
                for (var j = i + 1; j < n; j++) s += a[i, j] * x[j];
                x[i] = (b[i] - s) / a[i, i];
            }
        }
        return DenseVector.OfArray(x);
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
        var h = new double[n, n];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < n; j++)
                h[i, j] = a[i, j];
        var (t, z, d, e) = SchurCore(h, n);
        BackSubstitute(t, z, d, e, n, out var vr, out var vi);
        return new GeneralEigenDecomposition(n, t, z, d, e, vr, vi);
    }

    /// <summary>scipy.linalg.schur — real Schur decomposition (fattori T e Z nella decomp).</summary>
    public static GeneralEigenDecomposition RealSchur(Matrix<double> a)
    {
        var n = a.RowCount;
        if (n != a.ColumnCount) throw new ArgumentException("schur richiede una matrice quadrata.");
        var h = new double[n, n];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < n; j++)
                h[i, j] = a[i, j];
        var (t, z, d, e) = SchurCore(h, n);
        return new GeneralEigenDecomposition(n, t, z, d, e, null!, null!);
    }

    private static void Cdiv(double xr, double xi, double yr, double yi, out double cr, out double ci)
    {
        double r, d;
        if (Math.Abs(yr) > Math.Abs(yi))
        {
            r = yi / yr;
            d = yr + r * yi;
            cr = (xr + r * xi) / d;
            ci = (xi - r * xr) / d;
        }
        else
        {
            r = yr / yi;
            d = yi + r * yr;
            cr = (r * xr + xi) / d;
            ci = (r * xi - xr) / d;
        }
    }

    private static (double[,] T, double[,] Z, double[] D, double[] E) SchurCore(double[,] h, int n)
    {
        var v = new double[n, n];
        var orts = new double[n];
        const int low = 0;
        var high = n - 1;

        // ---- orthes: riduzione a forma di Hessenberg con accumulo di V ----
        for (var mm = low + 1; mm <= high - 1; mm++)
        {
            double scale = 0.0;
            for (var i = mm; i <= high; i++) scale += Math.Abs(h[i, mm - 1]);
            if (scale != 0.0)
            {
                double hh = 0.0;
                for (var i = high; i >= mm; i--) { orts[i] = h[i, mm - 1] / scale; hh += orts[i] * orts[i]; }
                var g = Math.Sqrt(hh);
                if (orts[mm] > 0) g = -g;
                hh -= orts[mm] * g;
                orts[mm] -= g;
                for (var j = mm; j < n; j++)
                {
                    double f = 0.0;
                    for (var i = high; i >= mm; i--) f += orts[i] * h[i, j];
                    f /= hh;
                    for (var i = mm; i <= high; i++) h[i, j] -= f * orts[i];
                }
                for (var i = 0; i <= high; i++)
                {
                    double f = 0.0;
                    for (var j = high; j >= mm; j--) f += orts[j] * h[i, j];
                    f /= hh;
                    for (var j = mm; j <= high; j++) h[i, j] -= f * orts[j];
                }
                orts[mm] = scale * orts[mm];
                h[mm, mm - 1] = scale * g;
            }
        }
        for (var i = 0; i < n; i++)
            for (var j = 0; j < n; j++)
                v[i, j] = i == j ? 1.0 : 0.0;
        for (var mm = high - 1; mm >= low + 1; mm--)
        {
            if (h[mm, mm - 1] != 0.0)
            {
                for (var i = mm + 1; i <= high; i++) orts[i] = h[i, mm - 1];
                for (var j = mm; j <= high; j++)
                {
                    double g = 0.0;
                    for (var i = mm; i <= high; i++) g += orts[i] * v[i, j];
                    g = (g / orts[mm]) / h[mm, mm - 1];
                    for (var i = mm; i <= high; i++) v[i, j] += g * orts[i];
                }
            }
        }

        // ---- hqr2: iterazione QR a doppio shift (Francis). Variabili a scope di
        // metodo, come in JAMA (z/s/r/p/q/x/y/w/t riusate tra i diversi blocchi).
        var d = new double[n];
        var e = new double[n];
        double anorm = 0;
        for (var i = 0; i < n; i++)
            for (var j = Math.Max(i - 1, 0); j < n; j++)
                anorm += Math.Abs(h[i, j]);

        var nn = n - 1;
        double t = 0;
        var converged = true;
        var maxIts = 30 * n;
        var total = 0;
        double s = 0, p = 0, q = 0, r = 0, x = 0, y = 0, w = 0, z = 0;
        while (nn >= 0)
        {
            var its = 0;
            int l;
            while (true)
            {
                for (l = nn; l > low; l--)
                {
                    s = Math.Abs(h[l - 1, l - 1]) + Math.Abs(h[l, l]);
                    if (s == 0.0) s = anorm;
                    if (Math.Abs(h[l, l - 1]) <= MachineEps * s)
                    {
                        h[l, l - 1] = 0.0;
                        break;
                    }
                }
                x = h[nn, nn];
                if (l == nn)
                {
                    d[nn] = x + t;
                    e[nn] = 0.0;
                    nn--;
                    break;
                }
                y = h[nn - 1, nn - 1];
                w = h[nn - 1, nn] * h[nn, nn - 1];
                if (l == nn - 1)
                {
                    p = 0.5 * (y - x);
                    q = p * p + w;
                    z = Math.Sqrt(Math.Abs(q));
                    x += t;
                    if (q >= 0.0)
                    {
                        z = p >= 0 ? p + z : p - z;
                        d[nn - 1] = x + z;
                        d[nn] = d[nn - 1];
                        if (z != 0.0) d[nn] = x - w / z;
                        e[nn - 1] = 0.0;
                        e[nn] = 0.0;
                        x = h[nn, nn - 1];
                        s = Math.Abs(x) + Math.Abs(z);
                        p = x / s;
                        q = z / s;
                        r = Math.Sqrt(p * p + q * q);
                        p /= r;
                        q /= r;
                        for (var j = nn - 1; j < n; j++)
                        {
                            z = h[nn - 1, j];
                            h[nn - 1, j] = q * z + p * h[nn, j];
                            h[nn, j] = q * h[nn, j] - p * z;
                        }
                        for (var i = 0; i <= nn; i++)
                        {
                            z = h[i, nn - 1];
                            h[i, nn - 1] = q * z + p * h[i, nn];
                            h[i, nn] = q * h[i, nn] - p * z;
                        }
                        for (var i = low; i <= high; i++)
                        {
                            z = v[i, nn - 1];
                            v[i, nn - 1] = q * z + p * v[i, nn];
                            v[i, nn] = q * v[i, nn] - p * z;
                        }
                    }
                    else
                    {
                        d[nn - 1] = x + p;
                        d[nn] = x + p;
                        e[nn - 1] = z;
                        e[nn] = -z;
                    }
                    nn -= 2;
                    break;
                }
                if (its == 30)
                {
                    converged = false;
                    break;
                }
                if (its == 10 || its == 20)
                {
                    t += x;
                    for (var i = low; i <= nn; i++) h[i, i] -= x;
                    s = Math.Abs(h[nn, nn - 1]) + Math.Abs(h[nn - 1, nn - 2]);
                    x = 0.75 * s;
                    y = x;
                    w = -0.4375 * s * s;
                }
                its++;
                total++;
                if (total > maxIts)
                {
                    converged = false;
                    break;
                }
                var m = nn - 2;
                for (; m >= l; m--)
                {
                    z = h[m, m];
                    r = x - z;
                    s = y - z;
                    p = (r * s - w) / h[m + 1, m] + h[m, m + 1];
                    q = h[m + 1, m + 1] - z - r - s;
                    r = h[m + 2, m + 1];
                    s = Math.Abs(p) + Math.Abs(q) + Math.Abs(r);
                    p /= s;
                    q /= s;
                    r /= s;
                    if (m == l) break;
                    if (Math.Abs(h[m, m - 1]) * (Math.Abs(q) + Math.Abs(r)) <=
                        MachineEps * Math.Abs(p) * (Math.Abs(h[m - 1, m - 1]) + Math.Abs(z) + Math.Abs(h[m + 1, m + 1])))
                        break;
                }
                for (var i = m + 2; i <= nn; i++)
                {
                    h[i, i - 2] = 0.0;
                    if (i != m + 2) h[i, i - 3] = 0.0;
                }
                for (var k = m; k <= nn - 1; k++)
                {
                    var notlast = k != nn - 1;
                    if (k != m)
                    {
                        p = h[k, k - 1];
                        q = h[k + 1, k - 1];
                        r = notlast ? h[k + 2, k - 1] : 0.0;
                        x = Math.Abs(p) + Math.Abs(q) + Math.Abs(r);
                        if (x == 0.0) continue;
                        p /= x;
                        q /= x;
                        r /= x;
                    }
                    s = Math.Sqrt(p * p + q * q + r * r);
                    if (p < 0.0) s = -s;
                    if (s != 0.0)
                    {
                        if (k != m) h[k, k - 1] = -s * x;
                        else if (l != m) h[k, k - 1] = -h[k, k - 1];
                        p += s;
                        x = p / s;
                        y = q / s;
                        z = r / s;
                        q /= p;
                        r /= p;
                        for (var j = k; j <= nn; j++)
                        {
                            p = h[k, j] + q * h[k + 1, j];
                            if (notlast)
                            {
                                p += r * h[k + 2, j];
                                h[k + 2, j] -= p * z;
                            }
                            h[k, j] -= p * x;
                            h[k + 1, j] -= p * y;
                        }
                        for (var i = 0; i <= Math.Min(nn, k + 3); i++)
                        {
                            p = x * h[i, k] + y * h[i, k + 1];
                            if (notlast)
                            {
                                p += z * h[i, k + 2];
                                h[i, k + 2] -= p * r;
                            }
                            h[i, k] -= p;
                            h[i, k + 1] -= p * q;
                        }
                        for (var i = low; i <= high; i++)
                        {
                            p = x * v[i, k] + y * v[i, k + 1];
                            if (notlast)
                            {
                                p += z * v[i, k + 2];
                                v[i, k + 2] -= p * r;
                            }
                            v[i, k] -= p;
                            v[i, k + 1] -= p * q;
                        }
                    }
                }
            }
            if (!converged) break;
        }

        if (!converged)
            throw new InvalidOperationException("La decomposizione agli autovalori non è convergente.");

        return (h, v, d, e);
    }

    /// <summary>
    /// Back-substitution sugli autovettori della forma quasi-triangolare (JAMA hqr2,
    /// seconda parte), poi trasformazione alla base originale W = Z·W.
    /// Le variabili zz/ss/ra/sa sono persistenti tra le iterazioni della i-loop.
    /// </summary>
    private static void BackSubstitute(double[,] t, double[,] z, double[] d, double[] e, int n,
        out double[,] vr, out double[,] vi)
    {
        double anorm = 0;
        for (var i = 0; i < n; i++)
            for (var j = Math.Max(i - 1, 0); j < n; j++)
                anorm += Math.Abs(t[i, j]);

        double zr = 0, rr = 0, sr = 0, ra = 0, sa = 0;
        for (var nn = n - 1; nn >= 0; nn--)
        {
            var p = d[nn];
            var q = e[nn];

            if (q == 0)
            {
                // ---- Vettore reale ----
                var l = nn;
                t[nn, nn] = 1.0;
                for (var i = nn - 1; i >= 0; i--)
                {
                    var w = t[i, i] - p;
                    double r = 0.0;
                    for (var j = l; j <= nn; j++) r += t[i, j] * t[j, nn];
                    if (e[i] < 0.0)
                    {
                        zr = w;
                        sr = r;
                    }
                    else
                    {
                        l = i;
                        if (e[i] == 0.0)
                        {
                            t[i, nn] = w != 0.0 ? -r / w : -r / (MachineEps * anorm);
                        }
                        else
                        {
                            var x = t[i, i + 1];
                            var y = t[i + 1, i];
                            var q2 = (d[i] - p) * (d[i] - p) + e[i] * e[i];
                            var tval = (x * sr - zr * r) / q2;
                            t[i, nn] = tval;
                            if (Math.Abs(x) > Math.Abs(zr))
                                t[i + 1, nn] = (-r - w * tval) / x;
                            else
                                t[i + 1, nn] = (-sr - y * tval) / zr;
                        }
                        var scale = Math.Abs(t[i, nn]);
                        if ((MachineEps * scale) * scale > 1)
                            for (var j = i; j <= nn; j++)
                                t[j, nn] /= scale;
                    }
                }
            }
            else if (q < 0)
            {
                // ---- Vettore complesso ----
                var l = nn - 1;
                if (Math.Abs(t[nn, nn - 1]) > Math.Abs(t[nn - 1, nn]))
                {
                    t[nn - 1, nn - 1] = q / t[nn, nn - 1];
                    t[nn - 1, nn] = -(t[nn, nn] - p) / t[nn, nn - 1];
                }
                else
                {
                    Cdiv(0.0, -t[nn - 1, nn], t[nn - 1, nn - 1] - p, q, out t[nn - 1, nn - 1], out t[nn - 1, nn]);
                }
                t[nn, nn - 1] = 0.0;
                t[nn, nn] = 1.0;
                for (var i = nn - 2; i >= 0; i--)
                {
                    ra = 0.0;
                    sa = 0.0;
                    for (var j = l; j <= nn; j++)
                    {
                        ra += t[i, j] * t[j, nn - 1];
                        sa += t[i, j] * t[j, nn];
                    }
                    var w = t[i, i] - p;
                    if (e[i] < 0.0)
                    {
                        zr = w;
                        rr = ra;
                        sr = sa;
                    }
                    else
                    {
                        l = i;
                        if (e[i] == 0)
                        {
                            Cdiv(-ra, -sa, w, q, out t[i, nn - 1], out t[i, nn]);
                        }
                        else
                        {
                            var x = t[i, i + 1];
                            var y = t[i + 1, i];
                            var vr2 = (d[i] - p) * (d[i] - p) + e[i] * e[i] - q * q;
                            var vi2 = (d[i] - p) * 2.0 * q;
                            if (vr2 == 0.0 && vi2 == 0.0)
                                vr2 = MachineEps * anorm * (Math.Abs(w) + Math.Abs(q) + Math.Abs(x) + Math.Abs(y) + Math.Abs(zr));
                            Cdiv(x * rr - zr * ra + q * sa, x * sr - zr * sa - q * ra, vr2, vi2, out t[i, nn - 1], out t[i, nn]);
                            if (Math.Abs(x) > Math.Abs(zr) + Math.Abs(q))
                            {
                                t[i + 1, nn - 1] = (-ra - w * t[i, nn - 1] + q * t[i, nn]) / x;
                                t[i + 1, nn] = (-sa - w * t[i, nn] - q * t[i, nn - 1]) / x;
                            }
                            else
                            {
                                Cdiv(-rr - y * t[i, nn - 1], -sr - y * t[i, nn], zr, q, out t[i + 1, nn - 1], out t[i + 1, nn]);
                            }
                        }
                        var scale = Math.Max(Math.Abs(t[i, nn - 1]), Math.Abs(t[i, nn]));
                        if ((MachineEps * scale) * scale > 1)
                            for (var j = i; j <= nn; j++)
                            {
                                t[j, nn - 1] /= scale;
                                t[j, nn] /= scale;
                            }
                    }
                }
            }
        }

        // ---- Back transformation: autovettori nella base originale ----
        // wmat = colonne degli autovettori (nel riferimento della forma di Schur).
        var wmat = new double[n, n];
        for (var j = 0; j < n; j++)
            for (var i = 0; i < n; i++)
                wmat[i, j] = t[i, j];

        // Riallineamento per le coppie complesse: JAMA deposita l'autovettore
        // dell'autovalore e>0 (il "+z") come reale in colonna j, immaginaria in
        // colonna j+1; l'autovalore coniugato (e&lt;0) ha l'autovettore coniugato.
        // Reali: colonna j. Poi W = Z·W.
        var wre = new double[n, n];
        var wim = new double[n, n];
        for (var j = 0; j < n; j++)
        {
            if (e[j] == 0)
            {
                for (var i = 0; i < n; i++) { wre[i, j] = wmat[i, j]; wim[i, j] = 0.0; }
            }
            else if (e[j] < 0)
            {
                for (var i = 0; i < n; i++) { wre[i, j] = wmat[i, j - 1]; wim[i, j] = -wmat[i, j]; }
            }
            else
            {
                for (var i = 0; i < n; i++) { wre[i, j] = wmat[i, j]; wim[i, j] = wmat[i, j + 1]; }
            }
        }
        vr = new double[n, n];
        vi = new double[n, n];
        for (var j = 0; j < n; j++)
            for (var i = 0; i < n; i++)
            {
                double rv = 0, iv = 0;
                for (var k = 0; k < n; k++)
                {
                    rv += z[i, k] * wre[k, j];
                    iv += z[i, k] * wim[k, j];
                }
                vr[i, j] = rv;
                vi[i, j] = iv;
            }
    }
}
