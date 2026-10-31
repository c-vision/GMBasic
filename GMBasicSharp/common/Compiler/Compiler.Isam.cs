// FILE: Interpreter.Compiler.Compiler.Isam.cs

// ReSharper disable UseUtf8StringLiteral



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per tutte le istruzioni che interagiscono
/// con il motore di database ISAM.
/// </summary>
public sealed partial class Compiler
{
    // --- Comandi di Gestione e Apertura ---

/// <summary>
    /// Compila un'istruzione ISAM USE.
    /// </summary>
    private void VisitIsamUseStatement(IsamUseStmt stmt)
    {
        // Spinge sulla stack tutti gli argomenti per l'operazione di USE
        VisitExpression(stmt.FileName); // 1. Percorso del file (stringa)
        EmitConstant(stmt.OriginalTypeName.Lexeme); // 2. Tipo definito dall'utente (stringa)
        EmitConstant((double)(byte)stmt.Locking); // 3. Locking mode (byte)
        EmitConstant(stmt.IsUnsafe); // 4. IsUnsafe (boolean)

        // 5. IndexedByClause (Expr KeyExpression, bool IsUnsafe, bool IsDescending)
        //
        // BUG STORICO (trovato e risolto): il conteggio chiavi veniva pushato PRIMA delle chiavi
        // stesse — incompatibile con un pop LIFO, dato che la VM (PCodeVm/PCodeProcessor.cs,
        // case SysCall.ISAM_USE) non puo' sapere quante chiavi popolare finche' non ha gia'
        // pop-ato il conteggio. Il pop di "idxCount" leggeva quindi il valore SBAGLIATO (il flag
        // IsDescending della prima chiave, un booleano interpretato come numero), disallineando
        // a cascata TUTTI i pop successivi (isUnsafe, locking, typeName, fileName) e lasciando
        // 3 valori orfani sullo stack a corrompere lo statement successivo — causa reale del
        // file spurio "False" quando si apriva un secondo database ISAM nello stesso programma
        // (vedi TODO-PCODE-BUGS.md Bug 19.4). Fix: stesso pattern GIA' corretto e verificato
        // usato per FieldCount/Fields subito sotto — le chiavi vengono pushate PRIMA (in ordine
        // di dichiarazione INVERSO, cosi' il pop LIFO le restituisce nell'ordine originale) e il
        // conteggio per ULTIMO.
        if (stmt.IndexedBy is not null)
        {
            for (int ki = stmt.IndexedBy.KeyFields.Length - 1; ki >= 0; ki--)
            {
                var keyField = stmt.IndexedBy.KeyFields[ki];
                // Un identificatore nudo (es. "ID_Ordine" in INDEXED BY o ISAM.OPEN(..., {"ID_Ordine"}))
                // e' il NOME del campo chiave, non una variabile da leggere: VisitExpression su un
                // VariableExpr emette un LoadVariable, che per un nome mai dichiarato risolve a un
                // default vuoto/nullo — ISAM_USE (PCodeVm) poi ricade sul suo fallback hardcoded "ID"
                // quando riceve una stringa vuota, corrompendo silenziosamente il nome della chiave
                // primaria/secondaria ogni volta che il campo NON si chiama letteralmente "ID" (bug
                // scoperto testando DbIsamFullTests.bas: ISAM.OPEN(...,{"ID_Ordine","DataOrdine"})
                // creava una chiave primaria chiamata "ID" invece di "ID_Ordine", causando violazioni
                // di unicita' spurie su ogni APPEND successivo al primo). Stesso pattern gia' corretto
                // per ISAM CREATE INDEX in VisitIsamCreateIndexStatement. Le espressioni composte
                // (es. UPPER(Nome)) restano gestite dal path generico VisitExpression.
                if (keyField.KeyExpression is VariableExpr keyVar)
                    EmitConstant(keyVar.NameCleaned.Lexeme);
                else
                    VisitExpression(keyField.KeyExpression); // Emette il codice per l'espressione di chiave
                EmitConstant(keyField.IsUnsafe); // Emette il flag IsUnsafe per ogni chiave
                EmitConstant(keyField.IsDescending); // Emette il flag IsDescending per ogni chiave
            }
            EmitConstant((double)stmt.IndexedBy.KeyFields.Length); // Numero di chiavi, pushato per ULTIMO
        }
        else
        {
            EmitConstant(0.0); // Nessun IndexedBy (NIL come double 0 per indicare 0 chiavi)
        }

        // 6. Alias
        if (stmt.OriginalAliasToken is not null) EmitConstant(stmt.OriginalAliasToken.Lexeme);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        // 7. FieldCount + FieldNames pushed AFTER Alias: Alias on top, then FieldNames, then FieldCount
        // Pop order: FieldNames(strings via PopRef), then FieldCount(double via PopNum)
        var fieldCount = 0;
        if (_ctx != null)
        {
            var typeName = stmt.OriginalTypeName.Lexeme;
            var udt = _ctx.GetAllUserDefinedTypes().FirstOrDefault(u =>
                u.Name.Lexeme.Equals(typeName, StringComparison.OrdinalIgnoreCase));
            if (udt != null)
            {
                fieldCount = udt.Fields.Length;
                foreach (var f in udt.Fields)
                {
                    EmitConstant(f.Name.Lexeme);
                    var len = (f.DataType as StringType)?.FixedLength ?? 0;
                    EmitConstant((double)len);
                    EmitConstant((double)f.SymbolId);
                    // Push data type name for correct ISAM schema. Mappava SOLO
                    // String/Long/Integer/Double: qualunque altro campo (CURRENCY, BIT, DATE,
                    // SINGLE, QUAD, ecc.) cadeva nel default "STRING" — lo schema ISAM salvava
                    // il campo come testo, quindi ISAM.GET restituiva una STRINGA al posto del
                    // valore numerico/data reale (es. "Saldo AS CURRENCY" letto dopo un
                    // ISAM.GET dava "-120" stringa: "Saldo + 100" faceva concatenazione invece
                    // di somma, "-120100" invece di -20 — scoperto verificando la sezione
                    // Transazioni di DbIsamFullTests.bas). Stessa lista di codici già usata dal
                    // decoder VM gemello in PCodeProcessor.Syscalls.cs (case ISAM_USE).
                    var typeCode = f.DataType switch
                    {
                        StringType => "STRING",
                        LongType => "LONG",
                        IntegerType => "INTEGER",
                        DoubleType => "DOUBLE",
                        CurrencyType => "CURRENCY",
                        BitType => "BIT",
                        ByteType => "BYTE",
                        SByteType => "SBYTE",
                        SingleType => "SINGLE",
                        ULongType => "ULONG",
                        UIntegerType => "UINTEGER",
                        QuadType => "QUAD",
                        UQuadType => "UQUAD",
                        DateType => "DATE",
                        TimeType => "TIME",
                        DateTimeType => "DATETIME",
                        TimeSpanType => "TIMESPAN",
                        JsonType => "JSON",
                        _ => "STRING"
                    };
                    EmitConstant(typeCode);
                }
                EmitConstant((double)fieldCount);
            }
        }
        if (fieldCount == 0)
            EmitConstant(0.0);
        
        var totalArgs = 4 // FileName, TypeName, Locking, IsUnsafe
            + (stmt.IndexedBy is not null ? 1 + stmt.IndexedBy.KeyFields.Length * 3 : 1); // IndexedBy

        // L'opcode è OP_SYSCALL con il sottocomando ISAM_USE
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_USE); EmitByte((byte)(totalArgs)); 
        EmitByte((byte)OpCode.OP_POP); // discard area number
    }

    /// <summary>
    /// Push file number/alias as string (for ISAM/DBF commands).
    /// The linker resolves the alias to a VariableExpr; we push it as a constant string.
    /// </summary>
    private void EmitFileNumber(Expr fileNum)
    {
        if (fileNum is VariableExpr ve)
            EmitConstant(ve.NameCleaned.Lexeme);
        else
            VisitExpression(fileNum);
    }

    /// <summary>
    /// Compila un'istruzione ISAM SELECT (work area).
    /// </summary>
    private void VisitIsamSelectWorkAreaStatement(IsamSelectWorkAreaStmt stmt)
    {
        EmitFileNumber(stmt.WorkArea);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SELECT); EmitByte((byte)1);
    }
    
    // --- Comandi di Navigazione ---

    /// <summary>
    /// Compila un'istruzione ISAM GO TOP/BOTTOM.
    /// </summary>
    private void VisitIsamRecordStatement(IsamRecordStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitConstant((double)stmt.Location.Kind); // TOP/BOTTOM come valore numerico
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_RECORD); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM SKIP.
    /// </summary>
    private void VisitIsamSkipStatement(IsamSkipStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        if (stmt.RecordCount is not null) VisitExpression(stmt.RecordCount);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SKIP); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM SEEK.
    /// </summary>
    private void VisitIsamSeekStatement(IsamSeekStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        VisitExpression(stmt.PositionExpr);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SEEK); EmitByte((byte)2);
    }

    // --- Comandi di Stato e Manutenzione ---

    /// <summary>
    /// Compila un'istruzione ISAM DELETE.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// </summary>
    private void VisitIsamDeleteStatement(IsamDeleteStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        
        EmitConstant(stmt.Scope is not null); // Arg 2: Flag hasScope (boolean)
        
        if (stmt.Scope is DbfNextClause nextClause) VisitExpression(nextClause.Count); // Arg 3: Count per NEXT (Expr)
        else if (stmt.Scope is DbfRecordClause recordClause) VisitExpression(recordClause.RecordNumber); // Arg 3: n per RECORD
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg 3: NIL se non c'è count
        
        // Arg 4: Riferimento al blocco di codice della condizione o NIL.
        if (stmt.Scope is DbfForClause forClause) 
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(forClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is DbfWhileClause whileClause)
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(whileClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is DbfAllClause)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se ALL
        }
        else if (stmt.Scope is DbfRestClause)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se REST
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessun scope (singolo record)
        }
        else
        {
            throw new NotImplementedException($"Compiler support for ISAM DELETE scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }

        // 4 argomenti: FileNumber, hasScope, scopeCount, conditionCodeRef
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_DELETE); EmitByte((byte)4); 
    }

    /// <summary>
    /// Compila un'istruzione ISAM RECALL.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// </summary>
    private void VisitIsamRecallStatement(IsamRecallStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        
        EmitConstant(stmt.Scope is not null); // Arg 2: Flag hasScope (boolean)
        
        if (stmt.Scope is DbfNextClause nextClause) VisitExpression(nextClause.Count); // Arg 3: Count per NEXT (Expr)
        else if (stmt.Scope is DbfRecordClause recordClause) VisitExpression(recordClause.RecordNumber); // Arg 3: n per RECORD
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg 3: NIL se non c'è count
        
        // Arg 4: Riferimento al blocco di codice della condizione o NIL.
        if (stmt.Scope is DbfForClause forClause) 
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(forClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is DbfWhileClause whileClause)
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(whileClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is DbfAllClause)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se ALL
        }
        else if (stmt.Scope is DbfRestClause)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se REST
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessun scope (singolo record)
        }
        else
        {
            throw new NotImplementedException($"Compiler support for ISAM RECALL scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_RECALL); EmitByte((byte)4); 
    }

    /// <summary>
    /// Compila un'istruzione ISAM PACK.
    /// </summary>
    private void VisitIsamPackStatement(IsamPackStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_PACK); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione ISAM REINDEX.
    /// </summary>
    private void VisitIsamReindexStatement(IsamReindexStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_REINDEX); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione ISAM REPAIR.
    /// </summary>
    private void VisitIsamRepairStatement(IsamRepairStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_REPAIR); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione ISAM BACKUP.
    /// </summary>
    private void VisitIsamBackupStatement(IsamBackupStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        VisitExpression(stmt.DestinationFile);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_BACKUP); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM APPEND.
    /// </summary>
    private void VisitIsamAppendStatement(IsamAppendStmt stmt)
    {
        // Push alias as string constant (not variable expression)
        // The FileNumber expression is the alias name resolved by the linker
        if (stmt.FileNumber is VariableExpr ve)
            EmitConstant(ve.NameCleaned.Lexeme);
        else
            EmitFileNumber(stmt.FileNumber);
        VisitExpression(stmt.Source);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_APPEND); EmitByte((byte)2);
    }

    /// <summary>
    /// ISAM PUT (alias [, udtVar]) — modifica il record corrente. Pattern PCodeVM ISAM_PUT:
    /// arg 1 = alias (stringa, o area attiva), arg 2 = sorgente (record UDT). Prima il parser
    /// produceva un PutFileStmt (I/O file) per `ISAM PUT`, che compilava PUT_FILE (handle
    /// invalido); il nodo dedicato IsamPutStmt compila la syscall corretta.
    /// </summary>
    private void VisitIsamPutStatement(IsamPutStmt stmt)
    {
        if (stmt.Alias is VariableExpr ve)
            EmitConstant(ve.NameCleaned.Lexeme);
        else
            EmitFileNumber(stmt.Alias);
        VisitExpression(stmt.Source ?? new NilExpr(new Token(TokenKind.Nil, "NIL", null, stmt.Token.Position)));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_PUT); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM LOCK.
    /// </summary>
    private void VisitIsamLockStatement(IsamLockStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitConstant(stmt.IsRecordLock);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_LOCK); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione ISAM UNLOCK.
    /// </summary>
    private void VisitIsamUnlockStatement(IsamUnlockStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitConstant(stmt.IsRecordLock);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_UNLOCK); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione ISAM SET ORDER.
    /// </summary>
    private void VisitIsamSetOrderStatement(IsamSetOrderStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        VisitExpression(stmt.IndexName); // IndexName è un Expr
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SET_ORDER); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM SET RELATION.
    /// Compila l'espressione chiave in un blocco P-Code.
    /// </summary>
    private void VisitIsamSetRelationStatement(IsamSetRelationStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber del padre

        // Arg 2: nome del campo chiave come STRINGA (non un code-ref) — IsamDriver.SetRelation/
        // SyncRelation (Databases/Rdd/IsamDriver.cs) usa la keyExpression per un lookup diretto
        // del campo via GetFieldIndex sul driver padre, non per una valutazione dinamica per
        // record; un OP_PUSH_CODE_REF qui produceva un numero (l'indice del blocco compilato)
        // che l'ISAM_SET_RELATION handler non poteva mai risolvere a un nome di campo.
        if (stmt.KeyExpression is not null)
            EmitAliasOrExpression(stmt.KeyExpression);
        else
            EmitByte((byte)OpCode.OP_PUSH_NIL);

        // Arg 3: Alias del figlio (stringa o NIL).
        if (stmt.ChildAlias is not null) EmitConstant(stmt.ChildAlias.Lexeme);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SET_RELATION); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione ISAM CREATE INDEX.
    /// L'espressione della chiave viene compilata in un blocco P-Code.
    /// </summary>
    private void VisitIsamCreateIndexStatement(IsamCreateIndexStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        VisitExpression(stmt.IndexName);  // Arg 2: IndexName (Expr)

        // Arg 3: KeyExpression testuale — Engine.CreateIndexAsync vuole una STRINGA (il nome
        // campo o un'espressione tipo "CONVERSIONS.CSTR(Attivo)"), valutata poi per record da un
        // evaluator esterno durante l'inserimento/rebuild. NON e' un blocco da eseguire ora:
        // CompileExpressionToCodeBlock (usato qui prima) compila codice P-Code raggiungibile solo
        // tramite CallCodeRefSync, che qui non viene mai invocato — il syscall riceveva quindi il
        // puntatore/indirizzo grezzo (un numero tipo "1306") al posto del testo della chiave,
        // scoperto testando DbIsamFullTests.bas (indice creato con nome numerico invece del nome
        // reale, DROP INDEX falliva sempre silenziosamente perché non trovava mai quel nome).
        // Sintassi legacy KEY (Saldo): il parser produce un riferimento bare al campo (VariableExpr),
        // che va preso per NOME testuale, non valutato come variabile. Sintassi qualificata: la
        // chiave arriva già come stringa letterale, VisitExpression basta.
        if (stmt.KeyExpression is VariableExpr keyVar)
            EmitConstant(keyVar.NameCleaned.Lexeme);
        else
            VisitExpression(stmt.KeyExpression);

        VisitExpression(stmt.KeyLength); // Arg 4: KeyLength (Expr)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_CREATE_INDEX); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione ISAM DROP INDEX.
    /// </summary>
    private void VisitIsamDropIndexStatement(IsamDropIndexStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        VisitExpression(stmt.IndexName); // IndexName è un Expr
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_DROP_INDEX); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione ISAM VALIDATE.
    /// </summary>
    private void VisitIsamValidateStatement(IsamValidateStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_VALIDATE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione ISAM COPY TO.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// NON passa il flag IsUnsafe, che verrà gestito a runtime dall'Executor.
    /// </summary>
    private void VisitIsamCopyToStatement(IsamCopyToStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        VisitExpression(stmt.DestinationFile); // Arg 2: DestinationFile
        
        EmitConstant(stmt.Scope is not null); // Arg 3: Flag hasScope (boolean)
        if (stmt.Scope is { } forClause) 
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(forClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg 4: NIL se nessun scope
        }
        else
        {
            throw new NotImplementedException($"Compiler support for ISAM COPY TO scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }
        // Rimosso: EmitConstant(stmt.IsUnsafe); // QUESTO NON VA PIÙ QUI

        // Il numero di argomenti corretto è 4 (FileNumber, DestinationFile, hasScope, conditionCodeRef)
        // L'Executor determinerà l'unsafe-ness dalla work area a runtime.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_COPY_TO); EmitByte((byte)4); 
    }

    /// <summary>
    /// Compila un'istruzione ISAM JOIN WITH.
    /// </summary>
    private void VisitIsamJoinWithStatement(IsamJoinWithStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        EmitConstant(stmt.SecondaryAliasToken.Lexeme);
        VisitExpression(stmt.DestinationFile);

        // Condizione serializzata a STRINGA (ExprPlain.From): il marshaller VM/C ne estrae
        // i campi di join sinistro/destro (Engine_JoinWith lavora su nomi campo). Un
        // code-ref non sarebbe decompilabile lato C.
        EmitConstant(ExprPlain.From(stmt.Condition.Condition));

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_JOIN_WITH); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione ISAM SORT TO.
    /// Ora compila la condizione del filtro e le espressioni di ordinamento in blocchi P-Code.
    /// </summary>
    private void VisitIsamSortToStatement(IsamSortToStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        VisitExpression(stmt.DestinationFile); // Arg 2: DestinationFile
        
        // Arg 3: Numero di campi di ordinamento
        EmitConstant((double)stmt.SortFields.Length); 
        // Arg 4...: Campi di ordinamento ([KeyExpressionCodeRef, Descending, IsUnsafe])
        foreach (var field in stmt.SortFields)
        {
            var keyExpression = field.KeyExpression;
            var descending = false;

            // Controlla se l'espressione è un'operazione di divisione per il flag /D (discendente).
            if (keyExpression is BinaryExpr { Op.Kind: TokenKind.Divide } be)
            {
                if (be.Right is VariableExpr ve && ve.NameCleaned.Lexeme.Equals("D", StringComparison.OrdinalIgnoreCase))
                {
                    keyExpression = be.Left;
                    descending = true;
                }
            }
            
            var keyExprCodeRef = CompileExpressionToCodeBlock(keyExpression);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
            EmitShort(keyExprCodeRef);
            
            EmitConstant(descending);
            EmitConstant(field.IsUnsafe);
        }

        EmitConstant(stmt.Scope is not null); // Arg N+1: Flag hasScope
        if (stmt.Scope is { } forClause) 
        {
            var conditionCodeRef = CompileExpressionToCodeBlock(forClause.Condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF); 
            EmitShort(conditionCodeRef);
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg N+1: NIL se nessun scope
        }
        else
        {
            throw new NotImplementedException($"Compiler support for ISAM SORT TO scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }
        
        // Calcolo corretto di argCount:
        // 2 (file, dest) + 1 (num_fields) + (num_fields * 3) + 1 (has_scope) + 1 (scope_cond_ref)
        var totalArgs = 5 + stmt.SortFields.Length * 3;
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SORT_TO); EmitByte((byte)(totalArgs)); 
    }

    /// <summary>
    /// Compila un'istruzione ISAM SET FILTER.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// </summary>
    private void VisitIsamSetFilterStatement(IsamSetFilterStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber); // Arg 1: FileNumber
        
        EmitConstant(stmt.Condition is not null); // Arg 2: Flag hasCondition
        if (stmt.Condition is not null)
        {
            if (stmt.Condition is MacroExpr macroFilter)
            {
                // Espansione macro UNA volta a setup (come DBF/SPEED SET FILTER e Clipper): il VALORE dell'interno
                // (es. `f` = "Price > 10 AND Qty > 0") E' il testo del filtro. MAI VisitExpression(MacroExpr): emette
                // OP_EVAL_MACRO_EXPR che ESEGUE il testo come codice (nessuna stringa al motore -> filtro mai
                // installato e cursore invalido: hang del WHILE NOT ISAM.EOF).
                VisitExpression(macroFilter.Expression);
            }
            else if (stmt.Condition is LambdaExpr)
            {
                // Filtro lambda (FN(rec) => ...): si emette il TESTO del corpo (come l'IL,
                // ExprPlain.FilterText) — il motore lo ri-parsa per record. Emettere la
                // ClosureData obbligava il VM-C a invocarla per record (callback non
                // supportato dall'engine C) → filtro silenziosamente ignorato: era l'UNICA
                // divergenza fra backend sul filtro lambda. Testo = sorgente unica per
                // PCode/VM-C/C99, identica all'IL.
                EmitConstant(Basic.ExprPlain.FilterText(stmt.Condition));
            }
            else
            {
                // Filtro condizionale runtime (espressione semplice, non lambda/macro): il
                // TESTO della condizione, come fa l'IL (ExprPlain.FilterText). Prima emetteva
                // un CODE_REF (offset) che NESSUN handler (PCode/VM-C/C99) sapeva valutare →
                // `ISAM SET FILTER TO Citta = "Roma" AND Saldo > 100` era silenziosamente
                // ignorato (tutti i record letti). La VM ri-parsa il testo per record.
                EmitConstant(Basic.ExprPlain.FilterText(stmt.Condition));
            }
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg 3: NIL se nessun filtro
        }
        
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_SET_FILTER); EmitByte((byte)3);
    }
    
    /// <summary>
    /// Compila un'istruzione BEGIN TRANSACTION.
    /// </summary>
    private void VisitBeginTransactionStatement(BeginTransactionStmt stmt)
    {
        // Spinge il livello di isolamento come valore numerico sulla stack.
        EmitConstant((double)(byte)stmt.Level); 
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.BEGIN_TRANSACTION); EmitByte((byte)1); // 1 argomento
    }

    /// <summary>
    /// Compila un'istruzione COMMIT.
    /// </summary>
    private void VisitCommitStatement(CommitStmt stmt)
    {
        // COMMIT non ha argomenti diretti per la SysCall.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.COMMIT); EmitByte((byte)0); // 0 argomenti
    }

    /// <summary>
    /// Compila un'istruzione ROLLBACK.
    /// </summary>
    private void VisitRollbackStatement(RollbackStmt stmt)
    {
        // ROLLBACK non ha argomenti diretti per la SysCall.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ROLLBACK); EmitByte((byte)0); // 0 argomenti
    }

    /// <summary>
    /// Compila un'istruzione EXPORT TO.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// </summary>
    /// <summary>
    /// Compila un'istruzione EXPORT TO.
    /// NON supporta condizioni di filtro nel suo AST.
    /// </summary>
    private void VisitExportToFileStatement(ExportToFileStmt stmt)
    {
        // Arg 1: EngineType (es. ISAM, DBF)
        EmitConstant((double)stmt.EngineType.Kind);
        // Arg 2: Area (Expr: #file o alias)
        VisitExpression(stmt.Area);
        // Arg 3: FilePath (Expr)
        VisitExpression(stmt.FilePath);
        // Arg 4: Format (es. CSV, JSON)
        EmitConstant((double)stmt.Format.Kind);
        
        // Rimosso: logica di compilazione per 'Condition' e 'IsUnsafe', che non esistono nell'AST.

        // ArgCount: (EngineType, Area, FilePath, Format) = 4
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.EXPORT_TO_FILE); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione IMPORT FROM.
    /// </summary>
    private void VisitImportFromFileStatement(ImportFromFileStmt stmt)
    {
        EmitConstant((double)stmt.EngineType.Kind); // Arg 1: EngineType (es. ISAM, DBF)
        VisitExpression(stmt.Area); // Arg 2: Area (Expr: #file o alias)
        VisitExpression(stmt.FilePath); // Arg 3: FilePath (Expr)
        EmitConstant((double)stmt.Format.Kind); // Arg 4: Format (es. CSV, JSON)
        
        // Arg 5: IsUnsafe (boolean) - Questo campo NON ESISTE NELL'AST FORNITO, va rimosso.
        // Lo stato unsafe viene gestito dalla workarea a runtime.
        // Quindi il parametro verrà rimosso e ArgCount diminuito.

        // ArgCount: (EngineType, Area, FilePath, Format) = 4
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IMPORT_FROM_FILE); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione CONVERT DBF TO ISAM.
    /// </summary>
    private void VisitConvertDbfToIsamStatement(ConvertDbfToIsamStmt stmt)
    {
        VisitExpression(stmt.SourceFile); // Arg 1: SourceFile (Expr)
        VisitExpression(stmt.DestinationFile); // Arg 2: DestinationFile (Expr)
        EmitConstant(stmt.OriginalTypeName.Lexeme); // Arg 3: TypeName (string)
        EmitConstant(stmt.OriginalPkFieldName.Lexeme); // Arg 4: PrimaryKeyField (string)
        VisitExpression(stmt.KeyLength); // Arg 5: KeyLength (Expr)
        EmitConstant(stmt.IsUnsafe); // Arg 6: IsUnsafe (boolean)

        // Arg 7+: nome REALE + codice tipo REALE di ogni campo del TYPE, nell'ordine di
        // dichiarazione (stesso motivo/pattern di VisitIsamUseStatement sopra, stessa lista
        // di codici) — il motore ISAM (Engine.TranslateDbfSchemaToIsam) semplifica OGNI campo
        // DBF Numeric a DoubleType a prescindere dal vero tipo dichiarato ("Codice AS LONG"
        // scritto come DoubleType) e il DBF tronca i nomi campo a 10 caratteri
        // ("Descrizione" -> "Descrizion") — entrambi mai compatibili col vero TYPE al
        // successivo ISAM OPEN ... OF <UDT> (SchemaManager.AreSchemasCompatible,
        // "[SCHEMA] Field[i] name/type mismatch" — scoperti dal vivo su DbIsamFullTests.bas
        // sez. 11, insieme al bug gemello sul TypeName dello schema letto dal nome del file
        // invece che dal TYPE — vedi IlRuntime.IsamConvertDbf per il fix identico lato IL).
        var fieldCount = 0;
        if (_ctx != null)
        {
            var udt = _ctx.GetAllUserDefinedTypes().FirstOrDefault(u =>
                u.Name.Lexeme.Equals(stmt.OriginalTypeName.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (udt != null)
            {
                fieldCount = udt.Fields.Length;
                foreach (var f in udt.Fields)
                {
                    var typeCode = f.DataType switch
                    {
                        StringType => "STRING",
                        LongType => "LONG",
                        IntegerType => "INTEGER",
                        DoubleType => "DOUBLE",
                        CurrencyType => "CURRENCY",
                        BitType => "BIT",
                        ByteType => "BYTE",
                        SByteType => "SBYTE",
                        SingleType => "SINGLE",
                        ULongType => "ULONG",
                        UIntegerType => "UINTEGER",
                        QuadType => "QUAD",
                        UQuadType => "UQUAD",
                        DateType => "DATE",
                        TimeType => "TIME",
                        DateTimeType => "DATETIME",
                        TimeSpanType => "TIMESPAN",
                        JsonType => "JSON",
                        _ => "STRING"
                    };
                    EmitConstant(f.Name.Lexeme);
                    EmitConstant(typeCode);
                }
            }
        }
        EmitConstant((double)fieldCount); // Conteggio pushato per ULTIMO (pop LIFO)

        // ArgCount: (SourceFile, DestFile, TypeName, PKField, KeyLength, IsUnsafe, [nome,tipo]*, count)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CONVERT_DBF_TO_ISAM); EmitByte((byte)(7 + fieldCount * 2));
    }
    
}