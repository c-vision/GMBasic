// FILE: Interpreter.Compiler.Compiler.Console.cs

// ReSharper disable InconsistentNaming



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni relative alla console,
/// all'input da tastiera e agli appunti.
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione CLS.
    /// </summary>
    private void VisitClsStatement(ClsStmt stmt)
    {
        if (stmt.Color is not null) VisitExpression(stmt.Color);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLS); EmitByte((byte)1);
    }
    
    /// <summary>
    /// Compila un'istruzione LOCATE.
    /// </summary>
    private void VisitLocateStatement(LocateStmt stmt)
    {
        VisitExpression(stmt.LineExpr);
        VisitExpression(stmt.ColumnExpr);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LOCATE); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione INPUT.
    /// </summary>
    private void VisitInputStatement(InputStmt stmt)
    {
        if (stmt.Prompt is not null) VisitExpression(stmt.Prompt);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.INPUT); EmitByte((byte)(stmt.Variables.Length));
        
        for (var i = stmt.Variables.Length - 1; i >= 0; i--)
        {
            AssignVariable(stmt.Variables[i], isLet: false);
        }
    }
    
    /// <summary>
    /// Compila un'istruzione LINE INPUT.
    /// </summary>
    private void VisitLineInputStatement(LineInputStmt stmt)
    {
        if (stmt.Prompt is not null) VisitExpression(stmt.Prompt);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LINE_INPUT); EmitByte((byte)1);
        
        // CORREZIONE ARCHITETTURALE:
        // `stmt.Variable` è già il Target corretto, fornito dall'AST refattorizzato.
        // Il compilatore deve semplicemente passarlo alla funzione di assegnazione.
        AssignVariable(stmt.Variable, isLet: false);
    }
    
    /// <summary>
    /// Compila un'istruzione GET (tastiera).
    /// </summary>
    private void VisitGetKeyboardStatement(GetKeyboardStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GET_KEY); EmitByte((byte)0);
        
        // CORREZIONE ARCHITETTURALE:
        // `stmt.Variable` è già il Target corretto fornito dall'AST.
        AssignVariable(stmt.Variable, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione INKEY.
    /// </summary>
    private void VisitInkeyStatement(InkeyStmt stmt)
    {
        if (stmt.TimeoutExpr is not null) VisitExpression(stmt.TimeoutExpr);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.INKEY); EmitByte((byte)1);
        
        // CORREZIONE ARCHITETTURALE:
        // `stmt.Variable` è già il Target corretto fornito dall'AST.
        AssignVariable(stmt.Variable, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione WAITKEY.
    /// </summary>
    private void VisitWaitKeyStatement(WaitKeyStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WAITKEY); EmitByte((byte)0);
        EmitByte((byte)OpCode.OP_POP); 
    }
    
    /// <summary>
    /// Compila un'istruzione COLOR.
    /// </summary>
    private void VisitColorStatement(ColorStmt stmt)
    {
        if (stmt.Foreground is not null) VisitExpression(stmt.Foreground);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Background is not null) VisitExpression(stmt.Background);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.COLOR); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione PRINT in modo ottimale, utilizzando opcode dedicati
    /// per ridurre il numero totale di istruzioni P-Code.
    /// </summary>
    private void VisitPrintStatement(PrintStmt stmt)
    {
        // Caso speciale: PRINT senza argomenti è solo un a capo.
        if (stmt.Arguments.IsEmpty)
        {
            EmitByte((byte)OpCode.OP_PRINT_NEWLINE);
            return;
        }

        for (var i = 0; i < stmt.Arguments.Length; i++)
        {
            var arg = stmt.Arguments[i];
            var isLastArgument = (i == stmt.Arguments.Length - 1);

            if (arg.Expression != null)
            {
                VisitExpression(arg.Expression);
                
                // Se questa espressione è l'ultima parte dell'istruzione e non è
                // seguita da un separatore, usiamo l'opcode ottimizzato OP_PRINT_LINE
                // che stampa il valore e va a capo in una sola operazione.
                if (isLastArgument && arg.Separator is null)
                {
                    EmitByte((byte)OpCode.OP_PRINT_LINE);
                }
                else
                {
                    // Altrimenti, usiamo OP_PRINT_VALUE che stampa solo il valore.
                    EmitByte((byte)OpCode.OP_PRINT_VALUE);
                }
            }
            else if (arg.Separator == ",")
            {
                // La virgola si traduce direttamente nell'opcode di tabulazione.
                EmitByte((byte)OpCode.OP_PRINT_TAB_ZONE);
            }
            // Il punto e virgola ';' non produce P-Code, perché la sua funzione
            // (sopprimere lo spazio/newline) è il comportamento di default tra
            // due OP_PRINT_VALUE consecutivi.
        }

        // Se l'istruzione PRINT termina con una virgola o un punto e virgola,
        // il newline finale viene soppresso. La nostra logica sopra già gestisce
        // questo caso non emettendo OP_PRINT_LINE o OP_PRINT_NEWLINE.
        // Se l'istruzione termina con un'espressione, OP_PRINT_LINE viene già emesso.
        // L'unico caso non gestito è un `PRINT` seguito da un commento `REM`,
        // che il parser potrebbe non considerare come "senza separatore finale".
        // La logica attuale è sufficientemente robusta per la maggior parte dei casi.
    }
    
    /// <summary>
    /// Compila un'istruzione SET.
    /// </summary>
    private void VisitSetAttributeStatement(SetAttributeStmt stmt)
    {
        EmitConstant((double)stmt.Attribute.Kind);
        EmitConstant((double)stmt.State.Kind);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_ATTRIBUTE); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione CURSOR ON/OFF.
    /// </summary>
    private void VisitCursorStateStatement(CursorStateStmt stmt)
    {
        EmitConstant((double)stmt.State.Kind);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CURSOR_STATE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione CURSOR SHAPE.
    /// </summary>
    private void VisitCursorShapeStatement(CursorShapeStmt stmt)
    {
        VisitExpression(stmt.Shape);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CURSOR_SHAPE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione CLIP, usando il nuovo opcode OP_CLIP_CMD.
    /// </summary>
    private void VisitClipStatement(ClipStmt stmt)
    {
        var subOpCode = stmt.Command.Kind switch
        {
            TokenKind.Get => ClipSubOpCode.GET,
            TokenKind.Set => ClipSubOpCode.SET,
            TokenKind.Clear => ClipSubOpCode.CLEAR,
            _ => throw new NotImplementedException($"Unsupported CLIP command: {stmt.Command.Kind}")
        };

        byte argCount = 0;
        if (stmt.Argument is not null) 
        {
            VisitExpression(stmt.Argument);
            argCount = 1;
        }
        
        EmitBytes((byte)OpCode.OP_CLIP_CMD, (byte)subOpCode, argCount);
        
        if (stmt.Destination is not null)
        {
            // Se il comando restituisce un valore (es. GET), lo assegniamo.
            AssignVariable(stmt.Destination, isLet: false);
        }
    }

    /// <summary>
    /// Compila @ row, col SAY expr [PICTURE pic]
    /// </summary>
    private void VisitAtSayStatement(AtSayStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        VisitExpression(stmt.Expression);
        if (stmt.Picture is not null) VisitExpression(stmt.Picture);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.AT_SAY); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila @ row, col GET variable [options]
    /// </summary>
    private void VisitAtGetStatement(AtGetStmt stmt)
    {
        VisitExpression(stmt.Row);
        VisitExpression(stmt.Col);
        // Riferimento alla variabile (ByRefBox/RefToVar) per l'editing GET — NON un'assegnazione:
        // AssignVariable(isLet:false) emetteva OP_STORE_LOCAL (scriveva il TOP dello stack come
        // se il syscall avesse pushato un risultato), ma AT_GET crea il Get (valore corrente +
        // apply al READ), quindi serve il REF della variabile.
        if (stmt.Variable is VariableTarget vtG)
        {
            var nmG = vtG.NameCleaned.Lexeme;
            var upIdxG = ResolveUpvalueByName(nmG);
            if (upIdxG != -1) { EmitByte((byte)OpCode.OP_PUSH_NIL); } // ref upvalue: non supportato (gap)
            else
            {
                var liG = ResolveLocal(nmG);
                // LOCALI: si passa lo SLOT come NUMERO. La VM/C (gmb_task_capture_local_cell)
                // e la PCodeVM costruiscono una ByRefBox write-through, che funziona anche per
                // le STRINGHE. Con OP_MAKE_BYREF la variabile stringa arrivava al campo come
                // VALORE e il testo digitato andava perso (VM/C mostrava il campo vuoto).
                if (liG >= 0) { var slotCiG = AddConstant((double)liG); EmitByte((byte)OpCode.OP_PUSH_CONST); EmitShort((ushort)slotCiG); }
                else if (ShouldUseDynamicScope(NormalizeIdentifierName(nmG))) EmitConstant(nmG);
                else if (_globals.TryGetValue(nmG, out var giG)) { EmitByte((byte)OpCode.OP_MAKE_BYREF_GLOBAL); EmitShort(giG); }
                else EmitByte((byte)OpCode.OP_PUSH_NIL);
            }
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Picture is not null) VisitExpression(stmt.Picture); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Valid is not null)
        {
            // VALID: NON valuta l'espressione qui (sarebbe il valore PRE-READ, inutile come
            // predicato live). Registra una subroutine (emessa a fine procedura) e pusha il suo
            // indirizzo: la VM la chiama quando deve rivalutare col valore corrente del campo.
            var pool = _enclosing != null ? _enclosing._chunk.Constants : _chunk.Constants;
            var validCi = AddConstant(0.0); // placeholder → patchato con l'indirizzo della subroutine
            _pendingValidPredicates.Add((validCi, _currentProcedureName, stmt.Valid, pool));
            if (validCi > ushort.MaxValue)
                throw new Exception("Constant pool limit (65536) exceeded.");
            EmitByte((byte)OpCode.OP_PUSH_CONST);
            EmitByte((byte)((validCi >> 8) & 0xFF));
            EmitByte((byte)(validCi & 0xFF));
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.RangeLo is not null) VisitExpression(stmt.RangeLo); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.RangeHi is not null) VisitExpression(stmt.RangeHi); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ControlType is not null)
            { EmitConstant(stmt.ControlType); }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Items is not null) VisitExpression(stmt.Items); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ControlWidth is not null) VisitExpression(stmt.ControlWidth); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ControlHeight is not null) VisitExpression(stmt.ControlHeight); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Caption is not null) VisitExpression(stmt.Caption); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.StepValue is not null) VisitExpression(stmt.StepValue); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.MinValue is not null) VisitExpression(stmt.MinValue); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.MaxValue is not null) VisitExpression(stmt.MaxValue); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.When is not null)
        {
            var poolW = _enclosing != null ? _enclosing._chunk.Constants : _chunk.Constants;
            var whenCi = AddConstant(0.0);
            _pendingValidPredicates.Add((whenCi, _currentProcedureName, stmt.When, poolW));
            if (whenCi > ushort.MaxValue) throw new Exception("Constant pool limit (65536) exceeded.");
            EmitByte((byte)OpCode.OP_PUSH_CONST);
            EmitByte((byte)((whenCi >> 8) & 0xFF));
            EmitByte((byte)(whenCi & 0xFF));
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        // 17 argomenti: row, col, var, picture, valid, rangeLo, rangeHi, color, type, items, w, h, caption, step, min, max, when
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.AT_GET); EmitByte((byte)17);
        /* MESSAGE/ERROR: testo di stato del campo (ultimo aggiunto). */
        if (stmt.Message is not null)
        { VisitExpression(stmt.Message); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_MESSAGE); EmitByte((byte)1); }
        if (stmt.Error is not null)
        { VisitExpression(stmt.Error); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_ERROR); EmitByte((byte)1); }
    }

    /// <summary>
    /// Compila READ (Clipper interactive) — avvia il loop interattivo.
    /// </summary>
    private void VisitClipperReadStatement(ClipperReadStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLIPPER_READ); EmitByte((byte)0);
    }

    /// <summary>
    /// CCLS [color]: pulisce la console TUI (anche con la GUI aperta — i comandi C* vanno
    /// SEMPRE alla console, mai alla finestra grafica).
    /// </summary>
    private void VisitCClsStatement(CCLSStmt stmt)
    {
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CCLS); EmitByte((byte)1);
    }

    /// <summary>
    /// CLOCATE row, col: posiziona il cursore della console TUI.
    /// </summary>
    private void VisitCLocateStatement(CLocateStmt stmt)
    {
        VisitExpression(stmt.LineExpr);
        VisitExpression(stmt.ColumnExpr);
        EmitConstant(-1.0); // fg: usa il colore corrente
        EmitConstant(-1.0); // bg
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLOCATE); EmitByte((byte)4);
    }

    /// <summary>
    /// CPRINT testo: scrive sulla console TUI (con la GUI aperta il testo va alla console,
    /// NON alla finestra — il syscall CPRINT usa _runtime.Console direttamente).
    /// </summary>
    private void VisitCPrintStatement(CPrintStmt stmt)
    {
        EmitConstant(-1.0); // row: posizione corrente
        EmitConstant(-1.0); // col
        if (stmt.Arguments.Length == 1 && stmt.Arguments[0].Expression is { } onlyExpr)
        {
            VisitExpression(onlyExpr);
        }
        else
        {
            // Più argomenti: concatenali con il separatore (o spazio di default)
            for (var i = 0; i < stmt.Arguments.Length; i++)
            {
                var arg = stmt.Arguments[i];
                if (arg.Expression is { } e) VisitExpression(e);
                else EmitConstant("");
                if (i < stmt.Arguments.Length - 1)
                {
                    EmitConstant(arg.Separator ?? " ");
                    EmitByte((byte)OpCode.OP_ADD);
                }
            }
        }
        // Terminatore come PRINT (parita' IL): '\n' se ultimo separatore assente o "\n",
        // TAB se ','; nessuno se ';'.
        {
            var cpLast = stmt.Arguments.Length > 0 ? stmt.Arguments[^1].Separator : null;
            if (cpLast == ",") { EmitConstant("\t"); EmitByte((byte)OpCode.OP_ADD); }
            else if (cpLast == null || cpLast == "\n") { EmitConstant("\n"); EmitByte((byte)OpCode.OP_ADD); }
        }
        EmitConstant(-1.0); // fg
        EmitConstant(-1.0); // bg
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CPRINT); EmitByte((byte)5);
    }

    /// <summary>
    /// Compila BOX top, left, bottom, right [SINGLE|DOUBLE]
    /// </summary>
    private void VisitBoxStatement(BoxStmt stmt)
    {
        VisitExpression(stmt.Top);
        VisitExpression(stmt.Left);
        VisitExpression(stmt.Bottom);
        VisitExpression(stmt.Right);
        if (stmt.BorderStyle != null)
            EmitConstant(stmt.BorderStyle.Lexeme);
        else
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.BOX_DRAW); EmitByte((byte)5);
    }

    /// <summary>
    /// Compila SCROLL top, left, bottom, right, n
    /// </summary>
    private void VisitScrollStatement(ScrollStmt stmt)
    {
        VisitExpression(stmt.Top);
        VisitExpression(stmt.Left);
        VisitExpression(stmt.Bottom);
        VisitExpression(stmt.Right);
        VisitExpression(stmt.Count);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SCROLL_REGION); EmitByte((byte)5);
    }

    private void VisitSetClipperStatement(SetClipperStmt stmt)
    {
        // subCode numerico coerente con ExecuteSetClipper (0-5 esistenti; 6-16 i nuovi Clipper).
        int subCode = stmt.AttributeName.ToUpperInvariant() switch
        {
            "WRAP" => 6, "ESCAPE" => 7, "DELIMITERS" => 8, "TYPEAHEAD" => 9, "INTENSITY" => 10,
            "EPOCH" => 11, "DECIMALS" => 12, "FIXED" => 13, "PRINTER" => 14, "DEVICE" => 15, "MESSAGE" => 16,
            _ => -1
        };
        VisitExpression(stmt.Value);
        EmitConstant((double)subCode);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SET_CLIPPER); EmitByte((byte)2);
    }
}
