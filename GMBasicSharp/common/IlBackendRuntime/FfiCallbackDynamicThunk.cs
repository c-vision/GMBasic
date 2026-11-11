// FILE: IlBackendRuntime/FfiCallbackDynamicThunk.cs
// Trampolino nativo GENERICO per Callback come parametro FFI (2026-11-11): costruisce a
// runtime il tipo delegate con la firma nativa ESATTA (kind per slot: INT64, DOUBLE o UDT per
// valore) e un thunk che la richiama, senza i delegate pre-dichiarati di
// FfiCallbackTrampolineTypes (che coprono solo firme interne OMOGENEE e arità 0-8).
//
// Perché serve: una callback C può avere firma mista (`int (*)(int64_t, double)`), arità > 8
// o una struct per valore — il CLR sa generare un thunk nativo per OGNI istanza di un delegate
// taggato [UnmanagedFunctionPointer(Cdecl)] (Marshal.GetFunctionPointerForDelegate), ma il
// tipo delegate deve esistere: qui viene creato con Reflection.Emit (cache per firma, vedi
// DynamicDelegateTypes) e il corpo del thunk è una expression tree che converte gli argomenti
// in object[], invoca il delegato BASIC e riconverte il risultato nel tipo nativo atteso.
//
// Il chiamante DEVE mantenere vivo il delegate restituito (GC-rooted) per tutta la durata
// della chiamata nativa: il puntatore nativo da solo non impedisce la raccolta.

using System.Linq.Expressions;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Basic.IlBackend;

internal static class FfiCallbackDynamicThunk
{
    private const BindingFlags AnyStatic =
        BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;

    private static readonly MethodInfo ToI64Method =
        typeof(FfiCallbackTrampolineTypes).GetMethod(nameof(FfiCallbackTrampolineTypes.ToI64), AnyStatic)!;
    private static readonly MethodInfo ToF64Method =
        typeof(FfiCallbackTrampolineTypes).GetMethod(nameof(FfiCallbackTrampolineTypes.ToF64), AnyStatic)!;

    /// <summary>Trampolino nativo per una callback scalare con firma per-slot.
    /// <paramref name="paramIsInt"/>: true = il parametro i-esimo è un intero a 8 byte
    /// (LONG/INTEGER/QUAD/BYTE/...), false = DOUBLE/SINGLE. <paramref name="retKind"/>:
    /// 0 = double, 1 = int64, 2 = nessun ritorno (SUB). <paramref name="invoke"/> riceve gli
    /// argomenti nativi già convertiti in object? e restituisce il risultato BASIC.</summary>
    internal static (Delegate KeepAlive, IntPtr Ptr) Create(bool[] paramIsInt, int retKind, Func<object?[], object?> invoke)
    {
        var types = new Type[paramIsInt.Length];
        for (var i = 0; i < paramIsInt.Length; i++)
            types[i] = paramIsInt[i] ? typeof(long) : typeof(double);
        return Create(types, retKind switch { 1 => typeof(long), 2 => typeof(void), _ => typeof(double) }, invoke);
    }

    /// <summary>Variante con i tipi CLR espliciti: usata per le callback con parametri/ritorno
    /// UDT per valore (struct blittable, passata per valore nell'ABI C).</summary>
    internal static (Delegate KeepAlive, IntPtr Ptr) Create(Type[] paramTypes, Type returnType, Func<object?[], object?> invoke)
    {
        var delegateType = DynamicDelegateTypes.GetOrBuild(returnType, paramTypes, cdecl: true);
        var ps = new ParameterExpression[paramTypes.Length];
        for (var i = 0; i < paramTypes.Length; i++) ps[i] = Expression.Parameter(paramTypes[i], "a" + i);
        var boxed = Expression.NewArrayInit(typeof(object),
            Array.ConvertAll(ps, p => (Expression)Expression.Convert(p, typeof(object))));
        var call = Expression.Invoke(Expression.Constant(invoke), boxed);
        Expression body;
        if (returnType == typeof(void)) body = Expression.Block(call, Expression.Empty());
        else if (returnType == typeof(long)) body = Expression.Call(ToI64Method, call);
        else if (returnType == typeof(double)) body = Expression.Call(ToF64Method, call);
        else body = Expression.Convert(call, returnType);   // struct/altro: unbox del risultato
        var d = Expression.Lambda(delegateType, body, ps).Compile();
        return (d, Marshal.GetFunctionPointerForDelegate(d));
    }
}
