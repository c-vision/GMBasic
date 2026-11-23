// FILE: IlBackend/Partial/IlProgramCompiler.BuiltinsString.cs
// PARTIAL — dominio BUILTIN STRING/DATE/FINANCE/CRYPTO/LINALG/ANALYSIS (call emit).
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

/// <summary>Partial del transpiler IL: dominio BUILTIN STRING/DATE/FINANCE/CRYPTO/LINALG/ANALYSIS.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitStringUnaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");
        EmitExpression(expr.Args[0]);
        // Argomenti numerici (es. PROVIDER.ACTIVE(%CHAT) o TRIM$(n) con n numerico): il
        // builtin attende string, ma lo stack ha un valore numerico nativo → box + dispatch
        // runtime (JsonValueToString) per evitare mismatch CLR e IL invalido.
        var argType = NormalizeInferredType(InferType(expr.Args[0]));
        if (argType is not StringType and not AnyType and not Basic.JsonType)
        {
            EmitDynamicValueBox(expr.Args[0]);
            _il.Emit(OpCodes.Call, IlRuntimeJsonValueToStringMethod);
        }
        // I simboli ridefiniti (`S AS SINGLE` / `S$ AS STRING`) vivono in un local
        // object. Quando l'uso corrente è stringa, ripristina il tipo richiesto dalla
        // firma CLR del builtin prima della call (VAL/ASC/TRIM/...).
        if (expr.Args[0] is VariableExpr variable
            && _locals.TryGetValue(variable.SymbolId, out var local)
            && local.LocalType == typeof(object))
            _il.Emit(OpCodes.Castclass, typeof(string));
        // VAL(x) con x DINAMICO (JSON/ANY): il valore a runtime può essere un NUMERO
        // (`VAL(body.prezzo)` con `body` JSON dal body della request: a runtime è un Double) →
        // `castclass string` su un Double lancia InvalidCastException (bug MVC/IL rotta POST:
        // `a["prezzo"] = VAL(body.prezzo)`). Conversione TOLLERANTE: JsonValueToString
        // gestisce double/string/bool (invariant), poi VAL(string). Vale SOLO per VAL: gli
        // altri builtin di questa helper ricevono davvero stringhe e il cast è corretto per loro.
        else if (ReferenceEquals(method, StringHelpersValMethod))
            _il.Emit(OpCodes.Call, IlRuntimeJsonValueToStringMethod);
        // Variabile di iterazione di un FOR EACH su collection boxed (es. `FOR EACH word$
        // IN MyTrie`): il symbol è AnyType/JsonType (local object) ma contiene stringhe —
        // senza castclass, callvirt String::ToUpperInvariant su un object = IL invalido
        // con ILOptim. Il SymbolId della variabile di iterazione NON matcha i _locals
        // (registrata per nome/scope dal Linker), da qui il check per InferType.
        else if (NormalizeInferredType(InferType(expr.Args[0])) is AnyType or Basic.JsonType)
            _il.Emit(OpCodes.Castclass, typeof(string));
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitChrCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: CHR richiede esattamente 1 argomento.");
        EmitExpression(expr.Args[0]);
        var sourceType = NormalizeInferredType(InferType(expr.Args[0]));
        var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, _types.Int64);
        _il.Emit(OpCodes.Call, StringHelpersChrMethod);
    }

    private void EmitCStrCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: CSTR richiede esattamente 1 argomento.");
        EmitExpression(expr.Args[0]);
        // CSTR(BIT) → "True"/"False" (canonico: isam_protocol41 `CSTRbool:[True]`). Il
        // valore BIT sullo stack è un I4 (bool 0/1 oppure int32 -1/0/numero): il box come
        // Boolean lo normalizza a vero/falso, come le gambe C (`b ? "True" : "False"`).
        // Prima, per le variabili (local int32), si boxava Int32 e usciva "-1".
        if (NormalizeInferredType(InferType(expr.Args[0])) is BitType)
        {
            _il.Emit(OpCodes.Box, _types.Boolean);
            _il.Emit(OpCodes.Call, StringHelpersCStrMethod);
            return;
        }
        // Box col tipo CLR REALE del valore sullo stack (come EmitDynamicValueBox), NON col
        // tipo statico da DataTypeMapper: un BIT (DIM bResult AS BIT) vive in un local
        // int32 (GetOrDeclareLocal) ma DataTypeMapper.Map(BitType) = System.Boolean →
        // box Boolean su un int32 = IL invalido che il JIT rifiuta nei metodi grandi
        // (DbIsamFullTests Proc_601, `IF CONVERSIONS.CSTR(bResult) THEN`).
        Type clr;
        if (expr.Args[0] is VariableExpr cv && _locals.TryGetValue(cv.SymbolId, out var cb)) clr = cb.LocalType;
        else
        {
            var cType = NormalizeInferredType(InferType(expr.Args[0]));
            clr = DataTypeMapper.Map(cType, _types, _classTypes);
        }
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, StringHelpersCStrMethod);
    }

    private void EmitInt64StringBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");
        EmitExpression(expr.Args[0]);
        var sourceType = NormalizeInferredType(InferType(expr.Args[0]));
        var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
        IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, _types.Int64);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitDupCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: DUP richiede esattamente 2 argomenti.");
        EmitExpression(expr.Args[0]);
        var countType = NormalizeInferredType(InferType(expr.Args[0]));
        IlConversion.EmitConvert(_il, _types, countType, DataTypeMapper.Map(countType, _types, _classTypes), _types.Int64);
        // Secondo argomento: stringa (DUP(n, "x")) o NUMERO (DUP(n, 42) — il char ASCII,
        // come la VM) → box object → IlRuntime.Dup(long, object).
        EmitExpression(expr.Args[1]);
        var valClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[1])), _types, _classTypes);
        if (valClr.IsValueType) _il.Emit(OpCodes.Box, valClr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Dup), typeof(long), typeof(object)));
    }

    private void EmitStringBinaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti.");
        // I parametri del metodo sono string: un argomento numerico va convertito, altrimenti
        // si emette un double sullo slot string -> InvalidProgramException a runtime
        // (es. `AI SET TEMP "openrouter", 0.7`, bug AiProviders).
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); _il.Emit(OpCodes.Call, method);
    }
    /// <summary>MEMORYASK(query[, topK]): primo argomento sempre, secondo opzionale.</summary>
    private void EmitMemoryAskCall(BuiltinFunctionCallExpr expr, string? methodName = null)
    {
        methodName ??= nameof(IlRuntime.MemoryAsk);
        if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException($"Backend IL: {methodName} richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_R8, 5.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string), typeof(double)));
    }
    private void EmitAiAgentAskStream(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (nome, prompt, callback).");
        EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]);
        if (expr.Args[2] is not StringExpr callbackName)
            throw new NotSupportedException("Backend IL: AI AGENT ASK ... INTO richiede il nome di una SUB.");
        var normalized = callbackName.Value.Replace("_", "");
        var definition = _procedureDefinitions.Values
            .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(normalized, StringComparison.OrdinalIgnoreCase));
        if (definition is null)
            throw new NotSupportedException($"Backend IL: procedura '{callbackName.Value}' non trovata per la callback ASK.");
        if (definition.Parameters.Length != 1 || definition.Parameters[0].DataType is not StringType)
            throw new NotSupportedException($"Backend IL: la callback '{callbackName.Value}' deve essere una SUB con un parametro STRING.");
        // Il dispatch avviene come TASK GPM (IlRuntime.DispatchHandlerAsGpmOrDirect) —
        // la callback NON gira mai sul thread di Task.Run.
        _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAgentAskStream)));
    }
    private void EmitStringTernaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti.");
        foreach (var argument in expr.Args) EmitExpression(argument); _il.Emit(OpCodes.Call, method);
    }
    private void EmitValidationLengthCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (value, min, max).");
        EmitExpression(expr.Args[0]);          // string
        EmitDoubleArgument(expr.Args[1]);      // min come double
        EmitDoubleArgument(expr.Args[2]);      // max come double
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsValidLength), typeof(string), typeof(double), typeof(double)));
    }
    private void EmitStringRepeatCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException("Backend IL: REPEAT richiede 2 o 3 argomenti.");
        // Primo argomento SEMPRE stringa: REPEAT ha firma (string, long, string?). Se arriva un
        // numero (es. il vecchio `STRING$(3, 65)` riscritto senza CHR) l'IL risultava INVALIDO
        // (InvalidProgramException a runtime). Caso trovato dal MiniTest strings/string_dollar.
        EmitExpression(expr.Args[0]);
        {
            var firstType = NormalizeInferredType(InferType(expr.Args[0]));
            if (firstType is not StringType and not AnyType)
                EmitConvertToString(firstType);
        }
        EmitInt64Argument(expr.Args[1]);
        if (expr.Args.Length == 3) EmitExpression(expr.Args[2]); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, StringHelpersRepeatMethod);
    }
    private void EmitStringPadCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 o 3 argomenti.");
        EmitExpression(expr.Args[0]); EmitInt64Argument(expr.Args[1]);
        if (expr.Args.Length == 3) EmitExpression(expr.Args[2]); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, method);
    }
    private void EmitStringWrapCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException("Backend IL: WRAP richiede 2 o 3 argomenti.");
        EmitExpression(expr.Args[0]); EmitInt64Argument(expr.Args[1]);
        if (expr.Args.Length == 3) EmitInt64Argument(expr.Args[2]); else _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_I8);
        _il.Emit(OpCodes.Call, StringHelpersWrapMethod);
    }
    private void EmitStringSplitCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: SPLIT richiede testo e separatore opzionale."); EmitExpression(expr.Args[0]); if (expr.Args.Length == 2) EmitExpression(expr.Args[1]); else _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, StringHelpersSplitMethod); }
    private void EmitStringJoinCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: JOIN richiede array e separatore."); EmitCryptoObjectArgument(expr.Args[0]); EmitExpression(expr.Args[1]); _il.Emit(OpCodes.Call, StringHelpersJoinMethod); }
    private void EmitFormatCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: FORMAT richiede valore e formato."); EmitCryptoObjectArgument(expr.Args[0]); EmitExpression(expr.Args[1]); _il.Emit(OpCodes.Call, StringHelpersFormatMethod); }
    private void EmitInt64Argument(Expr expr)
    {
        EmitExpression(expr); var type = NormalizeInferredType(InferType(expr));
        IlConversion.EmitConvert(_il, _types, type, DataTypeMapper.Map(type, _types, _classTypes), _types.Int64);
    }

    // ── ON KEY / ON MOUSE / ON WINDOW ────────────────────────────────
    // Porting da GMBasic (commit 5485907e). Compilavano a no-op (case: break;). Solo l'azione
    // CALL è supportata — vedi il commento in IlRuntime.RegisterKeyHandler per il perché
    private void EmitOnKeyStatement(OnKeyStmt stmt)
    {
        if (stmt.KeyExpression is null)
            throw new NotSupportedException("Backend IL: ON KEY STOP globale (senza tasto specifico) non supportato.");
        switch (stmt.Action)
        {
            case StopAction:
                EmitInt32Argument(stmt.KeyExpression);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.UnregisterKeyHandler)));
                break;
            case CallAction callAction:
                if (callAction.Call is not FunctionExpr callExpr)
                    throw new NotSupportedException("Backend IL: azione ON KEY CALL non valida: attesa una chiamata a procedura.");
                // SymbolId → dispatch come TASK GPM (mai sul thread del dispatcher esterno).
                EmitInt32Argument(stmt.KeyExpression);
                _il.Emit(OpCodes.Ldc_I4, callExpr.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterKeyHandlerBySymbol)));
                break;
            case GosubAction gosubAction:
                // ON KEY ... GOSUB <handler>: registra un handler. Risolve la procedura
                // (Proc_<symbolId>) a RUNTIME via RegisterKeyHandlerBySymbol — ldftn su un
                // MethodBuilder non ancora CreateType lancia InvalidProgramException
                // sull'intero stepper (stesso schema di ON SPRITE COLLISION). Se la label
                // non è una procedura, symbolId=-1 → handler no-op.
                var keySymbolId = ResolveWindowGosubSymbolId(gosubAction);
                EmitInt32Argument(stmt.KeyExpression);
                _il.Emit(OpCodes.Ldc_I4, keySymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterKeyHandlerBySymbol), typeof(int), typeof(int)));
                break;
            default:
                throw new NotSupportedException("Backend IL: ON KEY supporta solo STOP e CALL (GOTO/GOSUB non supportati).");
        }
    }
    private void EmitOnMouseStatement(OnMouseStmt stmt)
    {
        switch (stmt.Action)
        {
            case StopAction:
                _il.Emit(OpCodes.Ldc_I4, (int)stmt.EventKind);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.UnregisterMouseHandler)));
                break;
            case CallAction callAction:
                if (callAction.Call is not FunctionExpr callExpr)
                    throw new NotSupportedException("Backend IL: azione ON MOUSE CALL non valida: attesa una chiamata a procedura.");
                // SymbolId → dispatch come TASK GPM (mai sul thread del dispatcher esterno).
                _il.Emit(OpCodes.Ldc_I4, (int)stmt.EventKind);
                _il.Emit(OpCodes.Ldc_I4, callExpr.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterMouseHandlerBySymbol)));
                break;
            case GosubAction gosubAction:
                // ON MOUSE ... GOSUB <handler>: SymbolId → dispatch come TASK GPM. Se la label
                // non è una procedura, symbolId=-1 → handler no-op.
                var mouseSymbolId = ResolveWindowGosubSymbolId(gosubAction);
                _il.Emit(OpCodes.Ldc_I4, (int)stmt.EventKind);
                _il.Emit(OpCodes.Ldc_I4, mouseSymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterMouseHandlerBySymbol)));
                break;
            default:
                throw new NotSupportedException("Backend IL: ON MOUSE supporta solo STOP e CALL (GOTO/GOSUB non supportati).");
        }
    }
    private void EmitOnWindowStatement(OnWindowStmt stmt)
    {
        // ON WINDOW <EV> [OF <handle>] <azione>: il handle (o -1 = qualunque finestra) viaggia
        // fino al runtime, che filtra sulla finestra che ha prodotto l'evento.
        void EmitWindowHandle()
        {
            if (stmt.Of is null) _il.Emit(OpCodes.Ldc_I4_M1);
            else EmitInt32Argument(stmt.Of);
        }

        switch (stmt.Action)
        {
            case StopAction:
                _il.Emit(OpCodes.Ldc_I4, (int)stmt.EventKind);
                EmitWindowHandle();
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.UnregisterWindowHandler)));
                break;
            case CallAction callAction:
                if (callAction.Call is not FunctionExpr callExpr)
                    throw new NotSupportedException("Backend IL: azione ON WINDOW CALL non valida: attesa una chiamata a procedura.");
                // SymbolId → dispatch come TASK GPM (mai sul thread del dispatcher esterno).
                _il.Emit(OpCodes.Ldc_I4, (int)stmt.EventKind);
                EmitWindowHandle();
                _il.Emit(OpCodes.Ldc_I4, callExpr.SymbolId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterWindowHandlerBySymbol)));
                break;
            case GosubAction gosubAction:
                EmitWindowHandlerSymbol(gosubAction, stmt.EventKind, stmt.Of);
                break;
            case GotoAction gotoAction:
                // Nel contesto di un handler di evento GOTO e GOSUB coincidono: non esiste un
                // punto di ritorno nel codice interrotto, quindi entrambi registrano il
                // SymbolId ed eseguono il blocco come task GPM (parità VM/C e C99Native).
                EmitWindowHandlerSymbol(gotoAction, stmt.EventKind, stmt.Of);
                break;
            default:
                throw new NotSupportedException("Backend IL: azione ON WINDOW non supportata.");
        }
    }

    /// <summary>Registra un handler ON WINDOW il cui target è un nome (SUB o label), via
    /// SymbolId: il runtime lo invoca come task GPM. Condiviso da GOSUB e GOTO.</summary>
    private void EmitWindowHandlerSymbol(OnKeyAction action, WindowEventKind kind, Expr? of)
    {
        var label = action switch
        {
            GosubAction g => g.TargetLabel,
            GotoAction go => go.TargetLabel,
            _ => throw new NotSupportedException("Backend IL: azione ON WINDOW non valida.")
        };
        // Se il target è una label di riga/modulo (non una procedura), symbolId=-1 →
        // handler no-op.
        var windowSymbolId = ResolveWindowHandlerSymbolId(label);
        _il.Emit(OpCodes.Ldc_I4, (int)kind);
        if (of is null) _il.Emit(OpCodes.Ldc_I4_M1);
        else EmitInt32Argument(of);
        _il.Emit(OpCodes.Ldc_I4, windowSymbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterWindowHandlerBySymbol)));
    }

    /// <summary>Compatibilità: gli altri emettitori di handler evento (ON KEY/MOUSE,
    /// MESSAGE, SPRITE) risolvono per GOSUB.</summary>
    private int ResolveWindowGosubSymbolId(GosubAction action) =>
        ResolveWindowHandlerSymbolId(action.TargetLabel);

    /// <summary>SymbolId della procedura (Proc_&lt;id&gt;) per un handler GOSUB/GOTO (ON KEY/
    /// MOUSE/WINDOW), o -1 se la label non corrisponde a una procedura senza parametri.
    /// Risolto a RUNTIME (vedi IlRuntime.RegisterKeyHandlerBySymbol) per evitare ldftn su
    /// MethodBuilder non ancora CreateType → InvalidProgramException sullo stepper.</summary>
    private int ResolveWindowHandlerSymbolId(Token labelToken)
    {
        var name = labelToken.Lexeme;
        foreach (var (sid, def) in _procedureDefinitions)
        {
            if (def.Parameters.Length != 0) continue;
            if (def.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase)
                || def.OriginalName.Lexeme.Equals(name + "$", StringComparison.OrdinalIgnoreCase)
                || def.OriginalName.Lexeme.Equals(name.Replace("_", ""), StringComparison.OrdinalIgnoreCase))
                return sid;
        }
        return -1;
    }
    /// <summary>SymbolId della procedura handler per ON MESSAGE (anche con parametri:
    /// il runtime fornisce id/topic/payload), o -1 se non trovata.</summary>
    private int ResolveMessageHandlerSymbolId(Token handler)
    {
        var name = handler.Lexeme;
        foreach (var (sid, def) in _procedureDefinitions)
            if (def.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase)
                || def.OriginalName.Lexeme.Equals(name + "$", StringComparison.OrdinalIgnoreCase)
                || def.OriginalName.Lexeme.Equals(name.Replace("_", ""), StringComparison.OrdinalIgnoreCase))
                return sid;
        return -1;
    }

    private void EmitOnSpriteCollisionStatement(OnSpriteCollisionStmt stmt)
    {
        var def = _procedureDefinitions.Values.FirstOrDefault(p =>
            p.OriginalName.Lexeme.Equals(stmt.HandlerName, StringComparison.OrdinalIgnoreCase)
            || p.OriginalName.Lexeme.Equals(stmt.HandlerName + "$", StringComparison.OrdinalIgnoreCase));
        if (def is null)
            throw new NotSupportedException(
                $"Backend IL: handler '{stmt.HandlerName}' di ON SPRITE COLLISION(S) non trovato.");
        // ldftn su MethodBuilder (firma Action<object> vs Proc_(double[]) ) è IL
        // illegale → InvalidProgramException sull'intero stepper. Stesso schema di
        // FETCH/AI: SymbolId, delegate creato a runtime.
        _il.Emit(OpCodes.Ldc_I4, def.SymbolId);
        _il.Emit(OpCodes.Ldc_I4, stmt.IsPlural ? 1 : 0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterSpriteCollisionBySymbol)));
    }

    private void EmitNoArgBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    { if (expr.Args.Length != 0) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} non accetta argomenti."); _il.Emit(OpCodes.Call, method); }

    private void EmitJoySingleArgCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento (port).");
        EmitInt32Argument(expr.Args[0]);
        _il.Emit(OpCodes.Call, method);
    }    private void EmitInt16TernaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); foreach (var arg in expr.Args) EmitInt16Argument(arg); _il.Emit(OpCodes.Call, method); }
    private void EmitInt16Argument(Expr expr)
    { EmitExpression(expr); var type = NormalizeInferredType(InferType(expr)); IlConversion.EmitConvert(_il, _types, type, DataTypeMapper.Map(type, _types, _classTypes), _types.Int16); }
    private void EmitInt32Argument(Expr expr)
    {
        EmitExpression(expr);
        var type = NormalizeInferredType(InferType(expr));
        var fromClr = DataTypeMapper.Map(type, _types, _classTypes);
        if (expr is VariableExpr ve && _locals.TryGetValue(ve.SymbolId, out var lb)) fromClr = lb.LocalType;
        if (fromClr == _types.Int32) return;
        IlConversion.EmitConvert(_il, _types, type, fromClr, _types.Int32);
    }
    private void EmitInt64UnaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento."); EmitInt64Argument(expr.Args[0]); _il.Emit(OpCodes.Call, method); }
    private void EmitDateTimeUnaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        // Le builtin DateTime (YEAR/MONTH/ISLEAPYEAR/DAYSINMONTH/...) richiedono DateTime CLR.
        // Se l'argomento è un NUMERO (OADate, es. ISLEAPYEAR(2024) o DATE serializzato) va
        // convertito double→DateTime prima della chiamata — altrimenti `IsLeapYear(double)`
        // è un mismatch di firma → InvalidProgram.
        var argType = NormalizeInferredType(InferType(expr.Args[0]));
        var argClr = DataTypeMapper.Map(argType, _types, _classTypes);
        if (argClr != typeof(DateTime))
            IlConversion.EmitConvert(_il, _types, argType, argClr, typeof(DateTime));
        _il.Emit(OpCodes.Call, method);
    }
    private void EmitEomonthCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: EOMONTH richiede 1 o 2 argomenti."); EmitExpression(expr.Args[0]); if (expr.Args.Length == 2) EmitInt16Argument(expr.Args[1]); else _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Conv_I2); _il.Emit(OpCodes.Call, IlRuntimeEomonthMethod); }
    private void EmitDateAddCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: DATEADD richiede 3 argomenti."); EmitExpression(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); EmitExpression(expr.Args[2]); _il.Emit(OpCodes.Call, IlRuntimeDateAddMethod); }

    private void EmitDoubleBinaryBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 2 argomenti.");
        foreach (var argument in expr.Args)
        {
            EmitExpression(argument);
            var sourceType = NormalizeInferredType(InferType(argument));
            var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, _types.Double);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitDoubleArityBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method, int arity)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente {arity} argomenti.");
        foreach (var argument in expr.Args) EmitDoubleArgument(argument);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>
    /// Fase 9 del piano (docs/StdLib-Runtime-Plan.md): variante di
    /// <see cref="EmitDoubleArityBuiltinCall"/> per chiamare <c>StdLib.Runtime</c> direttamente
    /// invece del wrapper locale in <see cref="IlRuntime"/>. La sintassi BASIC per le PDF/CDF
    /// continue dichiara sempre <c>x</c> come PRIMO parametro (es. <c>NORMALPDF(x, mean,
    /// stdDev)</c>), ma <c>StdLib.Runtime.MathFunctions</c> rispecchia la convenzione di
    /// <c>CommonLib</c> (parametri della distribuzione, poi <c>x</c> per ultimo) — verificato,
    /// funzione per funzione, che questo sia SEMPRE e SOLO uno shift di una posizione (mai una
    /// permutazione più complessa) per l'intero dominio PDF/CDF continuo. Le PMF discrete
    /// (Bernoulli/Binomial/ecc.) NON usano questo helper: mescolano `int`/`double` e in un caso
    /// (BinomialPmf) l'ordine non è un semplice shift — gestite a parte.
    /// </summary>
    private void EmitOptionalParamsCall(BuiltinFunctionCallExpr expr, MethodInfo method, int totalArity, params double[] defaults)
    {
        // PDF/CDF con param opzionali (mu/sigma/xi): il campione chiama STATS.NORMALPDF(0)
        // con 1 solo argomento — i mancanti usano i default (come la VM: mu=0, sigma=1).
        if (expr.Args.Length < 1 || expr.Args.Length > totalArity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede da 1 a {totalArity} argomenti.");
        for (var i = 1; i < totalArity; i++)
        {
            if (i < expr.Args.Length) EmitDoubleArgument(expr.Args[i]);
            else _il.Emit(OpCodes.Ldc_R8, defaults[i - 1]);
        }
        EmitDoubleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitDoubleArityBuiltinCallXLast(BuiltinFunctionCallExpr expr, MethodInfo method, int arity)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente {arity} argomenti.");
        for (var i = 1; i < arity; i++) EmitDoubleArgument(expr.Args[i]);
        EmitDoubleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>
    /// Fase 9 parte B (docs/StdLib-Runtime-Plan.md): le 6 PMF discrete (Bernoulli/Binomial/
    /// Poisson/Geometric/NegativeBinomial/Hypergeometric) non seguono il semplice shift di
    /// <see cref="EmitDoubleArityBuiltinCallXLast"/> — <c>StdLib.Runtime.MathFunctions</c>
    /// dichiara <c>k</c>/<c>n</c>/<c>population</c>/... come veri <c>int</c>, non <c>double</c>
    /// come ovunque nel resto del dominio Math, e <c>BinomialPmf</c> ha una permutazione vera
    /// (non solo "x per ultimo"). <paramref name="pushSpec"/> elenca, in ordine di push,
    /// l'indice nell'array <c>expr.Args</c> BASIC e se va convertito a <c>int</c>. La
    /// conversione usa <c>conv.i4</c> su un double già sullo stack — TRONCA verso zero, la
    /// stessa semantica del cast C# <c>(int)x</c> già usata dal vecchio wrapper
    /// <see cref="IlRuntime"/> (NON <see cref="EmitInt32Arg"/> esistente altrove nel file, che
    /// ARROTONDA con <c>Convert.ToInt32</c> — userlo qui cambierebbe silenziosamente il
    /// comportamento per un <c>k</c> non intero, es. <c>2.9</c> tronca a 2 ma arrotonda a 3).
    /// </summary>
    private void EmitPmfBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method, int arity, (int ArgIndex, bool IsInt)[] pushSpec)
    {
        if (expr.Args.Length != arity)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente {arity} argomenti.");
        foreach (var (argIndex, isInt) in pushSpec)
        {
            EmitDoubleArgument(expr.Args[argIndex]);
            if (isInt) _il.Emit(OpCodes.Conv_I4);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private static MethodInfo RuntimeMethod(string name) => typeof(IlRuntime).GetMethod(name)
        ?? throw new InvalidOperationException($"Backend IL: metodo runtime '{name}' non trovato.");

    private static MethodInfo RuntimeMethod(string name, params Type[] parameterTypes) =>
        typeof(IlRuntime).GetMethod(name, parameterTypes)
        ?? throw new InvalidOperationException($"Backend IL: metodo runtime '{name}' non trovato.");

    private static Type[] TradingOptionTypes(int count) =>
        Enumerable.Repeat(typeof(double), count).ToArray();

    private void EmitDoubleArrayArgument(Expr argument)
    {
        var at = NormalizeInferredType(InferType(argument));
        if (at is ArrayType) { EmitExpression(argument); return; }
        // Argomento SCALARE dove la StdLib attende un array (es. i `gen_*` che chiamano
        // TIMESERIES.TSMA(0, 0)): il C# riceve un array di 1 elemento [scalare] — stessa
        // coercizione del PCode (ToDoubleArray) — e solleva lo STESSO errore di validazione,
        // invece di far fallire la COMPILAZIONE IL ("atteso un array di DOUBLE").
        if (at is DoubleType or SingleType or LongType or ULongType or QuadType or UQuadType
            or IntegerType or UIntegerType or ByteType or SByteType or BitType or CurrencyType
            or InferredType or AnyType)
        {
            _il.Emit(OpCodes.Ldc_I4_1);
            _il.Emit(OpCodes.Newarr, typeof(double));
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4_0);
            EmitDoubleArgument(argument);
            _il.Emit(OpCodes.Stelem_R8);
            return;
        }
        throw new NotSupportedException("Backend IL: atteso un array di DOUBLE.");
    }

    private void EmitTimeSeriesDoubleArrayDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitSvmFitCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 5 argomenti.");
        EmitDoubleArrayArrayArgument(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        EmitInt64Argument(expr.Args[2]);
        EmitDoubleArgument(expr.Args[3]);
        EmitInt64Argument(expr.Args[4]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitTwoArrayLongLongCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti.");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        EmitInt64Argument(expr.Args[2]);
        EmitInt64Argument(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitThreeArrayTwoDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 5 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); EmitDoubleArgument(expr.Args[3]); EmitDoubleArgument(expr.Args[4]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitThreeArrayThreeDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 6) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 6 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); for (var i = 3; i < 6; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitThreeFlatArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTpeCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 6) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 6 argomenti.");
        EmitExpression(expr.Args[0]);
        // FUNCTION NOMINATA con parametri array: EmitExpression produce il delegate `ref`
        // (CbRef1D, firma T[]&) mentre IlRuntime.Tpe vuole Func<double[],double>.
        if (expr.Args[0] is FunctionReferenceExpr fr
            && _program.Symbols.GetSymbol(fr.SymbolId).DeclarationNode is FunctionDefinition fd
            && TryRefDelegateType(fd) is { } refDel
            && EmitsRefCallbackDelegate(fd))
            EmitRefCallbackAdapter(refDel);
        EmitDoubleArrayArgument(expr.Args[1]);
        EmitDoubleArrayArgument(expr.Args[2]);
        EmitInt64Argument(expr.Args[3]);
        EmitInt64Argument(expr.Args[4]);
        EmitDoubleArgument(expr.Args[5]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitBoostFitCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 6) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 6 argomenti.");
        EmitDoubleArrayArrayArgument(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        EmitInt64Argument(expr.Args[2]);
        EmitInt64Argument(expr.Args[3]);
        EmitDoubleArgument(expr.Args[4]);
        EmitInt64Argument(expr.Args[5]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitThreeArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti.");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitDoubleArrayArrayArgument(expr.Args[1]);
        EmitDoubleArrayArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitArrayFiveDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 6) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 6 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); for (var i = 1; i < 6; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitNoArgCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 0) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede zero argomenti."); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitDoubleArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitDoubleTwoArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitDoubleArrayArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesThreeArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisMatrixLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArrayArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisMatrixLongLongLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti."); EmitDoubleArrayArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]); EmitInt64Argument(expr.Args[3]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisMatrixLongLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitThreeArrayCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); _il.Emit(OpCodes.Call, method); }
    private void EmitThreeArrayDoubleCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti."); for (var i = 0; i < 3; i++) EmitDoubleArrayArgument(expr.Args[i]); EmitDoubleArgument(expr.Args[3]); _il.Emit(OpCodes.Call, method); }
    private void EmitTimeSeriesArrayOnlyCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento."); EmitDoubleArrayArgument(expr.Args[0]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayLongLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesTwoArrayLongCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitInt64Argument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayFourDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 5 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); for (var i = 1; i < 5; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayLongDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTimeSeriesArrayLongDoubleDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); EmitDoubleArgument(expr.Args[3]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitCryptoObjectArgument(Expr argument)
    {
        var type = NormalizeInferredType(InferType(argument));
        EmitExpression(argument);
        var clrType = DataTypeMapper.Map(type, _types, _classTypes);
        if (clrType.IsValueType) _il.Emit(OpCodes.Box, clrType);
    }
    private void EmitCryptoObjectUnaryCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento."); EmitCryptoObjectArgument(expr.Args[0]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }

    /// <summary>SIZEOF(Tipo|var): dimensione in byte del TIPO STATICO dell'argomento, calcolata
    /// a compile-time da <see cref="TypeLayout"/> (canone unico condiviso con col
    /// fold del compilatore bytecode che alimenta PCodeVM/VM-C). LA DELEGA AL RUNTIME È ELIMINATA:
    /// <c>IlRuntime.SizeOf</c> measureva l'oggetto CLR (array di 5 LONG → 48), che non è il
    /// canone — un tipo senza layout statico vale <see cref="TypeLayout.NoStaticSize"/> (-1) su
    /// tutte e cinque le gambe. Lo stack lasciato è sempre Int64.</summary>
    private void EmitSizeOfCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: SIZEOF richiede 1 argomento.");
        // `SIZEOF((x))`: le parentesi producono un GroupingExpr, ma la FORMA dell'argomento resta
        // una variabile → vale il suo tipo statico (canone TypeLayout; il C99Native, che non ha il
        // nodo Grouping, rispondeva già 2 mentre qui si dava -1: divergenza misurata 2026-11-01).
        var sizeArg = expr.Args[0];
        while (sizeArg is GroupingExpr grp) sizeArg = grp.Expression;
        if (sizeArg is not VariableExpr argVar)
        {
            // NON identificatore: solo il letterale numerico ha un tipo statico (8 = il double di
            // default del linguaggio); ogni altra espressione (aritmetica, chiamata, membro,
            // indice, stringa, booleano) vale -1, come le gambe C che non inferiscono.
            _il.Emit(OpCodes.Ldc_I8, sizeArg is NumberExpr
                ? TypeLayout.ScalarByteSize(new DoubleType()) : TypeLayout.NoStaticSize);
            return;
        }
        // KEYWORD DI TIPO SCALARE (SIZEOF(INTEGER) = 2, SIZEOF(STRING) = -1): PRIMA di ogni
        // risoluzione del simbolo — un nome che è keyword non può essere una variabile, e il
        // linker gli altrimenti attribuisce il double di default (8), divergente dalle gambe C.
        if (TypeLayout.TrySizeOfTypeName(argVar.NameCleaned.Lexeme, out var sizeofTypeName))
        {
            _il.Emit(OpCodes.Ldc_I8, sizeofTypeName);
            return;
        }
        var argType = NormalizeInferredType(InferType(argVar));
        // NOME DI TIPO (es. SIZEOF(PackedPoint)): il parser produce un placeholder UDT con
        // SymbolId<0 e SENZA campi — va sostituito con la dichiarazione reale del modulo e la
        // dimensione si risponde SUBITO: questo è l'unico modo in cui un NOME che è anche una
        // keyword (es. <c>TYPE Empty</c>) arriva a SIZEOF come tipo utente, e il test "keyword =
        // funzione built-in" qui sotto non deve mai vederlo.
        if (argType is UserDefinedType { SymbolId: < 0 } placeholder)
        {
            var realUdt = _program.GetAllUserDefinedTypes()
                .FirstOrDefault(u => string.Equals(u.OriginalName.Lexeme, placeholder.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (realUdt is null)
                throw new NotSupportedException($"Backend IL: SIZEOF del tipo '{placeholder.OriginalName.Lexeme}' — UDT non trovato nel modulo.");
            _il.Emit(OpCodes.Ldc_I8, TypeLayout.ByteSize(realUdt));
            return;
        }
        // KEYWORD CHE NON È NOME DI TIPO NÉ TYPE UTENTE (placeholder già risolto sopra) =
        // FUNZIONE/COSTANTE built-in a zero argomenti (DATE, TIME, NOW, PI, LEN, FREEFILE, …): il
        // simbolo porta il tipo di RITORNO, che non è una variabile misurabile e che le gambe C non
        // conoscono → nessun tipo statico, -1 (canone regola 5; stesso test sul TokenKind in
        // gmb_sizeof_arg di C99Native). Una variabile dichiarata non può avere nome di keyword.
        if (TypeLayout.IsBuiltinFunctionKeyword(argVar.NameCleaned.Kind))
        {
            _il.Emit(OpCodes.Ldc_I8, TypeLayout.NoStaticSize);
            return;
        }
        // Identificatore SENZA tipo statico dichiarato (Inferred/Any): il DOUBLE di default del
        // linguaggio. L'inferimento sul flusso dati qui è vietato: le gambe C non ce l'hanno e
        // divergerebbero (TypeLayout).
        if (argType is InferredType or AnyType)
        {
            _il.Emit(OpCodes.Ldc_I8, TypeLayout.ScalarByteSize(new DoubleType()));
            return;
        }
        _il.Emit(OpCodes.Ldc_I8, TypeLayout.ByteSize(argType));
    }
    private void EmitCryptoObjectBinaryCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitCryptoObjectArgument(expr.Args[0]); EmitCryptoObjectArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitCryptoObjectAesCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        // AESENCRYPT/AESDECRYPT: 2 o 3 argomenti (data, key[, iv]) — come la VM, il 3° (IV)
        // è ignorato (AES-256-GCM deriva il nonce interno); emesso e scartato.
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 o 3 argomenti.");
        EmitCryptoObjectArgument(expr.Args[0]);
        EmitCryptoObjectArgument(expr.Args[1]);
        if (expr.Args.Length == 3)
        {
            EmitCryptoObjectArgument(expr.Args[2]);
            _il.Emit(OpCodes.Pop);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    private void EmitCryptoObjectTernaryCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); foreach (var argument in expr.Args) EmitCryptoObjectArgument(argument); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitCryptoHashFileCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 o 2 argomenti."); EmitExpression(expr.Args[0]); if (expr.Args.Length == 2) EmitExpression(expr.Args[1]); else _il.Emit(OpCodes.Ldstr, "SHA256"); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitInterpolationCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede X, Y e il punto target."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitIoCountHandleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede conteggio e handle."); EmitInt64Argument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    /// <summary>FREAD(handle [, count]) — funzione PURA: ritorna i dati letti (string). Il
    /// secondo argomento e' opzionale, ma in IL un default va comunque spinto: quando manca si
    /// passa 0 (= leggi una riga).</summary>
    private void EmitFReadCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 1 || expr.Args.Length > 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede handle e (opzionale) count.");
        EmitInt64Argument(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitInt64Argument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_I8, 0L);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FRead)));
    }

    private void EmitIoHandleObjectCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede handle e buffer/dati."); EmitInt64Argument(expr.Args[0]); EmitCryptoObjectArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitIoInt64PairCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede handle e posizione."); EmitInt64Argument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitLongUnaryCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento."); EmitInt64Argument(expr.Args[0]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitLongDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitInt64Argument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitLinAlgLongPairCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 handle/dimensioni."); EmitInt64Argument(expr.Args[0]); EmitInt64Argument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }

    /// <summary>Argomento "handle matrice" che accetta SIA un handle LONG SIA un valore
    /// matrice (MATRIX OF): boxing + IlRuntime.LinAlgMatrixHandle (che registra il valore
    /// o restituisce il long). Canone del ponte handle↔valore (ADR-MATRIX-Valore §5).</summary>
    private void EmitLinAlgHandleArgument(Expr arg)
    {
        EmitObjectBoxedArgument(arg);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
    }

    /// <summary>Due argomenti handle-ou-valore (MULTIPLY/KRONECKER/BLOCKDIAG/CONCAT*).</summary>
    private void EmitLinAlgHandlePairCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 handle/matrici.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitLinAlgHandleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>F4: come HandlePairCall ma il metodo del runtime restituisce un VALORE
    /// matrice (double[,]) invece di un handle.</summary>
    private void EmitLinAlgHandlePairValueCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 handle/matrici.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitLinAlgHandleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>MATRIX.CREATE a valore (F1): identità rank-2, tipo MATRIX OF (INT,INT,DOUBLE).</summary>
    private void EmitMatrixCreateValueCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: MATRIX.CREATE richiede (rows, cols).");
        EmitInt64Argument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixCreateValue)));
    }

    private void EmitLinAlgLongPairDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 handle e un double.");
        EmitInt64Argument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitMatrixSliceCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 5) throw new NotSupportedException("Backend IL: MATRIX.SLICE richiede matrice/handle e quattro indici.");
        // Valore MatrixValue: view O(1) sullo storage (nessuna copia).
        if (IsMatrixValueExpr(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            for (var i = 1; i < 5; i++) EmitInt64Argument(expr.Args[i]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueSliceView)));
            return;
        }
        EmitLinAlgHandleArgument(expr.Args[0]);
        for (var i = 1; i < 5; i++) EmitInt64Argument(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixSliceValue)));
    }

    private void EmitMatrixReshapeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: MATRIX.RESHAPE richiede matrice/handle, righe e colonne.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixReshapeValue)));
    }

    /// <summary>MATRIX.ROW/COL: matrice/handle + indice → valore rank-2 (1×cols / rows×1).</summary>
    private void EmitMatrixRowColCall(BuiltinFunctionCallExpr expr, MethodInfo method, bool isRow)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede matrice/handle e un indice.");
        // Valore MatrixValue: view O(1) (1×cols / rows×1).
        if (IsMatrixValueExpr(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            EmitInt64Argument(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(isRow ? nameof(IlRuntime.MatrixValueRowView) : nameof(IlRuntime.MatrixValueColView)));
            return;
        }
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Funzione matrice→matrice (TRANSPOSE/CLONEMATRIX): restituisce il VALORE
    /// rank-2 (F2), accettando un valore o un handle.</summary>
    private void EmitLinAlgUnaryValueCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Come EmitLinAlgUnaryValueCall, ma se l'argomento è un VALORE MatrixValue usa
    /// l'overload dedicato (es. TRANSPOSE/CLONE: view O(1) / clone indipendente).</summary>
    private void EmitMatrixUnaryValue(BuiltinFunctionCallExpr expr, MethodInfo handleMethod, string matrixValueMethod)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        if (IsMatrixValueExpr(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            /* campo-array rank-2 di CLASSE/UDT: double[,] → MatrixValue */
            if (IsRectArrayField(expr.Args[0]))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueFromRect), typeof(double[,])));
            _il.Emit(OpCodes.Call, RuntimeMethod(matrixValueMethod));
            return;
        }
        EmitLinAlgUnaryValueCall(expr, handleMethod);
    }

    /// <summary>ROWCOUNT/COLUMNCOUNT/ISCONTIGUOUS: se l'argomento è un valore MatrixValue usa
    /// il metodo dedicato (per ISCONTIGUOUS conserva lo stato di view a strides).</summary>
    private void EmitMatrixValueOrHandleUnary(BuiltinFunctionCallExpr expr, string valueMethod, MethodInfo handleMethod)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        if (IsMatrixValueExpr(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            /* CAMPO-ARRAY rank-2 di CLASSE/UDT: staticamente è double[,] (non MatrixValue)
             * → conversione nel descrittore prima del metodo a valore. */
            if (IsRectArrayField(expr.Args[0]))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueFromRect), typeof(double[,])));
            _il.Emit(OpCodes.Call, RuntimeMethod(valueMethod));
            return;
        }
        EmitLinAlgUnaryHandleCall(expr, handleMethod, vector: false);
    }

    private void EmitMatrixBroadcastCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede matrice/handle e vettore.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    private void EmitMatrixThresholdCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede matrice/handle e soglia.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    private void EmitMatrixVectorProductCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede matrice/handle e vettore.");
        EmitLinAlgHandleArgument(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>DETERMINANT/LU/QR/... su un handle LONG o su una MATRICE/VETTORE REALE
    /// (Double[,]/Double[] — il campione passa l'array direttamente, es. DETERMINANT(A)
    /// con DIM A(1,1)): registra SEMPRE via LinAlgMatrix/VectorHandle, che gestisce sia
    /// l'array (→ nuovo handle) sia un long (→ lo stesso handle).</summary>
    private void EmitLinAlgUnaryHandleCall(BuiltinFunctionCallExpr expr, MethodInfo method, bool vector)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento.");
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(vector ? nameof(IlRuntime.LinAlgVectorHandle) : nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>ADR-MATRIX F4: le DECOMPOSIZIONI sono VALORI (dizionario dei fattori come
    /// MatrixValue), non handle: il metodo handle-based viene avvolto da
    /// <see cref="IlRuntime.DecompositionValue"/>, che costruisce il dizionario e rimuove
    /// l'handle dal registry.</summary>
    private void EmitLinAlgDecompValueCall(BuiltinFunctionCallExpr expr, MethodInfo handleMethod, string kind)
    {
        EmitLinAlgUnaryHandleCall(expr, handleMethod, vector: false);
        _il.Emit(OpCodes.Ldstr, kind);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DecompositionValue), typeof(long), typeof(string)));
    }
    private void EmitLinAlgSetValueCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 4) throw new NotSupportedException("Backend IL: MATRIX.SETVALUE richiede 4 argomenti (matrice/handle, row, col, value).");
        // Stage D: valore matrice (MATRIX OF o DIM rank-2) — entrambi MatrixValue a livello CLR:
        // mutazione diretta sullo storage (view comprese).
        if (IsMatrixValueArgument(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]); EmitDoubleArgument(expr.Args[3]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueSetAt)));
            return;
        }
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
        EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); EmitDoubleArgument(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixSetValue)));
    }
    private void EmitLinAlgGetValueCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: MATRIX.GETVALUE richiede 3 argomenti (matrice/handle, row, col).");
        if (IsMatrixValueArgument(expr.Args[0]))
        {
            EmitExpression(expr.Args[0]);
            /* campo-array rank-2 di CLASSE/UDT: double[,] → MatrixValue */
            if (IsRectArrayField(expr.Args[0]))
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueFromRect), typeof(double[,])));
            EmitInt64Argument(expr.Args[1]); EmitInt64Argument(expr.Args[2]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueGetAt)));
            return;
        }
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
        EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixGetValue)));
    }

    /// <summary>True quando l'argomento è una matrice VALORE (MATRIX OF oppure array rank-2
    /// DOUBLE): entrambi mappati a <see cref="MatrixValue"/> nel backend IL (Stage D).</summary>
    private bool IsMatrixValueExpr(Expr arg)
        => NormalizeInferredType(InferType(arg)) is MatrixValueType
           || NormalizeInferredType(InferType(arg)) is ArrayType { Rank: 2, ElementType: DoubleType };

    /// <summary>True quando l'argomento è un CAMPO-ARRAY rank-2 di CLASSE/UDT il cui
    /// FieldBuilder è un vero <c>double[,]</c> (non un MatrixValue): va convertito con
    /// <see cref="IlRuntime.MatrixValueFromRect"/> prima di un metodo che vuole un
    /// MatrixValue. Serve perché <c>DataTypeMapper.Map</c> mappa ArrayType rank-2 DOUBLE a
    /// MatrixValue (Stage D), quindi il tipo statico non basta a distinguere i due casi.</summary>
    private bool IsRectArrayField(Expr arg)
    {
        if (arg is not FieldAccessExpr fa) return false;
        int sym = fa.FieldSymbolId;
        if (sym < 0)
        {
            try
            {
                sym = ResolveFieldOrPropertySymbolIdByName(
                    NormalizeInferredType(InferType(fa.Structure)), fa.Field.Lexeme);
            }
            catch (NotSupportedException) { return false; }
        }
        return sym >= 0 && _classFieldArrays.TryGetValue(sym, out var fi)
               && fi.Field.FieldType == typeof(double[,]);
    }

    private bool IsMatrixValueArgument(Expr arg) => IsMatrixValueExpr(arg);

    /// <summary>True quando l'argomento è un CAMPO matrice di CLASSE/UDT il cui FieldBuilder
    /// è un <see cref="MatrixValue"/> (Stage D: i campi rank-2 DOUBLE sono descrittori).</summary>
    private bool IsMatrixField(Expr arg)
    {
        if (arg is not FieldAccessExpr fa) return false;
        int sym = fa.FieldSymbolId;
        if (sym < 0)
        {
            try
            {
                sym = ResolveFieldOrPropertySymbolIdByName(
                    NormalizeInferredType(InferType(fa.Structure)), fa.Field.Lexeme);
            }
            catch (NotSupportedException) { return false; }
        }
        return sym >= 0 && _classFieldArrays.TryGetValue(sym, out var fi)
               && fi.Field.FieldType == typeof(MatrixValue);
    }

    /// <summary>SOLVE(A, B): il PRIMO argomento è la MATRICE, il SECONDO il VETTORE —
    /// registra separatamente (LinAlgMatrixHandle/LinAlgVectorHandle). DOTPRODUCT(B, B):
    /// entrambi vettori.</summary>
    private void EmitLinAlgSolveCall(BuiltinFunctionCallExpr expr, bool vectorDot = false)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti.");
        EmitObjectBoxedArgument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(vectorDot ? nameof(IlRuntime.LinAlgVectorHandle) : nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
        EmitObjectBoxedArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgVectorHandle), typeof(object)));
        _il.Emit(OpCodes.Call, RuntimeMethod(vectorDot ? nameof(IlRuntime.VectorDotProduct) : nameof(IlRuntime.MatrixSolve)));
    }
    private void EmitLinAlgDecompGetFactorCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede la decomposizione e il nome del fattore.");
        /* Valore (dizionario dei fattori) oppure handle legacy: il runtime distingue. */
        EmitExpression(expr.Args[0]);
        var argType = NormalizeInferredType(InferType(expr.Args[0]));
        var argClr = DataTypeMapper.Map(argType, _types, _classTypes);
        if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
        EmitStringArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DecompositionGetFactorValueFrom), typeof(object), typeof(string)));
    }
    private void EmitStringArgument(Expr expr)
    { EmitExpression(expr); var type = NormalizeInferredType(InferType(expr)); IlConversion.EmitConvert(_il, _types, type, DataTypeMapper.Map(type, _types, _classTypes), _types.String); }
    private void EmitLinAlgVectorCreateCall(BuiltinFunctionCallExpr expr)
    { if (expr.Args.Length is < 3 or > 4) throw new NotSupportedException("Backend IL: VECTOR.CREATE richiede 3 o 4 componenti."); foreach (var argument in expr.Args) EmitDoubleArgument(argument); if (expr.Args.Length == 3) _il.Emit(OpCodes.Ldc_R8, 0d); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.VectorCreate))); }






    private void EmitDoubleArrayArrayArgument(Expr expr)
    {
        // Matrice jagged BASIC (DIM M(3) AS ARRAY OF DOUBLE + M(i)={...}) -> double[][] statico.
        // Array RETTANGOLARE rank-2 (DIM M(3, 1)) → double[][] via RectToJagged (il runtime
        // ANALYSIS — Pca/Anova — vuole jagged; il rettangolare CLR non è convertibile a runtime).
        // Solo RETTANGOLARE (double[,] — DIM M(3,1)): il JAGGED (double[][] — DIM FftRes() =
        // FFT(...)) passa come riferimento (RectToJagged(double[,]) su un jagged lancerebbe
        // 'Array does not have that many dimensions').
        if (expr is VariableExpr veArr && _arrays.TryGetValue(veArr.SymbolId, out var arrInfo) && arrInfo.Rank == 2)
        {
            if (arrInfo.ClrArrayType == typeof(MatrixValue))
            {
                EmitArrayRefFromInfo(arrInfo);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixValueToJagged), typeof(MatrixValue)));
                return;
            }
            if (arrInfo.ClrArrayType.IsArray && !arrInfo.ClrArrayType.GetElementType()!.IsArray)
            {
                _il.Emit(OpCodes.Ldloc, arrInfo.Local);
                _il.Emit(OpCodes.Call, IlRuntimeRectToJaggedMethod);
                return;
            }
        }
        EmitExpression(expr);
    }


    private void EmitFgBgRgbCall(BuiltinFunctionCallExpr expr, bool fg)
    {
        if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {(fg ? "FGRGB" : "BGRGB")} richiede 4 argomenti.");
        EmitStringArgument(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        EmitDoubleArgument(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(fg ? nameof(IlRuntime.FgRgb) : nameof(IlRuntime.BgRgb), typeof(string), typeof(double), typeof(double), typeof(double)));
    }


    /// <summary>Valori del tail variadico di una builtin (ABIENCODE/…). La StdLib dichiara il
    /// parametro come <c>ParamArray</c>, quindi il LINKER incapsula il tail in un
    /// <c>ArrayLiteralExpr</c>; qui il wrapper viene RIMOSSO. La rimozione è ripetuta finché il
    /// literal ha un solo elemento che è a sua volta un literal: il tail può arrivare annidato
    /// (una passata di rewrite in più, o un elemento che l'inferenza ha coercito a "array di 1").
    /// Senza, il runtime riceve un <c>double[]</c> dove attende uno scalare e la builtin fallisce
    /// (caso reale: <c>ABIENCODE</c> restituiva <c>0x</c> con l'eccezione inghiottita dal catch).</summary>
    private static ImmutableArray<Expr> ExtractVariadicValues(BuiltinFunctionCallExpr expr, int fixedPrefix)
    {
        var rest = expr.Args.Length - fixedPrefix;
        if (rest == 1 && expr.Args[fixedPrefix] is ArrayLiteralExpr l1)
        {
            var elems = l1.Elements;
            while (elems.Length == 1 && elems[0] is ArrayLiteralExpr inner && inner.Elements.Length > 0)
                elems = inner.Elements;
            return elems;
        }
        return [.. expr.Args[fixedPrefix..]];
    }

    private void EmitStringArrayArgument(Expr expr) => EmitExpression(expr);

    private void EmitAnalysisMatrixCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento (gruppi)."); EmitDoubleArrayArrayArgument(expr.Args[0]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTripleScalarCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti.");
        EmitDoubleArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitAnalysisTwoArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisArrayScalarCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisFourScalarCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); for (var i = 1; i < 4; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisFiveCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 5 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); for (var i = 1; i < 5; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitStatsArrayTwoScalarCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitStatsFourScalarCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti."); foreach (var argument in expr.Args) EmitDoubleArgument(argument); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }

    private void EmitAnalysisInterpCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisInterpGenericCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length is < 4 or > 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4-5 argomenti."); EmitStringArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); EmitDoubleArrayArgument(expr.Args[2]); EmitDoubleArgument(expr.Args[3]); if (expr.Args.Length == 5) EmitDoubleArgument(expr.Args[4]); else _il.Emit(OpCodes.Ldc_R8, 0.0); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }

    private void EmitAnalysisArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento."); EmitDoubleArrayArgument(expr.Args[0]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisArrayArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 array.");
        EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitAnalysisIntArrayCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 intero.");
        EmitInt64Argument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitAnalysisComplexCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (reale, immaginaria)."); EmitDoubleArrayArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisClosureOneCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (funzione, x)."); EmitExpression(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisClosureTwoCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (funzione, a, b)."); EmitExpression(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisClosureTwoCall2(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 4) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 4 argomenti (funzione, a, b, n)."); EmitExpression(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); EmitDoubleArgument(expr.Args[2]); EmitDoubleArgument(expr.Args[3]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitAnalysisOdeCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 5) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 5 argomenti (fn(t,y), y0, t0, t1, steps)."); EmitExpression(expr.Args[0]); for (var i = 1; i < 5; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitSolveIvpCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 4) throw new NotSupportedException("Backend IL: SOLVEIVP richiede (fn(t,y[]), y0[], t0, t1).");
        EmitExpression(expr.Args[0]);
        EmitDoubleArrayArgument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        EmitDoubleArgument(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SolveIvp)));
    }
    private void EmitSignalWelchCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: WELCH richiede (x[], nperseg, fs).");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitInt32Argument(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.Welch)));
    }
    private void EmitSignalIntIntCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede (x[], int, int).");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitInt32Argument(expr.Args[1]);
        EmitInt32Argument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitSignalIntCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede (x[], int).");
        EmitDoubleArrayArgument(expr.Args[0]);
        EmitInt32Argument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitAnalysisIntDoubleCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (n, d)."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitSignalDesignCall(BuiltinFunctionCallExpr expr, string methodName, int nArgs)
    {
        // [n(, rp)(, rs)] + wn[] + btype — gli ultimi 2 sono array e stringa.
        if (expr.Args.Length != nArgs) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {nArgs} argomenti.");
        for (var i = 0; i < nArgs - 2; i++) EmitDoubleArgument(expr.Args[i]);
        EmitDoubleArrayArgument(expr.Args[nArgs - 2]);
        EmitStringArgument(expr.Args[nArgs - 1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }
    private void EmitSignalApplyCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (handle, data)."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArrayArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitSignalFreqzCall(BuiltinFunctionCallExpr expr, string methodName)
    { if (expr.Args.Length != 2) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (handle, n)."); EmitDoubleArgument(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }
    private void EmitTsFilterCall(BuiltinFunctionCallExpr expr, string methodName, int nArgs)
    { if (expr.Args.Length != nArgs) throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede {nArgs} argomenti."); EmitDoubleArrayArgument(expr.Args[0]); for (var i = 1; i < nArgs; i++) EmitDoubleArgument(expr.Args[i]); _il.Emit(OpCodes.Call, RuntimeMethod(methodName)); }

    /// <summary>Adatta un delegate `ref` (FfiRefCallbackTypes) alla <c>Func&lt;&gt;</c> che un
    /// parametro <c>AS FUNCTION</c> di una procedura utente si aspetta (2026-11-03). Il
    /// chiamante ha già lasciato il delegate sullo stack.</summary>
    private void EmitRefCallbackAdapter(Type refDel)
    {
        string mname;
        if (refDel == typeof(FfiRefCallbackTypes.CbRef1ArrD)) mname = nameof(IlRuntime.OptVecVecFromRef);
        else if (refDel == typeof(FfiRefCallbackTypes.CbRef1S1D) || refDel == typeof(FfiRefCallbackTypes.CbRef1S1I)) mname = nameof(IlRuntime.OptModelFromRef);
        else if (refDel == typeof(FfiRefCallbackTypes.CbRef2ArrD)) mname = nameof(IlRuntime.OptVModelFromRef);
        else mname = nameof(IlRuntime.OptVecFromRef);
        _il.Emit(OpCodes.Call, RuntimeMethod(mname, refDel));
    }

}
