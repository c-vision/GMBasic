// FILE: Interpreter.StdLib.Torch.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Torch — namespace 'TORCH'
 * 76 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (76):
 *   ABS
 *   ACCURACY
 *   ADD
 *   ALLCLOSE
 *   ARGMAX
 *   ARGSORT
 *   AVAILABLE
 *   BLEU
 *   CLIP
 *   CONCAT
 *   CREATE
 *   DATASETLOAD
 *   DEVICE
 *   DEVICEOF
 *   DIV
 *   DTYPE
 *   EXP
 *   EYE
 *   FREE
 *   FROM
 *   GELU
 *   GET
 *   IMATRIXLOAD
 *   IMATRIXRANKS
 *   LIBPATH
 *   LOADED
 *   LOG
 *   LOSSBINARY
 *   LOSSCROSSENTROPY
 *   LOSSMSE
 *   MATMUL
 *   MAX
 *   MEAN
 *   MIN
 *   MODELCREATE
 *   MODELDPOSTEP
 *   MODELEXPORT
 *   MODELFORWARD
 *   MODELFREE
 *   MODELINPUT
 *   MODELLOSS
 *   MODELOUTPUT
 *   MODELPARAMETERS
 *   MODELRESET
 *   MODELSAVE
 *   MODELSTATE
 *   MODELTRAINSTEP
 *   MUL
 *   NORMALIZE
 *   ONEHOT
 *   ONES
 *   PERMUTE
 *   POW
 *   RANDN
 *   RELU
 *   RESHAPE
 *   ROUGE
 *   SET
 *   SHAPE
 *   SIGMOID
 *   SIMILARITY
 *   SLICE
 *   SOFTMAX
 *   SPLIT
 *   SQRT
 *   STACK
 *   STANDARDIZE
 *   SUB
 *   SUM
 *   TANH
 *   TEXTDECODE
 *   TEXTENCODE
 *   TOARRAY
 *   TOPK
 *   TRANSPOSE
 *   ZEROS
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TORCH NAMESPACE (runtime ML nativo — PyTorch/libtorch via gmb_torch) ---
    // Device/loader (1100-1102), tensor (1103+). TRAIN riservato a 1200+.
    // ========================================================================

    private void CreateTorchNamespace()
    {
        const string namespaceName = "TORCH";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        var doubleArr = new ArrayType(new DoubleType());
        var longType = new LongType();
        var doubleType = new DoubleType();
        var stringType = new StringType();
        var boolType = new BitType();

        var rowsParam = new Parameter(-1, T(TokenKind.Identifier, "rows"), new IntegerType(), noFlags, null, false);
        var colsParam = new Parameter(-1, T(TokenKind.Identifier, "cols"), new IntegerType(), noFlags, null, false);
        var dataParam = new Parameter(-1, T(TokenKind.Identifier, "data"), doubleArr, arrayFlag, null, false);
        var aParam = new Parameter(-1, T(TokenKind.Identifier, "a"), longType, noFlags, null, false);
        var bParam = new Parameter(-1, T(TokenKind.Identifier, "b"), longType, noFlags, null, false);
        var hParam = new Parameter(-1, T(TokenKind.Identifier, "h"), longType, noFlags, null, false);
        var iParam = new Parameter(-1, T(TokenKind.Identifier, "i"), new IntegerType(), noFlags, null, false);
        var jParam = new Parameter(-1, T(TokenKind.Identifier, "j"), new IntegerType(), noFlags, null, false);
        var vParam = new Parameter(-1, T(TokenKind.Identifier, "v"), doubleType, noFlags, null, false);
        var tolParam = new Parameter(-1, T(TokenKind.Identifier, "tol"), doubleType, noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            // device / loader
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDevice, "DEVICE"),
                [], stringType, TokenKind.TorchDevice),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLibPath, "LIBPATH"),
                [], stringType, TokenKind.TorchLibPath),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLoaded, "LOADED"),
                [], boolType, TokenKind.TorchLoaded),
            // tensor — costruttori
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchCreate, "CREATE"),
                [rowsParam, colsParam], longType, TokenKind.TorchCreate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchFrom, "FROM"),
                [dataParam], longType, TokenKind.TorchFrom),
            // tensor — aritmetica
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchAdd, "ADD"),
                [aParam, bParam], longType, TokenKind.TorchAdd),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSub, "SUB"),
                [aParam, bParam], longType, TokenKind.TorchSub),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchMul, "MUL"),
                [aParam, bParam], longType, TokenKind.TorchMul),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDiv, "DIV"),
                [aParam, bParam], longType, TokenKind.TorchDiv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchMatmul, "MATMUL"),
                [aParam, bParam], longType, TokenKind.TorchMatmul),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchRelu, "RELU"),
                [aParam], longType, TokenKind.TorchRelu),
            // tensor — accesso
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchGet, "GET"),
                [hParam, iParam, jParam], doubleType, TokenKind.TorchGet),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSet, "SET"),
                [hParam, iParam, jParam, vParam], doubleType, TokenKind.TorchSet),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchToArray, "TOARRAY"),
                [hParam], doubleArr, TokenKind.TorchToArray),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchShape, "SHAPE"),
                [hParam], new ArrayType(longType), TokenKind.TorchShape),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchFree, "FREE"),
                [hParam], longType, TokenKind.TorchFree),
            // tensor — riduzioni / confronto
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchAllclose, "ALLCLOSE"),
                [aParam, bParam, tolParam], boolType, TokenKind.TorchAllclose),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSum, "SUM"),
                [hParam], doubleType, TokenKind.TorchSum),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchMean, "MEAN"),
                [hParam], doubleType, TokenKind.TorchMean),
            // tensor — costruttori extra
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchZeros, "ZEROS"),
                [rowsParam, colsParam], longType, TokenKind.TorchZeros),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchOnes, "ONES"),
                [rowsParam, colsParam], longType, TokenKind.TorchOnes),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchEye, "EYE"),
                [rowsParam], longType, TokenKind.TorchEye),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchRandn, "RANDN"),
                [rowsParam, colsParam], longType, TokenKind.TorchRandn),
            // tensor — attivazioni / operazioni puntuali
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSigmoid, "SIGMOID"),
                [aParam], longType, TokenKind.TorchSigmoid),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchTanh, "TANH"),
                [aParam], longType, TokenKind.TorchTanh),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSoftmax, "SOFTMAX"),
                [aParam], longType, TokenKind.TorchSoftmax),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchExp, "EXP"),
                [aParam], longType, TokenKind.TorchExp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLog, "LOG"),
                [aParam], longType, TokenKind.TorchLog),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSqrt, "SQRT"),
                [aParam], longType, TokenKind.TorchSqrt),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchAbs, "ABS"),
                [aParam], longType, TokenKind.TorchAbs),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchPow, "POW"),
                [aParam, tolParam], longType, TokenKind.TorchPow),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchTranspose, "TRANSPOSE"),
                [aParam], longType, TokenKind.TorchTranspose),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchReshape, "RESHAPE"),
                [aParam, rowsParam, colsParam], longType, TokenKind.TorchReshape),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchMax, "MAX"),
                [hParam], doubleType, TokenKind.TorchMax),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchMin, "MIN"),
                [hParam], doubleType, TokenKind.TorchMin),
            // model / training (TRAIN)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelCreate, "MODELCREATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "arch"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "cfg"), stringType, noFlags, null, false)], longType, TokenKind.TorchModelCreate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelTrainStep, "MODELTRAINSTEP"),
                [hParam, aParam, bParam,
                 new Parameter(-1, T(TokenKind.Identifier, "opt"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "loss"), stringType, noFlags, null, false),
                 tolParam,
                 new Parameter(-1, T(TokenKind.Identifier, "opts"), stringType, SymbolFlags.IsOptional, null, true)], longType, TokenKind.TorchModelTrainStep),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelLoss, "MODELLOSS"),
                [hParam, aParam, bParam], doubleType, TokenKind.TorchModelLoss),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelForward, "MODELFORWARD"),
                [hParam, aParam], longType, TokenKind.TorchModelForward),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelParameters, "MODELPARAMETERS"),
                [hParam], longType, TokenKind.TorchModelParameters),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelState, "MODELSTATE"),
                [hParam], stringType, TokenKind.TorchModelState),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelFree, "MODELFREE"),
                [hParam], longType, TokenKind.TorchModelFree),
            // tensor — extra (gelu / shape ops)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchGelu, "GELU"),
                [aParam], longType, TokenKind.TorchGelu),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchPermute, "PERMUTE"),
                [aParam, iParam, jParam], longType, TokenKind.TorchPermute),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSlice, "SLICE"),
                [aParam, iParam, jParam, new Parameter(-1, T(TokenKind.Identifier, "end"), new IntegerType(), noFlags, null, false)], longType, TokenKind.TorchSlice),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchConcat, "CONCAT"),
                [aParam, bParam], longType, TokenKind.TorchConcat),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchStack, "STACK"),
                [aParam, bParam], longType, TokenKind.TorchStack),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSplit, "SPLIT"),
                [aParam, iParam, new Parameter(-1, T(TokenKind.Identifier, "n"), new IntegerType(), noFlags, null, false)], longType, TokenKind.TorchSplit),
            // bonus (piano ML §3.6, da libtorch): array-returning → Double[]; SIMILARITY → Double
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchTopK, "TOPK"),
                [aParam, iParam], doubleArr, TokenKind.TorchTopK),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchArgmax, "ARGMAX"),
                [hParam], longType, TokenKind.TorchArgmax),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchArgsort, "ARGSORT"),
                [aParam], doubleArr, TokenKind.TorchArgsort),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchOneHot, "ONEHOT"),
                [iParam, new Parameter(-1, T(TokenKind.Identifier, "n"), new IntegerType(), noFlags, null, false)], doubleArr, TokenKind.TorchOneHot),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchClip, "CLIP"),
                [aParam, tolParam, new Parameter(-1, T(TokenKind.Identifier, "hi"), doubleType, noFlags, null, false)], doubleArr, TokenKind.TorchClip),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchNormalize, "NORMALIZE"),
                [aParam], doubleArr, TokenKind.TorchNormalize),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchStandardize, "STANDARDIZE"),
                [aParam], doubleArr, TokenKind.TorchStandardize),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchSimilarity, "SIMILARITY"),
                [aParam, bParam, new Parameter(-1, T(TokenKind.Identifier, "metric"), stringType, noFlags, null, false)], doubleType, TokenKind.TorchSimilarity),
            // L7 Training: metriche di eval testuale (BLEU-4 + ROUGE-L, variante GMBasic in gmb_rag_eval.c)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchBleu, "BLEU"),
                [new Parameter(-1, T(TokenKind.Identifier, "ref"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "hyp"), stringType, noFlags, null, false)], doubleType, TokenKind.TorchBleu),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchRouge, "ROUGE"),
                [new Parameter(-1, T(TokenKind.Identifier, "ref"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "hyp"), stringType, noFlags, null, false)], doubleType, TokenKind.TorchRouge),
            // L5 Training: un passo preference DPO/ORPO (mr<=0 = ORPO senza ref).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDpoStep, "MODELDPOSTEP"),
                [hParam,
                 new Parameter(-1, T(TokenKind.Identifier, "ref"), longType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), longType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "chosen"), longType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "rejected"), longType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "opts"), stringType, SymbolFlags.IsOptional, null, true)], doubleType, TokenKind.TorchDpoStep),
            // L6 Training: pipeline dati testuale toy (char-level).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchTextEncode, "TEXTENCODE"),
                [new Parameter(-1, T(TokenKind.Identifier, "text"), stringType, noFlags, null, false)], longType, TokenKind.TorchTextEncode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchTextDecode, "TEXTDECODE"),
                [hParam], stringType, TokenKind.TorchTextDecode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDatasetLoad, "DATASETLOAD"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "opts"), stringType, SymbolFlags.IsOptional, null, true)], longType, TokenKind.TorchDatasetLoad),
            // L9: export esplicito (merge+save flat; formato validato).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelExport, "MODELEXPORT"),
                [hParam, new Parameter(-1, T(TokenKind.Identifier, "path"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "format"), stringType, SymbolFlags.IsOptional, null, true)], longType, TokenKind.TorchModelExport),
            // L11: lettore imatrix (sensibilita' per-layer) + mappa rank.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchImatrixLoad, "IMATRIXLOAD"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), stringType, noFlags, null, false)], longType, TokenKind.TorchImatrixLoad),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchImatrixRanks, "IMATRIXRANKS"),
                [new Parameter(-1, T(TokenKind.Identifier, "path"), stringType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "min_rank"), longType, noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "max_rank"), longType, noFlags, null, false)], longType, TokenKind.TorchImatrixRanks),
            // info / model extras
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDtype, "DTYPE"),
                [hParam], stringType, TokenKind.TorchDtype),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchAvailable, "AVAILABLE"),
                [], stringType, TokenKind.TorchAvailable),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchDeviceOf, "DEVICEOF"),
                [hParam], stringType, TokenKind.TorchDeviceOf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelSave, "MODELSAVE"),
                [hParam, new Parameter(-1, T(TokenKind.Identifier, "path"), stringType, noFlags, null, false)], longType, TokenKind.TorchModelSave),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelReset, "MODELRESET"),
                [hParam], longType, TokenKind.TorchModelReset),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLossMse, "LOSSMSE"),
                [aParam, bParam], doubleType, TokenKind.TorchLossMse),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLossCrossEntropy, "LOSSCROSSENTROPY"),
                [aParam, bParam], doubleType, TokenKind.TorchLossCrossEntropy),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchLossBinary, "LOSSBINARY"),
                [aParam, bParam], doubleType, TokenKind.TorchLossBinary),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchAccuracy, "ACCURACY"),
                [aParam, bParam], doubleType, TokenKind.TorchAccuracy),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelInput, "MODELINPUT"),
                [hParam], longType, TokenKind.TorchModelInput),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TorchModelOutput, "MODELOUTPUT"),
                [hParam], longType, TokenKind.TorchModelOutput),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
