// FILE: IlBackend/Partial/IlProgramCompiler.BuiltinsMath.cs
// PARTIAL — dominio BUILTIN MATH/CONVERSION (len/ubound/lbound/array-utils/conversion).
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

/// <summary>Partial del transpiler IL: dominio BUILTIN MATH/CONVERSION.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitInt32Arg(Expr expr)
    {
        var argType = InferType(expr);
        // RGBA/DisplayMode (double[4]) usati come colore: converti [r,g,b,a] → 0xRRGGBBAA
        // via IlRuntime.RgbaToColorInt (COLOR fg, bg / PSET con un argomento RGBA).
        if (NormalizeInferredType(argType) is RgbaColorType or DisplayModeType)
        {
            EmitExpression(expr);
            _il.Emit(OpCodes.Call, IlRuntimeRgbaToColorIntMethod);
            return;
        }
        EmitExpression(expr);

        // Delega centralizzata a IlConversion. Mantiene esattamente la semantica
        // pregressa: Double → Convert.ToInt32(double) (arrotonda, semantica
        // dell'Executor esistente); integrale → conv.i4 (niente perdita). Qualunque
        // altro tipo numerico ora NON viene più rifiutato subito ma cade nel
        // dispatcher di IlConversion (widening/convert espliciti), che lancia
        // esplicito se davvero non supportato.
        var clrFrom = DataTypeMapper.Map(argType, _types, _classTypes);
        // Il tipo INFERITO può divergere dal tipo REALE del locale (es. `LOAD TEXTURE ...
        // INTO x`: locale int32 ma simbolo inferito double): senza, Convert::ToInt32 su un
        // int32 = InvalidProgram (GraphicsSpriteBatch, 2026-10-03).
        if (expr is VariableExpr veArg && _locals.TryGetValue(veArg.SymbolId, out var lbArg))
            clrFrom = lbArg.LocalType;
        if (clrFrom == _types.Int32)
            return; // già Int32 sullo stack, nessuna conversione
        IlConversion.EmitConvert(_il, _types, argType, clrFrom, _types.Int32);
    }

    /// <summary>Valuta un'espressione e la lascia come Single (float) sullo stack.</summary>
    private void EmitFloatArg(Expr expr)
    {
        EmitExpression(expr);
        var argType = InferType(expr);
        var clrFrom = DataTypeMapper.Map(argType, _types, _classTypes);
        if (clrFrom == _types.Single)
            return; // già Single
        if (clrFrom == _types.Double)
        {
            _il.Emit(OpCodes.Conv_R4);
            return;
        }
        IlConversion.EmitConvert(_il, _types, argType, clrFrom, _types.Single);
    }

    /// <summary>
    /// CLNG/CINT/ecc.: conversioni numeriche inserite implicitamente dal Linker,
    /// sia sugli argomenti letterali interi delle funzioni builtin (es. il "3" in
    /// LEFT$("BASIC", 3) arriva come CLNG(3), non come NumberExpr grezzo) sia
    /// sulle assegnazioni a variabili/array di tipo più stretto di Double (es.
    /// "LET N%(0) = 10" arriva come CINT(10), dato che N% è Integer/Int16 e il
    /// letterale 10 è Double per costruzione in questo backend). Copertura
    /// attuale: solo argomento Double in ingresso (l'unico caso finora
    /// osservato) — arrotonda via il metodo Convert.ToXxx(double) passato,
    /// stessa semantica dell'Executor esistente (mai troncamento).
    /// </summary>
    private void EmitNumericConversionCall(BuiltinFunctionCallExpr expr, MethodInfo convertMethod)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");

        var argType = InferType(expr.Args[0]);
        // Delega a IlConversion la scelta widening/arrotondamento; il target è derivato
        // dal metodo Convert.ToXxx già risolto dal chiamante (es. Convert.ToInt64 → long,
        // Convert.ToInt16 → short). L'ingresso resta Double (contratto pregresso).
        var targetClr = convertMethod == _types.ConvertToInt64Double ? _types.Int64
            : convertMethod == _types.ConvertToInt16Double ? _types.Int16
            : _types.Int32;

        EmitExpression(expr.Args[0]);
        // CINT(CINT(2)) o CINT(CBYTE(...)): il Linker può avvolgere lo stesso letterale in
        // UNA conversione per OGNI livello di inferenza (RewriteSingleArgument chiamato più
        // volte) -> CINT annidati. Se l'argomento è GIÀ una conversione numerica implicita,
        // la coerce qui sarebbe una SECONDA Convert.ToInt16(double) su un valore già short
        // (il tipo statico dell'argomento resta double) = InvalidProgramException. Saltarla
        // è sicuro: il CInt interno ha già prodotto il valore del tipo target.
        if (expr.Args[0] is not BuiltinFunctionCallExpr { Name.Kind: TokenKind.CInt or TokenKind.CByte or TokenKind.CLng or TokenKind.CDbl or TokenKind.CSng or TokenKind.CCur })
        {
            // Argomento con tipo RIDEFINITO (S/S$): il LOAD pusha object — converte con
            // IlRuntime.ToDouble(object) (gestisce boxed SINGLE/stringa) invece di EmitConvert
            // che si aspetta il tipo statico (Single/String) e produrrebbe IL invalido.
            if (expr.Args[0] is VariableExpr reVar &&
                _redefinedTypeSymbols.Contains(reVar.SymbolId))
            {
                _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
            }
            else
            {
                // Il tipo CLR di PARTENZA è quello REALE dell'argomento, non sempre Double:
                // `CLNG(<variabile INTEGER>)` lasciava un intero sullo stack mentre la
                // conversione assumeva un double → stack type mismatch =
                // InvalidProgramException a runtime. MapType(argType) risolve il tipo vero
                // (Double per i letterali/variabili double, Int16/Int32 per gli interi, ...).
                var clrFrom = DataTypeMapper.Map(argType, _types, _classTypes);
                IlConversion.EmitConvert(_il, _types, argType, clrFrom, targetClr);
            }
        }
    }

    private void EmitNumericConversionTo(BuiltinFunctionCallExpr expr, Type targetClr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");

        var sourceType = NormalizeInferredType(InferType(expr.Args[0]));
        var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
        EmitExpression(expr.Args[0]);
        // CDBL su un SINGLE (float): parità VM — il float va convertito tramite la sua
        // rappresentazione STRINGA (CDblSingle), non esteso (float→double diretto =
        // 1.23456704616547 invece di 1.234567; TypesConversions).
        var cdblSymbolType = expr.Args[0] is VariableExpr cdblVar
            ? NormalizeInferredType(_program.Symbols.GetSymbol(cdblVar.SymbolId).DataType)
            : sourceType;
        // Per un SINGLE redefinito il DataType del simbolo può non essere più SingleType
        // (S$ AS STRING sovrascrive), ma il LOCAL caricato è ancora System.Single — usa il
        // tipo del local se disponibile (più fedele al valore sullo stack).
        if (expr.Args[0] is VariableExpr cdblVar2 && _locals.TryGetValue(cdblVar2.SymbolId, out var cdblLb)
            && cdblLb.LocalType == typeof(float))
            cdblSymbolType = new SingleType();
        if (targetClr == _types.Double && cdblSymbolType is SingleType)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CDblSingle), typeof(float)));
            return;
        }
        // Argomento con tipo RIDEFINITO (S/S$): il LOAD pusha object — converte con
        // IlRuntime.ToDouble(object) (gestisce boxed SINGLE/stringa) invece di EmitConvert
        // che si aspetta il tipo statico (Single/String) e produrrebbe IL invalido.
        // Per CDBL (target double) usa CDblValue: se l'object è un float boxed lo converte
        // tramite la sua STRINGA (1.234567 pulito) come la VM, non esteso (1.23456704616547).
        if (expr.Args[0] is VariableExpr reVar &&
            _redefinedTypeSymbols.Contains(reVar.SymbolId))
        {
            if (targetClr == _types.Double)
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CDblValue), typeof(object)));
            else
                _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
        }
        else
        {
            IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, targetClr);
        }
    }

    private void EmitComparisonNumericSide(Expr side, DataType sideType)
    {
        var nt = NormalizeInferredType(sideType);
        if (nt is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType)
        {
            EmitExpression(side);
            var clr = DataTypeMapper.Map(nt, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDouble), typeof(object)));
        }
        else
        {
            EmitExpression(side);
            // DateTime/TimeSpan vs numero (es. `DATEFROMUNIX(...) <> 0`): il confronto CIL
            // ceq richiede operandi dello STESSO tipo numerico → converto il lato data a
            // double (OADate / secondi) come l'altro lato.
            var ntClr = DataTypeMapper.Map(nt, _types, _classTypes);
            if (ntClr == typeof(DateTime) || ntClr == typeof(System.TimeSpan))
                IlConversion.EmitConvert(_il, _types, nt, ntClr, _types.Double);
            // L'ALTRO lato è dinamico (Any/Json/ERR) → double: allinea anche questo lato a
            // double, altrimenti resta int/long accanto a un double → Add/Sub/Ceq su tipi
            // diversi = IL invalido (bug Compiler-InvalidOperandTypes: una variabile riassegnata
            // di tipo diverso è Any/object → `q - p` con q=ToDouble(object) e p=int64).
            else if (IlConversion.IsNumeric(nt) && ntClr != typeof(double))
                IlConversion.EmitConvert(_il, _types, nt, ntClr, _types.Double);
        }
    }

    /// <summary>Operando in contesto numerico quando uno dei lati è ERR (Basic.ErrInstance):
    /// il lato ERR va emesso come ERR.Number (int32) convertito a double — altrimenti un
    /// oggetto finisce su ceq/clt/cgt accanto a un double = IL invalido (bug `IF ERR = 0`).</summary>
    private void EmitNumericOperandWithErr(Expr e, DataType t)
    {
        if (NormalizeInferredType(t) is ErrType)
        {
            EmitExpression(e);
            _il.Emit(OpCodes.Call, _types.ErrInstanceNumberGetter);
            _il.Emit(OpCodes.Conv_R8);
        }
        else
        {
            EmitDoubleArgument(e);
        }
    }

    private void EmitBinaryExpression(BinaryExpr expr)
    {
        // IS NIL / IS <tipo>: identità e tipo a runtime. Va gestito prima del percorso
        // ordinario dei confronti perché il lato destro è un TypeExpr, non un valore.
        if (expr.Op.Kind == TokenKind.Is)
        {
            if (expr.Right is VariableExpr { NameCleaned.Lexeme: "NIL" } or NilExpr or TypeExpr { Type: NilType })
            {
                EmitExpression(expr.Left);
                // Simbolo redefined (SafeCast/ridefinizione $): il local è GIÀ object boxed.
                var isRedefinedLeft = expr.Left is VariableExpr ivN && _redefinedTypeSymbols.Contains(ivN.SymbolId);
                if (!isRedefinedLeft)
                {
                    var leftClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Left)), _types, _classTypes);
                    if (leftClr.IsValueType) _il.Emit(OpCodes.Box, leftClr);
                }
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ceq);
                return;
            }
            if (expr.Right is TypeExpr typeExpr)
            {
                EmitIsTypeExpression(expr.Left, typeExpr.Type);
                return;
            }
            throw new NotSupportedException($"Backend IL: il lato destro di IS deve essere NIL o un tipo, trovato '{expr.Right}'.");
        }

        var leftType = InferType(expr.Left);
        var rightType = InferType(expr.Right);

        // OPERATOR sovraccaricato: il Linker NON riscrive mai un BinaryExpr con operandi
        // ClassType in una chiamata esplicita (resta un BinaryExpr generico — il p-code
        // Compiler/Executor dispatcha dinamicamente a runtime, vedi Compiler.Expressions.cs
        // OP_ADD ecc.). Questo backend risolve invece staticamente qui, stesso identico
        // algoritmo di ricerca (per TokenKind, risalendo la gerarchia base) usato da
        // Linker.InferTypes.LinkerInferExpressionType per l'inferenza del tipo.
        if (leftType is ClassType leftClassType)
        {
            var leftClassSymbolId = ResolveClassSymbolIdForOperatorLookup(expr.Left) ?? leftClassType.Definition.SymbolId;
            if (FindOperatorInHierarchy(leftClassSymbolId, expr.Op.Kind, 2) is MethodBuilder operatorMethod)
            {
                EmitExpression(expr.Left);
                EmitExpression(expr.Right);
                _il.Emit(OpCodes.Call, operatorMethod);
                return;
            }
        }
        // OPERATOR su UDT (es. `v1 + v2` con Vector2D): stesso dispatch statico, ma il tipo
        // dell'operando sinistro è un UserDefinedType (value type) non un ClassType.
        if (leftType is UserDefinedType leftUdtType)
        {
            var leftUdtSymbolId = ResolveClassSymbolIdForOperatorLookup(expr.Left) ?? leftUdtType.SymbolId;
            if (FindOperatorInHierarchy(leftUdtSymbolId, expr.Op.Kind, 2) is MethodBuilder udtOperatorMethod)
            {
                EmitExpression(expr.Left);
                EmitExpression(expr.Right);
                _il.Emit(OpCodes.Call, udtOperatorMethod);
                return;
            }
        }

        // Concatenazione stringhe: unico caso in cui "+" non è aritmetica.
        if (expr.Op.Kind == TokenKind.Plus && (leftType is StringType || rightType is StringType))
        {
            EmitExpression(expr.Left);
            EmitConvertToString(leftType);
            EmitExpression(expr.Right);
            EmitConvertToString(rightType);
            _il.Emit(OpCodes.Call, _types.StringConcat);
            return;
        }

        if (expr.Op.Kind == TokenKind.Plus
            && (NormalizeInferredType(leftType) is AnyType || NormalizeInferredType(rightType) is AnyType))
        {
            EmitObjectBoxedArgument(expr.Left);
            EmitObjectBoxedArgument(expr.Right);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicAdd), typeof(object), typeof(object)));
            return;
        }

        // Aritmetica Date/Time/TimeSpan (D2 - D → TimeSpan, D + T → DateTime, D + 10 →
        // AddDays): delegata a IlRuntime.DateBinary (dispatch per tipo).
        if (expr.Op.Kind is TokenKind.Plus or TokenKind.Minus
            && (NormalizeInferredType(leftType) is DateType or TimeType or DateTimeType or TimeSpanType
                || NormalizeInferredType(rightType) is DateType or TimeType or DateTimeType or TimeSpanType))
        {
            // Il primo argomento (l: DateTime) è il valore DATE/DateTime/TimeSpan DIRETTO
            // (un box a object causerebbe mismatch con la firma → InvalidProgram).
            EmitExpression(expr.Left);
            _il.Emit(OpCodes.Ldc_R8, expr.Op.Kind == TokenKind.Minus ? 1.0 : 0.0);
            _il.Emit(OpCodes.Ldc_R8, TypeKindOf(NormalizeInferredType(leftType)));
            EmitExpression(expr.Right);
            var rightClr = DataTypeMapper.Map(NormalizeInferredType(rightType), _types, _classTypes);
            if (rightClr.IsValueType) _il.Emit(OpCodes.Box, rightClr);
            _il.Emit(OpCodes.Ldc_R8, TypeKindOf(NormalizeInferredType(rightType)));
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DateBinary), typeof(DateTime), typeof(double), typeof(double), typeof(object), typeof(double)));
            // DateBinary ritorna object boxed (DateTime/TimeSpan): unbox al tipo del risultato.
            var resultClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr)), _types, _classTypes);
            if (resultClr.IsValueType) _il.Emit(OpCodes.Unbox_Any, resultClr);
            return;
        }

        if (IsComparisonOperator(expr.Op.Kind))
        {
            // UDT (STRUCT, value type CLR): ceq su una struct con campi riferimento
            // confronta male (o è comportamento indefinito). Confronto PER CAMPI — AND
            // delle uguaglianze. Golden TypesUdtGlobal 'P1 e P2 sono uguali? (Atteso:
            // False) --> 0' (P2.Nome modificato dopo P2 = P1).
            if (NormalizeInferredType(leftType) is UserDefinedType udtLeft)
            {
                EmitUdtComparison(expr.Left, expr.Right, udtLeft, expr.Op.Kind == TokenKind.Equal);
                return;
            }
            if (NormalizeInferredType(rightType) is UserDefinedType udtRight)
            {
                EmitUdtComparison(expr.Left, expr.Right, udtRight, expr.Op.Kind == TokenKind.Equal);
                return;
            }
            var isString = leftType is StringType || rightType is StringType;
            // Handle a RIFERIMENTO (Channel/Context/WaitGroup/...) confrontato con 0/NIL:
            // un handle non-null è "vero". Prima finiva in EmitComparisonNumericSide → ceq
            // fra un ref e un double = IL invalido (bug context/done.bas `IF ch <> 0`).
            {
                var lN2 = NormalizeInferredType(leftType);
                var rN2 = NormalizeInferredType(rightType);
                static bool IsRefLikeHandle(DataType t) => t is ChannelType or ContextType or WaitGroupType
                    or MutexType or RwMutexType or OnceType or AtomicType or PoolType or CondType
                    or ConcurrentMapType or FutureType;
                if ((IsRefLikeHandle(lN2) && IlConversion.IsNumeric(rN2))
                    || (IsRefLikeHandle(rN2) && IlConversion.IsNumeric(lN2)))
                {
                    EmitExpression(IsRefLikeHandle(lN2) ? expr.Left : expr.Right);
                    _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Ceq);   // ref == null ? 1 : 0
                    if (expr.Op.Kind == TokenKind.NotEqual)
                    {
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Ceq); // → ref != null
                    }
                    return;
                }
            }
            // ERR (Basic.ErrInstance) usato come VALORE in un confronto: è il codice d'errore
            // (Number) — va emesso come double, non come oggetto (bug AgentCodeGenDemo `IF ERR = 0`).
            {
                var lNE = NormalizeInferredType(leftType);
                var rNE = NormalizeInferredType(rightType);
                if ((lNE is ErrType || rNE is ErrType) && !isString)
                {
                    EmitNumericOperandWithErr(expr.Left, leftType);
                    EmitNumericOperandWithErr(expr.Right, rightType);
                    EmitComparisonOnStackValues(expr.Op.Kind, false);
                    return;
                }
            }
            // CIL ceq/clt/cgt richiede operandi numerici dello stesso tipo. I
            // letterali BASIC sono DOUBLE mentre UBOUND/LBOUND e molte funzioni
            // native restituiscono LONG/INTEGER: normalizziamo entrambi a DOUBLE
            // prima del confronto, senza alterare le comparazioni stringa/oggetto.
            if (!isString && IlConversion.IsNumeric(leftType) && IlConversion.IsNumeric(rightType))
            {
                EmitDoubleArgument(expr.Left);
                EmitDoubleArgument(expr.Right);
            }
            else if (expr.Op.Kind is TokenKind.Equal or TokenKind.ExactlyEqual or TokenKind.NotEqual
                     && (NormalizeInferredType(leftType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType
                         || NormalizeInferredType(rightType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType))
            {
                // Semantica NIL (parità PCodeVM/Clipper, golden ClipperSyntax): NIL==NIL è
                // vero, NIL==<qualsiasi valore> è falso (ToDouble(null)=0 avrebbe detto
                // NIL==0 true). Operandi boxed → DynamicEquals ritorna -1/0.
                EmitObjectBoxedArgument(expr.Left);
                EmitObjectBoxedArgument(expr.Right);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicEquals), typeof(object), typeof(object)));
                // DynamicEquals ritorna -1.0/0.0 (double), ma il chiamante si aspetta l'int32 0/1 di un `ceq` (poi lo moltiplica per -1
                // per ottenere il booleano BASIC): riportare entrambi i rami a 0/1. Prima `=` lasciava il double: `okc = (j.x = 1)`
                // valeva +1 invece di -1 e il metodo conteneva IL non valido (mul double*int32; ILOptim: "Invalid operand types in
                // BinaryInst" stampato su stdout, DIFF su minitests/json/member_typed).
                if (expr.Op.Kind == TokenKind.NotEqual)
                {
                    _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Ceq);
                }
                else
                {
                    _il.Emit(OpCodes.Ldc_R8, -1.0);
                    _il.Emit(OpCodes.Ceq);
                }
                return;
            }
            else if (NormalizeInferredType(leftType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType
                     || NormalizeInferredType(rightType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType)
            {
                // Confronto RELAZIONALE con un operando DINAMICO (campo nudo RDD/val boxed):
                // confronto type-aware a runtime — string ordinale se uno dei due è una
                // stringa, altrimenti numerico (mirror di gmb_val_compare / PCodeVM / Clipper).
                // Senza, il lato dinamico → ToDouble(object) (double) accanto a una stringa →
                // `call string.CompareOrdinal(double, string)` = InvalidProgram (bug bare field
                // `IF NAME > "AA"`). Ritorna int32 -1/0/1 → confrontato con 0 (poi il chiamante
                // applica Neg per il canone -1/0 BASIC).
                EmitObjectBoxedArgument(expr.Left);
                EmitObjectBoxedArgument(expr.Right);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DynamicCompare), typeof(object), typeof(object)));
                _il.Emit(OpCodes.Ldc_I4_0);
                EmitComparison(expr.Op.Kind);
                return;
            }
            else
            {
                // Un lato AnyType/object (es. dec(0) da un object[] ABI): il valore è boxed —
                // va convertito a double via IlRuntime.ToDouble prima del confronto, altrimenti
                // oggetto vs double su ceq/clt/cgt = InvalidProgram (twin GM c342b047).
                EmitComparisonNumericSide(expr.Left, leftType);
                EmitComparisonNumericSide(expr.Right, rightType);
            }
            EmitComparisonOnStackValues(expr.Op.Kind, isString);
            return;
        }

        // Parità VM (OP_AND/OP_OR → (long)a & (long)b): AND/OR è SEMPRE BITWISE, anche
        // su operandi booleani (i BIT BASIC sono -1/0, e bitwise di -1/0 equivale al
        // logico: -1 & -1 = -1, -1 & 0 = 0). Prima EmitShortCircuit* faceva 6 AND 3 = true
        // = -1 e 0 OR 5 = 0 (golden TypesNumerics '6 AND 3 = 2', 'FALSO OR 5 = 5').
        // AND/OR LOGICI short-circuit, risultato booleano GMBasic (-1 vero / 0 falso).
        // Canone condiviso con Compiler (PCode/VM-C) e C99Native.
        // AND/OR short-circuit con semantica condivisa col Compiler (PCode/VM-C):
        //   AND: sinistra falsa -> 0, altrimenti VALORE destro.
        //   OR:  sinistra vera  -> -1 (TRUE), altrimenti VALORE destro.
        if (expr.Op.Kind == TokenKind.And)
        {
            var falseL = _il.DefineLabel();
            var endL = _il.DefineLabel();
            /* Sinitra convertita a double (gestisce anche i local OBJECT boxed: un BIT
             * ri-tipato dal Linker vive in un object e `conv.i8` su un riferimento testava
             * l'INDIRIZZO → `FALSO(0) OR 5` dava -1). */
            EmitDoubleArgument(expr.Left); _il.Emit(OpCodes.Conv_I8); _il.Emit(OpCodes.Brfalse, falseL);
            EmitDoubleArgument(expr.Right);   /* VALORE destro (double), non normalizzato a -1/0 */
            _il.Emit(OpCodes.Br, endL);
            _il.MarkLabel(falseL); _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_R8);
            _il.MarkLabel(endL);
            return;
        }
        if (expr.Op.Kind == TokenKind.Or)
        {
            var trueL = _il.DefineLabel();
            var endL = _il.DefineLabel();
            EmitDoubleArgument(expr.Left); _il.Emit(OpCodes.Conv_I8); _il.Emit(OpCodes.Brtrue, trueL);
            EmitDoubleArgument(expr.Right);   /* VALORE destro (double), non normalizzato a -1/0 */
            _il.Emit(OpCodes.Br, endL);
            _il.MarkLabel(trueL); _il.Emit(OpCodes.Ldc_I4_M1); _il.Emit(OpCodes.Conv_R8);
            _il.MarkLabel(endL);
            return;
        }
        if (expr.Op.Kind == TokenKind.Xor)
        {
            /* BITWISE sui valori (canone: `6 XOR 3` = 5, `255 XOR 15` = 240). */
            EmitDoubleArgument(expr.Left); _il.Emit(OpCodes.Conv_I8);
            EmitDoubleArgument(expr.Right); _il.Emit(OpCodes.Conv_I8);
            _il.Emit(OpCodes.Xor); _il.Emit(OpCodes.Conv_R8);
            return;
        }
        if (expr.Op.Kind is TokenKind.BitAnd or TokenKind.BitOr or TokenKind.BitXor)
        {
            /* Veri/valori numerici: EmitDoubleArgument (gestisce anche i local OBJECT
             * boxed), poi cast a int64 per l'operazione bit a bit. */
            EmitDoubleArgument(expr.Left); _il.Emit(OpCodes.Conv_I8);
            EmitDoubleArgument(expr.Right); _il.Emit(OpCodes.Conv_I8);
            _il.Emit(expr.Op.Kind == TokenKind.BitAnd ? OpCodes.And
                   : expr.Op.Kind == TokenKind.BitOr ? OpCodes.Or : OpCodes.Xor);
            _il.Emit(OpCodes.Conv_R8);
            return;
        }

        // ?? (null-coalescing): val = left; se non-null usa left, altrimenti right.
        // Il left di un accesso SAFE (d["x"]?) è già un riferimento (object) o va boxato.
        if (expr.Op.Kind == TokenKind.QuestionMarkQuestion)
        {
            var coalesceEnd = _il.DefineLabel();
            EmitExpression(expr.Left);
            var leftClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Left)), _types, _classTypes);
            if (leftClr.IsValueType) _il.Emit(OpCodes.Box, leftClr);
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Brtrue, coalesceEnd); // non-null → salta al risultato
            _il.Emit(OpCodes.Pop);
            EmitExpression(expr.Right);
            var rightClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Right)), _types, _classTypes);
            if (rightClr.IsValueType) _il.Emit(OpCodes.Box, rightClr);
            _il.MarkLabel(coalesceEnd);
            return;
        }

        // BIT si comporta da numero (-1/0): va trattato nel ramo numerico, altrimenti gli
        // operandi finiscono grezzi sullo stack (un local object boxed + `add` = InvalidProgram:
        // `PRINT v + 0` con `DIM v AS BIT`).
        if ((IlConversion.IsNumeric(leftType) || leftType is BitType)
            && (IlConversion.IsNumeric(rightType) || rightType is BitType))
        {
            EmitDoubleArgument(expr.Left);
            EmitDoubleArgument(expr.Right);
        }
        else if (NormalizeInferredType(leftType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType
                 || NormalizeInferredType(rightType) is AnyType or Basic.JsonType or NullableType or UnresolvedQualifiedType)
        {
            // Aritmetica con un lato DINAMICO (oggetto boxed, es. il valore di un FOR EACH
            // su un array letterale object[]): converti entrambi a double via ToDouble
            // (come i confronti — twin GM c342b047), altrimenti Add/Sub/Mul su object =
            // InvalidProgram.
            EmitComparisonNumericSide(expr.Left, leftType);
            EmitComparisonNumericSide(expr.Right, rightType);
        }
        else
        {
            EmitExpression(expr.Left);
            EmitExpression(expr.Right);
        }
        if (expr.Op.Kind == TokenKind.Power)
        {
            // ^ (POWER): non esiste un opcode CIL — gli operandi (già normalizzati a DOUBLE
            // dall'emit sopra) vengono consumati da Math.Pow.
            _il.Emit(OpCodes.Call, typeof(System.Math).GetMethod(nameof(System.Math.Pow), [typeof(double), typeof(double)])!);
        }
        else if (expr.Op.Kind == TokenKind.IntegerDivide)
        {
            // Divisione INTERA (stile BASIC `\`): troncamento di a/b con ERR 11 su divisore 0
            // (parità C99/VM-C/PCode). L'helper sostituisce `div`+`Math.Truncate`: il `div` nudo
            // dava +Infinity e il truncate su infinito andava in overflow (ERR 6) invece di ERR 11.
            _il.Emit(OpCodes.Ldc_I4, expr.Op.Position.Line > 0 ? expr.Op.Position.Line : 0);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SafeIntDivide), typeof(double), typeof(double), typeof(int)));
        }
        else if (expr.Op.Kind is TokenKind.Divide or TokenKind.Mod)
        {
            // `/` e MOD: ERR 11 "Division by zero" su divisore 0 — fonte unica gestita in
            // IlRuntime.SafeDivide/SafeMod (prima: `div`/`rem` nudi → +Infinity/NaN, nessun
            // errore, mentre C99Native/VM/C sollevavano ERR 11).
            var safeName = expr.Op.Kind == TokenKind.Divide
                ? nameof(IlRuntime.SafeDivide)
                : nameof(IlRuntime.SafeMod);
            // Riga dell'OPERATORE (non del primo token dello statement): è la riga che C99Native
            // riporta per la stessa espressione (gmb_safe_div riceve e->line).
            _il.Emit(OpCodes.Ldc_I4, expr.Op.Position.Line > 0 ? expr.Op.Position.Line : 0);
            _il.Emit(OpCodes.Call, RuntimeMethod(safeName, typeof(double), typeof(double), typeof(int)));
        }
        else
        {
            _il.Emit(expr.Op.Kind switch
            {
                TokenKind.Plus => OpCodes.Add,
                TokenKind.Minus => OpCodes.Sub,
                TokenKind.Multiply => OpCodes.Mul,
                _ => throw new NotSupportedException(
                    $"Backend IL: operatore binario '{expr.Op.Kind}' non ancora supportato.")
            });
        }
    }

    /// <summary>Confronto di uguaglianza di un UDT (STRUCT) PER CAMPI: AND delle Ceq di ogni
    /// campo (string per riferimento, numerici/bool per valore). Ceq su una struct intera
    /// è indefinito per struct con campi riferimento. Golden TypesUdtGlobal.</summary>
    private void EmitUdtComparison(Expr left, Expr right, UserDefinedType udtType, bool isEqual)
    {
        var fields = udtType.Fields;
        if (fields.Length == 0)
        {
            _il.Emit(isEqual ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
            return;
        }
        void EmitFieldCompare(TypeField field)
        {
            if (!_classFields.TryGetValue(field.SymbolId, out var fieldBuilder))
                throw new NotSupportedException($"Backend IL: campo UDT '{field.Name.Lexeme}' non registrato per il confronto.");
            EmitExpression(left);
            _il.Emit(OpCodes.Ldfld, fieldBuilder);
            EmitExpression(right);
            _il.Emit(OpCodes.Ldfld, fieldBuilder);
            _il.Emit(OpCodes.Ceq);
        }
        EmitFieldCompare(fields[0]);
        for (var i = 1; i < fields.Length; i++)
        {
            EmitFieldCompare(fields[i]);
            _il.Emit(OpCodes.And);
        }
        if (!isEqual)
        {
            _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Ceq);
        }
    }

    /// <summary>
    /// Emette il controllo runtime dell'operatore BASIC IS per tipi scalari, UDT,
    /// classi e collezioni generiche raw.
    /// </summary>
    /// <param name="value">Espressione il cui valore runtime deve essere controllato.</param>
    /// <param name="targetType">Tipo BASIC indicato a destra dell'operatore IS.</param>
    private void EmitIsTypeExpression(Expr value, DataType targetType)
    {
        var normalizedTarget = NormalizeInferredType(targetType);
        var normalizedValue = NormalizeInferredType(InferType(value));
        EmitExpression(value);

        var valueIsAlreadyObject = value is VariableExpr variable
            && _redefinedTypeSymbols.Contains(variable.SymbolId);
        if (!valueIsAlreadyObject)
        {
            var valueClr = DataTypeMapper.Map(normalizedValue, _types, _classTypes);
            if (valueClr.IsValueType) _il.Emit(OpCodes.Box, valueClr);
        }

        var targetClr = DataTypeMapper.Map(normalizedTarget, _types, _classTypes);
        if (IsRawGenericTypeTarget(normalizedTarget) && targetClr.IsGenericType)
        {
            _il.Emit(OpCodes.Ldstr, targetClr.GetGenericTypeDefinition().FullName!);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsRawGenericType), typeof(object), typeof(string)));
            return;
        }

        _il.Emit(OpCodes.Isinst, targetClr);
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Cgt_Un);
    }

    /// <summary>
    /// Determina se il tipo a destra di IS rappresenta una famiglia generica senza
    /// argomenti espliciti, per esempio LIST che il parser rappresenta come LIST OF ANY.
    /// </summary>
    /// <param name="type">Tipo BASIC normalizzato da esaminare.</param>
    /// <returns>True quando gli argomenti generici sono segnaposto ANY.</returns>
    private static bool IsRawGenericTypeTarget(DataType type) => type switch
    {
        ListType { ElementType: AnyType } or HashSetType { ElementType: AnyType }
            or QueueType { ElementType: AnyType } or StackType { ElementType: AnyType }
            or LinkedListType { ElementType: AnyType } or DoublyLinkedListType { ElementType: AnyType }
            or CircularLinkedListType { ElementType: AnyType } => true,
        DictType { KeyType: AnyType, ValueType: AnyType }
            or SortedDictType { KeyType: AnyType, ValueType: AnyType } => true,
        _ => false
    };

    /// <summary>
    /// AND con corto circuito: se il lato sinistro è falso, il risultato è falso senza
    /// valutare il destro (evita side-effect/errori nel ramo non valutato, es. AND che
    /// protegge una divisione). Stessa semantica del p-code Compiler esistente
    /// (Compiler.Expressions.cs, OP_JUMP_IF_FALSE) — a differenza dell'Executor ad
    /// albero, che valuta entrambi i lati eagerly prima del confronto booleano (quirk
    /// della sua implementazione a visita ricorsiva, non semantica di linguaggio
    /// deliberata). Questo backend segue il p-code Compiler, il riferimento più diretto
    /// per un "vero compilatore" (vedi header di questo file).
    /// </summary>
    private void EmitShortCircuitAnd(BinaryExpr expr)
    {
        var falseLabel = _il.DefineLabel();
        var endLabel = _il.DefineLabel();

        EmitExpression(expr.Left);
        _il.Emit(OpCodes.Brfalse, falseLabel);
        EmitExpression(expr.Right);
        _il.Emit(OpCodes.Br, endLabel);
        _il.MarkLabel(falseLabel);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.MarkLabel(endLabel);
    }

    /// <summary>OR con corto circuito: speculare a EmitShortCircuitAnd.</summary>
    private static bool IsComparisonOperator(TokenKind kind) => kind is TokenKind.Equal or TokenKind.NotEqual
        or TokenKind.LessThan or TokenKind.LessEqual or TokenKind.GreaterThan or TokenKind.GreaterEqual
        or TokenKind.ExactlyEqual;

    /// <summary>Parità PCode (2026-09-30): un confronto DIRETTO come operando di AND/OR/XOR
    /// arriva sullo stack come int32 0/1 (ceq), mentre il canone booleano BASIC è -1/0.
    /// Il chiamante emette <see cref="OpCodes.Neg"/> (1→-1, 0→0) quando questo è vero.
    /// Vale SOLO per confronti diretti (attraverso Grouping): AND/OR/XOR annidati e valori
    /// numerici seguono la semantica valore (es. `(6 AND 3)` = 3) e non si toccano.</summary>
    private static bool IsDirectComparisonOperand(Expr e)
    {
        while (e is GroupingExpr ge) e = ge.Expression;
        return e is BinaryExpr b && IsComparisonOperator(b.Op.Kind);
    }

    /// <summary>
    /// IL ha solo ceq/clt/cgt diretti: NotEqual/LessEqual/GreaterEqual si ottengono
    /// negando il confronto opposto (es. A>=B equivale a NOT(A&lt;B)).
    /// </summary>
    private void EmitComparison(TokenKind op)
    {
        switch (op)
        {
            case TokenKind.Equal: _il.Emit(OpCodes.Ceq); break;
            case TokenKind.ExactlyEqual: _il.Emit(OpCodes.Ceq); break;
            case TokenKind.NotEqual: _il.Emit(OpCodes.Ceq); _il.Emit(OpCodes.Ldc_I4_1); _il.Emit(OpCodes.Xor); break;
            case TokenKind.LessThan: _il.Emit(OpCodes.Clt); break;
            case TokenKind.GreaterThan: _il.Emit(OpCodes.Cgt); break;
            case TokenKind.LessEqual: _il.Emit(OpCodes.Cgt); _il.Emit(OpCodes.Ldc_I4_1); _il.Emit(OpCodes.Xor); break;
            case TokenKind.GreaterEqual: _il.Emit(OpCodes.Clt); _il.Emit(OpCodes.Ldc_I4_1); _il.Emit(OpCodes.Xor); break;
            default: throw new NotSupportedException($"Backend IL: operatore di confronto '{op}' non ancora supportato.");
        }
    }

    /// <summary>
    /// Nucleo di confronto condiviso — presuppone i DUE OPERANDI GIÀ SULLO STACK (mai una
    /// coppia di Expr da valutare: quello resta compito del chiamante, che sia
    /// EmitBinaryExpression, per cui i due operandi sono le sue Left/Right, o SELECT
    /// CASE/MATCH — EmitCaseTestMatches sotto — dove uno dei due operandi vive in un
    /// locale temporaneo che non va rivalutato a ogni test). Stringhe: = e &lt;&gt; usano
    /// String.Equals (ordinale, non l'opcode ceq che confronterebbe i riferimenti);
    /// &lt;/&lt;=/&gt;/&gt;= usano String.CompareOrdinal seguito dal confronto numerico del
    /// risultato con 0 (stessa semantica lessicografica dell'Executor esistente).
    /// Numeri: <see cref="EmitComparison"/>.
    /// </summary>
    private void EmitComparisonOnStackValues(TokenKind op, bool isString)
    {
        if (!isString)
        {
            EmitComparison(op);
            return;
        }

        if (op is TokenKind.Equal or TokenKind.NotEqual)
        {
            _il.Emit(OpCodes.Call, _types.StringEquals);
            if (op == TokenKind.NotEqual)
            {
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Ceq);
            }
            return;
        }

        _il.Emit(OpCodes.Call, _types.StringCompareOrdinal);
        _il.Emit(OpCodes.Ldc_I4_0);
        EmitComparison(op);
    }

    private void EmitUnaryExpression(UnaryExpr expr)
    {
        // OPERATOR unario sovraccaricato — stesso identico meccanismo di risoluzione
        // statica di EmitBinaryExpression (vedi lì), arità 1 invece di 2 per
        // disambiguare da un eventuale overload binario dello stesso TokenKind (es.
        // Minus: sottrazione binaria vs negazione unaria).
        if (InferType(expr.Right) is ClassType rightClassType)
        {
            var classSymbolId = ResolveClassSymbolIdForOperatorLookup(expr.Right) ?? rightClassType.Definition.SymbolId;
            if (FindOperatorInHierarchy(classSymbolId, expr.Op.Kind, 1) is MethodBuilder operatorMethod)
            {
                EmitExpression(expr.Right);
                _il.Emit(OpCodes.Call, operatorMethod);
                return;
            }
        }

        EmitExpression(expr.Right);
        switch (expr.Op.Kind)
        {
            case TokenKind.Minus: _il.Emit(OpCodes.Neg); break;
            case TokenKind.Not:
                /* NOT logico: (x == 0) ? -1 : 0. L'operando va ri-emesso con la
                 * conversione a double: una variabile BIT vive in un local OBJECT
                 * (boxed) e `conv.i8` su un riferimento dava l'indirizzo, non il valore
                 * (`NOT f` con f = 0 stampava 0 invece di -1: divergenza IL vs
                 * PCode/VM-C/C99). */
                _il.Emit(OpCodes.Pop);
                if (expr.Right is VariableExpr notVar && EmittedValueIsObject(notVar.SymbolId))
                {
                    // local/param OBJECT (BIT boxato): il valore va risolto a runtime
                    EmitExpression(expr.Right);
                    _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                }
                else EmitDoubleArgument(expr.Right);
                _il.Emit(OpCodes.Conv_I8);
                // `conv.i8` lascia un Int64 sullo stack: il letterale 0 deve essere Int64
                // (`conv.i8` anche su di lui), altrimenti `ceq` confronta Int64 con Int32 =
                // IL INVALIDO per ECMA (bug reale: ILOptim rifiutava l'intero metodo con
                // "typeL == typeR || (Int && NInt)" e l'ottimizzatore rinunciava; il JIT
                // tollerava, ma l'IL restava non valido). Ogni `NOT x` ne era affetto.
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Conv_I8);
                _il.Emit(OpCodes.Ceq);
                _il.Emit(OpCodes.Neg);      /* 1 (x==0) -> -1 ; 0 -> 0 */
                _il.Emit(OpCodes.Conv_R8);
                break;
            case TokenKind.BitNot:
                /* ! bitwise: ~(long)x */
                _il.Emit(OpCodes.Conv_I8); _il.Emit(OpCodes.Not); _il.Emit(OpCodes.Conv_R8); break;
            default:
                throw new NotSupportedException($"Backend IL: operatore unario '{expr.Op.Kind}' non ancora supportato.");
        }
    }

    /// <summary>Normalizza il valore di un'espressione a -1 (vero) / 0 (falso).</summary>
    private void EmitBoolNormalized(Expr e)
    {
        var trueL = _il.DefineLabel();
        var endL = _il.DefineLabel();
        EmitExpression(e); _il.Emit(OpCodes.Conv_I8); _il.Emit(OpCodes.Brtrue, trueL);
        _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_R8); _il.Emit(OpCodes.Br, endL);
        _il.MarkLabel(trueL); _il.Emit(OpCodes.Ldc_I4_M1); _il.Emit(OpCodes.Conv_R8);
        _il.MarkLabel(endL);
    }

    private static bool IsDirectToStringType(DataType t) => t is DoubleType or SingleType or LongType
        or ULongType or QuadType or UQuadType or IntegerType or UIntegerType or ByteType or SByteType
        or BitType or CurrencyType or DateType or StringType;

    private void EmitDynamicValueBox(Expr e)
    {
        // tipo CLR REALE del valore sullo stack (locale/array registrati/symbol) — il box
        // per i value type; object/ref restano come sono. Il fallback sul SymbolId copre
        // il caso in cui il symbol del print non coincide col locale registrato
        // (DIM k = chiavi(i): il valore è un double non boxato). Porting GM a4333d15.
        Type clr;
        // Local BYREF catturato da un'altra closure (Fase 4, Closure-ByRef-Box-Linker-
        // Definitivo): LocalType è ByRefBox<T> (reference type, mai boxato) ma
        // EmitLoadVariable ha già emesso il deref (ldfld Value) — il tipo REALE sullo
        // stack a questo punto è T, non il box: serve T per decidere se boxare.
        if (e is VariableExpr v && _locals.TryGetValue(v.SymbolId, out var lb))
            clr = IsByRefBoxFieldType(lb.LocalType) ? lb.LocalType.GetGenericArguments()[0] : lb.LocalType;
        else if (e is VariableExpr v2 && _arrays.TryGetValue(v2.SymbolId, out var ai)) clr = ai.ClrArrayType;
        else if (e is VariableExpr v3)
        {
            var st = NormalizeInferredType(_program.Symbols.GetSymbol(v3.SymbolId).DataType);
            clr = DataTypeMapper.Map(st, _types, _classTypes);
        }
        else
        {
            // Espressione NON-variabile (somma, chiamata, campo...): il tipo CLR dal tipo
            // INFERITO (es. DoubleType → double → box) — prima typeof(object) non boxava
            // il double raw → PrintFormatValue(object) = InvalidProgram.
            var st = NormalizeInferredType(InferType(e));
            clr = DataTypeMapper.Map(st, _types, _classTypes);
        }
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
    }

    private void EmitConvertToString(DataType type)
    {
        // Delega centralizzata a IlConversion (vedi IlBackend/IlConversion.cs): stessa
        // matrice Double→StringHelpers*ToInvariantString, Long→Convert.ToString(long), ecc.
        // ma in UNICO punto, già testato. Qualsiasi DataType non coperto lancia
        // NotSupportedIlDataTypeException esplicita, come prima.
        IlConversion.EmitToString(_il, _types, type);
    }

}
