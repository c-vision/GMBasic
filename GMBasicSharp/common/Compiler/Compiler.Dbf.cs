// FILE: Interpreter.Compiler.Compiler.Dbf.cs

// ReSharper disable UseUtf8StringLiteral



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per tutte le istruzioni che interagiscono
/// con il motore di database DBF.
/// </summary>
public sealed partial class Compiler
{
    // Nota: È necessario definire questo enum, preferibilmente in un file condiviso.
    public enum DbfScopeType
    {
        For,
        While,
        Next,
        All,
        Record,
        Rest
    }
    
    // --- Comandi di Gestione e Apertura ---

    /// <summary>
    /// Compila un'istruzione DBF USE.
    /// </summary>
    private void VisitDbfListStatement(DbfListStmt stmt)
    {
        // Campi (CSV di nomi nudi), destinazione (file o ""), flag stampa.
        var fields = stmt.Fields is null ? "" : string.Join(",", stmt.Fields.FieldExpressions.Select(DbfBareFieldName));
        EmitConstant(fields);
        EmitConstant(stmt.Destination is null ? "" : DbfExprString(stmt.Destination));
        EmitConstant(stmt.ToPrint);
        // FOR/WHILE (anche nella forma `FN(rec) => ...`): testo della condizione. Prima non
        // veniva emesso e LIST stampava TUTTI i record.
        Expr? listCond = stmt.Scope switch
        {
            DbfForClause lf => lf.Condition,
            DbfWhileClause lw => lw.Condition,
            _ => null
        };
        if (listCond is LambdaExpr lx && lx.ExpressionBody is not null) listCond = lx.ExpressionBody;
        EmitConstant(listCond is null ? "" : ExprPlain.From(listCond));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_LIST); EmitByte((byte)4);
    }

    private void VisitDbfCreateViewStatement(DbfCreateViewStmt stmt)
    {
        // Vista = snapshot del file DBF corrente, registrata per nome.
        EmitConstant(stmt.ViewNameToken.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_CREATE_VIEW); EmitByte((byte)1);
    }

    private void VisitDbfAggregateStatement(DbfAggregateStmt stmt)
    {
        // Chiave (nome campo nudo), file destinazione (senza virgolette), aggregazioni
        // "SUM(Amt) AS Totale,..." estratte dall'ArrayLiteralExpr di StringExpr.
        EmitConstant(DbfBareFieldName(stmt.KeyExpression));
        EmitConstant(DbfExprString(stmt.FileName));
        var aggs = stmt.AggregationsExpr is ArrayLiteralExpr ar
            ? string.Join(",", ar.Elements.OfType<StringExpr>().Select(s => s.Value))
            : (stmt.AggregationsExpr is null ? "" : ExprPlain.From(stmt.AggregationsExpr));
        EmitConstant(aggs);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_AGGREGATE); EmitByte((byte)3);
    }

    private void VisitDbfReportStatement(DbfReportStmt stmt)
    {
        // Form file, destinazione (file o ""), flag stampa.
        EmitConstant(DbfExprString(stmt.FormFile));
        EmitConstant(stmt.Destination is null ? "" : DbfExprString(stmt.Destination));
        EmitConstant(stmt.ToPrint);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_REPORT); EmitByte((byte)3);
    }

    private void VisitDbfUseStatement(DbfUseStmt stmt)
    {
        VisitExpression(stmt.FileName); // Percorso del file (stringa)
        EmitConstant((double)(byte)stmt.Locking); // Locking mode (byte)
        EmitConstant(stmt.IsUnsafe); // IsUnsafe (boolean)
        
        // IndexPaths: push paths first, then count (so count is on top for handler to pop first)
        foreach (var indexPath in stmt.IndexPaths)
        {
            VisitExpression(indexPath);
        }
        EmitConstant((double)stmt.IndexPaths.Length);

        // Alias
        if (stmt.OriginalAliasToken is not null) EmitConstant(stmt.OriginalAliasToken.Lexeme);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        if (stmt.TypeSymbolId > 0)
        {
            // Passa l'ID e la stringa (per la diagnostica)
            EmitConstant((double)stmt.TypeSymbolId);
            EmitConstant(stmt.OriginalTypeName!.Lexeme);
        }
        else
        {
            // Se non c'è AS TypeName, passa NIL per ID e NIL per nome.
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Nome campo + SymbolId per ogni campo dell'UDT: la VM/C ora riceve (nome, lunghezza,
        // isString, typeCode) — lo STESSO formato di DBF CREATE TABLE — così può CREARE il
        // file quando `USE ... AS Tipo` lo trova assente (semantica documentata, parità
        // IL/C99). Prima pushava (nome, symbolId): il marshaller VM li SCARTAVA (teneva solo
        // path/iarg) e il file mancante restava un open fallito silenzioso (g_last_dbf=0 →
        // DBF.SELECTED()=0, LASTREC=0, GET vuoto: tutta la gamba VM/C del sample Clipper
        // divergente). I symbolId non erano usati da nessun consumatore.
        var fieldCount = 0;
        if (stmt.TypeSymbolId > 0 && stmt.OriginalTypeName is not null)
            fieldCount = EmitUdtFieldMetadata(stmt.OriginalTypeName.Lexeme, includeSymbolId: true);
        EmitConstant((double)fieldCount);

        // 9 argomenti totali (FileName, Locking, IsUnsafe, IndexPaths (count + paths), Alias,
        // TypeSymbolId, OriginalTypeName, FieldCount + tuple Nome/Len/IsString/TypeCode)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_USE); EmitByte((byte)((8 + stmt.IndexPaths.Length + fieldCount * 5)));
    }

    /// <summary>
    /// Compila un'istruzione DBF CREATE.
    /// </summary>
    private void VisitDbfCreateStatement(DbfCreateStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitConstant(stmt.TypeNameToken.Lexeme);

        // Metadati campi dal TYPE (nome, lunghezza, isString, typeCode) — vedi
        // EmitUdtFieldMetadata (condiviso con DBF USE ... AS Tipo).
        var fieldCount = EmitUdtFieldMetadata(stmt.TypeNameToken.Lexeme);
        EmitConstant((double)fieldCount);

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_CREATE_TABLE); EmitByte((byte)((2 + fieldCount * 4 + 1)));
    }

    /// <summary>
    /// Emette i metadati dei campi di un TYPE come tuple (nome, lunghezza, isString,
    /// typeCode) e ritorna il numero di campi. Formato CONDIVISO da `DBF CREATE TABLE` e
    /// `DBF USE ... AS Tipo`: la VM/C li usa sia per creare lo schema sia per CREARE il file
    /// quando `USE ... AS Tipo` lo trova assente (create-if-missing, parità IL/C99).
    /// Il typeCode è quello già decodificato lato VM (senza, ogni campo non-stringa diventava
    /// Numeric con decimals=0: CURRENCY 12.34 scritto come "12").
    /// </summary>
    private int EmitUdtFieldMetadata(string typeName, bool includeSymbolId = false)
    {
        if (_ctx == null) return 0;
        var udt = _ctx.GetAllUserDefinedTypes().FirstOrDefault(u =>
            u.Name.Lexeme.Equals(typeName, StringComparison.OrdinalIgnoreCase));
        if (udt == null) return 0;
        foreach (var f in udt.Fields)
        {
            EmitConstant(f.Name.Lexeme);
            // USE ... AS Tipo: la PCodeVM usa il symbolId per il wrapper __class__/FieldNames
            // che OP_LOAD_MEMBER richiede per rec.Campo (DBF GET <var>). CREATE non lo usa.
            if (includeSymbolId) EmitConstant((double)f.SymbolId);
            var len = (f.DataType as StringType)?.FixedLength ?? 0;
            var isString = f.DataType is StringType;
            EmitConstant((double)len);
            EmitConstant(isString);
            EmitConstant(TypeCodeOf(f.DataType));
        }
        return udt.Fields.Length;
    }

    private static string TypeCodeOf(object dataType) => dataType switch
    {
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

    /// <summary>
    /// Compila un'istruzione DBF SELECT (work area).
    /// L'argomento può essere un numero o una stringa (alias), quindi viene visitato come espressione.
    /// </summary>
    private void VisitDbfSelectWorkAreaStatement(DbfSelectWorkAreaStmt stmt)
    {
        VisitExpression(stmt.WorkArea);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SELECT); EmitByte((byte)1);
    }
    
    /// <summary>
    /// Compila un'istruzione DBF CLOSE DATABASES.
    /// </summary>
    private void VisitDbfCloseDatabasesStatement(DbfCloseStmt stmt)
    {
        // DBF CLOSE DATABASES (WorkAreas vuoto) = chiudi TUTTO; DBF CLOSE <alias>[, ...] = chiudi
        // SOLO l'area corrente (il parser risolve le aree; CLOSE_CURRENT chiude g_last_dbf).
        // Prima QUALSIASI DBF CLOSE emetteva CLOSE_DATABASES → `DBF CLOSE Temp` chiudeva anche
        // l'area principale e il comando successivo sull'area corrente falliva (VM-C/PCode).
        if (stmt.WorkAreas.IsEmpty)
        {
            // CLOSE DATABASES/ALL → tutte; CLOSE nudo → solo la corrente.
            EmitByte((byte)OpCode.OP_SYSCALL);
            EmitSysCall(stmt.All ? SysCall.DBF_CLOSE_DATABASES : SysCall.DBF_CLOSE_CURRENT);
            EmitByte((byte)0);
        }
        else
        {
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_CLOSE_CURRENT); EmitByte((byte)0);
        }
    }

    // --- Comandi di Navigazione ---

    /// <summary>
    /// Compila un'istruzione DBF GO (TOP, BOTTOM, o numero di record).
    /// </summary>
    private void VisitDbfGoStatement(DbfGoStmt stmt)
    {
        // Visita l'espressione di destinazione. Questo spingerà sullo stack
        // il valore del numero di record, o il valore di una variabile che
        // contiene "TOP" o "BOTTOM".
        VisitExpression(stmt.Destination);
        
        // Emette la SysCall. L'Executor ora riceverà un valore (numerico o stringa)
        // dallo stack e dovrà interpretarlo per decidere se andare a TOP, BOTTOM
        // o a un numero di record specifico.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_GO); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SKIP.
    /// </summary>
    private void VisitDbfSkipStatement(DbfSkipStmt stmt)
    {
        if (stmt.RecordCount is not null) VisitExpression(stmt.RecordCount);
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Default SKIP 1
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SKIP); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SEEK.
    /// </summary>
    private void VisitDbfSeekStatement(DbfSeekStmt stmt)
    {
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SEEK); EmitByte((byte)1);
    }

    // --- Comandi di Ricerca e Modifica ---

    /// <summary>
    /// Compila un'istruzione GET per file DBF.
    /// Gestisce sia `GET variabile` (che spinge un valore sullo stack per l'assegnazione)
    /// sia `GET` (che popola il buffer di record interno a runtime).
    /// </summary>
    private void VisitDbfGetRecordStatement(DbfGetRecordStmt stmt)
    {
        if (stmt.Destination != null)
        {
            // --- Caso 1: GET <variabile> ---
            // Emette una syscall che legge il record e lo spinge sullo stack della VM.
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_GET_RECORD); EmitByte((byte)0);
            // Emette il codice per assegnare il valore in cima allo stack alla variabile di destinazione.
            AssignVariable(stmt.Destination, isLet: false);
        }
        else
        {
            // --- Caso 2: GET (senza argomenti, stile Clipper) ---
            // Emette una syscall diversa che legge il record e lo memorizza
            // in un buffer interno del runtime, senza modificare lo stack della VM.
            //EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_GET_BUFFER); EmitByte((byte)0);
        }
    }

    /// <summary>
    /// Compila un'istruzione DBF REPLACE.
    /// Ora che `REPLACE` usa `Assignment`, il Target viene compilato in modo da generare
    /// un riferimento (es. a un campo late-bound) che la VM userà per l'assegnazione.
    /// </summary>
    private void VisitDbfReplaceStatement(DbfReplaceStmt stmt)
    {
        // La VM (case SysCall.DBF_REPLACE) popa: scope (per primo, cima — pattern DBF_DELETE/
        // PopScopeClause), poi count, poi le coppie (value, fieldName). Lo scope è pushato
        // PER ULTIMO così resta in cima (le syscall consumano solo dalla cima dello stack).
        // Ordine stack (dal basso): [fname1, val1, ..., count, scope...] con scope in cima.

        foreach (var assignment in stmt.Replacements)
        {
            // FieldTarget (campo risolto via CurrentUdtScopeId, es. `DBF USE ... AS Type`):
            // usare .ToString() emetteva la rappresentazione debug del record
            // ("_dbRecord#-2.Codice#3000") invece del nome campo — la VM (SysCall.DBF_REPLACE)
            // fa un match esatto per nome, quindi il REPLACE diventava un no-op silenzioso.
            if (assignment.Target is VariableTarget vt)
                EmitConstant(vt.NameCleaned.Lexeme);
            else if (assignment.Target is FieldTarget ft)
                EmitConstant(ft.OriginalToken.Lexeme);
            else if (assignment.Target is MacroTarget mt && mt.Expression is MacroExpr mExpr)
                // MacroTarget (&cCampo): il NOME campo è il valore a runtime dell'espressione
                // interna della macro (es. cCampo = "CampoA" → campo "CampoA"). NON passare da
                // OP_EVAL_MACRO (ricompilerebbe "CampoA" come codice → NIL): si valuta l'interno.
                VisitExpression(mExpr.Expression);
            else
                EmitConstant(assignment.Target?.ToString() ?? "");
            var _prevDbfValMode = _dbfReplaceValueMode;
            _dbfReplaceValueMode = true;
            try { VisitExpression(assignment.Value); }
            finally { _dbfReplaceValueMode = _prevDbfValMode; }
        }
        EmitConstant((double)stmt.Replacements.Length);

        var scopeArgCount = CompileScopeClause(stmt.Scope);

        // Argomenti totali per la syscall: scope + count + N coppie (nome, valore)
        var totalArgs = (byte)(scopeArgCount + 1 + (stmt.Replacements.Length * 2));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_REPLACE); EmitByte((byte)(totalArgs));
    }

    /// <summary>
    /// Compila un'istruzione DBF APPEND BLANK.
    /// </summary>
    private void VisitDbfAppendBlankStatement(DbfAppendBlankStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_APPEND_BLANK); EmitByte((byte)0);
    }

    /// <summary>
    /// Compila un'istruzione DBF APPEND RECORD.
    /// </summary>
    private void VisitDbfAppendRecordStatement(DbfAppendRecordStmt stmt)
    {
        VisitExpression(stmt.Source);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_APPEND_RECORD); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF APPEND FROM.
    /// </summary>
    private void VisitDbfAppendFromStatement(DbfAppendFromStmt stmt)
    {
        VisitExpression(stmt.FileName); // Nome del file sorgente
        EmitConstant(stmt.Scope is not null); // Flag hasScope
        
        if (stmt.Scope is { } forClauseApp)
        {
            // Text (ExprPlain.From): il motore C valuta il FOR per-record dentro
            // AppendFrom (un code-ref non è eseguibile dal motore).
            EmitConstant(ExprPlain.From(forClauseApp.Condition));
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }
        else
        {
            throw new NotImplementedException($"Compiler support for DBF APPEND FROM scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_APPEND_FROM); EmitByte((byte)3); // file, hasScope, condition_ref
    }

    /// <summary>
    /// Compila un'istruzione DBF DELETE.
    /// </summary>
    private void VisitDbfDeleteStatement(DbfDeleteStmt stmt)
    {
        if (EmitDbfScopedLoop(stmt.Scope, SysCall.DBF_DELETE)) return;
        var scopeArgCount = CompileScopeClause(stmt.Scope);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_DELETE); EmitByte((byte)(scopeArgCount));
    }

    private bool EmitDbfScopedLoop(DbfScopeClause? scope, SysCall op)
    {
        Expr? cond = null;
        var isWhile = false;
        if (scope is DbfForClause f) cond = f.Condition;
        else if (scope is DbfWhileClause w) { cond = w.Condition; isWhile = true; }
        if (cond is null) return false;
        // FOR FN(rec) => <espressione>: il parametro è solo un marker, il corpo usa i
        // campi nudi dell'area corrente → si compila direttamente l'espressione.
        if (cond is LambdaExpr lx)
        {
            if (lx.ExpressionBody is null) return false;
            cond = lx.ExpressionBody;
        }

        if (!isWhile)
        {
            EmitConstant("TOP");
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_GO); EmitByte((byte)1);
            EmitByte((byte)OpCode.OP_POP);
        }
        var loopStart = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_CALL_BUILTIN); EmitShort((ushort)BuiltInFunction.EOFFUNC); EmitByte((byte)0);
        var jExit = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        {
            // In una clausola FOR/WHILE di un comando DBF ogni nome nudo è un CAMPO
            // dell'area corrente: il Linker non riscrive il nodo bare-field qui
            // (ID resta VariableExpr#-1) → lo si emette name-based come i campi nudi.
            var prevScope = _dbfScopeCondMode;
            _dbfScopeCondMode = true;
            try { VisitExpression(cond); }
            finally { _dbfScopeCondMode = prevScope; }
        }
        var jCondFalse = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(op); EmitByte((byte)0);
        EmitByte((byte)OpCode.OP_POP);
        if (!isWhile) PatchJump(jCondFalse);
        EmitConstant(1.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SKIP); EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP);
        EmitLoop(loopStart);
        PatchJump(jExit);
        if (isWhile) PatchJump(jCondFalse);
        return true;
    }

    /// <summary>
    /// Compila un'istruzione DBF RECALL.
    /// </summary>
    private void VisitDbfRecallStatement(DbfRecallStmt stmt)
    {
        if (EmitDbfScopedLoop(stmt.Scope, SysCall.DBF_RECALL)) return;
        var scopeArgCount = CompileScopeClause(stmt.Scope);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_RECALL); EmitByte((byte)(scopeArgCount));
    }

    /// <summary>
    /// Compila un'istruzione DBF PACK.
    /// </summary>
    private void VisitDbfPackStatement(DbfPackStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_PACK); EmitByte((byte)0);
    }

    /// <summary>
    /// Compila un'istruzione DBF ZAP.
    /// </summary>
    private void VisitDbfZapStatement(DbfZapStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_ZAP); EmitByte((byte)0);
    }

    /// <summary>
    /// Compila un'istruzione DBF REINDEX.
    /// </summary>
    private void VisitDbfReindexStatement(DbfReindexStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_REINDEX); EmitByte((byte)0);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET ORDER.
    /// L'argomento può essere un nome di indice (stringa) o un numero.
    /// </summary>
    private void VisitDbfSetOrderStatement(DbfSetOrderStmt stmt)
    {
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_ORDER); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET RELATION.
    /// </summary>
    private void VisitDbfSetRelationStatement(DbfSetRelationStmt stmt)
    {
        if (stmt.KeyExpression is not null)
        {
            var keyName = ExprPlain.RelationKeyName(stmt.KeyExpression);
            if (keyName != null) EmitConstant(keyName);
            else VisitExpression(stmt.KeyExpression);
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        // L'alias del figlio è un letterale, non una variabile.
        if (stmt.ChildAlias is not null) EmitConstant(stmt.ChildAlias.Lexeme);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_RELATION); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET DELETED ON/OFF.
    /// </summary>
    private void VisitDbfSetDeletedStatement(DbfSetDeletedStmt stmt)
    {
        EmitConstant(stmt.State.Kind == TokenKind.On);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_DELETED); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET EXACT ON/OFF.
    /// </summary>
    private void VisitDbfSetExactStatement(DbfSetExactStmt stmt)
    {
        EmitConstant(stmt.State.Kind == TokenKind.On);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_EXACT); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET FILTER.
    /// </summary>
    private void VisitDbfSetFilterStatement(DbfSetFilterStmt stmt)
    {
        EmitConstant(stmt.Condition is not null); // Flag hasCondition
        if (stmt.Condition is MacroExpr macroFilter)
        {
            // Espansione macro UNA volta a setup (parità Clipper/ISAM): il VALORE
            // dell'interno è il testo del filtro (es. &cCond con cCond="A > 1").
            // MAI OP_EVAL_MACRO qui (eseguirebbe il testo come codice invece di
            // installarlo — dava sempre 0 record).
            VisitExpression(macroFilter.Expression);
        }
        else if (stmt.Condition is not null)
        {
            // Condizione serializzata a STRINGA (ExprPlain.From): valutata per-record da
            // RuntimeExpressionEvaluator contro il dict del record (come filtro ISAM stringa e
            // backend IL). Un code-block non funziona — i nomi campo nudi vengono compilati
            // come variabili globali, non come letture dal record corrente.
            EmitConstant(ExprPlain.From(stmt.Condition));
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessun filtro
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_FILTER); EmitByte((byte)2); // 2 argomenti
    }

    /// <summary>
    /// Compila un'istruzione DBF INDEX ON.
    /// </summary>
    private void VisitDbfIndexOnStatement(DbfIndexOnStmt stmt)
    {
        // KeyExpression as code block
        var keyExprCodeRef = CompileExpressionToCodeBlock(stmt.KeyExpression);
        EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
        EmitShort(keyExprCodeRef);
        
        // Key expression as string (for SYSCALL handler to create index)
        EmitConstant(stmt.KeyExpression.ToString());
        
        // FileName (indice)
        VisitExpression(stmt.FileName);
        
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_INDEX_ON); EmitByte((byte)3); // 3 argomenti
    }

    /// <summary>
    /// Compila un'istruzione DBF COPY TO.
    /// </summary>
    private void VisitDbfCopyToStatement(DbfCopyToStmt stmt)
    {
        VisitExpression(stmt.FileName); // File di destinazione
        EmitConstant(stmt.Scope is not null); // Flag hasScope
        
        if (stmt.Scope is { } forClause) 
        {
            // Text (ExprPlain.From): il dispatcher VM/C lo applica come filtro temporaneo
            // attorno al COPY (un code-ref non è eseguibile dal motore).
            EmitConstant(ExprPlain.From(forClause.Condition));
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessun scope
        }
        else
        {
            throw new NotImplementedException($"Compiler support for DBF COPY TO scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }
        
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_COPY_TO); EmitByte((byte)3); // 3 argomenti
    }

    /// <summary>
    /// Compila un'istruzione DBF JOIN WITH.
    /// </summary>
    private void VisitDbfJoinWithStatement(DbfJoinWithStmt stmt)
    {
        EmitConstant(stmt.OriginalAliasToken.Lexeme); // Alias della tabella secondaria
        VisitExpression(stmt.FileName); // File di destinazione
        // `FOR FN(cli, mov) => Clienti.ID = Movimenti.ID`: il testo del filtro è il CORPO
        // della lambda (i parametri sono solo marker). Senza unwrap arrivava il ToString
        // dell'AST ("(Lambda(...) => ...)") e il JOIN produceva 0 righe.
        var jCondExpr = stmt.Condition.Condition is LambdaExpr jl && jl.ExpressionBody is not null
            ? jl.ExpressionBody : stmt.Condition.Condition;
        EmitConstant(ExprPlain.FromQualified(jCondExpr));
        
        // Fields (se presenti)
        if (stmt.Fields is not null)
        {
            EmitConstant((double)stmt.Fields.FieldExpressions.Length);
            foreach (var fieldExpr in stmt.Fields.FieldExpressions)
            {
                // Le espressioni dei campi devono essere compilate come blocchi di codice
                // per gestire i FieldAccessExpr a runtime.
                var fieldCodeRef = CompileExpressionToCodeBlock(fieldExpr);
                EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
                EmitShort(fieldCodeRef);
            }
        }
        else
        {
            EmitConstant(0.0); // Nessun campo specifico
        }
        
        // ArgCount: alias(1), fileName(1), conditionRef(1), fields_count(1), field_refs(N)
        var totalArgs = 4 + (stmt.Fields?.FieldExpressions.Length ?? 0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_JOIN_WITH); EmitByte((byte)(totalArgs));
    }

    /// <summary>
    /// Compila un'istruzione DBF SORT TO.
    /// </summary>
    private void VisitDbfSortStatement(DbfSortStmt stmt)
    {
        VisitExpression(stmt.FileName); // File di destinazione
        
        // Campi di ordinamento
        EmitConstant((double)stmt.SortFields.Length);
        foreach (var field in stmt.SortFields)
        {
            EmitConstant(field.OriginalToken.Lexeme);
            EmitConstant(field.Descending);
        }

        EmitConstant(stmt.Scope is not null); // Flag hasScope
        if (stmt.Scope is { } forClauseSort) 
        {
            // Text (ExprPlain.From): il motore C applica il FOR come filtro temporaneo
            // durante SORT (un code-ref non è eseguibile dal motore).
            EmitConstant(ExprPlain.From(forClauseSort.Condition));
        }
        else if (stmt.Scope is null)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessun scope
        }
        else
        {
            throw new NotImplementedException($"Compiler support for DBF SORT TO scope type '{stmt.Scope.GetType().Name}' is not yet implemented.");
        }
        
        // ArgCount: fileName, num_fields, [field, desc]..., has_scope, scope_cond_ref
        // = 1 + 1 + 2*fields + 1 + 1 = 4 + 2*fields. Prima era 3 + 2*fields: la VM popava
        // UN ARGOMENTO IN MENO, lasciava `fileName` sullo stack e leggeva i pop SHIFTATI
        // (count = desc dell'ultimo campo, dest = primo campo) → SORT su un file/dest
        // sbagliato e stack imbalance per le istruzioni successive.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SORT); EmitByte((byte)((4 + stmt.SortFields.Length * 2))); 
    }

    /// <summary>
    /// Compila un'istruzione DBF UPDATE ON.
    /// </summary>
    private void VisitDbfUpdateStatement(DbfUpdateStmt stmt)
    {
        // Chiave come NOME CAMPO (non valore): il runtime di UPDATE ON deve sapere quale
        // campo confrontare tra destinazione e sorgente (`ON ID`).
        EmitConstant(DbfBareFieldName(stmt.KeyExpression));
        EmitConstant(stmt.OriginalAliasToken.Lexeme); // Alias della tabella sorgente

        // Replacements: (Campo, Valore) — il VALORE è serializzato a STRINGA
        // (ExprPlain.From): il motore C lo valuta per-record con l'evaluator di
        // espressioni (gmb_expr_eval_filter_ex). Un code-ref non sarebbe valutabile lato C.
        EmitConstant((double)stmt.Replacements.Length);
        foreach (var repl in stmt.Replacements)
        {
            EmitConstant(repl.NameCleaned.Lexeme);
            EmitConstant(ExprPlain.FromQualified(repl.Value));
        }

        // ArgCount: keyExpr, alias, num_replacements, [field, value]...
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_UPDATE); EmitByte((byte)((3 + stmt.Replacements.Length * 2)));
    }

    /// <summary>
    /// Compila un'istruzione DBF TOTAL ON.
    /// </summary>
        private void VisitDbfTotalOnStatement(DbfTotalOnStmt stmt)
    {
        // Come DBF SET FILTER, chiave e campi sono serializzati a STRINGA (nome campo nudo
        // per il motore C gmb_db_dbf_total_on; il C# p-code li scarta comunque). Un code-ref
        // non è estraibile dal marshaller VM/C.
        EmitConstant(DbfBareFieldName(stmt.KeyExpression));

        // File di destinazione
        VisitExpression(stmt.FileName);

        // Campi (se presenti): CSV di nomi campo nudi separati da virgola.
        if (stmt.Fields is not null)
        {
            EmitConstant(string.Join(",", stmt.Fields.FieldExpressions.Select(DbfBareFieldName)));
        }
        else
        {
            EmitConstant(""); // Nessun campo specifico (il motore scrive solo la chiave)
        }

        // Scope (se presente) — il motore C valuta il FOR per-record dentro TOTAL ON.
        EmitConstant(stmt.Scope is not null); // Flag hasScope
        if (stmt.Scope is { } forClause)
        {
            EmitConstant(ExprPlain.From(forClause.Condition));
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // NIL se nessuno scope
        }

        // ArgCount: keyField_str(1), fileName(1), sumFields_csv(1), has_scope(1), scope_cond_ref(1)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_TOTAL_ON); EmitByte((byte)5);
    }

    /// <summary>Stringa PIANA da un'espressione (letterale stringa senza virgolette).</summary>
    private static string DbfExprString(Expr e) => e switch
    {
        StringExpr se => se.Value,
        _ => (ExprPlain.From(e) ?? "").Trim('"'),
    };

    /// <summary>Nome campo nudo da un'espressione chiave/somma di TOTAL ON/UPDATE:
    /// FieldAccessExpr → nome campo; VariableExpr → nome variabile; altrimenti testo plain.</summary>
    private static string DbfBareFieldName(Expr e) => e switch
    {
        FieldAccessExpr fa => fa.Field.Lexeme,
        VariableExpr ve => ve.NameCleaned.Lexeme,
        _ => ExprPlain.From(e),
    };

    /// <summary>
    /// Compila un'istruzione DBF COPY STRUCTURE TO.
    /// </summary>
    private void VisitDbfCopyStructureStatement(DbfCopyStructureStmt stmt)
    {
        VisitExpression(stmt.FileName); // File di destinazione
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_COPY_STRUCTURE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET DEFAULT.
    /// </summary>
    private void VisitDbfSetDefaultStatement(DbfSetDefaultStmt stmt)
    {
        VisitExpression(stmt.Path);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_DEFAULT); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET PATH.
    /// </summary>
    private void VisitDbfSetPathStatement(DbfSetPathStmt stmt)
    {
        VisitExpression(stmt.PathList);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_PATH); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET EXCLUSIVE.
    /// </summary>
    private void VisitDbfSetExclusiveStatement(DbfSetExclusiveStmt stmt)
    {
        EmitConstant(stmt.State.Kind == TokenKind.On);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_EXCLUSIVE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF SET SCHEMA STRICT.
    /// </summary>
    private void VisitDbfSetSchemaStatement(DbfSetSchemaStmt stmt)
    {
        EmitConstant(stmt.State.Kind == TokenKind.On);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_SET_SCHEMA); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione DBF VALIDATE.
    /// </summary>
    private void VisitDbfValidateStatement(DbfValidateStmt stmt)
    {
        if (stmt.Area is not null) VisitExpression(stmt.Area);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DBF_VALIDATE); EmitByte((byte)1);
    }
    
        /// <summary>
    /// Compila una clausola di scope (FOR/WHILE/NEXT/ALL) per i comandi DBF.
    /// Restituisce il numero di argomenti pushati sulla stack.
    /// </summary>
    private byte CompileScopeClause(DbfScopeClause? scope)
    {
        if (scope == null)
        {
            EmitConstant(false); // hasScope
            EmitByte((byte)OpCode.OP_PUSH_NIL); // scopeType
            EmitByte((byte)OpCode.OP_PUSH_NIL); // arg1 (es. count)
            EmitByte((byte)OpCode.OP_PUSH_NIL); // arg2 (es. condition)
            return 4;
        }

        EmitConstant(true); // hasScope
        
        switch (scope)
        {
            case DbfNextClause next:
                EmitConstant((double)DbfScopeType.Next);
                VisitExpression(next.Count);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg2 non usato
                break;
            case DbfForClause forClause:
                EmitConstant((double)DbfScopeType.For);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg1 non usato
                CompileScopeCondition(forClause.Condition);
                break;
            case DbfWhileClause whileClause:
                EmitConstant((double)DbfScopeType.While);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg1 non usato
                CompileScopeCondition(whileClause.Condition);
                break;
            case DbfRecordClause recordClause:
                EmitConstant((double)DbfScopeType.Record);
                VisitExpression(recordClause.RecordNumber);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg2 non usato
                break;
            case DbfRestClause:
                EmitConstant((double)DbfScopeType.Rest);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg1 non usato
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg2 non usato
                break;
            case DbfAllClause:
                EmitConstant((double)DbfScopeType.All);
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg1 non usato
                EmitByte((byte)OpCode.OP_PUSH_NIL); // arg2 non usato
                break;
            default:
                throw new NotImplementedException($"DBF scope type '{scope.GetType().Name}' not implemented.");
        }
        
        return 4;
    }

    /// <summary>Compila la CONDIZIONE di una clausola di scope FOR/WHILE (arg2).
    /// Il testo (ExprPlain.From) viene valutato PER-RECORD da RuntimeExpressionEvaluator
    /// contro il record corrente — i nomi campo nudi funzionano (stesso meccanismo di
    /// SET FILTER). Un codeblock NON funziona per i campi nudi (compilati come variabili
    /// globali → sempre false). Solo le condizioni MACRO (&c) restano codeblock legacy:
    /// il loro VALORE runtime è il testo, valutato a setup dal loop manuale.</summary>
    private void CompileScopeCondition(Expr condition)
    {
        if (condition is MacroExpr)
        {
            var conditionRef = CompileExpressionToCodeBlock(condition);
            EmitBytes((byte)OpCode.OP_PUSH_CODE_REF, (byte)(conditionRef >> 8), (byte)conditionRef);
            return;
        }
        EmitConstant(ExprPlain.From(condition) ?? "");
    }

}
