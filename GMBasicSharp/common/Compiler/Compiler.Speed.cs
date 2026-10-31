// FILE: Interpreter.Compiler.Compiler.Speed.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per tutte le istruzioni che interagiscono
/// con il motore di database Speed.
/// </summary>
public sealed partial class Compiler
{
    // Ricorda, per ogni vista SPEED creata in questa unità di compilazione, il Target INTO
    // della sua SELECT interna — permette a SPEED REFRESH VIEW (che riferisce la vista solo
    // per nome, senza l'AST della SELECT originale) di riassegnare gli stessi risultati
    // aggiornati alla stessa variabile, con lo stesso pattern AssignVariable usato da
    // VisitSpeedSelectStatement. Copre il caso comune (CREATE e REFRESH nello stesso scope);
    // non copre viste create in uno scope diverso da quello del REFRESH.
    private readonly Dictionary<string, Target> _speedViewIntoTargets = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Se <paramref name="queryExpr"/> è una stringa letterale nota a tempo di compilazione
    /// (es. l'argomento di SPEED.SELECT("...") / SPEED.CREATEVIEW(nome, tipo, "...")), la
    /// parsea QUI, a tempo di compilazione, riusando la stessa infrastruttura JIT di
    /// Parser.JustInTime (Lexer+Parser "usa e getta", l'identica strada già percorsa dalle
    /// macro di comando Clipper `&comando`) — producendo un vero SpeedSelectStmt STATICO,
    /// indistinguibile da uno scritto direttamente con la sintassi DSL. Il testo (che inizia
    /// già con "SELECT", essendo quello il punto della sintassi dinamica) viene prefissato con
    /// "SPEED " in modo da rientrare nel normale dispatcher del Parser (ParseSpeedStatement si
    /// aspetta sempre la keyword SPEED davanti).
    ///
    /// Il SymbolId che il parser "usa e getta" assegna alla eventuale variabile INTO è
    /// IRRILEVANTE e viene scartato: AssignVariable (chiamato dal chiamante su
    /// SpeedSelectStmt.IntoTarget) risolve un VariableTarget SOLO per NOME
    /// (vt.NameCleaned.Lexeme) contro i _locals/_globals del Compiler REALE che sta compilando
    /// il programma — non usa mai il SymbolId del nodo. Per questo non serve condividere la
    /// symbol table fra il parser temporaneo e quello reale: il nome basta, ed è esattamente
    /// lo stesso meccanismo già usato dal DSL statico.
    ///
    /// Se la stringa non è un letterale (es. costruita a runtime da variabili) o contiene un
    /// errore di sintassi, ritorna null: il chiamante mantiene il comportamento no-op
    /// preesistente per quel caso, che resta l'unico non risolvibile a tempo di compilazione.
    /// </summary>
    private static SpeedSelectStmt? TryParseDynamicSpeedSelect(Expr? queryExpr)
    {
        var sql = queryExpr switch
        {
            StringExpr se => se.Value,
            CheckedStringExpr cse => cse.Value,
            _ => null
        };
        if (string.IsNullOrWhiteSpace(sql)) return null;

        try
        {
            var settings = new CommonLib.EffectiveSettings();
            var program = new BasicProgram(settings, null, Directory.GetCurrentDirectory());
            // Il dispatcher del Parser riconosce "SPEED xxx" come comando legacy solo se "SPEED"
            // risulta tra i namespace importati del programma (normalmente via IMPORT SPEED WITH
            // ALIAS) — un BasicProgram "usa e getta" appena creato non ne ha nessuno, quindi va
            // registrato esplicitamente qui prima di parsare, altrimenti "SPEED" viene letto come
            // un identificatore qualunque e "SELECT" resta un token residuo inatteso.
            program.Namespaces = ImmutableHashSet.Create("SPEED");
            var position = queryExpr!.Position;
            var sourceLine = new SourceLine(position.FileId, "<speed-dynamic-sql>", position.Line, sql);
            // `sql` inizia già con "SELECT" (è quello l'intero punto della sintassi dinamica),
            // basta anteporre "SPEED " per farlo rientrare nel dispatcher legacy — anteporre
            // "SPEED SELECT " (con un secondo SELECT) duplicherebbe la keyword e produrrebbe un
            // campo spurio "SELECT * INTO" mal interpretato come espressione invece del
            // riconoscimento speciale di "*" in ParseSelectClause.
            var parsedStmt = Parser.ParseSingleStatement("SPEED " + sql, program, sourceLine, 1);
            return parsedStmt as SpeedSelectStmt;
        }
        catch
        {
            return null;
        }
    }

    // --- Comandi di Gestione e Apertura ---

    /// <summary>
    /// Compila un'istruzione SPEED OPEN.
    /// Emette il codice per valutare il percorso del database e spinge l'alias come costante.
    /// </summary>
    private void VisitSpeedOpenStatement(SpeedOpenStmt stmt)
    {
        // 1. Compila l'espressione del percorso del database e la spinge sullo stack.
        VisitExpression(stmt.DatabasePath);
        
        // 2. Spinge l'alias del database (una stringa costante) sullo stack.
        EmitConstant(stmt.OriginalAliasToken.Lexeme);

        // 3. Emette la syscall per aprire il database Speed.
        // La VM si aspetta 2 argomenti: [database_path_string, alias_string]
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_OPEN); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione `SPEED CLOSE [alias | DATABASES]`.
    /// Emette 1 argomento (stringa): l'alias da chiudere, oppure "" per chiudere
    /// tutte le aree SPEED (DATABASES / area corrente).
    /// </summary>
    private void VisitSpeedCloseStatement(SpeedCloseStmt stmt)
    {
        EmitConstant(stmt.AllDatabases || stmt.AliasToken is null ? "" : stmt.AliasToken.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_CLOSE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione SPEED USE.
    /// Emette il codice per valutare le espressioni e spingere i dati della tabella
    /// (nome, riferimento al DB, tipo, alias, indici) sullo stack per la syscall.
    /// </summary>
    private void VisitSpeedUseStatement(SpeedUseStmt stmt)
    {
        // Argomenti base (6)
        VisitExpression(stmt.TableName);            // 1. Nome della tabella (stringa)
        EmitFileNumber(stmt.DatabaseReference);       // 2. Alias del database (come stringa)
        EmitConstant((double)stmt.TypeSymbolId);      // 3. ID del simbolo del tipo UDT (numero)
        EmitConstant(stmt.OriginalTypeName.Lexeme); // 4. Nome originale del tipo (stringa, per diagnostica)
        EmitConstant(stmt.OriginalAliasToken.Lexeme);// 5. Alias della tabella (stringa)
        EmitConstant(stmt.IsUnsafe);                // 6. Flag IsUnsafe (booleano)

        // Argomenti per IndexedBy
        if (stmt.IndexedBy is not null)
        {
            EmitConstant((double)stmt.IndexedBy.KeyFields.Length); // 7. Numero di chiavi
            foreach (var keyField in stmt.IndexedBy.KeyFields)
            {
                // Per ogni chiave, spingiamo l'espressione come stringa e il suo flag unsafe.
                // La VM dovrà parsare/interpretare questa stringa di espressione a runtime.
                EmitConstant(keyField.KeyExpression.ToString()); // 8a, 9a, ... Espressione chiave (stringa)
                EmitConstant(keyField.IsUnsafe);                 // 8b, 9b, ... Flag unsafe della chiave (booleano)
            }
        }
        else
        {
            EmitConstant(0.0); // 7. Nessun IndexedBy (0.0 per indicare 0 chiavi)
        }

        // Campi dell'UDT associato (Nome, SymbolId) — servono a SPEED_GET per costruire il
        // wrapper "__class__"/FieldNames che OP_LOAD_MEMBER richiede quando GET popola una
        // variabile record (stesso pattern già usato per DBF USE/GET in Compiler.Dbf.cs — senza
        // questa metadata, "rec.Campo" dopo un SPEED GET risolveva sempre a NIL, perché il
        // dizionario restituito da SPEED_GET non aveva modo di essere letto per offset via
        // OP_LOAD_MEMBER).
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
                    EmitConstant((double)f.SymbolId);
                }
            }
        }
        EmitConstant((double)fieldCount);

        // Calcolo del numero totale di argomenti passati alla syscall.
        var totalArgs = 6; // I 6 argomenti base.
        if (stmt.IndexedBy is not null)
        {
            totalArgs += 1; // Per il conteggio delle chiavi.
            totalArgs += stmt.IndexedBy.KeyFields.Length * 2; // Per ogni coppia (KeyExprString, IsUnsafe).
        }
        else
        {
            totalArgs += 1; // Per il contatore 0.0 che indica "nessun IndexedBy".
        }
        totalArgs += 1 + fieldCount * 2; // FieldCount + coppie (Nome, SymbolId).

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_USE); EmitByte((byte)(totalArgs));
    }

    // --- Comandi di Navigazione ---

    /// <summary>
    /// Compila un'istruzione SPEED GO TOP/BOTTOM.
    /// </summary>
    private void VisitSpeedGoStatement(SpeedGoStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        EmitConstant((double)stmt.Location.Kind); // TOP/BOTTOM come valore numerico
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_GO); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione SPEED SKIP.
    /// </summary>
    private void VisitSpeedSkipStatement(SpeedSkipStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        if (stmt.Count is not null) VisitExpression(stmt.Count);
        else EmitConstant(1.0); // Default SKIP 1
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SKIP); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione SPEED SEEK.
    /// </summary>
    private void VisitSpeedSeekStatement(SpeedSeekStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        VisitExpression(stmt.KeyValue);
        if (stmt.IndexName is not null) VisitExpression(stmt.IndexName);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SEEK); EmitByte((byte)3);
    }

    // --- Comandi di Ricerca e Modifica ---

    /// <summary>
    /// Compila un'istruzione GET per il motore Speed.
    /// </summary>
    private void VisitSpeedGetStatement(SpeedGetStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_GET); EmitByte((byte)1);
        AssignVariable(stmt.Destination, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione PUT per il motore Speed.
    /// </summary>
    private void VisitSpeedPutStatement(SpeedPutStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        // Il record sorgente non veniva mai pushato (era commentato): SPEED_PUT riceveva
        // sempre "source = null" (in realtà qualunque valore fosse rimasto casualmente in cima
        // allo stack da codice precedente, dato che argCount dichiarava comunque 2 argomenti),
        // quindi PUT non scriveva mai il record — scoperto verificando che una successiva GET
        // non trovava mai i dati appena inseriti.
        if (stmt.Source is not null) VisitExpression(stmt.Source);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_PUT); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione DELETE per il motore Speed.
    /// </summary>
    private void VisitSpeedDeleteStatement(SpeedDeleteStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        // Stesso bug/fix di VisitSpeedPutStatement sopra.
        if (stmt.Source is not null) VisitExpression(stmt.Source);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_DELETE_RECORD); EmitByte((byte)2);
    }

    // --- Comandi di Stato e Manutenzione ---

    /// <summary>
    /// Compila un'istruzione SPEED CREATE INDEX.
    /// L'espressione della chiave viene compilata in un blocco P-Code.
    /// </summary>
    private void VisitSpeedCreateIndexStatement(SpeedCreateIndexStmt stmt)
    {
        EmitFileNumber(stmt.Alias); // Arg 1: Alias della tabella
        VisitExpression(stmt.IndexName); // Arg 2: Nome dell'indice (Expr)

        // Arg 3: la KeyExpression viene passata come nodo AST grezzo (non compilata a P-Code):
        // la VM non ha un meccanismo di esecuzione di sotto-blocchi P-Code legato a un contesto
        // di riga (i FOR/WHILE di DBF che usano lo stesso pattern CompileExpressionToCodeBlock +
        // OP_PUSH_CODE_REF non vengono infatti mai eseguiti — restano sullo stack e basta). Il
        // motore Speed valuta invece l'espressione direttamente sull'AST tramite un piccolo
        // evaluator dedicato (SpeedEvaluateExpr in PCodeProcessor.SpeedQuery.cs), che riceve
        // l'oggetto Expr così com'è: OP_PUSH_CONST pusha qualunque valore .NET presente nella
        // constant pool, quindi funziona per un nodo AST tanto quanto per una stringa o un numero.
        EmitConstant(stmt.KeyExpression);

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_CREATE_INDEX); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione SPEED DROP INDEX.
    /// </summary>
    private void VisitSpeedDropIndexStatement(SpeedDropIndexStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        VisitExpression(stmt.IndexName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_DROP_INDEX); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione SPEED SET ORDER.
    /// </summary>
    private void VisitSpeedSetOrderStatement(SpeedSetOrderStmt stmt)
    {
        EmitFileNumber(stmt.Alias);
        VisitExpression(stmt.IndexName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SET_ORDER); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione SPEED SET FILTER.
    /// Ora compila la condizione del filtro in un blocco P-Code.
    /// </summary>
    private void VisitSpeedSetFilterStatement(SpeedSetFilterStmt stmt)
    {
        EmitFileNumber(stmt.Alias); // Arg 1: Alias della tabella
        EmitConstant(stmt.Condition is not null); // Arg 2: Flag hasCondition

        if (stmt.Condition is MacroExpr macroFilter)
        {
            // Come DBF SET FILTER: espansione macro UNA volta a setup (il VALORE
            // dell'interno è il testo del filtro).
            VisitExpression(macroFilter.Expression);
        }
        else if (stmt.Condition is not null)
        {
            // Nodo AST grezzo, non code-ref — vedi commento in VisitSpeedCreateIndexStatement.
            EmitConstant(stmt.Condition);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Arg 3: NIL se nessun filtro
        }

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SET_FILTER); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione SPEED SET RELATION.
    /// </summary>
    private void VisitSpeedSetRelationStatement(SpeedSetRelationStmt stmt)
    {
        EmitFileNumber(stmt.ParentAlias);
        if (stmt.KeyExpression is not null)
        {
            // Nodo AST grezzo, non code-ref — vedi commento in VisitSpeedCreateIndexStatement.
            EmitConstant(stmt.KeyExpression);
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        if (stmt.ChildAlias is not null) EmitFileNumber(stmt.ChildAlias);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SET_RELATION); EmitByte((byte)3);
    }

    // --- Comandi di Query ---

    /// <summary>
    /// Compila un'istruzione SPEED SELECT.
    /// Questa versione è stata corretta per gestire i campi nullable del nuovo
    /// record AST, e distingue tra la compilazione di una query statica e una dinamica.
    /// </summary>
    private void VisitSpeedSelectStatement(SpeedSelectStmt stmt)
    {
        // --- CASO 1: QUERY DINAMICA (da stringa, es. SPEED.SELECT("SELECT ... FROM ...")) ---
        // Se la stringa è un letterale noto a tempo di compilazione, viene parsata QUI (non a
        // runtime) riusando l'infrastruttura JIT di Parser.JustInTime (la stessa delle macro
        // Clipper `&comando`) per produrre un vero SpeedSelectStmt statico, poi ricompilato
        // ricorsivamente come CASO 2 sotto — stesso identico bytecode del DSL statico, INCLUSA
        // la risoluzione per nome della variabile INTO in AssignVariable (che non dipende dal
        // SymbolId prodotto dal parse "usa e getta", solo dal nome — vedi TryParseDynamicSpeedSelect).
        // Solo le stringhe NON letterali (composte a runtime) restano no-op: non c'è modo di
        // sapere a tempo di compilazione quale variabile scrivere per il loro INTO.
        if (stmt.QueryExpression != null)
        {
            var parsed = TryParseDynamicSpeedSelect(stmt.QueryExpression);
            if (parsed != null)
            {
                VisitSpeedSelectStatement(parsed);
            }
            return;
        }

        // --- CASO 2: QUERY STATICA (legacy) ---
        // Aggiunge controlli per assicurarsi che i campi obbligatori non siano null
        if (stmt.From is null || stmt.SelectFields is null || stmt.IntoTarget is null)
        {
            throw new InvalidOperationException(
                "Internal Compiler Error: Static SpeedSelectStmt has null required fields.");
        }

        // Le espressioni (WHERE/GROUP BY/HAVING/ORDER BY/campi SELECT) vengono passate come nodi
        // AST grezzi (EmitConstant), non compilate a P-Code: vedi il commento in
        // VisitSpeedCreateIndexStatement sul perché CompileExpressionToCodeBlock+OP_PUSH_CODE_REF
        // non è utilizzabile per esprimere una valutazione differita legata a un contesto riga.
        // Il motore Speed (PCodeProcessor.SpeedQuery.cs) valuta questi nodi con un evaluator
        // dedicato che risolve i riferimenti campo (anche qualificati Alias.Campo) contro il
        // dizionario della riga corrente.

        // Campi del TYPE di destinazione (nome, symbolId) — pushati PRIMA di tutti gli
        // altri argomenti (in FONDO allo stack, quindi poppati per ULTIMI da P-Code e VM/C).
        // Servono alla VM/C per costruire l'array UDT risultato con le chiavi "__m<sid>"
        // che OP_LOAD_MEMBER/OP_STORE_MEMBER richiedono (come SPEED_GET): il runtime C non
        // conosce la SymbolTable del TYPE `OF <Tipo>`.
        var destFieldCount = 0;
        if (_ctx != null && stmt.DestinationUdtToken is { } destTok)
        {
            var destTypeName = destTok.Lexeme;
            var destUdt = _ctx.GetAllUserDefinedTypes().FirstOrDefault(u =>
                u.Name.Lexeme.Equals(destTypeName, StringComparison.OrdinalIgnoreCase));
            if (destUdt != null)
            {
                destFieldCount = destUdt.Fields.Length;
                foreach (var df in destUdt.Fields)
                {
                    EmitConstant(df.Name.Lexeme);
                    EmitConstant((double)df.SymbolId);
                }
            }
        }
        EmitConstant((double)destFieldCount);

        // Alias della tabella FROM
        EmitConstant(stmt.From.TableAlias.Lexeme);

        // Clausola JOIN (se presente): la condizione (es. Clienti.ID = Ordini.ID_Cliente) viene
        // scomposta qui, a tempo di compilazione — quando l'intero AST è ancora disponibile — in
        // un'espressione "chiave sinistra" (valutata sulla riga FROM per ottenere il valore da
        // cercare) e un nome di campo "destro" nudo (usato per il lookup diretto sulla tabella
        // JOIN). Radice della scomposizione: quale lato della condizione referenzia l'alias FROM
        // e quale l'alias JOIN.
        if (stmt.Join is not null)
        {
            EmitConstant(stmt.Join.TableAlias.Lexeme); // Alias JOIN
            var (leftKeyExpr, rightFieldName) = DecomposeJoinCondition(
                stmt.Join.Condition, stmt.From.TableAlias.Lexeme, stmt.Join.TableAlias.Lexeme);
            EmitConstant(leftKeyExpr);
            EmitConstant(rightFieldName);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Nessun alias JOIN
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Nessuna chiave sinistra JOIN
            EmitByte((byte)OpCode.OP_PUSH_NIL); // Nessun campo destro JOIN
        }

        // Clausola WHERE (se presente)
        if (stmt.Where is not null) EmitConstant(stmt.Where);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        // Clausola GROUP BY (se presente). Conteggio pushato PER ULTIMO (dopo gli elementi):
        // la VM pop-a LIFO, quindi deve trovare il conteggio in cima per sapere quanti elementi
        // pop-are subito dopo — stesso pattern già usato altrove nel progetto (es.
        // VisitDbfUseStatement/ISAM_USE per gli IndexPaths).
        if (stmt.GroupBy is not null && stmt.GroupBy.Value.Any())
        {
            foreach (var expr in stmt.GroupBy.Value) EmitConstant(expr);
            EmitConstant((double)stmt.GroupBy.Value.Length);
        }
        else
        {
            EmitConstant(0.0); // Nessun GROUP BY
        }

        // Clausola HAVING (se presente)
        if (stmt.Having is not null) EmitConstant(stmt.Having);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);

        // Clausola ORDER BY (se presente): stesso pattern conteggio-per-ultimo.
        if (stmt.OrderBy is not null && stmt.OrderBy.Value.Any())
        {
            foreach (var field in stmt.OrderBy.Value)
            {
                EmitConstant(field.Expression);
                EmitConstant(field.Descending);
            }
            EmitConstant((double)stmt.OrderBy.Value.Length);
        }
        else
        {
            EmitConstant(0.0); // Nessun ORDER BY
        }

        // SelectFields (es. COUNT(*), ID AS ClientID). "SELECT *" arriva qui come un singolo
        // campo la cui Expression è la StringExpr letterale "*" (vedi Parser.Stmt.Speed.cs,
        // ParseSelectClause) — il runtime la riconosce e la espande in tutte le colonne della
        // riga sorgente, non essendo un vero riferimento di campo da valutare. Stesso pattern
        // conteggio-per-ultimo.
        foreach (var field in stmt.SelectFields.Value)
        {
            EmitConstant(field.Expression);
            if (field.Alias is not null) EmitConstant(field.Alias.Lexeme);
            else EmitByte((byte)OpCode.OP_PUSH_NIL);
        }
        EmitConstant((double)stmt.SelectFields.Value.Length);

        // Calcolo ArgCount per la SysCall: FromAlias(1) + JoinAlias/LeftKey/RightField(3) +
        // Where(1) + GroupByCount+N(1+N) + Having(1) + OrderByCount+2*N(1+2N) +
        // SelectFieldCount+2*N(1+2N)
        var argCount = (byte)((1 + destFieldCount * 2) + 1 + 3 + 1 + (1 + (stmt.GroupBy?.Length ?? 0)) + 1 +
                              (1 + (stmt.OrderBy?.Length ?? 0) * 2) + (1 + (stmt.SelectFields.Value.Length) * 2));

        // La destinazione INTO NON viene pushata come argomento: la syscall lascia l'array dei
        // risultati sullo stack come valore di ritorno, assegnato qui alla variabile di
        // destinazione con lo stesso pattern usato da DBF GET/ISAM GET (AssignVariable dopo la
        // syscall) — LoadVariable pusherebbe solo il VALORE corrente dell'array, non uno slot
        // scrivibile, quindi non è utilizzabile per restituire il risultato della query.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_SELECT); EmitByte((byte)(argCount));
        AssignVariable(stmt.IntoTarget, isLet: false);
    }

    /// <summary>
    /// Scompone una condizione di JOIN in forma "AliasSinistro.Campo = AliasDestro.Campo" (in
    /// qualunque ordine) in un'espressione chiave da valutare sulla riga sinistra e nel nome nudo
    /// del campo destro da cercare. Se la condizione non ha questa forma (espressione complessa
    /// su uno dei due lati, operatore diverso da =), la restituisce così com'è come chiave
    /// sinistra con campo destro vuoto: il motore a runtime tratterà comunque la condizione come
    /// booleana generica riga-per-riga di fallback (JOIN più lento, nested loop pieno, ma corretto).
    /// </summary>
    private static (Expr LeftKeyExpr, string RightFieldName) DecomposeJoinCondition(
        Expr condition, string leftAlias, string rightAlias)
    {
        if (condition is BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.ExactlyEqual } bin)
        {
            var leftSideAlias = GetFieldAccessAlias(bin.Left);
            var rightSideAlias = GetFieldAccessAlias(bin.Right);
            if (string.Equals(leftSideAlias, leftAlias, StringComparison.OrdinalIgnoreCase) &&
                string.Equals(rightSideAlias, rightAlias, StringComparison.OrdinalIgnoreCase))
            {
                return (bin.Left, GetFieldName(bin.Right));
            }

            if (string.Equals(rightSideAlias, leftAlias, StringComparison.OrdinalIgnoreCase) &&
                string.Equals(leftSideAlias, rightAlias, StringComparison.OrdinalIgnoreCase))
            {
                return (bin.Right, GetFieldName(bin.Left));
            }
        }

        return (condition, "");
    }

    private static string? GetFieldAccessAlias(Expr e) =>
        e is FieldAccessExpr { Structure: VariableExpr v } ? v.NameCleaned.Lexeme : null;

    private static string GetFieldName(Expr e) =>
        e switch
        {
            FieldAccessExpr f => f.Field.Lexeme,
            VariableExpr v => v.NameCleaned.Lexeme,
            _ => ""
        };

    /// <summary>
    /// Compila un'istruzione SPEED CREATE VIEW.
    /// Ora compila correttamente la SELECT statement interna come un blocco P-Code.
    /// </summary>
    private void VisitSpeedCreateViewStatement(SpeedCreateViewStmt stmt)
    {
        // Arg 1: Nome della vista (stringa)
        EmitConstant(stmt.OriginalViewName.Lexeme);

        EmitConstant((double)stmt.OfTypeSymbolId); // Passa l'ID del tipo
        EmitConstant(stmt.OriginalTypeName.Lexeme); // Passa il nome per la diagnostica

        // Arg 4: nodo AST grezzo della SELECT statement interna. Se AS SELECT ... è statico lo
        // usa direttamente; se invece è AS "stringa SQL" (sintassi dinamica) e la stringa è un
        // letterale noto a tempo di compilazione, la parsea QUI (vedi TryParseDynamicSpeedSelect,
        // stesso meccanismo di VisitSpeedSelectStatement) producendo lo stesso identico
        // SpeedSelectStmt statico che avrebbe prodotto la sintassi AS SELECT — valutato dal
        // motore Speed con lo stesso evaluator usato per SPEED SELECT. Il RISULTATO della SELECT
        // (incluso il popolamento della sua clausola INTO) viene anche RICORDATO dalla VM per
        // permettere a SPEED REFRESH VIEW di ri-eseguire la stessa query in seguito.
        var effectiveSelect = stmt.SelectStatement ?? TryParseDynamicSpeedSelect(stmt.QueryExpression);
        if (effectiveSelect is not null)
        {
            EmitConstant(effectiveSelect);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Arg 5: espressione dinamica originale — valutata per side-effect SOLO quando non è
        // stata risolta a tempo di compilazione sopra (stringa non letterale, es. composta da
        // variabili): resta no-op nel motore Speed, ma il calcolo della stringa avviene comunque.
        // Se invece è già stata risolta in effectiveSelect, non serve rivalutarla: NIL.
        if (stmt.QueryExpression is not null && effectiveSelect is null)
        {
            VisitExpression(stmt.QueryExpression);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Numero di argomenti: ViewName(1) + TypeSymbolId(1) + OriginalTypeName(1) +
        // SelectStatement(1) + QueryExpression(1) = 5
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_CREATE_VIEW); EmitByte((byte)5);

        // La syscall lascia sempre sullo stack l'array dei risultati della SELECT interna
        // (vuoto se non c'è una SELECT statica/dinamica risolta) — va sempre consumato per
        // bilanciare lo stack: assegnato alla variabile INTO se presente, altrimenti scartato.
        if (effectiveSelect?.IntoTarget is { } intoTarget)
        {
            _speedViewIntoTargets[stmt.OriginalViewName.Lexeme] = intoTarget;
            AssignVariable(intoTarget, isLet: false);
        }
        else
        {
            EmitByte((byte)OpCode.OP_POP);
        }
    }

    /// <summary>
    /// Compila un'istruzione SPEED REFRESH VIEW.
    /// </summary>
    private void VisitSpeedRefreshViewStatement(SpeedRefreshViewStmt stmt)
    {
        EmitConstant(stmt.ViewName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_REFRESH_VIEW); EmitByte((byte)1);

        // Stesso bilanciamento stack di SPEED CREATE VIEW: riassegna alla stessa variabile INTO
        // ricordata dalla CREATE VIEW originale (stesso scope), se nota; altrimenti scarta.
        if (_speedViewIntoTargets.TryGetValue(stmt.ViewName.Lexeme, out var intoTarget))
        {
            AssignVariable(intoTarget, isLet: false);
        }
        else
        {
            EmitByte((byte)OpCode.OP_POP);
        }
    }

    /// <summary>
    /// Compila un'istruzione SPEED DROP VIEW.
    /// </summary>
    private void VisitSpeedDropViewStatement(SpeedDropViewStmt stmt)
    {
        EmitConstant(stmt.ViewName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPEED_DROP_VIEW); EmitByte((byte)1);
    }
}