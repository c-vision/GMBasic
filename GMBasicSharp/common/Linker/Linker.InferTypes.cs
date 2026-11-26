// FILE: Interpreter.Linker.InferTypes.cs
// ReSharper disable RedundantArgumentDefaultValue
// ReSharper disable TailRecursiveCall

using System.Collections.Immutable;
using Basic.RegexNative;

namespace Basic;

public static partial class Linker
{
    /// <summary>Promuove ricorsivamente il tipo ELEMENTO di un letterale array annidato oltre 2
    /// livelli (es. <c>{{{1},{2}},{{3},{4}}}</c>) — usata dal case ArrayLiteralExpr di
    /// <see cref="LinkerInferExpressionType"/> quando gli elementi interni sono a loro volta
    /// tutti ArrayType. Se un livello NON è interamente ArrayType, ricade sul fold scalare
    /// normale (GetPromotedType), che lì funziona correttamente.</summary>
    private static DataType PromoteNestedArrayElementType(List<DataType> elementTypes, SourcePosition position,
        AstRewriteContext context, BasicProgram program)
    {
        var innerElementTypes = elementTypes.Cast<ArrayType>().Select(a => a.ElementType).ToList();
        var innerCommon = innerElementTypes.All(t => t is ArrayType)
            ? PromoteNestedArrayElementType(innerElementTypes, position, context, program)
            : innerElementTypes.Aggregate((DataType)new InferredType(),
                (acc, t) => GetPromotedType(acc, t, new Token(TokenKind.Plus, "+", null, position), context, program));
        if (innerCommon is InferredType) innerCommon = new DoubleType();
        return new ArrayType(innerCommon);
    }

    /// <summary>
    /// Determina il tipo di dato risultante di un'operazione binaria,
    /// gestendo la promozione dei tipi e l'overloading degli operatori.
    /// </summary>
    private static DataType GetPromotedType(DataType left, DataType right, Token op, AstRewriteContext context,
        BasicProgram program)
    {
        // --- Validazione OPTION STRICT ---
        if (context.IsInStrictContext)
        {
            var isLeftNumericOp = IsNumericType(left);
            var isRightNumericOp = IsNumericType(right);
            var isLeftString = left is StringType;
            var isRightString = right is StringType;

            // Se i tipi sono diversi e non sono entrambi numerici, è un potenziale errore.
            if (left.GetType() != right.GetType() && !(isLeftNumericOp && isRightNumericOp))
            {
                // L'unica operazione permessa tra stringa e altro tipo è la concatenazione (+)
                if (op.Kind == TokenKind.Plus && (isLeftString || isRightString))
                {
                    // In modalità strict, vietiamo anche la concatenazione implicita
                    throw CreateError(op,
                        $"OPTION STRICT ON: L'operatore '{op.Lexeme}' non può essere applicato a operandi di tipo '{left}' e '{right}'. È richiesta una conversione di tipo esplicita (es. CSTR()).",
                        ErrorCode.TypeMismatch);
                }
                // Per tutti gli altri operatori, se i tipi non sono numerici e diversi, è un errore.
                else if (isLeftNumericOp != isRightNumericOp)
                {
                    throw CreateError(op,
                        $"OPTION STRICT ON: L'operatore '{op.Lexeme}' non può essere applicato a operandi di tipo '{left}' e '{right}'.",
                        ErrorCode.TypeMismatch);
                }
            }
        }

        // 1. Controllo Operator Overloading su UDT
        if (left is UserDefinedType { SymbolId: >= 0 } udt)
        {
            var symbol = program.Symbols.GetSymbol(udt.SymbolId);
            if (symbol.DeclarationNode is UserDefinedType udtDef)
            {
                var opMatch = udtDef.Operators.FirstOrDefault(o =>
                    o.OperatorToken.Kind == op.Kind &&
                    o.Parameters.Length == 2 &&
                    CanCoerce(left, o.Parameters[0].DataType, context, program) &&
                    CanCoerce(right, o.Parameters[1].DataType, context, program)
                );
                if (opMatch != null) return opMatch.ReturnType;
            }
        }

        // 2. Controllo Operator Overloading su Classi
        if (left is ClassType { Definition.SymbolId: >= 0 } ct)
        {
            var symbol = program.Symbols.GetSymbol(ct.Definition.SymbolId);
            if (symbol.DeclarationNode is ClassDefinition classDef)
            {
                var current = classDef;
                while (current != null)
                {
                    var opMatch = current.Members.OfType<OperatorMember>().Select(m => m.Operator).FirstOrDefault(o =>
                        o.OperatorToken.Kind == op.Kind &&
                        o.Parameters.Length == 2 &&
                        CanCoerce(left, o.Parameters[0].DataType, context, program) &&
                        CanCoerce(right, o.Parameters[1].DataType, context, program)
                    );
                    if (opMatch != null) return opMatch.ReturnType;

                    current = current.ResolvedBaseClass;
                }
            }
        }

        // 3. Logica Standard Primitiva
        if (op.Kind == TokenKind.QuestionMarkQuestion)
        {
            if (AreSameType(left, right, context, program)) return left;
            if (left is DoubleType && right is not InferredType && right is not DoubleType) return right;
            if (right is DoubleType && left is not InferredType && left is not DoubleType) return left;
            if (CanCoerce(right, left, context, program)) return left;
            if (CanCoerce(left, right, context, program)) return right;
            throw CreateError(op, $"Tipi incompatibili per l'operatore '??': '{left}' e '{right}'.",
                ErrorCode.TypeMismatch);
        }

        if (op.Kind == TokenKind.Plus && (left is StringType || right is StringType)) return new StringType();

        // Date Arithmetic
        if (op.Kind == TokenKind.Plus)
        {
            if ((left is DateType or DateTimeType) && IsNumericType(right)) return left; // Date + Num = Date
            if (IsNumericType(left) && (right is DateType or DateTimeType)) return right; // Num + Date = Date
            if ((left is DateType or DateTimeType) && right is TimeSpanType) return left;
            if (left is TimeSpanType && (right is DateType or DateTimeType)) return right;
        }

        if (op.Kind == TokenKind.Minus)
        {
            if ((left is DateType or DateTimeType) && IsNumericType(right)) return left; // Date - Num = Date
            if ((left is DateType or DateTimeType) && (right is DateType or DateTimeType)) return new TimeSpanType();
            if (left is TimeType && right is TimeType) return new TimeSpanType();
        }

        // Confronti/logici → SEMPRE BitType: la promo numerica sotto produce DoubleType per
        // operandi misti (es. `o.Quantita > 5` con Int16 vs Double) e per Bit vs Double —
        // un confronto/AND/OR non è mai DoubleType (una lambda predicato `A AND B` deduceva
        // DoubleType → ASCAN riceveva Func<T,double> invece di Func<T,bool> → cast invalido).
        if (op.Kind is TokenKind.Equal or TokenKind.NotEqual or TokenKind.LessThan or TokenKind.LessEqual
            or TokenKind.GreaterThan or TokenKind.GreaterEqual or TokenKind.ExactlyEqual
            or TokenKind.And or TokenKind.Or)
            return new BitType();

        // Numeric Promotion - Guarded by IsNumericType to avoid capturing UDTs
        var isLeftNumeric = IsNumericType(left) || left is BitType || left is InferredType;
        var isRightNumeric = IsNumericType(right) || right is BitType || right is InferredType;

        if (isLeftNumeric && isRightNumeric)
        {
            if (left is DoubleType || right is DoubleType) return new DoubleType();
            if (left is SingleType || right is SingleType) return new SingleType();
            if (left is CurrencyType || right is CurrencyType) return new CurrencyType();
            if (left is LongType || right is LongType) return new LongType();
            if (left is IntegerType || right is IntegerType) return new IntegerType();
        }

        // Se l'operazione non è tra tipi numerici, e non è un'operazione su tipi custom (gestita sopra),
        // allora è un confronto o un'operazione logica su altri tipi primitivi (es. stringhe).
        if (left is not ClassType && left is not UserDefinedType && right is not ClassType &&
            right is not UserDefinedType)
        {
            return op.Kind switch
            {
                TokenKind.Equal or TokenKind.NotEqual or TokenKind.LessThan or TokenKind.LessEqual
                    or TokenKind.GreaterThan
                    or TokenKind.GreaterEqual or TokenKind.And or TokenKind.Or
                    or TokenKind.ExactlyEqual => new BitType(),
                _ => left
            };
        }

        // Fallback per tipi custom senza un operatore sovraccaricato corrispondente.
        // Restituisce il tipo di sinistra per permettere all'inferenza di continuare,
        // lasciando che un eventuale errore venga catturato in una fase successiva se l'operazione è illegale.
        return left;
    }

    /// <summary>
    /// Determina il tipo di dato risultante di un'operazione unaria (es. -x, NOT x), 
    /// implementando il supporto per l'overloading degli operatori (TODO #19).
    /// </summary>
    /// <remarks>
    /// La logica di risoluzione segue questa priorità:
    /// 1. Cerca un metodo OPERATOR corrispondente nella gerarchia della classe dell'operando.
    /// 2. Cerca un metodo OPERATOR nella definizione dell'UDT dell'operando.
    /// 3. Se non vengono trovati overload, restituisce il tipo dell'operando stesso (logica primitiva).
    /// </remarks>
    /// <param name="operand">Il tipo di dato dell'operando a cui è applicato l'operatore.</param>
    /// <param name="op">Il token dell'operatore unario (es. '-', 'NOT').</param>
    /// <param name="context">Il contesto di riscrittura corrente per la validazione dei tipi.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable e alle definizioni.</param>
    /// <returns>Il <see cref="DataType"/> risultante dall'operazione.</returns>
    private static DataType GetPromotedUnaryType(DataType operand, Token op, AstRewriteContext context,
        BasicProgram program)
    {
        if (operand is ClassType ct)
        {
            var current = ct.Definition;
            while (current != null)
            {
                var opMatch = current.Members.OfType<OperatorMember>().Select(m => m.Operator).FirstOrDefault(o =>
                    o.OperatorToken.Kind == op.Kind && o.Parameters.Length == 1 &&
                    CanCoerce(operand, o.Parameters[0].DataType, context, program));
                if (opMatch != null) return opMatch.ReturnType;
                current = current.ResolvedBaseClass;
            }
        }

        if (operand is UserDefinedType { SymbolId: >= 0 } udt)
        {
            var udtDef = program.Symbols.GetSymbol(udt.SymbolId).DeclarationNode as UserDefinedType;
            var opMatch = udtDef?.Operators.FirstOrDefault(o =>
                o.OperatorToken.Kind == op.Kind && o.Parameters.Length == 1 &&
                CanCoerce(operand, o.Parameters[0].DataType, context, program));
            if (opMatch != null) return opMatch.ReturnType;
        }

        return operand;
    }

    /// <summary>
    /// Determina se un dato <see cref="DataType"/> rappresenta un valore numerico (intero, virgola mobile o valuta).
    /// Utile per la logica di inferenza dei tipi, in particolare per determinare se due tipi diversi
    /// possono essere promossi a un tipo comune (es. nel caso dell'operatore ternario).
    /// </summary>
    /// <param name="type">Il tipo di dato da controllare.</param>
    /// <returns>True se il tipo è numerico; altrimenti, False.</returns>
    private static bool IsNumericType(DataType type)
    {
        return type is ByteType or SByteType or IntegerType or UIntegerType or
            LongType or ULongType or QuadType or UQuadType or
            SingleType or DoubleType or CurrencyType;
    }

    /// <summary>
    /// Inferisce il tipo di dato di un'espressione analizzandola ricorsivamente.
    /// Questo metodo è il cuore del sistema di inferenza dei tipi del Linker. Attraversa l'AST
    /// di un'espressione per determinare il suo tipo di ritorno, gestendo letterali, variabili,
    /// operazioni, chiamate a funzione e costrutti complessi come le comprensioni di lista.
    /// Accetta una mappa opzionale per i parametri delle lambda per gestire correttamente gli scope locali.
    /// </summary>
    /// <param name="expr">Il nodo dell'espressione (Expr) da analizzare.</param>
    /// <param name="context">Il contesto di riscrittura che contiene informazioni sullo scope corrente e sugli alias.</param>
    /// <param name="program">L'istanza del programma che contiene la SymbolTable per la risoluzione dei simboli.</param>
    /// <returns>Un'istanza di <see cref="DataType"/> che rappresenta il tipo inferito dell'espressione.
    /// Restituisce <see cref="InferredType"/> se il tipo non può essere determinato in questa passata.</returns>
    public static DataType LinkerInferExpressionType(Expr expr, AstRewriteContext context, BasicProgram program)
    {
        const bool log = false; // Impostare a 'true' per un logging verboso dell'inferenza.
        Log($"[INFER] Inizio inferenza per nodo di tipo: {expr.GetType().Name} ({expr})", log);

        switch (expr)
        {
            // --- Logica per riferimenti a funzione ---
            case FunctionReferenceExpr fre:
            {
                if (fre.SymbolId < 0) return new InferredType(); // Non ancora risolto

                var symbol = program.Symbols.GetSymbol(fre.SymbolId);
                if (symbol.DeclarationNode is not ProcedureDefinition procDef)
                {
                    // Questo indicherebbe un errore grave del compilatore
                    return new InferredType();
                }

                var paramTypes = procDef.Parameters.Select(p => p.DataType).ToImmutableArray();
                var returnType = procDef switch
                {
                    FunctionDefinition fds => fds.ReturnType,
                    NativeProcedureDefinition npd => npd.ReturnType,
                    ExternalProcedureDefinition epd => epd.ReturnType ?? new VoidType(),
                    _ => new VoidType()
                };
                return new FunctionPointerType(paramTypes, returnType);
            }
            // --- Letterali ---
            case NumberExpr ne:
            {
                // **LOGICA DI INFERENZA CON TARGET TYPING E DEFAULT A DOUBLE**
                
                // PRIORITÀ 1: Se c'è un tipo atteso, tentiamo di conformarci a quello.
                if (context.ExpectedType != null)
                {
                    // Se il tipo atteso è numerico, lo adottiamo. La validazione del range 
                    // e della coercizione avverrà a un livello superiore.
                    if (IsNumericType(context.ExpectedType))
                    {
                        return context.ExpectedType;
                    }
                    // Altrimenti, se il tipo atteso non è numerico, ignoriamolo per ora e usiamo il fallback.
                }

                // PRIORITÀ 2 (FALLBACK): Se non c'è un tipo atteso, qualsiasi letterale numerico
                // viene trattato come DOUBLE, il tipo numerico di default del linguaggio.
                // Questo risolve l'inferenza errata (es. BYTE) per i letterali interi in contesti generici.
                return new DoubleType();
            }
            // Gestione dell'espressione di tipo (es. il lato destro di 'IS STRING').
            // Restituisce il DataType contenuto nel nodo. Questo permette al Linker di confrontare
            // staticamente i tipi se possibile o di validare la coerenza dell'operazione.
            case TypeExpr te:
                return te.Type;

            case StringExpr:
                return new StringType();
            case InterpolatedStringExpr:
                return new StringType();
            case CheckedStringExpr:
                return new BitType();
            case BooleanExpr:
                return new BitType();

            // --- Letterali di Collezione ---
            case ArrayLiteralExpr ale:
            {
                // PRIORITÀ 1: TARGET TYPING. Se il contesto si aspetta un tipo array, lo adottiamo.
                if (context.ExpectedType is ArrayType expectedArrayType)
                {
                    return expectedArrayType;
                }

                if (!ale.Elements.Any())
                {
                    return new ArrayType(new InferredType());
                }

                // PRIORITÀ 2: INFERENZA DAL TIPO PIÙ AMPIO DEGLI ELEMENTI
                var elementContext = context with { ExpectedType = null }; // Rimuove il contesto per l'analisi dei figli

                var elementTypes = ale.Elements
                    .Select(e => LinkerInferExpressionType(e, elementContext, program))
                    .ToList();

                // Letterale ANNIDATO ({{1,2},{3,4},{5,6}}): ogni elemento è già un ArrayType
                // (l'array interno). GetPromotedType è pensata per operandi SCALARI di un vero
                // operatore binario — non sa comporre due ArrayType e li lascia invariati
                // (torna sempre 'left'), quindi il fold sotto restava bloccato a InferredType
                // e il fallback "ancora Inferred -> Double" appiattiva l'array ESTERNO a
                // DoubleType invece di ArrayType(Double): l'array esterno veniva poi costruito
                // dal codegen come array-di-double (newarr Double) invece di array-di-array
                // (newarr Double[]), corrompendo ogni riferimento interno memorizzato con
                // stelem.r8 (i bit del puntatore reinterpretati come double) al posto di
                // stelem.ref — bug reale: ArraysFluent.bas, AFLAT su array jagged dava valori
                // spazzatura (denormal doubles) su IL. Fold ricorsivo dedicato: se TUTTI gli
                // elementi sono ArrayType, promuove i tipi ELEMENTO interni (stessa
                // GetPromotedType, che lì funziona già bene su tipi scalari) e ri-avvolge.
                if (elementTypes.Count > 0 && elementTypes.All(t => t is ArrayType))
                {
                    return new ArrayType(PromoteNestedArrayElementType(elementTypes, ale.Position, context, program));
                }

                // Trova il tipo comune più "ampio" usando una gerarchia di promozione definita.
                DataType commonType = new InferredType();
                foreach (var type in elementTypes)
                {
                    // La funzione GetPromotedType contiene la logica per determinare il tipo più "ampio"
                    // tra 'commonType' e 'type'.
                    commonType = GetPromotedType(commonType, type,
                        new Token(TokenKind.Plus, "+", null, ale.Position), // Operatore fittizio
                        context, program);
                }

                // Se dopo l'analisi il tipo comune è ancora Inferred (es. array di NIL), usiamo un default sicuro.
                if (commonType is InferredType)
                {
                    commonType = new DoubleType();
                }

                var finalArrayType = new ArrayType(commonType);
                return finalArrayType;
            }

            case ArraySliceExpr ase:
                // Il tipo di uno slice è lo stesso tipo della collezione originale.
                return LinkerInferExpressionType(ase.Base, context, program);

            case ListLiteralExpr lle:
                if (!lle.Elements.Any())
                    return context.ExpectedType is ListType expectedList
                        ? expectedList
                        : new ListType(new InferredType());
                var firstListElementType = LinkerInferExpressionType(lle.Elements[0], context, program);
                return new ListType(firstListElementType);

            case DictionaryLiteralExpr dle:
                if (!dle.Entries.Any())
                    return context.ExpectedType is DictType expectedDict
                        ? expectedDict
                        : new DictType(new InferredType(), new InferredType());
                var firstKeyType = LinkerInferExpressionType(dle.Entries[0].Key, context, program);
                var firstValueType = LinkerInferExpressionType(dle.Entries[0].Value, context, program);
                return new DictType(firstKeyType, firstValueType);

            // --- Variabili e Simboli ---
            case VariableExpr ve:
            {
                Log(
                    $"[INFER-VAR] Analizzo VariableExpr ('{ve}'). ExpectedType: {context.ExpectedType?.ToString() ?? "N/A"}.",
                    log);

                if (ve.NameCleaned.Kind is TokenKind.Self or TokenKind.Base ||
                    ve.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase) ||
                    ve.NameCleaned.Lexeme.Equals("BASE", StringComparison.OrdinalIgnoreCase))
                {
                    if (context.CurrentClass != null)
                    {
                        return new ClassType(context.CurrentClass);
                    }

                    return new InferredType();
                }

                // Priorità allo scope della lambda, se presente
                if (context.LambdaScope != null &&
                    context.LambdaScope.TryGetValue(ve.NameCleaned.Lexeme, out var lambdaVarType))
                {
                    Log($"[INFER-VAR]   -> Risolto come parametro Lambda con tipo: {lambdaVarType}.", log);
                    return lambdaVarType;
                }

                // Altrimenti, usa il SymbolId per l'accesso O(1)
                // Se l'ID è valido, controlliamo se punta a una PROCEDURA.
                if (ve.SymbolId >= 0)
                {
                    var symbol = program.Symbols.GetSymbol(ve.SymbolId);
                    Log(
                        $"[INFER-VAR]   -> Simbolo '{symbol.OriginalName}' (ID: {symbol.Id}) trovato in SymbolTable. DataType attuale: {symbol.DataType}.",
                        log);

                    // Se il simbolo è una procedura, restituiamo un FunctionPointerType invece del tipo di ritorno.
                    if (symbol.DeclarationNode is ProcedureDefinition proc)
                    {
                        var paramTypes = proc.Parameters.Select(p => p.DataType).ToImmutableArray();
                        var returnType = proc switch
                        {
                            FunctionDefinition fdi => fdi.ReturnType,
                            NativeProcedureDefinition npd => npd.ReturnType,
                            ExternalProcedureDefinition epd => epd.ReturnType ?? new VoidType(),
                            _ => new VoidType()
                        };
                        Log(
                            $"[INFER-VAR]   -> Simbolo è una procedura. Restituisco FunctionPointerType: {new FunctionPointerType(paramTypes, returnType)}.",
                            log);
                        return new FunctionPointerType(paramTypes, returnType);
                    }

                    // Se è una variabile normale, procediamo con la logica standard e Target Typing.
                    var currentVarType = symbol.DataType;

                    // FIX: Per tipi strutturati (Classi/UDT), usiamo il DeclarationNode come fonte di verità
                    // aggiornata. symbol.DataType potrebbe contenere una copia vecchia ("stantia") della definizione
                    // creata al momento della registrazione (Fase 2), priva dei collegamenti di ereditarietà (BaseClassSymbolId)
                    // risolti successivamente dal Linker.
                    // CORREZIONE: Se la variabile ha già un tipo UserDefinedType (es. specializzazione generica
                    // con SymbolId=-1), NON sovrascriverlo con il DeclarationNode generico.
                    if (symbol.DeclarationNode is ClassDefinition classDef)
                    {
                        currentVarType = new ClassType(classDef);
                    }
                    else if (symbol.DeclarationNode is UserDefinedType udtDef
                        && currentVarType is not UserDefinedType { SymbolId: -1 })
                    {
                        currentVarType = udtDef;
                    }

                    var resolvedCurrentVarType = ResolveDataType(currentVarType, program, context.ModulePath);
                    Log(
                        $"[INFER-VAR]   -> Tipo attuale della variabile (dopo risoluzione): {resolvedCurrentVarType}. ExpectedType dal contesto: {context.ExpectedType?.ToString() ?? "N/A"}.",
                        log);


                    // --- TARGET TYPING PER VARIABILI (CORREZIONE CRUCIALE) ---
                    // Se il tipo attuale della variabile è INFERRED (mai dichiarato esplicitamente
                    // con AS <tipo>), e c'è un ExpectedType che è un tipo numerico intero,
                    // e la variabile non è una costante o un parametro BYREF con tipo fisso.
                    // NOTA: il ramo "|| IsNumericType(resolvedCurrentVarType)" è stato RIMOSSO —
                    // permetteva di riscrivere il tipo di una variabile GIÀ dichiarata esplicitamente
                    // (es. DIM i AS INTEGER) con un tipo più STRETTO dedotto dal contesto d'uso (es.
                    // l'elemento BYTE di un array indicizzato/assegnato da i in key(i)=i),
                    // silenziosamente NARROWING oltre l'intenzione dell'utente — mai solo un WIDENING
                    // come il commento originale suggeriva ("un tipo numerico più piccolo"), la
                    // condizione non verificava affatto la direzione. Un tipo GIÀ concreto (non
                    // InferredType) è per definizione una dichiarazione esplicita dell'utente: non va
                    // mai sovrascritto da qui — la sua "fonte di verità" resta la clausola AS nella
                    // dichiarazione (vedi RewriteVariableDeclarationWithInference, "Autorità 1").
                    if (context.ExpectedType is not null && // Aggiunto controllo di nullabilità qui.
                        context.ExpectedType is not InferredType &&
                        IsIntegerBasedType(context
                            .ExpectedType) && // Passa ExpectedType, che ora è garantito non nullo.
                        // BYTE/SBYTE sono esclusi: restringere una variabile implicita (tipicamente il contatore di un
                        // FOR, `B(I) = I` con B array di BYTE) a 8 bit la fa andare in overflow oltre 255 e, nel backend
                        // IL, la rende un local uint8 a cui il ciclo scrive un float64 (IL non valido / indici errati).
                        context.ExpectedType is not (ByteType or SByteType) &&
                        resolvedCurrentVarType is InferredType &&
                        !symbol.Has(SymbolFlags.IsConst) &&
                        !symbol.Has(SymbolFlags.IsParameter | SymbolFlags.IsByRef))
                    {
                        // Aggiorniamo il tipo della variabile nella SymbolTable al tipo atteso.
                        program.Symbols.UpdateSymbolType(symbol.Id, context.ExpectedType);
                        Log(
                            $"[INFER-VAR]   -> TARGET TYPING: Aggiornato tipo di '{symbol.OriginalName}' da {resolvedCurrentVarType} a {context.ExpectedType}.",
                            log);
                        context.OnTypeInferred?.Invoke(); // Segnala che un'inferenza è avvenuta.
                        return context.ExpectedType; // Restituisce il nuovo tipo.
                    }

                    // Logica di refresh per UDT stantii
                    if (currentVarType is UserDefinedType udtRaw &&
                        resolvedCurrentVarType is UserDefinedType udtResolved &&
                        udtRaw != udtResolved)
                    {
                        Log($"[INFER-VAR] Refreshed UDT type for var '{ve.NameCleaned.Lexeme}'. " +
                            $"Methods: {udtRaw.Methods.Length} -> {udtResolved.Methods.Length}", false);
                        context.OnTypeInferred?.Invoke(); // Segnala che un'inferenza è avvenuta.
                    }

                    Log($"[INFER-VAR]   -> Restituisco tipo risolto: {resolvedCurrentVarType}.", log);
                    return resolvedCurrentVarType;
                }

                // Fallback: se l'ID non è valido (es. late-binding DBF), restituisce InferredType.
                Log($"[INFER-VAR]   -> VariableExpr ID è negativo o invalido. Restituisco InferredType.", log);
                return new InferredType();
            }

            // --- Espressioni Composte ---
            case TernaryExpr te:
            {
                var trueType = LinkerInferExpressionType(te.TruePart, context, program);
                var falseType = LinkerInferExpressionType(te.FalsePart, context, program);

                // Se uno dei due rami non è ancora stato inferito, non possiamo decidere il tipo finale.
                if (trueType is InferredType || falseType is InferredType) return new InferredType();
                // Se i tipi sono identici, il risultato è quel tipo.
                if (AreSameType(trueType, falseType, context, program)) return trueType;
                // Se uno dei due rami è una stringa, il risultato viene promosso a Stringa.
                if (trueType is StringType || falseType is StringType) return new StringType();
                // Se entrambi sono numerici, promuovi al tipo più ampio.
                if (IsNumericType(trueType) && IsNumericType(falseType))
                    return new DoubleType(); // Semplice promozione a Double
                // Controlli di coercizione per altri tipi.
                if (CanCoerce(falseType, trueType, context, program)) return trueType;
                if (CanCoerce(trueType, falseType, context, program)) return falseType;
                // Fallback: restituisce il tipo del primo ramo se non risolvibile.
                return trueType;
            }

            case BinaryExpr be:
                var leftType = LinkerInferExpressionType(be.Left, context, program);
                var rightType = LinkerInferExpressionType(be.Right, context, program);
                return GetPromotedType(leftType, rightType, be.Op, context, program);

            case UnaryExpr ue:
            {
                var operandType = LinkerInferExpressionType(ue.Right, context, program);
                return GetPromotedUnaryType(operandType, ue.Op, context, program);
            }

            case GroupingExpr ge:
                return LinkerInferExpressionType(ge.Expression, context, program);

            case SafeCastExpr sce:
                return new NullableType(sce.TargetType);

            // --- Chiamate a Funzione e Metodi ---
            case BuiltinFunctionCallExpr bce:
            {
                if (bce.Name.Kind == TokenKind.Iif && bce.Args.Length == 3)
                {
                    var trueType = LinkerInferExpressionType(bce.Args[1], context, program);
                    var falseType = LinkerInferExpressionType(bce.Args[2], context, program);

                    if (trueType is InferredType || falseType is InferredType) return new InferredType();
                    if (AreSameType(trueType, falseType, context, program)) return trueType;
                    if (trueType is StringType || falseType is StringType) return new StringType();
                    if (IsNumericType(trueType) && IsNumericType(falseType)) return new DoubleType();
                    if (CanCoerce(falseType, trueType, context, program)) return trueType;
                    if (CanCoerce(trueType, falseType, context, program)) return falseType;
                    return trueType;
                }

                // JSON.KEYS(<json>): le chiavi sono STRINGHE. Senza, `FOR EACH k IN JSON.KEYS(g)` tipizzava k come InferredType (DOUBLE in IL):
                // `g[k]` dentro il ciclo prendeva il percorso a indice NUMERICO (FormatException 'aa') e HandleApiPackages del registry su IL
                // rispondeva vuoto.
                if (bce.Name.Kind == TokenKind.Keys && bce.Args.Length == 1
                    && LinkerInferExpressionType(bce.Args[0], context, program) is Basic.JsonType or AnyType)
                    return new ArrayType(new StringType());

                var declaredReturnType = bce.ReturnType;
                if (declaredReturnType is InferredType || declaredReturnType is ArrayType { ElementType: InferredType })
                {
                    return InferGenericPolymorphicReturnType(bce, context, program);
                }

                return declaredReturnType;
            }

            /* CHIAMATA INDIRETTA di un valore FUNZIONE (`t1(…)]`, `EVAL(t1, …)` riscritto):
             * il risultato è il ReturnType della firma del delegate. Senza, cadeva nel ramo
             * generico (ResolvedDefinition null → InferredType) e `DIM t1 = EVAL(block,args)`
             * perdeva il tipo: il successivo `EVAL(t1, …)` dava "target dinamico non
             * supportato". */
            case FunctionExpr feInd when feInd.IsIndirect
                                       && feInd.ReturnType is not null
                                       && feInd.ReturnType is not InferredType:
                return feInd.ReturnType;

            case FunctionExpr fe:
                if (fe.ResolvedDefinition is FunctionDefinition fd)
                {
                    // Se il tipo di ritorno non è ancora inferito, proviamo a farlo ora.
                    return fd.ReturnType is InferredType ? InferUdfReturnType(fd, context, program) : fd.ReturnType;
                }

                return new InferredType();

            // EVAL(target [, args...]): con un code block di tipo noto il risultato è il suo
            // ReturnType; per una stringa (macro-eval) il valore dipende dal testo compilato a
            // runtime → InferredType.
            case EvalExpr ee:
            {
                var evalTargetType = LinkerInferExpressionType(ee.Target, context with { ExpectedType = null }, program);
                return evalTargetType is FunctionPointerType evalFpt ? evalFpt.ReturnType : new InferredType();
            }

            case MethodCallExpr mce:
                var instanceTypeForMethod = LinkerInferExpressionType(mce.Instance, context, program);
                // REGEX nativo (r.IsMatch/Match/Matches/Replace/Split): il tipo del return
                // dal metodo CLR della RegexWrapper (il linker non risolve questi metodi —
                // prima restavano InferredType → il DIM del risultato era InferredType).
                if (instanceTypeForMethod is RegexType)
                {
                    var method = typeof(Basic.RegexNative.RegexWrapper).GetMethods().FirstOrDefault(m =>
                        m.Name.Equals(mce.MethodName.Lexeme, StringComparison.OrdinalIgnoreCase)
                        && m.GetParameters().Length == mce.Args.Length);
                    if (method != null)
                    {
                        if (method.ReturnType == typeof(string)) return new StringType();
                        if (method.ReturnType == typeof(string[])) return new ArrayType(new StringType());
                        if (method.ReturnType == typeof(double)) return new DoubleType();
                        if (method.ReturnType == typeof(List<Basic.RegexNative.RegexMatchValue>)) return new ListType(new RegexMatchType());
                        return new RegexMatchType();
                    }
                }
                var collectionMethodType =
                    InferCollectionMethodReturnType(instanceTypeForMethod, mce.MethodName.Lexeme);
                if (collectionMethodType != null)
                {
                    return collectionMethodType;
                }

                if (mce.ResolvedDefinition is FunctionDefinition fdMethod)
                {
                    return fdMethod.ReturnType;
                }

                return new InferredType();

            // --- Accesso a Membri e Indici ---
            case FieldAccessExpr fae:
                var structureType = LinkerInferExpressionType(fae.Structure, context, program);

                // A. Controllo nel registro degli schemi built-in (priorità ai tipi nativi)
                if (TypeSchemaRegistry.TryGetValue(structureType.GetType(), out var schema))
                {
                if (schema!.Fields.TryGetValue(fae.Field.Lexeme, out var fieldType))
                {
                    return fieldType;
                }
                }

                // A2. Campo di un oggetto JSON/ANY (`c.campo`): valore dinamico -> ANY (come l'elemento `g["k"]`). Prima restava InferredType e una
                // variabile implicita (`m = c.chiave`) veniva tipizzata DOUBLE dai backend tipizzati (IL): CoerceDynamicNumber + confronto con una
                // stringa = IL non valido (InvalidProgramException) e LEN(m$) = 1.
                if (structureType is Basic.JsonType or AnyType) return new AnyType();

                // B. Controllo per tipi definiti dall'utente
                switch (structureType)
                {
                    case UserDefinedType udt:
                    {
                        // Cerca prima tra i campi
                        var field = udt.Fields.FirstOrDefault(f =>
                            f.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
                        if (field != null)
                        {
                            // Se il campo ha dimensioni definite, è un array. Il RANGO è il
                            // numero di dimensioni dichiarate (`DIM m(N,M)` → 2): prima si
                            // assumeva sempre rank 1, quindi `DIM x = t.m` / `x(i,j)` su un
                            // campo matrice venivano trattati come array monodimensionali
                            // (IL: "numero di indici (2) diverso dal rank dell'array (1)").
                            if (field.Dimensions.Any())
                            {
                                return new ArrayType(field.DataType, Math.Max(1, field.Dimensions.Length));
                            }

                            // Altrimenti, è un campo scalare.
                            return field.DataType;
                        }

                        // Se non è un campo, cerca tra le proprietà
                        var prop = udt.Properties.FirstOrDefault(p =>
                            p.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
                        if (prop != null)
                        {
                            return prop.Type;
                        }

                        return new InferredType(); // Non trovato
                    }
                    case ClassType ct:
                    {
                        var (member, _) = FindFieldInHierarchy(ct.Definition, fae.Field.Lexeme);
                        if (member is FieldMember fm)
                        {
                            // A differenza di UserDefinedType sopra, Declaration.DataType di un
                            // campo di CLASSE è già il tipo pienamente risolto (RegisterFieldMember/
                            // SymbolTable lo aggiornano a un vero ArrayType per un campo con
                            // Dimensions non vuote) — ri-wrapparlo qui produceva un doppio
                            // ArrayType ("ARRAY OF ARRAY OF DOUBLE" invece di "ARRAY OF DOUBLE"),
                            // scoperto testando i campi ARRAY di classe.
                            return fm.Declaration.DataType ?? new InferredType();
                        }

                        if (member is PropertyMember pm) return pm.Property.Type;
                        break;
                    }
                }

                return new InferredType();

            case PropertyAccessExpr pae:
                var instanceTypeForProp = LinkerInferExpressionType(pae.Instance, context, program);
                switch (instanceTypeForProp)
                {
                    case UserDefinedType udtProp:
                        return udtProp.Properties.FirstOrDefault(p =>
                                       p.Name.Lexeme.Equals(pae.PropertyName.Lexeme,
                                           StringComparison.OrdinalIgnoreCase))
                                   ?.Type ??
                               new InferredType();
                    case ClassType classType:
                    {
                        var (member, _) = FindFieldInHierarchy(classType.Definition, pae.PropertyName.Lexeme);
                        if (member is PropertyMember pm) return pm.Property.Type;
                        break;
                    }
                }

                return new InferredType();

            case ArrayAccessExpr aae:
            {
                // Se l'ID non è ancora stato risolto, non possiamo inferire nulla.
                if (aae.SymbolId < 0) return new InferredType();

                var symbol = program.Symbols.GetSymbol(aae.SymbolId);

                // Caso 1: È una chiamata a funzione (es. `miaFunc(x)`) mascherata da ArrayAccessExpr.
                if (symbol.DataType is FunctionPointerType fpt)
                {
                    return fpt.ReturnType;
                }

                // Caso 2: È un array.
                if (symbol.DataType is ArrayType arrayType)
                {
                    // Se la lista di indici è vuota, significa che stiamo passando l'intero
                    // array come riferimento (es. `PRINT myArray()`). Il tipo è l'array stesso.
                    if (aae.Indices.IsEmpty)
                    {
                        return arrayType;
                    }

                    // Se ci sono indici, stiamo accedendo a un singolo elemento.
                    // Il tipo è il tipo dell'elemento.
                    var elementType = arrayType.ElementType;
                    if (aae.IsSafe && elementType is not InferredType)
                        return new NullableType(elementType);
                    return elementType;
                }

                // Caso 3: accesso a indice/chiave su un oggetto JSON/ANY (`g["k"]`, `g[0]`): il valore e' un
                // elemento dinamico (dizionario, lista, scalare) -> ANY. Senza questo restava InferredType e una
                // variabile implicita (`claim = g["k"]`) veniva tipizzata DOUBLE dai backend tipizzati (IL):
                // il dizionario annidato veniva convertito a double (InvalidCastException) e `claim.campo` dava 0.
                if (symbol.DataType is Basic.JsonType or AnyType && !aae.Indices.IsEmpty)
                    return new AnyType();

                // Fallback se il simbolo non è né un puntatore a funzione né un array.
                return new InferredType();
            }

            case IndexedAccessExpr iae:
            {
                // `g["k"]` / `g[0]` su JSON/ANY: elemento dinamico -> ANY (vedi il caso ArrayAccessExpr sopra).
                if (LinkerInferExpressionType(iae.Base, context, program) is Basic.JsonType or AnyType)
                    return new AnyType();
                var elementType = InferElementType(iae.Base, context, program);
                if (iae.IsSafe && elementType is not null && elementType is not InferredType)
                    return new NullableType(elementType);
                return elementType ?? new InferredType();
            }

            // --- Altre Espressioni Complesse ---
            case LambdaExpr le:
                return new FunctionPointerType([..le.Parameters.Select(p => p.DataType)], le.ReturnType);

            case NewExpr ne:
                return ne.ParsedType;

            case ListComprehensionExpr lce:
            {
                var cumulativeScope = context.LambdaScope != null
                    ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

                var currentContext = context;

                foreach (var clause in lce.Clauses)
                {
                    // Inferisce il tipo dell'elemento della sorgente corrente
                    var elementType = InferElementType(clause.SourceExpression, currentContext, program) ??
                                      new InferredType();

                    // Aggiunge il nuovo iteratore allo scope
                    cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;

                    // Aggiorna il contesto per la prossima iterazione
                    currentContext = context with { LambdaScope = cumulativeScope };
                }

                var outputType = LinkerInferExpressionType(lce.OutputExpression, currentContext, program);

                return new ArrayType(outputType);
            }

            // { k : v FOR x IN src [IF c] } → DICT OF K,V ; { x FOR x IN src } → HASHSET OF T. Prima solo ListComprehensionExpr era
            // inferita: `DIM d = { k : v FOR ... }` (senza tipo) restava INFERRED e i backend C dichiaravano la variabile col tipo sbagliato.
            case DictComprehensionExpr dce:
            {
                var scope = context.LambdaScope != null
                    ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
                var cc = context;
                foreach (var clause in dce.Clauses)
                {
                    scope[clause.IteratorVariable.Lexeme] =
                        InferElementType(clause.SourceExpression, cc, program) ?? new InferredType();
                    cc = context with { LambdaScope = scope };
                }
                return new DictType(LinkerInferExpressionType(dce.KeyExpression, cc, program),
                                    LinkerInferExpressionType(dce.ValueExpression, cc, program));
            }

            case SetComprehensionExpr sce:
            {
                var scope = context.LambdaScope != null
                    ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
                var cc = context;
                foreach (var clause in sce.Clauses)
                {
                    scope[clause.IteratorVariable.Lexeme] =
                        InferElementType(clause.SourceExpression, cc, program) ?? new InferredType();
                    cc = context with { LambdaScope = scope };
                }
                return new HashSetType(LinkerInferExpressionType(sce.ElementExpression, cc, program));
            }

            case GeneratorExpr ge:
            {
                var cumulativeScope = context.LambdaScope != null
                    ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

                var currentContext = context;

                foreach (var clause in ge.Clauses)
                {
                    var elementType = InferElementType(clause.SourceExpression, currentContext, program) ??
                                      new InferredType();
                    cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;
                    currentContext = context with { LambdaScope = cumulativeScope };
                }

                var outputType = LinkerInferExpressionType(ge.OutputExpression, currentContext, program);
                return new GeneratorType(outputType);
            }

            case NullConditionalAccessExpr nca:
            {
                DataType? innerType = null;
                if (nca.Kind == NullConditionalKind.Member && nca.MemberToken is { } mt)
                {
                    var structType = LinkerInferExpressionType(nca.Expression, context, program);
                    if (structType is UserDefinedType udt)
                    {
                        innerType = udt.Fields.FirstOrDefault(f =>
                            f.Name.Lexeme.Equals(mt.Lexeme, StringComparison.OrdinalIgnoreCase))?.DataType;
                    }
                    else if (structType is ClassType ct)
                    {
                        var (member, _) = FindFieldInHierarchy(ct.Definition, mt.Lexeme);
                        innerType = member switch
                        {
                            FieldMember fm => fm.Declaration.DataType,
                            PropertyMember pm => pm.Property.Type,
                            _ => null
                        };
                    }
                    else if (TypeSchemaRegistry.TryGetValue(structType.GetType(), out var fieldSchema)
                             && fieldSchema!.Fields.TryGetValue(mt.Lexeme, out var ft))
                    {
                        innerType = ft;
                    }
                }
                else if (nca.Kind == NullConditionalKind.Index)
                {
                    innerType = InferElementType(nca.Expression, context, program);
                }
                if (innerType is null || innerType is InferredType) return new InferredType();
                return new NullableType(innerType);
            }

            // --- Tuple literal: inferisce TupleType dai tipi degli elementi ---
            case TupleLiteralExpr tle:
            {
                var fields = tle.Elements.Select(e => new TupleField(e.Name?.Lexeme, LinkerInferExpressionType(e.Value, context, program))).ToImmutableArray();
                return new TupleType(fields);
            }

            default:
                Log(
                    $"[INFER] -> ATTENZIONE: Nodo non gestito '{expr.GetType().Name}'. Restituisco InferredType.", log);
                return new InferredType();
        }
    }

    /// <summary>
    /// Metodo helper che contiene la conoscenza semantica dei tipi di ritorno per tutti i metodi
    /// di tutte le collezioni native. Utilizza il tipo parametrico dell'istanza per l'inferenza.
    /// </summary>
    private static DataType? InferCollectionMethodReturnType(DataType instanceType, string methodName)
    {
        var upperMethodName = methodName.ToUpperInvariant();
        // 1. Logica per le collezioni generiche (Priorità Alta per propagare i tipi parametrici T)
        // Interrogando prima lo switch, garantiamo che lt.ElementType o hst.ElementType 
        // vengano usati per definire il tipo di ritorno di metodi come ToArray() o ToList().
        switch (instanceType)
        {
            case JsonType:
                return upperMethodName switch
                {
                    "KEYS" => new ArrayType(new StringType()),
                    "COUNT" => new LongType(),
                    "CONTAINSKEY" => new BitType(),
                    _ => null
                };
            /* RICEVENTE DINAMICA (`ANY`/InferredType: il tipo dichiarato si è perso — es.
             * `DIM l AS LIST<STRING> = [...]` che il linker tipizza InferredType): i metodi di
             * collezione si accettano lo stesso, con i tipi di ritorno del caso statico.
             * La leg C li accetta sempre (dispatch per nome a runtime) e l'IL emette per nome
             * (`IlProgramCompiler.Members.Calls` case "REMOVE"/"ADD"/...). */
            case AnyType:
                return upperMethodName switch
                {
                    /* `COUNT`/`INDEXOF` NON entrano qui: per la ricevente dinamica hanno già un
                     * percorso (2b-prime-3 → SYSTEM.COUNT/INDEXOF) e intercettarli qui faceva
                     * fallire l'overload ("argomenti non validi per alcun overload") su
                     * `big.Count()` con `DIM big = []`. */
                    "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" or "TRYGET" or "TRYADD"
                        => new BitType(),
                    "REMOVEAT" or "ADD" or "INSERT" or "CLEAR" or "SORT" or "REVERSE" or "ADDRANGE"
                        => new VoidType(),
                    "COUNT" or "INDEXOF" => new LongType(),
                    "TOLIST" or "TOARRAY" or "TOJSON" or "KEYS" or "VALUES" or "GET" or "SET"
                        or "SETKEYVALUE" or "TRYGETVALUE" or "GETRANGE" or "FIRST" or "LAST"
                        or "FRONT" or "BACK" or "DEQUEUE" or "PEEK" or "POP" or "MAX" or "MIN" or "SUM"
                        => new InferredType(),
                    _ => null
                };
            case InferredType:
                return upperMethodName switch
                {
                    "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" or "TRYGET" or "TRYADD"
                        => new BitType(),
                    "REMOVEAT" or "ADD" or "INSERT" or "CLEAR" or "SORT" or "REVERSE" or "ADDRANGE"
                        => new VoidType(),
                    /* COUNT/INDEXOF restano al percorso storico (fallback): intercettarli qui
                     * li mandava al builtin COUNT(string,string) e falliva l'overload. */
                    "TOLIST" or "TOARRAY" or "TOJSON" or "KEYS" or "VALUES" or "GET" or "SET"
                        or "SETKEYVALUE" or "TRYGETVALUE" or "GETRANGE" or "FIRST" or "LAST"
                        or "FRONT" or "BACK" or "DEQUEUE" or "PEEK" or "POP" or "MAX" or "MIN" or "SUM"
                        => new InferredType(),
                    _ => null
                };
            case ArrayType at:
                return upperMethodName switch
                {
                    "FILTER" or "MAP" or "SORT" or "REVERSE" or "UNIQUE" or "FLAT" or "SHUFFLE" => new ArrayType(at.ElementType),
                    "REDUCE" => new InferredType(),
                    "COUNT" => new LongType(),
                    "JOIN" => new StringType(),
                    _ => null
                };
            case ListType lt:
                return upperMethodName switch
                {
                    "TOLIST" => lt,
                    "TOARRAY" => new ArrayType(lt.ElementType),
                    "TOJSON" => new JsonType(),
                    "JOIN" => new StringType(),
                    "MAX" or "MIN" or "SUM" => new DoubleType(),
                    "INDEXOF" or "COUNT" => new LongType(),
                    "CONTAINS" or "REMOVE" or "ISEMPTY" => new BitType(),
                    "FIRST" or "LAST" or "FRONT" or "BACK" => lt.ElementType,
                    // F1: ADDUNIQUE/ADDAFTER/ADDBEFORE — comandi (InferredType: il call runtime
                    // restituisce object, il consumatore poppa — parity SET/SETKEYVALUE).
                    "ADDUNIQUE" or "ADDAFTER" or "ADDBEFORE" => new InferredType(),
                    "GETRANGE" => lt,
                    "ADD" or "INSERT" or "REMOVEAT" or "CLEAR" or "SORT" or "REVERSE" or "ADDRANGE" => new VoidType(),
                    _ => null
                };
            case DictType dt:
                return upperMethodName switch
                {
                    "ADD" => new VoidType(),
                    // SET/SETKEYVALUE: InferredType (non VoidType) — il ramo IL emette un
                    // call che RESTITUISCE object: il consumatore statement lo poppa
                    // (EmitExpressionStatement) e un uso in espressione (PRINT) lo boxa
                    // (EmitDynamicValueBox). VoidType → niente pop del consumatore → IL invalido.
                    "SET" or "SETKEYVALUE" => new InferredType(),
                    "GET" => dt.ValueType,
                    "TRYGETVALUE" => new InferredType(),
                    "TODICT" => dt,
                    "TOARRAY" => new ArrayType(new KeyValuePairType(dt.KeyType, dt.ValueType)),
                    "TOJSON" => new JsonType(),
                    "KEYS" => new ListType(dt.KeyType),
                    "VALUES" => new ListType(dt.ValueType),
                    "COUNT" => new LongType(),
                    // F1: MaxPair/MinPair → chiave (String); SET/SETKEYVALUE come sopra.
                    "MAXPAIR" or "MINPAIR" => new StringType(),
                    "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" or "TRYGET" or "TRYADD" => new BitType(),
                    "CLEAR" => new VoidType(),
                    _ => null
                };
            case HashSetType hst:
                return upperMethodName switch
                {
                    "UNION" or "INTERSECT" or "EXCEPT" or "SYMMETRICEXCEPT" => hst,
                    "TOARRAY" => new ArrayType(hst.ElementType),
                    "TOLIST" => new ListType(hst.ElementType),
                    "TOJSON" => new JsonType(),
                    "COUNT" => new LongType(),
                    "ADD" or "REMOVE" or "CONTAINS" => new BitType(),
                    "CLEAR" => new VoidType(),
                    _ => null
                };
            case HyperLogLogType:
                /* Tutti i metodi HLL passano dal dispatcher runtime (CollectionInvoke) che
                 * ritorna `object`: il tipo inferito è InferredType (dinamico), così le
                 * coercizioni/PRINT usano le conversioni val (gmb_val_to_double/to_string).
                 * Con DoubleType/StringType il consumer emetteva conv.r8/unbox su un object
                 * = InvalidProgram (verificato: h.Count() in PRINT). */
                return upperMethodName switch
                {
                    "COUNT" or "ERROR" or "PRECISION" or "ADD" or "MERGE" or "LOAD"
                        or "CLEAR" or "SERIALIZE" => new InferredType(),
                    _ => null
                };
            case QueueType qt:
                return upperMethodName switch
                {
                    "DEQUEUE" or "PEEK" or "FRONT" or "BACK" => qt.ElementType,
                    "CONTAINS" or "ISEMPTY" => new BitType(),
                    "COUNT" => new LongType(),
                    "TOARRAY" => new ArrayType(qt.ElementType),
                    "TOLIST" => new ListType(qt.ElementType),
                    "TOJSON" => new JsonType(),
                    "ENQUEUE" or "CLEAR" or "TRIMEXCESS" => new VoidType(),
                    _ => null
                };
            case StackType st:
                return upperMethodName switch
                {
                    "POP" or "PEEK" or "FRONT" or "BACK" => st.ElementType,
                    "CONTAINS" or "ISEMPTY" => new BitType(),
                    "COUNT" => new LongType(),
                    "TOARRAY" => new ArrayType(st.ElementType),
                    "TOLIST" => new ListType(st.ElementType),
                    "TOJSON" => new JsonType(),
                    "PUSH" or "CLEAR" or "TRIMEXCESS" => new VoidType(),
                    _ => null
                };
            case SortedDictType sdt:
                return upperMethodName switch
                {
                    "TODICT" => new DictType(sdt.KeyType, sdt.ValueType),
                    "TOARRAY" => new ArrayType(new KeyValuePairType(sdt.KeyType, sdt.ValueType)),
                    "TOJSON" => new JsonType(),
                    "KEYS" => new ListType(sdt.KeyType),
                    "VALUES" => new ListType(sdt.ValueType),
                    "COUNT" => new LongType(),
                    // SET/SETKEYVALUE: InferredType — parity DICT (vedi sopra).
                    "SET" or "SETKEYVALUE" => new InferredType(),
                    "GET" => sdt.ValueType,
                    "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" or "TRYGET" => new BitType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case TreeType or AVLTreeType or RedBlackTreeType or BPlusTreeType:
                var keyType = instanceType switch
                {
                    TreeType t => t.KeyType, AVLTreeType avl => avl.KeyType, RedBlackTreeType rbt => rbt.KeyType,
                    BPlusTreeType bpt => bpt.KeyType, _ => null
                };
                var valueType = instanceType switch
                {
                    TreeType t => t.ValueType, AVLTreeType avl => avl.ValueType,
                    RedBlackTreeType rbt => rbt.ValueType, BPlusTreeType bpt => bpt.ValueType, _ => null
                };
                if (keyType == null || valueType == null) return null;
                return upperMethodName switch
                {
                    "MIN" or "MAX" or "EXTRACTMIN" or "EXTRACTMAX" => new KeyValuePairType(keyType, valueType),
                    "GET" => valueType,
                    "TRYGETVALUE" or "TRYGET" or "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" => new BitType(),
                    "COUNT" => new LongType(),
                    "KEYS" => new ListType(keyType),
                    "VALUES" => new ListType(valueType),
                    "TOARRAY" => new ArrayType(new KeyValuePairType(keyType, valueType)),
                    "TODICT" => new DictType(keyType, valueType),
                    "TOJSON" => new JsonType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case BinaryIndexedTreeType bitt:
                return upperMethodName switch
                {
                    "QUERY" or "QUERYRANGE" => bitt.ElementType,
                    "TOARRAY" => new ArrayType(bitt.ElementType),
                    "TOLIST" => new ListType(bitt.ElementType),
                    "TOJSON" => new JsonType(),
                    "UPDATE" => new VoidType(),
                    _ => null
                };
            case TrieType:
                return upperMethodName switch
                {
                    "ADD" or "REMOVE" or "CONTAINS" or "STARTSWITH" => new BitType(),
                    "FINDALL" or "TOARRAY" or "TOLIST" => new ListType(new StringType()),
                    "COUNT" => new LongType(),
                    "TOJSON" => new JsonType(),
                    "CLEAR" => new VoidType(),
                    _ => null
                };
            case LinkedListType or DoublyLinkedListType:
                var llt = (instanceType as LinkedListType)?.ElementType ??
                          (instanceType as DoublyLinkedListType)!.ElementType;
                return upperMethodName switch
                {
                    "REMOVE" or "CONTAINS" => new BitType(),
                    "COUNT" or "INDEXOF" => new LongType(),
                    "FIRST" or "LAST" => llt,
                    "TOARRAY" => new ArrayType(llt),
                    "TOLIST" => new ListType(llt),
                    "TOJSON" => new JsonType(),
                    "ADD" or "ADDFIRST" or "ADDLAST" or "ADDAFTER" or "ADDBEFORE" or "ADDUNIQUE"
                        or "REMOVEFIRST" or "REMOVELAST" or "CLEAR" or "SORT" or "REVERSE" =>
                        new VoidType(),
                    _ => null
                };
            case CircularLinkedListType cllt:
                return upperMethodName switch
                {
                    "REMOVE" or "CONTAINS" => new BitType(),
                    "COUNT" or "INDEXOF" => new LongType(),
                    "CURRENT" => cllt.ElementType,
                    "TOARRAY" => new ArrayType(cllt.ElementType),
                    "TOLIST" => new ListType(cllt.ElementType),
                    "TOJSON" => new JsonType(),
                    "ADD" or "ROTATE" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case BinaryHeapType bht:
                return upperMethodName switch
                {
                    "EXTRACT" or "PEEK" => bht.ElementType,
                    "COUNT" => new LongType(),
                    "TOARRAY" => new ArrayType(bht.ElementType),
                    "TOLIST" => new ListType(bht.ElementType),
                    "TOJSON" => new JsonType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case BagType bag:
                return upperMethodName switch
                {
                    "REMOVE" or "CONTAINS" => new BitType(),
                    "COUNT" or "UNIQUECOUNT" => new LongType(),
                    "UNIQUEITEMS" => new HashSetType(bag.ElementType),
                    "TODICT" => new DictType(bag.ElementType, new LongType()),
                    "TOARRAY" => new ArrayType(bag.ElementType),
                    "TOLIST" => new ListType(bag.ElementType),
                    "TOJSON" => new JsonType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case CacheType cache:
                return upperMethodName switch
                {
                    "GET" => cache.ValueType,
                    "TRYGETVALUE" or "TRYGET" or "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" => new BitType(),
                    "COUNT" => new LongType(),
                    "KEYS" => new ListType(cache.KeyType),
                    "VALUES" => new ListType(cache.ValueType),
                    "TOARRAY" => new ArrayType(new KeyValuePairType(cache.KeyType, cache.ValueType)),
                    "TODICT" => new DictType(cache.KeyType, cache.ValueType),
                    "TOJSON" => new JsonType(),
                    "SET" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case GraphType graph:
                return upperMethodName switch
                {
                    "ADDNODE" or "ADDNODECONSISTENT" or "REMOVENODE" or "REMOVEEDGE" or "HASNODE" or "HASEDGE" or "HASCYCLE" or "ISBIPARTITE" => new BitType(),
                    "GETNEIGHBORS" or "BFS" or "DFS" or "NODES" or "KEYS" or "SHORTESTPATH" or "DIJKSTRAPATH" or "FINDPATH" or "ASTAR" or "TOPOLOGICALSORT" => new ListType(graph.NodeType),
                    "DIJKSTRA" or "BELLMANFORD" or "BETWEENNESSCENTRALITY" => new DictType(graph.NodeType, new InferredType()),
                    "FLOYDWARSHALL" => new DictType(graph.NodeType, new DictType(graph.NodeType, new InferredType())),
                    "CONNECTEDCOMPONENTS" or "STRONGLYCONNECTEDCOMPONENTS" => new ListType(new ListType(graph.NodeType)),
                    "PAGERANK" or "MINIMUMSPANNINGTREE" => new ListType(new DictType(graph.NodeType, new InferredType())),
                    "GETEDGEWEIGHT" or "CLOSENESSCENTRALITY" or "DIAMETER" or "ECCENTRICITY" or "CLUSTERINGCOEFFICIENT" => new DoubleType(),
                    "NODECOUNT" or "COUNT" or "GETINDEGREE" or "GETOUTDEGREE" or "GETDEGREE" => new LongType(),
                    "TOARRAY" => new ArrayType(new InferredType()),
                    "TODICT" => new DictType(graph.NodeType, new ListType(new InferredType())),
                    "TOJSON" => new JsonType(),
                    "ADDEDGE" => new VoidType(),
                    _ => null
                };
            case TimelineType tlt:
                return upperMethodName switch
                {
                    "REMOVE" or "CONTAINS" or "CONTAINSKEY" or "CONTAINSVALUE" => new BitType(),
                    "GET" or "FIRST" or "LAST" => tlt.ValueType,
                    "GETRANGE" => tlt,
                    "COUNT" => new LongType(),
                    "KEYS" => new ListType(new DateTimeType()),
                    "VALUES" => new ListType(tlt.ValueType),
                    "TOARRAY" => new ArrayType(new KeyValuePairType(new DateTimeType(), tlt.ValueType)),
                    "TODICT" => new DictType(new DateTimeType(), tlt.ValueType),
                    "TOJSON" => new JsonType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case HashRingType hrt:
                return upperMethodName switch
                {
                    "GETNODE" => hrt.NodeType,
                    "COUNT" => new LongType(),
                    "TOARRAY" or "TOLIST" => new ListType(hrt.NodeType),
                    "TOJSON" => new JsonType(),
                    "ADDNODE" or "REMOVENODE" => new VoidType(),
                    _ => null
                };
            case RingBufferType rbt:
                return upperMethodName switch
                {
                    "DEQUEUE" or "PEEK" => rbt.ElementType,
                    "COUNT" or "CAPACITY" => new LongType(),
                    "TOARRAY" => new ArrayType(rbt.ElementType),
                    "TOLIST" => new ListType(rbt.ElementType),
                    "TOJSON" => new JsonType(),
                    "ENQUEUE" or "CLEAR" => new VoidType(),
                    _ => null
                };
            case SparseSetType:
                return upperMethodName switch
                {
                    "REMOVE" or "HASITEM" => new BitType(),
                    "COUNT" or "CAPACITY" => new LongType(),
                    "TOARRAY" or "TOLIST" => new ListType(new LongType()),
                    "TOJSON" => new JsonType(),
                    "ADD" or "CLEAR" => new VoidType(),
                    _ => null
                };
        }

        // 2. Fallback nel registro degli schemi built-in (per tipi a struttura fissa come HttpOptions)
        if (TypeSchemaRegistry.TryGetValue(instanceType.GetType(), out var schema))
        {
            if (schema!.Methods.TryGetValue(methodName, out var returnType))
            {
                return returnType;
            }
        }

        return null;
    }
}
