// FILE: Interpreter.Parser.Expressions.Literals.cs

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

// ReSharper disable InvalidXmlDocComment
namespace Basic;

/// <summary>
///     Contiene i metodi per il parsing di espressioni, gestendo la precedenza degli operatori.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Parsa una clausola di tipo, che può essere implicita (suffisso) o esplicita (AS Tipo).
    /// </summary>
    private DataType ParseTypeClause(Token variableNameToken, bool isFunctionDeclaration = false)
    {
        const bool log = false;
        Log($"[ParseTypeClause] ENTER. Variable: {variableNameToken.Lexeme}, Current: {Peek()}", log);

        if (!Match(TokenKind.As))
        {
            if (isFunctionDeclaration)
            {
                throw CreateError(variableNameToken,
                    "A function declaration must have an explicit return type using the 'AS' clause.");
            }

            // Fallback al suffisso.
            var suffixType = DeduceTypeFromSuffix(variableNameToken.Lexeme);
            Log($"[ParseTypeClause] EXIT. Used suffix type: {suffixType.GetType().Name}. Next: {Peek()}", log);
            return suffixType;
        }

        // AS è presente
        var explicitType = ParseTypeName(); // Chiamata corretta

        Log($"[ParseTypeClause] EXIT. Used explicit type: {explicitType.GetType().Name}. Next: {Peek()}", log);
        return explicitType;
    }

    /// <summary>
    /// [FASE 1 - CORRETTO] Parsa un nome di tipo. Nella Fase 1, questo metodo risolve solo i tipi primitivi/collezione
    /// e crea un nodo UnresolvedQualifiedType per UDT e classi, senza alcuna ricerca nella SymbolTable.
    /// Questa versione unificata gestisce correttamente i nomi semplici, qualificati e la sintassi AS FUNCTION/AS SUB.
    /// **CORREZIONE:** Gestisce il tipo 'ARRAY' per prevenire l'annidamento errato di 'ArrayType'.
    /// </summary>
    /// <returns>Il <see cref="DataType"/> corrispondente o un segnaposto per il Linker.</returns>
    private DataType ParseTypeName()
    {
        var typeToken = Peek();

        // --- GESTIONE TIPO TUPLE: (T1, T2, ...) o (Nome AS T1, ...) ---
        if (typeToken.Kind == TokenKind.LeftParen)
        {
            Advance(); // Consuma (
            var fields = ImmutableArray.CreateBuilder<TupleField>();
            do
            {
                // Parsa il campo dell'elemento (con nome opzionale)
                var field = ParseTupleElementType();
                fields.Add(field);
            } while (Match(TokenKind.Comma));
            Consume(TokenKind.RightParen, "Expected ')' after tuple type elements.");
            return new TupleType(fields.ToImmutable());
        }

        // --- GESTIONE SPECIALE PER 'FUNCTION' o 'SUB' COME TIPO ---
        if (typeToken.Kind == TokenKind.Function)
        {
            var isSub = typeToken.Lexeme.Equals("SUB", StringComparison.OrdinalIgnoreCase);
            Advance(); // Consuma 'FUNCTION' o 'SUB'
            
            var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;
            
            DataType returnType;
            if (Match(TokenKind.As))
            {
                if (isSub)
                    throw CreateError(Previous(), "La sintassi 'AS SUB' non può specificare un tipo di ritorno con 'AS'.");
                returnType = ParseTypeName();
            }
            else
            {
                if (!isSub)
                    throw CreateError(typeToken, "La sintassi 'AS FUNCTION' deve specificare un tipo di ritorno esplicito con 'AS'.");
                returnType = new VoidType();
            }

            return new FunctionPointerType([..parameters.Select(p => p.DataType)], returnType);
        }

        // --- GESTIONE UNIFICATA PER IDENTIFICATORI E NOMI QUALIFICATI ---
        // Permissivo: anche una keyword può essere il nome di un tipo definito dall'utente
        // (parità col parser C: `TYPE step` + `DIM v AS step`). Se il tipo non esiste, il
        // linker dà l'errore "tipo sconosciuto" — stesso esito di un identificatore ignoto.
        if (!IsPermissiveNameToken(typeToken))
            throw CreateError(typeToken, $"'{typeToken.Lexeme}' non è un nome di tipo valido.");

        // Keyword non-structural (es. Point, Shape, Circle) che non sono tipi built-in:
        // trattale come identificatori/nomi di tipo definiti dall'utente
        if (typeToken.Kind != TokenKind.Identifier && !IsBuiltinTypeToken(typeToken.Kind))
        {
            var qualifiedName = ParseQualifiedNameExpression();
            return ParseGenericTypeArgumentsSuffix(new UnresolvedQualifiedType(qualifiedName));
        }

        // `AS DECISION` = alias del tipo JSON (decision layer JEV/SystemOne): il valore è
        // l'oggetto JSON con VALUE/PROBABILITY/CONFIDENCE. Il tipo nativo DECISION<T> è futuro.
        if (typeToken.Kind == TokenKind.Identifier
            && typeToken.Lexeme.Equals("DECISION", StringComparison.OrdinalIgnoreCase))
        {
            Advance();
            // `DECISION<T>` / `DECISION<T, U>`: il parametro è documentale (il valore resta
            // JSON); consuma l'intero suffisso generico con un contatore di profondità, così
            // il parser non lascia `<...>` appeso (causava un loop su PCode).
            if (Check(TokenKind.LessThan))
            {
                var depth = 0;
                do
                {
                    if (Check(TokenKind.LessThan)) depth++;
                    else if (Check(TokenKind.GreaterThan)) depth--;
                    Advance();
                } while (!IsAtEnd() && depth > 0);
            }
            return new JsonType(typeToken);
        }

        var isBuiltinKeyword = typeToken.Kind != TokenKind.Identifier;

        // Parsa il tipo sottostante (senza suffisso nullable).
        DataType baseType;
        if (isBuiltinKeyword)
        {
            Advance(); // Consuma il token keyword (es. 'ARRAY', 'LIST', 'STRING', 'BYTE')

            baseType = typeToken.Kind switch
            {
                // CORREZIONE QUI: ArrayType è il tipo wrapper di ParseSingleTypeCollection.
                // Non dobbiamo più passargli un costruttore che crea un nuovo ArrayType,
                // perché ParseSingleTypeCollection lo crea già.
                // ArrayType è una classe specializzata, quindi lo si gestisce così.
                TokenKind.Array => (ArrayType)ParseSingleTypeCollection(elementType => new ArrayType(elementType)),

                TokenKind.List => ParseListType(),
                TokenKind.Dict => ParseDictType(),
                TokenKind.HashSet => ParseHashSetType(),
                TokenKind.Queue => ParseQueueType(),
                TokenKind.Stack => ParseStackType(),
                TokenKind.Sorted => ParseSortedDictType(),
                TokenKind.Tree => ParseTreeType(),
                TokenKind.AVLTree => ParseAvlTreeType(),
                TokenKind.RedBlack => ParseRedBlackTreeType(),
                TokenKind.BITree => ParseBiTreeType(),
                TokenKind.Trie => new TrieType(),
                TokenKind.LinkedList => ParseLinkedListType(),
                TokenKind.DoublyLinkedList => ParseDoublyLinkedListType(),
                TokenKind.Circular => ParseCircularLinkedListType(),
                TokenKind.Heap => ParseHeapType(isMaxHeap: false),
                TokenKind.MaxHeap => ParseHeapType(isMaxHeap: true),
                TokenKind.Bag => ParseBagType(),
                TokenKind.Hll => new HyperLogLogType(),
                TokenKind.Cache => ParseCacheType(),
                TokenKind.Graph => ParseGraphType(),
                TokenKind.Timeline => ParseTimelineType(),
                TokenKind.HashRing => ParseHashRingType(),
                TokenKind.RingBuffer => ParseRingBufferType(),
                TokenKind.SparseSet => new SparseSetType(),
                TokenKind.BPlusTree => ParseBPlusTreeType(),
                TokenKind.Channel => ParseChannelType(),
                TokenKind.StringBuilder => new StringBuilderType(),
                TokenKind.Regex => new RegexType(),

                _ => ParseBaseType(typeToken) // Per tipi primitivi come STRING, BYTE, LONG, ecc.
            };
        }
        else // Non è una keyword built-in, è un identificatore (potrebbe essere UDT, Class, Enum, o qualificato)
        {
            var qualifiedName = ParseQualifiedNameExpression(); // Parsa l'intero nome qualificato (es. MyNamespace.MyType)
            baseType = ParseGenericTypeArgumentsSuffix(new UnresolvedQualifiedType(qualifiedName));
        }

        // --- SUFFISSO RANK ESPLICITO: T(,), T(,,), ... (rank = numero di dimensioni) ---
        // Sintassi simmetrica ai parametri `x(,) AS DOUBLE`: il rank e' parte del tipo e
        // attraversa ritorni e parametri. `()` (rank 1) resta espresso da `ARRAY OF T`.
        if (Check(TokenKind.LeftParen))
        {
            var save = _current;
            Advance(); // consuma '('
            var rank = 1;
            while (Match(TokenKind.Comma)) rank++;
            if (rank > 1 && Check(TokenKind.RightParen))
            {
                Advance(); // consuma ')'
                return new ArrayType(baseType, rank);
            }
            _current = save; // non era un suffisso di rank: ripristina
        }

        // --- SUFFISSO NULLABLE: Tipo? ---
        // Riconosce '?' dopo un tipo (es. INTEGER?, STRING?, MyClass?).
        // Non confligge con l'operatore ?? perché qui siamo in posizione di tipo (dopo AS).
        if (Match(TokenKind.QuestionMark))
        {
            return new NullableType(baseType);
        }

        return baseType;
    }

    /// <summary>
    /// [FASE 1] Riconosce ed eventualmente consuma una lista di type argument generici
    /// subito dopo un nome di tipo utente, es. <c>Box&lt;DOUBLE&gt;</c> o <c>Pair&lt;INTEGER, STRING&gt;</c>.
    /// Va invocato solo in posizione di tipo (dopo AS/parametro/campo), dove '&lt;' non può
    /// essere un operatore di confronto: è quindi sicuro consumarlo qui, prima che qualsiasi
    /// altra logica di parsing degli attributes (`ParseStatementAttributes`/`ParseAttributes`)
    /// abbia la possibilità di intercettarlo e fallire su un token non-identifier come DOUBLE.
    /// </summary>
    private DataType ParseGenericTypeArgumentsSuffix(UnresolvedQualifiedType baseType)
    {
        if (!Check(TokenKind.LessThan))
        {
            return baseType;
        }

        Advance(); // Consuma '<'
        var typeArguments = ImmutableArray.CreateBuilder<DataType>();
        do
        {
            typeArguments.Add(ParseTypeName());
        } while (Match(TokenKind.Comma));

        Consume(TokenKind.GreaterThan, "Expected '>' after generic type arguments.");

        return baseType with { TypeArguments = typeArguments.ToImmutable() };
    }

    /// <summary>
    /// Parsa un elemento di tipo tupla, supportando sia tipi semplici che campi nominati (Nome AS Tipo).
    /// Riconosce il pattern: Tipo | Nome AS Tipo
    /// </summary>
    private TupleField ParseTupleElementType()
    {
        // Se il prossimo token è un tipo primitivo o un identificatore, parsalo come tipo
        var current = Peek();

        // Se è un tipo primitivo (INTEGER, STRING, ecc.), parsalo direttamente (campo anonimo)
        if (IsPrimitiveTypeToken(current.Kind))
        {
            return new TupleField(null, ParseTypeName());
        }

        // Se è un identificatore, potrebbe essere un tipo definito dall'utente
        // o un nome di campo seguito da AS
        if (current.Kind == TokenKind.Identifier && !IsStructuralKeyword(current.Kind))
        {
            // Guarda il token successivo: se è AS, è un campo nominato
            if (At(1).Kind == TokenKind.As)
            {
                // Campo nominato: Nome AS Tipo
                var nameToken = Advance(); // Consuma il nome
                Advance(); // Consuma AS
                var fieldType = ParseTypeName(); // Parsa il tipo
                return new TupleField(nameToken.Lexeme, fieldType);
            }
            else
            {
                // È un tipo definito dall'utente (campo anonimo)
                return new TupleField(null, ParseTypeName());
            }
        }

        // Per keyword non-structural (es. MIN, MAX usati come nomi di campo)
        // o altri casi, prova a parsare come Nome AS Tipo
        if (!IsAtEnd() && current.Kind != TokenKind.RightParen && current.Kind != TokenKind.Comma)
        {
            // Guarda se dopo c'è AS
            if (At(1).Kind == TokenKind.As)
            {
                var nameToken = Advance(); // Consuma il nome
                Advance(); // Consuma AS
                var fieldType = ParseTypeName(); // Parsa il tipo
                return new TupleField(nameToken.Lexeme, fieldType);
            }
        }

        // Fallback: prova a parsare come tipo semplice (campo anonimo)
        return new TupleField(null, ParseTypeName());
    }

    /// <summary>
    /// Controlla se un token kind è un tipo primitivo.
    /// </summary>
    private static bool IsPrimitiveTypeToken(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.StringType or TokenKind.Single or TokenKind.Double or TokenKind.Currency
                or TokenKind.Bit or TokenKind.Bool or TokenKind.Boolean or TokenKind.Byte
                or TokenKind.SByte or TokenKind.Integer or TokenKind.Short or TokenKind.Int
                or TokenKind.UInteger or TokenKind.UShort or TokenKind.UInt
                or TokenKind.Long or TokenKind.ULong or TokenKind.Quad or TokenKind.UQuad
                or TokenKind.Date or TokenKind.Time or TokenKind.DateTime or TokenKind.TimeSpan
                or TokenKind.Void or TokenKind.Any or TokenKind.Json
                => true,
            _ => false
        };
    }

    /// <summary>
    /// Controlla se un token kind corrisponde a un tipo built-in riconosciuto dal parser.
    /// Restituisce true per tipi primitivi (STRING, INTEGER, DOUBLE, ecc.) e collezioni (LIST, DICT, ecc.).
    /// Restituisce false per keyword che non sono tipi (es. POINT, SHAPE, CIRCLE).
    /// </summary>
    private static bool IsBuiltinTypeToken(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.Array or TokenKind.List or TokenKind.Dict or TokenKind.HashSet or TokenKind.Hll
                or TokenKind.Queue or TokenKind.Stack or TokenKind.Sorted or TokenKind.Tree
                or TokenKind.AVLTree or TokenKind.RedBlack or TokenKind.BITree or TokenKind.Trie
                or TokenKind.LinkedList or TokenKind.DoublyLinkedList or TokenKind.Circular
                or TokenKind.Heap or TokenKind.MaxHeap or TokenKind.Bag or TokenKind.Cache
                or TokenKind.Graph or TokenKind.Timeline or TokenKind.HashRing or TokenKind.RingBuffer
                or TokenKind.SparseSet or TokenKind.BPlusTree or TokenKind.Channel
                or TokenKind.StringBuilder or TokenKind.Regex
                or TokenKind.StringType or TokenKind.Single or TokenKind.Double or TokenKind.Currency
                or TokenKind.Bit or TokenKind.Bool or TokenKind.Boolean or TokenKind.Byte
                or TokenKind.SByte or TokenKind.Integer or TokenKind.Short or TokenKind.Int
                or TokenKind.UInteger or TokenKind.UShort or TokenKind.UInt
                or TokenKind.Long or TokenKind.ULong or TokenKind.Quad or TokenKind.UQuad
                or TokenKind.Date or TokenKind.Time or TokenKind.DateTime or TokenKind.TimeSpan
                or TokenKind.Void or TokenKind.Any or TokenKind.Json or TokenKind.Waitgroup
                or TokenKind.Mutex or TokenKind.RwMutex or TokenKind.Once or TokenKind.Atomic or TokenKind.Context or TokenKind.Cond or TokenKind.ConcurrentMap or TokenKind.DisplayModeType or TokenKind.RgbaType or TokenKind.Err
                or TokenKind.HttpConfig or TokenKind.CookieJar or TokenKind.HttpOptions or TokenKind.HttpHeaders
                or TokenKind.HttpRequest or TokenKind.HttpResponse or TokenKind.HttpFetchResult
                or TokenKind.HttpNextHandler or TokenKind.WebSocket or TokenKind.SqlResultSet
                or TokenKind.HttpPostedData or TokenKind.FormField or TokenKind.FileField
                or TokenKind.TBrowse or TokenKind.TBColumn
                or TokenKind.Nil
                => true,
            _ => false
        };
    }

    // ========================================================================
    // --- Metodi Helper Minimali per la Sintassi Generica (FASE 1) ---
    // ========================================================================

    /// <summary>
    /// Parsa un tipo stringa, gestendo la lunghezza fissa opzionale (es. STRING * 80).
    /// </summary>
    /// <returns>Un <see cref="StringType"/>.</returns>
    private StringType ParseStringType(Token stringToken)
    {
        if (!Match(TokenKind.Multiply)) return new StringType(null, stringToken);
        var lengthToken = Consume(TokenKind.Number, "Expected a number for the fixed string length.");
        if (lengthToken.Literal is not double len || len < 1)
            throw CreateError(lengthToken, "Invalid fixed string length.");
        int? fixedLength = (int)len;
        return new StringType(fixedLength, stringToken);
    }

    /// <summary>
    /// [FASE 1 HELPER] Parsa un tipo di dato primitivo e restituisce l'istanza corretta.
    /// </summary>
    private DataType ParseBaseType(Token typeToken)
    {
        // La logica interna ora usa il typeToken passato per istanziare i tipi.
        return typeToken.Kind switch
        {
            TokenKind.StringType => ParseStringType(typeToken),
            TokenKind.Single => new SingleType(typeToken),
            TokenKind.Double => new DoubleType(typeToken),
            TokenKind.Currency => new CurrencyType(typeToken),
            TokenKind.Bit or TokenKind.Bool or TokenKind.Boolean => new BitType(typeToken),
            TokenKind.Byte => new ByteType(typeToken),
            TokenKind.SByte => new SByteType(typeToken),
            TokenKind.Integer or TokenKind.Short or TokenKind.Int => new IntegerType(typeToken),
            TokenKind.UInteger or TokenKind.UShort or TokenKind.UInt => new UIntegerType(typeToken),
            TokenKind.Long => new LongType(typeToken),
            TokenKind.ULong => new ULongType(typeToken),
            TokenKind.Quad => new QuadType(typeToken),
            TokenKind.UQuad => new UQuadType(typeToken),
            TokenKind.Date => new DateType(typeToken),
            TokenKind.Time => new TimeType(typeToken),
            TokenKind.DateTime => new DateTimeType(typeToken),
            TokenKind.TimeSpan => new TimeSpanType(typeToken),
            TokenKind.Void => new VoidType(typeToken),
            TokenKind.Any => new AnyType(typeToken),
            TokenKind.Json => new JsonType(typeToken),
            TokenKind.Waitgroup => new WaitGroupType(typeToken),
            TokenKind.Mutex => new MutexType(typeToken),
            TokenKind.RwMutex => new RwMutexType(typeToken),
            TokenKind.Once => new OnceType(typeToken),
            TokenKind.Atomic => new AtomicType(typeToken),
            TokenKind.Context => new ContextType(typeToken),
            TokenKind.Cond => new CondType(typeToken),
            TokenKind.ConcurrentMap => new ConcurrentMapType(typeToken),
            TokenKind.DisplayModeType => new DisplayModeType(typeToken),
            TokenKind.RgbaType => new RgbaColorType(typeToken),
            TokenKind.Err => new ErrType(typeToken),
            TokenKind.HttpConfig => new HttpConfigType(typeToken),
            TokenKind.CookieJar => new CookieJarType(typeToken),
            TokenKind.HttpOptions => new HttpOptionsType(typeToken),
            TokenKind.HttpHeaders => new HttpHeadersType(typeToken),
            TokenKind.HttpRequest => new HttpRequestType(typeToken),
            TokenKind.HttpResponse => new HttpResponseType(typeToken),
            TokenKind.HttpFetchResult => new FetchResultType(typeToken),
            TokenKind.SqlResultSet => new SqlResultSetType(typeToken),
            TokenKind.HttpNextHandler => new NextHandlerType(typeToken),
            TokenKind.WebSocket => new WebSocketType(typeToken),
            TokenKind.HttpPostedData => new HttpPostedDataType(typeToken),
            TokenKind.FormField => new FormFieldType(typeToken),
            TokenKind.FileField => new FileFieldType(typeToken),
            TokenKind.Trie => new TrieType(typeToken),
            TokenKind.SparseSet => new SparseSetType(typeToken),
            TokenKind.TBrowse => new TBrowseType(typeToken),
            TokenKind.TBColumn => new TBColumnType(typeToken),
            TokenKind.Nil => new NilType(typeToken),
            _ => throw CreateError(typeToken, $"Tipo primitivo non riconosciuto: {typeToken.Kind}")
        };
    }

    /// <summary>
    /// [FASE 1 HELPER] Parsa la sintassi generica per collezioni a singolo tipo (es. LIST OF STRING / LIST<STRING>).
    /// Questa versione rende opzionale la clausola generica per supportare i tipi "raw" necessari 
    /// all'operatore IS
    /// </summary>
    /// <param name="constructor">Delegato che istanzia il tipo concreto di collezione.</param>
    /// <returns>Il DataType della collezione, con tipo elemento specifico o inferito.</returns>
    private DataType ParseSingleTypeCollection(Func<DataType, DataType> constructor)
    {
        bool useAngles = Match(TokenKind.LessThan);
        bool useOf = !useAngles && Match(TokenKind.Of);

        // Se non vengono trovati '<' o 'OF', usiamo AnyType invece di InferredType.
        // Questo garantisce che 'LIST' venga risolto immediatamente come 'LIST OF ANY'.
        if (!useAngles && !useOf)
        {
            return constructor(new AnyType());
        }

        var elementType = ParseTypeName();

        if (useAngles)
        {
            Consume(TokenKind.GreaterThan, "Expected '>' to close generic type.");
        }

        return constructor(elementType);
    }

    /// <summary>
    /// [FASE 1 HELPER] Parsa la sintassi generica per collezioni a coppia chiave-valore (es. DICT OF STRING, LONG).
    /// Analogamente a ParseSingleTypeCollection, supporta ora i tipi "raw" senza clausola OF.
    /// </summary>
    /// <param name="constructor">Delegato che istanzia il tipo concreto di mappa.</param>
    /// <returns>Il DataType della mappa risolto o inferito.</returns>
    private DataType ParseKeyValueCollection(Func<DataType, DataType, DataType> constructor)
    {
        bool useAngles = Match(TokenKind.LessThan);
        bool useOf = !useAngles && Match(TokenKind.Of);

        if (!useAngles && !useOf)
        {
            // 'DICT' diventa 'DICT OF ANY, ANY'.
            return constructor(new AnyType(), new AnyType());
        }

        var keyType = ParseTypeName();
        Consume(TokenKind.Comma, "Expected ',' to separate key and value types.");
        var valueType = ParseTypeName();

        if (useAngles)
        {
            Consume(TokenKind.GreaterThan, "Expected '>' to close generic type.");
        }

        return constructor(keyType, valueType);
    }

    // --- Definizione di tutti i tipi di collezione (utilizzando i nuovi helper) ---
    private ListType ParseListType() => (ListType)ParseSingleTypeCollection(et => new ListType(et));
    private QueueType ParseQueueType() => (QueueType)ParseSingleTypeCollection(et => new QueueType(et));
    private StackType ParseStackType() => (StackType)ParseSingleTypeCollection(et => new StackType(et));

    private HashSetType ParseHashSetType() =>
        (HashSetType)ParseSingleTypeCollection(et => new HashSetType(et));

    private DictType ParseDictType() =>
        (DictType)ParseKeyValueCollection((kt, vt) => new DictType(kt, vt));

    private SortedDictType ParseSortedDictType() =>
        (SortedDictType)ParseKeyValueCollection((kt, vt) => new SortedDictType(kt, vt));

    private ChannelType ParseChannelType()
    {
        // DIM ch AS CHANNEL OF INTEGER [(capacity)] — parità Go make(chan T, n).
        // Direzione opzionale (parità Go chan direzionali): CHANNEL SEND OF T (chan<- T,
        // solo SEND) / CHANNEL RECEIVE OF T (<-chan T, solo RECEIVE), default bidirezionale.
        var direction = ChannelDirection.Bidirectional;
        if (Check(TokenKind.Send)) { Advance(); direction = ChannelDirection.SendOnly; }
        else if (Check(TokenKind.Receive)) { Advance(); direction = ChannelDirection.ReceiveOnly; }

        var ct = (ChannelType)ParseSingleTypeCollection(et => new ChannelType(et, 0, direction));
        if (Match(TokenKind.LeftParen))
        {
            var capacityExpr = ParseExpression();
            Consume(TokenKind.RightParen, "Expected ')' to close CHANNEL capacity.");
            var capacity = capacityExpr is NumberExpr ne
                ? (int)ne.Value
                : throw CreateError(Peek(), "CHANNEL capacity must be a constant integer.");
            if (capacity < 0)
                throw CreateError(Peek(), "CHANNEL capacity cannot be negative.");
            return ct with { Capacity = capacity };
        }
        return ct;
    }

    private TreeType ParseTreeType() =>
        (TreeType)ParseKeyValueCollection((kt, vt) => new TreeType(kt, vt));

    private AVLTreeType ParseAvlTreeType() =>
        (AVLTreeType)ParseKeyValueCollection((kt, vt) => new AVLTreeType(kt, vt));

    private RedBlackTreeType ParseRedBlackTreeType() =>
        (RedBlackTreeType)ParseKeyValueCollection((kt, vt) => new RedBlackTreeType(kt, vt));

    private BinaryIndexedTreeType ParseBiTreeType() =>
        (BinaryIndexedTreeType)ParseSingleTypeCollection(et => new BinaryIndexedTreeType(et));

    private LinkedListType ParseLinkedListType() =>
        (LinkedListType)ParseSingleTypeCollection(et => new LinkedListType(et));

    private DoublyLinkedListType ParseDoublyLinkedListType() =>
        (DoublyLinkedListType)ParseSingleTypeCollection(et => new DoublyLinkedListType(et));

    private CircularLinkedListType ParseCircularLinkedListType() =>
        (CircularLinkedListType)ParseSingleTypeCollection(et => new CircularLinkedListType(et));

    private BinaryHeapType ParseHeapType(bool isMaxHeap) =>
        (BinaryHeapType)ParseSingleTypeCollection(et => new BinaryHeapType(et, isMaxHeap));

    private BagType ParseBagType() => (BagType)ParseSingleTypeCollection(et => new BagType(et));

    private CacheType ParseCacheType() =>
        (CacheType)ParseKeyValueCollection((kt, vt) => new CacheType(kt, vt));

    private GraphType ParseGraphType() => (GraphType)ParseSingleTypeCollection(et => new GraphType(et));

    private TimelineType ParseTimelineType() =>
        (TimelineType)ParseSingleTypeCollection(et => new TimelineType(et));

    private HashRingType ParseHashRingType() =>
        (HashRingType)ParseSingleTypeCollection(et => new HashRingType(et));

    private RingBufferType ParseRingBufferType() =>
        (RingBufferType)ParseSingleTypeCollection(et => new RingBufferType(et));

    private BPlusTreeType ParseBPlusTreeType() =>
        (BPlusTreeType)ParseKeyValueCollection((kt, vt) => new BPlusTreeType(kt, vt));

    /// <summary>
    /// [FASE 1] Parsa espressioni letterali (numeri, stringhe, booleani) ed espressioni raggruppate tra parentesi.
    /// </summary>
    private Expr? ParseLiteralOrGrouping()
    {
        if (Match(TokenKind.Number) || Match(TokenKind.LineNumber))
        {
            return new NumberExpr((double)Previous().Literal!, Previous());
        }

        if (Match(TokenKind.InterpolatedStringLiteral))
        {
            var token = Previous();
            var template = (string)token.Literal!;
            var segments = ParseInterpolatedStringSegments(template, token);
            return new InterpolatedStringExpr(token, segments, token)
            {
                Template = template
            };
        }

        if (Match(TokenKind.StringLiteral))
        {
            var firstToken = Previous();
            var combinedValue = (string)firstToken.Literal!;
            var stringBuilder = new System.Text.StringBuilder(combinedValue);

            // Loop per consumare e fondere eventuali letterali stringa successivi.
            while (Check(TokenKind.StringLiteral))
            {
                var nextToken = Advance();
                // NOTA: La specifica originale prevedeva la fusione senza spazi.
                // "Ciao" "Mondo" -> "CiaoMondo"
                // Se si vuole uno spazio, la sorgente deve essere "Ciao" " Mondo".
                stringBuilder.Append((string)nextToken.Literal!);
            }

            // Se sono state fatte concatenazioni, crea un nuovo StringExpr
            // usando il valore combinato ma mantenendo il token originale per la posizione.
            var finalValue = stringBuilder.ToString();
            return finalValue.Length != combinedValue.Length
                ? new StringExpr(finalValue, firstToken)
                :
                // Se non ci sono state concatenazioni, restituisce il nodo originale.
                new StringExpr(combinedValue, firstToken);
        }

        if (Match(TokenKind.True))
        {
            return new BooleanExpr(true, Previous());
        }

        if (Match(TokenKind.False))
        {
            return new BooleanExpr(false, Previous());
        }
        
        if (Match(TokenKind.Nil))
        {
            return new NilExpr(Previous());
        }

        return null;

    }

    /// <summary>
    /// Segmenta il template raw di una stringa interpolata `$"..."` in letterali e sotto-espressioni,
    /// parsando ciascun `{expr}` (o `{expr:format}`) come un vero nodo <see cref="Expr"/> tramite il
    /// parser JIT (<see cref="ParseSingleExpression"/>). Il template usa ''/'' per le
    /// graffe letterali escaped ('{{'/'}}') prodotte dal lexer (<c>ScanInterpolatedString</c>).
    /// </summary>
    private ImmutableArray<InterpolatedStringSegment> ParseInterpolatedStringSegments(string template, Token dollarToken)
    {
        var segments = ImmutableArray.CreateBuilder<InterpolatedStringSegment>();
        var literalBuilder = new System.Text.StringBuilder();

        void FlushLiteral()
        {
            if (literalBuilder.Length == 0) return;
            segments.Add(new InterpolatedStringLiteralSegment(literalBuilder.ToString(), dollarToken));
            literalBuilder.Clear();
        }

        var originalSourceLine = _sourceMap[_currentSourceLineIndex];

        var i = 0;
        while (i < template.Length)
        {
            var c = template[i];

            if (c == '') { literalBuilder.Append('{'); i++; continue; }
            if (c == '') { literalBuilder.Append('}'); i++; continue; }

            if (c == '{')
            {
                FlushLiteral();

                var depth = 1;
                var exprStart = i + 1;
                var j = exprStart;
                var colonIdx = -1;
                while (j < template.Length && depth > 0)
                {
                    var cj = template[j];
                    if (cj is '{' or '(' or '[') depth++;
                    else if (cj is '}' or ')' or ']')
                    {
                        depth--;
                        if (depth == 0) break;
                    }
                    else if (cj == ':' && depth == 1 && colonIdx < 0)
                    {
                        colonIdx = j;
                    }
                    j++;
                }

                if (j >= template.Length)
                {
                    throw CreateError(dollarToken,
                        "Espressione interpolata non terminata in una stringa `$\"...\"` (manca la '}' di chiusura).");
                }

                string exprText;
                string? format = null;
                if (colonIdx >= 0)
                {
                    exprText = template.Substring(exprStart, colonIdx - exprStart);
                    format = template.Substring(colonIdx + 1, j - colonIdx - 1);
                }
                else
                {
                    exprText = template.Substring(exprStart, j - exprStart);
                }

                if (string.IsNullOrWhiteSpace(exprText))
                {
                    throw CreateError(dollarToken,
                        "Espressione interpolata vuota ('{}') in una stringa `$\"...\"`.");
                }

                var baseColumn = dollarToken.Position.Column + 2 + exprStart;
                var innerExpr = ParseSingleExpression(exprText, _program, originalSourceLine, baseColumn, _registry);

                segments.Add(new InterpolatedStringExprSegment(innerExpr, format, dollarToken));

                i = j + 1;
            }
            else
            {
                literalBuilder.Append(c);
                i++;
            }
        }

        FlushLiteral();
        return segments.ToImmutable();
    }

    /// <summary>
    /// [FASE 1 - VERSIONE DEFINITIVA] Parsa il corpo di un'espressione racchiusa tra graffe '{...}'.
    /// Utilizza uno scanner di lookahead robusto per disambiguare tra:
    /// 1. Code Block (`{ |... }`)
    /// 2. Comprensioni (`{ ... FOR ... }`)
    /// 3. Letterale Dizionario (`{ key:value, ... }`)
    /// 4. Letterale Array (fallback)
    /// </summary>
    private Expr ParseBraceEnclosedLiteralBody(Token openBrace)
    {
        // Caso 1: Blocco vuoto {} è sempre un ArrayLiteral vuoto.
        if (Check(TokenKind.RightBrace))
        {
            Consume(TokenKind.RightBrace, "Expected '}' to close empty literal.");
            return new ArrayLiteralExpr(ImmutableArray<Expr>.Empty, openBrace);
        }

        // Caso 2: Code Block Clipper. Il primo token è inequivocabile. `||` (lista
        // parametri VUOTA, `{ || 3.14 }`) è un token BitOr — il lexer fonde la doppia
        // barra — quindi va accettato qui come l'equivalente di due Bar.
        if (Check(TokenKind.Bar) || Check(TokenKind.BitOr))
        {
            return ParseCodeBlockBody(openBrace);
        }

        // *** CORREZIONE CRITICA ***
        // Consuma tutti i newline iniziali DOPO i controlli iniziali ma PRIMA del lookahead.
        while (Match(TokenKind.Newline))
        {
        }

        // Se dopo i newline è vuoto, lo gestiamo di nuovo per sicurezza.
        if (Check(TokenKind.RightBrace))
        {
            Consume(TokenKind.RightBrace, "Expected '}' to close empty literal.");
            return new ArrayLiteralExpr(ImmutableArray<Expr>.Empty, openBrace);
        }

        // --- LOOKAHEAD SCANNER ROBUSTO A SINGOLO PASSAGGIO ---
        var (isComprehension, isDictSyntax) = ScanBraceContent();

        // --- DISPATCHING CORRETTO CON IF/ELSE IF ---

        if (isComprehension)
        {
            // Se è una comprensione, la sintassi k:v o solo 'e' determina il tipo.
            var isDictComprehension = false;
            var parenDepth = 0;
            for (var i = 0;; i++)
            {
                var lookaheadToken = At(i);
                if (lookaheadToken.Kind == TokenKind.For && parenDepth == 0) break;
                if (lookaheadToken.Kind == TokenKind.DictColon && parenDepth == 0)
                {
                    isDictComprehension = true;
                    break;
                }

                if (lookaheadToken.Kind is TokenKind.RightBrace or TokenKind.Eof) break;
                if (lookaheadToken.Kind is TokenKind.LeftParen or TokenKind.LeftBrace or TokenKind.LeftBracket)
                    parenDepth++;
                else if (lookaheadToken.Kind is TokenKind.RightParen or TokenKind.RightBrace or TokenKind.RightBracket)
                    parenDepth--;
            }

            if (isDictComprehension)
            {
                var keyExpr = ParseExpression();
                Consume(TokenKind.DictColon, "Expected ':' in dictionary comprehension.");
                var valueExpr = ParseExpression();
                var clauses = ParseComprehensionClauses();
                Consume(TokenKind.RightBrace, "Expected '}' to close dictionary comprehension.");
                return new DictComprehensionExpr(keyExpr, valueExpr, clauses, openBrace);
            }
            else
            {
                var elementExpr = ParseExpression();
                var clauses = ParseComprehensionClauses();
                Consume(TokenKind.RightBrace, "Expected '}' to close set comprehension.");
                return new SetComprehensionExpr(elementExpr, clauses, openBrace);
            }
        }
        else if (isDictSyntax)
        {
            // È un letterale dizionario
            var entries = ImmutableArray.CreateBuilder<DictionaryEntry>();
            do
            {
                if (Check(TokenKind.RightBrace)) break;

                while (Match(TokenKind.Newline))
                {
                } // Pulisce i newline prima di ogni entry

                if (Check(TokenKind.RightBrace)) break; // Per virgole finali

                Expr key;
                if (IsValidNameToken(Peek())
                    && (At(1).Kind == TokenKind.DictColon || At(1).Kind == TokenKind.Equal)
                    && Peek().Kind != TokenKind.StringLiteral)
                {
                    var idToken = Advance();
                    key = new StringExpr(idToken.Lexeme, idToken);
                }
                else
                {
                    key = ParseExpression();
                }

                if (!Match(TokenKind.DictColon))
                    Consume(TokenKind.Equal, "Expected ':' or '=' in dictionary literal.");
                var value = ParseExpression();
                entries.Add(new DictionaryEntry(key, value));

                while (Match(TokenKind.Newline))
                {
                } // Pulisce i newline dopo ogni entry
            } while (Match(TokenKind.Comma));

            Consume(TokenKind.RightBrace, "Expected '}' to close dictionary literal.");
            return new DictionaryLiteralExpr(entries.ToImmutable(), openBrace);
        }
        else
        {
            // Se non è nessuna delle precedenti, è un Array/Set literal.
            var elements = ParseExpressionList(TokenKind.RightBrace);
            Consume(TokenKind.RightBrace, "Expected '}' to close array/set literal.");
            return new ArrayLiteralExpr(elements, openBrace);
        }
    }

    /// <summary>
    /// [HELPER SCANNER CON LOG] Esegue una scansione non distruttiva dei token.
    /// </summary>
    private (bool isComprehension, bool isDictSyntax) ScanBraceContent()
    {
        Log($"[DEBUG-SCAN] --- Starting Lookahead Scan from token: {Peek()} ---");
        var hasFor = false;
        var hasColon = false;
        var hasEqual = false;
        var parenDepth = 0;

        for (var i = 0;; i++)
        {
            var token = At(i);
            Log($"[DEBUG-SCAN]   - Scanning token[{i}]: {token} (ParenDepth: {parenDepth})");

            if (token.Kind is TokenKind.RightBrace or TokenKind.Eof)
            {
                Log($"[DEBUG-SCAN]   - Found end of block/file. Stopping scan.");
                break;
            }

            if (parenDepth == 0)
            {
                if (token.Kind == TokenKind.For)
                {
                    hasFor = true;
                    Log(
                        $"[DEBUG-SCAN]   - Found 'FOR' at top level. Marked as Comprehension and stopping primary scan.");
                    break;
                }

                if (token.Kind == TokenKind.DictColon)
                {
                    hasColon = true;
                    Log($"[DEBUG-SCAN]   - Found ':' at top level. Marked as potential Dict syntax.");
                }

                // Clipper `{ chiave = valore }`: `=` singolo a livello top = separatore di entry
                // (parità frontend C native gmb_parser_expressions.c). `==` (ExactlyEqual) NON conta.
                if (token.Kind == TokenKind.Equal)
                {
                    hasEqual = true;
                    Log($"[DEBUG-SCAN]   - Found '=' at top level. Marked as potential Dict syntax.");
                }
            }

            if (token.Kind is TokenKind.LeftParen or TokenKind.LeftBrace or TokenKind.LeftBracket) parenDepth++;
            else if (token.Kind is TokenKind.RightParen or TokenKind.RightBrace or TokenKind.RightBracket) parenDepth--;
        }

        if (hasFor)
        {
            var hasColonBeforeFor = false;
            parenDepth = 0;
            Log($"[DEBUG-SCAN] --- Comprehension detected. Starting secondary scan for ':' before 'FOR'. ---");
            for (var i = 0;; i++)
            {
                var token = At(i);
                Log($"[DEBUG-SCAN]   - SubScan token[{i}]: {token} (ParenDepth: {parenDepth})");
                if (parenDepth == 0 && token.Kind == TokenKind.For)
                {
                    Log($"[DEBUG-SCAN]   - Found 'FOR'. Secondary scan finished.");
                    break;
                }

                if (parenDepth == 0 && token.Kind == TokenKind.DictColon)
                {
                    hasColonBeforeFor = true;
                    Log(
                        $"[DEBUG-SCAN]   - Found ':' before 'FOR'. Marked as Dict Comprehension and stopping secondary scan.");
                    break;
                }

                if (token.Kind is TokenKind.RightBrace or TokenKind.Eof) break;
                if (token.Kind is TokenKind.LeftParen or TokenKind.LeftBrace or TokenKind.LeftBracket) parenDepth++;
                else if (token.Kind is TokenKind.RightParen or TokenKind.RightBrace or TokenKind.RightBracket)
                    parenDepth--;
            }

            return (true, hasColonBeforeFor);
        }

        return (false, hasColon || hasEqual);
    }

    /// <summary>
    /// [FASE 1 - NUOVO HELPER] Parsa una sequenza di una o più clausole di comprensione (FOR...IN...[IF]).
    /// </summary>
    private ImmutableArray<ComprehensionClause> ParseComprehensionClauses(bool isFirstForAlreadyConsumed = false)
    {
        var clauses = ImmutableArray.CreateBuilder<ComprehensionClause>();
        var first = true;
        do
        {
            if (first && isFirstForAlreadyConsumed)
            {
                first = false;
                // FOR token già consumato dal chiamante
            }
            else
            {
                Consume(TokenKind.For, "Expected 'FOR' in comprehension clause.");
            }
            first = false;
            var iteratorVar = Consume(TokenKind.Identifier, "Expected iterator variable after 'FOR'.");
            Consume(TokenKind.In, "Expected 'IN' after iterator variable.");
            var sourceExpr = ParseExpression();
            Expr? conditionExpr = null;
            if (Match(TokenKind.If))
            {
                conditionExpr = ParseExpression();
            }

            clauses.Add(new ComprehensionClause(-1, iteratorVar, sourceExpr, conditionExpr));
        } while (Check(TokenKind.For));

        return clauses.ToImmutable();
    }

    /// <summary>`ORDER BY <expr> [DESC|ASC] (, <expr> ...)` opzionale (0 o più), condiviso dalla
    /// comprehension letterale `[ ... ORDER BY ... ]`. Vuoto se assente. Stessa semantica del
    /// parser LINQ (ParseLinqQuery).</summary>
    private ImmutableArray<LinqOrderByClause> ParseOptionalOrderByClauses()
    {
        if (!(Check(TokenKind.Order) && At(1).Kind == TokenKind.By))
            return ImmutableArray<LinqOrderByClause>.Empty;
        Advance(); // ORDER
        Advance(); // BY
        var orderBys = ImmutableArray.CreateBuilder<LinqOrderByClause>();
        do
        {
            var orderExpr = ParseExpression();
            bool descending = Match(TokenKind.Desc);
            if (!descending) Match(TokenKind.Asc);
            orderBys.Add(new LinqOrderByClause(orderExpr, descending));
        } while (Match(TokenKind.Comma));
        return orderBys.ToImmutable();
    }
    
    /// <summary>
    /// [FASE 1] Parsa un'espressione che inizia con '[', distinguendo tra un
    /// letterale lista e una comprensione di lista tramite lookahead.
    /// Aggiornato per supportare le Comprensioni Annidate (multiple clausole FOR).
    /// </summary>
    private Expr ParseListOrComprehension(Token openBracket)
    {
        // Controlla se è una lista vuota '[]'
        if (Check(TokenKind.RightBracket))
        {
            Consume(TokenKind.RightBracket, "Expected ']' to close empty list literal.");
            return new ListLiteralExpr([], openBracket) { SourceLineIndex = _currentSourceLineIndex };
        }

        // Esegue un lookahead non distruttivo per la keyword 'FOR' dopo la prima espressione
        var isComprehension = false;
        var savedPosition = _current;
        try
        {
            ParseExpression(); // Parsa la prima parte (output expression o primo elemento)
            if (Check(TokenKind.For))
            {
                isComprehension = true;
            }
        }
        finally
        {
            _current = savedPosition; // Ripristina sempre la posizione
        }

        if (isComprehension)
        {
            var outputExpr = ParseExpression();
            var clauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

            // Parsa una o più clausole FOR ... IN ... [IF ...]
            // Il lookahead ha garantito che c'è almeno un FOR.
            do
            {
                Consume(TokenKind.For, "Expected 'FOR' in list comprehension.");
                var iteratorVar = Consume(TokenKind.Identifier, "Expected iterator variable after 'FOR'.");
                // In Fase 1, non registriamo il simbolo, quindi usiamo un ID fittizio.
                const int iteratorSymbolId = -1;
                
                Consume(TokenKind.In, "Expected 'IN' after iterator variable.");
                var sourceExpr = ParseExpression();
                
                Expr? conditionExpr = null;
                if (Match(TokenKind.If))
                {
                    conditionExpr = ParseExpression();
                }

                clauses.Add(new ComprehensionClause(iteratorSymbolId, iteratorVar, sourceExpr, conditionExpr));

            } while (Check(TokenKind.For));

            // ORDER BY opzionale nella forma LETTERALE `[ out FOR v IN src [IF c] ORDER BY k [DESC/ASC], ... ]`
            // (parità con la forma LINQ `FROM ... ORDER BY ... SELECT`). Prima la forma letterale
            // non lo parsava ("Expected ']' to end list comprehension").
            var orderBys = ParseOptionalOrderByClauses();

            Consume(TokenKind.RightBracket, "Expected ']' to end list comprehension.");
            
            return new ListComprehensionExpr(outputExpr, clauses.ToImmutable(), openBracket, orderBys) 
                { SourceLineIndex = _currentSourceLineIndex };
        }

        // È un letterale lista
        var elements = ParseExpressionList(TokenKind.RightBracket);
        Consume(TokenKind.RightBracket, "Expected ']' to close list literal.");
        return new ListLiteralExpr(elements, openBracket) { SourceLineIndex = _currentSourceLineIndex };
    }

}