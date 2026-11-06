// FILE: IlProgramCompiler.Expressions.cs
// PARTIAL — dominio EXPRESSIONS (emissioni espressione: literal, field, call, builtin).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio EXPRESSIONS.</summary>
public sealed partial class IlProgramCompiler
{

    internal void EmitExpression(Expr expr)
    {
        switch (expr)
        {
            case NumberExpr n: _il.Emit(OpCodes.Ldc_R8, n.Value); break;
            case NilExpr: _il.Emit(OpCodes.Ldnull); break;
            case StringExpr s: _il.Emit(OpCodes.Ldstr, s.Value); break;
            case BooleanExpr b: _il.Emit(b.Value ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0); break;
            case VariableExpr v:
                // Variabile di iterazione FOR con SymbolId NON risolto dal linker (gap:
                // `LINE (i*40,...), i, BF` → SymbolId=-1 nel corpo) → risolvi per NOME
                // contro il contatore FOR attivo e carica il suo local (aggiornato dal Next).
                if (v.SymbolId < 0 && _forLoopState.TryGetValue(v.NameCleaned.Lexeme, out var forVarState))
                {
                    _il.Emit(OpCodes.Ldloc, GetOrDeclareLocal(forVarState.SymbolId));
                    break;
                }
                // Variabile implicita (non-contatore) con SymbolId NON risolto dal linker
                // dentro un FOR (`ZZ = 240 - Px*10` poi `LINE ... ZZ` → SymbolId=-1):
                // fallback per NOME contro i local della procedura corrente (lo store ha
                // registrato il local per nome). Senza, il load leggeva un local dummy
                // mai scritto → 0.
                if (v.SymbolId < 0 && !_parameters.ContainsKey(v.SymbolId) &&
                    _localByName.TryGetValue(v.NameCleaned.Lexeme.ToUpperInvariant(), out var byNameLocal))
                {
                    _il.Emit(OpCodes.Ldloc, byNameLocal);
                    break;
                }
                // Nome NON risolto che è un CAMPO dell'area DBF attiva (late-binding Clipper):
                // il Linker produce il nodo bare-field solo in lettura (`PRINT Saldo`), ma
                // dentro `REPLACE ... WITH Saldo + 50` lascia una VariableExpr#-1 → qui si
                // leggeva un local dummy (0): il valore diventava 0+50. Stesso bind dei campi
                // nudi (parità PCode/VM-C/C99: 150 e non 50).
                if (v.SymbolId < 0 && !_parameters.ContainsKey(v.SymbolId))
                {
                    _il.Emit(OpCodes.Ldstr, v.NameCleaned.Lexeme);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfLateBindFieldNum), typeof(string)));
                    break;
                }
                EmitLoadVariable(v.SymbolId); break;
            // MakeByRefExpr (sintetico, Linker-only — Fase 4 Closure-ByRef-Box-Linker-
            // Definitivo): "dammi il box BYREF write-through per questo simbolo", usato dal
            // Linker per il seeding del campo __ClosureCtx di un parametro BYREF catturato.
            // Il simbolo è SEMPRE un parametro/local DELLA PROCEDURA CORRENTE (mai un simbolo
            // esterno) — già rappresentato come box (parametro con ParameterInfo.IsBox, o
            // local in BoxedSymbols) grazie alla Fase 4.3/4.4: basta pushare quel riferimento
            // TAL QUALE, mai costruirne uno nuovo (identità condivisa col chiamante).
            case MakeByRefExpr mbr:
                if (_parameters.TryGetValue(mbr.SymbolId, out var mbrParam) && mbrParam.IsBox)
                    _il.Emit(OpCodes.Ldarg, mbrParam.ArgIndex);
                else
                    _il.Emit(OpCodes.Ldloc, GetOrDeclareLocal(mbr.SymbolId));
                break;
            case BinaryExpr b: EmitBinaryExpression(b); break;
            case UnaryExpr u: EmitUnaryExpression(u); break;
            // AssignmentExpr come ESPRESSIONE (es. `a = b` dentro un'espressione più grande):
            // assegna il valore al target e lo lascia sullo stack (semantica "value of
            // assignment"), stesso pattern del VM VisitAssignmentExpression.
            case AssignmentExpr ae:
                if (ae.Target is VariableTarget avt)
                {
                    // Bug corretto (2026-08-31, audit closures/lambda): il vecchio approccio
                    // pushava ae.Value UNA VOLTA qui fuori, poi passava a EmitStoreVariable un
                    // callback "Dup + Coerce" assumendo che il valore fosse in cima allo stack
                    // nel momento in cui il callback viene invocato. Vero SOLO per il ramo
                    // "local semplice" di EmitStoreVariable (che chiama emitValue() senza
                    // pushare altro prima) — FALSO per i rami campo-catturato/globale/
                    // static/box, che pushano un proprio operando (Ldarg_0 per il campo
                    // catturato, l'indice globale, l'argomento box, ...) PRIMA di chiamare
                    // emitValue(): il Dup duplicava quell'operando, non ae.Value, corrompendo
                    // lo store (bug: `count := count + 1` dentro una closure scriveva il
                    // riferimento oggetto `this` invece del valore incrementato nel campo
                    // catturato — verificato decompilando l'IL generato, "capture_520 =
                    // (double)this" invece di "capture_520 = num + 1.0"). Fix: usa il percorso
                    // di store STANDARD (identico a un'assegnazione-istruzione, che già
                    // gestisce correttamente OGNI ramo), poi rilegge il valore appena scritto
                    // per lasciarlo sullo stack come risultato dell'espressione — ae.Value
                    // viene così valutato UNA sola volta (dentro EmitStoreVariable), niente
                    // doppia valutazione per espressioni con effetti collaterali.
                    EmitStoreVariable(avt.SymbolId, ae.Value);
                    EmitLoadVariable(avt.SymbolId);
                }
                else if (ae.Target is FieldTarget aft)
                    EmitFieldAssignment(aft, ae.Value);
                else throw new NotSupportedException($"Backend IL: target di AssignmentExpr '{ae.Target.GetType().Name}' non supportato.");
                break;
            case JsonToObjectExpr jtoe: EmitJsonToObject(jtoe); break;
            case InterpolatedStringExpr ise: EmitInterpolatedString(ise); break;
            case SwitchExpr sw: EmitSwitchExpression(sw); break;
            case TypeExpr te2: EmitTypeExpr(te2); break;
            case GeneratorExpr ge: EmitGeneratorExpr(ge); break;
            case BuiltinFunctionCallExpr f: EmitBuiltinFunctionCall(f); break;
            case ArrayAccessExpr a: EmitArrayAccess(a); break;
            case FunctionExpr fn: EmitFunctionCall(fn); break;
            case NewExpr ne: EmitNewExpression(ne); break;
            case FieldAccessExpr fa: EmitFieldAccess(fa); break;
            case MethodCallExpr mc: EmitMethodCall(mc); break;
            case UnresolvedCallExpr uce: EmitUnresolvedCall(uce); break;
            case PropertyAccessExpr pa: EmitPropertyAccess(pa); break;
            case IndexedAccessExpr ia: EmitIndexedAccess(ia); break;
            // EVAL({ |p| ... }, args...) — lambda INLINE come target: il rewrite del Linker
            // rende FunctionExpr indiretto solo per le VARIABILI; qui la lambda viene
            // materializzata e invocata direttamente (prima: NotSupportedIlExpressionException).
            case EvalExpr ev when ev.Target is LambdaExpr evLe: EmitInlineLambdaEval(ev, evLe); break;
            // EVAL(<espressione che ritorna un block>, args...) — es. EVAL(Mk(), 5):
            // valuta il target (delegate) e invoca via Invoke del tipo funzione.
            case EvalExpr evFn when ResolveEvalTargetFpt(evFn.Target) is FunctionPointerType fptEv:
            {
                var delType = DataTypeMapper.Map(fptEv, _types, _classTypes);
                var invokeFn = delType.GetMethod("Invoke")
                    ?? throw new NotSupportedException($"Backend IL: metodo Invoke non trovato sul delegate '{delType}'.");
                var tmp = _il.DeclareLocal(delType);
                EmitExpression(evFn.Target);
                _il.Emit(OpCodes.Castclass, delType);
                _il.Emit(OpCodes.Stloc, tmp);
                _il.Emit(OpCodes.Ldloc, tmp);
                for (var i = 0; i < evFn.Args.Length; i++)
                {
                    EmitExpression(evFn.Args[i]);
                    if (i < fptEv.ParameterTypes.Length)
                        EmitCoerceTypes(NormalizeInferredType(InferType(evFn.Args[i])),
                            NormalizeInferredType(fptEv.ParameterTypes[i]));
                }
                _il.Emit(OpCodes.Callvirt, invokeFn);
                break;
            }
            case LambdaExpr le: EmitLambdaCreation(le); break;
            case TupleLiteralExpr tuple: EmitTupleLiteral(tuple); break;
            case ListComprehensionExpr lc: EmitListComprehension(lc); break;
            case DictComprehensionExpr dc: EmitDictComprehension(dc); break;
            case SetComprehensionExpr sc: EmitSetComprehension(sc); break;
            case ListLiteralExpr list: EmitListLiteral(list, null); break;
            case DictionaryLiteralExpr dictionary: EmitDictionaryLiteral(dictionary, null); break;
            case WithMemberAccessExpr member: EmitWithMemberAccess(member); break;
            case GroupingExpr g: EmitExpression(g.Expression); break;
            case SafeCastExpr safeCast: EmitSafeCast(safeCast); break;
            // Macro &expr usata come VALORE: prima era un passthrough puro (bug — valutava
            // l'espressione INTERNA e ne ritornava il valore, es. x = &("1+2") assegnava la
            // stringa "1+2", mai il numero 3; vedi report Obsidian
            // Dynamic-Code-EVAL-Macro-Backends). Ora compila ed esegue davvero il testo a
            // runtime (IlRuntime.MacroEvalExpr), stessa logica di EmitMacroStatement per la
            // forma statement. NOTA: samples/graphics/GraphicsDrawTextures.bas usa
            // &hFFFFFFFF (identificatore nudo, non &("codice")) per un letterale colore — questo
            // dialetto non ha supporto letterali esadecimali nel Lexer, quindi quell'uso dipendeva
            // dal vecchio passthrough e molto probabilmente produceva già silenziosamente 0/NIL
            // (variabile "hFFFFFFFF" mai dichiarata). Con questo fix fallirà in modo rumoroso
            // invece che silenzioso — il sample va corretto separatamente (letterale numerico
            // reale, non l'operatore macro), non è una regressione di un comportamento corretto.
            case MacroExpr macro: EmitMacroExpression(macro); break;
            case HttpFetchExpr hf: EmitHttpFetchExpr(hf); break;
            // BASE (in una classe derivata): l'istanza della base È SELF — Ldarg 0.
            case BaseExpr:
                if (!_isInstanceMember)
                    throw new NotSupportedException("Backend IL: BASE usato fuori da un metodo di istanza.");
                _il.Emit(OpCodes.Ldarg, 0);
                break;
            case FunctionReferenceExpr fre when IsCallbackFunctionReferenceCore(fre.SymbolId, _program): EmitFunctionReference(fre); break;
            case FunctionReferenceExpr fre: EmitFunctionReferenceAsCall(fre); break;
            case ArrayLiteralExpr literal: EmitArrayLiteralExpression(literal); break;
            // x HAS VALUE (parità VM VisitNullableHasValueExpression: carica, PUSH_NIL,
            // CMP_NE) — il valore nullable è object (boxed o null) → NON è null.
            // Chiamata runtime opaca IlRuntime.NullableHasValue (NON sequenza inline
            // load; ldnull; ceq; ldc.i4.0; ceq): ILOptim/SimplifyInsts ripiegava il
            // pattern inline a costante 0 — `x HAS VALUE` sempre falso su IL
            // ottimizzato (2026-09-23, Parita-5-Backend-Residui §2, TypeInfoFullCoverage;
            // minitests/nullable/nullable_hasvalue.bas perdeva "val-has"). Il Box dei
            // value-type specchia IS NIL (EmitBinaryExpression). Ritorna -1/0
            // (convenzione BASIC, parità PCodeVM/VM-C).
            case NullableHasValueExpr nhv:
                EmitExpression(nhv.Expression);
                var hvIsRedefined = nhv.Expression is VariableExpr hvV && _redefinedTypeSymbols.Contains(hvV.SymbolId);
                if (!hvIsRedefined)
                {
                    var hvClr = DataTypeMapper.Map(NormalizeInferredType(InferType(nhv.Expression)), _types, _classTypes);
                    if (hvClr.IsValueType) _il.Emit(OpCodes.Box, hvClr);
                }
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NullableHasValue), typeof(object)));
                break;
            case NullableValueExpr nv when NormalizeInferredType(InferType(nv.Expression)) is RegexMatchType:
                {
                    var fieldName = nv.ValueToken.Lexeme ?? nv.ValueToken.ToString() ?? "";
                    var prop = typeof(Basic.RegexNative.RegexMatchValue).GetProperties()
                        .FirstOrDefault(p => p.Name.Equals(fieldName, StringComparison.OrdinalIgnoreCase))
                        ?? throw new NotSupportedException($"Backend IL: campo REGEXMATCH '{fieldName}' non trovato.");
                    EmitExpression(nv.Expression);
                    _il.Emit(OpCodes.Callvirt, prop.GetGetMethod()!);
                    if (prop.PropertyType == typeof(int)) _il.Emit(OpCodes.Conv_R8);
                    break;
                }
            case NullableValueExpr nv when NormalizeInferredType(InferType(nv.Expression)) is Basic.JsonType:
                // "X.VALUE" dinamico (vedi InferType, stesso caso) — identico a un
                // FieldAccessExpr con campo "Value" via JsonFieldGet, mai un unwrap Nullable<T>.
                EmitExpression(nv.Expression);
                var nvClrType = DataTypeMapper.Map(NormalizeInferredType(InferType(nv.Expression)), _types, _classTypes);
                if (nvClrType.IsValueType) _il.Emit(OpCodes.Box, nvClrType);
                _il.Emit(OpCodes.Ldstr, "Value");
                _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
                break;
            case NullableValueExpr nv: EmitExpression(nv.Expression); break;
            case QualifiedNameExpr qne: EmitQualifiedNameReference(qne); break;
            case TernaryExpr te: EmitTernaryExpression(te); break;
            case PrefixIncrementExpr pie: EmitIncrementExpression(pie.Target, pie.Operator.Kind == TokenKind.PlusPlus, postfix: false); break;
            case PostfixIncrementExpr poie: EmitIncrementExpression(poie.Target, poie.Operator.Kind == TokenKind.PlusPlus, postfix: true); break;
            // Argomento per nome (f(parm: 1)): il nome è già stato risolto dal linker
            // nell'ordine giusto — si valuta solo il valore (come il P-Code).
            case NamedArgumentExpr nae: EmitExpression(nae.Value); break;
            // Stringa letterale con marker di suffisso (es. x$ = "abc"$): il valore è
            // la stringa pura (il suffisso è sintassi), come il P-Code.
            case CheckedStringExpr cse: _il.Emit(OpCodes.Ldstr, cse.Value); break;
            // arr[start:end] — slice di array (P-Code ARRAY_SLICE). start/end opzionali
            // (null = dall'inizio / fino alla fine). Ritorna un nuovo array copiato.
            case ArraySliceExpr ase:
            {
                // Il valore base può essere un double[] (ArrayType) O una List<T> (ListType):
                // `DIM a AS ARRAY OF DOUBLE = [1,2,3,4]` / `DIM l AS LIST OF INTEGER = [...]`
                // producono una lista. Il vecchio codice chiamava SEMPRE ArraySlice(double[])
                // → InvalidCastException/ArgumentOutOfRangeException a runtime (bug reale).
                var sliceBase = NormalizeInferredType(InferType(ase.Base));
                EmitExpression(ase.Base);
                if (ase.Start != null) EmitInt32Argument(ase.Start);
                else _il.Emit(OpCodes.Ldc_I4_0);
                if (ase.End != null) EmitInt32Argument(ase.End);
                else _il.Emit(OpCodes.Ldc_I4_M1); // -1 = fino alla fine
                if (sliceBase is ListType)
                {
                    var elemClr = DataTypeMapper.Map(CollectionElementType(sliceBase), _types, _classTypes);
                    _il.Emit(OpCodes.Call, ArraySliceListMethod.MakeGenericMethod(elemClr));
                }
                else
                {
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ArraySlice), typeof(double[]), typeof(int), typeof(int)));
                }
                break;
            }
            // DIRECTCAST(expr, Type): estrae il valore boxed al tipo target (come il C99
            // gmb_val_t → .v_string/.v_int/.v_double). STRING/List/cast non-numerici via
            // IlRuntime.DirectCast (boxed object); numerici via CoerceToDouble.
            case DirectCastExpr dce:
                // Numerico (LONG/INTEGER/DOUBLE/...): estrae il double dal boxed.
                // Tutto il resto (STRING/List/Class/StringBuilder/Regex/...) resta boxed
                // object (il cast al tipo statico lo fa il call site / EmitStoreVariable).
                if (IlConversion.IsNumeric(NormalizeInferredType(dce.TargetType)))
                {
                    EmitObjectBoxedArgument(dce.Expression);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
                }
                else
                {
                    EmitObjectBoxedArgument(dce.Expression);
                    _il.Emit(OpCodes.Ldstr, dce.TargetType is StringType ? "STRING" : "OBJECT");
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DirectCast), typeof(object), typeof(string)));
                    // Cast al tipo CLR statico del target (List<T>/StringBuilder/...):
                    // il call site (es. lst2.Count) si aspetta il tipo CLR, non object.
                    var targetClr = DataTypeMapper.Map(NormalizeInferredType(dce.TargetType), _types, _classTypes);
                    if (targetClr != _types.Object && !targetClr.IsValueType)
                        _il.Emit(OpCodes.Castclass, targetClr);
                }
                break;
            // AWAIT task: le ASYNC FUNCTION in IL sono sincrone (la chiamata produce già il
            // risultato) — l'await valuta semplicemente il target (come il P-Code, che
            // esegue il task e ne prende il valore).
            case AwaitExpr awe: EmitExpression(awe.Target); break;
            // GO FUNCTION f(...) [IN wg] — in IL la GO è sincrona: chiama la funzione e
            // restituisce il risultato (il WaitGroup è no-op, vedi WAIT). Come il P-Code
            // che esegue il task inline nel back-end IL.
            case GoFunctionExpr gfe:
                if (gfe.WaitGroup is not null) { EmitExpression(gfe.WaitGroup); _il.Emit(OpCodes.Pop); }
                EmitFunctionCall(new FunctionExpr(gfe.SymbolId, gfe.OriginalToken, gfe.Args, gfe.ReturnType));
                break;
            // expr?.membro — accesso sicuro: se expr è NIL pusha il default del campo,
            // altrimenti accede al campo (come il P-Code: NIL → risultato nil).
            case NullConditionalAccessExpr nca when nca.Kind == NullConditionalKind.Member && nca.MemberToken is { } memTok:
            {
                var structType = UnwrapNullableType(NormalizeInferredType(InferType(nca.Expression)));
                var fieldSymbol = ResolveFieldOrPropertySymbolIdByName(structType, memTok.Lexeme);
                var fieldDataType = _program.Symbols.GetSymbol(fieldSymbol).DataType;
                var fieldClr = DataTypeMapper.Map(fieldDataType, _types, _classTypes);
                EmitObjectBoxedArgument(nca.Expression);
                var isNullLabel = _il.DefineLabel();
                var endLabel = _il.DefineLabel();
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ceq);
                _il.Emit(OpCodes.Brtrue, isNullLabel);
                // base non-null: legge il campo dall'istanza UDT.
                var baseClr = DataTypeMapper.Map(structType, _types, _classTypes);
                if (baseClr.IsValueType)
                {
                    // UDT (value type, base = NullableType boxed — DataTypeMapper): il
                    // risultato del campo viene BOXATO a object, uniformando il tipo statico
                    // dell'INTERA espressione a un valore dinamico (vedi
                    // InferNullConditionalType, stesso trattamento "sempre boxed" del P-Code:
                    // OP_LOAD_FIELD produce un valore dinamico uniforme lì, mai un tipo CLR
                    // statico diverso a seconda del campo). Necessario perché il ramo NIL
                    // sotto pusha Ldnull (letto da PRINT/PrintFormatValue come stringa vuota,
                    // canonico deciso 2026-09-21 — AiWiki
                    // NullConditionalAccess-Nullable-UDT-Divergenza) invece del "default
                    // tipizzato" (0 per un campo numerico) che il ramo statico-per-tipo
                    // produceva prima: PCode/VM-C non hanno questa distinzione, lì ogni
                    // valore è già dinamico e NIL è NIL indipendentemente dal campo.
                    _il.Emit(OpCodes.Unbox_Any, baseClr);
                    _il.Emit(OpCodes.Ldfld, _classFields[fieldSymbol]);
                    if (fieldClr.IsValueType) _il.Emit(OpCodes.Box, fieldClr);
                    // Campo STRING * N: PCode/VM-C restituiscono il valore LOGICO (senza il
                    // padding a lunghezza fissa) attraverso `?.` — verificato: entrambi
                    // concordano su "Mario" invece di "Mario" + 27 spazi per Nome AS STRING *
                    // 32, a differenza della lettura DIRETTA `c.Nome` (non ?.), che su TUTTI
                    // e 3 i backend mostra il padding intatto. RTrim (no-op su una stringa già
                    // priva di padding) allinea IL allo stesso comportamento SOLO per questo
                    // percorso dinamico/`?.`.
                    else if (fieldDataType is StringType) _il.Emit(OpCodes.Call, IlRuntimeStringTrimEndMethod);
                    _il.Emit(OpCodes.Br, endLabel);
                    _il.MarkLabel(isNullLabel);
                    _il.Emit(OpCodes.Pop);
                    _il.Emit(OpCodes.Ldnull);
                    _il.MarkLabel(endLabel);
                }
                else
                {
                    _il.Emit(OpCodes.Castclass, baseClr);
                    _il.Emit(OpCodes.Ldfld, _classFields[fieldSymbol]);
                    _il.Emit(OpCodes.Br, endLabel);
                    _il.MarkLabel(isNullLabel);
                    _il.Emit(OpCodes.Pop);
                    EmitDefaultValue(fieldDataType);
                    _il.MarkLabel(endLabel);
                }
                break;
            }
            default:
                throw new NotSupportedIlExpressionException(expr);
        }
    }

    /// <summary>
    /// Materializza una tupla BASIC come un vettore CLR di valori tipizzati.  Il vettore
    /// viene usato soltanto come trasporto per l'espressione: le assegnazioni multiple
    /// conservano poi ogni elemento in un locale CLR del suo tipo reale, così il lato
    /// destro è valutato esattamente una volta anche nel caso `(a,b)=(b,a)`.
    /// </summary>
    private void EmitTupleLiteral(TupleLiteralExpr expr)
    {
        _il.Emit(OpCodes.Ldc_I4, expr.Elements.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var index = 0; index < expr.Elements.Length; index++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, index);
            EmitCryptoObjectArgument(expr.Elements[index].Value);
            _il.Emit(OpCodes.Stelem_Ref);
        }
    }

    private void EmitTupleAssignmentStatement(ImmutableArray<Token> variableNames, Expr tupleExpression, Stmt context)
    {
        if (tupleExpression is not TupleLiteralExpr tuple)
        {
            // Caso: (a, b, c) = variabile — emette l'espressione, poi legge per indice.
            // TupleElement ritorna un object boxed: va convertito al tipo del target
            // (InferredType → double per default) prima dello store, altrimenti il
            // riferimento viene reinterpretato come double (valori spazzatura).
            var tupleType = InferType(tupleExpression) as TupleType;
            EmitExpression(tupleExpression);
            var values = new LocalBuilder[variableNames.Length];
            for (var i = 0; i < variableNames.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Conv_R8);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TupleElement), typeof(object), typeof(double)));
                values[i] = _il.DeclareLocal(typeof(object));
                _il.Emit(OpCodes.Stloc, values[i]);
            }
            _il.Emit(OpCodes.Pop); // rimuove la tupla dalla stack
            for (var i = 0; i < variableNames.Length; i++)
            {
                var symbol = _program.Symbols.FindSymbolsByName(variableNames[i].Lexeme)
                    .FirstOrDefault(s => s.ScopeId == context.ScopeId)
                    ?? throw new NotSupportedException($"Backend IL: simbolo tupla '{variableNames[i].Lexeme}' non risolto nello scope {context.ScopeId}.");
                var elementType = tupleType is not null && i < tupleType.Elements.Length
                    ? NormalizeInferredType(tupleType.Elements[i].DataType)
                    : new DoubleType();
                var local = values[i];
                EmitStoreVariable(symbol.Id, () =>
                {
                    _il.Emit(OpCodes.Ldloc, local);
                    if (elementType is StringType)
                        _il.Emit(OpCodes.Castclass, typeof(string));
                    else if (IlConversion.IsNumeric(elementType))
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
                });
            }
            return;
        }
        if (tuple.Elements.Length != variableNames.Length)
            throw new NotSupportedException("Backend IL: il numero di valori della tupla non coincide con i target.");

        // Tutti i RHS sono catturati prima di qualunque store LHS, preservando la
        // semantica atomica della multi-assegnazione e impedendo alias accidentali.
        var vals = new LocalBuilder[tuple.Elements.Length];
        for (var index = 0; index < tuple.Elements.Length; index++)
        {
            var type = InferTupleElementType(tuple.Elements[index].Value, context.ScopeId);
            vals[index] = _il.DeclareLocal(DataTypeMapper.Map(type, _types, _classTypes));
            EmitTupleElement(tuple.Elements[index].Value, context.ScopeId);
            _il.Emit(OpCodes.Stloc, vals[index]);
        }

        for (var index = 0; index < variableNames.Length; index++)
        {
            var symbol = _program.Symbols.FindSymbolsByName(variableNames[index].Lexeme)
                .FirstOrDefault(s => s.ScopeId == context.ScopeId)
                ?? throw new NotSupportedException($"Backend IL: simbolo tupla '{variableNames[index].Lexeme}' non risolto nello scope {context.ScopeId}.");
            var sourceType = InferTupleElementType(tuple.Elements[index].Value, context.ScopeId);
            EmitStoreVariable(symbol.Id, () =>
            {
                _il.Emit(OpCodes.Ldloc, vals[index]);
                EmitCoerceTypes(sourceType, NormalizeInferredType(symbol.DataType));
            });
        }
    }

    // Il resolver corrente non discende nei TupleLiteralExpr, quindi una variabile
    // interna conserva SymbolId=-1. Il backend la risolve nello scope già assegnato
    // al TupleAssignmentStmt, senza estendere il workaround al resto dell'AST.
    private DataType InferTupleElementType(Expr expression, int scopeId) => expression is VariableExpr { SymbolId: < 0 } variable
        ? NormalizeInferredType(ResolveTupleVariable(variable, scopeId).DataType)
        : NormalizeInferredType(InferType(expression));

    private void EmitTupleElement(Expr expression, int scopeId)
    {
        if (expression is VariableExpr { SymbolId: < 0 } variable)
        {
            EmitLoadVariable(ResolveTupleVariable(variable, scopeId).Id);
            return;
        }
        EmitExpression(expression);
    }

    private SymbolTable.Symbol ResolveTupleVariable(VariableExpr variable, int scopeId) =>
        _program.Symbols.FindSymbolsByName(variable.NameCleaned.Lexeme)
            .FirstOrDefault(s => s.ScopeId == scopeId)
        ?? throw new NotSupportedException($"Backend IL: variabile tupla '{variable.NameCleaned.Lexeme}' non risolta nello scope {scopeId}.");



    /// <summary>Loop annidati condivisi dalle comprehension (lista/dict/set): per ogni
    /// clausola genera un for sull'array sorgente; l'inner più profondo è il delegato
    /// del consumatore. GetValue boxa (object) — ToDouble per la convenzione numerica.</summary>


    private void EmitWithMemberAccess(WithMemberAccessExpr expr)
    {
        if (expr.Arguments is not null)
            throw new NotSupportedException("Backend IL: chiamate di metodo implicite dentro WITH non ancora supportate.");
        if (_withTargets.Count == 0)
            throw new NotSupportedException("Backend IL: accesso '.Membro' fuori da un blocco WITH.");
        var (_, receiverType) = _withTargets.Peek();
        var memberId = ResolveFieldOrPropertySymbolIdByName(receiverType, expr.MemberNameToken.Lexeme);
        if (_classFields.TryGetValue(memberId, out var field))
        {
            if (field.IsStatic) _il.Emit(OpCodes.Ldsfld, field);
            else { _il.Emit(OpCodes.Ldloc, _withTargets.Peek().Local); _il.Emit(OpCodes.Ldfld, field); }
            return;
        }
        if (_classProperties.TryGetValue(memberId, out var property) && property.Getter is not null)
        {
            _il.Emit(OpCodes.Ldloc, _withTargets.Peek().Local);
            EmitPropertyAccessorCall(property.Getter);
            return;
        }
        throw new NotSupportedException($"Backend IL: membro WITH '{expr.MemberNameToken.Lexeme}' non leggibile.");
    }

    /// <summary>
    /// NEW ClassName(args): `newobj` sul ConstructorBuilder risolto (per numero di
    /// argomenti, tra gli overload — vedi ResolveConstructorOverload) — copertura
    /// attuale solo classi utente semplici (vedi CompileToAssembly, Pass 0/1), non
    /// array/List/altre collezioni built-in create con NEW.
    /// </summary>
    private void EmitNewExpression(NewExpr expr)
    {
        // NEW HTTPCONFIG/HTTPOPTIONS: oggetto dinamico (Dictionary<string,object?>) come
        // il VM (typeCode fallback) — i campi cfg.Port = 8096 passano da JsonFieldSet.
        if (expr.ParsedType is HttpConfigType or HttpOptionsType or CookieJarType)
        {
            if (expr.ParsedType is CookieJarType)
            {
                var cjCtor = typeof(CommonLib.Http.CookieJar).GetConstructor(Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: ctor CookieJar non trovato.");
                _il.Emit(OpCodes.Newobj, cjCtor);
                return;
            }
            if (expr.ParsedType is HttpOptionsType)
            {
                // NEW HTTPOPTIONS: usa il metodo helper statico per creare il dict con tutti i default
                _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod("NewHttpOptionsDict", System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                    ?? throw new NotSupportedException("Backend IL: metodo NewHttpOptionsDict non trovato."));
                return;
            }
            if (expr.ParsedType is HttpConfigType)
            {
                // NEW HTTPCONFIG: dict con i default canonici (IlRuntime.NewHttpConfigDict →
                // PCodeProcessor.NewHttpConfigDefaults, unica sorgente con la gamba PCode).
                _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod("NewHttpConfigDict", System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                    ?? throw new NotSupportedException("Backend IL: metodo NewHttpConfigDict non trovato."));
                return;
            }
            return;
        }
        // NEW di strutture avanzate (Tree/AVL/Trie/Heap/Bag/Cache/Timeline/Ring/Graph/
        // HashRing/SparseSet/BPlusTree): rappresentate come object nel DataTypeMapper —
        // materializza un SortedDictionary (come il VM per Tree/AVL, typeCode 7-8) o la
        // classe CommonLib dedicata dove esiste. Il dispatch runtime (CollectionInvoke)
        // opera sul tipo reale.
        if (expr.ParsedType is TreeType or AVLTreeType or RedBlackTreeType or BinaryIndexedTreeType
            or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType or BinaryHeapType
            or BagType or CacheType or TimelineType or RingBufferType)
        {
            if (expr.InitialValues is not null || expr.ConstructorArgs is { Length: > 0 })
            {
                // NEW con initializer/args: per le classi CommonLib reali usa il ctor con args;
                // per il resto, ignora e crea il dict (approccio pragmatico, il sample usa
                // NEW RINGBUFFER(3) che ha un ctor reale).
                if (expr.ParsedType is RingBufferType && expr.ConstructorArgs is { Length: 1 })
                {
                    var ringClr = DataTypeMapper.Map(expr.ParsedType, _types, _classTypes);
                    var ringCtor = ringClr.GetConstructor([typeof(int)])
                        ?? throw new NotSupportedException("Backend IL: ctor RINGBUFFER(int) non trovato.");
                    EmitExpression(expr.ConstructorArgs.Value[0]);
                    // Il literal BASIC è double (ldc.r8 3) ma il ctor è (int) — senza
                    // conv.i4, newobj con un double per un param Int32 = IL invalido.
                    _il.Emit(OpCodes.Conv_I4);
                    _il.Emit(OpCodes.Newobj, ringCtor);
                    return;
                }
                if (expr.ParsedType is HashRingType && expr.ConstructorArgs is { Length: 1 })
                {
                    // NEW HASHRING(nodes): ConsistentHash<object> popolato (il ctor
                    // generico con IEnumerable<object> non accetta List<string>).
                    EmitExpression(expr.ConstructorArgs.Value[0]);
                    _il.Emit(OpCodes.Ldc_I4, 100);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NewHashRing), typeof(System.Collections.IEnumerable), typeof(int)));
                    return;
                }
                if (expr.ParsedType is BinaryIndexedTreeType && expr.ConstructorArgs is { Length: 1 })
                {
                    // NEW BITREE<T>([1,2,3,4,5]): il ctor CommonLib vuole double[] — prima
                    // si cadeva nel default (SortedDictionary vuoto) e Query restava 0
                    // (CollectionsModern §10: IL 0/0/10 vs 6/9/19 delle altre gambe).
                    var bitArg = expr.ConstructorArgs.Value[0];
                    if (bitArg is ListLiteralExpr bitList) EmitTypedArrayLiteral(bitList.Elements, new DoubleType());
                    else if (bitArg is ArrayLiteralExpr bitArr) EmitTypedArrayLiteral(bitArr.Elements, new DoubleType());
                    else
                    {
                        EmitExpression(bitArg);
                        var bitClr = DataTypeMapper.Map(NormalizeInferredType(InferType(bitArg)), _types, _classTypes);
                        if (bitClr != typeof(double[])) IlConversion.EmitConvert(_il, _types, NormalizeInferredType(InferType(bitArg)), bitClr, typeof(double[]));
                    }
                    _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.BinaryIndexedTree<double>)
                        .GetConstructor([typeof(double[])])!);
                    return;
                }
                if (expr.ParsedType is SparseSetType && expr.ConstructorArgs is { Length: 2 })
                {
                    var spClr = DataTypeMapper.Map(expr.ParsedType, _types, _classTypes);
                    var spCtor = spClr.GetConstructor([typeof(int), typeof(int)])
                        ?? throw new NotSupportedException("Backend IL: ctor SPARSESET(int,int) non trovato.");
                    foreach (var a in expr.ConstructorArgs.Value) EmitInt32Argument(a);
                    _il.Emit(OpCodes.Newobj, spCtor);
                    return;
                }
            }
            // Default: per le classi CommonLib reali senza args usa la classe dedicata
            // (Cache → Cache<object,object>, Timeline → Timeline<object>, ...), altrimenti
            // SortedDictionary (chiave ordinata, come il VM per Tree/AVL).
            if (expr.ParsedType is CacheType)
            {
                var cacheClr = typeof(CommonLib.Collections.Cache<object, object?>);
                var cacheCtor = cacheClr.GetConstructor(Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: ctor CACHE non trovato.");
                _il.Emit(OpCodes.Newobj, cacheCtor);
                return;
            }
            if (expr.ParsedType is GraphType)
            {
                // GRAPH<K> → Graph<object> (CommonLib), NON SortedDictionary: AddNode/AddEdge
                // e gli algoritmi canonici (ShortestPath/Dijkstra/PageRank/...) esistono solo
                // su Graph<T> (parità con il VM e con gmb_graph.c).
                var graphClr = typeof(CommonLib.Collections.Graph<object>);
                var graphCtor = graphClr.GetConstructor(Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: ctor GRAPH non trovato.");
                _il.Emit(OpCodes.Newobj, graphCtor);
                return;
            }
            var sdType = typeof(SortedDictionary<object, object?>);
            var sortedCtor = sdType.GetConstructor(Type.EmptyTypes)
                ?? throw new NotSupportedException("Backend IL: ctor SortedDictionary non trovato.");
            _il.Emit(OpCodes.Newobj, sortedCtor);
            return;
        }

        // NEW REBALANCESTRATEGY/BACKTESTRESULT (TRADING.BACKTESTS): oggetto dinamico
        // (Dictionary<string,object?> OrdinalIgnoreCase) — i campi Strat.Frequency = 3
        // e Result.TotalReturn passano da JsonFieldSet/JsonFieldGet, come nel VM.
        if (expr.ParsedType is RebalanceStrategyType or BacktestResultType)
        {
            var bsDictType = typeof(Dictionary<string, object?>);
            var bsCtor = bsDictType.GetConstructor([typeof(StringComparer)])
                ?? bsDictType.GetConstructor(Type.EmptyTypes)
                ?? throw new NotSupportedException("Backend IL: ctor Dictionary<string,object?> non trovato.");
            if (bsCtor.GetParameters().Length == 1)
            {
                var comparerGetter = typeof(StringComparer).GetProperty(nameof(StringComparer.OrdinalIgnoreCase))!.GetGetMethod()
                    ?? throw new NotSupportedException("Backend IL: getter StringComparer.OrdinalIgnoreCase non trovato.");
                _il.Emit(OpCodes.Call, comparerGetter);
            }
            _il.Emit(OpCodes.Newobj, bsCtor);
            return;
        }

        // NEW JSON: oggetto dinamico come nel VM (PCodeProcessor OP_NEW codice 24 →
        // Dictionary<string,object?> con comparer ordinal-ignore-case) — i campi dinamici
        // risposta.status = "ok" scrivono su questo dict via JsonFieldSet.
        if (expr.ParsedType is JsonType)
        {
            if (expr.InitialValues is not null || expr.ConstructorArgs is { Length: > 0 })
                throw new NotSupportedException("Backend IL: NEW JSON supporta solo il costruttore vuoto.");
            var dictType = typeof(Dictionary<string, object?>);
            var jsonCtor = dictType.GetConstructor([typeof(StringComparer)])
                ?? dictType.GetConstructor(Type.EmptyTypes)
                ?? throw new NotSupportedException("Backend IL: costruttore Dictionary<string,object?> non trovato.");
            if (jsonCtor.GetParameters().Length == 1)
            {
                // OrdinalIgnoreCase è una PROPRIETÀ statica (getter), non un campo.
                var comparerGetter = typeof(StringComparer).GetProperty(nameof(StringComparer.OrdinalIgnoreCase))!.GetGetMethod()
                    ?? throw new NotSupportedException("Backend IL: getter StringComparer.OrdinalIgnoreCase non trovato.");
                _il.Emit(OpCodes.Call, comparerGetter);
            }
            _il.Emit(OpCodes.Newobj, jsonCtor);
            return;
        }

        if (expr.ParsedType is StringBuilderType)
        {
            var sbType = typeof(System.Text.StringBuilder);
            if (expr.ConstructorArgs is { Length: 1 })
            {
                var sbCapCtor = sbType.GetConstructor([typeof(int)])
                    ?? throw new NotSupportedException("Backend IL: ctor StringBuilder(int) non trovato.");
                EmitInt32Argument(expr.ConstructorArgs.Value[0]);
                _il.Emit(OpCodes.Newobj, sbCapCtor);
            }
            else
            {
                var sbCtor = sbType.GetConstructor(Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: ctor StringBuilder() non trovato.");
                _il.Emit(OpCodes.Newobj, sbCtor);
            }
            return;
        }

        if (expr.ParsedType is ListType or DictType or HashSetType or QueueType or StackType or SortedDictType or LinkedListType)
        {
            if (expr.InitialValues is { Length: > 0 })
                throw new NotSupportedException($"Backend IL: NEW {expr.ParsedType} con inizializzatore non ancora supportato.");
            // NEW LIST<STRING>(collection): ctor con IEnumerable<T> (List<T> lo ha).
            if (expr.ConstructorArgs is { Length: 1 } listArgs && expr.ParsedType is ListType)
            {
                var listClr2 = DataTypeMapper.Map(expr.ParsedType, _types, _classTypes);
                var elemClr2 = DataTypeMapper.Map(((ListType)expr.ParsedType).ElementType, _types, _classTypes);
                var listIEnumCtor = listClr2.GetConstructor([typeof(IEnumerable<>).MakeGenericType(elemClr2)])
                    ?? throw new NotSupportedException($"Backend IL: ctor {expr.ParsedType}(IEnumerable) non trovato.");
                EmitExpression(listArgs[0]);
                _il.Emit(OpCodes.Newobj, listIEnumCtor);
                return;
            }
            if (expr.ConstructorArgs is { Length: > 0 })
                throw new NotSupportedException($"Backend IL: NEW {expr.ParsedType} supporta al momento solo il costruttore vuoto.");

            EmitNewNativeCollection(expr.ParsedType);
            return;
        }

        // ClassType avvolge la ClassDefinition; UserDefinedType (UDT) È esso stesso il
        // DataType (mai avvolto — vedi Ast/DataTypes.UdtClasses.cs). `newobj` su un
        // costruttore di value type push il VALORE (non un riferimento) per costruzione
        // ECMA-335 — nessuna distinzione di codegen qui, l'unica differenza è come si
        // ricava SymbolId/nome per la risoluzione dell'overload.
        var (typeSymbolId, typeName) = expr.ParsedType switch
        {
            ClassType ct => (ct.Definition.SymbolId, ct.Definition.OriginalName.Lexeme),
            UserDefinedType udt => (udt.SymbolId, udt.OriginalName.Lexeme),
            _ => throw new NotSupportedException($"Backend IL: NEW di tipo '{expr.ParsedType}' non ancora supportato (solo classi/UDT utente con un costruttore).")
        };
        if (!_classConstructors.TryGetValue(typeSymbolId, out var candidates))
            throw new NotSupportedException($"Backend IL: NEW di tipo '{expr.ParsedType}' non ancora supportato (nessun costruttore registrato).");
        if (expr.InitialValues is not null)
            throw new NotSupportedException("Backend IL: NEW con inizializzatore {...} non ancora supportato.");

        var args = expr.ConstructorArgs ?? ImmutableArray<Expr>.Empty;
        var ctor = ResolveConstructorOverload(candidates, args.Length, typeName);

        foreach (var arg in args) EmitExpression(arg);
        _il.Emit(OpCodes.Newobj, ctor.Builder);
    }

    /// <summary>
    /// Un generico chiuso (es. List&lt;Person&gt;) "contiene" un TypeBuilder se uno dei suoi
    /// argomenti generici è un TYPE/CLASS utente (TypeBuilder reale) — condizione ESATTA sotto
    /// cui Reflection.Emit richiede il ponte TypeBuilder.GetConstructor/GetMethod invece della
    /// risoluzione diretta (.GetConstructor()/.GetMethods()), che per un generico chiuso
    /// "normale" (es. List&lt;string&gt;, nessun argomento TypeBuilder) funziona già e va usata
    /// direttamente — il ponte lì fallirebbe con "must be or must contain a TypeBuilder as a
    /// generic argument". Controllo deterministico sugli argomenti generici, non un
    /// try/catch, per non nascondere altri NotSupportedException legittimi.
    /// </summary>
    private static bool ClosedGenericContainsTypeBuilder(Type closedGenericType)
        => closedGenericType.IsGenericType &&
           closedGenericType.GetGenericArguments().Any(a => a is TypeBuilder || (a.IsGenericType && ClosedGenericContainsTypeBuilder(a)));


    /// <summary>Spinge un Nullable&lt;T&gt; "null" (HasValue=false) sullo stack: per i
    /// parametri opzionali di un ctor (es. Cache(int? capacity, TimeSpan? cleanup)) —
    /// ldc.i4.0 + newobj Nullable&lt;T&gt;.ctor(T) con un default produce HasValue=true;
    /// initobj produce il default struct (HasValue=false).</summary>
    private void EmitNullableDefault(Type nullableType)
    {
        var underlying = Nullable.GetUnderlyingType(nullableType);
        if (underlying is null) { _il.Emit(OpCodes.Ldnull); return; }
        var tmp = _il.DeclareLocal(nullableType);
        _il.Emit(OpCodes.Ldloca, tmp);
        _il.Emit(OpCodes.Initobj, nullableType);
        _il.Emit(OpCodes.Ldloc, tmp);
    }



    /// <summary>
    /// Nucleo condiviso di EmitListLiteral, generalizzato su ImmutableArray&lt;Expr&gt; invece
    /// che su ListLiteralExpr — riusato per un ArrayLiteralExpr {...} assegnato a un target
    /// LIST OF T (vedi EmitStoreVariable(int,Expr)): il parser produce SEMPRE ArrayLiteralExpr
    /// per {...}, mai ListLiteralExpr, quindi serve poter costruire una List&lt;T&gt; anche da
    /// quello, non solo dal letterale [...] originale.
    /// </summary>

    /// <summary>
    /// Array CLR 1D (T[]) da un letterale {...} col tipo elemento REALE del target — stessa
    /// identica logica del ramo rank==1 di EmitArrayDeclaration (Newarr + Stelem per indice),
    /// riusata qui per il caso "array a dimensione implicita" (<c>DIM x() AS T = {...}</c>,
    /// Dimensions vuoto quindi instradato da EmitScalarDeclaration invece che
    /// EmitArrayDeclaration) dove EmitStoreVariable(int,Expr) non aveva altrimenti modo di
    /// costruire un array tipizzato. NON aggiorna <see cref="_arrays"/>: il chiamante
    /// (EmitScalarDeclaration) già usa GetOrDeclareLocal, un array "implicito" così creato non
    /// supporta indicizzazione via EmitArrayAccess (limite pre-esistente, non introdotto qui —
    /// solo FOR EACH via IlRuntime.ForEachValues/Entries lo consuma correttamente).
    /// </summary>



    /// <summary>
    /// Lettura di un campo O una PROPERTY (es. SELF.Nome, o Classe.CampoStatico): il
    /// Parser genera lo STESSO FieldAccessExpr per tutti questi casi (non solo in
    /// scrittura — vedi EmitFieldAssignment). Se FieldSymbolId non è risolto (-1 — capita
    /// per l'accesso a un campo STATIC tramite nome di classe, scoperto testando questa
    /// feature: un altro gap del Linker, stesso genere di quello già aggirato per
    /// PropertyAccessExpr), lo risolve per nome nella classe indicata da Structure. Un
    /// campo STATIC salta il caricamento dell'istanza (Ldsfld diretto, nessun `this`);
    /// un campo d'istanza valuta prima Structure (Ldfld).
    /// </summary>
    private void EmitFieldAccess(FieldAccessExpr expr)
    {
        // `DBF GET <var>` crea il SIMBOLO implicito nel Linker (RewriteDbfGetStatement) ma i
        // riferimenti successivi `var.Campo` possono restare con SymbolId<0: si risolve qui
        // per nome, così la struttura è la variabile LOCALE (dove il GET ha scritto il dict).
        if (expr.Structure is VariableExpr uvResolve && uvResolve.SymbolId < 0)
        {
            var foundIds = _program.Symbols.FindSymbolInScope(uvResolve.NameCleaned.Lexeme);
            if (foundIds is { Count: > 0 })
                expr = expr with { Structure = uvResolve with { SymbolId = foundIds[^1] } };
        }
        // Late-binding Clipper (campo nudo, es. `ITEM` dopo `DBF SELECT ORD` —
        // TryResolveLateBinding nel Linker produce FieldAccessExpr(_dbRecord, Campo) con
        // FieldSymbolId sempre non risolvibile). `_dbRecord` è un sentinella
        // (SymbolTable.DbfRecordBufferSymbolId) SENZA alcun backing store reale — non va
        // MAI emesso con EmitExpression(expr.Structure) (nessuna variabile realmente
        // dichiarata dietro), va intercettato PRIMA di qualunque altro branch che tenti di
        // valutare la struttura. Il nome campo si risolve solo a RUNTIME contro l'area
        // DBF/ISAM correntemente selezionata (quale alias è attivo dipende dal flusso
        // effettivo, non deducibile in generale a compile-time) — prima cadeva nel
        // fallback "campo non risolvibile, headless" (Ldc_R8 0.0 sempre), scartando
        // qualunque dato reale scritto dal record (vedi
        // ILBackend-DbfLateBinding-BareField-AlwaysZero, AiWiki).
        if (expr.Structure is VariableExpr dbLateBindVar && dbLateBindVar.SymbolId == SymbolTable.DbfRecordBufferSymbolId)
        {
            _il.Emit(OpCodes.Ldstr, expr.Field.Lexeme);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfLateBindField), typeof(string)));
            return;
        }

        if (TryEmitNativeCollectionFieldAccess(expr)) return;

        // SQLRESULTSET: rs.COUNT (righe) e rs.COLUMNS (colonne) — membri dello snapshot.
        var rsStructType = NormalizeInferredType(InferType(expr.Structure));
        if (rsStructType is SqlResultSetType)
        {
            var member = expr.Field.Lexeme;
            EmitExpression(expr.Structure);
            var rsType2 = NormalizeInferredType(InferType(expr.Structure));
            var rsClr2 = DataTypeMapper.Map(rsType2, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, rsType2, rsClr2, _types.Double);
            if (member.Equals("COUNT", StringComparison.OrdinalIgnoreCase))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlResultSetCount), typeof(double)));
            else if (member.Equals("COLUMNS", StringComparison.OrdinalIgnoreCase))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlResultSetColumnCount), typeof(double)));
            else
                throw new NotSupportedException($"Backend IL: membro SQLRESULTSET '{member}' non supportato (usa COUNT o COLUMNS).");
            return;
        }

        // REGEX: r.Pattern e m.Index/Length/Success/Value — campi CLR (RegexWrapper/RegexMatchValue).
        var regexStructType = NormalizeInferredType(InferType(expr.Structure));
        if (regexStructType is RegexType)
        {
            var patternProp = typeof(Basic.RegexNative.RegexWrapper).GetProperty("Pattern")!;
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Callvirt, patternProp.GetGetMethod()!);
            return;
        }
        if (regexStructType is RegexMatchType)
        {
            // I record espongono PROPERTY (Value/Index/Length/Success), non campi.
            var matchFieldName = expr.Field.Lexeme ?? expr.Field.ToString() ?? "";
            var matchProp = typeof(Basic.RegexNative.RegexMatchValue).GetProperties()
                .FirstOrDefault(p => p.Name.Equals(matchFieldName, StringComparison.OrdinalIgnoreCase));
            if (matchProp is null)
                throw new NotSupportedException($"Backend IL: campo REGEXMATCH '{matchFieldName}' non trovato.");
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Callvirt, matchProp.GetGetMethod()!);
            if (matchProp.PropertyType == typeof(int)) _il.Emit(OpCodes.Conv_R8);
            return;
        }
        // RGBA/DisplayMode: mappati a double[] — .Red/.Green/.Blue/.Alpha sono gli indici
        // 0..3 dell'array (porting dal comportamento VM/gmo).
        var rgbaStructType = NormalizeInferredType(InferType(expr.Structure));
        if (rgbaStructType is RgbaColorType)
        {
            var compName = expr.Field.Lexeme;
            var idx = compName.Equals("RED", StringComparison.OrdinalIgnoreCase) ? 0
                    : compName.Equals("GREEN", StringComparison.OrdinalIgnoreCase) ? 1
                    : compName.Equals("BLUE", StringComparison.OrdinalIgnoreCase) ? 2
                    : compName.Equals("ALPHA", StringComparison.OrdinalIgnoreCase) ? 3 : -1;
            if (idx < 0)
                throw new NotSupportedException($"Backend IL: componente '{compName}' non valido per RGBA.");
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Ldc_I4, idx);
            _il.Emit(OpCodes.Ldelem_R8);
            return;
        }
        // DISPLAYMODE (SCREENINFO()): dict {ModeID, Width, Height, Bpp, Colors, Active,
        // Description}. Prima qualunque campo ritornava 0.0 (IL stampava 0 per tutto,
        // divergenza con PCode/VM-C/C99): ora si legge il campo dal dict.
        if (rgbaStructType is DisplayModeType)
        {
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Ldstr, expr.Field.Lexeme.ToUpperInvariant());
            _il.Emit(OpCodes.Call, IlRuntimeDisplayModeFieldMethod);
            return;
        }

        // sb.Length / sb.Capacity (StringBuilder, accesso a campo senza parentesi): getter CLR.
        var sbStructType = NormalizeInferredType(InferType(expr.Structure));
        if (sbStructType is StringBuilderType
            && (expr.Field.Lexeme.Equals(nameof(System.Text.StringBuilder.Length), StringComparison.OrdinalIgnoreCase)
                || expr.Field.Lexeme.Equals(nameof(System.Text.StringBuilder.Capacity), StringComparison.OrdinalIgnoreCase)))
        {
            var sbPropName = expr.Field.Lexeme.Equals(nameof(System.Text.StringBuilder.Capacity), StringComparison.OrdinalIgnoreCase)
                ? nameof(System.Text.StringBuilder.Capacity)
                : nameof(System.Text.StringBuilder.Length);
            var sbLenGetter = typeof(System.Text.StringBuilder).GetProperty(sbPropName)!.GetGetMethod()!;
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Callvirt, sbLenGetter);
            _il.Emit(OpCodes.Conv_R8);
            return;
        }

        // Campo dinamico su un valore JSON (o l'entry di un FOR EACH a 1 variabile su un DICT,
        // vedi InferForEachElementTypes — stesso tipo statico "resta object", stesso campo
        // dinamico Key/Value): FieldSymbolId è sempre -1 (nessun simbolo REALE registrato per
        // un campo dinamico), quindi va intercettato PRIMA della risoluzione statica sotto
        // (ResolveFieldOrPropertySymbolIdByName, che lancia per un tipo non-UDT).
        var structType = NormalizeInferredType(InferType(expr.Structure));
        if (structType is Basic.JsonType or AnyType or HttpRequestType or HttpResponseType
            or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
            or SqlResultSetType or WebSocketType or PortfolioResultType or FetchResultType
            or RebalanceStrategyType or BacktestResultType
            or AnovaResultType or RegressionResultType or PcaResultType or KMeansResultType
            or ArimaResultType or HoltWintersResultType or AdfTestResultType or HmmResultType or JohansenResultType or GpResultType or SvmResultType or GmmResultType or PolyFitResultType or ParticleResultType)
        {
            EmitExpression(expr.Structure);
            var argClrType = DataTypeMapper.Map(structType, _types, _classTypes);
            if (argClrType.IsValueType) _il.Emit(OpCodes.Box, argClrType);
            _il.Emit(OpCodes.Ldstr, expr.Field.Lexeme);
            _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
            return;
        }

        // CATCH ex AS ERR: ex.Number/ex.Description non sono mai simboli registrati dal
        // Linker (ERR è uno pseudo-tipo, mai una vera CLASS/UDT — vedi Ast/DataTypes.cs,
        // ErrType), quindi FieldSymbolId è sempre -1 e ResolveFieldOrPropertySymbolIdByName
        // non li troverebbe comunque. Struttura già un Basic.ErrInstance (vedi
        // EmitBuildErrInstance) — chiama direttamente il getter del record.
        if (InferType(expr.Structure) is ErrType)
        {
            EmitExpression(expr.Structure);
            var isNumber = expr.Field.Lexeme.Equals("Number", StringComparison.OrdinalIgnoreCase);
            var isLineNumber = expr.Field.Lexeme.Equals("LineNumber", StringComparison.OrdinalIgnoreCase);
            _il.Emit(OpCodes.Call, isNumber ? _types.ErrInstanceNumberGetter
                : isLineNumber ? _types.ErrInstanceLineNumberGetter
                : expr.Field.Lexeme.Equals("Description", StringComparison.OrdinalIgnoreCase) ? _types.ErrInstanceDescriptionGetter
                : expr.Field.Lexeme.Equals("SourceLineText", StringComparison.OrdinalIgnoreCase) ? _types.ErrInstanceSourceLineTextGetter
                : throw new NotSupportedException($"Backend IL: campo '{expr.Field.Lexeme}' non supportato su ERR (solo Number/Description/LineNumber/SourceLineText)."));
            // ErrInstance.Number/LineNumber sono int32 CLR puri (vedi Ast/Exceptions.cs),
            // ma questo backend tratta OGNI valore numerico BASIC come double per
            // convenzione (vedi InferType sotto, stesso caso ErrType) — conversione qui,
            // non lì, per mantenere entrambi i lati in sincrono.
            if (isNumber || isLineNumber) _il.Emit(OpCodes.Conv_R8);
            return;
        }

        // Campo non risolvibile sul tipo STATICO (es. MODES(I).Active con MODES di tipo
        // Double/Inferred assegnato da SCREENS() che a runtime è un array di DISPLAYMODE —
        // test di sintassi GraphicsShowModes): il P-Code (tipi dinamici) lo accetta. Qui il
        // valore runtime headless è comunque un default → valuta la struttura (effetti
        // collaterali) e ritorna un valore di default numerico (0) per i campi noti.
        if (expr.FieldSymbolId < 0 && InferType(expr.Structure) is DoubleType or InferredType)
        {
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Pop);
            _il.Emit(OpCodes.Ldc_R8, 0.0);
            return;
        }

        // HLL: `.Count` senza parentesi. Non è un campo né una property CLR (Count() è un
        // METODO che stima la cardinalità) → dispatch runtime, senza passare dalla
        // risoluzione campo/property (che lancia "impossibile risolvere il campo").
        if (NormalizeInferredType(InferType(expr.Structure)) is HyperLogLogType
            && expr.Field.Lexeme.Equals("Count", StringComparison.OrdinalIgnoreCase))
        {
            EmitExpression(expr.Structure);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Conv_R8);
            return;
        }

        var fieldSymbolId = expr.FieldSymbolId >= 0
            ? expr.FieldSymbolId
            : ResolveFieldOrPropertySymbolIdByName(InferType(expr.Structure), expr.Field.Lexeme);

        if (_classFields.TryGetValue(fieldSymbolId, out var fieldBuilder))
        {
            // Un campo STRING mai assegnato è null in IL (struct Initobj), ma il canone dei
            // 4 backend è la STRINGA VUOTA: la lettura lo coalesce a "" (EmptyIfNull), così
            // TRIM/LEN/confronti coincidono con PCode/VM-C/C99Native.
            void EmitLdfldCoalesce(FieldBuilder fb)
            {
                _il.Emit(OpCodes.Ldfld, fb);
                if (fb.FieldType == typeof(string))
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EmptyIfNull), typeof(string)));
            }
            if (fieldBuilder.IsStatic) { _il.Emit(OpCodes.Ldsfld, fieldBuilder); return; }
            // Struct UDT: Ldfld richiede un INDIRIZZO. Se la struttura produce già un
            // indirizzo (parametro BYREF: EmitLoadVariable emette Ldarg T& per un UDT),
            // Ldfld diretto; altrimenti EmitExpression produce il VALORE (Ldloc o
            // Unbox_Any) → copia in un local temp e usa Ldloca. Per tipi riferimento
            // (classi/string) Ldfld sul reference è corretto.
            if (fieldBuilder.DeclaringType!.IsValueType)
            {
                // SELF in un metodo/accessor di UDT: arg 0 È GIÀ un managed pointer (T&)
                // per convenzione CLR sui metodi d'istanza di un value type → ldfld diretto
                // sul puntatore. Il ramo sotto (copia in local temp + ldloca) tratterebbe
                // il puntatore come un VALORE da copiare in un local struct → IL invalido.
                if (expr.Structure is VariableExpr sv && TryResolveSelfArgIndex(sv.SymbolId, out var selfIdx))
                {
                    _il.Emit(OpCodes.Ldarg, selfIdx);
                    EmitLdfldCoalesce(fieldBuilder);
                    return;
                }
                if (expr.Structure is VariableExpr v2
                    && _parameters.TryGetValue(v2.SymbolId, out var p2) && p2.IsByRef)
                {
                    EmitExpression(expr.Structure);
                    EmitLdfldCoalesce(fieldBuilder);
                }
                else
                {
                    EmitExpression(expr.Structure);
                    var tmpLocal = _il.DeclareLocal(fieldBuilder.DeclaringType);
                    _il.Emit(OpCodes.Stloc, tmpLocal);
                    _il.Emit(OpCodes.Ldloca, tmpLocal);
                    EmitLdfldCoalesce(fieldBuilder);
                }
            }
            else
            {
                EmitExpression(expr.Structure);
                EmitLdfldCoalesce(fieldBuilder);
                // BYREF catturato da una closure (piano Closure-ByRef-Box-Linker-Definitivo,
                // Fase 4): il campo __ClosureCtx è un ByRefBox<T> (reference type) — un Ldfld
                // su Value in più dereferenzia, stesso schema di EmitLoadVariable ramo
                // _capturedFields/local box. Senza, il valore letto sarebbe il box object
                // (conv.r8 su un reference → OverflowException in Convert.ToInt16).
                if (IsByRefBoxFieldType(fieldBuilder.FieldType))
                    _il.Emit(OpCodes.Ldfld, fieldBuilder.FieldType.GetField("Value")!);
            }
            return;
        }

        if (_classProperties.TryGetValue(fieldSymbolId, out var propInfo) && propInfo.Getter is not null)
        {
            EmitExpression(expr.Structure);
            EmitPropertyAccessorCall(propInfo.Getter);
            return;
        }

        throw new NotSupportedException($"Backend IL: campo/proprietà '{expr.Field.Lexeme}' non trovato/a, o proprietà senza GET.");
    }

    private void EmitFunctionCall(FunctionExpr expr)
    {
        if (expr.IsIndirect)
        {
            EmitIndirectFunctionCall(expr);
            return;
        }

        // GENERATOR ... YIELD: la chiamata NON esegue il corpo — crea l'iteratore (lazy) che lo
        // eseguirà passo-passo a ogni NEXT. Args boxati in object[]; il token del metodo
        // `Proc_<id>` è invocato dal runtime (IlRuntime.GeneratorCreate).
        if (_program.Symbols.GetSymbol(expr.SymbolId).DeclarationNode is GeneratorDefinition
            && _procedureMethods.TryGetValue(expr.SymbolId, out var generatorMethod))
        {
            _il.Emit(OpCodes.Ldtoken, generatorMethod);
            _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var gi = 0; gi < expr.Args.Length; gi++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, gi);
                EmitExpression(expr.Args[gi]);
                var genArgClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[gi])), _types, _classTypes);
                if (genArgClr.IsValueType) _il.Emit(OpCodes.Box, genArgClr);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GeneratorCreate), typeof(RuntimeMethodHandle), typeof(object[])));
            return;
        }

        if (!_procedureMethods.TryGetValue(expr.SymbolId, out var methodBuilder) ||
            !_procedureDefinitions.TryGetValue(expr.SymbolId, out var definition))
        {
            // GENERATOR procedure (GENERATOR Count(...) AS T ... YIELD): la macchina a stati
            // (YIELD/NEXT) non è ancora supportata dal backend IL — SKIP esplicito.
            if (_program.Symbols.GetSymbol(expr.SymbolId).DeclarationNode is GeneratorDefinition)
                throw new NotSupportedException(
                    "Backend IL: generatori PROCEDURA (GENERATOR ... YIELD / NEXT) non ancora supportati.");
            // Chiamata a una FUNCTION GENERICA non riscritta dal Linker alla sua
            // specializzazione (gap del linker: crea ResolvedSpecializations ma lascia il
            // FunctionExpr col SymbolId del TEMPLATE, che il backend esclude): risolvi la
            // specializzazione per nome+arity+ReturnType (già inferito dal linker).
            var specialized = ResolveGenericSpecialization(expr);
            if (specialized is null ||
                !_procedureMethods.TryGetValue(specialized.SymbolId, out methodBuilder) ||
                !_procedureDefinitions.TryGetValue(specialized.SymbolId, out definition))
            {
                // FFI (DECLARE ... LIB): l'ExternalProcedureDefinition non è compilata come
                // metodo IL (il backend include solo FunctionDefinition) — la chiamata va
                // instradata al servizio FFI runtime (Caricamento+dlsym+invocazione come
                // PCodeProcessor.OP_CALL_FFI). Emette la chiamata e conclude qui.
                if (expr.ResolvedDefinition is ExternalProcedureDefinition epd)
                {
                    EmitFfiCall(expr, epd);
                    return;
                }
                if (_program.Symbols.GetSymbol(expr.SymbolId).DeclarationNode is ExternalProcedureDefinition epdSym)
                {
                    EmitFfiCall(expr, epdSym);
                    return;
                }
                throw new NotSupportedException(
                    $"Backend IL: chiamata a una procedura non definita nel modulo principale ('{expr.OriginalToken.Lexeme}') non ancora supportata.");
            }
        }

        // Procedura LOCALE promossa a CLOSURE dal Linker (MethodMember d'istanza di una
        // classe __ClosureCtx — es. FunctionsLocalAndSubs: Moltiplica cattura `fattore`):
        // l'istanza del context heap-promotato va PRIMA di tutti gli argomenti + callvirt
        // (il CIL vuole this per primo). Dentro un altro metodo della STESSA closure il
        // context è il `this` corrente (Ldarg_0); nel chiamante (es. SUB Main) è il local
        // __ClosureCtx dichiarato dal Linker nel corpo corrente (FindClosureContextLocal).
        if (!methodBuilder.IsStatic)
        {
            if (_isInstanceMember && _currentClassSymbolId is int curClsSym
                && _classTypes.TryGetValue(curClsSym, out var curClsTb) && curClsTb == methodBuilder.DeclaringType)
            {
                _il.Emit(OpCodes.Ldarg_0);
            }
            else
            {
                var closureCtxLocal = FindClosureContextLocal(methodBuilder.DeclaringType!)
                    ?? throw new NotSupportedException(
                        $"Backend IL: chiamata a '{expr.OriginalToken.Lexeme}' (metodo di closure) senza istanza __context nel corpo corrente.");
                _il.Emit(OpCodes.Ldloc, closureCtxLocal);
            }
        }

        var boxArgPositions = GetByRefCapturedParamPositions(_program).GetValueOrDefault(definition.SymbolId);
        /* Array con KIND dell'elemento diverso dal parametro (2026-11-04): si passa una copia
         * convertita e si ricopia indietro dopo la Call. Elenco riempito mentre si emettono gli
         * argomenti, consumato subito dopo la chiamata. */
        static bool ElemIsInt(DataType t) => t is LongType or IntegerType or QuadType
            or ByteType or SByteType or ULongType or UIntegerType or UQuadType;
        var arrayKindFix = new System.Collections.Generic.List<(System.Reflection.Emit.LocalBuilder tmp, System.Reflection.Emit.LocalBuilder orig, System.Type tmpType, bool wantInt)>();
        for (var i = 0; i < expr.Args.Length; i++)
        {
            var paramDef = definition.Parameters[i];
            if (paramDef.Flags.HasFlag(SymbolFlags.IsByRef) && boxArgPositions != null && boxArgPositions.Contains(i))
            {
                // Fase 4 (Closure-ByRef-Box-Linker-Definitivo): il parametro del callee in
                // questa posizione è un ByRefBox<T> (BYVAL a livello CLR, vedi
                // MapParamClrTypeConsideringBox), non un vero T& — passa il BOX, mai
                // l'indirizzo (EmitAddressOf darebbe un T&, tipo sbagliato per la firma).
                EmitCallArgumentAsBox(expr.Args[i], paramDef.DataType);
                continue;
            }
            if (paramDef.Flags.HasFlag(SymbolFlags.IsByRef))
            {
                // Lista passata a un parametro ARRAY ByRef (es. CountIf(arr() AS DOUBLE)
                // con numeri = [1,2,...] — List<double>): la lista va convertita in un
                // array temporaneo e si passa l'INDIRIZZO del temporaneo. La lista
                // originale NON riceve il writeback (i sample la usano solo in lettura
                // nel callee; un REDIM dell'array dentro il callee è un limite noto).
                if (NormalizeInferredType(InferType(expr.Args[i])) is ListType byRefList
                    && NormalizeInferredType(paramDef.DataType) is ArrayType byRefArr)
                {
                    var listClr = DataTypeMapper.Map(byRefList, _types, _classTypes);
                    var tmpArr = _il.DeclareLocal(DataTypeMapper.Map(byRefArr, _types, _classTypes));
                    EmitExpression(expr.Args[i]);
                    var toArrayMethod = ClosedGenericContainsTypeBuilder(listClr)
                        ? TypeBuilder.GetMethod(listClr, listClr.GetGenericTypeDefinition().GetMethod("ToArray", Type.EmptyTypes)!)
                        : listClr.GetMethod("ToArray", Type.EmptyTypes);
                    _il.Emit(OpCodes.Callvirt, toArrayMethod ?? throw new NotSupportedException("Backend IL: List<T>.ToArray non trovato."));
                    _il.Emit(OpCodes.Stloc, tmpArr);
                    _il.Emit(OpCodes.Ldloca, tmpArr);
                }
                else if (NormalizeInferredType(paramDef.DataType) is ArrayType pbArr
                         && NormalizeInferredType(InferType(expr.Args[i])) is ArrayType abArr
                         && ElemIsInt(abArr.ElementType) != ElemIsInt(pbArr.ElementType)
                         && expr.Args[i] is not (LambdaExpr or FunctionReferenceExpr))
                {
                    /* BYREF con elemento di kind diverso: temp convertito + copy-back dopo la Call */
                    var tmpClr = DataTypeMapper.Map(pbArr, _types, _classTypes);
                    var origObj = _il.DeclareLocal(typeof(object));
                    var tmpArr = _il.DeclareLocal(tmpClr);
                    EmitExpression(expr.Args[i]);
                    _il.Emit(OpCodes.Stloc, origObj);
                    _il.Emit(OpCodes.Ldloc, origObj);
                    _il.Emit(OpCodes.Call, RuntimeMethod(ElemIsInt(pbArr.ElementType) ? nameof(IlRuntime.ToLongArray) : nameof(IlRuntime.ToDoubleArray), typeof(object)));
                    _il.Emit(OpCodes.Stloc, tmpArr);
                    _il.Emit(OpCodes.Ldloca, tmpArr);
                    arrayKindFix.Add((tmpArr, origObj, tmpClr, ElemIsInt(pbArr.ElementType)));
                }
                else
                {
                    EmitAddressOf(expr.Args[i]);
                }
            }
            else
            {
                var paramType = NormalizeInferredType(paramDef.DataType);
                if (paramType is AnyType)
                    EmitObjectBoxedArgument(expr.Args[i]);
                else if (paramType is ArrayType pArr
                         && NormalizeInferredType(InferType(expr.Args[i])) is ArrayType aArr
                         && ElemIsInt(aArr.ElementType) != ElemIsInt(pArr.ElementType)
                         && expr.Args[i] is not (LambdaExpr or FunctionReferenceExpr))
                {
                    var tmpClr = DataTypeMapper.Map(pArr, _types, _classTypes);
                    var origObj = _il.DeclareLocal(typeof(object));
                    var tmpArr = _il.DeclareLocal(tmpClr);
                    EmitExpression(expr.Args[i]);
                    _il.Emit(OpCodes.Stloc, origObj);
                    _il.Emit(OpCodes.Ldloc, origObj);
                    _il.Emit(OpCodes.Call, RuntimeMethod(ElemIsInt(pArr.ElementType) ? nameof(IlRuntime.ToLongArray) : nameof(IlRuntime.ToDoubleArray), typeof(object)));
                    _il.Emit(OpCodes.Stloc, tmpArr);
                    _il.Emit(OpCodes.Ldloc, tmpArr);
                    arrayKindFix.Add((tmpArr, origObj, tmpClr, ElemIsInt(pArr.ElementType)));
                }
                else
                {
                    EmitExpression(expr.Args[i]);
                    // FUNCTION NOMINATA con parametri ARRAY passata a un parametro FUNCTION di
                    // una procedura UTENTE (2026-11-03): EmitExpression produce il delegate
                    // `ref` (Cbrif*, firma T[]&) mentre il parametro è tipizzato Func<...> →
                    // senza adattatore lo stack è del tipo sbagliato e `f(...)` dentro il
                    // callee falliva (InvalidProgram/IndexOutOfRange, es. wrapper OPT).
                    if (expr.Args[i] is FunctionReferenceExpr frArg
                        && paramType is FunctionPointerType
                        && _program.Symbols.GetSymbol(frArg.SymbolId).DeclarationNode is FunctionDefinition fdArg
                        && TryRefDelegateType(fdArg) is { } refDelArg)
                        EmitRefCallbackAdapter(refDelArg);
                }
                // Coerce dell'argomento al tipo del PARAMETRO: n (double da un FOR EACH su
                // array) passato a `num AS INTEGER` (Int16 CLR) senza conversione → stack
                // mismatch → InvalidProgram (IsPrimo(n) in una comprensione/FOR EACH).
                // Lambda/callback: mai InferType (non gestito per LambdaExpr — lancia) e
                // nessun coerce numerico (il delegate è già del tipo del parametro FUNCTION).
                if (expr.Args[i] is LambdaExpr or FunctionReferenceExpr) continue;
                var argType = NormalizeInferredType(InferType(expr.Args[i]));
                if (IlConversion.IsNumeric(paramType) && IlConversion.IsNumeric(argType)
                    // L'argomento è GIÀ stato coercedo dal Linker (avvolto in CINT/CBYTE/
                    // CLNG/CDBL per il tipo del parametro — RewriteSingleArgument): ri-coercere
                    // qui emetteva una SECONDA Convert.ToX su un valore già convertito ma con
                    // tipo statico ancora double -> Convert.ToInt16(double) con un short sullo
                    // stack = InvalidProgramException (MATCH con FUNCTION AS INTEGER).
                    && expr.Args[i] is not BuiltinFunctionCallExpr { Name.Kind: TokenKind.CInt or TokenKind.CByte or TokenKind.CLng or TokenKind.CDbl or TokenKind.CSng or TokenKind.CCur })
                {
                    // Una VARIABILE BIT è un int32 già numerico (-1/0/valore); solo
                    // letterali/confronti/chiamate BIT sono bool CLR da convertire con × -1.
                    // Senza la distinzione S(b) con b BIT passava +1 a un parametro DOUBLE.
                    var fromClr = argType is BitType && !EmitsClrBoolValue(expr.Args[i])
                        ? _types.Int32
                        : DataTypeMapper.Map(argType, _types, _classTypes);
                    var toClr = DataTypeMapper.Map(paramType, _types, _classTypes);
                    IlConversion.EmitConvert(_il, _types, argType, fromClr, toClr);
                }
            }
        }

        // Parametri OPZIONALI mancanti: emette i DEFAULT (es. StampaLinea(35) con
        // `OPTIONAL Char AS STRING = "-"` — Args.Length < Parameters.Length).
        for (var extraIdx = expr.Args.Length; extraIdx < definition.Parameters.Length; extraIdx++)
        {
            var extraParam = definition.Parameters[extraIdx];
            if (extraParam.DefaultValue is not null)
            {
                EmitExpression(extraParam.DefaultValue);
                var dpType = NormalizeInferredType(extraParam.DataType);
                var dvType = NormalizeInferredType(InferType(extraParam.DefaultValue));
                if (IlConversion.IsNumeric(dpType) && IlConversion.IsNumeric(dvType))
                {
                    var fromClr = DataTypeMapper.Map(dvType, _types, _classTypes);
                    var toClr = DataTypeMapper.Map(dpType, _types, _classTypes);
                    IlConversion.EmitConvert(_il, _types, dvType, fromClr, toClr);
                }
            }
            else
            {
                _il.Emit(OpCodes.Ldnull);
            }
        }

        if (methodBuilder.IsStatic) _il.Emit(OpCodes.Call, methodBuilder);
        else _il.Emit(OpCodes.Callvirt, methodBuilder);

        if (arrayKindFix.Count > 0)
        {
            var hasRet = methodBuilder.ReturnType != typeof(void);
            System.Reflection.Emit.LocalBuilder? res = hasRet ? _il.DeclareLocal(methodBuilder.ReturnType) : null;
            if (res != null) _il.Emit(OpCodes.Stloc, res);
            foreach (var p in arrayKindFix)
            {
                _il.Emit(OpCodes.Ldloc, p.orig);
                _il.Emit(OpCodes.Ldloc, p.tmp);
                _il.Emit(OpCodes.Call, RuntimeMethod(p.wantInt ? nameof(IlRuntime.CopyBackLong) : nameof(IlRuntime.CopyBackDouble), typeof(object), p.tmpType));
            }
            if (res != null) _il.Emit(OpCodes.Ldloc, res);
        }
    }

    private void EmitFfiCall(FunctionExpr expr, ExternalProcedureDefinition epd)
    {
        // Nome libreria della DECLARE (... LIB "nome"). Un ExternalProcedureDefinition
        // SPECIALIZZATO dal linker (import/namespace) può NON portare LibraryName: in tal caso
        // riprendilo dal nodo di dichiarazione del simbolo. MAI passare null a
        // FfiService.LoadLibrary (prima causava ArgumentNullException su 'path1').
        string? libPath = epd.LibraryName is { } ln0
            ? (ln0.Literal as string ?? ln0.Lexeme?.Trim('"'))
            : null;
        if (string.IsNullOrEmpty(libPath)
            && _program.Symbols.GetSymbol(expr.SymbolId).DeclarationNode is ExternalProcedureDefinition epdSym
            && epdSym.LibraryName is { } ln1)
        {
            libPath = ln1.Literal as string ?? ln1.Lexeme?.Trim('"');
        }
        if (string.IsNullOrEmpty(libPath))
            throw new NotSupportedException(
                "FFI: nome della libreria mancante nella dichiarazione (atteso `LIB \"...\"`).");
        var symbolName = (epd.AliasName?.Literal as string ?? expr.OriginalToken.Lexeme);
        // Kind di marshalling (parità compiler PCode; messaggi IDENTICI alle altre gambe):
        // firma intera omogenea → int64; stringa/BYREF/mista → errore esplicito.
        bool IsI64(DataType? t) => t is IntegerType or UIntegerType or LongType
            or ULongType or QuadType or UQuadType or ByteType or SByteType or EnumType;
        // Array BYREF (2026-09-28, parità PCodeVM/Compiler.Expressions.cs): il parametro
        // viaggia come puntatore nativo → classe di registri intera, indipendentemente dal
        // tipo elemento — vedi AiWiki Bugs/(open) - FFI-Marshalling-Limitato-...md punto (1).
        bool IsArrayParam(Parameter p) => p.Flags.HasFlag(SymbolFlags.IsArray);
        // STRING (2026-09-28): un parametro/ritorno STRING è un puntatore (const char*) come un
        // array — stessa classe di registri intera. BYVAL solo (BYREF scalare di STRING resta
        // vietato, catturato da anyScalarByRef dato che una STRING senza `()` non è mai IsArray).
        bool IsStringParam(Parameter p) => !IsArrayParam(p) && p.DataType is StringType;
        // Whitelist esplicita (fix 2026-09-28, parità Compiler.Expressions.cs): un tipo non
        // riconosciuto (UDT, FUNCTION/SUB come tipo, collezioni, tuple, ...) NON deve cadere nel
        // default "double" — prima di questo fix ci cadeva silenziosamente. Vedi AiWiki
        // Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md.
        bool IsFfiDoubleType(DataType? t) => t is DoubleType or SingleType;
        // Nome leggibile per il messaggio d'errore: per un UDT usa il nome BASIC dichiarato
        // (es. "Point"), non il nome della classe AST interna — parità col messaggio
        // C99Native, che stampa già il nome del tipo dichiarato (fix 2026-09-28).
        string FfiTypeDisplayName(DataType t) => t is UserDefinedType udt ? udt.Name.Lexeme : t.GetType().Name;
        void CheckFfiType(DataType? t, string what)
        {
            if (t is null || IsI64(t) || IsFfiDoubleType(t)) return;
            throw new NotSupportedException(
                $"FFI '{symbolName}': tipo '{FfiTypeDisplayName(t)}' non supportato per {what} del marshalling FFI " +
                "(ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/..., array a 8 byte, STRING).");
        }
        // BYREF scalare (2026-09-28): come un array/STRING, forza la firma a Int64 (il
        // puntatore viaggia come intero indipendentemente da cosa punta). BYREF di STRING
        // resta vietato (char**, fuori scope). Vedi AiWiki
        // Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §1.
        bool IsByRefScalarParam(Parameter p) => p.Flags.HasFlag(SymbolFlags.IsByRef) && !IsArrayParam(p) && !IsStringParam(p);
        // Callback (2026-09-29, parità Compiler.Expressions.cs): un parametro `AS FUNCTION(...)
        // AS T`/`AS SUB(...)` viaggia come PUNTATORE nativo — stessa classe di registri intera
        // di array/STRING/BYREF, indipendentemente dal kind INTERNO del callback. v1: arità
        // 0-4, firma interna omogenea per kind, BYVAL. Vedi AiWiki Bugs/(open) - FFI-
        // Estensioni-Possibili-BYREF-Struct-Callback.md §3bis.
        bool IsCallbackParam(Parameter p) => !IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && p.DataType is FunctionPointerType;
        // Callback vettoriale (2026-11-02): 1-2 array di DOUBLE in testa, poi 0-1 scalare
        // DOUBLE, ritorno DOUBLE o ARRAY OF DOUBLE (ABI in virgola mobile).
        bool IsVecElem(DataType? t) => t is DoubleType or InferredType or AnyType;
        bool IsVecCallback(FunctionPointerType fp) =>
            fp.ParameterTypes.Length >= 1
            && fp.ParameterTypes[0] is ArrayType
            && fp.ParameterTypes.TakeWhile(t => t is ArrayType).All(t => IsVecElem(((ArrayType)t).ElementType))
            && fp.ReturnType is not VoidType
            && (fp.ReturnType is ArrayType rat ? IsVecElem(rat.ElementType) : IsFfiDoubleType(fp.ReturnType))
            && fp.ParameterTypes.SkipWhile(t => t is ArrayType).All(t => t is not ArrayType);
        int VecCallbackCode(FunctionPointerType fp)
        {
            int nArr = 0;
            while (nArr < fp.ParameterTypes.Length && fp.ParameterTypes[nArr] is ArrayType) nArr++;
            int nSca = fp.ParameterTypes.Length - nArr;
            bool arrRet = fp.ReturnType is ArrayType;
            return (nArr - 1) | (nSca << 2) | (arrRet ? 16 : 0);
        }
        int CallbackArityByte(FunctionPointerType fp) =>
            IsVecCallback(fp) ? 128 + VecCallbackCode(fp) : fp.ParameterTypes.Length;
        void ValidateCallbackForFfi(FunctionPointerType fp, string what)
        {
            if (IsVecCallback(fp))
            {
                int nArr = 0;
                while (nArr < fp.ParameterTypes.Length && fp.ParameterTypes[nArr] is ArrayType) nArr++;
                int nSca = fp.ParameterTypes.Length - nArr;
                if (!((nArr == 1 && nSca == 0) || (nArr == 2 && nSca == 0) || (nArr == 1 && nSca == 1)))
                    throw new NotSupportedException(
                        $"FFI '{symbolName}': forma di callback non supportata per {what} ({nArr} array, {nSca} scalari): ammesse: f(v()) -> scalare/array, f(a(), b()) -> scalare/array, f(v(), x) -> scalare.");
                return;
            }
            if (fp.ParameterTypes.Any(t => t is ArrayType))
                throw new NotSupportedException(
                    $"FFI '{symbolName}': callback con array usato per {what} non supportato " +
                    "(ammessi solo array di DOUBLE, con ritorno DOUBLE o ARRAY OF DOUBLE).");
            if (fp.ParameterTypes.Length > 4)
                throw new NotSupportedException(
                    $"FFI '{symbolName}': il callback usato per {what} ha {fp.ParameterTypes.Length} parametri (ammessi 0-4 nella v1 del supporto callback).");
            var hasReturn = fp.ReturnType is not VoidType;
            var kinds = fp.ParameterTypes.Select(t => IsI64(t)).ToList();
            if (hasReturn) kinds.Add(IsI64(fp.ReturnType));
            foreach (var (pt, idx) in fp.ParameterTypes.Select((t, i) => (t, i)))
                if (!IsI64(pt) && !IsFfiDoubleType(pt))
                    throw new NotSupportedException(
                        $"FFI '{symbolName}': tipo del parametro {idx + 1} del callback usato per {what} non supportato (ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/...).");
            if (hasReturn && !IsI64(fp.ReturnType) && !IsFfiDoubleType(fp.ReturnType))
                throw new NotSupportedException(
                    $"FFI '{symbolName}': tipo di ritorno del callback usato per {what} non supportato (ammessi: DOUBLE/SINGLE, interi LONG/INTEGER/QUAD/BYTE/..., o nessuno per un CALLBACK SUB).");
            if (kinds.Distinct().Count() > 1)
                throw new NotSupportedException(
                    $"FFI '{symbolName}': il callback usato per {what} ha una firma interna mista LONG/DOUBLE — non supportata nella v1 del supporto callback (tutti i parametri e l'eventuale ritorno dello stesso kind).");
        }
        bool CallbackIsInt64(FunctionPointerType fp) => IsVecCallback(fp)
            ? IsI64(fp.ReturnType)
            : fp.ParameterTypes.Length > 0 ? IsI64(fp.ParameterTypes[0]) : fp.ReturnType is not VoidType && IsI64(fp.ReturnType);
        foreach (var p in epd.Parameters)
            if (IsCallbackParam(p)) ValidateCallbackForFfi((FunctionPointerType)p.DataType!, $"il parametro '{p.Name.Lexeme}'");
        // Struct/UDT per valore (2026-09-29, parità Compiler.Expressions.cs/PCodeVM/VM-C): un
        // UDT "semplice" (nessun operatore/costruttore/metodo/generico) con TUTTI i campi dello
        // stesso kind a 8 byte viaggia per valore. Il backend IL è "gratis" rispetto a PCodeVM:
        // ogni UDT semplice compila già in un vero value type CLR a COMPILE TIME
        // (IlProgramCompiler.Host.Compile.cs, TypeBuilder) — nessuna forma-ombra da costruire a
        // runtime, il tipo reale (_classTypes[SymbolId]) si passa direttamente al delegate
        // dinamico. Vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §2.
        bool IsUdtParam(Parameter p) => !IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && p.DataType is UserDefinedType;
        bool IsSimpleUdt(UserDefinedType udt) => udt.TypeParameters.IsDefaultOrEmpty
            && udt.Operators.IsDefaultOrEmpty && udt.Constructors.IsDefaultOrEmpty && udt.Methods.IsDefaultOrEmpty;
        // Struct annidate (2026-09-29): a differenza di PCodeVM/VM-C/C99Native, IL non ha
        // bisogno di un path per FOGLIA — il tipo CLR reale di un UDT annidato è già un vero
        // struct-in-struct (TypeBuilder pre-dichiarato per ogni TYPE in Host.Compile.cs, box/
        // Unbox_Any copia l'intero blob) — qui serve SOLO contare/classificare le foglie per il
        // guard, l'emissione (EmitUdtParamTypesArray/EmitObjectBoxedArgument) resta invariata.
        // LeafKinds è "silenziosa" (nessun controllo, chiamata SOLO dopo ValidateUdtForFfi per
        // lo stesso UDT, quindi struttura già garantita valida).
        IEnumerable<bool> LeafKinds(UserDefinedType udt) => udt.Fields.SelectMany(f =>
            f.DataType is UserDefinedType nested ? LeafKinds(nested) : new[] { f.DataType is not DoubleType });
        bool IsMixedKindUdt(UserDefinedType udt) => LeafKinds(udt).Distinct().Count() > 1;
        int CountFfiLeaves(UserDefinedType udt, string what)
        {
            var count = 0;
            foreach (var f in udt.Fields)
            {
                if (f.DataType is UserDefinedType nested)
                {
                    if (!IsSimpleUdt(nested))
                        throw new NotSupportedException(
                            $"FFI '{symbolName}': lo UDT '{nested.Name.Lexeme}' (campo '{f.Name.Lexeme}') usato per {what} non è ammesso dal marshalling FFI (solo TYPE semplici, senza operatori/costruttori/metodi/generici).");
                    count += CountFfiLeaves(nested, what);
                }
                else if (f.DataType is DoubleType or LongType or QuadType or ULongType or UQuadType)
                    count++;
                else
                    throw new NotSupportedException(
                        $"FFI '{symbolName}': tipo del campo '{f.Name.Lexeme}' non supportato per {what} (UDT '{udt.Name.Lexeme}', solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).");
            }
            return count;
        }
        void ValidateUdtForFfi(UserDefinedType udt, string what, bool allowMixedKind)
        {
            if (!IsSimpleUdt(udt))
                throw new NotSupportedException(
                    $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} non è ammesso dal marshalling FFI (solo TYPE semplici, senza operatori/costruttori/metodi/generici).");
            // Limite a 8 campi FOGLIA dopo flattening ricorsivo (S0 2026-11-06) — parità
            // Compiler.Expressions.cs. Il vecchio limite a 1-2 foglie (16 byte) valeva finché la
            // VM/C chiamava con un cast di funzione scritto a mano; ora la VM/C usa libffi
            // (gmb_call_invoke_slots) e IL/PCode costruiscono un value type CLR per forma, quindi
            // il numero di campi non è più un vincolo ABI. Restano fuori 9+ campi e i tipi di
            // campo non a 8 byte: richiedono il layout C completo (stadio S1, descrittore v3).
            var leafCount = CountFfiLeaves(udt, what);
            if (leafCount is 0 or > 8)
                throw new NotSupportedException(
                    $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} ha {leafCount} campi/campi-foglia (ammessi da 1 a 8 — ogni foglia occupa 8 byte; oltre 8 campi serve il layout C completo, stadio S1).");
            // Kind misto (2026-09-29, parità Compiler.Expressions.cs): non un limite ABI reale
            // (SysV/AAPCS64 classificano ogni eightbyte indipendentemente) — vietato solo come
            // PARAMETRO perché VM/C chiama attraverso un cast di funzione con un solo tipo
            // scalare uniforme per l'intera lista argomenti (vedi gmb_vm_core.c). Come RITORNO
            // è ammesso ovunque (nessuna ambiguità di posizione nel dispatch del ritorno).
            if (!allowMixedKind && IsMixedKindUdt(udt))
                throw new NotSupportedException(
                    $"FFI '{symbolName}': lo UDT '{udt.Name.Lexeme}' usato per {what} ha campi di kind misto (DOUBLE e intero) — ammesso solo come TIPO DI RITORNO, non come parametro (limite del dispatch nativo VM/C).");
        }
        bool UdtIsInt64(UserDefinedType udt) => LeafKinds(udt).First();
        var anyUdtParam = epd.Parameters.Any(IsUdtParam);
        var udtReturn = epd.IsFunction && epd.ReturnType is UserDefinedType ? (UserDefinedType)epd.ReturnType : null;
        // Kind misto come PARAMETRO (2026-09-29, era vietato — parità Compiler.Expressions.cs):
        // ammesso, ma AL MASSIMO UNO per dichiarazione (limite del dispatch VM/C, non di IL —
        // IL non ne avrebbe bisogno, ma la guardia resta identica su tutti e 5 i backend per
        // parità cross-backend).
        foreach (var p in epd.Parameters)
            if (IsUdtParam(p)) ValidateUdtForFfi((UserDefinedType)p.DataType!, $"il parametro '{p.Name.Lexeme}'", allowMixedKind: true);
        if (udtReturn != null) ValidateUdtForFfi(udtReturn, "il ritorno", allowMixedKind: true);
        // S0 (2026-11-06): il limite "un solo parametro UDT a kind misto" era del dispatch
        // per-cast della VM/C; con libffi (slot per argomento) non serve più.
        foreach (var p in epd.Parameters)
            if (!IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && !IsUdtParam(p) && !IsCallbackParam(p)) CheckFfiType(p.DataType, $"il parametro '{p.Name.Lexeme}'");
        if (epd.IsFunction && !(epd.ReturnType is StringType) && udtReturn == null) CheckFfiType(epd.ReturnType, "il ritorno");
        var retIsString = epd.ReturnType is StringType;
        var anyStringByRef = epd.Parameters.Any(p => p.Flags.HasFlag(SymbolFlags.IsByRef) && IsStringParam(p));
        var anyArray = epd.Parameters.Any(IsArrayParam);
        var anyStringParam = epd.Parameters.Any(IsStringParam);
        var anyByRefScalar = epd.Parameters.Any(IsByRefScalarParam);
        var anyCallbackParam = epd.Parameters.Any(IsCallbackParam);
        var anyPtr = anyArray || anyStringParam || retIsString || anyByRefScalar || anyCallbackParam;
        // Limite slot nativi TOTALI (2026-09-29, parità Compiler.Expressions.cs): un parametro
        // UDT si "espande" in N slot nativi (uno per campo) sulla gamba VM/C — la parità
        // cross-backend richiede lo STESSO limite/messaggio qui, anche se IL stesso non
        // "espande" nulla (passa lo struct vero).
        var flattenedSlotCount = epd.Parameters.Sum(p => IsUdtParam(p) ? LeafKinds((UserDefinedType)p.DataType!).Count() : 1);
        if (flattenedSlotCount > 64)
            throw new NotSupportedException(
                $"FFI '{symbolName}': troppi slot nativi risultanti dall'espansione degli UDT per valore ({flattenedSlotCount}, massimo 8 — ogni campo UDT conta come uno slot).");
        if (anyStringByRef)
            throw new NotSupportedException(
                $"FFI '{symbolName}': BYREF di STRING non supportato dal marshalling FFI (passare BYVAL).");
        // Tipo elemento (array) O tipo di un parametro BYREF scalare: SOLO DOUBLE o famiglia
        // intera A 8 BYTE (LONG/QUAD/ULONG/UQUAD) — non tutta la famiglia intera scalare. Fix
        // 2026-09-28: INTEGER/BYTE/SBYTE/UINTEGER/SINGLE mappano a CLR byte[]/short[]/float[]/...
        // (DataTypeMapper.cs) — larghezza-elemento nativa più stretta di 8 byte, ma il
        // puntatore FFI viaggia sempre a 8 byte (gmb_int_t*/double*) su tutti i backend:
        // pinnare/puntare un valore più stretto produce un mismatch di larghezza silenzioso
        // (bug osservato 2026-09-28: `AS BYTE` letto come garbage dal nativo via un puntatore a
        // passo 8 byte). Per gli scalari BYVAL non è un problema (marshallano sempre a
        // int64_t), ma un array o un BYREF sono un indirizzo condiviso: la larghezza reale conta.
        bool IsI64ArrayElem(DataType? t) => t is LongType or QuadType or ULongType or UQuadType;
        /* BYTE/SBYTE (2026-11-06, parità Compiler.Expressions.cs IsFfiArrayElemType): ammessi come
         * elemento-array. Il byte[] CLR NON viene mai pinnato (elemento largo 1 byte, il puntatore
         * FFI viaggia a 8): FfiService.CallFunctionTypedArr costruisce uno scratch long[] con i
         * valori 0..255 e fa copy-back dopo la chiamata. Senza questa ammissione la gamba IL
         * rifiutava in compilazione le firme `arr() AS BYTE` che C99Native/VM-C accettano. */
        bool IsFfiArrayElem(DataType? t) => t is DoubleType || IsI64ArrayElem(t) || t is ByteType || t is SByteType;
        foreach (var p in epd.Parameters)
        {
            if (IsArrayParam(p))
            {
                var elem = (p.DataType as ArrayType)?.ElementType;
                if (!IsFfiArrayElem(elem))
                    throw new NotSupportedException(
                        $"FFI '{symbolName}': tipo elemento non supportato per il parametro array '{p.Name.Lexeme}' (solo AS DOUBLE, LONG/QUAD o BYTE — l'elemento viaggia comunque a 8 byte, non INTEGER/SINGLE).");
            }
            else if (IsByRefScalarParam(p) && p.DataType is not DoubleType && !IsI64ArrayElem(p.DataType))
                throw new NotSupportedException(
                    $"FFI '{symbolName}': tipo non supportato per il parametro BYREF '{p.Name.Lexeme}' (solo AS DOUBLE o LONG/QUAD, non INTEGER/BYTE/SINGLE — larghezza nativa a 8 byte).");
        }
        // Limite arità (parità 4 backend: la VM/C chiama al massimo 8 argomenti).
        if (epd.Parameters.Length > 64)
            throw new NotSupportedException(
                $"FFI '{symbolName}': massimo 64 argomenti supportati dal marshalling FFI.");
        // Firma OMOGENEA: parametri E ritorno dello stesso kind. Il ritorno va incluso
        // (solo per FUNCTION) — altrimenti `(LONG) AS DOUBLE` passava come all-double
        // (divergenza vs le gambe C, che mappano i tipi per-parametro). Fix parità 2026-09-22.
        // Kind misto sul RITORNO (2026-09-29): un UDT-ritorno a kind misto non ha un singolo
        // "kind" da far combaciare col resto della firma (il dispatch del ritorno sceglie la
        // propria forma autonomamente) — va escluso dal controllo di omogeneità, altrimenti
        // UdtIsInt64(...) (che guarda solo il PRIMO campo) rifiuterebbe firme valide.
        var udtReturnMixed = udtReturn != null && IsMixedKindUdt(udtReturn);
        // Kind misto come PARAMETRO (2026-09-29): stesso trattamento del ritorno misto — un
        // UDT-PARAMETRO a kind misto non ha un singolo "kind" da far combaciare (viaggia come
        // vero struct alla sua posizione, non flattenato) — escluso dal controllo di
        // omogeneità sotto.
        bool IsMixedUdtParam(Parameter p) => IsUdtParam(p) && IsMixedKindUdt((UserDefinedType)p.DataType!);
        var anyUdt = anyUdtParam || udtReturn != null;
        var kindList = epd.Parameters.Where(p => !IsMixedUdtParam(p)).Select(p => IsArrayParam(p) || IsStringParam(p) || IsByRefScalarParam(p) || IsCallbackParam(p)
            || (IsUdtParam(p) ? UdtIsInt64((UserDefinedType)p.DataType!) : IsI64(p.DataType))).ToList();
        if (epd.IsFunction && !udtReturnMixed) kindList.Add(IsI64(epd.ReturnType) || retIsString || (udtReturn != null && UdtIsInt64(udtReturn)));
        // Firma mista LONG/DOUBLE fra SCALARI SEMPLICI (2026-10, v1, parità
        // Compiler.Expressions.cs/gmb_c99_emit.c/C99CodeEmitter.Legacy.cs): ammessa SOLO
        // quando non c'è nessun array/STRING/BYREF/callback/UDT (quelli continuano a imporre
        // un kind unico su tutta la firma — motivo indipendente, invariato).
        var anyHardPtr = anyArray || anyByRefScalar || anyCallbackParam || retIsString;  /* STRING-param ammessa col misto */
        var mixedScalarsOnly = !anyHardPtr && !anyUdt && kindList.Distinct().Count() > 1;
        // 2026-11-02: firma mista ammessa con puntatori (array/STRING/BYREF) ma NON con
        // callback (trampolino + slot misti instabile: hang osservato) — per i callback resta
        // la firma omogenea, con errore di compilazione esplicito.
        var mixedKinds = kindList.Distinct().Count() > 1;
        var mixedWithPointers = anyHardPtr && !anyUdt && mixedKinds;
        if (!mixedScalarsOnly && !mixedWithPointers && mixedKinds)
            throw new NotSupportedException(
                anyPtr
                    ? $"FFI '{symbolName}': un parametro array, STRING o BYREF richiede firma INT64 (dichiarare gli altri parametri/il ritorno AS LONG — il puntatore viaggia come intero)."
                    : $"FFI '{symbolName}': firma mista LONG/DOUBLE non supportata dal marshalling FFI (tutti i parametri e il ritorno AS LONG oppure AS DOUBLE).");
        // SUB (ReturnType null): nessun ritorno da includere nel kind — decidono solo i
        // parametri (prima di questo fix un SUB con soli parametri LONG/array finiva
        // comunque su kind=0, corrompendo un eventuale parametro-puntatore — stesso bug
        // corretto in Compiler.Expressions.cs/PCodeVM).
        bool AllInt64Params() => epd.Parameters.Where(p => !IsMixedUdtParam(p)).All(p => IsArrayParam(p) || IsStringParam(p) || IsByRefScalarParam(p) || IsCallbackParam(p)
            || (IsUdtParam(p) ? UdtIsInt64((UserDefinedType)p.DataType!) : IsI64(p.DataType)));
        // 'kind' = kind del RITORNO per una FUNCTION (coincide col vecchio calcolo nel caso
        // omogeneo, dove AllInt64Params() è implicato da un ritorno Int64 — qui invece non lo
        // richiede più, perché con la firma mista i parametri possono differire dal ritorno).
        // Per una SUB resta "tutti i parametri sono Int64" (nessun ritorno da interpretare).
        // udtReturnMixed (2026-10, bug trovato testando LabeledND: primo campo intero, tutti i
        // parametri DOUBLE): UdtIsInt64(udtReturn) guarda SOLO il primo campo — non rappresenta
        // un kind reale per un ritorno a kind misto (il dispatch del ritorno sceglie la propria
        // forma via retUdtFieldKindMask, indipendente da 'kind'). Usarlo qui darebbe 'kind'=1
        // anche con parametri tutti DOUBLE — VM/C instraderebbe quei parametri nel ramo
        // kind=1/nativeArgs che li tronca a int64 prima della chiamata nativa. In quel caso
        // 'kind' deve seguire SOLO i parametri (stesso trattamento di una SUB).
        var kind = !epd.IsFunction || udtReturnMixed
            ? (AllInt64Params() ? 1 : 0)
            : (IsI64(epd.ReturnType) || retIsString || (udtReturn != null && UdtIsInt64(udtReturn))) ? 1 : 0;
        // scalarKindMask (2026-10, firma mista): bit i = il parametro SCALARE SEMPLICE alla
        // posizione i è Int64 — significativo solo per le posizioni non-array/STRING/BYREF/
        // callback/UDT (altrove il bit non è consultato dal runtime, lasciato a 'kind' per
        // leggibilità). Nel caso omogeneo legacy coincide sempre con 'kind'.
        long scalarKindMask = 0;
        for (var pi = 0; pi < epd.Parameters.Length && pi < 16; pi++)
        {
            var p = epd.Parameters[pi];
            var isPlainScalar = !IsArrayParam(p) && !IsStringParam(p) && !IsByRefScalarParam(p) && !IsCallbackParam(p) && !IsUdtParam(p);
            if (isPlainScalar ? IsI64(p.DataType)
                              : IsUdtParam(p) ? kind == 1
                              : true) scalarKindMask |= 1L << pi;
        }
        // Struct/UDT (2026-09-29): un UDT tutto-DOUBLE (nessun array/STRING/BYREF a forzare
        // Int64) deve comunque passare dal path "esteso" (tipi reali per i parametri/ritorno,
        // non lo scalare long/double omogeneo puro) — parità con "useTypedPath" in
        // Compiler.Expressions.cs (anyUdt forza il path tipizzato anche a kind=0/Double).
        long arrayMask = 0;
        long arrayElemIntMask = 0;
        long stringMask = 0;
        long byrefScalarMask = 0;
        long byrefScalarElemIntMask = 0;
        for (var pi = 0; pi < epd.Parameters.Length; pi++)
        {
            if (IsStringParam(epd.Parameters[pi])) stringMask |= 1L << pi;
            if (IsByRefScalarParam(epd.Parameters[pi]))
            {
                byrefScalarMask |= 1L << pi;
                if (IsI64(epd.Parameters[pi].DataType)) byrefScalarElemIntMask |= 1L << pi;
            }
            if (!IsArrayParam(epd.Parameters[pi])) continue;
            arrayMask |= 1L << pi;
            var elem = (epd.Parameters[pi].DataType as ArrayType)?.ElementType;
            if (IsI64(elem)) arrayElemIntMask |= 1L << pi;
        }
        // BYREF scalare: l'argomento deve essere una variabile semplice (stessa restrizione di
        // Compiler.Expressions.cs — serve un simbolo su cui fare il write-back dopo la chiamata).
        for (var pi = 0; pi < epd.Parameters.Length; pi++)
            if ((byrefScalarMask & (1L << pi)) != 0 && expr.Args[pi] is not VariableExpr)
                throw new NotSupportedException(
                    $"FFI '{symbolName}': il parametro BYREF '{epd.Parameters[pi].Name.Lexeme}' richiede una variabile semplice come argomento (non un'espressione).");
        _il.Emit(OpCodes.Ldstr, libPath);
        _il.Emit(OpCodes.Ldstr, symbolName);
        var arrLocal = _il.DeclareLocal(typeof(object[]));
        _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        _il.Emit(OpCodes.Stloc, arrLocal);
        for (var i = 0; i < expr.Args.Length; i++)
        {
            _il.Emit(OpCodes.Ldloc, arrLocal);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitFfiCallbackArg(i, expr.Args[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        // Callback FUNZIONE NOMINATA con parametri array (2026-11-02): il metodo BASIC ha gli
        // array `T[]&` (BYREF) mentre il delegate generico Func<T[],R> e' by-value -> il CLR
        // generava un thunk con ABI sbagliata (IndexOutOfRange/AccessViolation). Per gli argomenti
        // di un parametro callback che sono FunctionReferenceExpr si costruisce un delegate con
        // parametri `ref` (CbRef*), che combacia esattamente con la firma del metodo.
        void EmitFfiCallbackArg(int pi, Expr arg)
        {
            if (arg is FunctionReferenceExpr fre && FfiCallbackHasArrayShape(epd.Parameters[pi].DataType))
            {
                EmitFfiCallbackFunctionReference(fre);
                return;
            }
            EmitObjectBoxedArgument(arg);
        }

        // Callback come PARAMETRO (2026-09-29): risolve OGNI slot callback di arrLocal (un vero
        // delegate CLR — EmitObjectBoxedArgument/EmitFunctionReference lo ha già messo lì) in
        // un vero puntatore nativo (trampolino), PRIMA che arrLocal sia caricato come argomento
        // posizionale per la dispatch sotto — da quel momento lo slot è indistinguibile da un
        // normale argomento puntatore Int64 già risolto (array/STRING/BYREF/UDT), NESSUNA
        // modifica agli overload FfiCallTyped* esistenti è stata necessaria (stessa strategia
        // di normalizzazione già usata su PCodeVM/VM-C). LocalBuilder cbKeepAliveLocal (una
        // List&lt;Delegate&gt;) va tenuta viva fino a DOPO la chiamata FFI nativa
        // (EmitCallbackKeepAliveIfAny, chiamata da ogni punto di uscita del metodo sotto) — il
        // GC potrebbe altrimenti raccogliere un delegate trampolino il cui puntatore nativo è
        // ancora in uso dalla chiamata nativa in corso.
        LocalBuilder? cbKeepAliveLocal = null;
        if (anyCallbackParam)
        {
            long callbackKindMask = 0;
            long callbackHasRetMask = 0;
            for (var pi = 0; pi < epd.Parameters.Length; pi++)
            {
                if (!IsCallbackParam(epd.Parameters[pi])) continue;
                var fp = (FunctionPointerType)epd.Parameters[pi].DataType!;
                if (CallbackIsInt64(fp)) callbackKindMask |= 1L << pi;
                if (fp.ReturnType is not VoidType) callbackHasRetMask |= 1L << pi;
            }
            var cbAritySize = Math.Max(16, epd.Parameters.Length);
            _il.Emit(OpCodes.Ldc_I4, cbAritySize);
            _il.Emit(OpCodes.Newarr, typeof(int));
            var cbArityLocal = _il.DeclareLocal(typeof(int[]));
            _il.Emit(OpCodes.Stloc, cbArityLocal);
            for (var pi = 0; pi < cbAritySize; pi++)
            {
                _il.Emit(OpCodes.Ldloc, cbArityLocal);
                _il.Emit(OpCodes.Ldc_I4, pi);
                var arity = pi < epd.Parameters.Length && IsCallbackParam(epd.Parameters[pi])
                    ? CallbackArityByte((FunctionPointerType)epd.Parameters[pi].DataType!)
                    : 255;
                _il.Emit(OpCodes.Ldc_I4, arity);
                _il.Emit(OpCodes.Stelem_I4);
            }
            _il.Emit(OpCodes.Ldloc, arrLocal);
            _il.Emit(OpCodes.Ldloc, cbArityLocal);
            _il.Emit(OpCodes.Ldc_I4, callbackKindMask);
            _il.Emit(OpCodes.Ldc_I4, callbackHasRetMask);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.PrepareFfiCallbackArgs), typeof(object[]), typeof(int[]), typeof(int), typeof(int)));
            cbKeepAliveLocal = _il.DeclareLocal(typeof(List<Delegate>));
            _il.Emit(OpCodes.Stloc, cbKeepAliveLocal);
        }
        void EmitCallbackKeepAliveIfAny()
        {
            if (!anyCallbackParam) return;
            _il.Emit(OpCodes.Ldloc, cbKeepAliveLocal!);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.KeepAliveFfiCallback), typeof(object)));
        }
        _il.Emit(OpCodes.Ldloc, arrLocal);
        // BYREF scalare (2026-09-28): DOPO la chiamata, FfiService ha riscritto args[i] (=
        // arrLocal[i]) col valore OUT per ogni slot byrefScalarMask — questo helper rilegge
        // arrLocal e fa il write-back sulla variabile BASIC del chiamante (EmitStoreVariable,
        // stesso meccanismo generico di un LET/assegnazione). Va chiamato DOPO l'istruzione
        // Call: lo stack ha in cima il valore di ritorno (o il double scartato per una SUB) — i
        // push/pop di questo helper sono bilanciati, non lo disturbano.
        void EmitByRefScalarWriteback()
        {
            for (var pi = 0; pi < epd.Parameters.Length; pi++)
            {
                if ((byrefScalarMask & (1L << pi)) == 0) continue;
                var ve = (VariableExpr)expr.Args[pi];
                var isInt = (byrefScalarElemIntMask & (1L << pi)) != 0;
                var elemType = epd.Parameters[pi].DataType;
                var pi2 = pi;
                EmitStoreVariable(ve.SymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, arrLocal);
                    _il.Emit(OpCodes.Ldc_I4, pi2);
                    _il.Emit(OpCodes.Ldelem_Ref);
                    _il.Emit(OpCodes.Unbox_Any, isInt ? typeof(long) : typeof(double));
                    // FfiService boxa sempre gli interi come `long` (mai ulong) — ULONG/UQUAD
                    // mappano a UInt64 in DataTypeMapper.cs: serve una reinterpretazione dello
                    // stesso pattern a 64 bit per soddisfare il tipo esatto atteso da Stloc.
                    if (isInt && elemType is ULongType or UQuadType)
                        _il.Emit(OpCodes.Conv_U8);
                });
            }
        }
        // Struct/UDT (2026-09-29): costruisce un Type[] (un elemento per parametro, non-null
        // solo alle posizioni UDT) col tipo CLR REALE del UDT — usato dagli overload
        // *Udt*/UdtRet dei runtime helper, che lo inoltrano a FfiService.CallFunctionTypedArr
        // (udtParamClrTypes) così il delegate dinamico marshalla lo struct per valore con la
        // vera ABI .NET, nessuna forma-ombra/copia campo-per-campo (args[i] è già l'istanza
        // boxed giusta, vedi EmitObjectBoxedArgument/DataTypeMapper.Map). Lascia l'array in
        // cima allo stack — va chiamato appena prima del Call, come argomento posizionale.
        void EmitUdtParamTypesArray()
        {
            _il.Emit(OpCodes.Ldc_I4, epd.Parameters.Length);
            _il.Emit(OpCodes.Newarr, typeof(Type));
            var udtTypesLocal = _il.DeclareLocal(typeof(Type[]));
            _il.Emit(OpCodes.Stloc, udtTypesLocal);
            for (var pi = 0; pi < epd.Parameters.Length; pi++)
            {
                if (!IsUdtParam(epd.Parameters[pi])) continue;
                _il.Emit(OpCodes.Ldloc, udtTypesLocal);
                _il.Emit(OpCodes.Ldc_I4, pi);
                _il.Emit(OpCodes.Ldtoken, DataTypeMapper.Map(epd.Parameters[pi].DataType!, _types, _classTypes));
                _il.Emit(OpCodes.Call, GetTypeFromHandleMethod);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Ldloc, udtTypesLocal);
        }
        // SUB FFI (DECLARE SUB, ReturnType null/Void): come per una FUNCTION il runtime
        // helper (FfiCallTyped) lascia SEMPRE un double sullo stack; il chiamante statement
        // (EmitExpressionStatement, che crede DoubleType via InferType) lo popa. Nessun
        // Pop qui: doppio Pop = InvalidProgramException (probe vec_bump/vec_tick SU.
        if (!epd.IsFunction)
        {
            // SUB: mai ritorno STRING né UDT (nessun ritorno) — instrada su FfiCallTypedArrUdt
            // quando almeno un PARAMETRO è un UDT, su FfiCallTypedArr quando c'è almeno un
            // parametro array/STRING/BYREF, altrimenti il path scalare puro.
            _il.Emit(OpCodes.Ldc_I4, kind);
            if (anyUdtParam)
            {
                _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
                _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
                _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
                _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
                _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
                EmitUdtParamTypesArray();
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArrUdt), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(Type[])));
            }
            else if (arrayMask != 0 || stringMask != 0 || byrefScalarMask != 0 || anyCallbackParam)
            {
                _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
                _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
                _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
                _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
                _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
                _il.Emit(OpCodes.Ldc_I8, (long)scalarKindMask);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long)));
            }
            else if (mixedScalarsOnly)
            {
                // Firma mista (2026-10): nessun array/STRING/BYREF/UDT (per costruzione del
                // guard sopra) — instrada comunque su FfiCallTypedArr, mai sul fast path
                // FfiCallTyped (che assumerebbe un kind uniforme), passando scalarKindMask.
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I8, (long)scalarKindMask);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long)));
            }
            else
            {
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTyped), typeof(string), typeof(string), typeof(object[]), typeof(int)));
            }
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
            return;
        }
        // SEMPRE il path tipizzato (FfiCallTyped/FfiCallTypedArr(Str)): kind 0 = delegate double
        // (comportamento legacy identico), kind 1 = delegate int64. Il branch legacy con
        // FfiCall(string, string, object[]) + Ldc_I4 emesso qui sopra avrebbe sbilanciato lo
        // stack (FfiCall non accetta il kind → InvalidProgramException). arrayMask/stringMask/
        // byrefScalarMask != 0 → almeno un parametro puntatore (array, STRING o BYREF,
        // 2026-09-28): instrada su FfiCallTypedArr, che pinna/converte gli slot corrispondenti
        // invece di trattarli come scalari kind-omogenei. retIsString: il RITORNO è STRING —
        // serve l'helper che ritorna string (non double), scelto qui a compile-time perché IL è
        // staticamente tipizzato.
        _il.Emit(OpCodes.Ldc_I4, kind);
        if (udtReturn != null)
        {
            // Il RITORNO stesso è un UDT: FfiCallTypedArrUdtRet ritorna object (istanza boxed
            // del tipo CLR reale) — unboxata subito dopo il Call sul tipo noto a compile-time,
            // così lo stack ha il vero value type, come qualunque altra espressione UDT.
            var udtReturnClrType = DataTypeMapper.Map(udtReturn, _types, _classTypes);
            _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
            _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
            _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
            if (anyUdtParam) EmitUdtParamTypesArray(); else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Ldtoken, udtReturnClrType);
            _il.Emit(OpCodes.Call, GetTypeFromHandleMethod);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArrUdtRet), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(Type[]), typeof(Type)));
            _il.Emit(OpCodes.Unbox_Any, udtReturnClrType);
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
        }
        else if (anyUdtParam)
        {
            // Il ritorno NON è un UDT (double/long/STRING/void), ma almeno un PARAMETRO lo è.
            _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
            _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
            _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
            EmitUdtParamTypesArray();
            if (retIsString)
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArrUdtStr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(Type[])));
            else
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArrUdt), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(Type[])));
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
        }
        else if (retIsString)
        {
            _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
            _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
            _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArrStr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long)));
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
        }
        else if (arrayMask != 0 || stringMask != 0 || byrefScalarMask != 0 || anyCallbackParam)
        {
            _il.Emit(OpCodes.Ldc_I8, (long)arrayMask);
            _il.Emit(OpCodes.Ldc_I8, (long)arrayElemIntMask);
            _il.Emit(OpCodes.Ldc_I8, (long)stringMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarMask);
            _il.Emit(OpCodes.Ldc_I8, (long)byrefScalarElemIntMask);
            _il.Emit(OpCodes.Ldc_I8, (long)scalarKindMask);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long)));
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
        }
        else if (mixedScalarsOnly)
        {
            // Firma mista (2026-10): nessun array/STRING/BYREF/UDT (per costruzione del guard
            // sopra) — instrada comunque su FfiCallTypedArr, mai sul fast path FfiCallTyped.
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I8, (long)scalarKindMask);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTypedArr), typeof(string), typeof(string), typeof(object[]), typeof(int), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long)));
            EmitByRefScalarWriteback();
            EmitCallbackKeepAliveIfAny();
        }
        else
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FfiCallTyped), typeof(string), typeof(string), typeof(object[]), typeof(int)));
            EmitCallbackKeepAliveIfAny();
        }
    }

    /// <summary>
    /// Emette l'INDIRIZZO (non il valore) di un'espressione, per passarla a un parametro
    /// BYREF. Copertura: variabili semplici (Ldloca per un locale, Ldarga per un
    /// parametro BYVAL del chiamante — un NUOVO indirizzo valido per la durata di questa
    /// chiamata; se il parametro del chiamante è già esso stesso BYREF, il suo Ldarg dà
    /// già l'indirizzo originale, che viene semplicemente inoltrato, senza Ldarga) ed
    /// elementi di array (Ldelema per rank 1, il metodo Address dell'array rettangolare
    /// per rank N — vedi EmitArrayElementAddress). Un'espressione generica (letterale,
    /// A+B, chiamata a funzione, ecc.) non ha un indirizzo a cui appoggiarsi: molti
    /// dialetti BASIC richiedono comunque un l-value per un argomento BYREF, quindi
    /// lancia esplicito invece di inventare una copia scratch silenziosa.
    /// </summary>

    // ── Callback FFI nominati con parametri array (2026-11-02) ────────────────────────────
    // Il metodo BASIC dichiara gli array come `T[]&`; il delegate generico (Func<T[],R>) è
    // by-value. Serve un delegate con parametri `ref` che combaci ESATTAMENTE con la firma.
    private static bool FfiCallbackHasArrayShape(DataType? t) =>
        t is FunctionPointerType fp && fp.ParameterTypes.Length >= 1 && fp.ParameterTypes[0] is ArrayType;

    private void EmitFfiCallbackFunctionReference(FunctionReferenceExpr fre)
    {
        // Il percorso generico (EmitFunctionReference) sceglie automaticamente il delegate
        // `ref` quando la FUNCTION ha parametri array: qui basta delegare.
        EmitFunctionReference(fre);
    }

    /// <summary>Delegate dedicato con parametri `ref` per una FUNCTION con array (null se la
    /// forma non e' una di quelle coperte).</summary>
    private Type? TryRefDelegateType(FunctionDefinition fd)
    {
        int nArr = fd.Parameters.Count(p => p.Flags.HasFlag(SymbolFlags.IsArray));
        if (nArr == 0 || nArr > 2) return null;
        int nSca = fd.Parameters.Length - nArr;
        var retClr = DataTypeMapper.Map(NormalizeInferredType(fd.ReturnType), _types, _classTypes);
        bool isLong = retClr == typeof(long) || retClr == typeof(int) || retClr == typeof(short);
        bool isArr = retClr == typeof(double[]);
        return (nArr, nSca, isLong, isArr) switch
        {
            (1, 0, false, false) => typeof(FfiRefCallbackTypes.CbRef1D),
            (1, 0, true, false) => typeof(FfiRefCallbackTypes.CbRef1I),
            (1, 1, false, false) => typeof(FfiRefCallbackTypes.CbRef1S1D),
            (1, 1, true, false) => typeof(FfiRefCallbackTypes.CbRef1S1I),
            (2, 0, false, false) => typeof(FfiRefCallbackTypes.CbRef2D),
            (2, 0, true, false) => typeof(FfiRefCallbackTypes.CbRef2I),
            (1, 0, false, true) => typeof(FfiRefCallbackTypes.CbRef1ArrD),
            (2, 0, false, true) => typeof(FfiRefCallbackTypes.CbRef2ArrD),
            _ => null
        };
    }

}
