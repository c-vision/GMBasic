// FILE: IlBackendRuntime/DynamicDelegateTypes.cs
// Tipi delegate costruiti a RUNTIME (2026-11-11).
//
// Perché: i delegate del BCL sono definiti fino a 16 parametri (`System.Func`1..17`,
// `System.Action`1..16`) — un limite della libreria, non della piattaforma. GMBASIC permette
// callback fino a 64 parametri (FfiMaxArgs), quindi oltre i 16 serve un tipo delegate creato
// con Reflection.Emit. Due consumatori:
//   - il compilatore IL (DataTypeMapper): tipo del delegate delle lambda/riferimenti a
//     funzione. Deve stare NELLO STESSO modulo dell'assembly in compilazione (l'IL emesso
//     referenzia solo tipi del proprio modulo, e il .dll è salvato con PersistedAssemblyBuilder
//     → un tipo di un assembly dinamico "Run" non sarebbe risolvibile al caricamento);
//   - FfiCallbackDynamicThunk: tipo del THUNK nativo passato alla libreria C, con
//     [UnmanagedFunctionPointer(Cdecl)]. Qui un assembly "Run" basta e avanza (il tipo non è
//     mai referenziato dall'IL, solo da reflection a runtime).
//
// Cache per firma; nel caso "modulo di compilazione" il TypeBuilder va CreateType()-ato alla
// fine della compilazione (i parametri possono essere TypeBuilder di UDT non ancora creati):
// il chiamante lo registra in IlTypeContext.PendingDelegateTypes.

using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;

namespace Basic.IlBackend;

internal static class DynamicDelegateTypes
{
    private static readonly Dictionary<string, Type> Cache = new();
    private static readonly Dictionary<(ModuleBuilder, string), (TypeBuilder Type, ConstructorBuilder Ctor, MethodBuilder Invoke)> ModuleCache = new();

    /// <summary>Tipo delegate con la firma data in un assembly dinamico "Run" (creato subito).
    /// <paramref name="cdecl"/> = true aggiunge [UnmanagedFunctionPointer(Cdecl)].</summary>
    internal static Type GetOrBuild(Type returnType, Type[] paramTypes, bool cdecl)
    {
        var key = Key(returnType, paramTypes, cdecl);
        lock (Cache)
        {
            if (Cache.TryGetValue(key, out var cached)) return cached;
            var asm = AssemblyBuilder.DefineDynamicAssembly(
                new AssemblyName("GmbDynamicDelegates"), AssemblyBuilderAccess.Run);
            var mod = asm.DefineDynamicModule("Main");
            var created = BuildDelegateType(mod, returnType, paramTypes, cdecl).Type.CreateType()!;
            Cache[key] = created;
            return created;
        }
    }

    /// <summary>Tipo delegate definito NEL modulo dato (<paramref name="pending"/> riceve il
    /// TypeBuilder da creare alla fine). Riusa il TypeBuilder se già definito per la firma.</summary>
    internal static (TypeBuilder Type, ConstructorBuilder Ctor, MethodBuilder Invoke) DefineIn(
        ModuleBuilder module, List<TypeBuilder> pending, Type returnType, Type[] paramTypes, bool cdecl)
    {
        var key = Key(returnType, paramTypes, cdecl);
        lock (ModuleCache)
        {
            if (ModuleCache.TryGetValue((module, key), out var cached)) return cached;
            var built = BuildDelegateType(module, returnType, paramTypes, cdecl);
            ModuleCache[(module, key)] = built;
            pending.Add(built.Type);
            return built;
        }
    }

    private static string Key(Type returnType, Type[] paramTypes, bool cdecl)
    {
        static string Code(Type t) => t == typeof(void) ? "V" : t == typeof(long) ? "I"
            : t == typeof(double) ? "D" : t.FullName ?? t.Name;
        return (cdecl ? "C|" : "M|") + Code(returnType) + "|" + string.Join(",", Array.ConvertAll(paramTypes, Code));
    }

    private static (TypeBuilder Type, ConstructorBuilder Ctor, MethodBuilder Invoke) BuildDelegateType(
        ModuleBuilder modBuilder, Type returnType, Type[] paramTypes, bool cdecl)
    {
        var typeBuilder = modBuilder.DefineType("DynDel_" + Guid.NewGuid().ToString("N")[..8],
            TypeAttributes.Public | TypeAttributes.Sealed, typeof(MulticastDelegate));
        if (cdecl)
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
        return (typeBuilder, ctor, invoke);
    }
}
