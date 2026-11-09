// FILE: IlBackend/Partial/IlProgramCompiler.BuiltinsData.cs
// PARTIAL — dominio BUILTIN DATA (read/section/generator/switch/type/interpolated/
// json/dict/ai-vector/mcp/sql/provider/stats/compression/date).
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

/// <summary>Partial del transpiler IL: dominio BUILTIN DATA.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitReadValueCall(BuiltinFunctionCallExpr expr)
    {
        // Firma variabile (parità VM): 1 argomento (key — legge da app.settings.json implicito),
        // oppure 3-4 (file, section, key[, default]).
        if (expr.Args.Length == 1)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ReadValue), typeof(string)));
            return;
        }
        if (expr.Args.Length is < 3 or > 4)
            throw new NotSupportedException("Backend IL: READVALUE richiede 1 o 3-4 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        if (expr.Args.Length == 4) EmitStringArg(expr.Args[3]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ReadValue), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitReadSectionCall(BuiltinFunctionCallExpr expr)
    {
        // Firma variabile (parità VM): 1 argomento (section — legge da app.settings.json implicito),
        // oppure 2 (file, section).
        if (expr.Args.Length == 1)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ReadSection), typeof(string)));
            return;
        }
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: READSECTION richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ReadSection), typeof(string), typeof(string)));
    }

    private void EmitGeneratorExpr(GeneratorExpr expr)
    {
        // (x*x FOR x IN src [IF cond]) — materializzata come List<object> (la stessa
        // semantica della VM: il generatore lazy è materializzato in anticipo; il FOR EACH
        // itera la lista). L'output numerico resta boxed (object) per il FOR EACH generico.
        var resultLocal = _il.DeclareLocal(typeof(System.Collections.Generic.List<object?>));
        _il.Emit(OpCodes.Newobj, typeof(System.Collections.Generic.List<object?>).GetConstructor(Type.EmptyTypes)!);
        _il.Emit(OpCodes.Stloc, resultLocal);
        EmitComprehensionClauses(expr.Clauses, 0, resultLocal, () =>
        {
            _il.Emit(OpCodes.Ldloc, resultLocal);
            EmitExpression(expr.OutputExpression);
            var ot = NormalizeInferredType(InferType(expr.OutputExpression));
            var oClr = DataTypeMapper.Map(ot, _types, _classTypes);
            if (oClr.IsValueType) _il.Emit(OpCodes.Box, oClr);
            _il.Emit(OpCodes.Callvirt, typeof(System.Collections.Generic.List<object?>).GetMethod("Add")!);
        });
        _il.Emit(OpCodes.Ldloc, resultLocal);
    }

    private void EmitSwitchExpression(SwitchExpr expr)
    {
        // SWITCH value CASE a: r1 CASE b: r2 ELSE r3 — per ogni CASE: valuta value+cond,
        // confronta (==), salta se falso; il RESULT; else; fine. Parità VM.
        var endLabel = _il.DefineLabel();
        var valueType = NormalizeInferredType(InferType(expr.Value));
        var isString = valueType is StringType;
        foreach (var clause in expr.Cases)
        {
            var nextCase = _il.DefineLabel();
            if (isString)
            {
                EmitStringArg(expr.Value);
                EmitStringArg(clause.Condition);
            }
            else
            {
                EmitDoubleArgument(expr.Value);
                EmitDoubleArgument(clause.Condition);
            }
            if (isString) _il.Emit(OpCodes.Call, _types.StringEquals);
            else _il.Emit(OpCodes.Ceq);
            _il.Emit(OpCodes.Brfalse, nextCase);
            EmitExpression(clause.Result);
            _il.Emit(OpCodes.Br, endLabel);
            _il.MarkLabel(nextCase);
        }
        if (expr.ElseCase is not null) EmitExpression(expr.ElseCase);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.MarkLabel(endLabel);
    }

    private void EmitTypeExpr(TypeExpr expr)
    {
        // TypeExpr in espressione (la keyword di un tipo come valore): default per il tipo.
        switch (NormalizeInferredType(expr.Type))
        {
            case StringType: _il.Emit(OpCodes.Ldstr, ""); break;
            default: _il.Emit(OpCodes.Ldc_R8, 0.0); break;
        }
    }

    private void EmitInterpolatedString(InterpolatedStringExpr expr)
    {
        // $"testo {expr} {x:fmt}": concatena i segmenti (letterali ldstr + espressioni
        // convertite a stringa; il formato {x:##0.00} usa StringHelpers.Format come FORMAT).
        var first = true;
        foreach (var seg in expr.Segments)
        {
            switch (seg)
            {
                case InterpolatedStringLiteralSegment lit:
                    _il.Emit(OpCodes.Ldstr, lit.Text);
                    break;
                case InterpolatedStringExprSegment es:
                    if (es.Format is not null)
                    {
                        EmitCryptoObjectArgument(es.Expression);
                        _il.Emit(OpCodes.Ldstr, es.Format);
                        _il.Emit(OpCodes.Call, StringHelpersFormatMethod);
                    }
                    else
                    {
                        var segType = InferType(es.Expression);
                        EmitExpression(es.Expression);
                        // Box SOLO per i tipi DINAMICI (JsonValueToString(object)): per i
                        // numerici (Convert.ToString(int16)) il box spingerebbe object sul
                        // metodo int16 → InvalidProgram.
                        if (NormalizeInferredType(segType) is AnyType or Basic.JsonType or NullableType
                            or UnresolvedQualifiedType)
                        {
                            var segClr = DataTypeMapper.Map(NormalizeInferredType(segType), _types, _classTypes);
                            if (segClr.IsValueType) _il.Emit(OpCodes.Box, segClr);
                        }
                        EmitConvertToString(segType);
                    }
                    break;
                default:
                    throw new NotSupportedException($"Backend IL: segmento interpolato '{seg.GetType().Name}' non supportato.");
            }
            if (!first) _il.Emit(OpCodes.Call, _types.StringConcat);
            first = false;
        }
        if (first) _il.Emit(OpCodes.Ldstr, ""); // nessun segmento
    }

    private void EmitJsonToObject(JsonToObjectExpr expr)
    {
        // Parità VM (SysCall.JSON_TO_OBJECT): il valore (stringa JSON o oggetto boxed) →
        // Dictionary/List. Se il TargetType è una UDT, materializza un'istanza tipizzata
        // (struct Udt_*: initobj + stfld per campo) — il codice BASIC ci accede via campo
        // statico (NewUdt.Timeout / NewUdt.Ports(0)). NON si usa ldtoken su TypeBuilder
        // (handle invalido dopo save/reload dell'assembly) né reflection.
        var srcType = NormalizeInferredType(InferType(expr.JsonSource));
        var srcClr = DataTypeMapper.Map(srcType, _types, _classTypes);
        if (expr.TargetType is UserDefinedType udt)
        {
            var udtClr = DataTypeMapper.Map(udt, _types, _classTypes);
            EmitExpression(expr.JsonSource);
            if (srcClr.IsValueType) _il.Emit(OpCodes.Box, srcClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonToObject), typeof(object)));   // [dict]
            var resultLocal = _il.DeclareLocal(udtClr);
            var tmp = _il.DeclareLocal(typeof(object));
            /* initobj marca il locale come "definitely assigned" (il CLR richiede un'assegnazione
             * INTERA prima di ldloc di un value type: le sole stfld per campo non bastano) e azzera
             * i campi — poi le stfld sovrascrivono quelli presenti nel dict */
            _il.Emit(OpCodes.Ldloca, resultLocal);
            _il.Emit(OpCodes.Initobj, udtClr);
            foreach (var field in udt.Fields)
            {
                FieldBuilder? fb = null; Type? fieldType = null;
                if (!field.Dimensions.IsEmpty)
                {
                    if (_classFieldArrays.TryGetValue(field.SymbolId, out var fai)) { fb = fai.Field; fieldType = fai.ClrArrayType; }
                }
                else if (_classFields.TryGetValue(field.SymbolId, out var f)) { fb = f; fieldType = f.FieldType; }
                if (fb is null || fieldType is null || fieldType is TypeBuilder) continue;   /* campo annidato UDT: default */
                _il.Emit(OpCodes.Dup);                                  // [dict, dict]
                _il.Emit(OpCodes.Ldstr, field.Name.Lexeme);             // [dict, dict, name]
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonFieldRaw), typeof(object), typeof(string))); // [dict, raw]
                _il.Emit(OpCodes.Ldtoken, fieldType);                   // [dict, raw, RuntimeTypeHandle]
                _il.Emit(OpCodes.Call, typeof(Type).GetMethod(nameof(Type.GetTypeFromHandle))!);  // [dict, raw, Type]
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonConvertField), typeof(object), typeof(Type))); // [dict, val(object)]
                /* stfld richiede [addr, value] tipizzato: val in temp(object), ldloca+ldloc, cast, stfld */
                _il.Emit(OpCodes.Stloc, tmp);                           // [dict]
                _il.Emit(OpCodes.Ldloca, resultLocal);                  // [dict, addr]
                _il.Emit(OpCodes.Ldloc, tmp);                           // [dict, addr, val(object)]
                if (fieldType.IsValueType) _il.Emit(OpCodes.Unbox_Any, fieldType);
                else _il.Emit(OpCodes.Castclass, fieldType);            // [dict, addr, val(typed)]
                _il.Emit(OpCodes.Stfld, fb);                            // [dict]
            }
            _il.Emit(OpCodes.Pop);                                      // []
            _il.Emit(OpCodes.Ldloc, resultLocal);                       // [udt]
            return;
        }
        EmitExpression(expr.JsonSource);
        if (srcClr.IsValueType) _il.Emit(OpCodes.Box, srcClr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonToObject), typeof(object)));
    }



    /// <summary>KELLYCRITERION(rets[], riskFree): array di DOUBLE + double (firma 2 argomenti).</summary>

    /// <summary>MINCVAR(rets[], conf, rf): array di DOUBLE + due double.</summary>

    /// <summary>AIVECTOR.RERANK(query, resultsJson, topK): IlRuntime.AiVectorRerank(string, string, double).</summary>
    private void EmitAiVectorRerankCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 o 3 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitDoubleArgument(expr.Args[2]); else _il.Emit(OpCodes.Ldc_R8, 5.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorRerank), typeof(string), typeof(string), typeof(double)));
    }

    /// <summary>VECTORDB.PRECISIONAT/RECALLAT/NDCGAT(retrieved$, ideal$, k): (string, string, double) → double.</summary>
    private void EmitRagEvalMetricCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (retrieved$, ideal$, k).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(double)));
    }

    /// <summary>MOUSE.CLICK([x1,y1,x2,y2]): 0 o 4 argomenti → overload runtime.</summary>
    private void EmitMouseClickCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length == 0)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MouseClick), Type.EmptyTypes));
            return;
        }
        if (expr.Args.Length != 4)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 0 o 4 argomenti.");
        foreach (var a in expr.Args) EmitDoubleArgument(a);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MouseClick), typeof(double), typeof(double), typeof(double), typeof(double)));
    }

    /// <summary>MOUSE.BUTTON([id]): 0 o 1 argomento → overload runtime.</summary>
    private void EmitMouseButtonCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length == 0)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MouseButton), Type.EmptyTypes));
            return;
        }
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 0 o 1 argomento.");
        EmitDoubleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MouseButton), typeof(double)));
    }


    /// <summary>Accetta double[] OPPURE double[][] (matrice di storici) — il runtime appiattisce.</summary>
    private void EmitReturnsArgument(Expr argument)
    {
        var t = NormalizeInferredType(InferType(argument));
        if (t is not ArrayType { ElementType: DoubleType }
            and not ArrayType { ElementType: ArrayType { ElementType: DoubleType } })
            throw new NotSupportedException("Backend IL: atteso un array di DOUBLE (o una matrice 2D).");
        EmitExpression(argument);
    }

    private void EmitAiStringsCall(BuiltinFunctionCallExpr expr, string methodName, int arity)
    {
        if (expr.Args.Length != arity) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {arity} argomenti.");
        for (var i = 0; i < arity; i++) EmitStringArg(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, Enumerable.Repeat(typeof(string), arity).ToArray()));
    }
    private void EmitAiVectorEmbedCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: AIVECTOR.EMBED richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorEmbed), typeof(string), typeof(double)));
    }
    private void EmitAiVectorDimsCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length is < 2 or > 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2-4 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length >= 3) EmitDoubleArgument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
        if (expr.Args.Length >= 4) EmitStringArg(expr.Args[3]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(double), typeof(string)));
    }
    private void EmitAiVectorIndexTextCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (methodName == nameof(IlRuntime.AiVectorIndexText))
        {
            if (expr.Args.Length != 4) throw new NotSupportedException("Backend IL: AIVECTOR.INDEXTEXT richiede 4 argomenti.");
            foreach (var arg in expr.Args) EmitStringArg(arg);
            _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string), typeof(string)));
            return;
        }
        if (methodName == nameof(IlRuntime.AiVectorIndexMeta))
        {
            if (expr.Args.Length is < 4 or > 5) throw new NotSupportedException("Backend IL: AIVECTOR.INDEXMETA richiede 4 o 5 argomenti.");
            foreach (var arg in expr.Args) EmitStringArg(arg);
            if (expr.Args.Length == 4) _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string), typeof(string), typeof(string)));
            return;
        }
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: AIVECTOR.REMOVESOURCE richiede 3 argomenti.");
        foreach (var arg in expr.Args) EmitStringArg(arg);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string)));
    }
    private void EmitAiVectorPairCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string)));
    }
    private void EmitAiConsistencyCall(BuiltinFunctionCallExpr expr)
    {
        // AI.CONSISTENCY(prompt, options?)
        // Options: dict with k, temp, maxTokens
        if (expr.Args.Length < 1 || expr.Args.Length > 2)
            throw new NotSupportedException("Backend IL: AI.CONSISTENCY richiede 1-2 argomenti (prompt, options?).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2)
        {
            // Emit options dict - for simplicity emit as object, runtime will parse
            EmitExpression(expr.Args[1]);
        }
        else
        {
            _il.Emit(OpCodes.Ldnull);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiConsistency), typeof(string), typeof(object)));
    }
    private void EmitAiLexicalEntropyCall(BuiltinFunctionCallExpr expr)
    {
        // AI.LEXICALENTROPY(responses$): UNA stringa delimitata (';'/newline). Prima
        // l'emitter spingeva una stringa ma chiamava un metodo con firma `string[]`
        // (AiLexicalEntropy(string[])) → IL INVALIDO (InvalidProgramException). Il contratto
        // è la stringa, coerente con StdLib/`gmb_lexical_entropy` del runtime C.
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: AI.LEXICALENTROPY richiede 1 argomento (responses$).");
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiLexicalEntropy), typeof(string)));
    }
    private void EmitAiVectorTopKCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (methodName == nameof(IlRuntime.AiVectorSearch))
        {
            // AIVECTOR.SEARCH(db, coll, query [, topK] [, hybrid] [, filter] [, reranker] [, minEvidence] [, explain])
            if (expr.Args.Length is < 3 or > 9)
                throw new NotSupportedException("Backend IL: AIVECTOR.SEARCH richiede da 3 a 9 argomenti.");
            EmitStringArg(expr.Args[0]);
            EmitStringArg(expr.Args[1]);
            EmitStringArg(expr.Args[2]);
            if (expr.Args.Length >= 4) EmitDoubleArgument(expr.Args[3]);
            else _il.Emit(OpCodes.Ldc_R8, 5.0);
            if (expr.Args.Length >= 5) EmitDoubleArgument(expr.Args[4]);
            else _il.Emit(OpCodes.Ldc_R8, 1.0);
            if (expr.Args.Length >= 6) EmitStringArg(expr.Args[5]);
            else _il.Emit(OpCodes.Ldnull);
            if (expr.Args.Length >= 7) EmitDoubleArgument(expr.Args[6]);
            else _il.Emit(OpCodes.Ldc_R8, 1.0);              // %RERANK_KEYWORD default
            if (expr.Args.Length >= 8) EmitDoubleArgument(expr.Args[7]);
            else _il.Emit(OpCodes.Ldc_R8, double.NaN);        // minEvidence off
            if (expr.Args.Length >= 9) EmitDoubleArgument(expr.Args[8]);
            else _il.Emit(OpCodes.Ldc_R8, 0.0);               // explain off
            _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string),
                typeof(double), typeof(double), typeof(string), typeof(double), typeof(double), typeof(double)));
            return;
        }
        // AIVECTOR.RAGCONTEXT / AIVECTOR.ASK(db, coll, query [, topK] [, reranker] [, minEvidence])
        if (expr.Args.Length is < 3 or > 6) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3-6 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        if (expr.Args.Length >= 4) EmitDoubleArgument(expr.Args[3]);
        else _il.Emit(OpCodes.Ldc_R8, 5.0);
        if (expr.Args.Length >= 5) EmitDoubleArgument(expr.Args[4]);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);
        if (expr.Args.Length >= 6) EmitDoubleArgument(expr.Args[5]);
        else _il.Emit(OpCodes.Ldc_R8, double.NaN);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(string), typeof(string),
            typeof(double), typeof(double), typeof(double)));
    }
    private void EmitAiVectorLoadDocsCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 3 or > 5) throw new NotSupportedException("Backend IL: AIVECTOR.LOADDOCUMENTS richiede 3-5 argomenti.");
        for (var i = 0; i < expr.Args.Length; i++) EmitStringArg(expr.Args[i]);
        for (var i = expr.Args.Length; i < 5; i++) _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorLoadDocuments), typeof(string), typeof(string), typeof(string), typeof(string), typeof(string)));
    }
    private void EmitAiVectorGenTestCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 3 or > 3) throw new NotSupportedException("Backend IL: AIVECTOR.GENTEST richiede 3 argomenti (collection, n, types).");
        for (var i = 0; i < 3; i++) EmitStringArg(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorGenTest), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitAiVaultCreateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException("Backend IL: VAULT.CREATE richiede 2 o 3 argomenti (name, path [, pattern]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VaultCreate), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitAiVaultIndexCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1) throw new NotSupportedException("Backend IL: VAULT.INDEX richiede 0 o 1 argomento (name).");
        if (expr.Args.Length == 1) EmitStringArg(expr.Args[0]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VaultIndex), typeof(string)));
    }

    // ── BRAIN multi-use (R7) ───────────────────────────────────────────────
    private void EmitAiSearchMultiCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 7)
            throw new NotSupportedException("Backend IL: SEARCHMULTI richiede 1-7 argomenti (query [, topK] [, collection] [, brains] [, reranker] [, minEvidence] [, explain]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 5.0);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldnull);
        if (expr.Args.Length >= 4) EmitStringArg(expr.Args[3]);
        else _il.Emit(OpCodes.Ldnull);
        if (expr.Args.Length >= 5) EmitDoubleArgument(expr.Args[4]);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);              // %RERANK_KEYWORD default
        if (expr.Args.Length >= 6) EmitDoubleArgument(expr.Args[5]);
        else _il.Emit(OpCodes.Ldc_R8, double.NaN);        // minEvidence disattivato
        if (expr.Args.Length >= 7) EmitDoubleArgument(expr.Args[6]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);               // explain off
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiSearchMulti), typeof(string), typeof(double), typeof(string), typeof(string), typeof(double), typeof(double), typeof(double)));
    }

    private void EmitAiRagContextMultiCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 4)
            throw new NotSupportedException("Backend IL: RAGCONTEXTMULTI richiede 1-4 argomenti (query [, topK] [, reranker] [, minEvidence]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 5.0);
        if (expr.Args.Length >= 3) EmitDoubleArgument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);
        if (expr.Args.Length >= 4) EmitDoubleArgument(expr.Args[3]);
        else _il.Emit(OpCodes.Ldc_R8, double.NaN);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiRagContextMulti), typeof(string), typeof(double), typeof(double), typeof(double)));
    }

    private void EmitAiAskMultiCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 4)
            throw new NotSupportedException("Backend IL: ASKMULTI richiede 1-4 argomenti (question [, topK] [, reranker] [, minEvidence]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 5.0);
        if (expr.Args.Length >= 3) EmitDoubleArgument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);
        if (expr.Args.Length >= 4) EmitDoubleArgument(expr.Args[3]);
        else _il.Emit(OpCodes.Ldc_R8, double.NaN);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAskMulti), typeof(string), typeof(double), typeof(double), typeof(double)));
    }

    private void EmitGraphQueryMultiCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: GRAPH.QUERYMULTI richiede 1 o 2 argomenti (query [, brains]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GraphQueryMulti), typeof(string), typeof(string)));
    }

    private void EmitGraphPathCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: GRAPH.PATH richiede 3 argomenti (brain, from, to).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GraphPath), typeof(string), typeof(string), typeof(string)));
    }

    // ── GRAPH command (Second Brain L5, legacy) ────────────────────────────
    private void EmitGraphBuildCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: GRAPH BUILD richiede 1 o 2 argomenti (dir [, backend]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GraphBuild), typeof(string), typeof(string)));
    }

    private void EmitGraphFindPathCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: GRAPH FIND PATH richiede 2 argomenti (from, to).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GraphFindPath), typeof(string), typeof(string)));
    }

    // ── Gap revisione (F7 P3): folder props / link manuali / sync / web scrape ──
    private void EmitVaultAddFolderCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI VAULT ADD FOLDER richiede 2-3 argomenti (name, folder [, indexed]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VaultAddFolder), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitVaultSetFolderPropsCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: AI VAULT SET FOLDER richiede 3 argomenti (name, folder, indexed).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Ldnull); // tags
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VaultSetFolderProps), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitNoteLinkCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 3 or > 4)
            throw new NotSupportedException("Backend IL: NOTE.LINK richiede 3-4 argomenti (vault, from, to [, alias]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        if (expr.Args.Length == 4) EmitStringArg(expr.Args[3]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NoteLink), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitNoteUnlinkCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: NOTE.UNLINK richiede 3 argomenti (vault, from, to).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NoteUnlink), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitNoteSetAliasCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 4)
            throw new NotSupportedException("Backend IL: NOTE.SETALIAS richiede 4 argomenti (vault, from, to, alias).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]); EmitStringArg(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NoteSetAlias), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitSyncFolderCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 4)
            throw new NotSupportedException("Backend IL: AI SYNC FOLDER richiede 2-4 argomenti (src, vault [, direction [, conflict]]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]); else _il.Emit(OpCodes.Ldnull);
        if (expr.Args.Length >= 4) EmitStringArg(expr.Args[3]); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SyncFolder), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitIngestWebCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI INGEST WEB richiede 2-3 argomenti (vault, url [, relPath]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IngestWeb), typeof(string), typeof(string), typeof(string)));
    }

    // ── ENRICH / QA (Second Brain §2) ─────────────────────────────────────
    private void EmitEnrichCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.ENRICH richiede 2 argomenti (text, mode).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Enrich), typeof(string), typeof(string)));
    }

    private void EmitQaAnswerCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI.QAANSWER richiede 2 o 3 argomenti (question, brain [, window]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitDoubleArgument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_R8, 3.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.QaAnswer), typeof(string), typeof(string), typeof(double)));
    }

    // ── PROVENANCE (Second Brain §2) ──────────────────────────────────────
    private void EmitSignNoteCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.SIGNNOTE richiede 2 argomenti (content, key).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SignNote), typeof(string), typeof(string)));
    }

    private void EmitVerifyNoteCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: AI.VERIFYNOTE richiede 3 argomenti (content, key, sig).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VerifyNote), typeof(string), typeof(string), typeof(string)));
    }

    // ── BRAIN VAULT (Second Brain §2) ─────────────────────────────────────
    private void EmitBrainVaultBuildCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: AI.BRAINVAULTBUILD richiede 1 argomento (outputDir).");
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrainVaultBuild), typeof(string)));
    }

    private void EmitBrainVaultQueryCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI.BRAINVAULTQUERY richiede 1 o 2 argomenti (query [, topK]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 3.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrainVaultQuery), typeof(string), typeof(double)));
    }

    private void EmitBrainVaultWriteCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI.BRAINVAULTWRITE richiede 1 o 2 argomenti (request [, topK]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 3.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrainVaultWrite), typeof(string), typeof(double)));
    }


    private void EmitNoteCreateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException("Backend IL: NOTE.CREATE richiede 1-3 argomenti (path [, content] [, tags]).");
        _il.Emit(OpCodes.Ldstr, ""); // vault = brain attivo
        foreach (var a in expr.Args) EmitStringArg(a);
        for (var i = expr.Args.Length; i < 3; i++) _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.NoteCreate), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>Comando AI NOTE (UPDATE/APPEND/MOVE/DELETE): antepone vault="" (brain attivo)
    /// agli argomenti posizionali. Firme: Update/Append/Move(string,string,string),
    /// Delete(string,string).</summary>
    private void EmitNoteCmdCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        var expected = methodName == nameof(IlRuntime.NoteDelete) ? 2 : 3;
        if (expr.Args.Length != expected - 1)
            throw new NotSupportedException($"Backend IL: NOTE.{methodName} richiede {expected - 1} argomenti.");
        _il.Emit(OpCodes.Ldstr, ""); // vault = brain attivo
        foreach (var a in expr.Args) EmitStringArg(a);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName,
            expected == 3 ? [typeof(string), typeof(string), typeof(string)] : [typeof(string), typeof(string)]));
    }

    private void EmitIngestFileCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: INGEST.FILE richiede 1 o 2 argomenti (path [, rel]).");
        _il.Emit(OpCodes.Ldstr, ""); // vault = brain attivo
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IngestFile), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitIngestFolderCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: INGEST.FOLDER richiede 1 o 2 argomenti (path [, preserve]).");
        _il.Emit(OpCodes.Ldstr, ""); // vault = brain attivo
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IngestFolder), typeof(string), typeof(string), typeof(double)));
    }

    private void EmitMcpRegisterToolCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 5) throw new NotSupportedException("Backend IL: MCP.REGISTERTOOL richiede 2-5 argomenti (name, proc [, desc [, readOnly [, outputSchema]]]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        if (expr.Args.Length >= 4) EmitConditionBool(expr.Args[3]);
        else _il.Emit(OpCodes.Ldc_I4_0);
        // inputSchema dal COMPILER, non dalla reflection a runtime: `_basicProcedures` è una
        // dictionary PROCESS-WIDE condivisa col percorso P-Code (RegisterPCodeProcedure usa un
        // symbolId negativo) → in un processo che esegue più programmi/gambe la derivazione per
        // reflection falliva e lo schema diventava {} (divergenza IL ↔ PCode ↔ VM/C su
        // AI.MCPLISTTOOLS()). Stringa vuota = procedura non risolvibile qui (es. tool browser_*):
        // il runtime ricade sul suo fallback (BrowserToolSchema), come prima.
        _il.Emit(OpCodes.Ldstr, McpInputSchemaJson(expr));
        if (expr.Args.Length >= 5) EmitStringArg(expr.Args[4]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.McpRegisterToolWithSchema),
            typeof(string), typeof(string), typeof(string), typeof(bool), typeof(string), typeof(string)));
    }

    /// <summary>inputSchema JSON della PROCEDURA referenziata dal 2° argomento di
    /// `AI MCP REGISTER TOOL` (stessa ABI del compiler P-Code e dei codegen C: argomento
    /// sintetico in posizione 4). Ritorna "" se la procedura non è risolvibile a
    /// compile-time (il runtime ricade sulla reflection/BrowserToolSchema).</summary>
    private string McpInputSchemaJson(BuiltinFunctionCallExpr expr)
    {
        var procName = expr.Args.Length > 1 ? (expr.Args[1] as StringExpr)?.Value : null;
        if (string.IsNullOrWhiteSpace(procName)) return "";
        var proc = _program.Symbols.GetAllProcedures()
            .FirstOrDefault(p => string.Equals(p.OriginalName.Lexeme, procName, StringComparison.OrdinalIgnoreCase));
        if (proc is null || proc.Parameters.Length == 0) return "";
        var pars = new List<(string Name, string TypeName, bool Optional)>();
        foreach (var p in proc.Parameters)
            pars.Add((p.OriginalName.Lexeme,
                      DataTypeToBasicType(p.DataType, p.Flags.HasFlag(SymbolFlags.IsArray)), false));
        return Basic.Mcp.McpSchemaBuilder.FromParameters(pars).ToJsonString();
    }

    /// <summary>Mapping DataType → nome tipo BASIC per McpSchemaBuilder (specchio di
    /// C99CodeEmitter.DataTypeToBasicType / IlRuntime.ClrToBasicType).</summary>
    private static string DataTypeToBasicType(Basic.DataType dt, bool isArray) => dt switch
    {
        StringType => "STRING",
        BitType => "BIT",
        IntegerType or LongType or ByteType => "INTEGER",
        DoubleType or SingleType => "DOUBLE",
        Basic.JsonType => "JSON",
        ArrayType => "ARRAY OF " + (dt is ArrayType at && at.ElementType is StringType ? "STRING" : "NUMBER"),
        _ => isArray ? "ARRAY OF NUMBER" : "DOUBLE"
    };
    private void EmitConnectorRegisterCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException("Backend IL: CONNECTOR.REGISTER richiede 2 o 3 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) { EmitStringArg(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConnectorRegister3), typeof(string), typeof(string), typeof(string))); }
        else _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConnectorRegister), typeof(string), typeof(string)));
    }
    private void EmitMcpServeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 0 or > 3) throw new NotSupportedException("Backend IL: MCP.SERVE richiede 0-3 argomenti (mode [, port [, readOnly]]).");
        if (expr.Args.Length == 0) _il.Emit(OpCodes.Ldstr, "stdio");
        else EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, Basic.Mcp.McpHttpOptions.DefaultPort);
        if (expr.Args.Length >= 3) EmitConditionBool(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.McpServe), typeof(string), typeof(double), typeof(bool)));
    }
    private void EmitSqlExecFnCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 2) throw new NotSupportedException("Backend IL: SQL.EXEC richiede almeno 2 argomenti (alias, sql).");
        EmitSqlAliasString(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        // Parametri extra (posizionali o un DICT named) → object[].
        var extraCount = expr.Args.Length - 2;
        _il.Emit(OpCodes.Ldc_I4, extraCount);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var i = 0; i < extraCount; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(expr.Args[i + 2]);
            var t = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i + 2])), _types, _classTypes);
            if (t.IsValueType) _il.Emit(OpCodes.Box, t);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlExec), typeof(string), typeof(string), typeof(object[])));
    }
    // G10 (2026-10-09): ergonomia DataFrame (DESCRIBE/TOCSV/FROMCSV).
    private void EmitSqlDfSchemaCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: SQL.DESCRIBE richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        var st = NormalizeInferredType(InferType(expr.Args[0]));
        IlConversion.EmitConvert(_il, _types, st, DataTypeMapper.Map(st, _types, _classTypes), _types.Double);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlDfSchema)));
    }
    private void EmitSqlToCsvCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: SQL.TOCSV richiede 2 argomenti.");
        EmitExpression(expr.Args[0]);
        var st = NormalizeInferredType(InferType(expr.Args[0]));
        IlConversion.EmitConvert(_il, _types, st, DataTypeMapper.Map(st, _types, _classTypes), _types.Double);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlToCsv), typeof(double), typeof(string)));
    }
    private void EmitSqlFromCsvCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: SQL.FROMCSV richiede 3 argomenti.");
        for (var i = 0; i < 3; i++) EmitStringArg(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlFromCsv), typeof(string), typeof(string), typeof(string)));
    }
    // G10: SQL.COL(rs, col) → ARRAY (object[], misto double/string).
    private void EmitSqlColCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: SQL.COL richiede 2 argomenti.");
        EmitExpression(expr.Args[0]);
        var st = NormalizeInferredType(InferType(expr.Args[0]));
        IlConversion.EmitConvert(_il, _types, st, DataTypeMapper.Map(st, _types, _classTypes), _types.Double);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlCol), typeof(double), typeof(string)));
    }
    private void EmitProviderStringsCall(BuiltinFunctionCallExpr expr, string methodName, int arity)
    {
        if (expr.Args.Length != arity) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {arity} argomenti.");
        for (var i = 0; i < arity; i++) EmitStringArg(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, Enumerable.Repeat(typeof(string), arity).ToArray()));
    }
    private void EmitAiRegisterCall(BuiltinFunctionCallExpr expr)
    {
        // AI REGISTER alias, capability, source [, param] → AiRegisterSource(alias, cap, source [, param]).
        if (expr.Args.Length is < 3 or > 4)
            throw new NotSupportedException("Backend IL: AI.REGISTER richiede 3 o 4 argomenti.");
        for (var i = 0; i < 3; i++) EmitStringArg(expr.Args[i]);
        if (expr.Args.Length == 4) EmitStringArg(expr.Args[3]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiRegisterSource),
            typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitAiVerifyCall(BuiltinFunctionCallExpr expr)
    {
        // AI VERIFY <attempt> REQUIRE <require> → AiVerify(action, require, 3.0, names, values).
        // Il REQUIRE è valutato a runtime con le LOCALI del chiamante (snapshot come i siti
        // EVAL/&) + RESULT$ = risultato del tentativo. Ritorna la stringa finale accettata.
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI VERIFY richiede attempt e REQUIRE.");
        var entries = CollectEvalLocals();
        var actT = _il.DeclareLocal(typeof(string));
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Stloc, actT);
        var reqT = _il.DeclareLocal(typeof(string));
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Stloc, reqT);
        var namesTemp = _il.DeclareLocal(typeof(string[]));
        _il.Emit(OpCodes.Ldc_I4, entries.Count);
        _il.Emit(OpCodes.Newarr, typeof(string));
        _il.Emit(OpCodes.Stloc, namesTemp);
        for (var i = 0; i < entries.Count; i++)
        {
            _il.Emit(OpCodes.Ldloc, namesTemp);
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Ldstr, entries[i].Name);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        var valuesTemp = _il.DeclareLocal(typeof(object[]));
        _il.Emit(OpCodes.Ldc_I4, entries.Count);
        _il.Emit(OpCodes.Newarr, typeof(object));
        _il.Emit(OpCodes.Stloc, valuesTemp);
        for (var i = 0; i < entries.Count; i++)
        {
            _il.Emit(OpCodes.Ldloc, valuesTemp);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitEvalLocalLoad(entries[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Ldloc, actT);
        _il.Emit(OpCodes.Ldloc, reqT);
        _il.Emit(OpCodes.Ldc_R8, 3.0);
        _il.Emit(OpCodes.Ldloc, namesTemp);
        _il.Emit(OpCodes.Ldloc, valuesTemp);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVerify),
            typeof(string), typeof(string), typeof(double), typeof(string[]), typeof(object?[])));
    }

    private void EmitAiSkillsMarketplaceCall(BuiltinFunctionCallExpr expr)
    {
        // AI SKILLS MARKETPLACE url [, dest] → AiSkillsMarketplace(url [, dest]).
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI.SKILLSMARKETPLACE richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiSkillsMarketplace),
            typeof(string), typeof(string)));
    }

    private void EmitAiPluginsCreateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.PLUGINSCREATE richiede 2 argomenti (name, description).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiPluginsCreate),
            typeof(string), typeof(string)));
    }

    private void EmitAiPluginsMarketplaceCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI.PLUGINSMARKETPLACE richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiPluginsMarketplace),
            typeof(string), typeof(string)));
    }

    private void EmitAiPluginCallCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI.PLUGINCALL richiede 1 o 2 argomenti (name [, arg]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiPluginCall),
            typeof(string), typeof(string)));
    }

    /// <summary>PROCESS &lt;file&gt; PROMPT &lt;prompt&gt; (E0): IlRuntime.ProcessDocument(file, prompt).
    /// Il risultato (la risposta dell'agente) è stampato DENTRO il runtime (PrintDispatch) —
    /// comportamento unificato su tutti i backend; qui il valore è scartato.</summary>
    private void EmitProcessCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: PROCESS richiede 1 o 2 argomenti (file [, prompt]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ProcessDocument),
            typeof(string), typeof(string)));
        _il.Emit(OpCodes.Pop);
    }

    private void EmitProviderNameValueCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (name, value).");
        EmitStringArg(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(double)));
    }
    private void EmitProviderDiscoverCall(BuiltinFunctionCallExpr expr)
    {
        // DISCOVER(prefix, capability, baseUrl, apiKey [, soloFree]) — 4 stringhe + 1 double
        // opzionale (soloFree, default 0).
        if (expr.Args.Length is < 4 or > 5)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 o 5 argomenti.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        EmitStringArg(expr.Args[3]);
        if (expr.Args.Length == 5) EmitDoubleArgument(expr.Args[4]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ProviderDiscover),
            typeof(string), typeof(string), typeof(string), typeof(string), typeof(double)));
    }

    private void EmitProviderResetCall(BuiltinFunctionCallExpr expr)    {
        if (expr.Args.Length > 1) throw new NotSupportedException("Backend IL: PROVIDER.RESETFAILED richiede 0 o 1 argomento.");
        if (expr.Args.Length == 1) EmitStringArg(expr.Args[0]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ProviderResetFailed), typeof(string)));
    }

    private void EmitClipCall(BuiltinFunctionCallExpr expr)
    {
        // CLIP.GET()/CLIP.PUT(v): firma runtime Clip(command, arg). GET → stringa vuota.
        _il.Emit(OpCodes.Ldstr, expr.Name.Lexeme.ToUpperInvariant());
        if (expr.Name.Lexeme.Equals("PUT", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 1)
        {
            EmitStringArg(expr.Args[0]);
        }
        else if (expr.Name.Lexeme.Equals("GET", StringComparison.OrdinalIgnoreCase) && expr.Args.Length == 0)
        {
            _il.Emit(OpCodes.Ldnull);
        }
        else
        {
            throw new NotSupportedException($"Backend IL: CLIP.{expr.Name.Lexeme} non supportato (GET()/PUT(value)).");
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Clip), typeof(string), typeof(string)));
    }


    // MEMORY.*: superficie RIMOSSA (shared/libs/memory via FFI).

    private void EmitExtractCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: EXTRACT richiede 2 argomenti (json, key).");
        EmitExpression(expr.Args[0]);
        var t0 = NormalizeInferredType(InferType(expr.Args[0]));
        var clr0 = DataTypeMapper.Map(t0, _types, _classTypes);
        if (clr0.IsValueType) _il.Emit(OpCodes.Box, clr0);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Extract), typeof(object), typeof(string)));
    }

    private void EmitObjectArgsCall(BuiltinFunctionCallExpr expr, string methodName, int arity)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {arity} argomenti.");
        foreach (var a in expr.Args)
        {
            EmitExpression(a);
            var t = NormalizeInferredType(InferType(a));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

}
