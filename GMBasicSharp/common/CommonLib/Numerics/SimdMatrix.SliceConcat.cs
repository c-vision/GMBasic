// FILE: StdLib.SimdMatrix.SliceConcat.cs
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
///   <item><see cref="Slice(int, int, int, int)"/> — Estrae una sottomatrice</item>
///   <item><see cref="ConcatCols(SimdMatrix, SimdMatrix)"/> — Concatenazione orizzontale</item>
///   <item><see cref="ConcatRows(SimdMatrix, SimdMatrix)"/> — Concatenazione verticale</item>
///   <item><see cref="Reshape(int, int)"/> — Cambia la forma della matrice</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Estrae una sottomatrice dalla matrice corrente.
    /// </summary>
    /// <param name="rowStart">Indice di riga iniziale (inclusivo).</param>
    /// <param name="rowEnd">Indice di riga finale (esclusivo).</param>
    /// <param name="colStart">Indice di colonna iniziale (inclusivo).</param>
    /// <param name="colEnd">Indice di colonna finale (esclusivo).</param>
    /// <returns>Nuova matrice di dimensione (rowEnd - rowStart) × (colEnd - colStart).</returns>
    public SimdMatrix Slice(int rowStart, int rowEnd, int colStart, int colEnd)
    {
        var res = new SimdMatrix(rowEnd - rowStart, colEnd - colStart);
        for (var i = rowStart; i < rowEnd; i++)
        for (var j = colStart; j < colEnd; j++)
            res[i - rowStart, j - colStart] = this[i, j];
        return res;
    }

    /// <summary>
    /// Concatenazione orizzontale (affianca colonne di due matrici con stesso numero di righe).
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice (stesso numero di righe di a).</param>
    /// <returns>Nuova matrice con le colonne di a seguite dalle colonne di b.</returns>
    /// <exception cref="ArgumentException">Se il numero di righe delle due matrici differisce.</exception>
    public static SimdMatrix ConcatCols(SimdMatrix a, SimdMatrix b)
    {
        if (a.Rows != b.Rows) throw new ArgumentException("Row mismatch");
        var res = new SimdMatrix(a.Rows, a.Cols + b.Cols);
        for (var i = 0; i < a.Rows; i++)
        {
            for (var j = 0; j < a.Cols; j++)
                res[i, j] = a[i, j];
            for (var j = 0; j < b.Cols; j++)
                res[i, a.Cols + j] = b[i, j];
        }
        return res;
    }

    /// <summary>
    /// Concatenazione verticale (impila due matrici con stesso numero di colonne).
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice (stesso numero di colonne di a).</param>
    /// <returns>Nuova matrice con le righe di a seguite dalle righe di b.</returns>
    /// <exception cref="ArgumentException">Se il numero di colonne delle due matrici differisce.</exception>
    public static SimdMatrix ConcatRows(SimdMatrix a, SimdMatrix b)
    {
        if (a.Cols != b.Cols) throw new ArgumentException("Column mismatch");
        var res = new SimdMatrix(a.Rows + b.Rows, a.Cols);
        for (var i = 0; i < a.Rows; i++)
        for (var j = 0; j < a.Cols; j++)
            res[i, j] = a[i, j];
        for (var i = 0; i < b.Rows; i++)
        for (var j = 0; j < b.Cols; j++)
            res[a.Rows + i, j] = b[i, j];
        return res;
    }

    /// <summary>
    /// Cambia la forma della matrice (reshape). I dati rimangono gli stessi in ordine row-major.
    /// </summary>
    /// <param name="newRows">Nuovo numero di righe.</param>
    /// <param name="newCols">Nuovo numero di colonne.</param>
    /// <returns>Nuova matrice con la forma specificata e gli stessi dati.</returns>
    /// <exception cref="ArgumentException">Se il prodotto newRows × newCols non corrisponde al numero totale di elementi.</exception>
    public SimdMatrix Reshape(int newRows, int newCols)
    {
        if (newRows * newCols != Rows * Cols)
            throw new ArgumentException("Total size mismatch");
        return new SimdMatrix((double[])_data.Clone(), newRows, newCols);
    }
}
