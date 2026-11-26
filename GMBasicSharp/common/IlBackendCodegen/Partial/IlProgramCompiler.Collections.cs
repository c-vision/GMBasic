// FILE: IlProgramCompiler.Collections.cs
// PARTIAL — dominio COLLEZIONI (array/dict/list/matrix/vector compile-time emit).
// Migrazione in corso (3-step protocollo).

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

/// <summary>Partial del transpiler IL: dominio COLLEZIONI.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitTypedCollectionLiteral(ListLiteralExpr expr, DataType targetType)
    {
        var elementType = CollectionElementType(targetType);
        var elementClr = DataTypeMapper.Map(elementType, _types, _classTypes);
        var concreteType = DataTypeMapper.Map(targetType, _types, _classTypes);
        var methodName = targetType switch
        {
            QueueType => nameof(Queue<int>.Enqueue),
            StackType => nameof(Stack<int>.Push),
            LinkedListType => nameof(LinkedList<int>.AddLast),
            DoublyLinkedListType => nameof(LinkedList<int>.AddLast),
            _ => nameof(List<int>.Add)
        };
        var add = concreteType.GetMethod(methodName, [elementClr])
            ?? throw new NotSupportedException($"Backend IL: inizializzatore non disponibile per '{targetType}'.");
        var ctor = concreteType.GetConstructor(Type.EmptyTypes)
            ?? throw new NotSupportedException($"Backend IL: costruttore vuoto non disponibile per '{targetType}'.");
        _il.Emit(OpCodes.Newobj, ctor);
        foreach (var element in expr.Elements)
        {
            _il.Emit(OpCodes.Dup);
            EmitExpression(element);
            EmitCoerceTypes(NormalizeInferredType(InferType(element)), elementType);
            _il.Emit(OpCodes.Callvirt, add);
            if (add.ReturnType != typeof(void)) _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitDictionaryLiteral(DictionaryLiteralExpr expr, DataType? expectedType)
    {
        var dictType = expectedType ?? NormalizeInferredType(InferType(expr));
        var (keyType, valueType) = dictType switch
        {
            DictType dict => (dict.KeyType, dict.ValueType),
            SortedDictType dict => (dict.KeyType, dict.ValueType),
            // TREE/AVL/REDBLACK/BPLUS sono materializzati come SortedDictionary<object,object?>
            // (il case "Strutture avanzate" di EmitNewAdvancedCollection) — il letterale {..}
            // deve produrre lo STESSO tipo, non object (stloc object con un dict va bene ma
            // `MyTree.Add(...)` poi lo riuserebbe — prima il letterale non era gestito qui e
            // produceva `ldc.i4 0` come istanza → CollectionInvoke(int, ...) = IL invalido).
            TreeType or AVLTreeType or RedBlackTreeType or BPlusTreeType => (new AnyType(), new AnyType()),
            _ => throw new NotSupportedException("Backend IL: letterale {} privo di un tipo DICT/SORTED risolto.")
        };
        // Se i tipi dei VALORI (o delle chiavi) degli entry NON coincidono tutti col tipo
        // dichiarato/inferito del dict, usa object (parità PCode: un dict con valori misti è
        // Dictionary<string,object> con i value-type boxati). Senza, un expectedType sbagliato
        // (es. Dict<string,string> da un letterale {a:"x", b:1}) faceva `Add(string,string)` con
        // un double → InvalidProgramException (JsonFullCoverage/ForEachJsonEdgeCases).
        for (var ei = 0; ei < expr.Entries.Length; ei++)
        {
            var ev = NormalizeInferredType(InferType(expr.Entries[ei].Value));
            var ek = NormalizeInferredType(InferType(expr.Entries[ei].Key));
            // Numerico → numerico NON e' eterogeneo: EmitCoerceTypes converte il letterale (es. `1`, inferito Double) nel tipo
            // dichiarato (INTEGER/LONG/...). Promuovere a object in quel caso creava un Dictionary<K,object> per una variabile
            // Dictionary<K,short> → IL non valido e crash del processo (SIGBUS) su `DIM d AS DICT OF STRING, INTEGER = {"a": 1}`.
            var valueConvertible = IlConversion.IsNumeric(ev) && IlConversion.IsNumeric(valueType);
            var keyConvertible = IlConversion.IsNumeric(ek) && IlConversion.IsNumeric(keyType);
            if ((ev.GetType() != valueType.GetType() && !valueConvertible) || ev is AnyType) { valueType = new AnyType(); }
            if ((ek.GetType() != keyType.GetType() && !keyConvertible) || ek is AnyType) { keyType = new AnyType(); }
        }
        // Aggiorna anche dictType col tipo (possibilmente promosso) così concreteType/Add
        // combaciano (per un DICT; per TREE/AVL/... resta il dizionario ordinato).
        // ATTENZIONE: SORTED<...> deve restare SORTED — ricostruirlo come DictType emetteva
        // un Dictionary<K,V> in una variabile dichiarata SortedDictionary<K,V> →
        // InvalidCastException "Dictionary`2 -> SortedDictionary`2" (sample CollectionsBasics
        // §3B, DictExtended/DictLists/Linked).
        if (dictType is SortedDictType)
            dictType = new SortedDictType(keyType, valueType);
        else if (dictType is not (TreeType or AVLTreeType or RedBlackTreeType or BPlusTreeType))
            dictType = new DictType(keyType, valueType);
        var keyClrType = DataTypeMapper.Map(keyType, _types, _classTypes);
        var valueClrType = DataTypeMapper.Map(valueType, _types, _classTypes);
        var concreteType = dictType is TreeType or AVLTreeType or RedBlackTreeType or BPlusTreeType
            ? typeof(SortedDictionary<object, object?>)
            : DataTypeMapper.Map(dictType, _types, _classTypes);
        var ctor = concreteType.GetConstructor(Type.EmptyTypes)!;
        var add = concreteType.GetMethod(nameof(Dictionary<int, int>.Add), [keyClrType, valueClrType])!;
        var clrDictIsObjectValue = valueClrType == typeof(object);
        _il.Emit(OpCodes.Newobj, ctor);
        foreach (var entry in expr.Entries)
        {
            _il.Emit(OpCodes.Dup);
            EmitExpression(entry.Key);
            EmitCoerceTypes(NormalizeInferredType(InferType(entry.Key)), keyType);
            EmitExpression(entry.Value);
            EmitCoerceTypes(NormalizeInferredType(InferType(entry.Value)), valueType);
            // Valore VALUE TYPE per un target REFERENCE (es. DICT<STRING,ANY> = { timeout: 30 }
            // → Dictionary<string,object>): il box mancante produceva Add(string,object)
            // con un double → InvalidProgram.
            if (clrDictIsObjectValue)
            {
                var realValueClr = DataTypeMapper.Map(NormalizeInferredType(InferType(entry.Value)), _types, _classTypes);
                if (realValueClr.IsValueType) _il.Emit(OpCodes.Box, realValueClr);
            }
            _il.Emit(OpCodes.Callvirt, add);
        }
    }

    private void EmitTypedArrayLiteral(ImmutableArray<Expr> elements, DataType elementType)
    {
        var elementClrType = DataTypeMapper.Map(elementType, _types, _classTypes);
        _il.Emit(OpCodes.Ldc_I4, elements.Length);
        _il.Emit(OpCodes.Newarr, elementClrType);
        for (var i = 0; i < elements.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(elements[i]);
            EmitCoerceTypes(NormalizeInferredType(InferType(elements[i])), elementType);
            EmitArrayElementStore(elementType);
        }
    }

    private void EmitListLiteralElements(ImmutableArray<Expr> elements, ListType listType)
    {
        var elementClrType = DataTypeMapper.Map(listType.ElementType, _types, _classTypes);
        var concreteType = typeof(List<>).MakeGenericType(elementClrType);
        var ctor = concreteType.GetConstructor(Type.EmptyTypes)!;
        var add = concreteType.GetMethod(nameof(List<int>.Add), [elementClrType])!;
        _il.Emit(OpCodes.Newobj, ctor);
        foreach (var element in elements)
        {
            _il.Emit(OpCodes.Dup);
            var elementInferred = NormalizeInferredType(InferType(element));
            // Elemento NON-lista (es. "Help" stringa) in un letterale di LISTE (il MENU: le
            // voci possono essere liste (submenu) o stringhe semplici — l'IDIOMA del
            // ClipperMenuNested) → avvolgi in una lista di 1 elemento (prima: castclass
            // "Help"→List<string> = ArrayTypeMismatchException).
            if (listType.ElementType is ListType innerListElementType && elementInferred is not ListType)
            {
                EmitListLiteralElements([element], innerListElementType);
            }
            else if (listType.ElementType is AnyType)
            {
                EmitObjectBoxedArgument(element);
            }
            else
            {
                EmitExpression(element);
                EmitCoerceTypes(elementInferred, listType.ElementType);
            }
            _il.Emit(OpCodes.Callvirt, add);
        }
    }

    private void EmitListLiteral(ListLiteralExpr expr, ListType? expectedType)
    {
        var listType = expectedType ?? NormalizeInferredType(InferType(expr)) as ListType
            ?? throw new NotSupportedException("Backend IL: letterale [] privo di un tipo LIST risolto.");
        EmitListLiteralElements(expr.Elements, listType);
    }

    private void EmitArrayLiteralExpression(ArrayLiteralExpr expr)
    {
        // Inferisce il tipo dell'elemento dal PRIMO elemento del letterale (come
        // EmitListLiteral): prima era hardcoded a double[] + Stelem_R8, quindi un
        // letterale di STRINGHE (DIM Nomi = {"Mario","Luigi","Anna"}) emetteva
        // stelem.r8 su una stringa = stack mismatch -> InvalidProgramException
        // (FunctionsLambdaInference/ASCAN). Il tipo CLR dell'array deve combaciare
        // con il tipo statico degli elementi.
        var elementType = expr.Elements.Length == 0
            ? (DataType)new DoubleType()
            : NormalizeInferredType(InferType(expr.Elements[0]));
        var elementClrType = DataTypeMapper.Map(elementType, _types, _classTypes);
        _il.Emit(OpCodes.Ldc_I4, expr.Elements.Length);
        _il.Emit(OpCodes.Newarr, elementClrType);
        for (var i = 0; i < expr.Elements.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(expr.Elements[i]);
            EmitCoerceTypes(NormalizeInferredType(InferType(expr.Elements[i])), elementType);
            EmitArrayElementStore(elementType);
        }
    }

    private void EmitNewAdvancedCollection(DataType dataType)
    {
        switch (dataType)
        {
            case SparseSetType:
                _il.Emit(OpCodes.Ldc_I4, 1_000_000);
                _il.Emit(OpCodes.Ldc_I4, 64);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.SparseSet).GetConstructor([typeof(int), typeof(int)])!);
                return;
            case RingBufferType rb:
                // Tipo ELEMENTO reale (RINGBUFFER<LONG> → CircularQueue<long>): prima <object>
                // hardcoded → InvalidCastException (CircularQueue<object> vs <long>) o Capacity
                // garbage. Il local/bag ha il tipo di DataTypeMapper.Map.
                _il.Emit(OpCodes.Ldc_I4, 64);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.CircularQueue<>)
                    .MakeGenericType(DataTypeMapper.Map(rb.ElementType, _types, _classTypes))
                    .GetConstructor([typeof(int)])!);
                return;
            case HashRingType:
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Newarr, typeof(object));
                _il.Emit(OpCodes.Ldc_I4_1);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.ConsistentHash<object>).GetConstructor([typeof(IEnumerable<object>), typeof(int)])!);
                return;
            case BPlusTreeType:
                _il.Emit(OpCodes.Ldc_I4_3);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.BPlusTree<string, object?>).GetConstructor([typeof(int)])!);
                return;
            case TimelineType:
                // Timeline<ValueType> dal tipo passato (es. TIMELINE<STRING> → Timeline<string>),
                // NON Timeline<object> hardcoded: stloc in un local Timeline<string> con un
                // Timeline<object> = IL invalido (StackUnexpected a runtime).
                _il.Emit(OpCodes.Newobj, DataTypeMapper.Map(dataType, _types, _classTypes).GetConstructor(Type.EmptyTypes)!);
                return;
            case DoublyLinkedListType or CircularLinkedListType:
                /* LINKEDLIST doppia → LinkedList<T>; CIRCULAR → List<T> (DataTypeMapper):
                 * prima cadevano nel default SortedDictionary → local di tipo sbagliato
                 * (Count garbage / AccessViolation su DIM senza inizializzatore). */
                _il.Emit(OpCodes.Newobj, DataTypeMapper.Map(dataType, _types, _classTypes).GetConstructor(Type.EmptyTypes)!);
                return;
            case TrieType:
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.Trie).GetConstructor(Type.EmptyTypes)!);
                return;
            case GraphType:
                // GRAPH<K> ha API AddNode/AddEdge (CommonLib.Graph), NON è un SortedDictionary.
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.Graph<object>).GetConstructor(Type.EmptyTypes)!);
                return;
            case CacheType:
                EmitNewNativeCollection(dataType); /* Cache<K,V> ctor vuoto */
                return;
            case BinaryHeapType heapT:
                // HEAP/MAXHEAP → CommonLib.Collections.BinaryHeap<object> (Add/Extract/Peek/
                // Count con heapify). Prima si finiva nel default qui sotto (SortedDictionary)
                // e il LETTERALE produceva una List: Peek restituiva il primo inserito.
                _il.Emit(OpCodes.Ldc_I4, heapT.IsMaxHeap ? 1 : 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NewHeap), typeof(bool)));
                return;
            case BinaryIndexedTreeType:
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Newarr, typeof(double));
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.BinaryIndexedTree<double>).GetConstructor([typeof(double[])])!);
                return;
            case HyperLogLogType:
                // HLL: classe CommonLib (mirror del runtime C gmb_hll.c), precisione default.
                // Ordine IL: argomento (int p) PRIMA del newobj.
                _il.Emit(OpCodes.Ldc_I4, CommonLib.Collections.HyperLogLog.DefaultPrecision);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.HyperLogLog).GetConstructor([typeof(int)])!);
                return;
            default:
                // Tree/AVL/RedBlack/Graph/Bag/Heap/Circular/Doubly: SortedDictionary
                // (chiave ordinata, come il VM) o Dictionary per Bag.
                if (dataType is BagType bagT)
                {
                    // Tipo CHIAVE reale (BAG<STRING> → Dictionary<string,long>):
                    // DataTypeMapper.Map(BagType) — prima <object,long> hardcoded →
                    // InvalidCastException su Dictionary<string,long>.
                    _il.Emit(OpCodes.Newobj, typeof(IlRuntime.BagMultiset<>)
                        .MakeGenericType(DataTypeMapper.Map(bagT.ElementType, _types, _classTypes))
                        .GetConstructor(Type.EmptyTypes)!);
                    return;
                }
                _il.Emit(OpCodes.Newobj, typeof(SortedDictionary<object, object?>).GetConstructor(Type.EmptyTypes)!);
                return;
        }
    }

    private void EmitNewNativeCollection(DataType dataType)
    {
        var collectionType = DataTypeMapper.Map(dataType, _types, _classTypes);
        // GetConstructor diretto su un generico chiuso che contiene un TypeBuilder come
        // argomento (es. List<Person> con Person = TYPE utente) fallisce con "TypeBuilder
        // generic instantiation does not support resolving members" — limite noto di
        // Reflection.Emit, richiede il ponte esplicito TypeBuilder.GetConstructor(closedType,
        // ctorSulTipoAperto). Per un generico chiuso "normale" (es. List<string>, nessun
        // TypeBuilder coinvolto) vale l'inverso: il ponte stesso fallisce ("must be or must
        // contain a TypeBuilder"), va usato il percorso diretto originale.
        ConstructorInfo collectionCtor;
        if (ClosedGenericContainsTypeBuilder(collectionType))
        {
            var openCtor = collectionType.GetGenericTypeDefinition().GetConstructor(Type.EmptyTypes)
                ?? throw new NotSupportedException($"Backend IL: costruttore vuoto non disponibile per '{dataType}'.");
            collectionCtor = TypeBuilder.GetConstructor(collectionType, openCtor);
        }
        else
        {
            collectionCtor = collectionType.GetConstructor(Type.EmptyTypes)
                ?? collectionType.GetConstructor([typeof(int?), typeof(TimeSpan?)])
                ?? throw new NotSupportedException($"Backend IL: costruttore vuoto non disponibile per '{dataType}'.");
        }
        if (collectionCtor.GetParameters().Length > 0)
        {
            // Cache(int? capacity, TimeSpan? cleanup) — defaults: capacity=null, cleanup=null.
            // int? e TimeSpan? sono STRUCT (Nullable<T>): ldnull (reference) è IL invalido.
            EmitNullableDefault(typeof(int?));
            EmitNullableDefault(typeof(TimeSpan?));
        }
        _il.Emit(OpCodes.Newobj, collectionCtor);
    }

    private void EmitComprehensionClauses(ImmutableArray<ComprehensionClause> clauses, int idx, LocalBuilder result, Action inner)
    {
        if (idx >= clauses.Length)
        {
            inner();
            return;
        }
        var clause = clauses[idx];
        // Sorgente ARRAY o LISTA (la comprehension/generator expr itera entrambe — una
        // List come sorgente col Castclass a System.Array falliva InvalidCastException).
        var sourceClr = DataTypeMapper.Map(NormalizeInferredType(InferType(clause.SourceExpression)), _types, _classTypes);
        if (sourceClr.IsArray)
        {
            EmitExpression(clause.SourceExpression);
        }
        else
        {
            // Sorgente LISTA/collezione: non è un System.Array → converti (prima il
            // Castclass falliva InvalidCastException).
            EmitExpression(clause.SourceExpression);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EnumerableToArray), typeof(System.Collections.IEnumerable)));
        }
        var sourceLocal = _il.DeclareLocal(typeof(System.Array));
        _il.Emit(OpCodes.Castclass, typeof(System.Array));
        _il.Emit(OpCodes.Stloc, sourceLocal);

        var iLocal = _il.DeclareLocal(typeof(int));
        var loopLabel = _il.DefineLabel();
        var condLabel = _il.DefineLabel();
        var endLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, iLocal);
        _il.Emit(OpCodes.Br, condLabel);
        _il.MarkLabel(loopLabel);

        var iterLocal = GetOrDeclareLocal(clause.IteratorSymbolId);
        _il.Emit(OpCodes.Ldloc, sourceLocal);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Callvirt, typeof(System.Array).GetMethod("GetValue", [typeof(int)])!);
        // Elemento NUMERICO → ToDouble; NON-numerico (stringa/oggetto — es. UPPER(s) FOR s
        // IN words con words LIST<STRING>) → resta boxed (ToDouble('ciao') = FormatException).
        var srcForCompr = NormalizeInferredType(InferType(clause.SourceExpression));
        var srcElemNumeric = srcForCompr switch
        {
            ArrayType at => IlConversion.IsNumeric(at.ElementType),
            ListType lt => IlConversion.IsNumeric(lt.ElementType),
            _ => true
        };
        if (srcElemNumeric)
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
        _il.Emit(OpCodes.Stloc, iterLocal);

        if (clause.ConditionExpression is not null)
        {
            EmitExpression(clause.ConditionExpression);
            _il.Emit(OpCodes.Brfalse, endLabel);
        }
        EmitComprehensionClauses(clauses, idx + 1, result, inner);
        _il.MarkLabel(endLabel);

        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Stloc, iLocal);
        _il.MarkLabel(condLabel);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldloc, sourceLocal);
        _il.Emit(OpCodes.Callvirt, typeof(System.Array).GetMethod("get_Length")!);
        _il.Emit(OpCodes.Blt, loopLabel);
    }

    /// <summary>Emette `e` coercito al tipo `target` di un elemento di collection: conversione numerica (EmitCoerceTypes) e box
    /// quando la destinazione e' object e l'origine un value type.</summary>
    private void EmitComprehensionItem(Expr e, DataType target)
    {
        EmitExpression(e);
        var st = NormalizeInferredType(InferType(e));
        EmitCoerceTypes(st, target);
        var tClr = DataTypeMapper.Map(target, _types, _classTypes);
        var sClr = DataTypeMapper.Map(st, _types, _classTypes);
        if (tClr == typeof(object) && sClr.IsValueType) _il.Emit(OpCodes.Box, sClr);
    }

    private void EmitSetComprehension(SetComprehensionExpr expr, HashSetType? target = null)
    {
        // { x FOR x IN src } -> HashSet<T>: T dal DIM tipizzato (HASHSET OF T) o dall'espressione elemento (default double).
        var elemType = target?.ElementType is { } te && te is not InferredType
            ? NormalizeInferredType(te)
            : NormalizeInferredType(InferType(expr.ElementExpression));
        var setClr = typeof(System.Collections.Generic.HashSet<>).MakeGenericType(DataTypeMapper.Map(elemType, _types, _classTypes));
        var resultLocal = _il.DeclareLocal(setClr);
        _il.Emit(OpCodes.Newobj, setClr.GetConstructor(Type.EmptyTypes)!);
        _il.Emit(OpCodes.Stloc, resultLocal);
        EmitComprehensionClauses(expr.Clauses, 0, resultLocal, () =>
        {
            _il.Emit(OpCodes.Ldloc, resultLocal);
            EmitComprehensionItem(expr.ElementExpression, elemType);
            _il.Emit(OpCodes.Callvirt, setClr.GetMethod("Add")!);
            _il.Emit(OpCodes.Pop);
        });
        _il.Emit(OpCodes.Ldloc, resultLocal);
    }

    private void EmitDictComprehension(DictComprehensionExpr expr, DictType? target = null)
    {
        // { k : v FOR k IN src [IF cond] } -> Dictionary<K,V>: K/V dal DIM tipizzato (DICT OF K,V) o dalle espressioni.
        // (Prima era cablato su Dictionary<double,double>: una sorgente STRING dava FormatException.)
        var keyType = target?.KeyType is { } tk && tk is not InferredType
            ? NormalizeInferredType(tk) : NormalizeInferredType(InferType(expr.KeyExpression));
        var valType = target?.ValueType is { } tv && tv is not InferredType
            ? NormalizeInferredType(tv) : NormalizeInferredType(InferType(expr.ValueExpression));
        var dictClr = typeof(System.Collections.Generic.Dictionary<,>).MakeGenericType(
            DataTypeMapper.Map(keyType, _types, _classTypes), DataTypeMapper.Map(valType, _types, _classTypes));
        var resultLocal = _il.DeclareLocal(dictClr);
        _il.Emit(OpCodes.Newobj, dictClr.GetConstructor(Type.EmptyTypes)!);
        _il.Emit(OpCodes.Stloc, resultLocal);
        EmitComprehensionClauses(expr.Clauses, 0, resultLocal, () =>
        {
            _il.Emit(OpCodes.Ldloc, resultLocal);
            EmitComprehensionItem(expr.KeyExpression, keyType);
            EmitComprehensionItem(expr.ValueExpression, valType);
            _il.Emit(OpCodes.Callvirt, dictClr.GetMethod("set_Item")!);
        });
        _il.Emit(OpCodes.Ldloc, resultLocal);
    }

    private void EmitRedimStatement(RedimStmt stmt)
    {
        // REDIM u.campoArray(n) su un campo ARRAY di UDT/CLASS (es. ParserStress
        // `TYPE MyUdt: items(5) AS STRING * 10` + `REDIM u.items(1)`): indirizzo del
        // campo via Ldloca(istanza) + Ldflda(campo), poi Array.Resize.
        if (stmt.ArrayTarget is FieldTarget ft && ft.Structure is VariableTarget ftInst
            && _classFieldArrays.TryGetValue(ft.FieldSymbolId, out var ftArrayInfo)
            && _locals.TryGetValue(ftInst.SymbolId, out var ftInstLocal))
        {
            if (stmt.Initializer is not null)
                throw new NotSupportedException("Backend IL: REDIM con inizializzatore non ancora supportato.");
            if (stmt.Dimensions.Length != 1)
                throw new NotSupportedException("Backend IL: REDIM di array multi-dimensionali non ancora supportato (solo mono-dimensionali).");
            _il.Emit(OpCodes.Ldloca, ftInstLocal);
            _il.Emit(OpCodes.Ldflda, ftArrayInfo.Field);
            var ftElemClr = ftArrayInfo.ClrArrayType.GetElementType()!;
            EmitAxisLength(stmt.Dimensions[0], ftArrayInfo.LowerBounds.Length > 0 ? ftArrayInfo.LowerBounds[0] : 0);
            _il.Emit(OpCodes.Call, _types.ArrayResizeOpenGeneric.MakeGenericMethod(ftElemClr));
            return;
        }

        if (stmt.ArrayTarget is not VariableTarget target)
            throw new NotSupportedException("Backend IL: REDIM richiede un semplice nome di array (non un elemento indicizzato).");
        if (stmt.Dimensions.Length > 1)
            throw new NotSupportedException("Backend IL: REDIM di array multi-dimensionali non ancora supportato (solo mono-dimensionali).");

        if (!_arrays.TryGetValue(target.SymbolId, out var info))
            throw new NotSupportedException($"Backend IL: REDIM su un array non DIMensionato ('{target.NameCleaned.Lexeme}') non supportato — serve un DIM precedente.");
        if (info.Rank != 1)
            throw new NotSupportedException("Backend IL: REDIM di array multi-dimensionali non ancora supportato (solo mono-dimensionali).");

        var lowerBound = info.LowerBounds[0];

        // REDIM con inizializzatore letterale (REDIM C = {100,200,300} / REDIM PRESERVE
        // C(5) = {98,99}): crea/ridimensiona l'array poi popola i primi elementi.
        if (stmt.Initializer is ArrayLiteralExpr initLit)
        {
            var initElemClr = info.ClrArrayType.GetElementType()!;
            // Con PRESERVE, l'inizializzatore popola i NUOVI elementi (a partire dalla
            // vecchia lunghezza), non sovrascrive quelli preservati da indice 0 — bug
            // misurato: `REDIM PRESERVE C(5) = {98,99}` su C={100,200,300} sovrascriveva
            // C(0)/C(1) invece di popolare C(3)/C(4) (ArraysRedim.bas, "Estende l'array a 6
            // elementi (0-5) e popola i nuovi elementi (indici 3, 4)"). Va catturata la
            // lunghezza PRIMA del resize (Array.Resize muta l'array in place).
            LocalBuilder? oldLenLocal = null;
            if (stmt.Preserve is not null)
            {
                oldLenLocal = _il.DeclareLocal(_types.Int32);
                _il.Emit(OpCodes.Ldloc, info.Local);
                _il.Emit(OpCodes.Ldlen);
                _il.Emit(OpCodes.Conv_I4);
                _il.Emit(OpCodes.Stloc, oldLenLocal);

                var resizeMethod = _types.ArrayResizeOpenGeneric.MakeGenericMethod(initElemClr);
                _il.Emit(OpCodes.Ldloca, info.Local);
                EmitAxisLength(stmt.Dimensions[0], lowerBound);
                _il.Emit(OpCodes.Call, resizeMethod);
            }
            else if (stmt.Dimensions.Length == 1)
            {
                EmitAxisLength(stmt.Dimensions[0], lowerBound);
                _il.Emit(OpCodes.Newarr, initElemClr);
                _il.Emit(OpCodes.Stloc, info.Local);
            }
            else
            {
                // REDIM C = {..}: nessuna dimensione → lunghezza dal letterale.
                _il.Emit(OpCodes.Ldc_I4, initLit.Elements.Length);
                _il.Emit(OpCodes.Newarr, initElemClr);
                _il.Emit(OpCodes.Stloc, info.Local);
            }
            for (var initIdx = 0; initIdx < initLit.Elements.Length; initIdx++)
            {
                _il.Emit(OpCodes.Ldloc, info.Local);
                if (oldLenLocal != null)
                {
                    _il.Emit(OpCodes.Ldloc, oldLenLocal);
                    _il.Emit(OpCodes.Ldc_I4, initIdx);
                    _il.Emit(OpCodes.Add);
                }
                else
                {
                    _il.Emit(OpCodes.Ldc_I4, initIdx);
                }
                EmitExpression(initLit.Elements[initIdx]);
                EmitCoerceTypes(NormalizeInferredType(InferType(initLit.Elements[initIdx])), info.ElementDataType);
                EmitArrayElementStore(info.ElementDataType);
            }
            goto ArrayRedimWriteback;
        }

        if (stmt.Preserve is not null)
        {
            var elementClrType = info.ClrArrayType.GetElementType()!;
            var resizeMethod = _types.ArrayResizeOpenGeneric.MakeGenericMethod(elementClrType);
            _il.Emit(OpCodes.Ldloca, info.Local);
            EmitAxisLength(stmt.Dimensions[0], lowerBound);
            _il.Emit(OpCodes.Call, resizeMethod);
        }
        else
        {
            EmitAxisLength(stmt.Dimensions[0], lowerBound);
            _il.Emit(OpCodes.Newarr, info.ClrArrayType.GetElementType()!);
            _il.Emit(OpCodes.Stloc, info.Local);
        }

        ArrayRedimWriteback:
        // REDIM su un parametro-array BYREF (vedi RegisterParameters): il locale è stato
        // appena riassegnato/ridimensionato, ma resta una COPIA finché non viene scritta
        // indietro attraverso il puntatore BYREF originale — altrimenti il chiamante non
        // vedrebbe mai il nuovo array (esattamente il pattern di
        // Arrays/ArraysComplexProcessing.bas: SUB CreaDatiDiTest(BYREF Dati() AS Vendita)
        // che fa REDIM Dati(5) e si aspetta che il chiamante riceva l'array ridimensionato).
        if (_arrayParamWriteback.TryGetValue(target.SymbolId, out var argIndex))
        {
            _il.Emit(OpCodes.Ldarg, argIndex);
            _il.Emit(OpCodes.Ldloc, info.Local);
            _il.Emit(OpCodes.Stind_Ref);
        }
        // REDIM su un array STATIC/GLOBALE dichiarato con inizializzatore (`DIM x() = {...}`,
        // RegisterDynamicArrayLocal): il valore vero vive nel bag (ArrayInfo.LoadRef punta a
        // LoadGlobal/LoadStatic), NON nel local qui sopra riassegnato — senza scrivere indietro
        // nel bag, `REDIM C = {100,200,300}` aggiornava SOLO il local (mai più letto: ogni
        // lettura successiva di C passa da LoadRef/LoadGlobal) → il REDIM sembrava un no-op
        // silenzioso (bug: ArraysRedim.bas, "Array C dopo REDIM con inizializzatore: 1, 2"
        // invece di "100, 200, 300"). Un array dichiarato con dimensioni esplicite (`DIM A(1)`,
        // ArrayOps.cs:744, LoadRef sempre null) non ha questo problema: il local È la vera
        // storage, come dimostrato dalle sezioni 1-3 dello stesso sample che già funzionavano.
        else if (info.LoadRef != null)
        {
            _il.Emit(OpCodes.Ldc_I4, target.SymbolId);
            _il.Emit(OpCodes.Ldloc, info.Local);
            if (IsStaticSymbol(target.SymbolId))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreStatic), typeof(int), typeof(object)));
            else
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
        }
    }

    private void EmitComprehensionClauseLoop(ImmutableArray<ComprehensionClause> clauses, int index, Expr output, LocalBuilder resultList)
    {
        var clause = clauses[index];
        DataType elemType = new DoubleType();

        // emitSrcRef: azione che carica il riferimento all'array SOURCE sullo stack — MAI un
        // Ldloc diretto sul local registrato in _arrays, perché per un array STATIC/GLOBALE
        // (es. `DIM nums = {5,12,3,21,8,15}` a livello modulo, senza Dimensions esplicite)
        // quel local non viene MAI scritto (il valore vive nel bag via StoreGlobal — vedi
        // EmitStoreVariable/RegisterDynamicArrayLocal): un Ldloc diretto legge null → Ldlen
        // lancia NullReferenceException PRIMA ancora di entrare nel loop (bug: `FROM n IN
        // nums ...` su un nums globale). EmitArrayRefFromInfo rispetta ArrayInfo.LoadRef
        // (LoadGlobal/LoadStatic) quando presente, altrimenti fa comunque Ldloc.
        Action? emitSrcRef = null;
        if (clause.SourceExpression is VariableExpr sv && _arrays.TryGetValue(sv.SymbolId, out var svInfo))
        {
            emitSrcRef = () => EmitArrayRefFromInfo(svInfo);
            elemType = svInfo.ElementDataType;
        }
        else if (clause.SourceExpression is ArrayLiteralExpr ale)
        {
            // Sorgente LETTERALE: array di stringhe se gli elementi sono stringhe
            // (`{ c FOR c IN {"a","b"} }`), altrimenti double. Prima hardcoded double[] →
            // Stelem_R8 su una stringa = IL invalido (InvalidProgramException).
            bool strLit = ale.Elements.Length > 0
                && NormalizeInferredType(InferType(ale.Elements[0])) is StringType;
            var tmp = _il.DeclareLocal(strLit ? typeof(string[]) : typeof(double[]));
            _il.Emit(OpCodes.Ldc_I4, ale.Elements.Length);
            _il.Emit(OpCodes.Newarr, strLit ? _types.String : _types.Double);
            for (var i = 0; i < ale.Elements.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                if (strLit) { EmitExpression(ale.Elements[i]); _il.Emit(OpCodes.Stelem_Ref); }
                else { EmitDoubleArgument(ale.Elements[i]); _il.Emit(OpCodes.Stelem_R8); }
            }
            _il.Emit(OpCodes.Stloc, tmp);
            emitSrcRef = () => _il.Emit(OpCodes.Ldloc, tmp);
            elemType = strLit ? new StringType() : new DoubleType();
        }
        else if (clause.SourceExpression is VariableExpr sv2)
        {
            // Array GLOBALE o letterale non in _arrays: il DIM `numeri = {...}` vive come
            // local (double[] via GetOrDeclareLocal) — caricalo da lì (stesso percorso di
            // EmitLoadVariable per una variabile non-array). Il tipo statico è ArrayType.
            if (_arrays.TryGetValue(sv2.SymbolId, out var gInfo))
            {
                emitSrcRef = () => EmitArrayRefFromInfo(gInfo);
                elemType = gInfo.ElementDataType;
            }
            else
            {
                var symType = NormalizeInferredType(_program.Symbols.GetSymbol(sv2.SymbolId).DataType);
                if (symType is ArrayType globArr)
                {
                    var sv2Id = sv2.SymbolId;
                    elemType = globArr.ElementType;
                    // Non tracciato in _arrays (nessun ArrayInfo/LoadRef): se è comunque un
                    // simbolo GLOBALE, il valore vive nel bag (stesso motivo del ramo sopra) —
                    // altrimenti è un local vero (parametro/variabile di procedura).
                    if (IsGlobalSymbol(sv2Id))
                    {
                        var globClr = DataTypeMapper.Map(globArr, _types, _classTypes);
                        emitSrcRef = () =>
                        {
                            _il.Emit(OpCodes.Ldc_I4, sv2Id);
                            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
                            _il.Emit(OpCodes.Castclass, globClr);
                        };
                    }
                    else
                    {
                        var localRef = GetOrDeclareLocal(sv2Id);
                        emitSrcRef = () => _il.Emit(OpCodes.Ldloc, localRef);
                    }
                }
                else throw new NotSupportedException($"Backend IL: comprensione lista su source '{clause.SourceExpression}' non risolta (serve un array 1D).");
            }
        }
        else throw new NotSupportedException($"Backend IL: comprensione lista su source '{clause.SourceExpression.GetType().Name}' non ancora supportata (serve un array 1D).");

        var counter = _il.DeclareLocal(typeof(int));
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, counter);
        var iterLocal = GetOrDeclareLocal(clause.IteratorSymbolId);

        var loopStart = _il.DefineLabel();
        var loopEnd = _il.DefineLabel();
        _il.MarkLabel(loopStart);
        _il.Emit(OpCodes.Ldloc, counter);
        emitSrcRef();
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Bge, loopEnd);

        emitSrcRef();
        _il.Emit(OpCodes.Ldloc, counter);
        _il.Emit(OpCodes.Ldelem, DataTypeMapper.Map(elemType, _types, _classTypes));
        _il.Emit(OpCodes.Stloc, iterLocal);

        // La WHERE (ConditionExpression) può referenziare l'iteratore con un SymbolId
        // DIVERSO da clause.IteratorSymbolId (stesso rewrite del Linker già noto per
        // l'output, vedi sotto): senza questo alias, `EmitExpression(ConditionExpression)`
        // leggerebbe un local MAI scritto (default 0.0) — `n > 10` risulta sempre falso,
        // il filtro scarta OGNI elemento (bug: `FROM n IN nums WHERE n > 10 SELECT n`
        // ritornava un array vuoto, Count=0, pur senza eccezioni). Registra il local
        // dell'iteratore anche sotto l'eventuale SymbolId "sbagliato" trovato nella
        // condizione, così qualunque lettura successiva (EmitLoadVariable) lo risolve
        // correttamente.
        var iterName = _program.Symbols.GetSymbol(clause.IteratorSymbolId)?.OriginalName;
        if (iterName is not null) AliasIteratorReferences(clause.ConditionExpression, iterName, clause.IteratorSymbolId, iterLocal);

        if (index == clauses.Length - 1)
        {
            Label skipAppend = default;
            if (clause.ConditionExpression is not null)
            {
                skipAppend = _il.DefineLabel();
                EmitExpression(clause.ConditionExpression);
                _il.Emit(OpCodes.Brfalse, skipAppend);
            }
            _il.Emit(OpCodes.Ldloc, resultList);
            // L'output della comprensione può referenziare l'iteratore della clausola con un
            // SymbolId DIVERSO (rewrite del Linker) — EmitExpression(output) caricherebbe un
            // local mai scritto (NULL → NRE). Quando l'output è un VariableExpr con lo stesso
            // nome dell'iteratore, usa il local reale (iterLocal) appena popolato dal Ldelem.
            // Per output COMPOSTI che referenziano comunque l'iteratore (es. `SELECT n * 2`),
            // l'alias registrato sotto copre anche EmitExpression(output) nel ramo else.
            if (iterName is not null) AliasIteratorReferences(output, iterName, clause.IteratorSymbolId, iterLocal);
            if (output is VariableExpr outVe &&
                _program.Symbols.GetSymbol(outVe.SymbolId)?.OriginalName ==
                _program.Symbols.GetSymbol(clause.IteratorSymbolId)?.OriginalName)
            {
                _il.Emit(OpCodes.Ldloc, iterLocal);
            }
            else
            {
                EmitExpression(output);
            }
            // Tipo elemento della lista risultato (List<double> o List<string> per le
            // comprehension con sorgente/output stringa): derivalo dal local, così `Add`
            // riceve il tipo giusto (prima era hardcoded List<double>.Add(double) → IL
            // invalido `InvalidProgramException` su `[ n FOR n IN names ]`).
            _il.Emit(OpCodes.Callvirt, resultList.LocalType.GetMethod(nameof(List<double>.Add))!);
            if (clause.ConditionExpression is not null) _il.MarkLabel(skipAppend);
        }
        else
        {
            EmitComprehensionClauseLoop(clauses, index + 1, output, resultList);
        }

        _il.Emit(OpCodes.Ldloc, counter);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Stloc, counter);
        _il.Emit(OpCodes.Br, loopStart);
        _il.MarkLabel(loopEnd);
    }

    /// <summary>Cammina ricorsivamente <paramref name="expr"/> (Variable/Binary/Unary — i
    /// nodi che compaiono nelle condizioni/output di una comprensione) e per ogni VariableExpr
    /// che si chiama come <paramref name="iterName"/> ma con un SymbolId DIVERSO da
    /// <paramref name="correctSymbolId"/> (rewrite del Linker che assegna un SymbolId
    /// "duplicato" alla stessa variabile in punti diversi dell'espressione), registra un alias
    /// in _locals così che EmitLoadVariable/EmitExpression risolvano comunque al local reale
    /// dell'iteratore invece di un local mai scritto. Best-effort: nodi non gestiti sono
    /// ignorati silenziosamente (nessun crash, semplicemente non alias-ati).</summary>
    private void AliasIteratorReferences(Expr? expr, string iterName, int correctSymbolId, LocalBuilder iterLocal)
    {
        switch (expr)
        {
            case null:
                break;
            case VariableExpr v:
                if (v.SymbolId != correctSymbolId
                    && string.Equals(_program.Symbols.GetSymbol(v.SymbolId)?.OriginalName, iterName, StringComparison.OrdinalIgnoreCase))
                    _locals[v.SymbolId] = iterLocal;
                break;
            case BinaryExpr b:
                AliasIteratorReferences(b.Left, iterName, correctSymbolId, iterLocal);
                AliasIteratorReferences(b.Right, iterName, correctSymbolId, iterLocal);
                break;
            case UnaryExpr u:
                AliasIteratorReferences(u.Right, iterName, correctSymbolId, iterLocal);
                break;
        }
    }

    private void EmitListComprehension(ListComprehensionExpr expr)
    {
        // [ expr FOR a IN A FOR b IN B [IF cond] ]: prodotto cartesiano ricorsivo su N
        // clausole (ogni FOR è un loop annidato); nell'innermost si valuta l'output (con IF).
        // ORDER BY: supportato quando la chiave di ordinamento è l'output stesso
        // (pattern `FROM n IN x ORDER BY n SELECT n`) → Array.Sort sul risultato.
        // Chiavi diverse dall'output restano un limite esplicito (non stub).
        bool orderByOutput = false;
        bool orderDesc = false;
        if (expr.OrderByClauses.Length > 0)
        {
            // La chiave ORDER BY e' l'output stesso quando referenziano lo stesso iteratore
            // (il rewrite del Linker riscrive l'output ma non le clausole → SymbolId possono
            // differire a livello di ToString, ma il riferimento simbolico coincide).
            if (expr.OrderByClauses.All(o => SameLinqKeyAsOutput(o.Expression, expr.OutputExpression)))
            {
                orderByOutput = true;
                orderDesc = expr.OrderByClauses.Any(o => o.Descending);
            }
            else
            {
                throw new NotSupportedException("Backend IL: comprensione lista con ORDER BY su chiave diversa dall'output non ancora supportata.");
            }
        }

        // Elemento del risultato: StringType se l'output è stringa (sorgente/iteratore
        // stringa), altrimenti double (comportamento storico). `DIM up = [ n FOR n IN names ]`
        // produce string[]; `[ x*x FOR x IN nums ]` resta double[].
        bool elemIsStr = NormalizeInferredType(InferType(expr.OutputExpression)) is StringType;
        Type clrElem = elemIsStr ? typeof(string) : typeof(double);
        var listType = typeof(List<>).MakeGenericType(clrElem);
        var resultList = _il.DeclareLocal(listType);
        _il.Emit(OpCodes.Newobj, listType.GetConstructor(Type.EmptyTypes)!);
        _il.Emit(OpCodes.Stloc, resultList);

        EmitComprehensionClauseLoop(expr.Clauses, 0, expr.OutputExpression, resultList);

        // Il target BASIC è un ArrayType: converto la lista in T[] (List<T>.ToArray).
        _il.Emit(OpCodes.Ldloc, resultList);
        _il.Emit(OpCodes.Callvirt, listType.GetMethod(nameof(List<double>.ToArray), Type.EmptyTypes)!);
        if (orderByOutput)
        {
            // double[] in cima: ordina in-place (Array.Sort), poi Reverse se DESC.
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Call, ArrayHelpersSortArrayMethod);
            if (orderDesc)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Call, ArrayHelpersReverseArrayMethod);
            }
        }
    }

    private static DataType UnwrapArrayLevels(DataType type, int levels)
    {
        var t = NormalizeInferredType(type);
        for (var i = 0; i < levels; i++)
            t = t is ArrayType at ? at.ElementType : t;
        return t;
    }

    private bool TryGetDictSymbolInfo(int symbolId, out Type clrDictType, out System.Reflection.PropertyInfo indexer)
    {
        var symbol = _program.Symbols.GetSymbol(symbolId);
        if (symbol.DataType is DictType dictType)
        {
            clrDictType = DataTypeMapper.Map(dictType, _types, _classTypes);
            indexer = clrDictType.GetProperty("Item")!;
            return true;
        }
        clrDictType = null!;
        indexer = null!;
        return false;
    }

    private void EmitArrayElementStore(DataType elementType)
    {
        switch (elementType)
        {
            case DoubleType: _il.Emit(OpCodes.Stelem_R8); return;
            case SingleType: _il.Emit(OpCodes.Stelem_R4); return;
            case LongType or ULongType or IntegerType or UIntegerType: _il.Emit(OpCodes.Stelem_I8); return;
            case ByteType or SByteType or BitType: _il.Emit(OpCodes.Stelem_I1); return;
            case StringType or ClassType or InterfaceType: _il.Emit(OpCodes.Stelem_Ref); return;
            default: _il.Emit(OpCodes.Stelem, DataTypeMapper.Map(elementType, _types, _classTypes)); return;
        }
    }

    private void EmitArrayElementLoad(DataType elementType)
    {
        switch (elementType)
        {
            case DoubleType: _il.Emit(OpCodes.Ldelem_R8); return;
            // Elemento di array SINGLE: lo storage e' float[] (ABI FFI/AI.EMBED) ma il VALORE
            // nel resto del codice e' double (SINGLE scalare mappato a double, parita' C): senza
            // conv.r8 lo stack resta F e i consumatori emettono `box System.Double` / `add`
            // F8 su un float = IL invalido (visto su `d = a(1)` e `a(0) + a(1)`).
            case SingleType: _il.Emit(OpCodes.Ldelem_R4); _il.Emit(OpCodes.Conv_R8); return;
            case LongType or ULongType or IntegerType or UIntegerType: _il.Emit(OpCodes.Ldelem_I8); return;
            case ByteType: _il.Emit(OpCodes.Ldelem_U1); return;
            case SByteType or BitType: _il.Emit(OpCodes.Ldelem_I1); return;
            case StringType or ClassType or InterfaceType: _il.Emit(OpCodes.Ldelem_Ref); return;
            default: _il.Emit(OpCodes.Ldelem, DataTypeMapper.Map(elementType, _types, _classTypes)); return;
        }
    }

    private void EmitCoerceToElementClr(DataType elementType)
    {
        // Gli ARRAY SINGLE restano float[] (ABI FFI/AI.EMBED, vedi DataTypeMapper: caso
        // ArrayType): l'elemento va coercito a float32 anche quando lo SCALARE SINGLE è mappato
        // a double — con la mappatura scalare si emetteva conv.r8 su uno stelem.r4.
        if (elementType is SingleType) { _il.Emit(OpCodes.Conv_R4); return; }
        var elementClr = DataTypeMapper.Map(elementType, _types, _classTypes);
        if (elementClr == _types.Int16) _il.Emit(OpCodes.Conv_I2);
        else if (elementClr == _types.Int32) _il.Emit(OpCodes.Conv_I4);
        else if (elementClr == _types.Int64) _il.Emit(OpCodes.Conv_I8);
        else if (elementClr == _types.Byte || elementClr == _types.SByte) _il.Emit(OpCodes.Conv_I1);
        else if (elementClr == _types.Double) _il.Emit(OpCodes.Conv_R8);
        else if (elementClr == _types.Single) _il.Emit(OpCodes.Conv_R4);
    }

    private void EmitArrayElementStore(ArrayElementTarget target, Action emitValue)
    {
        var info = GetArrayInfo(target.SymbolId, target.Indices.Length);
        EmitLoadArrayRefAndIndices(info, target.Indices);
        emitValue();
        // Coerce del valore al tipo CLR dell'elemento prima dello store: `ExTimer(slot) =
        // TI()` (int64) con stelem.i2 senza conversione lancia Convert.ToInt16 overflow
        // (il JIT converte checked). conv.i2/i1/i8 wrappano/trattano senza overflow check,
        // come il P-Code.
        if (info.Rank == 1)
        {
            EmitCoerceToElementClr(info.ElementDataType);
            EmitArrayElementStore(info.ElementDataType);
        }
        else
        {
            _il.Emit(OpCodes.Call, info.ClrArrayType.GetMethod("Set")!);
        }
    }

    private void EmitArrayElementAssignment(ArrayElementTarget target, Expr value)
    {
        // F4: valore MatrixValue m(r,c) = v → mutazione diretta MatrixValueSetAt.
        if (NormalizeInferredType(_program.Symbols.GetSymbol(target.SymbolId).DataType) is MatrixValueType)
        {
            if (target.Indices.Length != 2)
                throw new NotSupportedException("Backend IL: una matrice-valore richiede 2 indici.");
            EmitLoadVariable(target.SymbolId);
            EmitInt64Argument(target.Indices[0]);
            EmitInt64Argument(target.Indices[1]);
            EmitDoubleArgument(value);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueSetAt)));
            _il.Emit(OpCodes.Pop);   // SetAt restituisce la matrice; qui l'effetto è la scrittura
            return;
        }
        if (TryGetDictSymbolInfo(target.SymbolId, out _, out var indexer))
        {
            EmitLoadVariable(target.SymbolId);
            EmitCollectionIndexerArgument(target.Indices[0], indexer.GetIndexParameters()[0].ParameterType);
            EmitExpression(value);
            var sourceType = NormalizeInferredType(InferType(value));
            IlConversion.EmitConvert(_il, _types, sourceType, DataTypeMapper.Map(sourceType, _types, _classTypes), indexer.PropertyType);
            _il.Emit(OpCodes.Callvirt, indexer.GetSetMethod()!);
            return;
        }

        // SORTED dict: TryGetDictSymbolInfo gestisce solo DictType — SortedDictType
        // (SortedDictionary<K,V>) ha lo stesso indexer Item.
        var aetSymbolType = NormalizeInferredType(_program.Symbols.GetSymbol(target.SymbolId).DataType);
        if (aetSymbolType is SortedDictType)
        {
            var sdClr = DataTypeMapper.Map(aetSymbolType, _types, _classTypes);
            var sdIndexer = sdClr.GetProperty("Item")
                ?? throw new NotSupportedException($"Backend IL: '{aetSymbolType}' non indicizzabile.");
            if (target.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione di un SORTED richiede 1 indice.");
            EmitLoadVariable(target.SymbolId);
            EmitCollectionIndexerArgument(target.Indices[0], sdIndexer.GetIndexParameters()[0].ParameterType);
            EmitExpression(value);
            var srcT = NormalizeInferredType(InferType(value));
            IlConversion.EmitConvert(_il, _types, srcT, DataTypeMapper.Map(srcT, _types, _classTypes), sdIndexer.PropertyType);
            _il.Emit(OpCodes.Callvirt, sdIndexer.GetSetMethod()!);
            return;
        }

        // TIMELINE: t(data) = valore — scrittura per CHIAVE DATA (Add con sovrascrittura),
        // dispatch runtime condiviso (normalizza la data → seriale). Prima cadeva nel path
        // array ("accesso a un array non DIMensionato").
        var aetTimelineType = NormalizeInferredType(_program.Symbols.GetSymbol(target.SymbolId).DataType);
        if (aetTimelineType is TimelineType)
        {
            if (target.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione di una TIMELINE richiede 1 indice (chiave data).");
            EmitLoadVariable(target.SymbolId);
            EmitExpression(target.Indices[0]);
            var tlIdxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(target.Indices[0])), _types, _classTypes);
            if (tlIdxClr.IsValueType) _il.Emit(OpCodes.Box, tlIdxClr);
            EmitExpression(value);
            var tlValClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
            if (tlValClr.IsValueType) _il.Emit(OpCodes.Box, tlValClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexSet), typeof(object), typeof(object), typeof(object)));
            return;
        }

        // MyList2(0) = "BETA" su una LIST (non un array): usa l'indexer CLR List<T>[int].
        // Il tipo statico può essere AnyType (`DIM MyList2 = MyList.ToList()` senza AS) — il
        // local è object boxed: dispatch runtime via IlRuntime.CollectionIndexSet.
        var elemSymbolType = NormalizeInferredType(_program.Symbols.GetSymbol(target.SymbolId).DataType);
        // Tipo CLR dal SIMBOLO/array — MAI GetOrDeclareLocal (parametro-array non è un locale).
        var elemLocalClr = elemSymbolType is ListType
            ? DataTypeMapper.Map(elemSymbolType, _types, _classTypes)
            : _arrays.TryGetValue(target.SymbolId, out var arrInfoT) ? arrInfoT.ClrArrayType
            : TrySynthesizeBagArrayInfo(target.SymbolId) is { } bagArrT ? bagArrT.ClrArrayType
            : _locals.TryGetValue(target.SymbolId, out var lbT) ? lbT.LocalType
            : typeof(object);
        if (elemLocalClr == typeof(object))
        {
            if (target.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione dinamica richiede 1 indice.");
            EmitLoadVariable(target.SymbolId);
            EmitExpression(target.Indices[0]);
            var idxClr = DataTypeMapper.Map(NormalizeInferredType(InferType(target.Indices[0])), _types, _classTypes);
            if (idxClr.IsValueType) _il.Emit(OpCodes.Box, idxClr);
            EmitExpression(value);
            var valClr = DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes);
            if (valClr.IsValueType) _il.Emit(OpCodes.Box, valClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionIndexSet), typeof(object), typeof(object), typeof(object)));
            return;
        }
        if (elemSymbolType is ListType || (elemLocalClr.IsGenericType && elemLocalClr.GetGenericTypeDefinition() == typeof(List<>)))
        {
            var listClr = elemSymbolType is ListType
                ? DataTypeMapper.Map(elemSymbolType, _types, _classTypes)
                : elemLocalClr;
            var indexerProp = listClr.GetProperty("Item");
            if (indexerProp is null)
                throw new NotSupportedException($"Backend IL: indicizzazione di '{elemSymbolType}' non supportata (nessun indexer).");
            if (target.Indices.Length != 1)
                throw new NotSupportedException("Backend IL: indicizzazione di una LIST richiede 1 indice.");
            EmitLoadVariable(target.SymbolId);
            EmitInt32Argument(target.Indices[0]);
            EmitExpression(value);
            var listElemClr = elemSymbolType is ListType elemListT
                ? DataTypeMapper.Map(elemListT.ElementType, _types, _classTypes)
                : indexerProp.PropertyType;
            IlConversion.EmitConvert(_il, _types, NormalizeInferredType(InferType(value)), DataTypeMapper.Map(NormalizeInferredType(InferType(value)), _types, _classTypes), listElemClr);
            _il.Emit(OpCodes.Callvirt, indexerProp.GetSetMethod()!);
            return;
        }

        // Il valore va convertito al tipo ELEMENTO dell'array (es. INTEGER/DOUBLE → BYTE)
        // PRIMA di stelem — senza questa conversione, un valore numerico di tipo diverso
        // dall'elemento (es. il contatore di un FOR, spesso rappresentato internamente
        // come DOUBLE) finiva su stelem.i1/stelem.i2/ecc. con la rappresentazione CLR
        // sbagliata: per un array BYTE il risultato erano SEMPRE zeri (i bit bassi di un
        // double con valore intero piccolo sono quasi sempre 0), scoperto con
        // "DIM k(31) AS BYTE: FOR i=0 TO 31: k(i)=i: NEXT" che riempiva k con soli zeri.
        var elementDataType = GetArrayInfo(target.SymbolId, target.Indices.Length).ElementDataType;
        EmitArrayElementStore(target, () =>
        {
            EmitExpression(value);
            EmitCoerceTypes(NormalizeInferredType(InferType(value)), elementDataType);
        });
    }

    private void EmitLoadArrayRefAndIndices(Action emitLoadRef, int rank, int[] lowerBounds, ImmutableArray<Expr> indices, Type infoClrArrayType, ArrayInfo? lowerInfo = null)
    {
        emitLoadRef();
        var isJagged = rank > 1 && infoClrArrayType.IsArray && infoClrArrayType.GetElementType()!.IsArray;
        for (var axis = 0; axis < rank; axis++)
        {
            EmitInt32Arg(indices[axis]);
            /* Parametri-array: lower runtime da LowerLocals; array locali: costante. */
            if ((lowerInfo?.LowerLocals is { Length: > 0 }) || lowerBounds[axis] != 0)
            {
                if (lowerInfo is not null) EmitArrayLower(lowerInfo, axis);
                else _il.Emit(OpCodes.Ldc_I4, lowerBounds[axis]);
                _il.Emit(OpCodes.Sub);
            }
            // Array JAGGED (rank > 1, es. double[][] dal DIM implicito 2D): gli axis intermedi
            // dereferenziano la riga (ldelem.ref), l'ultimo resta al consumatore (ldelem.r8/
            // stelem.r8). Gli array RETTANGOLARI (double[,]) NON fanno ldelem: il chiamante
            // usa Get/Set con tutti gli indici.
            if (isJagged && axis < rank - 1) _il.Emit(OpCodes.Ldelem_Ref);
        }
    }

    private void EmitArrayRefFromInfo(ArrayInfo info)
    {
        if (info.LoadRef != null) { info.LoadRef(); return; }
        _il.Emit(OpCodes.Ldloc, info.Local);
        // BUG REALE trovato 2026-09-07 (AccessViolationException su IL con due FOR EACH su
        // liste dinamiche come RANGE(...) nella stessa SUB): per un array DINAMICO
        // (RegisterDynamicArrayLocal, es. `DIM a = RANGE(1,3)`) il LocalBuilder può restare
        // dichiarato `object` invece del vero tipo array (`System.Double[]`) -- capita quando
        // il local per il simbolo viene dichiarato/riusato (GetOrDeclareLocal) prima che il
        // tipo dell'array sia completamente risolto, e IlRuntime.Range/ReverseGeneric/simili
        // ritornano `object` a livello di firma CLR. Il chiamante di questa funzione fa quasi
        // sempre `ldlen`/`ldelem` subito dopo: applicati a un valore `object` senza cast e'
        // IL non verificabile -- funziona per caso quando l'esecuzione capita di non toccare
        // quella memoria in modo distruttivo, va in AccessViolationException (memoria
        // corrotta) quando la pressione di allocazione cambia (es. un secondo ciclo nella
        // stessa SUB). Fix: cast esplicito al vero tipo array se il local non e' gia' quel
        // tipo -- stesso idiom gia' usato per il caricamento STATIC/GLOBAL qui sopra
        // (Castclass dopo LoadStatic/LoadGlobal in RegisterDynamicArrayLocal). Nessun costo
        // quando il local e' gia' tipizzato correttamente (il caso comune): Type.op_Equality
        // e' un confronto di riferimento, il cast non viene MAI emesso in quel caso.
        if (info.Local.LocalType != info.ClrArrayType)
            _il.Emit(OpCodes.Castclass, info.ClrArrayType);
    }

    private void EmitLoadArrayRefAndIndices(ArrayInfo info, ImmutableArray<Expr> indices)
        => EmitLoadArrayRefAndIndices(() => EmitArrayRefFromInfo(info), info.Rank, info.LowerBounds, indices, info.ClrArrayType, info);

    /// <summary>Emette il lower bound logico dell'asse: costante per gli array locali,
    /// letto da una local per i parametri-array (propagato via IlRuntime.ArrayLowerTable).</summary>
    private void EmitArrayLower(ArrayInfo info, int axis)
    {
        if (info.LowerLocals is { Length: > 0 } ll && axis >= 0 && axis < ll.Length && ll[axis] is not null)
            _il.Emit(OpCodes.Ldloc, ll[axis]!);
        else
            _il.Emit(OpCodes.Ldc_I4, axis < info.LowerBounds.Length ? info.LowerBounds[axis] : 0);
    }

    private ArrayInfo GetArrayInfo(int symbolId, int indicesCount)
    {
        if (!_arrays.TryGetValue(symbolId, out var info))
        {
            // Array GLOBALE/STATIC dichiarato in un'ALTRA istanza del compilatore (init-globali):
            // il valore vive nel bag, ricostruiamo l'ArrayInfo on-demand dal tipo del simbolo.
            var synthesized = TrySynthesizeBagArrayInfo(symbolId);
            if (synthesized is null)
                throw new NotSupportedException("Backend IL: accesso a un array non DIMensionato — DIM deve precedere l'uso.");
            _arrays[symbolId] = info = synthesized;
        }
        if (info.Rank != indicesCount)
            throw new NotSupportedException(
                $"Backend IL: numero di indici ({indicesCount}) diverso dal rank dell'array ({info.Rank}).");
        return info;
    }

    /// <summary>
    /// Ricostruisce un <see cref="ArrayInfo"/> per un array GLOBALE/STATIC non registrato in
    /// <c>_arrays</c> di QUESTA istanza (tipico: dichiarato a livello modulo, emesso dall'istanza
    /// di init-globali — vedi EmitArrayDeclaration/RegisterDynamicArrayLocal). Il riferimento
    /// vive nel bag (StoreGlobal/StoreStatic), quindi il LoadRef lo legge da lì.
    /// </summary>
    private ArrayInfo? TrySynthesizeBagArrayInfo(int symbolId)
    {
        var isStatic = IsStaticSymbol(symbolId);
        if (!isStatic && !IsGlobalSymbol(symbolId)) return null;
        var symbol = _program.Symbols.GetSymbol(symbolId);
        if (symbol is null) return null;
        if (NormalizeInferredType(symbol.DataType) is not ArrayType arrayType) return null;
        var rank = IlArrayRank(arrayType);
        DataType elementType = arrayType;
        while (elementType is ArrayType inner) { elementType = inner.ElementType; }
        if (rank < 1) return null;
        var clrArrayType = DataTypeMapper.Map(arrayType, _types, _classTypes);
        var local = GetOrDeclareLocal(symbolId);
        Action loadRef;
        if (isStatic)
        {
            loadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, symbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadStatic), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        else
        {
            loadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, symbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        return new ArrayInfo(local, clrArrayType, elementType, rank, new int[rank], loadRef);
    }

    private void EmitAxisLength(Expr upperBoundExpr, int lowerBound)
    {
        if (TryExtractCompileTimeInt32(upperBoundExpr, out var upperBound))
        {
            _il.Emit(OpCodes.Ldc_I4, upperBound - lowerBound + 1);
            return;
        }

        EmitExpression(upperBoundExpr);
        _il.Emit(OpCodes.Conv_I4);
        var offset = lowerBound - 1; // length = upperBound - (lowerBound - 1)
        if (offset != 0)
        {
            _il.Emit(OpCodes.Ldc_I4, offset);
            _il.Emit(OpCodes.Sub);
        }
    }

    private static int ExtractCompileTimeInt32(Expr expr) =>
        TryExtractCompileTimeInt32(expr, out var value)
            ? value
            : throw new NotSupportedException(
                $"Backend IL: limite di DIM/REDIM non costante a compile-time ('{expr.GetType().Name}') non ancora supportato.");

    private static bool TryExtractCompileTimeInt32(Expr expr, out int value)
    {
        switch (expr)
        {
            case NumberExpr n: value = (int)n.Value; return true;
            /* Conversioni intere iniettate dal Linker sugli argomenti con tipo dichiarato
             * LONG (es. la dimensione di LBOUND/UBOUND: `LBOUND(A, 1)` arriva come
             * `CONVERSIONS.CINT(1)`). Senza questo caso la dimensione era trattata come
             * RUNTIME (TryEmitRuntimeBound) e il lower bound logico (DIM 1 TO N) veniva
             * perso: UBOUND(a, 1) restituiva la length-1 CLR invece del bound BASIC. */
            case BuiltinFunctionCallExpr { Name.Kind: TokenKind.CLng or TokenKind.CInt or TokenKind.CByte } f when f.Args.Length == 1:
                return TryExtractCompileTimeInt32(f.Args[0], out value);
            case UnaryExpr u when u.Op.Kind == TokenKind.Minus:
                if (TryExtractCompileTimeInt32(u.Right, out var inner)) { value = -inner; return true; }
                value = 0;
                return false;
            default:
                value = 0;
                return false;
        }
    }

    /// accettano System.Array e ritornano System.Array): castclass degli argomenti array a
    /// Array, box del valore, e Castclass del ritorno al tipo originale — pattern EmitAPadCall.</summary>
    private void EmitArrayTypedCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length < 1)
            throw new NotSupportedException($"Backend IL: {methodName} richiede almeno 1 argomento.");
        var info = ResolveArrayArg(expr.Args[0], methodName);
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Castclass, typeof(Array));
        for (int i = 1; i < expr.Args.Length; i++)
        {
            var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            EmitExpression(expr.Args[i]);
            if (argClr.IsArray)
                _il.Emit(OpCodes.Castclass, typeof(Array));
            else if (argClr.IsValueType)
                _il.Emit(OpCodes.Box, argClr);
        }
        var method = typeof(IlRuntime).GetMethod(methodName)!;
        _il.Emit(OpCodes.Call, method);
        _il.Emit(OpCodes.Castclass, info.ClrArrayType);
    }

    private void EmitRangeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: RANGE richiede 1 argomento (ParamArray).");
        // Il Linker imballa gli argomenti del call site RANGE(a,b[,step]) in un unico
        // ArrayLiteralExpr sintetico, a volte DOPPIO (ParamArray annidato) — stesso pattern del
        // path FOR EACH (IlProgramCompiler.ControlFlow.EmitForEachCollectionAsObject). Senza
        // sprimere il wrapping, il letterale veniva emesso come array di ARRAY (newarr double[])
        // e lo stelem falliva con ArrayTypeMismatchException (ForEachJsonEdgeCases §1).
        Expr rangeArg = expr.Args[0];
        while (rangeArg is ArrayLiteralExpr { Elements: [ArrayLiteralExpr innerLit] }) rangeArg = innerLit;
        EmitExpression(rangeArg);
        var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(rangeArg)), _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Range), typeof(object)));
    }

    private void EmitDictSetCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: SET richiede 3 argomenti (collection, key, value).");
        for (var i = 0; i < 3; i++)
        {
            EmitExpression(expr.Args[i]);
            var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Set), typeof(object), typeof(object), typeof(object)));
    }

    private void EmitDictGetCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: GET richiede 2 argomenti (collection, key).");
        for (var i = 0; i < 2; i++)
        {
            EmitExpression(expr.Args[i]);
            var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Get), typeof(object), typeof(object)));
    }

    private void EmitArrayTwoDoublesCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (array, double, double).");
        // Il primo argomento può essere array 1-D o matrice jagged (MINCVAR/MINCDAR:
        // storici[][]): il runtime fa FlattenReturns(object).
        EmitReturnsArgument(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(object), typeof(double), typeof(double)));
    }

    private void EmitArrayDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (array, double).");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(double[]), typeof(double)));
    }

    private void EmitArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento (array).");
        EmitDoubleArrayArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(double[])));
    }

    private void EmitDictAddCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: ADD richiede 2 o 3 argomenti (collection, value[, key]).");
        EmitExpression(expr.Args[0]);
        var cClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
        if (cClr.IsValueType) _il.Emit(OpCodes.Box, cClr);
        EmitExpression(expr.Args[1]);
        var vClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[1])), _types, _classTypes);
        if (vClr.IsValueType) _il.Emit(OpCodes.Box, vClr);
        if (expr.Args.Length == 3)
        {
            EmitExpression(expr.Args[2]);
            var kClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[2])), _types, _classTypes);
            if (kClr.IsValueType) _il.Emit(OpCodes.Box, kClr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Add), typeof(object), typeof(object), typeof(object)));
    }
}
