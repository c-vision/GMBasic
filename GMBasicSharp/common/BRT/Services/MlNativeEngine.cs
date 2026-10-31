// FILE: Basic.Runtime.MlNativeEngine
using System;
using System.IO;
using System.Runtime.InteropServices;
using CommonLib;

namespace Basic.Runtime;

/// <summary>
/// P/Invoke verso <c>libgmb_torch_loader</c> (il loader C del runtime ML: scopre la lib
/// host in GmbiPaths.MlDir(rid) = &lt;root&gt;/libs-runtime/ml/&lt;rid&gt;/ (configurabile) e carica i simboli gmb_ml_*).
/// Se la lib host non è disponibile, gmb_ml_* ritorna GMB_ML_NOTLOADED (-2) — mai crash.
/// Stesso modello di <see cref="NativeCommercialDb"/>.
/// </summary>
public static partial class MlNativeEngine
{
    private const string LoaderName = "gmb_torch_loader";

    static MlNativeEngine()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(MlNativeEngine).Assembly, Resolve); }
        catch (InvalidOperationException) { /* resolver già installato (ModuleInitializer) */ }
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly assembly, DllImportSearchPath? search)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    private static string CurrentRid() => RuntimeInformation.ProcessArchitecture switch
    {
        Architecture.Arm64 when OperatingSystem.IsMacOS() => "osx-arm64",
        Architecture.X64 when OperatingSystem.IsMacOS() => "osx-x64",
        Architecture.Arm64 => "linux-arm64",
        _ => OperatingSystem.IsWindows() ? "win-x64" : "linux-x64",
    };

    /// <summary>Directory della lib host ML (la stessa del loader dylib): va passata al loader
    /// C con <see cref="gmb_ml_set_lib_dir"/> così <c>gml_find_lib</c> non dipende dal CWD/root
    /// del processo (nel processo IL/test il CWD è la dir del sample → la lib non veniva trovata).</summary>
    public static string ResolveMlDir() => GmbiPaths.MlDir(CurrentRid());

    // --- loader controls (gmb_torch.h) ---
    /// <summary>Carica la lib host ML: prima indica al loader C la directory (stessa del
    /// loader dylib) così gml_find_lib non dipende dal CWD/root del processo (nel processo
    /// IL/test il CWD è la dir del sample → la lib non veniva trovata).</summary>
    public static int gmb_ml_load_lib()
    {
        try { gmb_ml_set_lib_dir(ResolveMlDir()); } catch { }
        return gmb_ml_load_lib_raw();
    }
    [LibraryImport(LoaderName, EntryPoint = "gmb_ml_load_lib")] private static partial int gmb_ml_load_lib_raw();
    [LibraryImport(LoaderName)] public static partial void gmb_ml_unload_lib();
    [LibraryImport(LoaderName)] public static partial int gmb_ml_loaded();
    [LibraryImport(LoaderName)] public static partial int gmb_ml_set_lib_dir([MarshalAs(UnmanagedType.LPUTF8Str)] string dir);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_lib_path(IntPtr outBuf, int cap);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_active_device();
    [LibraryImport(LoaderName)] public static partial int gmb_ml_set_active_device(int device);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_set_active_device_name([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial IntPtr gmb_ml_last_error();

    // --- tensor (gmb_ml.h) ---
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_create(int rows, int cols, int device);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_from(int rows, int cols, [In] double[] data, int device);
    [LibraryImport(LoaderName)] public static partial void gmb_ml_tensor_free(long h);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_toarray(long h, [Out] double[] outBuf, int max);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_rows(long h);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_cols(long h);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_tensor_get(long h, int r, int c);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_set(long h, int r, int c, double v);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_add(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_sub(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_mul(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_div(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_matmul(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_relu(long a);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_allclose(long a, long b, double tol);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_tensor_sum(long h);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_tensor_mean(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_zeros(int rows, int cols, int device);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_ones(int rows, int cols, int device);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_eye(int n, int device);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_randn(int rows, int cols, int device);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_transpose(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_reshape(long h, int rows, int cols);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_sigmoid(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_tanh(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_softmax(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_exp(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_log(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_sqrt(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_pow(long h, double p);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_abs(long h);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_tensor_max(long h);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_tensor_min(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_gelu(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_permute(long h, int d0, int d1);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_slice(long h, int dim, int start, int end);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_concat(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_stack(long a, long b);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_split(long h, int index, int n);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_topk(long h, int k);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_argsort(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_argmax(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_onehot(int idx, int n);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_clamp(long h, double lo, double hi);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_normalize(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_tensor_standardize(long h);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial double gmb_ml_similarity(long a, long b, string metric);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_dtype(long h, IntPtr outBuf, int cap);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_tensor_device(long h, IntPtr outBuf, int cap);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_available(IntPtr outBuf, int cap);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial int gmb_ml_model_load(long h, string path);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial int gmb_ml_model_save(long h, string path);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_model_reset(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_model_input(long h);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_model_output(long h);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial double gmb_ml_loss(long y, long yhat, string name);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_accuracy(long y, long yhat);
    // --- model / training ---
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)] public static partial long gmb_ml_model_create(string arch, string cfg);
    [LibraryImport(LoaderName)] public static partial void gmb_ml_model_free(long h);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ml_model_train_step(long model, long x, long y, string opt, string loss, double lr);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ml_model_train_step_ex(long model, long x, long y, string opt, string loss, double lr, string optsJson);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial double gmb_ml_model_dpo_step(long ms, long mr, long x, long c, long r, string optsJson);
    // L6: pipeline dati testuale toy (char-level).
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial long gmb_ml_text_encode(string text);
    [LibraryImport(LoaderName)]
    public static partial int gmb_ml_text_decode(long h, IntPtr outBuf, int cap);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial long gmb_ml_dataset_load_text(string path, string optsJson);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ml_model_export(long h, string path, string format);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ml_imatrix_layer_energy(string path, IntPtr outBuf, int cap);
    [LibraryImport(LoaderName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ml_imatrix_rank_map(string path, int minRank, int maxRank, IntPtr outBuf, int cap);
    [LibraryImport(LoaderName)] public static partial double gmb_ml_model_loss(long model, long x, long y);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_model_forward(long model, long input);
    [LibraryImport(LoaderName)] public static partial long gmb_ml_model_parameters(long model);
    [LibraryImport(LoaderName)] public static partial int gmb_ml_model_state(long model, IntPtr outBuf, int cap);
}
