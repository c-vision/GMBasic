// FILE: Interpreter.Compiler.Emitters.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Aggiunge un singolo byte al chunk di codice, con la sua mappatura di linea.
    /// </summary>
    /// <param name="b">Il byte da emettere.</param>
    /// <param name="line">La linea sorgente opzionale (se diversa da quella corrente).</param>
    private void EmitByte(byte b, int? line = null)
    {
        _chunk.Code.Add(b);
        _chunk.Lines.Add(line ?? _currentSourceLineIndex);
    }

    /// <summary>
    /// Aggiunge una sequenza di byte al chunk di codice, con la stessa mappatura di linea.
    /// </summary>
    /// <param name="bytes">I byte da emettere.</param>
    private void EmitBytes(params byte[] bytes)
    {
        foreach (var b in bytes)
        {
            EmitByte((byte)(b));
        }
    }

    /// <summary>
    /// Emette un syscall code a 16-bit, big-endian — stesso ordine byte di EmitShort, che è
    /// quello che PCodeProcessor.ReadU16 si aspetta (hi=primo byte, lo=secondo). Prima di
    /// questo fix era little-endian: per ogni SysCall con valore V &lt; 256 (la maggioranza),
    /// ReadU16 decodificava V*256 invece di V, mancando sempre il case corretto in
    /// ExecuteSyscallDispatch e cadendo silenziosamente nel default (no-op). Bug scoperto
    /// verificando RDD.CURVAL(): restituiva sempre stringa vuota perché RDD_CURVAL non veniva
    /// mai eseguito, solo mai osservato prima perché molti SysCall producono output "plausibile"
    /// anche a vuoto (PRINT letterali incondizionati, valori di default che sembrano corretti).
    /// </summary>
    private void EmitSysCall(SysCall call)
    {
        // Formato UNIFORME 2 byte: la VM legge ReadU16 (syscall) + ReadU8 (argc).
        // La enum SysCall supera 255 (SET_CLIPPER=276, WINDOW_ACTION=334): il formato a
        // 1 byte troncava i syscall > 255 (276 -> 20) disallineando le letture.
        EmitByte((byte)(((ushort)call >> 8) & 0xFF));
        EmitByte((byte)((ushort)call & 0xFF));
    }

    /// <summary>
    /// Aggiunge un operando a 2 byte (short) al chunk di codice.
    /// </summary>
    /// <param name="value">Il valore a 16 bit da emettere.</param>
    private void EmitShort(int value)
    {
        EmitByte((byte)((value >> 8) & 0xFF));
        EmitByte((byte)(value & 0xFF));
    }

    /// <summary>
    /// Emette un'istruzione di salto (es. JUMP, JUMP_IF_FALSE) con un indirizzo segnaposto.
    /// </summary>
    /// <param name="instruction">L'opcode dell'istruzione di salto.</param>
    /// <returns>La posizione dell'indirizzo segnaposto da patchare in seguito.</returns>
    private int EmitJump(byte instruction)
    {
        EmitByte((byte)(instruction));
        EmitByte((byte)(0xFF)); // Segnaposto per l'offset del salto (byte alto)
        EmitByte((byte)(0xFF)); // Segnaposto per l'offset del salto (byte basso)
        return _chunk.Code.Count - 2;
    }

    /// <summary>
    /// Emette un'istruzione di loop che salta indietro a un indirizzo specifico.
    /// </summary>
    /// <param name="loopStart">L'indirizzo P-Code a cui tornare.</param>
    private void EmitLoop(int loopStart)
    {
        EmitByte((byte)OpCode.OP_JUMP_BACKWARD);

        // L'offset è la distanza tra la posizione ATTUALE (dopo l'operando del salto)
        // e il punto a cui vogliamo tornare.
        var offset = _chunk.Code.Count + 2 - loopStart;
        if (offset > ushort.MaxValue)
            throw new Exception("Loop body too large.");

        EmitShort(offset);
    }

    /// <summary>
    /// Aggiunge un valore alla constant pool (se non già presente)
    /// ed emette l'istruzione per pusharlo sulla stack.
    /// </summary>
    /// <param name="value">Il valore da aggiungere.</param>
    private int AddConstant(object? value)
    {
        // Se c'è un compilatore padre, aggiungi al pool del padre direttamente.
        // Così le costanti delle closure hanno gli indici corretti nel pool finale.
        if (_enclosing != null)
            return _enclosing.AddConstant(value);
        _chunk.Constants.Add(value);
        return _chunk.Constants.Count - 1;
    }

    /// <summary>
    /// Emette il codice per pushare una costante.
    /// </summary>
    /// <param name="value">Il valore da pushare.</param>
    private void EmitConstant(object value)
    {
        var constIndex = AddConstant(value);

        if (constIndex > ushort.MaxValue)
            throw new Exception("Constant pool limit (65536) exceeded.");

        EmitByte((byte)OpCode.OP_PUSH_CONST);
        EmitByte((byte)((constIndex >> 8) & 0xFF));
        EmitByte((byte)(constIndex & 0xFF));
    }
    
    /// <summary>
    /// Emette, a fine procedura, le subroutine dei predicati VALID pendenti di questa
    /// procedura (il body termina con OP_RETURN: questo codice è mai raggiungibile dal
    /// flusso — la VM ci salta con l'indirizzo registrato in Get.ValidAddress).
    /// Ogni subroutine valuta l'espressione (che referenzia la variabile del campo, già
    /// applicata) e termina con OP_END_VALID; il bool resta in cima allo stack.
    /// </summary>
    private void EmitValidPredicateSubroutines()
    {
        for (var i = 0; i < _pendingValidPredicates.Count; i++)
        {
            var (ci, procName, expr, pool) = _pendingValidPredicates[i];
            if (procName != _currentProcedureName) continue;
            var addr = _chunk.Code.Count;
            pool[ci] = (double)addr;
            VisitExpression(expr);
            EmitByte((byte)OpCode.OP_END_VALID);
        }
    }

    /// <summary>
    /// Compila un'espressione data in un blocco di P-Code separato
    /// e aggiunge l'indirizzo di inizio di questo blocco alla constant pool.
    /// </summary>
    /// <param name="expr">L'espressione AST da compilare.</param>
    /// <returns>L'indice nella constant pool che punta all'indirizzo del blocco di codice compilato.</returns>
    private int CompileExpressionToCodeBlock(Expr expr)
    {
        // Il blocco è eseguito SOLO per saltodei consumer (CallCodeBlockSync/CallClosureSync/
        // hook motori DBF/ISAM) via OP_PUSH_CODE_REF, MAI dal flusso principale: il flusso
        // principale lo SALTA con un jump incondizionato (come i body lambda). Il blocco:
        //   jump-oltre-blocco; [start] <expr lascia il valore>; OP_RETURN; [dopo]
        // Prima il blocco era inline SENZA OP_RETURN (solo OP_POP): CallCodeBlockSync, saltato
        // al suo Ip, eseguiva il blocco e poi FALLIVA nel codice successivo della procedura
        // (es. DBF CLOSE dopo un DBF DELETE FOR) → cursore null + EOF congelato → loop
        // infinito sulle operazioni scoped con condizione su file (hang documentato).
        // OP_RETURN è il termine obbligato (stesso pattern CompileStatementToCodeBlock);
        // l'OP_POP rimosso — il valore va LETTO dal chiamante (CallCodeBlockSync legge il
        // leftover sopra savedSp), non scartato.
        var currentSourceLine = _currentSourceLineIndex;
        var currentScopeDepth = _scopeDepth;
        var currentLocalsCount = _locals.Count;

        var skipJumpPos = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        var codeBlockStartAddress = _chunk.Code.Count;

        VisitExpression(expr); // Lascia il risultato sulla stack (letto dal chiamante sync).

        EmitByte((byte)OpCode.OP_RETURN); // Termina il blocco (MAI fall-through nel main).
        PatchJump(skipJumpPos);

        var codeBlockAddressConstIndex = AddConstant((double)codeBlockStartAddress);

        _currentSourceLineIndex = currentSourceLine;
        _scopeDepth = currentScopeDepth;
        if (_locals.Count > currentLocalsCount)
        {
            _locals.RemoveRange(currentLocalsCount, _locals.Count - currentLocalsCount);
        }

        return codeBlockAddressConstIndex;
    }
    
    /// <summary>
    /// Compila un'istruzione data in un blocco di P-Code separato
    /// e aggiunge l'indirizzo di inizio di questo blocco alla constant pool.
    /// Il blocco di codice compilato è inteso per essere eseguito dalla VM in un contesto specifico.
    /// </summary>
    /// <param name="stmt">L'istruzione AST da compilare.</param>
    /// <returns>L'indice nella constant pool che punta all'indirizzo del blocco di codice compilato.</returns>
    private int CompileStatementToCodeBlock(Stmt stmt)
    {
        var originalScopeDepth = _scopeDepth;
        var originalLocalsCount = _locals.Count;
        var codeBlockStartAddress = _chunk.Code.Count;

        BeginScope(); // Inizia un nuovo scope per il blocco di codice dello statement.

        VisitStatement(stmt); // Compila l'istruzione.

        EmitByte((byte)OpCode.OP_RETURN); // Un blocco di codice deve terminare con un RETURN
        EndScope(); // Chiude lo scope creato per lo statement.

        var codeBlockAddressConstIndex = AddConstant((double)codeBlockStartAddress);

        _scopeDepth = originalScopeDepth;
        if (_locals.Count > originalLocalsCount)
        {
            _locals.RemoveRange(originalLocalsCount, _locals.Count - originalLocalsCount);
        }

        return codeBlockAddressConstIndex;
    }
    
    /// <summary>
    /// Compila un riferimento a una procedura in un blocco P-Code per essere passato
    /// come parametro (es. callback).
    /// </summary>
    /// <param name="procedureNameToken">Il token del nome della procedura.</param>
    /// <returns>L'indice nella constant pool che punta all'indirizzo del blocco di codice.</returns>
    private int CompileProcedureToCodeRef(Token procedureNameToken)
    {
        // Porting da GMBasicAI (commit 7736047). Risoluzione RIMANDATA a PatchAllConstants (fine
        // compilazione), non immediata: al momento in cui questo statement compila (es. `HTTP
        // ROUTE ..., ShowToken()` dentro `SUB Main`), la procedura referenziata (`ShowToken`)
        // potrebbe non essere stata ancora compilata — CompileProcedures compila le procedure
        // nell'ordine in cui compaiono in userProcs, che non garantisce "il chiamante viene dopo
        // il chiamato" (anzi, per un programma a file singolo l'ordine è tipicamente quello di
        // dichiarazione nel sorgente, dove `Main` spesso PRECEDE le SUB che referenzia come
        // callback). Risolvere subito avrebbe richiesto affidarsi al fatto che _procedureAddresses
        // contenesse ancora un indirizzo "vecchio ma valido" lasciato da una compilazione ridondante
        // precedente della stessa procedura (bytecode duplicato) — comportamento fragile e
        // implicito, rotto non appena la duplicazione viene rimossa (bug reale: con compilazione
        // singola, il fallback "non ancora risolto" tornava silenziosamente l'indirizzo 0, che
        // casualmente ricadeva dentro il corpo di un'altra procedura già compilata — la richiesta
        // HTTP eseguiva quel corpo sbagliato invece dell'handler vero). Stesso pattern già usato da
        // VisitFetchStatement per il callback di HttpFetchStmt — vedi PatchAllConstants.
        var constIndex = AddConstant(0.0);
        _constantsToPatch.Add((constIndex, procedureNameToken));
        return constIndex;
    }

    /// <summary>
    /// Genera una label univoca per il compilatore P-Code.
    /// Queste etichette sono usate per i salti interni del bytecode (es. nei cicli, GO SELECT).
    /// </summary>
    /// <param name="prefix">Un prefisso descrittivo per l'etichetta (es. "LOOP_START").</param>
    /// <returns>Un Token fittizio che rappresenta l'etichetta generata.</returns>
    private Token GenerateCompilerLabel(string prefix)
    {
        _compilerLabelCounter++;
        var labelName = $"___{prefix}_{_compilerLabelCounter}";
        return new Token(TokenKind.Identifier, labelName, null, SourcePosition.Unknown); 
    }

    private bool IsCompilerGeneratedLabel(string name) => name.StartsWith("___");
    
}