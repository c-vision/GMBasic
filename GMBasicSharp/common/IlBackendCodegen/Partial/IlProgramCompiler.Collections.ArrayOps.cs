// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Collections.ArrayOps.cs
// PARTIAL — dominio COLLECTIONS/ARRAYOPS: statement/comandi ARRAYS.* (AAdd/AIns/ADel/
// AFill/ACopy/AEval/ASort/AShuffle/AScan/...). Migrato da
// IlProgramCompiler.Collections.cs (2026-08-30 — split, file <= 1200 righe).

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

/// <summary>Partial del transpiler IL: operazioni array/comandi ARRAYS.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitAEvalStatement(AEvalStmt stmt)
    {
        var (info, _) = ResolveArrayStmtTarget(stmt.Array, "AEVAL");
        var elementType = info.ClrArrayType.GetElementType()!;
        // T del generico = tipo del PARAMETRO della callback (come AMAP/AFILTER): il
        // risultato di AFILTER/AMAP tipizzato sul parametro (es. Pari short[] da
        // FN(n AS INTEGER)) NON coincide col tipo statico del simbolo (double[]) — senza,
        // AEval<double> con un delegate Action<short> era IL invalido.
        var evalParamClr = GetCallbackParamClrType(stmt.ActionFunction);
        if (evalParamClr is not null && evalParamClr != elementType)
        {
            _il.Emit(OpCodes.Ldloc, info.Local);
            _il.Emit(OpCodes.Call, ArrayHelpersConvertArrayMethod.MakeGenericMethod(elementType, evalParamClr));
            elementType = evalParamClr;
        }
        else
        {
            _il.Emit(OpCodes.Ldloc, info.Local);
        }
        EmitExpression(stmt.ActionFunction);

        if (GetCallbackArity(stmt.ActionFunction) == 2)
            _il.Emit(OpCodes.Call, ArrayHelpersAEvalIndexedMethod.MakeGenericMethod(elementType));
        else
            _il.Emit(OpCodes.Call, ArrayHelpersAEvalMethod.MakeGenericMethod(elementType));
    }

    private void EmitOrderDescendingFlag(Expr? order)
    {
        if (order is not null) EmitExpression(order);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldstr, "D");
        _il.Emit(OpCodes.Call, _types.StringEquals);
    }

    private void EmitAShuffleStatement(AShuffleStmt stmt)
    {
        var (info, _) = ResolveArrayStmtTarget(stmt.Array, "ASHUFFLE");
        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Call, ArrayUtilsShuffleMethod);
    }

    private void EmitASortStatement(ASortStmt stmt)
    {
        var (info, _) = ResolveArrayStmtTarget(stmt.Array, "ASORT");
        var elementType = info.ClrArrayType.GetElementType()!;

        _il.Emit(OpCodes.Ldloc, info.Local);

        if (stmt.CompareFunction is not null)
        {
            EmitExpression(stmt.CompareFunction);
            var compareReturnClrType = DataTypeMapper.Map(GetCallbackReturnType(stmt.CompareFunction), _types, _classTypes);
            EmitOrderDescendingFlag(stmt.Order);
            _il.Emit(OpCodes.Call, ArrayHelpersASortWithComparerMethod.MakeGenericMethod(elementType, compareReturnClrType));
        }
        else
        {
            EmitOrderDescendingFlag(stmt.Order);
            _il.Emit(OpCodes.Call, ArrayHelpersASortMethod.MakeGenericMethod(elementType));
        }
    }

    private void EmitACopyStatement(ACopyStmt stmt)
    {
        var (srcInfo, _) = ResolveArrayStmtTarget(stmt.Source, "ACOPY (sorgente)");
        var (destInfo, _) = ResolveArrayStmtTarget(stmt.Destination, "ACOPY (destinazione)");

        var srcStartLocal = _il.DeclareLocal(_types.Int32);
        if (stmt.SourceStart is not null) EmitInt32Arg(stmt.SourceStart);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, srcStartLocal);

        var destStartLocal = _il.DeclareLocal(_types.Int32);
        if (stmt.DestStart is not null) EmitInt32Arg(stmt.DestStart);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, destStartLocal);

        var countLocal = _il.DeclareLocal(_types.Int32);
        if (stmt.Count is not null)
        {
            EmitInt32Arg(stmt.Count);
            _il.Emit(OpCodes.Stloc, countLocal);
        }
        else
        {
            var srcRemainLocal = _il.DeclareLocal(_types.Int32);
            _il.Emit(OpCodes.Ldloc, srcInfo.Local);
            _il.Emit(OpCodes.Ldlen);
            _il.Emit(OpCodes.Conv_I4);
            _il.Emit(OpCodes.Ldloc, srcStartLocal);
            _il.Emit(OpCodes.Sub);
            _il.Emit(OpCodes.Stloc, srcRemainLocal);

            var destRemainLocal = _il.DeclareLocal(_types.Int32);
            _il.Emit(OpCodes.Ldloc, destInfo.Local);
            _il.Emit(OpCodes.Ldlen);
            _il.Emit(OpCodes.Conv_I4);
            _il.Emit(OpCodes.Ldloc, destStartLocal);
            _il.Emit(OpCodes.Sub);
            _il.Emit(OpCodes.Stloc, destRemainLocal);

            var srcIsSmallerLabel = _il.DefineLabel();
            var minDoneLabel = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, srcRemainLocal);
            _il.Emit(OpCodes.Ldloc, destRemainLocal);
            _il.Emit(OpCodes.Blt, srcIsSmallerLabel);
            _il.Emit(OpCodes.Ldloc, destRemainLocal);
            _il.Emit(OpCodes.Br, minDoneLabel);
            _il.MarkLabel(srcIsSmallerLabel);
            _il.Emit(OpCodes.Ldloc, srcRemainLocal);
            _il.MarkLabel(minDoneLabel);
            _il.Emit(OpCodes.Stloc, countLocal);
        }

        _il.Emit(OpCodes.Ldloc, srcInfo.Local);
        _il.Emit(OpCodes.Ldloc, srcStartLocal);
        _il.Emit(OpCodes.Ldloc, destInfo.Local);
        _il.Emit(OpCodes.Ldloc, destStartLocal);
        _il.Emit(OpCodes.Ldloc, countLocal);
        _il.Emit(OpCodes.Call, _types.ArrayCopyWithOffsets);
    }

    private void EmitAFillStatement(AFillStmt stmt)
    {
        var (info, _) = ResolveArrayStmtTarget(stmt.Array, "AFILL");

        var startLocal = _il.DeclareLocal(_types.Int32);
        if (stmt.Start is not null) EmitInt32Arg(stmt.Start);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, startLocal);

        var endLocal = _il.DeclareLocal(_types.Int32);
        if (stmt.Count is not null)
        {
            _il.Emit(OpCodes.Ldloc, startLocal);
            EmitInt32Arg(stmt.Count);
            _il.Emit(OpCodes.Add);
        }
        else
        {
            _il.Emit(OpCodes.Ldloc, info.Local);
            _il.Emit(OpCodes.Ldlen);
            _il.Emit(OpCodes.Conv_I4);
        }
        _il.Emit(OpCodes.Stloc, endLocal);

        var valueLocal = _il.DeclareLocal(DataTypeMapper.Map(info.ElementDataType, _types, _classTypes));
        EmitExpression(stmt.Value);
        // Conversione del valore al tipo dell'elemento (AFILL 9 su array INTEGER:
        // il letterale è DOUBLE, stelem.i2 richiede I2 — prima era un F8 crudo → IL invalido).
        var fillValType = NormalizeInferredType(InferType(stmt.Value));
        IlConversion.EmitConvert(_il, _types, fillValType,
            DataTypeMapper.Map(fillValType, _types, _classTypes),
            DataTypeMapper.Map(info.ElementDataType, _types, _classTypes));
        _il.Emit(OpCodes.Stloc, valueLocal);

        var iLocal = _il.DeclareLocal(_types.Int32);
        _il.Emit(OpCodes.Ldloc, startLocal);
        _il.Emit(OpCodes.Stloc, iLocal);

        var loopStart = _il.DefineLabel();
        var loopEnd = _il.DefineLabel();
        _il.MarkLabel(loopStart);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldloc, endLocal);
        _il.Emit(OpCodes.Bge, loopEnd);

        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldloc, valueLocal);
        EmitArrayElementStore(info.ElementDataType);

        _il.Emit(OpCodes.Ldloc, iLocal);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Stloc, iLocal);
        _il.Emit(OpCodes.Br, loopStart);
        _il.MarkLabel(loopEnd);
    }

    private void EmitADelStatement(ADelStmt stmt)
    {
        var (info, symbolId) = ResolveArrayStmtTarget(stmt.Array, "ADEL");
        var elementType = info.ClrArrayType.GetElementType()!;

        var positionLocal = _il.DeclareLocal(_types.Int32);
        EmitInt32Arg(stmt.PositionExpr);
        _il.Emit(OpCodes.Stloc, positionLocal);

        var oldArrayLocal = _il.DeclareLocal(info.ClrArrayType);
        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Stloc, oldArrayLocal);

        var newLenLocal = _il.DeclareLocal(_types.Int32);
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Sub);
        _il.Emit(OpCodes.Stloc, newLenLocal);

        var newArrayLocal = _il.DeclareLocal(info.ClrArrayType);
        _il.Emit(OpCodes.Ldloc, newLenLocal);
        _il.Emit(OpCodes.Newarr, elementType);
        _il.Emit(OpCodes.Stloc, newArrayLocal);

        // Array.Copy(old, 0, new, 0, posizione)
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Call, _types.ArrayCopyWithOffsets);

        // Array.Copy(old, posizione+1, new, posizione, newLen - posizione)
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Ldloc, newLenLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Sub);
        _il.Emit(OpCodes.Call, _types.ArrayCopyWithOffsets);

        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Stloc, info.Local);

        EmitArrayParamWriteback(symbolId, info.Local);
    }

    private void EmitAInsStatement(AInsStmt stmt)
    {
        var (info, symbolId) = ResolveArrayStmtTarget(stmt.Array, "AINS");
        var elementType = info.ClrArrayType.GetElementType()!;

        var positionLocal = _il.DeclareLocal(_types.Int32);
        EmitInt32Arg(stmt.PositionExpr);
        _il.Emit(OpCodes.Stloc, positionLocal);

        var oldArrayLocal = _il.DeclareLocal(info.ClrArrayType);
        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Stloc, oldArrayLocal);

        var oldLenLocal = _il.DeclareLocal(_types.Int32);
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Stloc, oldLenLocal);

        var newArrayLocal = _il.DeclareLocal(info.ClrArrayType);
        _il.Emit(OpCodes.Ldloc, oldLenLocal);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Newarr, elementType);
        _il.Emit(OpCodes.Stloc, newArrayLocal);

        // Array.Copy(old, 0, new, 0, posizione)
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Call, _types.ArrayCopyWithOffsets);

        // Array.Copy(old, posizione, new, posizione+1, oldLen - posizione)
        _il.Emit(OpCodes.Ldloc, oldArrayLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Ldloc, oldLenLocal);
        _il.Emit(OpCodes.Ldloc, positionLocal);
        _il.Emit(OpCodes.Sub);
        _il.Emit(OpCodes.Call, _types.ArrayCopyWithOffsets);

        _il.Emit(OpCodes.Ldloc, newArrayLocal);
        _il.Emit(OpCodes.Stloc, info.Local);

        EmitArrayParamWriteback(symbolId, info.Local);
    }

    private void EmitAAddStatement(AAddStmt stmt)
    {
        var (info, symbolId) = ResolveArrayStmtTarget(stmt.Array, "AADD");
        var elementType = info.ClrArrayType.GetElementType()!;

        _il.Emit(OpCodes.Ldloca, info.Local);
        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Add);
        _il.Emit(OpCodes.Call, _types.ArrayResizeOpenGeneric.MakeGenericMethod(elementType));

        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Ldloc, info.Local);
        _il.Emit(OpCodes.Ldlen);
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Sub);
        EmitExpression(stmt.Value);
        // Coerce al tipo CLR dell'elemento prima dello store: un `AS INTEGER` (Int16) riceve
        // un letterale double/val → senza conv.i2 lo `stelem.i2` scriveva 0 (bug AADD su
        // DIM e() AS INTEGER: e(0)=0 invece di 5). Stessa coercizione dei siti di
        // assegnazione (EmitArrayElementStore(target, emitValue)).
        EmitCoerceToElementClr(info.ElementDataType);
        EmitArrayElementStore(info.ElementDataType);

        EmitArrayParamWriteback(symbolId, info.Local);
    }

    private void EmitArrayParamWriteback(int symbolId, LocalBuilder local)
    {
        if (_arrayParamWriteback.TryGetValue(symbolId, out var argIndex))
        {
            _il.Emit(OpCodes.Ldarg, argIndex);
            _il.Emit(OpCodes.Ldloc, local);
            _il.Emit(OpCodes.Stind_Ref);
        }
    }

    private void EmitAScanCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: ASCAN richiede 2 argomenti (array, valore_o_predicato).");
        var info = ResolveArrayArg(expr.Args[0], "ASCAN");
        var elementType = info.ClrArrayType.GetElementType()!;
        _il.Emit(OpCodes.Ldloc, info.Local);
        EmitExpression(expr.Args[1]);
        if (IsCallbackArg(expr.Args[1]))
            _il.Emit(OpCodes.Call, ArrayHelpersAScanPredicateMethod.MakeGenericMethod(elementType));
        else
            _il.Emit(OpCodes.Call, ArrayHelpersAScanValueMethod.MakeGenericMethod(elementType));
    }

    private void EmitACountCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: ACOUNT richiede 2 argomenti (array, conditionFunc).");
        var info = ResolveArrayArg(expr.Args[0], "ACOUNT");
        var elementType = info.ClrArrayType.GetElementType()!;
        _il.Emit(OpCodes.Ldloc, info.Local);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, ArrayHelpersACountMethod.MakeGenericMethod(elementType));
    }

    private void EmitAReduceCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AREDUCE richiede 2 o 3 argomenti (array, reduceFunc, [initialValue]).");
        // T del generico = tipo del SECONDO parametro della callback (l'ELEMENTO; il primo
        // è l'accumulatore TAcc — FN(acc AS INTEGER, el AS INTEGER) su array double: il
        // delegate è Func<Int16,Int16,Int16> — AReduce<double,...> era IL invalido);
        // l'array va convertito (ConvertArray) come per AMAP/AFILTER.
        var reduceParamClr = GetCallbackParamClrType(expr.Args[1], parameterIndex: 1);
        var elementType = reduceParamClr ?? InferArrayOrListElementClrType(expr.Args[0]);
        EmitArrayArgAsCallbackParamType(expr.Args[0], elementType, reduceParamClr is not null);
        EmitExpression(expr.Args[1]);
        if (expr.Args.Length == 3)
        {
            // TAcc coerente col CALLBACK (firma CLR della FUNCTION/LAMBDA, es. CURRENCY →
            // decimal), mai col letterale initial (0 → double): il delegate costruito da
            // EmitFunctionReference/EmitLambdaCreation ha il TAcc del callback — se
            // l'helper usasse il TAcc del letterale avremmo un castclass tra delegate
            // incompatibili (Func<double,...> vs Func<decimal,...>) → IL invalido.
            DataType accDataType;
            try { accDataType = GetCallbackReturnType(expr.Args[1]); }
            catch (NotSupportedException) { accDataType = InferType(expr.Args[2]); }
            var accClr = DataTypeMapper.Map(NormalizeInferredType(accDataType), _types, _classTypes);
            EmitExpression(expr.Args[2]);
            var initType = NormalizeInferredType(InferType(expr.Args[2]));
            IlConversion.EmitConvert(_il, _types, initType, DataTypeMapper.Map(initType, _types, _classTypes), accClr);
            _il.Emit(OpCodes.Call, ArrayHelpersAReduceMethod.MakeGenericMethod(elementType, accClr));
        }
        else
        {
            _il.Emit(OpCodes.Call, ArrayHelpersAReduceNoInitialMethod.MakeGenericMethod(elementType));
        }
    }

    private void EmitAFilterCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AFILTER richiede 2 argomenti (array, filterFunc).");
        var callbackParamClrF = GetCallbackParamClrType(expr.Args[1]);
        var filterElemClr = callbackParamClrF ?? InferArrayOrListElementClrType(expr.Args[0]);
        EmitArrayArgAsCallbackParamType(expr.Args[0], filterElemClr, callbackParamClrF is not null);
        EmitExpression(expr.Args[1]);
        // Predicato BOOLEANO (BIT/BOOLEAN → Func<T,bool>) vs NUMERICO (0/1, es.
        // `AS INTEGER => (n MOD 2) = 0` → Func<T, short>): AFilterNumeric testa != 0.
        var callbackReturnClr = DataTypeMapper.Map(GetCallbackReturnType(expr.Args[1]), _types, _classTypes);
        if (callbackReturnClr == _types.Boolean)
            _il.Emit(OpCodes.Call, ArrayHelpersAFilterMethod.MakeGenericMethod(filterElemClr));
        else
            _il.Emit(OpCodes.Call, ArrayHelpersAFilterNumericMethod.MakeGenericMethod(filterElemClr, callbackReturnClr));
    }

    private Type InferArrayOrListElementClrType(Expr arg)
    {
        return NormalizeInferredType(InferType(arg)) switch
        {
            ListType lt => DataTypeMapper.Map(lt.ElementType, _types, _classTypes),
            ArrayType at => DataTypeMapper.Map(at.ElementType, _types, _classTypes),
            var other => throw new NotSupportedException($"Backend IL: atteso array/lista come argomento, trovato '{other}'.")
        };
    }

    private void EmitArrayArgAsCallbackParamType(Expr arg, Type targetElemClr, bool forceConvert)
    {
        var argType = NormalizeInferredType(InferType(arg));
        Type? elemClr = argType switch
        {
            ListType lt => DataTypeMapper.Map(lt.ElementType, _types, _classTypes),
            ArrayType at => DataTypeMapper.Map(at.ElementType, _types, _classTypes),
            _ => null
        };
        if (arg is VariableExpr veArr && _arrays.TryGetValue(veArr.SymbolId, out var arrInfo))
        {
            _il.Emit(OpCodes.Ldloc, arrInfo.Local);
            elemClr = arrInfo.ClrArrayType.GetElementType()!;
        }
        else if (argType is ListType listType)
        {
            EmitExpression(arg);
            var listClr = DataTypeMapper.Map(listType, _types, _classTypes);
            var toArrayM = ClosedGenericContainsTypeBuilder(listClr)
                ? TypeBuilder.GetMethod(listClr, listClr.GetGenericTypeDefinition().GetMethod("ToArray", Type.EmptyTypes)!)
                : listClr.GetMethod("ToArray", Type.EmptyTypes);
            _il.Emit(OpCodes.Callvirt, toArrayM ?? throw new NotSupportedException("Backend IL: List<T>.ToArray non trovato."));
        }
        else
        {
            EmitExpression(arg);
        }
        if (forceConvert && elemClr is not null && elemClr != targetElemClr)
            _il.Emit(OpCodes.Call, ArrayHelpersConvertArrayMethod.MakeGenericMethod(elemClr, targetElemClr));
    }

    private void EmitAMapCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AMAP richiede 2 argomenti (array, mapFunc).");
        // T del generico = tipo del PARAMETRO della callback (es. FN(n AS INTEGER) su un
        // array double: il delegate è Func<Int16,...> — AMap<double,...> con quel delegate
        // era IL invalido/overflow). L'array viene convertito al tipo del parametro.
        var callbackParamClr = GetCallbackParamClrType(expr.Args[1]);
        var resultElementType = DataTypeMapper.Map(GetCallbackReturnType(expr.Args[1]), _types, _classTypes);
        var mapElemClr = callbackParamClr ?? InferArrayOrListElementClrType(expr.Args[0]);
        EmitArrayArgAsCallbackParamType(expr.Args[0], mapElemClr, callbackParamClr is not null);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, ArrayHelpersAMapMethod.MakeGenericMethod(mapElemClr, resultElementType));
    }

    /// <summary>AEVAL(array, actionFunc) in forma di ESPRESSIONE (`ARRAYS.AEVAL(a, lambda)`
    /// come ExpressionStmt/BuiltinFunctionCallExpr — DIVERSA dalla forma statement dedicata
    /// `AEVAL a, lambda` che passa già da EmitAEvalStatement/ArrayHelpers.AEval&lt;T&gt; reale).
    /// Prima di questo fix la forma-espressione cadeva su EmitArrayTypedCall →
    /// IlRuntime.AEval, uno STUB che clona l'array senza mai invocare la callback (no-op
    /// silenzioso — Bugs/(open) - IL-AEVAL-Stub-Non-Invoca-Azione.md). Stesso pattern di
    /// EmitAMapCall/EmitAFilterCall (risoluzione array-per-tipo-parametro-callback), ma con
    /// ArrayHelpers.AEval&lt;T&gt; (void, vero side-effect) invece di AMap&lt;T,TR&gt;: l'array
    /// viene duplicato PRIMA della call così la copia originale resta sullo stack come valore
    /// di ritorno (AEVAL in StdLib è dichiarata ArrayType — parità Clipper, che ritorna
    /// l'array per il chaining).</summary>
    private void EmitAEvalCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AEVAL richiede 2 argomenti (array, actionFunc).");
        var callbackParamClr = GetCallbackParamClrType(expr.Args[1]);
        var elemClr = callbackParamClr ?? InferArrayOrListElementClrType(expr.Args[0]);
        EmitArrayArgAsCallbackParamType(expr.Args[0], elemClr, callbackParamClr is not null);
        _il.Emit(OpCodes.Dup);
        EmitExpression(expr.Args[1]);
        if (GetCallbackArity(expr.Args[1]) == 2)
            _il.Emit(OpCodes.Call, ArrayHelpersAEvalIndexedMethod.MakeGenericMethod(elemClr));
        else
            _il.Emit(OpCodes.Call, ArrayHelpersAEvalMethod.MakeGenericMethod(elemClr));
    }

    private void EmitATokensCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: ATOKENS richiede 1 o 2 argomenti (stringa, [delimitatori]).");
        EmitExpression(expr.Args[0]);
        if (expr.Args.Length == 2) EmitExpression(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, ArrayHelpersATokensMethod);
    }

    private void EmitAPadCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: APAD richiede 2 o 3 argomenti (array, nuovaLunghezza, [padValue]).");
        var info = ResolveArrayArg(expr.Args[0], "APAD");
        _il.Emit(OpCodes.Ldloc, info.Local);
        EmitInt32Arg(expr.Args[1]);
        if (expr.Args.Length == 3)
        {
            EmitExpression(expr.Args[2]);
            var padValueClrType = DataTypeMapper.Map(InferType(expr.Args[2]), _types, _classTypes);
            if (padValueClrType.IsValueType) _il.Emit(OpCodes.Box, padValueClrType);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, ArrayUtilsPadMethod);
        _il.Emit(OpCodes.Castclass, info.ClrArrayType);
    }

    private void EmitChunkCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: ACHUNK richiede 2 argomenti (array, dimensione blocco).");
        // Argomenti boxed: il rewrite del Linker può convertire l'argomento numerico in
        // int64 (RewriteArgumentsWithContext) — firma (object, object) con convert runtime.
        EmitObjectBoxedArgument(expr.Args[0]);
        EmitObjectBoxedArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, IlRuntimeChunkMethod);
    }

    private void EmitArrayUtilsArrayOnlyCall(BuiltinFunctionCallExpr expr, string name, MethodInfo utilsMethod)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {name} richiede esattamente 1 argomento.");
        var info = ResolveArrayArg(expr.Args[0], name);
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Call, utilsMethod);
        _il.Emit(OpCodes.Castclass, info.ClrArrayType);
    }

    /// <summary>AFLAT(array): a differenza di AJOIN/APAD/ecc. (via
    /// EmitArrayUtilsArrayOnlyCall/ResolveArrayArg, che RICHIEDE Rank==1), AFLAT accetta
    /// esplicitamente array multi-rank/jagged — è l'UNICA funzione il cui scopo è proprio
    /// appiattirli. Riusare ResolveArrayArg l'avrebbe sempre rifiutata su un letterale
    /// annidato ({{1,2},{3,4},{5,6}}, Rank 2) con "supporta solo array mono-dimensionali".
    /// Anche il Castclass finale deve differire: ArrayUtils.Flatten ritorna un array FLAT
    /// (rank 1, tipo elemento FOGLIA — es. double[] per un input double[][]), mai
    /// info.ClrArrayType (che per l'array sorgente è il tipo JAGGED originale, es.
    /// double[][] — un cast lì fallirebbe: il risultato piatto non è quel tipo).</summary>
    private void EmitAFlatCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: AFLAT richiede esattamente 1 argomento.");
        if (expr.Args[0] is not VariableExpr arrayVarExpr || !_arrays.TryGetValue(arrayVarExpr.SymbolId, out var info))
            throw new NotSupportedException("Backend IL: AFLAT supporta solo un array locale/globale DIMensionato direttamente (non un campo, non un'espressione generica).");
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Call, ArrayUtilsFlattenMethod);
        var flatClrType = DataTypeMapper.Map(info.ElementDataType, _types, _classTypes).MakeArrayType();
        _il.Emit(OpCodes.Castclass, flatClrType);
    }

    private void EmitAJoinCall(BuiltinFunctionCallExpr expr)
    {
        // AJOIN(array [, sep]): il 1° arg può essere un array DIMensionato (ResolveArrayArg)
        // O un'espressione che ritorna un array (es. list.ToArray() — fluent) — in quel caso
        // EmitExpression diretto e ArrayUtils.Join(IEnumerable, string).
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AJOIN richiede 1 o 2 argomenti (array, [separatore]).");
        if (expr.Args[0] is VariableExpr ve0 && _arrays.TryGetValue(ve0.SymbolId, out var info0))
        {
            if (info0.Rank != 1)
                throw new NotSupportedException("Backend IL: AJOIN supporta solo array mono-dimensionali.");
            EmitArrayRefFromInfo(info0);
        }
        else
        {
            EmitExpression(expr.Args[0]);
        }
        if (expr.Args.Length == 2) EmitExpression(expr.Args[1]);
        else _il.Emit(OpCodes.Ldstr, ",");
        _il.Emit(OpCodes.Call, ArrayUtilsJoinMethod);
    }

    private void EmitABinarySearchCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: ABINARYSEARCH richiede 2 argomenti (array, valore).");
        var info = ResolveArrayArg(expr.Args[0], "ABINARYSEARCH");
        EmitArrayRefFromInfo(info);
        EmitExpression(expr.Args[1]);
        var valueClrType = DataTypeMapper.Map(InferType(expr.Args[1]), _types, _classTypes);
        if (valueClrType.IsValueType) _il.Emit(OpCodes.Box, valueClrType);
        _il.Emit(OpCodes.Call, ArrayUtilsBinarySearchMethod);
        _il.Emit(OpCodes.Conv_I8); // int -> Long, vedi dichiarazione StdLib (ABINARYSEARCH: LongType)
    }

    private void EmitArrayUtilsMutateInPlace(BuiltinFunctionCallExpr expr, string name, MethodInfo utilsMutateMethod)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {name} richiede esattamente 1 argomento.");
        var info = ResolveArrayArg(expr.Args[0], name);
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Dup);
        _il.Emit(OpCodes.Call, utilsMutateMethod);
    }

    private void EmitArrayUtilsCloneThenMutate(BuiltinFunctionCallExpr expr, string name, MethodInfo utilsMutateMethod)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {name} richiede esattamente 1 argomento.");
        var info = ResolveArrayArg(expr.Args[0], name);
        var elementType = info.ClrArrayType.GetElementType()!;
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Call, ArrayHelpersACloneMethod.MakeGenericMethod(elementType));
        _il.Emit(OpCodes.Dup);
        _il.Emit(OpCodes.Call, utilsMutateMethod);
    }

    private void EmitArrayGenericCall(BuiltinFunctionCallExpr expr, string name, MethodInfo openGenericMethod, bool sameElementType)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {name} richiede esattamente 1 argomento.");
        var info = ResolveArrayArg(expr.Args[0], name);
        var elementType = info.ClrArrayType.GetElementType()!;
        EmitArrayRefFromInfo(info);
        _il.Emit(OpCodes.Call, openGenericMethod.MakeGenericMethod(elementType));
    }

    /// <summary>Call a un metodo ARRAYS.* non generico (AAdd/AIns/ADel/AFill/ACopy/AEval che
    /// accettano System.Array e ritornano System.Array): castclass degli argomenti array a
    /// Array, box del valore, e Castclass del ritorno al tipo originale — pattern EmitAPadCall.</summary>
    private void EmitArrayDeclaration(VariableDeclaration declaration)
    {
        var symbol = _program.Symbols.GetSymbol(declaration.SymbolId);
        if (symbol.DataType is not ArrayType symbolArrayType)
            throw new InvalidOperationException($"Backend IL: tipo array atteso per il simbolo '{declaration.NameCleaned.Lexeme}'.");

        var rank = declaration.Dimensions.Length;
        var elementDataType = symbolArrayType.ElementType;
        var elementClrType = DataTypeMapper.Map(elementDataType, _types, _classTypes);

        // Solo il LOWER BOUND resta vincolato a un intero noto a compile-time: è un int[]
        // "congelato" in ArrayInfo.LowerBounds e riletto ovunque nell'indicizzazione
        // (EmitLoadArrayRefAndIndices) — renderlo dinamico richiederebbe portare un
        // LocalBuilder invece di un int in tutti quei siti (fuori scope qui, ed è comunque
        // un caso rarissimo: DIM A(N TO M) con N variabile non è un idioma BASIC comune).
        // Il LIMITE SUPERIORE (quindi la lunghezza dell'asse) puo' invece essere
        // un'espressione RUNTIME qualunque (es. DIM A(N) dopo un INPUT) — vedi EmitAxisLength.
        var lowerBounds = new int[rank];
        for (var axis = 0; axis < rank; axis++)
        {
            lowerBounds[axis] = declaration.LowerBounds.Length > axis ? ExtractCompileTimeInt32(declaration.LowerBounds[axis]) : 0;
        }

        Type clrArrayType;
        LocalBuilder arrayLocal;

        if (rank == 1)
        {
            clrArrayType = elementClrType.MakeArrayType();
            // Stesso LocalBuilder di GetOrDeclareLocal: in stepper GPM i locali CIL si
            // azzerano a ogni tick — SaveGpmLocalsToFrame persiste solo `_locals`.
            // DeclareLocal qui creava un secondo slot, l'array restava null al resume
            // (SpritesExamples: SPRITE LOAD su SprID(I) → NRE).
            arrayLocal = GetOrDeclareLocal(declaration.SymbolId);
            EmitAxisLength(declaration.Dimensions[0], lowerBounds[0]);
            // Array di CLASS (reference type): gli elementi vanno creati — `new T[n]` li
            // lascia null e `arr(0).Campo = ...` darebbe NullReferenceException (il VM
            // materializza gli elementi). NewInstanceArray<T> fa new T() per ogni slot.
            // Gli UDT (struct, base ValueType) nascono già allocati: restano da
            // inizializzare i campi STRINGA a "" (default BASIC — il CLR li lascia null,
            // e TRIM(P.Nome) su null darebbe NullReferenceException).
            if (!elementClrType.IsValueType && !elementClrType.IsPrimitive
                && elementClrType != _types.String && elementClrType != _types.Object && !elementClrType.IsArray)
            {
                _il.Emit(OpCodes.Call, ArrayHelpersNewInstanceArrayMethod.MakeGenericMethod(elementClrType));
                _il.Emit(OpCodes.Stloc, arrayLocal);
            }
            else if (elementClrType == _types.String)
            {
                // DIM A(n) AS STRING: `new string[n]` lascerebbe ogni elemento null (il CLR
                // non ha un default "" per i reference type) — a differenza di uno scalare
                // STRING, già inizializzato a "" altrove in questo backend. Un elemento mai
                // assegnato e poi letto da LEN/TRIM/concatenazione darebbe NullReferenceException
                // (visto dal vivo: APPS/Masume — DIM arr(8) AS STRING poi LEN(arr(1))).
                _il.Emit(OpCodes.Call, ArrayHelpersNewStringArrayMethod);
                _il.Emit(OpCodes.Stloc, arrayLocal);
            }
            else
            {
                _il.Emit(OpCodes.Newarr, elementClrType);
                _il.Emit(OpCodes.Stloc, arrayLocal);
            }

            // Inizializzatore letterale: sia `{..}` (ArrayLiteralExpr) sia `[..]`
            // (ListLiteralExpr — forma ammessa per gli array).
            var initLitElems = declaration.InitialValue switch
            {
                ArrayLiteralExpr al => al.Elements,
                ListLiteralExpr ll => ll.Elements,
                _ => default(ImmutableArray<Expr>?),
            };
            if (initLitElems is { } literalElems)
            {
                for (var i = 0; i < literalElems.Length; i++)
                {
                    _il.Emit(OpCodes.Ldloc, arrayLocal);
                    _il.Emit(OpCodes.Ldc_I4, i);
                    EmitExpression(literalElems[i]);
                    // Coerce al tipo dell'elemento: un elemento `[...]` non è avvolto in
                    // CONVERSIONS.CLNG come nel `{...}` (es. DIM b() AS INTEGER = [2,10,1] ha
                    // elementi DOUBLE) → senza conv lo store in un Int16[]/Int64[] depositava i
                    // BIT del double (LONG dava 4.6e18, INTEGER dava 0). Stesso schema del path 2D.
                    EmitCoerceTypes(NormalizeInferredType(InferType(literalElems[i])), elementDataType);
                    EmitArrayElementStore(elementDataType);
                }
            }
        }
        else if (elementClrType is TypeBuilder)
        {
            // Elemento CLASS/UDT (TypeBuilder non finalizzato): il costruttore rettangolare
            // T[,...] non è risolvibile in Reflection.Emit ("Not supported in a non-reflected
            // type") → array JAGGED (T[][], ...) creato a runtime da IlRuntime.MakeJagged
            // (sotto-array FRESCHI per slot, mai aliasati). Stessa struttura jagged già gestita
            // in lettura/scrittura (EmitLoadArrayRefAndIndices: isJagged/ldelem.ref).
            clrArrayType = elementClrType;
            for (var i = 0; i < rank; i++) clrArrayType = clrArrayType.MakeArrayType();
            arrayLocal = _il.DeclareLocal(clrArrayType);
            _il.Emit(OpCodes.Ldtoken, elementClrType);
            _il.Emit(OpCodes.Call, typeof(Type).GetMethod(nameof(Type.GetTypeFromHandle))!);
            _il.Emit(OpCodes.Ldc_I4, rank);
            _il.Emit(OpCodes.Newarr, _types.Int32);
            for (var axis = 0; axis < rank; axis++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, axis);
                EmitAxisLength(declaration.Dimensions[axis], lowerBounds[axis]);
                _il.Emit(OpCodes.Stelem_I4);
            }
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MakeJagged), typeof(Type), typeof(int[])));
            _il.Emit(OpCodes.Castclass, clrArrayType);
            _il.Emit(OpCodes.Stloc, arrayLocal);
        }
        else
        {
            clrArrayType = elementClrType.MakeArrayType(rank);
            var ctor = clrArrayType.GetConstructor(Enumerable.Repeat(_types.Int32, rank).ToArray())
                ?? throw new InvalidOperationException($"Backend IL: costruttore non trovato per l'array rettangolare '{clrArrayType}'.");
            arrayLocal = _il.DeclareLocal(clrArrayType);
            for (var axis = 0; axis < rank; axis++)
                EmitAxisLength(declaration.Dimensions[axis], lowerBounds[axis]);
            _il.Emit(OpCodes.Newobj, ctor);
            _il.Emit(OpCodes.Stloc, arrayLocal);

            // Inizializzatore letterale multi-dim: `{{1,2},{3,4}}` (ArrayLiteralExpr) oppure
            // `[[1,2],[3,4]]` (ListLiteralExpr). Il Linker APPIATTISCE il letterale annidato in
            // un'unica sequenza FLAT row-major (Elements = {1,2,3,4,...}), stessa rappresentazione
            // consumata da OP_INIT_ARRAY nel p-code Compiler (Compiler.Assign.cs) — quindi serve
            // solo la decomposizione flat-index → (i0,i1,...) row-major, non una ricorsione.
            var multiDimElems = declaration.InitialValue switch
            {
                ArrayLiteralExpr al => al.Elements,
                ListLiteralExpr ll => ll.Elements,
                _ => default(ImmutableArray<Expr>?),
            };
            if (multiDimElems is { } multiDimLiteral)
            {
                var setMethod = clrArrayType.GetMethod("Set")!;
                var axisLengths = new int[rank];
                for (var axis = 0; axis < rank; axis++)
                    axisLengths[axis] = ExtractCompileTimeInt32(declaration.Dimensions[axis]) - lowerBounds[axis] + 1;

                for (var flatIndex = 0; flatIndex < multiDimLiteral.Length; flatIndex++)
                {
                    _il.Emit(OpCodes.Ldloc, arrayLocal);
                    // Decomposizione flat-index → (i0,i1,...) row-major: l'asse PIÙ a destra
                    // varia più velocemente (ultima dimensione = colonne). Calcolato in un array
                    // temporaneo (non pushato via via) perché gli indici vanno sullo stack in
                    // ordine i0,i1,...,iN-1, mentre la decomposizione naturale (resto/divisione)
                    // produce prima l'ultimo asse.
                    var idxAtAxis = new int[rank];
                    var remaining = flatIndex;
                    for (var axis = rank - 1; axis >= 0; axis--)
                    {
                        idxAtAxis[axis] = remaining % axisLengths[axis];
                        remaining /= axisLengths[axis];
                    }
                    for (var axis = 0; axis < rank; axis++)
                        _il.Emit(OpCodes.Ldc_I4, lowerBounds[axis] + idxAtAxis[axis]);
                    EmitExpression(multiDimLiteral[flatIndex]);
                    // Coerce al tipo dell'elemento (DIM m(1,1) AS INTEGER = {{1,2},{3,4}}:
                    // il letterale è double → Set(int32,int32,int16) senza conv.i2 =
                    // InvalidProgram "Expected I4, but got F8").
                    EmitCoerceTypes(NormalizeInferredType(InferType(multiDimLiteral[flatIndex])), elementDataType);
                    _il.Emit(OpCodes.Call, setMethod);
                }
            }
        }

        // Array GLOBALE/STATIC con dimensioni ESPLICITE: il valore creato qui vive nel local
        // del metodo di init-globali, MA il resto del programma è compilato da un'ALTRA
        // istanza del compilatore (_arrays per-istanza, vedi RegisterDynamicArrayLocal) →
        // senza scrivere nel bag il globale restava irraggiungibile in ogni altra procedura
        // ("accesso a un array non DIMensionato"). Scriviamo nel bag e registriamo un
        // LoadRef, così anche le altre istanze lo caricano via LoadGlobal/LoadStatic.
        Action? bagLoadRef = null;
        if (IsStaticSymbol(declaration.SymbolId))
        {
            _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
            _il.Emit(OpCodes.Ldloc, arrayLocal);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreStatic), typeof(int), typeof(object)));
            bagLoadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadStatic), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        else if (IsGlobalSymbol(declaration.SymbolId))
        {
            _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
            _il.Emit(OpCodes.Ldloc, arrayLocal);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            bagLoadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        _arrays[declaration.SymbolId] = bagLoadRef is null
            ? new ArrayInfo(arrayLocal, clrArrayType, elementDataType, rank, lowerBounds)
            : new ArrayInfo(arrayLocal, clrArrayType, elementDataType, rank, lowerBounds, bagLoadRef);
    }

    private void RegisterDynamicArrayLocal(VariableDeclaration declaration)
    {
        if (_program.Symbols.GetSymbol(declaration.SymbolId).DataType is not ArrayType arrayType) return;
        // rank = profondità dell'ArrayType annidato (ARRAY OF ARRAY OF DOUBLE = 2) — prima era
        // fisso a 1: 'DIM FftRes() = FFT(...)' (2D) falliva con 'indici 2 != rank 1'.
        int rank = 0;
        DataType elementType = arrayType;
        while (elementType is ArrayType inner) { rank++; elementType = inner.ElementType; }
        // ACHUNK/AZIP ritornano object?[] a runtime (ogni elemento è un SOTTO-ARRAY/coppia,
        // non un valore omogeneo): mappare l'InferredType interno a Double (il default
        // generico sotto, corretto per AMAP/AFILTER/ecc. il cui elemento È scalare) darebbe
        // ClrArrayType = double[], e il Castclass double[] nel loader (LoadRef) di questo
        // array GLOBALE/STATIC farebbe InvalidCastException su un valore realmente object[]
        // — stesso bug del cast in EmitCoerceToTarget (vedi ObjectArrayReturningBuiltins lì),
        // stessa causa: ArraysUncoveredCoverage.bas.
        var elementClrType = declaration.InitialValue is BuiltinFunctionCallExpr objArrDecl
            && ObjectArrayReturningBuiltins.Contains(objArrDecl.Name.Kind)
            ? _types.Object
            : DataTypeMapper.Map(elementType, _types, _classTypes);
        // ClrArrayType JAGGED per il rank (double[][] per rank 2) — coerente con la local
        // (Map ricorsivo) e con gli ldelem.ref intermedi del caricamento.
        var clrArrayType = elementClrType;
        for (var i = 0; i < rank; i++) clrArrayType = clrArrayType.MakeArrayType();
        var local = GetOrDeclareLocal(declaration.SymbolId);
        // Array STATIC o GLOBALE: il riferimento vive nel bag (StoreStatic/StoreGlobal —
        // vedi EmitStoreVariable, che per un simbolo GLOBALE/STATIC non scrive MAI nel local
        // qui dichiarato), non nel local — il loader legge dal bag, non da un local che
        // resterebbe sempre {0}/mai scritto (bug: `DIM t = FROM n IN nums ... SELECT n`
        // globale + `t.Join(",")` leggeva un local mai popolato → NullReferenceException/
        // InvalidProgramException, perché il valore del DIM viene salvato SOLO con
        // StoreGlobal quando la dichiarazione non ha Dimensions esplicite — vedi
        // EmitScalarDeclaration — e questo local non viene mai coinvolto in scrittura).
        Action? loadRef = null;
        if (IsStaticSymbol(declaration.SymbolId))
        {
            loadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadStatic), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        else if (IsGlobalSymbol(declaration.SymbolId))
        {
            loadRef = () =>
            {
                _il.Emit(OpCodes.Ldc_I4, declaration.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
                _il.Emit(OpCodes.Castclass, clrArrayType);
            };
        }
        _arrays[declaration.SymbolId] = new ArrayInfo(local, clrArrayType, elementType, rank, new int[rank], loadRef);
    }

}
