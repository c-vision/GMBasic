// FILE: IlBackend/IlModuleLoader.cs
// Loader GENERICO di moduli IL precompilati (.dll) per il backend IL — non limitato al
// PackageManager: carica qualunque assembly .NET con classi pubbliche statiche marcate
// [GmbHModule("Namespace")] e ne espone i metodi pubblici statici come `Namespace.Method(...)`.
//
// Come funziona:
//   1. Load(path)/Register(type) scansiona i metodi pubblici statici e registra
//      NativeProcedureDefinition in StdLib (come un normale namespace builtin), così il
//      parser riconosce `NS.METHOD` e il linker lo risolve normalmente.
//   2. Il backend IL intercepTA in EmitBuiltinFunctionCall il (namespace, nome) e chiama
//      direttamente il MethodInfo (EmitExternalCall), con marshalling di double/string/bool/object.
//
// Tipi CLR supportati: double/int/long/bool/string/void/object.

using System.Collections.Concurrent;
using System.Collections.Immutable;
using System.Reflection;

namespace Basic.IlBackend;

/// <summary>Marca una classe statica come modulo GMBasic (namespace BASIC esposto).</summary>
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public sealed class GmbHModuleAttribute(string name) : Attribute
{
    public string Name { get; } = name;
}

/// <summary>Loader generico di moduli IL (namespace → metodi statici esterni).</summary>
public static class IlModuleLoader
{
    private static readonly ConcurrentDictionary<(string, string), MethodInfo> _methods =
        new(new TupleKeyComparer());
    private static readonly ConcurrentDictionary<string, bool> _namespaces =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Namespace registrati.</summary>
    public static ICollection<string> Namespaces => _namespaces.Keys;

    private sealed class TupleKeyComparer : IEqualityComparer<(string, string)>
    {
        public bool Equals((string, string) x, (string, string) y)
            => string.Equals(x.Item1, y.Item1, StringComparison.OrdinalIgnoreCase)
            && string.Equals(x.Item2, y.Item2, StringComparison.OrdinalIgnoreCase);
        public int GetHashCode((string, string) obj) =>
            StringComparer.OrdinalIgnoreCase.GetHashCode(obj.Item1)
            ^ (StringComparer.OrdinalIgnoreCase.GetHashCode(obj.Item2) << 5);
    }

    static IlModuleLoader()
    {
        // Hook per Orchestrator (che non referenzia IlBackend): REQUIRE di un pacchetto
        // contentType dotnet → carica la .dll e registra i namespace prima del parse.
        StdLib.Instance.LoadExternalModule = Load;
    }

    /// <summary>Carica un assembly da file e registra tutti i tipi [GmbHModule].</summary>
    public static int Load(string dllPath)
    {
        var asm = Assembly.LoadFrom(dllPath);
        var count = 0;
        foreach (var t in asm.GetTypes())
            if (t.GetCustomAttribute<GmbHModuleAttribute>() is { } attr)
            {
                Register(t, attr.Name);
                count++;
            }
        return count;
    }

    /// <summary>Registra un tipo (statics pubbliche) sotto il namespace indicato.</summary>
    public static void Register(Type moduleType, string? namespaceName = null)
    {
        var attr = moduleType.GetCustomAttribute<GmbHModuleAttribute>();
        var ns = namespaceName ?? attr?.Name ?? moduleType.Name;
        var procedures = new List<ProcedureDefinition>();
        foreach (var m in moduleType.GetMethods(BindingFlags.Public | BindingFlags.Static))
        {
            if (m.IsSpecialName) continue; // property/getter/setter
            _methods[(ns!, m.Name)] = m;
            procedures.Add(ToNativeProcedure(ns!, m));
        }
        StdLib.Instance.RegisterExternalModule(ns!, procedures);
        StdLib.Instance.ExternalModuleNames.Add(ns!);
        _namespaces[ns!] = true;
    }

    /// <summary>Rimuove un namespace (per test/ricaricamento).</summary>
    public static void Unregister(string namespaceName)
    {
        foreach (var k in _methods.Keys)
            if (string.Equals(k.Item1, namespaceName, StringComparison.OrdinalIgnoreCase))
                _methods.TryRemove(k, out _);
        _namespaces.TryRemove(namespaceName, out _);
        StdLib.Instance.ExternalModuleNames.Remove(namespaceName);
        StdLib.Instance.UnregisterExternalModule(namespaceName);
    }

    /// <summary>Cerca un metodo esterno per (namespace, nome).</summary>
    public static bool TryGet(string ns, string method, out MethodInfo mi)
        => _methods.TryGetValue((ns, method), out mi!);

    private static DataType ToDataType(Type t)
    {
        if (t == typeof(double) || t == typeof(float)) return new DoubleType();
        if (t == typeof(bool)) return new BitType();
        if (t == typeof(string)) return new StringType();
        if (t == typeof(void)) return new VoidType();
        if (t == typeof(int) || t == typeof(long) || t == typeof(short) || t == typeof(byte)
            || t == typeof(uint) || t == typeof(ulong) || t == typeof(ushort) || t == typeof(sbyte))
            return new LongType();
        return new AnyType(); // object e altri tipi → Variant/Any
    }

    private static NativeProcedureDefinition ToNativeProcedure(string ns, MethodInfo m)
    {
        var nameToken = new Token(TokenKind.Identifier, m.Name, null, SourcePosition.Unknown);
        var parameters = m.GetParameters().Select(p =>
            new Parameter(-1, new Token(TokenKind.Identifier, p.Name ?? "p", null, SourcePosition.Unknown),
                ToDataType(p.ParameterType), SymbolFlags.None, null, false)).ToImmutableArray();
        var isFunction = m.ReturnType != typeof(void);
        return new NativeProcedureDefinition(-1,
            SymbolFlags.IsPublic | SymbolFlags.IsFunction,
            nameToken, parameters, ToDataType(m.ReturnType), TokenKind.Identifier, isFunction);
    }
}
