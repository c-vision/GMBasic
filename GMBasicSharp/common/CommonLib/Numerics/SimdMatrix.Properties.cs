// FILE: StdLib.SimdMatrix.Properties.cs
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Classe per la gestione efficiente di matrici dense di double in puro C# e con supporto SIMD.
/// Supporta tutte le principali operazioni algebriche, prodotto con vettori, determinante, inversa e matrice identità.
/// </summary>
/// <remarks>
/// Metodi definiti in questo file:
/// <list type="bullet">
///   <item><see cref="IsSymmetric(double)"/> — Verifica se la matrice è simmetrica</item>
///   <item><see cref="IsOrthogonal(double)"/> — Verifica se la matrice è ortogonale</item>
///   <item><see cref="IsUpperTriangular(double)"/> — Verifica se è triangolare superiore</item>
///   <item><see cref="IsLowerTriangular(double)"/> — Verifica se è triangolare inferiore</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Verifica se la matrice è simmetrica (entro una tolleranza).
    /// </summary>
    /// <param name="tol">Tolleranza per il confronto (default 1e-10).</param>
    /// <returns>True se la matrice è simmetrica, false altrimenti.</returns>
    public bool IsSymmetric(double tol = 1e-10)
    {
        if (Rows != Cols) return false;
        for (var i = 0; i < Rows; i++)
        for (var j = i + 1; j < Cols; j++)
            if (Math.Abs(this[i, j] - this[j, i]) > tol) return false;
        return true;
    }
    /// <summary>
    /// Verifica se la matrice è ortogonale (A * A^T = I entro una tolleranza).
    /// </summary>
    /// <param name="tol">Tolleranza per il confronto (default 1e-10).</param>
    /// <returns>True se la matrice è ortogonale, false altrimenti.</returns>
    public bool IsOrthogonal(double tol = 1e-10)
    {
        if (Rows != Cols) return false;
        var prod = (this * this.Transpose());
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            if (i == j)
            {
                if (Math.Abs(prod[i, j] - 1.0) > tol) return false;
            }
            else
            {
                if (Math.Abs(prod[i, j]) > tol) return false;
            }
        return true;
    }
    /// <summary>
    /// Verifica se la matrice è triangolare superiore.
    /// </summary>
    /// <param name="tol">Tolleranza per il confronto (default 1e-10).</param>
    /// <returns>True se la matrice è triangolare superiore, false altrimenti.</returns>
    public bool IsUpperTriangular(double tol = 1e-10)
    {
        if (Rows != Cols) return false;
        for (var i = 1; i < Rows; i++)
        for (var j = 0; j < i; j++)
            if (Math.Abs(this[i, j]) > tol) return false;
        return true;
    }
    /// <summary>
    /// Verifica se la matrice è triangolare inferiore.
    /// </summary>
    /// <param name="tol">Tolleranza per il confronto (default 1e-10).</param>
    /// <returns>True se la matrice è triangolare inferiore, false altrimenti.</returns>
    public bool IsLowerTriangular(double tol = 1e-10)
    {
        if (Rows != Cols) return false;
        for (var i = 0; i < Rows; i++)
        for (var j = i + 1; j < Cols; j++)
            if (Math.Abs(this[i, j]) > tol) return false;
        return true;
    }
}
