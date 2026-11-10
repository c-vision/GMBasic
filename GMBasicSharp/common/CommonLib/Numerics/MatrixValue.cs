// FILE: CommonLib/Numerics/MatrixValue.cs
// Descrittore di matrice a valore (mirror gestito di `gmb_matrix_t` in
// native/gmb_runtime_c/include/gmb_matrix_value.h).
//
// Rappresentazione: { storage condiviso, rows, cols, rowStride, colStride, offset }.
// Gli elementi si leggono/scrivono SEMPRE via Get/Set (mai indicizzazione a mano):
// una view (SLICE/TRANSPOSE/ROW/COL) condivide lo storage del proprietario senza
// copiare ed è O(1). La copia di un descrittore condivide lo storage (reference
// semantics, come gli array CLR); `Clone()` crea uno storage indipendente.
//
// Questa è la base del percorso "descrittore prima" (F4): abilita view a strides
// reali e la rimozione del registry handle nei backend, mantenendo l'equivalenza
// con i valori piatti esistenti tramite ToRect/FromRect.

namespace CommonLib;

/// <summary>Matrice a valore con descrittore e view a strides (O(1)).</summary>
public sealed class MatrixValue
{
    private readonly double[] _storage;   // storage condiviso (row-major o layout arbitrario)
    private readonly int _offset;         // indice del primo elemento in _storage

    private MatrixValue(double[] storage, int rows, int cols, int rowStride, int colStride, int offset)
    {
        _storage = storage;
        Rows = rows;
        Cols = cols;
        RowStride = rowStride;
        ColStride = colStride;
        _offset = offset;
    }

    /// <summary>Numero di righe.</summary>
    public int Rows { get; }

    /// <summary>Numero di colonne.</summary>
    public int Cols { get; }

    /// <summary>Salto tra righe, in elementi dello storage.</summary>
    public int RowStride { get; }

    /// <summary>Salto tra colonne, in elementi dello storage.</summary>
    public int ColStride { get; }

    /// <summary>Storage condiviso (non copiato).</summary>
    public double[] Storage => _storage;

    /// <summary>Indice del primo elemento in <see cref="Storage"/>.</summary>
    public int Offset => _offset;

    /// <summary>Matrice vuota (0×0).</summary>
    public static MatrixValue Empty { get; } = new([], 0, 0, 0, 1, 0);

    /// <summary>Matrice rows×cols contigua, identità sui primi min(rows, cols)
    /// (stesso canone di MATRIX.CREATE).</summary>
    public static MatrixValue Create(int rows, int cols)
    {
        if (rows < 0 || cols < 0) throw new ArgumentOutOfRangeException(nameof(rows));
        var storage = new double[(long)rows * cols];
        var m = new MatrixValue(storage, rows, cols, cols, 1, 0);
        var d = Math.Min(rows, cols);
        for (var i = 0; i < d; i++) m.Set(i, i, 1d);
        return m;
    }

    /// <summary>Copia contigua da una matrice rettangolare managed.</summary>
    public static MatrixValue FromRect(double[,] source)
    {
        ArgumentNullException.ThrowIfNull(source);
        int rows = source.GetLength(0), cols = source.GetLength(1);
        var storage = new double[(long)rows * cols];
        var m = new MatrixValue(storage, rows, cols, cols, 1, 0);
        for (var r = 0; r < rows; r++)
            for (var c = 0; c < cols; c++)
                m.Set(r, c, source[r, c]);
        return m;
    }

    /// <summary>Copia contigua in una matrice rettangolare managed (ponte legacy).</summary>
    public double[,] ToRect()
    {
        var result = new double[Rows, Cols];
        for (var r = 0; r < Rows; r++)
            for (var c = 0; c < Cols; c++)
                result[r, c] = Get(r, c);
        return result;
    }

    /// <summary>Elemento (r,c); 0 se fuori range (canone gmb_matrix_get).</summary>
    public double Get(int r, int c)
    {
        if (r < 0 || r >= Rows || c < 0 || c >= Cols) return 0d;
        return _storage[_offset + r * RowStride + c * ColStride];
    }

    /// <summary>Scrive (r,c); no-op se fuori range (canone gmb_matrix_set).</summary>
    public void Set(int r, int c, double value)
    {
        if (r < 0 || r >= Rows || c < 0 || c >= Cols) return;
        _storage[_offset + r * RowStride + c * ColStride] = value;
    }

    /// <summary>Contiguità del DESCRITTORE (non dello storage): stesse regole del runtime C.</summary>
    public bool IsContiguous
    {
        get
        {
            if (Rows <= 1 || Cols <= 1) return true;
            return ColStride == 1 && RowStride == Cols;
        }
    }

    /// <summary>View O(1): sotto-matrice righe [r0, r0+rows) e colonne [c0, c0+cols).</summary>
    public MatrixValue View(int r0, int c0, int rows, int cols)
    {
        if (rows < 0 || cols < 0 || r0 < 0 || c0 < 0
            || r0 + rows > Rows || c0 + cols > Cols)
            return Empty;
        return new MatrixValue(_storage, rows, cols, RowStride, ColStride,
            _offset + r0 * RowStride + c0 * ColStride);
    }

    /// <summary>View trasposta O(1).</summary>
    public MatrixValue TransposeView() =>
        new(_storage, Cols, Rows, ColStride, RowStride, _offset);

    /// <summary>Riga come view 1×cols.</summary>
    public MatrixValue RowView(int row) => View(row, 0, 1, Cols);

    /// <summary>Colonna come view rows×1.</summary>
    public MatrixValue ColView(int col) => View(0, col, Rows, 1);

    /// <summary>Copia INDIPENDENTE contigua (rompe l'aliasing con le view).</summary>
    public MatrixValue Clone()
    {
        var storage = new double[(long)Rows * Cols];
        var m = new MatrixValue(storage, Rows, Cols, Cols, 1, 0);
        for (var r = 0; r < Rows; r++)
            for (var c = 0; c < Cols; c++)
                m.Set(r, c, Get(r, c));
        return m;
    }
}
