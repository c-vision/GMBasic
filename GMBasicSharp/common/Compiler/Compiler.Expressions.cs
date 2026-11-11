// FILE: Interpreter.Compiler.Compiler.Expressions.cs

using System.Collections.Immutable;
namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Massimo numero di argomenti/slot FFI (2026-11: era 8, limite della tabella
    /// di dispatch; con il motore libffi il limite è il formato a 16 bit). Specchio di
    /// GMB_FFI_MAX_ARGS in gmb_bytecode/include/gmb_compiler.h.</summary>
    public const int FfiMaxArgs = 64;

    /// <summary>
    /// Dispatcher principale per visitare e compilare un nodo espressione (Expr).
    /// </summary>
    private void VisitExpression(Expr expr)
    {
        if (expr.SourceLineIndex >= 0)
            _currentSourceLineIndex = expr.SourceLineIndex;

        switch (expr)
        {
            case NumberExpr n: EmitConstant(n.Value); break;
            case StringExpr s: EmitConstant(s.Value); break;
            case BooleanExpr b: EmitByte((byte)(b.Value ? (byte)OpCode.OP_PUSH_TRUE : (byte)OpCode.OP_PUSH_FALSE)); break;
            case VariableExpr v:
                // Sentinella del record buffer DBF: base di un FieldAccessExpr "campo nudo"
                // (`PRINT NomeCampo` con area aperta), prodotta dal Linker (SymbolId = -2).
                // Va emessa come NIL esplicito: senza questo il compilatore la trattava come
                // variabile nuda → 0.0 numerico, e la VM/C leggeva il campo solo per l'effetto
                // collaterale di trattare un receiver non-ref come NIL, mentre il PCode
                // (che testa `obj == null`) leggeva vuoto. Parità IL/C99/C99Native.
                if (v.SymbolId == SymbolTable.DbfRecordBufferSymbolId)
                {
                    EmitByte((byte)OpCode.OP_PUSH_NIL);
                    break;
                }
                if (v.NameCleaned.Kind is TokenKind.Form or TokenKind.SqlQuery
                    && !IsRealVariableReference(v.NameCleaned.Lexeme))
                {
                    // FORM/QUERY usate come NOME BSP (`QUERY["name"]`): non sono variabili del
                    // programma, si risolvono dal dynamic scope dell'host. La guardia
                    // IsRealVariableReference e' indispensabile: se il programma ha DAVVERO
                    // dichiarato un simbolo con quel nome (`DIM Query = 9`), il simbolo dichiarato
                    // vince e va caricato dal suo slot — altrimenti la lettura restituiva
                    // silenziosamente il vuoto della keyword (bug solo P-Code).
                    var dsIndex = AddConstant(v.NameCleaned.Lexeme);
                    EmitByte((byte)OpCode.OP_LOAD_DYNAMIC_SCOPE);
                    EmitShort(dsIndex);
                }
                else if (_constants.TryGetValue(v.NameCleaned.Lexeme, out var constValue))
                {
                    EmitConstant(constValue);
                }
                else if (v.SymbolId < 0 && (_dbfReplaceValueMode || _dbfScopeCondMode) && !IsRealVariableReference(v.NameCleaned.Lexeme))
                {
                    // Nome NON risolto e NON dichiarato (campo late-binding) in un'espressione: se è un CAMPO dell'area DBF attiva
                    // (late-binding Clipper) va letto dal record corrente, come i campi nudi.
                    // Il Linker produce il nodo sentinella+bare-field in lettura (`PRINT Saldo`)
                    // ma dentro `REPLACE ... WITH Saldo + 50` lascia VariableExpr#-1 → qui
                    // EmitLoadVariable(-1) leggeva un local dummy 0 → valore 0+50 (vmc 50
                    // invece di 150; parità PCode/C99/IL). Stesso percorso name-based dei
                    // campi nudi: NIL + OP_LOAD_FIELD <nome>.
                    EmitByte((byte)OpCode.OP_PUSH_NIL);
                    var lbNameIdx = AddConstant(v.NameCleaned.Lexeme);
                    EmitByte((byte)OpCode.OP_LOAD_FIELD);
                    EmitShort(lbNameIdx);
                }
                else
                {
                    LoadVariable(new VariableTarget(-1, v.NameSuffixed, v.NameCleaned));
                }

                break;
            case ArrayAccessExpr a: VisitArrayAccessExpression(a); break;
            case FieldAccessExpr f: VisitFieldAccessExpression(f); break;
            case BinaryExpr b: VisitBinaryExpression(b); break;
            case UnaryExpr u: VisitUnaryExpression(u); break;
            case GroupingExpr g: VisitExpression(g.Expression); break;
            case ForeignAmountExpr fx:
                VisitExpression(fx.Amount);
                if (fx.ExchangeRate is not null) VisitExpression(fx.ExchangeRate);
                break;
            case FunctionExpr f: VisitFunctionExpression(f); break;
            case BuiltinFunctionCallExpr b: VisitBuiltinFunctionCallExpression(b); break;
            case IndexedAccessExpr i: VisitIndexedAccessExpression(i); break;
            case GoFunctionExpr g: VisitGoFunctionExpression(g); break;
            case ArrayLiteralExpr al: VisitArrayLiteralExpression(al); break;
            case DictionaryLiteralExpr dl: VisitDictionaryLiteralExpression(dl); break;
            case ListLiteralExpr ll: VisitListLiteralExpression(ll); break;
            case NewExpr n: VisitNewExpression(n); break;
            case HttpFetchExpr f: VisitFetchExpression(f); break;
            case PropertyAccessExpr p: VisitPropertyAccessExpression(p); break;
            case MethodCallExpr m: VisitMethodCallExpression(m); break;
            case LambdaExpr l: VisitLambdaExpression(l); break;
            case MacroExpr m: VisitMacroExpression(m); break;
            case EvalExpr ee: VisitEvalExpression(ee); break;
            case FunctionReferenceExpr fr: VisitFunctionReferenceExpression(fr); break;
            case MakeByRefExpr mbr: VisitMakeByRefExpression(mbr); break;
            case ListComprehensionExpr lc: VisitListComprehensionExpression(lc); break;
            case GeneratorExpr ge: VisitGeneratorExpression(ge); break;
            case ResolveExpr re: VisitResolveExpression(re); break;
            case QualifiedNameExpr qn:
                {
                    var lastPart = qn.Parts[^1];
                    if (_constants.TryGetValue(lastPart.Lexeme, out var qnConst))
                    {
                        EmitConstant(qnConst);
                    }
                    else
                    {
                        LoadVariable(new VariableTarget(-1, lastPart, lastPart));
                    }
                    break;
                }
            case BaseExpr be: { var selfIdx = ResolveLocal("SELF"); if (selfIdx >= 0) { EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selfIdx); } else EmitByte((byte)OpCode.OP_PUSH_NIL); } break;
            case TernaryExpr te: VisitTernaryExpression(te); break;
            case NilExpr ne: EmitByte((byte)OpCode.OP_PUSH_NIL); break;
            case InterpolatedStringExpr ise: VisitInterpolatedStringExpression(ise); break;
            case TupleLiteralExpr tle: VisitTupleLiteralExpression(tle); break;
            case AwaitExpr ae: VisitExpression(ae.Target); break;
            case AsyncFunctionExpr afe: VisitAsyncFunctionExpression(afe); break;
            // DIRECTCAST(expr, TipoNumerico): coercizione ESPLICITA (VAL: stringa→numero,
            // numero→sé stesso). Prima il cast era ignorato nel chunk → su VM/C `"3.5" - 3.5`
            // usava la coercizione aritmetica del runtime (divergente da PCode/IL).
            case DirectCastExpr dce:
                VisitExpression(dce.Expression);
                if (dce.TargetType is LongType or IntegerType or UIntegerType or ULongType or ByteType
                    or SByteType or QuadType or UQuadType or SingleType or DoubleType or CurrencyType)
                {
                    EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                    EmitShort((ushort)BuiltInFunction.VAL);
                    EmitByte((byte)(1));
                }
                break;
            case SafeCastExpr sce: 
                VisitExpression(sce.Expression);
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.SAFECAST);
                EmitByte((byte)(1));
                break;
            case NullConditionalAccessExpr nca: VisitNullConditionalAccessExpression(nca); break;
            case SwitchExpr sw: VisitSwitchExpression(sw); break;
            case NullableHasValueExpr nhv: VisitNullableHasValueExpression(nhv); break;
            case NullableValueExpr nv: VisitNullableValueExpression(nv); break;
            case PrefixIncrementExpr pie: VisitPrefixIncrementExpression(pie); break;
            case PostfixIncrementExpr poie: VisitPostfixIncrementExpression(poie); break;
            case ArraySliceExpr ase: VisitArraySliceExpression(ase); break;
            case NamedArgumentExpr nae: VisitExpression(nae.Value); break;
            case AssignmentExpr ae2: VisitAssignmentExpression(ae2); break;
            case LinqQueryExpr lqe: VisitLinqQueryExpression(lqe); break;
            case DictComprehensionExpr dce2: VisitDictComprehensionExpression(dce2); break;
            case SetComprehensionExpr sce: VisitSetComprehensionExpression(sce); break;
            case JsonToObjectExpr jtoe: VisitJsonToObjectExpression(jtoe); break;
            case UnresolvedCallExpr uce: VisitUnresolvedCallExpression(uce); break;
            default:
                throw new NotImplementedException(
                    $"Compiler support for expression type '{expr.GetType().Name}' is not yet implemented.");
        }
    }

    /// <summary>
    /// Compila un'espressione binaria, con gestione speciale per gli
    /// operatori logici AND e OR per implementare lo short-circuiting.
    /// </summary>
    private void VisitBinaryExpression(BinaryExpr expr)
    {
        // Gestione operatore IS (type check)
        if (expr.Op.Kind == TokenKind.Is)
        {
            VisitExpression(expr.Left);
            // IS operator handlers
            var typeCode = expr.Right switch
            {
                VariableExpr { NameCleaned.Lexeme: "STRING" } => 0,
                VariableExpr { NameCleaned.Lexeme: "INTEGER" } => 1,
                VariableExpr { NameCleaned.Lexeme: "DOUBLE" } => 2,
                VariableExpr { NameCleaned.Lexeme: "NIL" } => 3,
                VariableExpr { NameCleaned.Lexeme: "LIST" } => 5,
                VariableExpr { NameCleaned.Lexeme: "OBJECT" } => 4,
                TypeExpr { Type: StringType } => 0,
                TypeExpr { Type: IntegerType } => 1,
                TypeExpr { Type: DoubleType or SingleType or CurrencyType } => 2,
                TypeExpr { Type: NilType } => 3,
                TypeExpr { Type: ListType } => 5,
                TypeExpr { Type: ClassType or UserDefinedType or UnresolvedQualifiedType } => 4,
                TypeExpr => 4, // any unrecognized type → check as OBJECT
                _ => -1
            };
            if (typeCode >= 0)
            {
                EmitByte((byte)OpCode.OP_IS_TYPE);
                EmitByte((byte)typeCode);
            }
            else
            {
                EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_CMP_NE);
            }
            return;
        }

        // Gestione Short-Circuiting per null-coalescing (??)
        if (expr.Op.Kind == TokenKind.QuestionMarkQuestion)
        {
            VisitExpression(expr.Left);
            EmitByte((byte)OpCode.OP_DUP);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_CMP_NE);
            var jumpToEnd = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
            EmitByte((byte)OpCode.OP_POP);
            VisitExpression(expr.Right);
            PatchJump(jumpToEnd);
            return;
        }

        // UDT/CLASS binary operator overloading
        var leftInferred = CompilerInferExpressionType(expr.Left);
        if (leftInferred is UserDefinedType udtType)
        {
            var opName = $"__{expr.Op.Kind.ToString().ToUpper()}__";
            if (udtType.Operators.Any(o => o.OperatorToken.Kind == expr.Op.Kind))
            {
                VisitExpression(expr.Left);
                VisitExpression(expr.Left);
                VisitExpression(expr.Right);
                EmitConstant(2.0);
                var opIdx = AddConstant(opName);
                EmitBytes((byte)OpCode.OP_CALL_MEMBER);
                EmitShort(opIdx);
                return;
            }
        }
        if (leftInferred is ClassType ct && ct.Definition.Members.OfType<OperatorMember>().Any(o => o.Operator.OperatorToken.Kind == expr.Op.Kind))
        {
            var opName = $"__{expr.Op.Kind.ToString().ToUpper()}__";
            VisitExpression(expr.Left);
            VisitExpression(expr.Left);
            VisitExpression(expr.Right);
            EmitConstant(2.0);
            var opIdx = AddConstant(opName);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(opIdx);
            return;
        }

        // OR logico (keyword): short-circuit e risultato NORMALIZZATO -1/0.
        if (expr.Op.Kind == TokenKind.Or)
        {
            /* OR (keyword) short-circuit con il canone condiviso:
             * sinistra VERA -> -1, altrimenti il VALORE destro (`0 OR 5` = 5). */
            VisitExpression(expr.Left);
            var jumpToEndIfTrue = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
            VisitExpression(expr.Right);
            var skipShortcut = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
            PatchJump(jumpToEndIfTrue);
            EmitByte((byte)OpCode.OP_PUSH_TRUE);
            PatchJump(skipShortcut);
            return;
        }

        if (expr.Op.Kind == TokenKind.Xor)
        {
            /* XOR (keyword) = BITWISE sui valori (cast long), come `^^`/BitXor:
             * `6 XOR 3` = 5, `255 XOR 15` = 240, `5 XOR -1` = -6 (minitest expr_operators_xor). */
            VisitExpression(expr.Left);
            VisitExpression(expr.Right);
            EmitByte((byte)OpCode.OP_XOR);
            return;
        }
        if (expr.Op.Kind == TokenKind.BitXor)
        {
            VisitExpression(expr.Left);
            VisitExpression(expr.Right);
            EmitByte((byte)OpCode.OP_XOR);
            return;
        }
        if (expr.Op.Kind == TokenKind.BitAnd)
        {
            VisitExpression(expr.Left);
            VisitExpression(expr.Right);
            EmitByte((byte)OpCode.OP_BITAND);
            return;
        }
        if (expr.Op.Kind == TokenKind.BitOr)
        {
            VisitExpression(expr.Left);
            VisitExpression(expr.Right);
            EmitByte((byte)OpCode.OP_BITOR);
            return;
        }

        if (expr.Op.Kind == TokenKind.And)
        {
            /* AND (keyword) short-circuit con il canone condiviso:
             * sinistra FALSA -> 0, altrimenti il VALORE destro (mai normalizzato a -1/0:
             * `6 AND 3` = 3, `(2<>0) AND 5` = 5 — vedi minitest constructs/expr_operators_*). */
            VisitExpression(expr.Left);
            var jumpToEndIfFalse = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            VisitExpression(expr.Right);
            var skipShortcut = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
            PatchJump(jumpToEndIfFalse);
            EmitByte((byte)OpCode.OP_PUSH_FALSE);
            PatchJump(skipShortcut);
            return;
        }

        // --- CONSTANT FOLDING: se entrambi gli operandi sono costanti numeriche,
        // calcola il risultato a compile-time ed emetti una sola OP_PUSH_CONST.
        if (expr.Left is NumberExpr leftNum && expr.Right is NumberExpr rightNum)
        {
            var l = leftNum.Value;
            var r = rightNum.Value;
            // Divisione/MOD per zero: NON si piega. Il valore "legacy" (+Inf/NaN/0) non esiste
            // più: l'espressione deve arrivare all'opcode runtime e sollevare ERR 11 con lo
            // schema a 3 livelli (prima il folding produceva +Infinity e nessun errore).
            var divideByZero = r == 0 && expr.Op.Kind is TokenKind.Divide or TokenKind.IntegerDivide or TokenKind.Mod;
            if (!divideByZero)
            {
                double result = expr.Op.Kind switch
                {
                    TokenKind.Plus => l + r,
                    TokenKind.Minus => l - r,
                    TokenKind.Multiply => l * r,
                    TokenKind.Divide => l / r,
                    TokenKind.IntegerDivide => Math.Truncate(l / r),
                    TokenKind.Power => Math.Pow(l, r),
                    TokenKind.Mod => l % r,
                    TokenKind.Xor => (double)((long)l ^ (long)r),
                    TokenKind.Equal => l == r ? -1 : 0,
                    TokenKind.GreaterThan => l > r ? -1 : 0,
                    TokenKind.LessEqual => l <= r ? -1 : 0,
                    TokenKind.GreaterEqual => l >= r ? -1 : 0,
                    TokenKind.NotEqual => l != r ? -1 : 0,
                    TokenKind.LessThan => l < r ? -1 : 0,
                    _ => double.NaN
                };
                if (!double.IsNaN(result))
                {
                    EmitConstant(result);
                    return;
                }
            }
        }

        // Gestione standard per tutti gli altri operatori (o non-folding)
        VisitExpression(expr.Left);
        VisitExpression(expr.Right);

        var opCode = expr.Op.Kind switch
        {
            TokenKind.Plus => OpCode.OP_ADD,
            TokenKind.Minus => OpCode.OP_SUBTRACT,
            TokenKind.Multiply => OpCode.OP_MULTIPLY,
            TokenKind.Divide => OpCode.OP_DIVIDE,
            TokenKind.IntegerDivide => OpCode.OP_INT_DIVIDE,
            TokenKind.Power => OpCode.OP_POWER,
            TokenKind.Mod => OpCode.OP_MOD,
            TokenKind.Equal => OpCode.OP_CMP_EQ,
            TokenKind.GreaterThan => OpCode.OP_CMP_GT,
            TokenKind.LessEqual => OpCode.OP_CMP_LTE,
            TokenKind.GreaterEqual => OpCode.OP_CMP_GTE,
            TokenKind.ExactlyEqual => OpCode.OP_CMP_EQ,
            TokenKind.NotEqual => OpCode.OP_CMP_NE,
            TokenKind.LessThan => OpCode.OP_CMP_LT,
            _ => throw new NotImplementedException(
                $"Compiler support for binary operator '{expr.Op.Kind}' is not yet implemented.")
        };
        EmitByte((byte)opCode);
    }

    /// <summary>
    /// Compila un'espressione unaria.
    /// </summary>
    private void VisitUnaryExpression(UnaryExpr expr)
    {
        // Constant folding per negazione unaria: -5 → OP_PUSH_CONST -5
        if (expr.Op.Kind == TokenKind.Minus && expr.Right is NumberExpr num)
        {
            EmitConstant(-num.Value);
            return;
        }

        // UDT/CLASS unary operator overloading
        var unaryInferred = CompilerInferExpressionType(expr.Right);
        var unaryOpName = $"__{expr.Op.Kind.ToString().ToUpper()}__";
        if ((unaryInferred is UserDefinedType uudt && uudt.Operators.Any(o => o.OperatorToken.Kind == expr.Op.Kind))
            || (unaryInferred is ClassType uct && uct.Definition.Members.OfType<OperatorMember>().Any(o => o.Operator.OperatorToken.Kind == expr.Op.Kind)))
        {
            VisitExpression(expr.Right);
            VisitExpression(expr.Right);
            EmitConstant(1.0);
            var uopIdx = AddConstant(unaryOpName);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(uopIdx);
            return;
        }
        VisitExpression(expr.Right);

        // NOT (keyword) = negazione LOGICA: (right == 0) → -1/0, su qualunque tipo.
        if (expr.Op.Kind == TokenKind.Not)
        {
            EmitConstant(0.0);
            EmitByte((byte)OpCode.OP_CMP_EQ);
            return;
        }

        var opCode = expr.Op.Kind switch
        {
            TokenKind.Minus => OpCode.OP_NEGATE,
            TokenKind.BitNot => OpCode.OP_NOT,   // ! = NOT bitwise
            _ => throw new NotImplementedException(
                $"Compiler support for unary operator '{expr.Op.Kind}' is not yet implemented.")
        };
        EmitByte((byte)opCode);
    }

    /// <summary>
    /// Compila un'espressione di accesso indicizzato (es. myJson("key")).
    /// </summary>
    private void VisitIndexedAccessExpression(IndexedAccessExpr expr)
    {
        // Compila l'oggetto base (es. myJson)
        VisitExpression(expr.Base);

        // Compila tutte le espressioni degli indici.
        foreach (var index in expr.Indices)
        {
            VisitExpression(index);
        }

        // Emette l'istruzione per leggere l'elemento, seguita dal numero di indici.
        // La VM userà OP_LOAD_ELEMENT sia per gli array che per i JSON.
        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, (byte)expr.Indices.Length);
    }

    /// <summary>
    /// Vero se il nodo è già stato risolto dal linker a una VARIABILE dichiarata.
    /// Richiede la tabella simboli del programma (disponibile quando il contesto di
    /// compilazione è un <see cref="Basic.BasicProgram"/>); in ogni altro contesto
    /// (macro, host esterni) ritorna false e il comportamento resta invariato.
    /// </summary>
    private bool IsResolvedVariable(ArrayAccessExpr expr)
    {
        if (expr.SymbolId < 0) return false;
        if (_ctx is not Basic.BasicProgram bp) return false;
        return bp.Symbols.GetSymbol(expr.SymbolId).DeclarationNode is Basic.VariableDeclaration;
    }

    /// <summary>
    /// Compila un'espressione di accesso a un elemento di un array.
    /// Gestisce la disambiguazione con le chiamate a funzione e il caso speciale
    /// in cui un array viene passato per intero (con parentesi vuote).
    /// </summary>
    private void VisitArrayAccessExpression(ArrayAccessExpr expr)
    {
        // Caso speciale: Se un array viene referenziato con parentesi vuote (es. `MyArray()`),
        // significa che si sta facendo riferimento all'intero array, non a un elemento.
        // Lo trattiamo come una semplice variabile da caricare.
        if (expr.Indices.IsEmpty)
        {
            // Disambigua PRIMA: parentesi vuote possono essere anche una chiamata a una
            // SUB/FUNCTION senza argomenti (`ThrowAtDepth1()`). Senza questo check la chiamata
            // veniva compilata come LoadVariable (no-op PUSH_NIL) e il corpo non veniva MAI
            // eseguito (es. PCodeGpmExtremeUnwind: THROW 703 mai lanciato, FAIL:THROW-NOT-CAUGHT).
            if (_procedureMap.TryGetValue((expr.NameCleaned.Lexeme, 0), out var bridgeProc0))
            {
                var callLoc = _chunk.Code.Count;
                EmitByte((byte)OpCode.OP_CALL_N);
                EmitByte((byte)(0xFF)); // addr hi
                EmitByte((byte)(0xFF)); // addr lo
                EmitByte((byte)0); // 0 argomenti
                // 2026-09-24 (overload): patch con arità della definizione, non solo nome.
                _jumpsToPatch.Add((callLoc + 1, CallPatchToken(expr.NameCleaned, bridgeProc0, 0)));
                return;
            }
            if (expr.NameCleaned.Kind is TokenKind.Form or TokenKind.SqlQuery)
            {
                EmitByte((byte)OpCode.OP_LOAD_DYNAMIC_SCOPE);
                EmitShort(AddConstant(expr.NameCleaned.Lexeme));
                return;
            }
            LoadVariable(new VariableTarget(-1,expr.NameSuffixed, expr.NameCleaned));
            return;
        }

        // Controlla se il nome e il numero di "indici" corrispondono a una funzione definita.
        // MA: se il linker ha già risolto il nodo a una VARIABILE (es. DIM amax()),
        // è un accesso array — mai una chiamata, anche se il nome collide con una
        // procedura in mappa (es. builtin AMAX): OP_CALL_N verso un indirizzo senza
        // corpo non viene mai patchato e loopa la VM all'infinito (bug MATRIX.ARGMAX/
        // ARGMIN su PCode). Le chiamate vere arrivano come FunctionExpr o con SymbolId
        // di procedura/irrisolto.
        var procedureKey = (expr.NameCleaned.Lexeme, expr.Indices.Length);
        if (_procedureMap.TryGetValue(procedureKey, out var procArr) && !IsResolvedVariable(expr))
        {
            // È una funzione, non un array. La compiliamo come una chiamata a funzione.
            // Gestione BYREF anche qui (parità col percorso FunctionCallExpr): la chiamata a
            // una SUB/FUNCTION può arrivare come ArrayAccessExpr se il linker non la risolve a
            // FunctionCallExpr — senza MAKE_BYREF il parametro BYREF riceverebbe una copia e la
            // variabile del chiamante non si aggiornerebbe mai.
            for (var ai = 0; ai < expr.Indices.Length; ai++)
            {
                var argExpr = expr.Indices[ai];
                /* Gli ARRAY non si incapsulano in una ByRefBox (2026-11-03): l'array è già
                 * un riferimento e il callee lo usa tale e quale. Con la box, inoltrare un
                 * PARAMETRO array a un'altra procedura passava la box e il callee vedeva
                 * UBOUND=0/elementi vuoti (bug PCode: `SUB Outer(x()); CALL Inner(x)`). */
                var isByRefArg = procArr is ProcedureDefinition pdArr && ai < pdArr.Parameters.Length &&
                    pdArr.Parameters[ai].Flags.HasFlag(SymbolFlags.IsByRef) &&
                    !pdArr.Parameters[ai].Flags.HasFlag(SymbolFlags.IsArray);
                if (isByRefArg && argExpr is VariableExpr veArg)
                {
                    var localArgIdx = ResolveLocal(veArg.NameCleaned.Lexeme);
                    if (localArgIdx >= 0)
                    {
                        EmitByte((byte)OpCode.OP_MAKE_BYREF);
                        EmitShort((ushort)localArgIdx);
                    }
                    else if (_globals.TryGetValue(veArg.NameCleaned.Lexeme, out var gIdxArg))
                    {
                        EmitByte((byte)OpCode.OP_MAKE_BYREF_GLOBAL);
                        EmitShort((ushort)gIdxArg);
                    }
                    else VisitExpression(argExpr);
                }
                else if (isByRefArg && argExpr is ArrayAccessExpr aaeArg && aaeArg.Indices.Length == 1)
                {
                    // Stesso caso di VisitFunctionExpression sopra (Elenco(1) come argomento
                    // BYREF), qui per il percorso in cui la chiamata stessa arriva come
                    // ArrayAccessExpr — vedi OP_MAKE_BYREF_ELEMENT in OpCode.cs.
                    LoadVariable(new VariableTarget(-1, aaeArg.NameSuffixed, aaeArg.NameCleaned));
                    VisitExpression(aaeArg.Indices[0]);
                    EmitByte((byte)OpCode.OP_MAKE_BYREF_ELEMENT);
                }
                else VisitExpression(argExpr);
            }

            var callLoc = _chunk.Code.Count;
            EmitByte((byte)OpCode.OP_CALL_N);
            EmitByte((byte)(0xFF)); // addr hi
            EmitByte((byte)(0xFF)); // addr lo
            EmitByte((byte)expr.Indices.Length);
            // 2026-09-24 (overload): patch con arità della definizione, non solo nome.
            _jumpsToPatch.Add((callLoc + 1, CallPatchToken(expr.NameCleaned, procArr, expr.Indices.Length)));
            return;
        }

        // S_eval §ii: `Nome(args)` arriva come ArrayAccessExpr quando il linker non lo
        // risolve (macro: nessun simbolo host nel programma minimo). Se il nome è una
        // procedura HOST (tabella ExternalHostProcs), compila come OP_CALL_HOST invece
        // che come accesso array (che darebbe NIL). Guard: solo macro (tabella non nulla).
        // Le variabili DEL macro (locali/globali propri) vincono sempre sulla tabella host.
        // Limite: argomenti sempre BYVAL qui (niente MAKE_BYREF senza definizione proc —
        // parità col percorso CLR; scritture su byref perse, documentato).
        if (ExternalHostProcs != null
            && ResolveLocal(expr.NameCleaned.Lexeme) < 0
            && !_globals.TryGetValue(NormalizeIdentifierName(expr.NameCleaned.Lexeme), out _)
            && FindExternalHostProc(expr.NameCleaned.Lexeme, expr.Indices.Length, out var hostCallIdx))
        {
            foreach (var index in expr.Indices)
                VisitExpression(index);
            if (expr.Indices.Length > 255)
                throw new InvalidOperationException($"Macro chiama '{expr.NameCleaned.Lexeme}' con {expr.Indices.Length} argomenti (max 255).");
            EmitByte((byte)OpCode.OP_CALL_HOST);
            EmitShort((ushort)hostCallIdx);
            EmitByte((byte)expr.Indices.Length);
            return;
        }

        // Se non è una funzione, allora è un vero accesso ad array con indici.
        // FORM("x") (sintassi a parentesi, es. un campo di un form POST) arriva qui come
        // ArrayAccessExpr — QUERY["x"] invece usa sempre le parentesi quadre, quindi è sempre un
        // IndexedAccessExpr(Base=VariableExpr) gestito nel case VariableExpr sopra. Il SymbolId
        // NON è un segnale affidabile qui (il Resolver risolve FORM/QUERY a un simbolo REALE,
        // mai al sentinel DynamicScopeSymbolId), va riconosciuto il TOKEN KIND riservato
        // (TokenKind.Form/SqlQuery), stessa tecnica già usata sopra per COUNT/RewriteArrayAccess.
        if (expr.NameCleaned.Kind is TokenKind.Form or TokenKind.SqlQuery)
        {
            EmitByte((byte)OpCode.OP_LOAD_DYNAMIC_SCOPE);
            EmitShort(AddConstant(expr.NameCleaned.Lexeme));
        }
        else
        {
            LoadVariable(new VariableTarget(-1, expr.NameSuffixed, expr.NameCleaned));
        }

        /* ACCESSO SICURO su ARRAY NATIVO (`arr(9)?`, ArrayAccessExpr.IsSafe): OP_LOAD_ELEMENT
         * lancia ERR 9 fuori dai limiti (canone: l'accesso NON sicuro deve fallire). Il `?`
         * deve invece dare NIL → guard esplicito 0 <= i <= UBOUND(arr) (era l'unico caso
         * rotto: liste/dict/stringhe sono già null-safe nell'opcode — sample
         * Collections/CollectionsSafeAccess, gamba PCode in ERR 9). */
        if (expr.IsSafe && expr.Indices.Length == 1 && expr.SymbolId >= 0
            && _ctx is BasicProgram safeProg
            && safeProg.Symbols.GetSymbol(expr.SymbolId).DataType is ArrayType)
        {
            EmitSafeArrayIndexFromStack(expr.Indices[0]);
            return;
        }

        foreach (var index in expr.Indices)
        {
            VisitExpression(index);
        }

        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, (byte)expr.Indices.Length);
    }

    /// <summary>
    /// Compila una chiamata a una funzione definita dall'utente.
    /// La logica per le funzioni built-in è stata rimossa, in quanto ora vengono
    /// gestite esclusivamente da `VisitBuiltinFunctionCallExpression` grazie
    /// all'AST arricchito dal Linker.
    /// </summary>
    private void VisitFunctionExpression(FunctionExpr expr)
    {
        var baseKey = expr.OriginalToken.Lexeme;
        
        if (expr.IsIndirect)
        {
            // Function pointer call: push args, load pointer from variable, call indirectly
            for (var i = 0; i < expr.Args.Length; i++)
                VisitExpression(expr.Args[i]);
            var localIdx = ResolveLocal(baseKey);
            if (localIdx >= 0) { EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)localIdx); } else if (_globals.TryGetValue(baseKey, out var gIdx))
            {
                EmitByte((byte)OpCode.OP_LOAD_GLOBAL);
                EmitShort(gIdx);
            }
            else
            {
                // Function-value CATTURATO da una lambda (`DIM inner = {...};
                // DIM outer = { |x| RETURN EVAL(inner, x) }`): carica l'upvalue — prima
                // PUSH_NIL → call su nil (risultato vuoto, divergenza PCode/VM-C).
                var upIdx = ResolveUpvalueByName(baseKey);
                if (upIdx != -1) EmitBytes((byte)OpCode.OP_LOAD_UPVALUE, (byte)upIdx);
                else EmitByte((byte)OpCode.OP_PUSH_NIL);
            }
            EmitByte((byte)OpCode.OP_CALL_INDIRECT);
            EmitByte((byte)expr.Args.Length);
            return;
        }
        var found = false;
        ProcedureDefinition? proc = null;
        // 2026-09-24: la DEFINIZIONE risolta dal linker ha precedenza sulla ricerca per
        // nome: con omonime in namespace diversi, `_procedureMap` (chiave = nome nudo)
        // sceglierebbe sempre la stessa → entrambe le chiamate eseguirebbero la stessa
        // procedura (bug ImportPriority su PCode/VM-C). Il SymbolId risolto identifica
        // l'overload/namespace giusto (vedi CallPatchToken @sym{id}).
        // ESCLUSA la FFI (ExternalProcedureDefinition): va al percorso `foundFfi`
        // (OP_CALL_FFI*), non alla chiamata normale — altrimenti salterebbe l'indirizzo 0.
        if (expr.ResolvedDefinition is ProcedureDefinition resolvedProc
            and not ExternalProcedureDefinition)
        {
            found = true;
            proc = resolvedProc;
        }
        else
        {
            // Try exact match first, then with more args (for optional params)
            for (int tryArgs = expr.Args.Length; tryArgs <= expr.Args.Length + 20; tryArgs++)
            {
                if (_procedureMap.TryGetValue((baseKey, tryArgs), out var p))
                { found = true; proc = p; break; }
            }
            if (!found) _procedureMap.TryGetValue((baseKey, expr.Args.Length), out proc);
        }

        // Chiamare un GENERATOR PROCEDURE (`Count(3)`) NON deve eseguirne il corpo — a
        // differenza di una chiamata a FUNCTION/SUB normale (OP_CALL_N sotto), deve restituire
        // un handle sospeso che NEXT() guiderà passo-passo (vedi OP_NEW_GENERATOR_CALL). Il
        // corpo viene comunque compilato normalmente più avanti (GeneratorDefinition eredita da
        // ProcedureDefinition, quindi ctx.GetAllProcedures() la include già in CompileProcedures)
        // — qui intercettiamo solo il SITO DI CHIAMATA, prima che finisca nel percorso OP_CALL_N.
        if (found && proc is GeneratorDefinition genDef)
        {
            for (var gi = 0; gi < expr.Args.Length; gi++)
                VisitExpression(expr.Args[gi]);
            var genCallLoc = EmitJump((byte)OpCode.OP_NEW_GENERATOR_CALL);
            _jumpsToPatch.Add((genCallLoc, genDef.Name));
            EmitByte((byte)expr.Args.Length);
            return;
        }

        var foundFfi = _ffiFunctions.TryGetValue((baseKey, expr.Args.Length), out var ffiProc);

        // Chiamata a METODO di closure-context (`__ClosureCtx_...`, dai SUB/FUNCTION locali
        // riscritti dal Linker): gli argomenti vanno emessi dal ramo OP_CALL_CTOR
        // (args + instance + argCount) — il loop generico sotto li emetteva ANCHE, quindi
        // venivano spinti DUE volte e il primo argomento veniva sovrascritto dal secondo
        // (bug FunctionsLocalAndSubs: `Stampa("Mul(5)", Moltiplica(5))` stampava ` 5  -> 50`).
        var isClosureCtxCall = false;
        if (found && proc is ProcedureDefinition pdc0 && (pdc0.Flags & SymbolFlags.IsMethod) == SymbolFlags.IsMethod)
        {
            for (int ci = _locals.Count - 1; ci >= 0; ci--)
            {
                if (_locals[ci].Type is ClassType ||
                    _locals[ci].Name.Lexeme.Contains("__context", StringComparison.OrdinalIgnoreCase) ||
                    _locals[ci].Name.Lexeme.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase))
                { isClosureCtxCall = true; break; }
            }
        }

        for (var i = 0; i < expr.Args.Length && !isClosureCtxCall; i++)
        {
            /* Gli ARRAY non si incapsulano in una ByRefBox (2026-11-03) — vedi il gemello
             * nel percorso ArrayAccessExpr: inoltrare un PARAMETRO array a un'altra
             * procedura passava la box e il callee vedeva UBOUND=0. */
            var isByRef = found && proc is ProcedureDefinition pd &&
                i < pd.Parameters.Length &&
                pd.Parameters[i].Flags.HasFlag(SymbolFlags.IsByRef) &&
                !pd.Parameters[i].Flags.HasFlag(SymbolFlags.IsArray);
            // BYREF scalare FFI (2026-09-28): riusa lo STESSO meccanismo del BYREF ordinario
            // (OP_MAKE_BYREF/OP_MAKE_BYREF_GLOBAL — una ByRefBox generica, scrive-attraverso
            // sullo slot locale/globale del chiamante). L'argomento deve essere una VARIABILE
            // SEMPLICE (non un elemento di array, a differenza del BYREF ordinario — scelta
            // deliberata per restare uniforme sui 4 backend: IL/VM-C/C99* implementano solo
            // questo caso) — un'espressione qualunque cadrebbe silenziosamente nel ramo BYVAL
            // sotto, disallineando il marshalling. Vedi AiWiki
            // Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §1.
            var isFfiByRefScalar = foundFfi && i < ffiProc!.Parameters.Length &&
                ffiProc.Parameters[i].Flags.HasFlag(SymbolFlags.IsByRef) &&
                !ffiProc.Parameters[i].Flags.HasFlag(SymbolFlags.IsArray) &&
                ffiProc.Parameters[i].DataType is not StringType;
            if (isFfiByRefScalar && expr.Args[i] is not VariableExpr)
                throw new BasicException(expr.OriginalToken,
                    $"FFI '{ffiProc!.AliasName?.Literal as string ?? expr.OriginalToken.Lexeme}': il parametro BYREF " +
                    $"'{ffiProc.Parameters[i].Name.Lexeme}' richiede una variabile semplice come argomento (non un'espressione).");
            isByRef = isByRef || isFfiByRefScalar;
            if (isByRef && expr.Args[i] is VariableExpr ve)
            {
                var localIdx = ResolveLocal(ve.NameCleaned.Lexeme);
                if (localIdx >= 0)
                {
                    EmitByte((byte)OpCode.OP_MAKE_BYREF);
                    EmitShort((ushort)localIdx);
                }
                else if (_globals.TryGetValue(ve.NameCleaned.Lexeme, out var globalIdx))
                {
                    EmitByte((byte)OpCode.OP_MAKE_BYREF_GLOBAL);
                    EmitShort((ushort)globalIdx);
                }
                else { VisitExpression(expr.Args[i]); }
            }
            else if (isByRef && expr.Args[i] is ArrayAccessExpr aaeByRef && aaeByRef.Indices.Length == 1)
            {
                // `ModificaPersona(Elenco(1), ...)` con parametro BYREF: un elemento di array
                // (mono-dimensionale) — senza questo caso cadeva nel ramo BYVAL sotto
                // (VisitExpression semplice, via OP_LOAD_ELEMENT): una modifica al parametro
                // dentro la procedura non si rifletteva mai su Elenco(1) dopo il ritorno. Bug
                // scoperto 2026-09-02 verificando OP_LOAD_ELEMENT_REF, vedi OP_MAKE_BYREF_ELEMENT
                // in OpCode.cs. Array multi-dimensionali: non gestiti, restano BYVAL (sotto).
                LoadVariable(new VariableTarget(-1, aaeByRef.NameSuffixed, aaeByRef.NameCleaned));
                VisitExpression(aaeByRef.Indices[0]);
                EmitByte((byte)OpCode.OP_MAKE_BYREF_ELEMENT);
            }
            else
            {
                VisitExpression(expr.Args[i]);
                // Un parametro UDT passato BYVAL (predefinito) deve ricevere una COPIA
                // indipendente, non lo stesso dizionario del chiamante — altrimenti una
                // modifica al parametro dentro la procedura si riflette sulla variabile
                // originale del chiamante nonostante BYVAL (vedi VisitLetStatement per lo
                // stesso ragionamento su un semplice `P2 = P1`).
                if (found && proc is ProcedureDefinition pdByVal && i < pdByVal.Parameters.Length &&
                    pdByVal.Parameters[i].DataType is UserDefinedType)
                    EmitByte((byte)OpCode.OP_CLONE_UDT);
            }
        }
        // Push defaults for missing optional params
        if (found && proc is ProcedureDefinition pd2)
        {
            for (int i = expr.Args.Length; i < pd2.Parameters.Length; i++)
            {
                var defVal = pd2.Parameters[i].DefaultValue;
                if (defVal != null) VisitExpression(defVal);
                else EmitByte((byte)OpCode.OP_PUSH_NIL);
            }
        }


        if (found && proc is NativeProcedureDefinition nproc)
        {
            // Canonicalizzazione del funcCode (allineato GMBasicAI, fix 2026-08-11): l'operando di
            // OP_CALL_BUILTIN DEVE essere un valore BuiltInFunction, MAI un TokenKind grezzo — i due
            // enum hanno layout numerici completamente diversi (es. TokenKind.Sin=518 vs
            // BuiltInFunction.SIN=29) e un TokenKind emesso qui cadrebbe nel default della VM, che
            // conteneva magic-number di un layout obsoleto. Questo era l'UNICO sito di emissione non
            // canonica (vedi Structural-Bug-Fix-Builtin-Dispatch).
            if (!BuiltInFunctionMap.TryGetValue(nproc.BuiltinKind, out var canonBuiltin))
                throw new BasicException(expr,
                    $"Funzione builtin '{nproc.OriginalName}' (BuiltinKind {nproc.BuiltinKind}) non mappata a un " +
                    "BuiltInFunction. Aggiungere la voce in _builtInFunctionMap (Compiler/Compiler.Expressions.Functions.cs).");
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)canonBuiltin);
            EmitByte((byte)expr.Args.Length);
        }
        else if (found && proc is ProcedureDefinition)
        {
            // Check if this is a method call on a closure context class (IsMethod set by linker)
            if ((proc.Flags & SymbolFlags.IsMethod) == SymbolFlags.IsMethod)
            {
                // Find the context instance by scanning locals for a ClassType variable
                int? contextIdx = null;
                string? contextClassName = null;
                for (int ci = _locals.Count - 1; ci >= 0; ci--)
                {
                    if (_locals[ci].Type is ClassType ct)
                    { contextIdx = ci; contextClassName = ct.Definition.Name.Lexeme; break; }
                    if (_locals[ci].Name.Lexeme.Contains("__context", StringComparison.OrdinalIgnoreCase) ||
                        _locals[ci].Name.Lexeme.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase))
                    { contextIdx = ci; contextClassName = _locals[ci].Name.Lexeme; break; }
                }
                // Se non abbiamo trovato __context ma local[0] è SELF, siamo dentro un metodo
                // di classe — usa SELF (locale[0]) come contesto.
                if (!contextIdx.HasValue && _locals.Count > 0 && _locals[0].Name.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase))
                {
                    contextIdx = 0;
                    contextClassName = null;
                    // Cerca il nome della classe da _procedureAddresses: formato "ClassName.MethodName"
                    var methodName = baseKey;
                    foreach (var kv in _procedureAddresses)
                    {
                        if (kv.Key.EndsWith("." + methodName, StringComparison.OrdinalIgnoreCase))
                        {
                            contextClassName = kv.Key.Substring(0, kv.Key.Length - methodName.Length - 1);
                            break;
                        }
                    }
                }
                if (contextIdx.HasValue && contextClassName != null)
                {
                    // Convention: args, then SELF, then argCount
                    // Stack: [..., arg_0, ..., arg_n, instance, argCount]
                    for (var i = 0; i < expr.Args.Length; i++)
                        VisitExpression(expr.Args[i]);
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)contextIdx.Value);
                    EmitConstant((double)(expr.Args.Length + 1)); // argCount = args + SELF
                    // Resolve full method name: ClassName.MethodName
                    var fullMethodName = contextClassName + "." + proc.OriginalName.Lexeme;
                    var callLocation = EmitJump((byte)OpCode.OP_CALL_CTOR);
                    _jumpsToPatch.Add((callLocation,
                        new Token(TokenKind.Identifier, fullMethodName, null, expr.OriginalToken.Position)));
                    return;
                }
            }

            // Check if this is a local function — if a local variable with FunctionPointerType exists,
            // use indirect call through the variable instead of direct OP_CALL_N
            var hasFuncPtrLocal = false;
            for (int i = _locals.Count - 1; i >= 0; i--)
            {
                if (_locals[i].Name.Lexeme.Equals(baseKey, StringComparison.OrdinalIgnoreCase))
                {
                    if (_locals[i].Type is FunctionPointerType)
                    { hasFuncPtrLocal = true; break; }
                }
            }
            if (hasFuncPtrLocal)
            {
                // Use indirect call through the local variable
                for (var i = 0; i < expr.Args.Length; i++)
                    VisitExpression(expr.Args[i]);
                var localIdx = ResolveLocal(baseKey);
                if (localIdx >= 0) { EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)localIdx); } else EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_CALL_INDIRECT);
                EmitByte((byte)expr.Args.Length);
            }
            else
            {
                var callLocation = _chunk.Code.Count;
                EmitByte((byte)OpCode.OP_CALL_N);
                EmitByte((byte)(0xFF)); // addr hi
                EmitByte((byte)(0xFF)); // addr lo
                EmitByte((byte)(proc is ProcedureDefinition pdef4 ? pdef4.Parameters.Length : expr.Args.Length));
                // 2026-09-24 (overload): patch con arità della definizione risolta.
                _jumpsToPatch.Add((callLocation + 1, CallPatchToken(expr.OriginalToken, proc, expr.Args.Length)));
            }
        }
        else if (foundFfi)
        {
            var libIndex = AddConstant(ffiProc!.LibraryName.Lexeme.Trim('"'));
            var symbolName = ffiProc.AliasName?.Literal as string ?? expr.OriginalToken.Lexeme;
            var nameIndex = AddConstant(symbolName);
            // Marshalling tipizzato: firma intera (LONG/INTEGER/...) omogenea → OP_CALL_FFI_T
            // (kind=1, int64 nativo — ABI corretta vs il contratto all-double di OP_CALL_FFI,
            // che su long vec_add(long,long) leggeva registri sbagliati: vec_add(21,21)→21).
            // Messaggi (= IL/C99Native, parità degli errori):
            // stringa coinvolta / BYREF / firma mista long+double → errore esplicito.
            const byte FfiDouble = 0, FfiInt64 = 1;
            // Array BYREF (2026-09-28): il parametro viaggia come PUNTATORE nativo → classe di
            // registri intera (kind Int64) indipendentemente dal tipo degli elementi — vedi
            // AiWiki Bugs/(open) - FFI-Marshalling-Limitato-No-BYREF-Array-Struct.md punto (1).
            bool IsArrayParam(Parameter p) => p.Flags.HasFlag(SymbolFlags.IsArray) || p.DataType is ArrayType;
            // STRING (2026-09-28): un parametro/ritorno STRING è anch'esso un puntatore
            // (`const char*`) → stessa classe di registri intera degli array. BYVAL solo (BYREF
            // scalare di STRING resta vietato, catturato sotto da anyScalarByRef).
            bool IsStringParam(Parameter p) => !IsArrayParam(p) && p.DataType is StringType;
            // Whitelist esplicita (fix 2026-09-28): tipi validi per uno SCALARE FFI sono SOLO
            // DOUBLE/SINGLE (kind Double) — CurrencyType è ESCLUSO (decimale scalato, non un
            // bit-pattern IEEE754, trattarlo come double sarebbe silenziosamente sbagliato).
            // Qualunque tipo non riconosciuto (UDT, FUNCTION/SUB come tipo, collezioni List/
            // Dict/Graph/..., tuple) NON deve cadere nel default FfiDouble: prima di questo fix
            // ci cadeva silenziosamente, generando codice/marshalling errato invece di un
            // errore compile-time chiaro — vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-
            // BYREF-Struct-Callback.md.
            bool IsFfiDoubleType(DataType? t) => t is DoubleType or SingleType;
            // Nome leggibile per il messaggio d'errore: per un UDT usa il nome BASIC dichiarato
            // (es. "Point"), non il nome della classe AST interna — parità col messaggio
            // C99Native, che stampa già il nome del tipo dichiarato (fix 2026-09-28).
            string FfiTypeDisplayName(DataType t) => t is UserDefinedType udt ? udt.Name.Lexeme : t.GetType().Name;
            byte ClassifyFfiType(DataType? t, string what)
            {
                if (IsFfiInt64Type(t)) return FfiInt64;
                if (IsFfiDoubleType(t)) return FfiDouble;
                throw new BasicException(expr.OriginalToken,
                    $"FFI '{symbolName}': tipo '{(t is null ? "void" : FfiTypeDisplayName(t))}' non supportato per {what} del marshalling FFI " +
                    "(ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/..., array a 8 byte, STRING).");
            }
            // BYREF scalare (2026-09-28): come un array/STRING, un parametro BYREF scalare
            // viaggia come PUNTATORE nativo — stessa classe di registri intera, indipendente
            // dal tipo puntato. BYREF di STRING resta vietato (nessuna semantica in/out chiara
            // per un puntatore-a-puntatore char**, fuori scope). Vedi AiWiki
            // Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §1.
            bool IsByRefScalarParam(Parameter p) => p.Flags.HasFlag(SymbolFlags.IsByRef) && !IsArrayParam(p) && !IsStringParam(p) && p.DataType is not UserDefinedType;
            /* BYREF UDT (2026-11-06): `BYREF p AS <TYPE>` → `<Udt>_t*` (in/out struct). */
            bool IsByRefUdtParam(Parameter p) => p.Flags.HasFlag(SymbolFlags.IsByRef) && !IsArrayParam(p) && !IsStringParam(p) && p.DataType is UserDefinedType;
            // Callback (2026-09-29): un parametro `AS FUNCTION(...) AS T` / `AS SUB(...)`
            // viaggia come PUNTATORE nativo (l'indirizzo di un trampolino C vero, generato a
            // runtime da una famiglia pre-dichiarata — vedi gmb_vm_core.c) — stessa classe di
            // registri intera di array/STRING/BYREF, indipendentemente dal kind INTERNO del
            // callback. BYVAL solo. v1: firma del callback OMOGENEA per kind (niente kind
            // misto tra i suoi parametri, stessa semplificazione già usata per gli UDT prima
            // del supporto kind-misto) e arità 0-4 (non 0-8: il pool di trampolini
            // pre-dichiarati su VM/C cresce con arità×kind×slot — 8 è un limite pratica
            // documentato ed estendibile con lo stesso pattern, non un vincolo ABI). Vedi
            // AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §3.
            bool IsCallbackParam(Parameter p) => !IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && p.DataType is FunctionPointerType;
            // Callback VETTORIALE (2026-11-02): 1-2 parametri ARRAY di DOUBLE seguiti da 0-1
            // scalare DOUBLE, ritorno DOUBLE o ARRAY OF DOUBLE. È un'ABI in virgola mobile:
            // il nativo riceve (double* a0, int64_t n0[, double* a1, int64_t n1][, double s0])
            // e, per un ritorno ARRAY, un `double* out` finale. Elementi/ritorni interi NON
            // sono ammessi (nessun marshalling coerente tra i backend).
            bool IsVecElem(DataType? t) => t is DoubleType or InferredType or AnyType;
            bool IsVecCallback(FunctionPointerType fp) =>
                fp.ParameterTypes.Length >= 1
                && fp.ParameterTypes[0] is ArrayType
                && fp.ParameterTypes.TakeWhile(t => t is ArrayType).All(t => IsVecElem(((ArrayType)t).ElementType))
                && fp.ReturnType is not VoidType
                && (fp.ReturnType is ArrayType rat ? IsVecElem(rat.ElementType) : IsFfiDoubleType(fp.ReturnType))
                // array SOLO in testa (gli scalari vengono dopo): nessun array dopo il primo scalare
                && fp.ParameterTypes.SkipWhile(t => t is ArrayType).All(t => t is not ArrayType);
            int VecCallbackCode(FunctionPointerType fp)
            {
                int nArr = 0;
                while (nArr < fp.ParameterTypes.Length && fp.ParameterTypes[nArr] is ArrayType) nArr++;
                int nSca = fp.ParameterTypes.Length - nArr;
                bool arrRet = fp.ReturnType is ArrayType;
                return (nArr - 1) | (nSca << 2) | (arrRet ? 16 : 0);
            }
            /// <summary>Callback "solo scalari -> array" (2026-11-07): f(x[, y[, z]]) AS ARRAY OF
            /// DOUBLE. Non ha array in testa (non e' una IsVecCallback): descrittore 192+arità,
            /// ABI nativa `double (*)(double[, double[, double]], double* out)`.</summary>
            bool IsScalarArrayCallback(FunctionPointerType fp) =>
                fp.ParameterTypes.Length >= 1
                && fp.ParameterTypes.All(t => t is not ArrayType && IsFfiDoubleType(t))
                && fp.ReturnType is ArrayType sar && IsVecElem(sar.ElementType);
            int CallbackArityByte(FunctionPointerType fp) =>
                IsVecCallback(fp) ? 128 + VecCallbackCode(fp)
                : IsScalarArrayCallback(fp) ? 192 + fp.ParameterTypes.Length
                : fp.ParameterTypes.Length;
            void ValidateCallbackForFfi(FunctionPointerType fp, string what)
            {
                if (IsScalarArrayCallback(fp))
                {
                    if (fp.ParameterTypes.Length > 3)
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': il callback usato per {what} ha {fp.ParameterTypes.Length} parametri scalari con ritorno array (ammessi 1-3).");
                    return;
                }
                if (IsVecCallback(fp))
                {
                    int nArr = 0;
                    while (nArr < fp.ParameterTypes.Length && fp.ParameterTypes[nArr] is ArrayType) nArr++;
                    int nSca = fp.ParameterTypes.Length - nArr;
                    if (!((nArr == 1 && nSca == 0) || (nArr == 2 && nSca == 0) || (nArr == 1 && nSca == 1)))
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': forma di callback non supportata per {what} ({nArr} array, {nSca} scalari): ammesse: f(v()) -> scalare/array, f(a(), b()) -> scalare/array, f(v(), x) -> scalare/array.");
                    return;
                }
                if (fp.ParameterTypes.Any(t => t is ArrayType))
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': callback con array usato per {what} non supportato " +
                        "(ammessi solo array di DOUBLE, con ritorno DOUBLE o ARRAY OF DOUBLE).");
                if (fp.ParameterTypes.Length > 8)
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': il callback usato per {what} ha {fp.ParameterTypes.Length} parametri (ammessi 0-8).");
                var hasReturn = fp.ReturnType is not VoidType;
                var kinds = fp.ParameterTypes.Select(t => IsFfiInt64Type(t)).ToList();
                if (hasReturn) kinds.Add(IsFfiInt64Type(fp.ReturnType));
                foreach (var (pt, idx) in fp.ParameterTypes.Select((t, i) => (t, i)))
                    if (!IsFfiInt64Type(pt) && !IsFfiDoubleType(pt))
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': tipo del parametro {idx + 1} del callback usato per {what} non supportato (ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/...).");
                if (hasReturn && !IsFfiInt64Type(fp.ReturnType) && !IsFfiDoubleType(fp.ReturnType))
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': tipo di ritorno del callback usato per {what} non supportato (ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/..., o nessuno per un CALLBACK SUB).");
                if (kinds.Distinct().Count() > 1)
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': il callback usato per {what} ha una firma interna mista LONG/DOUBLE — non supportata nella v1 del supporto callback (tutti i parametri e l'eventuale ritorno dello stesso kind).");
            }
            // Kind del callback ai fini dell'omogeneità di firma e dell'emissione bytecode
            // (2026-09-29): il PRIMO parametro del callback se presente, altrimenti il suo
            // ritorno se presente — ValidateCallbackForFfi garantisce già l'omogeneità
            // dell'intera firma interna, quindi guardare solo il primo elemento è corretto. Un
            // callback arità-0/SUB (nessun parametro, nessun ritorno) non ha nessuno scalare da
            // marshallare: il kind non è mai consultato a runtime per quello slot, FfiDouble è
            // un default innocuo.
            bool CallbackIsInt64(FunctionPointerType fp) => IsVecCallback(fp)
                ? IsFfiInt64Type(fp.ReturnType)
                : fp.ParameterTypes.Length > 0
                    ? IsFfiInt64Type(fp.ParameterTypes[0])
                    : fp.ReturnType is not VoidType && IsFfiInt64Type(fp.ReturnType);
            foreach (var p in ffiProc.Parameters)
                if (IsCallbackParam(p)) ValidateCallbackForFfi((FunctionPointerType)p.DataType, $"il parametro '{p.Name.Lexeme}'");
            // Più callback nella stessa DECLARE: ammessi (le lib del monorepo ne usano 2, es.
            // METANSGA2 e LINESCALAR/LINEWORK/FLUX). Il pool dei backend C ha però 2 trampolini
            // per forma (kind, arità; codici vettoriali distinti): un terzo callback della stessa
            // forma fallisce a runtime su VM/C (pool esaurito) e su C99Native (errore dedicato).
            // Qui si rifiuta a compile-time con un messaggio unico sulle 4 gambe.
            {
                var cbShapeCount = new Dictionary<string, int>();
                foreach (var p in ffiProc.Parameters)
                {
                    if (!IsCallbackParam(p)) continue;
                    var fp = (FunctionPointerType)p.DataType;
                    string shape = IsVecCallback(fp) ? "v" + VecCallbackCode(fp)
                                 : IsScalarArrayCallback(fp) ? "a" + fp.ParameterTypes.Length
                                 : "s" + (CallbackIsInt64(fp) ? "i" : "d") + fp.ParameterTypes.Length;
                    cbShapeCount[shape] = cbShapeCount.TryGetValue(shape, out var cn) ? cn + 1 : 1;
                }
                foreach (var kv in cbShapeCount)
                    if (kv.Value > 2)
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': {kv.Value} callback con la stessa forma (kind, arità) nella stessa dichiarazione — i backend C hanno 2 trampolini per forma (massimo 2).");
            }
            // Struct/UDT per valore (2026-09-28): un UDT "semplice" (nessun operatore/
            // costruttore/metodo — quelli che emitter C# mappa già su uno struct C reale, non
            // su gmb_val_t opaco) con TUTTI i campi dello stesso kind a 8 byte (stessa
            // whitelist di array/BYREF) viaggia per valore. A differenza di array/STRING/BYREF
            // (SEMPRE puntatore = Int64), il kind di uno UDT è quello dei suoi campi — deve
            // combaciare con l'unico kind uniforme dell'intera dichiarazione (la regola "firma
            // omogenea" già esistente, semplicemente estesa): un UDT tutto-DOUBLE non può
            // coesistere con un array/STRING/BYREF (che forzano Int64) nella stessa
            // dichiarazione. Vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-
            // Callback.md §2.
            bool IsUdtParam(Parameter p) => !IsArrayParam(p) && !IsStringParam(p) && p.DataType is UserDefinedType && !IsCallbackParam(p);
            // Array di UDT (S3, 2026-11-06): elemento = TYPE semplice; il layout dell elemento
            // viaggia nello stesso modo del parametro UDT singolo (conteggio + size + tipo/offset).
            UserDefinedType? UdtArrayElem(Parameter p) => IsArrayParam(p) && (p.DataType as ArrayType)?.ElementType is UserDefinedType u ? u : null;
            bool IsSimpleUdt(UserDefinedType udt) => udt.TypeParameters.IsDefaultOrEmpty
                && udt.Operators.IsDefaultOrEmpty && udt.Constructors.IsDefaultOrEmpty && udt.Methods.IsDefaultOrEmpty;
            // Struct annidate (2026-09-29): un campo il cui tipo è A SUA VOLTA un TYPE semplice
            // (nesting) "espande" ricorsivamente nelle sue FOGLIE scalari — Path è il nome
            // punteggiato (es. "I.X") usato SIA per leggere l'istanza IN (dict PCodeVM annidato/
            // gmb_dict_t annidato VM/C, navigato splittando su '.') SIA come nome-costante nel
            // bytecode (nessun cambio di FORMATO: un campo diretto aveva già "nome+una seconda
            // costante per campo", ora "nome" può contenere punti). SymbolIdPath è la sequenza
            // di SymbolId di OGNI segmento del path (CSV, es. "5,9"), non solo della foglia
            // finale: OP_LOAD_MEMBER risolve un accesso a campo per OFFSET tramite
            // __class__.FieldNames dell'istanza, che per un livello intermedio ricostruito dal
            // marshalling FFI non esiste — serve invece il fallback "__field_<SymbolId>" (già
            // usato altrove nel motore, vedi PCodeProcessor.Builtins.cs) ad OGNI livello del
            // path, non solo sulla foglia, altrimenti `r.I` (il livello intermedio) non
            // risolverebbe prima di arrivare a `.X`.
            // ── FFI: tipi scalari e layout C reale degli UDT (S1, 2026-11-06) ──────────────
            // Codici tipo CONDIVISI col bytecode e la VM/C: 0=double 1=int64 2=int8 3=uint8
            // 4=int16 5=uint16 6=int32 7=uint32 8=float32 9=pointer/char* 10=uint64.
            // Larghezze CLASSICHE per i campi di uno struct FFI: INTEGER=16, LONG=32, QUAD=64,
            // BYTE=8, SINGLE=float32, DOUBLE=double64. È la larghezza DICHIARATA che conta per
            // rispecchiare un `struct` C (la rappresentazione interna del dialetto è 64-bit).
            static int FfiCode(DataType t) => t switch
            {
                DoubleType => 0, SingleType => 8, SByteType => 2, ByteType => 3,
                IntegerType => 4, UIntegerType => 5, LongType => 6, ULongType => 7,
                QuadType => 1, UQuadType => 10, StringType => 9,
                _ => -1,
            };
            static int FfiSz(int code) => code switch
            {
                0 or 1 or 9 or 10 => 8, 6 or 7 or 8 => 4, 4 or 5 => 2, _ => 1,
            };
            List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)> CollectFfiLeaves(
                UserDefinedType udt, string what, string pathPrefix, string symbolIdPrefix, out int size)
            {
                var leaves = new List<(string, string, int, int, bool)>();
                size = LayoutUdt(udt, what, pathPrefix, symbolIdPrefix, leaves);
                return leaves;
            }
            int LayoutUdt(UserDefinedType udt, string what, string pathPrefix, string symPrefix,
                          List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)> outLeaves)
            {
                var off = 0; var align = 1;
                foreach (var f in udt.Fields)
                {
                    var path = pathPrefix.Length == 0 ? f.Name.Lexeme : $"{pathPrefix}.{f.Name.Lexeme}";
                    var symPath = symPrefix.Length == 0 ? f.SymbolId.ToString() : $"{symPrefix},{f.SymbolId}";
                    if (f.DataType is ArrayType)
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': il campo ARRAY '{path}' non è supportato in un UDT passato per valore ({what}).");
                    // Campo ARRAY di uno UDT (S3b, 2026-11-06): N elementi contigui (`A(4)` = 5
                    // slot, come DIM). Diventano N foglie `A[k]` consecutive; il layout coincide
                    // con quello del membro C `T a[N]`.
                    if (!f.Dimensions.IsEmpty)
                    {
                        var codeArr = FfiCode(f.DataType);
                        // Solo elementi SCALARI numerici: un array di STRING sarebbe
                        // `char*[N]` nel layout C ma `gmb_string_t*` in BASIC (elemento non
                        // contiguo/per valore) — non riproducibile, meglio un errore esplicito.
                        if (codeArr == 9)
                            throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': il campo array '{path}' ha elementi STRING: non supportato ({what}).");
                        if (codeArr < 0)
                            throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': tipo del campo array '{path}' non supportato ({what}).");
                        var nArr = 1;
                        foreach (var d in f.Dimensions)
                        {
                            if (d is NumberExpr ne) nArr *= (int)ne.Value + 1;
                            else throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': il campo array '{path}' deve avere bound letterali ({what}).");
                        }
                        var szA = FfiSz(codeArr);
                        off = (off + szA - 1) / szA * szA;
                        for (var k = 0; k < nArr; k++)
                        {
                            outLeaves.Add(($"{path}[{k}]", symPath, codeArr, off, codeArr != 0 && codeArr != 8));
                            off += szA;
                        }
                        align = Math.Max(align, szA);
                        continue;
                    }
                    if (f.DataType is UserDefinedType sub)
                    {
                        if (!IsSimpleUdt(sub))
                            throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': lo UDT '{sub.Name.Lexeme}' (campo '{path}') usato per {what} non è ammesso dal marshalling FFI (solo TYPE semplici).");
                        var start = outLeaves.Count;
                        var subSize = LayoutUdt(sub, what, path, symPath, outLeaves);
                        var end = outLeaves.Count > start
                            ? outLeaves[^1].Off + FfiSz(outLeaves[^1].Code) : 0;
                        if (subSize != end)
                            throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} contiene la struttura annidata '{sub.Name.Lexeme}' con PADDING (size {subSize} vs {end}): il layout non è riproducibile — dichiara i campi nell'UDT esterno.");
                        var subAlign = 1;
                        foreach (var l in outLeaves.GetRange(start, outLeaves.Count - start))
                            subAlign = Math.Max(subAlign, FfiSz(l.Code));
                        off = (off + subAlign - 1) / subAlign * subAlign;
                        for (var k = start; k < outLeaves.Count; k++)
                        {
                            var l = outLeaves[k];
                            outLeaves[k] = (l.Path, l.SymbolIdPath, l.Code, l.Off + off, l.IsInt);
                        }
                        off += subSize;
                        align = Math.Max(align, subAlign);
                    }
                    else
                    {
                        var code = FfiCode(f.DataType);
                        if (code < 0)
                            throw new BasicException(expr.OriginalToken,
                                $"FFI '{symbolName}': tipo del campo '{path}' non supportato per {what} (UDT '{udt.Name.Lexeme}'; ammessi DOUBLE/SINGLE, interi a 1/2/4/8 byte, STRING).");
                        var sz = FfiSz(code);
                        off = (off + sz - 1) / sz * sz;
                        outLeaves.Add((path, symPath, code, off, code != 0 && code != 8));
                        off += sz;
                        align = Math.Max(align, sz);
                    }
                }
                return (off + align - 1) / align * align;
            }
            // IsMixedKindUdt/UdtKind sono chiamate SOLO dopo la validazione per lo stesso UDT.
            IEnumerable<bool> LeafKinds(UserDefinedType udt) => udt.Fields.SelectMany(f =>
                f.DataType is UserDefinedType nested ? LeafKinds(nested) : new[] { f.DataType is not DoubleType });
            bool IsMixedKindUdt(UserDefinedType udt) => LeafKinds(udt).Distinct().Count() > 1;
            List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)> ValidateUdtForFfi(UserDefinedType udt, string what)
            {
                if (!IsSimpleUdt(udt))
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} non è ammesso dal marshalling FFI (solo TYPE semplici, senza operatori/costruttori/metodi/generici).");
                var leaves = CollectFfiLeaves(udt, what, "", "", out _);
                if (leaves.Count is 0 or > 64)
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} ha {leaves.Count} campi-foglia (ammessi da 1 a 64).");
                return leaves;
            }
            byte UdtKind(UserDefinedType udt) => LeafKinds(udt).First() ? FfiInt64 : FfiDouble;
            var anyUdtParam = ffiProc.Parameters.Any(IsUdtParam);
            var udtReturn = ffiProc.IsFunction && ffiProc.ReturnType is UserDefinedType ? (UserDefinedType)ffiProc.ReturnType : null;
            // Kind misto come PARAMETRO (2026-09-29, era vietato — vedi AiWiki Bugs/(open) -
            // FFI-VMC-Dispatch-Non-Generico-Struct-Kind-Misto-Parametro.md): ammesso, ma AL
            // MASSIMO UNO per dichiarazione — VM/C lo passa come vero struct C-by-value alla
            // sua posizione (nessun flattening, nessuna assembly, lascia classificare l'ABI al
            // VERO compilatore C che compila gmb_vm_core.c, stessa filosofia del ritorno), ma
            // il dispatch generato copre un solo "slot strutturato" per chiamata (28 combinazioni
            // posizione × 2 ordini campo, non moltiplicabili per più UDT misti senza esplosione
            // combinatoria — vedi il bug doc per l'analisi completa delle opzioni scartate).
            // udtParamLeaves[pi] (2026-09-29, struct annidate): le foglie del parametro UDT alla
            // posizione pi, calcolate UNA VOLTA dalla validazione e riusate per l'emissione
            // bytecode sotto (evita ricalcolare/ridivergere la stessa enumerazione due volte).
            var udtParamLeaves = new List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)>?[FfiMaxArgs];
            for (var pi = 0; pi < ffiProc.Parameters.Length; pi++)
                if (IsUdtParam(ffiProc.Parameters[pi]))
                {
                    var leaves = ValidateUdtForFfi((UserDefinedType)ffiProc.Parameters[pi].DataType, $"il parametro '{ffiProc.Parameters[pi].Name.Lexeme}'");
                    if (pi < FfiMaxArgs) udtParamLeaves[pi] = leaves;
                }
            var udtReturnLeaves = udtReturn != null ? ValidateUdtForFfi(udtReturn, "il ritorno") : null;
            // S0 (2026-11-06): il limite "un solo parametro UDT a kind misto" era del dispatch
            // per-cast della VM/C; con libffi (slot per argomento) non serve più.
            // Kind misto sul RITORNO o su un PARAMETRO (2026-09-29): un UDT a kind misto non ha
            // un singolo "kind" da far combaciare col resto della firma — sia il dispatch del
            // ritorno sia quello del parametro misto selezionano la propria forma/posizione
            // indipendentemente (vedi ValidateUdtForFfi/gmb_vm_core.c). Vanno esclusi dal
            // controllo di omogeneità kindList sotto, altrimenti UdtKind(...) (che guarda solo
            // il PRIMO campo) rifiuterebbe spuriamente firme valide.
            var udtReturnMixed = udtReturn != null && IsMixedKindUdt(udtReturn);
            bool IsMixedUdtParam(Parameter p) => IsUdtParam(p) && IsMixedKindUdt((UserDefinedType)p.DataType);
            // Callback (2026-09-29, RIPRESO): un parametro callback viaggia SEMPRE come
            // puntatore nativo (l'indirizzo del trampolino) — stessa classe di registri intera
            // di array/STRING/BYREF, indipendentemente dal kind INTERNO del callback (per
            // questo qui è FfiInt64 fisso, non CallbackIsInt64 — quello serve solo per
            // l'omogeneità/emissione del TRAMPOLINO stesso, non per lo slot del puntatore).
            var paramKinds = ffiProc.Parameters.Where(p => !IsMixedUdtParam(p)).Select(p => IsArrayParam(p) || IsStringParam(p) || IsByRefScalarParam(p) || IsCallbackParam(p)
                ? FfiInt64
                : IsUdtParam(p) ? UdtKind((UserDefinedType)p.DataType)
                : ClassifyFfiType(p.DataType, $"il parametro '{p.Name.Lexeme}'")).ToArray();
            var retIsString = ffiProc.ReturnType is StringType;
            var retKind = retIsString
                ? FfiInt64
                : udtReturn != null && !udtReturnMixed ? UdtKind(udtReturn)
                : udtReturnMixed ? FfiInt64 /* placeholder, escluso da kindList sotto */
                : ffiProc.IsFunction ? ClassifyFfiType(ffiProc.ReturnType, "il ritorno") : FfiDouble;
            var anyStringByRef = ffiProc.Parameters.Any(p => p.Flags.HasFlag(SymbolFlags.IsByRef) && IsStringParam(p));
            var anyArray = ffiProc.Parameters.Any(IsArrayParam);
            var anyStringParam = ffiProc.Parameters.Any(IsStringParam);
            var anyByRefScalar = ffiProc.Parameters.Any(IsByRefScalarParam);
            var anyCallbackParam = ffiProc.Parameters.Any(IsCallbackParam);
            var anyPtr = anyArray || anyStringParam || retIsString || anyByRefScalar || anyCallbackParam;
            if (anyStringByRef)
                throw new BasicException(expr.OriginalToken,
                    $"FFI '{symbolName}': BYREF di STRING non supportato dal marshalling FFI (passare BYVAL).");
            // Limite slot nativi TOTALI (2026-09-28, struct/UDT): un parametro UDT si "espande"
            // in N slot nativi (uno per campo, valutati per valore) invece di uno solo — la
            // VM/C chiama al massimo 8 slot totali (a0..a7), non 8 parametri LOGICI. Un singolo
            // parametro UDT a 8 campi esaurisce da solo il limite.
            var flattenedSlotCount = ffiProc.Parameters.Sum(p => IsUdtParam(p) ? ((UserDefinedType)p.DataType).Fields.Length : 1);
            if (flattenedSlotCount > FfiMaxArgs)
                throw new BasicException(expr.OriginalToken,
                    $"FFI '{symbolName}': troppi slot nativi risultanti dall'espansione degli UDT per valore ({flattenedSlotCount}, massimo {FfiMaxArgs} — ogni campo UDT conta come uno slot).");
            // Tipo elemento dell'array O tipo di un parametro BYREF scalare: SOLO DOUBLE o
            // famiglia intera A 8 BYTE (LONG/QUAD/ULONG/UQUAD) — NON tutta la famiglia intera
            // scalare (fix 2026-09-28: INTEGER/BYTE/SBYTE/UINTEGER/SINGLE mappano a CLR
            // byte[]/short[]/float[]/... con larghezza-elemento NATIVA più stretta di 8 byte,
            // ma il puntatore FFI viaggia sempre come gmb_int_t*/double* a 8 byte su tutti i
            // backend — pinnare un array più stretto direttamente produce un mismatch di
            // larghezza silenzioso (letto come garbage dal nativo). Per gli scalari BYVAL non è
            // un problema — marshallano sempre a int64_t indipendentemente dalla larghezza
            // dichiarata — ma un ARRAY o un BYREF sono un buffer/indirizzo condiviso per
            // riferimento: la larghezza reale conta.
            /* BYTE (2026-11-06): ammesso come elemento-array — NON viene pinnato direttamente
             * (byte[] e' largo 1 byte, il puntatore FFI viaggia a 8): il marshalling costruisce
             * uno scratch a 8 byte per elemento e fa copy-in/copy-back con i valori 0..255.
             * Vedi FfiService.CallFunctionTypedArr (case byte[]/sbyte[]), gmb_vm_core.c
             * (array_elem_int_mask) e gmb_c99_emit.c (n_intarr). */
            bool IsFfiArrayElemType(DataType? t) => t is DoubleType or LongType or QuadType or ULongType or UQuadType or ByteType or SByteType;
            // BYREF scalare: solo tipi a 8 byte. Il puntatore punta a una cella di 8 byte: un
            // BYTE/SBYTE produrrebbe lo stesso mismatch di larghezza silenzioso degli array, e
            // per questo IL e C99Native lo rifiutano già. BYTE/SBYTE restano ammessi come
            // ELEMENTO di un array (scratch a 8 byte per elemento), non come BYREF.
            bool IsFfiByRefScalarType(DataType? t) => t is DoubleType or LongType or QuadType or ULongType or UQuadType;
            foreach (var p in ffiProc.Parameters)
            {
                if (IsArrayParam(p))
                {
                    var elem = (p.DataType as ArrayType)?.ElementType;
                    if (UdtArrayElem(p) is { } ue)
                    {
                        var lvTmp = new List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)>();
                        LayoutUdt(ue, $"il parametro '{p.Name.Lexeme}'", "", "", lvTmp);
                        if (lvTmp.Count is 0 or > 64)
                            throw new BasicException(expr.OriginalToken, $"FFI '{symbolName}': array di UDT '{ue.Name.Lexeme}' con {lvTmp.Count} campi (ammessi 1..64).");
                    }
                    else if (!IsFfiArrayElemType(elem) && elem is not StringType)
                        throw new BasicException(expr.OriginalToken,
                            $"FFI '{symbolName}': tipo elemento non supportato per il parametro array '{p.Name.Lexeme}' (solo AS DOUBLE, LONG/QUAD, BYTE, STRING o un TYPE).");
                }
                else if (IsByRefScalarParam(p) && !IsFfiByRefScalarType(p.DataType))
                    throw new BasicException(expr.OriginalToken,
                        $"FFI '{symbolName}': tipo non supportato per il parametro BYREF '{p.Name.Lexeme}' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).");
            }
            // Limite arità (parità 4 backend: la VM/C chiama al massimo 8 argomenti; senza guardia
            // una dichiarazione con >8 argomenti funzionava su PCode/IL/C99 ma tornava 0 su VM/C).
            if (ffiProc.Parameters.Length > FfiMaxArgs)
                throw new BasicException(expr.OriginalToken,
                    $"FFI '{symbolName}': massimo {FfiMaxArgs} argomenti supportati dal marshalling FFI.");
            // Firma OMOGENEA: parametri E ritorno dello stesso kind. Il ritorno va incluso
            // (solo per FUNCTION: per SUB non esiste) — altrimenti `(LONG) AS DOUBLE` passava
            // come all-double e il marshalling leggeva registri sbagliati (divergenza vs le
            // gambe C, che mappano i tipi per-parametro). Fix parità 2026-09-22.
            var anyUdt = anyUdtParam || udtReturn != null || ffiProc.Parameters.Any(p => UdtArrayElem(p) != null);
            var kindList = ffiProc.IsFunction && !udtReturnMixed ? paramKinds.Append(retKind) : paramKinds;
            // Firma mista LONG/DOUBLE fra SCALARI SEMPLICI (2026-10, v1): ammessa SOLO quando
            // non c'è nessun array/STRING/BYREF/callback/UDT nella firma — quelli continuano a
            // imporre un kind unico per un motivo indipendente (il puntatore viaggia come
            // intero, o lo UDT ha un dispatch nativo kind-specifico). Con solo scalari
            // semplici, ogni parametro/ritorno usa il proprio kind individuale (paramKinds/
            // retKind sopra già lo calcolano per-posizione) — nessun vincolo di omogeneità.
            // Vedi AiWiki Bugs/(solved) - FFI-Firma-Mista-Scalari-Semplici.md.
            var anyHardPtr = anyArray || anyByRefScalar || anyCallbackParam || retIsString;  /* STRING-param ammessa col misto */
        var mixedScalarsOnly = !anyHardPtr && !anyUdt && kindList.Distinct().Count() > 1;
            // 2026-11-02: firma MISTA ammessa in presenza di puntatori (array/STRING/BYREF),
            // NON con callback: il trampolino callback + slot misti resta instabile (hang
            // osservato su IL/C99) → per i callback vale ancora la firma omogenea, con errore
            // di compilazione esplicito invece di un crash.
            var mixedKinds = kindList.Distinct().Count() > 1;
            var mixedWithPointers = anyHardPtr && !anyUdt && mixedKinds;
            // UDT (2026-11-06, S1): con il layout esplicito ogni argomento ha il proprio tipo,
            // quindi la firma puo essere "mista" anche con UDT (prima era vietato).
            if (!mixedScalarsOnly && !mixedWithPointers && !anyUdt && mixedKinds)
                throw new BasicException(expr.OriginalToken,
                    anyPtr
                        ? $"FFI '{symbolName}': un parametro array, STRING o BYREF richiede firma INT64 (dichiarare gli altri parametri/il ritorno AS LONG — il puntatore viaggia come intero)."
                        : $"FFI '{symbolName}': firma mista LONG/DOUBLE non supportata dal marshalling FFI (tutti i parametri e il ritorno AS LONG oppure AS DOUBLE).");
            // SUB (ffiProc.ReturnType == null): non c'è un ritorno da includere nel kind — il
            // kind lo decidono solo i parametri (prima di questo fix un SUB con soli parametri
            // LONG/array finiva comunque su OP_CALL_FFI perché retKind defaultava a FfiDouble
            // per l'assenza di ritorno, corrompendo un eventuale parametro-puntatore).
            var allInt64 = ffiProc.IsFunction
                ? retKind == FfiInt64 && paramKinds.All(k => k == FfiInt64)
                : paramKinds.All(k => k == FfiInt64);
            // Instradamento su OP_CALL_FFI_T (2026-09-28, struct/UDT): prima serviva SOLO
            // "tutto Int64" (allInt64) — un UDT tutto-DOUBLE (nessun array/STRING/BYREF a
            // forzare Int64) ha bisogno comunque del path tipizzato per i suoi campi/slot, pur
            // restando kind Double. kindList è già garantito uniforme dal check sopra: il kind
            // da emettere è quello condiviso da tutti gli elementi (irrilevante quale, sono
            // tutti uguali) — FfiInt64 come fallback quando kindList è vuoto (non raggiunto se
            // useTypedPath è vero, perché un array/STRING/BYREF/UDT implica sempre ≥1 elemento).
            // Firma mista (2026-10): mai omogenea, serve SEMPRE il path tipizzato (OP_CALL_FFI
            // legacy è all-double, incompatibile con un kind Int64 anche per un solo slot).
            var useTypedPath = allInt64 || anyUdt || mixedScalarsOnly || mixedWithPointers;
            // emittedKind ('kind' nel bytecode) = kind del RITORNO per una FUNCTION (coincide
            // sempre col vecchio kindList.First() nel caso omogeneo, perché lì TUTTI gli
            // elementi di kindList sono garantiti uguali); per una SUB non c'è ritorno, resta
            // "il kind del primo parametro" come comportamento storico (nessun bit di 'kind' è
            // consultato per il ritorno di una SUB).
            // udtReturnMixed (2026-09-29): retKind è un PLACEHOLDER (FfiInt64, escluso da
            // kindList sopra) — non rappresenta un kind reale, usarlo qui darebbe un 'kind'
            // sbagliato (bug 2026-10 trovato testando LabeledND: primo campo intero, tutti i
            // parametri DOUBLE — emittedKind diventava 1, VM/C instradava i parametri DOUBLE
            // nel ramo kind=1/nativeArgs che li tronca a int64 prima della chiamata nativa). In
            // quel caso 'kind' deve seguire SOLO i parametri (stesso trattamento di una SUB).
            var emittedKind = ffiProc.IsFunction && !udtReturnMixed ? retKind : paramKinds.Length > 0 ? paramKinds[0] : FfiInt64;
            // scalarKindMask (2026-10, firma mista): bit i = il parametro SCALARE SEMPLICE alla
            // posizione i è Int64 — significativo SOLO per le posizioni non-array/STRING/
            // BYREF/callback/UDT (il runtime ignora il bit altrove, quelle posizioni sono già
            // classificate dalle rispettive maschere/campi dedicati). Nel caso omogeneo legacy
            // ogni bit coincide esattamente con 'emittedKind' — zero cambio di comportamento
            // per i programmi esistenti (letto ma mai divergente da 'kind' quando la firma è
            // già omogenea).
            long scalarKindMask = 0;
            for (var pi = 0; pi < ffiProc.Parameters.Length && pi < FfiMaxArgs; pi++)
            {
                var p = ffiProc.Parameters[pi];
                var isPlainScalar = !IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && !IsCallbackParam(p) && !IsUdtParam(p);
                // Puntatori (array/STRING/BYREF/callback) = Int64; UDT resta 'emittedKind'
                // (il suo dispatch è per-shape e il bit non deve creare un falso "misto").
                var bitIsInt = isPlainScalar ? ClassifyFfiType(p.DataType, $"il parametro '{p.Name.Lexeme}'") == FfiInt64
                              : IsUdtParam(p) ? emittedKind == FfiInt64
                              : true;
                if (bitIsInt) scalarKindMask |= 1L << pi;
            }
            if (useTypedPath)
            {
                EmitByte((byte)OpCode.OP_CALL_FFI_T);
                EmitShort(libIndex);
                EmitShort(nameIndex);
                EmitShort((ushort)expr.Args.Length);
                /* S1 (2026-11-06): bit7 di `kind` = ritorno UDT con LAYOUT ESPLICITO; i due byte
                 * successivi diventano il conteggio foglie u16 (al posto di count u8 + mask u8). */
                EmitByte((byte)(emittedKind | (udtReturnLeaves != null ? 0x80 : 0) | (ffiProc.IsNativeAbi ? 0x20 : 0)));   // 0x20 = DECLARE NATIVE (ABI esatta, niente estensioni array)
                EmitByte((byte)(retIsString ? 1 : 0));
                var retLeafCount = (ushort)(udtReturnLeaves?.Count ?? 0);
                EmitByte((byte)(retLeafCount & 0xFF));
                EmitByte((byte)((retLeafCount >> 8) & 0xFF));
                // Descrittore PER ARGOMENTO (2026-11-06): niente maschere a bit; 3 byte per
                // argomento (flags, aux1, aux2) — vedi cc_emit_ffi_call in gmb_compiler.c.
                for (var pi = 0; pi < expr.Args.Length; pi++)
                {
                    var p = ffiProc.Parameters[pi];
                    var isArr = IsArrayParam(p);
                    var isStr = !isArr && IsStringParam(p);
                    var isBr = !isArr && !isStr && IsByRefScalarParam(p);
                    var isBrUdt = !isArr && !isStr && IsByRefUdtParam(p);
                    var isCb = !isArr && !isStr && !isBr && IsCallbackParam(p);
                    var isUd = !isArr && !isStr && !isBr && !isCb && IsUdtParam(p);
                    int flags = 0;
                    var intClass = isArr || isStr || isBr || isCb
                        ? true
                        : isUd ? emittedKind == FfiInt64 : ClassifyFfiType(p.DataType, $"il parametro '{p.Name.Lexeme}'") == FfiInt64;
                    if (intClass) flags |= 1;
                    if (isArr)
                    {
                        flags |= 2;
                        var elemT = (p.DataType as ArrayType)?.ElementType;
                        if (elemT is UserDefinedType) flags |= 0x80;      /* array di struct */
                        else if (IsFfiInt64Type(elemT)) flags |= 0x40;
                        else if (elemT is StringType) flags |= 4;         /* array di STRING (char**, solo INPUT) */
                    }
                    else if (isStr) flags |= 4;
                    else if (isBr) { flags |= 8; if (IsFfiInt64Type(p.DataType)) flags |= 0x80; }
                    else if (isUd) { flags |= 0x20 | 0x80; if (isBrUdt) flags |= 8; }   /* 0x80 = layout nuovo, 8 = BYREF */
                    else if (isCb) flags |= 0x10;
                    EmitByte((byte)flags);
                    byte a1 = 0, a2 = 0;
                    if (isUd)
                    {
                        var leaves = udtParamLeaves[pi];
                        var cnt = (ushort)(leaves?.Count ?? 0);
                        a1 = (byte)(cnt & 0xFF);
                        a2 = (byte)((cnt >> 8) & 0xFF);
                    }
                    else if (isArr && UdtArrayElem(p) is { } aeCnt)
                    {
                        var lvC = new List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)>();
                        LayoutUdt(aeCnt, $"il parametro '{p.Name.Lexeme}'", "", "", lvC);
                        var cnt = (ushort)lvC.Count;
                        a1 = (byte)(cnt & 0xFF);
                        a2 = (byte)((cnt >> 8) & 0xFF);
                    }
                    else if (isCb)
                    {
                        var fp = (FunctionPointerType)p.DataType;
                        a1 = (byte)CallbackArityByte(fp);
                        if (CallbackIsInt64(fp)) a2 |= 1;
                        if (fp.ReturnType is not VoidType) a2 |= 2;
                    }
                    EmitByte(a1);
                    EmitByte(a2);
                }
                // Path foglia + SymbolIdPath (2026-09-28, struct/UDT; esteso al nesting
                // 2026-09-29): DUE u16 PER FOGLIA (indice costante del path-nomi, indice
                // costante del path-SymbolId), in ordine di dichiarazione, appesi in coda —
                // prima i parametri UDT (posizione 0..7, solo quelli con
                // udtParamFieldCount>0), poi il ritorno UDT (se presente). STESSO formato di
                // quando il secondo u16 era il SymbolId diretto: ora è l'indice di una COSTANTE
                // STRINGA CSV coi SymbolId di OGNI segmento del path (es. "5,9" per "I.X"), non
                // solo della foglia — un livello intermedio ricostruito dal marshalling FFI non
                // ha un __class__/FieldNames "vero", quindi la ricostruzione del ritorno (vedi
                // FfiService.cs) usa il fallback "__field_<SymbolId>" (già usato altrove nel
                // motore) ad OGNI livello, non solo sulla foglia finale — altrimenti `r.I` (il
                // livello intermedio) non risolverebbe prima di arrivare a `.X`. Il path-nomi
                // serve invece a LEGGERE un'istanza UDT PCodeVM in ingresso (Dictionary
                // <string,object> chiavato per nome, navigato ricorsivamente splittando su '.').
                for (var pi = 0; pi < ffiProc.Parameters.Length && pi < FfiMaxArgs; pi++)
                {
                    if (udtParamLeaves[pi] is { } leaves)
                    {
                        foreach (var leaf in leaves)
                        {
                            EmitShort(AddConstant(leaf.Path));
                            EmitShort(AddConstant(leaf.SymbolIdPath));
                        }
                        CollectFfiLeaves((UserDefinedType)ffiProc.Parameters[pi].DataType, "il parametro", "", "", out var psz);
                        EmitShort((ushort)psz);
                        foreach (var leaf in leaves)
                        {
                            EmitByte((byte)leaf.Code);
                            EmitShort((ushort)leaf.Off);
                        }
                        continue;
                    }
                    // Array di UDT (S3, 2026-11-06): stesso blocco del parametro UDT, riferito
                    // al TIPO DELL'ELEMENTO (il puntatore punta a N struct contigue).
                    if (UdtArrayElem(ffiProc.Parameters[pi]) is { } ae)
                    {
                        var alv = new List<(string Path, string SymbolIdPath, int Code, int Off, bool IsInt)>();
                        var asz = LayoutUdt(ae, $"il parametro '{ffiProc.Parameters[pi].Name.Lexeme}'", "", "", alv);
                        foreach (var leaf in alv)
                        {
                            EmitShort(AddConstant(leaf.Path));
                            EmitShort(AddConstant(leaf.SymbolIdPath));
                        }
                        EmitShort((ushort)asz);
                        foreach (var leaf in alv)
                        {
                            EmitByte((byte)leaf.Code);
                            EmitShort((ushort)leaf.Off);
                        }
                    }
                }
                if (udtReturnLeaves != null)
                {
                    foreach (var leaf in udtReturnLeaves)
                    {
                        EmitShort(AddConstant(leaf.Path));
                        EmitShort(AddConstant(leaf.SymbolIdPath));
                    }
                    CollectFfiLeaves(udtReturn!, "il ritorno", "", "", out var rsz);
                    EmitShort((ushort)rsz);
                    foreach (var leaf in udtReturnLeaves)
                    {
                        EmitByte((byte)leaf.Code);
                        EmitShort((ushort)leaf.Off);
                    }
                }
            }
            else
            {
                EmitByte((byte)OpCode.OP_CALL_FFI);
                EmitShort(libIndex);
                EmitShort(nameIndex);
                EmitByte((byte)expr.Args.Length);
            }
        }
        else
        {
            EmitConstant((double)expr.Args.Length);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }
    }
    /// <summary>Il tipo FFI dichiarato viaggia come int64 nativo (LONG/INTEGER/BYTE/... —
    /// lo stesso mapping di C99TypeMapper: interi → gmb_int_t = int64_t).</summary>
    private static bool IsFfiInt64Type(DataType? t) => t is IntegerType or UIntegerType or LongType
        or ULongType or QuadType or UQuadType or ByteType or SByteType or EnumType;

    private void VisitGoFunctionExpression(GoFunctionExpr expr)
    {
        // Compila tutti gli argomenti e li mette sulla stack.
        foreach (var arg in expr.Args)
        {
            VisitExpression(arg);
        }

        // Compila il WaitGroup, se presente.
        if (expr.WaitGroup is not null)
        {
            VisitExpression(expr.WaitGroup);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // OP_GO_FUNCTION (non OP_GO): a differenza di GO SUB, GO FUNCTION è
        // un'espressione (compilata come "X = GoFunctionExpr" da ParseGoStatement) e DEVE
        // lasciare esattamente un valore sulla stack — il future/handle del risultato — perché
        // il chiamante (VisitLetStatement) si aspetta di poppare un valore da assegnare a X.
        var goLocation = EmitJump((byte)OpCode.OP_GO_FUNCTION);
        // 2026-09-24 (overload): nessuna ResolvedDefinition sul nodo (solo SymbolId) —
        // patch con arità del call-site (overload esatti; optional/variadic → fallback nome).
        _jumpsToPatch.Add((goLocation, CallPatchToken(expr.OriginalToken, null, expr.Args.Length)));
    }

    /// <summary>
    /// Compila un'espressione letterale di un array (es. {1, 2, 3}).
    /// </summary>
    private void VisitArrayLiteralExpression(ArrayLiteralExpr expr)
    {
        // 1. Compila ricorsivamente ogni espressione elemento dell'array.
        //    I loro valori verranno lasciati sulla stack in ordine.
        foreach (var element in expr.Elements)
        {
            VisitExpression(element);
        }

        // 2. Emette l'istruzione per creare l'array, seguita dal numero
        //    di elementi da prelevare (pop) dalla stack.
        EmitByte((byte)OpCode.OP_MK_ARRAY);
        EmitShort(expr.Elements.Length);
    }

    /// <summary>
    /// Compila un'espressione letterale di un dizionario (es. {"a": 1, "b": 2}).
    /// </summary>
    private void VisitDictionaryLiteralExpression(DictionaryLiteralExpr expr)
    {
        // Use MK_DICT (original approach) — it's simpler and the stack issue is in constant emission
        foreach (var entry in expr.Entries)
        {
            VisitExpression(entry.Key);
            VisitExpression(entry.Value);
        }
        EmitByte((byte)OpCode.OP_MK_DICT);
        EmitShort(expr.Entries.Length);
    }

    /// <summary>
    /// Compila un'espressione letterale di una lista (es. [1, 2, 3]).
    /// </summary>
    private void VisitListLiteralExpression(ListLiteralExpr expr)
    {
        foreach (var element in expr.Elements)
        {
            VisitExpression(element);
        }

        EmitByte((byte)OpCode.OP_MK_LIST);
        EmitShort(expr.Elements.Length);
    }

    /// <summary>
    /// Compila un'espressione NEW per istanziare un nuovo oggetto, gestendo classi,
    /// tipi built-in e collezioni con inizializzatori.
    /// </summary>
    private void VisitNewExpression(NewExpr expr)
    {
        // Caso 1: Istanziazione di una CLASSE
        if (expr.ParsedType is ClassType classType)
        {
            if (!_compiledClassDescriptors.TryGetValue(classType.Definition.Name.Lexeme + "#" + classType.Definition.SymbolId, out var classDescriptor))
            {
                throw new Exception(
                    $"Compiler error: Class descriptor for '{classType.Definition.Name.Lexeme}' not found.");
            }

            var classDescriptorIndex = AddConstant(classDescriptor);

            // Registra per patching post-compilazione
            _classDescriptorsToPatch.Add((classDescriptorIndex, classType.Definition));

            // Gli argomenti del costruttore, se presenti, vengono pushati PRIMA dell'opcode
            // cosi' la VM puo' consumarli direttamente dallo stack.
            if (expr.ConstructorArgs.HasValue)
            {
                foreach (var arg in expr.ConstructorArgs.Value)
                {
                    VisitExpression(arg);
                }
                EmitConstant((double)expr.ConstructorArgs.Value.Length);
            }
            else
            {
                EmitConstant(0.0); // 0 argomenti
            }

            EmitByte((byte)OpCode.OP_NEW_CLASS_INSTANCE);
            EmitShort(classDescriptorIndex);
        }
        // Caso 2: Istanziazione di un UDT (con o SENZA costruttore esplicito). La forma
        // senza parentesi `NEW TipoUdt` è l'idioma classico dei TYPE: deve comunque produrre
        // un'istanza zero-inizializzata via OP_NEW_UDT_INSTANCE. Prima cadeva nel Caso 3
        // (tipi built-in) con typeCode 0 → VM/C la trattava come una LISTA e PCode come NIL.
        else if (expr.ParsedType is UserDefinedType udt)
        {
            if (!_compiledClassDescriptors.TryGetValue(udt.Name.Lexeme + "#" + udt.SymbolId, out var udtDescriptor))
            {
                throw new Exception($"Compiler error: UDT descriptor for '{udt.Name.Lexeme}' not found.");
            }

            var udtDescriptorIndex = AddConstant(udtDescriptor);

            // Registra per patching post-compilazione (VTable, Constructors, etc.)
            _classDescriptorsToPatch.Add((udtDescriptorIndex, udt));

            // Gli argomenti del costruttore, se presenti, vengono pushati PRIMA dell'opcode
            // cosi' la VM puo' consumarli direttamente dallo stack.
            if (expr.ConstructorArgs.HasValue)
            {
                foreach (var arg in expr.ConstructorArgs.Value)
                {
                    VisitExpression(arg);
                }
                EmitConstant((double)expr.ConstructorArgs.Value.Length);
            }
            else
            {
                EmitConstant(0.0); // 0 argomenti
            }

            EmitByte((byte)OpCode.OP_NEW_UDT_INSTANCE);
            EmitShort(udtDescriptorIndex);
        }
        else // Caso 3: Tipi built-in (collezioni, HTTPOPTIONS, etc.)
        {
            // Mappa il DataType a un TypeCode numerico per OP_NEW.
            var typeCode = expr.ParsedType switch
            {
                // ATTENZIONE: 0 e' anche il fallback generico per qualunque tipo built-in non
                // ancora mappato esplicitamente qui sotto (vedi "_ when ... EndsWith(Type) => 0"),
                // quindi HTTPOPTIONS non puo' condividerlo — altrimenti OP_NEW (PCodeProcessor.cs)
                // non potrebbe distinguere "NEW HTTPOPTIONS" da un qualunque altro "NEW TipoIgnoto"
                // che ricade sullo stesso fallback, e li tratterebbe entrambi come HTTPOPTIONS (o
                // entrambi come null, a seconda di quale branch OP_NEW implementasse). Serve un
                // codice dedicato.
                HttpOptionsType => 22,
                // NEW HTTPCONFIG: stesso motivo di HTTPOPTIONS sopra — senza un codice dedicato
                // ricade sul fallback generico (0, che OP_NEW mappa a null), quindi
                // "DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG" produrrebbe cfg = null e ogni
                // cfg.Port = ... successivo sarebbe un no-op silenzioso (bug scoperto verificando
                // Http/HttpFullServer.bas e Http/HttpStatic.bas: HTTP SERVER cfg.Port finiva per
                // usare sempre la porta di default 8080 invece di quella configurata).
                HttpConfigType => 23,
                // NEW JSON: stesso bug di HTTPOPTIONS/HTTPCONFIG sopra — senza un codice dedicato
                // "DIM x AS JSON = NEW JSON" produce x = null (fallback generico 0), quindi
                // x.campo = valore e' un no-op silenzioso e res.SendJSON(x) serializza il
                // letterale JSON "null" invece dell'oggetto atteso (bug scoperto verificando
                // Http/HttpServer.bas: il body di GET /data risultava il testo "null").
                JsonType => 24,
                // NEW COOKIEJAR: a differenza di HTTPOPTIONS/HTTPCONFIG/JSON sopra, non e'
                // duck-typed (nessun campo assegnabile da BASIC, solo il metodo Clear()) — OP_NEW
                // istanzia direttamente un vero CommonLib.Http.CookieJar, stesso pattern "oggetto
                // C# reale" gia' usato per PCodeFetchResult/PCodeHttpResponse (vedi OP_LOAD_FIELD/
                // OP_CALL_MEMBER), cosi' l'istanza puo' essere passata direttamente a
                // ConvertPCodeHttpOptions -> HttpOptions.CookieJar senza conversioni.
                CookieJarType => 25,
                ListType => 1,
                DictType => 2,
                HashSetType => 3,
                QueueType => 4,
                StackType => 5,
                SortedDictType => 6,
                TreeType => 7,
                AVLTreeType => 8,
                RedBlackTreeType => 9,
                BinaryIndexedTreeType => 10,
                TrieType => 11,
                LinkedListType => 12,
                DoublyLinkedListType => 12,   // stessa LinkedList<T> del P-Code (vedi OP_NEW 12)
                CircularLinkedListType => 13,
                BinaryHeapType { IsMaxHeap: true } => 27,
                BinaryHeapType => 14,
                BagType => 15,
                HyperLogLogType => 30,   // 28 = STRINGBUILDER, 29 = REGEX in PCode/VM-C
                CacheType => 16,
                GraphType => 17,
                TimelineType => 18,
                UnresolvedQualifiedType => 0,
                HashRingType => 19,
                RingBufferType => 20,
                SparseSetType => 21,
                BPlusTreeType => 26,
                StringBuilderType => 28,
                _ when expr.ParsedType.GetType().Name.EndsWith("Type") => 0,
                _ => throw new NotImplementedException(
                    $"NEW not implemented for built-in type {expr.ParsedType.GetType().Name}")
            };

            // Costruttori dei tipi built-in con argomenti (es. NEW RINGBUFFER<STRING>(3),
            // NEW SPARSESET(1000, 10), NEW BPLUSTREE(3), NEW HASHRING(Servers)):
            // il pattern è lo stesso delle classi — args pushati + count, poi OP_NEW,
            // cosicché la VM possa consumarli (vedi OP_NEW in gmb_vm_core.c).
            // PRIMA questi argomenti venivano SILENZIOSAMENTE SCARTATI: il bytecode
            // conteneva solo OP_NEW typeCode e la struttura nasceva coi default C# (es.
            // CircularQueue(64)) invece della capacità richiesta (verificato 2026-08-18:
            // NEW RINGBUFFER(3) creava capacity 64/8, non 3; la CommonLib C# vuole
            // CircularQueue(int capacity) che sovrascrive quando pieno).
            var builtinCtorArgs = expr.ConstructorArgs?.ToArray() ?? [];
            var isBitreeArrayCtor = expr.ParsedType is BinaryIndexedTreeType &&
                                    builtinCtorArgs.Length == 1;
            if (builtinCtorArgs.Length > 0 && !isBitreeArrayCtor)
            {
                foreach (var arg in builtinCtorArgs)
                    VisitExpression(arg);
                EmitConstant((double)builtinCtorArgs.Length);
            }
            else
            {
                EmitConstant(0.0); // 0 argomenti costruttore
            }

            EmitByte((byte)OpCode.OP_NEW);
            EmitShort(typeCode);

            // Se ci sono valori iniziali (es. NEW LIST OF INTEGER {1, 2, 3}),
            // li compiliamo e usiamo OP_INIT_ARRAY per popolare la collezione.
            if (expr.InitialValues is not null && expr.InitialValues.Value.Any())
            {
                var literal = expr.InitialValues.Value;
                foreach (var element in literal)
                {
                    VisitExpression(element);
                }

                EmitByte((byte)OpCode.OP_INIT_ARRAY); // Riutilizziamo questo opcode per le collezioni
                EmitShort(literal.Length);
            }
            // NEW BITREE<T>([...]): l'array è un ARGOMENTO (ConstructorArgs), non InitialValues.
            // BinaryIndexedTree prende il source ARRAY nel costruttore — si compilano gli
            // elementi e si usa OP_INIT_ARRAY (che ricrea il BIT col source, vedi VM).
            else if (expr.ParsedType is BinaryIndexedTreeType &&
                     expr.ConstructorArgs is { Length: 1 } cargs)
            {
                // NOTA: il parser mette `NEW BITREE([1,2,3,4,5])` come ListLiteralExpr,
                // non ArrayLiteralExpr (visto col debug BITDBGALL). Entrambi hanno Elements.
                ImmutableArray<Expr> bitElems = cargs[0] switch
                {
                    ArrayLiteralExpr al => al.Elements,
                    ListLiteralExpr ll => ll.Elements,
                    _ => default
                };
                if (!bitElems.IsDefault && bitElems.Any())
                {
                    foreach (var element in bitElems) VisitExpression(element);
                    EmitByte((byte)OpCode.OP_INIT_ARRAY);
                    EmitShort(bitElems.Length);
                }
            }
        }
    }

    /// <summary>
    /// Compila una chiamata alla funzione speciale FETCH.
    /// </summary>
    private void VisitFetchExpression(HttpFetchExpr expr)
    {
        VisitExpression(expr.Url);

        if (expr.Options is not null)
        {
            VisitExpression(expr.Options);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Placeholder per le opzioni
        }

        byte variant = 0; // 0 = Sincrono
        if (expr.CallbackName is not null)
        {
            variant = 1; // 1 = Asincrono con Callback
            // L'indirizzo della callback viene pushato come costante.
            var callbackAddress = AddConstant(null); // Placeholder
            _constantsToPatch.Add((callbackAddress, expr.CallbackName));
            EmitBytes((byte)OpCode.OP_PUSH_CONST);
            EmitShort(callbackAddress);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Placeholder per la callback
        }

        EmitBytes((byte)OpCode.OP_FETCH, variant);
    }

    /// <summary>
    /// Compila l'accesso a una proprietà di un'istanza (es. MyObject.MyProperty).
    /// </summary>
    private void VisitPropertyAccessExpression(PropertyAccessExpr expr)
    {
        VisitExpression(expr.Instance);

        // Se la proprietà ha un getter definito, chiama il getter via VTable.
        // Per le proprietà senza getter, accede direttamente al campo.
        if (expr.ResolvedDefinition is { Getter: not null })
        {
            EmitConstant(0.0); // Nessun argomento
            var getterIndex = AddConstant(expr.PropertyName.Lexeme);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(getterIndex);
        }
        else
        {
            var propertyNameIndex = AddConstant(expr.PropertyName.Lexeme);
            EmitBytes((byte)OpCode.OP_LOAD_FIELD);
            EmitShort(propertyNameIndex);
        }
    }

    /// <summary>
    /// Compila una chiamata a un metodo su un'istanza (es. MyObject.MyMethod(arg)).
    /// </summary>
    private void VisitMethodCallExpression(MethodCallExpr expr)
    {
        /* TRYGETVALUE(key, var) su contenitore chiave→valore: il P-Code non ha out-param
         * (gli argomenti sono VALORI, niente l-value) → riscrittura equivalente
         * `var = instance.Get(key); instance.ContainsKey(key)`. Prima la variabile out
         * restava invariata (CollectionsModern: "Trovato key1: " senza valore). */
        if ((expr.MethodName.Kind == TokenKind.CollTryGetValue
             || string.Equals(expr.MethodName.Lexeme, "TryGetValue", StringComparison.OrdinalIgnoreCase))
            && expr.Args.Length == 2 && expr.Args[1] is VariableExpr tgvDest
            && expr.Instance is VariableExpr && !string.IsNullOrEmpty(tgvDest.NameCleaned.Lexeme))
        {
            /* Predicato PRIMA (resta sullo stack come valore dell'espressione), poi
             * assegnazione della variabile out SOLO se il valore esiste. Il P-Code non
             * ha out-param: gli argomenti sono VALORI, quindi la write-back va emessa
             * qui (prima la variabile restava invariata). */
            VisitExpression(expr.Instance);
            VisitExpression(expr.Args[0]);
            EmitConstant(1.0);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(AddConstant("ContainsKey"));
            EmitByte((byte)OpCode.OP_DUP);
            var tgvSkip = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            VisitExpression(expr.Instance);
            VisitExpression(expr.Args[0]);
            EmitConstant(1.0);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(AddConstant("Get"));
            var tgvSlot = ResolveLocal(tgvDest.NameCleaned.Lexeme);
            if (tgvSlot != -1)
            {
                EmitAdjustStringForLocal(tgvSlot);
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)tgvSlot);
            }
            else if (TryResolveOrRegisterGlobal(tgvDest.NameCleaned.Lexeme, out var tgvGlobal))
            {
                EmitAdjustStringForGlobal(tgvGlobal);
                EmitByte((byte)OpCode.OP_STORE_GLOBAL); EmitShort((ushort)tgvGlobal);
            }
            else
            {
                EmitByte((byte)OpCode.OP_POP);
            }
            PatchJump(tgvSkip);
            return;
        }
        VisitExpression(expr.Instance);
        foreach (var arg in expr.Args) { VisitExpression(arg); }
        // If ResolvedDefinition is a base class method, emit direct call via OP_CALL_CTOR
        if (expr.ResolvedDefinition is { } def && expr.Instance is BaseExpr)
        {
            EmitConstant((double)(expr.Args.Length + 1)); // +1 for SELF with OP_CALL_CTOR
            var procName = def.Parent is ClassDefinition cd ? $"{cd.Name.Lexeme}.{def.OriginalName.Lexeme}" : def.OriginalName.Lexeme;
            var callLocation = EmitJump((byte)OpCode.OP_CALL_CTOR);
            _jumpsToPatch.Add((callLocation, new Token(TokenKind.Identifier, procName, null, def.OriginalName.Position)));
        }
        else
        {
            EmitConstant((double)expr.Args.Length);
            // F1e (regola d'oro): il chunk trasporta il NOME CANONICO derivato dal
            // TokenKind — .count()/.Count()/.COUNT() emettono tutti "Count" — una sola
            // forma stabile su cui VM/C (strcmp) e PCode dispatchano. I membri fuori
            // tabella (HTTP/WS/SQL/metodi utente) mantengono il lexeme.
            var canonName = CollMembers.Canonical(expr.MethodName.Kind) ?? expr.MethodName.Lexeme;
            var methodNameIndex = AddConstant(canonName);
            EmitBytes((byte)OpCode.OP_CALL_MEMBER);
            EmitShort(methodNameIndex);
        }
    }

    /// <summary>
    /// Compila un'espressione lambda. Questa è la logica centrale per le closure.
    /// Crea un compilatore figlio per compilare il corpo della funzione, cattura le
    /// variabili (upvalues) e genera l'opcode OP_CLOSURE.
    /// </summary>
    private void VisitLambdaExpression(LambdaExpr expr)
    {
        // 1. Inizia un nuovo compilatore "figlio" per compilare il corpo in modo isolato,
        //    passando `this` per abilitare la ricerca di upvalues.
        var functionCompiler = new Compiler(this);
        CompiledChunk functionChunk;
        // Byte aggiunti in testa al chunk del figlio (prologo STORE_LOCAL+CLEAR_RETURN),
        // da sommare agli offset dei jumps già registrati dal figlio PRIMA dell'insert:
        // altrimenti il patch finale finisce dentro il prologo stesso, corrompendolo.
        var prologueShift = 0;

        // 2. Compila il corpo della lambda a seconda che sia un blocco di istruzioni o una singola espressione.
        if (expr.BlockBody != null)
        {
            functionChunk = functionCompiler.CompileProcedureBody(expr.Parameters, expr.BlockBody.Value);
            // Inserisci STORE_LOCAL per i parametri + CLEAR_RETURN all'inizio del chunk
            // (i parametri lambda sono locals[0..N-1] perché non c'è return variable).
            // NB: OP_STORE_LOCAL legge lo slot come U16 big-endian (ReadU16 hi/lo) — un
            // singolo byte per slot (come era) disallineava lo stream: il primo STORE_LOCAL
            // consumava il byte successivo come hi dello slot (slot=284+), i parametri non
            // venivano mai legati e i locali restavano vuoti (SOLVEODE { |t,y| RETURN -y }
            // tornava -t invece di -y; Simpson a brace = 0). EmitShort scrive (hi, lo).
            var popCodes = new System.Collections.Generic.List<byte>();
            for (int pi = expr.Parameters.Length - 1; pi >= 0; pi--)
            {
                popCodes.Add((byte)OpCode.OP_STORE_LOCAL);
                popCodes.Add((byte)((pi >> 8) & 0xFF));
                popCodes.Add((byte)(pi & 0xFF));
            }
            popCodes.Add((byte)OpCode.OP_CLEAR_RETURN);
            popCodes.Add((byte)OpCode.OP_CLEAR_RETURN);
            functionChunk.Code.InsertRange(0, popCodes.ToArray());
            prologueShift = popCodes.Count;
        }
        else if (expr.ExpressionBody != null)
        {
            functionChunk = functionCompiler.CompileExpressionBody(expr.Parameters, expr.ExpressionBody);
        }
        else
        {
            throw new Exception("Lambda expression has neither a block body nor an expression body.");
        }

        // 3. Emetti JUMP_FORWARD per saltare il corpo della lambda.
        //    Il corpo della funzione non deve essere eseguito come codice normale;
        //    deve essere referenziato solo dall'istruzione OP_CLOSURE.
        var skipJumpPos = EmitJump((byte)OpCode.OP_JUMP_FORWARD);

        // 4. Aggiunge il P-Code e le costanti generate dal compilatore figlio al chunk principale.
        var functionAddress = _chunk.Code.Count;
        _chunk.Code.AddRange(functionChunk.Code);
        _chunk.Lines.AddRange(functionChunk.Lines);
        // S_eval: propaga lo snapshot locali del figlio traslato all'indirizzo reale.
        _chunk.MergeChildLocalNames(functionChunk, functionAddress, prologueShift);
        foreach (var constant in functionChunk.Constants)
        {
            AddConstant(constant);
        }

        // Propaga i jumps non ancora patchati dal figlio al genitore.
        foreach (var (childOffset, target) in functionCompiler.GetJumpsToPatch())
        {
            _jumpsToPatch.Add((functionAddress + prologueShift + childOffset, target));
        }

        // Riloca gli indirizzi-closure del figlio (anche annidati): + base del chunk nel
        // genitore (+ prologo). Alla radice si scrive nel pool; nei livelli intermedi si
        // propaga (l'accumulo avviene a ogni merge).
        foreach (var (fixIdx, fixAddr) in functionCompiler.GetPendingClosureFixes())
        {
            var relocated = fixAddr + functionAddress + prologueShift;
            if (_enclosing == null) _chunk.Constants[fixIdx] = (double)relocated;
            else _pendingClosureFixes.Add((fixIdx, relocated));
        }

        // 5. Patching del salto: dopo il corpo, pointerà qui.
        var afterFunc = _chunk.Code.Count;
        var jumpOffset = afterFunc - (skipJumpPos + 2);
        _chunk.Code[skipJumpPos] = (byte)((jumpOffset >> 8) & 0xFF);
        _chunk.Code[skipJumpPos + 1] = (byte)(jumpOffset & 0xFF);
        
        // 6. Emette l'istruzione OP_CLOSURE per creare la closure a runtime.
        EmitByte((byte)OpCode.OP_CLOSURE);
        var constIndex = AddConstant((double)functionAddress);
        EmitShort(constIndex);
        // In un compilatore FIGLIO l'indirizzo è relativo al proprio chunk: registralo
        // per la rilocazione al merge (closure annidate).
        if (_enclosing != null) _pendingClosureFixes.Add((constIndex, functionAddress));

        // 7. Emette le informazioni sulle upvalues catturate dal compilatore figlio.
        var upvalues = functionCompiler.GetUpvalues();
        EmitByte((byte)upvalues.Count);
        foreach (var upvalue in upvalues)
        {
            EmitByte(upvalue.Kind);
            EmitByte((byte)(upvalue.Index));
        }
    }

    /// <summary>
    /// Compila un'espressione macro (&...).
    /// </summary>
    private void VisitMacroExpression(MacroExpr expr)
    {
        // 1. Compila l'espressione che produce la stringa di codice.
        VisitExpression(expr.Expression);
        // 2. Emette l'opcode che istruisce la VM a compilare ed eseguire la stringa.
        EmitByte((byte)OpCode.OP_EVAL_MACRO_EXPR);
        // S_eval: cattura le locali vive al sito (incluse quelle di blocco IF/FOR).
        RegisterMacroSiteLocalNames();
    }

    /// <summary>
    /// EVAL(target [, args...]): raggiunge il compilatore SOLO per i testi macro compilati a
    /// RUNTIME (Parser.ParseSingleExpression + CompileExpressionBody, senza il Linker che di
    /// solito riscrive EvalExpr in MacroExpr/FunctionExpr). Semantica:
    /// <list type="bullet">
    ///   <item>senza argomenti → macro-eval del testo (identico a <see cref="MacroExpr"/>,
    ///   es. EVAL annidato `EVAL("EVAL(""g"") + 1")`);</item>
    ///   <item>con argomenti → invocazione dinamica del block (target ClosureData) via
    ///   OP_CALL_INDIRECT (PCode/VM-C la gestiscono nativamente).</item>
    /// </list>
    /// </summary>
    private void VisitEvalExpression(EvalExpr expr)
    {
        if (expr.Args.IsDefaultOrEmpty)
        {
            // EVAL(block) con block INLINE (lambda) o target che RITORNA un block
            // (es. EVAL(Mk0())) a 0 argomenti → INVOCAZIONE, non macro di stringa: prima
            // OP_EVAL_MACRO_EXPR trattava il valore funzione come testo (vuoto su PCode,
            // 0 su VM/C; IL/C99* già corretti via EmitInlineLambdaEval).
            if (expr.Target is LambdaExpr
                || (expr.Target is FunctionExpr fe0
                    && (fe0.ReturnType is FunctionPointerType
                        || (fe0.ResolvedDefinition as FunctionDefinition)?.ReturnType is FunctionPointerType)))
            {
                VisitExpression(expr.Target);
                EmitByte((byte)OpCode.OP_CALL_INDIRECT);
                EmitByte(0);
                return;
            }
            VisitExpression(expr.Target);
            EmitByte((byte)OpCode.OP_EVAL_MACRO_EXPR);
            RegisterMacroSiteLocalNames();
            return;
        }

        for (var i = 0; i < expr.Args.Length; i++)
            VisitExpression(expr.Args[i]);
        VisitExpression(expr.Target);
        EmitByte((byte)OpCode.OP_CALL_INDIRECT);
        EmitByte((byte)expr.Args.Length);
    }

    /// <summary>
    /// Compila un'istruzione ExpressionStmt. Valuta l'espressione e scarta il suo risultato.
    /// Tipicamente usata per chiamate a funzioni o metodi il cui valore di ritorno è ignorato.
    /// </summary>
    /// <param name="stmt">L'istruzione ExpressionStmt da compilare.</param>
    private void VisitExpressionStatement(ExpressionStmt stmt)
    {
        VisitExpression(stmt.Expression); // Compila l'espressione
        EmitByte((byte)OpCode.OP_POP); // Scarta il risultato
    }

/// <summary>
    /// Compila un'espressione di accesso a un campo/proprietà (es. `obj.Membro`).
    /// Questa versione utilizza il `FieldSymbolId` risolto dal Linker per generare
    /// P-Code efficiente basato su ID, anziché su nomi.
    /// </summary>
    private void VisitFieldAccessExpression(FieldAccessExpr expr)
    {
        // Campo STATIC di CLASS (condiviso da tutte le istanze, vedi OP_LOAD_STATIC_FIELD):
        // niente istanza da compilare/pushare, sia che l'accesso sia SELF.Campo (Structure = SELF,
        // scartata) sia NomeClasse.Campo (Structure = riferimento alla classe stessa, non
        // valutabile come espressione — vedi la stessa scoperta fatta lato GPM in
        // Gpm.Assigns.cs::ExecuteLetStep).
        if (expr.FieldSymbolId >= 0 && _staticFieldSymbolIds.Contains(expr.FieldSymbolId))
        {
            EmitByte((byte)OpCode.OP_LOAD_STATIC_FIELD);
            EmitShort((ushort)expr.FieldSymbolId);
            return;
        }

        // 1. Compila l'espressione base, che lascerà l'istanza dell'oggetto (UDT, classe, etc.)
        // sulla cima dello stack della VM.
        VisitExpression(expr.Structure);

        // 2. Determina la strategia di accesso.
        if (expr.FieldSymbolId >= 0)
        {
            // --- STRATEGIA A: ACCESSO ID-BASED (Performante) ---
            // Il Linker ha risolto il campo e ci ha fornito un ID univoco.
            // Questo è il caso per campi di UDT e Classi.
            // Emettiamo un opcode che usa l'ID per un accesso diretto (es. in un dizionario o array).
            EmitByte((byte)OpCode.OP_LOAD_MEMBER);
            EmitShort((ushort)expr.FieldSymbolId);
        }
        else
        {
            // --- STRATEGIA B: ACCESSO NAME-BASED (Fallback) ---
            // Il Linker non ha potuto risolvere un ID (FieldSymbolId è -1).
            // Questo accade per i tipi built-in (es. HttpOptions.Method) o per i campi di
            // database in contesti dinamici, dove i membri sono noti solo a runtime.
            // Emettiamo un opcode che usa il nome del campo per un accesso dinamico.
            var fieldNameIndex = AddConstant(expr.Field.Lexeme);
            EmitByte((byte)OpCode.OP_LOAD_FIELD);
            EmitShort(fieldNameIndex);
        }
    }

    /// <summary>
    /// MakeByRefExpr (sintetico, Linker-only): emette OP_MAKE_BYREF/OP_MAKE_BYREF_GLOBAL sullo
    /// slot del simbolo — stessa emissione già usata per un argomento BYREF a un call-site
    /// (vedi sopra in questo file), generalizzata a "prendi il box di QUESTO simbolo" come
    /// valore d'espressione qualunque. Usato SOLO da PerformHeapPromotion (Linker.Rewrites.cs)
    /// per inizializzare il campo box di una __ClosureCtx con IL BOX GIÀ RICEVUTO dal
    /// parametro BYREF (mai una copia del valore — OP_MAKE_BYREF su uno slot che contiene GIÀ
    /// un box ne crea uno che indirizza a quello stesso slot, quindi condivide comunque
    /// l'identità/scrittura verso il chiamante, solo con un livello di indirezione in più —
    /// per questo OP_LOAD_MEMBER/OP_STORE_MEMBER devono "srotolare" un'eventuale CATENA di box,
    /// non solo un livello — vedi PCodeProcessor.cs).
    /// </summary>
    private void VisitMakeByRefExpression(MakeByRefExpr expr)
    {
        var name = NormalizeIdentifierName(expr.OriginalToken.Lexeme);
        var localIdx = ResolveLocal(name);
        if (localIdx >= 0)
        {
            EmitByte((byte)OpCode.OP_MAKE_BYREF);
            EmitShort((ushort)localIdx);
            return;
        }
        if (_globals.TryGetValue(name, out var gIdx))
        {
            EmitByte((byte)OpCode.OP_MAKE_BYREF_GLOBAL);
            EmitShort((ushort)gIdx);
            return;
        }
        // Caso non atteso nello scenario base (il simbolo è un parametro/locale DIRETTO della
        // procedura che sta compilando questa inizializzazione): se il simbolo è già un
        // upvalue (closure annidata che ricattura un BYREF di un antenato), il box è già
        // disponibile via OP_LOAD_UPVALUE SENZA auto-unwrap qui — ma OP_LOAD_UPVALUE lo
        // dereferenzia sempre (fix esistente), quindi questo ramo resta un limite noto per le
        // closure annidate (vedi TODO §3.3) — fallback difensivo, non un crash.
        var upvalueIdx = ResolveUpvalueByName(name);
        if (upvalueIdx != -1) { EmitBytes((byte)OpCode.OP_LOAD_UPVALUE, (byte)upvalueIdx); return; }
        EmitByte((byte)OpCode.OP_PUSH_NIL);
    }

    /// <summary>
    /// Compila un accesso null-conditional (<c>expr?.membro</c>, <c>expr?.(args)</c>,
    /// <c>expr?[idx]</c>). A differenza di <see cref="FieldAccessExpr"/>, il Linker non
    /// riscrive/risolve mai questo nodo (nessun <c>FieldSymbolId</c> disponibile) — resta
    /// così fino al compilatore. Non serve però alcun controllo esplicito di NIL emesso qui:
    /// gli opcode di lettura sottostanti (OP_LOAD_FIELD, OP_LOAD_ELEMENT, OP_CALL_INDIRECT)
    /// sono già tutti "null-safe" per costruzione — quando l'oggetto in cima allo stack non
    /// corrisponde a nessun tipo gestito (incluso <c>null</c>), spingono semplicemente
    /// <c>NIL</c> invece di lanciare un'eccezione. Basta quindi compilare l'accesso
    /// ESATTAMENTE come la sua controparte non-null-conditional (VisitFieldAccessExpression /
    /// VisitIndexedAccessExpression / CALL_INDIRECT) — la sicurezza sul NIL arriva gratis
    /// dal comportamento già esistente della VM.
    /// </summary>
    private void VisitNullConditionalAccessExpression(NullConditionalAccessExpr expr)
    {
        switch (expr.Kind)
        {
            case NullConditionalKind.Member:
                VisitExpression(expr.Expression);
                if (expr.MemberToken != null)
                {
                    var fieldNameIndex = AddConstant(expr.MemberToken.Lexeme);
                    EmitByte((byte)OpCode.OP_LOAD_FIELD);
                    EmitShort(fieldNameIndex);
                }
                break;
            case NullConditionalKind.Index:
            {
                var indices = expr.Arguments ?? ImmutableArray<Expr>.Empty;
                /* ARRAY NATIVO: OP_LOAD_ELEMENT lancia ERR 9 fuori dai limiti (canone, parità
                 * IL/VM-C/C99). Per l'accesso SICURO `arr(i)?` serve quindi un guard esplicito
                 * (0 <= i <= UBOUND), altrimenti anche il `?` esplodeva con "Subscript out of
                 * range" (sample Collections/CollectionsSafeAccess). Liste/dict/stringhe sono
                 * già null-safe nell'opcode: nessun guard. */
                if (indices.Length == 1 && expr.Expression is VariableExpr bve
                    && SymbolDataType(bve) is ArrayType)
                {
                    EmitSafeArrayIndex(bve, indices[0]);
                    break;
                }
                // Stessa convenzione di VisitIndexedAccessExpression: base, poi indici.
                VisitExpression(expr.Expression);
                foreach (var index in indices)
                    VisitExpression(index);
                EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, (byte)indices.Length);
                break;
            }
            case NullConditionalKind.Call:
            {
                // expr?.(args) — expr stesso è il valore invocabile (funzione/delegato).
                // OP_CALL_INDIRECT vuole gli argomenti PRIMA del target (il target è
                // l'ultimo pop, in cima allo stack) — ordine OPPOSTO a Member/Index, quindi
                // qui expr.Expression va compilato PER ULTIMO, non per primo.
                var args = expr.Arguments ?? ImmutableArray<Expr>.Empty;
                foreach (var arg in args)
                    VisitExpression(arg);
                VisitExpression(expr.Expression);
                EmitBytes((byte)OpCode.OP_CALL_INDIRECT, (byte)args.Length);
                break;
            }
        }
    }

    /// <summary>
    /// `arr(i)?` su un ARRAY NATIVO: valuta base e indice UNA volta in due local temporanee,
    /// parte da NIL e legge l'elemento solo se `0 <= i <= UBOUND(arr)`. Evita l'ERR 9
    /// dell'OP_LOAD_ELEMENT (che resta il comportamento dell'accesso NON sicuro) senza
    /// introdurre nuovi opcode/builtin: usa solo confronti, salti e UBOUND.
    /// </summary>
    private void EmitSafeArrayIndex(VariableExpr baseExpr, Expr indexExpr)
    {
        VisitExpression(baseExpr);
        EmitSafeArrayIndexFromStack(indexExpr);
    }

    /// <summary>Come <see cref="EmitSafeArrayIndex"/>, ma con la base GIÀ sullo stack.</summary>
    private void EmitSafeArrayIndexFromStack(Expr indexExpr)
    {
        AddLocal(new Token(TokenKind.Identifier, $"__safeb{_locals.Count}", null, SourcePosition.Unknown));
        var baseSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)baseSlot);

        VisitExpression(indexExpr);
        AddLocal(new Token(TokenKind.Identifier, $"__safei{_locals.Count}", null, SourcePosition.Unknown));
        var idxSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)idxSlot);

        EmitByte((byte)OpCode.OP_PUSH_NIL);
        AddLocal(new Token(TokenKind.Identifier, $"__safer{_locals.Count}", null, SourcePosition.Unknown));
        var resSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)resSlot);

        var endLabel = GenerateCompilerLabel("SAFEIDX_END");
        // i < 0 → NIL
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)idxSlot);
        EmitConstant(0.0);
        EmitByte((byte)OpCode.OP_CMP_LT);
        var jLow = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((jLow, endLabel));
        // i > UBOUND(base) → NIL
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)idxSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)baseSlot);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.UBOUND);
        EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_CMP_GT);
        var jHigh = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((jHigh, endLabel));
        // res = base[i]
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)baseSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)idxSlot);
        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, 1);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)resSlot);

        _labels[endLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resSlot);
    }

/// <summary>
    /// Compila un'espressione di comprensione di lista (es. [x*2 FOR x IN nums IF x > 5]
    /// o [x+y FOR x IN listA FOR y IN listB]) in cicli P-Code efficienti.
    /// Utilizza la funzione built-in AADD per aggiungere elementi all'array risultato.
    /// </summary>
    private void VisitListComprehensionExpression(ListComprehensionExpr expr)
    {
        // 1. Inizia un nuovo scope per le variabili di supporto della comprensione.
        BeginScope();

        // 2. Crea la lista risultato (mutable) e la salva in una variabile locale.
        // Usa MK_LIST invece di MK_ARRAY perché List supporta Add() via AADD.
        EmitByte((byte)OpCode.OP_MK_LIST);
        EmitShort(0); // Dimensione iniziale 0
        AddLocal(new Token(TokenKind.Identifier, "__result", null, SourcePosition.Unknown));
        var resultSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)resultSlot);

        // 3. Inizia la catena di cicli annidati.
        // La funzione ricorsiva si occuperà di creare gli scope per ogni iteratore.
        CompileNestedClause(expr.Clauses, 0, expr.OutputExpression, resultSlot);

        // 3b. ORDER BY: sort the result list if ORDER BY clauses exist.
        foreach (var orderBy in expr.OrderByClauses)
        {
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resultSlot);
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)BuiltInFunction.SORT);
            EmitByte((byte)(1));
            EmitByte((byte)OpCode.OP_POP);
            // DESC: SORT è ascendente → REVERSE (clone) come ultimo passo. Prima `DESC` era
            // ignorato (PCode/VM-C davano l'ordine ascendente anche per ORDER BY ... DESC).
            if (orderBy.Descending)
            {
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resultSlot);
                EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                EmitShort((ushort)BuiltInFunction.REVERSE);
                EmitByte((byte)(1));
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)resultSlot);
            }
        }

        // 4. Carica l'array risultato finale sullo stack.
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resultSlot);

        // 5. Pulisce lo scope principale della comprensione.
        EndScope();
    }

/// <summary>
    /// Funzione ricorsiva helper per compilare le clausole annidate di una comprensione di lista.
    /// Ogni chiamata a questo metodo genera un ciclo FOR...NEXT per una singola clausola.
    /// </summary>
    private void CompileNestedClause(
        ImmutableArray<ComprehensionClause> clauses,
        int clauseIndex,
        Expr outputExpression,
        int resultSlot)
    {
        // Condizione di terminazione: se abbiamo compilato tutte le clausole,
        // siamo nel ciclo più interno. È ora di valutare l'output e aggiungerlo all'array.
        if (clauseIndex >= clauses.Length)
        {
            // A questo punto, tutte le condizioni IF dei cicli esterni sono già state verificate.
            // Dobbiamo solo valutare l'espressione di output.
            
            // Prepara la chiamata a AADD(__result, <valore>).
            EmitByte((byte)OpCode.OP_PUSH_REF_LOCAL); EmitShort((ushort)resultSlot); // Arg 1: Riferimento all'array
            VisitExpression(outputExpression);                         // Arg 2: Valore da aggiungere
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)BuiltInFunction.AADD);
            EmitByte((byte)(2));
            EmitByte((byte)OpCode.OP_POP); // Pulisce il NIL restituito da AADD.
            return;
        }

        var clause = clauses[clauseIndex];
        
        // --- INIZIO COMPILAZIONE CICLO FOR PER LA CLAUSOLA CORRENTE ---
        
        BeginScope();

        VisitExpression(clause.SourceExpression);
        AddLocal(new Token(TokenKind.Identifier, $"__source{clauseIndex}", null, SourcePosition.Unknown));
        var sourceSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)sourceSlot);
        
        EmitConstant(0.0);
        AddLocal(new Token(TokenKind.Identifier, $"__index{clauseIndex}", null, SourcePosition.Unknown));
        var indexSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);
        
        var loopStartLabel = GenerateCompilerLabel($"COMP_START_{clauseIndex}");
        var loopExitLabel = GenerateCompilerLabel($"COMP_EXIT_{clauseIndex}");
        var continueLabel = GenerateCompilerLabel($"COMP_CONT_{clauseIndex}");

        // --- INIZIO CICLO ---
        _labels[loopStartLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;

        // Condizione di uscita: IF __index > UBOUND(__source) GOTO loopExit
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.UBOUND);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_CMP_GT);
        var jumpToEnd = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((jumpToEnd, loopExitLabel));

        // --- CORPO DEL CICLO ---

        // Assegna l'elemento corrente alla variabile iteratore.
        BeginScope();
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        // OP_LOAD_ELEMENT_POSITIONAL: indexSlot è un contatore di iterazione (0,1,2,...), non una
        // chiave reale — stesso motivo di VisitForEachAdvanceAndAssignStatement (Compiler.Others.cs).
        // Porting da GMBasicAI (commit 701da5e).
        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT_POSITIONAL, 1);
        AddLocal(clause.IteratorVariable);
        if (_locals.Count > 0) EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
        
        // *** CORREZIONE: La condizione IF viene controllata qui, prima di scendere nel ciclo successivo ***
        if (clause.ConditionExpression != null)
        {
            VisitExpression(clause.ConditionExpression);
            // Se la condizione è FALSA, salta direttamente alla fine del corpo di questo ciclo (all'incremento).
            var jumpToContinue = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            _jumpsToPatch.Add((jumpToContinue, continueLabel));
        }

        // Chiamata Ricorsiva: Compila la prossima clausola (il ciclo annidato).
        CompileNestedClause(clauses, clauseIndex + 1, outputExpression, resultSlot);

        // Etichetta di destinazione per il salto della condizione IF.
        _labels[continueLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;

        // Pulisce lo scope dell'iteratore di questa clausola.
        EndScope(); 

        // Incrementa l'indice di questo ciclo.
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitConstant(1.0);
        EmitByte((byte)OpCode.OP_ADD);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);

        // Salta all'inizio del ciclo di questo livello.
        EmitLoop(_labels[loopStartLabel.Lexeme]);

        // --- FINE CICLO ---
        _labels[loopExitLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;

        // Pulisce lo scope di supporto di questo livello.
        EndScope(); 
    }

    /// <summary>
    /// Compila un'espressione generatore <c>(expr FOR var IN source [IF cond])</c> in un oggetto
    /// lazy costruito da 3 closure (sorgente/condizione/output), consumabile sia da FOR EACH
    /// (via IEnumerable, vedi OP_FOR_EACH_ADVANCE) sia da NEXT(). Le closure catturano lo scope
    /// esterno esattamente come una LAMBDA (stesso meccanismo di VisitLambdaExpression) — la
    /// valutazione della sorgente NON avviene qui: resta lazy fino al primo consumo, come in GPM
    /// (Gpm.Generators.cs::ExecuteGeneratorExpressionStep).
    /// </summary>
    private void VisitGeneratorExpression(GeneratorExpr expr)
    {
        var clause = expr.Clauses.Length > 0 ? expr.Clauses[0] : null;
        if (clause is null)
        {
            // Nessuna clausola: caso non valido a livello di sintassi, nessun generatore da
            // costruire. Comportamento precedente come fallback difensivo.
            VisitExpression(expr.OutputExpression);
            return;
        }

        // Closure sorgente: 0 argomenti, valuta clause.SourceExpression catturando lo scope esterno.
        EmitClosureForExpr(ImmutableArray<Parameter>.Empty, clause.SourceExpression);

        // Closure condizione (opzionale): 1 argomento posizionato sullo stesso SymbolId
        // dell'iteratore, cosi' i riferimenti a `x` dentro la condizione risolvono al parametro
        // invece che a un upvalue. NIL se la clausola non ha IF.
        if (clause.ConditionExpression != null)
        {
            var condParam = new Parameter(clause.IteratorSymbolId, clause.IteratorVariable, new InferredType(),
                SymbolFlags.None, null, false);
            EmitClosureForExpr(ImmutableArray.Create(condParam), clause.ConditionExpression);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Closure output: 1 argomento, stesso SymbolId dell'iteratore.
        var outParam = new Parameter(clause.IteratorSymbolId, clause.IteratorVariable, new InferredType(),
            SymbolFlags.None, null, false);
        EmitClosureForExpr(ImmutableArray.Create(outParam), expr.OutputExpression);

        EmitByte((byte)OpCode.OP_NEW_GENERATOR_EXPR);
    }

    /// <summary>
    /// Compila <paramref name="body"/> come corpo di una closure a espressione singola (stessa
    /// tecnica di VisitLambdaExpression per il ramo ExpressionBody: compilatore figlio isolato,
    /// salto del corpo, OP_CLOSURE con upvalue catturate) e la lascia sulla stack. Estratto come
    /// helper separato invece di riusare VisitLambdaExpression perché qui i parametri sono
    /// sintetici (creati da un SymbolId di iteratore già esistente, non da una vera LambdaExpr).
    /// </summary>
    private void EmitClosureForExpr(ImmutableArray<Parameter> parameters, Expr body)
    {
        var functionCompiler = new Compiler(this);
        var functionChunk = functionCompiler.CompileExpressionBody(parameters, body);

        var skipJumpPos = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        var functionAddress = _chunk.Code.Count;
        _chunk.Code.AddRange(functionChunk.Code);
        _chunk.Lines.AddRange(functionChunk.Lines);
        foreach (var constant in functionChunk.Constants)
        {
            AddConstant(constant);
        }

        foreach (var (childOffset, target) in functionCompiler.GetJumpsToPatch())
        {
            _jumpsToPatch.Add((functionAddress + childOffset, target));
        }
        foreach (var (fixIdx, fixAddr) in functionCompiler.GetPendingClosureFixes())
        {
            var relocated = fixAddr + functionAddress;
            if (_enclosing == null) _chunk.Constants[fixIdx] = (double)relocated;
            else _pendingClosureFixes.Add((fixIdx, relocated));
        }

        var afterFunc = _chunk.Code.Count;
        var jumpOffset = afterFunc - (skipJumpPos + 2);
        _chunk.Code[skipJumpPos] = (byte)((jumpOffset >> 8) & 0xFF);
        _chunk.Code[skipJumpPos + 1] = (byte)(jumpOffset & 0xFF);

        EmitByte((byte)OpCode.OP_CLOSURE);
        var constIndex = AddConstant((double)functionAddress);
        EmitShort(constIndex);
        if (_enclosing != null) _pendingClosureFixes.Add((constIndex, functionAddress));

        var upvalues = functionCompiler.GetUpvalues();
        EmitByte((byte)upvalues.Count);
        foreach (var upvalue in upvalues)
        {
            EmitByte(upvalue.Kind);
            EmitByte((byte)(upvalue.Index));
        }
    }

    /// <summary>
    /// Compila un'espressione di risoluzione di un FutureValue.
    /// </summary>
    private void VisitResolveExpression(ResolveExpr expr)
    {
        VisitExpression(expr.FutureToResolve);
        EmitByte((byte)OpCode.OP_RESOLVE_FUTURE);
    }

    /// <summary>
    /// Compila un riferimento a una funzione.
    /// </summary>
    private void VisitFunctionReferenceExpression(FunctionReferenceExpr expr)
    {
        // Riferimento NUDO (senza parentesi) a una procedura a 0 argomenti: è una CHIAMATA,
        // non una referenza di prima classe (parità con il Linker — `BareProcedureReference` —
        // e con IL `EmitFunctionReferenceAsCall`). Prima il P-Code spingeva una code-ref/closure
        // e `PRINT STRTIME` / `x = STRDATE` valevano 0 mentre IL e VM/C davano ora/data.
        {
            ProcedureDefinition? bareDef = null;
            foreach (var candidate in _procedureMap.Values)
                if (candidate.SymbolId == expr.SymbolId) { bareDef = candidate; break; }
            if (bareDef is NativeProcedureDefinition { Parameters.Length: 0, IsFunction: true } bareNpd)
            {
                // Namespace vuoto come in IL: il dispatch è per TokenKind (`bareNpd.BuiltinKind`,
                // già portato da expr.OriginalToken per le keyword come STRTIME/STRDATE).
                VisitBuiltinFunctionCallExpression(new BuiltinFunctionCallExpr(
                    new Token(TokenKind.Identifier, "", null, expr.OriginalToken.Position),
                    expr.OriginalToken,
                    ImmutableArray<Expr>.Empty,
                    bareNpd.ReturnType));
                return;
            }
            /* Un METODO di una classe sintetica di CLOSURE (funzione locale promossa da
             * PerformHeapPromotion: `__ClosureCtx_*`) referenziato NUDO è un VALORE
             * (delegato bound all'istanza __context), NON una chiamata: `RETURN Incrementa`
             * dentro CreaIncrementatore. Trattarlo come chiamata eseguiva il corpo della
             * funzione al momento della creazione (contatore già a 101) e lasciava nella
             * variabile FUNCTION il valore di ritorno (un numero) → la chiamata indiretta
             * successiva saltava a un indirizzo-spazzatura/loop (FunctionsAdvanced su
             * PCode e VM/C, exit 142). Il valore/la closure la costruisce il blocco più
             * sotto (OP_CLOSURE con l'upvalue __context). */
            bool isClosureCtxMethod = false;
            if (bareDef != null && (bareDef.Flags & SymbolFlags.IsMethod) == SymbolFlags.IsMethod)
            {
                var bareName = NormalizeIdentifierName(expr.OriginalToken.Lexeme);
                foreach (var kv in _procedureAddresses)
                    if (kv.Key.EndsWith("." + bareName, StringComparison.OrdinalIgnoreCase) &&
                        kv.Key.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase))
                    { isClosureCtxMethod = true; break; }
            }
            if (!isClosureCtxMethod && bareDef is FunctionDefinition { Parameters.Length: 0 } bareFd)
            {
                VisitFunctionExpression(new FunctionExpr(expr.SymbolId, expr.OriginalToken,
                    ImmutableArray<Expr>.Empty, bareFd.ReturnType));
                return;
            }
        }

        // Una FUNCTION LOCALE (nidificata dentro un'altra procedura, es. `Incrementa`
        // dentro `CreaIncrementatore`) è già stata compilata a un OP_CLOSURE nel punto
        // di dichiarazione (Compiler.Statements.cs, VisitLocalFunctionDeclaration —
        // vedi AddLocal + OP_STORE_LOCAL lì), che cattura le upvalue correnti (incluso
        // un parametro BYREF come ByRefBox) e memorizza la ClosureData risultante in un
        // locale/upvalue con lo STESSO nome della funzione. Un riferimento bruto (es.
        // `RETURN Incrementa`, nessuna chiamata) a quel nome deve quindi CARICARE quella
        // ClosureData già pronta — mai emettere OP_PUSH_CODE_REF, che spinge solo
        // l'indirizzo di codice grezzo, SENZA le upvalue catturate (nessuna nozione di
        // closure): la funzione risultante perderebbe l'accesso a `contatore`, e il
        // valore numerico spinto verrebbe poi scambiato per un indirizzo indiretto da
        // CALL_INDIRECT, saltando a codice sbagliato. OP_PUSH_CODE_REF resta corretto
        // SOLO per una procedura TOP-LEVEL genuina (mai una closure, mai in `_locals`).
        var normalizedName = NormalizeIdentifierName(expr.OriginalToken.Lexeme);
        var upvalueIndex = ResolveUpvalueByName(normalizedName);
        if (upvalueIndex != -1)
        {
            EmitBytes((byte)OpCode.OP_LOAD_UPVALUE, (byte)upvalueIndex);
            return;
        }
        var localIndex = ResolveLocal(normalizedName);
        if (localIndex != -1)
        {
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)localIndex);
            return;
        }

        // Una funzione locale sottoposta a HEAP PROMOTION (cattura variabili mutabili, es.
        // CreaContatore/ContatoreInterno) non è più una vera funzione locale a questo punto:
        // il Linker l'ha già trasformata in un METODO (SymbolFlags.IsMethod) sulla classe
        // sintetica __ClosureCtx_... — un riferimento bruto (RETURN ContatoreInterno, nessuna
        // chiamata) deve quindi produrre un valore "bound method" INVOCABILE più tardi, da
        // QUALSIASI scope (es. Contatore() dentro Main, dove __context non esiste più come
        // locale) — non un indirizzo di codice grezzo (OP_PUSH_CODE_REF), che perderebbe
        // completamente il riferimento all'istanza __context su cui operare. Riusiamo lo
        // stesso incapsulamento ClosureData/OP_CLOSURE delle lambda: 1 upvalue catturato per
        // valore (l'istanza __context — un'istanza di classe reale via OP_NEW_CLASS_INSTANCE,
        // stesso motivo per cui OP_CALL_INDIRECT già sa impostare locals[0] da un upvalue
        // Dictionary), indirizzo del metodo risolto in differita tramite lo stesso meccanismo
        // di OP_PUSH_CODE_REF (_constantsToPatch, chiave qualificata "NomeClasse.NomeMetodo" —
        // _procedureAddresses registra i metodi SOLO sotto quella chiave, mai il nome nudo).
        ProcedureDefinition? refProc = null;
        foreach (var candidate in _procedureMap.Values)
        {
            if (candidate.SymbolId == expr.SymbolId) { refProc = candidate; break; }
        }
        if (refProc != null && (refProc.Flags & SymbolFlags.IsMethod) == SymbolFlags.IsMethod)
        {
            int? contextIdx = null;
            string? contextClassName = null;
            for (int ci = _locals.Count - 1; ci >= 0; ci--)
            {
                if (_locals[ci].Type is ClassType ct)
                { contextIdx = ci; contextClassName = ct.Definition.Name.Lexeme; break; }
                if (_locals[ci].Name.Lexeme.Contains("__context", StringComparison.OrdinalIgnoreCase) ||
                    _locals[ci].Name.Lexeme.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase))
                { contextIdx = ci; contextClassName = _locals[ci].Name.Lexeme; break; }
            }
            if (!contextIdx.HasValue && _locals.Count > 0 && _locals[0].Name.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase))
            {
                contextIdx = 0;
                foreach (var kv in _procedureAddresses)
                {
                    if (kv.Key.EndsWith("." + normalizedName, StringComparison.OrdinalIgnoreCase))
                    {
                        contextClassName = kv.Key[..^(normalizedName.Length + 1)];
                        break;
                    }
                }
            }
            // SymbolFlags.IsMethod e' impostato per QUALUNQUE metodo di classe/UDT (vedi
            // Parser.Declarations.Classes.cs), non solo per le funzioni locali promosse da
            // PerformHeapPromotion — un riferimento a un metodo di una classe utente REALE
            // (es. `LET ptr = Obj.Metodo`) ha anch'esso IsMethod impostato, ma NON deve
            // passare da qui: userebbe come "contesto" lo scope SBAGLIATO (quello del
            // chiamante, non l'istanza dell'oggetto), producendo indirizzi/upvalue a caso.
            // Il marcatore affidabile e' il nome della classe sintetica, sempre prefissata
            // "__ClosureCtx" da PerformHeapPromotion.
            if (contextIdx.HasValue && contextClassName != null &&
                contextClassName.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase))
            {
                var fullMethodName = contextClassName + "." + refProc.OriginalName.Lexeme;
                var methodAddrConstIdx = AddConstant(null); // Placeholder, risolto da PatchAllConstants
                _constantsToPatch.Add((methodAddrConstIdx,
                    new Token(TokenKind.Identifier, fullMethodName, null, expr.OriginalToken.Position)));
                EmitByte((byte)OpCode.OP_CLOSURE);
                EmitShort(methodAddrConstIdx);
                EmitByte((byte)(1)); // numUpvalues
                EmitByte((byte)(1)); // isLocal = true
                EmitByte((byte)contextIdx.Value);
                return;
            }
        }

        var codeRefIndex = AddConstant(null); // Placeholder per l'indirizzo
        _constantsToPatch.Add((codeRefIndex, expr.OriginalToken));

        // Un riferimento NUDO a una procedura TOP-LEVEL come VALORE deve essere una ClosureData
        // a 0 upvalue (OP_CLOSURE), MAI l'indirizzo grezzo (OP_PUSH_CODE_REF → Double): un
        // Double è indistinguibile da un valore numerico, e le builtin che accettano SIA un
        // valore SIA un callback nello stesso slot (ASCAN(arr, valore) vs ASCAN(arr, predicato))
        // scambiavano il valore per un indirizzo di codice → CallNamedFnSync su un offset
        // arbitrario → stack overflow (bug reale: `ASCAN(v, 160)`). È esattamente ciò che fa
        // già VM-C nel case OP_PUSH_CODE_REF (closure a 0 upvalue): stesso oggetto di una lambda
        // letterale, così i consumer (`IsCallable`/`ref->kind == GMB_OBJ_CLOSURE`) riconoscono
        // il callback per TIPO, non per valore.
        EmitByte((byte)OpCode.OP_CLOSURE);
        EmitShort(codeRefIndex);
        EmitByte((byte)0); // numUpvalues = 0 (nessuna cattura)
    }

    /// <summary>
    /// Compila una chiamata a una funzione built-in che è già stata risolta dal linker.
    /// Utilizza un approccio a due livelli: prima cerca nella mappa standard basata su TokenKind,
    /// poi, in caso di fallimento (per nomi ambigui), usa una mappa speciale basata su
    /// namespace e nome della funzione.
    /// </summary>
    /// <summary>
    /// Emette un SYSCALL per funzioni di namespace database (LEDGER, SPEED, DBF, ISAM).
    /// Restituisce true se la funzione è stata gestita, false altrimenti.
    /// </summary>
    private bool TryEmitDatabaseSyscall(string nsName, string funcName, ImmutableArray<Expr> args)
    {
        SysCall? syscall = null;
        int expectedArgs = 0;

        switch (nsName.ToUpperInvariant())
        {
            case "LEDGER":
                switch (funcName.ToUpperInvariant())
                {
                    case "OPEN": syscall = SysCall.LEDGER_OPEN; expectedArgs = 1; break;
                    case "ADD": syscall = SysCall.LEDGER_ADD; expectedArgs = args.Length; break;
                    case "EDIT": syscall = SysCall.LEDGER_EDIT; expectedArgs = args.Length; break;
                    case "DELETE": syscall = SysCall.LEDGER_DELETE; expectedArgs = 2; break;
                    case "GET": syscall = SysCall.LEDGER_GET; expectedArgs = 2; break;
                    case "REGISTER": syscall = SysCall.LEDGER_REGISTER; expectedArgs = 2; break;
                    case "BALANCE": syscall = SysCall.LEDGER_BALANCE; expectedArgs = 2; break;
                    case "ACCOUNTS": syscall = SysCall.LEDGER_ACCOUNTS; expectedArgs = 2; break;
                    case "VALIDATE": syscall = SysCall.LEDGER_VALIDATE; expectedArgs = 1; break;
                    case "SAVE": syscall = SysCall.LEDGER_SAVE; expectedArgs = 1; break;
                    case "CLOSE": syscall = SysCall.LEDGER_CLOSE; expectedArgs = 1; break;
                    case "PACK": syscall = SysCall.LEDGER_PACK; expectedArgs = 1; break;
                    case "BUDGET": syscall = SysCall.LEDGER_BUDGET; expectedArgs = 4; break;
                    case "VARIANCE": syscall = SysCall.LEDGER_VARIANCE; expectedArgs = 3; break;
                    case "PLOT": syscall = SysCall.LEDGER_PLOT; expectedArgs = 3; break;
                    case "IMPORT": syscall = SysCall.LEDGER_IMPORT; expectedArgs = 3; break;
                }
                break;
            case "SPEED":
                switch (funcName.ToUpperInvariant())
                {
                    case "OPEN": syscall = SysCall.SPEED_OPEN; expectedArgs = 2; break;
                    case "USE": syscall = SysCall.SPEED_USE; expectedArgs = args.Length; break;
                    case "GO": syscall = SysCall.SPEED_GO; expectedArgs = 2; break;
                    case "SKIP": syscall = SysCall.SPEED_SKIP; expectedArgs = 2; break;
                    case "SEEK": syscall = SysCall.SPEED_SEEK; expectedArgs = 3; break;
                    case "GET": syscall = SysCall.SPEED_GET; expectedArgs = 1; break;
                    case "PUT": syscall = SysCall.SPEED_PUT; expectedArgs = 2; break;
                    case "DELETE": syscall = SysCall.SPEED_DELETE_RECORD; expectedArgs = 2; break;
                    case "SETORDER": syscall = SysCall.SPEED_SET_ORDER; expectedArgs = 2; break;
                    case "SETFILTER": syscall = SysCall.SPEED_SET_FILTER; expectedArgs = 3; break;
                    case "SETRELATION": syscall = SysCall.SPEED_SET_RELATION; expectedArgs = 3; break;
                    case "CREATEINDEX": syscall = SysCall.SPEED_CREATE_INDEX; expectedArgs = 3; break;
                    case "DROPINDEX": syscall = SysCall.SPEED_DROP_INDEX; expectedArgs = 2; break;
                    case "CREATEVIEW": syscall = SysCall.SPEED_CREATE_VIEW; expectedArgs = 4; break;
                    case "REFRESHVIEW": syscall = SysCall.SPEED_REFRESH_VIEW; expectedArgs = 1; break;
                    case "DROPVIEW": syscall = SysCall.SPEED_DROP_VIEW; expectedArgs = 1; break;
                     case "SETMANY":
                     {
                         EmitFileNumber(args[0]); // alias as string
                         for (int i = 1; i < args.Length; i++) VisitExpression(args[i]);
                         EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SETMANY); EmitByte((byte)(args.Length));
                         return true;
                     }
                     case "EOF":
                     {
                         // SPEED.EOF(alias): l'alias va passato come STRINGA (il default lo
                         // compilerebbe come variabile → nil, e EOFFUNC cadrebbe sul ramo
                         // file → sempre true). Stesso pattern di ISAM.EOF.
                         EmitAliasOrExpression(args[0]);
                         EmitByte((byte)OpCode.OP_CALL_BUILTIN);
                         EmitShort((ushort)BuiltInFunction.EOFFUNC);
                         EmitByte((byte)1);
                         return true;
                     }
                 }
                 break;
            case "AIVECTOR":
                if (funcName.Equals("ENSURECOLLECTION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_ENSURE_COLLECTION; expectedArgs = args.Length; }
                else if (funcName.Equals("INDEXTEXT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_INDEX_TEXT; expectedArgs = args.Length; }
                else if (funcName.Equals("INDEXMETA", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_INDEX_META; expectedArgs = args.Length; }
                else if (funcName.Equals("SEARCH", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_SEARCH; expectedArgs = args.Length; }
                else if (funcName.Equals("COUNT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_COUNT; expectedArgs = args.Length; }
                else if (funcName.Equals("LISTCOLLECTIONS", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_LIST_COLLECTIONS; expectedArgs = args.Length; }
                else if (funcName.Equals("REMOVESOURCE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_REMOVE_SOURCE; expectedArgs = args.Length; }
                else if (funcName.Equals("LOADDOCUMENTS", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_LOAD_DOCUMENTS; expectedArgs = args.Length; }
                else if (funcName.Equals("PURGEDEPRECATED", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_PURGE_DEPRECATED; expectedArgs = args.Length; }
                break;
            case "VECTORDB":
                if (funcName.Equals("ENSURECOLLECTION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_ENSURE_COLLECTION; expectedArgs = args.Length; }
                else if (funcName.Equals("INDEXTEXT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_INDEX_TEXT; expectedArgs = args.Length; }
                else if (funcName.Equals("INDEXMETA", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_INDEX_META; expectedArgs = args.Length; }
                else if (funcName.Equals("SEARCH", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_SEARCH; expectedArgs = args.Length; }
                else if (funcName.Equals("COUNT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_COUNT; expectedArgs = args.Length; }
                else if (funcName.Equals("LISTCOLLECTIONS", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_LIST_COLLECTIONS; expectedArgs = args.Length; }
                else if (funcName.Equals("REMOVESOURCE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_REMOVE_SOURCE; expectedArgs = args.Length; }
                else if (funcName.Equals("LOADDOCUMENTS", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_LOAD_DOCUMENTS; expectedArgs = args.Length; }
                else if (funcName.Equals("PURGEDEPRECATED", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.VECTORDB_PURGE_DEPRECATED; expectedArgs = args.Length; }
                break;
            case "SQL":
                if (funcName.Equals("FETCH", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.SQL_FETCH; expectedArgs = 1; }
                else if (funcName.Equals("CLOSERESULTSET", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.SQL_CLOSE_RESULT_SET; expectedArgs = 1; }
                else if (funcName.Equals("QUERY", StringComparison.OrdinalIgnoreCase))
                {
                    EmitAliasOrExpression(args[0]); // alias as string
                    for (int i = 1; i < args.Length; i++)
                        VisitExpression(args[i]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_QUERY); EmitByte((byte)(args.Length));
                    return true;
                }
                else if (funcName.Equals("EXEC", StringComparison.OrdinalIgnoreCase))
                {
                    // Expression path: compile args AND push paramCount at the end
                    // so the handler can use the same logic as statement path.
                    // args = [alias, sql, paramsArray] — `params` è IsParamArray: il parser
                    // raccoglie gli argomenti variadici in UN ArrayLiteralExpr. Va SCOMPATTATO
                    // nei singoli parametri (il syscall handler si aspetta alias, sql, param...,
                    // paramCount): prima l'array intero finiva come singolo parametro e il
                    // paramCount/ordine si rompeva ("Unable to cast Object[] to IConvertible").
                    EmitAliasOrExpression(args[0]); // alias (as string constant if simple identifier)
                    VisitExpression(args[1]); // sql
                    int extraCount = 0;
                    if (args.Length >= 3 && args[2] is ArrayLiteralExpr sqlParamArr)
                    {
                        foreach (var pe in sqlParamArr.Elements) { VisitExpression(pe); extraCount++; }
                    }
                    else
                    {
                        for (int i = 2; i < args.Length; i++) { VisitExpression(args[i]); extraCount++; }
                    }
                    EmitConstant((double)extraCount); // paramCount
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_EXEC); EmitByte((byte)(3 + extraCount));
                    return true;
                }
                break;
            case "IO":
                if (funcName.Equals("COPY", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.IO_COPY; expectedArgs = args.Length; }
                else if (funcName.Equals("MKDIR", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.IO_MKDIR; expectedArgs = args.Length; }
                else if (funcName.Equals("RMDIR", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.IO_RMDIR; expectedArgs = args.Length; }
                else if (funcName.Equals("CHDIR", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.IO_CHDIR; expectedArgs = args.Length; }
                else if (funcName.Equals("KILL", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.IO_KILL; expectedArgs = args.Length; }
                break;
            case "ISAM":
                if (funcName.Equals("FOUND", StringComparison.OrdinalIgnoreCase))
                    return false;
                if (funcName.Equals("EOF", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.ISAM_EOF; expectedArgs = args.Length; }
                else if (funcName.Equals("BOF", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.ISAM_BOF; expectedArgs = args.Length; }
                else if (funcName.Equals("RECNO", StringComparison.OrdinalIgnoreCase))
                    // ISAM.RECNO(alias) non aveva un dispatch dedicato: cadeva nel
                    // BuiltInFunction.RECNO generico condiviso con SQL.RECNO/SQL.SELECT (che
                    // controlla _runtime.SqlDataSession, non l'area ISAM) e restituiva sempre
                    // 0.0/1.0 invece del vero numero di record — bloccava silenziosamente
                    // qualunque `DO WHILE ... AND ISAM.RECNO(alias) > 0`.
                    { syscall = SysCall.ISAM_RECNO; expectedArgs = args.Length; }
                else if (funcName.Equals("SETREPLICATION", StringComparison.OrdinalIgnoreCase))
                {
                    EmitFileNumber(args[0]); // alias as string
                    for (int i = 1; i < args.Length; i++) VisitExpression(args[i]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SET_REPLICATION); EmitByte((byte)(args.Length));
                    return true;
                }
                else if (funcName.Equals("APPLYREPLICATION", StringComparison.OrdinalIgnoreCase))
                {
                    EmitFileNumber(args[0]);
                    for (int i = 1; i < args.Length; i++) VisitExpression(args[i]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_APPLY_REPLICATION); EmitByte((byte)(args.Length));
                    return true;
                }
                else if (funcName.Equals("FOLLOWREPLICATION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.ISAM_FOLLOW_REPLICATION; expectedArgs = args.Length; }
                else if (funcName.Equals("INTEGRITY", StringComparison.OrdinalIgnoreCase))
                {
                    // L'area è OPZIONALE (INTEGRITY() = area corrente): senza args[0] il
                    // compilatore andava in IndexOutOfRange e PCode/VM-C non compilavano.
                    if (args.Length > 0) EmitFileNumber(args[0]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_INTEGRITY); EmitByte((byte)(args.Length));
                    return true;
                }
                else if (funcName.Equals("INFO", StringComparison.OrdinalIgnoreCase))
                {
                    if (args.Length > 0) EmitFileNumber(args[0]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_INFO); EmitByte((byte)(args.Length));
                    return true;
                }
                else if (funcName.Equals("APPENDMANY", StringComparison.OrdinalIgnoreCase))
                {
                    // Forme: (records) → area CORRENTE; (area, records). Con UN solo argomento args[0] era
                    // trattato come numero d'area e i record venivano letti da uno stack vuoto (conteggio
                    // spazzatura, nessun insert). L'handler pop-a (batch, area): serve SEMPRE l'area.
                    if (args.Length == 1)
                    {
                        EmitConstant(0.0);                 // 0 = area ISAM corrente (ResolveAreaNum)
                        VisitExpression(args[0]);
                        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_APPENDMANY); EmitByte((byte)2);
                        return true;
                    }
                    EmitFileNumber(args[0]);
                    for (int i = 1; i < args.Length; i++) VisitExpression(args[i]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_APPENDMANY); EmitByte((byte)(args.Length));
                    return true;
                }
                break;
            case "DBF":
                if (funcName.Equals("FOUND", StringComparison.OrdinalIgnoreCase))
                    return false;
                if (funcName.Equals("USE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_USE; expectedArgs = args.Length; }
                else if (funcName.Equals("RECNO", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_RECNO; expectedArgs = 0; }
                else if (funcName.Equals("LASTREC", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_LASTREC; expectedArgs = 0; }
                else if (funcName.Equals("LUPDATE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_LUPDATE; expectedArgs = 0; }
                else if (funcName.Equals("APPENDMANY", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_APPENDMANY; expectedArgs = args.Length; }
                else if (funcName.Equals("APPENDFROM", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_APPEND_FROM; expectedArgs = args.Length; }
                else if (funcName.Equals("APPENDRECORD", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_APPEND_RECORD; expectedArgs = 1; }
                else if (funcName.Equals("CREATE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_CREATE_TABLE; expectedArgs = args.Length; }
                else if (funcName.Equals("SETDEFAULT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_SET_DEFAULT; expectedArgs = 1; }
                else if (funcName.Equals("COPYSTRUCTURE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_COPY_STRUCTURE; expectedArgs = 1; }
                else if (funcName.Equals("CLOSE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_CLOSE_CURRENT; expectedArgs = 0; }
                else if (funcName.Equals("SELECT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_SELECT; expectedArgs = 1; }
                else if (funcName.Equals("SETDURABILITY", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_SET_DURABILITY; expectedArgs = args.Length; }
                else if (funcName.Equals("BEGINGROUPCOMMIT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_BEGIN_GROUP_COMMIT; expectedArgs = args.Length; }
                else if (funcName.Equals("ENDGROUPCOMMIT", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_END_GROUP_COMMIT; expectedArgs = args.Length; }
                else if (funcName.Equals("SETREPLICATION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_SET_REPLICATION; expectedArgs = args.Length; }
                else if (funcName.Equals("APPLYREPLICATION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_APPLY_REPLICATION; expectedArgs = args.Length; }
                else if (funcName.Equals("FOLLOWREPLICATION", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.DBF_FOLLOW_REPLICATION; expectedArgs = args.Length; }
                break;
            case "CHART":
                if (funcName.Equals("CREATE", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.CHART_CREATE; expectedArgs = args.Length; }
                break;
            case "MEMORYMAP":
                if (funcName.Equals("MEMORYMAP", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.MEMORYMAP_OPEN; expectedArgs = args.Length; }
                else if (funcName.Equals("MMAPPEEK", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.MEMORYMAP_PEEK; expectedArgs = args.Length; }
                else if (funcName.Equals("MMAPREADBYTES", StringComparison.OrdinalIgnoreCase))
                    { syscall = SysCall.MEMORYMAP_READBYTES; expectedArgs = args.Length; }
                break;
        }

        if (syscall == null) return false;

        // Compila tutti gli argomenti
        foreach (var arg in args)
            VisitExpression(arg);

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(syscall.Value); EmitByte((byte)expectedArgs);
        return true;
    }

    /// <summary>AI MCP REGISTER TOOL: JSON `inputSchema` dai parametri della FUNCTION
    /// referenziata (nome letterale nel 2° argomento). Deve essere IDENTICO a
    /// `IlRuntime.BuildInputSchema` (IL, reflection) e a C99CodeEmitter.McpInputSchemaJson
    /// (C99/C99Native, compile-time): il Compiler però non referenzia il progetto AI (dove
    /// vive `McpSchemaBuilder`), quindi la mappatura tipo→JSON Schema è replicata qui con la
    /// stessa tabella (STRING→string, BIT→boolean, interi→integer, DOUBLE→number,
    /// ARRAY OF→array, JSON→object). Ritorna "{}" se la procedura non esiste o non ha
    /// parametri (stesso contratto degli altri backend).</summary>
    private string McpInputSchemaJsonFor(Expr? procArg)
    {
        var procName = (procArg as StringExpr)?.Value;
        if (string.IsNullOrWhiteSpace(procName)) return "{}";
        ProcedureDefinition? def = null;
        foreach (var kv in _procedureMap)
            if (string.Equals(kv.Key.Item1, procName, StringComparison.OrdinalIgnoreCase)) { def = kv.Value; break; }
        if (def is null || def.Parameters.Length == 0) return "{}";
        var props = new System.Text.StringBuilder();
        var req = new System.Text.StringBuilder();
        for (var i = 0; i < def.Parameters.Length; i++)
        {
            var p = def.Parameters[i];
            var name = p.OriginalName.Lexeme;
            if (i > 0) { props.Append(','); req.Append(','); }
            props.Append('"').Append(name).Append("\":").Append(McpJsonType(p));
            req.Append('"').Append(name).Append('"');
        }
        return "{\"type\":\"object\",\"properties\":{" + props + "},\"required\":[" + req + "]}";
    }

    /// <summary>JSON Schema di un parametro dichiarato (specchio di
    /// McpSchemaBuilder.MapBasicTypeToJsonSchema via DataTypeToBasicType).</summary>
    private static string McpJsonType(Parameter p)
    {
        var isArray = p.Flags.HasFlag(SymbolFlags.IsArray);
        return p.DataType switch
        {
            ArrayType at => at.ElementType is StringType
                ? "{\"type\":\"array\",\"items\":{\"type\":\"string\"}}"
                : "{\"type\":\"array\",\"items\":{\"type\":\"number\"}}",
            StringType => "{\"type\":\"string\"}",
            BitType => "{\"type\":\"boolean\"}",
            IntegerType or LongType or ByteType or UIntegerType or ULongType or SByteType
                or QuadType or UQuadType => "{\"type\":\"integer\"}",
            DoubleType or SingleType or CurrencyType => "{\"type\":\"number\"}",
            JsonType => "{\"type\":\"object\"}",
            // fallback di McpSchemaBuilder: tipo non mappato → string con description
            _ => isArray ? "{\"type\":\"array\",\"items\":{\"type\":\"number\"}}"
                        : $"{{\"type\":\"string\",\"description\":\"{p.DataType.ToString()!.ToUpperInvariant()}\"}}"
        };
    }

    /// <summary>AI MCP REGISTER TOOL: emissione dedicata (vedi il call site).
    /// Ordine argomenti FISSO a 6: (tool, proc, desc, readOnly, inputSchema, outputSchema),
    /// con desc/readOnly/outputSchema normalizzati a ""/NIL quando omessi — così i runtime
    /// C (gmb_mcp_register_tool_ex) e C#-PCode (IlRuntime.McpRegisterToolWithSchema) li
    /// leggono posizionalmente senza ambiguità sulla firma corta.</summary>
    private void CompileMcpRegisterTool(BuiltinFunctionCallExpr expr)
    {
        for (var i = 0; i < 3; i++)
        {
            if (i < expr.Args.Length) VisitExpression(expr.Args[i]);
            else EmitConstant(string.Empty);
        }
        if (expr.Args.Length > 3) VisitExpression(expr.Args[3]);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitConstant(McpInputSchemaJsonFor(expr.Args.Length > 1 ? expr.Args[1] : null));
        if (expr.Args.Length > 4) VisitExpression(expr.Args[4]);
        else EmitConstant(string.Empty);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.MCPREGISTERTOOL);
        EmitByte((byte)6);
    }

    private void VisitBuiltinFunctionCallExpression(BuiltinFunctionCallExpr expr)
    {
        /* TRAY: la coda comandi va DRENATA sul main thread del host (il BASIC gira sui
         * worker). Marca il chunk: il pump main-thread (PCodeVm/VM-C) si attiva solo per i
         * programmi che usano TRAY. */
        if (expr.Namespace.Lexeme.Equals("TRAY", StringComparison.OrdinalIgnoreCase)
            || (expr.Name.Kind >= TokenKind.TrayCreate && expr.Name.Kind <= TokenKind.TrayLast))
            _chunk.UsesTray = true;
        if (TryVisitFormBuiltin(expr)) return;
        var nsName = expr.Namespace.Lexeme;
        var funcName = expr.Name.Lexeme;

        // CLIP.GET()/SET(v)/CLEAR come ESPRESSIONE: TokenKind.Get/Set/Clear sono GENERICI
        // (collidono con LEDGER/dict GET ecc.) e finivano sul builtin sbagliato → GET non
        // leggeva il clipboard ("CLIP.GET()" vuoto). Instrada per NAMESPACE al sub-opcode CLIP
        // (parità IL EmitClipCall / statement CLIP GET/SET/CLEAR).
        if (nsName.Equals("CLIP", StringComparison.OrdinalIgnoreCase))
        {
            if (funcName.Equals("GET", StringComparison.OrdinalIgnoreCase))
            {
                EmitBytes((byte)OpCode.OP_CLIP_CMD, (byte)0, (byte)0);
                return;
            }
            if (funcName.Equals("SET", StringComparison.OrdinalIgnoreCase) && expr.Args.Length >= 1)
            {
                VisitExpression(expr.Args[0]);
                EmitBytes((byte)OpCode.OP_CLIP_CMD, (byte)1, (byte)1);
                return;
            }
            if (funcName.Equals("CLEAR", StringComparison.OrdinalIgnoreCase))
            {
                EmitBytes((byte)OpCode.OP_CLIP_CMD, (byte)2, (byte)0);
                return;
            }
        }

        // Un GENERATOR PROCEDURE definito dall'utente con lo stesso nome di un builtin (es.
        // `GENERATOR Count(n) ... END GENERATOR` collide col builtin COUNT, TokenKind.Count →
        // BuiltInFunction.COUNT) viene per default risolto/compilato come il builtin — il
        // Parser/Linker non distingue "nome utente" da "nome builtin" finché non arriva qui. Un
        // GeneratorDefinition con nome+arità corrispondenti ha SEMPRE la precedenza: non deve
        // eseguirne il corpo (vedi VisitFunctionExpression per lo stesso pattern completo), deve
        // restituire un handle sospeso via OP_NEW_GENERATOR_CALL.
        if (_procedureMap.TryGetValue((funcName, expr.Args.Length), out var maybeGenDef) &&
            maybeGenDef is GeneratorDefinition builtinCollisionGenDef)
        {
            foreach (var bgArg in expr.Args) VisitExpression(bgArg);
            var bgCallLoc = EmitJump((byte)OpCode.OP_NEW_GENERATOR_CALL);
            _jumpsToPatch.Add((bgCallLoc, builtinCollisionGenDef.Name));
            EmitByte((byte)expr.Args.Length);
            return;
        }

        // CTYPE(value, "TypeName") su un'istanza UDT/CLASS con un OPERATOR CTYPE dichiarato
        // per quel tipo di destinazione: dispatch diretto al membro __CTYPE__{TYPENAME}
        // invece del generico builtin CTYPE (che non conosce gli overload utente). Copre solo
        // il caso in cui il nome del tipo è una stringa letterale nota a compile-time — è
        // l'unico caso esercitato dal linguaggio (CTYPE con nome tipo dinamico userebbe la
        // coercizione builtin generica, senza operatori utente).
        if (expr.Name.Kind == TokenKind.CType && expr.Args.Length == 2 && expr.Args[1] is StringExpr typeNameLit)
        {
            var targetTypeName = typeNameLit.Value.ToUpperInvariant().Replace(" ", "");
            var sourceType = CompilerInferExpressionType(expr.Args[0]);
            var hasOperator = sourceType switch
            {
                UserDefinedType udt => udt.Operators.Any(o =>
                    o.OperatorToken.Kind == TokenKind.CType &&
                    o.ReturnType.ToString()!.ToUpperInvariant().Replace(" ", "") == targetTypeName),
                ClassType ct => ct.Definition.Members.OfType<OperatorMember>().Any(m =>
                    m.Operator.OperatorToken.Kind == TokenKind.CType &&
                    m.Operator.ReturnType.ToString()!.ToUpperInvariant().Replace(" ", "") == targetTypeName),
                _ => false
            };
            if (hasOperator)
            {
                VisitExpression(expr.Args[0]);
                VisitExpression(expr.Args[0]);
                EmitConstant(1.0);
                var ctypeOpIdx = AddConstant($"__CTYPE__{targetTypeName}");
                EmitBytes((byte)OpCode.OP_CALL_MEMBER);
                EmitShort(ctypeOpIdx);
                return;
            }
        }

        // Database namespace SYSCALL dispatch
        if (TryEmitDatabaseSyscall(nsName, funcName, expr.Args))
            return;

        // NET.PEEK(handle, count): park-based via OP_NET_CMD/TCP_PEEK invece del dispatch
        // BuiltInFunction sincrono — vedi Bugs/(solved) -
        // NET-TcpPeek-Bloccante-Hang-Test-Goroutine.md. Prima passava sempre per
        // BuiltInFunction.PEEK, che chiamava IlRuntime.TcpPeek -> NetworkService.TcpPeek
        // (Socket.Receive sincrono) direttamente sul worker GPM: con GMBASIC_GOMAXPROCS=1
        // l'unico worker restava bloccato per sempre in assenza di dati. Solo la forma a 2
        // argomenti numerici (socket peek) e' intercettata qui — PEEK(lista) su una singola
        // collezione resta sul dispatch generico esistente (nessuna namespace "NET").
        if (nsName.Equals("NET", StringComparison.OrdinalIgnoreCase) &&
            funcName.Equals("PEEK", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 2)
        {
            VisitExpression(expr.Args[0]);
            VisitExpression(expr.Args[1]);
            EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_PEEK, 2);
            return;
        }

        var tokenKind = expr.Name.Kind;
        BuiltInFunction builtInFunc;
        var found = BuiltInFunctionMap.TryGetValue(tokenKind, out builtInFunc);
        // RDD.RECNO() e DBF.RECNO() condividono TokenKind.Recno: la forma RDD ha un codice proprio (vedi BuiltInFunction.RDDRECNO).
        if (found && builtInFunc == BuiltInFunction.RECNO && nsName.Equals("RDD", StringComparison.OrdinalIgnoreCase))
            builtInFunc = BuiltInFunction.RDDRECNO;

        // ENV$ / ENV.HAS: una sola stringa (nome). Il vecchio compilatore P-Code/VM-C non
        // applicava gli overload di StdLib: con 0/2 argomenti emetteva una chiamata che in
        // VM/C leggeva args[0] fuori range (exit 134) o ignorava l'extra. Rifiuto esplicito
        // qui = stessa classe di errore di IL/C99.
        if (found && builtInFunc is BuiltInFunction.ENVGET or BuiltInFunction.ENVHAS
            && expr.Args.Length != 1)
        {
            throw new BasicException(expr.OriginalToken,
                $"{expr.OriginalToken.Lexeme} richiede 1 argomento (nome della variabile d'ambiente).");
        }


        // SIZEOF: la chiamata NON gira — diventa una costante, quando il tipo statico
        // dell'argomento esiste (vedi TypeLayout e Compiler.SizeOf.cs). È l'unico modo di
        // avere lo stesso numero su tutte e cinque le gambe: il runtime di VM/C e PCodeVM
        // vede solo valori (ogni numerico è un double, quindi INTEGER risulterebbe 8 invece
        // di 2), mentre IL/C99Native conoscono il tipo dichiarato. Resta al
        // runtime il caso "nome di tipo" (sotto: spinge il descrittore, che porta la chiave
        // "Size") e quello senza layout statico (-1).
        if (found && builtInFunc == BuiltInFunction.SIZEOF && expr.Args.Length == 1 &&
            TryStaticSizeOf(expr.Args[0], out var sizeofBytes))
        {
            EmitConstant((double)sizeofBytes);
            return;
        }

        // AI MCP REGISTER TOOL — emissione DEDICATA con ABI unica per tutti i produttori di
        // chunk/programmi: (tool, proc, desc, readOnly, inputSchema, outputSchema).
        // L'inputSchema è derivato a COMPILE-TIME dai parametri della FUNCTION referenziata
        // (McpInputSchemaJsonFor): sul percorso P-Code/VM-C la reflection NON conosce le
        // procedure del chunk, quindi restava `{}` e `AI.MCPLISTTOOLS()` divergeva da IL
        // (reflection) e dai codegen C (compile-time). outputSchema è il 5° argomento Basic.
        if (found && builtInFunc == BuiltInFunction.MCPREGISTERTOOL)
        {
            CompileMcpRegisterTool(expr);
            return;
        }

        // 1. Compila tutti gli argomenti. Pre-eval NEW to avoid stack corruption from constructor yield
        var hasNewExpr = expr.Args.Any(a => ContainsNewExpr(a));
        if (hasNewExpr)
        {
            // Store each arg to a temp local, then reload in order
            var tempSlots = new List<int>();
            for (int i = 0; i < expr.Args.Length; i++)
            {
                VisitExpression(expr.Args[i]);
                var slot = _locals.Count;
                AddLocal(new Token(TokenKind.Identifier, "__ba_" + i, null, expr.Args[i].Position));
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)slot);
                tempSlots.Add(slot);
            }
            for (int i = 0; i < expr.Args.Length; i++) { EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)tempSlots[i]); }
        }
        else
        {
            foreach (var arg in expr.Args)
            {
                // SIZEOF(Tipo): l'argomento può essere un NOME DI TIPO (UDT/CLASS, es.
                // `SIZEOF(PackedPoint)`) e non una variabile — senza questa risoluzione il
                // compilatore emetteva OP_PUSH_NIL (variabile inesistente) e SIZEOF
                // restituiva sempre 0. Cerca il descrittore per nome in
                // _compiledClassDescriptors e lo spinge come costante.
                if (builtInFunc == BuiltInFunction.SIZEOF && arg is VariableExpr szVe)
                {
                    Dictionary<string, object?>? szDesc = null;
                    foreach (var kv in _compiledClassDescriptors)
                    {
                        if (kv.Value != null &&
                            kv.Value.TryGetValue("Name", out var szNm) &&
                            szNm is string szNmS &&
                            string.Equals(szNmS, szVe.NameSuffixed.Lexeme, StringComparison.OrdinalIgnoreCase))
                        { szDesc = kv.Value; break; }
                    }
                    if (szDesc != null) { EmitConstant(szDesc); continue; }
                }
                // SELECTED(alias): gli alias di work area (es. ISAM.SELECTED(Clienti)) sono
                // identificatori NON dichiarati come variabili — il compilatore li emetteva
                // come OP_PUSH_NIL e la builtin non poteva risolvere l'area per nome.
                // Emetti il NOME dell'identificatore come stringa (l'alias) quando non è una
                // variabile dichiarata.
                // DBF.SELECTED(alias): un identificatore NUDO è SEMPRE il nome dell'alias,
                // anche se esiste una variabile con quel nome (parità IL/C99, dove
                // EmitDbfAliasCall emette Ldstr del nome). Prima la PCode valutava la
                // variabile dichiarata → `DBF.SELECTED(n)` dava 2 mentre IL/C99 davano 0.
                // Per passare un numero d'area usare un letterale o un'espressione
                // (`DBF.SELECTED(2)`, `DBF.SELECTED(1+1)`), non un identificatore nudo.
                if (builtInFunc == BuiltInFunction.DBFSELECTED && arg is VariableExpr selVe)
                {
                    EmitConstant(selVe.NameSuffixed.Lexeme);
                    continue;
                }
                // FOUND(alias): stesso problema di SELECTED sopra — ISAM.FOUND(Clienti)/
                // DBF.FOUND(Clienti) con Clienti alias non dichiarato come variabile emetteva
                // OP_PUSH_NIL, quindi ExecuteBuiltIn risolveva sempre l'area ambient (o niente)
                // invece dell'alias passato esplicitamente.
                if (builtInFunc == BuiltInFunction.FOUND && arg is VariableExpr foundVe &&
                    ResolveLocal(foundVe.NameSuffixed.Lexeme) == -1 &&
                    !_globals.ContainsKey(foundVe.NameSuffixed.Lexeme))
                {
                    EmitConstant(foundVe.NameSuffixed.Lexeme);
                    continue;
                }
                // RECSIZE([alias]): stesso problema di FOUND sopra — l'alias di work area
                // (es. RECSIZE(Ordini)) NON è una variabile dichiarata: emesso come OP_PUSH_NIL/0
                // faceva risolvere l'area ambient (Clienti) invece dell'alias → RECSIZE 89 vs 87.
                if (builtInFunc == BuiltInFunction.RECSIZE && arg is VariableExpr rsVe &&
                    ResolveLocal(rsVe.NameSuffixed.Lexeme) == -1 &&
                    !_globals.ContainsKey(rsVe.NameSuffixed.Lexeme))
                {
                    EmitConstant(rsVe.NameSuffixed.Lexeme);
                    continue;
                }
                // LASTREC([alias]): l'alias di work area (es. ISAM.LASTREC(Clienti)) è un
                // identificatore NON dichiarato come variabile — emesso come valore dava 0 e la
                // builtin ripiegava sull'area CORRENTE: con due aree aperte `ISAM.LASTREC(A1)`
                // leggeva il conteggio di A2 (misurato: IL 2, PCode/VM-C 5). Emetti il NOME
                // dell'identificatore (stessa regola di `EmitDbfAliasCall` lato IL). Escluso
                // LEDGER.LASTREC(handle), dove l'argomento è un handle numerico, non un alias.
                if (builtInFunc == BuiltInFunction.LASTREC && arg is VariableExpr lrVe &&
                    !nsName.Equals("LEDGER", StringComparison.OrdinalIgnoreCase) &&
                    ResolveLocal(lrVe.NameSuffixed.Lexeme) == -1 &&
                    !_globals.ContainsKey(lrVe.NameSuffixed.Lexeme))
                {
                    EmitConstant(lrVe.NameSuffixed.Lexeme);
                    continue;
                }
                // SQL.CHANGES/SQL.LASTINSERTROWID(alias): l'alias SQL (SQL OPEN ... AS x) è un
                // identificatore NON dichiarato come variabile — emetti il NOME come stringa
                // (parità EmitAliasOrExpression usata da SQL.QUERY/EXEC), altrimenti il
                // compilatore emette OP_PUSH_NIL e la builtin non risolve l'alias.
                if ((builtInFunc == BuiltInFunction.SQLCHANGES || builtInFunc == BuiltInFunction.SQLLASTINSERTROWID)
                    && arg is VariableExpr sqlAliasVe
                    && ResolveLocal(sqlAliasVe.NameSuffixed.Lexeme) == -1
                    && !_globals.ContainsKey(sqlAliasVe.NameSuffixed.Lexeme))
                {
                    EmitConstant(sqlAliasVe.NameSuffixed.Lexeme);
                    continue;
                }
                // MESSAGE.DELIVERY(bus): P-CodeVM e VM/C identificano il bus col NOME della
                // variabile (vedi EmitMessageOp/HandleNameExpr per CONNECT/SUBSCRIBE/PUBLISH/PING/
                // CLOSE): passando il VALORE il bus non verrebbe mai trovato e i contatori
                // sarebbero tutti zeri (falso verde). IL e C99Native usano l'handle numerico
                // assegnato da CONNECT e non passano da qui.
                if (builtInFunc == BuiltInFunction.MESSAGEDELIVERY && arg is VariableExpr mdVe)
                {
                    EmitConstant(mdVe.NameCleaned.Lexeme);
                    continue;
                }
                VisitExpression(arg);
            }
        }

        if (found)
        {
            // 2. Emette l'opcode e gli operandi corretti.
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)builtInFunc);
            EmitByte((byte)expr.Args.Length);
        }
        else
        {
            // 3. NESSUN fallback silenzioso (allineato GMBasicAI, fix 2026-08-11): una funzione builtin
            //    non mappata è un bug di registrazione — manca la voce TokenKind→BuiltInFunction in
            //    _builtInFunctionMap (Compiler/Compiler.Expressions.Functions.cs). Emettere OP_PUSH_NIL
            //    produceva un valore NIL silenzioso a runtime (classe di bug "Nessuna funzione pubblica X"),
            //    vietato dalla regola anti-stub del progetto. Errore esplicito con il nome esatto.
            throw new BasicException(expr.Name,
                $"Funzione builtin '{nsName}.{funcName}' (TokenKind {tokenKind}) non mappata a un BuiltInFunction. " +
                "Aggiungere la voce in _builtInFunctionMap.");
        }
    }

    /// <summary>
    /// Metodo helper per inferire il DataType di un'espressione a tempo di compilazione.
    /// Questa versione è stata aggiornata per gestire correttamente le funzioni array
    /// polimorfiche come AMAP, AFILTER, etc.
    /// </summary>
    private DataType CompilerInferExpressionType(Expr expr)
    {
        switch (expr)
        {
            case NumberExpr: return new DoubleType();
            case StringExpr: return new StringType();
            case BooleanExpr: return new BitType();

            case VariableExpr ve:
            {
                for (var i = _locals.Count - 1; i >= 0; i--)
                {
                    if (_locals[i].Name.Lexeme.Equals(ve.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase))
                        return _locals[i].Type;
                }

                if (_globals.TryGetValue(ve.NameCleaned.Lexeme, out var globalIndex))
                {
                    if (globalIndex < _globalInfos.Count)
                        return _globalInfos[globalIndex].Type;
                }

                // "__context"/"__ClosureCtx_*" (il riferimento al contesto di closure che il
                // linker inietta nel corpo di un metodo promosso da PerformHeapPromotion) non
                // esiste MAI come local per nome dentro il metodo — a runtime e' SELF
                // (_locals[0]), non "__context". Senza questo fallback il tipo cadrebbe su
                // DeduceTypeFromSuffix (un numero), e AssignField/InferExpressionTypeFromTarget
                // non riconoscerebbe mai il ClassType, scrivendo i campi con OP_STORE_FIELD
                // per-nome invece di OP_STORE_MEMBER per-offset (la stessa chiave usata in
                // lettura) — un incremento del campo diventava invisibile alla lettura successiva.
                if (_currentMethodClassType != null &&
                    (ve.NameCleaned.Lexeme.StartsWith("__context", StringComparison.OrdinalIgnoreCase) ||
                     ve.NameCleaned.Lexeme.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase)))
                {
                    return _currentMethodClassType;
                }

                // Variabile non ancora registrata (implicita): il tipo viene dal SUFFISSO scritto nel sorgente
                // (NameSuffixed). Con NameCleaned (suffisso gia' tolto) `NOME$` risultava numerico e
                // `INPUT #1, NOME$` riceveva un hint numerico (PCode/VM-C: " 0 " al posto del testo).
                return DeduceTypeFromSuffix(ve.NameSuffixed.Lexeme);
            }

            case BinaryExpr be:
            {
                // Comparison operators return BIT (rappresentato numericamente a runtime).
                if (be.Op.Kind is TokenKind.Equal or TokenKind.NotEqual or TokenKind.LessThan or TokenKind.GreaterThan or TokenKind.LessEqual or TokenKind.GreaterEqual or TokenKind.ExactlyEqual)
                    return new BitType();
                var leftType = CompilerInferExpressionType(be.Left);
                return leftType;
            }

            case UnaryExpr ue:
                return CompilerInferExpressionType(ue.Right);

            case GroupingExpr ge:
                return CompilerInferExpressionType(ge.Expression);

            //case FunctionExpr fe:
            //    return fe.ReturnType;

            case BuiltinFunctionCallExpr bce:
            {
                if (!BuiltInFunctionMap.TryGetValue(bce.Name.Kind, out var builtInFunc))
                    return new InferredType(); // Non dovrebbe accadere se la mappa è completa
                // Se la funzione è una di quelle che restituiscono un array dello stesso tipo dell'input
                // (es. AMAP, AFILTER), il tipo di ritorno viene inferito dal primo argomento.
                if (IsArrayPolymorphic(builtInFunc) && bce.Args.Length > 0)
                {
                    return CompilerInferExpressionType(bce.Args[0]);
                }

                // Altrimenti, usa il tipo di ritorno definito nell'AST dal Linker.
                return bce.ReturnType;
            }
            case LambdaExpr le:
            {
                // Il tipo di un'espressione lambda è un puntatore a funzione (o closure)
                // la cui firma è definita dalla lambda stessa.
                var parameterTypes = le.Parameters.Select(p => p.DataType).ToImmutableArray();
                return new FunctionPointerType(parameterTypes, le.ReturnType);
            }
            case FieldAccessExpr fae:
            {
                var baseType = CompilerInferExpressionType(fae.Structure);
                switch (baseType)
                {
                    case UserDefinedType udt:
                    {
                        var field = udt.Fields.FirstOrDefault(f =>
                            f.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
                        if (field != null) return field.DataType;
                        break;
                    }
                    case ClassType:
                        // Logica futura per classi.
                        break;
                }

                return new InferredType();
            }

            case ArrayAccessExpr aae:
            {
                var arrayVarType = CompilerInferExpressionType(new VariableExpr(-1,aae.NameSuffixed, aae.NameCleaned));
                if (arrayVarType is ArrayType at)
                {
                    return at.ElementType;
                }

                return new InferredType();
            }

            default:
                return new InferredType();
        }
    }

    /// <summary>
    /// Metodo helper per determinare se una funzione built-in è "polimorfica"
    /// rispetto all'array, cioè se restituisce un array dello stesso tipo
    /// dell'array passato come primo argomento.
    /// </summary>
    private static bool IsArrayPolymorphic(BuiltInFunction func)
    {
        return func switch
        {
            BuiltInFunction.AMAP => true,
            BuiltInFunction.AFILTER => true,
            BuiltInFunction.ACLONE => true,
            BuiltInFunction.AUNIQUE => true,
            BuiltInFunction
                    .ASORT =>
                true, // ASORT può modificare l'array in-place, ma se usato in un'assegnazione, restituisce una copia.
            _ => false
        };
    }

    /// <summary>
    /// Metodo helper per inferire il DataType di un Target.
    /// Questa versione include logging diagnostico.
    /// </summary>
    private DataType InferExpressionTypeFromTarget(Target target)
    {
        //LogDebug($"[InferExpressionTypeFromTarget] Inferenza per target: {target.GetType().Name} ({target})");
        switch (target)
        {
            // SELF/ME: il local SELF non è sempre in _locals → DeduceTypeFromSuffix fallirebbe
            // (DoubleType). In un metodo d'istanza il tipo è noto da _currentMethodClassType:
            // senza, `SELF.r.f` (campo UDT annidato) non risolveva il tipo → lunghezza fissa
            // STRING * N non applicata in scrittura (bug campo UDT STRING*N via campo di classe).
            case VariableTarget vt when _currentMethodClassType is not null
                && (vt.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase)
                    || vt.NameCleaned.Lexeme.Equals("ME", StringComparison.OrdinalIgnoreCase)):
                return _currentMethodClassType;

            case VariableTarget vt:
                return CompilerInferExpressionType(new VariableExpr(-1,vt.NameSuffixed, vt.NameCleaned));

            case ArrayElementTarget aet:
            {
                var arrayType = CompilerInferExpressionType(new VariableExpr(-1, aet.NameSuffixed, aet.NameCleaned));
                //LogDebug(
                //    $"  [ArrayElementTarget] Accesso all'array '{aet.Name.Lexeme}'. Tipo base inferito: {arrayType}");
                if (arrayType is ArrayType at)
                {
                    return at.ElementType;
                }

                return new InferredType();
            }

            // Elemento indicizzato di un array (`SELF.arr(i,j)` / `arr(i)`): il tipo è quello
            // dell'ELEMENTO dell'array base. Senza, la lunghezza fissa `STRING * N` di un campo
            // di un elemento UDT 2-D (`SELF.arr(i,j).campo = str`) non veniva applicata.
            case IndexedTarget it:
            {
                var baseType = CompilerInferExpressionType(it.Base);
                if (baseType is ArrayType itArr) return itArr.ElementType;
                // `SELF.arr(i,j)` / `arr(i,j)` (campo-array della CLASSE corrente): il tipo del
                // campo è noto dal tipo della classe (SELF non è sempre risolvibile). Senza,
                // la lunghezza fissa `STRING * N` del campo dell'elemento non era nota.
                var fldName = it.Base switch
                {
                    FieldAccessExpr fa => fa.Field.Lexeme,
                    VariableExpr ve => ve.NameCleaned.Lexeme,
                    _ => null,
                };
                if (fldName is not null && _currentMethodClassType is ClassType cct)
                {
                    var fm = cct.Definition.Members.OfType<FieldMember>()
                        .FirstOrDefault(m => m.Declaration.NameCleaned.Lexeme.Equals(fldName, StringComparison.OrdinalIgnoreCase));
                    if (fm?.Declaration.DataType is ArrayType faArr) return faArr.ElementType;
                }
                return new InferredType();
            }

            case FieldTarget ft:
            {
                var baseType = InferExpressionTypeFromTarget(ft.Structure);
                //LogDebug($"  [FieldTarget] Accesso al campo '{ft.Field.Lexeme}'. Tipo base inferito: {baseType}");
                if (baseType is UserDefinedType udt)
                {
                    var field = udt.Fields.FirstOrDefault(f =>
                        f.Name.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                    if (field != null) return field.DataType;
                }
                // Campo di una CLASSE (base = ClassType, es. `SELF.r`): il tipo del campo si
                // legge dai membri. Serve per risolvere i campi UDT annidati dentro una classe.
                if (baseType is ClassType ct)
                {
                    var fm = ct.Definition.Members.OfType<FieldMember>().FirstOrDefault(m =>
                        m.Declaration.NameCleaned.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                    if (fm?.Declaration.DataType is not null) return fm.Declaration.DataType;
                }

                return new InferredType();
            }

            default:
                return new InferredType();
        }
    }

    /// <summary>
    /// DEDUCE IL TIPO DI DATO DAL SUFFISSO DEL NOME DELLA VARIABILE.
    /// </summary>
    private static DataType DeduceTypeFromSuffix(string name)
    {
        if (name.EndsWith('$')) return new StringType();
        if (name.EndsWith('%')) return new IntegerType();
        if (name.EndsWith('&')) return new LongType();
        if (name.EndsWith('!')) return new SingleType();
        if (name.EndsWith('#')) return new DoubleType();
        if (name.EndsWith('@')) return new CurrencyType();
        return new DoubleType(); // Default
    }

    private void VisitTernaryExpression(TernaryExpr expr)
    {
        VisitExpression(expr.Condition);
        var jumpToFalse = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
        VisitExpression(expr.TruePart);
        var jumpToEnd = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        PatchJump(jumpToFalse);
        VisitExpression(expr.FalsePart);
        PatchJump(jumpToEnd);
    }

    private void VisitSwitchExpression(SwitchExpr expr)
    {
        // Compila ogni CASE: valuta switch value, confronta con CASE cond, salta se falso
        var endJumps = new List<int>();

        foreach (var clause in expr.Cases)
        {
            VisitExpression(expr.Value);
            VisitExpression(clause.Condition);
            EmitByte((byte)OpCode.OP_CMP_EQ);
            var jumpToNext = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            VisitExpression(clause.Result);
            endJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
            PatchJump(jumpToNext);
            EmitByte((byte)OpCode.OP_POP); // pulisce il risultato falso
        }

        // ELSE branch
        if (expr.ElseCase != null)
        {
            VisitExpression(expr.ElseCase);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Patching dei salti alla fine
        foreach (var jump in endJumps)
            PatchJump(jump);
    }

    private void VisitInterpolatedStringExpression(InterpolatedStringExpr expr)
    {
        foreach (var segment in expr.Segments)
        {
            if (segment is InterpolatedStringLiteralSegment lit)
            {
                EmitConstant(lit.Text);
            }
            else if (segment is InterpolatedStringExprSegment eseg)
            {
                VisitExpression(eseg.Expression);
            }
        }
        for (var i = 1; i < expr.Segments.Length; i++)
        {
            EmitByte((byte)OpCode.OP_ADD);
        }
        if (expr.Segments.Length == 0)
        {
            EmitConstant(string.Empty);
        }
    }

    private void VisitTupleLiteralExpression(TupleLiteralExpr expr)
    {
        foreach (var element in expr.Elements)
        {
            VisitExpression(element.Value);
        }
        EmitByte((byte)OpCode.OP_MK_ARRAY);
        EmitShort(expr.Elements.Length);
    }

    private void VisitAsyncFunctionExpression(AsyncFunctionExpr expr)
    {
        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(expr.Definition.Body);
        foreach (var stmt in expr.Definition.Body)
        {
            VisitStatement(stmt);
        }
    }

    private void VisitNullableHasValueExpression(NullableHasValueExpr expr)
    {
        VisitExpression(expr.Expression);
        EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_CMP_NE);
    }

    private void VisitNullableValueExpression(NullableValueExpr expr)
    {
        VisitExpression(expr.Expression);
    }

    private void VisitPrefixIncrementExpression(PrefixIncrementExpr expr)
    {
        LoadVariable(expr.Target);
        EmitConstant(1.0);
        EmitByte((byte)(expr.Operator.Kind == TokenKind.PlusPlus ? OpCode.OP_ADD : OpCode.OP_SUBTRACT));
        EmitByte((byte)OpCode.OP_DUP);
        AssignVariable(expr.Target, false);
    }

    private void VisitPostfixIncrementExpression(PostfixIncrementExpr expr)
    {
        LoadVariable(expr.Target);
        EmitByte((byte)OpCode.OP_DUP);
        EmitConstant(1.0);
        EmitByte((byte)(expr.Operator.Kind == TokenKind.PlusPlus ? OpCode.OP_ADD : OpCode.OP_SUBTRACT));
        AssignVariable(expr.Target, false);
    }

    private void VisitArraySliceExpression(ArraySliceExpr expr)
    {
        VisitExpression(expr.Base);
        if (expr.Start != null) VisitExpression(expr.Start);
        else EmitConstant(0.0);
        if (expr.End != null) VisitExpression(expr.End);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ARRAY_SLICE); EmitByte((byte)3);
    }

    private void VisitAssignmentExpression(AssignmentExpr expr)
    {
        VisitExpression(expr.Value);
        EmitByte((byte)OpCode.OP_DUP);
        AssignVariable(expr.Target, false);
    }

    private void VisitLinqQueryExpression(LinqQueryExpr expr)
    {
        VisitExpression(expr.Source);
        AddLocal(expr.ItemVar);
        if (expr.WhereClause != null)
        {
            VisitExpression(expr.WhereClause);
            EmitByte((byte)OpCode.OP_POP);
        }
        if (expr.SelectClause != null)
        {
            VisitExpression(expr.SelectClause);
        }
        else
        {
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)(_locals.Count - 1));
        }
    }

    private void VisitDictComprehensionExpression(DictComprehensionExpr expr)
    {
        // { k : v FOR x IN coll [IF cond] } — dict risultato, loop annidati, inserimento per coppia.
        BeginScope();
        EmitByte((byte)OpCode.OP_MK_DICT);
        EmitShort(0);
        AddLocal(new Token(TokenKind.Identifier, "__result", null, SourcePosition.Unknown));
        var dictResultSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)dictResultSlot);
        CompileNestedClauseDict(expr.Clauses, 0, expr.KeyExpression, expr.ValueExpression, dictResultSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)dictResultSlot);
        EndScope();
    }

    private void CompileNestedClauseDict(
        ImmutableArray<ComprehensionClause> clauses,
        int clauseIndex,
        Expr keyExpr,
        Expr valueExpr,
        int resultSlot)
    {
        if (clauseIndex >= clauses.Length)
        {
            // Corpo più interno: dict[chiave] = valore (stack: [val, arr, key]).
            VisitExpression(valueExpr);
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resultSlot);
            VisitExpression(keyExpr);
            EmitBytes((byte)OpCode.OP_STORE_ELEMENT, 1);
            return;
        }

        var clause = clauses[clauseIndex];
        BeginScope();
        VisitExpression(clause.SourceExpression);
        AddLocal(new Token(TokenKind.Identifier, $"__source{clauseIndex}", null, SourcePosition.Unknown));
        var sourceSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)sourceSlot);
        EmitConstant(0.0);
        AddLocal(new Token(TokenKind.Identifier, $"__index{clauseIndex}", null, SourcePosition.Unknown));
        var indexSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);
        var loopStartLabel = GenerateCompilerLabel($"COMPD_START_{clauseIndex}");
        var loopExitLabel = GenerateCompilerLabel($"COMPD_EXIT_{clauseIndex}");
        var continueLabel = GenerateCompilerLabel($"COMPD_CONT_{clauseIndex}");
        _labels[loopStartLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.UBOUND);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_CMP_GT);
        var jumpToEnd = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((jumpToEnd, loopExitLabel));
        BeginScope();
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT_POSITIONAL, 1);
        AddLocal(clause.IteratorVariable);
        if (_locals.Count > 0) EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
        if (clause.ConditionExpression != null)
        {
            VisitExpression(clause.ConditionExpression);
            var jumpToContinue = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            _jumpsToPatch.Add((jumpToContinue, continueLabel));
        }
        CompileNestedClauseDict(clauses, clauseIndex + 1, keyExpr, valueExpr, resultSlot);
        _labels[continueLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EndScope();
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitConstant(1.0);
        EmitByte((byte)OpCode.OP_ADD);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);
        EmitLoop(_labels[loopStartLabel.Lexeme]);
        _labels[loopExitLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EndScope();
    }

    private void VisitSetComprehensionExpression(SetComprehensionExpr expr)
    {
        // { x FOR x IN coll [IF cond] } — lista risultato con dedup (insieme).
        BeginScope();
        EmitByte((byte)OpCode.OP_MK_LIST);
        EmitShort(0);
        AddLocal(new Token(TokenKind.Identifier, "__result", null, SourcePosition.Unknown));
        var setResultSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)setResultSlot);
        CompileNestedClauseSet(expr.Clauses, 0, expr.ElementExpression, setResultSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)setResultSlot);
        EndScope();
    }

    private void CompileNestedClauseSet(
        ImmutableArray<ComprehensionClause> clauses,
        int clauseIndex,
        Expr elementExpr,
        int resultSlot)
    {
        if (clauseIndex >= clauses.Length)
        {
            // Corpo più interno: se CONTAINS(__result, elem) salta; altrimenti AADD(__result, elem).
            var skipAddLabel = GenerateCompilerLabel("COMPS_SKIP");
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)resultSlot);
            VisitExpression(elementExpr);
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)BuiltInFunction.CONTAINS);
            EmitByte((byte)(2));
            var jumpSkip = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
            _jumpsToPatch.Add((jumpSkip, skipAddLabel));
            EmitByte((byte)OpCode.OP_PUSH_REF_LOCAL); EmitShort((ushort)resultSlot);
            VisitExpression(elementExpr);
            EmitByte((byte)OpCode.OP_CALL_BUILTIN);
            EmitShort((ushort)BuiltInFunction.AADD);
            EmitByte((byte)(2));
            EmitByte((byte)OpCode.OP_POP);
            _labels[skipAddLabel.Lexeme] = _chunk.Code.Count;
            _isReachable = true;
            return;
        }

        var clause = clauses[clauseIndex];
        BeginScope();
        VisitExpression(clause.SourceExpression);
        AddLocal(new Token(TokenKind.Identifier, $"__source{clauseIndex}", null, SourcePosition.Unknown));
        var sourceSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)sourceSlot);
        EmitConstant(0.0);
        AddLocal(new Token(TokenKind.Identifier, $"__index{clauseIndex}", null, SourcePosition.Unknown));
        var indexSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);
        var loopStartLabel = GenerateCompilerLabel($"COMPS_START_{clauseIndex}");
        var loopExitLabel = GenerateCompilerLabel($"COMPS_EXIT_{clauseIndex}");
        var continueLabel = GenerateCompilerLabel($"COMPS_CONT_{clauseIndex}");
        _labels[loopStartLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.UBOUND);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_CMP_GT);
        var jumpToEnd = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((jumpToEnd, loopExitLabel));
        BeginScope();
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)sourceSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitBytes((byte)OpCode.OP_LOAD_ELEMENT_POSITIONAL, 1);
        AddLocal(clause.IteratorVariable);
        if (_locals.Count > 0) EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
        if (clause.ConditionExpression != null)
        {
            VisitExpression(clause.ConditionExpression);
            var jumpToContinue = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            _jumpsToPatch.Add((jumpToContinue, continueLabel));
        }
        CompileNestedClauseSet(clauses, clauseIndex + 1, elementExpr, resultSlot);
        _labels[continueLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EndScope();
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)indexSlot);
        EmitConstant(1.0);
        EmitByte((byte)OpCode.OP_ADD);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)indexSlot);
        EmitLoop(_labels[loopStartLabel.Lexeme]);
        _labels[loopExitLabel.Lexeme] = _chunk.Code.Count;
        _isReachable = true;
        EndScope();
    }

    private void VisitJsonToObjectExpression(JsonToObjectExpr expr)
    {
        VisitExpression(expr.JsonSource);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.JSON_TO_OBJECT); EmitByte((byte)1);
        /* Target UDT: allega il descrittore della DICHIARAZIONE all'istanza (VM/C e PCodeVM
         * risolvono offset→nome campo via __class__; i symbol id dei campi del descrittore
         * serializzato NEL JSON possono differire da quelli del sito d'uso — es. `DIM back AS
         * CfgData = JSON.TOOBJECT(CfgData, j)`: il tipo della DIM è una copia con id propri).
         * Stesso lookup/registrazione di Compiler.Assign.cs (DIM AS <UDT>). */
        if (expr.TargetType is UserDefinedType udtT)
        {
            System.Collections.Generic.Dictionary<string, object?>? desc = null;
            var descKey = udtT.Name.Lexeme + "#" + udtT.SymbolId;
            if (_compiledClassDescriptors.TryGetValue(descKey, out var d0)) desc = d0;
            else
            {
                foreach (var kv in _compiledClassDescriptors)
                    if (kv.Value != null && kv.Value.TryGetValue("Name", out var nm) && nm is string nmS
                        && string.Equals(nmS, udtT.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                    { desc = kv.Value; break; }
            }
            if (desc != null)
            {
                var descIdx = AddConstant(desc);
                _classDescriptorsToPatch.Add((descIdx, udtT));
                EmitByte((byte)OpCode.OP_ATTACH_UDT_DESC); EmitShort((ushort)descIdx);
            }
        }
    }

    /// <summary>Check if expression tree contains a NewExpr (to decide if args need temp storage)</summary>
    private static bool ContainsNewExpr(Expr expr) => expr switch
    {
        NewExpr => true,
        FieldAccessExpr fae => ContainsNewExpr(fae.Structure),
        _ => false
    };
}
