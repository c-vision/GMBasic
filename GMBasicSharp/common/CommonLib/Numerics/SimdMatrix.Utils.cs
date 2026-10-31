// FILE: StdLib.SimdMatrix.Utils.cs
// ReSharper disable CheckNamespace
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable MemberCanBePrivate.Global
namespace CommonLib;

/// <summary>
/// Classe per la gestione efficiente di matrici dense di double in puro C# e con supporto SIMD.
/// Supporta tutte le principali operazioni algebriche, prodotto con vettori, determinante, inversa e matrice identità.
/// </summary>
/// <remarks>
/// Metodi definiti in questo file:
/// <list type="bullet">
///   <item><see cref="Max"/> — Valore massimo</item>
///   <item><see cref="Min"/> — Valore minimo</item>
///   <item><see cref="ArgMax"/> — Posizione del massimo</item>
///   <item><see cref="ArgMin"/> — Posizione del minimo</item>
///   <item><see cref="Clamp(double, double)"/> — Limita valori tra min e max</item>
///   <item><see cref="Fill(double)"/> — Riempie la matrice con un valore</item>
///   <item><see cref="MapInPlace(Func{double, double})"/> — Applica funzione in-place</item>
///   <item><see cref="Map(Func{double, double})"/> — Applica funzione e restituisce nuova matrice</item>
///   <item><see cref="GreaterThan(double)"/> — Confronto con soglia (matrice booleana 2D)</item>
///   <item><see cref="Mask(double)"/> — Maschera booleana</item>
/// </list>
/// </remarks>
public partial class SimdMatrix
{
    /// <summary>
    /// Restituisce il valore massimo nella matrice.
    /// </summary>
    /// <returns>Valore massimo tra tutti gli elementi.</returns>
    public double Max()
    {
        var max = _data[0];
        for (var i = 1; i < _data.Length; i++)
            if (_data[i] > max) max = _data[i];
        return max;
    }

    /// <summary>
    /// Restituisce il valore minimo nella matrice.
    /// </summary>
    /// <returns>Valore minimo tra tutti gli elementi.</returns>
    public double Min()
    {
        var min = _data[0];
        for (var i = 1; i < _data.Length; i++)
            if (_data[i] < min) min = _data[i];
        return min;
    }

    /// <summary>
    /// Restituisce la posizione (riga, colonna) del valore massimo.
    /// </summary>
    /// <returns>Tupla (riga, colonna) del primo elemento massimo trovato.</returns>
    public (int row, int col) ArgMax()
    {
        var max = _data[0];
        var idx = 0;
        for (var i = 1; i < _data.Length; i++)
            if (_data[i] > max) { max = _data[i]; idx = i; }
        return (idx / Cols, idx % Cols);
    }

    /// <summary>
    /// Restituisce la posizione (riga, colonna) del valore minimo.
    /// </summary>
    /// <returns>Tupla (riga, colonna) del primo elemento minimo trovato.</returns>
    public (int row, int col) ArgMin()
    {
        var min = _data[0];
        var idx = 0;
        for (var i = 1; i < _data.Length; i++)
            if (_data[i] < min) { min = _data[i]; idx = i; }
        return (idx / Cols, idx % Cols);
    }

    /// <summary>
    /// Limita tutti gli elementi della matrice tra min e max (in-place).
    /// </summary>
    /// <param name="min">Valore minimo consentito.</param>
    /// <param name="max">Valore massimo consentito.</param>
    public void Clamp(double min, double max)
    {
        for (var i = 0; i < _data.Length; i++)
            _data[i] = Math.Max(min, Math.Min(max, _data[i]));
    }

    /// <summary>
    /// Riempie la matrice con il valore specificato.
    /// </summary>
    /// <param name="value">Valore da assegnare a tutti gli elementi.</param>
    public void Fill(double value)
    {
        for (var i = 0; i < _data.Length; i++)
            _data[i] = value;
    }

    /// <summary>
    /// Applica una funzione elemento per elemento alla matrice (in-place).
    /// </summary>
    /// <param name="func">Funzione da applicare a ogni elemento.</param>
    public void MapInPlace(Func<double, double> func)
    {
        for (var i = 0; i < _data.Length; i++)
            _data[i] = func(_data[i]);
    }

    /// <summary>
    /// Applica una funzione elemento per elemento e restituisce una nuova matrice.
    /// </summary>
    /// <param name="func">Funzione da applicare a ogni elemento.</param>
    /// <returns>Nuova matrice con i valori trasformati.</returns>
    public SimdMatrix Map(Func<double, double> func)
    {
        var res = new SimdMatrix(Rows, Cols);
        for (var i = 0; i < _data.Length; i++)
            res._data[i] = func(_data[i]);
        return res;
    }

    /// <summary>
    /// Applica un confronto elemento per elemento rispetto a un valore soglia.
    /// Restituisce una matrice booleana 2D dove true indica elementi maggiori della soglia.
    /// </summary>
    /// <param name="threshold">Valore soglia.</param>
    /// <returns>Matrice booleana rows×cols con l'esito del confronto.</returns>
    public bool[,] GreaterThan(double threshold)
    {
        var outMask = new bool[Rows, Cols];
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            outMask[i, j] = this[i, j] > threshold;
        return outMask;
    }

    /// <summary>
    /// Maschera booleana: true se elemento > soglia.
    /// Restituisce un array monodimensionale allineato all'array interno.
    /// </summary>
    /// <param name="threshold">Valore soglia.</param>
    /// <returns>Array booleano di lunghezza Rows×Cols.</returns>
    public bool[] Mask(double threshold)
    {
        var mask = new bool[_data.Length];
        for (var i = 0; i < _data.Length; i++)
            mask[i] = _data[i] > threshold;
        return mask;
    }
}
