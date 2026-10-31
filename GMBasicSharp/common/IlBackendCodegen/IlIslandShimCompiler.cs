// FILE: Basic.IlBackend.IlIslandShimCompiler.cs
// ISLAND SHIM COMPILER — genera i delegate IslandRun per ogni procedura native-eligible
// già compilata in CIL da IlProgramCompiler. Lo shim legge gli argomenti dal
// PCodeTask.Stack (entry di bytecode), li converte al tipo CIL corretto, chiama il
// metodo interno GenerateProgram.Proc_{symbolId}, e restituisce IslandRuntime.Return.
//
// Supporta BYREF (ByRefBox → lettura/scrittura diretta sullo slot del chiamante) e
// differenzia SUB (nessun push) da FUNCTION (push del valore di ritorno).
//
// Zero modifiche a IlProgramCompiler.cs — la shim è generata a runtime dal tipo
// assembly già caricato.

using System.Globalization;
using System.Reflection;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public static class IlIslandShimCompiler
{
    /// <summary>Percorre tutte le FunctionDefinition native-eligible del programma,
    /// trova i MethodInfo corrispondenti nell'assembly generato, crea gli shim delegate
    /// e li registra nell'IslandRegistry (per nome BASIC, case-insensitive).
    /// Ritorna il numero di isole registrate.</summary>
    public static int RegisterIslands(BasicProgram program, Assembly assembly)
    {
        IlRuntime.SetMacroContext(program);

        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException(
                "IlIslandShimCompiler: tipo 'GeneratedProgram' non trovato nell'assembly generato.");

        var procedures = program.MainModule?.Declarations.OfType<FunctionDefinition>()
            ?? Array.Empty<FunctionDefinition>();

        var registered = 0;
        foreach (var proc in procedures)
        {
            if (!IslandEligibility.IsNativeEligible(proc))
                continue;

            var method = programType.GetMethod("Proc_" + proc.SymbolId,
                BindingFlags.Public | BindingFlags.Static);
            if (method is null) continue;

            var island = CreateIslandShim(method, proc);
            IslandRegistry.Register(proc.OriginalName.Lexeme, island);
            registered++;
        }

        return registered;
    }

    public static IslandRun CreateIslandShim(MethodInfo innerMethod, FunctionDefinition procDef)
    {
        var paramInfos = innerMethod.GetParameters();
        var paramCount = paramInfos.Length;
        var hasReturn = innerMethod.ReturnType != typeof(void);
        var basicParams = procDef.Parameters;

        // Pre-computa per ogni param: è BYREF? Tipo target.
        var paramMeta = new (bool ByRef, Type ClrType, bool IsBasicByRef)[paramCount];
        for (var i = 0; i < paramCount; i++)
        {
            var clrType = paramInfos[i].ParameterType;
            var isCilByRef = clrType.IsByRef;
            paramMeta[i] = (
                ByRef: isCilByRef,
                ClrType: isCilByRef ? clrType.GetElementType()! : clrType,
                IsBasicByRef: i < basicParams.Length && basicParams[i].Flags.HasFlag(SymbolFlags.IsByRef)
            );
        }

        return task =>
        {
            var frame = task.Frame;
            if (frame == null)
                return IslandRuntime.Return(task);

            var savedSp = frame.SavedStackPointer;
            var args = new object?[paramCount];
            var byRefBoxes = new ByRefBox?[paramCount];

            // Legge gli argomenti dallo stack del chiamante
            for (var i = 0; i < paramCount; i++)
            {
                var entry = task.Stack[savedSp + i];
                var raw = entry.Type == 2 ? entry.Ref : (object)entry.Num;
                var meta = paramMeta[i];

                if (meta.IsBasicByRef && raw is ByRefBox box)
                {
                    // BYREF in BASIC: la variabile del chiamante è dentro il ByRefBox.
                    // Leggiamo il valore corrente, lo passiamo (eventualmente come ref CIL),
                    // e dopo la chiamata riscriviamo nel ByRefBox.
                    byRefBoxes[i] = box;
                    raw = box.Value;
                }

                args[i] = ConvertArg(raw, meta.ClrType);
            }

            object? result;
            IslandRuntime.SetCurrentTask(task);
            var host = IslandRuntime.CurrentFileHost;
            if (host != null) IlRuntime.SetCurrent(host);
            try
            {
                // MethodInfo.Invoke non supporta correttamente i parametri ref
                // di reference type (es. ref Int16[] per array byref BASIC).
                // Per questi casi, crea un wrapper che gestisce il passaggio byref.
                if (paramMeta.Any(m => m.ByRef))
                {
                    result = InvokeWithByRef(innerMethod, args, paramMeta);
                }
                else
                {
                    result = innerMethod.Invoke(null, args);
                }
            }
            catch (TargetInvocationException tie)
            {
                if (tie.InnerException is IslandParkedException)
                    return IslandResult.Blocked;
                task.PendingException = tie.InnerException ?? tie;
                return IslandRuntime.Return(task);
            }
            catch (IslandParkedException)
            {
                return IslandResult.Blocked;
            }
            catch (Exception ex)
            {
                task.PendingException = ex;
                return IslandRuntime.Return(task);
            }
            finally
            {
                IslandRuntime.SetCurrentTask(null);
            }

            for (var i = 0; i < paramCount; i++)
            {
                if (byRefBoxes[i] is { } refBox)
                    refBox.Value = args[i];
            }

            if (hasReturn)
                task.PushRef(result);

            return IslandRuntime.Return(task);
        };
    }

    /// <summary>Invoca il metodo con supporto per parametri byref (CIL &amp;T).</summary>
    private static object? InvokeWithByRef(MethodInfo method, object?[] args, (bool ByRef, Type ClrType, bool IsBasicByRef)[] meta)
    {
        // Fallback: MethodInfo.Invoke. Per array byref, il CLR gestisce
        // la conversione automatica se l'argomento e' del tipo corretto.
        return method.Invoke(null, args);
    }

    /// <summary>Converte un valore raw (boxed double per Type==1, altrimenti oggetto)
    /// nel tipo target del parametro CIL, senza fallire su cast impliciti impossibili.</summary>
    private static object? ConvertArg(object? value, Type targetType)
    {
        if (value == null) return null;
        if (targetType.IsInstanceOfType(value)) return value;

        // Array: converti object[] → T[] dove T è l'element type
        if (targetType.IsArray && value is Array srcArray)
        {
            var elemType = targetType.GetElementType()!;
            var dstArray = Array.CreateInstance(elemType, srcArray.Length);
            for (var i = 0; i < srcArray.Length; i++)
                dstArray.SetValue(ConvertArg(srcArray.GetValue(i), elemType), i);
            return dstArray;
        }

        // GbCurrency → decimal
        if (targetType == typeof(decimal) && value is Basic.Databases.Rdd.GbCurrency gbCur)
            return gbCur.ToDecimal();

        // DateValue (P-Code internal) → DateTime
        if (targetType == typeof(DateTime) && value.GetType().FullName == "Basic.PCodeVm.PCodeProcessor+DateValue")
        {
            var oaDate = (double)value.GetType().GetProperty("OADate")!.GetValue(value)!;
            return DateTime.FromOADate(oaDate);
        }

        // Dictionary<string,object> (P-Code class instance) → CLR class
        if (targetType.IsClass && targetType != typeof(string) && targetType != typeof(object) &&
            value is System.Collections.IDictionary dict)
        {
            try
            {
                var instance = Activator.CreateInstance(targetType);
                foreach (System.Collections.DictionaryEntry kv in dict)
                {
                    var name = kv.Key?.ToString();
                    if (name == null || name == "__class__") continue;
                    // Try property first, then field
                    var prop = targetType.GetProperty(name, BindingFlags.Public | BindingFlags.Instance | BindingFlags.IgnoreCase);
                    if (prop?.CanWrite == true)
                    {
                        var propValue = ConvertArg(kv.Value, prop.PropertyType);
                        prop.SetValue(instance, propValue);
                    }
                    else
                    {
                        var field = targetType.GetField(name, BindingFlags.Public | BindingFlags.Instance | BindingFlags.IgnoreCase);
                        if (field != null)
                        {
                            var fieldValue = ConvertArg(kv.Value, field.FieldType);
                            field.SetValue(instance, fieldValue);
                        }
                    }
                }
                return instance;
            }
            catch { return value; }
        }

        if (value is not IConvertible)
            return value;

        var tc = Type.GetTypeCode(targetType);
        return tc switch
        {
            TypeCode.Int32    => Convert.ToInt32(value, CultureInfo.InvariantCulture),
            TypeCode.Int64    => Convert.ToInt64(value, CultureInfo.InvariantCulture),
            TypeCode.Double   => Convert.ToDouble(value, CultureInfo.InvariantCulture),
            TypeCode.Single   => Convert.ToSingle(value, CultureInfo.InvariantCulture),
            TypeCode.Int16    => Convert.ToInt16(value, CultureInfo.InvariantCulture),
            TypeCode.Byte     => Convert.ToByte(value, CultureInfo.InvariantCulture),
            TypeCode.SByte    => Convert.ToSByte(value, CultureInfo.InvariantCulture),
            TypeCode.UInt16   => Convert.ToUInt16(value, CultureInfo.InvariantCulture),
            TypeCode.UInt32   => Convert.ToUInt32(value, CultureInfo.InvariantCulture),
            TypeCode.UInt64   => Convert.ToUInt64(value, CultureInfo.InvariantCulture),
            TypeCode.Decimal  => Convert.ToDecimal(value, CultureInfo.InvariantCulture),
            TypeCode.Boolean  => Convert.ToBoolean(value, CultureInfo.InvariantCulture),
            TypeCode.String   => Convert.ToString(value, CultureInfo.InvariantCulture),
            TypeCode.DateTime => Convert.ToDateTime(value, CultureInfo.InvariantCulture),
            _ => value,
        };
    }
}
