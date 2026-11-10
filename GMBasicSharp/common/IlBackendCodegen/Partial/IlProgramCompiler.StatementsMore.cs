// FILE: IlBackend/Partial/IlProgramCompiler.StatementsMore.cs
// PARTIAL — dominio STATEMENTS AGGIUNTIVI (input/print/let/declaration/redim/
// load-*service/list-comprehension/asort).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using CommonLib;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio STATEMENTS AGGIUNTIVI.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitOnJumpStatement(OnJumpStmt stmt)
    {
        EmitExpression(stmt.IndexExpression);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Sub); // BASIC is 1-based
        var targets = stmt.TargetLabels;
        var defaultLabel = _il.DefineLabel();
        var labels = new Label[targets.Length];
        for (int i = 0; i < targets.Length; i++)
            labels[i] = _il.DefineLabel();
        _il.Emit(OpCodes.Switch, labels);
        _il.Emit(OpCodes.Br, defaultLabel);
        for (int i = 0; i < targets.Length; i++)
        {
            _il.MarkLabel(labels[i]);
            EmitBranch(targets[i]);
        }
        _il.MarkLabel(defaultLabel);
    }

    private void EmitInputStatement(InputStmt stmt)
    {
        // INPUT con più variabili (es. INPUT "..."; NOME$, ETA): il VM pusha la riga letta
        // UNA volta e la assegna a ogni variabile (nessuno split) — qui ReadLine produce
        // UN valore sullo stack: per N variabili serve Dup prima di ogni Stloc tranne
        // l'ultimo, altrimenti il 2° Stloc non trova nulla → InvalidProgram.
        _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadLine), Type.EmptyTypes)!);
        for (var i = 0; i < stmt.Variables.Length; i++)
        {
            var target = stmt.Variables[i];
            if (target is VariableTarget vt)
            {
                if (i < stmt.Variables.Length - 1) _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
            }
            else _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitLineInputStatement(LineInputStmt stmt)
    {
        _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.ReadLine), Type.EmptyTypes)!);
        if (stmt.Variable is VariableTarget vt)
            _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else
            _il.Emit(OpCodes.Pop);
    }

    private void EmitASortFunctionCall(BuiltinFunctionCallExpr expr)
    {
        // ARRAYS.ASORT(arr [, compareFunc] [, order$]) come FUNZIONE: ordina l'array
        // in-place (reference type: la copia del riferimento resta lo stesso array) e lo
        // ritorna — `DIM sorted = ASORT(unsorted)` produce sorted == unsorted ordinato.
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException("Backend IL: ASORT richiede da 1 a 3 argomenti.");
        // L'argomento può essere un'ESPRESSIONE array (ASORT(d.Keys()) — il valore è già un
        // array CLR): il tipo dell'elemento viene dall'inferenza; solo la variabile diretta
        // ha un ArrayInfo per il path in-place.
        _arrays.TryGetValue(expr.Args[0] is VariableExpr veSort ? veSort.SymbolId : -1, out var info);
        var isDirectArray = expr.Args[0] is VariableExpr && info is not null;
        var elementType = isDirectArray
            ? info!.ClrArrayType.GetElementType()!
            : NormalizeInferredType(InferType(expr.Args[0])) is ArrayType atSort
                ? DataTypeMapper.Map(atSort.ElementType, _types, _classTypes)
                : throw new NotSupportedException("Backend IL: ASORT richiede un array come argomento.");

        if (expr.Args.Length >= 2 && expr.Args[1] is not NilExpr)
        {
            EmitExpression(expr.Args[0]);
            EmitExpression(expr.Args[1]);
            var compareReturnClrType = DataTypeMapper.Map(GetCallbackReturnType(expr.Args[1]), _types, _classTypes);
            if (expr.Args.Length >= 3) EmitExpression(expr.Args[2]); else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Ldstr, "D");
            _il.Emit(OpCodes.Call, typeof(IlProgramCompiler)
                .Assembly.GetType("Basic.IlBackend.ArrayHelpers")!
                .GetMethod(nameof(ArrayHelpers.ASortWithComparer))!
                .MakeGenericMethod(elementType, compareReturnClrType));
        }
        else
        {
            // Ordina in-place via ASort<T>(array, bool) — carica l'array, lo duplica per
            // ritornarlo (il riferimento duplicato è lo stesso array), poi il flag descending.
            if (isDirectArray) _il.Emit(OpCodes.Ldloc, info!.Local);
            else EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Dup);
            EmitOrderDescendingFlag(expr.Args.Length >= 2 ? expr.Args[1] : null);
            _il.Emit(OpCodes.Call, ArrayHelpersASortMethod.MakeGenericMethod(elementType));
        }
    }


    /// <summary>Loop ricorsivo per ogni clausola FOR della comprensione (prodotto cartesiano).</summary>

    private DataType InferNullConditionalType(NullConditionalAccessExpr nca)
    {
        if (nca.Kind == NullConditionalKind.Member && nca.MemberToken is { } mt)
        {
            var structType = UnwrapNullableType(NormalizeInferredType(InferType(nca.Expression)));
            try
            {
                var fieldSymbol = ResolveFieldOrPropertySymbolIdByName(structType, mt.Lexeme);
                // UDT (value type) di base: il codegen (Expressions.cs, case
                // NullConditionalAccessExpr) boxa SEMPRE il risultato a object — il tipo
                // statico dell'espressione deve seguirlo, altrimenti il chiamante (PRINT,
                // EmitCoerceTypes, ...) genererebbe IL per il tipo NUDO del campo (es.
                // int32) su uno stack che in realtà porta un object boxed/null.
                if (DataTypeMapper.Map(structType, _types, _classTypes).IsValueType)
                    return new AnyType();
                return _program.Symbols.GetSymbol(fieldSymbol).DataType;
            }
            catch (NotSupportedException)
            {
                return new AnyType();
            }
        }
        if (nca.Kind == NullConditionalKind.Index)
            return InferType(nca.Expression);
        return new AnyType();
    }

    private void EmitDefaultValue(DataType type)
    {
        switch (type)
        {
            case DoubleType or InferredType: _il.Emit(OpCodes.Ldc_R8, 0.0); return;
            // SINGLE scalare: se il backend lo mappa a double (parità C, DataTypeMapper) il
            // default deve essere R8 — un Ldc_R4 finirebbe in un local/box double (merge IL
            // invalido: "ldc.r4 0 / box System.Double", visto su DIM ... AS SINGLE con la
            // mappatura double). Con la mappatura float resta R4.
            case SingleType:
                if (DataTypeMapper.Map(type, _types, _classTypes) == _types.Double) _il.Emit(OpCodes.Ldc_R8, 0.0);
                else _il.Emit(OpCodes.Ldc_R4, 0.0f);
                return;
            case LongType or ULongType or QuadType or UQuadType: _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_I8); return;
            // INTEGER/UINTEGER = 64 bit (DataTypeMapper): il default deve avere lo STESSO
            // tipo di stack del valore vero (unbox.any Int64 → I8). Con Ldc_I4_0 nudo il
            // merge dei rami "non inizializzato"/"inizializzato" di TryEmitLoadGlobal
            // vedeva I4 vs I8 → ILOptim "Inconsistent evaluation stack between basic
            // blocks" (tools/basic/settings_bundle.bas, funzioni con un globale INTEGER).
            case IntegerType or UIntegerType: _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_I8); return;
            case ByteType or SByteType or BitType: _il.Emit(OpCodes.Ldc_I4_0); return;
            case StringType: _il.Emit(OpCodes.Ldstr, string.Empty); return;
            case CurrencyType: _il.Emit(OpCodes.Ldc_R8, 0.0); _il.Emit(OpCodes.Call, _types.ConvertToDecimalDouble); return;
            // CLASS/INTERFACE: sempre un reference type CLR (mai una struct) — il default
            // di qualunque tipo riferimento è null, esattamente come "Nothing"/null in
            // BASIC per una variabile oggetto mai assegnata.
            case ClassType or InterfaceType or FunctionPointerType: _il.Emit(OpCodes.Ldnull); return;
            // UDT: struct CLR (value type) — il default NON è mai null (Ldnull su una
            // struct è IL invalido e può crashare il JIT, es. lettura di un UDT globale
            // mai inizializzato: il ramo "null" di TryEmitLoadGlobal produceva Ldnull
            // invece di una struct default). Initobj del tipo.
            case UserDefinedType udtDefault:
            {
                var structClr = DataTypeMapper.Map(type, _types, _classTypes);
                if (structClr.IsValueType)
                {
                    var tmpLocal = _il.DeclareLocal(structClr);
                    _il.Emit(OpCodes.Ldloca, tmpLocal);
                    _il.Emit(OpCodes.Initobj, structClr);
                    // Campi ARRAY del UDT (S3b, 2026-11-06): un initobj li lascia a NULL, e la
                    // variabile GLOBALE (questo ramo serve le globali senza inizializzatore)
                    // non passa mai da EmitScalarDeclaration → `g.A(0)` andava in
                    // NullReferenceException. Stessa allocazione del percorso locale.
                    foreach (var field in udtDefault.Fields)
                    {
                        if (field.Dimensions.IsEmpty) continue;
                        if (_classFieldArrays.TryGetValue(field.SymbolId, out var fai))
                            EmitFieldArrayAllocationCode(_il, _types, fai, field.Dimensions, () => _il.Emit(OpCodes.Ldloca, tmpLocal));
                    }
                    _il.Emit(OpCodes.Ldloc, tmpLocal);
                }
                else _il.Emit(OpCodes.Ldnull);
                return;
            }
            // DATE/TIME/DATETIME: mappati a System.DateTime (DataTypeMapper) — il default
            // è 0001-01-01, mai null (value type).
            case DateType or TimeType or DateTimeType or TimeSpanType:
            {
                var dtClr = DataTypeMapper.Map(type, _types, _classTypes);
                var tmpLocal = _il.DeclareLocal(dtClr);
                _il.Emit(OpCodes.Ldloca, tmpLocal);
                _il.Emit(OpCodes.Initobj, dtClr);
                _il.Emit(OpCodes.Ldloc, tmpLocal);
                return;
            }
            // Un array è anch'esso un reference type CLR — default null, come CLASS/INTERFACE
            // (serve per una FUNCTION AS <tipo>() il cui flusso raggiunge END FUNCTION senza
            // un RETURN/FUNCNAME= esplicito, vedi EmitEndProcedureStatement/EmitStatementBody).
            case ArrayType: _il.Emit(OpCodes.Ldnull); return;
            // Nullable (INTEGER?, STRING?, ...) e NIL: a runtime sono object (valore boxed
            // o null) — il default di un nullable è null (NIL). Golden TypesNullable.
            case NullableType or NilType: _il.Emit(OpCodes.Ldnull); return;
            // Collezioni/stringhe dinamiche: reference type → null di default.
            case ListType or DictType or SortedDictType or HashSetType or QueueType or StackType
                or LinkedListType or CircularLinkedListType or DoublyLinkedListType or RingBufferType
                or JsonType or AnyType or TupleType or SqlResultSetType: _il.Emit(OpCodes.Ldnull); return;
            // Handle/tipi mappati a object boxed (vedi DataTypeMapper): reference → null di default
            // (es. `DIM ws AS WEBSOCKET`, `DIM o AS HTTPOPTIONS`, `DIM r AS FETCHRESULT`).
            case HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
                or WebSocketType or CookieJarType or FetchResultType
                // REQ/RES delle pagine BSP (DIM REQ AS HTTPREQUEST / DIM RES AS HTTPRESPONSE
                // — il BspCompiler li dichiara perché l'emitter statico risolve campi/metodi):
                // il valore reale viene seminato dall'host prima dell'esecuzione.
                or HttpRequestType or HttpResponseType: _il.Emit(OpCodes.Ldnull); return;
            // FUTURE (GO FUNCTION ... INTO x): reference type (PCodeFutureValue, sealed class) —
            // default null come le altre collezioni/handle a riferimento. Bug 2026-09-08:
            // mancava questo case, il default cadeva su NotSupportedIlDataTypeException
            // ("tipo 'FutureType' non ancora mappato a un tipo CLR") per qualunque variabile
            // FUTURE dichiarata implicitamente prima dell'assegnazione da GO FUNCTION ... INTO.
            case FutureType: _il.Emit(OpCodes.Ldnull); return;
            // CHANNEL / WAITGROUP: come le collezioni — istanza REALE, non null (lo stepper GPM
            // opera sull'oggetto: GO SUB ... IN wg / WAIT wg / SEND/RECV). Stessa inizializzazione
            // del percorso LOCALE (EmitScalarDeclaration) per le GLOBALI senza inizializzatore.
            // Bug 2026-09-08: la capacità ESPLICITA (`DIM x AS CHANNEL(N) OF T`) veniva
            // ignorata in silenzio — sempre 10, qualunque N scritto dal programmatore.
            // Rispetta N se > 0 (0 = "non specificata" nell'AST, ChannelType.Capacity default),
            // altrimenti mantiene il default storico 10 (comportamento invariato per i
            // programmi esistenti che non specificano una capacità — cambiarlo a 0/unbuffered
            // richiederebbe una decisione di semantica del linguaggio condivisa con gli altri
            // backend, vedi Bugs/(open) - BackendC99-GoroutineChannel-Send-Senza-Receive-Hang.md).
            case ChannelType ct:
                _il.Emit(OpCodes.Ldc_I4, ct.Capacity > 0 ? ct.Capacity : 10);
                _il.Emit(OpCodes.Newobj, typeof(ChannelInfo).GetConstructor([typeof(int)])!);
                return;
            case WaitGroupType:
                _il.Emit(OpCodes.Newobj, typeof(PCodeWaitGroup).GetConstructor(Type.EmptyTypes)!);
                return;
            case MutexType:
                _il.Emit(OpCodes.Newobj, typeof(PCodeMutex).GetConstructor(Type.EmptyTypes)!);
                return;
            case RwMutexType:
                _il.Emit(OpCodes.Newobj, typeof(PCodeRwMutex).GetConstructor(Type.EmptyTypes)!);
                return;
            case OnceType:
                _il.Emit(OpCodes.Newobj, typeof(PCodeOnce).GetConstructor(Type.EmptyTypes)!);
                return;
            // Collezioni avanzate (materializzate in CommonLib/object/Dictionary): reference
            // type → null di default (DIM senza inizializzatore, es. `DIM g AS GRAPH`).
            case GraphType or CacheType or TrieType or BagType or TreeType or AVLTreeType
                or RedBlackTreeType or BPlusTreeType or BinaryHeapType or HashRingType or SparseSetType
                or BinaryIndexedTreeType or TimelineType or RebalanceStrategyType
                or BacktestResultType or PortfolioResultType or PortfolioConstraintsType: _il.Emit(OpCodes.Ldnull); return;
            // STRINGBUILDER / REGEX: reference type CLR (StringBuilder/RegexWrapper) → null.
            case StringBuilderType or RegexType: _il.Emit(OpCodes.Ldnull); return;
            // UnresolvedQualifiedType (es. il tipo di ritorno di un OPERATOR UDT, "Vector2D"
            // non ancora legato): mappato a object nel DataTypeMapper → default null.
            case UnresolvedQualifiedType: _il.Emit(OpCodes.Ldnull); return;
            // F4: valore matrice — default = matrice vuota (0×0), mai null (il descrittore è
            // un reference type ma le operazioni assumono sempre un valore valido).
            case MatrixValueType:
                _il.Emit(OpCodes.Call, typeof(MatrixValue).GetProperty(nameof(MatrixValue.Empty))!.GetGetMethod()!);
                return;
            default: throw new NotSupportedIlDataTypeException(type);
        }
    }

    private void EmitPrintStatement(PrintStmt stmt)
    {
        // Staging per-statement: il parziale non deve sopravvivere a un errore durante la
        // valutazione di un argomento (canone: scartare, parità PCode/VM-C/C99).
        _il.Emit(OpCodes.Call, IlRuntimePrintStageBeginMethod);
        foreach (var arg in stmt.Arguments)
        {
            // Parità VM: il parser genera il separatore ',' come un PrintArg(null, ",")
            // SEPARATO (vedi Parser.Stmt.Graphics.ParsePrintArguments) — NON come parte
            // del PrintArg dell'espressione. Qui si traduce in OP_PRINT_TAB_ZONE.
            if (arg.Expression is null)
            {
                if (arg.Separator == ",")
                {
                    EmitLoadConsoleService();
                    _il.Emit(OpCodes.Call, IlPrintTabZoneMethod); // ritorna la stringa di padding
                    EmitPrinterWrite(stmt.Token.Position, newLine: false);
                    var skipTabVideo = _il.DefineLabel();
                    var afterTabVideo = _il.DefineLabel();
                    _il.Emit(OpCodes.Call, IlPrinterOnlyMethod);
                    _il.Emit(OpCodes.Brtrue, skipTabVideo);
                    _il.Emit(OpCodes.Call, IlRuntimePrintDispatchMethod);
                    _il.Emit(OpCodes.Pop); // console scartato (PrintDispatch ha consumato la stringa)
                    _il.Emit(OpCodes.Br, afterTabVideo);
                    _il.MarkLabel(skipTabVideo);
                    _il.Emit(OpCodes.Pop);
                    _il.Emit(OpCodes.Pop);
                    _il.MarkLabel(afterTabVideo);
                }
                continue;
            }

            var argType = InferType(arg.Expression);
            // GPM: un operando che legge un future-target (GO FUNCTION...INTO x / FETCH
            // espresso, vedi EmitLoadVariable) può PARKARE durante la valutazione
            // (implicit-await). Il park richiede stack VUOTO al punto di blocco, ma il
            // pattern PRINT spinge la console SOTTO l'operando (EmitLoadConsoleService):
            // per questi operandi valuto PRIMA a stack vuoto in un temp object, poi carico
            // la console, poi ricarico il valore — il park è a stack pulito e la console
            // resta sotto il valore per il resto dell'emissione.
            var mayParkOperand = _gpmStepperMode
                && WalkAst(arg.Expression).OfType<VariableExpr>().Any(v => _gpmFutureTargets.Contains(v.SymbolId));
            if (mayParkOperand)
            {
                var parkTemp = _il.DeclareLocal(typeof(object));
                EmitExpression(arg.Expression);
                EmitDynamicValueBox(arg.Expression);
                _il.Emit(OpCodes.Stloc, parkTemp);
                EmitLoadConsoleService();
                _il.Emit(OpCodes.Ldloc, parkTemp);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.PrintFormatValue), typeof(object)));
            }
            else
            {
                EmitLoadConsoleService();
                EmitExpression(arg.Expression);
                // Formato PRINT parità VM (porting GM 8e48bed0/1ed14c73/79511cce): i
                // NUMERICI/BOOL e i tipi DINAMICI passano da IlRuntime.PrintFormatValue
                // (spazi + G15 + bool); gli altri (string/date) da EmitConvertToString.
                // EmitExpression è GIÀ stato emesso sopra — qui SOLO la conversione/box sul
                // valore sullo stack (un secondo EmitExpression produceva stack extra).
                var argTypeN = NormalizeInferredType(argType);
                // Simbolo "tipo ridefinito" (es. `S` numerico e `S$` stringa che condividono lo
                // STESSO SymbolId, vedi _redefinedTypeSymbols): il local è object e il tipo
                // statico può risultare STRING anche quando il valore è un Single boxed →
                // EmitConvertToString(StringType) è un no-op e il boxed finisce dove serve una
                // stringa: "InvalidCastException: Unable to cast System.Single to System.String"
                // (sample TypesConversions.bas, `DIM S AS SINGLE` + `DIM S$ AS STRING`).
                // Stampa TIPIZZATA a runtime, come per i numerici.
                var redefinedArg = arg.Expression is VariableExpr rvRedef
                    && _redefinedTypeSymbols.Contains(rvRedef.SymbolId);
                if (redefinedArg
                    || argTypeN is DoubleType or SingleType or LongType or ULongType or QuadType or UQuadType
                        or IntegerType or UIntegerType or ByteType or SByteType or BitType or CurrencyType
                    || argType is InferredType or AnyType or Basic.JsonType
                    || !IsDirectToStringType(argTypeN))
                {
                    EmitDynamicValueBox(arg.Expression);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.PrintFormatValue), typeof(object)));
                }
                else
                {
                    EmitConvertToString(argType);
                }
            }
            EmitPrinterWrite(arg.Expression.Position, newLine: false);
            var skipVideo = _il.DefineLabel();
            var afterVideo = _il.DefineLabel();
            _il.Emit(OpCodes.Call, IlPrinterOnlyMethod);
            _il.Emit(OpCodes.Brtrue, skipVideo);
            _il.Emit(OpCodes.Call, IlRuntimePrintDispatchMethod);
            _il.Emit(OpCodes.Pop); // console scartato (PrintDispatch ha consumato la stringa)
            _il.Emit(OpCodes.Br, afterVideo);
            _il.MarkLabel(skipVideo);
            _il.Emit(OpCodes.Pop); // testo NON stampato a video (DEVICE=PRINTER)
            _il.Emit(OpCodes.Pop); // console scartato
            _il.MarkLabel(afterVideo);
        }

        // PRINT termina con ';' o ',' come ULTIMO argomento (PrintArg(null, sep)) → NESSUN
        // a capo: il cursore resta sulla riga e il successivo PRINT si accoda (parità BASIC/
        // Clipper). La virgola emette già il padding tab-zone nel ramo argomenti.
        var endsWithSeparator = stmt.Arguments.Length > 0
            && stmt.Arguments[^1] is { Expression: null, Separator: ";" or "," };
        if (endsWithSeparator)
        {
            _il.Emit(OpCodes.Ldc_I4_1);
            _il.Emit(OpCodes.Call, IlRuntimePrintStageEndMethod);
            return;
        }

        _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Call, IlWritePrinterMethod);
        _il.Emit(OpCodes.Call, IlPrinterOnlyMethod);
        var skipNewLine = _il.DefineLabel();
        _il.Emit(OpCodes.Brtrue, skipNewLine);
        _il.Emit(OpCodes.Call, IlRuntimePrintNewLineDispatchMethod);
        _il.MarkLabel(skipNewLine);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Call, IlRuntimePrintStageEndMethod);
    }

    /// <summary>
    /// Emette la sequenza <c>IlRuntime.Current.Console</c>, lasciando sullo stack il
    /// riferimento a IConsoleService su cui invocare i successivi callvirt.
    /// </summary>
    /// <summary>
    /// Carica il riferimento a IRuntimeHost in un local condiviso, riusato da tutti
    /// i service loader. Evita di chiamare IlRuntime.Current (ThreadStatic) N volte.
    /// </summary>
    /// <summary>
    /// Root cause del bug "ON ERROR GOTO come primissima riga + statement adiacente che
    /// lancia" (vedi AiWiki IL-OnError-Loop-Bug.md/GMBasicAI_Errors.md): questi 6 loader
    /// (Runtime/Console/File/Network/Database/Http) cachavano il valore in un local
    /// METODO-WIDE popolato pigramente alla prima chiamata — corretto SOLO se "primo
    /// compilato" coincide con "primo eseguito", vero nel percorso semplice ma FALSO
    /// sotto il dispatcher ON ERROR per-riga: un chunk può benissimo non essere MAI
    /// eseguito a runtime (es. un errore in un chunk precedente salta DIRETTAMENTE
    /// all'handler, scavalcando chunk intermedi che pure vengono compilati PRIMA
    /// dell'handler nello stream). Se il chunk che "per primo" (in ordine di
    /// compilazione) popolava la cache non esegue mai, il local resta al suo default
    /// (null) — qualunque chunk SUCCESSIVO che lo riusa assumendolo già popolato
    /// dereferenzia null → NullReferenceException (a volte poi ri-catturata dal nuovo
    /// secondo catch(Exception) dello STESSO chunk, con errorHandlerTarget "sticky" =
    /// se stesso → loop infinito invece di un crash pulito). Fix: sotto ON ERROR
    /// (<see cref="_onErrorDispatch"/> non null — vero per OGNI chunk del dispatcher,
    /// impostato/azzerato ad ogni iterazione) riassegna SEMPRE il local anche se già
    /// dichiarato in precedenza — nessun costo di correttezza, costo di performance
    /// minimo (IlRuntime.Current è un getter ThreadStatic). Porting identico da GMBasic.
    /// </summary>
    private void EmitLoadRuntime()
    {
        if (_runtimeLocal == null)
        {
            _runtimeLocal = _il.DeclareLocal(typeof(Runtime.IRuntimeHost));
            _il.Emit(OpCodes.Call, IlRuntimeCurrentGetter);
            _il.Emit(OpCodes.Stloc, _runtimeLocal);
        }
        else
        {
            // Ricarica SEMPRE (stesso motivo del console: il local dichiarato nel PRIMO ramo
            // di un IF non è inizializzato negli altri rami → NRE).
            _il.Emit(OpCodes.Call, IlRuntimeCurrentGetter);
            _il.Emit(OpCodes.Stloc, _runtimeLocal);
        }
        _il.Emit(OpCodes.Ldloc, _runtimeLocal);
    }

    private void EmitLoadConsoleService()
    {
        if (_consoleLocal == null)
        {
            _consoleLocal = _il.DeclareLocal(typeof(Runtime.IConsoleService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostConsoleGetter); _il.Emit(OpCodes.Stloc, _consoleLocal);
        }
        else
        {
            // Ricarica SEMPRE (anche nel percorso lineare): il local dichiarato nel PRIMO
            // ramo di un IF non è inizializzato negli ALTRI rami (il primo Stloc è nel ramo
            // preso) — un PRINT in un ramo alternativo leggeva il local null → NRE
            // (FunctionsDelegates.bas: il ramo '-' di GetOperation).
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostConsoleGetter); _il.Emit(OpCodes.Stloc, _consoleLocal);
        }
        _il.Emit(OpCodes.Ldloc, _consoleLocal);
    }

    private void EmitLoadGraphicsService()
    {
        if (_graphicsLocal == null)
        {
            _graphicsLocal = _il.DeclareLocal(typeof(Runtime.IGraphicsService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostGraphicsGetter); _il.Emit(OpCodes.Stloc, _graphicsLocal);
        }
        else
        {
            // Ricarica SEMPRE (stesso motivo di EmitLoadConsoleService: il local del primo
            // ramo di un IF non è inizializzato negli altri rami).
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostGraphicsGetter); _il.Emit(OpCodes.Stloc, _graphicsLocal);
        }
        _il.Emit(OpCodes.Ldloc, _graphicsLocal);
    }

    private void EmitLoadAudioService()
    {
        if (_audioLocal == null)
        {
            _audioLocal = _il.DeclareLocal(typeof(Runtime.IAudioService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostAudioGetter); _il.Emit(OpCodes.Stloc, _audioLocal);
        }
        else
        {
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostAudioGetter); _il.Emit(OpCodes.Stloc, _audioLocal);
        }
        _il.Emit(OpCodes.Ldloc, _audioLocal);
    }

    private void EmitLoadFileService()
    {
        if (_fileLocal == null)
        {
            _fileLocal = _il.DeclareLocal(typeof(Runtime.IFileService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostFileGetter); _il.Emit(OpCodes.Stloc, _fileLocal);
        }
        else
        {
            // Ricarica SEMPRE (stesso pattern di EmitLoadConsoleService/GraphicsService/AudioService):
            // il local dichiarato nel PRIMO ramo di un IF non è inizializzato negli ALTRI rami
            // (il primo Stloc è nel ramo preso) — una CHDIR dopo un MKDIR saltato
            // (IF NOT DIREXISTS THEN MKDIR) leggeva il local null → NRE. Bug CHDIR-concat.
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostFileGetter); _il.Emit(OpCodes.Stloc, _fileLocal);
        }
        _il.Emit(OpCodes.Ldloc, _fileLocal);
    }

    private void EmitLoadNetworkService()
    {
        if (_networkLocal == null)
        {
            _networkLocal = _il.DeclareLocal(typeof(Runtime.INetworkService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostNetworkGetter); _il.Emit(OpCodes.Stloc, _networkLocal);
        }
        else if (_onErrorDispatch is not null)
        {
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostNetworkGetter); _il.Emit(OpCodes.Stloc, _networkLocal);
        }
        _il.Emit(OpCodes.Ldloc, _networkLocal);
    }

    private void EmitLoadDatabaseService()
    {
        if (_dbLocal == null)
        {
            _dbLocal = _il.DeclareLocal(typeof(Runtime.IDatabaseService));
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostDatabaseGetter); _il.Emit(OpCodes.Stloc, _dbLocal);
        }
        else if (_onErrorDispatch is not null)
        {
            EmitLoadRuntime(); _il.Emit(OpCodes.Callvirt, RuntimeHostDatabaseGetter); _il.Emit(OpCodes.Stloc, _dbLocal);
        }
        _il.Emit(OpCodes.Ldloc, _dbLocal);
    }

    private void EmitLetStatement(LetStmt stmt)
    {
        foreach (var assignment in stmt.Assignments)
        {
            switch (assignment.Target)
            {
                case VariableTarget target:
                    EmitStoreVariable(target.SymbolId, assignment.Value);
                    break;
                case ArrayElementTarget target:
                    EmitArrayElementAssignment(target, assignment.Value);
                    break;
                case FieldTarget target:
                    EmitFieldAssignment(target, assignment.Value);
                    break;
                case IndexedTarget target:
                    EmitIndexedTargetAssignment(target, assignment.Value);
                    break;
                default:
                    throw new NotSupportedException(
                        $"Backend IL: assegnazione a target '{assignment.Target.GetType().Name}' non ancora supportata.");
            }
        }
    }

    /// <summary>
    /// DIM: array di qualunque rank N (mono e multi-dimensionali), con limiti
    /// inferiori espliciti costanti a compile-time (DIM A(2 TO 10)) e
    /// inizializzatore letterale {...} per il caso mono-dimensionale.
    /// "DIM A(N)" alloca N+1 elementi (indici 0..N inclusi), stessa semantica
    /// del p-code Compiler esistente (vedi Compiler.Assign.cs, "declaredSize =
    /// (int)num.Value + 1"). Rank 1 usa un vero SZArray CLR (T[], newarr);
    /// rank N usa un vero array rettangolare CLR (T[,...], newobj sul
    /// costruttore che prende N lunghezze) — MAI un array jagged (T[][], che
    /// sarebbe N allocazioni separate non contigue) e MAI Array.CreateInstance
    /// con limiti inferiori nativi (che forzerebbe GetValue/SetValue, quindi
    /// boxing per i tipi valore). Limiti inferiori diversi da 0 sono invece
    /// implementati con un trucco a compile-time: l'indice richiesto viene
    /// shiftato (sottratto del limite inferiore) prima di ogni ldelem/stelem/
    /// Get/Set, cosi' l'array sottostante resta sempre un vero 0-based
    /// contiguo — stesso approccio di un compilatore Pascal/Ada nativo,
    /// nessun boxing, nessuna indirezione extra oltre una sottrazione per
    /// indice. GLOBAL/LOCAL/STATIC non ancora supportati.
    /// </summary>
    private void EmitDeclarationStatement(DeclarationStmt stmt)
    {
        // LET X() = {...} è la forma di DICHIARAZIONE implicita del BASIC (il parser la
        // rappresenta come DeclarationStmt con keyword LET) — stesso trattamento del DIM
        // (come GM, porting 55eb83a2).
        if (stmt.Keyword.Kind != TokenKind.Dim && stmt.Keyword.Kind != TokenKind.Let
            && stmt.Keyword.Kind != TokenKind.Local && stmt.Keyword.Kind != TokenKind.Global
            && stmt.Keyword.Kind != TokenKind.Dynamic && stmt.Keyword.Kind != TokenKind.Static
            && stmt.Keyword.Kind != TokenKind.Var)
            throw new NotSupportedException(
                $"Backend IL: dichiarazione '{stmt.Keyword.Lexeme}' non ancora supportata (solo DIM/LET/LOCAL/GLOBAL/DYNAMIC/STATIC/VAR).");

        // DYNAMIC (scope dinamico per NOME, spec VARIABLES §2.5): la dichiarazione crea (o
        // ombreggia) un valore nel frame dello scope dinamico corrente — MAI un local CIL (un
        // local sarebbe visibile solo alla procedura dichiarante). Scala re: DYNAMIC è pensata
        // per scalari; un'eventuale forma ad array ricade nel percorso normale.
        if (stmt.Keyword.Kind == TokenKind.Dynamic
            && stmt.Declarations.All(d => d.Dimensions.IsEmpty))
        {
            foreach (var declaration in stmt.Declarations)
                EmitDynamicDeclaration(declaration);
            return;
        }

        foreach (var declaration in stmt.Declarations)
        {
            if (declaration.Dimensions.IsEmpty)
            {
                EmitScalarDeclaration(declaration);
                RegisterDynamicArrayLocal(declaration);
                // Array DINAMICO senza inizializzatore: parte VUOTO (Array.Empty<T>), mai
                // null — AADD/AREDIM (Array.Resize) su un array null dava
                // NullReferenceException (array.Length).
                if (declaration.InitialValue is null
                    && _arrays.TryGetValue(declaration.SymbolId, out var dynArrayInfo))
                {
                    var elemClr = dynArrayInfo.ClrArrayType.GetElementType()!;
                    if (IsStaticSymbol(declaration.SymbolId))
                    {
                        // STATIC senza inizializzatore: bag → array vuoto UNA SOLA volta
                        // (il local sarebbe inutile, il LoadRef legge dal bag).
                        _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Newarr, elemClr);
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StaticInitOnce), typeof(int), typeof(object)));
                    }
                    else if (IsGlobalSymbol(declaration.SymbolId))
                    {
                        // GLOBALE senza inizializzatore: il riferimento vive nel BAG
                        // (LoadGlobal), non nel local → l'array vuoto va scritto nel bag.
                        // Prima: Stloc sul local (mai letto per un globale) → il globale
                        // restava null e PRINT/UBOUND di `DIM x()` a livello script
                        // stampavano vuoto (ArraysPrintFormat.bas: empty = [] vs vuoto).
                        _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Newarr, elemClr);
                        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
                    }
                    else
                    {
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Newarr, elemClr);
                        _il.Emit(OpCodes.Stloc, dynArrayInfo.Local);
                    }
                }
                continue;
            }
            EmitArrayDeclaration(declaration);
        }
    }

    /// <summary>
    /// DIM di una variabile scalare non-array (es. <c>DIM P AS Punto</c>) — necessario
    /// soprattutto per i tipi CLASSE: senza un tipo dichiarato esplicitamente, il Linker
    /// tratta un nome non suffissato come DOUBLE per convenzione e rifiuta di assegnargli
    /// un'istanza di classe ("Impossibile assegnare un valore di tipo ClassType a una
    /// variabile di tipo DOUBLE"). Alloca il locale col tipo giusto (GetOrDeclareLocal
    /// legge già symbol.DataType) ed emette l'inizializzatore se presente — nessun'altra
    /// differenza da una LET su una variabile mai vista prima.
    /// </summary>
    private void EmitScalarDeclaration(VariableDeclaration declaration)
    {
        if (IsGlobalSymbol(declaration.SymbolId))
        {
            if (declaration.InitialValue is not null)
            {
                EmitStoreVariable(declaration.SymbolId, declaration.InitialValue);
                return;
            }
            // Un DIM globale SENZA inizializzatore non deve sovrascrivere un valore già
            // iniettato dall'host in ProgramGlobals PRIMA dell'esecuzione (es. il seed BSP di
            // REQ/RES/SESSION in ServeBspPageIlBackend): il default si applica solo se l'host
            // non ha fornito nulla (LoadGlobal → null). Senza, la entry `DIM REQ AS HTTPREQUEST`
            // azzerava REQ/RES con StoreGlobal(id, null) e ogni REQ.Method/RES.Send leggeva null.
            var skipDefaultStore = _il.DefineLabel();
            _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
            _il.Emit(OpCodes.Brtrue, skipDefaultStore);

            _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
            var declaredType = declaration.DataType
                ?? NormalizeInferredType(_program.Symbols.GetSymbol(declaration.SymbolId).DataType);
            // StringBuilder/REGEX "pronto all'uso" (r.Pattern=..., sb.Append(...) subito): un
            // GLOBALE senza inizializzatore deve materializzare l'istanza, non restare null
            // (come il percorso LOCALE in EmitScalarDeclaration).
            if (declaredType is StringBuilderType or RegexType)
                EmitNewNativeCollection(declaredType);
            // Collezioni avanzate (RINGBUFFER/TREE/BAG/GRAPH/...): un GLOBALE senza
            // inizializzatore deve materializzare l'istanza, non restare null —
            // altrimenti MyRing.ENQUEUE/Count → NullReferenceException e Capacity=0
            // (bug IL di CollectionsGraphMissing, top-level).
            else if (declaredType is RingBufferType or TreeType or AVLTreeType or RedBlackTreeType
                or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType
                or BinaryHeapType or BagType or CacheType or TimelineType or BinaryIndexedTreeType
                or CircularLinkedListType or DoublyLinkedListType or HyperLogLogType)
                EmitNewAdvancedCollection(declaredType);
            else
                EmitDefaultValue(declaredType);
            // GLOBALE: box col tipo del BAG (GlobalClrType), non DataTypeMapper — per BIT il
            // bag usa Int32 (parità VM: i logici sono 1/0/-1), mentre DataTypeMapper mappa BIT
            // a bool: lo store boxava bool e la lettura (LoadGlobal→GlobalClrType=Int32)
            // falliva con InvalidCastException Boolean→Int32 (bug BIT globale, 2026-09-30).
            // Tipo del BOX dal tipo DICHIARATO (non dal simbolo): il Linker può aver
            // ri-tipato il simbolo (es. `DIM V AS BIT` + `V = -1` → symbol ANY, che
            // GlobalClrType mappa a object) e allora il box non veniva emesso su un default
            // che è un VALUE TYPE → `StoreGlobal(int, object)` con un int32 = IL invalido
            // (InvalidProgram alla prima esecuzione: MiniTest/sample con BIT globale).
            // BIT resta Int32 nel bag (convenzione GlobalClrType, parità VM).
            var declaredClr = declaredType is BitType
                ? _types.Int32
                : DataTypeMapper.Map(declaredType, _types, _classTypes);
            if (declaredClr.IsValueType) _il.Emit(OpCodes.Box, declaredClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            _il.MarkLabel(skipDefaultStore);
            return;
        }
        if (IsStaticSymbol(declaration.SymbolId))
        {
            // STATIC: persiste tra chiamate. Con inizializzatore → init UNA SOLA volta
            // (StaticInitOnce: stack [id, boxedValue]). Senza → niente a compile-time: le
            // letture (TryEmitLoadStatic) ricadono sul default del tipo se mai scritte.
            if (declaration.InitialValue is not null)
            {
                _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                EmitStoredValue(declaration.SymbolId, declaration.InitialValue);
                var declaredClr = StaticClrType(declaration.SymbolId);
                if (declaredClr.IsValueType) _il.Emit(OpCodes.Box, declaredClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StaticInitOnce), typeof(int), typeof(object)));
            }
            return;
        }
        /* `DIM f = <FUNCTION nominata con parametri array>`: il riferimento è un delegate
         * `ref T[]` (CbRef…), non un Func<…>; dichiara il local con QUEL tipo, altrimenti
         * lo Stloc è invalido e la successiva Invoke passa un riferimento array sbagliato. */
        if (declaration.InitialValue is FunctionReferenceExpr fre
            && _program.Symbols.GetSymbol(fre.SymbolId).DeclarationNode is FunctionDefinition fdRef
            && TryRefDelegateType(fdRef) is { } refDel)
        {
            var refLocal = _il.DeclareLocal(refDel);
            _locals[declaration.SymbolId] = refLocal;
            EmitExpression(fre);
            _il.Emit(OpCodes.Stloc, refLocal);
            return;
        }
        var local = GetOrDeclareLocal(declaration.SymbolId);
        // Fase 4 (Closure-ByRef-Box-Linker-Definitivo): la PRIMA scrittura di un local box-typed
        // deve COSTRUIRE il box (Newobj), mai scrivere "attraverso" un box che non esiste ancora
        // (a differenza di ogni riassegnazione successiva, gestita dal ramo generico di
        // EmitStoreVariable — quello sì trova il box già presente e ci scrive dentro).
        if (BoxedSymbols.Contains(declaration.SymbolId))
        {
            var boxCtor = local.LocalType.GetConstructor([local.LocalType.GetGenericArguments()[0]])!;
            if (declaration.InitialValue is not null)
                EmitStoredValue(declaration.SymbolId, declaration.InitialValue);
            else
                EmitDefaultValue(declaration.DataType ?? NormalizeInferredType(_program.Symbols.GetSymbol(declaration.SymbolId).DataType));
            _il.Emit(OpCodes.Newobj, boxCtor);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.InitialValue is not null)
        {
            EmitStoreVariable(declaration.SymbolId, declaration.InitialValue);
            return;
        }

        // Un simbolo ridefinito usa un local object, il cui default CLR sarebbe null.
        // Ogni DIM deve però applicare il default del tipo BASIC dichiarato in quel
        // punto (`SINGLE` = 0, `STRING` = "", ecc.) e boxarlo quando è un value type.
        if (_redefinedTypeSymbols.Contains(declaration.SymbolId))
        {
            var declaredType = declaration.DataType
                ?? NormalizeInferredType(_program.Symbols.GetSymbol(declaration.SymbolId).DataType);
            EmitDefaultValue(declaredType);
            var declaredClr = DataTypeMapper.Map(declaredType, _types, _classTypes);
            if (declaredClr.IsValueType) _il.Emit(OpCodes.Box, declaredClr);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // Le collezioni CLR hanno una semantica BASIC da "valore pronto all'uso":
        // `DIM V AS LIST OF INTEGER` deve consentire subito `V.Add(...)`, senza
        // imporre un NEW esplicito.  Il CLR inizializza invece ogni reference a null,
        // dunque materializziamo qui lo stesso costruttore vuoto usato da NEW.
        if (declaration.DataType is ListType or DictType or HashSetType or QueueType or StackType or SortedDictType or LinkedListType or StringBuilderType)
        {
            EmitNewNativeCollection(declaration.DataType);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // String senza inizializzatore: il CLR default è null, ma il BASIC usa "" — e un
        // local string mai scritto viene ri-tipato da ILOptim (DistIL) come int32 (il
        // default numerico) → stelem int32 in un object[] = IL invalido (es. il target
        // out `TryGetValue(k, v$)` con v$ mai assegnata).
        if (declaration.DataType is StringType && local.LocalType == typeof(string))
        {
            _il.Emit(OpCodes.Ldstr, "");
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // Strutture avanzate (CommonLib reali o SortedDictionary): anche `DIM x AS CACHE`
        // senza NEW deve essere "pronto all'uso" (x.Set(...) subito) — prima restava null
        // e ogni metodo era un no-op silenzioso (bug scoperto verificando Cache con TTL).
        if (declaration.DataType is CacheType or TimelineType or TrieType or SparseSetType
            or RingBufferType or TreeType or AVLTreeType or RedBlackTreeType or BinaryIndexedTreeType
            or BPlusTreeType or GraphType or HashRingType or BinaryHeapType or BagType or CircularLinkedListType or DoublyLinkedListType
            or HyperLogLogType)
        {
            // NB: usa il tipo del SYMBOL (ValueType già risolto, es. Timeline<string>), non
            // declaration.DataType: il tipo dichiarato nel nodo può avere il ValueType NON
            // materializzato (InferredType → Timeline<object>) mentre il local è il tipo
            // giusto → newobj Timeline<object> in un local Timeline<string> = IL invalido.
            var advancedType = NormalizeInferredType(_program.Symbols.GetSymbol(declaration.SymbolId).DataType);
            EmitNewAdvancedCollection(advancedType);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // GPM flat (parità PCodeVM, G8-bis): POOL/COND/SYNCMAP/ATOMIC — oggetti "pronti
        // all'uso" anche nel path lineare (es. un programma solo POOL non è may-park).
        if (declaration.DataType is PoolType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodePool).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is CondType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeCond).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is ConcurrentMapType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeConcurrentMap).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is AtomicType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeAtomic).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }


        // REGEX (r AS REGEX): allocazione immediata — r.Pattern = ... senza NEW deve
        // funzionare (come le collezioni "pronte all'uso").
        if (declaration.DataType is RegexType)
        {
            _il.Emit(OpCodes.Newobj, typeof(Basic.RegexNative.RegexWrapper).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // ChannelType e WaitGroupType: inizializzazione automatica come le collezioni
        if (declaration.DataType is ChannelType declCt)
        {
            // Bug 2026-09-08: capacità esplicita (`DIM x AS CHANNEL(N) OF T`) ignorata in
            // silenzio — vedi nota gemella in EmitDefaultValueForType sopra.
            _il.Emit(OpCodes.Ldc_I4, declCt.Capacity > 0 ? declCt.Capacity : 10); // default capacity
            _il.Emit(OpCodes.Newobj, typeof(ChannelInfo).GetConstructor([typeof(int)])!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is WaitGroupType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeWaitGroup).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is MutexType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeMutex).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }
        if (declaration.DataType is RwMutexType)
        {
            _il.Emit(OpCodes.Newobj, typeof(PCodeRwMutex).GetConstructor(Type.EmptyTypes)!);
            _il.Emit(OpCodes.Stloc, local);
            return;
        }

        // DIM x AS TipoUdt (senza NEW, l'idioma classico per un TYPE...END TYPE — mai un
        // vero SUB NEW, a differenza di una CLASS) non passa da NESSUN costruttore: il
        // locale è già zero-inizializzato dal CLR (.locals init), ma un eventuale campo
        // ARRAY del UDT resterebbe null per sempre se non allocato qui esplicitamente —
        // stessa allocazione fatta da EmitUdtConstructorBody per il caso (più raro) di un
        // UDT costruito via NEW, solo con Ldloca (indirizzo del locale) invece di Ldarg_0
        // ("this" di un costruttore) come riferimento su cui fare Stfld.
        if (declaration.DataType is UserDefinedType udt)
        {
            foreach (var field in udt.Fields)
            {
                if (field.Dimensions.IsEmpty) continue;
                EmitFieldArrayAllocationCode(_il, _types, _classFieldArrays[field.SymbolId], field.Dimensions, () => _il.Emit(OpCodes.Ldloca, local));
            }
            return;
        }

        // Scalari NUMERICI senza inizializzatore (DIM s AS INTEGER/DOUBLE/...): emetti lo
        // store ESPLICITO del default (0). Il CLR azzererebbe comunque (.locals init), ma
        // SsaPromotion (ILOptim) non vede alcun def per il local → il phi usa `Undef` e, una
        // volta rimossa la slot, il valore viene letto da uno slot RIUSATO (es. il contatore
        // di un FOR precedente) → risultato ERRATO (bug: accumulatore `DIM s AS INTEGER` con
        // array + doppio FOR dava 39 invece di 36; le altre gambe corrette). Uno store
        // esplicito dà al pass un def reale. Solo tipi numerici: UDT/collezioni/stringa hanno
        // i propri rami sopra (un default-store qui romperebbe le loro allocazioni).
        if (declaration.DataType is SingleType or DoubleType or CurrencyType or IntegerType
            or LongType or ByteType or QuadType or BitType or DateType or TimeType or DateTimeType)
        {
            EmitDefaultValue(declaration.DataType);
            _il.Emit(OpCodes.Stloc, local);
        }
    }

    /// <summary>
    /// DIM x() AS T (parentesi vuote, nessuna dimensione) — un array "dinamico" il cui
    /// contenuto arriva SEMPRE da una riassegnazione whole-array successiva (idioma
    /// <c>x() = QualcheFunzione(...)</c>, comunissimo per i builtin che restituiscono
    /// ARRAY OF T, es. AESENCRYPT/FROMHEX). EmitScalarDeclaration alloca solo il locale
    /// (null-inizializzato, nessun Newarr: la dimensione non è nota qui) senza mai
    /// registrarlo in _arrays — UBOUND/LBOUND/l'indicizzazione (che dipendono da
    /// _arrays.TryGetValue, vedi ResolveArrayBoundArgs/GetArrayInfo) fallivano quindi
    /// sempre per un array così dichiarato, anche DOPO l'assegnazione che lo popola.
    /// Rank sempre 1: la sintassi a parentesi vuote non permette di esprimere un rank
    /// diverso (nessuna dimensione da cui dedurlo), e nessun idioma osservato in questo
    /// dialetto la usa per un array rettangolare.
    /// </summary>


    /// <summary>
    /// REDIM: copertura attuale solo array MONO-DIMENSIONALI (rank N richiederebbe
    /// una logica di ridimensionamento per-asse che preserva solo l'ultima
    /// dimensione, come nei dialetti BASIC di riferimento — non ancora fatta).
    /// La nuova dimensione può essere un'espressione RUNTIME qualunque (non solo un
    /// letterale compile-time), vedi EmitAxisLength — es. <c>REDIM A(N)</c> con N
    /// variabile. Con PRESERVE usa Array.Resize&lt;T&gt; (metodo generico,
    /// MakeGenericMethod sul vero tipo elemento): rialloca un nuovo T[] e copia il
    /// contenuto in modo contiguo, senza boxing (T resta un parametro di tipo reale,
    /// mai object). Senza PRESERVE è equivalente a un nuovo DIM (contenuto azzerato).
    /// </summary>

    /// <summary>
    /// Risolve il Target di uno statement array in stile Clipper (AADD/AINS/ADEL/ASORT/
    /// AEVAL/AFILL/ACOPY) — stesso vincolo di REDIM: solo un VariableTarget (un semplice
    /// nome di array DIMensionato direttamente), mai un elemento indicizzato o un campo.
    /// </summary>
    private (ArrayInfo Info, int SymbolId) ResolveArrayStmtTarget(Target target, string name)
    {
        if (target is not VariableTarget vt)
            throw new NotSupportedException($"Backend IL: {name} richiede un semplice nome di array (non un elemento indicizzato).");
        if (!_arrays.TryGetValue(vt.SymbolId, out var info))
            throw new NotSupportedException($"Backend IL: {name} su un array non DIMensionato ('{vt.NameCleaned.Lexeme}') non supportato — serve un DIM precedente.");
        if (info.Rank != 1)
            throw new NotSupportedException($"Backend IL: {name} supporta solo array mono-dimensionali.");
        return (info, vt.SymbolId);
    }

    /// <summary>Stessa scrittura indietro attraverso un parametro-array BYREF di EmitRedimStatement — vedi lì per il perché.</summary>
}
