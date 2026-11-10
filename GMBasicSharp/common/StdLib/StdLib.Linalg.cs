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
    // --- LINALG NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'MATRIX'.
    /// Contiene comandi e funzioni per le matrici, con registrazioni che usano delegati semplici.
    /// </summary>
    private void CreateLinalgNamespace()
    {
        const string namespaceName = "LINALG";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        var matrixArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"),
            new LongType(), noFlags, null, false);

        // --- Funzioni (Sintassi Moderna Qualificata: es. MATRIX.CREATE) ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Inverse, "INVERSE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.LinAlgInverseFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Solve, "SOLVE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "matrixHandle"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "vectorHandle"), new LongType(), noFlags, null, false)
                ],
                new ArrayType(new DoubleType()), TokenKind.Solve),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Determinant, "DETERMINANT"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.Determinant),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixPseudoInverse, "PINVERSE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixPseudoInverse),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixExp, "EXPM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixExp),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixRank, "RANK"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.MatrixRank),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixNorm, "NORM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.MatrixNorm),

            // --- scipy.linalg: funzioni matriciali (SQRTM/LOGM/HESSENBERG/BLOCKDIAG/NULLSPACE) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSqrt, "SQRTM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixSqrt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixLogm, "LOGM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixLogm),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixHessenberg, "HESSENBERG"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixHessenberg),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixNullSpace, "NULLSPACE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixNullSpace),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixCosm, "COSM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixCosm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSinm, "SINM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixSinm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixTanm, "TANM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixTanm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSignm, "SIGNM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixSignm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixFractionalPower, "FRACTIONALPOWER"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixFractionalPower),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixOrth, "ORTH"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new AnyType(), noFlags, null, false)],
                new MatrixValueType(), TokenKind.MatrixOrth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSubspaceAngles, "SUBSPACEANGLES"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false)],
                new ArrayType(new DoubleType()), TokenKind.MatrixSubspaceAngles),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixPolar, "POLAR"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.MatrixPolar),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixCond, "COND"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.MatrixCond),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixNuclearNorm, "NUCLEARNORM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.MatrixNuclearNorm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSolveTriangular, "SOLVETRIANGULAR"),
                [new Parameter(-1, T(TokenKind.Identifier, "handleA"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "handleB"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "lower"), new BitType(), noFlags, null, false)],
                new ArrayType(new DoubleType()), TokenKind.MatrixSolveTriangular),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixEig, "EIG"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.MatrixEig),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixSchur, "SCHUR"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.MatrixSchur),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Lu, "LU"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new LuDecompositionType(), TokenKind.LinAlgLuFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Qr, "QR"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new QrDecompositionType(), TokenKind.LinAlgQrFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Svd, "SVD"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new SvdDecompositionType(), TokenKind.LinAlgSvdFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Evd, "EVD"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new EvdDecompositionType(), TokenKind.LinAlgEvdFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cholesky, "CHOLESKY"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)],
                new CholeskyDecompositionType(), TokenKind.LinAlgCholeskyFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Get, "FACTOR"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "decompositionObject"), new InferredType(), noFlags,
                        null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "factorName"), new StringType(), noFlags, null, false)
                ],
                new MatrixValueType(), TokenKind.LinAlgDecompGetFactorFunc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixFrobeniusNorm, "FROBENIUSNORM"), [matrixArrayParam], new DoubleType(), TokenKind.MatrixFrobeniusNorm, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixOneNorm, "ONENORM"), [matrixArrayParam], new DoubleType(), TokenKind.MatrixOneNorm, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MatrixInfinityNorm, "INFINITYNORM"), [matrixArrayParam], new DoubleType(), TokenKind.MatrixInfinityNorm, ExecutionKind: NativeExecutionKind.Pure),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "LuDecomposition", new LuDecompositionType() },
            { "QrDecomposition", new QrDecompositionType() },
            { "SvdDecomposition", new SvdDecompositionType() },
            { "EvdDecomposition", new EvdDecompositionType() },
            { "CholeskyDecomposition", new CholeskyDecompositionType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
