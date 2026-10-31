// FILE: StdLib.SimdMatrix.cs
// ReSharper disable CheckNamespace

using System.Numerics;
// ReSharper disable MemberCanBePrivate.Global

namespace CommonLib;

/// <summary>
/// Classe per la gestione efficiente di matrici dense di double in puro C# e con supporto SIMD.
/// Supporta tutte le principali operazioni algebriche, prodotto con vettori, determinante, inversa e matrice identità.
/// </summary>
/// <remarks>
/// Metodi e proprietà definiti in questo file:
/// <list type="bullet">
///   <item><see cref="SimdMatrix(int, int)"/> — Costruttore a zeri</item>
///   <item><see cref="SimdMatrix(double[], int, int)"/> — Costruttore da array</item>
///   <item><see cref="Rows"/> — Numero di righe</item>
///   <item><see cref="Cols"/> — Numero di colonne</item>
///   <item><see cref="this[int, int]"/> — Accesso per indice</item>
///   <item><see cref="Identity(int)"/> — Matrice identità</item>
///   <item><see cref="ToArray"/> — Copia dell'array interno</item>
///   <item><see cref="operator +(SimdMatrix, SimdMatrix)"/> — Somma tra matrici</item>
///   <item><see cref="operator -(SimdMatrix, SimdMatrix)"/> — Sottrazione tra matrici</item>
///   <item><see cref="operator *(SimdMatrix, double)"/> — Moltiplicazione per scalare</item>
///   <item><see cref="operator *(double, SimdMatrix)"/> — Moltiplicazione per scalare (invertito)</item>
///   <item><see cref="operator *(SimdMatrix, SimdMatrix)"/> — Prodotto tra matrici</item>
///   <item><see cref="Multiply(double[])"/> — Prodotto matrice-vettore</item>
///   <item><see cref="MultiplyFromLeft(double[])"/> — Prodotto vettore-matrice</item>
///   <item><see cref="Transpose"/> — Trasposizione</item>
///   <item><see cref="Determinant"/> — Determinante</item>
///   <item><see cref="Inverse"/> — Inversa</item>
///   <item><see cref="Equals(object)"/> — Confronto con tolleranza</item>
///   <item><see cref="GetHashCode"/> — Hash code</item>
///   <item><see cref="Clone"/> — Copia profonda</item>
///   <item><see cref="SwapRows(int, int)"/> — Scambio righe</item>
///   <item><see cref="SwapCols(int, int)"/> — Scambio colonne</item>
///   <item><see cref="ToString"/> — Rappresentazione testuale</item>
/// </list>
/// </remarks>
/// <example>
/// Esempio di utilizzo:
/// <code>
/// // Creazione di una matrice 2x2
/// var A = new SimdMatrix(new double[] { 1, 2, 3, 4 }, 2, 2);
/// // Matrice identità
/// var I = SimdMatrix.Identity(2);
/// // Somma e prodotto tra matrici
/// var somma = A + I;
/// var prodotto = A * I;
/// // Prodotto matrice-vettore
/// var v = new double[] { 1, 1 };
/// var Av = A.Multiply(v);
/// // Determinante e inversa
/// double det = A.Determinant();
/// var inv = A.Inverse();
/// </code>
/// </example>
public partial class SimdMatrix
{
    /// <summary>
    /// Array monodimensionale in formato row-major contenente tutti gli elementi della matrice.
    /// </summary>
    private readonly double[] _data;

    /// <summary>
    /// Numero di righe della matrice.
    /// </summary>
    public int Rows { get; }

    /// <summary>
    /// Numero di colonne della matrice.
    /// </summary>
    public int Cols { get; }

    /// <summary>
    /// Crea una matrice di dimensioni specificate inizializzata a zeri.
    /// </summary>
    /// <param name="rows">Numero di righe.</param>
    /// <param name="cols">Numero di colonne.</param>
    public SimdMatrix(int rows, int cols)
    {
        Rows = rows;
        Cols = cols;
        _data = new double[rows * cols];
    }

    /// <summary>
    /// Crea una matrice dalle celle specificate come array monodimensionale in formato row-major.
    /// </summary>
    /// <param name="flat">Array di valori (lunghezza = rows * cols).</param>
    /// <param name="rows">Numero di righe.</param>
    /// <param name="cols">Numero di colonne.</param>
    /// <exception cref="ArgumentException">Se la lunghezza dell'array non corrisponde a rows*cols.</exception>
    public SimdMatrix(double[] flat, int rows, int cols)
    {
        if (flat.Length != rows * cols)
            throw new ArgumentException("Mismatched dimensions");
        Rows = rows;
        Cols = cols;
        _data = (double[])flat.Clone();
    }

    /// <summary>
    /// Crea una matrice identità quadrata di dimensione n x n.
    /// </summary>
    /// <param name="n">Dimensione della matrice identità (numero di righe e colonne).</param>
    /// <returns>Matrice identità nxn.</returns>
    /// <example>
    /// <code>
    /// var I = SimdMatrix.Identity(3); // 3x3
    /// </code>
    /// </example>
    public static SimdMatrix Identity(int n)
    {
        var res = new SimdMatrix(n, n);
        for (var i = 0; i < n; i++)
            res[i, i] = 1.0;
        return res;
    }

    /// <summary>
    /// Accesso o modifica di un elemento (riga, colonna) della matrice.
    /// </summary>
    /// <param name="r">Indice di riga (0-based).</param>
    /// <param name="c">Indice di colonna (0-based).</param>
    /// <returns>Valore della cella.</returns>
    public double this[int r, int c]
    {
        get => _data[r * Cols + c];
        set => _data[r * Cols + c] = value;
    }

    /// <summary>
    /// Restituisce una copia dell'array interno in formato row-major.
    /// </summary>
    /// <returns>Array di double con tutti gli elementi della matrice.</returns>
    public double[] ToArray() => (double[])_data.Clone();

    /// <summary>
    /// Somma elemento per elemento tra due matrici (SIMD ottimizzato).
    /// Utilizza SIMD per sommare blocchi contigui di double, massimizzando la velocità.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla somma.</returns>
    /// <exception cref="ArgumentException">Se le dimensioni non coincidono.</exception>
    public static SimdMatrix operator +(SimdMatrix a, SimdMatrix b)
    {
        if (a.Rows != b.Rows || a.Cols != b.Cols)
            throw new ArgumentException("Dimension mismatch");
        var res = new SimdMatrix(a.Rows, a.Cols);
        var len = a._data.Length;
        var simdLen = Vector<double>.Count;
        var i = 0;
        // SIMD: somma blocchi contigui di double
        for (; i <= len - simdLen; i += simdLen)
        {
            var va = new Vector<double>(a._data, i);
            var vb = new Vector<double>(b._data, i);
            (va + vb).CopyTo(res._data, i);
        }
        // Somma scalare per la coda rimanente
        for (; i < len; i++)
            res._data[i] = a._data[i] + b._data[i];
        return res;
    }

    /// <summary>
    /// Sottrazione elemento per elemento tra due matrici (SIMD ottimizzato).
    /// Utilizza SIMD per sottrarre blocchi contigui di double.
    /// </summary>
    /// <param name="a">Prima matrice.</param>
    /// <param name="b">Seconda matrice.</param>
    /// <returns>Nuova matrice risultante dalla sottrazione.</returns>
    /// <exception cref="ArgumentException">Se le dimensioni non coincidono.</exception>
    public static SimdMatrix operator -(SimdMatrix a, SimdMatrix b)
    {
        if (a.Rows != b.Rows || a.Cols != b.Cols)
            throw new ArgumentException("Dimension mismatch");
        var res = new SimdMatrix(a.Rows, a.Cols);
        var len = a._data.Length;
        var simdLen = Vector<double>.Count;
        var i = 0;
        // SIMD: sottrai blocchi contigui di double
        for (; i <= len - simdLen; i += simdLen)
        {
            var va = new Vector<double>(a._data, i);
            var vb = new Vector<double>(b._data, i);
            (va - vb).CopyTo(res._data, i);
        }
        // Sottrazione scalare per la coda rimanente
        for (; i < len; i++)
            res._data[i] = a._data[i] - b._data[i];
        return res;
    }

    /// <summary>
    /// Moltiplicazione di una matrice per uno scalare (SIMD ottimizzato).
    /// Utilizza SIMD per moltiplicare blocchi contigui di double per lo scalare.
    /// </summary>
    /// <param name="a">Matrice da moltiplicare.</param>
    /// <param name="scalar">Valore scalare.</param>
    /// <returns>Nuova matrice moltiplicata per lo scalare.</returns>
    public static SimdMatrix operator *(SimdMatrix a, double scalar)
    {
        var res = new SimdMatrix(a.Rows, a.Cols);
        var len = a._data.Length;
        var simdLen = Vector<double>.Count;
        var vscal = new Vector<double>(scalar);
        var i = 0;
        // SIMD: moltiplica blocchi contigui per scalare
        for (; i <= len - simdLen; i += simdLen)
        {
            var va = new Vector<double>(a._data, i);
            (va * vscal).CopyTo(res._data, i);
        }
        // Moltiplicazione scalare per la coda rimanente
        for (; i < len; i++)
            res._data[i] = a._data[i] * scalar;
        return res;
    }

    /// <summary>
    /// Moltiplicazione di una matrice per uno scalare (SIMD ottimizzato, ordine invertito).
    /// </summary>
    /// <param name="scalar">Valore scalare.</param>
    /// <param name="a">Matrice da moltiplicare.</param>
    /// <returns>Nuova matrice moltiplicata per lo scalare.</returns>
    public static SimdMatrix operator *(double scalar, SimdMatrix a) => a * scalar;

    /// <summary>
    /// Prodotto tra due matrici (SIMD ottimizzato).
    /// SIMD viene usato per il prodotto riga × colonna (con gather manuale sulla colonna).
    /// </summary>
    /// <param name="a">Matrice a sinistra.</param>
    /// <param name="b">Matrice a destra.</param>
    /// <returns>Nuova matrice risultato del prodotto.</returns>
    /// <exception cref="ArgumentException">Se le dimensioni sono incompatibili.</exception>
    public static SimdMatrix operator *(SimdMatrix a, SimdMatrix b)
    {
        if (a.Cols != b.Rows)
            throw new ArgumentException("Dimension mismatch for matrix multiplication");
        int m = a.Rows, n = a.Cols, p = b.Cols;
        var res = new SimdMatrix(m, p);

        var simdLen = Vector<double>.Count;
        var ad = a._data;
        var bd = b._data;
        var rd = res._data;

        var temp = new double[simdLen];

        for (var i = 0; i < m; i++)
        {
            var ai = i * n;
            var ri = i * p;
            for (var j = 0; j < p; j++)
            {
                double sum = 0;
                var k = 0;
                if (simdLen > 1)
                {
                    var vsum = Vector<double>.Zero;
                    // SIMD: prodotto e somma di blocchi della riga di A × colonna di B
                    for (; k <= n - simdLen; k += simdLen)
                    {
                        var va = new Vector<double>(ad, ai + k);
                        // Gather manuale per la colonna di B (non contigua in memoria)
                        for (var t = 0; t < simdLen; t++)
                            temp[t] = bd[(k + t) * p + j];
                        var vb = new Vector<double>(temp);
                        vsum += va * vb;
                    }
                    for (var t = 0; t < simdLen; t++)
                        sum += vsum[t];
                }
                // Resto scalare
                for (; k < n; k++)
                    sum += ad[ai + k] * bd[k * p + j];
                rd[ri + j] = sum;
            }
        }
        return res;
    }

    /// <summary>
    /// Prodotto matrice-vettore (SIMD ottimizzato).
    /// SIMD viene utilizzato per moltiplicare blocchi contigui di riga della matrice × blocchi di vettore.
    /// </summary>
    /// <param name="vector">Array double di lunghezza pari al numero di colonne della matrice.</param>
    /// <returns>Array double risultante (lunghezza = numero righe).</returns>
    /// <exception cref="ArgumentException">Se le dimensioni sono incompatibili.</exception>
    /// <example>
    /// <code>
    /// var A = new SimdMatrix(new double[] {1,2,3,4},2,2);
    /// var v = new double[] {1,1};
    /// var res = A.Multiply(v); // [3,7]
    /// </code>
    /// </example>
    public double[] Multiply(double[] vector)
    {
        if (Cols != vector.Length)
            throw new ArgumentException("Dimension mismatch for matrix-vector multiplication");
        var result = new double[Rows];
        var simdLen = Vector<double>.Count;
        for (var i = 0; i < Rows; i++)
        {
            double sum = 0;
            var j = 0;
            var rowStart = i * Cols;
            if (simdLen > 1)
            {
                var vsum = Vector<double>.Zero;
                // SIMD: moltiplica blocchi contigui della riga di matrice × blocchi di vettore
                for (; j <= Cols - simdLen; j += simdLen)
                {
                    var va = new Vector<double>(_data, rowStart + j);
                    var vb = new Vector<double>(vector, j);
                    vsum += va * vb;
                }
                for (var t = 0; t < simdLen; t++)
                    sum += vsum[t];
            }
            // Resto scalare
            for (; j < Cols; j++)
                sum += _data[rowStart + j] * vector[j];
            result[i] = sum;
        }
        return result;
    }

    /// <summary>
    /// Prodotto vettore (a sinistra) per matrice.
    /// Per colonne non contigue, SIMD non è utilizzato.
    /// </summary>
    /// <param name="vector">Array double di lunghezza pari al numero di righe.</param>
    /// <returns>Array double di lunghezza pari al numero di colonne.</returns>
    public double[] MultiplyFromLeft(double[] vector)
    {
        if (Rows != vector.Length)
            throw new ArgumentException("Dimension mismatch for vector-matrix multiplication");
        var result = new double[Cols];
        // Non si può usare SIMD efficientemente sulle colonne (dati non contigui)
        for (var j = 0; j < Cols; j++)
        {
            double sum = 0;
            for (var i = 0; i < Rows; i++)
                sum += vector[i] * this[i, j];
            result[j] = sum;
        }
        return result;
    }

    /// <summary>
    /// Restituisce la trasposta della matrice.
    /// </summary>
    /// <returns>Nuova matrice trasposta.</returns>
    /// <example>
    /// <code>
    /// var A = new SimdMatrix(new double[] {1,2,3,4},2,2);
    /// var T = A.Transpose(); // T[0,1] == 3, T[1,0] == 2
    /// </code>
    /// </example>
    public SimdMatrix Transpose()
    {
        var res = new SimdMatrix(Cols, Rows);
        for (var i = 0; i < Rows; i++)
        for (var j = 0; j < Cols; j++)
            res._data[j * Rows + i] = _data[i * Cols + j];
        return res;
    }

    /// <summary>
    /// Calcola il determinante della matrice quadrata usando decomposizione LU (senza pivoting).
    /// </summary>
    /// <returns>Determinante della matrice.</returns>
    /// <exception cref="InvalidOperationException">Se la matrice non è quadrata.</exception>
    /// <example>
    /// <code>
    /// var A = new SimdMatrix(new double[] {1,2,3,4},2,2);
    /// double det = A.Determinant(); // -2
    /// </code>
    /// </example>
    public double Determinant()
    {
        if (Rows != Cols) throw new InvalidOperationException("Matrix must be square");
        var n = Rows;
        var mat = (double[])_data.Clone();
        var det = 1.0;
        for (var i = 0; i < n; i++)
        {
            var pivot = i * n + i;
            if (Math.Abs(mat[pivot]) < 1e-16) return 0.0;
            det *= mat[pivot];
            for (var j = i + 1; j < n; j++)
            {
                var f = mat[j * n + i] / mat[pivot];
                for (var k = i; k < n; k++)
                    mat[j * n + k] -= f * mat[i * n + k];
            }
        }
        return det;
    }

    /// <summary>
    /// Calcola l'inversa della matrice quadrata tramite eliminazione Gauss-Jordan.
    /// </summary>
    /// <returns>Nuova matrice inversa.</returns>
    /// <exception cref="InvalidOperationException">Se la matrice non è quadrata o è singolare.</exception>
    /// <example>
    /// <code>
    /// var A = new SimdMatrix(new double[] {1,2,3,4},2,2);
    /// var inv = A.Inverse();
    /// </code>
    /// </example>
    public SimdMatrix Inverse()
    {
        if (Rows != Cols) throw new InvalidOperationException("Matrix must be square");
        var n = Rows;
        var mat = new double[n, 2 * n];
        // Copy data and append identity
        for (var i = 0; i < n; i++)
        for (var j = 0; j < n; j++)
            mat[i, j] = this[i, j];
        for (var i = 0; i < n; i++)
            mat[i, n + i] = 1.0;

        // Gauss-Jordan
        for (var i = 0; i < n; i++)
        {
            var pivot = mat[i, i];
            if (Math.Abs(pivot) < 1e-16) throw new InvalidOperationException("Matrix is singular");
            for (var j = 0; j < 2 * n; j++)
                mat[i, j] /= pivot;
            for (var k = 0; k < n; k++)
            {
                if (k == i) continue;
                var factor = mat[k, i];
                for (var j = 0; j < 2 * n; j++)
                    mat[k, j] -= factor * mat[i, j];
            }
        }
        // Extract inverse
        var res = new SimdMatrix(n, n);
        for (var i = 0; i < n; i++)
        for (var j = 0; j < n; j++)
            res[i, j] = mat[i, n + j];
        return res;
    }

    /// <summary>
    /// Determina se due matrici sono uguali (stessa dimensione e valori simili entro una certa tolleranza).
    /// </summary>
    /// <param name="obj">Oggetto da confrontare.</param>
    /// <returns>True se uguali entro la tolleranza, false altrimenti.</returns>
    public override bool Equals(object? obj)
    {
        const double epsilon = 1e-10; // tolleranza per il confronto
        if (obj is not SimdMatrix other || other.Rows != Rows || other.Cols != Cols) return false;
        for (var i = 0; i < _data.Length; i++)
            if (Math.Abs(_data[i] - other._data[i]) > epsilon) return false;
        return true;
    }

    /// <summary>
    /// Hash code della matrice, combinato da righe, colonne e lunghezza dell'array interno.
    /// </summary>
    /// <returns>Valore hash intero.</returns>
    public override int GetHashCode() => HashCode.Combine(Rows, Cols, _data.Length);

    /// <summary>
    /// Restituisce una copia profonda della matrice.
    /// </summary>
    /// <returns>Nuova matrice con gli stessi valori.</returns>
    public SimdMatrix Clone() => new(_data, Rows, Cols);

    /// <summary>
    /// Scambia due righe della matrice.
    /// </summary>
    /// <param name="rowA">Indice della prima riga.</param>
    /// <param name="rowB">Indice della seconda riga.</param>
    public void SwapRows(int rowA, int rowB)
    {
        if (rowA == rowB) return;
        for (var j = 0; j < Cols; j++)
        {
            var idxA = rowA * Cols + j;
            var idxB = rowB * Cols + j;
            (_data[idxA], _data[idxB]) = (_data[idxB], _data[idxA]);
        }
    }

    /// <summary>
    /// Scambia due colonne della matrice.
    /// </summary>
    /// <param name="colA">Indice della prima colonna.</param>
    /// <param name="colB">Indice della seconda colonna.</param>
    public void SwapCols(int colA, int colB)
    {
        if (colA == colB) return;
        for (var i = 0; i < Rows; i++)
        {
            var idxA = i * Cols + colA;
            var idxB = i * Cols + colB;
            (_data[idxA], _data[idxB]) = (_data[idxB], _data[idxA]);
        }
    }

    /// <summary>
    /// Restituisce una rappresentazione testuale della matrice.
    /// </summary>
    /// <returns>Stringa con i valori della matrice, formattati per riga.</returns>
    public override string ToString()
    {
        var s = "";
        for (var i = 0; i < Rows; i++)
        {
            for (var j = 0; j < Cols; j++)
                s += $"{this[i, j]:0.###}\t";
            s += Environment.NewLine;
        }
        return s;
    }
}
