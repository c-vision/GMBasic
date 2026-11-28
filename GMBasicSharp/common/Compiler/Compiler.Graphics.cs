// FILE: Interpreter.Compiler.Compiler.Graphics.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per tutte le istruzioni grafiche.
/// </summary>
public sealed partial class Compiler
{
    // ========================================================================
    // --- GRAFICA STANDARD (SysCalls) ---
    // ========================================================================

    /// <summary>
    /// Compila un'istruzione SCREEN.
    /// </summary>
    private void VisitScreenStatement(ScreenStmt stmt)
    {
        VisitExpression(stmt.Mode);
        if (stmt.ActivePage is not null) VisitExpression(stmt.ActivePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.VisiblePage is not null) VisitExpression(stmt.VisiblePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.CloseRepl is not null) VisitExpression(stmt.CloseRepl); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.WindowModeExpr is not null) VisitExpression(stmt.WindowModeExpr); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SCREEN); EmitByte((byte)5);
        /* Spazio coordinate DICHIARATO: 1 = CELLE, 2 = PIXEL, 0 = AUTO (nessuna chiamata).
         * Il motore e' il runtime C (gmb_gfx_set_coord_mode), come per gli altri backend. */
        if (stmt.CoordMode != 0)
        {
            EmitConstant(stmt.CoordMode == 1 ? 1.0 : 0.0);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SCREEN_COORDS); EmitByte((byte)1);
        }
    }

    /// <summary>
    /// Compila un'istruzione TEXTMODE (selezione modalita' console).
    /// </summary>
    private void VisitTextModeStatement(TextModeStmt stmt)
    {
        VisitExpression(stmt.Mode);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEXTMODE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione PSET.
    /// </summary>
    private void VisitPSetStatement(PSetStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PSET); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione PRESET.
    /// </summary>
    private void VisitPresetStatement(PresetStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PRESET); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione LINE.
    /// </summary>
    private void VisitLineStatement(LineStmt stmt)
    {
        if (stmt.X1 is not null) VisitExpression(stmt.X1); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Y1 is not null) VisitExpression(stmt.Y1); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        // Style ("B"/"BF"): il parser lo parsa come IdentifierExpr (senza virgolette) →
        // LoadVariable("B") → PUSH_NIL silenzioso → LINE B/BF cadeva nel default (diagonale).
        // Un IdentifierExpr "B"/"BF" va emesso come COSTANTE STRINGA.
        if (stmt.Style is not null)
        {
            if (stmt.Style is VariableExpr varStyle)
                EmitConstant(varStyle.NameCleaned.Lexeme);
            else if (stmt.Style is StringExpr sExpr)
                EmitConstant(sExpr.Value);
            else
                VisitExpression(stmt.Style);
        }
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LINE); EmitByte((byte)6);
    }

    /// <summary>
    /// Compila un'istruzione CIRCLE.
    /// </summary>
    private void VisitCircleStatement(CircleStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        VisitExpression(stmt.Radius);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Start is not null) VisitExpression(stmt.Start); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.End is not null) VisitExpression(stmt.End); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Aspect is not null) VisitExpression(stmt.Aspect); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CIRCLE); EmitByte((byte)7);
    }

    /// <summary>
    /// Compila un'istruzione PAINT.
    /// </summary>
    private void VisitPaintStatement(PaintStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        if (stmt.FillColor is not null) VisitExpression(stmt.FillColor); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.BorderColor is not null) VisitExpression(stmt.BorderColor); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PAINT); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione DRAW.
    /// </summary>
    private void VisitDrawStatement(DrawStmt stmt)
    {
        VisitExpression(stmt.CommandString);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DRAW); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione FONT.
    /// </summary>
    private void VisitFontStatement(FontStmt stmt)
    {
        VisitExpression(stmt.FontPath);
        VisitExpression(stmt.Size);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FONT); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione PCOPY.
    /// </summary>
    private void VisitPCopyStatement(PCopyStmt stmt)
    {
        VisitExpression(stmt.SourcePage);
        VisitExpression(stmt.DestinationPage);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PCOPY); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione SCREENSET.
    /// </summary>
    private void VisitScreenSetStatement(ScreenSetStmt stmt)
    {
        if (stmt.VisiblePage is not null) VisitExpression(stmt.VisiblePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ActivePage is not null) VisitExpression(stmt.ActivePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SCREENSET); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione IMAGEGET.
    /// </summary>
    private void VisitImageGetStatement(ImageGetStmt stmt)
    {
        VisitExpression(stmt.X1);
        VisitExpression(stmt.Y1);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IMAGEGET); EmitByte((byte)4);
        AssignVariable(stmt.Array, isLet: false);
    }

    /// <summary>
    /// Compila UNLOAD TEXTURE/FONT — rilascio esplicito delle risorse.
    /// Prima era EmitExprAndPop (no-op) → le risorse non venivano MAI scaricate.
    /// </summary>
    private void VisitUnloadResourceStatement(UnloadResourceStmt stmt)
    {
        if (stmt.ResourceKind == UnloadResourceKind.Font)
        {
            EmitConstant((double)1); // 1 = font
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.UNLOAD_RESOURCE); EmitByte((byte)1);
        }
        else
        {
            EmitConstant((double)0); // 0 = texture
            VisitExpression(stmt.Handle);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.UNLOAD_RESOURCE); EmitByte((byte)2);
        }
    }

    /// <summary>
    /// Compila un'istruzione POLYGON: carica l'array di punti + colore, syscall.
    /// Prima era EmitExprAndPop (no-op) → il POLYGON non disegnava MAI.
    /// </summary>
    private void VisitPolygonStatement(PolygonStmt stmt)
    {
        LoadVariable(stmt.PointsArray);
        VisitExpression(stmt.Color);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.POLYGON); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione IMAGEPUT.
    /// </summary>
    private void VisitImagePutStatement(ImagePutStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        LoadVariable(stmt.Array); // L'array è sulla stack
        if (stmt.Mode is not null) VisitExpression(stmt.Mode); else EmitByte((byte)OpCode.OP_PUSH_NIL); // La modalità (stringa)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IMAGEPUT); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione IMAGEPUTS (batch).
    /// </summary>
    private void VisitImagePutsStatement(ImagePutsStmt stmt)
    {
        LoadVariable(stmt.SpriteArray); // L'array degli sprite
        LoadVariable(stmt.PositionsArray); // L'array delle posizioni
        if (stmt.Mode is not null) VisitExpression(stmt.Mode); else EmitByte((byte)OpCode.OP_PUSH_NIL); // La modalità (stringa)
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IMAGEPUTS); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione PALETTE.
    /// </summary>
    private void VisitPaletteStatement(PaletteStmt stmt)
    {
        VisitExpression(stmt.Index);
        VisitExpression(stmt.R);
        VisitExpression(stmt.G);
        VisitExpression(stmt.B);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione SAVE SCREEN.
    /// </summary>
    private void VisitSaveScreenStatement(SaveScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SAVE_SCREEN); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione RESTORE SCREEN.
    /// </summary>
    private void VisitRestoreScreenStatement(RestoreScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RESTORE_SCREEN); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione BSAVE.
    /// </summary>
    private void VisitBSaveStatement(BSaveStmt stmt)
    {
        VisitExpression(stmt.X1);
        VisitExpression(stmt.Y1);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.BSAVE); EmitByte((byte)4);
        AssignVariable(stmt.Buffer, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione BLOAD.
    /// </summary>
    private void VisitBLoadStatement(BLoadStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        VisitExpression(stmt.Buffer);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.BLOAD); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione CLEAR (rettangolo).
    /// </summary>
    private void VisitClearRectStatement(ClearRectStmt stmt)
    {
        VisitExpression(stmt.X1);
        VisitExpression(stmt.Y1);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLEAR_RECT); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione POINT.
    /// </summary>
    private void VisitPointStatement(PointStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.POINT); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione LOAD TEXTURE.
    /// </summary>
    private void VisitLoadTextureStatement(LoadTextureStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LOAD_TEXTURE); EmitByte((byte)1);
        AssignVariable(stmt.Destination, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione DRAW TEXTURE.
    /// </summary>
    private void VisitDrawTextureStatement(DrawTextureStmt stmt)
    {
        VisitExpression(stmt.TextureId);
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        if (stmt.Rotation is not null) VisitExpression(stmt.Rotation); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ScaleX is not null) VisitExpression(stmt.ScaleX); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ScaleY is not null) VisitExpression(stmt.ScaleY); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DRAW_TEXTURE); EmitByte((byte)6);
    }
    
    /// <summary>
    /// Compila DRAW TEXTURES (batch, array paralleli): texId, X(), Y(), Rot(), Scale().
    /// </summary>
    private void VisitDrawTexturesStatement(DrawTexturesStmt stmt)
    {
        if (stmt.TextureId != null) VisitExpression(stmt.TextureId); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.XArray != null) LoadVariable(stmt.XArray); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.YArray != null) LoadVariable(stmt.YArray); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.RotArray != null) LoadVariable(stmt.RotArray); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.ScaleArray != null) LoadVariable(stmt.ScaleArray); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DRAW_TEXTURES); EmitByte((byte)5);
    }

    /// <summary>
    /// Compila un'istruzione SYNC.
    /// </summary>
    private void VisitSyncStatement(SyncStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SYNC); EmitByte((byte)0);
    }

    // ========================================================================
    // --- GRAFICA AVANZATA (OPCODE CON SOTTOCOMANDI) ---
    // ========================================================================
    
    /// <summary>
    /// Compila un comando VECTOR generico.
    /// </summary>
    private void VisitVectorCommandStatement(VectorCommandStmt stmt)
    {
        var subOpCode = stmt.Command.Kind switch
        {
            TokenKind.LinAlgVectorAddCommand => VectorSubOpCode.ADD,
            TokenKind.LinAlgVectorSubCommand => VectorSubOpCode.SUB,
            TokenKind.LinAlgVectorMulCommand => VectorSubOpCode.MUL,
            TokenKind.LinAlgVectorDivCommand => VectorSubOpCode.DIV,
            TokenKind.LinAlgVectorSetCommand => VectorSubOpCode.SET,
            TokenKind.LinAlgVectorFillRandomCommand => VectorSubOpCode.FILLRANDOM,
            TokenKind.LinAlgVectorWrapCommand => VectorSubOpCode.WRAP,
            TokenKind.LinAlgVectorAddWrapCommand => VectorSubOpCode.ADDWRAP,
            TokenKind.LinAlgVectorBounce2dCommand => VectorSubOpCode.BOUNCE2D,
            _ => throw new NotImplementedException($"Unsupported VECTOR command: {stmt.Command.Lexeme}")
        };

        foreach (var arg in stmt.Arguments)
            VisitExpression(arg);

        EmitBytes((byte)OpCode.OP_VECTOR_CMD, (byte)subOpCode, (byte)stmt.Arguments.Length);
    }

    private void VisitMatrixCommandStatement(MatrixCommandStmt stmt)
    {
        var subOpCode = stmt.Command.Kind switch
        {
            TokenKind.LinAlgMatrixCreateFunc => MatrixSubOpCode.CREATE,
            TokenKind.LinAlgMatrixIdentityCommand => MatrixSubOpCode.IDENTITY,
            TokenKind.LinAlgMatrixTranslateCommand => MatrixSubOpCode.TRANSLATE,
            TokenKind.LinAlgMatrixRotateCommand => MatrixSubOpCode.ROTATE,
            TokenKind.LinAlgMatrixScaleCommand => MatrixSubOpCode.SCALE,
            TokenKind.LinAlgMatrixMultiplyCommand => MatrixSubOpCode.MULTIPLY,
            TokenKind.LinAlgMatrixPushCommand => MatrixSubOpCode.PUSH,
            TokenKind.LinAlgMatrixPopCommand => MatrixSubOpCode.POP,
            TokenKind.LinAlgMatrixSetWorldCommand => MatrixSubOpCode.WORLD,
            TokenKind.LinAlgMatrixSetViewCommand => MatrixSubOpCode.VIEW,
            TokenKind.LinAlgMatrixSetProjectionCommand => MatrixSubOpCode.PROJECTION,
            _ => throw new NotImplementedException($"Unsupported MATRIX command: {stmt.Command.Kind}")
        };

        foreach (var arg in stmt.Arguments)
            VisitExpression(arg);

        EmitBytes((byte)OpCode.OP_MATRIX_CMD, (byte)subOpCode, (byte)stmt.Arguments.Length);
    }
    
    /// <summary>
    /// Compila un'istruzione GraphicsPrintExprStmt.
    /// Viene generata dall'Executor quando espande un PRINT in modalità grafica.
    /// </summary>
    private void VisitGraphicsPrintExprStatement(GraphicsPrintExprStmt stmt)
    {
        VisitExpression(stmt.Expression); // Compila l'espressione
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PRINT); EmitByte((byte)1); // SysCall.PRINT gestirà la stampa grafica
    }

    /// <summary>
    /// Compila un'istruzione GraphicsTabZoneStmt.
    /// Viene generata dall'Executor per implementare il TABZONE in grafica.
    /// </summary>
    private void VisitGraphicsTabZoneStatement(GraphicsTabZoneStmt stmt)
    {
        // Questo non ha argomenti, la logica complessa è tutta nell'Executor.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GFX_TAB_ZONE); EmitByte((byte)0); // SysCall dedicata per GFX TABZONE
    }

    /// <summary>
    /// Compila un'istruzione GraphicsNewlineStmt.
    /// Viene generata dall'Executor per implementare il NEWLINE in grafica.
    /// </summary>
    private void VisitGraphicsNewlineStatement(GraphicsNewlineStmt stmt)
    {
        // Questo non ha argomenti, la logica complessa è tutta nell'Executor.
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GFX_NEWLINE); EmitByte((byte)0); // SysCall dedicata per GFX NEWLINE
    }

    /// <summary>
    /// Compila un'istruzione GraphicsTabStmt.
    /// Viene generata dall'Executor per implementare il TAB(col) in grafica.
    /// </summary>
    private void VisitGraphicsTabStatement(GraphicsTabStmt stmt)
    {
        VisitExpression(stmt.ColumnExpression); // Compila l'espressione della colonna di destinazione
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GFX_TAB); EmitByte((byte)1); // SysCall dedicata per GFX TAB
    }

    private void VisitViewStatement(ViewStmt stmt)
    {
        VisitExpression(stmt.X1);
        VisitExpression(stmt.Y1);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Border is not null) VisitExpression(stmt.Border); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VIEWPORT); EmitByte((byte)6);
    }

    private void VisitWindowStatement(ViewPortStmt stmt)
    {
        // WINDOW senza argomenti = reset (ResetViewportAndWindow). Il syscall
        // WINDOW_VIEWPORT gestisce il reset quando lo stack è vuoto; con gli
        // argomenti pusha (x1,y1,x2,y2) + il flag screen. Il vecchio case WINDOW
        // popava 5 valori su 4 push (disallineamento: screen=y2, coordinate
        // scorrimentate → i disegni successivi finivano fuori schermo).
        if (stmt.Screen is false && stmt.X1 is NumberExpr n1 && n1.Value == 0 &&
            stmt.Y1 is NumberExpr n1y && n1y.Value == 0 &&
            stmt.X2 is NumberExpr n2 && n2.Value == 0 &&
            stmt.Y2 is NumberExpr n2y && n2y.Value == 0)
        {
            // reset: nessun push → il case vede lo stack vuoto e fa ResetViewportAndWindow
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_VIEWPORT); EmitByte((byte)0);
            return;
        }
        VisitExpression(stmt.X1);
        VisitExpression(stmt.Y1);
        VisitExpression(stmt.X2);
        VisitExpression(stmt.Y2);
        EmitConstant(stmt.Screen ? 1.0 : 0.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_VIEWPORT); EmitByte((byte)5);
    }

    // SpriteId pushato PER ULTIMO (dopo tutte le righe) cosi' e' in cima allo stack ed e'
    // il primo a essere poppato dall'handler — stesso schema "ultimo pushato, primo poppato"
    // gia' usato e verificato per BSAVE (X1,Y1,X2,Y2 pushati in ordine, poppati Y2,X2,Y1,X1).
    private void VisitSpriteDefStmtC(SpritedefStmt stmt) { foreach (var row in stmt.Rows) EmitConstant(row); VisitExpression(stmt.SpriteId); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPRITE_DEF); EmitByte((byte)(stmt.Rows.Length + 1)); }
    /// <summary>
    /// Compila un'istruzione SPRITE. Due bug corretti rispetto alla versione precedente:
    /// 1) Il sottocomando (stmt.Command.Lexeme) veniva pushato PER PRIMO ma l'handler VM lo
    ///    poppava per primo — con la semantica LIFO dello stack, l'ultimo valore pushato e'
    ///    il primo poppato, quindi il pop prendeva in realta' l'ULTIMO argomento, non il
    ///    comando. Ora il comando e' pushato per ULTIMO (stesso schema gia' verificato per
    ///    BSAVE/SPRITE_DEF/TILEMAP_CMD).
    /// 2) stmt.Command.Lexeme e' incoerente tra sintassi legacy e qualificata: "SPRITE SET
    ///    POSITION ..." produce Lexeme="SET POSITION" (due parole, spazio incluso — vedi
    ///    Parser.Stmt.Graphics.cs ParseSpriteStatement), mentre la sintassi qualificata
    ///    GRAPHICS.SPRITE("SETPOSITION", ...) si aspetta "SETPOSITION" (senza spazio) — nessuno
    ///    dei due combacia mai col case "POSITION" che l'handler controllava, quindi SPRITE SET
    ///    POSITION non ha mai funzionato a runtime. L'AST executor (Interpreter/Gpm.Graphics.cs,
    ///    ExecuteSpriteCommandStep) dispaccia correttamente su stmt.Command.Kind (TokenKind),
    ///    stabile indipendentemente dalla sintassi usata — stessa fonte di verita' usata qui da
    ///    NormalizeSpriteSubCommand, che produce un nome coerente per l'handler VM.
    /// </summary>
    private void VisitSpriteCmdStmtC(SpriteCommandStmt stmt)
    {
        // SPRITE LOAD ha una firma speciale: <file$>, <handleVar> — il secondo argomento e' un
        // TARGET di assegnazione che riceve l'ID sprite caricato, non un valore da leggere (vedi
        // Interpreter/Gpm.Graphics.cs, ExecuteSpriteCommandStep: TargetFromExpression(Arguments[1])).
        // Leggerlo con VisitExpression (come faceva il codice precedente) legge il valore ATTUALE
        // della variabile invece di prepararla per la scrittura. Stesso schema di
        // VisitBSaveStatement: il syscall pusha il risultato, AssignVariable lo consuma e lo scrive.
        if (stmt.Command.Kind == TokenKind.SpriteLoad && stmt.Arguments.Length >= 2)
        {
            VisitExpression(stmt.Arguments[0]);
            EmitConstant(NormalizeSpriteSubCommand(stmt.Command.Kind));
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPRITE_CMD); EmitByte((byte)2);
            if (stmt.Arguments[1] is VariableExpr ve)
                AssignVariable(new VariableTarget(ve.SymbolId, ve.NameSuffixed, ve.NameCleaned), isLet: false);
            else if (stmt.Arguments[1] is ArrayAccessExpr aae && aae.Indices.Length > 0)
            {
                // `SPRITE LOAD "f.png", S(I)` — il risultato (l'id) è sullo stack in fondo;
                // pusha l'array e gli indici, poi OP_STORE_ELEMENT scrive l'elemento.
                LoadVariable(new VariableTarget(-1, aae.NameSuffixed, aae.NameCleaned));
                foreach (var index in aae.Indices) VisitExpression(index);
                EmitBytes((byte)OpCode.OP_STORE_ELEMENT, (byte)aae.Indices.Length);
            }
            return;
        }
        // SPRITE GETPIXELS <id> INTO <arr>: la syscall pusha l'array IMAGEGET (con -1 per i
        // trasparenti), AssignVariable lo assegna alla destinazione.
        if (stmt.Command.Kind == TokenKind.SpriteGetPixels && stmt.Arguments.Length >= 2)
        {
            VisitExpression(stmt.Arguments[0]);
            EmitConstant(NormalizeSpriteSubCommand(stmt.Command.Kind));
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPRITE_CMD); EmitByte((byte)2);
            if (stmt.Arguments[1] is VariableExpr ve2)
                AssignVariable(new VariableTarget(ve2.SymbolId, ve2.NameSuffixed, ve2.NameCleaned), isLet: false);
            else if (stmt.Arguments[1] is ArrayAccessExpr aae2 && aae2.Indices.Length > 0)
            {
                LoadVariable(new VariableTarget(-1, aae2.NameSuffixed, aae2.NameCleaned));
                foreach (var index in aae2.Indices) VisitExpression(index);
                EmitBytes((byte)OpCode.OP_STORE_ELEMENT, (byte)aae2.Indices.Length);
            }
            return;
        }

        foreach (var a in stmt.Arguments) VisitExpression(a);
        EmitConstant(NormalizeSpriteSubCommand(stmt.Command.Kind));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SPRITE_CMD); EmitByte((byte)(stmt.Arguments.Length + 1));
    }

    /// <summary>Nome normalizzato del sottocomando SPRITE, stabile tra sintassi legacy e
    /// qualificata (vedi commento su VisitSpriteCmdStmtC).</summary>
    private static string NormalizeSpriteSubCommand(TokenKind kind) => kind switch
    {
        TokenKind.SpriteLoad => "LOAD",
        TokenKind.SpriteSetPosition => "POSITION",
        TokenKind.SpriteOn => "ON",
        TokenKind.SpriteOff => "OFF",
        TokenKind.SpritePriority => "PRIORITY",
        TokenKind.SpriteTransform => "TRANSFORM",
        TokenKind.SpriteSetMatrix => "MATRIX",
        TokenKind.Scale => "SET SCALE",
        TokenKind.Rotate => "SET ROTATION",
        TokenKind.SpriteGetPixels => "GETPIXELS",
        _ => "UNKNOWN"
    };
    // Stesso schema di VisitSpriteDefStmtC: il nome del sottocomando e' pushato per ULTIMO
    // cosi' l'handler lo poppa per primo (vedi PCodeProcessor case SysCall.TILEMAP_CMD).
    private void VisitTilemapCmdStmtC(TilemapCommandStmt stmt) { foreach (var a in stmt.Arguments) VisitExpression(a); EmitConstant(stmt.Command.Lexeme.ToUpperInvariant()); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TILEMAP_CMD); EmitByte((byte)(stmt.Arguments.Length + 1)); }
    // TICK = inizio frame: argomenti 0, la PCodeVM consegna solo le collisioni del frame
    // precedente (nessuna attesa, nessun present).
    private void VisitTickStmtC(TickStmt stmt) { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TICK_PROCESS); EmitByte((byte)0); }
    // END TICK = fine frame. Due syscall, nell'ordine del riferimento C99Native
    // (`gmb_tick_pace` poi `gmb_gfx_present_frame`): il rate (0 = nessun limiter) è l'argomento
    // di TICK_PROCESS, che PARCHEGGIA il task fino al confine del frame (mai Thread.Sleep su un
    // worker GPM); il SYNC emesso subito dopo è l'UNICO present del frame, eseguito quindi al
    // risveglio. La scadenza è per task (PCodeTask.TickDeadlineMs), non per thread.
    private void VisitTickEndStmtC(TickEndStmt stmt)
    {
        EmitConstant(stmt.TickRate);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TICK_PROCESS); EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SYNC_PROCESS); EmitByte((byte)0);
    }
    private void VisitSyncStmtC(SyncStmt stmt) { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SYNC_PROCESS); EmitByte((byte)0); }
    private void VisitViewStmtC(ViewStmt stmt) { VisitExpression(stmt.X1); VisitExpression(stmt.Y1); VisitExpression(stmt.X2); VisitExpression(stmt.Y2); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VIEW_GFX); EmitByte((byte)4); }
}
