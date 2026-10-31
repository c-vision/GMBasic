// FILE: IlBackendCodegen/Partial/IlProgramCompiler.BuiltinsData.Stats.cs
// PARTIAL — dominio BUILTIN DATA / STATS-FIN-COMPRESSION (stats/date/compression/zip/string).
// Migrato da IlProgramCompiler.BuiltinsData.cs (2026-08-30 — split 6.0.1, file <= 1200 righe).

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

/// <summary>Partial del transpiler IL: builtin data — stats/finanza/date/compressione/stringhe.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitStatsArrayCall(BuiltinFunctionCallExpr expr, MethodInfo method, int arity = 1)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente {arity} argomenti.");
        foreach (var argument in expr.Args)
        {
            var argType = NormalizeInferredType(InferType(argument));
            // AMAX/AMIN/ASUM da .Max()/.Min()/.Sum() su una LIST (il Linker converte i
            // metodi lista in queste funzioni — t6.Max() → AMAX(t6)): il valore è una
            // List<double> CLR, NON un array — converti a double[] (ToArray).
            if (argType is ListType listTypeStats)
            {
                EmitExpression(argument);
                var elemClr = DataTypeMapper.Map(listTypeStats.ElementType, _types, _classTypes);
                _il.Emit(OpCodes.Call, EnumerableToArrayMethod.MakeGenericMethod(elemClr));
                continue;
            }
            if (argType is not ArrayType)
                throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede un array.");
            EmitExpression(argument);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitStatsArrayDoubleCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 2 argomenti.");
        if (NormalizeInferredType(InferType(expr.Args[0])) is not ArrayType)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede un array come primo argomento.");
        EmitExpression(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitDoubleParamsBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        // MATH.MIN(3,1,4,1,5,9) arriva dal Linker come ARRAY ANIDATO (ParamArray):
        // il ToString mostra { { 3, 1, 4, 1, 5, 9 } } — l'outer array contiene UN elemento
        // che è l'inner array con i valori. Appiattimento come il P-Code VM
        // (case BuiltInFunction.MIN: "nested array wrapping").
        ImmutableArray<Expr> arguments;
        if (expr.Args.Length == 1 && expr.Args[0] is ArrayLiteralExpr literal)
        {
            arguments = literal.Elements.Length == 1 && literal.Elements[0] is ArrayLiteralExpr inner
                ? inner.Elements
                : literal.Elements;
        }
        else arguments = expr.Args;
        // Il Linker puo' avvolgere l'argomento in PIU' livelli di ParamArray ({ { { v } } }): si appiattiscono finche' resta un solo
        // letterale-contenitore (altrimenti la variabile array `v` veniva trattata come elemento: AVG(v) leggeva il puntatore come
        // double → 2.4E-314, SUM/MAX(v) → InvalidProgram).
        while (arguments.Length == 1 && arguments[0] is ArrayLiteralExpr nestedParams)
            arguments = nestedParams.Elements;
        if (arguments.Length == 0)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede almeno un argomento.");
        // MATH.AVG(v) / MIN(v) con v VARIABILE ARRAY (double[]): passa l'array direttamente
        // (params double[] = double[] sullo stack) — prima il valore dell'array veniva
        // trattato come elemento singolo (newarr 1 + stelem del boxed) → InvalidProgram.
        // Variabile array dichiarata con DIM d(n) AS T: il simbolo ha il tipo dell'ELEMENTO (+ flag array), quindi InferType non dà ArrayType —
        // si riconosce dalla tabella degli array del compilatore IL (_arrays), altrimenti l'array finiva trattato come elemento singolo
        // (AVG(d) leggeva il puntatore come double → 2.4E-314, SUM/MAX → InvalidProgram).
        if (arguments.Length == 1 && (NormalizeInferredType(InferType(arguments[0])) is ArrayType
            || (arguments[0] is VariableExpr arrVar && _arrays.ContainsKey(arrVar.SymbolId))))
        {
            EmitExpression(arguments[0]);
            _il.Emit(OpCodes.Call, method);
            return;
        }
        _il.Emit(OpCodes.Ldc_I4, arguments.Length);
        _il.Emit(OpCodes.Newarr, _types.Double);
        for (var index = 0; index < arguments.Length; index++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, index);
            EmitDoubleArgument(arguments[index]);
            _il.Emit(OpCodes.Stelem_R8);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitStringifyCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: STRINGIFY richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Stringify), typeof(object)));
    }

    private void EmitConfirmCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 1)
            throw new NotSupportedException("Backend IL: CONFIRM richiede almeno 1 argomento.");
        // Firma completa: (message, width=0, align=1, fg=-1, bg=-1, border=1, buttons=0).
        // Passa gli argomenti forniti, default per i mancanti.
        for (var i = 0; i < 7; i++)
        {
            if (i < expr.Args.Length) EmitExpression(expr.Args[i]);
            else _il.Emit(OpCodes.Ldc_R8, i switch { 1 => 0.0, 2 => 1.0, 5 => 1.0, _ => -1.0 });
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Confirm),
            typeof(string), typeof(double), typeof(double), typeof(double),
            typeof(double), typeof(double), typeof(double)));
    }

    private void EmitIifCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: IIF richiede 3 argomenti.");
        // LAZY: valuta la condizione, poi SOLO il ramo scelto (branch nativo). Prima
        // valutava entrambi i rami → IIF(D.ContainsKey("tema"), D["tema"], "Standard")
        // lanciava KeyNotFound anche quando il ramo non veniva usato.
        EmitDoubleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Ceq);
        var falseLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Brtrue, falseLabel);
        EmitExpression(expr.Args[1]);
        var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[1])), _types, _classTypes);
        if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
        var endLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Br, endLabel);
        _il.MarkLabel(falseLabel);
        EmitExpression(expr.Args[2]);
        var bClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[2])), _types, _classTypes);
        if (bClr.IsValueType) _il.Emit(OpCodes.Box, bClr);
        _il.MarkLabel(endLabel);
    }

    private void EmitChooseCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 2)
            throw new NotSupportedException("Backend IL: CHOOSE richiede almeno 2 argomenti.");
        // Il PARSER impacchetta le opzioni in un ArrayLiteral (firma del builtin con ultimo
        // parametro array) e a volte lo annida (CHOOSE(index, { { opt1, ... } })): si
        // appiattiscono ricorsivamente gli ArrayLiteral per estrarre le singole opzioni. Se
        // EmitChooseCall emettesse l'ArrayLiteral così com'è finirebbe un array-typed dentro
        // object[] (Choose lo ritornerebbe intero → ToDouble(double[]) lancia InvalidCast —
        // caso di samples/Community/Graphics/GraphicsDrawTextures.bas: Color = CHOOSE(..., -1, ...)).
        var flat = new List<Expr>();
        void CollectOptions(Expr e)
        {
            if (e is ArrayLiteralExpr al)
            {
                foreach (var el in al.Elements) CollectOptions(el);
            }
            else
            {
                flat.Add(e);
            }
        }
        Expr[] options;
        if (expr.Args.Length == 2)
        {
            CollectOptions(expr.Args[1]);
            options = flat.ToArray();
        }
        else
        {
            options = expr.Args.Skip(1).ToArray();
        }
        // L'indice arriva come CINT(2) (Int16) — il runtime vuole double: conv.
        EmitDoubleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Ldc_I4, options.Length);
        _il.Emit(OpCodes.Newarr, _types.Object);
        for (var i = 0; i < options.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(options[i]);
            var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(options[i])), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Choose), typeof(double), typeof(object[])));
    }

    private void EmitValTypeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: VALTYPE richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ValType), typeof(object)));
    }

    private void EmitTabCall(BuiltinFunctionCallExpr expr)
    {
        // La VM ignora gli argomenti (BuiltInFunction.TAB ritorna sempre "\t").
        _il.Emit(OpCodes.Call, IlRuntimeTabMethod);
    }

    private void EmitDateArgBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        // CDOW/DOW/DTOC prendono una DATA: l'argomento è DateTime CLR (DateType), il runtime
        // vuole double OADate — converte (DateTime.ToOADate), mai EmitConvert diretto che
        // non gestisce DateTime→double (non numerico) → stack mismatch → InvalidProgram.
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");
        EmitExpression(expr.Args[0]);
        var srcType = NormalizeInferredType(InferType(expr.Args[0]));
        if (srcType is DateType or TimeType or DateTimeType)
        {
            _il.Emit(OpCodes.Call, IlRuntimeToOADateMethod);
        }
        else
        {
            var srcClr = DataTypeMapper.Map(srcType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, srcType, srcClr, _types.Double);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitCompressionCall(BuiltinFunctionCallExpr expr, MethodInfo method, string? fixedAlgorithm)
    {
        if (expr.Args.Length < 1 || (fixedAlgorithm == null && expr.Args.Length < 2))
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {(fixedAlgorithm == null ? 2 : 1)} argomento(i).");
        EmitStringArgument(expr.Args[0]);
        if (fixedAlgorithm != null) _il.Emit(OpCodes.Ldstr, fixedAlgorithm);
        else EmitStringArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, method);
    }

    // ZIP(sources, dest) / UNZIP(archive, dir) — funzioni qualificate COMPRESSION.ZIP/UNZIP.
    private void EmitZipCall(BuiltinFunctionCallExpr expr, bool isZip)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {(isZip ? "ZIP" : "UNZIP")} richiede 2 argomenti.");
        EmitStringArgument(expr.Args[0]);
        EmitStringArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, isZip ? IlRuntimeZipMethod : IlRuntimeUnzipMethod);
        // Il runtime ritorna il booleano BASIC come double (-1/0), ma il tipo statico di ZIP/UNZIP e' BIT: i consumatori lo
        // convertono come un bool 0/1 (`ldc.i4.m1; mul`). Lasciare il double dava IL non valido (mul double*int32) e valore
        // sbagliato (+1): riportalo a bool 0/1 (risultato != 0).
        _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Ceq);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ceq);
    }

    private void EmitPixelBufferStatement(PixelBufferStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.Width);
        EmitInt32Arg(stmt.Height);
        EmitExpression(stmt.Buffer);
        var bClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Buffer)), _types, _classTypes);
        if (bClr.IsValueType) _il.Emit(OpCodes.Box, bClr);
        _il.Emit(OpCodes.Callvirt, GraphicsDrawPixelBufferMethod);
    }

    private void EmitRgbCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 3 or > 4)
            throw new NotSupportedException("Backend IL: RGB richiede 3 o 4 argomenti.");
        EmitDoubleArgument(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        if (expr.Args.Length == 4) EmitDoubleArgument(expr.Args[3]);
        else _il.Emit(OpCodes.Ldc_R8, 255.0);
        _il.Emit(OpCodes.Call, IlRuntimeRgbMethod);
    }

    private void EmitCellCall(BuiltinFunctionCallExpr expr)
    {
        // CELL: (row, column, mode) → IlRuntime.CellValue (long); CELLSTR: (row, column) → string.
        if (expr.Name.Kind == TokenKind.CellStr)
        {
            if (expr.Args.Length != 2)
                throw new NotSupportedException("Backend IL: CELLSTR richiede 2 argomenti.");
            EmitDoubleArgument(expr.Args[0]);
            EmitDoubleArgument(expr.Args[1]);
            _il.Emit(OpCodes.Call, IlRuntimeCellMethod);
            return;
        }
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: CELL richiede 3 argomenti (ricevuti {expr.Args.Length}).");
        foreach (var a in expr.Args) EmitDoubleArgument(a);
        _il.Emit(OpCodes.Call, IlRuntimeCellValueMethod);
    }

    private void EmitC2PCall(BuiltinFunctionCallExpr expr, MethodInfo method, bool widthDefault)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 o 2 argomenti.");
        EmitExpression(expr.Args[0]);
        if (expr.Args.Length == 2) EmitExpression(expr.Args[1]);
        else if (widthDefault) { _il.Emit(OpCodes.Ldstr, " "); _il.Emit(OpCodes.Call, IlRuntimeTextWidthMethod); }
        else _il.Emit(OpCodes.Call, IlRuntimeTextHeightMethod);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitTextWidthCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: TEXTWIDTH richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        var t = NormalizeInferredType(InferType(expr.Args[0]));
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, IlRuntimeTextWidthMethod);
    }

    /// <summary>Spinge un argomento RAISE EVENT boxato. I tipi CLASSE/INTERFACCIA sono già
    /// reference (niente box, e il Map su uno snapshot ClassType stale dell'EVENT — es.
    /// `EVENT Click(sender AS Button)` — lancerebbe "classe non ancora registrata").</summary>
    private void EmitEventArgumentBoxed(Expr arg)
    {
        EmitExpression(arg);
        var t = NormalizeInferredType(InferType(arg));
        if (t is ClassType or InterfaceType) return;
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
    }

    private void EmitObjectArgCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        var t = NormalizeInferredType(InferType(expr.Args[0]));
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Chiamata a un metodo IlRuntime con marshalling tipato per parametro
    /// (long/double/string), in base ai ParameterInfo del metodo target. Usato per i builtin
    /// TORCH (handle = long, scalari = double, ecc.) il cui TokenKind non era dispatchato.</summary>
    private void EmitTypedRuntimeCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        var pars = method.GetParameters();
        if (expr.Args.Length > pars.Length)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} accetta {pars.Length} argomenti, ricevuti {expr.Args.Length}.");
        for (int i = 0; i < pars.Length; i++)
        {
            if (i >= expr.Args.Length)
            {
                _il.Emit(pars[i].ParameterType.IsValueType ? OpCodes.Ldc_I4_0 : OpCodes.Ldnull);
                continue;
            }
            var pt = pars[i].ParameterType;
            if (pt == typeof(long) || pt == typeof(ulong))
                EmitInt64Argument(expr.Args[i]);
            else if (pt == typeof(double) || pt == typeof(float) || pt == typeof(int))
                EmitDoubleArgument(expr.Args[i]);
            else if (pt == typeof(string))
                EmitStringArgument(expr.Args[i]);
            else if (pt == typeof(Array))
            {
                EmitExpression(expr.Args[i]);
                _il.Emit(OpCodes.Castclass, typeof(Array));
            }
            else if (pt == typeof(object))
            {
                EmitExpression(expr.Args[i]);
                var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
                if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
            }
            else if (pt == typeof(bool))
            {
                EmitExpression(expr.Args[i]);
                _il.Emit(OpCodes.Conv_I1);
            }
            else
                EmitExpression(expr.Args[i]);
        }
        _il.Emit(OpCodes.Call, method);
    }

    // Chiamata a un metodo statico di un modulo IL esterno (IlModuleLoader). Marshalling
    // per parametri/ritorno BASIC → CLR: double/int/long/bool/string/object.
    private void EmitExternalCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        var pars = method.GetParameters();
        if (expr.Args.Length > pars.Length)
            throw new NotSupportedException(
                $"Backend IL: {method.DeclaringType?.Name}.{method.Name} accetta {pars.Length} argomenti, ricevuti {expr.Args.Length}.");

        for (var i = 0; i < expr.Args.Length; i++)
        {
            EmitExpression(expr.Args[i]);
            var t = NormalizeInferredType(InferType(expr.Args[i]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            var want = pars[i].ParameterType;

            if (want == typeof(object))
            {
                if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            }
            else if (want == typeof(string))
            {
                EmitConvertToString(t);
            }
            else if (want == typeof(bool))
            {
                if (clr != typeof(bool))
                {
                    _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Ceq);
                    _il.Emit(OpCodes.Ldc_I4_0);
                    _il.Emit(OpCodes.Ceq); // arg != 0 → true
                }
            }
            else if (want.IsValueType)
            {
                if (clr != want)
                {
                    if (clr == typeof(string)) EmitConvertToString(t);
                    else _il.Emit(OpCodes.Conv_R8);
                }
            }
            else
            {
                if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            }
        }

        _il.Emit(OpCodes.Call, method);

        var ret = method.ReturnType;
        if (ret == typeof(void))
        {
            // metodo void usato come istruzione: NON lasciare nulla sullo stack (lo statement
            // emitter, vedendo VoidType, non emette Pop → un ldc.r8 qui sbilancia lo stack).
            return;
        }
        if (ret.IsValueType && ret != typeof(double))
        {
            if (ret == typeof(string)) return;
            if (ret == typeof(object)) return;
            _il.Emit(OpCodes.Conv_R8); // bool/int/long/... → double BASIC
        }
    }

    private void EmitMethodInfoCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 o 2 argomenti.");
        EmitStringArgument(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitPeekCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length == 2)
        {
            // NET.PEEK(handle, numBytes): sbircia dal socket senza consumare (parità C99Native).
            EmitInt64Argument(expr.Args[0]);
            EmitInt32Arg(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TcpPeek)));
            return;
        }
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: PEEK richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, IlRuntimePeekMethod);
        var elemType = NormalizeInferredType(CollectionElementType(InferType(expr.Args[0])));
        var clr = DataTypeMapper.Map(elemType, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Unbox_Any, clr);
    }

    private void EmitDateDiffCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: DATEDIFF richiede 3 argomenti.");
        EmitExpression(expr.Args[0]); // interval (stringa)
        EmitDoubleArgument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, IlRuntimeDateDiffMethod);
    }

    private void EmitAlertCall(BuiltinFunctionCallExpr expr)
    {
        // ALERT(msg [, width [, align [, fg [, bg [, border [, buttons]]]]]]): in headless
        // solo msg conta (e ritorna 1 = OK); gli altri parametri di stile sono scartati.
        if (expr.Args.Length < 1)
            throw new NotSupportedException("Backend IL: ALERT richiede almeno 1 argomento.");
        EmitExpression(expr.Args[0]);
        EmitConvertToString(NormalizeInferredType(InferType(expr.Args[0])));
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Alert), typeof(string), typeof(string)));
    }

    private void EmitInt32BuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method, int arity)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente {arity} argomenti.");
        foreach (var argument in expr.Args) EmitInt32Arg(argument);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitDoubleArgument(Expr argument)
    {
        EmitExpression(argument);
        var sourceType = NormalizeInferredType(InferType(argument));
        var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
        // Il tipo INFERITO può divergere dal tipo REALE del valore emesso (es. variabile
        // ritipata int32 da `LOAD TEXTURE ... INTO` ma ancora inferita double): senza la
        // conversione, ceq/bne.un confronta int32 con float64 → InvalidProgram
        // (GraphicsSpriteBatch, 2026-10-03). Per le variabili locali usa il LocalType.
        if (argument is VariableExpr ve && _locals.TryGetValue(ve.SymbolId, out var lb))
            sourceClr = lb.LocalType;
        IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, _types.Double);
    }

    private void EmitLenCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: LEN richiede esattamente 1 argomento.");
        // LEN su stringa: CARATTERI (code point), non .Length (unità UTF-16: "😀" darebbe 2).
        // Passa da IlRuntime.Len, la stessa implementazione di PCode e del caso dinamico.
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, IlRuntimeLenMethod);
    }

    /// <summary>LENB(s): BYTE della codifica UTF-8 (LEN conta i CARATTERI).
    /// Argomento non stringa → -1 su tutte le gambe (parità parametri).</summary>
    private void EmitLenBCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: LENB richiede esattamente 1 argomento.");
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, IlRuntimeLenBytesMethod);
    }

    /// <summary>
    /// UBOUND(array[, dimensione]): a differenza del p-code (dove ogni array puo' essere un
    /// vero array CLR non-zero-based via Array.CreateInstance con lowerBounds custom, cosi'
    /// Array.GetUpperBound/GetLowerBound restituiscono GIA' il valore logico BASIC), gli
    /// array di QUESTO backend sono SEMPRE CLR SZ array 0-based (Newarr) o rettangolari
    /// standard (il costruttore T[,...] con soli lengths, mai lowerBounds) — vedi
    /// EmitArrayDeclaration. Il lower bound logico BASIC vive SOLO in
    /// ArrayInfo.LowerBounds[], mai nell'array CLR stesso, quindi va sommato qui
    /// esplicitamente: UBOUND logico = ArrayInfo.LowerBounds[asse] + Array.GetUpperBound(asse)
    /// (che per un array 0-based e' semplicemente length-1). GetUpperBound resta comunque
    /// una chiamata a RUNTIME (necessaria per una lunghezza dinamica, vedi EmitAxisLength),
    /// solo l'offset del lower bound e' sommato come costante compile-time.
    /// </summary>
    /// <summary>LBOUND/UBOUND con dimensione a RUNTIME (es. UBOUND(MAT, D)): GetLowerBound/
    /// GetUpperBound(int) sul tipo array CLR (il metodo è ereditato da System.Array).
    /// Porting GM 55eb83a2.</summary>
    private bool TryEmitRuntimeBound(BuiltinFunctionCallExpr expr, string name, bool isUpper)
    {
        if (expr.Args.Length != 2 || expr.Args[0] is not VariableExpr rv
            || !_arrays.TryGetValue(rv.SymbolId, out var rArr) || TryExtractCompileTimeInt32(expr.Args[1], out _))
            return false;
        EmitExpression(expr.Args[0]);
        EmitInt32Argument(expr.Args[1]);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Sub);
        var m = rArr.ClrArrayType.GetMethod(isUpper ? "GetUpperBound" : "GetLowerBound", [typeof(int)]);
        _il.Emit(m!.IsVirtual ? OpCodes.Callvirt : OpCodes.Call, m);
        _il.Emit(OpCodes.Conv_I8);
        return true;
    }

    private void EmitUBoundCall(BuiltinFunctionCallExpr expr)
    {
        if (TryEmitRuntimeBound(expr, "UBOUND", isUpper: true)) return;
        // UBOUND su una LIST (es. UBOUND(Numeri) con Numeri AS LIST<INTEGER>): il
        // conteggio runtime − 1 (le liste non hanno un ArrayInfo).
        if (expr.Args.Length >= 1 && NormalizeInferredType(InferType(expr.Args[0])) is ListType)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Sub);
            return;
        }
        // UBOUND su un'ESPRESSIONE che produce un array/collection (es. UBOUND(AI.AGENTEVENTS())
        // — risultato di una function call, non una VariableExpr in _arrays): valuta
        // l'espressione e usa CollectionCount-1 a runtime.
        if (expr.Args.Length >= 1
            && NormalizeInferredType(InferType(expr.Args[0])) is ArrayType
            && expr.Args[0] is not VariableExpr)
        {
            if (expr.Args.Length > 1)
                throw new NotSupportedException("Backend IL: UBOUND su espressione array supporta solo la dimensione 1.");
            var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
            EmitExpression(expr.Args[0]);
            if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Sub);
            return;
        }
        // UBOUND su un valore DINAMICO (AnyType — es. UBOUND(sortedKeys) con sortedKeys =
        // Scores.Keys()): l'array/collection reale è boxed → runtime Count-1.
        if (expr.Args.Length >= 1 && NormalizeInferredType(InferType(expr.Args[0])) is AnyType)
        {
            if (expr.Args.Length > 1)
                throw new NotSupportedException("Backend IL: UBOUND su oggetto dinamico supporta solo la dimensione 1.");
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Sub);
            return;
        }
        // UBOUND su un ARRAY runtime reale non registrato in _arrays (parametro procedura,
        // variabile assegnata da una funzione come SCREENS() — tipo statico Double ma valore
        // runtime array, REDIM, ecc.): la lunghezza si legge a runtime sull'array stesso.
        if (expr.Args.Length >= 1 && expr.Args[0] is VariableExpr rv && !_arrays.ContainsKey(rv.SymbolId))
        {
            if (expr.Args.Length > 1)
                throw new NotSupportedException("Backend IL: UBOUND su array runtime supporta solo la dimensione 1.");
            EmitExpression(expr.Args[0]);
            // Il local di una variabile implicita è spesso un value type (uint32/double):
            // CollectionCount prende object → box col tipo reale del local.
            if (_locals.TryGetValue(rv.SymbolId, out var ubLb) && ubLb.LocalType.IsValueType)
                _il.Emit(OpCodes.Box, ubLb.LocalType);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Sub);
            return;
        }

        var (arrayInfo, axisIndex) = ResolveArrayBoundArgs(expr, "UBOUND");

        EmitArrayRefFromInfo(arrayInfo);
        _il.Emit(OpCodes.Ldc_I4, axisIndex);
        _il.Emit(OpCodes.Callvirt, _types.ArrayGetUpperBound);
        var lowerBound = arrayInfo.LowerBounds[axisIndex];
        if (lowerBound != 0)
        {
            _il.Emit(OpCodes.Ldc_I4, lowerBound);
            _il.Emit(OpCodes.Add);
        }
        // UBOUND e' dichiarato LongType (Interpreter/StdLib/StdLib.System.cs,
        // NativeProcedureDefinition), non Double — il valore sullo stack deve essere Int64
        // per combaciare con InferType(expr)/EmitConvertToString a valle (un Conv_R8 qui
        // produceva un mismatch di tipo silenzioso a compile-time, rilevato dal JIT solo a
        // runtime come InvalidProgramException).
        _il.Emit(OpCodes.Conv_I8);
    }

    /// <summary>
    /// LBOUND(array[, dimensione]): il lower bound logico BASIC e' SEMPRE noto a
    /// compile-time (ArrayInfo.LowerBounds[], vedi EmitArrayDeclaration) — mai derivato
    /// dall'array CLR sottostante, che qui e' SEMPRE 0-based (vedi EmitUBoundCall). Nessuna
    /// chiamata a runtime necessaria, a differenza di UBOUND.
    /// </summary>
    private void EmitLBoundCall(BuiltinFunctionCallExpr expr)
    {
        if (TryEmitRuntimeBound(expr, "LBOUND", isUpper: false)) return;
        var (arrayInfo, axisIndex) = ResolveArrayBoundArgs(expr, "LBOUND");
        _il.Emit(OpCodes.Ldc_I4, arrayInfo.LowerBounds[axisIndex]);
        _il.Emit(OpCodes.Conv_I8); // LongType, vedi EmitUBoundCall
    }

    /// <summary>
    /// Risolve l'array e l'indice di asse (0-based) condivisi da UBOUND/LBOUND. L'argomento
    /// "dimensione" (1-based, default 1 se omesso) deve essere noto a COMPILE-TIME: seleziona
    /// quale ArrayInfo.LowerBounds[asse] leggere, un int[] compile-time — supportare un asse
    /// scelto a runtime richiederebbe un dispatch (es. una piccola tabella) non ancora fatto,
    /// scenario comunque rarissimo in codice BASIC reale (quasi sempre un letterale, es.
    /// <c>UBOUND(A, 2)</c>).
    /// </summary>
    private (ArrayInfo Info, int AxisIndex) ResolveArrayBoundArgs(BuiltinFunctionCallExpr expr, string name)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException($"Backend IL: {name} richiede 1 o 2 argomenti (array[, dimensione]).");
        if (expr.Args[0] is not VariableExpr arrayVarExpr || !_arrays.TryGetValue(arrayVarExpr.SymbolId, out var arrayInfo))
            throw new NotSupportedException($"Backend IL: {name} supporta solo un array locale/globale DIMensionato direttamente (non un campo, non un'espressione generica).");

        var axisIndex = 0;
        if (expr.Args.Length == 2)
        {
            if (!TryExtractCompileTimeInt32(expr.Args[1], out var dim))
                throw new NotSupportedException($"Backend IL: {name} con un argomento 'dimensione' non costante a compile-time non ancora supportato.");
            axisIndex = dim - 1;
        }
        if (axisIndex < 0 || axisIndex >= arrayInfo.Rank)
            throw new NotSupportedException($"Backend IL: {name}, dimensione {axisIndex + 1} fuori dal rank dell'array ({arrayInfo.Rank}).");

        return (arrayInfo, axisIndex);
    }

    /// <summary>
    /// Risolve l'ArrayInfo di un argomento di una funzione array in stile Clipper — stesso
    /// vincolo di ResolveArrayBoundArgs (solo un array locale/globale DIMensionato
    /// direttamente, non un campo/espressione generica), fattorizzato qui perché condiviso
    /// da tutte le funzioni array sotto.
    /// </summary>
    private ArrayInfo ResolveArrayArg(Expr arg, string name)
    {
        if (arg is not VariableExpr arrayVarExpr || !_arrays.TryGetValue(arrayVarExpr.SymbolId, out var arrayInfo))
            throw new NotSupportedException($"Backend IL: {name} supporta solo un array locale/globale DIMensionato direttamente (non un campo, non un'espressione generica).");
        if (arrayInfo.Rank != 1)
            throw new NotSupportedException($"Backend IL: {name} supporta solo array mono-dimensionali.");
        return arrayInfo;
    }

    /// <summary>
    /// Un argomento "callback" (predicato/funzione) di una funzione array può essere una
    /// lambda letterale (LambdaExpr) o una variabile di tipo FunctionPointerType — mai
    /// InferType(expr) direttamente su un LambdaExpr, che non lo gestisce (lancia). Usato da
    /// ASCAN per distinguere la forma "valore" dalla forma "predicato" (stesso dualismo
    /// dell'Executor/PCodeVm, qui risolto a COMPILE-TIME dal tipo statico dell'argomento
    /// anziché a runtime dal tipo dinamico).
    /// </summary>
    private bool IsCallbackArg(Expr arg) => arg is LambdaExpr or FunctionReferenceExpr ||
        (arg is VariableExpr v && _program.Symbols.GetSymbol(v.SymbolId).DataType is FunctionPointerType);


    /// <summary>
    /// AREVERSE: clona (ArrayHelpers.AClone, mai l'array originale) poi inverte la copia sul
    /// posto (CommonLib.ArrayUtils.Reverse, void) — la copia duplicata resta sullo stack come
    /// risultato. Nessun Castclass esplicito necessario per passare un T[] dove il parametro
    /// è System.Array: l'ampliamento di riferimento verso Array è sempre valido in IL.
    /// </summary>

    /// <summary>
    /// ASHUFFLE: muta l'array ORIGINALE sul posto (nessuna copia). L'interprete a runtime
    /// restituisce BIT true (vedi Gpm.Arrays.Functions.Sync.cs), ma il Linker
    /// (InferGenericPolymorphicReturnType, "Caso 2") assegna staticamente ad ASHUFFLE lo
    /// stesso ArrayType(elementType) dell'input — un'incoerenza preesistente tollerabile a
    /// runtime nell'interprete dinamico ma non qui: l'IL è tipizzato staticamente, quindi il
    /// valore prodotto DEVE rispettare il tipo inferito dal Linker (l'array stesso, non un
    /// bool), altrimenti il chiamante (es. `DIM B() = ASHUFFLE(A)`) genererebbe un Castclass
    /// impossibile bool→T[].
    /// </summary>





    private void EmitZipCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AZIP richiede 2 argomenti (array1, array2).");
        EmitObjectBoxedArgument(expr.Args[0]);
        EmitObjectBoxedArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, IlRuntimeZipArraysMethod);
    }

    private void EmitObjectBoxedArgument(Expr arg)
    {
        // Parità VM (OP_MK_DICT): un dict-literal passato come argomento boxed
        // (es. DBF.APPENDRECORD({ID: 1, NAME: "Alpha"})) può avere valori ETEROGENEI
        // (double per ID, string per NAME). Il tipo inferito dal Linker deriva dal
        // SOLO primo entry (DictType<String,Double>) e il secondo valore non si
        // coerce a double -> EmitDictionaryLiteral spinge "Alpha" su
        // Dictionary<string,double>.Add(string,double) = stack imbalance ->
        // InvalidProgramException. Emettere sempre Dictionary<string, object>
        // (con box dei value-type, già gestito da clrDictIsObjectValue) mantiene i
        // valori eterogenei del record, come fa la VM con il dict runtime.
        if (arg is DictionaryLiteralExpr dictLiteral)
        {
            EmitDictionaryLiteral(dictLiteral, new DictType(new StringType(), new AnyType()));
            return;
        }
        // Letterale array ANNIDATO o ETEROGENEO ({{"Nome", 10}, {"Qta", 6}}: TBROWSE colonne/dati, API TUI): il tipo inferito segue il PRIMO elemento (String[]),
        // quindi `stelem.ref` di un double in un string[] = IL non valido (segfault del CLR in IsInstanceOf). Nel contesto object il consumatore legge solo IList:
        // si costruisce un object[] (ricorsivo) con gli elementi boxati, come la VM (OP_MK_ARRAY di valori misti).
        if (arg is ArrayLiteralExpr arrLiteral && IsNestedOrHeterogeneousArrayLiteral(arrLiteral))
        {
            EmitObjectArrayLiteral(arrLiteral);
            return;
        }
        EmitExpression(arg);
        var t = NormalizeInferredType(InferType(arg));
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
    }

    private bool IsNestedOrHeterogeneousArrayLiteral(ArrayLiteralExpr lit)
    {
        if (lit.Elements.Length == 0) return false;
        var firstIsString = NormalizeInferredType(InferType(lit.Elements[0])) is StringType;
        foreach (var el in lit.Elements)
        {
            if (el is ArrayLiteralExpr) return true;
            if ((NormalizeInferredType(InferType(el)) is StringType) != firstIsString) return true;
        }
        return false;
    }

    private void EmitObjectArrayLiteral(ArrayLiteralExpr lit)
    {
        _il.Emit(OpCodes.Ldc_I4, lit.Elements.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var i = 0; i < lit.Elements.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            if (lit.Elements[i] is ArrayLiteralExpr inner) EmitObjectArrayLiteral(inner);
            else EmitObjectBoxedArgument(lit.Elements[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
    }




    /// <summary>Carica l'argomento array/lista e, se necessario, lo converte al tipo del
    /// parametro della callback (FN(n AS INTEGER) su array double: il delegate è
    /// Func<Int16,...> — AMap<double,...> con quel delegate era IL invalido/overflow).</summary>

    /// <summary>Tipo CLR del PRIMO parametro della callback, se dichiarato esplicitamente
    /// (per AMAP/AFILTER il T del generico deve matchare il delegate della lambda).</summary>
    private Type? GetCallbackParamClrType(Expr arg, int parameterIndex = 0)
    {
        DataType? paramType = arg switch
        {
            LambdaExpr le when le.Parameters.Length > parameterIndex => le.Parameters[parameterIndex].DataType,
            FunctionReferenceExpr fr when _program.Symbols.GetSymbol(fr.SymbolId).DeclarationNode is FunctionDefinition ffd && ffd.Parameters.Length > parameterIndex => ffd.Parameters[parameterIndex].DataType,
            _ => null
        };
        if (paramType is null or InferredType or UnresolvedQualifiedType) return null;
        return DataTypeMapper.Map(NormalizeInferredType(paramType), _types, _classTypes);
    }





    /// <summary>
    /// ASCAN(array, valore) o ASCAN(array, predicateFunc) — la forma è nota a COMPILE-TIME
    /// dal tipo statico del secondo argomento (IsCallbackArg), non dal tipo dinamico a
    /// runtime come nell'Executor/PCodeVm.
    /// </summary>

    private void EmitStringUtilsCall(BuiltinFunctionCallExpr expr, MethodInfo method, int expectedArgCount)
    {
        if (expr.Args.Length != expectedArgCount)
            throw new NotSupportedException(
                $"Backend IL: '{expr.OriginalToken.Lexeme}' richiede esattamente {expectedArgCount} argomenti.");

        EmitExpression(expr.Args[0]); // Stringa
        EmitInt32Arg(expr.Args[1]);   // Lunghezza
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitMidCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: MID richiede 2 o 3 argomenti.");

        EmitExpression(expr.Args[0]); // Stringa
        EmitInt32Arg(expr.Args[1]);   // Inizio

        if (expr.Args.Length == 3)
        {
            EmitInt32Arg(expr.Args[2]); // Lunghezza
            _il.Emit(OpCodes.Call, StringHelpersMidWithLengthMethod);
        }
        else
        {
            _il.Emit(OpCodes.Call, StringHelpersMidToEndMethod);
        }
    }

    private void EmitInstrCall(BuiltinFunctionCallExpr expr)
    {
        // Due forme BASIC: InStr(haystack, needle [, start]) e l'overload Clipper
        // InStr(start, haystack, needle) (StdLib.Strings — due NativeProcedure con lo
        // stesso TokenKind.Instr). Il runtime StringUtils.Instr(source, substring, start)
        // vuole SEMPRE l'ordine (stringa, sottostringa, start): con l'overload Clipper il
        // 1° arg è numerico → va riordinato, altrimenti EmitInt32Arg(start) su args[0]
        // (stringa) e EmitExpression(args[1]) (stringa) col runtime che si aspetta
        // (source, substring, start) → stack mismatch → InvalidProgram.
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: INSTR richiede 2 o 3 argomenti.");

        var firstIsNumeric = IlConversion.IsNumeric(NormalizeInferredType(InferType(expr.Args[0])));
        if (firstIsNumeric)
        {
            // Overload Clipper: (start, haystack, needle) → riordina per il runtime.
            EmitExpression(expr.Args[1]); // haystack
            EmitExpression(expr.Args[2]); // needle
            EmitInt32Arg(expr.Args[0]);   // start
        }
        else
        {
            EmitExpression(expr.Args[0]); // haystack
            EmitExpression(expr.Args[1]); // needle
            if (expr.Args.Length == 3) EmitInt32Arg(expr.Args[2]);
            else _il.Emit(OpCodes.Ldc_I4_1);
        }
        _il.Emit(OpCodes.Call, StringUtilsInstrMethod);
    }

    private void EmitStringInstanceCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: '{expr.OriginalToken.Lexeme}' richiede esattamente 1 argomento.");

        EmitExpression(expr.Args[0]);
        // Variabile di iterazione su collection boxed (FOR EACH word$ IN MyTrie) o simbolo
        // ridefinito: il local è object (contiene stringhe) — senza castclass, callvirt
        // String::ToUpperInvariant su un object = IL invalido con ILOptim.
        var sArgType = NormalizeInferredType(InferType(expr.Args[0]));
        var isBoxedLocal = expr.Args[0] is VariableExpr sv
            && _locals.TryGetValue(sv.SymbolId, out var sl) && sl.LocalType == typeof(object);
        if (isBoxedLocal || sArgType is AnyType or Basic.JsonType)
            _il.Emit(OpCodes.Castclass, typeof(string));
        _il.Emit(OpCodes.Callvirt, method);
    }

    /// <summary>
    /// Converte l'espressione numerica in Int32 sullo stack. Il Linker inserisce
    /// spesso una conversione implicita a Long (CLNG) sugli argomenti letterali
    /// delle funzioni builtin (es. LEFT$/RIGHT$/INSTR — vedi EmitClngCall), quindi
    /// qui arrivano sia Double sia Long: Double usa Convert.ToInt32(double)
    /// (ARROTONDA, non tronca — stessa semantica dell'Executor esistente); Long è
    /// già un valore integrale, quindi conv.i4 (troncamento) non perde precisione.
    /// </summary>
}
