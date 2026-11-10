// FILE: Interpreter.StdLib.Matrix.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Matrix — namespace 'MATRIX'
 * 75 funzioni | 10 comandi | 5 tipi | 0 costanti
 *
 * FUNZIONI (75):
 *   ABOVEMASK
 *   ABOVEMATRIX
 *   ADDCOLVECTOR
 *   ADDROWVECTOR
 *   ARGMAX
 *   ARGMIN
 *   ASCSV
 *   ASJSON
 *   BLOCKDIAG
 *   CHOLESKY
 *   CIRCULANT
 *   CLONEMATRIX
 *   COL
 *   COLMEANS
 *   COLSUMS
 *   COLUMNCOUNT
 *   COLVARIANCES
 *   CONCATCOLS
 *   CONCATROWS
 *   COND
 *   COSM
 *   CREATE
 *   DETERMINANT
 *   EIG
 *   EVD
 *   EXPM
 *   FACTOR
 *   FRACTIONALPOWER
 *   FROBENIUSNORM
 *   GETVALUE
 *   HANKEL
 *   HESSENBERG
 *   INFINITYNORM
 *   INVERSE
 *   ISCONTIGUOUS
 *   ISLOWERTRIANGULAR
 *   ISORTHOGONAL
 *   ISSYMMETRIC
 *   ISUPPERTRIANGULAR
 *   KRONECKER
 *   LOADCSV
 *   LOADJSON
 *   LOGM
 *   LU
 *   MAXVALUE
 *   MINVALUE
 *   MULTIPLY
 *   MULTIPLYLEFTVECTOR
 *   MULTIPLYVECTOR
 *   NORM
 *   NUCLEARNORM
 *   NULLSPACE
 *   ONENORM
 *   ORTH
 *   PINVERSE
 *   POLAR
 *   QR
 *   RANK
 *   RESHAPE
 *   ROW
 *   ROWCOUNT
 *   ROWMEANS
 *   ROWSUMS
 *   ROWVARIANCES
 *   SCHUR
 *   SETVALUE
 *   SIGNM
 *   SINM
 *   SLICE
 *   SOLVE
 *   SOLVETRIANGULAR
 *   SQRTM
 *   SUBSPACEANGLES
 *   SVD
 *   TANM
 *   TOEPLITZ
 *   TRACE
 *   TRANSPOSE
 * COMANDI (10):
 *   IDENTITY
 *   MULTIPLY
 *   POP
 *   PROJECTION
 *   PUSH
 *   ROTATE
 *   SCALE
 *   TRANSLATE
 *   VIEW
 *   WORLD
 * TIPI (5):
 *   CholeskyDecomposition
 *   EvdDecomposition
 *   LuDecomposition
 *   QrDecomposition
 *   SvdDecomposition
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- MATRIX NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'MATRIX'.
    /// Contiene comandi e funzioni per le matrici, con registrazioni che usano delegati semplici.
    /// </summary>
    private void CreateMatrixNamespace()
    {
        const string namespaceName = "MATRIX";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        var matrixArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"),
            new LongType(), noFlags, null, false);

        // --- Comandi (Sintassi Legacy) ---
        RegisterCommand(TokenKind.Matrix, "MATRIX", namespaceName, Parser.DispatchMatrixStatement);

        // --- Funzioni (Sintassi Moderna Qualificata: es. MATRIX.CREATE) ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LinAlgMatrixCreateFunc, "CREATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rows"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "cols"), new LongType(), noFlags, null, false)
                ],
                new MatrixValueType(), TokenKind.LinAlgMatrixCreateFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Multiply, "MULTIPLY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new AnyType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new AnyType(), noFlags, null, false)
                ],
                new MatrixValueType(), TokenKind.LinAlgMultiplyFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Transpose, "TRANSPOSE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.LinAlgTransposeFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixKron, "KRONECKER"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle1"), new AnyType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "handle2"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixKron),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixTrace, "TRACE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.MatrixTrace),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixBlockDiag, "BLOCKDIAG"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle1"), new AnyType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "handle2"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixBlockDiag),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSetValue, "SETVALUE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "row"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "col"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixSetValue),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixGetValue, "GETVALUE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "row"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "col"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.MatrixGetValue),

            // --- scipy.linalg completato: funzioni matriciali, SVD, costruttori, eig/schur ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixToeplitz, "TOEPLITZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "c"), new ArrayType(new DoubleType()), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "r"), new ArrayType(new DoubleType()), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixToeplitz),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixHankel, "HANKEL"),
                [new Parameter(-1, T(TokenKind.Identifier, "c"), new ArrayType(new DoubleType()), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "r"), new ArrayType(new DoubleType()), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixHankel),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixCirculant, "CIRCULANT"),
                [new Parameter(-1, T(TokenKind.Identifier, "c"), new ArrayType(new DoubleType()), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixCirculant),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRowSums, "ROWSUMS"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixRowSums, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixColSums, "COLSUMS"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixColSums, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRowMeans, "ROWMEANS"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixRowMeans, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixColMeans, "COLMEANS"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixColMeans, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRowVariances, "ROWVARIANCES"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixRowVariances, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixColVariances, "COLVARIANCES"), [matrixArrayParam], new ArrayType(new DoubleType()), TokenKind.MatrixColVariances, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixIsSymmetric, "ISSYMMETRIC"), [matrixArrayParam], new BitType(), TokenKind.MatrixIsSymmetric, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixIsOrthogonal, "ISORTHOGONAL"), [matrixArrayParam], new BitType(), TokenKind.MatrixIsOrthogonal, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixIsUpperTriangular, "ISUPPERTRIANGULAR"), [matrixArrayParam], new BitType(), TokenKind.MatrixIsUpperTriangular, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixIsLowerTriangular, "ISLOWERTRIANGULAR"), [matrixArrayParam], new BitType(), TokenKind.MatrixIsLowerTriangular, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixArgMax, "ARGMAX"), [matrixArrayParam], new ArrayType(new LongType()), TokenKind.MatrixArgMax, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixArgMin, "ARGMIN"), [matrixArrayParam], new ArrayType(new LongType()), TokenKind.MatrixArgMin, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSlice, "SLICE"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "rowStart"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "rowEnd"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "colStart"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "colEnd"), new LongType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixSlice, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixReshape, "RESHAPE"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "rows"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "cols"), new LongType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixReshape, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixConcatRows, "CONCATROWS"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "other"), new AnyType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixConcatRows, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixConcatCols, "CONCATCOLS"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "other"), new AnyType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixConcatCols, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAddRowVector, "ADDROWVECTOR"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new DoubleType()), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixAddRowVector, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAddColVector, "ADDCOLVECTOR"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new DoubleType()), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixAddColVector, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixMaxValue, "MAXVALUE"), [matrixArrayParam], new DoubleType(), TokenKind.MatrixMaxValue, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixMinValue, "MINVALUE"), [matrixArrayParam], new DoubleType(), TokenKind.MatrixMinValue, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAboveMask, "ABOVEMASK"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "threshold"), new DoubleType(), noFlags, null, false)], new ArrayType(new BitType()), TokenKind.MatrixAboveMask, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAboveMatrix, "ABOVEMATRIX"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "threshold"), new DoubleType(), noFlags, null, false)], new ArrayType(new BitType()), TokenKind.MatrixAboveMatrix, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAsCsv, "ASCSV"), [matrixArrayParam], new StringType(), TokenKind.MatrixAsCsv, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixAsJson, "ASJSON"), [matrixArrayParam], new StringType(), TokenKind.MatrixAsJson, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRowCount, "ROWCOUNT"), [matrixArrayParam], new LongType(), TokenKind.MatrixRowCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixColumnCount, "COLUMNCOUNT"), [matrixArrayParam], new LongType(), TokenKind.MatrixColumnCount, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixClone, "CLONEMATRIX"), [matrixArrayParam], new MatrixValueType(), TokenKind.MatrixClone, ExecutionKind: NativeExecutionKind.Pure),
            // MATRIX F2b (2026-11-09): ROW/COL come matrice rank-2 (1×cols / rows×1) e
            // predicato di contiguità del descrittore.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRow, "ROW"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "row"), new LongType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixRow, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixCol, "COL"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "col"), new LongType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixCol, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixIsContiguous, "ISCONTIGUOUS"), [matrixArrayParam], new BitType(), TokenKind.MatrixIsContiguous, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixMultiplyVector, "MULTIPLYVECTOR"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new DoubleType()), noFlags, null, false)], new ArrayType(new DoubleType()), TokenKind.MatrixMultiplyVector, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixMultiplyLeftVector, "MULTIPLYLEFTVECTOR"), [matrixArrayParam, new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new DoubleType()), noFlags, null, false)], new ArrayType(new DoubleType()), TokenKind.MatrixMultiplyLeftVector, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixLoadCsv, "LOADCSV"), [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixLoadCsv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixLoadJson, "LOADJSON"), [new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)], new MatrixValueType(), TokenKind.MatrixLoadJson)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
