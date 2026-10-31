// FILE: StdLib.SimdMatrix.RowsCols.cs
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
///   <item><see cref="RowSums"/> — Somma per riga</item>
///   <item><see cref="ColSums"/> — Somma per colonna</item>
///   <item><see cref="RowMeans"/> — Media per riga</item>
///   <item><see cref="ColMeans"/> — Media per colonna</item>
///   <item><see cref="RowVariances"/> — Varianza per riga</item>
///   <item><see cref="ColVariances"/> — Varianza per colonna</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Restituisce la somma degli elementi di ciascuna riga.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di righe, contenente la somma di ogni riga.</returns>
    public double[] RowSums()
    {
        var sums = new double[Rows];
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            sums[i] += this[i, j];
        return sums;
    }

    /// <summary>
    /// Restituisce la somma degli elementi di ciascuna colonna.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di colonne, contenente la somma di ogni colonna.</returns>
    public double[] ColSums()
    {
        var sums = new double[Cols];
        for (var j = 0; j < Cols; j++)
        for (var i = 0; i < Rows; i++)
            sums[j] += this[i, j];
        return sums;
    }

    /// <summary>
    /// Restituisce la media degli elementi di ciascuna riga.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di righe, contenente la media di ogni riga.</returns>
    public double[] RowMeans()
    {
        var sums = RowSums();
        for (var i = 0; i < Rows; i++)
            sums[i] /= Cols;
        return sums;
    }

    /// <summary>
    /// Restituisce la media degli elementi di ciascuna colonna.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di colonne, contenente la media di ogni colonna.</returns>
    public double[] ColMeans()
    {
        var sums = ColSums();
        for (var j = 0; j < Cols; j++)
            sums[j] /= Rows;
        return sums;
    }

    /// <summary>
    /// Restituisce la varianza degli elementi di ciascuna riga.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di righe, contenente la varianza di ogni riga.</returns>
    public double[] RowVariances()
    {
        var means = RowMeans();
        var vars = new double[Rows];
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            vars[i] += Math.Pow(this[i, j] - means[i], 2);
        for (var i = 0; i < Rows; i++)
            vars[i] /= Cols;
        return vars;
    }

    /// <summary>
    /// Restituisce la varianza degli elementi di ciascuna colonna.
    /// </summary>
    /// <returns>Array di double di lunghezza pari al numero di colonne, contenente la varianza di ogni colonna.</returns>
    public double[] ColVariances()
    {
        var means = ColMeans();
        var vars = new double[Cols];
        for (var j = 0; j < Cols; j++)
        for (var i = 0; i < Rows; i++)
            vars[j] += Math.Pow(this[i, j] - means[j], 2);
        for (var j = 0; j < Cols; j++)
            vars[j] /= Rows;
        return vars;
    }
}
