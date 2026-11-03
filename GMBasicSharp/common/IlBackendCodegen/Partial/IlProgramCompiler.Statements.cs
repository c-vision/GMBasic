// FILE: IlProgramCompiler.Statements.cs
// PARTIAL — dominio STATEMENTS (emissioni statement: console/grafica/file/rete/http/
// eventi/memory/transaction/go/db/tui/window/agent/team/workflow/macro/xml/config).
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

/// <summary>Partial del transpiler IL: dominio STATEMENTS.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitWithStatement(WithStmt stmt)
    {
        var receiverType = NormalizeInferredType(InferType(stmt.Expression));
        var receiver = _il.DeclareLocal(DataTypeMapper.Map(receiverType, _types, _classTypes));
        EmitExpression(stmt.Expression);
        _il.Emit(OpCodes.Stloc, receiver);
        _withTargets.Push((receiver, receiverType));
        try { foreach (var bodyStatement in stmt.Body) EmitStatement(bodyStatement); }
        finally { _withTargets.Pop(); }
    }

    private void EmitClsStatement(ClsStmt stmt)
    {
        // CLS polimorfico: dispatch a runtime (schermo grafico attivo → CLS_GPM; altrimenti
        // console). Prima andava SEMPRE alla console → il CLS grafico non cancellava la
        // pagina (sprites/poligoni di demo precedenti restavano sullo schermo).
        // L'argomento va passato RAW (object), come per COLOR: forzarlo a int32
        // (EmitInt32Arg) spezzava "CLS RGB(r,g,b)" — RGB() restituisce un double[4], la
        // conversione a int32 produceva un valore senza senso invece del colore richiesto.
        if (stmt.Color is not null)
        {
            EmitExpression(stmt.Color);
            var clrType = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Color)), _types, _classTypes);
            if (clrType.IsValueType) _il.Emit(OpCodes.Box, clrType);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeClsDispatchMethod);
    }

    private void EmitColorStatement(ColorStmt stmt)
    {
        // Parità PCodeVM di gmo: COLOR → dispatch a runtime (schermo grafico attivo →
        // COLOR_GPM; altrimenti console). L'argomento è passato RAW: numerico = indice
        // palette (boxato per il parametro object), RGBA (double[]) = colore diretto.
        if (stmt.Foreground is not null)
        {
            EmitExpression(stmt.Foreground);
            var fgClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Foreground)), _types, _classTypes);
            if (fgClr.IsValueType) _il.Emit(OpCodes.Box, fgClr);
        }
        else _il.Emit(OpCodes.Ldnull);
        if (stmt.Background is not null)
        {
            EmitExpression(stmt.Background);
            var bgClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Background)), _types, _classTypes);
            if (bgClr.IsValueType) _il.Emit(OpCodes.Box, bgClr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeColorDispatchMethod);
    }

    private void EmitLocateStatement(LocateStmt stmt)
    {
        // Parità PCodeVM di gmo: con schermo grafico attivo → LOCATE_GPM(col, line) senza
        // -1 (il GPM lavora in pixel); altrimenti console SetCursorPosition(col-1, line-1).
        EmitInt32Arg(stmt.ColumnExpr);
        EmitInt32Arg(stmt.LineExpr);
        _il.Emit(OpCodes.Call, IlRuntimeLocateDispatchMethod);
    }

    private void EmitConsoleCaptionStatement(ConsoleCaptionStmt stmt)
    {
        EmitLoadConsoleService();
        EmitExpression(stmt.TitleExpression);
        _il.Emit(OpCodes.Callvirt, ConsoleTitleSetterMethod);
    }

    private void EmitPaletteStatement(PaletteStmt stmt)
    {
        // PALETTE polimorfico: dispatch a runtime (schermo grafico attivo → PALETTE_GPM;
        // altrimenti console). Prima andava SEMPRE alla console → la palette grafica
        // non veniva mai ridefinita (PALETTE 5,r,g,b era no-op grafico).
        EmitInt32Arg(stmt.Index); EmitInt32Arg(stmt.R); EmitInt32Arg(stmt.G); EmitInt32Arg(stmt.B);
        _il.Emit(OpCodes.Call, IlRuntimePaletteDispatchMethod);
    }

    // ─── Grafica (IGraphicsService / CommonGraphics; headless → NullGraphicsService) ───
    // Ogni comando delega al GraphicsService del RuntimeHost: in headless è un no-op
    // (NullGraphicsService), in futuro una implementazione SDL basta sostituirla.

    private void EmitViewPrintStatement(ViewPrintStmt stmt)
    {
        EmitLoadConsoleService();
        if (stmt.TopLine is not null) EmitInt32Arg(stmt.TopLine); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.BottomLine is not null) EmitInt32Arg(stmt.BottomLine); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, ConsoleSetScrollRegionMethod);
    }

    // Operazioni file sincrone: emettono callvirt al contratto BRT, senza passare
    // dal dispatcher VM. OPEN/INPUT/GET/PUT restano separati perché mantengono nel
    // runtime interpreter metadati di canale e semantiche record/CSV più ricche.
    // ── Module System ─────────────────────────────────────────────────
    private void EmitImportStatement(ImportStmt stmt) { } // linking risolto a compile-time
    private void EmitRequireStatement(RequireStmt stmt) { } // linking risolto a compile-time

    // ── Clipboard ─────────────────────────────────────────────────────
    private void EmitClipStatement(ClipStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Command.Lexeme);
        if (stmt.Argument is not null) EmitStringArg(stmt.Argument);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeClipMethod);
        if (stmt.Destination is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    // ── Audio Control ─────────────────────────────────────────────────
    

    // ── Concurrency ──────────────────────────────────────────────────
    private void EmitGoProcedureStatement(GoProcedureStmt stmt)
    {
        // GO SUB è INLINE in IL (niente scheduler cooperativo): il FunctionExpr della call
        // consuma già TUTTI gli argomenti, incluso il WaitGroup passato alla SUB. Il P-Code
        // pusha il WaitGroup una SECONDA volta (per OP_GO) — qui un `Pop` dopo la call
        // sarebbe uno stack imbalance (la call void non pusha nulla) → InvalidProgram
        // (NetSslConcurrency.bas, fix 2026-08-11).
        EmitExpression(stmt.Call);
    }
    private void EmitGoExitStatement(GoExitStmt stmt) { _il.Emit(OpCodes.Ret); }
    /// <summary>YIELD v: passa il valore (boxato) al consumatore del generatore (NEXT) e sospende il corpo
    /// finché NEXT non lo riprende — vedi IlRuntime.GeneratorYield.</summary>
    private void EmitYieldStatement(YieldStmt stmt)
    {
        EmitExpression(stmt.ValueToYield);
        var yieldClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.ValueToYield)), _types, _classTypes);
        if (yieldClr.IsValueType) _il.Emit(OpCodes.Box, yieldClr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GeneratorYield), typeof(object)));
    }

    // ── Database/Transaction ─────────────────────────────────────────
    // ISAM BEGIN TRANSACTION/COMMIT/ROLLBACK erano no-op completi: caricavano il
    // DatabaseService e lo scartavano subito, senza aprire/chiudere/annullare alcuna
    // transazione — a differenza del compilatore PCode (VisitBeginTransactionStatement/
    // VisitCommitStatement/VisitRollbackStatement in Compiler.Isam.cs), che emette vere
    // SysCall collegate a Engine.BeginTransaction/Commit/Rollback. Conseguenza: ISAM
    // ROLLBACK non aveva mai nulla da annullare, quindi una PUT dentro BEGIN/ROLLBACK
    // restava permanente (scoperto verificando "[ASSERT] ROLLBACK FAIL" in
    // DbIsamFullTests.bas, sezione Transazioni).
    private void EmitBeginTransactionStatement(BeginTransactionStmt stmt)
    {
        _il.Emit(OpCodes.Ldc_I4, (int)(byte)stmt.Level);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamBeginTransaction), typeof(byte)));
    }
    private void EmitCommitStatement(CommitStmt stmt)
    {
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamCommitTransaction)));
    }
    private void EmitRollbackStatement(RollbackStmt stmt)
    {
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamRollbackTransaction)));
    }

    // ── Audio ────────────────────────────────────────────────────────
    
    
    

    // ── Keyboard ─────────────────────────────────────────────────────

    /// <summary>Scrive il testo (già sullo stack, convertito) nella stampante virtuale se
    /// SET PRINTER ON / SET DEVICE TO PRINTER. Il testo resta sullo stack per il video.</summary>
    private void EmitPrinterWrite(Basic.SourcePosition pos, bool newLine)
    {
        _il.Emit(OpCodes.Dup);
        _il.Emit(newLine ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlWritePrinterMethod);
    }
    /// <summary>SET legacy Clipper (WRAP/ESCAPE/.../PRINTER/DEVICE/MESSAGE) → IlRuntime.SetClipperCmd.</summary>
    private void EmitSetClipperStatement(SetClipperStmt scs)
    {
        int sub = scs.AttributeName.ToUpperInvariant() switch
        {
            "WRAP" => 6, "ESCAPE" => 7, "DELIMITERS" => 8, "TYPEAHEAD" => 9, "INTENSITY" => 10,
            "EPOCH" => 11, "DECIMALS" => 12, "FIXED" => 13, "PRINTER" => 14, "DEVICE" => 15, "MESSAGE" => 16,
            _ => -1
        };
        _il.Emit(OpCodes.Ldc_I4, sub);
        if (scs.Value is BooleanExpr be)
        {
            _il.Emit(be.Value ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
            EmitConvertToString(new IntegerType());
        }
        else
        {
            EmitExpression(scs.Value);
            // DEVICE TO PRINTER: valore già stringa (StringExpr) — nessuna conversione;
            // i valori numerici (DECIMALS/TYPEAHEAD/...) vanno convertiti per il wrapper.
            if (scs.Value is not StringExpr)
                EmitConvertToString(InferType(scs.Value));
        }
        _il.Emit(OpCodes.Call, IlSetClipperCmdMethod);
    }
    private void EmitInkeyStatement(InkeyStmt stmt)
    {
        // IlRuntime.Inkey (con catch interno): in headless Console.ReadKey() senza intercept
        // lancia/attende — il metodo restituisce "" se non c'è input reale.
        // Timeout opzionale in millisecondi: INKEY k$, 4000 → Inkey(int); 0/NIL → bloccante.
        if (stmt.TimeoutExpr != null) EmitInt32Argument(stmt.TimeoutExpr);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlInkeyTimeoutMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }
    private void EmitWaitKeyStatement(WaitKeyStmt stmt)
    {
        _il.Emit(OpCodes.Call, IlRuntimeReadKeyMethod);
        _il.Emit(OpCodes.Pop);
    }
    private void EmitGetKeyboardStatement(GetKeyboardStmt stmt)
    {
        _il.Emit(OpCodes.Call, IlRuntimeReadKeyMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    // ── Mouse ────────────────────────────────────────────────────────
    private void EmitMouseConfigStatement(MouseConfigStmt stmt)
    {
        _il.Emit(stmt.Enabled ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeMouseEnableMethod);
    }
    private void EmitMouseSetStatement(MouseSetStmt stmt)
    {
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        _il.Emit(OpCodes.Call, IlRuntimeMouseSetPosMethod);
    }
    private void EmitMouseVisibilityStatement(MouseVisibilityStmt stmt)
    {
        _il.Emit(stmt.Visible ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeMouseShowMethod);
    }
    private void EmitMouseCursorStatement(MouseCursorStmt stmt)
    {
        EmitInt32Arg(stmt.CursorId);
        _il.Emit(OpCodes.Call, IlRuntimeMouseCursorMethod);
    }
    private void EmitMouseLimitStatement(MouseLimitStmt stmt)
    {
        EmitNullableIntArg(stmt.X1); EmitNullableIntArg(stmt.Y1); EmitNullableIntArg(stmt.X2); EmitNullableIntArg(stmt.Y2);
        _il.Emit(OpCodes.Call, IlRuntimeMouseLimitMethod);
    }
    private void EmitMouseWaitStatement(MouseWaitStmt stmt)
    {
        _il.Emit(OpCodes.Call, IlRuntimeMouseWaitMethod);
    }

    // ── IO Watch ─────────────────────────────────────────────────────
    private void EmitIoWatchStatement(IoWatchStmt stmt)
    {
        EmitStringArg(stmt.Path);
        EmitInt32Arg(stmt.EventsMask);
        /* Il nome della SUB callback: IlRuntime.IoWatch lo usa per il dispatch GPM. */
        _il.Emit(OpCodes.Ldstr, stmt.Callback.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeIoWatchMethod);
    }
    private void EmitIoUnwatchStatement(IoUnwatchStmt stmt)
    {
        EmitStringArg(stmt.Path);
        _il.Emit(OpCodes.Call, IlRuntimeIoUnwatchMethod);
    }

    // ── Settings ─────────────────────────────────────────────────────
    private void EmitSetDateFormatStatement(SetDateFormatStmt stmt)
    {
        EmitStringArg(stmt.Format);
        _il.Emit(OpCodes.Call, IlRuntimeSetDateFormatMethod);
    }
    private void EmitSetThemeStatement(SetThemeStmt stmt)
    {
        EmitStringArg(stmt.ThemeName);
        _il.Emit(OpCodes.Call, IlRuntimeSetThemeMethod);
    }

    // ── Palette ──────────────────────────────────────────────────────
    // Tutti e 5 gli emettitori sotto erano stub rotti: ConsoleSetPaletteMethod
    // (IConsoleService.SetPaletteColor(int index, int r, int g, int b)) imposta UN
    // SOLO colore per indice — non esiste alcun setter "intero array" — ma
    // Define/Replace lo chiamavano con SOLO l'istanza del servizio sullo stack (mai gli
    // argomenti richiesti, mai stmt.Name/stmt.Colors), causando uno stack-underflow
    // verificato dal CLR come InvalidProgramException al caricamento
    // dell'assembly (mai un errore chiaro a compile-time). Use/Load/Save avevano lo
    // stesso problema strutturale: Use lasciava l'istanza del servizio sullo stack come
    // residuo mai consumato (statement, non espressione — deve lasciare lo stack
    // vuoto), Load/Save la POPpavano invece di usarla per davvero (nessuna vera
    // implementazione di caricamento/salvataggio). Finché non esiste un vero design per
    // il caso "intera palette con nome" nel backend IL (richiederebbe una nuova API
    // runtime, non solo il setter per-colore esistente), NotSupportedException esplicito
    // — SKIP pulito in test_il, coerente con gli altri costrutti non ancora supportati,
    // invece del crash silenzioso di prima. Porting identico da GMBasic.
    private void EmitPaletteDefineStatement(PaletteDefineStmt stmt)
    {
        // PALETTE DEFINE: palette nominata — nessuna API runtime per impostarla in blocco;
        // il singolo colore si imposta via PALETTE index, r, g, b (EmitPaletteStatement).
        // In headless no-op (coerente con il resto della grafica non-SDL).
    }
    private void EmitPaletteUseStatement(PaletteUseStmt stmt)
    {
        // PALETTE USE: seleziona una palette nominata — headless no-op.
    }
    private void EmitPaletteLoadStatement(PaletteLoadStmt stmt)
    {
        // PALETTE LOAD da file — headless no-op.
    }
    private void EmitPaletteSaveStatement(PaletteSaveStmt stmt)
    {
        // PALETTE SAVE su file — headless no-op.
    }
    private void EmitPaletteReplaceStatement(PaletteReplaceStmt stmt)
    {
        // PALETTE REPLACE — headless no-op.
    }

    // ── JSON ─────────────────────────────────────────────────────────
    private void EmitJsonSaveStatement(JsonSaveStmt stmt)
    {
        EmitExpression(stmt.Source);
        EmitStringArg(stmt.FilePath);
        _il.Emit(OpCodes.Call, IlRuntimeJsonSaveMethod);
    }

    private void EmitJsonWriteValueStatement(JsonWriteValueStmt stmt)
    {
        EmitStringArg(stmt.Key);
        EmitObjectBoxedArgument(stmt.Value);
        if (stmt.Section is not null) EmitStringArg(stmt.Section);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeJsonWriteValueMethod);
    }

    private void EmitJsonWriteSectionStatement(JsonWriteSectionStmt stmt)
    {
        EmitObjectBoxedArgument(stmt.Source);
        if (stmt.Section is not null) EmitStringArg(stmt.Section);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeJsonWriteSectionMethod);
    }

    // ── TUI Widget Helpers ─────────────────────────────────────────────
    /// <summary>Emette la chiamata a un metodo IlRuntime.Tui*(IReadOnlyList&lt;object?&gt;)
    /// con gli argomenti REALI dello statement (row/col/width/height/items in ordine
    /// PCodeVM) e assegna il risultato al target. NOTA: il metodo usa il list che riceve
    /// in ordine fisso, quindi qui si costruisce un object[] boxato + call.</summary>
    private readonly System.Collections.Generic.List<(string Name, Target Into)> _formControls = new();
    private int _pxCounter = 0;
    private static string PxStr(Expr? e) => e is StringExpr se ? (se.Value ?? "") : "";
    private static string PxSpec(System.Collections.Generic.List<PixelClause> cs)
    {
        var parts = new System.Collections.Generic.List<string>();
        int Num(PixelClause c) => c.Value is NumberExpr n ? (int)n.Value : 0;
        foreach (var c in cs) switch (c.ResolveKeyKind())
        {
            case TokenKind.PixelName: parts.Add($"name=\"{PxStr(c.Value)}\" "); break;
            case TokenKind.Tag: parts.Add($"tag=\"{PxStr(c.Value)}\" "); break;
            case TokenKind.PixelTooltip: parts.Add($"tooltip=\"{PxStr(c.Value)}\" "); break;
            case TokenKind.PixelVisible: parts.Add($"visible={Num(c)} "); break;
            case TokenKind.PixelEnabled: parts.Add($"enabled={Num(c)} "); break;
            case TokenKind.PixelTabIndex: parts.Add($"tabindex={Num(c)} "); break;
            case TokenKind.PixelTabStop: parts.Add($"tabstop={Num(c)} "); break;
            case TokenKind.Focus: parts.Add("focus "); break;
            case TokenKind.PixelStyle: parts.Add(PxStr(c.Value) + " "); break;
        }
        var r = string.Join("", parts).Trim();
        return r.Length > 0 ? r : null!;
    }

    private void EmitPixelControl(PixelControlStmt pc)
    {
        _il.Emit(OpCodes.Ldstr, pc.Kind.ToUpperInvariant());
        _il.Emit(OpCodes.Ldc_I4, pc.Args.Count);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (int i = 0; i < pc.Args.Count; i++)
        {
            _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
            if (pc.Args[i] is StringExpr se) _il.Emit(OpCodes.Ldstr, se.Value ?? "");
            else EmitObjectBoxedArgument(pc.Args[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        var name = pc.Clauses.FirstOrDefault(c => c.Key.Equals("NAME", StringComparison.OrdinalIgnoreCase)) is { } nm && !string.IsNullOrEmpty(PxStr(nm.Value))
            ? PxStr(nm.Value) : $"px{_pxCounter}";
        _il.Emit(OpCodes.Ldstr, name);
        _il.Emit(OpCodes.Call, IlRuntimeFormAddControlMethod);
        if (pc.Id != null) { _il.Emit(OpCodes.Call, IlRuntimeFormLastHandleMethod); EmitAssignTargetFromTop(pc.Id, new DoubleType()); }
        var _spec = PxSpec(pc.Clauses);
        if (_spec is not null) { _il.Emit(OpCodes.Ldstr, _spec); _il.Emit(OpCodes.Call, IlRuntimeFormSetLastStyleMethod); }
        if (pc.Into is not null) _formControls.Add((name, pc.Into));
    }

    private int ResolveControlEventSymbolId(string name)
    {
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

    private void EmitFireEvent(FireEventStmt fe)
    {
        EmitExpression(fe.Handle); _il.Emit(OpCodes.Conv_R8);
        EmitExpression(fe.Event);
        _il.Emit(OpCodes.Call, IlRuntimeFormFireEventMethod);
    }

    private void EmitControlEvent(ControlEventStmt ce)
    {
        EmitExpression(ce.Id); _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Ldstr, ce.Event.ToUpperInvariant());
        var kind = (ce.ActionKind ?? "CALL").ToUpperInvariant();
        _il.Emit(OpCodes.Ldc_I4, kind == "STOP" ? -1 : ResolveControlEventSymbolId(ce.Target ?? ""));
        _il.Emit(OpCodes.Call, IlRuntimeRegisterControlEventMethod);
    }

    private void EmitLdStr(Expr? e)
    {
        if (e is null) { _il.Emit(OpCodes.Ldstr, ""); return; }
        if (e is StringExpr se) { _il.Emit(OpCodes.Ldstr, se.Value ?? ""); return; }
        EmitObjectBoxedArgument(e); _il.Emit(OpCodes.Call, typeof(Convert).GetMethod(nameof(Convert.ToString), [typeof(object)])!);
    }
    // Argomenti numerici dei CONTROL <sub>: conversione per TIPO STATICO (IlConversion), non un Conv_* nudo: un argomento ANY/object (es. l'handle `g` di
    // `Paint(g AS ANY)` in CONTROL DRAW) finiva in `conv.r8` su un riferimento -> IL non valido (InvalidProgramException al primo Paint reale);
    // un'espressione intera (`i - 1`) con tipo inferito diverso da quello emesso rompeva anche l'ottimizzatore IL ("Invalid operand types in BinaryInst").
    private void EmitLdI4(Expr? e) { if (e is null) { _il.Emit(OpCodes.Ldc_I4_0); return; } EmitInt32Argument(e); }
    private void EmitLdDbl(Expr? e) { if (e is null) { _il.Emit(OpCodes.Ldc_R8, 0.0); return; } EmitDoubleArgument(e); }

    private void EmitControlMethod(ControlMethodStmt cm)
    {
        var a = cm.Args;
        switch (cm.ResolveSubKind())
        {
            case TokenKind.Set:
                EmitLdStr(a.Count > 0 ? a[0] : null); EmitLdStr(a.Count > 1 ? a[1] : null); EmitLdDbl(a.Count > 2 ? a[2] : null); EmitLdStr(a.Count > 3 ? a[3] : null);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlSetMethod); break;
            case TokenKind.Get:
                EmitLdStr(a.Count > 0 ? a[0] : null); EmitLdStr(a.Count > 1 ? a[1] : null);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlGetMethod);
                if (cm.Into is VariableTarget gv) EmitAssignTargetFromTop(gv); else _il.Emit(OpCodes.Pop);
                break;
            case TokenKind.ControlGets:
                EmitLdStr(a.Count > 0 ? a[0] : null); EmitLdStr(a.Count > 1 ? a[1] : null);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlGetStrMethod);
                if (cm.Into is VariableTarget gs) EmitAssignTargetFromTop(gs, new StringType()); else _il.Emit(OpCodes.Pop);
                break;
            case TokenKind.Move:
                EmitLdStr(a.Count > 0 ? a[0] : null); EmitLdI4(a.Count > 1 ? a[1] : null); EmitLdI4(a.Count > 2 ? a[2] : null); EmitLdI4(a.Count > 3 ? a[3] : null); EmitLdI4(a.Count > 4 ? a[4] : null);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlMoveMethod); break;
            case TokenKind.Focus:
                EmitLdStr(a.Count > 0 ? a[0] : null); _il.Emit(OpCodes.Call, IlRuntimeFormControlFocusMethod); break;
            case TokenKind.ControlInvalidate:
                EmitLdStr(a.Count > 0 ? a[0] : null); _il.Emit(OpCodes.Call, IlRuntimeFormControlInvalidateMethod); break;
            case TokenKind.Refresh:
                EmitLdStr(a.Count > 0 ? a[0] : null); _il.Emit(OpCodes.Call, IlRuntimeFormControlRefreshMethod); break;
            case TokenKind.ControlZorder:
                EmitLdStr(a.Count > 0 ? a[0] : null); EmitLdI4(a.Count > 1 ? a[1] : null); _il.Emit(OpCodes.Call, IlRuntimeFormControlZorderMethod); break;
            case TokenKind.Add:
                if (a.Count > 0) EmitExpression(a[0]); else _il.Emit(OpCodes.Ldnull);
                EmitLdI4(a.Count > 1 ? a[1] : null); EmitLdI4(a.Count > 2 ? a[2] : null); EmitLdI4(a.Count > 3 ? a[3] : null); EmitLdI4(a.Count > 4 ? a[4] : null);
                _il.Emit(OpCodes.Ldstr, PixelRefName(PixelPosEnd(a) > 5 ? a[5] : null));   /* PARENT (contenitore) */
                _il.Emit(OpCodes.Ldstr, PixelRefName(a.Count > 0 ? a[0] : null));   /* nome del controllo */
                _il.Emit(OpCodes.Call, IlRuntimeFormControlAddMethod);
                EmitUserControlEventBindings(a.Count > 0 ? a[0] : null);
                if (cm.Into is VariableTarget ivt) { _il.Emit(OpCodes.Call, IlRuntimeFormLastHandleMethod); EmitAssignTargetFromTop(ivt, new DoubleType()); }
                else if (cm.Into != null) { _il.Emit(OpCodes.Call, IlRuntimeFormLastHandleMethod); _il.Emit(OpCodes.Pop); }
                break;
            case TokenKind.Remove:
                if (a.Count > 0) EmitExpression(a[0]); else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlRemoveMethod); break;
            case TokenKind.Draw:
                EmitLdDbl(a.Count > 0 ? a[0] : null); EmitLdDbl(a.Count > 1 ? a[1] : null); EmitLdDbl(a.Count > 2 ? a[2] : null);
                EmitLdDbl(a.Count > 3 ? a[3] : null); EmitLdDbl(a.Count > 4 ? a[4] : null); EmitLdDbl(a.Count > 5 ? a[5] : null);
                EmitLdStr(a.Count > 6 ? a[6] : null);
                _il.Emit(OpCodes.Call, IlRuntimeFormControlDrawMethod); break;
        }
    }

    /* Nome di riferimento per la composizione (CONTROL ADD): nome-variabile (PARENT c) o stringa. */
    private static string PixelRefName(Expr? e) => e switch
    {
        VariableExpr v => v.NameCleaned.Lexeme,
        StringExpr s => s.Value ?? "",
        _ => ""
    };

    /* Fine degli args POSIZIONALI di CONTROL ADD: il linker appende il marker "__UCBIND__" + le
     * triple (evName, metodo, symbolId) dei metodi-evento della classe (§11.7-3). */
    private static int PixelPosEnd(System.Collections.Generic.List<Expr> a)
    {
        int m = a.FindIndex(e => e is StringExpr s && s.Value == "__UCBIND__");
        return m < 0 ? a.Count : m;
    }

    /* §11.7-3: cabla i metodi-evento della classe UserControl (Click/Change/… ) sul firing
     * dell'evento del controllo. Il metodo generato è `Proc_<symbolId>`. */
    private void EmitUserControlEventBindings(Expr? instExpr)
    {
        if (instExpr is not VariableExpr ive) return;
        if (_program.Symbols.GetSymbol(ive.SymbolId)?.DataType is not ClassType ct || ct.Definition is null) return;
        foreach (var mem in ct.Definition.Members)
        {
            if (mem is not MethodMember mm) continue;
            var ev = ControlEventName(mm.Procedure.OriginalName.Lexeme);
            if (ev == null) continue;
            EmitExpression(instExpr);
            _il.Emit(OpCodes.Ldstr, ev);
            _il.Emit(OpCodes.Ldstr, "Proc_" + mm.Procedure.SymbolId);
            _il.Emit(OpCodes.Call, IlRuntimeFormBindUserControlEventMethod);
        }
    }

    private static string? ControlEventName(string m) => m.ToUpperInvariant() switch
    {
        "CLICK" => "CLICK", "DBLCLICK" => "DBLCLICK", "CHANGE" => "CHANGE",
        "GOTFOCUS" => "GOTFOCUS", "LOSTFOCUS" => "LOSTFOCUS",
        "MOUSEDOWN" => "MOUSEDOWN", "MOUSEUP" => "MOUSEUP", "MOUSEMOVE" => "MOUSEMOVE",
        "KEYDOWN" => "KEYDOWN", "KEYPRESS" => "KEYPRESS", "KEYUP" => "KEYUP",
        "DRAGDROP" => "DRAGDROP", "DRAGOVER" => "DRAGOVER", "VALIDATE" => "VALIDATE",
        _ => null
    };

    private void EmitFormFrame(Expr? ms)
    {
        EmitLdI4(ms); _il.Emit(OpCodes.Call, IlRuntimeFormFrameMethod);
        foreach (var (nm, into) in _formControls)
        {
            if (into is not VariableTarget vt) continue;
            _il.Emit(OpCodes.Ldstr, nm);
            bool isStr = false; try { isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType; } catch { }
            if (isStr) { _il.Emit(OpCodes.Ldstr, "text"); _il.Emit(OpCodes.Call, IlRuntimeFormControlGetStrMethod); }
            else { _il.Emit(OpCodes.Ldstr, "value"); _il.Emit(OpCodes.Call, IlRuntimeFormControlGetMethod); }
            EmitAssignTargetFromTop(vt, isStr ? new StringType() : new DoubleType());
        }
        _il.Emit(OpCodes.Ldc_R8, 0.0);   /* FORM_FRAME usato come espressione: lascia un valore */
    }

    private void EmitTuiCall(System.Reflection.MethodInfo method, Expr?[] args, Target? into)
    {
        _il.Emit(OpCodes.Ldc_I4, args.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var i = 0; i < args.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            var a = args[i];
            if (a is null)
            {
                _il.Emit(OpCodes.Ldnull);
            }
            else if (a is StringExpr se)
            {
                _il.Emit(OpCodes.Ldstr, se.Value ?? "");
            }
            else if (a is VariableExpr varr && (NormalizeInferredType(InferType(a)) is ArrayType
                     || _program.Symbols.GetSymbol(varr.SymbolId).DataType is ArrayType))
            {
                /* Array passato a un controllo (es. COMBOBOX ... voci INTO c): va
                 * passato il RIFERIMENTO all'array, non boxato come scalare (prima
                 * InferType/box lo riduceva a un double: item persi, label vuota). */
                EmitLoadVariable(varr.SymbolId);
            }
            else
            {
                EmitObjectBoxedArgument(a);
            }
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, method);

        if (into is VariableTarget wt)
        {
            var local = GetOrDeclareLocal(wt.SymbolId);
            /* Il valore di ritorno va scritto nel DESTINAZIONE REALE (globale/statico o
             * locale): EmitStoreVariable gestisce globali e statici, che Stloc su un
             * local NON aggiorna (COMBOBOX/RADIOGROUP INTO stringa restavano vuoti). */
            var tmp = _il.DeclareLocal(method.ReturnType);
            _il.Emit(OpCodes.Stloc, tmp);
            EmitStoreVariable(wt.SymbolId, () =>
            {
                _il.Emit(OpCodes.Ldloc, tmp);
                if (method.ReturnType == typeof(double))
                {
                    // I metodi TUI ritornano double (handle/valore): converti al tipo target.
                    if (local.LocalType == typeof(long)) _il.Emit(OpCodes.Conv_I8);
                    else if (local.LocalType == typeof(int) || local.LocalType == typeof(bool)) _il.Emit(OpCodes.Conv_I4);
                    else if (local.LocalType == typeof(float)) _il.Emit(OpCodes.Conv_R4);
                }
                else if (method.ReturnType == typeof(string) && local.LocalType != typeof(string))
                {
                    _il.Emit(OpCodes.Call, IlRuntimeToDoubleMethod);
                }
            });
        }
        else
        {
            _il.Emit(OpCodes.Pop);
        }
    }

    /// <summary>PRINTERDIALOG var, copies, duplex, "title" — 3 target: ritorna array
    /// [string selezionato, double copie, bool duplex] e li splitta nei 3 variabili.</summary>
    private void EmitTuiPrinterDialog(PrinterDialogStmt stmt)
    {
        var main = _il.DeclareLocal(typeof(object[]));
        _il.Emit(OpCodes.Ldc_I4, 1);
        _il.Emit(OpCodes.Newarr, typeof(object));
        _il.Emit(OpCodes.Dup);
        _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.Title is { } tt) EmitObjectBoxedArgument(tt);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Stelem_Ref);
        _il.Emit(OpCodes.Call, IlRuntimeTuiPrinterDialogMethod);
        _il.Emit(OpCodes.Stloc, main);

        // printerVar = [0] (string)
        _il.Emit(OpCodes.Ldloc, main);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldelem_Ref);
        StoreTarget(stmt.PrinterVar);

        // copiesVar = [1] (double)
        _il.Emit(OpCodes.Ldloc, main);
        _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Ldelem_Ref);
        _il.Emit(OpCodes.Unbox_Any, typeof(double));
        StoreTarget(stmt.CopiesVar);

        // duplexVar = [2] (bool)
        _il.Emit(OpCodes.Ldloc, main);
        _il.Emit(OpCodes.Ldc_I4_2);
        _il.Emit(OpCodes.Ldelem_Ref);
        _il.Emit(OpCodes.Unbox_Any, typeof(bool));
        StoreTarget(stmt.DuplexVar);
    }

    private void StoreTarget(Target t)
    {
        if (t is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private readonly System.Collections.Generic.List<AtGetStmt> _pendingReadFields = new();

    private void EmitAtGetStatement(AtGetStmt stmt)
    {
        // @GET → campo del loop READ inline (predicati WHEN/VALID inline, nessun delegate/closure).
        if (stmt.Variable is VariableTarget) _pendingReadFields.Add(stmt);
    }

    /// <summary>Al READ: per ogni campo @GET pendente emette il loop inline:
    /// WHEN(.F.→skip) ; read ; assign ; VALID(.F.→re-read). Nessun delegate.</summary>
    private void EmitPendingReadAssignments()
    {
        /* Blocchi con WHEN: esecuzione SEQUENZIALE via helper (nessuna label/branch), cosi'
         * il predicato vede i valori confermati dai campi precedenti dello stesso READ. */
        if (_pendingReadFields.Exists(x => x.When is not null))
        {
            for (var i = 0; i < _pendingReadFields.Count; i++)
            {
                var f = _pendingReadFields[i];
                if (f.Variable is not VariableTarget vt) continue;
                var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;
                var againField = _il.DefineLabel();
                var doneField = _il.DefineLabel();
                _il.MarkLabel(againField);
                EmitExpression(f.Row); _il.Emit(OpCodes.Conv_R8);
                EmitExpression(f.Col); _il.Emit(OpCodes.Conv_R8);
                if (f.ControlWidth != null) { EmitExpression(f.ControlWidth); _il.Emit(OpCodes.Conv_R8); }
                else _il.Emit(OpCodes.Ldc_R8, 10.0);
                EmitReadInitial(vt.SymbolId, isStr);
                _il.Emit(OpCodes.Ldstr, f.ControlType ?? "");
                EmitDoubleOrZero(f.MinValue ?? f.RangeLo);
                EmitDoubleOrZero(f.MaxValue ?? f.RangeHi);
                EmitDoubleOrZero(f.StepValue);
                if (f.When is not null)
                {
                    EmitExpression(f.When);
                    _il.Emit(OpCodes.Conv_I4);
                    _il.Emit(OpCodes.Ldc_I4_0);
                    _il.Emit(OpCodes.Ceq);            /* 1 = predicato falso → skip */
                }
                else _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Call, IlRuntimeReadSequentialFieldMethod);
                if (isStr) { var sLoc = _il.DeclareLocal(typeof(string)); _il.Emit(OpCodes.Stloc, sLoc);
                             EmitStoreVariable(vt.SymbolId, () => _il.Emit(OpCodes.Ldloc, sLoc)); }
                else { _il.Emit(OpCodes.Call, IlRuntimeToDoubleMethod); EmitStoreVariable(vt.SymbolId, () => { }); }
                /* VALID anche nel percorso sequenziale (blocchi con WHEN): la variabile e'
                 * gia' scritta, quindi il predicato la legge; .F. -> riproponi lo stesso campo. */
                if (f.Valid is not null)
                {
                    EmitExpression(f.Valid);
                    _il.Emit(OpCodes.Conv_I4);
                    _il.Emit(OpCodes.Ldc_I4_0);
                    _il.Emit(OpCodes.Ceq);                /* 1 = invalido */
                    _il.Emit(OpCodes.Brtrue, againField);
                }
                _il.MarkLabel(doneField);
            }
            _pendingReadFields.Clear();
            return;
        }
        // READ NATIVO (unica implementazione condivisa): add dei campi, run, assegnazione.
        for (var i = 0; i < _pendingReadFields.Count; i++)
        {
            var f = _pendingReadFields[i];
            if (f.Variable is not VariableTarget vt) continue;
            var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;
            EmitExpression(f.Row); _il.Emit(OpCodes.Conv_R8);
            EmitExpression(f.Col); _il.Emit(OpCodes.Conv_R8);
            if (f.ControlWidth != null) { EmitExpression(f.ControlWidth); _il.Emit(OpCodes.Conv_R8); } else _il.Emit(OpCodes.Ldc_R8, 10.0);
            EmitReadInitial(vt.SymbolId, isStr);
            _il.Emit(OpCodes.Ldstr, f.ControlType ?? "");
            _il.Emit(OpCodes.Ldstr, "");   // PICTURE
            _il.Emit(OpCodes.Ldstr, "");   // ITEMS
            EmitDoubleOrZero(f.MinValue ?? f.RangeLo);
            EmitDoubleOrZero(f.MaxValue ?? f.RangeHi);
            EmitDoubleOrZero(f.StepValue);
            _il.Emit(OpCodes.Call, IlRuntimeReadNativeAddMethod);
            _il.Emit(OpCodes.Pop);
            if (f.Message is not null)
            { EmitExpression(f.Message); _il.Emit(OpCodes.Call, IlRuntimeReadSetMessageMethod); }
            if (f.Error is not null)
            { EmitExpression(f.Error); _il.Emit(OpCodes.Call, IlRuntimeReadSetErrorMethod); }
        }
        var again = _il.DefineLabel();
        _il.MarkLabel(again);
        _il.Emit(OpCodes.Call, IlRuntimeReadNativeRunMethod);
        _il.Emit(OpCodes.Pop);
        for (var i = 0; i < _pendingReadFields.Count; i++)
        {
            var f = _pendingReadFields[i];
            if (f.Variable is not VariableTarget vt) continue;
            var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Call, IlRuntimeReadNativeValueMethod);
            if (isStr) { var sLoc = _il.DeclareLocal(typeof(string)); _il.Emit(OpCodes.Stloc, sLoc);
                         EmitStoreVariable(vt.SymbolId, () => _il.Emit(OpCodes.Ldloc, sLoc)); }
            else { _il.Emit(OpCodes.Call, IlRuntimeToDoubleMethod); EmitStoreVariable(vt.SymbolId, () => { }); }
        }
        for (var i = 0; i < _pendingReadFields.Count; i++)
        {
            var f = _pendingReadFields[i];
            if (f.Valid is null || f.Variable is not VariableTarget) continue;
            var ok = _il.DefineLabel();
            EmitConditionBool(f.Valid);
            _il.Emit(OpCodes.Brtrue, ok);
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Call, IlRuntimeReadNativeResumeMethod);
            _il.Emit(OpCodes.Br, again);
            _il.MarkLabel(ok);
        }
        _pendingReadFields.Clear();
    }

    /// <summary>Valore iniziale di edit di un campo @GET = il VALORE CORRENTE della variabile,
    /// boxato (string o double): il runtime lo formatta con IlGpmRuntime.EditInitial. Passare ""
    /// per i numerici faceva azzerare la variabile sia su un Enter non digitato sia su un campo
    /// saltato da WHEN (ToDouble("") == 0); C99/C99Native passano gia' il valore.</summary>
    private void EmitReadInitial(int symbolId, bool isStr)
    {
        EmitLoadVariable(symbolId);
        if (!isStr) { _il.Emit(OpCodes.Conv_R8); _il.Emit(OpCodes.Box, typeof(double)); }
    }

    private void EmitDoubleOrZero(Expr? e)
    {
        if (e != null) { EmitExpression(e); _il.Emit(OpCodes.Conv_R8); } else _il.Emit(OpCodes.Ldc_R8, 0.0);
    }

    private void EmitBoxStatement(BoxStmt stmt)
    {
        EmitInt32Arg(stmt.Top); EmitInt32Arg(stmt.Left); EmitInt32Arg(stmt.Bottom); EmitInt32Arg(stmt.Right);
        if (stmt.BorderStyle is not null) _il.Emit(OpCodes.Ldstr, stmt.BorderStyle.Lexeme); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeDrawBoxMethod);
    }

    private void EmitScrollStatement(ScrollStmt stmt)
    {
        EmitInt32Arg(stmt.Top); EmitInt32Arg(stmt.Left);
        EmitInt32Arg(stmt.Bottom); EmitInt32Arg(stmt.Right); EmitInt32Arg(stmt.Count);
        _il.Emit(OpCodes.Call, IlRuntimeScrollRegionMethod);
    }

    private void EmitStatusBarStatement(StatusBarStmt stmt)
    {
        EmitStringArg(stmt.Text);
        _il.Emit(OpCodes.Call, IlRuntimeStatusBarMethod);
    }

    private void EmitWindowSetTitleStatement(WindowSetTitleStmt stmt)
    {
        // WINDOW SET TITLE "t": imposta il titolo della finestra focalizzata (WindowManager
        // TUI). La forma qualificata WINDOW.SETTITLE non esiste (i comandi usano solo la
        // sintassi legacy a spazio — direttiva AGENTS §2.0).
        EmitStringArg(stmt.Title);
        _il.Emit(OpCodes.Call, IlRuntimeWindowSetTitleMethod);
    }

    private void EmitWindowActionStatement(WindowActionStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.Action.ToString());
        _il.Emit(OpCodes.Call, IlRuntimeWindowActionMethod);
    }

    private void EmitWindowVisibilityStatement(WindowVisibilityStmt stmt)
    {
        // WINDOW SHOW | WINDOW HIDE → IlRuntime.WindowSetVisible(bool)
        _il.Emit(OpCodes.Ldc_I4, stmt.Show ? 1 : 0);
        _il.Emit(OpCodes.Call, IlRuntimeWindowSetVisibleMethod);
    }

    private void EmitWindowAppendLineStatement(WindowAppendLineStmt stmt)
    {
        // WINDOW nome PRINT "testo" → IlRuntime.WindowAppendLine(string name, string text)
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        EmitStringArg(stmt.Text);
        _il.Emit(OpCodes.Call, IlRuntimeWindowAppendMethod);
    }

    private void EmitWindowSetContentStatement(WindowSetContentStmt stmt)
    {
        // WINDOW nome TEXT lines() → IlRuntime.WindowSetContent(string name, string[] lines)
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        EmitExpression(stmt.Lines);
        _il.Emit(OpCodes.Call, IlRuntimeWindowSetContentMethod);
    }

    private void EmitWindowClearStatement(WindowClearStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeWindowClearMethod);
    }

    private void EmitWindowScrollStatement(WindowScrollStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        EmitExpression(stmt.Delta);
        _il.Emit(OpCodes.Call, IlRuntimeWindowScrollMethod);
    }

    private void EmitWindowEditStatement(WindowEditStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        _il.Emit(stmt.Enable ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeWindowEditMethod);
    }

    private void EmitWindowSelectStatement(WindowSelectStmt stmt)
    {
        // WINDOW SELECT target → IlRuntime.WindowSelect(object)
        EmitObjectBoxedArgument(stmt.Target);
        _il.Emit(OpCodes.Call, IlRuntimeWindowSelectMethod);
    }

    private void EmitAgentNameStatement(MethodInfo method, Token name)
    {
        _il.Emit(OpCodes.Ldstr, name.Lexeme);
        _il.Emit(OpCodes.Call, method);
        // Metodi che RESTITUISCONO un valore (es. AgentSave → string): il valore va
        // scartato (statement) — senza il Pop il residuo sullo stack si propaga fino al
        // ret → InvalidProgram ("found ref 'string'" — AgentTeam).
        if (method.ReturnType != typeof(void)) _il.Emit(OpCodes.Pop);
    }

    private void EmitAgentTwoNameStatement(MethodInfo method, Token name1, Token name2)
    {
        _il.Emit(OpCodes.Ldstr, name1.Lexeme);
        _il.Emit(OpCodes.Ldstr, name2.Lexeme);
        _il.Emit(OpCodes.Call, method);
    }

    private void EmitTeamMessageStatement(TeamMessageStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Message);
        _il.Emit(OpCodes.Call, IlRuntimeTeamMessageMethod);
    }

    private void EmitTeamSetRoleStatement(TeamSetRoleStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.AgentName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.RoleName.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetRoleMethod);
    }

    private void EmitTeamSetStrategyStatement(TeamSetStrategyStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.StrategyName.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetStrategyMethod);
    }

    private void EmitTeamSetRecursionStatement(TeamSetRecursionStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitExpression(stmt.Rounds);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetRecursionMethod);
    }

    private void EmitTeamSetLatentStatement(TeamSetLatentStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.Mode.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetLatentMethod);
    }

    private void EmitTeamSetLinkStatement(TeamSetLinkStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.Which.Lexeme);
        EmitStringArg(stmt.Config);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetLinkMethod);
    }

    private void EmitTeamSetThoughtsStatement(TeamSetThoughtsStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitExpression(stmt.Count);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetThoughtsMethod);
    }

    private void EmitTeamTrainInnerStatement(TeamTrainInnerStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Agent);
        EmitStringArg(stmt.Dataset);
        _il.Emit(OpCodes.Call, IlRuntimeTeamTrainInnerMethod);
    }

    private void EmitTeamTrainOuterStatement(TeamTrainOuterStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Dataset);
        _il.Emit(OpCodes.Call, IlRuntimeTeamTrainOuterMethod);
    }

    private void EmitTeamSetDagStatement(TeamSetDagStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitExpression(stmt.DMax);
        EmitExpression(stmt.LMax);
        EmitExpression(stmt.NMax);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetDagMethod);
    }

    private void EmitTeamSetComplexityStatement(TeamSetComplexityStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitExpression(stmt.Threshold);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetComplexityMethod);
    }

    private void EmitTeamSetReflectionStatement(TeamSetReflectionStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.Mode.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetReflectionMethod);
    }

    private void EmitTeamSetCriticStatement(TeamSetCriticStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.Personas.Lexeme);
        EmitStringArg(stmt.Judge);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetCriticMethod);
    }

    private void EmitTeamSetPruneStatement(TeamSetPruneStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitExpression(stmt.TopK);
        _il.Emit(OpCodes.Call, IlRuntimeTeamSetPruneMethod);
    }

    private void EmitTeamTrainThinkingStatement(TeamTrainThinkingStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Dataset);
        _il.Emit(OpCodes.Call, IlRuntimeTeamTrainThinkingMethod);
    }

    private void EmitTeamTrainActorCriticStatement(TeamTrainActorCriticStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Dataset);
        _il.Emit(OpCodes.Call, IlRuntimeTeamTrainActorCriticMethod);
    }

    private void EmitTeamDistillThinkingStatement(TeamDistillThinkingStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Dataset);
        _il.Emit(OpCodes.Call, IlRuntimeTeamDistillThinkingMethod);
    }

    private void EmitTeamRunStatement(TeamRunStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Task);
        _il.Emit(OpCodes.Call, IlRuntimeTeamRunMethod);
        _il.Emit(OpCodes.Pop); // il risultato dell'orchestrazione è uno statement (void)
    }

    private void EmitTeamBroadcastStatement(TeamBroadcastStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Message);
        _il.Emit(OpCodes.Call, IlRuntimeTeamBroadcastMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitTeamRedefineStatement(TeamRedefineStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.TeamName.Lexeme);
        EmitStringArg(stmt.Definition);
        _il.Emit(OpCodes.Call, IlRuntimeTeamRedefineMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitWorkflowDefStatement(WorkflowDefStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.WorkflowName.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeWorkflowDefineMethod);
    }

    private void EmitWorkflowStepStatement(WorkflowStepStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.StepName.Lexeme);
        EmitStringArg(stmt.Action);
        if (stmt.RetryCount is not null) EmitInt32Arg(stmt.RetryCount); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.TimeoutMs is not null) EmitInt32Arg(stmt.TimeoutMs); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeWorkflowStepMethod);
    }

    private void EmitWorkflowExecuteStatement(WorkflowExecuteStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.WorkflowName.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeWorkflowExecuteMethod);
    }

    private void EmitMacroStatement(MacroStmt stmt)
    {
        EmitExpression(stmt.Expression);
        EmitConvertToString(InferType(stmt.Expression));
        // S_eval Gap B: snapshot/restore locali attorno alla chiamata (il risultato resta
        // sullo stack per il Pop sotto — identico stack-shape di prima).
        EmitMacroCallWithLocals(IlRuntimeMacroEvalStmtWithLocalsMethod);
        _il.Emit(OpCodes.Pop);
    }

    /// <summary>Stessa logica di <see cref="EmitMacroStatement"/> ma per &amp;expr usata come
    /// VALORE: il risultato resta sullo stack (non un Pop), è quello
    /// che il chiamante (assegnazione, PRINT, argomento di funzione, ecc.) consumerà.</summary>
    private void EmitMacroExpression(MacroExpr macro)
    {
        EmitExpression(macro.Expression);
        EmitConvertToString(InferType(macro.Expression));
        // S_eval Gap B: snapshot/restore locali (stack-shape invariato: result object).
        EmitMacroCallWithLocals(IlRuntimeMacroEvalExprWithLocalsMethod);
    }

    private void EmitRunStatement(RunStmt stmt)
    {
        EmitStringArg(stmt.Command);
        _il.Emit(OpCodes.Call, IlRuntimeRunCommandMethod);
    }

    

    private void EmitConsoleScrollModeStatement(ConsoleScrollModeStmt stmt)
    {
        // Come PCode PopInt: numerici e stringhe-numeriche al valore, resto 0 — MAI throw
        // (EmitInt32Argument scagliava FormatException su "BUFFERED"; VAL è safe).
        EmitStringArg(stmt.ModeExpression);
        _il.Emit(OpCodes.Call, StringHelpersValMethod);
        _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Ceq);
        var lblOff = _il.DefineLabel();
        var lblEnd = _il.DefineLabel();
        _il.Emit(OpCodes.Brtrue_S, lblOff);
        _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldc_I4, 25);
        _il.Emit(OpCodes.Br_S, lblEnd);
        _il.MarkLabel(lblOff);
        _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldc_I4_0);
        _il.MarkLabel(lblEnd);
        _il.Emit(OpCodes.Call, IlRuntimeSetScrollRegionMethod);
    }

    private void EmitConsoleSendKeyStatement(ConsoleSendKeyStmt stmt)
    {
        // Prima emetteva sempre "" (argomento scartato) → SENDKEY inaffidabile su gamba IL.
        EmitStringArg(stmt.KeysExpression);
        _il.Emit(OpCodes.Call, IlRuntimeConsoleSendKeysMethod);
    }

    private void EmitCursorShapeStatement(CursorShapeStmt stmt)
    {
        // SHAPE accetta nome ("BLOCK"/"UNDERLINE"/"BAR" → codici DECSCUSR 2/4/6,
        // parità PCode/VM-C/runtime C) o numero (CursorSize/DECSCUSR diretto).
        // Prima: solo numerico (FormatException sui nomi).
        if (stmt.Shape is StringExpr shapeStr)
        {
            var code = shapeStr.Value.ToUpperInvariant() switch
            {
                "BLOCK" => 2,
                "UNDERLINE" => 4,
                "BAR" => 6,
                _ => 0
            };
            if (int.TryParse(shapeStr.Value, out var shapeNum)) code = Math.Clamp(shapeNum, 0, 6);
            _il.Emit(OpCodes.Ldc_I4, code);
        }
        else EmitInt32Arg(stmt.Shape);
        _il.Emit(OpCodes.Call, IlRuntimeCursorShapeMethod);
    }

    private void EmitCursorStateStatement(CursorStateStmt stmt)
    {
        _il.Emit(stmt.State.Kind == TokenKind.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeCursorVisibleMethod);
    }

    private void EmitClearRectStatement()
    {
        _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldc_I4, 80); _il.Emit(OpCodes.Ldc_I4, 24);
        _il.Emit(OpCodes.Call, IlRuntimeClearRectMethod);
    }

    private void EmitScreenStatement(ScreenStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.Mode);
        if (stmt.ActivePage is not null) EmitInt32Arg(stmt.ActivePage); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.VisiblePage is not null) EmitInt32Arg(stmt.VisiblePage); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsScreenMethod);
    }

    private void EmitTextModeStatement(TextModeStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.Mode);
        _il.Emit(OpCodes.Callvirt, GraphicsTextModeMethod);
    }

    private void EmitSaveScreenStatement(SaveScreenStmt stmt)
    {
        EmitStringArg(stmt.Name);
        _il.Emit(OpCodes.Call, IlRuntimeSaveScreenMethod);
    }

    private void EmitRestoreScreenStatement(RestoreScreenStmt stmt)
    {
        EmitStringArg(stmt.Name);
        _il.Emit(OpCodes.Call, IlRuntimeRestoreScreenMethod);
    }

    private void EmitWindowRegionStatement(ViewPortStmt stmt)
    {
        // WINDOW (x1, y1)-(x2, y2) [SCREEN]: finestra di coordinate logiche. I valori reali
        // dall'AST (prima erano hardcoded 0,0,80,24). Firmata come SetWindowRegion(bool
        // screen, int x1, int y1, int x2, int y2).
        _il.Emit(stmt.Screen ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        EmitInt32Arg(stmt.X1); EmitInt32Arg(stmt.Y1);
        EmitInt32Arg(stmt.X2); EmitInt32Arg(stmt.Y2);
        _il.Emit(OpCodes.Call, IlRuntimeSetWindowRegionMethod);
    }

    private void EmitSetAttributeStatement(SetAttributeStmt stmt)
    {
        if (stmt.Attribute.Kind is TokenKind.Cursor)
        {
            _il.Emit(stmt.State.Kind == TokenKind.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Call, IlRuntimeCursorVisibleMethod);
            return;
        }

        if (stmt.Attribute.Kind is TokenKind.Shell)
        {
            // SET SHELL ON|OFF → IlRuntime.SetShellEnabled(bool) — opt-in per SYSTEM.SHELL/EXEC.
            _il.Emit(stmt.State.Kind == TokenKind.On ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Call, IlRuntimeSetShellEnabledMethod);
            return;
        }

        _il.Emit(OpCodes.Ldstr, stmt.Attribute.Lexeme);
        _il.Emit(OpCodes.Ldstr, stmt.State.Lexeme);
        _il.Emit(OpCodes.Call, IlRuntimeSetAttributeMethod);
    }

    private void EmitDefineWindowStatement(DefineWindowStmt stmt)
    {
        // WINDOW DEFINE name FROM top,left TO bottom,right [TITLE "t"] [DOUBLE] [SHADOW] [PIXEL] [DESKTOP n].
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        EmitInt32Arg(stmt.Top); EmitInt32Arg(stmt.Left);
        EmitInt32Arg(stmt.Bottom); EmitInt32Arg(stmt.Right);
        if (stmt.Title is not null) EmitStringArg(stmt.Title); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(stmt.Double ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(stmt.Shadow ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(stmt.Pixel ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldc_I4, (int)stmt.Style);
        if (stmt.Desktop is not null) EmitInt32Arg(stmt.Desktop); else _il.Emit(OpCodes.Ldc_I4_1);
        _il.Emit(OpCodes.Call, IlRuntimeDefineWindowMethod);
    }

    private void EmitWindowThemeStatement(WindowThemeStmt stmt)
    {
        EmitStringArg(stmt.ThemeName);
        _il.Emit(OpCodes.Call, IlRuntimeWindowThemeMethod);
    }

    private void EmitWindowDesktopStatement(WindowDesktopStmt stmt)
    {
        EmitInt32Arg(stmt.Desktop);
        _il.Emit(OpCodes.Call, IlRuntimeWindowDesktopMethod);
    }

    private void EmitWindowMoveDesktopStatement(WindowMoveDesktopStmt stmt)
    {
        _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        EmitInt32Arg(stmt.Desktop);
        _il.Emit(OpCodes.Call, IlRuntimeWindowMoveDesktopMethod);
    }

    private void EmitWindowFrameStatement(WindowFrameStmt stmt)
    {
        if (stmt.Name is not null) _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldc_I4, (int)stmt.Style);
        _il.Emit(OpCodes.Call, IlRuntimeWindowFrameMethod);
    }

    private void EmitWindowShadowStatement(WindowShadowStmt stmt)
    {
        if (stmt.Name is not null) _il.Emit(OpCodes.Ldstr, stmt.Name.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(stmt.Enable ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, IlRuntimeWindowShadowMethod);
    }

    private void EmitPrinterDialogStatement(PrinterDialogStmt stmt)
    {
        _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Ldnull);
        if (stmt.Title is not null) EmitStringArg(stmt.Title); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimePrinterDialogMethod);
    }

    private void EmitNullableIntArg(Expr? expr)
    {
        /* Il parametro è Nullable<int> (value type), NON object: boxare l'int o pushare
         * Ldnull produce uno stack type mismatch → InvalidProgramException a JIT (MOUSE
         * LIMIT/UNLIMIT). Presente: new Nullable<int>(v). Assente: default(Nullable<int>)
         * via initobj (Ldnull renderebbe HasValue=true con 0 → semantica UNLIMIT rotta). */
        if (expr is not null)
        {
            EmitInt32Arg(expr);
            _il.Emit(OpCodes.Newobj, typeof(int?).GetConstructor(new[] { typeof(int) })!);
        }
        else
        {
            var tmp = _il.DeclareLocal(typeof(int?));
            _il.Emit(OpCodes.Ldloca, tmp);
            _il.Emit(OpCodes.Initobj, typeof(int?));
            _il.Emit(OpCodes.Ldloc, tmp);
        }
    }

    // ── SQL ──────────────────────────────────────────────────────────
    private void EmitDatabaseNoop() { EmitLoadDatabaseService(); _il.Emit(OpCodes.Pop); }

    /// <summary>Parità col compilatore bytecode (`Compiler.Sql.EmitAliasOrExpression`):
    /// l'alias di un comando SQL, se è un NOME (VariableExpr / QualifiedName a 1 parte), è la
    /// STRINGA del nome — NON il valore della variabile. `SQL EXEC C, ...` usa l'alias "C"
    /// registrato da `SQL OPEN ... ALIAS C`, visibile a ogni procedura/goroutine; caricare la
    /// variabile `C` (locale alla procedura) rendeva il comando "database is not open" fuori da Main.</summary>
    private void EmitSqlAliasObject(Expr alias) => EmitSqlAliasCore(alias, boxObject: true);
    private void EmitSqlAliasString(Expr alias) => EmitSqlAliasCore(alias, boxObject: false);
    private void EmitSqlAliasCore(Expr alias, bool boxObject)
    {
        if (alias is VariableExpr ve) { _il.Emit(OpCodes.Ldstr, ve.NameCleaned.Lexeme); return; }
        if (alias is QualifiedNameExpr qn && qn.Parts.Length == 1) { _il.Emit(OpCodes.Ldstr, qn.Parts[0].Lexeme); return; }
        if (boxObject) EmitObjectBoxedArgument(alias); else EmitStringArg(alias);
    }

    private void EmitSqlOpenStatement(SqlOpenStmt stmt)
    {
        // SqlOpen statico (porting GM 7b8546b9 — senza EmitLoadDatabaseService: il service
        // restava sullo stack al return della void = InvalidProgram).
        EmitStringArg(stmt.FilePath);
        if (stmt.OriginalAliasToken is not null) _il.Emit(OpCodes.Ldstr, stmt.OriginalAliasToken.Lexeme);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlOpen), typeof(string), typeof(string)));
        var aliasSymbol = _program.Symbols.FindSymbolsByName(stmt.OriginalAliasToken!.Lexeme).FirstOrDefault();
        if (aliasSymbol is null) _il.Emit(OpCodes.Pop);
        else
        {
            var aliasLocal = GetOrDeclareLocal(aliasSymbol.Id);
            if (aliasLocal.LocalType == typeof(double)) _il.Emit(OpCodes.Conv_R8);
            _il.Emit(OpCodes.Stloc, aliasLocal);
        }
    }

    private void EmitSqlCloseStatement(SqlCloseStmt stmt)
    {
        // DbAlias può essere un handle numerico (DIM AS LONG) o un alias STRINGA
        // ("govdb") — il runtime SqlClose(object?) risolve entrambi (parità VM).
        EmitSqlAliasObject(stmt.DbAlias);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlClose), typeof(object)));
    }

    private void EmitSqlCloseResultSetStatement(SqlCloseResultSetStmt stmt)
    {
        EmitDoubleArgument(stmt.ResultSet);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlCloseResultSet), typeof(double)));
    }

    private void EmitSqlExecStatement(SqlExecStmt stmt)
    {
        EmitSqlAliasObject(stmt.DbAlias);
        EmitStringArg(stmt.Sql);
        EmitSqlParameterArray(stmt.Parameters);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlExecDriver), typeof(object), typeof(string), typeof(object[])));
    }

    private void EmitSqlQueryStatement(SqlQueryStmt stmt)
    {
        EmitSqlAliasObject(stmt.DbAlias);
        EmitStringArg(stmt.Sql);
        EmitSqlParameterArray(stmt.Parameters);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlQueryDriver), typeof(object), typeof(string), typeof(object[])));
        if (stmt.ResultSet is VariableTarget vt)
        {
            // Store corretto (global/local/static/param), NON GetOrDeclareLocal: per un
            // result-set a livello MODULO (globale) lo Stloc diretto scriveva un local
            // fantasma mentre le letture (PRINT/SQL.FETCH) usavano LoadGlobal → rs nullo.
            EmitAssignTargetFromTop(stmt.ResultSet,
                NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType));
        }
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitSqlParameterArray(ImmutableArray<Expr> parameters)
    {
        _il.Emit(OpCodes.Ldc_I4, parameters.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var index = 0; index < parameters.Length; index++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, index);
            EmitCryptoObjectArgument(parameters[index]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
    }

    private void EmitSqlAttachStatement(SqlAttachStmt stmt)
    {
        // Parità VM (SysCall.SQL_ATTACH): ATTACH sul driver SqlDriver dedicato
        // (Attach(path, schema) — ATTACH DATABASE @path AS "schema"), NON query SQL
        // concatata (prima: Concat("ATTACH DATABASE 'aux' ", path) = sintassi invertita
        // e non parametrizzata -> ATTACH non eseguito -> query su aux.* vuota).
        EmitSqlAliasObject(stmt.DbAlias);
        EmitExpression(stmt.AttachPath);
        EmitConvertToString(InferType(stmt.AttachPath));
        _il.Emit(OpCodes.Ldstr, stmt.SchemaToken.Lexeme);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlAttach), typeof(object), typeof(string), typeof(string)));
    }
    private void EmitSqlDetachStatement(SqlDetachStmt stmt)
    {
        EmitSqlAliasObject(stmt.DbAlias);
        EmitStringArg(stmt.SchemaName);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlDetach), typeof(object), typeof(string)));
    }
    private void EmitSqlBackupStatement(SqlBackupStmt stmt)
    {
        // Backup sul SqlDriver (SqlDriver.Backup — SQLite reale; gli altri provider
        // lanciano NotSupportedException → errore BASIC). Prima emetteva il testo
        // "BACKUP DATABASE TO <dest>" (sintassi SQL Server) via SqlExecute: gap.
        EmitSqlAliasObject(stmt.DbAlias);
        EmitExpression(stmt.DestinationPath);
        EmitConvertToString(InferType(stmt.DestinationPath));
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SqlBackup), typeof(object), typeof(string)));
    }

    // ── VectorDB ─────────────────────────────────────────────────────
    // ── Stop ─────────────────────────────────────────────────────────
    private void EmitVectorDbStatement(Stmt stmt)
    {
        switch (stmt)
        {
            case VectorDbEnsureCollectionStmt vec:
                EmitStringArg(vec.DbPath); EmitStringArg(vec.CollectionName);
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorEnsureCollection), typeof(string), typeof(string), typeof(double)));
                _il.Emit(OpCodes.Pop);
                break;
            case VectorDbIndexTextStmt vi:
                EmitStringArg(vi.DbPath); EmitStringArg(vi.CollectionName);
                EmitStringArg(vi.DocId); EmitStringArg(vi.Text);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorIndexText), typeof(string), typeof(string), typeof(string), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            case VectorDbIndexMetaStmt vm:
                // vm.MetaJson finiva sempre nello slot "text" di AiVectorIndexMeta (con
                // metadata forzato a Ldnull): VECTORDB INDEX META non aveva un argomento
                // Text separato, quindi il JSON dei metadata veniva indicizzato come TESTO
                // del documento e i metadata reali andavano perduti — VECTORDB PURGE
                // DEPRECATED (che filtra per lo status nei metadata) restava un no-op
                // silenzioso su ogni documento indicizzato così.
                EmitStringArg(vm.DbPath); EmitStringArg(vm.CollectionName);
                EmitStringArg(vm.DocId); EmitStringArg(vm.Text); EmitStringArg(vm.MetaJson);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorIndexMeta), typeof(string), typeof(string), typeof(string), typeof(string), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            // --- TORCH (comandi legacy, nessun valore di ritorno) ---
            case TorchSetDeviceStmt tds:
                EmitStringArg(tds.Device);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TorchSetDevice), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            case TorchLoadLibStmt tll:
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TorchLoadLib)));
                _il.Emit(OpCodes.Pop);
                break;
            case TorchUnloadLibStmt tul:
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TorchUnloadLib)));
                _il.Emit(OpCodes.Pop);
                break;
            case TorchSetLibPathStmt tslp:
                EmitStringArg(tslp.LibPath);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TorchSetLibPath), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            case TorchInfoStmt tii:
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TorchInfo)));
                _il.Emit(OpCodes.Pop);
                break;
            case VectorDbSearchStmt vs:
                EmitStringArg(vs.DbPath); EmitStringArg(vs.CollectionName);
                EmitStringArg(vs.Query); EmitDoubleArgument(vs.TopK);
                _il.Emit(OpCodes.Ldc_R8, 1.0);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldc_R8, 1.0);              // reranker default (%RERANK_KEYWORD)
                _il.Emit(OpCodes.Ldc_R8, double.NaN);        // minEvidence off
                _il.Emit(OpCodes.Ldc_R8, 0.0);               // explain off
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorSearch), typeof(string), typeof(string), typeof(string), typeof(double), typeof(double), typeof(string), typeof(double), typeof(double), typeof(double)));
                EmitAssignTargetFromTop(vs.Result, new StringType());
                break;
            case VectorDbCountStmt vc:
                EmitStringArg(vc.DbPath); EmitStringArg(vc.CollectionName);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorCountEmbeddings), typeof(string), typeof(string)));
                EmitAssignTargetFromTop(vc.Result, new LongType());
                break;
            case VectorDbListCollectionsStmt vl:
                EmitStringArg(vl.DbPath);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorListCollections), typeof(string)));
                EmitAssignTargetFromTop(vl.Result, new StringType());
                break;
            case VectorDbRemoveSourceStmt vr:
                EmitStringArg(vr.DbPath); EmitStringArg(vr.CollectionName);
                EmitStringArg(vr.SourceId);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorRemoveSource), typeof(string), typeof(string), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            case VectorDbLoadDocumentsStmt vld:
                EmitStringArg(vld.DbPath); EmitStringArg(vld.CollectionName);
                EmitStringArg(vld.DirectoryPath); _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorLoadDocuments), typeof(string), typeof(string), typeof(string), typeof(string), typeof(string)));
                EmitAssignTargetFromTop(vld.Result, new DoubleType());
                break;
            case VectorDbPurgeDeprecatedStmt vp:
                EmitStringArg(vp.DbPath); EmitStringArg(vp.CollectionName);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVectorPurgeDeprecated), typeof(string), typeof(string)));
                _il.Emit(OpCodes.Pop);
                break;
            default:
                break;
        }
    }

    private void EmitMenuStatement(MenuStmt stmt)
    {
        EmitLoadConsoleService();
        _il.Emit(OpCodes.Ldstr, "");
        EmitExpression(stmt.Items);
        _il.Emit(OpCodes.Call, IlRuntimeStringifyArrayMethod);
        _il.Emit(OpCodes.Callvirt, ConsoleMenuMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitFileDialogStatement(FileDialogStmt stmt)
    {
        EmitLoadConsoleService();
        EmitStringArg(stmt.Title);
        EmitStringArg(stmt.Mask);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, ConsoleFileDialogMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitColorPickerStatement(ColorPickerStmt stmt)
    {
        EmitLoadConsoleService();
        _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Ldc_I4, 0);
        _il.Emit(OpCodes.Callvirt, ConsoleColorPickerMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitDatePickerStatement(DatePickerStmt stmt)
    {
        EmitLoadConsoleService();
        _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Callvirt, ConsoleDatePickerMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitSpinnerStatement(SpinnerStmt stmt)
    {
        // Registry TUI runtime (come gli altri controlli): il controllo è creabile/leggibile via
        // CONTROL GET/SET (PRESET/INDEX). Il path C# ClipperLib console non lo registrava.
        EmitTuiCall(IlRuntimeTuiSpinnerMethod, [stmt.Row, stmt.Col, new NumberExpr(4.0, stmt.Token), stmt.Preset], stmt.Variable);
        EmitTuiIdIL(stmt.Id);
        EmitTuiPropsIL(stmt.Props, stmt.Token);
        if (stmt.Events != null) foreach (var _e in stmt.Events) EmitControlEvent(_e);
    }

    private void EmitToggleStatement(ToggleStmt stmt)
    {
        EmitLoadConsoleService();
        _il.Emit(OpCodes.Ldstr, "");
        EmitStringArg(stmt.Caption);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, ConsoleToggleMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitSliderStatement(SliderStmt stmt)
    {
        EmitLoadConsoleService();
        _il.Emit(OpCodes.Ldstr, "");
        EmitDoubleArgument(stmt.Min);
        EmitDoubleArgument(stmt.Max);
        EmitDoubleArgument(stmt.Min); // initial = min
        _il.Emit(OpCodes.Callvirt, ConsoleSliderMethod);
        if (stmt.Variable is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitRenameStatement(RenameStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.OldFileName); EmitStringArg(stmt.NewFileName);
        _il.Emit(OpCodes.Callvirt, FileMoveMethod);
    }

    private void EmitMkDirStatement(MkDirStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.DirectoryPath);
        _il.Emit(OpCodes.Callvirt, FileCreateDirectoryMethod);
    }

    private void EmitChDirStatement(ChDirStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.DirectoryPath);
        _il.Emit(OpCodes.Callvirt, FileSetCurrentDirectoryMethod);
    }

    private void EmitCopyStatement(CopyStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.SourceFile); EmitStringArg(stmt.DestinationFile);
        _il.Emit(OpCodes.Callvirt, FileCopyMethod);
    }

    private void EmitMoveStatement(MoveStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.SourceFile); EmitStringArg(stmt.DestinationFile);
        _il.Emit(OpCodes.Callvirt, FileMoveMethod);
    }

    private void EmitRmDirStatement(RmDirStmt stmt)
    {
        EmitStringArg(stmt.DirectoryPath);
        if (stmt.RecursiveExpr is not null) EmitExpression(stmt.RecursiveExpr); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RemoveDirectory)));
    }

    private void EmitXCopyStatement(XCopyStmt stmt)
    {
        EmitStringArg(stmt.SourceDirectory);
        EmitStringArg(stmt.DestinationDirectory);
        if (stmt.RecursiveExpr is not null) EmitExpression(stmt.RecursiveExpr); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.OverwriteExpr is not null) EmitExpression(stmt.OverwriteExpr); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CopyDirectory)));
    }

    private void EmitAtSayStatement(AtSayStmt stmt)
    {
        // @ SAY row, col, expr → IlRuntime.AtSayDispatch(row, col, text): con finestra attiva
        // scrive nel buffer della finestra (coordinate relative viewport); altrimenti console.
        EmitInt32Arg(stmt.Row);
        EmitInt32Arg(stmt.Col);
        if (stmt.Picture is not null)
        {
            EmitCryptoObjectArgument(stmt.Expression);
            EmitStringArg(stmt.Picture);
            _il.Emit(OpCodes.Call, StringHelpersFormatMethod);
        }
        else
        {
            EmitExpression(stmt.Expression); EmitConvertToString(NormalizeInferredType(InferType(stmt.Expression)));
        }
        _il.Emit(OpCodes.Call, IlRuntimeAtSayDispatchMethod);
    }

}
