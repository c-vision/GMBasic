namespace Basic.IlBackend;

public static partial class IlRuntime
{
    public static double FfiCall(string lib, string symbol, object?[] args)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunction(fnPtr, args);
        return Convert.ToDouble(result ?? 0.0);
    }

    /// <summary>FFI tipizzata (OP_CALL_FFI_T): kind omogeneo 0=double, 1=int64 — il
    /// delegate CLR usa la firma reale del simbolo (long/long), il valore torna double.</summary>
    public static double FfiCallTyped(string lib, string symbol, object?[] args, int kind)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTyped(fnPtr, args, (byte)kind);
        return Convert.ToDouble(result ?? 0.0);
    }

    /// <summary>FFI tipizzata con parametro array BYREF e/o STRING (2026-09-28): <paramref
    /// name="arrayMask"/> bit i settato → args[i] è un array CLR (double[]/long[]) da pinnare e
    /// passare come puntatore nativo; <paramref name="stringMask"/> bit i settato → args[i] è
    /// una string CLR (UTF-8, valida solo durante la chiamata). Gli altri slot restano scalari
    /// kind-omogenei. Usato quando il RITORNO non è STRING (altrimenti vedi
    /// <see cref="FfiCallTypedArrStr"/>) — vedi IlProgramCompiler.Expressions.cs EmitFfiCall e
    /// FfiService.CallFunctionTypedArr.</summary>
    /// <para>BYREF scalare (2026-09-28): <paramref name="byrefScalarMask"/> bit i settato →
    /// args[i] è il valore IN di un parametro BYREF scalare (marshallato con un parametro CLR
    /// ref dinamico — vedi FfiService.CallFunctionTypedArr). DOPO la chiamata, <c>args[i]</c> è
    /// stato sovrascritto dal chiamato (FfiService) col valore OUT — il chiamante (IL generato
    /// da EmitFfiCall) rilegge <c>args</c> per il write-back sulla variabile BASIC. <paramref
    /// name="byrefScalarElemIntMask"/> indica se quel valore è intero (AS LONG/...) invece di
    /// DOUBLE.</para>
    /// <param name="scalarKindMask">Firma mista LONG/DOUBLE fra scalari semplici (2026-10, v1,
    /// ammessa solo quando non c'è nessun array/STRING/BYREF/UDT nella firma — guard a
    /// compile-time in IlProgramCompiler.Expressions.cs): bit i = il parametro SCALARE
    /// SEMPLICE i è Int64, invece del <paramref name="kind"/> uniforme. Default 0: nel caso
    /// omogeneo esistente il chiamante passa comunque il valore coerente con <paramref
    /// name="kind"/> — vedi FfiService.CallFunctionTypedArr.</param>
    public static double FfiCallTypedArr(string lib, string symbol, object?[] args, int kind, long arrayMask, long arrayElemIntMask, long stringMask, long byrefScalarMask = 0, long byrefScalarElemIntMask = 0, long scalarKindMask = 0)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTypedArr(fnPtr, args, kind, arrayMask, arrayElemIntMask, stringMask, retIsString: false, byrefScalarMask, byrefScalarElemIntMask, scalarKindMask: scalarKindMask);
        return Convert.ToDouble(result ?? 0.0);
    }

    /// <summary>Come <see cref="FfiCallTypedArr"/>, per una dichiarazione FFI il cui RITORNO è
    /// STRING (2026-09-28): il backend IL è staticamente tipizzato — deve sapere a
    /// COMPILE-TIME se il valore che arriva sullo stack è una string o un double, quindi serve
    /// un metodo runtime a parte che ritorna string invece di double (FfiService fa il
    /// copy-out UTF-8 del puntatore nativo prima di tornare — vedi
    /// FfiService.CallFunctionTypedArr). PCodeVM non ne ha bisogno: task.PushRef è già generico.</summary>
    public static string FfiCallTypedArrStr(string lib, string symbol, object?[] args, int kind, long arrayMask, long arrayElemIntMask, long stringMask, long byrefScalarMask = 0, long byrefScalarElemIntMask = 0)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTypedArr(fnPtr, args, kind, arrayMask, arrayElemIntMask, stringMask, retIsString: true, byrefScalarMask, byrefScalarElemIntMask, scalarKindMask: (kind == 1 ? 0xFFFF : 0));
        return result as string ?? "";
    }

    /// <summary>Struct/UDT per valore (2026-09-29): come <see cref="FfiCallTypedArr"/>, ma
    /// almeno un parametro è un UDT semplice passato per VALORE — <paramref
    /// name="udtParamTypes"/>[i] non-null è il tipo CLR REALE (già costruito a compile-time,
    /// TypeBuilder) di quel parametro; <c>args[i]</c> è già l'istanza boxed corrispondente
    /// (EmitObjectBoxedArgument, EmitFfiCall). Usato quando il RITORNO NON è un UDT né STRING
    /// (altrimenti vedi <see cref="FfiCallTypedArrUdtStr"/>/<see cref="FfiCallTypedArrUdtRet"/>).</summary>
    public static double FfiCallTypedArrUdt(string lib, string symbol, object?[] args, int kind, long arrayMask, long arrayElemIntMask, long stringMask, long byrefScalarMask, long byrefScalarElemIntMask, string[] udtShapes, long scalarKindMask, long byrefUdtMask = 0)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTypedArr(fnPtr, args, kind, arrayMask, arrayElemIntMask, stringMask, retIsString: false, byrefScalarMask, byrefScalarElemIntMask, udtShapeStrs: udtShapes, scalarKindMask: scalarKindMask, byrefUdtMask: byrefUdtMask);
        return Convert.ToDouble(result ?? 0.0);
    }

    /// <summary>BYREF UDT (2026-11-06): riconverte la struct-ombra (layout C, tipo del
    /// delegate) nel tipo del DIALETTO (campos 64-bit) partendo dal valore corrente della
    /// variabile BASIC e applicando le foglie lette — usato dal write-back del BYREF UDT.</summary>
    public static object FfiCopyShapeToStruct(string shapeStr, object? shadow, object? current)
        => Basic.Runtime.Services.FfiService.CopyShapeToStruct(shapeStr, shadow, current);

    /// <summary>Come <see cref="FfiCallTypedArrUdt"/>, per una dichiarazione il cui RITORNO è
    /// STRING (stesso motivo di <see cref="FfiCallTypedArrStr"/>: IL è staticamente tipizzato).</summary>
    public static string FfiCallTypedArrUdtStr(string lib, string symbol, object?[] args, int kind, long arrayMask, long arrayElemIntMask, long stringMask, long byrefScalarMask, long byrefScalarElemIntMask, string[] udtShapes, long scalarKindMask, long byrefUdtMask = 0)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTypedArr(fnPtr, args, kind, arrayMask, arrayElemIntMask, stringMask, retIsString: true, byrefScalarMask, byrefScalarElemIntMask, udtShapeStrs: udtShapes, scalarKindMask: scalarKindMask, byrefUdtMask: byrefUdtMask);
        return result as string ?? "";
    }

    /// <summary>Come <see cref="FfiCallTypedArrUdt"/>, per una dichiarazione il cui RITORNO
    /// stesso è un UDT per valore: <paramref name="udtReturnType"/> è il tipo CLR reale del
    /// ritorno — il risultato torna COM'È (istanza boxed di quel tipo), il chiamante
    /// (EmitFfiCall) la unboxa con <c>Unbox_Any</c> sul tipo noto a compile-time. <paramref
    /// name="udtParamTypes"/> può essere null se nessun PARAMETRO è un UDT (solo il ritorno).</summary>
    public static object FfiCallTypedArrUdtRet(string lib, string symbol, object?[] args, int kind, long arrayMask, long arrayElemIntMask, long stringMask, long byrefScalarMask, long byrefScalarElemIntMask, string[]? udtShapes, long scalarKindMask, long byrefUdtMask, string udtReturnShape, Type udtReturnType)
    {
        var ffi = Current.Ffi;
        var handle = ffi.LoadLibrary(lib);
        var fnPtr = ffi.GetFunction(handle, symbol);
        var result = ffi.CallFunctionTypedArr(fnPtr, args, kind, arrayMask, arrayElemIntMask, stringMask, retIsString: false, byrefScalarMask, byrefScalarElemIntMask, udtReturnClrType: udtReturnType, scalarKindMask: scalarKindMask, udtShapeStrs: udtShapes, udtReturnShapeStr: udtReturnShape, byrefUdtMask: byrefUdtMask);
        return result!;
    }
}