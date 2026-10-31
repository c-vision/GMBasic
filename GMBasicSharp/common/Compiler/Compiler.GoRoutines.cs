// FILE: Interpreter.Compiler.Compiler.GoRoutines.cs



namespace Compiler;

public sealed partial class Compiler
{
    private void VisitGoRoutineStatement(GoProcedureStmt stmt)
    {
        // P-CODE VM: la goroutine viene accodata sullo scheduler cooperativo (PCodeScheduler,
        // OP_GO) — nessun thread reale, nessuna esecuzione sincrona: gira quando le tocca il
        // turno, esattamente come qualunque altro task del GPM.

        // Compila tutti gli argomenti della SUB e li mette sulla stack.
        if (stmt.Call is FunctionExpr fe)
        {
            foreach (var arg in fe.Args)
            {
                VisitExpression(arg);
            }
            
            // Compila il WaitGroup, se presente, e lo mette sulla stack.
            if (stmt.WaitGroup is not null)
            {
                VisitExpression(stmt.WaitGroup);
            }
            else
            {
                EmitByte((byte)OpCode.OP_PUSH_NIL);
            }
            
            // Emette l'istruzione GO (GOSUB-like), patchata con l'indirizzo della SUB.
            // 2026-09-24 (overload): patch con arità della definizione risolta.
            var goLocation = EmitJump((byte)OpCode.OP_GO);
            _jumpsToPatch.Add((goLocation, CallPatchToken(fe.OriginalToken, fe.ResolvedDefinition as ProcedureDefinition, fe.Args.Length)));
        }
    }
    
    private void VisitWaitStatement(WaitStmt stmt)
    {
        // Compila l'espressione del WaitGroup.
        VisitExpression(stmt.WaitGroup);
        // Emette l'istruzione di attesa.
        EmitByte((byte)OpCode.OP_WG_WAIT);
    }
    
    private void VisitDoneStatement(DoneStmt stmt)
    {
        // Compila l'espressione del WaitGroup.
        VisitExpression(stmt.WaitGroup);
        // Emette l'istruzione di decremento.
        EmitByte((byte)OpCode.OP_WG_DONE);
    }

    private void VisitSendStatement(SendStmt stmt)
    {
        // Parità Go chan direzionali: SEND su un canale ReceiveOnly ('<-chan T') è un errore
        // di compilazione. Verifica statica per variabili/parametri/globali risolvibili.
        if (ResolveChannelType(stmt.Channel) is { Direction: ChannelDirection.ReceiveOnly } chRecv)
            throw new Exception(
                $"SEND su un canale ReceiveOnly ('<-chan T'): non si può inviare su '{stmt.Channel}'.");
        // Compila il valore da inviare.
        VisitExpression(stmt.Value);
        // Compila l'espressione del canale.
        VisitExpression(stmt.Channel);
        // Emette l'istruzione di invio.
        EmitByte((byte)OpCode.OP_SEND);
    }

    private void VisitReceiveStatement(ReceiveStmt stmt)
    {
        // Parità Go chan direzionali: RECEIVE su un canale SendOnly ('chan<- T') è un errore.
        if (ResolveChannelType(stmt.Channel) is { Direction: ChannelDirection.SendOnly } chSend)
            throw new Exception(
                $"RECEIVE su un canale SendOnly ('chan<- T'): non si può ricevere da '{stmt.Channel}'.");
        // Compila l'espressione del canale.
        VisitExpression(stmt.Channel);

        if (stmt.OkFlag is not null)
        {
            // RECEIVE v FROM ch OK ok: OP_RECV_OK pusha (valore, flag).
            EmitByte((byte)OpCode.OP_RECV_OK);
            // Flag ok in cima, valore sotto: assegniamo ok poi v.
            AssignVariable(stmt.OkFlag, isLet: false);
            AssignVariable(stmt.Destination, isLet: false);
        }
        else
        {
            // RECEIVE v FROM ch: comportamento attuale.
            EmitByte((byte)OpCode.OP_RECV);
            AssignVariable(stmt.Destination, isLet: false);
        }
    }

    private void VisitCloseChannelStatement(CloseChannelStmt stmt)
    {
        // Compila l'espressione del canale.
        VisitExpression(stmt.Channel);
        // Emette l'istruzione di chiusura canale.
        EmitByte((byte)OpCode.OP_CHANNEL_CLOSE);
    }
    
    /// <summary>
    /// Compila un'istruzione GOEXIT.
    /// </summary>
    private void VisitGoExitStatement(GoExitStmt stmt)
    {
        // GOEXIT termina la goroutine corrente, ma deve attraversare FINALLY/cleanup.
        EmitByte((byte)OpCode.OP_GO_EXIT);
    }

    /// <summary>
    /// Compila un'istruzione GO SELECT.
    /// Emette GO_SELECT_SETUP con gli indirizzi di ogni CASE, cosicché la VM
    /// possa saltare direttamente al corpo del CASE selezionato.
    /// Formato bytecode:
    ///   [OP_GO_SELECT_SETUP] [num_cases (byte)] [num_sends (byte)] [default_offset (ushort)]
    ///   [case0_addr (ushort)] [case1_addr (ushort)] ... [caseN-1_addr (ushort)]
    ///   [case 0 body] [JUMP_FORWARD end]
    ///   [case 1 body] [JUMP_FORWARD end]
    ///   ...
    ///   [default body]
    ///   [end: patched end jumps]
    /// </summary>
    private void VisitGoSelectStatement(GoSelectStmt stmt)
    {
        var endJumps = new List<int>();
        var caseAddrs = new List<int>();
        var caseAddrPositions = new List<int>();
        
        // 1. Raccogli e compila: valori SEND, poi per ogni case non-SEND il suo "canale"
        //    (ChannelInfo per RECEIVE, durata numerica per TIMEOUT) nell'ordine dei case.
        var sendValuesExpressions = new List<Expr>();
        var selectSlots = new List<Expr>(); // canale (SEND/RECEIVE) o durata (TIMEOUT) per OGNI case, in ordine
        foreach (var caseClause in stmt.Cases)
        {
            if (caseClause.CommunicationStmt is SendStmt send)
            {
                sendValuesExpressions.Add(send.Value);
                selectSlots.Add(send.Channel); // il canale del case SEND DEVE essere pushato (runtime: baseSp+numSends+j)
            }
            else if (caseClause.CommunicationStmt is ReceiveStmt receive)
            {
                selectSlots.Add(receive.Channel);
            }
            else if (caseClause.CommunicationStmt is TimeoutStmt timeout)
            {
                selectSlots.Add(timeout.Duration);
            }
        }
        // Stack finale: [send_val_0..S-1] [slot_0..numCases-1] dove ogni slot è il canale
        // (case SEND/RECEIVE) o la durata numerica (TIMEOUT) del case i-esimo, NELL'ORDINE
        // dei case. Il runtime accede a baseSp + numSends + j (slot del case j) e a
        // baseSp + j (valore SEND j): senza i canali dei case SEND il baseSp risultava
        // negativo e un GO SELECT con case SEND parcheggiava sempre (deadlock).
        foreach (var expr in sendValuesExpressions) VisitExpression(expr);
        foreach (var expr in selectSlots) VisitExpression(expr);
        
        // 2. Emetti GO_SELECT_SETUP con placeholders
        // Formato: [OP_GO_SELECT_SETUP] [num_cases u16] [num_sends u16] [default_offset u16]
        //          [case_addr u16] * num_cases  (limite u8 -> u16: 65535 case, parità piano §20.3)
        EmitByte((byte)OpCode.OP_GO_SELECT_SETUP);
        EmitShort((ushort)stmt.Cases.Length);
        EmitShort((ushort)sendValuesExpressions.Count);
        int defaultOffPos = _chunk.Code.Count;
        EmitShort(0); // placeholder default offset
        for (int i = 0; i < stmt.Cases.Length; i++)
        {
            caseAddrPositions.Add(_chunk.Code.Count);
            EmitShort(0); // placeholder case addr
        }
        
        // 3. Compila i corpi dei CASE (registrando gli indirizzi)
        for (var i = 0; i < stmt.Cases.Length; i++)
        {
            caseAddrs.Add(_chunk.Code.Count);
            
            BeginScope();
            var caseClause = stmt.Cases[i];
            
            if (caseClause.CommunicationStmt is ReceiveStmt receiveStmt)
            {
                AssignVariable(receiveStmt.Destination, isLet: false);
            }
            
            foreach (var caseStmt in caseClause.Body)
            {
                VisitStatement(caseStmt);
            }
            EndScope();
            
            endJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
        }
        
        // 4. Patcha default offset
        if (stmt.DefaultCase.HasValue && stmt.DefaultCase.Value.Any())
        {
            int defaultAddr = _chunk.Code.Count;
            _chunk.Code[defaultOffPos] = (byte)(defaultAddr >> 8);
            _chunk.Code[defaultOffPos + 1] = (byte)defaultAddr;
            
            BeginScope();
            foreach (var defaultStmt in stmt.DefaultCase.Value)
            {
                VisitStatement(defaultStmt);
            }
            EndScope();
        }
        else
        {
            // Sentinel 0xFFFF = "nessun DEFAULT nel sorgente". Prima si usava endAddr (sempre
            // un indirizzo bytecode valido, quindi sempre > 0), rendendo il check runtime
            // "defaultOffset > 0" sempre vero — un GO SELECT senza DEFAULT non bloccava mai,
            // eseguiva sempre il ramo "default" verso endAddr invece di parcheggiarsi. Vedi
            // PCodeVm/PCodeProcessor.cs, handler OP_GO_SELECT_SETUP.
            _chunk.Code[defaultOffPos] = 0xFF;
            _chunk.Code[defaultOffPos + 1] = 0xFF;
        }
        
        // 5. Patcha case addresses
        for (int i = 0; i < caseAddrPositions.Count; i++)
        {
            _chunk.Code[caseAddrPositions[i]] = (byte)(caseAddrs[i] >> 8);
            _chunk.Code[caseAddrPositions[i] + 1] = (byte)caseAddrs[i];
        }
        
        // 6. Patcha end jumps
        PatchJumps(endJumps);
    }

    /// <summary>Risolve il tipo CHANNEL di un'espressione canale per la verifica statica della
    /// direzione (parità Go chan direzionali). Copre variabili locali/parametri e globali;
    /// se il tipo non è risolvibile qui ritorna null (nessuna verifica — il runtime è
    /// indifferente alla direzione).</summary>
    private ChannelType? ResolveChannelType(Expr channelExpr)
    {
        if (channelExpr is not VariableExpr ve)
            return null;
        var name = ve.NameCleaned.Lexeme;
        var li = ResolveLocal(name);
        if (li != -1 && li < _locals.Count)
            return _locals[li].Type as ChannelType;
        if (_globals.TryGetValue(name, out var gi) && gi < _globalInfos.Count)
            return _globalInfos[gi].Type as ChannelType;
        return null;
    }

}
