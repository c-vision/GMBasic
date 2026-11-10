// FILE: Basic.IlBackend.PCode.PCodeProcessor.Linalg.cs
// Algebra lineare (LINALG) per la P-CODE VM — implementazione nativa.
// Sostituisce i vecchi stub (`ExecuteMatrixOp` restituiva identita' 4x4, `ExecuteVectorOp`
// restituiva gli args). Implementa le FUNZIONI (CREATE/TRANSPOSE/INVERSE/MULTIPLY/
// DETERMINANT/SOLVE/LU/QR/SVD/EVD/CHOLESKY/GETFACTOR/DOTPRODUCT/NORM) e i COMANDI
// algebrici MATRIX/VECTOR (IDENTITY/TRANSLATE/ROTATE/SCALE/MULTIPLY + ADD/SUB/MUL/DIV/
// SET/FILLRANDOM). Le entita' (matrici, vettori, decomposizioni) sono opache: a runtime un
// handle e' un `double` che punta ai registry di questa VM. Le operazioni sono manipolazione
// algebrica di matrice/vettore (NON grafica): niente SDL2/sun, niente ISAM/DBF/SPEED/LEDGER.

using System.Collections.Concurrent;
using Basic.Runtime.Gpm;
using Compiler;
using MathNet.Numerics.LinearAlgebra.Factorization;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    // ==== Registry opachi (handle -> entita' algebriche MathNet) ====
    // ConcurrentDictionary + Interlocked sugli id: i builtin CPU parcheggiati (Welch,
    // Splrep, Cdist, DistanceMatrix, Rfft) scrivono dal thread del blocking pool mentre i
    // worker eseguono altri task — nessun accesso ai registry puo' correre in parallelo.
    private readonly ConcurrentDictionary<long, global::MathNet.Numerics.LinearAlgebra.Matrix<double>> _linMatrices = new();
    private readonly ConcurrentDictionary<long, global::MathNet.Numerics.LinearAlgebra.Vector<double>> _linVectors = new();
    private readonly ConcurrentDictionary<long, object> _linDecompositions = new();
    private readonly ConcurrentDictionary<long, Stack<global::MathNet.Numerics.LinearAlgebra.Matrix<double>>> _linMatrixStack = new();
    private long _nextLinearMatrixId;
    private long _nextLinearVectorId;
    private long _nextLinearDecompositionId;

    private static double HandleOf(long id) => id;

    /// <summary>Parcheggia le operazioni LINALG pesanti (parità piano §20.2): se la matrice
    /// supera la soglia di costo stimato, il calcolo viene eseguito sul pool CPU
    /// (ParkCpuBuiltin) e il task parcheggiato; altrimenti resta sincrono. `_linMatrices` è
    /// ConcurrentDictionary (scrittura thread-safe) e `_nextLinearMatrixId` atomico.</summary>
    private object? RunLinOp(PCodeTask task, long id,
        Func<MathNet.Numerics.LinearAlgebra.Matrix<double>, MathNet.Numerics.LinearAlgebra.Matrix<double>> op)
    {
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        if (m.RowCount * m.ColumnCount >= 1024) // soglia: matrice >= 32x32 (O(n³) pesante)
            return ParkCpuBuiltin(task, () =>
            {
                _linMatrices[newId] = op(m);
                return HandleOf(newId);
            });
        _linMatrices[newId] = op(m);
        return HandleOf(newId);
    }

    /// <summary>F4: come RunLinOp ma restituisce il VALORE matrice (double[,]) invece
    /// dell'handle, mantenendo il park per le matrici grandi.</summary>
    private object? RunLinOpRect(PCodeTask task, long id,
        Func<MathNet.Numerics.LinearAlgebra.Matrix<double>, MathNet.Numerics.LinearAlgebra.Matrix<double>> op)
    {
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        double[,] Compute() => op(m).ToArray();
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () => (object?)Compute());
        return Compute();
    }

    private long ToLongHandle(object?[] args, int index)
    {
        if (index >= args.Length || args[index] == null)
            return 0;
        // Ponte handle↔valore (ADR-MATRIX-Valore §5): un valore MATRIX OF (double[,]) viene
        // registrato come handle per le funzioni storiche MATRIX.*.
        if (args[index] is double[,] matrix) return StoreLinMatHandle(matrix);
        if (args[index] is double d) return (long)d;
        if (args[index] is int i) return i;
        return 0;
    }

    /// <summary>Registra un valore matrice e ritorna l'id handle (long) — usato dal ponte
    /// handle↔valore; StoreLinMat ritorna l'oggetto handle, qui serve l'id nudo.</summary>
    private long StoreLinMatHandle(double[,] data)
    {
        var m = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.Create(
            data.GetLength(0), data.GetLength(1), (r, c) => data[r, c]);
        var id = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[id] = m;
        return id;
    }

    private static double ToDouble(object?[] args, int index)
    {
        if (index >= args.Length || args[index] == null)
            return 0.0;
        return Convert.ToDouble(args[index]);
    }

    // ========================================================================
    // FUNZIONI BUILT-IN
    // ========================================================================
    private object? ExecuteLinAlgBuiltIn(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            case BuiltInFunction.LINALGMATRIXCREATEFUNC: return LuMatrixCreate(args);
            case BuiltInFunction.LINALGVECTORCREATEFUNC: return LuVectorCreate(args);
            case BuiltInFunction.LINALGTRANSPOSEFUNC: return LuMatrixTranspose(args);
            case BuiltInFunction.LINALGINVERSEFUNC: return LuMatrixInverse(task, args);
            case BuiltInFunction.MATRIXPSEUDOINVERSE: return LuMatrixPseudoInverse(args);
            case BuiltInFunction.MATRIX_EXPM: return LuMatrixExp(task, args);
            case BuiltInFunction.MATRIX_KRONECKER: return LuMatrixKronecker(args);
            case BuiltInFunction.MATRIX_SQRTM: return LuMatrixSqrt(task, args);
            case BuiltInFunction.MATRIX_LOGM: return LuMatrixLogm(task, args);
            case BuiltInFunction.MATRIX_HESSENBERG: return LuMatrixHessenberg(task, args);
            case BuiltInFunction.MATRIX_BLOCKDIAG: return LuMatrixBlockDiag(args);
            case BuiltInFunction.MATRIX_NULLSPACE: return LuMatrixNullSpace(task, args);
            case BuiltInFunction.MATRIX_SETVALUE: return LuMatrixSetValue(args);
            case BuiltInFunction.MATRIX_GETVALUE: return LuMatrixGetValue(args);
            case BuiltInFunction.MATRIX_COSM: return LuMatrixCosm(task, args);
            case BuiltInFunction.MATRIX_SINM: return LuMatrixSinm(task, args);
            case BuiltInFunction.MATRIX_TANM: return LuMatrixTanm(task, args);
            case BuiltInFunction.MATRIX_SIGNM: return LuMatrixSignm(task, args);
            case BuiltInFunction.MATRIX_FRACTIONALPOWER: return LuMatrixFractionalPower(task, args);
            case BuiltInFunction.MATRIX_ORTH: return LuMatrixOrth(task, args);
            case BuiltInFunction.MATRIX_SUBSPACEANGLES: return LuMatrixSubspaceAngles(args);
            case BuiltInFunction.MATRIX_POLAR: return LuMatrixPolar(task, args);
            case BuiltInFunction.MATRIX_COND: return LuMatrixCond(task, args);
            case BuiltInFunction.MATRIX_NUCLEARNORM: return LuMatrixNuclearNorm(task, args);
            case BuiltInFunction.MATRIX_TOEPLITZ: return LuMatrixToeplitz(args);
            case BuiltInFunction.MATRIX_HANKEL: return LuMatrixHankel(args);
            case BuiltInFunction.MATRIX_CIRCULANT: return LuMatrixCirculant(args);
            case BuiltInFunction.MATRIX_SOLVETRIANGULAR: return LuMatrixSolveTriangular(args);
            case BuiltInFunction.MATRIX_EIG: return LuMatrixEig(task, args);
            case BuiltInFunction.MATRIX_SCHUR: return LuMatrixSchur(task, args);
            case BuiltInFunction.MATRIXRANK: return LuMatrixRank(args);
            case BuiltInFunction.MATRIXTRACE: return LuMatrixTrace(args);
            case BuiltInFunction.MATRIXNORM: return LuMatrixNorm(args);
            case BuiltInFunction.LINALGMULTIPLYFUNC:
            {
                // F4: MATRIX.MULTIPLY (token LinAlgMultiplyFunc) restituisce il VALORE.
                var r = LuMultiply(args);
                if (r is double[,] rect) return rect;
                if (r is long h && _linMatrices.TryGetValue(h, out var mat))
                    return SimdToRect(new CommonLib.SimdMatrix(mat.ToRowMajorArray(), mat.RowCount, mat.ColumnCount));
                return r;
            }
            case BuiltInFunction.DOTPRODUCT: return LuVectorDotProduct(args);
            case BuiltInFunction.NORM: return LuVectorNorm(args);
            case BuiltInFunction.VECTORMANHATTAN: return LuVectorManhattan(args);
            case BuiltInFunction.VECTORCHEBYSHEV: return LuVectorChebyshev(args);
            case BuiltInFunction.VECTORCOSINEDISTANCE: return LuVectorCosineDistance(args);
            case BuiltInFunction.VECTORMINKOWSKI: return LuVectorMinkowski(args);
            case BuiltInFunction.DETERMINANT: return LuMatrixDeterminant(task, args);
            case BuiltInFunction.SOLVE: return LuSolve(args);
            case BuiltInFunction.LU: return LuDecomp(task, args, "lu");
            case BuiltInFunction.LINALGLUFUNC: return LuDecomp(task, args, "lu");
            case BuiltInFunction.QR: return LuDecomp(task, args, "qr");
            case BuiltInFunction.LINALGQRFUNC: return LuDecomp(task, args, "qr");
            case BuiltInFunction.SVD: return LuDecomp(task, args, "svd");
            case BuiltInFunction.LINALGSVDFUNC: return LuDecomp(task, args, "svd");
            case BuiltInFunction.EVD: return LuDecomp(task, args, "evd");
            case BuiltInFunction.LINALGEVDFUNC: return LuDecomp(task, args, "evd");
            case BuiltInFunction.CHOLESKY: return LuDecomp(task, args, "cholesky");
            case BuiltInFunction.LINALGCHOLESKYFUNC: return LuDecomp(task, args, "cholesky");
            case BuiltInFunction.LINALGDECOMPGETFACTORFUNC: return LuDecompGetFactor(args);
            case BuiltInFunction.MATRIXROWSUMS: return LuSimd(args).RowSums();
            case BuiltInFunction.MATRIXCOLSUMS: return LuSimd(args).ColSums();
            case BuiltInFunction.MATRIXROWMEANS: return LuSimd(args).RowMeans();
            case BuiltInFunction.MATRIXCOLMEANS: return LuSimd(args).ColMeans();
            case BuiltInFunction.MATRIXROWVARIANCES: return LuSimd(args).RowVariances();
            case BuiltInFunction.MATRIXCOLVARIANCES: return LuSimd(args).ColVariances();
            case BuiltInFunction.MATRIXFROBENIUSNORM: return LuSimd(args).FrobeniusNorm();
            case BuiltInFunction.MATRIXONENORM: return LuSimd(args).OneNorm();
            case BuiltInFunction.MATRIXINFINITYNORM: return LuSimd(args).InfinityNorm();
            case BuiltInFunction.MATRIXISSYMMETRIC: return LuSimd(args).IsSymmetric();
            case BuiltInFunction.MATRIXISORTHOGONAL: return LuSimd(args).IsOrthogonal();
            case BuiltInFunction.MATRIXISUPPERTRIANGULAR: return LuSimd(args).IsUpperTriangular();
            case BuiltInFunction.MATRIXISLOWERTRIANGULAR: return LuSimd(args).IsLowerTriangular();
            case BuiltInFunction.MATRIXARGMAX:
            { var (row, column) = LuSimd(args).ArgMax(); return new long[] { row, column }; }
            case BuiltInFunction.MATRIXARGMIN:
            { var (row, column) = LuSimd(args).ArgMin(); return new long[] { row, column }; }
            case BuiltInFunction.MATRIXSLICE:
                return SimdToRect(LuSimd(args).Slice(checked((int)ToDouble(args, 1)), checked((int)ToDouble(args, 2)), checked((int)ToDouble(args, 3)), checked((int)ToDouble(args, 4))));
            case BuiltInFunction.MATRIXRESHAPE:
                return SimdToRect(LuSimd(args).Reshape(checked((int)ToDouble(args, 1)), checked((int)ToDouble(args, 2))));
            // F4: CONCATROWS/CONCATCOLS restituiscono il VALORE matrice.
            case BuiltInFunction.MATRIXCONCATROWS:
                return SimdToRect(CommonLib.SimdMatrix.ConcatRows(LuSimdAt(args, 0), LuSimdAt(args, 1)));
            case BuiltInFunction.MATRIXCONCATCOLS:
                return SimdToRect(CommonLib.SimdMatrix.ConcatCols(LuSimdAt(args, 0), LuSimdAt(args, 1)));
            case BuiltInFunction.MATRIXADDROWVECTOR:
                return SimdToRect(LuSimd(args).AddRowVector(ToDoubleArr(args[1])));
            case BuiltInFunction.MATRIXADDCOLVECTOR:
                return SimdToRect(LuSimd(args).AddColVector(ToDoubleArr(args[1])));
            case BuiltInFunction.MATRIXMAXVALUE: return LuSimd(args).Max();
            case BuiltInFunction.MATRIXMINVALUE: return LuSimd(args).Min();
            case BuiltInFunction.MATRIXABOVEMASK: return LuSimd(args).Mask(ToDouble(args, 1));
            case BuiltInFunction.MATRIXABOVEMATRIX:
            {
                var mask = LuSimd(args).GreaterThan(ToDouble(args, 1));
                var flat = new bool[mask.GetLength(0) * mask.GetLength(1)];
                for (var row = 0; row < mask.GetLength(0); row++)
                    for (var column = 0; column < mask.GetLength(1); column++)
                        flat[row * mask.GetLength(1) + column] = mask[row, column];
                return flat;
            }
            case BuiltInFunction.MATRIXASCSV: return LuSimd(args).ToCsv();
            case BuiltInFunction.MATRIXASJSON: return LuSimd(args).ToJson();
            case BuiltInFunction.MATRIXROWCOUNT: return (long)LuSimd(args).Rows;
            case BuiltInFunction.MATRIXCOLUMNCOUNT: return (long)LuSimd(args).Cols;
            case BuiltInFunction.MATRIXCLONE: return SimdToRect(LuSimd(args).Clone());
            // MATRIX F2b (2026-11-09): ROW/COL/ISCONTIGUOUS.
            case BuiltInFunction.MATRIXROW: return SimdRowRect(LuSimd(args), checked((int)ToDouble(args, 1)));
            case BuiltInFunction.MATRIXCOL: return SimdColRect(LuSimd(args), checked((int)ToDouble(args, 1)));
            case BuiltInFunction.MATRIXISCONTIGUOUS: _ = LuSimd(args); return true;
            case BuiltInFunction.MATRIXMULTIPLYVECTOR: return LuSimd(args).Multiply(ToDoubleArr(args[1]));
            case BuiltInFunction.MATRIXMULTIPLYLEFTVECTOR: return LuSimd(args).MultiplyFromLeft(ToDoubleArr(args[1]));
            case BuiltInFunction.MATRIXMULTIPLY:
            {
                // F4: MATRIX.MULTIPLY restituisce il VALORE matrice (rank-2).
                var r = LuMultiply(args);
                if (r is double[,] rect) return rect;
                if (r is long h && _linMatrices.TryGetValue(h, out var mat))
                    return SimdToRect(new CommonLib.SimdMatrix(mat.ToRowMajorArray(), mat.RowCount, mat.ColumnCount));
                return r;
            }
            case BuiltInFunction.MATRIXLOADCSV: return SimdToRect(CommonLib.SimdMatrix.LoadCsv(ResolveMatrixPath(Convert.ToString(args[0]) ?? "")));
            case BuiltInFunction.MATRIXLOADJSON: return SimdToRect(CommonLib.SimdMatrix.LoadJson(ResolveMatrixPath(Convert.ToString(args[0]) ?? "")));
            default: return null;
        }
    }

    private static string ResolveMatrixPath(string path)
    {
        if (Path.IsPathRooted(path) || File.Exists(path)) return path;
        for (var directory = new DirectoryInfo(Directory.GetCurrentDirectory()); directory != null; directory = directory.Parent)
        {
            var candidate = Path.Combine(directory.FullName, path);
            if (File.Exists(candidate)) return candidate;
        }
        return path;
    }

    private CommonLib.SimdMatrix LuSimdAt(object?[] args, int index)
    {
        var id = ToLongHandle(args, index);
        if (!_linMatrices.TryGetValue(id, out var matrix))
            throw new ArgumentException($"Handle matrice non valido: {id}.");
        return new CommonLib.SimdMatrix(matrix.ToRowMajorArray(), matrix.RowCount, matrix.ColumnCount);
    }

    private double StoreSimd(CommonLib.SimdMatrix matrix)
    {
        var id = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[id] = global::MathNet.Numerics.LinearAlgebra.Matrix<double>.Build.DenseOfRowMajor(matrix.Rows, matrix.Cols, matrix.ToArray());
        return HandleOf(id);
    }

    private CommonLib.SimdMatrix LuSimd(object?[] args)
    {
        // Le funzioni che restituiscono un array ricevono dalla VM anche lo slot di
        // destinazione prima degli argomenti BASIC. Individuare l'handle registrato
        // rende il percorso uniforme per risultati scalari e array.
        foreach (var argument in args)
        {
            var id = argument switch
            {
                double[,] data => StoreLinMatHandle(data),
                object[,] data => StoreLinMatHandle(ObjectRectToDouble(data)),
                double value => (long)value,
                long value => value,
                int value => value,
                _ => 0
            };

            if (_linMatrices.TryGetValue(id, out var matrix))
                return new CommonLib.SimdMatrix(matrix.ToRowMajorArray(), matrix.RowCount, matrix.ColumnCount);
        }

        throw new ArgumentException($"Handle matrice non valido (args={string.Join(",", args.Select(value => $"{value}:{value?.GetType().Name}"))}).");
    }

    private object? LuMatrixCreate(object?[] args)
    {
        var rows = Convert.ToInt32(args.Length > 0 ? args[0] : 4);
        var cols = Convert.ToInt32(args.Length > 1 ? args[1] : 4);
        // MATRIX.CREATE a VALORE (F1): rank-2 contigua, identità sui primi min(rows,cols).
        // Il tipo del linguaggio è MATRIX OF (INT,INT,DOUBLE): niente handle nel registry.
        var arr = new double[rows, cols];
        for (var i = 0; i < Math.Min(rows, cols); i++) arr[i, i] = 1.0;
        return arr;
    }

    private global::MathNet.Numerics.LinearAlgebra.Vector<double>? VecById(object?[] args, int i)
    {
        var id = ToLongHandle(args, i);
        return _linVectors.TryGetValue(id, out var v) ? v : null;
    }

    private object? VecGetP(object?[] args)
    {
        var v = VecById(args, 0);
        var idx = (int)SafeDouble(args.Length > 1 ? args[1] : null);
        if (v is null || idx < 0 || idx >= v.Count) return null;
        return v[idx];
    }

    private object? VecOpP(object?[] args, int op)
    {
        var a = VecById(args, 0); var b = VecById(args, 1);
        if (a is null || b is null) return null;
        var r = new double[4];
        for (var i = 0; i < 4; i++)
        {
            var x = a[i]; var y = b[i];
            r[i] = op switch
            {
                0 => x + y, 1 => x - y, 2 => x * y, 3 => y != 0.0 ? x / y : 0.0,
                8 => x < y ? x : y, 9 => x > y ? x : y, _ => x,
            };
        }
        if (op == 7)
        {
            r[0] = a[1] * b[2] - a[2] * b[1];
            r[1] = a[2] * b[0] - a[0] * b[2];
            r[2] = a[0] * b[1] - a[1] * b[0];
            r[3] = 0.0;
        }
        var newId = Interlocked.Increment(ref _nextLinearVectorId);
        _linVectors[newId] = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(r);
        return HandleOf(newId);
    }

    private object? VecScaleP(object?[] args)
    {
        var a = VecById(args, 0);
        if (a is null) return null;
        var s = SafeDouble(args.Length > 1 ? args[1] : null);
        var r = new double[4];
        for (var i = 0; i < 4; i++) r[i] = a[i] * s;
        var newId = Interlocked.Increment(ref _nextLinearVectorId);
        _linVectors[newId] = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(r);
        return HandleOf(newId);
    }

    private object? VecNormP(object?[] args)
    {
        var a = VecById(args, 0);
        if (a is null) return null;
        var n = a.L2Norm();
        var r = new double[4];
        for (var i = 0; i < 4; i++) r[i] = n > 0.0 ? a[i] / n : a[i];
        var newId = Interlocked.Increment(ref _nextLinearVectorId);
        _linVectors[newId] = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(r);
        return HandleOf(newId);
    }

    private object? VecLerpP(object?[] args)
    {
        var a = VecById(args, 0); var b = VecById(args, 1);
        if (a is null || b is null) return null;
        var t = SafeDouble(args.Length > 2 ? args[2] : null);
        var r = new double[4];
        for (var i = 0; i < 4; i++) r[i] = a[i] + (b[i] - a[i]) * t;
        var newId = Interlocked.Increment(ref _nextLinearVectorId);
        _linVectors[newId] = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(r);
        return HandleOf(newId);
    }

    private object? VecAngleP(object?[] args)
    {
        var a = VecById(args, 0); var b = VecById(args, 1);
        if (a is null || b is null) return null;
        var na = a.L2Norm(); var nb = b.L2Norm();
        if (na <= 0.0 || nb <= 0.0) return null;
        var c = Math.Abs(a.DotProduct(b)) / (na * nb);
        if (c > 1.0) c = 1.0;
        return Math.Acos(c);
    }

    private object? VecDistP(object?[] args)
    {
        var a = VecById(args, 0); var b = VecById(args, 1);
        if (a is null || b is null) return null;
        double s = 0;
        for (var i = 0; i < 4; i++) { var d = a[i] - b[i]; s += d * d; }
        return Math.Sqrt(s);
    }

    private object? LuVectorCreate(object?[] args)
    {
        var x = ToDouble(args, 0);
        var y = ToDouble(args, 1);
        var z = args.Length > 2 ? ToDouble(args, 2) : 0.0;
        var w = args.Length > 3 ? ToDouble(args, 3) : 0.0;
        var newId = Interlocked.Increment(ref _nextLinearVectorId);
        _linVectors[newId] = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense([x, y, z, w]);
        return HandleOf(newId);
    }

    private object? LuMatrixTranspose(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return null;
        return m.Transpose().ToArray();
    }

    /// <summary>Converte un SimdMatrix nel valore rank-2 (double[,]) del linguaggio.</summary>
    private static double[,] SimdToRect(CommonLib.SimdMatrix m)
    {
        var result = new double[m.Rows, m.Cols];
        for (var row = 0; row < m.Rows; row++)
            for (var column = 0; column < m.Cols; column++)
                result[row, column] = m[row, column];
        return result;
    }

    /// <summary>Converte un object[,] del linguaggio (elementi boxati) in double[,].</summary>
    private static double[,] ObjectRectToDouble(object[,] data)
    {
        int rows = data.GetLength(0), cols = data.GetLength(1);
        var result = new double[rows, cols];
        for (var row = 0; row < rows; row++)
            for (var column = 0; column < cols; column++)
                result[row, column] = Convert.ToDouble(data[row, column] ?? 0d);
        return result;
    }

    /// <summary>MATRIX.ROW: riga come matrice rank-2 1×cols (fuori range → zeri).</summary>
    private static double[,] SimdRowRect(CommonLib.SimdMatrix m, int row)
    {
        var result = new double[1, m.Cols];
        if (row < 0 || row >= m.Rows) return result;
        for (var column = 0; column < m.Cols; column++) result[0, column] = m[row, column];
        return result;
    }

    /// <summary>MATRIX.COL: colonna come matrice rank-2 rows×1 (fuori range → zeri).</summary>
    private static double[,] SimdColRect(CommonLib.SimdMatrix m, int col)
    {
        var result = new double[m.Rows, 1];
        if (col < 0 || col >= m.Cols) return result;
        for (var row = 0; row < m.Rows; row++) result[row, 0] = m[row, col];
        return result;
    }

    private object? LuMatrixInverse(PCodeTask task, object?[] args) => RunLinOpRect(task, ToLongHandle(args, 0), m => m.Inverse());

    private object? LuMatrixPseudoInverse(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return null;
        
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        var svd = m.Svd();
        var tolerance = Math.Max(m.RowCount, m.ColumnCount) * svd.S[0] * 1e-15;

        // Costruisci S⁺
        var invertedS = global::MathNet.Numerics.LinearAlgebra.Matrix<double>.Build.Dense(m.ColumnCount, m.RowCount);
        for (var i = 0; i < svd.S.Count; i++)
        {
            if (svd.S[i] > tolerance)
                invertedS[i, i] = 1.0 / svd.S[i];
        }

        // A⁺ = V * S⁺ * Uᵀ
        var result = svd.VT.Transpose() * invertedS * svd.U.Transpose();
        _linMatrices[newId] = result;
        return HandleOf(newId);
    }

    private object? LuMatrixRank(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return 0;
        
        var svd = m.Svd();
        var tolerance = Math.Max(m.RowCount, m.ColumnCount) * svd.S[0] * 1e-15;

        int rank = 0;
        for (var i = 0; i < svd.S.Count; i++)
            if (svd.S[i] > tolerance) rank++;
        
        return rank;
    }

    private object? LuMatrixTrace(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return 0.0;
        
        if (m.RowCount != m.ColumnCount)
            return double.NaN;

        double trace = 0;
        for (var i = 0; i < m.RowCount; i++)
            trace += m[i, i];
        
        return trace;
    }

    private object? LuMatrixNorm(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return 0.0;
        
        var svd = m.Svd();
        return svd.S[0];
    }

    private object? LuMultiply(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        if (_linMatrices.TryGetValue(id1, out var m1) && _linMatrices.TryGetValue(id2, out var m2))
        {
            var newId = Interlocked.Increment(ref _nextLinearMatrixId);
            _linMatrices[newId] = m1.Multiply(m2);
            return HandleOf(newId);
        }
        if (_linMatrices.TryGetValue(id1, out m1) && _linVectors.TryGetValue(id2, out var v2))
        {
            var newId = Interlocked.Increment(ref _nextLinearVectorId);
            _linVectors[newId] = m1.Multiply(v2);
            return HandleOf(newId);
        }
        return null;
    }

    private object? LuVectorDotProduct(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        if (!_linVectors.TryGetValue(id1, out var v1) || !_linVectors.TryGetValue(id2, out var v2))
            return 0.0;
        return global::CommonLib.Numerics.VectorDotProduct(v1, v2);
    }

    private object? LuVectorNorm(object?[] args)
    {
        if (args.Length < 1) return 0.0;
        if (args[0] is Array)
        {
            double[] vals = ToDoubleArr(args[0]);
            double sum = 0.0;
            foreach (var d in vals) sum += d * d;
            return Math.Sqrt(sum);
        }
        var id = ToLongHandle(args, 0);
        if (!_linVectors.TryGetValue(id, out var v))
            return 0.0;
        return global::CommonLib.Numerics.VectorNorm(v);
    }

    private object? LuVectorManhattan(object?[] args)
    {
        var id1 = ToLongHandle(args, 0); var id2 = ToLongHandle(args, 1);
        if (!_linVectors.TryGetValue(id1, out var v1) || !_linVectors.TryGetValue(id2, out var v2))
            return 0.0;
        return global::CommonLib.Numerics.VectorManhattan(v1.ToArray(), v2.ToArray());
    }

    private object? LuVectorChebyshev(object?[] args)
    {
        var id1 = ToLongHandle(args, 0); var id2 = ToLongHandle(args, 1);
        if (!_linVectors.TryGetValue(id1, out var v1) || !_linVectors.TryGetValue(id2, out var v2))
            return 0.0;
        return global::CommonLib.Numerics.VectorChebyshev(v1.ToArray(), v2.ToArray());
    }

    private object? LuVectorCosineDistance(object?[] args)
    {
        var id1 = ToLongHandle(args, 0); var id2 = ToLongHandle(args, 1);
        if (!_linVectors.TryGetValue(id1, out var v1) || !_linVectors.TryGetValue(id2, out var v2))
            return 0.0;
        return global::CommonLib.Numerics.VectorCosineDistance(v1.ToArray(), v2.ToArray());
    }

    private object? LuVectorMinkowski(object?[] args)
    {
        var id1 = ToLongHandle(args, 0); var id2 = ToLongHandle(args, 1);
        var p = args.Length > 2 && args[2] is double pd ? pd : 0.0;
        if (!_linVectors.TryGetValue(id1, out var v1) || !_linVectors.TryGetValue(id2, out var v2))
            return 0.0;
        return global::CommonLib.Numerics.VectorMinkowski(v1.ToArray(), v2.ToArray(), p);
    }

    private object? LuMatrixDeterminant(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return 0.0;
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () => global::CommonLib.Numerics.MatrixDeterminant(m));
        return global::CommonLib.Numerics.MatrixDeterminant(m);
    }

    private object? LuSolve(object?[] args)
    {
        var mId = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(mId, out var a))
            return null;
        global::MathNet.Numerics.LinearAlgebra.Vector<double> b;
        if (args.Length > 1 && args[1] is System.Collections.IEnumerable && args[1] is not string)
            b = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(ToDoubleArr(args[1]));
        else
        {
            var vId = ToLongHandle(args, 1);
            if (!_linVectors.TryGetValue(vId, out b!)) return null;
        }
        return a.Solve(b).ToArray();
    }

    private object? LuDecomp(PCodeTask task, object?[] args, string kind)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return null;
        var newId = Interlocked.Increment(ref _nextLinearDecompositionId);
        if (m.RowCount * m.ColumnCount >= 1024) // soglia piano §20.2: decomposizioni O(n³) sul pool
            return ParkCpuBuiltin(task, () =>
            {
                switch (kind)
                {
                    case "lu": _linDecompositions[newId] = m.LU(); break;
                    case "qr": _linDecompositions[newId] = m.QR(); break;
                    case "svd": _linDecompositions[newId] = m.Svd(); break;
                    case "evd": _linDecompositions[newId] = m.Evd(); break;
                    default: _linDecompositions[newId] = m.Cholesky(); break;
                }
                return HandleOf(newId);
            });
        switch (kind)
        {
            case "lu": _linDecompositions[newId] = m.LU(); break;
            case "qr": _linDecompositions[newId] = m.QR(); break;
            case "svd": _linDecompositions[newId] = m.Svd(); break;
            case "evd": _linDecompositions[newId] = m.Evd(); break;
            default: _linDecompositions[newId] = m.Cholesky(); break;
        }
        return HandleOf(newId);
    }

    private object? LuDecompGetFactor(object?[] args)
    {
        var handle = ToLongHandle(args, 0);
        var factorName = Convert.ToString(args.Length > 1 ? args[1] : null) ?? "";
        if (!_linDecompositions.TryGetValue(handle, out var decomp))
            return null;
        global::MathNet.Numerics.LinearAlgebra.Matrix<double>? matrixFactor = null;
        global::MathNet.Numerics.LinearAlgebra.Vector<double>? vectorFactor = null;
        switch (decomp)
        {
            case LU<double> lu:
                matrixFactor = factorName.ToUpperInvariant() switch
                {
                    "L" => lu.L,
                    "U" => lu.U,
                    _ => null
                };
                break;
            case QR<double> qr:
                matrixFactor = factorName.ToUpperInvariant() switch
                {
                    "Q" => qr.Q,
                    "R" => qr.R,
                    _ => null
                };
                break;
            case Svd<double> svd:
                switch (factorName.ToUpperInvariant())
                {
                    case "S": matrixFactor = svd.W; break;
                    case "SVECTOR": vectorFactor = svd.S; break;
                    case "U": matrixFactor = svd.U; break;
                    case "VT": matrixFactor = svd.VT; break;
                }
                break;
            case Evd<double> evd:
                switch (factorName.ToUpperInvariant())
                {
                    case "D": matrixFactor = evd.D; break;
                    case "V": matrixFactor = evd.EigenVectors; break;
                    case "DREAL": vectorFactor = ExtractEigenvalueVector(evd, real: true); break;
                    case "DIMAG": vectorFactor = ExtractEigenvalueVector(evd, real: false); break;
                }
                break;
            case Cholesky<double> chol:
                if (factorName.ToUpperInvariant() == "L")
                    matrixFactor = chol.Factor;
                break;
            case global::CommonLib.Numerics.GeneralEigenDecomposition eig:
                switch (factorName.ToUpperInvariant())
                {
                    case "T": matrixFactor = eig.RealSchurForm; break;
                    case "Z": matrixFactor = eig.SchurVectors; break;
                    case "VRREAL": matrixFactor = eig.EigenvectorsReal; break;
                    case "VRIMAG": matrixFactor = eig.EigenvectorsImag; break;
                    case "DREAL": matrixFactor = AsLinColumn(eig.EigenvaluesReal); break;
                    case "DIMAG": matrixFactor = AsLinColumn(eig.EigenvaluesImag); break;
                }
                break;
            case global::CommonLib.Numerics.PolarDecomposition polar:
                if (factorName.ToUpperInvariant() == "U") matrixFactor = polar.U;
                else if (factorName.ToUpperInvariant() == "P") matrixFactor = polar.P;
                break;
        }

        if (matrixFactor != null)
        {
            var newId = Interlocked.Increment(ref _nextLinearMatrixId);
            _linMatrices[newId] = matrixFactor;
            return HandleOf(newId);
        }
        if (vectorFactor != null)
        {
            var newId = Interlocked.Increment(ref _nextLinearVectorId);
            _linVectors[newId] = vectorFactor;
            return HandleOf(newId);
        }
        return null;
    }

    private static global::MathNet.Numerics.LinearAlgebra.Vector<double> ExtractEigenvalueVector(
        Evd<double> evd, bool real)
    {
        var values = evd.EigenValues;
        var result = global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(values.Count);
        for (var i = 0; i < values.Count; i++)
            result[i] = real ? values[i].Real : values[i].Imaginary;
        return result;
    }

    // ========================================================================
    // COMANDI MATRIX (manipolazione algebrica, NON grafica)
    // ========================================================================
    private object? ExecuteLinAlgMatrixCommand(PCodeTask task, MatrixSubOpCode subCode, object?[] args)
    {
        switch (subCode)
        {
            case MatrixSubOpCode.IDENTITY:
                var id = ToLongHandle(args, 0);
                if (_linMatrices.TryGetValue(id, out var mIdent))
                    _linMatrices[id] = global::MathNet.Numerics.LinearAlgebra.Matrix<double>.Build
                        .DenseIdentity(mIdent.RowCount, mIdent.ColumnCount);
                return null;

            case MatrixSubOpCode.TRANSLATE:
                MatrixTranslate(args);
                return null;
            case MatrixSubOpCode.ROTATE:
                MatrixRotate(args);
                return null;
            case MatrixSubOpCode.SCALE:
                MatrixScale(args);
                return null;
            case MatrixSubOpCode.MULTIPLY:
                MatrixMultiplyInto(args);
                return null;
            case MatrixSubOpCode.PUSH:
                MatrixPush(args);
                return null;
            case MatrixSubOpCode.POP:
                MatrixPop(args);
                return null;
            // CREATE e' una funzione (LINALGMATRIXCREATEFUNC), non un comando.
            case MatrixSubOpCode.WORLD:
            case MatrixSubOpCode.VIEW:
            case MatrixSubOpCode.PROJECTION:
            {
                // Trasformazioni pipeline (matrice mondo/vista/proiezione): notificano il
                // GraphicsManager (cache di rendering) SOLO se lo screen grafico è attivo —
                // altrimenti no-op (come nel vecchio interpreter). Prima erano no-op sempre
                // (wiring perso nel porting).
                if (PCodeGraphics.Instance.IsScreenOn)
                {
                    var mId = ToLongHandle(args, 0);
                    Action<long, Action<object?, Exception?>> gpm = subCode switch
                    {
                        MatrixSubOpCode.WORLD => PCodeGraphics.Instance.UPDATE_WORLD_MATRIX_GPM,
                        MatrixSubOpCode.VIEW => PCodeGraphics.Instance.UPDATE_VIEW_MATRIX_GPM,
                        _ => PCodeGraphics.Instance.UPDATE_PROJECTION_MATRIX_GPM,
                    };
                    ParkGraphics(task, cb => gpm(mId, cb));
                    return ParkedMatrixResult;
                }
                return null;
            }
            default:
                return null;
        }
    }

    private void MatrixTranslate(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var matrix) || matrix.RowCount != 4 || matrix.ColumnCount != 4)
            return;
        var x = ToDouble(args, 1);
        var y = ToDouble(args, 2);
        var z = ToDouble(args, 3);
        var tr = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(new double[,]
        {
            { 1, 0, 0, x },
            { 0, 1, 0, y },
            { 0, 0, 1, z },
            { 0, 0, 0, 1 }
        });
        _linMatrices[id] = matrix.Multiply(tr);
    }

    private void MatrixRotate(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var matrix) || matrix.RowCount != 4 || matrix.ColumnCount != 4)
            return;
        var ax = ToDouble(args, 1) * (Math.PI / 180.0);
        var ay = ToDouble(args, 2) * (Math.PI / 180.0);
        var az = ToDouble(args, 3) * (Math.PI / 180.0);
        var rotX = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(new double[,]
        {
            { 1, 0, 0, 0 },
            { 0, Math.Cos(ax), -Math.Sin(ax), 0 },
            { 0, Math.Sin(ax), Math.Cos(ax), 0 },
            { 0, 0, 0, 1 }
        });
        var rotY = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(new double[,]
        {
            { Math.Cos(ay), 0, Math.Sin(ay), 0 },
            { 0, 1, 0, 0 },
            { -Math.Sin(ay), 0, Math.Cos(ay), 0 },
            { 0, 0, 0, 1 }
        });
        var rotZ = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(new double[,]
        {
            { Math.Cos(az), -Math.Sin(az), 0, 0 },
            { Math.Sin(az), Math.Cos(az), 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        });
        _linMatrices[id] = matrix.Multiply(rotZ * rotY * rotX);
    }

    private void MatrixScale(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var matrix) || matrix.RowCount != 4 || matrix.ColumnCount != 4)
            return;
        var sx = ToDouble(args, 1);
        var sy = ToDouble(args, 2);
        var sz = ToDouble(args, 3);
        var scaleMatrix = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(new double[,]
        {
            { sx, 0, 0, 0 },
            { 0, sy, 0, 0 },
            { 0, 0, sz, 0 },
            { 0, 0, 0, 1 }
        });
        _linMatrices[id] = matrix.Multiply(scaleMatrix);
    }

    private void MatrixMultiplyInto(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        var resultId = ToLongHandle(args, 2);
        if (!_linMatrices.TryGetValue(id1, out var m1) || !_linMatrices.TryGetValue(id2, out var m2) ||
            !_linMatrices.TryGetValue(resultId, out var res))
            return;
        var multiplied = m1.Multiply(m2);
        if (res.RowCount == multiplied.RowCount && res.ColumnCount == multiplied.ColumnCount)
            _linMatrices[resultId] = multiplied;
    }

    private void MatrixPush(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m))
            return;
        if (!_linMatrixStack.TryGetValue(id, out var stack))
        {
            stack = new Stack<global::MathNet.Numerics.LinearAlgebra.Matrix<double>>();
            _linMatrixStack[id] = stack;
        }
        stack.Push(m.Clone());
    }

    private void MatrixPop(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (_linMatrixStack.TryGetValue(id, out var stack) && stack.Count > 0)
            _linMatrices[id] = stack.Pop();
    }

    // ========================================================================
    // COMANDI VECTOR (manipolazione algebrica, NON grafica)
    // ========================================================================
    private object? ExecuteLinAlgVectorCommand(VectorSubOpCode subCode, object?[] args)
    {
        switch (subCode)
        {
            case VectorSubOpCode.ADD:
                VectorBinary(args, (d, s) => d + s);
                return null;
            case VectorSubOpCode.SUB:
                VectorBinary(args, (d, s) => d - s);
                return null;
            case VectorSubOpCode.MUL:
                VectorBinary(args, (d, s) => d * s);
                return null;
            case VectorSubOpCode.DIV:
                VectorBinary(args, (d, s) => d / s);
                return null;
            case VectorSubOpCode.SET:
                VectorSet(args);
                return null;
            case VectorSubOpCode.FILLRANDOM:
                VectorFillRandom(args);
                return null;
            case VectorSubOpCode.WRAP:
                VectorWrap(args);
                return null;
            case VectorSubOpCode.ADDWRAP:
                VectorAddWrap(args);
                return null;
            case VectorSubOpCode.BOUNCE2D:
                VectorBounce2D(args);
                return null;
            default:
                return null;
        }
    }

    /// <summary>WRAP dest, min, max — avvolge (modulo periodico) ogni elemento di dest nel
    /// range [min, max]. Semantica verificata dallo smoke test LINALG (GMBasic):
    /// wrap(x) = min + mod_pos(x - min, max - min), dove mod_pos è sempre non-negativo.
    /// es. wrap(4, 0, 3) = 1; wrap(-1, 0, 3) = 2.</summary>
    private void VectorWrap(object?[] args)
    {
        var dest = args.Length > 0 ? args[0] : null;
        var min = ToDouble(args, 1);
        var max = ToDouble(args, 2);
        if (dest == null || max <= min) return;
        var range = max - min;
        ApplyInPlace(dest, v =>
        {
            var d = v - min;
            var m = d % range;
            if (m < 0) m += range;
            return min + m;
        });
    }

    /// <summary>ADD WRAP dest, source, min, max — dest = wrap(dest + source) nel range.
    /// source può essere un vettore (component-wise) o uno scalare.</summary>
    private void VectorAddWrap(object?[] args)
    {
        var dest = args.Length > 0 ? args[0] : null;
        var src = args.Length > 1 ? args[1] : null;
        var min = ToDouble(args, 2);
        var max = ToDouble(args, 3);
        if (dest == null || max <= min) return;
        var range = max - min;
        var srcArr = ToDoubleArr(src);
        var isVector = srcArr.Length > 0 && !(src is double);
        var srcScalar = isVector ? 0.0 : Convert.ToDouble(src ?? 0.0);
        var idx = 0;
        ApplyInPlace(dest, v =>
        {
            var s = isVector ? (idx < srcArr.Length ? srcArr[idx] : 0.0) : srcScalar;
            idx++;
            var d = (v + s) - min;
            var m = d % range;
            if (m < 0) m += range;
            return min + m;
        });
    }

    /// <summary>BOUNCE2D pos, vel, width, height | posX, posY, velX, velY, minX, minY, maxX, maxY —
    /// rimbalza la posizione nel rettangolo [minX,maxX]×[minY,maxY], invertendo la velocità
    /// sull'asse che esce. Se pos < min → pos = min + (min - pos) e vel = -vel; idem per max.</summary>
    private void VectorBounce2D(object?[] args)
    {
        if (args.Length >= 8)
        {
            // 8 argomenti: posX, posY, velX, velY, minX, minY, maxX, maxY
            var posX = args[0]; var posY = args[1];
            var velX = args[2]; var velY = args[3];
            var minX = ToDouble(args, 4); var minY = ToDouble(args, 5);
            var maxX = ToDouble(args, 6); var maxY = ToDouble(args, 7);
            BounceAxis(posX, velX, minX, maxX);
            BounceAxis(posY, velY, minY, maxY);
            return;
        }
        if (args.Length >= 4)
        {
            // 4 argomenti: pos, vel, width, height — pos/vel sono vettori di COPPIE (x,y):
            // pos[0] rimbalza in [0,width] invertendo vel[0], pos[1] in [0,height] invertendo vel[1].
            var pos = args[0]; var vel = args[1];
            var width = ToDouble(args, 2); var height = ToDouble(args, 3);
            BouncePair(pos, vel, width, height);
        }
    }

    /// <summary>Rimbalzo 2D su vettori coppia (x,y): pos[0]↔[0,width], pos[1]↔[0,height].
    /// Semantica allineata a IlRuntime.VectorBounce2D: NON sposta la posizione dentro il range;
    /// inverte la velocità SOLO se la posizione è fuori E la velocità punta ulteriormente fuori.</summary>
    private void BouncePair(object? pos, object? vel, double width, double height)
    {
        if (pos == null || vel == null) return;
        var p = ToDoubleArr(pos);
        var v = ToDoubleArr(vel);
        var n = Math.Min(p.Length, v.Length);
        for (var i = 0; i < n && i < 2; i++)
        {
            var limit = i == 0 ? width : height;
            if (limit <= 0) continue;
            var pv = p[i];
            var vx = v[i];
            if (pv < 0 && vx < 0) SetElement(vel, i, -vx);
            else if (pv > limit && vx > 0) SetElement(vel, i, -vx);
        }
    }

    /// <summary>Rimbalza un asse: inverte la velocità se la posizione esce dal range [min,max]
    /// E la velocità punta ulteriormente fuori. Non sposta la posizione (allineato a
    /// IlRuntime.VectorBounce2D).</summary>
    private void BounceAxis(object? pos, object? vel, double min, double max)
    {
        if (pos == null || vel == null || max <= min) return;
        var p = ToDoubleArr(pos);
        var v = ToDoubleArr(vel);
        var n = Math.Min(p.Length, v.Length);
        for (var i = 0; i < n; i++)
        {
            var pv = p[i];
            var vx = v[i];
            if (pv < min && vx < 0) SetElement(vel, i, -vx);
            else if (pv > max && vx > 0) SetElement(vel, i, -vx);
        }
    }

    /// <summary>Applica una trasformazione in-place a ogni elemento di un array BASIC
    /// (double[], Object[] boxed o IList) — lo stesso pattern di VectorSet/VectorFillRandom.</summary>
    private void ApplyInPlace(object? dest, Func<double, double> f)
    {
        if (dest is double[] da)
        {
            for (var i = 0; i < da.Length; i++) da[i] = f(da[i]);
            return;
        }
        if (dest is System.Collections.IList list)
        {
            for (var i = 0; i < list.Count; i++) list[i] = f(SafeDouble(list[i]));
        }
    }

    /// <summary>Imposta un elemento in-place di un array BASIC (double[]/Object[]/IList).</summary>
    private void SetElement(object? dest, int index, double value)
    {
        if (dest is double[] da) { da[index] = value; return; }
        if (dest is System.Collections.IList list && index < list.Count) list[index] = value;
    }

    private void VectorSet(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        var value = ToDouble(args, 1);
        if (_linVectors.TryGetValue(id, out var dest))
        {
            for (var i = 0; i < dest.Count; i++)
                dest[i] = value;
            return;
        }
        // Scrittura IN-PLACE (come VectorFillRandom): ToDoubleArr copia → no-op su Object[].
        if (args[0] is double[] da)
        {
            for (var i = 0; i < da.Length; i++)
                da[i] = value;
            return;
        }
        if (args[0] is System.Collections.IList list)
        {
            for (var i = 0; i < list.Count; i++)
                list[i] = value;
        }
    }

    private void VectorFillRandom(object?[] args)
    {
        var destId = ToLongHandle(args, 0);
        var min = ToDouble(args, 1);
        var max = ToDouble(args, 2);
        if (min > max)
            return;
        var range = max - min;
        if (_linVectors.TryGetValue(destId, out var dest))
        {
            for (var i = 0; i < dest.Count; i++)
                dest[i] = _random.NextDouble() * range + min;
            return;
        }
        // Scrittura IN-PLACE sull'array originale: gli array BASIC `DIM X#(n)` arrivano
        // come Object[] (elementi boxed) o double[]. `ToDoubleArr` crea una COPIA → i
        // valori scritti andavano persi (tutti 0 → tutti gli sprite a (0,0)).
        if (args[0] is double[] da)
        {
            for (var i = 0; i < da.Length; i++)
                da[i] = _random.NextDouble() * range + min;
            return;
        }
        if (args[0] is System.Collections.IList list)
        {
            for (var i = 0; i < list.Count; i++)
                list[i] = _random.NextDouble() * range + min;
            return;
        }
    }

    private void VectorBinary(object?[] args, Func<double, double, double> op)
    {
        var destId = ToLongHandle(args, 0);
        if (_linVectors.TryGetValue(destId, out var dest))
        {
            var src = args.Length > 1 ? args[1] : null;
            if (src is double sh && _linVectors.TryGetValue((long)sh, out var sv))
            {
                var n = Math.Min(dest.Count, sv.Count);
                for (var i = 0; i < n; i++)
                    dest[i] = op(dest[i], sv[i]);
                return;
            }
            var scalar = Convert.ToDouble(src ?? 0.0);
            for (var i = 0; i < dest.Count; i++)
                dest[i] = op(dest[i], scalar);
            return;
        }
        // Scrittura IN-PLACE sull'array originale (Object[] boxed o double[]):
        // `ToDoubleArr` copia → le modifiche andavano perse (posizioni ferme a 0).
        var srcV = args.Length > 1 ? args[1] : null;
        if (args[0] is double[] dad)
        {
            var sArr = ToDoubleArr(srcV);
            if (sArr.Length > 0 && !(srcV is double))
            {
                var n = Math.Min(dad.Length, sArr.Length);
                for (var i = 0; i < n; i++)
                    dad[i] = op(dad[i], sArr[i]);
                return;
            }
            var sc2 = Convert.ToDouble(srcV ?? 0.0);
            for (var i = 0; i < dad.Length; i++)
                dad[i] = op(dad[i], sc2);
            return;
        }
        if (args[0] is System.Collections.IList list)
        {
            var sArr = ToDoubleArr(srcV);
            if (sArr.Length > 0 && !(srcV is double))
            {
                var n = Math.Min(list.Count, sArr.Length);
                for (var i = 0; i < n; i++)
                    list[i] = op(SafeDouble(list[i]), sArr[i]);
                return;
            }
            var sc3 = Convert.ToDouble(srcV ?? 0.0);
            for (var i = 0; i < list.Count; i++)
                list[i] = op(SafeDouble(list[i]), sc3);
        }
    }

    private object? LuMatrixExp(PCodeTask task, object?[] args) => RunLinOpRect(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.MatrixExp(m));

    private object? LuMatrixKronecker(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        if (!_linMatrices.TryGetValue(id1, out var m1) || !_linMatrices.TryGetValue(id2, out var m2))
            return null;
        // F4: KRONECKER restituisce il VALORE matrice.
        var res = global::CommonLib.Numerics.MatrixKron(m1, m2);
        return SimdToRect(new CommonLib.SimdMatrix(res.ToRowMajorArray(), res.RowCount, res.ColumnCount));
    }

    // scipy.linalg: sqrtm/logm/hessenberg/block_diag/null_space.
    private object? LuMatrixSqrt(PCodeTask task, object?[] args) => RunLinOpRect(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.MatrixSqrt(m));

    private object? LuMatrixLogm(PCodeTask task, object?[] args) => RunLinOpRect(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.MatrixLogm(m));

    private object? LuMatrixHessenberg(PCodeTask task, object?[] args) => RunLinOp(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.MatrixHessenberg(m));

    private object? LuMatrixBlockDiag(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        if (!_linMatrices.TryGetValue(id1, out var m1) || !_linMatrices.TryGetValue(id2, out var m2))
            return null;
        // F4: BLOCKDIAG restituisce il VALORE matrice.
        var res = global::CommonLib.Numerics.BlockDiag(m1, m2);
        return SimdToRect(new CommonLib.SimdMatrix(res.ToRowMajorArray(), res.RowCount, res.ColumnCount));
    }

    private object? LuMatrixNullSpace(PCodeTask task, object?[] args) => RunLinOp(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.NullSpace(m));

    private object? LuMatrixSetValue(object?[] args)
    {
        // Valore MATRIX OF: mutazione diretta (visibile al chiamante), niente registry.
        if (args.Length > 0 && args[0] is double[,] dm)
        {
            var r = (int)SafeDouble(args.Length > 1 ? args[1] : null);
            var c = (int)SafeDouble(args.Length > 2 ? args[2] : null);
            if (r >= 0 && r < dm.GetLength(0) && c >= 0 && c < dm.GetLength(1))
                dm[r, c] = SafeDouble(args.Length > 3 ? args[3] : null);
            return dm;
        }
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        m[(int)SafeDouble(args[1]), (int)SafeDouble(args[2])] = SafeDouble(args[3]);
        return m.ToArray();
    }

    private object? LuMatrixGetValue(object?[] args)
    {
        if (args.Length > 0 && args[0] is double[,] dm)
        {
            var r = (int)SafeDouble(args.Length > 1 ? args[1] : null);
            var c = (int)SafeDouble(args.Length > 2 ? args[2] : null);
            return r >= 0 && r < dm.GetLength(0) && c >= 0 && c < dm.GetLength(1) ? dm[r, c] : 0.0;
        }
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        return m[(int)SafeDouble(args[1]), (int)SafeDouble(args[2])];
    }

    internal object? StoreLinMat(double[,] data)
    {
        var rows = data.GetLength(0);
        var cols = data.GetLength(1);
        var m = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.Create(rows, cols, (r, c) => data[r, c]);
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[newId] = m;
        return HandleOf(newId);
    }

    internal object? LuStoreMatrix(double[,] data)
    {
        var rows = data.GetLength(0);
        var cols = data.GetLength(1);
        var m = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.Create(rows, cols, (r, c) => data[r, c]);
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[newId] = m;
        return HandleOf(newId);
    }

    internal object? LuStoreSpline(CommonLib.Numerics.CubicSplineRep sp)
    {
        var newId = Interlocked.Increment(ref _nextLinearDecompositionId);
        _linDecompositions[newId] = sp;
        return HandleOf(newId);
    }

    // scipy.integrate.solve_ivp (RK45 adattivo): traiettoria matrice registrata qui.
    internal object? LuSolveIvp(Func<double, double[], double[]> f, double[] y0, double t0, double t1)
    {
        var traj = global::CommonLib.Numerics.SolveIvpk45(f, y0, t0, t1);
        var rows = traj.GetLength(0);
        var cols = traj.GetLength(1);
        var m = global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.Create(rows, cols, (r, c) => traj[r, c]);
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[newId] = m;
        return HandleOf(newId);
    }

    // ---- scipy.linalg completato: funzioni matriciali, SVD-based, costruttori, eig/schur ----
    private object? LuMatrixCosm(PCodeTask task, object?[] args) => ParkStoreLinMat(task, ToLongHandle(args, 0), global::CommonLib.Numerics.MatrixCos);
    private object? LuMatrixSinm(PCodeTask task, object?[] args) => ParkStoreLinMat(task, ToLongHandle(args, 0), global::CommonLib.Numerics.MatrixSin);
    private object? LuMatrixTanm(PCodeTask task, object?[] args) => ParkStoreLinMat(task, ToLongHandle(args, 0), global::CommonLib.Numerics.MatrixTan);
    private object? LuMatrixSignm(PCodeTask task, object?[] args) => ParkStoreLinMat(task, ToLongHandle(args, 0), global::CommonLib.Numerics.MatrixSignm);
    private object? LuMatrixOrth(PCodeTask task, object?[] args) => ParkStoreLinMat(task, ToLongHandle(args, 0), global::CommonLib.Numerics.Orth);

    private object? LuMatrixFractionalPower(PCodeTask task, object?[] args)
    {
        var power = ToDouble(args, 1);
        return RunLinOp(task, ToLongHandle(args, 0), m => global::CommonLib.Numerics.MatrixFractionalPower(m, power));
    }

    private object? LuMatrixSubspaceAngles(object?[] args)
    {
        var id1 = ToLongHandle(args, 0);
        var id2 = ToLongHandle(args, 1);
        if (!_linMatrices.TryGetValue(id1, out var m1) || !_linMatrices.TryGetValue(id2, out var m2)) return null;
        return global::CommonLib.Numerics.SubspaceAngles(m1, m2);
    }

    private object? LuMatrixPolar(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        var newId = Interlocked.Increment(ref _nextLinearDecompositionId);
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () =>
            {
                _linDecompositions[newId] = global::CommonLib.Numerics.Polar(m);
                return HandleOf(newId);
            });
        _linDecompositions[newId] = global::CommonLib.Numerics.Polar(m);
        return HandleOf(newId);
    }

    private object? LuMatrixCond(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () => global::CommonLib.Numerics.ConditionNumber(m));
        return global::CommonLib.Numerics.ConditionNumber(m);
    }

    private object? LuMatrixNuclearNorm(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () => global::CommonLib.Numerics.NuclearNorm(m));
        return global::CommonLib.Numerics.NuclearNorm(m);
    }

    private object? LuMatrixToeplitz(object?[] args)
    {
        // F4: TOEPLITZ restituisce il VALORE matrice.
        var res = global::CommonLib.Numerics.Toeplitz(ToDoubleArr(args.Length > 0 ? args[0] : null), ToDoubleArr(args.Length > 1 ? args[1] : null));
        return SimdToRect(new CommonLib.SimdMatrix(res.ToRowMajorArray(), res.RowCount, res.ColumnCount));
    }

    private object? LuMatrixHankel(object?[] args)
    {
        // F4: HANKEL restituisce il VALORE matrice.
        var res = global::CommonLib.Numerics.Hankel(ToDoubleArr(args.Length > 0 ? args[0] : null), ToDoubleArr(args.Length > 1 ? args[1] : null));
        return SimdToRect(new CommonLib.SimdMatrix(res.ToRowMajorArray(), res.RowCount, res.ColumnCount));
    }

    private object? LuMatrixCirculant(object?[] args)
    {
        // F4: CIRCULANT restituisce il VALORE matrice.
        var res = global::CommonLib.Numerics.Circulant(ToDoubleArr(args.Length > 0 ? args[0] : null));
        return SimdToRect(new CommonLib.SimdMatrix(res.ToRowMajorArray(), res.RowCount, res.ColumnCount));
    }

    private object? LuMatrixSolveTriangular(object?[] args)
    {
        var idA = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(idA, out var mA)) return null;
        double[] bArr;
        if (args.Length > 1 && (args[1] is System.Collections.IList || args[1] is double[]))
            bArr = ToDoubleArr(args[1]);
        else
        {
            var idB = ToLongHandle(args, 1);
            if (!_linVectors.TryGetValue(idB, out var vB)) return null;
            bArr = vB.ToArray();
        }
        return global::CommonLib.Numerics.SolveTriangular(mA, global::MathNet.Numerics.LinearAlgebra.Double.DenseVector.OfArray(bArr), ToDouble(args, 2) != 0).ToArray();
    }

    private object? LuMatrixEig(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        var newId = Interlocked.Increment(ref _nextLinearDecompositionId);
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () =>
            {
                _linDecompositions[newId] = global::CommonLib.Numerics.GeneralEigen(m);
                return HandleOf(newId);
            });
        _linDecompositions[newId] = global::CommonLib.Numerics.GeneralEigen(m);
        return HandleOf(newId);
    }

    private object? LuMatrixSchur(PCodeTask task, object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) return null;
        var newId = Interlocked.Increment(ref _nextLinearDecompositionId);
        if (m.RowCount * m.ColumnCount >= 1024)
            return ParkCpuBuiltin(task, () =>
            {
                _linDecompositions[newId] = global::CommonLib.Numerics.RealSchur(m);
                return HandleOf(newId);
            });
        _linDecompositions[newId] = global::CommonLib.Numerics.RealSchur(m);
        return HandleOf(newId);
    }

    private global::MathNet.Numerics.LinearAlgebra.Matrix<double> UnaryMat(object?[] args)
    {
        var id = ToLongHandle(args, 0);
        if (!_linMatrices.TryGetValue(id, out var m)) throw new ArgumentException("Handle matrice non valido.");
        return m;
    }

    private object? StoreLinMat(global::MathNet.Numerics.LinearAlgebra.Matrix<double> m, Func<global::MathNet.Numerics.LinearAlgebra.Matrix<double>, global::MathNet.Numerics.LinearAlgebra.Matrix<double>> f)
    {
        var newId = Interlocked.Increment(ref _nextLinearMatrixId);
        _linMatrices[newId] = f(m);
        return HandleOf(newId);
    }

    /// <summary>Versione parcheggiata di StoreLinMat (piano §20.2): recupera la matrice
    /// dall'handle e, se grande (>=32x32), calcola sul pool CPU (ParkCpuBuiltin).</summary>
    private object? ParkStoreLinMat(PCodeTask task, long id,
        Func<global::MathNet.Numerics.LinearAlgebra.Matrix<double>, global::MathNet.Numerics.LinearAlgebra.Matrix<double>> f)
        => RunLinOp(task, id, f);

    private static global::MathNet.Numerics.LinearAlgebra.Matrix<double> AsLinColumn(double[] values)
    {
        var col = new double[values.Length, 1];
        for (var i = 0; i < values.Length; i++) col[i, 0] = values[i];
        return global::MathNet.Numerics.LinearAlgebra.Double.DenseMatrix.OfArray(col);
    }
}
