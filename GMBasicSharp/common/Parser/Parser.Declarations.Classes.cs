// FILE: Interpreter.Parser.Declarations.Classes.cs

using System.Collections.Immutable;

// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault
// ReSharper disable ParameterOnlyUsedForPreconditionCheck.Local

namespace Basic;

/// <summary>
/// Contiene le versioni "pure" (Fase 1) dei metodi del Parser per analizzare
/// la sintassi delle Classi e dei loro membri, senza interagire con la SymbolTable.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Analizza e parsa una definizione di classe completa, dal `CLASS` iniziale all'`END CLASS` finale.
    /// Questo metodo gestisce la firma della classe, inclusi i modificatori di visibilità (`PUBLIC`/`PRIVATE`),
    /// l'astrazione (`ABSTRACT`), l'ereditarietà (`INHERITS`) e l'implementazione di interfacce (`IMPLEMENTS`).
    /// Successivamente, cicla sui membri interni (campi, metodi, proprietà, etc.) delegando la loro analisi
    /// al metodo `ParseClassMember`.
    /// </summary>
    /// <remarks>
    /// <b>Ruolo Architetturale:</b>
    /// Questo è un parser di dichiarazioni di alto livello per la Fase 1. Il suo compito è tradurre la
    /// sintassi completa di un blocco `CLASS` in una struttura dati AST (`ClassDefinition`) senza
    /// eseguire alcuna registrazione nella SymbolTable. L'AST prodotto viene poi utilizzato dalle
    /// fasi successive del compilatore per la costruzione dei simboli, la risoluzione dell'ereditarietà
    /// e la validazione semantica.
    /// 
    /// <b>Sintassi Gestita:</b>
    /// Il parser è progettato per analizzare una firma di classe complessa che può includere
    /// diverse clausole opzionali in un ordine specifico:
    /// `[PUBLIC|PRIVATE] [ABSTRACT] CLASS NomeClasse [INHERITS ClasseBase] [IMPLEMENTS Interfaccia1, Interfaccia2, ...]`
    /// 
    /// Dopo aver parsato la firma, il metodo entra in un loop per analizzare il corpo della classe,
    /// invocando `ParseClassMember` per ogni dichiarazione di membro fino a quando non incontra
    /// la keyword di chiusura `END CLASS`.
    /// </remarks>
    /// <returns>
    /// Un nuovo nodo AST <see cref="ClassDefinition"/> che rappresenta l'intera classe parsata,
    /// pronto per essere aggiunto alla lista delle dichiarazioni del modulo.
    /// </returns>
    /// <example>
    /// Il codice seguente mostra una definizione di classe complessa che questo metodo è in grado di parsare:
    /// <code>
    /// ' Una classe base astratta che implementa un'interfaccia.
    /// PUBLIC ABSTRACT CLASS Veicolo IMPLEMENTS ILocalizzabile
    ///     ' Firma di un metodo astratto che le classi derivate devono implementare.
    ///     PUBLIC ABSTRACT SUB AvviaMotore()
    ///     
    ///     ' Proprietà concreta.
    ///     PROPERTY Posizione AS GeoPoint
    /// END CLASS
    /// 
    /// ' Una classe concreta che eredita da Veicolo.
    /// PUBLIC CLASS Automobile INHERITS Veicolo
    ///     ' Campi privati.
    ///     DIM _marca AS STRING
    /// 
    ///     ' Costruttore.
    ///     PUBLIC SUB NEW(marca AS STRING)
    ///         _marca = marca
    ///     END SUB
    /// 
    ///     ' Implementazione del metodo astratto della classe base.
    ///     PUBLIC OVERRIDE SUB AvviaMotore()
    ///         PRINT "Motore avviato!"
    ///     END SUB
    /// END CLASS
    /// </code>
    /// </example>
    /// <exception cref="BasicException">
    /// Lanciata se la sintassi della dichiarazione di classe non è valida, ad esempio se manca il nome della classe,
    /// se una clausola come `INHERITS` non è seguita da un nome valido, o se manca la keyword di chiusura `END CLASS`.
    /// </exception>
    private ClassDefinition ParseClassDefinition()
    {
        var isPublic = Match(TokenKind.Public);
        var isAbstract = Match(TokenKind.Abstract);
        Consume(TokenKind.Class, "Expected 'CLASS' keyword.");

        var nameToken = Peek();
        if (!IsPermissiveNameToken(nameToken))
        {
            throw CreateError(nameToken, "Expected a class name.");
        }
        Advance();

        // --- TYPE PARAMETERS: <T, U WHERE T IS IComparable> ---
        var typeParameters = Check(TokenKind.LessThan)
            ? ParseTypeParameterList()
            : ImmutableArray<TypeParameter>.Empty;

        Token? baseClassName = null;
        if (Match(TokenKind.Inherits))
        {
            // Z3: il nome della classe base può essere QUALIFICATO (INHERITS MVC.HttpController)
            // o provenire da un altro file importato — NON solo un singolo nome nudo dello
            // stesso file di parsing. Consuma una sequenza IDENT ('.' IDENT)* e la fonde in un
            // unico token con lexeme puntato ("MVC.HttpController"), che il linker risolve
            // cercando il tipo nel namespace (GetSymbolIdsByNamespace).
            baseClassName = ParseQualifiedNameToken("a base class name after 'INHERITS'");
        }

        // --- NUOVO: Parsing clausola IMPLEMENTS ---
        var implementedInterfaces = ImmutableArray.CreateBuilder<Token>();
        if (Match(TokenKind.Implements))
        {
            do
            {
                // Z3: anche i nomi di interfaccia possono essere qualificati (IMPLEMENTS NS.IFoo).
                implementedInterfaces.Add(ParseQualifiedNameToken("an interface name after 'IMPLEMENTS'"));
            } while (Match(TokenKind.Comma));
        }
        // ------------------------------------------

        Consume(TokenKind.Newline, "Expected a newline after CLASS declaration.");

        var members = ImmutableArray.CreateBuilder<ClassMember>();
        while (!Check(TokenKind.EndClass) && !IsAtEnd())
        {
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber)) { /* Skip */ }

            if (Check(TokenKind.EndClass) || IsAtEnd()) break;

            members.AddRange(ParseClassMember());
        }

        Consume(TokenKind.EndClass, "Expected 'END CLASS' to close the class block.");

        var flags = isPublic
            ? SymbolFlags.IsPublic | SymbolFlags.IsClass
            : SymbolFlags.IsPrivate | SymbolFlags.IsClass;

        if (isAbstract) flags |= SymbolFlags.IsAbstract;

        var classDef = new ClassDefinition(
            -1,
            nameToken,
            flags,
            null,
            baseClassName,
            implementedInterfaces.ToImmutable(),
            members.ToImmutable(),
            typeParameters
        ) { OriginalName = nameToken, DeclaredNamespace = _currentNamespace };

        foreach (var member in classDef.Members)
        {
            member.Parent = classDef;
            if (member is MethodMember methodMember)
            {
                methodMember.Procedure.Parent = classDef;
            }
            if (member is ConstructorMember constructorMember)
            {
                constructorMember.Constructor.Parent = classDef;
            }
        }

        return classDef;
    }

    /// <summary>
    /// [Z3] Consuma un nome QUALIFICATO `IDENT ('.' IDENT)*` (es. <c>MVC.HttpController</c>) e
    /// lo restituisce come un singolo <see cref="Token"/> il cui lexeme è la stringa puntata
    /// completa (es. "MVC.HttpController"). I token '.' non sono separatori di statement: dopo
    /// il nome qualificato il parsing prosegue normalmente (newline per la clausola CLASS,
    /// virgola/linea per IMPLEMENTS). Usato per INHERITS/IMPLEMENTS con classe base o
    /// interfaccia da un altro file o qualificata.
    /// </summary>
    private Token ParseQualifiedNameToken(string expectedDescription)
    {
        var first = Peek();
        if (!IsPermissiveNameToken(first))
        {
            throw CreateError(first, $"Expected {expectedDescription}.");
        }
        Advance();

        var lexeme = first.Lexeme;
        var parts = new List<Token> { first };
        while (Check(TokenKind.Dot))
        {
            Advance(); // consuma '.'
            var part = Peek();
            if (!IsPermissiveNameToken(part))
            {
                throw CreateError(part, $"Expected an identifier after '.' in {expectedDescription}.");
            }
            Advance();
            lexeme += "." + part.Lexeme;
            parts.Add(part);
        }

        // Fonde il nome qualificato in un unico token: il linker lo usa per la risoluzione
        // per namespace (GetSymbolIdsByNamespace) e i downstream leggono il lexeme puntato.
        return first with { Lexeme = lexeme };
    }

    /// <summary>
    /// Parsa il modificatore di accesso di un membro di classe (`PUBLIC`, `PRIVATE`, `PROTECTED`, `FRIEND`).
    /// </summary>
    /// <returns>L'enum <see cref="AccessModifier"/> corrispondente. Restituisce `Private` se non viene specificato alcun modificatore.</returns>
    private AccessModifier ParseAccessModifier()
    {
        if (Match(TokenKind.Public)) return AccessModifier.Public;
        if (Match(TokenKind.Private)) return AccessModifier.Private;
        if (Match(TokenKind.Protected)) return AccessModifier.Protected;
        return Match(TokenKind.Friend)
            ? AccessModifier.Friend
            : AccessModifier.Private; // Il default all'interno di una classe è Private
    }

    /// <summary>
    /// [FASE 1] Parsa un singolo membro di classe (campo, metodo, proprietà, etc.) senza registrare simboli.
    /// Gestisce i modificatori di accesso (`PUBLIC`, `PRIVATE`, etc.) e i modificatori di comportamento
    /// (`STATIC`, `VIRTUAL`, `OVERRIDE`, `ABSTRACT`).
    /// </summary>
    /// <remarks>
    /// Il metodo analizza la sequenza di token per determinare il tipo di membro da parsare e
    /// accumula i flag appropriati (es. `IsStatic`, `IsVirtual`, `IsAbstract`) da passare ai parser specifici.
    /// </remarks>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Campo di istanza (default a PRIVATE)
    /// DIM Health AS INTEGER
    /// 
    /// ' Metodo astratto (deve essere in una classe astratta)
    /// PUBLIC ABSTRACT FUNCTION CalcolaArea() AS DOUBLE
    /// 
    /// ' Campo statico pubblico
    /// PUBLIC STATIC DIM MaxPlayers AS INTEGER = 4
    ///
    /// ' Metodo virtuale protetto
    /// PROTECTED VIRTUAL SUB Update()
    /// 
    /// ' Metodo di override pubblico
    /// PUBLIC OVERRIDE FUNCTION GetDescription() AS STRING
    /// 
    /// ' Costruttore
    /// PUBLIC SUB NEW()
    /// </code>
    /// </example>
    private IEnumerable<ClassMember> ParseClassMember()
    {
        var memberAttributes = ParseAttributes();
        var access = ParseAccessModifier();

        var isStatic = Match(TokenKind.Static);
        var isAbstract = Match(TokenKind.Abstract);
        var isVirtual = Match(TokenKind.Virtual);
        var isOverride = Match(TokenKind.Override);

        // --- VALIDAZIONI SINTATTICHE DEI MODIFICATORI ---
        if (isVirtual && isOverride)
            throw CreateError(Peek(), "Un membro non può essere sia VIRTUAL che OVERRIDE.");
        if (isStatic && (isVirtual || isOverride || isAbstract))
            throw CreateError(Peek(), "Un membro STATIC non può essere VIRTUAL, OVERRIDE o ABSTRACT.");
        if (isAbstract && (isVirtual || isOverride))
            throw CreateError(Peek(),
                "Un membro ABSTRACT è implicitamente virtuale e non può essere dichiarato VIRTUAL o OVERRIDE.");
        if (isAbstract && access == AccessModifier.Private)
            throw CreateError(Peek(), "Un membro ABSTRACT non può essere PRIVATE.");

        var nextToken = Peek();
        switch (nextToken.Kind)
        {
            case TokenKind.Dim:
                if (isVirtual || isOverride || isAbstract)
                    throw CreateError(nextToken,
                        "VIRTUAL, OVERRIDE o ABSTRACT possono essere applicati solo a SUB o FUNCTION.");
                return ParseFieldMembers(access, isStatic, memberAttributes);

            case TokenKind.Function:
                if (At(1).Kind == TokenKind.New)
                {
                    if (isStatic || isVirtual || isOverride || isAbstract)
                        throw CreateError(nextToken,
                            "I costruttori non possono essere STATIC, VIRTUAL, OVERRIDE o ABSTRACT.");
                    return access != AccessModifier.Public
                        ? throw CreateError(nextToken, "I costruttori devono essere PUBLIC.")
                        : [ParseConstructorMember(memberAttributes)];
                }

                if (At(1).Kind == TokenKind.Destructor)
                {
                    if (isStatic || isVirtual || isOverride || isAbstract)
                        throw CreateError(nextToken,
                            "I distruttori non possono essere STATIC, VIRTUAL, OVERRIDE o ABSTRACT.");
                    return [ParseDestructorMember(access, memberAttributes)];
                }

                var methodFlags = SymbolFlags.IsMethod;
                if (isStatic) methodFlags |= SymbolFlags.IsStatic;
                if (isVirtual) methodFlags |= SymbolFlags.IsVirtual;
                if (isOverride) methodFlags |= SymbolFlags.IsOverride;
                if (isAbstract)
                    methodFlags |=
                        SymbolFlags.IsAbstract |
                        SymbolFlags.IsVirtual; // Astratto implica virtuale
                return [ParseMethodMember(access, methodFlags, memberAttributes)];

            case TokenKind.Property:
                if (isVirtual || isOverride || isAbstract)
                    throw CreateError(nextToken,
                        "VIRTUAL, OVERRIDE o ABSTRACT possono essere applicati solo a SUB o FUNCTION.");
                return [ParsePropertyMember(access, isStatic, memberAttributes)];

            case TokenKind.Operator:
                if (isVirtual || isOverride || isAbstract)
                    throw CreateError(nextToken, "Gli operatori non possono essere VIRTUAL, OVERRIDE o ABSTRACT.");
                if (access != AccessModifier.Public)
                    throw CreateError(nextToken, "Gli operatori devono essere PUBLIC.");

                var operatorFlags = SymbolFlags.IsOperator | SymbolFlags.IsMethod;
                if (isStatic)
                {
                    operatorFlags |= SymbolFlags.IsStatic;
                }

                return [ParseOperatorMember(access, operatorFlags, memberAttributes)];

            case TokenKind.Event:
                if (isVirtual || isOverride || isAbstract)
                    throw CreateError(nextToken, "Gli eventi non possono essere VIRTUAL, OVERRIDE o ABSTRACT.");
                return [ParseEventMember(access, isStatic, memberAttributes)];

            default:
                throw CreateError(nextToken,
                    "Prevista una definizione di membro (DIM, FUNCTION, SUB, PROPERTY, OPERATOR, EVENT).");
        }
    }

    /// <summary>
    /// [FASE 1] Parsa la parte centrale di un metodo di classe (nome, parametri, tipo di ritorno, corpo),
    /// assumendo che i modificatori (`PUBLIC`, `VIRTUAL`, `ABSTRACT`, etc.) siano già stati consumati.
    /// Ora supporta la sintassi a corpo di espressione e gestisce i metodi astratti (senza corpo).
    /// </summary>
    /// <param name="initialFlags">I flag (`IsMethod`, `IsStatic`, `IsAbstract`, etc.) già determinati dal chiamante.</param>
    /// <returns>Un nodo <see cref="FunctionDefinition"/> completo che rappresenta il metodo.</returns>
    /// <example>
    /// Sintassi a Blocco:
    /// <code>
    /// PUBLIC SUB MyMethod(x AS INTEGER)
    ///     PRINT x
    /// END SUB
    /// </code>
    /// Sintassi a Corpo di Espressione:
    /// <code>
    /// PUBLIC FUNCTION GetValue() AS INTEGER => 42
    /// PUBLIC SUB SetValue(x AS INTEGER) => _field = x
    /// </code>
    /// Sintassi Astratta:
    /// <code>
    /// PUBLIC ABSTRACT FUNCTION CalcolaArea() AS DOUBLE
    /// </code>
    /// </example>
    private FunctionDefinition ParseClassMethodBody(SymbolFlags initialFlags)
    {
        var isSub = Peek().Lexeme.Equals("SUB", StringComparison.OrdinalIgnoreCase);
        Consume(TokenKind.Function, "Expected 'FUNCTION' or 'SUB' keyword.");
        var nameToken = ConsumeProcedureName();

        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        DataType returnType;
        if (Match(TokenKind.As))
        {
            if (isSub)
                throw CreateError(Previous(), "A SUB procedure cannot have a return type specified with 'AS'.");
            returnType = ParseTypeName();
        }
        else
        {
            if (!isSub)
                throw CreateError(nameToken,
                    "A FUNCTION declaration must have an explicit return type using the 'AS' clause.");
            returnType = new VoidType();
        }

        // --- GESTIONE METODI ASTRATTI ---
        if (initialFlags.HasFlag(SymbolFlags.IsAbstract))
        {
            // Un metodo astratto non può avere un corpo. Deve essere seguito solo da un newline.
            if (Check(TokenKind.FatArrow))
            {
                throw CreateError(Peek(), "An ABSTRACT method cannot have an expression body '=>'.");
            }

            Consume(TokenKind.Newline, "An ABSTRACT method declaration must be followed by a newline.");

            // Il corpo è vuoto per definizione.
            return new FunctionDefinition(-1, nameToken, parameters, returnType, ImmutableArray<Stmt>.Empty,
                    initialFlags)
                { OriginalName = nameToken };
        }

        // --- GESTIONE METODI CONCRETI (CON CORPO) ---
        List<Stmt> bodyStmts;

        if (Match(TokenKind.FatArrow))
        {
            Stmt bodyStatement;

            if (isSub)
            {
                // Il corpo di una SUB è un'istruzione, non un'espressione.
                var statements = ParseStatement(null);
                if (statements is not [Stmt stmtBody])
                {
                    throw CreateError(Previous(),
                        "Expression body for a SUB method must be a single, valid statement.");
                }

                bodyStatement = stmtBody;
            }
            else
            {
                // Per una FUNCTION, il corpo è un'espressione che viene restituita implicitamente.
                var expressionBody = ParseExpression();
                bodyStatement = new ReturnStmt(expressionBody, Previous());
            }

            // Aggiungiamo un EndProcedureStmt fittizio per coerenza con i corpi a blocco.
            bodyStmts = [bodyStatement, new EndProcedureStmt(Previous())];
        }
        else
        {
            Consume(TokenKind.Newline, "Expected newline or '=>' after method signature.");

            bodyStmts = [];
            var bodyDecls = new List<AstNode>();

            _functionContextStack.Push(nameToken);
            _functionContextReturnsValue.Push(returnType is not VoidType);
            try
            {
                var dummyProc = new FunctionDefinition(-1, nameToken, parameters, returnType, [], initialFlags);

                while (!Check(TokenKind.EndFunction) && !IsAtEnd())
                {
                    while (Match(TokenKind.Newline))
                    {
                    }

                    if (Check(TokenKind.EndFunction) || IsAtEnd()) break;
                    if (Peek().Kind == TokenKind.LineNumber)
                        throw CreateError(Peek(), "Line numbers are not allowed inside a CLASS block.");

                    var clsPosBefore = _current;
                    ParseLogicalLine(bodyStmts, bodyDecls, dummyProc);
                    if (_current == clsPosBefore) ThrowIfStrayBlockTerminator();
                }
            }
            finally
            {
                _functionContextStack.Pop();
                _functionContextReturnsValue.Pop();
            }

            var endToken = Consume(TokenKind.EndFunction, "Expected 'END FUNCTION' or 'END SUB'.");
            bodyStmts.Add(new EndProcedureStmt(endToken));
        }

        return new FunctionDefinition(-1, nameToken, parameters, returnType, [..bodyStmts], initialFlags)
            { OriginalName = nameToken };
    }

    /// <summary>
    /// [FASE 1] Parsa un metodo (`SUB` o `FUNCTION`) come membro di una classe.
    /// Questo metodo è un wrapper che chiama il parser di definizione di funzione generico,
    /// passandogli i flag specifici del metodo (es. `IsVirtual`, `IsOverride`, `IsStatic`).
    /// </summary>
    /// <param name="access">Il modificatore di accesso già parsato.</param>
    /// <param name="methodFlags">I flag del simbolo (`IsMethod`, `IsVirtual`, etc.) già calcolati.</param>
    /// <returns>Un nodo AST <see cref="MethodMember"/>.</returns>
    private MethodMember ParseMethodMember(AccessModifier access, SymbolFlags methodFlags, ImmutableArray<Token> attributes)
    {
        // Delega il parsing del corpo del metodo all'helper dedicato,
        // passando i flag che abbiamo già raccolto.
        var procDef = ParseClassMethodBody(methodFlags);
        return new MethodMember(procDef, access, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa un campo (membro `DIM`) all'interno di una classe.
    /// </summary>
    /// <param name="access">Il modificatore di accesso (`PUBLIC`, `PRIVATE`, etc.) già parsato.</param>
    /// <param name="isStatic">True se il campo è stato dichiarato con la keyword `STATIC`.</param>
    /// <returns>Un nodo AST <see cref="FieldMember"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// PRIVATE DIM _health AS INTEGER = 100
    /// PUBLIC STATIC DIM InstanceCount AS LONG
    /// </code>
    /// </example>
    private List<FieldMember> ParseFieldMembers(AccessModifier access, bool isStatic, ImmutableArray<Token> attributes)
    {
        Consume(TokenKind.Dim, "Expected 'DIM' for a field declaration.");

        // Parsa l'intera lista di dichiarazioni, che può contenerne una o più.
        var declarations = ParseDeclarationListSyntaxOnly();

        var fieldMembers = new List<FieldMember>();

        // Itera su ogni dichiarazione parsata e crea un FieldMember per ciascuna.
        foreach (var decl in declarations)
        {
            var fieldFlags = SymbolFlags.IsField;
            if (isStatic)
            {
                fieldFlags |= SymbolFlags.IsStatic;
            }

            switch (access)
            {
                case AccessModifier.Public:
                    fieldFlags |= SymbolFlags.IsPublic;
                    break;
                case AccessModifier.Protected:
                    fieldFlags |= SymbolFlags.IsProtected;
                    break;
                case AccessModifier.Private:
                default:
                    fieldFlags |= SymbolFlags.IsPrivate;
                    break;
            }

            var updatedDecl = decl with { Flags = fieldFlags, SymbolId = -1 };

            fieldMembers.Add(new FieldMember(updatedDecl, access, attributes));
        }

        return fieldMembers;
    }

    /// <summary>
    /// [FASE 1] Parsa una proprietà (`PROPERTY...END PROPERTY`) all'interno di una classe.
    /// Ora gestisce anche la sintassi a corpo di espressione per le proprietà di sola lettura.
    /// </summary>
    /// <param name="access">Il modificatore di accesso (`PUBLIC`, `PRIVATE`, etc.) già parsato.</param>
    /// <param name="isStatic">True se la proprietà è stata dichiarata con la keyword `STATIC`.</param>
    /// <returns>Un nodo AST <see cref="PropertyMember"/>.</returns>
    /// <example>
    /// Sintassi a Blocco (completa):
    /// <code>
    /// PUBLIC PROPERTY Name AS STRING
    ///     GET
    ///         RETURN _name
    ///     END GET
    ///     SET(BYVAL value AS STRING)
    ///         _name = TRIM(value)
    ///     END SET
    /// END PROPERTY
    /// </code>
    /// Sintassi a Corpo di Espressione (sola lettura):
    /// <code>
    /// PUBLIC PROPERTY Name AS STRING => _name
    /// </code>
    /// </example>
    private PropertyMember ParsePropertyMember(AccessModifier access, bool isStatic, ImmutableArray<Token> attributes)
    {
        Consume(TokenKind.Property, "Expected 'PROPERTY'.");
        var nameToken = ConsumeNameToken("Expected property name.");
        var dataType = ParseTypeClause(nameToken);

        PropertyGetter? getter = null;
        PropertySetter? setter = null;

        // --- NUOVA LOGICA: Controllo per la sintassi a corpo di espressione '=>' ---
        if (Match(TokenKind.FatArrow))
        {
            // Se '=>' è presente, è una proprietà di sola lettura con corpo a espressione.
            var expressionBody = ParseExpression();

            // Il corpo del getter è un'istruzione di ritorno implicita.
            var returnStmt = new ReturnStmt(expressionBody, Previous());
            getter = new PropertyGetter([returnStmt]);

            // Non ci possono essere 'END PROPERTY' o blocchi SET in questa sintassi.
        }
        else
        {
            // --- Logica Esistente: Sintassi a blocco con GET/SET espliciti ---
            Consume(TokenKind.Newline, "Expected newline or '=>' after PROPERTY signature.");

            while (!Check(TokenKind.EndProperty) && !IsAtEnd())
            {
                if (Match(TokenKind.Get))
                {
                    if (getter != null) throw CreateError(Peek(), "GET block already defined for this property.");
                    getter = ParsePropertyGetter();
                }
                else if (Match(TokenKind.Set))
                {
                    if (setter != null) throw CreateError(Peek(), "SET block already defined for this property.");
                    setter = ParsePropertySetter(dataType);
                }
                else
                {
                    if (!Match(TokenKind.Newline))
                    {
                        throw CreateError(Peek(), "Expected GET, SET, or END PROPERTY.");
                    }
                }
            }

            Consume(TokenKind.EndProperty, "Expected 'END PROPERTY' to close the property block.");
        }


        if (getter == null && setter == null)
            throw CreateError(nameToken, "Property must have at least a GET or a SET block.");

        var flags = SymbolFlags.IsProperty;
        if (isStatic)
        {
            flags |= SymbolFlags.IsStatic;
        }

        switch (access)
        {
            case AccessModifier.Public:
                flags |= SymbolFlags.IsPublic;
                break;
            case AccessModifier.Protected:
                flags |= SymbolFlags.IsProtected;
                break;
            case AccessModifier.Private:
            default:
                flags |= SymbolFlags.IsPrivate;
                break;
        }

        var propDef = new PropertyDefinition(-1, nameToken, dataType, flags, getter, setter);

        return new PropertyMember(propDef, access, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa un blocco `GET...END GET` o un accessorio `GET` a corpo di espressione.
    /// Il corpo del getter è trattato come il corpo di una funzione.
    /// </summary>
    /// <returns>Un nodo AST <see cref="PropertyGetter"/> contenente il corpo del getter.</returns>
    /// <example>
    /// Sintassi a Blocco:
    /// <code>
    /// GET
    ///     RETURN _fieldName
    /// END GET
    /// </code>
    /// Sintassi a Corpo di Espressione:
    /// <code>
    /// GET => _fieldName
    /// </code>
    /// </example>
    private PropertyGetter ParsePropertyGetter()
    {
        var getToken = Previous(); // 'GET' è già stato consumato dal chiamante
        ImmutableArray<Stmt> body;

        // --- NUOVA LOGICA: Controllo per la sintassi a corpo di espressione '=>' ---
        if (Match(TokenKind.FatArrow))
        {
            var expressionBody = ParseExpression();
            var returnStmt = new ReturnStmt(expressionBody, Previous());
            body = [returnStmt];
            // La sintassi '=>' non ha un 'END GET'
        }
        else
        {
            // --- Logica Esistente: Sintassi a Blocco ---
            Consume(TokenKind.Newline, "Expected newline or '=>' after GET.");
            var dummyProc =
                new FunctionDefinition(-1, getToken, [], new InferredType(), [], SymbolFlags.None);
            body = ParseBlockBodyUntil(dummyProc, TokenKind.EndGet);
            Consume(TokenKind.EndGet, "Expected 'END GET'.");
        }

        return new PropertyGetter(body);
    }

    /// <summary>
    /// [FASE 1] Parsa un blocco `SET(value)...END SET` o un accessorio `SET` a corpo di espressione.
    /// Il corpo del setter è trattato come il corpo di una subroutine.
    /// </summary>
    /// <param name="propertyType">Il tipo di dato della proprietà, usato per validare il parametro `value`.</param>
    /// <returns>Un nodo AST <see cref="PropertySetter"/> contenente il parametro e il corpo del setter.</returns>
    /// <example>
    /// Sintassi a Blocco:
    /// <code>
    /// SET(BYVAL value AS INTEGER)
    ///     IF value > 0 THEN _field = value
    /// END SET
    /// </code>
    /// Sintassi a Corpo di Espressione:
    /// <code>
    /// SET(BYVAL value AS INTEGER) => _field = value
    /// </code>
    /// </example>
    private PropertySetter ParsePropertySetter(DataType propertyType)
    {
        var setToken = Previous(); // 'SET' è già stato consumato
        Consume(TokenKind.LeftParen, "Expected '(' after SET.");

        var isByRef = Match(TokenKind.ByRef);
        if (!isByRef) Match(TokenKind.ByVal);

        // VALUE e HAS sono keyword contestuali (nullable): accettali anche come nome parametro.
        var name = Consume("Expected parameter name for SET value.", TokenKind.Identifier, TokenKind.Value, TokenKind.Has);
        var valueDataType = ParseTypeClause(name);

        if (DataTypeToString(valueDataType) != DataTypeToString(propertyType))
            throw CreateError(name,
                $"The type of the 'value' parameter ('{DataTypeToString(valueDataType)}') must match the property type ('{DataTypeToString(propertyType)}').");

        var flags = SymbolFlags.IsParameter |
                    (isByRef ? SymbolFlags.IsByRef : SymbolFlags.None);
        var valueParameter = new Parameter(-1, name, valueDataType, flags, null, true) { OriginalName = name };

        Consume(TokenKind.RightParen, "Expected ')' after SET parameter.");

        ImmutableArray<Stmt> body;

        // --- NUOVA LOGICA: Controllo per la sintassi a corpo di espressione '=>' ---
        if (Match(TokenKind.FatArrow))
        {
            var statements = ParseStatement(null);
            if (statements is not [Stmt statementBody])
            {
                throw CreateError(Previous(), "Expression body for a SET accessor must be a single, valid statement.");
            }

            body = [statementBody];
        }
        else
        {
            // --- Logica Esistente: Sintassi a Blocco ---
            Consume(TokenKind.Newline, "Expected newline or '=>' after SET signature.");
            var dummyProc = new FunctionDefinition(-1, setToken, [valueParameter], new VoidType(), [],
                SymbolFlags.None);
            body = ParseBlockBodyUntil(dummyProc, TokenKind.EndSet);
            Consume(TokenKind.EndSet, "Expected 'END SET'.");
        }

        return new PropertySetter(valueParameter, body);
    }

    /// <summary>
    /// [FASE 1] Parsa un costruttore (`SUB NEW`) all'interno di una classe.
    /// Ora supporta la sintassi a corpo di espressione per le chiamate al costruttore base.
    /// </summary>
    /// <returns>Un nodo AST <see cref="ConstructorMember"/>.</returns>
    /// <example>
    /// Sintassi a Blocco:
    /// <code>
    /// PUBLIC SUB NEW(nomeIniziale AS STRING)
    ///     BASE.NEW(nomeIniziale)
    ///     SELF.Health = 100
    /// END SUB
    /// </code>
    /// Sintassi a Corpo di Espressione (solo per chiamare BASE.NEW):
    /// <code>
    /// PUBLIC SUB NEW(nomeIniziale AS STRING) => BASE.NEW(nomeIniziale)
    /// </code>
    /// </example>
    private ConstructorMember ParseConstructorMember(ImmutableArray<Token> attributes)
    {
        Consume(TokenKind.Function, "Expected 'SUB'.");
        var newToken = Consume(TokenKind.New, "Expected 'NEW'.");

        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        ImmutableArray<Stmt> body;

        // --- La sintassi a corpo di espressione '=>' è già corretta e rimane invariata ---
        if (Match(TokenKind.FatArrow))
        {
            if (!Check(TokenKind.Base) || At(1).Kind != TokenKind.Dot || At(2).Kind != TokenKind.New)
                throw CreateError(Peek(),
                    "Expression body for a constructor can only be a call to the base constructor (BASE.NEW(...)).");

            Advance(); // BASE
            Advance(); // .
            Advance(); // NEW
            var args = ParseArgumentListInParentheses();
            var baseCallStmt = new BaseConstructorCallStmt(args, newToken);
            body = [baseCallStmt, new EndProcedureStmt(Previous())];
        }
        else
        {
            // --- Logica Corretta: Sintassi a Blocco ---
            Consume(TokenKind.Newline, "Expected newline or '=>' after SUB NEW signature.");

            // Salta tutte le righe vuote (token Newline) che possono trovarsi tra la firma
            // del costruttore e la prima istruzione effettiva. Questo risolve il bug.
            while (Match(TokenKind.Newline))
            {
                /* Skip blank lines */
            }

            var bodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            // Questo controllo ora funziona correttamente anche in presenza di righe vuote.
            if (Check(TokenKind.Base) && At(1).Kind == TokenKind.Dot && At(2).Kind == TokenKind.New)
            {
                var baseToken = Consume(TokenKind.Base, "Expected 'BASE'.");
                Consume(TokenKind.Dot, "Expected '.' after 'BASE'.");
                Consume(TokenKind.New, "Expected 'NEW' after 'BASE.'.");
                var args = ParseArgumentListInParentheses();
                bodyBuilder.Add(new BaseConstructorCallStmt(args, baseToken));
                if (!IsAtEnd() && !Check(TokenKind.EndFunction))
                {
                    Consume(TokenKind.Newline, "Expected newline after BASE.NEW(...) call.");
                }
            }

            var dummyProc = new FunctionDefinition(-1, newToken, parameters, new VoidType(), [],
                SymbolFlags.None);
            var restOfBody = ParseBlockBodyUntil(dummyProc, TokenKind.EndFunction);
            bodyBuilder.AddRange(restOfBody);

            var endToken = Consume(TokenKind.EndFunction, "Expected 'END SUB' to close the constructor block.");
            bodyBuilder.Add(new EndProcedureStmt(endToken));
            body = bodyBuilder.ToImmutable();
        }

        var constructorDef = new ConstructorDefinition(parameters, body);
        return new ConstructorMember(constructorDef, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa un distruttore (`SUB DESTRUCTOR`) all'interno di una classe.
    /// </summary>
    /// <param name="access">Il modificatore di accesso (deve essere `PUBLIC`).</param>
    /// <returns>Un nodo AST <see cref="DestructorMember"/>.</returns>
    /// <example>
    /// Sintassi gestita (come SUB NEW, ma SENZA parentesi):
    /// <code>
    /// PUBLIC SUB DESTRUCTOR
    ///     ' Codice di pulizia...
    /// END SUB
    /// </code>
    /// </example>
    private DestructorMember ParseDestructorMember(AccessModifier access, ImmutableArray<Token> attributes)
    {
        if (access != AccessModifier.Public)
        {
            throw CreateError(Previous(), "Destructors (SUB DESTRUCTOR) must be PUBLIC.");
        }

        Consume(TokenKind.Function, "Expected 'SUB'.");
        var destructorToken = Consume(TokenKind.Destructor, "Expected 'DESTRUCTOR'.");

        if (Check(TokenKind.LeftParen))
        {
            throw CreateError(destructorToken, "A destructor (SUB DESTRUCTOR) cannot have parameters.");
        }

        Consume(TokenKind.Newline, "Expected newline after SUB DESTRUCTOR.");

        var dummyProc = new FunctionDefinition(-1, destructorToken, [], new VoidType(), [], SymbolFlags.None);
        var body = ParseBlockBodyUntil(dummyProc, TokenKind.EndFunction);

        Consume(TokenKind.EndFunction, "Expected 'END SUB' to close the destructor block.");

        var destructorDef = new DestructorDefinition(body);
        return new DestructorMember(destructorDef, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa un operatore (`OPERATOR...END OPERATOR`) come membro di una classe.
    /// Questo metodo funge da wrapper, chiamando il parser della definizione dell'operatore.
    /// </summary>
    /// <param name="access">Il modificatore di accesso (deve essere `PUBLIC`).</param>
    /// <param name="flags">I flag di base del simbolo (es. `IsOperator`, `IsStatic`) già determinati dal chiamante.</param>
    /// <returns>Un nodo AST <see cref="OperatorMember"/>.</returns>
    private OperatorMember ParseOperatorMember(AccessModifier access, SymbolFlags flags, ImmutableArray<Token> attributes)
    {
        // Passa i flag (che includono IsStatic) al parser della definizione.
        var opDef = ParseOperatorDefinition(access == AccessModifier.Public, flags);
        return new OperatorMember(opDef, access, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa la definizione completa di un operatore sovraccaricato (`OPERATOR...END OPERATOR`).
    /// </summary>
    /// <param name="isPublic">Flag che indica se l'operatore è stato dichiarato `PUBLIC`.</param>
    /// <param name="flags">I flag di base del simbolo (es. `IsOperator`, `IsStatic`) già determinati dal chiamante.</param>
    /// <returns>Un nodo AST <see cref="OperatorDefinition"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Operatore di istanza (implicito)
    /// PUBLIC OPERATOR +(BYVAL a AS Vettore, BYVAL b AS Vettore) AS Vettore
    ///     OPERATOR = NEW Vettore(a.X + b.X, a.Y + b.Y)
    /// END OPERATOR
    /// 
    /// ' Operatore statico (esplicito)
    /// PUBLIC STATIC OPERATOR -(BYVAL v AS Vettore) AS Vettore
    ///     OPERATOR = NEW Vettore(-v.X, -v.Y)
    /// END OPERATOR
    /// </code>
    /// </example>
    private OperatorDefinition ParseOperatorDefinition(bool isPublic, SymbolFlags flags)
    {
        Consume(TokenKind.Operator, "Expected 'OPERATOR'.");

        var operatorToken = Advance();
        if (!IsOverloadableOperator(operatorToken.Kind))
            throw CreateError(operatorToken, $"Operator '{operatorToken.Lexeme}' cannot be overloaded.");

        var parameters = ParseParameters();
        var returnType = ParseTypeClause(operatorToken);

        Consume(TokenKind.Newline, "Expected newline after OPERATOR signature.");

        var dummyProc =
            new FunctionDefinition(-1, operatorToken, parameters, returnType, [], SymbolFlags.None);
        var body = ParseBlockBodyUntil(dummyProc, TokenKind.EndOperator);
        Consume(TokenKind.EndOperator, "Expected 'END OPERATOR' to close the operator block.");

        // Aggiunge i flag di visibilità ai flag già passati.
        var finalFlags = flags | (isPublic ? SymbolFlags.IsPublic : SymbolFlags.IsPrivate);

        return new OperatorDefinition(operatorToken, finalFlags, parameters, returnType, body);
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione di evento (`EVENT...`) all'interno di una classe.
    /// VERSIONE CORRETTA: Utilizza un helper per consumare il nome dell'evento,
    /// accettando sia un identificatore generico sia una keyword valida come 'Click'.
    /// </summary>
    private EventMember ParseEventMember(AccessModifier access, bool isStatic, ImmutableArray<Token> attributes)
    {
        Consume(TokenKind.Event, "Expected 'EVENT'.");
        var nameToken = ConsumeEventName(); // Usa l'helper per il nome

        // Gli eventi definiscono la loro firma tramite una lista di parametri, come una SUB.
        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        var flags = SymbolFlags.IsEvent;
        if (isStatic) flags |= SymbolFlags.IsStatic;

        switch (access)
        {
            case AccessModifier.Public: flags |= SymbolFlags.IsPublic; break;
            case AccessModifier.Protected: flags |= SymbolFlags.IsProtected; break;
            case AccessModifier.Private:
            default: flags |= SymbolFlags.IsPrivate; break;
        }

        // Dopo la firma (con o senza parametri), ci deve essere un newline.
        Consume(TokenKind.Newline, "Expected a newline after event declaration.");

        var eventDef = new EventDefinition(-1, nameToken, parameters, flags);
        return new EventMember(eventDef, access, attributes);
    }
    
    /// <summary>
    /// [FASE 1] Analizza e parsa una definizione di interfaccia completa, dalla keyword `INTERFACE` fino a `END INTERFACE`.
    /// Questo metodo è responsabile di costruire il nodo AST `InterfaceDefinition` che rappresenta il contratto,
    /// inclusi il nome, la visibilità e le firme dei membri.
    /// </summary>
    /// <remarks>
    /// <b>Ruolo Architetturale:</b>
    /// Questo metodo è un parser di dichiarazioni di alto livello, analogo a `ParseClassDefinition`.
    /// Il suo unico scopo nella Fase 1 è tradurre la sintassi del blocco `INTERFACE` in una struttura dati
    /// AST, senza eseguire alcuna registrazione nella SymbolTable. L'AST generato sarà poi processato
    /// dal Binder (Fase 2) per creare il simbolo dell'interfaccia e dal Linker (Fase 3) per validare
    /// le classi che la implementano.
    /// 
    /// <b>Sintassi e Comportamento:</b>
    /// Il parser gestisce i modificatori di visibilità opzionali (`PUBLIC` o `PRIVATE`) prima della
    /// keyword `INTERFACE`. Se nessun modificatore è specificato, un'interfaccia è `PUBLIC` di default.
    /// Dopo aver parsato la firma (`INTERFACE Nome`), entra in un loop che delega il parsing di ogni
    /// membro a `ParseInterfaceMember` fino a quando non incontra la keyword di chiusura `END INTERFACE`.
    /// </remarks>
    /// <param name="initialFlags">
    /// Flag di visibilità (es. `IsPublic`) che potrebbero essere stati già parsati da un metodo chiamante,
    /// come `ParseStatement`. Se non vengono forniti, il metodo parserà i modificatori autonomamente.
    /// </param>
    /// <returns>
    /// Un nuovo nodo AST <see cref="InterfaceDefinition"/> che rappresenta l'interfaccia parsata,
    /// pronto per essere aggiunto alla lista delle dichiarazioni del modulo.
    /// </returns>
    /// <example>
    /// Il codice seguente mostra una definizione di interfaccia che questo metodo è in grado di parsare:
    /// <code>
    /// PUBLIC INTERFACE ISerializable
    ///     ' Firma di un metodo (FUNCTION) che restituisce una stringa.
    ///     FUNCTION Serialize() AS STRING
    /// 
    ///     ' Firma di un metodo (SUB) che accetta dati.
    ///     SUB Deserialize(data AS STRING)
    ///     
    ///     ' Firma di una proprietà di sola lettura.
    ///     PROPERTY IsDirty AS BOOLEAN
    /// END INTERFACE
    /// </code>
    /// </example>
    /// <exception cref="BasicException">
    /// Lanciata se la sintassi non è valida, ad esempio se manca il nome dell'interfaccia,
    /// se un membro non è valido per un'interfaccia (es. `DIM`), o se manca la keyword di chiusura `END INTERFACE`.
    /// </exception>
    private InterfaceDefinition ParseInterfaceDefinition(SymbolFlags initialFlags = SymbolFlags.None)
    {
        var flags = initialFlags | SymbolFlags.IsInterface;
        
        // Gestione visibilità se non passata dal chiamante
        if ((initialFlags & (SymbolFlags.IsPublic | SymbolFlags.IsPrivate)) == 0)
        {
             if (Match(TokenKind.Public)) flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
             else if (Match(TokenKind.Private)) flags |= SymbolFlags.IsPrivate;
             else flags |= SymbolFlags.IsPublic; // Default
        }

        Consume(TokenKind.Interface, "Expected 'INTERFACE' keyword.");
        var nameToken = Consume(TokenKind.Identifier, "Expected interface name.");

        // --- TYPE PARAMETERS: <T, U> ---
        var typeParameters = Check(TokenKind.LessThan)
            ? ParseTypeParameterList()
            : ImmutableArray<TypeParameter>.Empty;

        Consume(TokenKind.Newline, "Expected newline after INTERFACE declaration.");

        var members = ImmutableArray.CreateBuilder<ClassMember>();

        while (!Check(TokenKind.EndInterface) && !IsAtEnd())
        {
             while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber)) { /* Skip */ }
             if (Check(TokenKind.EndInterface) || IsAtEnd()) break;

             members.AddRange(ParseInterfaceMember());
        }

        Consume(TokenKind.EndInterface, "Expected 'END INTERFACE'.");

        return new InterfaceDefinition(-1, nameToken, flags, members.ToImmutable(), typeParameters)
        {
             DeclaredNamespace = _currentNamespace
        };
    }

    /// <summary>
    /// [FASE 1] Analizza e parsa una singola dichiarazione di membro all'interno di un blocco `INTERFACE`.
    /// Riconosce le firme di metodi (`FUNCTION`/`SUB`), proprietà (`PROPERTY`) ed eventi (`EVENT`),
    /// applicando le regole semantiche specifiche delle interfacce.
    /// </summary>
    /// <remarks>
    /// <b>Ruolo Architetturale:</b>
    /// Questo metodo è un componente chiave della fase di parsing puro (Fase 1). Agisce come un dispatcher
    /// che esamina il token successivo per determinare il tipo di membro da parsare (metodo, proprietà o evento)
    /// e delega l'analisi al parser specializzato corretto. La sua responsabilità è costruire i nodi AST
    /// che rappresentano il contratto dell'interfaccia, senza alcuna implementazione.
    /// 
    /// <b>Regole Semantiche Applicate:</b>
    /// Tutti i membri di un'interfaccia sono considerati parte del suo contratto pubblico e sono astratti
    /// per definizione. Questo metodo applica programmaticamente i flag
    /// <see cref="SymbolFlags.IsPublic"/> e <see cref="SymbolFlags.IsAbstract"/>
    /// ai nodi AST generati, ignorando eventuali modificatori di accesso espliciti.
    /// 
    /// Le interfacce non possono contenere stato o implementazione. Questo metodo lancia un'eccezione
    /// <see cref="BasicException"/> se incontra dichiarazioni di campi (`DIM`), costanti (`CONST`),
    /// costruttori (`SUB NEW`), o operatori. Per le dichiarazioni `FUNCTION` e `SUB`, viene
    /// garantito che venga analizzata solo la firma, senza un corpo di implementazione.
    /// </remarks>
    /// <returns>
    /// Un `IEnumerable&lt;ClassMember&gt;` contenente il membro di interfaccia parsato. Sebbene il tipo di ritorno
    /// sia una collezione per coerenza con altri parser di membri, in questo contesto viene restituito sempre un singolo elemento.
    /// </returns>
    /// <example>
    /// Il codice seguente mostra le dichiarazioni valide che questo metodo è in grado di parsare all'interno di un'interfaccia:
    /// <code>
    /// INTERFACE IDisposable
    ///     ' Firma di un metodo (SUB)
    ///     SUB Dispose()
    /// 
    ///     ' Firma di una funzione (FUNCTION)
    ///     FUNCTION GetHandle() AS LONG
    /// 
    ///     ' Firma di una proprietà (PROPERTY)
    ///     PROPERTY IsDisposed AS BOOLEAN
    ///     
    ///     ' Firma di un evento (EVENT)
    ///     EVENT OnDispose()
    /// END INTERFACE
    /// </code>
    /// </example>
    /// <exception cref="BasicException">
    /// Lanciata se viene incontrato un tipo di membro non valido per un'interfaccia (es. `DIM`, `CONST`, `OPERATOR`)
    /// o un modificatore non permesso (es. `PRIVATE`, `STATIC`, `VIRTUAL`).
    /// </exception>
    private IEnumerable<ClassMember> ParseInterfaceMember()
    {
        var attributes = ParseAttributes();
        // Ignora modificatori di accesso espliciti (sono sempre Public)
        if (Match(TokenKind.Public)) { } 

        var token = Peek();
        
        // I membri di interfaccia sono implicitamente metodi astratti (firme senza corpo)
        const SymbolFlags interfaceMethodFlags = 
            SymbolFlags.IsMethod | 
            SymbolFlags.IsPublic | 
            SymbolFlags.IsAbstract | 
            SymbolFlags.IsVirtual;

        switch (token.Kind)
        {
            case TokenKind.Function:
                // ParseClassMethodBody gestisce correttamente la logica ABSTRACT se il flag è impostato
                var procDef = ParseClassMethodBody(interfaceMethodFlags);
                return [new MethodMember(procDef, AccessModifier.Public, attributes)];

            case TokenKind.Property:
                // Per le proprietà, dobbiamo parsare solo la firma
                return [ParseInterfacePropertyMember(attributes)];
            
            case TokenKind.Event:
                 return [ParseEventMember(AccessModifier.Public, false, ImmutableArray<Token>.Empty)];

            default:
                throw CreateError(token, "Interfaces can only contain SUB, FUNCTION, PROPERTY, or EVENT signatures.");
        }
    }

    /// <summary>
    /// [FASE 1] Analizza e parsa la firma di una proprietà all'interno di una definizione di interfaccia.
    /// A differenza delle proprietà di classe, le proprietà di interfaccia non contengono blocchi di codice
    /// per <c>GET</c> o <c>SET</c>, ma definiscono esclusivamente il contratto (Nome e Tipo).
    /// </summary>
    /// <remarks>
    /// <para><strong>Comportamento del Parser:</strong></para>
    /// Il metodo consuma la keyword <c>PROPERTY</c>, l'identificatore del nome e la clausola di tipo (<c>AS Tipo</c>).
    /// Poiché le interfacce definiscono solo contratti, vengono generati automaticamente accessori
    /// <c>GET</c> e <c>SET</c> astratti (privi di corpo) per rappresentare la natura di lettura/scrittura
    /// della proprietà nel contratto.
    /// <para>
    /// I flag del simbolo risultante sono impostati automaticamente su:
    /// <see cref="SymbolFlags.IsProperty"/> |
    /// <see cref="SymbolFlags.IsPublic"/> |
    /// <see cref="SymbolFlags.IsAbstract"/>.
    /// </para>
    /// </remarks>
    /// <returns>
    /// Un nodo AST <see cref="PropertyMember"/> contenente la definizione astratta della proprietà,
    /// pronta per essere inclusa nella lista dei membri dell'interfaccia.
    /// </returns>
    /// <example>
    /// Esempio di definizione di proprietà in un'interfaccia:
    /// <code>
    /// INTERFACE IGeometria
    ///     ' Proprietà definita solo dalla firma
    ///     PROPERTY Area AS DOUBLE
    ///     PROPERTY Perimetro AS DOUBLE
    /// END INTERFACE
    /// </code>
    /// </example>
    private PropertyMember ParseInterfacePropertyMember(ImmutableArray<Token> attributes)
    {
        Consume(TokenKind.Property, "Expected 'PROPERTY'.");
        var nameToken = ConsumeNameToken("Expected property name.");
        var dataType = ParseTypeClause(nameToken);

        // Nelle interfacce moderne, ci aspettiamo indicazioni su Get/Set ma senza corpo.
        // Esempio semplificato: PROPERTY MyProp AS STRING (sottinteso Get/Set o solo Get?)
        // Per ora supportiamo la definizione standard e assumiamo GET/SET impliciti o richiediamo la sintassi a blocco vuota?
        // Usiamo una sintassi semplificata per le interfacce: PROPERTY Name AS Type
        // Questo implica solitamente GET e SET. Se vogliamo specificare, servirebbe sintassi tipo { GET; SET; }
        
        // Per coerenza con il parser attuale che è line-based:
        // PROPERTY Name AS Type
        // GET
        // END GET
        // ... è troppo verboso per un'interfaccia.
        
        // Assumiamo che PROPERTY in interfaccia definisca una proprietà con Get e Set.
        // Se si vuole solo Get, si usa ReadOnly (non ancora supportato nel parser).
        
        // Creiamo accessori fittizi astratti
        var getter = new PropertyGetter(ImmutableArray<Stmt>.Empty); // Vuoto
        
        // Creiamo un setter fittizio
        var valueParam = new Parameter(-1, new Token(TokenKind.Identifier, "value", null, SourcePosition.Unknown), dataType, SymbolFlags.IsParameter, null, true);
        var setter = new PropertySetter(valueParam, ImmutableArray<Stmt>.Empty);

        var flags = SymbolFlags.IsProperty | SymbolFlags.IsPublic | SymbolFlags.IsAbstract;
        var propDef = new PropertyDefinition(-1, nameToken, dataType, flags, getter, setter);

        Consume(TokenKind.Newline, "Expected newline after PROPERTY signature.");

        return new PropertyMember(propDef, AccessModifier.Public, attributes);
    }

    /// <summary>
    /// [FASE 1] Parsa una lista di attributes nella sintaxis &lt;AttributeName&gt;.
    /// Gli attributes possono essere applicati a classi, campi, metodi, proprietà e variabili.
    /// </summary>
    private ImmutableArray<Token> ParseAttributes()
    {
        var attributes = ImmutableArray.CreateBuilder<Token>();
        while (Check(TokenKind.LessThan))
        {
            Advance();
            var nameToken = Peek();
            if (nameToken.Kind == TokenKind.Identifier || nameToken.Kind == TokenKind.Serializable || nameToken.Kind == TokenKind.Required)
            {
                Advance();
                attributes.Add(nameToken);
            }
            else
            {
                throw CreateError(nameToken, $"Expected attribute name after '<', got '{nameToken.Lexeme}'.");
            }
            Consume(TokenKind.GreaterThan, "Expected '>' after attribute name.");
        }
        return attributes.ToImmutable();
    }

}