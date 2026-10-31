// FILE: Interpreter.Parser.Declarations.Udts.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene le versioni "pure" (Fase 1) dei metodi del Parser per analizzare
/// la sintassi degli UDT e dei loro membri, senza interagire con la SymbolTable.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [NUOVO HELPER - CORRETTO] Verifica ricorsivamente se un tipo di dato è valido come campo di un UDT.
    /// I campi di un UDT devono avere una dimensione fissa in memoria. Gli array sono permessi
    /// solo se il loro tipo base è a sua volta a dimensione fissa.
    /// </summary>
    /// <param name="dt">Il DataType da controllare.</param>
    /// <returns>True se il tipo è a dimensione fissa; altrimenti False.</returns>
    private static bool IsValidUdtFieldType(DataType dt)
    {
        return dt switch
        {
            // Tipi non validi (dimensione intrinsecamente variabile o gestiti da reference speciali)
            ListType or DictType or HashSetType or QueueType or StackType or SortedDictType or JsonType
                or StringType { FixedLength: null } or AnyType => // Stringa a lunghezza variabile
                false,
            // Gli array sono validi SE il loro tipo base è valido.
            ArrayType at => IsValidUdtFieldType(at.ElementType),
            // Tipi composti validi: un UDT può contenere un altro UDT
            UserDefinedType udt => udt.Fields.All(f => IsValidUdtFieldType(f.DataType)),
            _ => true
        };
    }

    /// <summary>
    /// [FASE 1] Parsa un singolo campo all'interno di un `TYPE`, gestendo e validando
    /// gli array a dimensione fissa.
    /// VERSIONE CORRETTA: Parsa le dimensioni e le memorizza nel nodo TypeField.
    /// </summary>
    private TypeField ParseTypeField()
    {
        var nameToken = ConsumeProcedureName();
        var dimensions = ImmutableArray<Expr>.Empty;

        // Controlla la presenza di dimensioni dell'array, es. `items(5)`
        if (Match(TokenKind.LeftParen))
        {
            if (Check(TokenKind.RightParen))
            {
                // Sintassi `Nome()` - Array dinamico, non permesso.
                throw CreateError(Previous(),
                    "Dynamic arrays with empty parentheses `()` are not allowed as fields in a TYPE definition. Use fixed dimensions like `(10)`.");
            }

            dimensions = ParseExpressionList(TokenKind.RightParen);
            Consume(TokenKind.RightParen, "Expected ')' after array dimensions.");
        }

        // Parsa il tipo di dato base (es. STRING * 10, INTEGER)
        var baseDataType = ParseTypeClause(nameToken);

        // Se sono state specificate le dimensioni, il tipo finale è un array.
        var finalDataType = !dimensions.IsEmpty ? new ArrayType(baseDataType) : baseDataType;

        // --- VALIDAZIONE ---
        // I campi a lunghezza variabile (STRING, LIST/DICT, ANY) sono LECITI in un TYPE.
        // La lunghezza fissa NON è più richiesta a livello di dichiarazione: è imposta solo
        // dai comandi a record (OPEN RANDOM, DBF, FIELD, mmap/FFI) tramite
        // UserDefinedType.IsFixedSize. Vedi Parser.Declarations.Udts.IsValidUdtFieldType.

        Expr? defaultValue = null;
        if (Match(TokenKind.Equal))
        {
            defaultValue = ParseExpression();
        }

        // Crea il nodo TypeField passando le dimensioni parsate.
        return new TypeField(-1, nameToken, baseDataType, dimensions, defaultValue);
    }

    /// <summary>
    /// [FASE 1] Parsa un blocco `TYPE...END TYPE` e i suoi membri, senza registrare alcun simbolo.
    /// VERSIONE CORRETTA: Assegna correttamente i flag di visibilità e popola la lista dei metodi.
    /// </summary>
    private UserDefinedType ParseTypeDefinition()
    {
        var flags = SymbolFlags.IsStruct;
        var isPacked = Match(TokenKind.Packed);

        // Un TYPE a livello di modulo è pubblico nel suo modulo.
        // Solo la keyword PUBLIC lo rende esportabile.
        if (Match(TokenKind.Public))
        {
            flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
        }
        else if (Match(TokenKind.Private))
        {
             // Se è esplicitamente privato, lo marchiamo come tale.
            flags |= SymbolFlags.IsPrivate;
        }
        else
        {
            // Se non c'è modificatore, il default è pubblico all'interno del modulo.
            flags |= SymbolFlags.IsPublic;
        }


        Consume(TokenKind.Type, "Expected 'TYPE'.");
        var nameToken = ConsumeProcedureName();

        // --- TYPE PARAMETERS: <T1, T2, ...> ---
        var typeParameters = Check(TokenKind.LessThan)
            ? ParseTypeParameterList()
            : ImmutableArray<TypeParameter>.Empty;

        var implementedInterfaces = ImmutableArray.CreateBuilder<Token>();
        if (Match(TokenKind.Implements))
        {
            do
            {
                var interfaceToken = Peek();
                if (!IsPermissiveNameToken(interfaceToken))
                    throw CreateError(interfaceToken, "Expected an interface name after 'IMPLEMENTS'.");
                implementedInterfaces.Add(Advance());
            } while (Match(TokenKind.Comma));
        }

        Consume(TokenKind.Newline, "Expected newline after TYPE definition.");

        var fields = ImmutableArray.CreateBuilder<TypeField>();
        var properties = ImmutableArray.CreateBuilder<PropertyDefinition>();
        var operators = ImmutableArray.CreateBuilder<OperatorDefinition>();
        var methods = ImmutableArray.CreateBuilder<ProcedureDefinition>();
        var constructors = ImmutableArray.CreateBuilder<ConstructorDefinition>();
        var destructors = ImmutableArray.CreateBuilder<DestructorDefinition>();

        while (!Check(TokenKind.EndType) && !IsAtEnd())
        {
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
            {
                /* Skip */
            }

            if (Check(TokenKind.EndType) || IsAtEnd()) break;

            var memberIsPublic = Match(TokenKind.Public);
            var memberIsPrivate = false;
            if (!memberIsPublic)
            {
                memberIsPrivate = Match(TokenKind.Private);
            }

            if (Check(TokenKind.Property))
            {
                properties.Add(ParsePropertyDefinition(memberIsPublic));
            }
            else if (Check(TokenKind.Operator))
            {
                operators.Add(ParseOperatorDefinition(memberIsPublic));
            }
            else if (Check(TokenKind.Function))
            {
                if (At(1).Kind == TokenKind.New)
                {
                    if (memberIsPublic || memberIsPrivate)
                        throw CreateError(Previous(), "Access modifiers are not allowed on constructors (SUB NEW).");
                    constructors.Add(ParseConstructorDefinition());
                }
                else if (At(1).Kind == TokenKind.Destructor)
                {
                    if (memberIsPublic || memberIsPrivate)
                        throw CreateError(Previous(), "Access modifiers are not allowed on destructors (SUB DESTRUCTOR).");
                    if (destructors.Any())
                        throw CreateError(At(1), "A TYPE can only have one destructor (SUB DESTRUCTOR).");
                    destructors.Add(ParseDestructorDefinition());
                }
                else
                {
                    var methodFlags = SymbolFlags.IsMethod;
                    if (memberIsPrivate)
                    {
                        methodFlags |= SymbolFlags.IsPrivate;
                    }
                    else
                    {
                        // Se non è specificato PRIVATE, il default per i membri di un TYPE è PUBLIC.
                        methodFlags |= SymbolFlags.IsPublic;
                    }
                    methods.Add(ParseFunctionDefinition(methodFlags));
                }
            }
            else
            {
                if (memberIsPublic || memberIsPrivate)
                    throw CreateError(Previous(),
                        "Access modifiers (PUBLIC/PRIVATE) are not allowed on fields in a TYPE definition.");
                fields.Add(ParseTypeField());
            }
        }

        Consume(TokenKind.EndType, "Expected 'END TYPE' to close the type block.");

        // --- RESOLVE TYPE PARAMETERS in fields and methods ---
        if (typeParameters.Length > 0)
        {
            var typeParamNames = new HashSet<string>(
                typeParameters.Select(tp => tp.Name.Lexeme), StringComparer.OrdinalIgnoreCase);
            for (var i = 0; i < fields.Count; i++)
            {
                var f = fields[i];
                fields[i] = f with { DataType = ResolveTypeParametersInType(f.DataType, typeParameters, typeParamNames) };
            }
            for (var i = 0; i < methods.Count; i++)
            {
                var m = methods[i];
                var newParams = ResolveTypeParametersInParameters(m.Parameters, typeParameters, typeParamNames);
                if (m is FunctionDefinition fd)
                {
                    var newReturnType = ResolveTypeParametersInType(fd.ReturnType, typeParameters, typeParamNames);
                    methods[i] = fd with { Parameters = newParams, ReturnType = newReturnType };
                }
                else
                {
                    methods[i] = m with { Parameters = newParams };
                }
            }
        }

        Log($"[PARSER-NS-TRACE] Creazione UDT '{nameToken.Lexeme}'. Namespace attivo corrente: '{_currentNamespace ?? "_global"}'");
        var udt = new UserDefinedType(-1, nameToken, fields.ToImmutable(), flags, typeParameters)
        {
            IsPacked = isPacked,
            ImplementedInterfaceNames = implementedInterfaces.ToImmutable(),
            Properties = properties.ToImmutable(),
            Operators = operators.ToImmutable(),
            Methods = methods.ToImmutable(),
            Constructors = constructors.ToImmutable(),
            Destructors = destructors.ToImmutable(),
            DeclaredNamespace = _currentNamespace
        };

        foreach (var method in udt.Methods)
        {
            method.Parent = udt;
        }

        return udt;
    }

    /// <summary>
    /// [FASE 1] Parsa la definizione di una proprietà (PROPERTY...END PROPERTY) in un UDT.
    /// </summary>
    private PropertyDefinition ParsePropertyDefinition(bool isPublic)
    {
        Consume(TokenKind.Property, "Expected 'PROPERTY'.");
        var nameToken = ConsumeNameToken("Expected property name.");
        var dataType = ParseTypeClause(nameToken);

        Consume(TokenKind.Newline, "Expected newline after PROPERTY signature.");

        PropertyGetter? getter = null;
        PropertySetter? setter = null;

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
                Consume(TokenKind.Newline, "Expected newline or member definition.");
                if (IsAtEnd() || Check(TokenKind.EndProperty)) continue;
                throw CreateError(Peek(), "Expected GET, SET, or END PROPERTY.");
            }
        }

        Consume(TokenKind.EndProperty, "Expected 'END PROPERTY' to close the property block.");

        if (getter == null && setter == null)
            throw CreateError(nameToken, "Property must have at least a GET or a SET block.");

        var flags = SymbolFlags.IsProperty |
                    (isPublic ? SymbolFlags.IsPublic : SymbolFlags.IsPrivate);
        return new PropertyDefinition(-1, nameToken, dataType, flags, getter, setter);
    }

    /// <summary>
    /// [FASE 1] Parsa la definizione di un costruttore (SUB NEW...END SUB) in un UDT.
    /// </summary>
    private ConstructorDefinition ParseConstructorDefinition()
    {
        Consume(TokenKind.Function, "Expected 'SUB'.");
        var newToken = Consume(TokenKind.New, "Expected 'NEW'.");

        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        Consume(TokenKind.Newline, "Expected newline after SUB NEW signature.");

        var bodyBuilder = ImmutableArray.CreateBuilder<Stmt>();

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

        var dummyProc =
            new FunctionDefinition(-1, newToken, parameters, new VoidType(), [], SymbolFlags.None);
        var restOfBody = ParseBlockBodyUntil(dummyProc, TokenKind.EndFunction);
        bodyBuilder.AddRange(restOfBody);

        Consume(TokenKind.EndFunction, "Expected 'END SUB' to close the constructor block.");

        return new ConstructorDefinition(parameters, bodyBuilder.ToImmutable());
    }

    /// <summary>
    /// [FASE 1] Parsa la definizione di un distruttore (SUB DESTRUCTOR...END SUB) in un UDT.
    /// </summary>
    private DestructorDefinition ParseDestructorDefinition()
    {
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

        return new DestructorDefinition(body);
    }

    /// <summary>
    /// [FASE 1] Parsa la definizione di un operatore sovraccaricato (OPERATOR...END OPERATOR) in un UDT.
    /// </summary>
    private OperatorDefinition ParseOperatorDefinition(bool isPublic)
    {
        Consume(TokenKind.Operator, "Expected 'OPERATOR'.");

        var operatorToken = Advance();
        if (!IsOverloadableOperator(operatorToken.Kind))
            throw CreateError(operatorToken, $"Operator '{operatorToken.Lexeme}' cannot be overloaded.");

        var parameters = ParseParameters();
        var returnType = ParseTypeClause(operatorToken);

        // La validazione dei parametri è semantica e verrà fatta dal Linker.
        Consume(TokenKind.Newline, "Expected newline after OPERATOR signature.");

        var dummyProc =
            new FunctionDefinition(-1, operatorToken, parameters, returnType, [], SymbolFlags.None);
        var body = ParseBlockBodyUntil(dummyProc, TokenKind.EndOperator);
        Consume(TokenKind.EndOperator, "Expected 'END OPERATOR' to close the operator block.");

        var flags = SymbolFlags.IsOperator | SymbolFlags.IsMethod |
                    (isPublic ? SymbolFlags.IsPublic : SymbolFlags.IsPrivate);

        return new OperatorDefinition(operatorToken, flags, parameters, returnType, body);
    }

    /// <summary>
    /// Determina se un operatore rappresentato da un <see cref="TokenKind"/> può essere sovraccaricato.
    /// </summary>
    /// <param name="kind">Il tipo di token da controllare.</param>
    /// <returns><c>true</c> se l'operatore può essere sovraccaricato; altrimenti, <c>false</c>.</returns>
    private static bool IsOverloadableOperator(TokenKind kind)
    {
        return kind switch
        {
            // Operatori binari aritmetici
            TokenKind.Plus => true,
            TokenKind.Minus => true,
            TokenKind.Multiply => true,
            TokenKind.Divide => true,
            TokenKind.IntegerDivide => true,
            TokenKind.Mod => true,
            TokenKind.Power => true,

            // Operatori binari di comparazione
            TokenKind.Equal => true,
            TokenKind.NotEqual => true,
            TokenKind.LessThan => true,
            TokenKind.LessEqual => true,
            TokenKind.GreaterThan => true,
            TokenKind.GreaterEqual => true,

            // Operatori binari logici
            TokenKind.And => true,
            TokenKind.Or => true,

            // Operatori unari
            TokenKind.Not => true,

            // Operatore di conversione
            TokenKind.CType => true,

            // Operatori non sovraccaricabili
            _ => false
        };
    }
}