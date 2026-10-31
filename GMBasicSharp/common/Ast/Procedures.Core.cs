// FILE: Ast.Procedures.Core.cs

using System.Diagnostics.CodeAnalysis;

namespace Basic;

/// <summary>
/// Rappresenta in modo univoco la firma di una procedura (SUB o FUNCTION).
/// L'uguaglianza è basata sul nome originale della procedura e sui tipi dei parametri.
/// </summary>
[JsonDerivedType(typeof(ProcedureSignature), nameof(ProcedureSignature))]
public record ProcedureSignature
{
    /// <summary>
    /// Ottiene il nome originale (non decorato) della procedura.
    /// </summary>
    public string Name { get; }

    /// <summary>
    /// Ottiene il numero totale di parametri.
    /// </summary>
    public int ParameterCount { get; }

    /// <summary>
    /// Ottiene una lista ordinata dei tipi di dato di ciascun parametro.
    /// </summary>
    public ImmutableArray<DataType> ParameterTypes { get; }

    /// <summary>
    /// Una rappresentazione testuale stabile e univoca della firma, usata per l'uguaglianza e l'hashing.
    /// </summary>
    private readonly string _signatureKey;

    /// <summary>
    /// Restituisce la chiave testuale stabile della firma.
    /// </summary>
    public string SignatureKey => _signatureKey;

    /// <summary>
    /// Restituisce la rappresentazione testuale stabile della firma,
    /// usata da SymbolTable.GetQualifiedLabelName per generare le chiavi etichetta.
    /// </summary>
    public override string ToString() => _signatureKey;

    /// <summary>
    /// Inizializza una nuova istanza di ProcedureSignature.
    /// </summary>
    [JsonConstructor]
    public ProcedureSignature(string originalName, ImmutableArray<Parameter> parameters)
    {
        Name = originalName;
        ParameterCount = parameters.Length;
        ParameterTypes = [..parameters.Select(p => p.DataType)];

        // Calcola la chiave stabile usando il nome ORIGINALE e i nomi di tipo NORMALIZZATI.
        // Questo garantisce che `UnresolvedQualifiedType("Prodotto")` e `UserDefinedType("Prodotto")`
        // producano la stessa firma, risolvendo il bug di lookup tra le passate del compilatore.
        _signatureKey = $"{Name.ToUpperInvariant()}:{string.Join(",", ParameterTypes.Select(GetNormalizedTypeName))}";
    }

    /// <summary>
    /// Helper ricorsivo per ottenere un nome di tipo normalizzato e canonico,
    /// indipendente dalla sua rappresentazione AST (es. Unresolved vs. UserDefined).
    /// </summary>
    private static string GetNormalizedTypeName(DataType dt)
    {
        return dt switch
        {
            // --- Tipi di Risoluzione Fondamentali ---
            UnresolvedQualifiedType uqt => uqt.QualifiedName.FullName.ToUpperInvariant(),
            UserDefinedType udt => udt.OriginalName.Lexeme.ToUpperInvariant(),
            ClassType ct => ct.Definition.OriginalName.Lexeme.ToUpperInvariant(),
            InferredType => "<INFERRED>",
            AnyType => "ANY",

            // --- Tipi Contenitore (con gestione ricorsiva) ---
            ArrayType at => $"ARRAY OF {GetNormalizedTypeName(at.ElementType)}",
            ListType lt => $"LIST OF {GetNormalizedTypeName(lt.ElementType)}",
            HashSetType hst => $"HASHSET OF {GetNormalizedTypeName(hst.ElementType)}",
            QueueType qt => $"QUEUE OF {GetNormalizedTypeName(qt.ElementType)}",
            StackType st => $"STACK OF {GetNormalizedTypeName(st.ElementType)}",
            LinkedListType llt => $"LINKEDLIST OF {GetNormalizedTypeName(llt.ElementType)}",
            DoublyLinkedListType dllt => $"DOUBLYLINKEDLIST OF {GetNormalizedTypeName(dllt.ElementType)}",
            CircularLinkedListType cllt => $"CIRCULAR OF {GetNormalizedTypeName(cllt.ElementType)}",
            BagType bt => $"BAG OF {GetNormalizedTypeName(bt.ElementType)}",
            GraphType gt => $"GRAPH OF {GetNormalizedTypeName(gt.NodeType)}",
            TimelineType tlt => $"TIMELINE OF {GetNormalizedTypeName(tlt.ValueType)}",
            HashRingType hrt => $"HASHRING OF {GetNormalizedTypeName(hrt.NodeType)}",
            RingBufferType rbt => $"RINGBUFFER OF {GetNormalizedTypeName(rbt.ElementType)}",
            BinaryIndexedTreeType bitt => $"BITREE OF {GetNormalizedTypeName(bitt.ElementType)}",
            BinaryHeapType bht => $"{(bht.IsMaxHeap ? "MAXHEAP" : "HEAP")} OF {GetNormalizedTypeName(bht.ElementType)}",
            ChannelType chant => $"CHANNEL OF {GetNormalizedTypeName(chant.ValueType)}",
            GeneratorType gent => $"GENERATOR OF {GetNormalizedTypeName(gent.YieldType)}",
            FunctionPointerType fpt =>
                $"CALLBACK ({string.Join(",", fpt.ParameterTypes.Select(GetNormalizedTypeName))}) AS {GetNormalizedTypeName(fpt.ReturnType)}",
            DictType dict => $"DICT OF {GetNormalizedTypeName(dict.KeyType)},{GetNormalizedTypeName(dict.ValueType)}",
            SortedDictType sdt =>
                $"SORTED OF {GetNormalizedTypeName(sdt.KeyType)},{GetNormalizedTypeName(sdt.ValueType)}",
            TreeType tt => $"TREE OF {GetNormalizedTypeName(tt.KeyType)},{GetNormalizedTypeName(tt.ValueType)}",
            AVLTreeType avlt =>
                $"AVLTREE OF {GetNormalizedTypeName(avlt.KeyType)},{GetNormalizedTypeName(avlt.ValueType)}",
            RedBlackTreeType rbt =>
                $"REDBLACK OF {GetNormalizedTypeName(rbt.KeyType)},{GetNormalizedTypeName(rbt.ValueType)}",
            CacheType cachet =>
                $"CACHE OF {GetNormalizedTypeName(cachet.KeyType)},{GetNormalizedTypeName(cachet.ValueType)}",
            BPlusTreeType bpt =>
                $"BPLUSTREE OF {GetNormalizedTypeName(bpt.KeyType)},{GetNormalizedTypeName(bpt.ValueType)}",
            KeyValuePairType kvpt =>
                $"KEYVALUEPAIR OF {GetNormalizedTypeName(kvpt.KeyType)},{GetNormalizedTypeName(kvpt.ValueType)}",

            // --- Tipi Primitivi ---
            SingleType => "SINGLE",
            DoubleType => "DOUBLE",
            CurrencyType => "CURRENCY",
            StringType st => st.ToString().ToUpperInvariant(), // Gestisce STRING * N
            BitType => "BIT",
            ByteType => "BYTE",
            SByteType => "SBYTE",
            IntegerType => "INTEGER",
            UIntegerType => "UINTEGER",
            LongType => "LONG",
            ULongType => "ULONG",
            QuadType => "QUAD",
            UQuadType => "UQUAD",
            DateType => "DATE",
            TimeType => "TIME",
            DateTimeType => "DATETIME",
            TimeSpanType => "TIMESPAN",
            VoidType => "VOID",

            // --- Tipi di Sistema e Speciali ---
            JsonType => "JSON",
            WaitGroupType => "WAITGROUP",
            FutureType => "FUTURE",
            DisplayModeType => "DISPLAYMODE",
            ErrType => "ERR",
            HttpOptionsType => "HTTPOPTIONS",
            HttpResponseType => "HTTPRESPONSE",
            HttpConfigType => "HTTPCONFIG",
            CookieJarType => "COOKIEJAR",
            HttpRequestType => "HTTPREQUEST",
            HttpPostedDataType => "HTTPPOSTEDDATA",
            FetchResultType => "FETCHRESULT",
            SqlResultSetType => "SQLRESULTSET",
            TrieType => "TRIE",
            SparseSetType => "SPARSESET",
            TBrowseType => "TBROWSE",
            TBColumnType => "TBCOLUMN",
            NextHandlerType => "NEXTHANDLER",
            WebSocketType => "WEBSOCKET",

            // --- Tipi di Risultato (Analisi, Finanza, etc.) ---
            AnovaResultType => "ANOVARESULT",
            ArimaResultType => "ARIMARESULT",
            RegressionResultType => "REGRESSIONRESULT",
            KMeansResultType => "KMEANSRESULT",
            HoltWintersResultType => "HOLTWINTERSRESULT",
            AdfTestResultType => "ADFTESTRESULT",
            PcaResultType => "PCARESULT",
            LuDecompositionType => "LUDECOMPOSITION",
            QrDecompositionType => "QRDECOMPOSITION",
            SvdDecompositionType => "SVDDECOMPOSITION",
            EvdDecompositionType => "EVDDECOMPOSITION",
            CholeskyDecompositionType => "CHOLESKYDECOMPOSITION",
            PortfolioResultType => "PORTFOLIORESULT",
            PortfolioConstraintsType => "PORTFOLIOCONSTRAINTS",
            GaParametersType => "GAPARAMETERS",
            BacktestResultType => "BACKTESTRESULT",
            PortfolioMetricsType => "PORTFOLIOMETRICS",
            RebalanceStrategyType => "REBALANCESTRATEGY",

            // Fallback per eventuali tipi non elencati esplicitamente
            _ => dt.ToString().ToUpperInvariant()
        };
    }

    /// <summary>
    /// Determina se l'istanza corrente è uguale a un'altra istanza di ProcedureSignature.
    /// </summary>
    public virtual bool Equals(ProcedureSignature? other)
    {
        if (ReferenceEquals(null, other)) return false;
        if (ReferenceEquals(this, other)) return true;
        return _signatureKey == other._signatureKey;
    }

    /// <summary>
    /// Calcola un hash code basato sul valore, coerente con l'implementazione di Equals.
    /// </summary>
    public override int GetHashCode()
    {
        return _signatureKey.GetHashCode();
    }
}

/// <summary>
/// Classe base astratta per tutte le definizioni di procedure (SUB, FUNCTION, GENERATOR).
/// Ora eredita da Decl.
/// </summary>
public abstract record ProcedureDefinition(
    int SymbolId,
    SymbolFlags Flags,
    Token Name,
    ImmutableArray<Parameter> Parameters,
    ImmutableArray<Stmt> Body,
    bool IsAsync = false,
    ImmutableArray<TypeParameter> TypeParameters = default,
    ImmutableArray<Token> Attributes = default) : Decl(Name.Position)
{
    /// <summary>
    /// Contiene il token originale del nome della procedura, incluso il suffisso di tipo.
    /// </summary>
    public Token OriginalName { get; init; } = Name;

    /// <summary>
    /// Ottiene la firma univoca della procedura, basata sul nome originale e sui tipi dei parametri.
    /// Viene calcolata in modo lazy.
    /// </summary>
    [JsonIgnore]
    [field: AllowNull, MaybeNull]
    public ProcedureSignature Signature => field ??= new ProcedureSignature(OriginalName.Lexeme, Parameters);

    /// <summary>
    /// Ottiene un riferimento al nodo genitore nell'AST.
    /// </summary>
    [JsonIgnore]
    public object? Parent { get; internal set; }

    /// <summary>
    /// Crea una copia profonda di questa definizione di procedura.
    /// </summary>
    public abstract override AstNode DeepClone();

    /// <summary>
    /// Implementa l'uguaglianza personalizzata per ProcedureDefinition.
    /// </summary>
    public virtual bool Equals(ProcedureDefinition? other)
    {
        if (ReferenceEquals(null, other)) return false;
        if (ReferenceEquals(this, other)) return true;

        return GetType() == other.GetType() &&
               SymbolId == other.SymbolId &&
               Flags == other.Flags && // Confronto diretto sui flag
               Name.Equals(other.Name) &&
               OriginalName.Equals(other.OriginalName) &&
               Parameters.SequenceEqual(other.Parameters) &&
               Body.SequenceEqual(other.Body);
    }

    /// <summary>
    /// Genera un codice hash basato sul valore.
    /// </summary>
    public override int GetHashCode()
    {
        var hashCode = new HashCode();
        hashCode.Add(GetType());
        hashCode.Add(SymbolId);
        hashCode.Add(Flags);
        hashCode.Add(Name);
        hashCode.Add(OriginalName);

        foreach (var param in Parameters)
            hashCode.Add(param);

        foreach (var stmt in Body)
            hashCode.Add(stmt);

        return hashCode.ToHashCode();
    }
}

/// <summary>
/// Classifica il requisito di stato di esecuzione di una <see cref="NativeProcedureDefinition"/>:
/// determina SE la sua implementazione può vivere in una libreria portabile (StdLib.Runtime,
/// zero dipendenze da un motore di esecuzione specifico) o se DEVE vivere dentro il motore che la
/// esegue (VM/IL/un futuro Executor), perché ha bisogno di qualcosa che sopravvive oltre la singola
/// chiamata — un handle, un registry, il contesto di un task.
/// <para>Il default è <see cref="Stateful"/> (non <see cref="Pure"/>): una funzione non ancora
/// classificata NON deve essere assunta spostabile — va verificato esplicitamente prima di marcarla
/// Pure. Vedi docs/StdLib-Runtime-Plan.md per il criterio di classificazione e la procedura.</para>
/// </summary>
public enum NativeExecutionKind
{
    /// <summary>
    /// Calcolabile SOLO dagli argomenti: nessun accesso a stato che sopravvive oltre la singola
    /// chiamata (nessun handle aperto, nessun registry, nessun contesto di task/VM). Implementabile
    /// come metodo statico in StdLib.Runtime, riusabile da qualunque motore di esecuzione presente o
    /// futuro tramite un adapter sottile specifico del motore.
    /// </summary>
    Pure,

    /// <summary>
    /// Richiede stato che sopravvive oltre la singola chiamata (handle aperti — file/chiavi
    /// crittografiche/matrici LINALG —, registry, contesto del task corrente, side effect su I/O).
    /// L'implementazione deve vivere dove quello stato è posseduto: il motore di esecuzione (VM,
    /// IL) o un registry condiviso a livello di runtime (es. BRT) se più motori ne hanno bisogno.
    /// </summary>
    Stateful
}

/// <summary>
/// Rappresenta una procedura (FUNCTION o SUB) implementata nativamente in C# (built-in).
/// </summary>
public sealed record NativeProcedureDefinition(
    int SymbolId,
    SymbolFlags Flags,
    Token Name,
    ImmutableArray<Parameter> Parameters,
    DataType ReturnType,
    TokenKind BuiltinKind,
    bool IsFunction = true,
    NativeExecutionKind ExecutionKind = NativeExecutionKind.Stateful
) : ProcedureDefinition(SymbolId, Flags, Name, Parameters, ImmutableArray<Stmt>.Empty)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale che identifica questa come una funzione nativa.
    /// </summary>
    public override string ToString()
    {
        var procType = IsFunction ? "FUNCTION" : "SUB";
        var parameterString = string.Join(", ", Parameters.Select(p => p.OriginalName.Lexeme));
        var returnTypeStr = IsFunction ? $" AS {ReturnType}" : "";
        return $"[NATIVE] {procType} {OriginalName.Lexeme}({parameterString}){returnTypeStr} [{Flags}]";
    }

    /// <summary>
    /// Determina se l'istanza corrente è uguale a un'altra istanza di <see cref="NativeProcedureDefinition"/>.
    /// </summary>
    public bool Equals(NativeProcedureDefinition? other)
    {
        if (ReferenceEquals(null, other)) return false;
        if (ReferenceEquals(this, other)) return true;
        return base.Equals(other) &&
               ReturnType.Equals(other.ReturnType) &&
               BuiltinKind == other.BuiltinKind &&
               IsFunction == other.IsFunction;
    }

    /// <summary>
    /// Calcola un codice hash per l'istanza corrente.
    /// </summary>
    public override int GetHashCode()
    {
        var hashCode = new HashCode();
        hashCode.Add(base.GetHashCode());
        hashCode.Add(ReturnType);
        hashCode.Add(BuiltinKind);
        hashCode.Add(IsFunction);
        return hashCode.ToHashCode();
    }

    /// <summary>
    /// Crea una copia profonda di questa definizione di procedura nativa.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new NativeProcedureDefinition(
            SymbolId,
            Flags,
            (Token)Name.DeepClone(),
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            (DataType)ReturnType.DeepClone(),
            BuiltinKind,
            IsFunction
        )
        {
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}




/// <summary>
/// Rappresenta la definizione di una FUNCTION...END FUNCTION.
/// </summary>
public sealed record FunctionDefinition(
    int SymbolId,
    Token Name,
    ImmutableArray<Parameter> Parameters,
    DataType ReturnType,
    ImmutableArray<Stmt> Body,
    SymbolFlags Flags = SymbolFlags.IsPublic, // MODIFICATO: Usa Flags
    ImmutableArray<TypeParameter> TypeParameters = default,
    ImmutableArray<Token> Attributes = default)
    : ProcedureDefinition(SymbolId, Flags, Name, Parameters, Body, TypeParameters: TypeParameters, Attributes: Attributes)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale completa della firma della funzione.
    /// </summary>
    public override string ToString()
    {
        // Rimosso l'uso dell'helper IsPublic e la sua logica di visualizzazione
        var parameterString = string.Join(", ", Parameters.Select(p => p.OriginalName.Lexeme));
        return $"FUNCTION {OriginalName.Lexeme}({parameterString}) AS {ReturnType} [{Flags}]";
    }

    /// <summary>
    /// Determina se l'istanza corrente è uguale a un'altra istanza di <see cref="FunctionDefinition"/>.
    /// </summary>
    public bool Equals(FunctionDefinition? other)
    {
        if (ReferenceEquals(null, other)) return false;
        if (ReferenceEquals(this, other)) return true;
        return base.Equals(other) && ReturnType.Equals(other.ReturnType);
    }

    /// <summary>
    /// Calcola un codice hash per l'istanza corrente.
    /// </summary>
    public override int GetHashCode()
    {
        var hashCode = new HashCode();
        hashCode.Add(base.GetHashCode());
        hashCode.Add(ReturnType);
        return hashCode.ToHashCode();
    }

    /// <summary>
    /// Crea una copia profonda di questa definizione di funzione.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new FunctionDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            (DataType)ReturnType.DeepClone(),
            [..Body.Select(s => (Stmt)s.DeepClone())],
            Flags,
            [..TypeParameters.Select(tp => (TypeParameter)tp.DeepClone())],
            Attributes
        )
        {
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta la definizione di un generatore (GENERATOR...END GENERATOR).
/// </summary>
public sealed record GeneratorDefinition(
    int SymbolId,
    SymbolFlags Flags, // MODIFICATO: Sostituisce IsPublic
    Token Name,
    ImmutableArray<Parameter> Parameters,
    DataType YieldType,
    ImmutableArray<Stmt> Body
) : ProcedureDefinition(SymbolId, Flags, Name, Parameters, Body)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale completa della firma del generatore.
    /// </summary>
    public override string ToString()
    {
        // Rimosso l'uso dell'helper IsPublic e la sua logica di visualizzazione
        var parameterString = string.Join(", ", Parameters.Select(p => p.OriginalName.Lexeme));
        return $"GENERATOR {OriginalName.Lexeme}({parameterString}) AS {YieldType} [{Flags}]";
    }

    /// <summary>
    /// Determina se l'istanza corrente è uguale a un'altra istanza di <see cref="GeneratorDefinition"/>.
    /// </summary>
    public bool Equals(GeneratorDefinition? other)
    {
        if (ReferenceEquals(null, other)) return false;
        if (ReferenceEquals(this, other)) return true;

        return base.Equals(other) && YieldType.Equals(other.YieldType);
    }

    /// <summary>
    /// Calcola un codice hash per l'istanza corrente.
    /// </summary>
    public override int GetHashCode()
    {
        var hashCode = new HashCode();
        hashCode.Add(base.GetHashCode());
        hashCode.Add(YieldType);
        return hashCode.ToHashCode();
    }

    /// <summary>
    /// Crea una copia profonda di questa definizione di generatore.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new GeneratorDefinition(
            SymbolId,
            Flags,
            (Token)Name.DeepClone(),
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            (DataType)YieldType.DeepClone(),
            [..Body.Select(s => (Stmt)s.DeepClone())]
        )
        {
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta le istruzioni END SUB o END FUNCTION che terminano una procedura.
/// </summary>
public sealed record EndProcedureStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => Token.Lexeme;

    public override AstNode DeepClone()
    {
        return new EndProcedureStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta la dichiarazione di una procedura locale all'interno di un'altra procedura.
/// </summary>
public sealed record LocalProcedureStmt(ProcedureDefinition Procedure)
    : Stmt(Procedure.OriginalName.Position)
{
    public override string ToString() => $"LOCAL {Procedure}";

    public override AstNode DeepClone()
    {
        return new LocalProcedureStmt((ProcedureDefinition)Procedure.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione YIELD all'interno di un generatore.
/// </summary>
public sealed record YieldStmt(Expr ValueToYield, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"YIELD {ValueToYield}";

    public override AstNode DeepClone()
    {
        return new YieldStmt((Expr)ValueToYield.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Definisce le convenzioni di chiamata supportate per le funzioni FFI.
/// </summary>
public enum FfiCallingConvention
{
    CDecl,
    StdCall,
    FastCall,
    ThisCall
}

/// <summary>
/// Rappresenta la definizione di una procedura esterna (FFI) dichiarata tramite `DECLARE`.
/// </summary>
public sealed record ExternalProcedureDefinition(
    int SymbolId,
    bool IsFunction,
    Token Name,
    Token LibraryName,
    Token? AliasName,
    ImmutableArray<Parameter> Parameters,
    DataType? ReturnType,
    FfiCallingConvention CallingConvention
) : ProcedureDefinition(
    SymbolId,
    SymbolFlags.IsPublic | (IsFunction ? SymbolFlags.IsFunction : SymbolFlags.None),
    Name: Name,
    Parameters: Parameters,
    Body: ImmutableArray<Stmt>.Empty
)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale che identifica questa come una funzione FFI.
    /// </summary>
    public override string ToString()
    {
        var procType = IsFunction ? "FUNCTION" : "SUB";
        var paramList = string.Join(", ", Parameters.Select(p =>
            $"{(p.Flags.HasFlag(SymbolFlags.IsByRef) ? "BYREF " : "BYVAL ")}{p.OriginalName.Lexeme} AS {p.DataType}"));
        var returnTypeStr = IsFunction && ReturnType != null ? $" AS {ReturnType}" : "";
        var aliasStr = AliasName != null ? $" ALIAS \"{AliasName.Lexeme}\"" : "";
        var callConvStr = CallingConvention == FfiCallingConvention.CDecl ? "CDECL" : "STDCALL";

        return
            $"[FFI] DECLARE {procType} {OriginalName.Lexeme} LIB \"{LibraryName.Lexeme}\"{aliasStr} ({paramList}){returnTypeStr} {callConvStr}";
    }

    /// <summary>
    /// Crea una copia profonda di questa definizione di procedura esterna.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new ExternalProcedureDefinition(
            SymbolId,
            IsFunction,
            (Token)Name.DeepClone(),
            (Token)LibraryName.DeepClone(),
            (Token?)AliasName?.DeepClone(),
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            ReturnType?.DeepClone() as DataType,
            CallingConvention
        )
        {
            Flags = Flags,
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta la definizione di un tipo delegato (un alias per una firma di funzione).
/// Es: DELEGATE FUNCTION MioCompare(a AS INTEGER, b AS INTEGER) AS INTEGER
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questo delegato.</param>
/// <param name="Name">Il token con il nome del delegato.</param>
/// <param name="Parameters">La lista dei parametri che definiscono la firma.</param>
/// <param name="ReturnType">Il tipo di dato di ritorno della firma.</param>
/// <param name="Flags">I flag semantici (es. visibilità).</param>
public sealed record DelegateDefinition(
    int SymbolId,
    Token Name,
    ImmutableArray<Parameter> Parameters,
    DataType ReturnType,
    SymbolFlags Flags
) : Decl(Name.Position)
{
    public Token OriginalName { get; init; } = Name;
    
    /// <summary>
    /// Rappresenta la firma univoca del delegato, usata per i confronti di tipo.
    /// </summary>
    [JsonIgnore]
    [field: System.Diagnostics.CodeAnalysis.AllowNull, System.Diagnostics.CodeAnalysis.MaybeNull]
    public ProcedureSignature Signature => field ??= new ProcedureSignature(OriginalName.Lexeme, Parameters);

    public override string ToString()
    {
        var paramsStr = string.Join(", ", Parameters.Select(p => p.ToString()));
        return $"DELEGATE {OriginalName.Lexeme}({paramsStr}) AS {ReturnType} [{Flags}]";
    }

    public override AstNode DeepClone()
    {
        return new DelegateDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            (DataType)ReturnType.DeepClone(),
            Flags
        ) { OriginalName = (Token)OriginalName.DeepClone() };
    }
}
