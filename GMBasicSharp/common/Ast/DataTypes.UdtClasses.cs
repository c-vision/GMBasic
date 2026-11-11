// FILE: Ast.DataTypes.UdtClasses.cs

using System.Diagnostics.CodeAnalysis;

namespace Basic;

/// <summary>
/// Rappresenta un singolo campo all'interno di una definizione TYPE...END TYPE.
/// Ora include le dimensioni per gli array a dimensione fissa.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questo campo.</param>
/// <param name="Name">Il token con il nome del campo.</param>
/// <param name="DataType">Il tipo di dato del campo.</param>
/// <param name="Dimensions">La lista di espressioni per le dimensioni, se è un array a dimensione fissa.</param>
/// <param name="DefaultValue">L'espressione opzionale che definisce il valore iniziale del campo.</param>
public record TypeField(
    int SymbolId,
    Token Name,
    DataType DataType,
    ImmutableArray<Expr> Dimensions,
    Expr? DefaultValue,
    /// <summary>BITFIELD (2026-11-11): larghezza in bit (1..64) della clausola `WIDTH n`
    /// dopo il tipo; 0 = campo normale. Il valore viene mascherato/esteso al segno in base al
    /// tipo dichiarato e impacchettato LSB-first nell'unità di storage (regole C).</summary>
    int BitWidth = 0) : Decl(Name.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var dimsString = Dimensions.IsEmpty ? "" : $"({string.Join(", ", Dimensions)})";
        var bitsString = BitWidth > 0 ? $" WIDTH {BitWidth}" : "";
        return $"{Name.Lexeme}{dimsString}#{SymbolId} AS {DataType}{bitsString}{(DefaultValue is not null ? $" = {DefaultValue}" : "")}";
    }

    /// <summary>
    /// Crea una copia profonda di questo oggetto TypeField.
    /// </summary>
    /// <returns>Una nuova istanza di TypeField con copie dei suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TypeField(
            SymbolId,
            (Token)Name.DeepClone(),
            (DataType)DataType.DeepClone(),
            [..Dimensions.Select(d => (Expr)d.DeepClone())],
            (Expr?)DefaultValue?.DeepClone(),
            BitWidth);
    }
}

/// <summary>
/// Rappresenta il blocco GET di una proprietà.
/// </summary>
/// <param name="Body">La lista di istruzioni nel corpo del GET.</param>
public record PropertyGetter(ImmutableArray<Stmt> Body)
{
    /// <summary>
    /// Crea una copia profonda di questo oggetto PropertyGetter.
    /// </summary>
    /// <returns>Una nuova istanza di PropertyGetter con copie del suo corpo.</returns>
    public PropertyGetter DeepClone()
    {
        return new PropertyGetter([..Body.Select(s => (Stmt)s.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta il blocco SET di una proprietà.
/// </summary>
/// <param name="ValueParameter">Il parametro speciale 'value' disponibile nel SET.</param>
/// <param name="Body">La lista di istruzioni nel corpo del SET.</param>
public record PropertySetter(Parameter ValueParameter, ImmutableArray<Stmt> Body)
{
    /// <summary>
    /// Crea una copia profonda di questo oggetto PropertySetter.
    /// </summary>
    /// <returns>Una nuova istanza di PropertySetter con copie dei suoi figli.</returns>
    public PropertySetter DeepClone()
    {
        return new PropertySetter((Parameter)ValueParameter.DeepClone(), [..Body.Select(s => (Stmt)s.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta la definizione completa di una proprietà (PROPERTY...END PROPERTY)
/// all'interno di un TYPE.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questa proprietà.</param>
/// <param name="Name">Il token che contiene il nome originale della proprietà.</param>
/// <param name="Type">Il tipo di dato della proprietà.</param>
/// <param name="Flags">I flag semantici della proprietà (visibilità, virtual, override, etc.).</param>
/// <param name="Getter">Il blocco GET opzionale.</param>
/// <param name="Setter">Il blocco SET opzionale.</param>
public record PropertyDefinition(
    int SymbolId,
    Token Name,
    DataType Type,
    SymbolFlags Flags,
    PropertyGetter? Getter,
    PropertySetter? Setter
) : Decl(Name.Position)
{
    /// <summary>
    /// Contiene il token originale del nome della proprietà, come apparso nel codice sorgente.
    /// </summary>
    public Token OriginalName => Name;

    /// <summary>
    /// Crea una copia profonda di questa definizione di proprietà.
    /// </summary>
    /// <returns>Una nuova istanza di PropertyDefinition con copie dei suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PropertyDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            (DataType)Type.DeepClone(),
            Flags,
            Getter?.DeepClone(),
            Setter?.DeepClone());
    }

    /// <summary>
    /// Fornisce una rappresentazione testuale della definizione della proprietà.
    /// </summary>
    public override string ToString()
    {
        // Rimosso l'uso dell'helper IsPublic, ora bisogna fare la risoluzione a monte
        return $"PROPERTY {OriginalName.Lexeme}#{SymbolId} AS {Type} [{Flags}]";
    }
    
}

/// <summary>
/// Rappresenta la definizione di un costruttore (SUB NEW...END SUB)
/// all'interno di un TYPE.
/// </summary>
/// <param name="Parameters">La lista dei parametri del costruttore.</param>
/// <param name="Body">La lista di istruzioni che implementano la logica del costruttore.</param>
public record ConstructorDefinition(
    ImmutableArray<Parameter> Parameters,
    ImmutableArray<Stmt> Body
)
{
    public object? Parent { get; internal set; }

    /// <summary>
    /// Contiene il nome decorato (mangled) del costruttore, che lo lega al suo tipo contenitore.
    /// Es. 'MyType::NEW'.
    /// </summary>
    public string MangledName { get; init; } = string.Empty;

    /// <summary>
    /// Crea una copia profonda di questa definizione di costruttore.
    /// </summary>
    /// <returns>Una nuova istanza di ConstructorDefinition con copie dei suoi figli.</returns>
    public ConstructorDefinition DeepClone()
    {
        // Cast esplicito aggiunto in seguito al refactoring Parameter
        return new ConstructorDefinition(
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            [..Body.Select(s => (Stmt)s.DeepClone())]
        )
        {
            MangledName = MangledName
        };
    }
}

/// <summary>
/// Rappresenta la definizione di un distruttore (SUB DESTRUCTOR...END SUB)
/// all'interno di un TYPE o di una CLASS.
/// </summary>
/// <param name="Body">La lista di istruzioni che implementano la logica del distruttore.</param>
public record DestructorDefinition(ImmutableArray<Stmt> Body)
{
    /// <summary>
    /// Contiene il nome decorato (mangled) del distruttore, che lo lega al suo tipo contenitore.
    /// Es. 'MyType::DELETE'.
    /// </summary>
    public string MangledName { get; init; } = string.Empty;

    /// <summary>
    /// Crea una copia profonda di questa definizione di distruttore.
    /// </summary>
    /// <returns>Una nuova istanza di DestructorDefinition con copie del suo corpo.</returns>
    public DestructorDefinition DeepClone()
    {
        return new DestructorDefinition([..Body.Select(s => (Stmt)s.DeepClone())])
        {
            MangledName = MangledName
        };
    }
}

/// <summary>
/// Rappresenta un tipo di dato definito dall'utente (UDT) tramite TYPE...END TYPE.
/// Ora eredita da DataType e usa SymbolFlags per la visibilità.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questo tipo.</param>
/// <param name="SymbolId">L'identificativo univoco del simbolo nella SymbolTable.</param>
/// <param name="Name">Il token con il nome decorato (mangled) del tipo.</param>
/// <param name="Fields">La lista immutabile dei campi definiti nel tipo.</param>
/// <param name="Flags">I flag semantici del tipo (visibilità, struttura, ecc.).</param>
[SuppressMessage("ReSharper", "TypeWithSuspiciousEqualityIsUsedInRecord.Global")]
public record UserDefinedType(
    int SymbolId,
    Token Name,
    ImmutableArray<TypeField> Fields,
    SymbolFlags Flags,
    ImmutableArray<TypeParameter> TypeParameters = default) : DataType(Name.Position)
{
    /// <summary>
    /// Indica se il tipo è dichiarato con layout packed (PACKED TYPE),
    /// senza padding/alignment tra i campi. Usato per FFI interop con struct C packed.
    /// </summary>
    public bool IsPacked { get; init; }

    /// <summary>
    /// UNION (2026-11-11): tutti i campi partono dall'offset 0 e condividono la stessa
    /// storage (semantica C). Taglia = massimo dei campi, allineamento = massimo.
    /// Scrivere un campo e leggere un altro restituisce la reinterpretazione dei byte.
    /// Ammessi solo campi NUMERICI scalari (v1).
    /// </summary>
    public bool IsUnion { get; init; }

    /// <summary>True se il tipo è a LUNGHEZZA FISSA (serializzabile a record: tutti i campi
    /// sono fixed — scalari, STRING*N, array fisso di fixed, UDT fixed). False se contiene
    /// campi a lunghezza variabile (STRING, LIST/DICT/collezioni, ANY, UDT non-fixed).
    /// I comandi a record (OPEN RANDOM, DBF, FIELD, mmap/FFI) richiedono IsFixedSize=true;
    /// in-memory un UDT variabile è lecito (accesso campi, SIGNAL, JSON).</summary>
    [JsonIgnore]
    public bool IsFixedSize => Fields.All(f => DataTypeIsFixedSize(f.DataType));

    /// <summary>True se il DataType è a lunghezza fissa (vedi <see cref="IsFixedSize"/>).</summary>
    public static bool DataTypeIsFixedSize(DataType dt) => dt switch
    {
        ListType or DictType or HashSetType or QueueType or StackType or SortedDictType or JsonType => false,
        StringType { FixedLength: null } => false,
        AnyType => false,
        ArrayType at => DataTypeIsFixedSize(at.ElementType),
        UserDefinedType udt => udt.IsFixedSize,
        _ => true
    };

    /// <summary>
    /// Ottiene o imposta il nome del namespace in cui questa dichiarazione è contenuta.
    /// Viene popolato dal Parser (Fase 1) in base alla direttiva `NAMESPACE` attiva.
    /// Se null, la dichiarazione appartiene al namespace globale o eredita il contesto.
    /// </summary>
    public string? DeclaredNamespace { get; init; }

    /// <summary>
    /// Nomi delle interfacce implementati dichiarati nella clausola IMPLEMENTS.
    /// </summary>
    public ImmutableArray<Token> ImplementedInterfaceNames { get; init; } = ImmutableArray<Token>.Empty;

    /// <summary>
    /// Riferimenti risolti alle interfacce implementate (popolato dal Linker).
    /// </summary>
    [JsonIgnore]
    public ImmutableArray<InterfaceDefinition> ResolvedInterfaces { get; set; } = ImmutableArray<InterfaceDefinition>.Empty;

    /// <summary>
    /// Contiene il token originale del nome del tipo, come apparso nel codice sorgente.
    /// </summary>
    public Token OriginalName { get; init; } = Name;

    /// <summary>
    /// Ottiene la lista immutabile delle proprietà definite in questo tipo.
    /// </summary>
    public ImmutableArray<PropertyDefinition> Properties { get; init; } = ImmutableArray<PropertyDefinition>.Empty;

    /// <summary>
    /// Ottiene la lista immutabile degli operatori sovraccaricati definiti in questo tipo.
    /// </summary>
    public ImmutableArray<OperatorDefinition> Operators { get; init; } = ImmutableArray<OperatorDefinition>.Empty;

    /// <summary>
    /// Ottiene la lista immutabile dei metodi (SUB/FUNCTION) definiti in questo tipo.
    /// </summary>
    public ImmutableArray<ProcedureDefinition> Methods { get; init; } = ImmutableArray<ProcedureDefinition>.Empty;

    /// <summary>
    /// Ottiene la lista immutabile dei costruttori (SUB NEW) definiti in questo tipo.
    /// </summary>
    public ImmutableArray<ConstructorDefinition> Constructors { get; init; } =
        ImmutableArray<ConstructorDefinition>.Empty;

    /// <summary>
    /// Ottiene la lista immutabile dei distruttori (SUB DESTRUCTOR) definiti in questo tipo.
    /// </summary>
    public ImmutableArray<DestructorDefinition> Destructors { get; init; } = ImmutableArray<DestructorDefinition>.Empty;

    /// <summary>
    /// Ottiene un riferimento al nodo genitore nell'AST.
    /// </summary>
    [JsonIgnore]
    public AstNode? Parent { get; internal set; }

    /// <summary>
    /// Fornisce una rappresentazione testuale della dichiarazione del tipo per il dump dell'AST.
    /// </summary>
    public override string ToString()
    {
        var implementation = ImplementedInterfaceNames.Any() ? $" IMPLEMENTS {string.Join(", ", ImplementedInterfaceNames.Select(t => t.Lexeme))}" : "";
        return $"TYPE {OriginalName.Lexeme}{implementation} [{Flags}]";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo UserDefinedType.
    /// </summary>
    /// <returns>Una nuova istanza di UserDefinedType con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UserDefinedType(SymbolId, (Token)Name.DeepClone(),
            [..Fields.Select(f => (TypeField)f.DeepClone())],
            Flags,
            [..TypeParameters.Select(tp => (TypeParameter)tp.DeepClone())])
        {
            IsPacked = IsPacked,
            IsUnion = IsUnion,
            DeclaredNamespace = DeclaredNamespace, // Propaga il namespace
            OriginalName = (Token)OriginalName.DeepClone(),
            ImplementedInterfaceNames = [..ImplementedInterfaceNames.Select(t => (Token)t.DeepClone())],
            ResolvedInterfaces = ResolvedInterfaces,
            Properties = [..Properties.Select(p => (PropertyDefinition)p.DeepClone())],
            Operators = [..Operators.Select(o => (OperatorDefinition)o.DeepClone())],
            Methods = [..Methods.Select(m => (ProcedureDefinition)m.DeepClone())],
            Constructors = [..Constructors.Select(c => c.DeepClone())],
            Destructors = [..Destructors.Select(d => d.DeepClone())],
            Parent = Parent
        };
    }
}

/// <summary>
/// Rappresenta la definizione di un operatore sovraccaricato (OPERATOR...END OPERATOR)
/// all'interno di un TYPE.
/// </summary>
public record OperatorDefinition(
    Token OperatorToken,
    SymbolFlags Flags,
    ImmutableArray<Parameter> Parameters,
    DataType ReturnType,
    ImmutableArray<Stmt> Body
) : Decl(OperatorToken.Position)
{
    /// <summary>
    /// Contiene il nome decorato (mangled) dell'operatore, che lo lega al suo tipo contenitore.
    /// </summary>
    public string MangledName { get; init; } = string.Empty;

    /// <summary>
    /// Crea una copia profonda di questa definizione di operatore.
    /// </summary>
    /// <returns>Una nuova istanza di OperatorDefinition con copie dei suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new OperatorDefinition(
            (Token)OperatorToken.DeepClone(),
            Flags,
            [..Parameters.Select(p => (Parameter)p.DeepClone())],
            (DataType)ReturnType.DeepClone(),
            [..Body.Select(s => (Stmt)s.DeepClone())]
        )
        {
            MangledName = MangledName
        };
    }
}

/// <summary>
/// Definisce i possibili modificatori di accesso per i membri di una classe.
/// </summary>
public enum AccessModifier
{
    Private,
    Protected,
    Friend,
    Public
}

/// <summary>
/// Classe base astratta per tutti i membri definiti all'interno di un blocco CLASS.
/// </summary>
public abstract record ClassMember(Token Name, AccessModifier Access, ImmutableArray<Token> Attributes = default)
{
    [JsonIgnore] public ClassDefinition? Parent { get; internal set; }
    public abstract ClassMember DeepClone();
}

/// <summary>
/// Rappresenta un campo (variabile) definito all'interno di una classe.
/// </summary>
public record FieldMember(VariableDeclaration Declaration, AccessModifier Access, ImmutableArray<Token> Attributes = default)
    : ClassMember(Declaration.NameCleaned, Access, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new FieldMember((VariableDeclaration)Declaration.DeepClone(), Access, Attributes);
    }
}

/// <summary>
/// Rappresenta un metodo (SUB o FUNCTION) definito all'interno di una classe.
/// Rimosse le proprietà booleane IsVirtual e IsOverride, che sono ora gestite dalla ProcedureDefinition incapsulata.
/// </summary>
public record MethodMember(ProcedureDefinition Procedure, AccessModifier Access, ImmutableArray<Token> Attributes = default)
    : ClassMember(Procedure.OriginalName, Access, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new MethodMember((ProcedureDefinition)Procedure.DeepClone(), Access, Attributes);
    }
}

/// <summary>
/// Rappresenta una proprietà definita all'interno di una classe.
/// </summary>
public record PropertyMember(PropertyDefinition Property, AccessModifier Access, ImmutableArray<Token> Attributes = default)
    : ClassMember(Property.OriginalName, Access, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new PropertyMember((PropertyDefinition)Property.DeepClone(), Access, Attributes);
    }
}

/// <summary>
/// Rappresenta un costruttore (SUB NEW) definito all'interno di una classe.
/// </summary>
public record ConstructorMember(ConstructorDefinition Constructor, ImmutableArray<Token> Attributes = default)
    : ClassMember(new Token(TokenKind.New, "NEW", null, SourcePosition.Unknown), AccessModifier.Public, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new ConstructorMember(Constructor.DeepClone(), Attributes);
    }
}

/// <summary>
/// Rappresenta un distruttore (SUB DESTRUCTOR) definito all'interno di una classe.
/// </summary>
public record DestructorMember(DestructorDefinition Destructor, ImmutableArray<Token> Attributes = default)
    : ClassMember(new Token(TokenKind.Delete, "DELETE", null, SourcePosition.Unknown), AccessModifier.Public, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new DestructorMember(Destructor.DeepClone(), Attributes);
    }
}

/// <summary>
/// Rappresenta un operatore sovraccaricato definito all'interno di una classe.
/// </summary>
public record OperatorMember(OperatorDefinition Operator, AccessModifier Access, ImmutableArray<Token> Attributes = default)
    : ClassMember(Operator.OperatorToken, Access, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new OperatorMember((OperatorDefinition)Operator.DeepClone(), Access, Attributes);
    }
}

/// <summary>
/// Rappresenta la definizione completa di un'interfaccia (INTERFACE...END INTERFACE).
/// Contiene solo le firme dei metodi e delle proprietà, senza implementazione.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questa interfaccia.</param>
/// <param name="Name">Il token con il nome dell'interfaccia.</param>
/// <param name="Flags">I flag semantici (es. visibilità).</param>
/// <param name="Members">La lista dei membri (solo firme).</param>
public record InterfaceDefinition(
    int SymbolId,
    Token Name,
    SymbolFlags Flags,
    ImmutableArray<ClassMember> Members,
    ImmutableArray<TypeParameter> TypeParameters = default) : DataType(Name.Position)
{
    public string? DeclaredNamespace { get; init; }
    public Token OriginalName { get; init; } = Name;

    public override string ToString() => $"INTERFACE {OriginalName.Lexeme} [{Flags}]";

    public override AstNode DeepClone()
    {
        return new InterfaceDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            Flags,
            [..Members.Select(m => m.DeepClone())],
            [..TypeParameters.Select(tp => (TypeParameter)tp.DeepClone())]
        )
        {
            DeclaredNamespace = DeclaredNamespace,
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta la definizione completa di una classe (CLASS...END CLASS).
/// Ora eredita da DataType, usa SymbolFlags per la visibilità ed è allineata all'architettura ID-based.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questa classe.</param>
/// <param name="Name">Il token con il nome decorato (mangled) della classe.</param>
/// <param name="Flags">I flag semantici della classe (visibilità, astratto, sealed, ecc.).</param>
/// <param name="BaseClassSymbolId">L'ID del simbolo della classe base da cui ereditare. Null se non c'è ereditarietà.</param>
/// <param name="OriginalBaseClassName">Il token opzionale con il nome originale della classe base, per diagnostica e risoluzione posticipata.</param>
/// <param name="Members">La lista immutabile di tutti i membri definiti nella classe.</param>
public record ClassDefinition(
    int SymbolId,
    Token Name,
    SymbolFlags Flags,
    int? BaseClassSymbolId,
    Token? OriginalBaseClassName,
    ImmutableArray<Token> ImplementedInterfaceNames,
    ImmutableArray<ClassMember> Members,
    ImmutableArray<TypeParameter> TypeParameters = default) : DataType(Name.Position)
{
    /// <summary>
    /// Ottiene o imposta il nome del namespace in cui questa dichiarazione è contenuta.
    /// Viene popolato dal Parser (Fase 1) in base alla direttiva `NAMESPACE` attiva.
    /// Se null, la dichiarazione appartiene al namespace globale o eredita il contesto.
    /// </summary>
    public string? DeclaredNamespace { get; init; }

    /// <summary>
    /// Contiene il token originale del nome della classe, come apparso nel codice sorgente.
    /// </summary>
    public Token OriginalName { get; init; } = Name;

    /// <summary>
    /// Ottiene o imposta un riferimento diretto alla definizione della classe base,
    /// che viene popolato durante la fase di linking.
    /// Viene ignorato dalla serializzazione per prevenire riferimenti circolari.
    /// </summary>
    [JsonIgnore]
    public ClassDefinition? ResolvedBaseClass { get; set; }

    // Riferimenti risolti alle interfacce implementate
    [JsonIgnore]
    public ImmutableArray<InterfaceDefinition> ResolvedInterfaces { get; set; } = ImmutableArray<InterfaceDefinition>.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo ClassDefinition.
    /// </summary>
    /// <returns>Una nuova istanza di ClassDefinition con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ClassDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            Flags,
            BaseClassSymbolId,
            (Token?)OriginalBaseClassName?.DeepClone(),
            [.. ImplementedInterfaceNames.Select(t => (Token)t.DeepClone())],
            [..Members.Select(m => m.DeepClone())],
            [..TypeParameters.Select(tp => (TypeParameter)tp.DeepClone())]
        )
        {
            DeclaredNamespace = DeclaredNamespace,
            OriginalName = (Token)OriginalName.DeepClone(),
            ResolvedBaseClass = ResolvedBaseClass,
            ResolvedInterfaces = ResolvedInterfaces
        };
    }

    /// <summary>
    /// Fornisce una rappresentazione testuale della dichiarazione della classe per il debug.
    /// </summary>
    public override string ToString()
    {
        var inheritance = OriginalBaseClassName != null ? $" INHERITS {OriginalBaseClassName.Lexeme}" : "";
        var implementation = ImplementedInterfaceNames.Any() ? $" IMPLEMENTS {string.Join(", ", ImplementedInterfaceNames.Select(t => t.Lexeme))}" : "";
        return $"CLASS {OriginalName.Lexeme}{inheritance}{implementation} [{Flags}]";
    }
    
}

/// <summary>
/// Rappresenta una chiamata esplicita al costruttore di una classe base (BASE.NEW(...)).
/// </summary>
[JsonDerivedType(typeof(BaseConstructorCallStmt), nameof(BaseConstructorCallStmt))]
public record BaseConstructorCallStmt(ImmutableArray<Expr> Args, Token BaseToken)
    : Stmt(BaseToken.Position)
{
    public override string ToString() => $"BASE.NEW({string.Join(", ", Args)})";

    public override AstNode DeepClone()
    {
        return new BaseConstructorCallStmt([..Args.Select(a => (Expr)a.DeepClone())], (Token)BaseToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta un tipo di dato il cui nome è qualificato ma non ancora risolto.
/// </summary>
[JsonDerivedType(typeof(UnresolvedQualifiedType), nameof(UnresolvedQualifiedType))]
public record UnresolvedQualifiedType(
    QualifiedNameExpr QualifiedName,
    ImmutableArray<DataType> TypeArguments = default) : DataType(QualifiedName.Position)
{
    public override string ToString() => TypeArguments.IsDefaultOrEmpty
        ? QualifiedName.FullName
        : $"{QualifiedName.FullName}<{string.Join(", ", TypeArguments)}>";

    public override AstNode DeepClone()
    {
        return new UnresolvedQualifiedType(
            (QualifiedNameExpr)QualifiedName.DeepClone(),
            TypeArguments.IsDefaultOrEmpty
                ? default
                : [..TypeArguments.Select(t => (DataType)t.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta la dichiarazione di un tipo (TYPE...END TYPE) all'interno di una procedura.
/// </summary>
[JsonDerivedType(typeof(LocalTypeStmt), nameof(LocalTypeStmt))]
public record LocalTypeStmt(UserDefinedType Definition)
    : Stmt(Definition.OriginalName.Position)
{
    public override string ToString() => $"LOCAL {Definition}";

    public override AstNode DeepClone()
    {
        return new LocalTypeStmt((UserDefinedType)Definition.DeepClone());
    }
}

/// <summary>
/// Rappresenta la definizione di un evento (EVENT Name(args...)) all'interno di una classe.
/// </summary>
[JsonDerivedType(typeof(EventDefinition), nameof(EventDefinition))]
public record EventDefinition(
    int SymbolId,
    Token Name,
    ImmutableArray<Parameter> Parameters,
    SymbolFlags Flags
) : Decl(Name.Position)
{
    public Token OriginalName { get; init; } = Name;

    public override string ToString()
    {
        var paramsStr = string.Join(", ", Parameters.Select(p => p.ToString()));
        return $"EVENT {OriginalName.Lexeme}({paramsStr}) [{Flags}]";
    }

    public override AstNode DeepClone()
    {
        return new EventDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            [.. Parameters.Select(p => (Parameter)p.DeepClone())],
            Flags
        ) { OriginalName = (Token)OriginalName.DeepClone() };
    }
}

/// <summary>
/// Rappresenta un evento definito all'interno di una classe.
/// </summary>
[JsonDerivedType(typeof(EventMember), nameof(EventMember))]
public record EventMember(EventDefinition Event, AccessModifier Access, ImmutableArray<Token> Attributes = default)
    : ClassMember(Event.OriginalName, Access, Attributes)
{
    public override ClassMember DeepClone()
    {
        return new EventMember((EventDefinition)Event.DeepClone(), Access, Attributes);
    }
}