// FILE: Ast.DataTypes.cs



// ReSharper disable InconsistentNaming
// ReSharper disable NotAccessedPositionalProperty.Global

namespace Basic;

/// <summary>
/// Classe base astratta per tutti i tipi di dato riconosciuti dal linguaggio.
/// Ora eredita da AstNode per avere una SourcePosition.
/// </summary>

#region JsonDerivedType Attributes for DataType

[JsonDerivedType(typeof(SingleType), nameof(SingleType))]
[JsonDerivedType(typeof(DoubleType), nameof(DoubleType))]
[JsonDerivedType(typeof(MatrixValueType), nameof(MatrixValueType))]
[JsonDerivedType(typeof(CurrencyType), nameof(CurrencyType))]
[JsonDerivedType(typeof(StringType), nameof(StringType))]
[JsonDerivedType(typeof(BitType), nameof(BitType))]
[JsonDerivedType(typeof(ByteType), nameof(ByteType))]
[JsonDerivedType(typeof(SByteType), nameof(SByteType))]
[JsonDerivedType(typeof(IntegerType), nameof(IntegerType))]
[JsonDerivedType(typeof(UIntegerType), nameof(UIntegerType))]
[JsonDerivedType(typeof(LongType), nameof(LongType))]
[JsonDerivedType(typeof(ULongType), nameof(ULongType))]
[JsonDerivedType(typeof(QuadType), nameof(QuadType))]
[JsonDerivedType(typeof(UQuadType), nameof(UQuadType))]
[JsonDerivedType(typeof(DateType), nameof(DateType))]
[JsonDerivedType(typeof(TimeType), nameof(TimeType))]
[JsonDerivedType(typeof(DateTimeType), nameof(DateTimeType))]
[JsonDerivedType(typeof(TimeSpanType), nameof(TimeSpanType))]
[JsonDerivedType(typeof(VoidType), nameof(VoidType))]
[JsonDerivedType(typeof(JsonType), nameof(JsonType))]
[JsonDerivedType(typeof(WaitGroupType), nameof(WaitGroupType))]
[JsonDerivedType(typeof(ChannelType), nameof(ChannelType))]
[JsonDerivedType(typeof(FutureType), nameof(FutureType))]
[JsonDerivedType(typeof(DisplayModeType), nameof(DisplayModeType))]
[JsonDerivedType(typeof(ErrType), nameof(ErrType))]
[JsonDerivedType(typeof(HttpOptionsType), nameof(HttpOptionsType))]
[JsonDerivedType(typeof(HttpHeadersType), nameof(HttpHeadersType))]
[JsonDerivedType(typeof(HttpResponseType), nameof(HttpResponseType))]
[JsonDerivedType(typeof(HttpConfigType), nameof(HttpConfigType))]
[JsonDerivedType(typeof(CookieJarType), nameof(CookieJarType))]
[JsonDerivedType(typeof(HttpRequestType), nameof(HttpRequestType))]
[JsonDerivedType(typeof(HttpPostedDataType), nameof(HttpPostedDataType))]
[JsonDerivedType(typeof(FormFieldType), nameof(FormFieldType))]
[JsonDerivedType(typeof(FileFieldType), nameof(FileFieldType))]
[JsonDerivedType(typeof(FetchResultType), nameof(FetchResultType))]
[JsonDerivedType(typeof(SqlResultSetType), nameof(SqlResultSetType))]
[JsonDerivedType(typeof(ListType), nameof(ListType))]
[JsonDerivedType(typeof(DictType), nameof(DictType))]
[JsonDerivedType(typeof(HashSetType), nameof(HashSetType))]
[JsonDerivedType(typeof(QueueType), nameof(QueueType))]
[JsonDerivedType(typeof(StackType), nameof(StackType))]
[JsonDerivedType(typeof(SortedDictType), nameof(SortedDictType))]
[JsonDerivedType(typeof(TreeType), nameof(TreeType))]
[JsonDerivedType(typeof(AVLTreeType), nameof(AVLTreeType))]
[JsonDerivedType(typeof(RedBlackTreeType), nameof(RedBlackTreeType))]
[JsonDerivedType(typeof(BinaryIndexedTreeType), nameof(BinaryIndexedTreeType))]
[JsonDerivedType(typeof(TrieType), nameof(TrieType))]
[JsonDerivedType(typeof(LinkedListType), nameof(LinkedListType))]
[JsonDerivedType(typeof(CircularLinkedListType), nameof(CircularLinkedListType))]
[JsonDerivedType(typeof(BinaryHeapType), nameof(BinaryHeapType))]
[JsonDerivedType(typeof(BagType), nameof(BagType))]
[JsonDerivedType(typeof(HyperLogLogType), nameof(HyperLogLogType))]
[JsonDerivedType(typeof(CacheType), nameof(CacheType))]
[JsonDerivedType(typeof(GraphType), nameof(GraphType))]
[JsonDerivedType(typeof(TimelineType), nameof(TimelineType))]
[JsonDerivedType(typeof(RingBufferType), nameof(RingBufferType))]
[JsonDerivedType(typeof(SparseSetType), nameof(SparseSetType))]
[JsonDerivedType(typeof(BPlusTreeType), nameof(BPlusTreeType))]
[JsonDerivedType(typeof(HashRingType), nameof(HashRingType))]
[JsonDerivedType(typeof(InferredType), nameof(InferredType))]
[JsonDerivedType(typeof(GeneratorType), nameof(GeneratorType))]
[JsonDerivedType(typeof(AnovaResultType), nameof(AnovaResultType))]
[JsonDerivedType(typeof(ArimaResultType), nameof(ArimaResultType))]
[JsonDerivedType(typeof(RegressionResultType), nameof(RegressionResultType))]
[JsonDerivedType(typeof(KMeansResultType), nameof(KMeansResultType))]
[JsonDerivedType(typeof(HoltWintersResultType), nameof(HoltWintersResultType))]
[JsonDerivedType(typeof(AdfTestResultType), nameof(AdfTestResultType))]
[JsonDerivedType(typeof(PcaResultType), nameof(PcaResultType))]
[JsonDerivedType(typeof(HmmResultType), nameof(HmmResultType))]
[JsonDerivedType(typeof(JohansenResultType), nameof(JohansenResultType))]
[JsonDerivedType(typeof(GpResultType), nameof(GpResultType))]
[JsonDerivedType(typeof(SvmResultType), nameof(SvmResultType))]
[JsonDerivedType(typeof(GmmResultType), nameof(GmmResultType))]
[JsonDerivedType(typeof(PolyFitResultType), nameof(PolyFitResultType))]
[JsonDerivedType(typeof(ParticleResultType), nameof(ParticleResultType))]
[JsonDerivedType(typeof(LuDecompositionType), nameof(LuDecompositionType))]
[JsonDerivedType(typeof(QrDecompositionType), nameof(QrDecompositionType))]
[JsonDerivedType(typeof(SvdDecompositionType), nameof(SvdDecompositionType))]
[JsonDerivedType(typeof(EvdDecompositionType), nameof(EvdDecompositionType))]
[JsonDerivedType(typeof(CholeskyDecompositionType), nameof(CholeskyDecompositionType))]
[JsonDerivedType(typeof(FunctionPointerType), nameof(FunctionPointerType))]
[JsonDerivedType(typeof(InterfaceType), nameof(InterfaceType))]
[JsonDerivedType(typeof(ClassType), nameof(ClassType))]
[JsonDerivedType(typeof(UserDefinedType), nameof(UserDefinedType))]
[JsonDerivedType(typeof(TBrowseType), nameof(TBrowseType))]
[JsonDerivedType(typeof(TBColumnType), nameof(TBColumnType))]
[JsonDerivedType(typeof(ArrayType), nameof(ArrayType))]
[JsonDerivedType(typeof(UnresolvedQualifiedType), nameof(UnresolvedQualifiedType))]
[JsonDerivedType(typeof(PortfolioResultType), nameof(PortfolioResultType))]
[JsonDerivedType(typeof(PortfolioConstraintsType), nameof(PortfolioConstraintsType))]
[JsonDerivedType(typeof(GaParametersType), nameof(GaParametersType))]
[JsonDerivedType(typeof(BacktestResultType), nameof(BacktestResultType))]
[JsonDerivedType(typeof(PortfolioMetricsType), nameof(PortfolioMetricsType))]
[JsonDerivedType(typeof(RebalanceStrategyType), nameof(RebalanceStrategyType))]
[JsonDerivedType(typeof(NextHandlerType), nameof(NextHandlerType))]
[JsonDerivedType(typeof(WebSocketType), nameof(WebSocketType))]
[JsonDerivedType(typeof(KeyValuePairType), nameof(KeyValuePairType))]
[JsonDerivedType(typeof(EnumType), nameof(EnumType))]
[JsonDerivedType(typeof(RgbaColorType), nameof(RgbaColorType))]
[JsonDerivedType(typeof(AnyType), nameof(AnyType))]
[JsonDerivedType(typeof(TupleType), nameof(TupleType))]
[JsonDerivedType(typeof(NullableType), nameof(NullableType))]
[JsonDerivedType(typeof(TypeParameter), nameof(TypeParameter))]
#endregion

public abstract record DataType(SourcePosition Position) : AstNode(Position)
{
    /// <summary>
    /// Costruttore di default per i tipi che non hanno una posizione fisica,
    /// come i tipi singleton creati internamente (es. tipi primitivi).
    /// </summary>
    protected DataType() : this(SourcePosition.Unknown)
    {
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    /// <returns>Una nuova istanza di AstNode che è una copia completa dell'originale.</returns>
    public abstract override AstNode DeepClone();
}

/// <summary>
/// Rappresenta il tipo di dato a virgola mobile a precisione singola (4 byte).
/// Corrisponde alla keyword SINGLE.
/// </summary>
public record SingleType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "SINGLE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato a virgola mobile a doppia precisione (8 byte).
/// Corrisponde alla keyword DOUBLE.
/// </summary>
public record DoubleType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "DOUBLE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un VALORE matrice (descrittore `MatrixValue`): tipo dei risultati
/// delle funzioni `MATRIX.*`/LINALG matriciali a valore (F4). Distinto da
/// <see cref="ArrayType"/> rank-2 (che resta il layout flat `double[,]` di un DIM).
/// </summary>
public record MatrixValueType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>Rappresentazione testuale compatta.</summary>
    public override string ToString() => "MATRIX VALUE";

    /// <inheritdoc />
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato finanziario a 4 decimali fissi.
/// Corrisponde alla keyword CURRENCY.
/// </summary>
public record CurrencyType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "CURRENCY";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato stringa.
/// </summary>
/// <param name="FixedLength">
/// Se non nullo, rappresenta la lunghezza fissa della stringa.
/// Se nullo, la stringa ha lunghezza variabile.
/// </param>
public record StringType(int? FixedLength = null, Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };

    /// <summary>
    /// Restituisce una rappresentazione testuale del tipo stringa,
    /// includendo la lunghezza fissa se specificata.
    /// </summary>
    /// <returns>Una stringa come "STRING" o "STRING * 10".</returns>
    public override string ToString()
    {
        return FixedLength.HasValue ? $"STRING * {FixedLength.Value}" : "STRING";
    }

    // Due StringType con la stessa FixedLength sono LO STESSO tipo anche se costruiti da
    // token sorgente diversi (es. uno sintetizzato da codice con SourceToken=null, l'altro
    // risolto dal parsing di una clausola "AS STRING"). L'equality di default dei record
    // includerebbe SourceToken, facendo fallire il confronto di tipo in ogni punto che
    // usa .Equals() invece di un pattern match "is StringType" (es. risoluzione
    // dell'overload OPERATOR CTYPE per tipo di ritorno in Gpm.Conversions.cs).
    public virtual bool Equals(StringType? other) => other is not null && FixedLength == other.FixedLength;

    public override int GetHashCode() => FixedLength.GetHashCode();
}

/// <summary>
/// Rappresenta il tipo di dato booleano (True/False).
/// Corrisponde alle keyword BIT, BOOL, BOOLEAN.
/// </summary>
public record BitType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "BIT";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 8-bit senza segno (0 to 255).
/// Corrisponde alla keyword BYTE.
/// </summary>
public record ByteType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "BYTE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 8-bit con segno (-128 to 127).
/// Corrisponde alla keyword SBYTE.
/// </summary>
public record SByteType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "SBYTE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 16-bit con segno.
/// Corrisponde alle keyword INTEGER, SHORT, INT.
/// </summary>
public record IntegerType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "INTEGER";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 16-bit senza segno.
/// Corrisponde alle keyword UINTEGER, USHORT, UINT.
/// </summary>
public record UIntegerType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "UINTEGER";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 32-bit con segno.
/// Corrisponde alla keyword LONG.
/// </summary>
public record LongType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "LONG";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 32-bit senza segno.
/// Corrisponde alla keyword ULONG.
/// </summary>
public record ULongType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ULONG";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 64-bit con segno.
/// Corrisponde alla keyword QUAD.
/// </summary>
public record QuadType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "QUAD";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato intero a 64-bit senza segno.
/// Corrisponde alla keyword UQUAD.
/// </summary>
public record UQuadType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "UQUAD";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo di dato per la sola data (anno, mese, giorno).
/// Corrisponde alla keyword DATE.
/// </summary>
public record DateType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "DATE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo di dato per la sola ora (ora, minuti, secondi).
/// Corrisponde alla keyword TIME.
/// </summary>
public record TimeType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "TIME";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo di dato che combina data e ora.
/// Corrisponde alla keyword DATETIME.
/// </summary>
public record DateTimeType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "DATETIME";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo di dato per un intervallo di tempo.
/// Corrisponde alla keyword TIMESPAN.
/// </summary>
public record TimeSpanType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "TIMESPAN";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato VOID, usato per indicare l'assenza di un valore di ritorno.
/// </summary>
public record VoidType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "VOID";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo NIL, usato nell'operatore IS per verificare se un valore e' null.
/// </summary>
public record NilType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "NIL";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato JSON.
/// Corrisponde alla keyword JSON.
/// </summary>
public record JsonType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "JSON";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato WAITGROUP per la sincronizzazione di task concorrenti.
/// </summary>
public record WaitGroupType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "WAITGROUP";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato CHANNEL per la comunicazione tra task concorrenti.
/// </summary>
/// <param name="ValueType">Il tipo di dato dei valori che il canale può trasportare.</param>
public record ChannelType(DataType ValueType, int Capacity = 0,
    ChannelDirection Direction = ChannelDirection.Bidirectional) : DataType(ValueType.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// Capacity = 0 significa canale NON bufferizzato (unbuffered, handshake), parita' Go make(chan T).
    /// </summary>
    public override string ToString() => "CHANNEL";

    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new ChannelType((DataType)ValueType.DeepClone(), Capacity, Direction);
}

/// <summary>Direzione di un canale (parità Go chan direzionali): Bidirectional (chan T),
/// SendOnly (chan&lt;- T — si può solo SEND), ReceiveOnly (&lt;-chan T — si può solo RECEIVE).
/// Il runtime è invariato (ChannelInfo); la direzione è un vincolo di tipo verificato a
/// compile-time (SEND su ReceiveOnly / RECEIVE su SendOnly = errore).</summary>
public enum ChannelDirection
{
    Bidirectional = 0,
    SendOnly = 1,
    ReceiveOnly = 2
}

/// <summary>
/// Rappresenta il tipo di dato FUTURE, un placeholder per un valore che verrà calcolato in modo asincrono.
/// </summary>
public record FutureType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "FUTURE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato strutturato built-in DISPLAYMODE.
/// </summary>
public record DisplayModeType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "DISPLAYMODE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato predefinito per le informazioni sulle eccezioni.
/// </summary>
public record ErrType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ERR";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato HTTPOPTIONS nel linguaggio BASIC.
/// </summary>
public record HttpOptionsType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPOPTIONS";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato HTTPHEADERS nel linguaggio BASIC.
/// </summary>
public record HttpHeadersType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPHEADERS";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato HTTPRESPONSE nel linguaggio BASIC.
/// </summary>
public record HttpResponseType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPRESPONSE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato HTTPCONFIG nel linguaggio BASIC.
/// </summary>
public record HttpConfigType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPCONFIG";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato COOKIEJAR nel linguaggio BASIC — un contenitore di cookie
/// indipendente, assegnabile a HTTPOPTIONS.CookieJar per mantenere sessioni HTTP separate
/// (es. simulare più utenti/tab di un browser in parallelo). Vedi CommonLib.Http.CookieJar.
/// </summary>
public sealed record CookieJarType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "COOKIEJAR";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato HTTPREQUEST nel linguaggio BASIC.
/// </summary>
public record HttpRequestType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPREQUEST";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per i dati di un form parsato nel linguaggio BASIC.
/// Questo nodo funge da marcatore di tipo per l'oggetto che contiene i campi
/// e i file di un form multipart.
/// </summary>
public record HttpPostedDataType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HTTPPOSTEDDATA";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un singolo campo di testo (`name`/`value`)
/// all'interno di un oggetto `HTTPPOSTEDDATA`.
/// </summary>
public record FormFieldType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "FORMFIELD";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un singolo file caricato (`name`/`filename`/`data`)
/// all'interno di un oggetto `HTTPPOSTEDDATA`.
/// </summary>
public record FileFieldType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "FILEFIELD";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato FETCHRESULT nel linguaggio BASIC.
/// </summary>
public record FetchResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "FETCHRESULT";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un result set SQL restituito da <c>SQL QUERY</c>; supporta <c>rs("column")</c>.
/// </summary>
public record SqlResultSetType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "SQLRESULTSET";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo di dato speciale che funge da placeholder per l'inferenza di tipo.
/// </summary>
public record InferredType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale del tipo per il dump dell'AST.
    /// </summary>
    public override string ToString() => "<inferred>";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato di un'istanza di generatore.
/// </summary>
/// <param name="YieldType">Il tipo di dato dei valori che questo generatore cede (yields).</param>
public record GeneratorType(DataType YieldType) : DataType(YieldType.Position)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale del tipo per il dump dell'AST.
    /// </summary>
    public override string ToString() => $"GENERATOR OF {YieldType}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new GeneratorType((DataType)YieldType.DeepClone());
}

// ========================================================================
// --- Strutture Dati per Risultati Statistici Avanzati ---
// ========================================================================

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'analisi ANOVA.
/// </summary>
public record AnovaResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ANOVARESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'analisi ARIMA.
/// </summary>
public record ArimaResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ARIMARESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di una regressione.
/// </summary>
public record RegressionResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "REGRESSIONRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'analisi K-Means.
/// </summary>
public record KMeansResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "KMEANSRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un modello Holt-Winters.
/// </summary>
public record HoltWintersResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "HOLTWINTERSRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un test ADF.
/// </summary>
public record AdfTestResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ADFTESTRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'analisi PCA.
/// </summary>
public record PcaResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "PCARESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un particle filter.
/// </summary>
public record ParticleResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "PARTICLERESULT";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un fit polinomiale.
/// </summary>
public record PolyFitResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "POLYFITRESULT";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un GMM 1D (EM).
/// </summary>
public record GmmResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "GMMRESULT";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un SVM multiclasse.
/// </summary>
public record SvmResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "SVMRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di una regressione con processo gaussiano.
/// </summary>
public record GpResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "GPRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato del test di cointegrazione di Johansen.
/// </summary>
public record JohansenResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "JOHANSENRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un Hidden Markov Model.
/// </summary>
public record HmmResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "HMMRESULT";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un handle a una decomposizione LU.
/// </summary>
public record LuDecompositionType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "LUDECOMPOSITION";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un handle a una decomposizione QR.
/// </summary>
public record QrDecompositionType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "QRDECOMPOSITION";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un handle a una decomposizione SVD.
/// </summary>
public record SvdDecompositionType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "SVDDECOMPOSITION";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un handle a una decomposizione EVD.
/// </summary>
public record EvdDecompositionType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "EVDDECOMPOSITION";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per un handle a una decomposizione Cholesky.
/// </summary>
public record CholeskyDecompositionType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "CHOLESKYDECOMPOSITION";

    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato di un puntatore a funzione (callback FFI).
/// </summary>
/// <param name="ParameterTypes">Un array immutabile dei tipi di dato dei parametri della callback.</param>
/// <param name="ReturnType">Il tipo di dato del valore di ritorno della callback.</param>
public record FunctionPointerType(ImmutableArray<DataType> ParameterTypes, DataType ReturnType)
    : DataType(ReturnType.Position)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale del tipo.
    /// </summary>
    public override string ToString() => $"CALLBACK ({string.Join(", ", ParameterTypes)}) AS {ReturnType}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() =>
        new FunctionPointerType([..ParameterTypes.Select(p => (DataType)p.DeepClone())],
            (DataType)ReturnType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato di un'interfaccia.
/// Usato per variabili dichiarate come `AS IMyInterface`.
/// </summary>
public record InterfaceType(InterfaceDefinition Definition) : DataType(Definition.Position)
{
    public override AstNode DeepClone() => this with { }; // InterfaceDefinition è condivisa
    
    public override string ToString() => Definition.OriginalName.Lexeme;
}

/// <summary>
/// Rappresenta il tipo di dato di un'istanza di classe.
/// </summary>
/// <param name="Definition">La <see cref="ClassDefinition"/> che definisce la struttura e i membri della classe.</param>
public record ClassType(ClassDefinition Definition) : DataType(Definition.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo MUTEX (parità Go sync.Mutex): un lock esclusivo usato con LOCK/UNLOCK.
/// </summary>
public record MutexType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "MUTEX";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo RWMUTEX (parità Go sync.RWMutex): lock lettore/scrittore usato con
/// MUTEX LOCK/UNLOCK (scrittore, esclusivo) e MUTEX RLOCK/RUNLOCK (lettori, condivisi).
/// </summary>
public record RwMutexType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "RWMUTEX";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo ONCE (parità Go sync.Once): un flag che garantisce che una procedura
/// venga eseguita una sola volta anche con chiamate concorrenti (comando ONCE RUN o, proc).
/// </summary>
public record OnceType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ONCE";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo ATOMIC (parità Go sync/atomic): un box con un valore Int64 accessibile
/// in modo atomico via le funzioni del namespace ATOMIC (INCREMENT/FETCHVALUE/STORE/COMPARESWAP).
/// </summary>
public record PoolType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "POOL";
    public override AstNode DeepClone() => this;
}
public record CondType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "COND";
    public override AstNode DeepClone() => this;
}
public record ConcurrentMapType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "CONCURRENTMAP";
    public override AstNode DeepClone() => this;
}
public record AtomicType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "ATOMIC";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo CONTEXT (parità Go context.Context): cancellazione propagata. Un ctx
/// si crea con CONTEXT.BACKGROUND(), si deriva con CONTEXT.WITHCANCEL(ctx), si cancella con
/// CONTEXT.CANCEL(ctx); CONTEXT.DONE(ctx) ritorna un canale chiuso alla cancellazione (pattern
/// worker che termina su ctx.Done()).
/// </summary>
public record ContextType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "CONTEXT";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato built-in TBrowse nel sistema di tipi.
/// </summary>
public record TBrowseType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato built-in TBColumn nel sistema di tipi.
/// </summary>
public record TBColumnType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un tipo array, specificando il tipo degli elementi e il RANK statico
/// (numero di dimensioni della matrice rettangolare). Il rank e' parte del TIPO:
/// attraversa parametri e valori di ritorno senza deduzione dal valore.
/// NON esistono array annidati: `ARRAY OF ARRAY OF T` è vietato dal parser; usa
/// <c>MATRIX OF (I1, …, In, T)</c> per le matrici a rank n.
/// </summary>
/// <param name="ElementType">Il tipo di dato degli elementi contenuti nell'array.</param>
/// <param name="Rank">Numero di dimensioni (>= 1). Default 1 (vettore).</param>
public record ArrayType(DataType ElementType, int Rank = 1) : DataType(ElementType.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del tipo array.
    /// </summary>
    public override string ToString() => Rank > 1 ? $"ARRAY({Rank}) OF {ElementType}" : $"ARRAY OF {ElementType}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new ArrayType((DataType)ElementType.DeepClone(), Rank);
}

// ========================================================================
// --- Tipi di Dato per Ottimizzazione di Portafoglio e Backtesting ---
// ========================================================================

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'operazione di ottimizzazione.
/// </summary>
public record PortfolioResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per i vincoli di un'ottimizzazione.
/// </summary>
public record PortfolioConstraintsType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per i parametri di un algoritmo genetico.
/// </summary>
public record GaParametersType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per il risultato di un'operazione di backtest.
/// </summary>
public record BacktestResultType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per le metriche di performance di un portafoglio.
/// </summary>
public record PortfolioMetricsType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per una strategia di ribilanciamento.
/// </summary>
public record RebalanceStrategyType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato speciale per l'handle della funzione 'next' nel middleware del server HTTP.
/// </summary>
public record NextHandlerType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato per una connessione WebSocket.
/// </summary>
public record WebSocketType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato ENUM.
/// Ora contiene il nome e il tipo sottostante per una risoluzione più semplice.
/// </summary>
/// <param name="Name">Il token con il nome dell'enumerazione.</param>
/// <param name="UnderlyingType">Il tipo di dato sottostante (es. INTEGER).</param>
public record EnumType(Token Name, DataType UnderlyingType)
    : DataType(Name?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => $"ENUM {Name.Lexeme}";
    public override AstNode DeepClone() => new EnumType((Token)Name.DeepClone(), (DataType)UnderlyingType.DeepClone());
}


/// <summary>
/// Rappresenta un tipo di dato speciale per un valore di colore RGBA.
/// Funge da marcatore di tipo per il compilatore e l'Executor.
/// </summary>
public record RgbaColorType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "RGBA";

    /// <summary>
    /// Crea una copia profonda di questo nodo. Poiché il tipo è immutabile, restituisce se stesso.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato dinamico ANY. 
/// Una variabile di questo tipo può contenere qualsiasi valore a runtime.
/// </summary>
public record AnyType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "ANY";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta un singolo campo di una tupla con nome opzionale.
/// </summary>
/// <param name="Name">Il nome del campo (null per campi anonimi).</param>
/// <param name="DataType">Il tipo di dato del campo.</param>
public record TupleField(string? Name, DataType DataType)
{
    public override string ToString() => Name is not null ? $"{Name} AS {DataType}" : DataType.ToString()!;
}

/// <summary>
/// Rappresenta un tipo tuple: (T1, T2, ..., Tn) con nomi opzionali.
/// Usato per i parametri di tipo tuple nelle funzioni e per i literal tuple.
/// </summary>
public record TupleType(ImmutableArray<TupleField> Elements) : DataType(SourcePosition.Unknown)
{
    public override string ToString() => $"({string.Join(", ", Elements.Select(e => e.ToString()))})";

    public override AstNode DeepClone() => new TupleType(Elements.Select(e => new TupleField(e.Name, (DataType)e.DataType.DeepClone())).ToImmutableArray());

    /// <summary>
    /// Crea un TupleType da soli tipi (campi anonimi).
    /// </summary>
    public static TupleType OfTypes(params DataType[] types)
        => new(types.Select(t => new TupleField(null, t)).ToImmutableArray());

    /// <summary>
    /// Crea un TupleType da soli tipi (campi anonimi).
    /// </summary>
    public static TupleType OfTypes(ImmutableArray<DataType> types)
        => new(types.Select(t => new TupleField(null, t)).ToImmutableArray());
}

/// <summary>
/// Rappresenta un tipo nullable (es. <c>INTEGER?</c>).
/// A runtime un valore nullable è rappresentato da <c>null</c> (C#) quando assente,
/// oppure dal valore boxed del tipo sottostante quando presente.
/// </summary>
/// <param name="ElementType">Il tipo di dato sottostante (es. <see cref="IntegerType"/>).</param>
public record NullableType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"{ElementType}?";

    public override AstNode DeepClone() => new NullableType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta un parametro di tipo generico (es. T in LIST OF T, o T in FUNCTION Foo(Of T)).
/// A compile-time, un type parameter è un placeholder che viene sostituito dal tipo concreto
/// quando il generico viene specializzato (es. LIST OF INTEGER → T = INTEGER).
/// </summary>
/// <param name="Name">Il token con il nome del parametro (es. "T").</param>
/// <param name="Constraints">Vincoli opzionali su questo parametro (es. WHERE T IS IComparable).</param>
public record TypeParameter(
    Token Name,
    ImmutableArray<DataType>? Constraints = null) : DataType(Name.Position)
{
    public override string ToString() => Name.Lexeme;

    public override AstNode DeepClone() => this with { Name = (Token)Name.DeepClone() };

    // Due TypeParameter con lo stesso nome (es. "T" nella clausola <T> e "T" referenziato
    // nel corpo, es. "Value AS T") sono LO STESSO parametro anche se sono token diversi
    // con posizioni sorgente diverse. L'equality di default dei record includerebbe
    // Token.Position (parametro del costruttore primario di Token), facendo fallire ogni
    // lookup nella substitution map di SubstituteTypeParameters non appena il parametro
    // viene referenziato più di una volta nella dichiarazione (praticamente sempre).
    public virtual bool Equals(TypeParameter? other) =>
        other is not null && Name.Lexeme.Equals(other.Name.Lexeme, StringComparison.OrdinalIgnoreCase);

    public override int GetHashCode() => Name.Lexeme.ToUpperInvariant().GetHashCode();
}


