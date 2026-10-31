// FILE: StdLib.SimdMatrix.Decomposition.cs
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace CommonLib;

using MathNet.Numerics.LinearAlgebra.Double;

/// <summary>
/// Classe per la gestione efficiente di matrici dense di double in puro C# e con supporto SIMD.
/// Supporta tutte le principali operazioni algebriche, prodotto con vettori, determinante, inversa e matrice identità.
/// </summary>
/// <remarks>
/// Metodi definiti in questo file:
/// <list type="bullet">
///   <item><see cref="LU"/> — Decomposizione LU (Doolittle)</item>
///   <item><see cref="QR"/> — Decomposizione QR (Gram-Schmidt)</item>
///   <item><see cref="Cholesky"/> — Decomposizione di Cholesky</item>
///   <item><see cref="SVD"/> — Decomposizione ai valori singolari</item>
///   <item><see cref="Eigen"/> — Autovalori e autovettori</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    // =================== LU Decomposition ===================
    /// <summary>
    /// Decomposizione LU (Doolittle, senza pivoting).
    /// Restituisce (L,U) con L triangolare inferiore e U triangolare superiore.
    /// </summary>
    /// <returns>Tupla (L, U) dove L è triangolare inferiore (diagonale unitaria) e U è triangolare superiore.</returns>
    /// <exception cref="InvalidOperationException">Se la matrice non è quadrata.</exception>
    public (SimdMatrix L, SimdMatrix U) LU()
    {
        if (Rows != Cols) throw new InvalidOperationException("LU: Matrix must be square");
        var n = Rows;
        var L = new SimdMatrix(n, n);
        var U = new SimdMatrix(n, n);
        for (var i = 0; i < n; i++)
        {
            // Upper
            for (var k = i; k < n; k++)
            {
                double sum = 0;
                for (var j = 0; j < i; j++)
                    sum += L[i, j] * U[j, k];
                U[i, k] = this[i, k] - sum;
            }

            // Lower
            for (var k = i; k < n; k++)
            {
                if (i == k)
                    L[i, i] = 1;
                else
                {
                    double sum = 0;
                    for (var j = 0; j < i; j++)
                        sum += L[k, j] * U[j, i];
                    L[k, i] = (this[k, i] - sum) / U[i, i];
                }
            }
        }

        return (L, U);
    }

    // =================== QR Decomposition (Gram-Schmidt) ===================
    /// <summary>
    /// Decomposizione QR tramite Gram-Schmidt (Q ortogonale, R triangolare superiore).
    /// </summary>
    /// <returns>Tupla (Q, R) dove Q è una matrice ortogonale e R è triangolare superiore.</returns>
    public (SimdMatrix Q, SimdMatrix R) QR()
    {
        int m = Rows, n = Cols;
        var Q = new SimdMatrix(m, n);
        var R = new SimdMatrix(n, n);
        var A = this;
        for (var k = 0; k < n; k++)
        {
            // Copy column
            for (var i = 0; i < m; i++)
                Q[i, k] = A[i, k];
            for (var j = 0; j < k; j++)
            {
                double r = 0;
                for (var i = 0; i < m; i++)
                    r += Q[i, j] * A[i, k];
                R[j, k] = r;
                for (var i = 0; i < m; i++)
                    Q[i, k] -= r * Q[i, j];
            }

            double norm = 0;
            for (var i = 0; i < m; i++)
                norm += Q[i, k] * Q[i, k];
            norm = Math.Sqrt(norm);
            R[k, k] = norm;
            for (var i = 0; i < m; i++)
                Q[i, k] /= norm;
        }

        return (Q, R);
    }

    // =================== Cholesky Decomposition (solo per simmetriche positive definite) ===================
    /// <summary>
    /// Decomposizione di Cholesky (matrici simmetriche positive definite).
    /// Restituisce L triangolare inferiore tale che A = L * L^T.
    /// </summary>
    /// <returns>Matrice L triangolare inferiore.</returns>
    /// <exception cref="InvalidOperationException">Se la matrice non è quadrata.</exception>
    public SimdMatrix Cholesky()
    {
        if (Rows != Cols) throw new InvalidOperationException("Matrix must be square");
        var n = Rows;
        var L = new SimdMatrix(n, n);
        for (var i = 0; i < n; i++)
        for (var j = 0; j <= i; j++)
        {
            double sum = 0;
            for (var k = 0; k < j; k++)
                sum += L[i, k] * L[j, k];
            if (i == j)
                L[i, j] = Math.Sqrt(this[i, i] - sum);
            else
                L[i, j] = 1.0 / L[j, j] * (this[i, j] - sum);
        }

        return L;
    }

    // =================== SVD & Eigen (Stub) ===================
    /// <summary>
    /// SVD: decomposizione ai valori singolari tramite MathNet.Numerics.
    /// </summary>
    /// <returns>(U, S, Vt) dove U e Vt sono matrici ortogonali e S è la matrice diagonale dei valori singolari.</returns>
    public (SimdMatrix U, SimdMatrix S, SimdMatrix Vt) SVD()
    {
        // Conversione a matrice MathNet
        var mat = DenseMatrix.Build.Dense(Rows, Cols, _data);
        var svd = mat.Svd(computeVectors: true);

        // Ricostruzione delle matrici nei tuoi formati
        var U = new SimdMatrix(svd.U.ToRowMajorArray(), svd.U.RowCount, svd.U.ColumnCount);
        // S è matrice diagonale, la rappresentiamo come matrice quadrata S
        var S = new SimdMatrix(new double[svd.S.Count * svd.S.Count], svd.S.Count, svd.S.Count);
        for (var i = 0; i < svd.S.Count; i++)
            S[i, i] = svd.S[i];
        var Vt = new SimdMatrix(svd.VT.ToRowMajorArray(), svd.VT.RowCount, svd.VT.ColumnCount);
        return (U, S, Vt);
    }

    /// <summary>
    /// Calcolo degli autovalori/autovettori tramite MathNet.Numerics.
    /// </summary>
    /// <returns>(array autovalori reali, matrice degli autovettori come colonne).</returns>
    public (double[] eigenvalues, SimdMatrix eigenvectors) Eigen()
    {
        // Conversione a matrice MathNet
        var mat = DenseMatrix.Build.Dense(Rows, Cols, _data);
        var evd = mat.Evd();

        // Solo parte reale degli autovalori (MathNet supporta anche complessi, qui assumiamo reale)
        var evals = new double[evd.EigenValues.Count];
        for (var i = 0; i < evals.Length; i++)
            evals[i] = evd.EigenValues[i].Real;

        var evecs = new SimdMatrix(evd.EigenVectors.ToRowMajorArray(), evd.EigenVectors.RowCount,
            evd.EigenVectors.ColumnCount);
        return (evals, evecs);
    }
}
