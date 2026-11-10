// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Expressions.Address.cs
// PARTIAL — dominio EXPRESSIONS/ADDRESS: EmitAddressOf, array address/access.
// Migrato da IlProgramCompiler.Expressions.cs (2026-08-30 — split 6.0.1).

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

/// <summary>Partial del transpiler IL: address-of e accesso array.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitAddressOf(Expr expr)
    {
        switch (expr)
        {
            /* Parametro catturato e materializzato in ByRefBox<T> (BoxCapturedParameters):
             * l'indirizzo del VALORE è il campo Value, non il box (passare `ldloca` del box
             * darebbe un ByRefBox<T>& a un callee che si aspetta T&). */
            case VariableExpr v when _locals.TryGetValue(v.SymbolId, out var boxedParamLocal)
                                      && IsByRefBoxFieldType(boxedParamLocal.LocalType):
                _il.Emit(OpCodes.Ldloc, boxedParamLocal);
                _il.Emit(OpCodes.Ldflda, boxedParamLocal.LocalType.GetField("Value")!);
                return;
            case VariableExpr v when _parameters.TryGetValue(v.SymbolId, out var param):
                if (param.IsByRef)
                    _il.Emit(OpCodes.Ldarg, param.ArgIndex); // già un indirizzo (T&): inoltrato as-is (es. un parametro-array forwarded BYREF a una call annidata)
                else
                    _il.Emit(OpCodes.Ldarga, param.ArgIndex); // indirizzo dello slot argomento BYVAL
                return;
            // Un array DIM'd localmente vive SOLO in _arrays, mai in _locals — controllato
            // PRIMA del fallback generico, altrimenti GetOrDeclareLocal ne dichiarerebbe uno
            // NUOVO scorrelato (bug: passare un array locale BYREF a una SUB, es.
            // ModificaArray(Numeri), passerebbe l'indirizzo di un locale sbagliato).
            case VariableExpr v when _arrays.TryGetValue(v.SymbolId, out var arrayInfo):
                /* Array TOP-LEVEL/GLOBALE: il valore vero vive nel bag (ArrayInfo.LoadRef set) e
                 * il method-local è allocato ma MAI scritto (le scritture vanno al bag) → Ldloca
                 * diretto darebbe un Double[]& NULL → NRE nella SUB callee. Materializza prima il
                 * ref reale (dal bag) nel local, così il byref punta all'array condiviso vero. */
                if (arrayInfo.LoadRef != null) { EmitArrayRefFromInfo(arrayInfo); _il.Emit(OpCodes.Stloc, arrayInfo.Local); }
                _il.Emit(OpCodes.Ldloca, arrayInfo.Local);
                return;
            case VariableExpr v:
            {
                // Array GLOBALE/STATIC dichiarato in un'ALTRA istanza del compilatore (il
                // DIM esplicito a livello modulo è emesso dall'istanza dell'entry point):
                // in QUESTA istanza _arrays è vuoto per quel simbolo e il fallback
                // GetOrDeclareLocal dichiarerebbe un local MAI scritto (il valore vive nel
                // bag) → ldloca di un array null → NullReferenceException nel callee al
                // primo accesso. Sintetizza l'ArrayInfo dal bag (stesso meccanismo di
                // GetArrayInfo) e materializza il riferimento reale PRIMA di prenderne
                // l'indirizzo. Riprodotto con `F1(g())` (array globale → parametro `a()`).
                if (TrySynthesizeBagArrayInfo(v.SymbolId) is { } bagArrayInfo)
                {
                    _arrays[v.SymbolId] = bagArrayInfo;
                    EmitArrayRefFromInfo(bagArrayInfo);
                    _il.Emit(OpCodes.Stloc, bagArrayInfo.Local);
                    _il.Emit(OpCodes.Ldloca, bagArrayInfo.Local);
                    return;
                }
                _il.Emit(OpCodes.Ldloca, GetOrDeclareLocal(v.SymbolId));
                return;
            }
            case ArrayAccessExpr a:
                EmitArrayElementAddress(a);
                return;
            /* Array literal come argomento BYREF (2026-11-03): `CALL F({1, 2})` non ha una
             * variabile da indirizzare. Si materializza un array temporaneo (locale) e se ne
             * passa l'indirizzo: il callee puo' leggerlo/modificarlo, le modifiche restano nel
             * temporaneo (nessun chiamante le osserva, e' un letterale). */
            case ArrayLiteralExpr lit:
            {
                var et = lit.Elements.Length == 0
                    ? (DataType)new DoubleType()
                    : NormalizeInferredType(InferType(lit.Elements[0]));
                var clrElem = DataTypeMapper.Map(et, _types, _classTypes);
                EmitArrayLiteralExpression(lit);
                var tmp = _il.DeclareLocal(clrElem.MakeArrayType());
                _il.Emit(OpCodes.Stloc, tmp);
                _il.Emit(OpCodes.Ldloca, tmp);
                return;
            }
            default:
                throw new NotSupportedException(
                    "Backend IL: un argomento passato BYREF deve essere una variabile o un elemento di array semplice, non un'espressione generica.");
        }
    }

    private void EmitArrayElementAddress(ArrayAccessExpr expr)
    {
        var info = GetArrayInfo(expr.SymbolId, expr.Indices.Length);
        EmitLoadArrayRefAndIndices(info, expr.Indices);

        if (info.Rank == 1)
        {
            _il.Emit(OpCodes.Ldelema, DataTypeMapper.Map(info.ElementDataType, _types, _classTypes));
        }
        else if (info.ClrArrayType == typeof(MatrixValue))
        {
            // Indirizzo di un ELEMENTO di matrice a descrittore: non esiste un riferimento
            // stabile (l'elemento vive in uno storage flat con strides). Caso rarissimo
            // (passaggio BYREF di m(i,j)); errore esplicito invece di IL invalido.
            throw new NotSupportedException(
                "Backend IL: indirizzo (BYREF) di un elemento di matrice non supportato — copia l'elemento in una variabile.");
        }
        else if (info.ClrArrayType.IsArray && info.ClrArrayType.GetElementType()!.IsArray)
        {
            // JAGGED (T[][], ...): EmitLoadArrayRefAndIndices ha già fatto ldelem.ref sugli
            // assi intermedi; l'ultimo indice è ancora sullo stack → indirizzo con ldelema.
            // Un SZArray non ha il metodo CLR Address (solo i rettangolari T[,]).
            _il.Emit(OpCodes.Ldelema, DataTypeMapper.Map(info.ElementDataType, _types, _classTypes));
        }
        else
        {
            _il.Emit(OpCodes.Call, info.ClrArrayType.GetMethod("Address")!);
        }
    }

    private void EmitArrayAccess(ArrayAccessExpr expr)
    {
        // Accesso SAFE (arr(9)?): il valore boxed o null se fuori range (ArraySafeIndex).
        if (expr.IsSafe)
        {
            EmitLoadVariable(expr.SymbolId);
            if (_arrays.TryGetValue(expr.SymbolId, out var safeArrInfo)
                && safeArrInfo.ClrArrayType.IsValueType == false)
            {
                // reference array: box non necessario
            }
            else if (_locals.TryGetValue(expr.SymbolId, out var safeLb) && safeLb.LocalType.IsValueType)
                _il.Emit(OpCodes.Box, safeLb.LocalType);
            if (expr.Indices.Length == 0) _il.Emit(OpCodes.Ldc_R8, 0.0);
            else EmitDoubleArgument(expr.Indices[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ArraySafeIndex), typeof(object), typeof(double)));
            return;
        }
        var arrSymT0 = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
        // SQLRESULTSET (rs("col")): il risultato di SQL QUERY è un handle double
        // (parità VM: rsId dentro il dict __rsid__). La colonna della riga corrente
        // si legge dal driver SqlDriver via GetCurrentRow(rsId)[col].
        if (arrSymT0 is SqlResultSetType)
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: accesso a SQLRESULTSET richiede 1 indice (colonna).");
            // Parità VM: rs è un handle boxed (object contenente un double rsId, vedi
            // SqlQueryDriver). PRIMA della colonna va caricato rs e unboxxato a double,
            // altrimenti SqlResultSetField(double, string) riceve solo la colonna ->
            // stack mismatch -> InvalidProgramException.
            EmitLoadVariable(expr.SymbolId);
            var rsType = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
            var rsClr = DataTypeMapper.Map(rsType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, rsType, rsClr, _types.Double);
            // rs(i) NUMERICO → per indice di colonna; rs("col") STRINGA → per nome.
            var idxType = NormalizeInferredType(InferType(expr.Indices[0]));
            if (IlConversion.IsNumeric(idxType))
            {
                EmitInt32Argument(expr.Indices[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlResultSetFieldByIndex), typeof(double), typeof(int)));
            }
            else
            {
                EmitExpression(expr.Indices[0]);
                EmitConvertToString(idxType);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlResultSetField), typeof(double), typeof(string)));
            }
            return;
        }
        if (arrSymT0 is DictType or SortedDictType)
        {
            var dictClr = DataTypeMapper.Map(arrSymT0, _types, _classTypes);
            var dictIndexer = dictClr.GetProperty("Item")
                ?? throw new NotSupportedException($"Backend IL: '{arrSymT0}' non indicizzabile.");
            EmitLoadVariable(expr.SymbolId);
            EmitCollectionIndexerArgument(expr.Indices[0], dictIndexer.GetIndexParameters()[0].ParameterType);
            _il.Emit(OpCodes.Callvirt, dictIndexer.GetGetMethod()!);
            return;
        }
        // F4: valore matrice (descrittore) indicizzato m(r,c) → MatrixValueGetAt. Prima del
        // calcolo del tipo CLR "runtime array" (che per un parametro MatrixValueType cadrebbe
        // su object e rifiuterebbe i 2 indici).
        if (_program.Symbols.GetSymbol(expr.SymbolId).DataType is MatrixValueType)
        {
            if (expr.Indices.Length != 2)
                throw new NotSupportedException("Backend IL: una matrice-valore richiede 2 indici.");
            EmitLoadVariable(expr.SymbolId);
            EmitInt64Argument(expr.Indices[0]);
            EmitInt64Argument(expr.Indices[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueGetAt)));
            return;
        }
        if (TryGetDictSymbolInfo(expr.SymbolId, out _, out var indexer))
        {
            EmitLoadVariable(expr.SymbolId);
            EmitCollectionIndexerArgument(expr.Indices[0], indexer.GetIndexParameters()[0].ParameterType);
            _il.Emit(OpCodes.Callvirt, indexer.GetGetMethod()!);
            return;
        }

        // MyList(0) su una LIST (indexer getter) — non un array DIMensionato.
        var arrSymType = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
        // Tipo CLR dal SIMBOLO/array registrato — MAI GetOrDeclareLocal: un parametro-array
        // (es. v() dentro una lambda) non è un locale, e qui lanciava. Fallback: array
        // GLOBALE/STATIC dichiarato in un'ALTRA istanza (init-globali) — ricostruito dal tipo
        // del simbolo (bug 2026-10-01: senza, un array globale scalare cadeva nel path
        // dinamico CollectionIndexGet e il valore boxato veniva poi ri-boxato dal tipo statico
        // → bit di puntatore stampati invece del valore).
        var arrLocalClr = arrSymType is ListType
            ? DataTypeMapper.Map(arrSymType, _types, _classTypes)
            : _arrays.TryGetValue(expr.SymbolId, out var arrInfoL) ? arrInfoL.ClrArrayType
            : _locals.TryGetValue(expr.SymbolId, out var lbL) ? lbL.LocalType
            : TrySynthesizeBagArrayInfo(expr.SymbolId) is { } bagArrOuter ? bagArrOuter.ClrArrayType
            : typeof(object);
        if (arrSymType is ListType || (arrLocalClr.IsGenericType && arrLocalClr.GetGenericTypeDefinition() == typeof(List<>)))
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione di una LIST richiede 1 indice.");
            var listClr = arrSymType is ListType ? DataTypeMapper.Map(arrSymType, _types, _classTypes) : arrLocalClr;
            var listIndexer = listClr.GetProperty("Item")
                ?? throw new NotSupportedException("Backend IL: LIST non indicizzabile.");
            EmitLoadVariable(expr.SymbolId);
            EmitCollectionIndexerArgument(expr.Indices[0], listIndexer.GetIndexParameters()[0].ParameterType);
            _il.Emit(OpCodes.Callvirt, listIndexer.GetGetMethod()!);
            return;
        }
        if (arrLocalClr == typeof(object))
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione dinamica richiede 1 indice.");
            EmitLoadVariable(expr.SymbolId);
            EmitExpression(expr.Indices[0]);
            var idxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Indices[0])), _types, _classTypes);
            if (idxClr.IsValueType) _il.Emit(OpCodes.Box, idxClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexGet), typeof(object), typeof(object)));
            return;
        }

        // j("nome") su una variabile JSON/oggetto dinamico: il parser produce un
        // ArrayAccessExpr (nome non risolto come funzione), ma il symbol NON è un array
        // statico — accesso a indice dinamico via JsonIndexGet (come il VM).
        var symDataType = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
        if (symDataType is Basic.JsonType or AnyType)
        {
            if (expr.Indices.Length != 1)
                throw new NotSupportedException($"Backend IL: accesso a indice dinamico con {expr.Indices.Length} indici non supportato (serve 1).");
            EmitLoadVariable(expr.SymbolId);
            // La chiave decide il percorso (come per IndexedAccessExpr): STRINGA -> per nome, numero -> per indice, DINAMICA (object/ANY, es. la variabile di un
            // FOR EACH su JSON.KEYS) -> a runtime. Prima SEMPRE indice numerico: g[k] con k stringa dava FormatException ('aa') e il registry su IL rispondeva vuoto.
            var arrKeyRaw = InferType(expr.Indices[0]);
            var arrKeyType = NormalizeInferredType(arrKeyRaw);
            if (arrKeyType is StringType)
            {
                EmitStringArg(expr.Indices[0]);
                _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
            }
            else if (IsDynamicJsonKey(expr.Indices[0]))
            {
                EmitExpression(expr.Indices[0]);
                var arrKeyClr = DataTypeMapper.Map(arrKeyType, _types, _classTypes);
                if (arrKeyClr.IsValueType) _il.Emit(OpCodes.Box, arrKeyClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonIndexGetDynamic), typeof(object), typeof(object)));
            }
            else
            {
                EmitInt32Argument(expr.Indices[0]);
                _il.Emit(OpCodes.Call, IlJsonIndexGetMethod);
            }
            return;
        }

        // "nome()" senza indici: riferimento all'INTERO array (vedi InferType, stesso
        // ArrayAccessExpr di "nome(i)" — solo Indices.Length li distingue). Nessun
        // ldelem/Get da emettere, solo il riferimento CLR all'array così com'è — bypassa
        // il controllo Rank==indicesCount di GetArrayInfo (qui SEMPRE 0 per costruzione,
        // non ha senso confrontarlo col rank).
        if (expr.Indices.IsEmpty)
        {
            if (!_arrays.TryGetValue(expr.SymbolId, out var wholeArrayInfo))
                throw new NotSupportedException("Backend IL: accesso a un array non DIMensionato — DIM deve precedere l'uso.");
            _il.Emit(OpCodes.Ldloc, wholeArrayInfo.Local);
            return;
        }

        // Jagged (ArrayType di ArrayType — es. risultato FFT double[][]): 2 indici → [i][j]
        // (il primo ldelem dà l'array interno, il secondo l'elemento).
        var jaggedSymType = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
        if (jaggedSymType is ArrayType jaggedOuter && jaggedOuter.ElementType is ArrayType jaggedInner
            && expr.Indices.Length == 2 && _arrays.TryGetValue(expr.SymbolId, out var jaggedInfo))
        {
            _il.Emit(OpCodes.Ldloc, jaggedInfo.Local);
            EmitInt32Arg(expr.Indices[0]);
            _il.Emit(OpCodes.Ldelem, DataTypeMapper.Map(jaggedOuter.ElementType, _types, _classTypes));
            EmitInt32Arg(expr.Indices[1]);
            _il.Emit(OpCodes.Ldelem, DataTypeMapper.Map(jaggedInner.ElementType, _types, _classTypes));
            return;
        }

        // Jagged con 1 SOLO indice (double[][] → riga double[], es. `rollReg(regLen)(1)`):
        // GetArrayInfo richiederebbe rank==1 indice e lancerebbe. Rilevamento dall'ArrayInfo
        // REGISTRATO (rank>1 e ClrArrayType elemento-array), non dal type del simbolo (che
        // per 'ARRAY OF DOUBLE' e' rank-1 mentre il valore runtime e' jagged).
        if (expr.Indices.Length == 1 && _arrays.TryGetValue(expr.SymbolId, out var jRowInfo)
            && jRowInfo.Rank > 1 && jRowInfo.ClrArrayType.IsArray
            && jRowInfo.ClrArrayType.GetElementType()?.IsArray == true)
        {
            _il.Emit(OpCodes.Ldloc, jRowInfo.Local);
            EmitInt32Arg(expr.Indices[0]);
            _il.Emit(OpCodes.Ldelem, jRowInfo.ClrArrayType.GetElementType()!);
            return;
        }

        // Array RUNTIME non-DIM (es. MODES = SCREENS() in GraphicsShowModes: tipo statico
        // Double/Inferred ma valore runtime = array di DISPLAYMODE): il P-Code (tipi
        // dinamici) lo accetta. Il valore headless è comunque un default: valuta l'indice
        // (effetti collaterali) e ritorna 0.0 — evita il boxing del valore runtime di tipo
        // incerto (object/uint32/double) che causava InvalidProgram.
        if (!_arrays.ContainsKey(expr.SymbolId))
        {
            var runtimeSymType = NormalizeInferredType(_program.Symbols.GetSymbol(expr.SymbolId).DataType);
            if (runtimeSymType is DoubleType or InferredType)
            {
                if (expr.Indices.Length != 1)
                    throw new NotSupportedException("Backend IL: accesso a un array runtime non-DIM supporta solo 1 indice.");
                EmitExpression(expr.Indices[0]);
                _il.Emit(OpCodes.Pop);
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                return;
            }
            // Tupla (object[] da una FUNCTION AS (...)): result(0) — legge l'elemento reale
            // dall'object[] boxed. Il tipo dell'elemento dipende dall'indice: con indice
            // costante lo ricaviamo dal TupleType (stringa vs numerico); con indice dinamico
            // si assume numerico (ToDouble).
            if (runtimeSymType is TupleType tupleType)
            {
                if (expr.Indices.Length != 1)
                    throw new NotSupportedException("Backend IL: accesso a un elemento di tupla richiede 1 indice.");
                EmitLoadVariable(expr.SymbolId);
                EmitDoubleArgument(expr.Indices[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TupleElement), typeof(object), typeof(double)));
                var elementType = expr.Indices[0] is NumberExpr n && (int)n.Value >= 0
                                  && (int)n.Value < tupleType.Elements.Length
                    ? NormalizeInferredType(tupleType.Elements[(int)n.Value].DataType)
                    : null;
                if (elementType is StringType)
                    _il.Emit(OpCodes.Castclass, typeof(string));
                else if (elementType is null || IlConversion.IsNumeric(elementType))
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
                // Altrimenti il campo non è scalare (es. tupla annidata): l'object resta
                // così com'è (come il P-Code, che stampa System.Object[]).
                return;
            }
        }

        var info = GetArrayInfo(expr.SymbolId, expr.Indices.Length);
        EmitLoadArrayRefAndIndices(info, expr.Indices);

        if (info.Rank == 1)
        {
            EmitArrayElementLoad(info.ElementDataType);
        }
        else
        {
            // JAGGED (double[][] — DIM implicito 2D): ldelem.r8 sull'elemento scalare;
            // RETTANGOLARE (double[,] — DIM esplicito) o DESCRITTORE (MatrixValue —
            // Stage D): Get(ind0, ind1, ...) sul tipo CLR dell'array.
            if (info.ClrArrayType.IsArray && info.ClrArrayType.GetElementType()!.IsArray)
                EmitArrayElementLoad(info.ElementDataType);
            else
                _il.Emit(OpCodes.Call, info.ClrArrayType.GetMethod("Get")!);
        }
    }

    /// <summary>
    /// Copertura attuale (2026-07-19): solo le funzioni stringa più comuni, per
    /// argomenti numerici SOLO di tipo Double (vedi EmitInt32Arg) — ogni altra
    /// funzione builtin (LEN su array, funzioni matematiche/data/ecc.) lancia
    /// esplicito. INSTR copre solo la forma standard
    /// <c>INSTR(Stringa, Sottostringa, [Inizio])</c>, non l'overload "Clipper"
    /// <c>INSTR(Inizio, Stringa, Sottostringa)</c> che l'Executor esistente
    /// distingue a runtime in base al tipo dinamico del primo argomento — questo
    /// backend è staticamente tipizzato, quindi andrebbe distinto in base al
    /// DataType statico, non ancora fatto. MID$ usa StringHelpers.MidToEnd/
    /// MidWithLength (vedi StringHelpers.cs) invece di chiamare direttamente
    /// StringUtils.Mid, per evitare Nullable&lt;int&gt; nella firma del metodo
    /// chiamato dal codice generato.
    /// </summary>
}
