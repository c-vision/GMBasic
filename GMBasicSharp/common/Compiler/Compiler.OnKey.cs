// FILE: Interpreter.Compiler.Compiler.OnKey.cs



namespace Compiler;

public sealed partial class Compiler
{
    private const int ON_KEY_ACTION_STOP = 0;
    private const int ON_KEY_ACTION_GOTO = 1;
    private const int ON_KEY_ACTION_GOSUB = 2;
    private const int ON_KEY_ACTION_CALL = 3;

    /// <summary>
    /// Compila un'istruzione ON KEY.
    /// </summary>
    private void VisitOnKeyStatement(OnKeyStmt stmt)
    {
        // 1. Compila l'espressione del tasto.
        if (stmt.KeyExpression is not null)
        {
            VisitExpression(stmt.KeyExpression);
        }
        else
        {
            EmitConstant(0.0); // Placeholder per "tutti i tasti" in ON KEY STOP.
        }

        // 2. Compila l'azione e il suo target (se necessario).
        switch (stmt.Action)
        {
            case StopAction:
                EmitConstant((double)ON_KEY_ACTION_STOP);
                EmitConstant(0.0); // Indirizzo non usato, placeholder.
                break;
                
            case GotoAction gotoAction:
                EmitConstant((double)ON_KEY_ACTION_GOTO);
                // Aggiunge un placeholder 'null' alla constant pool, ottiene il suo indice,
                // e registra quell'indice per il patching.
                var gotoConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gotoConstIndex);
                _constantsToPatch.Add((gotoConstIndex, gotoAction.TargetLabel));
                break;

            case GosubAction gosubAction:
                EmitConstant((double)ON_KEY_ACTION_GOSUB);
                var gosubConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gosubConstIndex);
                _constantsToPatch.Add((gosubConstIndex, gosubAction.TargetLabel));
                break;

            case CallAction callAction:
                EmitConstant((double)ON_KEY_ACTION_CALL);
                if (callAction.Call is not FunctionExpr callExpr)
                {
                    throw new InvalidOperationException("Azione ON KEY CALL non valida: attesa una chiamata a procedura.");
                }
                var callConstIndex = AddConstant(null); // Placeholder per l'indirizzo
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(callConstIndex);
                _constantsToPatch.Add((callConstIndex, callExpr.OriginalToken));
                break;
        }

        // 3. Emette l'istruzione finale.
        EmitByte((byte)OpCode.OP_ON_KEY);
    }
    /// <summary>
    /// Compila un'istruzione ON MOUSE (CLICK/DBLCLICK/DOWN/UP/MOVE/SCROLL). Stessa struttura
    /// di ON KEY: stack = [eventKind, actionType, targetAddr] + OP_ON_MOUSE. La VM registra
    /// l'handler nel EventDispatcher (RegisterMouse) e il dispatch (mouse TUI/GUI) spawna il
    /// task dell'handler (FireMouseEvent).
    /// </summary>
    private void VisitOnMouseStatement(OnMouseStmt stmt)
    {
        EmitConstant((double)(int)stmt.EventKind);

        switch (stmt.Action)
        {
            case StopAction:
                EmitConstant((double)ON_KEY_ACTION_STOP);
                EmitConstant(0.0);
                break;

            case GotoAction gotoAction:
                EmitConstant((double)ON_KEY_ACTION_GOTO);
                var gotoConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gotoConstIndex);
                _constantsToPatch.Add((gotoConstIndex, gotoAction.TargetLabel));
                break;

            case GosubAction gosubAction:
                EmitConstant((double)ON_KEY_ACTION_GOSUB);
                var gosubConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gosubConstIndex);
                _constantsToPatch.Add((gosubConstIndex, gosubAction.TargetLabel));
                break;

            case CallAction callAction:
                EmitConstant((double)ON_KEY_ACTION_CALL);
                if (callAction.Call is not FunctionExpr callExpr)
                {
                    throw new InvalidOperationException("Azione ON MOUSE CALL non valida: attesa una chiamata a procedura.");
                }
                var callConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(callConstIndex);
                _constantsToPatch.Add((callConstIndex, callExpr.OriginalToken));
                break;
        }

        EmitByte((byte)OpCode.OP_ON_MOUSE);
    }


    /// <summary>
    /// Compila un'istruzione ON WINDOW (RESIZE/QUIT). A differenza di ON KEY, che nella
    /// P-CODE VM e' tuttora uno stub (OP_ON_KEY consuma i 3 valori e non fa nulla — vedi
    /// PCodeProcessor.cs case OpCode.OP_ON_KEY), ON WINDOW passa per un vero SysCall
    /// (ON_WINDOW_EVENT) con un handler funzionante: vedi PCodeProcessor per il meccanismo
    /// di registrazione/dispatch.
    ///
    /// GOTO/GOSUB/CALL condividono TUTTI lo stesso meccanismo runtime: spawn di un task
    /// INDIPENDENTE all'indirizzo target (stesso schema di LaunchGoTask/GO SUB) quando
    /// l'evento finestra si verifica — non un vero interrupt del task che era in esecuzione
    /// al momento dell'evento SDL (arriva da un thread diverso, in modo asincrono; la
    /// P-CODE VM non ha un meccanismo simile nemmeno per ON KEY). Per GOSUB questo e'
    /// comunque sicuro: PCodeProcessor.OP_RETURN gia' gestisce il caso "nessun frame da cui
    /// tornare" (task.Frame == null, task spawnato direttamente sull'indirizzo del target
    /// invece che tramite OP_GOSUB) terminando semplicemente il task invece di lanciare
    /// un'eccezione — vedi il branch finale di OP_RETURN. GOTO/GOSUB risolvono il target
    /// tramite _labels, CALL tramite _procedureAddresses: PatchAllConstants prova gia'
    /// entrambi in sequenza per lo stesso identico meccanismo di patch (vedi ON KEY sopra),
    /// quindi qui basta passare il token giusto.
    /// </summary>
    /// <summary>
    /// ON SPRITE COLLISION <handler> / ON SPRITE COLLISIONS <handler>: registra l'handler
    /// per nome nel sistema eventi (ADD_HANDLER "SPRITE_COLLISION(S)"). La VM dispaccia
    /// l'evento nel SYNC quando il motore rileva collisioni.
    /// </summary>
    private void VisitOnSpriteCollisionStatement(OnSpriteCollisionStmt stmt)
    {
        EmitConstant(stmt.IsPlural ? "SPRITE_COLLISIONS" : "SPRITE_COLLISION");
        EmitConstant(stmt.HandlerName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ADD_HANDLER); EmitByte((byte)2);
    }

    private void VisitOnWindowStatementC(OnWindowStmt stmt)
    {
        // 1. Handle della finestra (clausola OF) — -1 = qualunque finestra. È il PRIMO valore
        // pushato, quindi l'ULTIMO poppato dal runtime (ordine: handle, eventKind, actionType,
        // target).
        if (stmt.Of is null) EmitConstant(-1.0);
        else VisitExpression(stmt.Of);   // il runtime converte (PopInt) — nessun opcode Conv nel p-code

        // 2. Tipo di evento finestra: WindowEventKind (Ast/Graphics.cs).
        EmitConstant((double)(int)stmt.EventKind);

        // 3. Azione e target.
        switch (stmt.Action)
        {
            case StopAction:
                EmitConstant((double)ON_KEY_ACTION_STOP);
                EmitConstant(0.0); // Target non usato per STOP.
                break;

            case CallAction callAction:
                if (callAction.Call is not FunctionExpr callExpr)
                    throw new InvalidOperationException("Azione ON WINDOW CALL non valida: attesa una chiamata a procedura.");
                EmitConstant((double)ON_KEY_ACTION_CALL);
                var callConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(callConstIndex);
                _constantsToPatch.Add((callConstIndex, callExpr.OriginalToken));
                break;

            case GotoAction gotoAction:
                EmitConstant((double)ON_KEY_ACTION_GOTO);
                var gotoConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gotoConstIndex);
                _constantsToPatch.Add((gotoConstIndex, gotoAction.TargetLabel));
                break;

            case GosubAction gosubAction:
                EmitConstant((double)ON_KEY_ACTION_GOSUB);
                var gosubConstIndex = AddConstant(null);
                EmitBytes((byte)OpCode.OP_PUSH_CONST);
                EmitShort(gosubConstIndex);
                _constantsToPatch.Add((gosubConstIndex, gosubAction.TargetLabel));
                break;

            default:
                throw new NotSupportedException($"P-CODE VM: azione ON WINDOW '{stmt.Action.GetType().Name}' non supportata.");
        }

        // 4. Emette il SysCall (stack: handle, eventKind, actionType, target — 4 valori).
        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(SysCall.ON_WINDOW_EVENT);
        EmitByte((byte)4);
    }
}