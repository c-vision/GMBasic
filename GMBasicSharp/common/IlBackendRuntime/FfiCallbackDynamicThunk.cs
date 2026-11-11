// FILE: IlBackendRuntime/FfiCallbackDynamicThunk.cs
// Trampolino nativo GENERICO per Callback come parametro FFI (2026-11-11): costruisce a
// runtime il tipo delegate con la firma nativa ESATTA (kind per slot: INT64 o DOUBLE, ritorno
// INT64/DOUBLE/nessuno) e un thunk che la richiama, senza i 46 delegate pre-dichiarati di
// FfiCallbackTrampolineTypes (che coprono solo firme interne OMOGENEE e arità 0-8).
//
// Perché serve: una callback C può avere firma mista (`int (*)(int64_t, double)`) o arità > 8
// — il CLR sa generare un thunk nativo per OGNI istanza di un delegate taggato
// [UnmanagedFunctionPointer(Cdecl)] (Marshal.GetFunctionPointerForDelegate), ma il tipo
// delegate deve esistere: qui viene creato con Reflection.Emit (cache per firma) e il corpo del
// thunk è una expression tree che converte gli argomenti in object[], invoca il delegato BASIC
// e riconverte il risultato nel tipo nativo atteso.
//
// Il chiamante DEVE mantenere vivo il delegate restituito (GC-rooted) per tutta la durata
// della chiamata nativa: il puntatore nativo da solo non impedisce la raccolta.

using System.Linq.Expressions;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace Basic.IlBackend;

internal static class FfiCallbackDynamicThunk
{
    /// <summary>Tipi delegate già costruiti, per firma: "R|k0,k1,..." con R/k = 'I' (int64),
    /// 'D' (double) o 'V' (void). Un tipo per firma, riusato da tutte le istanze.</summary>
    private static readonly Dictionary<string, Type> DelegateTypes = new();

    private const BindingFlags AnyStatic =
        BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;

    private static readonly MethodInfo ToI64Method =
        typeof(FfiCallbackTrampolineTypes).GetMethod(nameof(FfiCallbackTrampolineTypes.ToI64), AnyStatic)!;
    private static readonly MethodInfo ToF64Method =
        typeof(FfiCallbackTrampolineTypes).GetMethod(nameof(FfiCallbackTrampolineTypes.ToF64), AnyStatic)!;

    /// <summary>Crea il trampolino nativo per una callback scalare con firma per-slot.
    /// <paramref name="paramIsInt"/>: true = il parametro i-esimo è un intero a 8 byte
    /// (LONG/INTEGER/QUAD/BYTE/...), false = DOUBLE/SINGLE. <paramref name="retKind"/>:
    /// 0 = double, 1 = int64, 2 = nessun ritorno (SUB). <paramref name="invoke"/> riceve gli
    /// argomenti nativi già convertiti in object? e restituisce il risultato BASIC.</summary>
    internal static (Delegate KeepAlive, IntPtr Ptr) Create(bool[] paramIsInt, int retKind, Func<object?[], object?> invoke)
    {
        var types = new Type[paramIsInt.Length];
        for (var i = 0; i < paramIsInt.Length; i++)
            types[i] = paramIsInt[i] ? typeof(long) : typeof(double);
        var returnType = retKind switch
        {
            1 => typeof(long),
            2 => typeof(void),
            _ => typeof(double)
        };
        var delegateType = GetOrBuildDelegateType(returnType, types);
        var ps = new ParameterExpression[types.Length];
        for (var i = 0; i < types.Length; i++) ps[i] = Expression.Parameter(types[i], "a" + i);
        var boxed = Expression.NewArrayInit(typeof(object),
            Array.ConvertAll(ps, p => (Expression)Expression.Convert(p, typeof(object))));
        var call = Expression.Invoke(Expression.Constant(invoke), boxed);
        Expression body = retKind switch
        {
            1 => Expression.Call(ToI64Method, call),
            2 => Expression.Block(call, Expression.Empty()),
            _ => Expression.Call(ToF64Method, call)
        };
        var d = Expression.Lambda(delegateType, body, ps).Compile();
        return (d, Marshal.GetFunctionPointerForDelegate(d));
    }

    private static Type GetOrBuildDelegateType(Type returnType, Type[] paramTypes)
    {
        static char Code(Type t) => t == typeof(void) ? 'V' : t == typeof(long) ? 'I' : 'D';
        var key = Code(returnType) + "|" + string.Concat(Array.ConvertAll(paramTypes, Code));
        lock (DelegateTypes)
        {
            if (DelegateTypes.TryGetValue(key, out var cached)) return cached;
            var asmName = new AssemblyName("FfiCallbackDelegates");
            var asmBuilder = AssemblyBuilder.DefineDynamicAssembly(asmName, AssemblyBuilderAccess.Run);
            var modBuilder = asmBuilder.DefineDynamicModule("Main");
            var typeBuilder = modBuilder.DefineType("FfiCb_" + Guid.NewGuid().ToString("N")[..8],
                TypeAttributes.Public | TypeAttributes.Sealed, typeof(MulticastDelegate));
            // Cdecl: le librerie native su macOS/Linux usano la convenzione C (su Windows il
            // runtime FFI la ignora per default, stesso comportamento storico dei delegate
            // pre-dichiarati in FfiCallbackTrampolineTypes).
            typeBuilder.SetCustomAttribute(new CustomAttributeBuilder(
                typeof(UnmanagedFunctionPointerAttribute).GetConstructor(new[] { typeof(CallingConvention) })!,
                new object[] { CallingConvention.Cdecl }));

            var ctor = typeBuilder.DefineConstructor(
                MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.SpecialName | MethodAttributes.RTSpecialName,
                CallingConventions.Standard, new[] { typeof(object), typeof(IntPtr) });
            ctor.SetImplementationFlags(MethodImplAttributes.Runtime | MethodImplAttributes.Managed);

            var invoke = typeBuilder.DefineMethod("Invoke",
                MethodAttributes.Public | MethodAttributes.HideBySig | MethodAttributes.NewSlot | MethodAttributes.Virtual,
                returnType, paramTypes);
            invoke.SetImplementationFlags(MethodImplAttributes.Runtime | MethodImplAttributes.Managed);

            var created = typeBuilder.CreateType()!;
            DelegateTypes[key] = created;
            return created;
        }
    }
}
