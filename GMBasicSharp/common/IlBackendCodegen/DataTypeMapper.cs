// FILE: Basic.IlBackend.DataTypeMapper.cs

using System.Linq;
using System.Reflection.Emit;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

/// <summary>
/// Mappa ogni <see cref="DataType"/> BASIC al tipo CLR nativo corrispondente,
/// usato dal backend IL per dichiarare variabili locali, campi e firme di
/// metodo con tipi reali invece del boxing a <c>object</c> usato ovunque
/// dall'Executor/PCodeVm.
///
/// Riceve un <see cref="IlTypeContext"/> invece di usare <c>typeof(X)</c>
/// direttamente: i tipi scalari devono provenire dallo STESSO "mondo" di
/// assembly di tutto il resto dell'emissione (vedi <see cref="IlTypeContext"/>
/// per il perché).
///
/// Copertura attuale: solo gli scalari serviti dalla fetta verticale MVP
/// (vedi TODO Obsidian, Filone B, fase B6). Ogni altro <see cref="DataType"/>
/// lancia <see cref="NotSupportedIlDataTypeException"/> — errore esplicito a
/// compile-time, non fallback silenzioso (per design, vedi B6).
/// </summary>
public static class DataTypeMapper
{
    /// <param name="classTypes">
    /// TypeBuilder di ogni CLASS/INTERFACE utente già dichiarata, keyed per SymbolId
    /// (vedi IlProgramCompiler._classTypes) — serve a mappare <see cref="ClassType"/>/
    /// <see cref="InterfaceType"/> al TypeBuilder reale invece che a un placeholder.
    /// Classi e interfacce condividono lo stesso dizionario: i loro SymbolId vivono
    /// nello stesso spazio globale della SymbolTable, nessuna collisione possibile.
    /// Vuoto per un programma senza classi/interfacce.
    /// </param>
    public static Type Map(DataType dataType, IlTypeContext types, IReadOnlyDictionary<int, TypeBuilder> classTypes) => dataType switch
    {
        DoubleType => types.Double,
        /* SINGLE: le gambe C (VM/C, C99Native) rappresentano TUTTI i numerici come double e
         * non arrotondano le variabili SINGLE. Per parità di stampa/valore anche il backend
         * gestito usa double (CSNG è già identità double). Gli ARRAY SINGLE restano float[]
         * (vedi caso ArrayType sotto) per non cambiare l'ABI FFI (AI.EMBED). */
        SingleType => types.Double,
        LongType => types.Int64,
        // Il dialetto usa QUAD come intero signed a 64 bit (sinonimo storico di LONG
        // nella rappresentazione CLR del backend); mappare entrambi evita firme IL
        // incoerenti nelle API filesystem che dichiarano QUAD.
        QuadType => types.Int64,
        ULongType => types.UInt64,
        UQuadType => types.UInt64,
        /* INTEGER = intero a 64 bit, come su PCode/VM-C/C99Native (dove è un double/intero senza
         * troncamento). Con Int16 la gamba IL era l'UNICA a divergere: `DIM i AS INTEGER :
         * i = 70000` lanciava OverflowException (le altre gambe stampavano 70000) e
         * `i = LEN(corpo)` su un corpo di 70 KB dava 4464 (70000 mod 65536) solo qui.
         * Il dialetto non emula il wraparound classico a 16 bit: nessuna gamba lo fa, e
         * introdurlo troncherebbe in silenzio valori legittimi. */
        IntegerType => types.Int64,
        UIntegerType => types.UInt64,
        ByteType => types.Byte,
        SByteType => types.SByte,
        BitType => types.Boolean,
        EnumType e => Map(e.UnderlyingType, types, classTypes),
        StringType => types.String,
        StringBuilderType => typeof(System.Text.StringBuilder),
        VoidType => types.Void,
        CurrencyType => types.Decimal,
        // Il backend rappresenta DATE, TIME e DATETIME con DateTime CLR. DATE usa
        // sempre la componente oraria 00:00:00; TIME usa una data sentinella. Questa
        // scelta evita DateOnly/TimeOnly (TypeSpec aggiuntivi nell'AOT reference world)
        // e consente alle primitive DateTime pure di restare integralmente native.
        DateType or TimeType or DateTimeType => types.DateTime,
        TimeSpanType => typeof(System.TimeSpan),
        // Tupla: il letterale/valore è un object[] (vedi EmitTupleLiteral).
        TupleType => types.Object.MakeArrayType(),
        // Decomposizioni LINALG: il valore BASIC è un handle LONG opaco nel registry
        // IL (vedi IlRuntime.LinAlgDecompositions); il tipo statico LuDecompositionType
        // ecc. resta per la firma del linker, ma il CLR type è un intero a 64 bit.
        LuDecompositionType or QrDecompositionType or SvdDecompositionType or EvdDecompositionType or CholeskyDecompositionType
            => types.Int64,
        // Placeholder pre-inferenza del Linker: capita per variabili dichiarate implicitamente
        // senza tipo esplicito né suffisso (es. contatore FOR I=1 TO 5, mai visto da un LET/DIM
        // tipizzato prima) — il linker non lo risolve staticamente perché il runtime dinamico
        // esistente (Executor/PCodeVm) non ne ha bisogno finché non esegue. Default a double,
        // stesso tipo numerico implicito usato ovunque in questo backend per literal/variabili
        // non suffissate (vedi IlProgramCompiler.InferType).
        InferredType => types.Double,
        ChannelType => typeof(ChannelInfo),
        WaitGroupType => typeof(PCodeWaitGroup),
        MutexType => typeof(PCodeMutex),
        RwMutexType => typeof(PCodeRwMutex),
        OnceType => typeof(PCodeOnce),
        AtomicType => typeof(PCodeAtomic),
        PoolType => typeof(PCodePool),
        CondType => typeof(PCodeCond),
        ConcurrentMapType => typeof(PCodeConcurrentMap),
        ContextType => typeof(PCodeContext),
        FutureType => typeof(PCodeFutureValue),
        // Solo array mono-dimensionali (vedi IlProgramCompiler, DIM/ArrayAccessExpr):
        // un vero array CLR con elementi unboxed (T[]) è la rappresentazione più
        // veloce possibile — accesso diretto via ldelem/stelem, niente wrapper
        // dinamico/generico come il CREATE_ARRAY del p-code Compiler esistente.
        ArrayType a => (a.ElementType is SingleType)
            ? types.Single.MakeArrayType(Math.Max(1, a.Rank))   /* ABI FFI: array SINGLE = float[] (AI.EMBED) */
            : Map(a.ElementType, types, classTypes).MakeArrayType(Math.Max(1, a.Rank)),
        // Tipi grafici custom mappati a double[] (la VM li rappresenta come array numerici:
        // SCREENINFO -> {25, 80}, RGB -> {r, g, b, a}). Porting da GMBasic.
        DisplayModeType => types.Double.MakeArrayType(),
        RgbaColorType => types.Double.MakeArrayType(),
        // DICT con chiave STRING e valore BYTE[]: necessario alle primitive AEAD
        // (CHACHA20POLY1305ENCRYPT), che restituiscono ciphertext e tag senza
        // passare dalla rappresentazione dinamica della VM.
        DictType { KeyType: StringType, ValueType: ArrayType { ElementType: ByteType } }
            => typeof(Dictionary<string, byte[]>),
        // Collezioni CLR generiche: il backend le conserva come oggetti nativi
        // tipizzati. Le operazioni specifiche restano vincolate alla relativa API
        // BASIC, ma il valore non passa mai dalla rappresentazione object della VM.
        ListType l => typeof(List<>).MakeGenericType(Map(l.ElementType, types, classTypes)),
        DictType d => typeof(Dictionary<,>).MakeGenericType(Map(d.KeyType, types, classTypes), Map(d.ValueType, types, classTypes)),
        HashSetType s => typeof(HashSet<>).MakeGenericType(Map(s.ElementType, types, classTypes)),
        QueueType q => typeof(Queue<>).MakeGenericType(Map(q.ElementType, types, classTypes)),
        StackType s => typeof(Stack<>).MakeGenericType(Map(s.ElementType, types, classTypes)),
        SortedDictType d => typeof(SortedDictionary<,>).MakeGenericType(Map(d.KeyType, types, classTypes), Map(d.ValueType, types, classTypes)),
        LinkedListType l => typeof(LinkedList<>).MakeGenericType(Map(l.ElementType, types, classTypes)),
        // DOUBLYLINKEDLIST: semanticamente identica a LinkedList nel backend IL (stessi
        // metodi Add/First/Last/Contains/Remove/Count nello schema VM) — mappa allo stesso
        // CLR LinkedList<T>.
        DoublyLinkedListType l => typeof(LinkedList<>).MakeGenericType(Map(l.ElementType, types, classTypes)),
        // CIRCULARLINKEDLIST: rappresentata come List<T> nel backend IL — gli helper runtime
        // (CircularCurrent/CircularRotate) mantengono l'indice corrente per istanza; il CLR
        // SingleLinkedList non espone né IList né Current/Rotate.
        CircularLinkedListType l => typeof(List<>).MakeGenericType(Map(l.ElementType, types, classTypes)),
        // BAG: multiset — nel backend mappa a Dictionary<T,long> (elemento→conteggio),
        // coerente con lo schema VM (Count/UniqueCount).
        BagType b => typeof(Dictionary<,>).MakeGenericType(Map(b.ElementType, types, classTypes), types.Int64),
        // HLL: classe CommonLib reale (stima di cardinalità; mirror del runtime C gmb_hll.c).
        HyperLogLogType => typeof(CommonLib.Collections.HyperLogLog),
        // CACHE/TIMELINE: classi CommonLib reali (TTL cache, timeline ordinata).
        CacheType c => typeof(CommonLib.Collections.Cache<,>).MakeGenericType(Map(c.KeyType, types, classTypes), Map(c.ValueType, types, classTypes)),
        TimelineType t => typeof(CommonLib.Collections.Timeline<>).MakeGenericType(Map(t.ValueType, types, classTypes)),
        // RINGBUFFER → CircularQueue (CommonLib).
        RingBufferType r => typeof(CommonLib.Collections.CircularQueue<>).MakeGenericType(Map(r.ElementType, types, classTypes)),
        // TREE/AVL/RedBlack/BinaryIndexed/Trie/BPlus/Graph/HashRing/SparseSet/BinaryHeap:
        // strutture avanzate — il backend IL le rappresenta con le classi CommonLib dove
        // esistono, altrimenti SortedDictionary (semplice chiave→valore) o object.
        // Trie/SparseSet: classi CommonLib reali (Trie senza generici, SparseSet(int,int)).
        TrieType => typeof(CommonLib.Collections.Trie),
        SparseSetType => typeof(CommonLib.Collections.SparseSet),
        GraphType => typeof(CommonLib.Collections.Graph<object>),
        TreeType or AVLTreeType or RedBlackTreeType or BinaryIndexedTreeType
            or BPlusTreeType or HashRingType or BinaryHeapType => types.Object,
        // Istanza di CLASS utente: un vero TypeBuilder (reference type CLR), mai un
        // dizionario dinamico di campi (vedi IlProgramCompiler, sezione classi).
        ClassType c => classTypes.TryGetValue(c.Definition.SymbolId, out var typeBuilder)
            ? typeBuilder
            : throw new NotSupportedException($"Backend IL: classe '{c.Definition.OriginalName.Lexeme}' non ancora registrata."),
        // Un'interfaccia utente è un vero TypeBuilder CLR con TypeAttributes.Interface
        // (metodi/proprietà astratti, nessuna implementazione) — le classi che la
        // implementano dichiarano AddInterfaceImplementation + DefineMethodOverride
        // esplicita per ogni membro (vedi IlProgramCompiler, Pass -1/1.5b). Le chiamate
        // attraverso un riferimento di tipo interfaccia usano sempre `callvirt` (un
        // metodo d'interfaccia non ha un corpo `call`-abile direttamente).
        InterfaceType i => classTypes.TryGetValue(i.Definition.SymbolId, out var interfaceTypeBuilder)
            ? interfaceTypeBuilder
            // Interfaccia NATIVA della StdLib (es. IERC20 dal namespace BLOCKCHAIN — un
            // proxy di contratto, mai un TypeBuilder del modulo): object (il CONTRACT
            // produce un dict/oggetto dinamico). Il DataTypeMapper non deve lanciare.
            : types.Object,
        // Istanza di TYPE...END TYPE: un vero VALUE TYPE CLR (TypeBuilder che eredita da
        // System.ValueType, mai System.Object) — copia per valore nativa (Stloc/Ldloc),
        // mai un dizionario dinamico di campi. Condivide lo stesso dizionario "classTypes"
        // di classi/interfacce (stesso spazio globale di SymbolId, nessuna collisione).
        UserDefinedType udt => classTypes.TryGetValue(udt.SymbolId, out var udtTypeBuilder)
            ? udtTypeBuilder
            : throw new NotSupportedException($"Backend IL: UDT '{udt.OriginalName.Lexeme}' non ancora registrato."),
        // CALLBACK/FUNCTION() come tipo di variabile/parametro (es. `DIM f AS FUNCTION(n AS
        // DOUBLE) AS DOUBLE`, popolata da una lambda — vedi IlProgramCompiler, sezione
        // Closures) — un vero delegate CLR, System.Func&lt;...&gt; per ritorno non-VOID,
        // System.Action/Action&lt;...&gt; per SUB/VOID. Arità 0..8 supportata (vedi
        // IlTypeContext.FuncOpenGenericsByArity/ActionOpenGenericsByArity); oltre lancia
        // esplicito piuttosto che un errore criptico di IndexOutOfRange.
        FunctionPointerType fpt => MapFunctionPointerType(fpt, types, classTypes),
        // Tipo di una variabile CATCH ex AS ERR (o CATCH ex senza AS, che il Linker
        // risolve implicitamente a ErrType) — vedi IlProgramCompiler, sezione TRY/CATCH.
        // Basic.ErrInstance, non BasicException stesso: un record leggero con
        // Number/Description, costruito al momento della cattura dai dettagli
        // dell'eccezione CLR catturata (mai esposto direttamente all'utente BASIC).
        ErrType => types.ErrInstanceType,
        // JSON è un valore dinamico (oggetto/array/scalare non noto a compile-time) — stessa
        // rappresentazione della VM p-code (Dictionary<string,object?>/List<object?>/scalare,
        // vedi IlRuntime.JsonParse), mai un tipo CLR dedicato: un plain object CLR è la
        // rappresentazione più semplice possibile che permette a ForEachValues/ForEachEntries/
        // JsonFieldGet/JsonIndexGet (IlRuntime.cs) di operarci con lo stesso dispatch a
        // runtime già usato per LIST/DICT generici.
        JsonType => types.Object,
        // HttpRequest/HttpResponse nel sample BASIC sono accessibili a CAMPO DINAMICO
        // (es. req.BodyJson, req.Path) come nel VM — il backend IL li tratta come object
        // boxed e risolve i membri via dispatch dinamico (JsonFieldGet), NON come classi
        // CLR tipizzate (la Basic.Runtime.HttpRequest non ha BodyJson).
        HttpRequestType or HttpResponseType => types.Object,
        // Tipi che nel VM sono handle/marker opachi (WebSocket handle, SqlResultSet,
        // opzioni/config HTTP, tipi risolti per nome dal linker): il backend IL li
        // tratta come object boxed — i loro metodi/field passano dal dispatch dinamico
        // del runtime.
        HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
            or WebSocketType or SqlResultSetType or UnresolvedQualifiedType or NullableType or AnyType => types.Object,
        // NEXTHANDLER (parametro del middleware HTTP "next AS NEXTHANDLER"): handle opaco
        // per la catena dei middleware — boxed object (il runtime lo passa al chaining).
        NextHandlerType => types.Object,
        // GeneratorExpr materializzata: List<object?> (il FOR EACH la itera).
        GeneratorType => typeof(object),   // IlGeneratorInstance (IlRuntime.GeneratorCreate)
        // REGEX nativo (r AS REGEX — Pattern/IsMatch/Match/...): classe wrapper reale.
        RegexType => typeof(Basic.RegexNative.RegexWrapper),
        RegexMatchType => typeof(Basic.RegexNative.RegexMatchValue),
        // FETCHRESULT: la classe reale CommonLib (StatusCode/Body/Headers/IsSuccess).
        FetchResultType => typeof(CommonLib.Http.FetchResult),
        CookieJarType => typeof(CommonLib.Http.CookieJar),
        // Tipi risultato ANALYSIS (Anova/Regress/Pca/KMeans/Arima/HoltWinters → dict campi)
        AnovaResultType or RegressionResultType or PcaResultType or KMeansResultType
            or ArimaResultType or HoltWintersResultType or AdfTestResultType or HmmResultType or JohansenResultType or GpResultType or SvmResultType or GmmResultType or PolyFitResultType or ParticleResultType => typeof(Dictionary<string, object>),
        PortfolioResultType or RebalanceStrategyType or BacktestResultType or PortfolioConstraintsType or GaParametersType => typeof(Dictionary<string, object>),
        _ => throw new NotSupportedIlDataTypeException(dataType)
    };

    private static Type MapFunctionPointerType(FunctionPointerType fpt, IlTypeContext types, IReadOnlyDictionary<int, TypeBuilder> classTypes)
    {
        var arity = fpt.ParameterTypes.Length;
        var paramTypes = fpt.ParameterTypes.Select(p => Map(p, types, classTypes)).ToArray();

        if (fpt.ReturnType is VoidType)
        {
            if (arity >= types.ActionOpenGenericsByArity.Length)
                throw new NotSupportedException($"Backend IL: CALLBACK/SUB con {arity} parametri non ancora supportato (max {types.ActionOpenGenericsByArity.Length - 1}).");
            return arity == 0 ? types.ActionOpenGenericsByArity[0] : types.ActionOpenGenericsByArity[arity].MakeGenericType(paramTypes);
        }

        if (arity >= types.FuncOpenGenericsByArity.Length)
            throw new NotSupportedException($"Backend IL: CALLBACK/FUNCTION con {arity} parametri non ancora supportato (max {types.FuncOpenGenericsByArity.Length - 1}).");
        var returnType = Map(fpt.ReturnType, types, classTypes);
        return types.FuncOpenGenericsByArity[arity].MakeGenericType([.. paramTypes, returnType]);
    }
}

/// <summary>
/// Lanciata quando il backend IL incontra un <see cref="DataType"/> non
/// ancora mappato. Segnala una lacuna di copertura nota (vedi TODO Obsidian,
/// Filone B, fase B7 — espansione sistematica), non un bug.
/// </summary>
public sealed class NotSupportedIlDataTypeException(DataType dataType)
    : NotSupportedException($"Backend IL: tipo '{dataType.GetType().Name}' non ancora mappato a un tipo CLR.")
{
    public DataType DataType { get; } = dataType;
}
