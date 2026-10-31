// FILE: StdLib.Numerics.LinAlg.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>
    /// Calcola il determinante di una matrice quadrata.
    /// </summary>
    public static double MatrixDeterminant(Matrix<double> m) => m.Determinant();

    /// <summary>
    /// Calcola il prodotto scalare (dot product) tra due vettori.
    /// </summary>
    public static double VectorDotProduct(Vector<double> v1, Vector<double> v2) => v1.DotProduct(v2);

    /// <summary>
    /// Calcola la norma euclidea (L2) di un vettore.
    /// </summary>
    public static double VectorNorm(Vector<double> v) => v.L2Norm();

    /// <summary>
    /// Calcola la matrice pseudoinversa (Moore-Penrose) tramite SVD.
    /// </summary>
    public static Matrix<double> PseudoInverse(Matrix<double> m)
    {
        var svd = m.Svd();
        var tolerance = Math.Max(m.RowCount, m.ColumnCount) * svd.S[0] * 1e-15;
        var invertedS = new DenseMatrix(m.ColumnCount, m.RowCount);
        for (var i = 0; i < svd.S.Count; i++)
        {
            if (svd.S[i] > tolerance)
                invertedS[i, i] = 1.0 / svd.S[i];
        }
        return svd.VT.Transpose() * invertedS * svd.U.Transpose();
    }

    /// <summary>
    /// Calcola il rank (rango) di una matrice tramite SVD.
    /// </summary>
    public static int MatrixRank(Matrix<double> m)
    {
        var svd = m.Svd();
        var tolerance = Math.Max(m.RowCount, m.ColumnCount) * svd.S[0] * 1e-15;
        int rank = 0;
        for (var i = 0; i < svd.S.Count; i++)
            if (svd.S[i] > tolerance) rank++;
        return rank;
    }

    /// <summary>
    /// Risolve il problema dei minimi quadrati Ax = b tramite SVD.
    /// </summary>
    public static Vector<double> Lstsq(Matrix<double> A, Vector<double> b)
    {
        return A.Solve(b);
    }

    /// <summary>
    /// Calcola la traccia di una matrice (somma degli elementi sulla diagonale principale).
    /// </summary>
    public static double MatrixTrace(Matrix<double> m)
    {
        if (m.RowCount != m.ColumnCount)
            throw new ArgumentException("Matrix must be square");
        return m.Trace();
    }

    /// <summary>
    /// Calcola il prodotto di Kronecker tra due matrici.
    /// </summary>
    public static Matrix<double> Kronecker(Matrix<double> a, Matrix<double> b)
    {
        var result = new DenseMatrix(a.RowCount * b.RowCount, a.ColumnCount * b.ColumnCount);
        for (var i = 0; i < a.RowCount; i++)
        {            for (var j = 0; j < a.ColumnCount; j++)
            {
                for (var k = 0; k < b.RowCount; k++)
                {
                    for (var l = 0; l < b.ColumnCount; l++)
                    {
                        result[i * b.RowCount + k, j * b.ColumnCount + l] = a[i, j] * b[k, l];
                    }
                }
            }
        }
        return result;
    }

    /// <summary>
    /// Costruisce una matrice diagonale a blocchi da una sequenza di matrici.
    /// </summary>
    public static Matrix<double> BlockDiag(params Matrix<double>[] matrices)
    {
        if (matrices.Length == 0)
            return new DenseMatrix(0, 0);

        int totalRows = 0, totalCols = 0;
        foreach (var m in matrices)
        {
            totalRows += m.RowCount;
            totalCols += m.ColumnCount;
        }

        var result = new DenseMatrix(totalRows, totalCols);
        int rowOffset = 0, colOffset = 0;
        foreach (var m in matrices)
        {
            for (var i = 0; i < m.RowCount; i++)
            {
                for (var j = 0; j < m.ColumnCount; j++)
                {
                    result[rowOffset + i, colOffset + j] = m[i, j];
                }
            }
            rowOffset += m.RowCount;
            colOffset += m.ColumnCount;
        }
        return result;
    }

    /// <summary>
    /// Calcola lo spazio nullo (null space) di una matrice tramite SVD.
    /// </summary>
    public static Matrix<double> NullSpace(Matrix<double> m)
    {
        var svd = m.Svd();
        var tolerance = Math.Max(m.RowCount, m.ColumnCount) * svd.S[0] * 1e-15;
        int rank = 0;
        for (var i = 0; i < svd.S.Count; i++)
        {
            if (svd.S[i] > tolerance) rank++;
        }
        int nullDim = svd.VT.RowCount - rank;
        if (nullDim == 0)
            return new DenseMatrix(svd.VT.RowCount, 0);

        var nullSpace = new DenseMatrix(svd.VT.RowCount, nullDim);
        for (var i = 0; i < nullDim; i++)
        {
            for (var j = 0; j < svd.VT.RowCount; j++)
            {
                nullSpace[j, i] = svd.VT[rank + i, j];
            }
        }
        return nullSpace;
    }

    /// <summary>
    /// Calcola la norma di una matrice (L2 norm = massimo valore singolare).
    /// </summary>
    public static double MatrixNorm(Matrix<double> m)
    {
        var svd = m.Svd();
        return svd.S[0];
    }

    /// <summary>
    /// Risolve un sistema lineare Ax = b usando la decomposizione LU.
    /// </summary>
    public static Vector<double> LuSolve(Matrix<double> A, Vector<double> b)
    {
        return A.Solve(b);
    }
}
