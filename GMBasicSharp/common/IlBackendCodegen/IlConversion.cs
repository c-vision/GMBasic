// FILE: Basic.IlBackend.IlConversion.cs

using System.Reflection;
using System.Reflection.Emit;

namespace Basic.IlBackend;

/// <summary>
/// Classifica la famiglia di una conversione di tipo generata dal backend IL.
/// Modellata su <c>ConversionKind</c> del MacroCompiler di XSharp (vedi nota
/// Obsidian <c>IlBackend-Patterns-from-XSharp.md</c>) — ma calata sui tipi di
/// questo backend, senza importare Roslyn né la semantica <c>__Usual</c>.
/// </summary>
public enum IlConversionKind
{
    /// <summary>Stesso tipo CLR: nessuna istruzione.</summary>
    Identity,

    /// <summary>Widening numerico senza perdita (byte→int, single→double): opcode <c>Conv.X</c>.</summary>
    NumericWiden,

    /// <summary>Narrowing/arrotondamento numerico via <c>System.Convert.ToXxx</c> (mai troncare).</summary>
    NumericConvert,

    /// <summary>Versione stringa locale-invariante (<see cref="StringHelpers"/> / <c>Convert.ToString</c>).</summary>
    ToString,

    /// <summary>Value type → reference type (<c>Box</c>).</summary>
    Box,

    /// <summary>Reference type → value type (<c>Unbox_Any</c>).</summary>
    UnboxNative,

    /// <summary>Cast tra reference type (<c>Castclass</c>).</summary>
    CastReference
}

/// <summary>
/// Centralizza la conversione/coerzione di tipo nel backend IL, così che i punti
/// dispersi di <c>Conv_I4/Conv_R8/Convert.ToXxx</c> e <c>EmitConvertToString</c> di
/// <see cref="IlProgramCompiler"/> abbiano un'unica implementazione (vedi
/// <c>IlProgramCompiler.EmitInt32Arg</c>, <c>EmitNumericConversionCall</c>,
/// <c>EmitConvertToString</c>).
///
/// Regole di progettazione (ereditate dal MacroCompiler X):
/// - le conversioni numeriche **narrowing** usano SEMPRE <c>System.Convert.ToXxx</c>
///   (arrotondano), mai l'opcode <c>Conv.X</c> (che tronca) — coerenti con
///   l'Executor/PCodeVm esistenti;
/// - le **widening** usano solo opcode <c>Conv.X</c> (nessuna perdita);
/// - le stringhe passano da <see cref="StringHelpers"/> per la locale invariante;
/// - ogni caso non coperto lancia esplicito, mai fallback a <c>object</c>.
/// </summary>
public static class IlConversion
{
    /// <summary>Contesto dell ultima istruzione emessa (riga:colonna e tipo), per diagnosi.</summary>
    public static string CurrentStatement = "";
    /// <summary>True se il <see cref="DataType"/> è un numerico scalare del backend.</summary>
    public static bool IsNumeric(DataType t) =>
        t is DoubleType or SingleType or LongType or ULongType or QuadType or UQuadType
            or IntegerType or UIntegerType or ByteType or SByteType or CurrencyType or BitType;

    /// <summary>
    /// Emette la conversione da un valore di tipo BASIC <paramref name="from"/> (il cui
    /// CLR sullo stack è <paramref name="fromClr"/>) al tipo CLR <paramref name="toClr"/>.
    /// Nessuna istruzione quando i due tipi CLR coincidono. Non supporta la conversione
    /// di un valore <see cref="object"/> già boxe: usa <see cref="EmitConvertBoxed"/>.
    /// </summary>
    public static void EmitConvert(
        ILGenerator il, IlTypeContext types, DataType from, Type fromClr, Type toClr)
    {
        if (fromClr == toClr) return; // Identity

        if (IsNumeric(from))
        {
            EmitNumeric(il, types, fromClr, toClr);
            return;
        }
        if (from is StringType)
        {
            // Stringa → stringa CLR: nessuna istruzione. Stringa → NUMERICO (es.
            // argomento stringa a builtin con parametro double): IlRuntime.ToDouble
            // fa il parse invariant (prima: nessuna istruzione → string sullo stack
            // dove serve double = InvalidProgramException a runtime).
            if (toClr == typeof(double))
            {
                il.Emit(OpCodes.Call, types.IlRuntimeToDoubleMethod);
                return;
            }
            if (toClr == typeof(float) || toClr == typeof(int) || toClr == typeof(uint)
                || toClr == typeof(long) || toClr == typeof(ulong) || toClr == typeof(short)
                || toClr == typeof(ushort) || toClr == typeof(byte) || toClr == typeof(sbyte)
                // decimal (CURRENCY: CCUR("12.5")): senza questo caso non si emetteva NULLA e
                // restava una stringa sullo stack dove serve un decimal → InvalidProgramException
                // ("Common Language Runtime detected an invalid program").
                || toClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.IlRuntimeToDoubleMethod);
                EmitNumeric(il, types, typeof(double), toClr);
                return;
            }
            return;
        }

        // DateTime → double (OA date, es. DATEDIFF/assegnazioni date): Convert.ToDouble
        // lancia InvalidCast — serve ToOADate(). Un `call`/`callvirt` diretto al metodo
        // d'istanza DateTime.ToOADate su un VALUETYPE TEMPORANEO è illegale nel verifier
        // ("Callvirt on a value type method" / "expected address") → delega al runtime
        // helper IlRuntime.ToOADate(DateTime) (call statica, compilata dal C#).
        if (fromClr == typeof(DateTime) && toClr == typeof(double))
        {
            il.Emit(OpCodes.Call, types.IlRuntimeToOADateMethod);
            return;
        }
        // TimeSpan → double (secondi, es. DATEDIFF su TimeSpan).
        if (fromClr == typeof(System.TimeSpan) && toClr == typeof(double))
        {
            il.Emit(OpCodes.Call, typeof(System.TimeSpan).GetMethod("get_TotalSeconds")!);
            return;
        }
        // Entrambi value type (es. lo stesso UDT struct) ma fromClr != toClr per identity:
        // capita quando toClr arriva da TypeBuilder.GetMethod(closedType, openMethod) — il
        // ponte richiesto per risolvere metodi su una collezione generica chiusa il cui
        // argomento è un TypeBuilder ancora in costruzione (vedi ResolveNativeCollectionMethod)
        // — il Type restituito per il parametro bridged non è mai `==` al Type ottenuto
        // altrove via DataTypeMapper per lo STESSO UDT, pur rappresentando lo stesso value
        // type. Boxare qui (il ramo sotto, prima di questo controllo) sarebbe sbagliato: il
        // parametro SUL TIPO CHIUSO REALE (es. List<Person>.Add(Person)) vuole il value type
        // diretto sullo stack, non un riferimento boxed — bug scoperto con
        // `customers.Add(c1)` su una LIST OF Person (UDT), InvalidProgramException a runtime.
        // Il type checker BASIC (Linker) garantisce già la compatibilità dell'argomento prima
        // di arrivare qui: se sono entrambi value type non-numerici/stringa, sono già lo
        // stesso struct — nessuna conversione IL necessaria.
        if (fromClr.IsValueType && toClr.IsValueType) return;

        // Non numerico: boxing/unboxing/cast per value-vs-reference.
        // Da object (variabile con tipo RIDEFINITO, es. S/S$): il boxed può essere di un
        // tipo diverso dal target (SINGLE, STRING, ...) — Unbox_Any diretto lancerebbe.
        // IlRuntime.ToDouble(object) gestisce qualunque boxed/stringa numerica.
        if (fromClr == typeof(object) && toClr.IsValueType && toClr != typeof(object))
        {
            if (toClr == typeof(double))
            {
                il.Emit(OpCodes.Call, types.IlRuntimeToDoubleMethod);
                return;
            }
            // Altri numerici: ToDouble poi conversione
            il.Emit(OpCodes.Call, types.IlRuntimeToDoubleMethod);
            EmitNumeric(il, types, typeof(double), toClr);
            return;
        }
        if (!fromClr.IsValueType && toClr.IsValueType) { il.Emit(OpCodes.Unbox_Any, toClr); return; }
        if (fromClr.IsValueType) { il.Emit(OpCodes.Box, fromClr); return; }
        il.Emit(OpCodes.Castclass, toClr);
    }

    /// <summary>
    /// Emette la conversione tra due tipi numerici. **Widening** → opcode <c>Conv.X</c>;
    /// **narrowing/arrotondamento** (double→int, double→long, ecc.) →
    /// <c>System.Convert.ToXxx</c> (mai troncare).
    /// </summary>
    private static void EmitNumeric(ILGenerator il, IlTypeContext types, Type fromClr, Type toClr)
    {
        // Identity (es. Int16→Int16 da una coerce ridondante su un literal già convertito):
        // nessuna istruzione. Prima Int16→Int16 cadeva in ToInt16Method(short) che NON ha un
        // case per short -> risolveva a Convert.ToInt16(double) con un Int16 nativo sullo stack
        // = stack type mismatch -> InvalidProgramException (ConstStructuralCanary/MATCH con
        // FUNCTION AS INTEGER).
        if (fromClr == toClr) return;
        // bool → numerico (es. il RETURN di un confronto `(n MOD 2) = 0` in una FUNCTION
        // AS INTEGER): il bool è un int32 0/1 — Convert.ToX(bool) non esiste e ToInt16(F8)
        // con un I4 sullo stack era IL invalido ("Expected F8, but got I4"). conv diretto.
        if (fromClr == typeof(bool))
        {
            if (toClr == typeof(bool)) return;
            // BASIC: il valore numerico di true è -1 (parità PCode/VM/C/C99). Un bool CLR è
            // un I4 0/1: * -1 → 0/-1, poi conversione al target (MUL, non NEG: il -1
            // numerico dei veri va prodotto qui, il letterale resta 1 nel mondo CLR).
            il.Emit(OpCodes.Ldc_I4_M1);
            il.Emit(OpCodes.Mul);
            // decimal (CURRENCY: CCUR(TRUE)) non ha opcode Conv: ConvertOpcode lanciava
            // "nessun opcode di conversione per 'Decimal'" e CCUR(TRUE) non compilava.
            // Stessa via degli integrali (ramo toClr == Decimal): conv.r8 + Convert.ToDecimal(double).
            if (toClr == typeof(decimal))
            {
                il.Emit(OpCodes.Conv_R8);
                il.Emit(OpCodes.Call, types.ConvertToDecimalDouble);
                return;
            }
            il.Emit(ConvertOpcode(toClr));
            return;
        }
        if (Widens(fromClr, toClr))
        {
            il.Emit(ConvertOpcode(toClr));
            return;
        }

        if (toClr == types.Int32)
        {
            // Da integrale già di larghezza <= Int32, la conversione è un semplice
            // conv.i4 (nessuna perdita, nessun arrotondamento) — comportamento
            // pregresso di EmitInt32Arg. Da floating usa Convert.ToInt32(double),
            // arrotondando (semantica Executor); un Single viene prima allargato a
            // double (conv.r8), mai chiamato Convert.ToInt32(single) che non è un
            // membro risolto nel "mondo" dei tipi di IlTypeContext.
            if (IsIntegral(fromClr)) il.Emit(OpCodes.Conv_I4);
            else if (fromClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.ConvertToInt32Decimal);
            }
            else
            {
                if (fromClr == typeof(float)) il.Emit(OpCodes.Conv_R8);
                il.Emit(OpCodes.Call, ToInt32Method(types, fromClr));
            }
            return;
        }
        if (toClr == types.Int64)
        {
            if (fromClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.ConvertToInt64Decimal);
            }
            else
            {
                il.Emit(OpCodes.Call, ToInt64Method(types, fromClr));
            }
            return;
        }
        if (toClr == types.Int16)
        {
            // Da integrale: conv.i2 UNCHECKED (wraparound, semantica P-Code/Executor che
            // tronca) — Convert.ToInt16(long) checked lancia OverflowException per valori
            // fuori range (es. `ExTimer(slot) = TI()` con TI() int64 grande su un
            // int16[]). Da decimal/float: Convert.ToInt16 (arrotonda, come prima).
            if (IsIntegral(fromClr)) il.Emit(OpCodes.Conv_I2);
            else if (fromClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.ConvertToInt16Decimal);
            }
            else
            {
                il.Emit(OpCodes.Call, ToInt16Method(types, fromClr));
            }
            return;
        }
        if (toClr == types.Byte)
        {
            // Clamp a [0,255] poi Conv_U1 — parità PCodeVM (Math.Clamp) e C99 (gmb_cbyte).
            // Conv_U1 puro MODULA (CByte(300)=44, CByte(-5)=251) e Convert.ToByte LANCIA
            // (OverflowException su 256). Il clamp satura: CByte(300)=255, CByte(-5)=0.
            // Conv_U1 dopo il clamp tronca la frazione verso zero (CByte(1.9)=1, come VM).
            if (fromClr != typeof(double))
                il.Emit(OpCodes.Conv_R8);
            il.Emit(OpCodes.Ldc_R8, 0.0);
            il.Emit(OpCodes.Ldc_R8, 255.0);
            il.Emit(OpCodes.Call, typeof(Math).GetMethod(nameof(Math.Clamp), [typeof(double), typeof(double), typeof(double)])!);
            il.Emit(OpCodes.Conv_U1);
            return;
        }
        if (toClr == types.Double)
        {
            if (fromClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.ConvertToDoubleDecimal);
            }
            else if (fromClr == typeof(DateTime))
            {
                il.Emit(OpCodes.Call, types.IlRuntimeToOADateMethod);
            }
            else
            {
                il.Emit(ConvertOpcode(typeof(double)));
            }
            return;
        }
        if (toClr == types.Single)
        {
            if (fromClr == typeof(decimal))
            {
                // decimal → double → float
                il.Emit(OpCodes.Call, types.ConvertToDoubleDecimal);
                il.Emit(OpCodes.Conv_R4);
            }
            else
            {
                il.Emit(ConvertOpcode(typeof(float)));
            }
            return;
        }
        if (toClr == types.Decimal)
        {
            // Narrowing/arrotondamento verso decimal via Convert.ToDecimal
            if (fromClr == typeof(double))
            {
                il.Emit(OpCodes.Call, types.ConvertToDecimalDouble);
            }
            else if (fromClr == typeof(float))
            {
                // float → double → decimal
                il.Emit(OpCodes.Conv_R8);
                il.Emit(OpCodes.Call, types.ConvertToDecimalDouble);
            }
            else if (IsIntegral(fromClr))
            {
                // Integral → decimal via widening (conv.r8) + Convert.ToDecimal(double)
                il.Emit(OpCodes.Conv_R8);
                il.Emit(OpCodes.Call, types.ConvertToDecimalDouble);
            }
            return;
        }
        if (toClr == types.DateTime)
        {
            // DateTime from double (OA date) or long (ticks)
            if (fromClr == typeof(double))
            {
                // Convert.ToDateTime(double) lancia InvalidCast su .NET Core → FromOADate.
                il.Emit(OpCodes.Call, types.IlRuntimeFromOADateMethod);
            }
            else if (fromClr == typeof(long))
            {
                // long ticks → DateTime via constructor
                var ctor = typeof(DateTime).GetConstructor([typeof(long)])!;
                il.Emit(OpCodes.Newobj, ctor);
            }
            return;
        }
        if (toClr == types.Double)
        {
            if (fromClr == typeof(decimal))
            {
                il.Emit(OpCodes.Call, types.ConvertToDoubleDecimal);
            }
            else if (fromClr == typeof(DateTime))
            {
                il.Emit(OpCodes.Call, types.IlRuntimeToOADateMethod);
            }
            else
            {
                il.Emit(ConvertOpcode(typeof(double)));
            }
            return;
        }
        if (toClr == types.Single)
        {
            if (fromClr == typeof(decimal))
            {
                // decimal → double → float
                il.Emit(OpCodes.Call, types.ConvertToDoubleDecimal);
                il.Emit(OpCodes.Conv_R4);
            }
            else
            {
                il.Emit(ConvertOpcode(typeof(float)));
            }
            return;
        }
        if (toClr == typeof(object))
        {
            // Numerico → object: box (es. lst.Add(10) con lst AS LIST → List<object>).
            il.Emit(OpCodes.Box, fromClr == typeof(decimal) ? typeof(decimal) : fromClr);
            return;
        }
        if (toClr == typeof(bool))
        {
            // Numerico → bool: conv a double poi IlRuntime.ToBool (0 = false, ≠0 = true).
            // Una call semplice (non una sequenza di Ceq) evita bug di ILOptim sui
            // confronti float (BinaryInst "Invalid operand types").
            if (fromClr == typeof(float)) il.Emit(OpCodes.Conv_R8);
            else if (fromClr == typeof(decimal)) il.Emit(OpCodes.Call, types.ConvertToDoubleDecimal);
            else if (fromClr != typeof(double)) il.Emit(ConvertOpcode(typeof(double)));
            il.Emit(OpCodes.Call, types.IlRuntimeToBoolMethod);
            return;
        }
        throw new NotSupportedException(
            $"Backend IL: conversione numerica da {fromClr.Name} a {toClr.Name} non supportata"
            + (string.IsNullOrEmpty(CurrentStatement) ? "" : $" @ {CurrentStatement}") + ".");
    }

    /// <summary>Vero se <c>from → to</c> è un widening numerico senza perdita (byte→int, float→double, ...).</summary>
    private static bool Widens(Type from, Type to)
    {
        if (to == typeof(double) && from == typeof(float)) return true;
        if (to == typeof(long) && IsIntegral(from)) return true;
        if (to == typeof(ulong) && (from == typeof(uint) || from == typeof(ushort) || from == typeof(byte))) return true;
        if (to == typeof(float) && IsSignedWidth(from)) return true;
        if (to == typeof(int) && (from == typeof(short) || from == typeof(byte) || from == typeof(sbyte))) return true;
        if (to == typeof(uint) && (from == typeof(ushort) || from == typeof(byte))) return true;
        return false;
    }

    private static bool IsIntegral(Type t) =>
        t == typeof(int) || t == typeof(long) || t == typeof(short) || t == typeof(byte) ||
        t == typeof(sbyte) || t == typeof(uint) || t == typeof(ushort);

    private static bool IsSignedWidth(Type t) => t == typeof(sbyte) || t == typeof(byte);

    private static OpCode ConvertOpcode(Type t)
    {
        if (t == typeof(int)) return OpCodes.Conv_I4;
        if (t == typeof(long)) return OpCodes.Conv_I8;
        if (t == typeof(float)) return OpCodes.Conv_R4;
        if (t == typeof(double)) return OpCodes.Conv_R8;
        if (t == typeof(ulong)) return OpCodes.Conv_U8;
        if (t == typeof(uint)) return OpCodes.Conv_U4;
        if (t == typeof(short)) return OpCodes.Conv_I2;
        if (t == typeof(ushort)) return OpCodes.Conv_U2;
        if (t == typeof(byte)) return OpCodes.Conv_U1;
        if (t == typeof(sbyte)) return OpCodes.Conv_I1;
        throw new NotSupportedException($"Backend IL: nessun opcode di conversione per '{t.Name}'.");
    }

    /// <summary>Metodo <c>Convert.ToInt32</c> corretto per il tipo sorgente (double → il più comune).</summary>
    private static MethodInfo ToInt32Method(IlTypeContext types, Type fromClr) =>
        fromClr == typeof(double) ? types.ConvertToInt32Double : typeof(Convert).GetMethod(nameof(Convert.ToInt32), [typeof(double)])!;

    private static MethodInfo ToInt64Method(IlTypeContext types, Type fromClr)
    {
        // Integrali: Convert.ToInt64 dall'overload corretto per il tipo sullo stack.
        // Floating: Convert.ToInt64(double). (L'>Int64 da Int32/Int16/Byte ecc. è già
        // coperto da Widens→Conv_I8 prima di arrivare qui; questo ramo serve i casi
        // narrowing/ugual-larghezza e i floating.)
        if (fromClr == typeof(short)) return typeof(Convert).GetMethod(nameof(Convert.ToInt64), [typeof(short)])!;
        if (fromClr == typeof(byte)) return typeof(Convert).GetMethod(nameof(Convert.ToInt64), [typeof(byte)])!;
        if (fromClr == typeof(int)) return typeof(Convert).GetMethod(nameof(Convert.ToInt64), [typeof(int)])!;
        return types.ConvertToInt64Double;
    }

    private static MethodInfo ToInt16Method(IlTypeContext types, Type fromClr)
    {
        // Integralo: Convert.ToInt16 dall'overload corretto. Double: converte (arrotonda).
        // Single non è un membro risolto in IlTypeContext — deve già essere double
        // (Conv_R8 emessa dal chiamante) se passa da qui.
        // NB: gli integrali MINORI di int (short/ushort/sbyte/byte...) hanno un overload
        // Convert.ToInt16 dedicato — senza, un valore short già sullo stack (coerce
        // ridondante, es. un literal coerce due volte in un MATCH con FUNCTION AS INTEGER)
        // cadeva sul default Convert.ToInt16(double) con un Int16 nativo = stack type
        // mismatch -> InvalidProgramException (ConstStructuralCanary).
        if (fromClr == typeof(long)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(long)])!;
        if (fromClr == typeof(int)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(int)])!;
        if (fromClr == typeof(short)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(short)])!;
        if (fromClr == typeof(ushort)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(ushort)])!;
        if (fromClr == typeof(sbyte)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(sbyte)])!;
        if (fromClr == typeof(byte)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(byte)])!;
        if (fromClr == typeof(uint)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(uint)])!;
        if (fromClr == typeof(ulong)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(ulong)])!;
        if (fromClr == typeof(char)) return typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(char)])!;
        return types.ConvertToInt16Double;
    }

    /// <summary>Emette la conversione a <see cref="string"/> di un valore sullo stack — locale-invariant.</summary>
    public static void EmitToString(ILGenerator il, IlTypeContext types, DataType from)
    {
        if (from is StringType) return; // già stringa, nessuna conversione

        var method = from switch
        {
            DoubleType => StringHelpersDoubleToInvariantStringMethod,
            SingleType => StringHelpersSingleToInvariantStringMethod,
            LongType or QuadType => types.ConvertToStringInt64,
            UQuadType => types.ConvertToStringUInt64,
            IntegerType => types.ConvertToStringInt16,
            UIntegerType => types.ConvertToStringUInt16,
            ByteType => types.ConvertToStringByte,
            SByteType => types.ConvertToStringSByte,
            BitType => types.ConvertToStringBoolean,
            ULongType => types.ConvertToStringUInt64,
            CurrencyType => StringHelpersDecimalToInvariantStringMethod,
            DateType or TimeType or DateTimeType => types.ConvertToStringDateTime,
            // JSON: tipo statico sempre object (DataTypeMapper, JsonType → System.Object — un
            // valore JSON è dinamico, forma nota solo a runtime), quindi la conversione non può
            // scegliersi qui a compile-time come per gli scalari — delega a IlRuntime, dispatch
            // sul tipo REALE del valore boxed.
            // JSON/ANY/tipi dinamici: tipo statico sempre object (DataTypeMapper → Object) —
            // la conversione delega a IlRuntime, dispatch sul tipo REALE del valore boxed.
            Basic.JsonType or AnyType or HttpRequestType or HttpResponseType or HttpOptionsType
                or HttpHeadersType or HttpConfigType or HttpPostedDataType or SqlResultSetType
                or WebSocketType or NullableType or UnresolvedQualifiedType or ArrayType or TypeParameter or NilType => IlRuntimeJsonValueToStringMethod,
            // Collezioni (LIST/DICT/HASHSET/QUEUE/STACK/...): PRINT diretto di una lista
            // lanciava "tipo 'ListType' non mappato" — rappresentazione "[a, b, c]"/"{k: v}".
            ListType or DictType or SortedDictType or HashSetType or QueueType or StackType
                or LinkedListType or StringBuilderType or ChannelType or WaitGroupType or RedBlackTreeType
                or TreeType or AVLTreeType or BinaryIndexedTreeType or TrieType or BPlusTreeType
                or GraphType or HashRingType or SparseSetType or BinaryHeapType or BagType
                or CacheType or TimelineType or RingBufferType or TupleType
                or MutexType or RwMutexType or OnceType or AtomicType or PoolType or CondType or ConcurrentMapType => IlRuntimeCollectionToStringMethod,
            _ => throw new NotSupportedIlDataTypeException(from)
        };
        il.Emit(OpCodes.Call, method);
    }

    private static readonly MethodInfo StringHelpersDoubleToInvariantStringMethod =
        typeof(StringHelpers).GetMethod(nameof(StringHelpers.DoubleToInvariantString), [typeof(double)])!;
    private static readonly MethodInfo StringHelpersSingleToInvariantStringMethod =
        typeof(StringHelpers).GetMethod(nameof(StringHelpers.SingleToInvariantString), [typeof(float)])!;
    private static readonly MethodInfo StringHelpersDecimalToInvariantStringMethod =
        typeof(StringHelpers).GetMethod(nameof(StringHelpers.DecimalToInvariantString), [typeof(decimal)])!;
    private static readonly MethodInfo IlRuntimeJsonValueToStringMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.JsonValueToString))!;
    private static readonly MethodInfo IlRuntimeCollectionToStringMethod =
        typeof(IlRuntime).GetMethod(nameof(IlRuntime.CollectionToString))!;
}
