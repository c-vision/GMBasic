// FILE: Basic.IlBackend.IlTypeContext.cs

using System.Linq;
using System.Reflection;
using System.Reflection.Emit;

namespace Basic.IlBackend;

/// <summary>
/// Raccoglie i tipi CLR scalari usati dall'emettitore (<see cref="IlProgramCompiler"/>,
/// <see cref="DataTypeMapper"/>) risolti tutti dallo STESSO "mondo" di assembly.
///
/// Esiste perché <see cref="System.Reflection.Emit.PersistedAssemblyBuilder"/> decide se
/// usare la codifica primitiva CLI compatta (element-type built-in, es. ELEMENT_TYPE_VOID)
/// o un TypeRef esplicito confrontando l'identità di ogni <see cref="Type"/> passato con il
/// proprio <c>coreAssembly</c> registrato al costruttore — NON basandosi sul fatto che il
/// tipo sia "concettualmente" System.Void/Object/ecc. ovunque provenga. Passare
/// <c>typeof(void)</c> (di RUNTIME, cioè System.Private.CoreLib) mentre il builder è stato
/// costruito con un <c>coreAssembly</c> di RIFERIMENTO (System.Runtime, per un .dll pensato
/// per essere referenziato da un'altra compilazione, es. un <c>dotnet publish
/// -p:PublishAot=true</c> su un progetto host) fa sì che il builder non lo riconosca e
/// scriva un TypeRef esplicito a System.Private.CoreLib — che il progetto consumer, che
/// compila contro System.Runtime.dll, non riesce a risolvere
/// ("CS0012: tipo definito in un assembly di cui manca il riferimento").
///
/// Due fabbriche:
/// - <see cref="ForRuntime"/>: tipi di RUNTIME (<c>typeof(X)</c> diretti) — corretti per il
///   caricamento in-process (<c>Assembly.Load</c>) nello stesso processo che genera
///   l'assembly (il percorso veloce usato dal loop di test principale).
/// - <see cref="ForReferenceAssemblies"/>: tipi risolti da un
///   <see cref="System.Reflection.MetadataLoadContext"/> puntato agli assembly di
///   riferimento del SDK — corretti quando il .dll generato deve essere referenziato da
///   un'ALTRA compilazione (es. la prova end-to-end fino a un eseguibile nativo).
/// </summary>
public sealed class IlTypeContext
{
    /// <summary>Modulo dell'assembly in compilazione (null in <see cref="ForRuntime"/>).
    /// Serve a definire nello STESSO modulo i tipi che l'IL emesso deve referenziare —
    /// oggi i delegate con arità &gt; 16 (limite del BCL): un tipo di un assembly dinamico
    /// "Run" NON è risolvibile dal .dll salvato (PersistedAssemblyBuilder).</summary>
    public ModuleBuilder? ModuleBuilder { get; set; }

    /// <summary>TypeBuilder di delegate generati durante la compilazione, da
    /// <c>CreateType()</c>-are alla fine (dopo i tipi utente, che possono comparire nei
    /// loro parametri), con costruttore e Invoke già risolti (su un TypeBuilder non ancora
    /// creato <c>GetConstructor</c>/<c>GetMethod</c> lanciano).</summary>
    public List<TypeBuilder>? PendingDelegateTypes { get; set; }

    /// <summary>Membri dei delegate di cui sopra, per tipo (chiave = il TypeBuilder).</summary>
    public Dictionary<Type, (ConstructorInfo Ctor, MethodInfo Invoke)>? DynamicDelegateMembers { get; set; }

    /// <summary>Copia shallow del contesto legata a un modulo di compilazione.</summary>
    public IlTypeContext WithModule(ModuleBuilder module)
    {
        var copy = (IlTypeContext)MemberwiseClone();
        copy.ModuleBuilder = module;
        copy.PendingDelegateTypes ??= new List<TypeBuilder>();
        copy.DynamicDelegateMembers ??= new Dictionary<Type, (ConstructorInfo, MethodInfo)>();
        return copy;
    }

    public required Type Object { get; init; }
    public required Type ValueType { get; init; }
    public required Type Void { get; init; }
    public required Type Double { get; init; }
    public required Type Single { get; init; }
    public required Type Int64 { get; init; }
    public required Type UInt64 { get; init; }
    public required Type Int32 { get; init; }
    public required Type Int16 { get; init; }
    public required Type UInt16 { get; init; }
    public required Type Byte { get; init; }
    public required Type SByte { get; init; }
    public required Type Boolean { get; init; }
    public required Type String { get; init; }
    public required Type Decimal { get; init; }
    public required Type DateTime { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(double), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToStringDouble { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt32(double) (arrotonda, non tronca — stessa semantica usata dall'Executor esistente per gli argomenti interi delle funzioni builtin), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt32Double { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(long) (es. per PRINT di LEN, che restituisce Long), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToStringInt64 { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(float), risolto dallo stesso "mondo" (per PRINT di variabili/array Single).</summary>
    public required MethodInfo ConvertToStringSingle { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(short), risolto dallo stesso "mondo" (per PRINT di variabili/array Integer).</summary>
    public required MethodInfo ConvertToStringInt16 { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(ushort), risolto dallo stesso "mondo" (per PRINT di variabili/array UInteger).</summary>
    public required MethodInfo ConvertToStringUInt16 { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(byte), risolto dallo stesso "mondo" (per PRINT di variabili/array Byte).</summary>
    public required MethodInfo ConvertToStringByte { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(sbyte), risolto dallo stesso "mondo" (per PRINT di variabili/array SByte).</summary>
    public required MethodInfo ConvertToStringSByte { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(bool), risolto dallo stesso "mondo" (per PRINT di variabili/array Bit/Boolean).</summary>
    public required MethodInfo ConvertToStringBoolean { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(ulong), risolto dallo stesso "mondo" (per PRINT di variabili/array ULong).</summary>
    public required MethodInfo ConvertToStringUInt64 { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(decimal), risolto dallo stesso "mondo" (per PRINT di variabili/array Currency).</summary>
    public required MethodInfo ConvertToStringDecimal { get; init; }

    /// <summary>MethodInfo di System.Convert.ToString(DateTime), risolto dallo stesso "mondo" (per PRINT di variabili/array Date).</summary>
    public required MethodInfo ConvertToStringDateTime { get; init; }

    /// <summary>MethodInfo di System.Convert.ToDecimal(double), risolto dallo stesso "mondo" (per conversioni Double→Currency).</summary>
    public required MethodInfo ConvertToDecimalDouble { get; init; }

    /// <summary>MethodInfo di System.Convert.ToDouble(decimal), risolto dallo stesso "mondo" (per conversioni Currency→Double).</summary>
    public required MethodInfo ConvertToDoubleDecimal { get; init; }

    /// <summary>MethodInfo di System.Convert.ToDateTime(double) — NON usato direttamente; le conversioni Date passano da string parsing o long ticks.</summary>
    public required MethodInfo ConvertToDateTimeDouble { get; init; }

    /// <summary>MethodInfo di System.Convert.ToDouble(DateTime) — per DateTime→Double (OA date).</summary>
    public required MethodInfo ConvertToDoubleDateTime { get; init; }
    /// <summary>IlRuntime.ToDouble(object) — converte un boxed/stringa in double (variabili con tipo ridefinito S/S$).</summary>
    public required MethodInfo IlRuntimeToDoubleMethod { get; init; }
    /// <summary>IlRuntime.ToOADate(DateTime) — DateTime→double OA date (Convert.ToDouble lancia InvalidCast).</summary>
    public required MethodInfo IlRuntimeToOADateMethod { get; init; }
    /// <summary>IlRuntime.FromOADate(double) — double OA date → DateTime (Convert.ToDateTime(double) lancia su .NET Core).</summary>
    public required MethodInfo IlRuntimeFromOADateMethod { get; init; }
    public required MethodInfo IlRuntimeArrayElementAtMethod { get; init; }
    public required MethodInfo IlRuntimeToBoolMethod { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt32(decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt32Decimal { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt64(decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt64Decimal { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt16(decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt16Decimal { get; init; }

    /// <summary>MethodInfo di System.Decimal.op_Addition(decimal, decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo DecimalAdd { get; init; }

    /// <summary>MethodInfo di System.Decimal.op_Subtraction(decimal, decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo DecimalSubtract { get; init; }

    /// <summary>MethodInfo di System.Decimal.op_Multiply(decimal, decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo DecimalMultiply { get; init; }

    /// <summary>MethodInfo di System.Decimal.op_Division(decimal, decimal), risolto dallo stesso "mondo".</summary>
    public required MethodInfo DecimalDivide { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt64(double) (per CLNG, che il Linker inserisce implicitamente sugli argomenti interi letterali delle funzioni builtin — es. LEFT$/RIGHT$/INSTR), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt64Double { get; init; }

    /// <summary>MethodInfo di System.Convert.ToInt16(double) (per CINT, inserito implicitamente dal Linker sugli argomenti letterali assegnati a variabili/array di tipo Integer), risolto dallo stesso "mondo".</summary>
    public required MethodInfo ConvertToInt16Double { get; init; }

    /// <summary>MethodInfo di System.String.Concat(string, string), risolto dallo stesso "mondo".</summary>
    public required MethodInfo StringConcat { get; init; }

    /// <summary>MethodInfo di System.String.Equals(string, string) (ordinale), risolto dallo stesso "mondo".</summary>
    public required MethodInfo StringEquals { get; init; }

    /// <summary>MethodInfo di System.String.CompareOrdinal(string, string), risolto dallo stesso "mondo".</summary>
    public required MethodInfo StringCompareOrdinal { get; init; }

    /// <summary>Getter di System.String.Length, risolto dallo stesso "mondo" (per LEN).</summary>
    public required MethodInfo StringLengthGetter { get; init; }

    /// <summary>MethodInfo di System.String.ToUpperInvariant(), risolto dallo stesso "mondo" (per UPPER$).</summary>
    public required MethodInfo StringToUpperInvariant { get; init; }

    /// <summary>MethodInfo di System.String.ToLowerInvariant(), risolto dallo stesso "mondo" (per LOWER$).</summary>
    public required MethodInfo StringToLowerInvariant { get; init; }

    /// <summary>MethodInfo di System.String.Trim(), risolto dallo stesso "mondo" (per TRIM$).</summary>
    public required MethodInfo StringTrim { get; init; }

    /// <summary>MethodInfo di System.String.TrimStart(), risolto dallo stesso "mondo" (per LTRIM$).</summary>
    public required MethodInfo StringTrimStart { get; init; }

    /// <summary>MethodInfo di System.String.TrimEnd(), risolto dallo stesso "mondo" (per RTRIM$).</summary>
    public required MethodInfo StringTrimEnd { get; init; }

    /// <summary>
    /// Definizione generica APERTA di System.Array.Resize&lt;T&gt;(ref T[], int), risolta
    /// dallo stesso "mondo" (per REDIM PRESERVE) — chiusa via MakeGenericMethod sul tipo
    /// elemento reale al call site, mai su object (nessun boxing per i tipi valore).
    /// </summary>
    public required MethodInfo ArrayResizeOpenGeneric { get; init; }

    /// <summary>
    /// System.Array.Copy(Array, int sourceIndex, Array, int destinationIndex, int length),
    /// risolto dallo stesso "mondo" — usato da AINS/ADEL (spostare un blocco di elementi di
    /// un offset dentro un nuovo array più grande/piccolo) e da ACOPY (copiare un blocco tra
    /// due array indipendenti con offset/lunghezza arbitrari).
    /// </summary>
    public required MethodInfo ArrayCopyWithOffsets { get; init; }

    /// <summary>
    /// System.Array.GetUpperBound(int dimension), risolto dallo stesso "mondo" (per
    /// UBOUND) — chiamato per-istanza (Callvirt) su qualunque array CLR reale, rank 1 o
    /// rettangolare, con l'indice di dimensione (0-based) sullo stack. Solo UBOUND ne ha
    /// bisogno: gli array di questo backend sono SEMPRE CLR 0-based (mai
    /// Array.CreateInstance con lowerBounds custom, vedi EmitArrayDeclaration), quindi
    /// LBOUND e' interamente noto a compile-time (ArrayInfo.LowerBounds) — nessuna
    /// chiamata runtime necessaria per quello, vedi EmitLBoundCall.
    /// </summary>
    public required MethodInfo ArrayGetUpperBound { get; init; }

    /// <summary>
    /// Definizioni generiche APERTE di System.Func&lt;...,TResult&gt; per arità 0..8
    /// (indice = numero di parametri, es. FuncOpenGenericsByArity[0] = Func&lt;TResult&gt;,
    /// [2] = Func&lt;T1,T2,TResult&gt;), risolte dallo stesso "mondo" — usate per mappare
    /// un CALLBACK/FUNCTION() con tipo di ritorno non-VOID a un vero delegate CLR (vedi
    /// DataTypeMapper, FunctionPointerType). Chiuse via MakeGenericType sui tipi
    /// parametro+ritorno reali al call site.
    /// </summary>
    public required Type[] FuncOpenGenericsByArity { get; init; }

    /// <summary>
    /// Come <see cref="FuncOpenGenericsByArity"/> ma per System.Action/System.Action&lt;...&gt;
    /// (arità 0..8) — usate per un CALLBACK/SUB() con tipo di ritorno VOID.
    /// </summary>
    public required Type[] ActionOpenGenericsByArity { get; init; }

    // --- TRY/CATCH/THROW/ERROR — vedi IlProgramCompiler, sezione TRY/CATCH ---
    // A differenza degli scalari sopra, questi tipi NON vengono dal BCL ma dall'assembly
    // Ast (Basic.BasicException/Basic.ErrInstance): non soggetti al problema di identità
    // CoreLib-vs-Reference-assembly che giustifica questa classe (Ast.dll non è parte
    // dello shared framework), ma vanno comunque risolti dallo stesso "mondo" per
    // coerenza — ForReferenceAssemblies li carica nello stesso MetadataLoadContext via
    // LoadFromAssemblyPath, non li mescola con tipi caricati a runtime.

    /// <summary>Basic.BasicException — unico tipo CLR catturato da un blocco CATCH generato da questo backend (vedi EmitTryCatchStatement): ogni THROW/ERROR BASIC vi si incapsula, il dispatch per tipo/ERR avviene POI dentro il singolo blocco catch.</summary>
    public required Type BasicExceptionType { get; init; }

    /// <summary>Costruttore BasicException(ErrorCode, SourcePosition, string?, SourceLine?, Exception?) — i 3 parametri opzionali vanno comunque passati esplicitamente in IL (i default sono zucchero solo lato C#).</summary>
    public required ConstructorInfo BasicExceptionConstructor { get; init; }

    /// <summary>BasicException.FromClrException(Exception, SourcePosition) — avvolge un'eccezione CLR nativa (IndexOutOfRangeException/FileNotFoundException/...) in una BasicException con l'ErrorCode più vicino, usato dal secondo catch(Exception) dei blocchi ON ERROR/TRY-CATCH.</summary>
    public required MethodInfo BasicExceptionFromClrExceptionMethod { get; init; }

    /// <summary>Getter di BasicException.Code (ErrorCode, enum Int32).</summary>
    public required MethodInfo BasicExceptionCodeGetter { get; init; }

    /// <summary>Getter/setter di BasicException.UserExceptionInstance (object?) — dove finisce il valore di un THROW di un'istanza CLASS utente (mai un ErrorCode grezzo).</summary>
    public required MethodInfo BasicExceptionUserExceptionInstanceGetter { get; init; }
    public required MethodInfo BasicExceptionUserExceptionInstanceSetter { get; init; }

    /// <summary>System.Exception — base di BasicException, per Exception.Message (ereditato, mai ridichiarato).</summary>
    public required Type ExceptionType { get; init; }
    public required MethodInfo ExceptionMessageGetter { get; init; }

    /// <summary>Basic.ErrInstance — a cosa si lega una variabile CATCH ex AS ERR (record con Number/Description/LineNumber/SourceLineText/StackTrace).</summary>
    public required Type ErrInstanceType { get; init; }
    public required ConstructorInfo ErrInstanceConstructor { get; init; }
    public required MethodInfo ErrInstanceNumberGetter { get; init; }
    public required MethodInfo ErrInstanceDescriptionGetter { get; init; }
    public required MethodInfo ErrInstanceLineNumberGetter { get; init; }
    public required MethodInfo ErrInstanceSourceLineTextGetter { get; init; }

    /// <summary>Basic.SourcePosition — struct (FileId, Line, Column), embeddato per valore a ogni THROW/ERROR/costruzione di BasicException (posizione nota a compile-time, dal Token dello statement).</summary>
    public required ConstructorInfo SourcePositionConstructor { get; init; }

    public static IlTypeContext ForRuntime() => new()
    {
        Object = typeof(object),
        ValueType = typeof(ValueType),
        Void = typeof(void),
        Double = typeof(double),
        Single = typeof(float),
        Int64 = typeof(long),
        UInt64 = typeof(ulong),
        Int32 = typeof(int),
        Int16 = typeof(short),
        UInt16 = typeof(ushort),
        Byte = typeof(byte),
        SByte = typeof(sbyte),
        Boolean = typeof(bool),
        String = typeof(string),
        Decimal = typeof(decimal),
        DateTime = typeof(DateTime),
        ConvertToStringDouble = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(double)])!,
        ConvertToInt32Double = typeof(Convert).GetMethod(nameof(Convert.ToInt32), [typeof(double)])!,
        ConvertToStringInt64 = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(long)])!,
        ConvertToStringSingle = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(float)])!,
        ConvertToStringInt16 = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(short)])!,
        ConvertToStringUInt16 = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(ushort)])!,
        ConvertToStringByte = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(byte)])!,
        ConvertToStringSByte = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(sbyte)])!,
        ConvertToStringBoolean = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(bool)])!,
        ConvertToStringUInt64 = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(ulong)])!,
        ConvertToStringDecimal = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(decimal)])!,
        ConvertToStringDateTime = typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(DateTime)])!,
        ConvertToInt64Double = typeof(Convert).GetMethod(nameof(Convert.ToInt64), [typeof(double)])!,
        ConvertToInt16Double = typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(double)])!,
        ConvertToInt32Decimal = typeof(Convert).GetMethod(nameof(Convert.ToInt32), [typeof(decimal)])!,
        ConvertToInt64Decimal = typeof(Convert).GetMethod(nameof(Convert.ToInt64), [typeof(decimal)])!,
        ConvertToInt16Decimal = typeof(Convert).GetMethod(nameof(Convert.ToInt16), [typeof(decimal)])!,
        ConvertToDecimalDouble = typeof(Convert).GetMethod(nameof(Convert.ToDecimal), [typeof(double)])!,
        ConvertToDoubleDecimal = typeof(Convert).GetMethod(nameof(Convert.ToDouble), [typeof(decimal)])!,
        ConvertToDateTimeDouble = typeof(Convert).GetMethod(nameof(Convert.ToDateTime), [typeof(double)])!,
        ConvertToDoubleDateTime = typeof(Convert).GetMethod(nameof(Convert.ToDouble), [typeof(DateTime)])!,
        IlRuntimeToDoubleMethod = typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.ToDouble))!,
        IlRuntimeToOADateMethod = typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.ToOADate))!,
        IlRuntimeFromOADateMethod = typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.FromOADate))!,
        IlRuntimeArrayElementAtMethod = typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.ArrayElementAt))!,
        IlRuntimeToBoolMethod = typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.ToBool))!,
        DecimalAdd = typeof(decimal).GetMethod("op_Addition", [typeof(decimal), typeof(decimal)])!,
        DecimalSubtract = typeof(decimal).GetMethod("op_Subtraction", [typeof(decimal), typeof(decimal)])!,
        DecimalMultiply = typeof(decimal).GetMethod("op_Multiply", [typeof(decimal), typeof(decimal)])!,
        DecimalDivide = typeof(decimal).GetMethod("op_Division", [typeof(decimal), typeof(decimal)])!,
        StringConcat = typeof(string).GetMethod(nameof(string.Concat), [typeof(string), typeof(string)])!,
        StringEquals = typeof(string).GetMethod(nameof(string.Equals), [typeof(string), typeof(string)])!,
        StringCompareOrdinal = typeof(string).GetMethod(nameof(string.CompareOrdinal), [typeof(string), typeof(string)])!,
        StringLengthGetter = typeof(string).GetProperty(nameof(string.Length))!.GetGetMethod()!,
        StringToUpperInvariant = typeof(string).GetMethod(nameof(string.ToUpperInvariant), Type.EmptyTypes)!,
        StringToLowerInvariant = typeof(string).GetMethod(nameof(string.ToLowerInvariant), Type.EmptyTypes)!,
        StringTrim = typeof(string).GetMethod(nameof(string.Trim), Type.EmptyTypes)!,
        StringTrimStart = typeof(string).GetMethod(nameof(string.TrimStart), Type.EmptyTypes)!,
        StringTrimEnd = typeof(string).GetMethod(nameof(string.TrimEnd), Type.EmptyTypes)!,
        ArrayResizeOpenGeneric = typeof(Array).GetMethods()
            .Single(m => m.Name == nameof(Array.Resize) && m.IsGenericMethodDefinition),
        ArrayCopyWithOffsets = typeof(Array).GetMethod(nameof(Array.Copy),
            [typeof(Array), typeof(int), typeof(Array), typeof(int), typeof(int)])!,
        ArrayGetUpperBound = typeof(Array).GetMethod(nameof(Array.GetUpperBound), [typeof(int)])!,
        // Func`N / Action`N risolti PER NOME (2026-11-11): l'indice dell'array è l'ARITÀ (0
        // parametri → Func`1 = Func<TResult>; N parametri → Func`(N+1)). Scritti a mano si
        // contano le virgole — un errore produce un open generic con l'arità sbagliata SENZA
        // errore di compilazione. Fino a 16 parametri (limite di System.Func/Action).
        FuncOpenGenericsByArity = Enumerable.Range(1, 17)
            .Select(i => Type.GetType($"System.Func`{i}")!).ToArray(),
        ActionOpenGenericsByArity = Enumerable.Range(0, 17)
            .Select(i => i == 0 ? typeof(Action) : Type.GetType($"System.Action`{i}")!).ToArray(),
        BasicExceptionType = typeof(BasicException),
        BasicExceptionConstructor = typeof(BasicException).GetConstructor(
            [typeof(ErrorCode), typeof(SourcePosition), typeof(string), typeof(SourceLine), typeof(Exception)])!,
        BasicExceptionFromClrExceptionMethod = typeof(BasicException).GetMethod(nameof(BasicException.FromClrException))!,
        BasicExceptionCodeGetter = typeof(BasicException).GetProperty(nameof(BasicException.Code))!.GetGetMethod()!,
        BasicExceptionUserExceptionInstanceGetter = typeof(BasicException).GetProperty(nameof(BasicException.UserExceptionInstance))!.GetGetMethod()!,
        BasicExceptionUserExceptionInstanceSetter = typeof(BasicException).GetProperty(nameof(BasicException.UserExceptionInstance))!.GetSetMethod()!,
        ExceptionType = typeof(Exception),
        ExceptionMessageGetter = typeof(Exception).GetProperty(nameof(Exception.Message))!.GetGetMethod()!,
        ErrInstanceType = typeof(ErrInstance),
        ErrInstanceConstructor = typeof(ErrInstance).GetConstructor(
            [typeof(int), typeof(string), typeof(int), typeof(string), typeof(string)])!,
        ErrInstanceNumberGetter = typeof(ErrInstance).GetProperty(nameof(ErrInstance.Number))!.GetGetMethod()!,
        ErrInstanceDescriptionGetter = typeof(ErrInstance).GetProperty(nameof(ErrInstance.Description))!.GetGetMethod()!,
        ErrInstanceLineNumberGetter = typeof(ErrInstance).GetProperty(nameof(ErrInstance.LineNumber))!.GetGetMethod()!,
        ErrInstanceSourceLineTextGetter = typeof(ErrInstance).GetProperty(nameof(ErrInstance.SourceLineText))!.GetGetMethod()!,
        SourcePositionConstructor = typeof(SourcePosition).GetConstructor([typeof(int), typeof(int), typeof(int)])!
    };

    /// <summary>
    /// Risolve i tipi scalari e i metodi corelib usati dall'emettitore dallo stesso
    /// <paramref name="referenceAssemblies"/> (tipicamente un
    /// <see cref="System.Reflection.MetadataLoadContext"/> puntato al pacchetto
    /// Microsoft.NETCore.App.Ref del SDK — vedi <see cref="IlProgramCompiler"/> per il
    /// contesto completo del problema che questo risolve).
    /// </summary>
    public static IlTypeContext ForReferenceAssemblies(MetadataLoadContext referenceAssemblies)
    {
        var coreAssembly = referenceAssemblies.CoreAssembly
            ?? throw new InvalidOperationException("MetadataLoadContext.CoreAssembly non risolto.");

        Type Resolve(string fullName) => coreAssembly.GetType(fullName)
            ?? throw new InvalidOperationException($"Tipo '{fullName}' non trovato nell'assembly di riferimento '{coreAssembly.FullName}'.");

        var stringType = Resolve("System.String");
        var doubleType = Resolve("System.Double");
        var int64Type = Resolve("System.Int64");
        var singleType = Resolve("System.Single");
        var int16Type = Resolve("System.Int16");
        var uint16Type = Resolve("System.UInt16");
        var byteType = Resolve("System.Byte");
        var sbyteType = Resolve("System.SByte");
        var booleanType = Resolve("System.Boolean");
        var uint64Type = Resolve("System.UInt64");
        var convertType = coreAssembly.GetType("System.Convert")
            ?? referenceAssemblies.LoadFromAssemblyName("System.Runtime").GetType("System.Convert")
            ?? throw new InvalidOperationException("Tipo 'System.Convert' non trovato negli assembly di riferimento.");
        var decimalType = Resolve("System.Decimal");
        var dateTimeType = Resolve("System.DateTime");
        var arrayType = coreAssembly.GetType("System.Array")
            ?? referenceAssemblies.LoadFromAssemblyName("System.Runtime").GetType("System.Array")
            ?? throw new InvalidOperationException("Tipo 'System.Array' non trovato negli assembly di riferimento.");

        Type ResolveOpenGeneric(string metadataName) =>
            coreAssembly.GetType(metadataName)
            ?? referenceAssemblies.LoadFromAssemblyName("System.Runtime").GetType(metadataName)
            ?? throw new InvalidOperationException($"Tipo '{metadataName}' non trovato negli assembly di riferimento.");

        // Nomi metadata CLR per arità: System.Func`1 = Func<TResult>, `2 = Func<T1,TResult>,
        // ecc.; System.Action (nessun suffisso) = arità 0, `1 = Action<T1>, ecc.
        var funcOpenGenerics = new Type[9];
        var actionOpenGenerics = new Type[9];
        for (var arity = 0; arity <= 8; arity++)
        {
            funcOpenGenerics[arity] = ResolveOpenGeneric($"System.Func`{arity + 1}");
            actionOpenGenerics[arity] = arity == 0 ? ResolveOpenGeneric("System.Action") : ResolveOpenGeneric($"System.Action`{arity}");
        }

        var exceptionType = Resolve("System.Exception");

        // Basic.BasicException/Basic.ErrInstance NON sono nel corelib/reference pack (vivono
        // in Ast.dll, un assembly di PROGETTO) — vanno caricati esplicitamente nello STESSO
        // MetadataLoadContext (LoadFromAssemblyPath, dalla posizione fisica dell'assembly
        // caricato a runtime in QUESTO processo) per condividere il "mondo" di identità di
        // tutto il resto qui risolto, invece di mescolare un tipo di runtime (typeof(...))
        // con tipi di riferimento — esattamente l'errore che questa classe esiste per evitare.
        var astAssembly = referenceAssemblies.LoadFromAssemblyPath(typeof(BasicException).Assembly.Location);
        var ilBackendAssembly = referenceAssemblies.LoadFromAssemblyPath(typeof(Basic.IlBackend.IlRuntime).Assembly.Location);
        var basicExceptionType = astAssembly.GetType(typeof(BasicException).FullName!)
            ?? throw new InvalidOperationException($"Tipo '{typeof(BasicException).FullName}' non trovato in '{astAssembly.FullName}'.");
        var errInstanceType = astAssembly.GetType(typeof(ErrInstance).FullName!)
            ?? throw new InvalidOperationException($"Tipo '{typeof(ErrInstance).FullName}' non trovato in '{astAssembly.FullName}'.");
        var errorCodeType = astAssembly.GetType(typeof(ErrorCode).FullName!)
            ?? throw new InvalidOperationException($"Tipo '{typeof(ErrorCode).FullName}' non trovato in '{astAssembly.FullName}'.");
        var sourcePositionType = astAssembly.GetType(typeof(SourcePosition).FullName!)
            ?? throw new InvalidOperationException($"Tipo '{typeof(SourcePosition).FullName}' non trovato in '{astAssembly.FullName}'.");
        var sourceLineType = astAssembly.GetType(typeof(SourceLine).FullName!)
            ?? throw new InvalidOperationException($"Tipo '{typeof(SourceLine).FullName}' non trovato in '{astAssembly.FullName}'.");

        return new IlTypeContext
        {
            Object = Resolve("System.Object"),
            ValueType = Resolve("System.ValueType"),
            Void = Resolve("System.Void"),
            Double = doubleType,
            Single = singleType,
            Int64 = int64Type,
            UInt64 = uint64Type,
            Int32 = Resolve("System.Int32"),
            Int16 = int16Type,
            UInt16 = uint16Type,
            Byte = byteType,
            SByte = sbyteType,
            Boolean = booleanType,
            String = stringType,
            Decimal = decimalType,
            DateTime = dateTimeType,
            ConvertToStringDouble = convertType.GetMethod("ToString", [doubleType])!,
            ConvertToInt32Double = convertType.GetMethod("ToInt32", [doubleType])!,
            ConvertToStringInt64 = convertType.GetMethod("ToString", [int64Type])!,
            ConvertToInt64Double = convertType.GetMethod("ToInt64", [doubleType])!,
            ConvertToInt16Double = convertType.GetMethod("ToInt16", [doubleType])!,
            ConvertToStringSingle = convertType.GetMethod("ToString", [singleType])!,
            ConvertToStringInt16 = convertType.GetMethod("ToString", [int16Type])!,
            ConvertToStringUInt16 = convertType.GetMethod("ToString", [uint16Type])!,
            ConvertToStringByte = convertType.GetMethod("ToString", [byteType])!,
            ConvertToStringSByte = convertType.GetMethod("ToString", [sbyteType])!,
            ConvertToStringBoolean = convertType.GetMethod("ToString", [booleanType])!,
            ConvertToStringUInt64 = convertType.GetMethod("ToString", [uint64Type])!,
            ConvertToStringDecimal = convertType.GetMethod("ToString", [decimalType])!,
            ConvertToStringDateTime = convertType.GetMethod("ToString", [dateTimeType])!,
            ConvertToInt32Decimal = convertType.GetMethod("ToInt32", [decimalType])!,
            ConvertToInt64Decimal = convertType.GetMethod("ToInt64", [decimalType])!,
            ConvertToInt16Decimal = convertType.GetMethod("ToInt16", [decimalType])!,
            ConvertToDecimalDouble = convertType.GetMethod("ToDecimal", [doubleType])!,
            ConvertToDoubleDecimal = convertType.GetMethod("ToDouble", [decimalType])!,
            ConvertToDateTimeDouble = convertType.GetMethod("ToDateTime", [doubleType])!,
            ConvertToDoubleDateTime = convertType.GetMethod("ToDouble", [Resolve("System.DateTime")])!,
            IlRuntimeToDoubleMethod = ilBackendAssembly.GetType(typeof(Basic.IlBackend.IlRuntime).FullName!)!
                .GetMethod(nameof(Basic.IlBackend.IlRuntime.ToDouble), [Resolve("System.Object")])!,
            IlRuntimeToOADateMethod = ilBackendAssembly.GetType(typeof(Basic.IlBackend.IlRuntime).FullName!)!
                .GetMethod(nameof(Basic.IlBackend.IlRuntime.ToOADate), [Resolve("System.DateTime")])!,
            IlRuntimeFromOADateMethod = ilBackendAssembly.GetType(typeof(Basic.IlBackend.IlRuntime).FullName!)!
                .GetMethod(nameof(Basic.IlBackend.IlRuntime.FromOADate), [doubleType])!,
            IlRuntimeArrayElementAtMethod = ilBackendAssembly.GetType(typeof(Basic.IlBackend.IlRuntime).FullName!)!
                .GetMethod(nameof(Basic.IlBackend.IlRuntime.ArrayElementAt), [Resolve("System.Object"), Resolve("System.Int32")])!,
            IlRuntimeToBoolMethod = ilBackendAssembly.GetType(typeof(Basic.IlBackend.IlRuntime).FullName!)!
                .GetMethod(nameof(Basic.IlBackend.IlRuntime.ToBool), [Resolve("System.Double")])!,
            DecimalAdd = decimalType.GetMethod("op_Addition", [decimalType, decimalType])!,
            DecimalSubtract = decimalType.GetMethod("op_Subtraction", [decimalType, decimalType])!,
            DecimalMultiply = decimalType.GetMethod("op_Multiply", [decimalType, decimalType])!,
            DecimalDivide = decimalType.GetMethod("op_Division", [decimalType, decimalType])!,
            StringConcat = stringType.GetMethod("Concat", [stringType, stringType])!,
            StringEquals = stringType.GetMethod("Equals", [stringType, stringType])!,
            StringCompareOrdinal = stringType.GetMethod("CompareOrdinal", [stringType, stringType])!,
            StringLengthGetter = stringType.GetProperty("Length")!.GetGetMethod()!,
            StringToUpperInvariant = stringType.GetMethod("ToUpperInvariant", Type.EmptyTypes)!,
            StringToLowerInvariant = stringType.GetMethod("ToLowerInvariant", Type.EmptyTypes)!,
            StringTrim = stringType.GetMethod("Trim", Type.EmptyTypes)!,
            StringTrimStart = stringType.GetMethod("TrimStart", Type.EmptyTypes)!,
            StringTrimEnd = stringType.GetMethod("TrimEnd", Type.EmptyTypes)!,
            ArrayResizeOpenGeneric = arrayType.GetMethods()
                .Single(m => m.Name == "Resize" && m.IsGenericMethodDefinition),
            ArrayCopyWithOffsets = arrayType.GetMethod("Copy",
                [arrayType, Resolve("System.Int32"), arrayType, Resolve("System.Int32"), Resolve("System.Int32")])!,
            ArrayGetUpperBound = arrayType.GetMethod("GetUpperBound", [Resolve("System.Int32")])!,
            FuncOpenGenericsByArity = funcOpenGenerics,
            ActionOpenGenericsByArity = actionOpenGenerics,
            BasicExceptionType = basicExceptionType,
            BasicExceptionConstructor = basicExceptionType.GetConstructor(
                [errorCodeType, sourcePositionType, stringType, sourceLineType, exceptionType])!,
            BasicExceptionFromClrExceptionMethod = basicExceptionType.GetMethod("FromClrException")!,
            BasicExceptionCodeGetter = basicExceptionType.GetProperty("Code")!.GetGetMethod()!,
            BasicExceptionUserExceptionInstanceGetter = basicExceptionType.GetProperty("UserExceptionInstance")!.GetGetMethod()!,
            BasicExceptionUserExceptionInstanceSetter = basicExceptionType.GetProperty("UserExceptionInstance")!.GetSetMethod()!,
            ExceptionType = exceptionType,
            ExceptionMessageGetter = exceptionType.GetProperty("Message")!.GetGetMethod()!,
            ErrInstanceType = errInstanceType,
            ErrInstanceConstructor = errInstanceType.GetConstructor(
                [Resolve("System.Int32"), stringType, Resolve("System.Int32"), stringType, stringType])!,
            ErrInstanceNumberGetter = errInstanceType.GetProperty("Number")!.GetGetMethod()!,
            ErrInstanceDescriptionGetter = errInstanceType.GetProperty("Description")!.GetGetMethod()!,
            ErrInstanceLineNumberGetter = errInstanceType.GetProperty("LineNumber")!.GetGetMethod()!,
            ErrInstanceSourceLineTextGetter = errInstanceType.GetProperty("SourceLineText")!.GetGetMethod()!,
            SourcePositionConstructor = sourcePositionType.GetConstructor(
                [Resolve("System.Int32"), Resolve("System.Int32"), Resolve("System.Int32")])!
        };
    }
}
