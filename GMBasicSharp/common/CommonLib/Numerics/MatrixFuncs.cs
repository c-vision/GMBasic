// FILE: CommonLib.Numerics.MatrixFuncs.cs
// Funzioni matriciali scipy.linalg per GMBasic (runtime C#), implementate qui in modo
// autonomo (nessuna dipendenza da codice altrui non committato):
//   MatrixExp — esponenziale di matrice (scipy.linalg.expm): scaling-and-squaring + Taylor.
//   MatrixKron — prodotto di Kronecker (numpy.kron / scipy.linalg.kron).

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>Esponenziale di matrice (scipy.linalg.expm).</summary>
    public static Matrix<double> MatrixExp(Matrix<double> a)
    {
        if (a.RowCount != a.ColumnCount)
            throw new ArgumentException("expm richiede una matrice quadrata.", nameof(a));
        var n = a.RowCount;
        if (n == 0) return new DenseMatrix(0, 0);
        var norm = a.InfinityNorm();
        var s = Math.Max(0, (int)Math.Ceiling(Math.Log2(norm)) + 1);
        var scaled = a / Math.Pow(2, s);
        Matrix<double> result = DenseMatrix.CreateIdentity(n);
        Matrix<double> term = DenseMatrix.CreateIdentity(n);
        for (var k = 1; k <= 25; k++)
        {
            term = term * scaled / k;
            result = result + term;
        }
        for (var i = 0; i < s; i++)
            result = result * result;
        return result;
    }

    /// <summary>Prodotto di Kronecker A⊗B (numpy.kron).</summary>
    public static Matrix<double> MatrixKron(Matrix<double> a, Matrix<double> b)
    {
        var result = new DenseMatrix(a.RowCount * b.RowCount, a.ColumnCount * b.ColumnCount);
        for (var i = 0; i < a.RowCount; i++)
            for (var j = 0; j < a.ColumnCount; j++)
            {
                var av = a[i, j];
                if (av == 0) continue;
                for (var k = 0; k < b.RowCount; k++)
                    for (var l = 0; l < b.ColumnCount; l++)
                        result[i * b.RowCount + k, j * b.ColumnCount + l] = av * b[k, l];
            }
        return result;
    }

    /// <summary>Radice quadrata di matrice (scipy.linalg.sqrtm): A = V·D·V⁻¹ con D la diag
    /// degli autovalori (reali), sqrtm = V·√D·V⁻¹. Valida per autovalori reali ≥ 0.</summary>
    public static Matrix<double> MatrixSqrt(Matrix<double> a)
    {
        var n = a.RowCount;
        var evd = a.Evd();
        var v = evd.EigenVectors;
        var vals = evd.EigenValues;
        var diag = Matrix<double>.Build.DenseIdentity(n);
        for (var i = 0; i < n; i++) diag[i, i] = Math.Sqrt(vals[i].Real);
        return v * diag * v.Inverse();
    }

    /// <summary>Logaritmo di matrice (scipy.linalg.logm): logm = V·log(D)·V⁻¹ con D la diag
    /// degli autovalori (reali > 0).</summary>
    public static Matrix<double> MatrixLogm(Matrix<double> a)
    {
        var n = a.RowCount;
        var evd = a.Evd();
        var v = evd.EigenVectors;
        var vals = evd.EigenValues;
        var diag = Matrix<double>.Build.DenseIdentity(n);
        for (var i = 0; i < n; i++) diag[i, i] = Math.Log(vals[i].Real);
        return v * diag * v.Inverse();
    }

    /// <summary>Forma di Hessenberg superiore (scipy.linalg.hessenberg): H = Q·Hh·Qᵀ con
    /// Householder. Zeri sotto la sottodiagonale.</summary>
    public static Matrix<double> MatrixHessenberg(Matrix<double> a)
    {
        var n = a.RowCount;
        var h = a.Clone();
        for (var k = 0; k < n - 2; k++)
        {
            var m = n - k - 1;
            var x = new double[m];
            for (var i = 0; i < m; i++) x[i] = h[k + 1 + i, k];
            double norm = 0;
            foreach (var xv in x) norm += xv * xv;
            norm = Math.Sqrt(norm);
            if (norm < 1e-14) continue;
            var alpha = x[0] >= 0 ? -norm : norm;
            var u = new double[m];
            u[0] = x[0] - alpha;
            for (var i = 1; i < m; i++) u[i] = x[i];
            double unorm = 0;
            foreach (var uv in u) unorm += uv * uv;
            unorm = Math.Sqrt(unorm);
            if (unorm < 1e-14) continue;
            for (var i = 0; i < m; i++) u[i] /= unorm;
            // P = I - 2 u u^T (blocco inferiore-destro)
            var p = Matrix<double>.Build.DenseIdentity(n);
            for (var i = 0; i < m; i++)
                for (var j = 0; j < m; j++)
                    p[k + 1 + i, k + 1 + j] -= 2.0 * u[i] * u[j];
            h = p * h * p;
        }
        return h;
}
}
