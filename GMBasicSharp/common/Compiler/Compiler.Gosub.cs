// FILE: Interpreter.Compiler.Compiler.Gosub.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione GOSUB.
    /// </summary>
    private void VisitGosubStatement(GosubStmt stmt)
    {
        // BUG-6 residuo: una procedura con GOSUB rende ogni RETURN/EXIT SUB ambiguo a
        // compile-time (può tornare da una subroutine GOSUB attiva invece di terminare la
        // procedura) — la finalizzazione dei locali del body viene skippata in quel caso.
        _currentProcHasGosub = true;
        var instructionIp = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_GOSUB);
        var patchOffset = _chunk.Code.Count;
        EmitShort(0); // Placeholder per l'indirizzo assoluto
        _explicitControlTransfers.Add(new(instructionIp, patchOffset, nameof(OpCode.OP_GOSUB)));

        if (stmt.Target.Kind == TokenKind.Number)
        {
            var lineNumber = (int)(double)(stmt.Target.Literal ?? 0);
            _lineNumberJumpsToPatch.Add((patchOffset, lineNumber));
        }
        else
        {
            _jumpsToPatch.Add((patchOffset, stmt.Target));
        }
    }
    
    /// <summary>
    /// Compila un'istruzione RETURN.
    /// </summary>
    private void VisitReturnStatement(ReturnStmt stmt)
    {
        if (stmt.Value is not null)
        {
            // RETURN da una FUNCTION: compila il valore di ritorno.
            VisitExpression(stmt.Value);
            /* Il valore va COERCIZTO al tipo dichiarato di ritorno (parità IL): una
             * FUNCTION ... AS DOUBLE che fa RETURN di una data deve rendere il seriale
             * OADate, non un oggetto DateTime (FINN_OA di finance.inc: PCode lanciava
             * Invalid cast from DateTime to Double nell'array double[] del chiamante). */
            EmitCoerceForTarget(_currentFunctionReturnType, stmt.Value);
            // Apply STRING * N padding for function return type
            if (_currentFunctionReturnType is StringType { FixedLength: int rlen } && rlen > 0)
            {
                EmitByte((byte)OpCode.OP_ADJUST_STRING);
                EmitShort((ushort)rlen);
            }
        }
        else
        {
            // RETURN da un GOSUB o SUB: pusha NIL come valore di ritorno di default.
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }
        // BUG-6 residuo (2026-09-03): un RETURN/EXIT SUB esplicito a metà corpo termina la
        // procedura quando NON c'è un GOSUB che possa consumarlo come ritorno di subroutine
        // (_currentProcHasGosub false) — finalizza i locali del body PRIMA di uscire, come fa
        // il fallthrough in VisitEndProcedureStatement. Skip se la FUNCTION restituisce un tipo
        // oggetto (stessa guardia del fallthrough: il pattern factory "Crea() AS Risorsa"
        // assegna al return-slot lo stesso riferimento di un local del body). Con GOSUB il
        // RETURN è ambiguo (può tornare dalla subroutine senza terminare la procedura) → skip.
        if (_bodyLocalsBoundary >= 0 && !_currentProcHasGosub
            && _currentFunctionReturnType is not (ClassType or UserDefinedType))
        {
            EmitFinalizeBodyLocals();
        }
        EmitByte((byte)OpCode.OP_RETURN);
        _isReachable = false; // Il codice dopo un RETURN è irraggiungibile.
    }
    
    /// <summary>
    /// Compila la fine di una procedura (END SUB/FUNCTION).
    /// </summary>
    private void VisitEndProcedureStatement(EndProcedureStmt stmt)
    {
        if (_isCompilingConstructor)
        {
            // Niente da emettere qui: VisitConstructorMember emette gia' il proprio
            // OP_RETURN subito dopo (vedi _isCompilingConstructor).
            _isReachable = false;
            return;
        }

        // BUG-6: finalizza (DESTRUCTOR) le istanze create con NEW dentro il BODY di questa
        // SUB/FUNCTION standalone, quando il flusso raggiunge naturalmente END SUB/FUNCTION
        // (fallthrough). _bodyLocalsBoundary è -1 (quindi skip) per metodi di classe/lambda,
        // che non passano da CompileProcedures — vedi _bodyLocalsBoundary. PRIMA di caricare
        // l'eventuale valore di ritorno: i local finalizzati qui sono per indice SEMPRE
        // successivi al return-slot (che vive a scopeDepth 0, fuori dal range finalizzato).
        //
        // Skip totale se la FUNCTION restituisce un tipo oggetto (ClassType/UserDefinedType):
        // il pattern "FUNCTION Crea() AS Risorsa: DIM r = NEW Risorsa(): Crea = r: END FUNCTION"
        // assegna al return-slot lo STESSO riferimento di un local del body — senza analisi
        // degli alias non possiamo distinguere "questo local è quello restituito" da "questo
        // local è davvero morto qui", e finalizzare per errore l'oggetto restituito (DESTRUCTOR
        // chiamato PRIMA che il chiamante lo usi) è peggio di non finalizzare affatto. I local
        // di una SUB (nessun return-slot oggetto) restano coperti dal fix.
        if (_bodyLocalsBoundary >= 0 && _currentFunctionReturnType is not (ClassType or UserDefinedType))
        {
            EmitFinalizeBodyLocals();
        }

        if (_currentFunctionReturnType != null)
        {
            // Il return-slot (local col nome della FUNCTION stessa, riservato da
            // CompileProcedureBody(ProcedureDefinition)/CompileProcedures) contiene già il
            // valore assegnato da un'eventuale `NomeFunzione = espr` bare (o resta al suo
            // default se la funzione ha sempre usato RETURN esplicito e non è mai arrivata
            // qui — vedi il controllo _isReachable in VisitStatement, che salta del tutto
            // questo metodo se l'ultima istruzione era già un RETURN). NON basta lasciare
            // che la VM lo recuperi da task.Frame.ReturnValue: il fallback in OP_RETURN
            // (PCodeProcessor.cs) usa un controllo ASSOLUTO "task.Sp > 0" (quasi sempre vero,
            // vedi i commenti lì: il valore parte già da 1 alla primissima istruzione del
            // programma) invece che relativo a SavedStackPointer — per una chiamata INDIRETTA
            // (CALL_INDIRECT, cioè ogni LOCAL FUNCTION/lambda invocata tramite closure) questo
            // fa sì che OP_RETURN sottragga alla VALUE STACK condivisa un residuo estraneo
            // (osservato: l'ARGOMENTO stesso della chiamata) invece del vero valore di ritorno
            // — bug: `FUNCTION Quadrato(N): Quadrato = N * N: END FUNCTION`, chiamata come
            // LOCAL FUNCTION, restituiva N invece di N*N sul motore PCode-managed (le chiamate
            // DIRETTE, OP_CALL_N/OP_CALL, non pushano un fnPtr aggiuntivo e non manifestavano
            // il sintomo). Pusha esplicitamente il valore del return-slot prima di OP_RETURN,
            // esattamente come fa un RETURN esplicito (VisitReturnStatement, canale già
            // corretto/verificato): diventa l'ULTIMA cosa sullo stack, quindi il pop in
            // OP_RETURN prende sempre e solo questo valore, indipendentemente da cosa resti
            // più in basso nello stack condiviso.
            var retSlot = ResolveLocal(_currentProcedureName ?? "");
            if (retSlot != -1)
            {
                EmitByte((byte)OpCode.OP_LOAD_LOCAL);
                EmitShort((ushort)retSlot);
            }
            else
            {
                EmitByte((byte)OpCode.OP_PUSH_NIL);
            }
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }
        EmitByte((byte)OpCode.OP_RETURN);
        _isReachable = false;
    }

}
