// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Runtime ML (TORCH): delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> TorchBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.TORCH_DEVICE,
        BuiltInFunction.TORCH_LIBPATH,
        BuiltInFunction.TORCH_LOADED,
        BuiltInFunction.TORCH_CREATE,
        BuiltInFunction.TORCH_FROM,
        BuiltInFunction.TORCH_ADD,
        BuiltInFunction.TORCH_SUB,
        BuiltInFunction.TORCH_MUL,
        BuiltInFunction.TORCH_DIV,
        BuiltInFunction.TORCH_MATMUL,
        BuiltInFunction.TORCH_RELU,
        BuiltInFunction.TORCH_GET,
        BuiltInFunction.TORCH_SET,
        BuiltInFunction.TORCH_TOARRAY,
        BuiltInFunction.TORCH_SHAPE,
        BuiltInFunction.TORCH_FREE,
        BuiltInFunction.TORCH_ALLCLOSE,
        BuiltInFunction.TORCH_SUM,
        BuiltInFunction.TORCH_MEAN,
        BuiltInFunction.TORCH_ZEROS,
        BuiltInFunction.TORCH_ONES,
        BuiltInFunction.TORCH_EYE,
        BuiltInFunction.TORCH_RANDN,
        BuiltInFunction.TORCH_TRANSPOSE,
        BuiltInFunction.TORCH_RESHAPE,
        BuiltInFunction.TORCH_SIGMOID,
        BuiltInFunction.TORCH_TANH,
        BuiltInFunction.TORCH_SOFTMAX,
        BuiltInFunction.TORCH_EXP,
        BuiltInFunction.TORCH_LOG,
        BuiltInFunction.TORCH_SQRT,
        BuiltInFunction.TORCH_POW,
        BuiltInFunction.TORCH_ABS,
        BuiltInFunction.TORCH_MAX,
        BuiltInFunction.TORCH_MIN,
        BuiltInFunction.TORCH_GELU,
        BuiltInFunction.TORCH_PERMUTE,
        BuiltInFunction.TORCH_SLICE,
        BuiltInFunction.TORCH_CONCAT,
        BuiltInFunction.TORCH_STACK,
        BuiltInFunction.TORCH_SPLIT,
        BuiltInFunction.TORCH_TOPK,
        BuiltInFunction.TORCH_ARGSORT,
        BuiltInFunction.TORCH_ARGMAX,
        BuiltInFunction.TORCH_ONEHOT,
        BuiltInFunction.TORCH_CLIP,
        BuiltInFunction.TORCH_NORMALIZE,
        BuiltInFunction.TORCH_STANDARDIZE,
        BuiltInFunction.TORCH_SIMILARITY,
        BuiltInFunction.TORCH_BLEU,
        BuiltInFunction.TORCH_ROUGE,
        BuiltInFunction.TORCH_DPOSTEP,
        BuiltInFunction.TORCH_TEXTENCODE,
        BuiltInFunction.TORCH_TEXTDECODE,
        BuiltInFunction.TORCH_DATASETLOAD,
        BuiltInFunction.TORCH_MODELEXPORT,
        BuiltInFunction.TORCH_IMATRIXLOAD,
        BuiltInFunction.TORCH_IMATRIXRANKS,
        BuiltInFunction.TORCH_DTYPE,
        BuiltInFunction.TORCH_AVAILABLE,
        BuiltInFunction.TORCH_DEVICEOF,
        BuiltInFunction.TORCH_MODELSAVE,
        BuiltInFunction.TORCH_MODELRESET,
        BuiltInFunction.TORCH_LOSSMSE,
        BuiltInFunction.TORCH_LOSSCROSSENTROPY,
        BuiltInFunction.TORCH_LOSSBINARY,
        BuiltInFunction.TORCH_ACCURACY,
        BuiltInFunction.TORCH_MODELINPUT,
        BuiltInFunction.TORCH_MODELOUTPUT,
        BuiltInFunction.TORCH_MODELCREATE,
        BuiltInFunction.TORCH_MODELTRAINSTEP,
        BuiltInFunction.TORCH_MODELLOSS,
        BuiltInFunction.TORCH_MODELFORWARD,
        BuiltInFunction.TORCH_MODELPARAMETERS,
        BuiltInFunction.TORCH_MODELSTATE,
        BuiltInFunction.TORCH_MODELFREE,
    };

    /// <summary>Torch: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _torchDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> TORCHDispatch
        => _torchDispatch ??= BuildTorchDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildTorchDispatch() => new()
    {
        [BuiltInFunction.TORCH_DEVICE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchDevice();
        },
        [BuiltInFunction.TORCH_LIBPATH] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchLibPath();
        },
        [BuiltInFunction.TORCH_LOADED] = (task, args) =>
        {
                                // LOADED() legge lo stato corrente SENZA forzare il load: altrimenti
                                // annullerebbe un esplicito TORCH UNLOAD LIB (ri-caricando la lib).
                                return Basic.IlBackend.IlRuntime.TorchLoaded() ? -1.0 : 0.0;
        },
        [BuiltInFunction.TORCH_CREATE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchCreate((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_FROM] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchFrom(ToDoubleArr(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_ADD] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchAdd((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_SUB] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSub((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_MUL] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchMul((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_DIV] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchDiv((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_MATMUL] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchMatmul((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_RELU] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchRelu((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_GET] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchGet((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_SET] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchSet((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
        },
        [BuiltInFunction.TORCH_TOARRAY] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchToArray((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_SHAPE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchShape((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_FREE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchFree((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_ALLCLOSE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchAllclose((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2))) ? -1.0 : 0.0;
        },
        [BuiltInFunction.TORCH_SUM] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchSum((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MEAN] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchMean((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_ZEROS] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchZeros((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_ONES] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchOnes((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_EYE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchEye((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_RANDN] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchRandn((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_TRANSPOSE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchTranspose((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_RESHAPE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchReshape((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_SIGMOID] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSigmoid((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_TANH] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchTanh((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_SOFTMAX] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSoftmax((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_EXP] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchExp((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_LOG] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchLog((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_SQRT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSqrt((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_POW] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchPow((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_ABS] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchAbs((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MAX] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchMax((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MIN] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchMin((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_GELU] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchGelu((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_PERMUTE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchPermute((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_SLICE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSlice((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
        },
        [BuiltInFunction.TORCH_CONCAT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchConcat((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_STACK] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchStack((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_SPLIT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchSplit((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_TOPK] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchTopK((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_ARGMAX] = (task, args) =>
        {
            Basic.IlBackend.IlRuntime.TorchEnsureLoaded();
            return Basic.IlBackend.IlRuntime.TorchArgmax((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_ARGSORT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchArgsort((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_ONEHOT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchOneHot(SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_CLIP] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchClip((long)SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)), SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_NORMALIZE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchNormalize((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_STANDARDIZE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchStandardize((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_SIMILARITY] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchSimilarity((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)), (ArgOr(args, 2)?.ToString())!);
        },
        [BuiltInFunction.TORCH_BLEU] = (task, args) =>
        {
                                return (double)Basic.IlBackend.IlRuntime.TorchBleu((ArgOr(args, 0)?.ToString())!, (ArgOr(args, 1)?.ToString())!);
        },
        [BuiltInFunction.TORCH_ROUGE] = (task, args) =>
        {
                                return (double)Basic.IlBackend.IlRuntime.TorchRouge((ArgOr(args, 0)?.ToString())!, (ArgOr(args, 1)?.ToString())!);
        },
        [BuiltInFunction.TORCH_DPOSTEP] = (task, args) =>
        {
                                /* L5: opts$ opzionale; assente = default (beta 0.1, stessi scheduler). */
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelDpoStep((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)),
                                    (long)SafeDouble(ArgOr(args, 2)), (long)SafeDouble(ArgOr(args, 3)), (long)SafeDouble(ArgOr(args, 4)), (ArgOr(args, 5)?.ToString())!);
        },
        [BuiltInFunction.TORCH_TEXTENCODE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchTextEncode((ArgOr(args, 0)?.ToString())!);
        },
        [BuiltInFunction.TORCH_TEXTDECODE] = (task, args) =>
        {
                                return Basic.IlBackend.IlRuntime.TorchTextDecode((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_DATASETLOAD] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchDatasetLoad((ArgOr(args, 0)?.ToString())!, ArgOr(args, 1)?.ToString());
        },
        [BuiltInFunction.TORCH_MODELEXPORT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelExport((long)SafeDouble(ArgOr(args, 0)), (ArgOr(args, 1)?.ToString())!, ArgOr(args, 2)?.ToString());
        },
        [BuiltInFunction.TORCH_IMATRIXLOAD] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchImatrixLoad((ArgOr(args, 0)?.ToString())!);
        },
        [BuiltInFunction.TORCH_IMATRIXRANKS] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchImatrixRanks((ArgOr(args, 0)?.ToString())!, (long)SafeDouble(ArgOr(args, 1)), (long)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_DTYPE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchDtype((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_AVAILABLE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchAvailable();
        },
        [BuiltInFunction.TORCH_DEVICEOF] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchDeviceOf((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MODELSAVE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelSave((long)SafeDouble(ArgOr(args, 0)), ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.TORCH_MODELRESET] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelReset((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_LOSSMSE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchLossMse((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_LOSSCROSSENTROPY] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchLossCrossEntropy((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_LOSSBINARY] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchLossBinary((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_ACCURACY] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchAccuracy((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_MODELINPUT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelInput((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MODELOUTPUT] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelOutput((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MODELCREATE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelCreate(ArgOr(args, 0)?.ToString() ?? "linear", ArgOr(args, 1)?.ToString() ?? "{}");
        },
        [BuiltInFunction.TORCH_MODELTRAINSTEP] = (task, args) =>
        {
                                /* L2: 7o parametro opts$ opzionale (JSON scheduler/warmup/accum/clip/resume);
                                   assente = "{}" (const, come prima). Sempre via _ex: il vecchio C delega. */
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelTrainStepEx((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)),
                                    (long)SafeDouble(ArgOr(args, 2)), (ArgOr(args, 3)?.ToString())!, (ArgOr(args, 4)?.ToString())!, SafeDouble(ArgOr(args, 5)), (ArgOr(args, 6)?.ToString())!);
        },
        [BuiltInFunction.TORCH_MODELLOSS] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchModelLoss((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)), (long)SafeDouble(ArgOr(args, 2)));
        },
        [BuiltInFunction.TORCH_MODELFORWARD] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelForward((long)SafeDouble(ArgOr(args, 0)), (long)SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.TORCH_MODELPARAMETERS] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelParameters((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MODELSTATE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return Basic.IlBackend.IlRuntime.TorchModelState((long)SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.TORCH_MODELFREE] = (task, args) =>
        {
                                Basic.IlBackend.IlRuntime.TorchEnsureLoaded();return (double)Basic.IlBackend.IlRuntime.TorchModelFree((long)SafeDouble(ArgOr(args, 0)));
        },
    };

    private object? ExecuteBuiltInTorch(PCodeTask task, BuiltInFunction fn, object?[] args)
        => TORCHDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInTorch (fn={fn}).");

}
