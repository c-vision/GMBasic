// FILE: StdLib.SimdMatrix.Norms.cs
using System.Numerics;
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
///   <item><see cref="FrobeniusNorm"/> — Norma di Frobenius</item>
///   <item><see cref="OneNorm"/> — 1-norma (massima somma colonna)</item>
///   <item><see cref="InfinityNorm"/> — Norma infinito (massima somma riga)</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Calcola la norma di Frobenius della matrice.
    /// </summary>
    /// <returns>Norma di Frobenius (radice quadrata della somma dei quadrati di tutti gli elementi).</returns>
    public double FrobeniusNorm()
    {
        double sum = 0;
        var simdLen = Vector<double>.Count;
        var i = 0;
        for (; i <= _data.Length - simdLen; i += simdLen)
        {
            var v = new Vector<double>(_data, i);
            sum += Vector.Dot(v, v);
        }
        for (; i < _data.Length; i++)
            sum += _data[i] * _data[i];
        return Math.Sqrt(sum);
    }

    /// <summary>
    /// Calcola la 1-norma della matrice (massima somma assoluta per colonna).
    /// </summary>
    /// <returns>1-norma della matrice.</returns>
    public double OneNorm()
    {
        double max = 0;
        for (var j = 0; j < Cols; j++)
        {
            double colSum = 0;
            for (var i = 0; i < Rows; i++)
                colSum += Math.Abs(this[i, j]);
            if (colSum > max) max = colSum;
        }
        return max;
    }

    /// <summary>
    /// Calcola la norma infinito della matrice (massima somma assoluta per riga).
    /// </summary>
    /// <returns>Norma infinito della matrice.</returns>
    public double InfinityNorm()
    {
        double max = 0;
        for (var i = 0; i < Rows; i++)
        {
            double rowSum = 0;
            for (var j = 0; j < Cols; j++)
                rowSum += Math.Abs(this[i, j]);
            if (rowSum > max) max = rowSum;
        }
        return max;
    }
}
