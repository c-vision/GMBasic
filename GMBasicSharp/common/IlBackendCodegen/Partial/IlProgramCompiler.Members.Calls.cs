// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Members.Calls.cs
// PARTIAL — dominio MEMBERS/CALLS: method-call, indicizzazione, fetch/http, operatori,
// referenze qualificate. Migrato da IlProgramCompiler.Members.cs (2026-08-30 — split
// 6.0.1, file <= 1200 righe).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using CommonLib;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: method-call/indicizzazione/operatori.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitFetchFunctionCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: FETCH richiede 1 o 2 argomenti (url, [options]).");
        EmitObjectBoxedArgument(expr.Args[0]);
        if (expr.Args.Length == 2) EmitObjectBoxedArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeFetchFunctionMethod);
    }

    private void EmitHttpBodyCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        // HTTP.POST/PUT(url [, body [, contentType]]) → IlRuntime.HttpPost/HttpPut(string, string?, string?).
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1, 2 o 3 argomenti (url, [body], [contentType]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string)));
    }

    private void EmitStringBuilderMethodCall(MethodCallExpr expr)
    {
        var name = expr.MethodName.Lexeme;
        var sbType = typeof(System.Text.StringBuilder);
        EmitExpression(expr.Instance);
        switch (name.ToUpperInvariant())
        {
            case "APPEND":
            {
                if (expr.Args.Length != 1)
                    throw new NotSupportedException("Backend IL: StringBuilder.Append richiede 1 argomento.");
                var m = sbType.GetMethod("Append", [typeof(string)])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.Append(string) non trovato.");
                // Append converte SEMPRE a stringa (sb.Append(3.14159) come nel sample):
                // come la VM (args[0]?.ToString()).
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "APPENDLINE":
            {
                if (expr.Args.Length != 1)
                    throw new NotSupportedException("Backend IL: StringBuilder.AppendLine richiede 1 argomento.");
                var m = sbType.GetMethod("AppendLine", [typeof(string)])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.AppendLine(string) non trovato.");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "APPENDFORMAT":
            {
                if (expr.Args.Length < 1)
                    throw new NotSupportedException("Backend IL: StringBuilder.AppendFormat richiede almeno 1 argomento.");
                var m = sbType.GetMethod("AppendFormat", [typeof(string), typeof(object[])])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.AppendFormat(string, object[]) non trovato.");
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Ldc_I4, expr.Args.Length - 1);
                _il.Emit(OpCodes.Newarr, _types.Object);
                for (var i = 1; i < expr.Args.Length; i++)
                {
                    _il.Emit(OpCodes.Dup);
                    _il.Emit(OpCodes.Ldc_I4, i - 1);
                    EmitExpression(expr.Args[i]);
                    var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
                    if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
                    _il.Emit(OpCodes.Stelem_Ref);
                }
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "TOSTRING":
            {
                var m = sbType.GetMethod("ToString", Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.ToString() non trovato.");
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "CLEAR":
            {
                var m = sbType.GetMethod("Clear", Type.EmptyTypes)
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.Clear() non trovato.");
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "INSERT":
            {
                if (expr.Args.Length != 2)
                    throw new NotSupportedException("Backend IL: StringBuilder.Insert richiede 2 argomenti (posizione, testo).");
                var m = sbType.GetMethod("Insert", [typeof(int), typeof(string)])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.Insert(int, string) non trovato.");
                EmitInt32Argument(expr.Args[0]);
                EmitStringArg(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "REMOVE":
            {
                if (expr.Args.Length != 2)
                    throw new NotSupportedException("Backend IL: StringBuilder.Remove richiede 2 argomenti (inizio, lunghezza).");
                var m = sbType.GetMethod("Remove", [typeof(int), typeof(int)])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.Remove(int, int) non trovato.");
                EmitInt32Argument(expr.Args[0]);
                EmitInt32Argument(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            case "REPLACE":
            {
                if (expr.Args.Length != 2)
                    throw new NotSupportedException("Backend IL: StringBuilder.Replace richiede 2 argomenti (da, a).");
                var m = sbType.GetMethod("Replace", [typeof(string), typeof(string)])
                    ?? throw new NotSupportedException("Backend IL: StringBuilder.Replace(string, string) non trovato.");
                EmitStringArg(expr.Args[0]);
                EmitStringArg(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, m);
                break;
            }
            default:
                throw new NotSupportedException($"Backend IL: metodo StringBuilder '{name}' non ancora supportato.");
        }
    }

    private void EmitMethodCallCore(Expr instance, FunctionDefinition methodDef, ImmutableArray<Expr> args,
        bool forceNonVirtual = false)
    {
        if (!_procedureMethods.TryGetValue(methodDef.SymbolId, out var methodBuilder))
            throw new NotSupportedException($"Backend IL: metodo '{methodDef.OriginalName.Lexeme}' non trovato o non ancora supportato.");

        // STATIC: nessuna istanza da caricare sullo stack, mai callvirt (un metodo
        // statico non partecipa al dispatch dinamico — non ha un tipo runtime da cui
        // risolvere l'override). Un metodo d'istanza su un VALUE TYPE (UDT) richiede
        // l'INDIRIZZO dell'istanza (T&), mai il VALORE — `call` su un metodo d'istanza di
        // uno struct si aspetta sempre un managed pointer come `this` per convenzione CLR
        // (anche se la firma del MethodBuilder non lo mostra esplicitamente), a differenza
        // di un metodo di CLASS (reference type: il riferimento stesso è il "this").
        var isStatic = methodDef.Flags.HasFlag(SymbolFlags.IsStatic);
        /* Argomenti BYREF che vivono nel bag globale/statico come reference type: stesso
         * meccanismo di EmitFunctionCall (temporaneo + write-back dopo la call). */
        var bagSlotFixes = new System.Collections.Generic.List<ByRefBagSlotFix>();
        var prevBagSlotFixes = _byRefBagSlotFixes;
        _byRefBagSlotFixes = bagSlotFixes;
        if (!isStatic)
        {
            if (methodBuilder.DeclaringType!.IsValueType) EmitAddressOf(instance);
            else EmitExpression(instance);
        }
        for (var i = 0; i < args.Length; i++)
        {
            if (methodDef.Parameters[i].Flags.HasFlag(SymbolFlags.IsByRef))
                EmitAddressOf(args[i]);
            else if (NormalizeInferredType(methodDef.Parameters[i].DataType) is AnyType)
                EmitObjectBoxedArgument(args[i]);   // come la chiamata a procedura (Expressions.cs): un letterale numerico/booleano verso `g AS ANY` va BOXATO (prima `ldc.r8` nudo -> InvalidProgram)
            else
            {
                EmitExpression(args[i]);
                // Coerce numerico dell'argomento al tipo del parametro (parità con
                // EmitFunctionCall). Senza di ciò un argomento double (es. VAL(...)) verso un
                // parametro INTEGER/LONG/SINGLE lascia un double sullo stack -> `call` con firma
                // int16/int64 -> InvalidProgram. Caso reale: i bridge MVC generati dal Linker
                // (Linker.Mvc) invocano l'azione con `VAL(Params["id"])` (double) verso
                // `Get(id AS LONG)` (int64): su IL mancava il conv.i8 (PCode/VM-C/C99 lo
                // facevano implicitamente). Se l'argomento è già un C* del Linker, non
                // ri-coercere (doppia conversione -> stack mismatch, come in EmitFunctionCall).
                var mParamType = NormalizeInferredType(methodDef.Parameters[i].DataType);
                if (args[i] is not (LambdaExpr or FunctionReferenceExpr)
                    && IlConversion.IsNumeric(mParamType)
                    && IlConversion.IsNumeric(NormalizeInferredType(InferType(args[i])))
                    && args[i] is not BuiltinFunctionCallExpr { Name.Kind: TokenKind.CInt or TokenKind.CByte or TokenKind.CLng or TokenKind.CDbl or TokenKind.CSng or TokenKind.CCur })
                {
                    var mArgType = NormalizeInferredType(InferType(args[i]));
                    IlConversion.EmitConvert(_il, _types, mArgType,
                        DataTypeMapper.Map(mArgType, _types, _classTypes),
                        DataTypeMapper.Map(mParamType, _types, _classTypes));
                }
            }
        }

        // forceNonVirtual (BASE.metodo): `call` diretto sul metodo della base — callvirt
        // farebbe il dispatch VIRTUALE risolvendo all'override (ricorsione infinita su
        // BASE.StampaInfo() da un override che chiama la base).
        var isVirtualDispatch = !forceNonVirtual && !isStatic
            && (methodDef.Flags.HasFlag(SymbolFlags.IsVirtual) || methodDef.Flags.HasFlag(SymbolFlags.IsOverride) || methodDef.Parent is InterfaceDefinition);
        _il.Emit(isVirtualDispatch ? OpCodes.Callvirt : OpCodes.Call, methodBuilder);
        _byRefBagSlotFixes = prevBagSlotFixes;
        EmitByRefBagSlotWriteBacks(bagSlotFixes, methodBuilder.ReturnType);
    }

    /// <summary>
    /// "Istanza.Metodo(args)" quando l'istanza ha tipo dichiarato INTERFACCIA: il Linker
    /// non riscrive questa forma in un vero MethodCallExpr (gap scoperto testando questa
    /// feature — stessa famiglia di quello già aggirato per PropertyAccessExpr/campi
    /// STATIC), resta un IndexedAccessExpr generico (Base=FieldAccessExpr "Istanza.Metodo",
    /// Indices=argomenti — la stessa forma sintattica userebbe un'indicizzazione array).
    /// Risolve il metodo per NOME nella classe o interfaccia dichiarata dall'istanza.
    /// </summary>
    private void EmitIndexedAccess(IndexedAccessExpr expr)
    {
        if (TryEmitNativeCollectionIndexedAccess(expr)) return;

        // Globali dynamic-scope per NOME (FORM/QUERY delle pagine BSP — keyword non
        // dichiarabili con DIM): il linker assegna DynamicScopeSymbolId (-3) e
        // EmitLoadVariable(-3) produrrebbe un local null. L'accesso a indice risolve il
        // valore dal bag dynamic-scope per NOME (iniettato dall'host ServeBspPage).
        if (expr.Base is VariableExpr dynBase && dynBase.SymbolId < 0)
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: accesso a indice dynamic-scope con più indici non supportato.");
            var dynIndexType = NormalizeInferredType(InferType(expr.Indices[0]));
            _il.Emit(OpCodes.Ldstr, dynBase.NameCleaned.Lexeme);
            if (dynIndexType is StringType)
            {
                EmitStringArg(expr.Indices[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicScopeIndexGet), typeof(string), typeof(string)));
            }
            else
            {
                EmitInt32Argument(expr.Indices[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicScopeIndexGetNum), typeof(string), typeof(int)));
            }
            return;
        }

        // j("nome") su un valore JSON (o oggetto dinamico object): accesso a indice
        // dinamico via JsonIndexGet — stesso pattern del campo dinamico JsonFieldGet.
        // Altrimenti riscadeva in "array non DIMensionato" (l'array statico non esiste).
        // NB: "obj.Metodo(args)" (Base = FieldAccessExpr su variabile AnyType — es.
        // PricesCopy.Remove("Apple") con PricesCopy = d.ToDict() → AnyType) NON deve
        // cadere qui: è una chiamata a metodo su collection boxed, gestita dal dispatch
        // runtime CollectionInvoke sotto, non un accesso a indice JSON.
        var isDynamicMethodCallOnVar = expr.Base is FieldAccessExpr faeDyn
            && faeDyn.Structure is VariableExpr ivDyn
            && NormalizeInferredType(InferType(ivDyn)) is AnyType;
        if (!isDynamicMethodCallOnVar
            && NormalizeInferredType(InferType(expr.Base)) is Basic.JsonType or AnyType
            or HttpRequestType or HttpResponseType)
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException($"Backend IL: accesso a indice dinamico con {expr.Indices.Length} indici non supportato (serve 1).");
            // L'indice può essere una CHIAVE STRINGA (accesso a dizionario per nome, es.
            // req.Params["name"], gPackages[name] con gPackages DICT<STRING,...>/JSON) o un
            // NUMERO (array-style, es. jsonArr[0]) — prima si assumeva SEMPRE numerico
            // (EmitInt32Argument + JsonIndexGet(object,int)): con una chiave stringa,
            // IlConversion.EmitConvert tentava una conversione STRING→INT32 priva di una
            // sequenza IL valida, producendo un programma non verificabile
            // (InvalidProgramException al primo utilizzo REALE — scoperto collegando
            // HTTP ROUTE a un server reale: req.Params["name"] su una route con placeholder
            // {name} è esattamente questo caso, mai esercitato prima perché HTTP ROUTE non
            // riceveva mai traffico). Le chiavi stringa vanno invece a JsonFieldGet(object,
            // string) — stesso metodo già usato per l'accesso a campo dinamico via punto,
            // dizionario/reflection case-insensitive, coerente con l'uso a punto.
            var indexType = NormalizeInferredType(InferType(expr.Indices[0]));
            EmitExpression(expr.Base);
            var baseClrType = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Base)), _types, _classTypes);
            if (baseClrType.IsValueType) _il.Emit(OpCodes.Box, baseClrType);
            if (indexType is StringType)
            {
                EmitStringArg(expr.Indices[0]);
                _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
            }
            else if (IsDynamicJsonKey(expr.Indices[0]))
            {
                // Chiave DINAMICA (variabile `object`, es. la variabile di un FOR EACH su JSON.KEYS): stringa -> per NOME, altrimenti indice numerico.
                // Prima si assumeva numerico: ToDouble("aa") -> FormatException e HandleApiPackages del registry su IL rispondeva vuoto.
                EmitExpression(expr.Indices[0]);
                var dynIdxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Indices[0])), _types, _classTypes);
                if (dynIdxClr.IsValueType) _il.Emit(OpCodes.Box, dynIdxClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonIndexGetDynamic), typeof(object), typeof(object)));
            }
            else
            {
                EmitInt32Argument(expr.Indices[0]);
                _il.Emit(OpCodes.Call, IlJsonIndexGetMethod);
            }
            return;
        }

        // Array TOP-LEVEL (MatrixNested(0, 0) — il parser produce IndexedAccessExpr per la
        // forma (i, j), non ArrayAccessExpr): indicizzazione per symbol (porting GM 55eb83a2).
        if (expr.Base is VariableExpr veIdx && _arrays.TryGetValue(veIdx.SymbolId, out var idxArrInfo))
        {
            EmitLoadArrayRefAndIndices(idxArrInfo, expr.Indices);
            if (idxArrInfo.Rank == 1) EmitArrayElementLoad(idxArrInfo.ElementDataType);
            else if (idxArrInfo.ClrArrayType.IsArray && idxArrInfo.ClrArrayType.GetElementType()!.IsArray) EmitArrayElementLoad(idxArrInfo.ElementDataType);
            else _il.Emit(OpCodes.Call, idxArrInfo.ClrArrayType.GetMethod("Get")!);
            return;
        }

        // Base ESPRESSIONE di tipo MATRIX VALUE (es. `lu.U(0,0)` — campo di una
        // decomposizione riscritto in LINALG.FACTOR, o `MATRIX.TRANSPOSE(m)(i,j)`):
        // valuta il valore matrice e legge l'elemento.
        if (expr.Indices.Length == 2
            && NormalizeInferredType(InferType(expr.Base)) is MatrixValueType
            && expr.Base is not VariableExpr)
        {
            EmitExpression(expr.Base);
            EmitInt64Argument(expr.Indices[0]);
            EmitInt64Argument(expr.Indices[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueGetAt)));
            return;
        }

        // "Istanza.CampoArray(i)" in lettura riscade nella STESSA forma sintattica
        // generica di "Istanza.Metodo(args)" (stesso gap del Linker, vedi sopra) — va
        // controllato PRIMA della risoluzione come metodo, altrimenti un campo array
        // verrebbe scambiato per una chiamata a metodo inesistente.
        if (TryResolveIndexedFieldArray(expr.Base, out var loadInstance, out var fieldArrayInfo))
        {
            /* Campo MATRICE rank-2 (MatrixValue): accesso elemento m(r,c) diretto. */
            if (fieldArrayInfo.ClrArrayType == typeof(MatrixValue))
            {
                if (expr.Indices.Length != 2)
                    throw new NotSupportedException("Backend IL: un campo matrice richiede 2 indici.");
                loadInstance();
                _il.Emit(OpCodes.Ldfld, fieldArrayInfo.Field);
                EmitInt64Argument(expr.Indices[0]);
                EmitInt64Argument(expr.Indices[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueGetAt)));
                return;
            }
            EmitLoadArrayRefAndIndices(() => { loadInstance(); _il.Emit(OpCodes.Ldfld, fieldArrayInfo.Field); }, fieldArrayInfo.Rank, fieldArrayInfo.LowerBounds, expr.Indices, fieldArrayInfo.ClrArrayType);
            if (fieldArrayInfo.Rank == 1)
            {
                EmitArrayElementLoad(fieldArrayInfo.ElementDataType);
            }
            else
            {
                if (fieldArrayInfo.ClrArrayType.IsArray && fieldArrayInfo.ClrArrayType.GetElementType()!.IsArray)
                    EmitArrayElementLoad(fieldArrayInfo.ElementDataType);
                else
                    _il.Emit(OpCodes.Call, fieldArrayInfo.ClrArrayType.GetMethod("Get")!);
            }
            return;
        }

        // "Istanza.Metodo[args]" su un oggetto AnyType (es. PricesCopy.Remove["Apple"] con
        // PricesCopy = d.ToDict() → AnyType): dispatch RUNTIME via CollectionInvoke.
        // MyRB["c"] in LETTURA su oggetto dinamico (RedBlack/Tree/... boxed, Base =
        // VariableExpr): dispatch runtime CollectionIndexGet.
        if (expr.Base is VariableExpr dynVar2)
        {
            var dynT2 = NormalizeInferredType(InferType(dynVar2));
            if (dynT2 is RedBlackTreeType or TreeType or AVLTreeType or BinaryIndexedTreeType
                or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType
                or BinaryHeapType or BagType or CacheType or TimelineType or RingBufferType or AnyType)
            {
                if (expr.Indices.Length != 1)
                    throw new NotSupportedException("Backend IL: indicizzazione dinamica richiede 1 indice.");
                EmitExpression(dynVar2);
                EmitExpression(expr.Indices[0]);
                var idxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Indices[0])), _types, _classTypes);
                if (idxClr.IsValueType) _il.Emit(OpCodes.Box, idxClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexGet), typeof(object), typeof(object)));
                return;
            }
        }

        if (expr.Base is FieldAccessExpr memberAccess2 && memberAccess2.Structure is VariableExpr iv2
            && NormalizeInferredType(InferType(iv2)) is AnyType)
        {
            EmitExpression(iv2);
            _il.Emit(OpCodes.Ldstr, memberAccess2.Field.Lexeme);
            _il.Emit(OpCodes.Ldc_I4, expr.Indices.Length);
            _il.Emit(OpCodes.Newarr, _types.Object);
            for (var i = 0; i < expr.Indices.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitExpression(expr.Indices[i]);
                var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Indices[i])), _types, _classTypes);
                if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvoke), typeof(object), typeof(string), typeof(object[])));
            return;
        }

        // "Campo[index]" su PortfolioResult (Ottimo.Weights(0)): il campo è un double[]
        // dentro il dictionary — field get (JsonFieldGet), cast, ldelem.
        if (expr.Base is FieldAccessExpr fieldAcc && NormalizeInferredType(InferType(fieldAcc.Structure)) is PortfolioResultType)
        {
            EmitExpression(fieldAcc.Structure);
            var sClr = DataTypeMapper.Map(NormalizeInferredType(InferType(fieldAcc.Structure)), _types, _classTypes);
            if (sClr.IsValueType) _il.Emit(OpCodes.Box, sClr);
            _il.Emit(OpCodes.Ldstr, fieldAcc.Field.Lexeme);
            _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
            _il.Emit(OpCodes.Castclass, _types.Double.MakeArrayType());
            EmitInt32Arg(expr.Indices[0]);
            _il.Emit(OpCodes.Ldelem_R8);
            return;
        }

        // Indice su ESPRESSIONE array (es. FROMHEX(hex$)(0) — il Base è una chiamata che
        // ritorna un array): EmitExpression(Base) + indice + ldelem dell'elemento.
        if (expr.Indices.Length == 1
            && NormalizeInferredType(InferType(expr.Base)) is ArrayType idxArrType)
        {
            EmitExpression(expr.Base);
            EmitInt32Arg(expr.Indices[0]);
            _il.Emit(OpCodes.Ldelem, DataTypeMapper.Map(idxArrType.ElementType, _types, _classTypes));
            return;
        }

        var methodDef = ResolveIndexedAccessMethodDefinition(expr, out var instanceExpr);
        EmitMethodCallCore(instanceExpr, methodDef, expr.Indices);
    }

    /// <summary>
    /// "Istanza.CampoArray(i) = valore" — l'assegnazione riscade in un IndexedTarget
    /// generico (stessa famiglia di IndexedAccessExpr in lettura, vedi EmitIndexedAccess):
    /// nessun'altra forma di IndexedTarget è supportata (un array top-level usa già
    /// ArrayElementTarget, risolto per SymbolId — vedi EmitArrayElementAssignment).
    /// </summary>
    private void EmitIndexedTargetAssignment(IndexedTarget target, Expr value)
    {
        if (TryEmitNativeCollectionIndexedAssignment(target, value)) return;

        // Array TOP-LEVEL (MatrixNested(0,0) = 1 — il parser produce IndexedTarget per la
        // forma (i, j), non ArrayElementTarget): indicizzazione per symbol (porting GM 55eb83a2).
        if (target.Base is VariableExpr veTop && _arrays.TryGetValue(veTop.SymbolId, out var topArrInfo))
        {
            EmitLoadArrayRefAndIndices(topArrInfo, target.Indices);
            EmitExpression(value);
            if (topArrInfo.Rank == 1)
            {
                // Coerce al tipo CLR dell'elemento (come EmitArrayElementStore per ArrayElementTarget):
                // senza, `DIM D(4) AS LONG : D[0] = 300` salvava i BIT del double (stelem.i8 su un
                // double) e la lettura D(0) restituiva 4643985272004935680.
                EmitCoerceToElementClr(topArrInfo.ElementDataType);
                EmitArrayElementStore(topArrInfo.ElementDataType);
            }
            else if (topArrInfo.ClrArrayType.IsArray && topArrInfo.ClrArrayType.GetElementType()!.IsArray) EmitArrayElementStore(topArrInfo.ElementDataType);
            else _il.Emit(OpCodes.Call, topArrInfo.ClrArrayType.GetMethod("Set")!);
            return;
        }

        // MyRB["c"] = ... su un oggetto dinamico (RedBlack/Tree/AVL/... boxed, Base è un
        // VariableExpr): dispatch runtime CollectionIndexSet (chiave boxed, valore boxed).
        if (target.Base is VariableExpr dVar)
        {
            var dType = NormalizeInferredType(InferType(dVar));
            if (dType is RedBlackTreeType or TreeType or AVLTreeType or BinaryIndexedTreeType
                or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType
                or BinaryHeapType or BagType or CacheType or TimelineType or RingBufferType)
            {
                if (target.Indices.Length != 1)
                    throw new NotSupportedException("Backend IL: indicizzazione dinamica richiede 1 indice.");
                EmitExpression(dVar);
                EmitExpression(target.Indices[0]);
                var idxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(target.Indices[0])), _types, _classTypes);
                if (idxClr.IsValueType) _il.Emit(OpCodes.Box, idxClr);
                EmitExpression(value);
                var valClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
                if (valClr.IsValueType) _il.Emit(OpCodes.Box, valClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexSet), typeof(object), typeof(object), typeof(object)));
                return;
            }
        }

        // JSON / AnyType / HTTP: pkgData["name"] = value → JsonFieldSet (stesso pattern
        // del campo dinamico in EmitFieldAssignment). Serve 1 indice (chiave stringa).
        // Base = variabile, campo dinamico (`claim.collaborators[k] = v`) o elemento (`gUsers[u]["k"] = v`) di un JSON: l'espressione base e' a sua
        // volta un JSON/ANY e viene letta con JsonFieldGet/JsonIndexGet da EmitExpression(target.Base). Prima solo VariableExpr: il resto cadeva in "solo campi array di classe".
        if (target.Base is VariableExpr or FieldAccessExpr or IndexedAccessExpr
            && NormalizeInferredType(InferType(target.Base)) is Basic.JsonType or AnyType or HttpRequestType or HttpResponseType
                or HttpOptionsType or HttpHeadersType or HttpConfigType or HttpPostedDataType
                or SqlResultSetType or WebSocketType or FetchResultType)
        {
            if (target.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: assegnazione a indice JSON richiede 1 indice.");
            EmitExpression(target.Base);
            var jsonBaseClr = DataTypeMapper.Map(NormalizeInferredType(InferType(target.Base)), _types, _classTypes);
            if (jsonBaseClr.IsValueType) _il.Emit(OpCodes.Box, jsonBaseClr);
            // Chiave: se l'indice è NUMERICO (j[0] = v) va scritto come elemento di lista
            // (JsonIndexSet); solo le chiavi stringa sono campi dict (JsonFieldSet).
            var jsonIdxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(target.Indices[0])), _types, _classTypes);
            EmitExpression(target.Indices[0]);
            if (jsonIdxClr.IsValueType) _il.Emit(OpCodes.Box, jsonIdxClr);
            EmitExpression(value);
            var jsonValClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
            if (jsonValClr.IsValueType) _il.Emit(OpCodes.Box, jsonValClr);
            if (jsonIdxClr == typeof(string))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonFieldSet), typeof(object), typeof(string), typeof(object)));
            else
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonIndexSet), typeof(object), typeof(object), typeof(object)));
            return;
        }

        if (!TryResolveIndexedFieldArray(target.Base, out var loadInstance, out var fieldArrayInfo))
            throw new NotSupportedException($"Backend IL: assegnazione a target indicizzato '{target.Base.GetType().Name}' non ancora supportata (solo campi array di classe).");

        /* Campo MATRICE rank-2 (MatrixValue): scrittura elemento m(r,c) = v. */
        if (fieldArrayInfo.ClrArrayType == typeof(MatrixValue))
        {
            if (target.Indices.Length != 2)
                throw new NotSupportedException("Backend IL: un campo matrice richiede 2 indici.");
            loadInstance();
            _il.Emit(OpCodes.Ldfld, fieldArrayInfo.Field);
            EmitInt64Argument(target.Indices[0]);
            EmitInt64Argument(target.Indices[1]);
            EmitDoubleArgument(value);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueSetAt)));
            _il.Emit(OpCodes.Pop);
            return;
        }

        EmitLoadArrayRefAndIndices(() => { loadInstance(); _il.Emit(OpCodes.Ldfld, fieldArrayInfo.Field); }, fieldArrayInfo.Rank, fieldArrayInfo.LowerBounds, target.Indices, fieldArrayInfo.ClrArrayType);
        EmitExpression(value);
        // Coerce al tipo CLR dell'elemento PRIMA dello store (come il path array TOP-LEVEL,
        // riga ~387): senza, `m(i,j) = ...` su un campo-array INTEGER multidim pushava un
        // DOUBLE dove il metodo CLR Set(int32,int32,int16) — o stelem.i2 — attende un int16
        // → programma IL non verificabile (InvalidProgramException).
        EmitCoerceToElementClr(fieldArrayInfo.ElementDataType);
        if (fieldArrayInfo.Rank == 1)
        {
            EmitArrayElementStore(fieldArrayInfo.ElementDataType);
        }
        else
        {
            _il.Emit(OpCodes.Call, fieldArrayInfo.ClrArrayType.GetMethod("Set")!);
        }
    }

    private bool TryEmitNativeCollectionIndexedAccess(IndexedAccessExpr expr)
    {
        if (expr.Indices.Length != 1 || !TryGetNativeCollectionType(InferType(expr.Base), out var collectionType)) return false;
        // Gli ARRAY reali (es. double[] / ARRAY OF DOUBLE) NON hanno l'indexer Item:
        // la loro indicizzazione è gestita dal path array standard (EmitIndexedAccess).
        if (NormalizeInferredType(InferType(expr.Base)) is ArrayType) return false;
        // SAFE (d["x"]? / nums[5]?): TryGetValue → null se assente/fuori range (invece di
        // KeyNotFoundException/IndexOutOfRange) — il valore è boxed object.
        if (expr.IsSafe)
        {
            EmitExpression(expr.Base);
            var baseClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Base)), _types, _classTypes);
            if (baseClr.IsValueType) _il.Emit(OpCodes.Box, baseClr);
            EmitExpression(expr.Indices[0]);
            var idxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Indices[0])), _types, _classTypes);
            if (idxClr.IsValueType) _il.Emit(OpCodes.Box, idxClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DictTryGetValue), typeof(object), typeof(object)));
            // Il risultato è object (null se assente) ma il tipo INFERITO è l'elemento
            // (es. int16 da DICT<STRING,INTEGER>): adatta al tipo statico — numerico →
            // ToDouble, stringa → cast, altrimenti resta object (parità VM: null-safe).
            var inferredSafe = NormalizeInferredType(InferType(expr));
            if (IlConversion.IsNumeric(inferredSafe))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
            else if (inferredSafe is StringType)
            {
                _il.Emit(OpCodes.Castclass, _types.String);
            }
            return true;
        }
        var listType = NormalizeInferredType(InferType(expr.Base)) as ListType;
        var listElement = listType?.ElementType is UnresolvedQualifiedType unresolved
            ? _program.GetAllUserDefinedTypes().FirstOrDefault(u =>
                u.OriginalName.Lexeme.Equals(unresolved.QualifiedName.FullName, StringComparison.OrdinalIgnoreCase))
            : listType?.ElementType;
        if (listElement is UserDefinedType udt)
        {
            // Il tipo inferito del campo LIST OF UDT può conservare un
            // UnresolvedQualifiedType: Map lo trasformerebbe in List<object>, mentre
            // il campo CLR reale è List<Udt>. Evita il get_Item del tipo sbagliato.
            EmitExpression(expr.Base);
            _il.Emit(OpCodes.Castclass, typeof(System.Collections.IList));
            EmitInt32Argument(expr.Indices[0]);
            _il.Emit(OpCodes.Callvirt, typeof(System.Collections.IList).GetProperty("Item")!.GetGetMethod()!);
            _il.Emit(OpCodes.Unbox_Any, DataTypeMapper.Map(udt, _types, _classTypes));
            return true;
        }
        var indexer = collectionType.GetProperty("Item")
            ?? throw new NotSupportedException($"Backend IL: collection '{InferType(expr.Base)}' non è indicizzabile.");
        var getter = indexer.GetGetMethod()
            ?? throw new NotSupportedException($"Backend IL: indexer in lettura non disponibile per '{InferType(expr.Base)}'.");
        EmitExpression(expr.Base);
        EmitCollectionIndexerArgument(expr.Indices[0], indexer.GetIndexParameters()[0].ParameterType);
        _il.Emit(OpCodes.Callvirt, getter);
        return true;
    }

    private bool TryEmitNativeCollectionIndexedAssignment(IndexedTarget target, Expr value)
    {
        if (target.Indices.Length != 1 || !TryGetNativeCollectionType(InferType(target.Base), out var collectionType)) return false;
        if (NormalizeInferredType(InferType(target.Base)) is ArrayType) return false;
        var indexer = collectionType.GetProperty("Item")
            ?? throw new NotSupportedException($"Backend IL: collection '{InferType(target.Base)}' non è indicizzabile.");
        var setter = indexer.GetSetMethod()
            ?? throw new NotSupportedException($"Backend IL: indexer in scrittura non disponibile per '{InferType(target.Base)}'.");
        EmitExpression(target.Base);
        EmitCollectionIndexerArgument(target.Indices[0], indexer.GetIndexParameters()[0].ParameterType);
        EmitExpression(value);
        var sourceType = NormalizeInferredType(InferType(value));
        IlConversion.EmitConvert(_il, _types, sourceType, DataTypeMapper.Map(sourceType, _types, _classTypes), indexer.PropertyType);
        _il.Emit(OpCodes.Callvirt, setter);
        return true;
    }

    private void EmitCollectionIndexerArgument(Expr index, Type parameterType)
    {
        EmitExpression(index);
        var sourceType = NormalizeInferredType(InferType(index));
        IlConversion.EmitConvert(_il, _types, sourceType, DataTypeMapper.Map(sourceType, _types, _classTypes), parameterType);
    }

    /// <summary>
    /// Riconosce "Istanza.CampoArray" (Base di un IndexedAccessExpr/IndexedTarget) come
    /// riferimento a un campo di classe di tipo ARRAY, distinguendolo da "Istanza.Metodo"
    /// (che risolve invece a un metodo, vedi ResolveIndexedAccessMethodDefinition) — stessa
    /// forma sintattica per entrambi, il Linker non li distingue (vedi nota su
    /// EmitIndexedAccess). SELF.CampoArray(i) rientra qui: SELF è comunque un VariableExpr
    /// (risolto per nome, vedi TryResolveSelfArgIndex), stesso identico shape di
    /// un'istanza qualunque.
    /// </summary>
    private bool TryResolveIndexedFieldArray(Expr baseExpr, out Action loadInstance, out FieldArrayInfo fieldArrayInfo)
    {
        loadInstance = () => { };
        fieldArrayInfo = null!;
        // `v(i).CampoArray(k)` (S3b, 2026-11-06): l'istanza proprietaria del campo-array è
        // un ELEMENTO di un array di UDT (value type). Si carica l'elemento in un local
        // temporaneo e si passa il suo INDIRIZZO (Ldloca) a Ldfld, come per un'istanza
        // locale. Prima questa forma cadeva in "assegnazione a target indicizzato
        // 'FieldAccessExpr' non ancora supportata" (in scrittura) e in
        // "espressione 'IndexedAccessExpr' non ancora supportata" (in lettura).
        if (baseExpr is FieldAccessExpr elemMember
            && elemMember.Structure is IndexedAccessExpr or ArrayAccessExpr)
        {
            var elemType = NormalizeInferredType(InferType(elemMember.Structure));
            if (elemType is UserDefinedType elemUdt)
            {
                var elemInfo = FindFieldArrayInHierarchy(elemUdt.SymbolId, elemMember.Field.Lexeme);
                if (elemInfo is not null)
                {
                    var elemClr = DataTypeMapper.Map(elemUdt, _types, _classTypes);
                    EmitExpression(elemMember.Structure);
                    var tmp = _il.DeclareLocal(elemClr);
                    _il.Emit(OpCodes.Stloc, tmp);
                    loadInstance = () => _il.Emit(OpCodes.Ldloca, tmp);
                    fieldArrayInfo = elemInfo;
                    return true;
                }
            }
            return false;
        }
        if (baseExpr is not FieldAccessExpr memberAccess || memberAccess.Structure is not VariableExpr instanceVar)
            return false;

        // Istanza DINAMICA (AnyType — es. PricesCopy = d.ToDict()): nessun campo array di
        // classe da risolvere; il dispatch runtime lo gestisce. ResolveOwnerClassSymbolId
        // lancerebbe "impossibile risolvere il tipo".
        if (NormalizeInferredType(InferType(instanceVar)) is AnyType or PortfolioResultType)
            return false;

        var ownerClassSymbolId = ResolveOwnerClassSymbolId(instanceVar);
        var info = ownerClassSymbolId is int classSymbolId ? FindFieldArrayInHierarchy(classSymbolId, memberAccess.Field.Lexeme) : null;
        if (info is null)
            return false;

        loadInstance = () => EmitExpression(memberAccess.Structure);
        fieldArrayInfo = info;
        return true;
    }

    /// <summary>
    /// Risolve il SymbolId della classe proprietaria di <paramref name="instanceVar"/>. Per
    /// SELF/ME usa <see cref="_currentClassSymbolId"/> (impostato dal Pass 2 dalla lista
    /// AUTOREVOLE di classi), MAI <see cref="ResolveMemberOwner"/>: quest'ultimo legge
    /// symbol.DataType, che per SELF/ME è uno snapshot di ClassType/ClassDefinition
    /// catturato PRIMA che RegisterFieldMember completi la risoluzione dei SymbolId dei
    /// campi (bug del Linker scoperto testando i campi ARRAY di classe) — persino il
    /// SymbolId della classe stessa risulta ancora -1 in quello snapshot, non solo quello
    /// dei campi. Per ogni altra istanza (una variabile normale come "V") ResolveMemberOwner
    /// resta affidabile.
    /// </summary>
    private int? ResolveOwnerClassSymbolId(VariableExpr instanceVar)
    {
        if (TryResolveSelfArgIndex(instanceVar.SymbolId, out _))
            return _currentClassSymbolId;
        // Un UDT (TYPE...END TYPE) non è mai avvolto in ClassType/ClassDefinition come una
        // CLASS — è esso stesso il DataType (vedi Ast/DataTypes.UdtClasses.cs) — quindi
        // ResolveMemberOwner (che conosce solo ClassType/ClassDefinition/InterfaceType/
        // InterfaceDefinition) lancerebbe esplicito per una variabile di tipo UDT.
        // Controllato PRIMA di ResolveMemberOwner, non dentro: quest'ultimo è condiviso da
        // altri consumer (risoluzione di metodi/campi di CLASS/INTERFACE) che non devono
        // sapere nulla dei UDT — l'unico uso di questo metodo che riguarda i UDT è
        // TryResolveIndexedFieldArray (campi array), l'unico membro che un UDT può avere
        // qui risolto per nome tramite _classFieldArrays.
        if (_program.Symbols.GetSymbol(instanceVar.SymbolId).DataType is UserDefinedType udt)
            return udt.SymbolId;
        // JSON/ANY sono dinamici: nessuna classe proprietaria (`claim.collaborators[k] = v` con claim AS JSON non e' un
        // campo array di CLASS). Prima ResolveMemberOwner lanciava "impossibile risolvere il tipo per il simbolo"
        // e il registry (PackageServer.bas) non compilava su IL; null = il chiamante ricade sul percorso dinamico.
        if (NormalizeInferredType(_program.Symbols.GetSymbol(instanceVar.SymbolId).DataType) is Basic.JsonType or AnyType)
            return null;
        var (classDef, _) = ResolveMemberOwner(instanceVar.SymbolId);
        return classDef?.SymbolId;
    }

    /// <summary>
    /// Risale la gerarchia di ereditarietà per SOLO SymbolId (mai <c>ClassDefinition
    /// .Members</c>/<c>.ResolvedBaseClass</c> direttamente — vedi nota su
    /// <see cref="ResolveOwnerClassSymbolId"/> sul perché), cercando in
    /// <see cref="_classFieldArrays"/> un campo array con questo nome appartenente a
    /// QUESTA classe o a una delle sue basi. La catena di ereditarietà è ricavata da
    /// <see cref="_classTypes"/> (il TypeBuilder.BaseType reale, già affidabile — mai da
    /// una ClassDefinition potenzialmente stale).
    /// </summary>
    private FieldArrayInfo? FindFieldArrayInHierarchy(int classSymbolId, string fieldName)
    {
        for (int? current = classSymbolId; current is int id; current = GetBaseClassSymbolId(id))
        {
            var match = _classFieldArrays.Values.FirstOrDefault(info =>
                info.ClassSymbolId == id && info.FieldName.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
            if (match is not null) return match;
        }
        return null;
    }

    private int? GetBaseClassSymbolId(int classSymbolId)
    {
        var baseType = _classTypes[classSymbolId].BaseType;
        if (baseType is null) return null;
        foreach (var (id, typeBuilder) in _classTypes)
            if (ReferenceEquals(typeBuilder, baseType))
                return id;
        return null;
    }

    /// <summary>
    /// Come <see cref="ResolveOwnerClassSymbolId"/> ma per un'espressione generica (non
    /// necessariamente una variabile) usata come operando sinistro di un OPERATOR
    /// sovraccaricato — per SELF/ME usa <see cref="_currentClassSymbolId"/> (mai
    /// InferType/symbol.DataType, stesso identico bug del Linker sullo snapshot stale di
    /// SELF già documentato per i campi ARRAY di classe); per ogni altra espressione
    /// restituisce null (il chiamante ricade su InferType, affidabile in quel caso).
    /// </summary>
    private int? ResolveClassSymbolIdForOperatorLookup(Expr expr) =>
        expr is VariableExpr v && TryResolveSelfArgIndex(v.SymbolId, out _) ? _currentClassSymbolId : null;

    /// <summary>
    /// Risale la gerarchia di ereditarietà per SOLO SymbolId (vedi GetBaseClassSymbolId),
    /// cercando in <see cref="_classOperators"/> un OPERATOR con questo TokenKind e questa
    /// ARITÀ (1 = unario, 2 = binario — necessaria per disambiguare, es. TokenKind.Minus
    /// usato SIA per la sottrazione binaria SIA per la negazione unaria: la stessa classe
    /// potrebbe sovraccaricare entrambe) appartenente a QUESTA classe o a una delle sue basi.
    /// </summary>
    private MethodBuilder? FindOperatorInHierarchy(int classSymbolId, TokenKind opKind, int arity, DataType? returnType = null)
    {
        for (int? current = classSymbolId; current is int id; current = GetBaseClassSymbolId(id))
        {
            if (returnType is not null && _classOperators.TryGetValue(
                    (id, opKind, arity, OperatorReturnKey(returnType)), out var exactMethod))
                return exactMethod;
            var match = _classOperators.FirstOrDefault(entry =>
                entry.Key.OwnerSymbolId == id && entry.Key.OpKind == opKind && entry.Key.Arity == arity);
            if (match.Value is not null) return match.Value;
        }
        return null;
    }

    private MethodBuilder? FindOperatorInHierarchy(int ownerSymbolId, TokenKind opKind, int arity, string returnType)
    {
        for (int? current = ownerSymbolId; current is int id; current = GetBaseClassSymbolId(id))
            if (_classOperators.TryGetValue((id, opKind, arity, returnType), out var method))
                return method;
        return null;
    }

    private static string OperatorReturnKey(DataType returnType) =>
        NormalizeInferredType(returnType).ToString()?.ToUpperInvariant().Replace(" ", string.Empty) ?? returnType.GetType().Name;

    private FunctionDefinition ResolveIndexedAccessMethodDefinition(IndexedAccessExpr expr, out Expr instanceExpr)
    {
        if (expr.Base is not FieldAccessExpr memberAccess || memberAccess.Structure is not VariableExpr instanceVar)
            throw new NotSupportedIlExpressionException(expr);

        instanceExpr = memberAccess.Structure;
        var (classDef, ifaceDef) = ResolveMemberOwner(instanceVar.SymbolId);
        var methodDef = classDef is not null
            ? FindMethodDefinitionInHierarchy(classDef, memberAccess.Field.Lexeme)
            : ifaceDef is not null ? FindMethodDefinitionInInterface(ifaceDef, memberAccess.Field.Lexeme) : null;

        return methodDef ?? throw new NotSupportedException($"Backend IL: metodo '{memberAccess.Field.Lexeme}' non trovato o non ancora supportato.");
    }

    /// <summary>
    /// Risolve la classe O l'interfaccia (mai entrambe) a cui appartiene il simbolo —
    /// generalizza ResolveClassDefinitionFromSymbol per coprire anche le interfacce,
    /// necessario per EmitIndexedAccess (un'istanza dichiarata AS UnaInterfaccia).
    /// </summary>
    private (ClassDefinition? ClassDef, InterfaceDefinition? InterfaceDef) ResolveMemberOwner(int symbolId)
    {
        var symbol = _program.Symbols.GetSymbol(symbolId);
        if (symbol.DeclarationNode is ClassDefinition classFromDecl) return (classFromDecl, null);
        if (symbol.DeclarationNode is InterfaceDefinition ifaceFromDecl) return (null, ifaceFromDecl);
        if (symbol.DataType is ClassDefinition classDirect) return (classDirect, null);
        if (symbol.DataType is ClassType classType) return (classType.Definition, null);
        if (symbol.DataType is InterfaceDefinition ifaceDirect) return (null, ifaceDirect);
        if (symbol.DataType is InterfaceType ifaceType) return (null, ifaceType.Definition);
        throw new NotSupportedException($"Backend IL: impossibile risolvere il tipo per il simbolo '{symbol.OriginalName}'.");
    }

    private static FunctionDefinition? FindMethodDefinitionInHierarchy(ClassDefinition classDef, string methodName)
    {
        for (var current = classDef; current is not null; current = current.ResolvedBaseClass)
            foreach (var member in current.Members)
                if (member is MethodMember { Procedure: FunctionDefinition fd } && fd.OriginalName.Lexeme.Equals(methodName, StringComparison.OrdinalIgnoreCase))
                    return fd;
        return null;
    }

    private static FunctionDefinition? FindMethodDefinitionInInterface(InterfaceDefinition ifaceDef, string methodName)
    {
        foreach (var member in ifaceDef.Members)
            if (member is MethodMember { Procedure: FunctionDefinition fd } && fd.OriginalName.Lexeme.Equals(methodName, StringComparison.OrdinalIgnoreCase))
                return fd;
        return null;
    }

    /// <summary>
    /// Chiamata a una SUB/FUNCTION utente, sia in contesto espressione (FUNCTION che
    /// restituisce un valore) sia in contesto statement (SUB, tramite ExpressionStmt che
    /// scarta il risultato — vedi EmitExpressionStatement, già gestisce il caso Void).
    /// `call` diretta al MethodBuilder specifico (mai `callvirt`/vtable — vedi TODO
    /// Obsidian, Filone B, "Domande aperte" #5: ogni chiamata è risolta staticamente,
    /// nessun bisogno di dispatch a runtime). Per un parametro BYVAL, gli argomenti sono
    /// già stati coercizzati dal Linker al tipo atteso (stesso pattern di CLNG/CINT sugli
    /// argomenti delle funzioni builtin — vedi EmitNumericConversionCall), quindi
    /// EmitExpression basta. Per un parametro BYREF va passato l'INDIRIZZO
    /// dell'argomento, non il suo valore — vedi EmitAddressOf.
    /// </summary>



    private void EmitQualifiedNameReference(QualifiedNameExpr qne)
    {
        // Referenza a una builtin senza parentesi (es. `ANALYSIS.FFTC <> 0` in AnalysisTests):
        // come EmitUnresolvedCall — il TokenKind del membro, se è una builtin a 0 argomenti
        // la chiama; con parametri emette il default 0 (il P-Code stampa l'indirizzo).
        var member = qne.Parts[^1];
        if (member.Kind is TokenKind.Fft or TokenKind.Ifft or TokenKind.FftComplex or TokenKind.IfftComplex)
        {
            _il.Emit(OpCodes.Ldc_R8, 0.0);
            return;
        }
        // Builtin a 0 argomenti come valore (es. `-MATH.PI` in GraphicsWindows): la chiamata
        // senza parentesi è equivalente alla funzione (il P-Code valuta il membro qualificato
        // come chiamata a 0 argomenti).
        if (member.Kind is TokenKind.Pi)
        {
            _il.Emit(OpCodes.Call, IlRuntimePiMethod);
            return;
        }
        // Membro NON-builtin (es. `MATRIX.LINALGINVERSE`, `VECTOR.VECTORCREATE` in
        // AlgebraTests — nomi fittizi/command di test completezza): il P-Code fa
        // LoadVariable(nome) di una variabile inesistente → default 0. Parità: emette 0.0.
        _il.Emit(OpCodes.Ldc_R8, 0.0);
    }
}
