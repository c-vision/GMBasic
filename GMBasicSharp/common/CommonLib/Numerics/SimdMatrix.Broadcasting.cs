// FILE: StdLib.SimdMatrix.Broadcasting.cs
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
///   <item><see cref="AddRowVector(double[])"/> — Somma vettore riga (broadcast)</item>
///   <item><see cref="AddColVector(double[])"/> — Somma vettore colonna (broadcast)</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Somma elemento per elemento tra matrice e vettore riga (broadcast sulla riga).
    /// Il vettore viene ripetuto su ogni riga della matrice.
    /// </summary>
    /// <param name="rowVec">Vettore riga di lunghezza pari a <see cref="Cols"/>.</param>
    /// <returns>Nuova matrice risultato della somma.</returns>
    /// <exception cref="ArgumentException">Se la lunghezza del vettore non corrisponde al numero di colonne.</exception>
    public SimdMatrix AddRowVector(double[] rowVec)
    {
        if (rowVec.Length != Cols) throw new ArgumentException("Row vector wrong length");
        var res = new SimdMatrix(Rows, Cols);
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            res[i, j] = this[i, j] + rowVec[j];
        return res;
    }

    /// <summary>
    /// Somma elemento per elemento tra matrice e vettore colonna (broadcast sulla colonna).
    /// Il vettore viene ripetuto su ogni colonna della matrice.
    /// </summary>
    /// <param name="colVec">Vettore colonna di lunghezza pari a <see cref="Rows"/>.</param>
    /// <returns>Nuova matrice risultato della somma.</returns>
    /// <exception cref="ArgumentException">Se la lunghezza del vettore non corrisponde al numero di righe.</exception>
    public SimdMatrix AddColVector(double[] colVec)
    {
        if (colVec.Length != Rows) throw new ArgumentException("Col vector wrong length");
        var res = new SimdMatrix(Rows, Cols);
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            res[i, j] = this[i, j] + colVec[i];
        return res;
    }
}
