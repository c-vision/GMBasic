// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Statements.Graphics.cs
// PARTIAL — dominio STATEMENTS/GRAPHICS+AUDIO: PSet/Preset/Font/Circle/Line/Draw/
// sprite/tilemap/sound/audio. Migrato da IlProgramCompiler.Statements.cs
// (2026-08-30 — split, file <= 1200 righe).

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

/// <summary>Partial del transpiler IL: statement grafica/audio.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitPSetStatement(PSetStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        if (stmt.Color is not null) EmitInt32Arg(stmt.Color); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsPSetMethod);
    }

    private void EmitPresetStatement(PresetStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        _il.Emit(OpCodes.Callvirt, GraphicsPresetMethod);
    }

    private void EmitFontStatement(FontStmt stmt)
    {
        // FONT sincrono (SdlGraphicsService.Font → Gfx.FONT(...).GetResult) BLOCCHEREBBE un
        // worker GPM in silenzio nel path FLAT (come LOAD TEXTURE). Il v2 lo parcheggia; qui
        // il guard vieta il worker: errore esplicito, mai un blocco muto dello scheduler.
        EmitThrowIfWorker("FONT");
        EmitLoadGraphicsService();
        EmitStringArg(stmt.FontPath); EmitInt32Arg(stmt.Size);
        _il.Emit(OpCodes.Callvirt, GraphicsFontMethod);
    }

    private void EmitPCopyStatement(PCopyStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.SourcePage); EmitInt32Arg(stmt.DestinationPage);
        _il.Emit(OpCodes.Callvirt, GraphicsPCopyMethod);
    }

    private void EmitCircleStatement(CircleStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y); EmitInt32Arg(stmt.Radius);
        if (stmt.Color is not null) EmitInt32Arg(stmt.Color); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsCircleMethod);
    }

    /// <summary>Coordinata grafica: Math.Round AwayFromZero come gmb_gfx_line_f (lround).
    /// EmitInt32Arg troncava e divergeva da C99Native con coordinate frazionarie.</summary>
    private void EmitCoordArg(Expr e)
    {
        EmitExpression(e);
        _il.Emit(OpCodes.Conv_R8);
        _il.Emit(OpCodes.Ldc_I4_1);   /* MidpointRounding.AwayFromZero */
        _il.Emit(OpCodes.Call, IlRuntimeMathRoundMethod);
        _il.Emit(OpCodes.Conv_I4);
    }

    private void EmitLineStatement(LineStmt stmt)
    {
        EmitLoadGraphicsService();
        if (stmt.X1 is not null) EmitCoordArg(stmt.X1); else _il.Emit(OpCodes.Ldc_I4_M1);
        if (stmt.Y1 is not null) EmitCoordArg(stmt.Y1); else _il.Emit(OpCodes.Ldc_I4_M1);
        EmitCoordArg(stmt.X2); EmitCoordArg(stmt.Y2);
        if (stmt.Color is not null)
        {
            // Colore RGBA (RGB(r,g,b) → double[]) → colore diretto (flag 0x40000000 | RGB);
            // numerico → indice palette. Supporta QUALSIASI colore per la palette RGB.
            if (NormalizeInferredType(InferType(stmt.Color)) is RgbaColorType or DisplayModeType)
            {
                EmitExpression(stmt.Color);
                _il.Emit(OpCodes.Call, IlRuntimeRgbaToLineColorMethod);
            }
            else EmitInt32Arg(stmt.Color);
        }
        else _il.Emit(OpCodes.Ldc_I4_0);
        // Stile LINE: "B" (solo bordo) / "BF" (bordo + riempimento) — il parser lo produce
        // come Expr (identificatore BF/B o stringa). Senza stile → null.
        string? styleStr = stmt.Style switch
        {
            StringExpr se => se.Value,
            VariableExpr v when v.NameCleaned.Lexeme.Equals("BF", StringComparison.OrdinalIgnoreCase) => "BF",
            VariableExpr v when v.NameCleaned.Lexeme.Equals("B", StringComparison.OrdinalIgnoreCase) => "B",
            _ => null
        };
        if (styleStr is not null) _il.Emit(OpCodes.Ldstr, styleStr);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, GraphicsLineMethod);
    }

    private void EmitDrawStatement(DrawStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitStringArg(stmt.CommandString);
        _il.Emit(OpCodes.Callvirt, GraphicsDrawMethod);
    }

    private void EmitPaintStatement(PaintStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        if (stmt.FillColor is not null) EmitInt32Arg(stmt.FillColor); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.BorderColor is not null) EmitInt32Arg(stmt.BorderColor); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsPaintMethod);
    }

    private void EmitImageGetStatement(ImageGetStmt stmt)
    {
        // IMAGEGET sincrono (Gfx.IMAGEGET(...).GetResult) BLOCCHEREBBE un worker GPM in
        // silenzio nel path FLAT (come LOAD TEXTURE). Il v2 lo parcheggia; qui il guard vieta
        // il worker: errore esplicito, mai un blocco muto dello scheduler.
        EmitThrowIfWorker("IMAGEGET");
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X1); EmitInt32Arg(stmt.Y1); EmitInt32Arg(stmt.X2); EmitInt32Arg(stmt.Y2);
        _il.Emit(OpCodes.Callvirt, GraphicsImageGetMethod); // → int[] [w,h,palette...]
        // Copia nel target BASIC (Array.GetValue/SetValue converte il tipo elemento).
        switch (stmt.Array)
        {
            case VariableTarget vt:
                EmitLoadVariable(vt.SymbolId);
                _il.Emit(OpCodes.Call, IlRuntimeCopyIntArrayToMethod);
                break;
            case ArrayElementTarget aet:
                EmitLoadVariable(aet.SymbolId);
                _il.Emit(OpCodes.Call, IlRuntimeCopyIntArrayToMethod);
                break;
            default:
                _il.Emit(OpCodes.Pop);
                break;
        }
    }

    private void EmitImagePutStatement(ImagePutStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        // Carica l'array [w,h,palette...] del target.
        switch (stmt.Array)
        {
            case VariableTarget vt: EmitLoadVariable(vt.SymbolId); break;
            case ArrayElementTarget aet: EmitLoadVariable(aet.SymbolId); break;
            default:
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Newarr, typeof(int));
                break;
        }
        if (stmt.Mode is not null) EmitInt32Arg(stmt.Mode); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsImagePutMethod);
    }

    private void EmitDrawTextStatement(DrawTextStmt stmt)
    {
        // DRAW TEXT "testo", x, y [, ALIGN_*] → IGraphicsService.DrawText(text, x, y, align).
        // Il colore è quello corrente (COLOR); rotation/color espliciti non sono ancora
        // gestiti dal backend IL (come in PCodeVM, il testo usa il colore corrente).
        EmitLoadGraphicsService();
        EmitExpression(stmt.Text);
        EmitInt32Arg(stmt.X);
        EmitInt32Arg(stmt.Y);
        _il.Emit(OpCodes.Ldc_R8, (double)(int)stmt.Align);
        _il.Emit(OpCodes.Callvirt, GraphicsDrawTextMethod);
    }

    private void EmitPolygonStatement(PolygonStmt stmt)
    {
        // POLYGON arrayPunti(), colore → delega a IGraphicsService.Polygon(float[], int).
        // I punti (Target → array nativo) sono letti a runtime da IlRuntime.ReadPolygonPoints.
        if (stmt.PointsArray is not VariableTarget vt)
            throw new NotSupportedException("Backend IL: POLYGON richiede un array di punti (variabile).");
        EmitLoadGraphicsService();
        EmitLoadVariable(vt.SymbolId);
        _il.Emit(OpCodes.Call, IlRuntimeReadPolygonPointsMethod);
        EmitInt32Arg(stmt.Color);
        _il.Emit(OpCodes.Callvirt, GraphicsPolygonMethod);
    }

    // ─── Console grafica / schermo / texture / immagine (IGraphicsService) ───
    private void EmitCClsStatement(CCLSStmt stmt)
    {
        EmitLoadGraphicsService();
        _il.Emit(OpCodes.Callvirt, GraphicsCClsMethod);
    }

    private void EmitCPrintStatement(CPrintStmt stmt)
    {
        // Stampa grafica: concatena gli argomenti (ToString) e delega a IGraphicsService.CPrint.
        EmitLoadGraphicsService();
        // Accumula la stringa concatenando ogni espressione via IlRuntime.ObjToStr.
        for (int i = 0; i < stmt.Arguments.Length; i++)
        {
            var a = stmt.Arguments[i];
            if (a.Expression is null) { _il.Emit(OpCodes.Ldstr, a.Separator ?? ""); }
            else { EmitExpression(a.Expression); _il.Emit(OpCodes.Call, IlRuntimeObjToStrMethod); }
            if (i > 0) _il.Emit(OpCodes.Call, IlRuntimeStringConcatMethod);
        }
        if (stmt.Arguments.Length == 0) _il.Emit(OpCodes.Ldstr, "");
        // Terminatore come PRINT (parita' PCode): CPRINT "x" manda a capo,
        // con ',' produce un TAB, con ';' (o altro) nessun terminatore.
        {
            var cpLast = stmt.Arguments.Length > 0 ? stmt.Arguments[^1].Separator : null;
            if (cpLast == ",") { _il.Emit(OpCodes.Ldstr, "\t"); _il.Emit(OpCodes.Call, IlRuntimeStringConcatMethod); }
            else if (cpLast == null || cpLast == "\n") { _il.Emit(OpCodes.Ldstr, "\n"); _il.Emit(OpCodes.Call, IlRuntimeStringConcatMethod); }
        }
        _il.Emit(OpCodes.Callvirt, GraphicsCPrintMethod);
    }

    private void EmitCLocateStatement(CLocateStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.LineExpr);
        EmitInt32Arg(stmt.ColumnExpr);
        _il.Emit(OpCodes.Callvirt, GraphicsCLocateMethod);
    }

    private void EmitScreenSetStatement(ScreenSetStmt stmt)
    {
        // SCREENSET → IGraphicsService.ScreenSet(width, height, bpp): i due campi AST
        // sono (visible, active); l'interfaccia espone (width, height, bpp). Passiamo
        // i due campi e bpp=0 (default).
        EmitLoadGraphicsService();
        if (stmt.VisiblePage is not null) EmitInt32Arg(stmt.VisiblePage); else _il.Emit(OpCodes.Ldc_I4_0);
        if (stmt.ActivePage is not null) EmitInt32Arg(stmt.ActivePage); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsScreenSetMethod);
    }

    private void EmitPointStatement(PointStmt stmt)
    {
        // gmo SysCall.POINT legge il pixel (non PSET). Senza GPM non c'è PCodeTask:
        // il percorso sincrono lancia (SdlGraphicsService.Point).
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X);
        EmitInt32Arg(stmt.Y);
        _il.Emit(OpCodes.Callvirt, GraphicsPointMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitSyncStatement(SyncStmt stmt)
    {
        // SYNC + consegna/prelievo delle collisioni sprite (ON SPRITE COLLISION), come TrySyncPark nello stepper.
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SyncWithCollisions)));
    }

    private void EmitLoadTextureStatement(LoadTextureStmt stmt)
    {
        // LOAD TEXTURE file INTO id: carica (GraphicsService.LoadTexture) e assegna il
        // risultato (id int) alla variabile destination via IlRuntime.LoadTextureValue.
        if (stmt.Destination is not VariableTarget vt)
            throw new NotSupportedException("Backend IL: LOAD TEXTURE richiede un target variabile.");
        EmitThrowIfWorker("LOAD TEXTURE");
        EmitLoadGraphicsService();
        EmitStringArg(stmt.FileName);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsLoadTextureMethod); // IGraphicsService.LoadTexture(string,int) → int
        // Il servizio restituisce l'id caricato (sincrono): lo salva nella destination.
        // Per gli ARRAY (LOAD TEXTURE INTO SpriteIDs, CELL(...)) il valore di ritorno non
        // viene salvato (sovrascriverebbe il riferimento): va consumato con Pop per non
        // lasciare lo stack sbilanciato → IL invalido.
        if (!_arrays.ContainsKey(vt.SymbolId))
            EmitStoreVariable(vt.SymbolId, () => { });
        else
            _il.Emit(OpCodes.Pop);
    }

    private void EmitDrawTextureStatement(DrawTextureStmt stmt)
    {
        // IlRuntime.DrawTexture(object,...) usa Current.Graphics internamente; ogni argomento
        // va BOXATO (i parametri sono object). NON spingere il service.
        EmitExpression(stmt.TextureId);
        var t0 = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.TextureId)), _types, _classTypes);
        if (t0.IsValueType) _il.Emit(OpCodes.Box, t0);
        EmitExpression(stmt.X);
        var t1 = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.X)), _types, _classTypes);
        if (t1.IsValueType) _il.Emit(OpCodes.Box, t1);
        EmitExpression(stmt.Y);
        var t2 = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Y)), _types, _classTypes);
        if (t2.IsValueType) _il.Emit(OpCodes.Box, t2);
        EmitExprBoxed(stmt.Rotation);
        EmitExprBoxed(stmt.ScaleX);
        EmitExprBoxed(stmt.ScaleY);
        _il.Emit(OpCodes.Call, IlRuntimeDrawTextureMethod);
    }

    private void EmitExprBoxed(Expr? e)
    {
        if (e is null) { _il.Emit(OpCodes.Ldnull); return; }
        EmitExpression(e);
        var t = DataTypeMapper.Map(NormalizeInferredType(InferType(e)), _types, _classTypes);
        if (t.IsValueType) _il.Emit(OpCodes.Box, t);
    }

    private void EmitDrawTexturesStatement(DrawTexturesStmt stmt)
    {
        // Modalità Oggetto-Sprite (DRAW TEXTURES sprite_object_array()): legge ogni oggetto
        // SpriteInfo via IlRuntime.DrawTexturesObjects e disegna l'istanza. Prima era NO-OP
        // ("TODO(gfx)") → la demo4 disegnava sempre nero.
        if (stmt.SpritesArray is not null)
        {
            if (stmt.SpritesArray is VariableTarget vtSprite)
                EmitLoadVariable(vtSprite.SymbolId);
            else
                _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Call, IlRuntimeDrawTexturesObjectsMethod);
            return;
        }
        // Modalità Array Paralleli (legacy): TEXTURE id, x(), y(), rot(), scale()
        if (stmt.TextureId is null)
            throw new NotSupportedException("Backend IL: DRAW TEXTURES richiede la modalità array paralleli (TextureId).");
        // IlRuntime.DrawTextures(int, double[], ...). Il texture id è passato BOXED a
        // Convert.ToInt32(object): con l'argomento int/double nudo l'ottimizzatore ILOptim
        // può coalescere lo slot a int32 mentre la call Convert::ToInt32(double) resta
        // nell'IL → InvalidProgram (GraphicsSpriteBatch, 2026-10-03). Col boxed object il
        // tipo dello slot non entra più nel confronto con la firma.
        EmitObjectBoxedArgument(stmt.TextureId);
        _il.Emit(OpCodes.Call, typeof(System.Convert).GetMethod(nameof(System.Convert.ToInt32), [typeof(object)])!);
        EmitTargetArrayArg(stmt.XArray);
        EmitTargetArrayArg(stmt.YArray);
        EmitTargetArrayArg(stmt.RotArray);
        EmitTargetArrayArg(stmt.ScaleArray);
        _il.Emit(OpCodes.Call, IlRuntimeDrawTexturesMethod);
    }

    private void EmitTargetArrayArg(Target? target)
    {
        if (target is VariableTarget vt)
        {
            EmitLoadVariable(vt.SymbolId);
        }
        else
        {
            throw new NotSupportedException("Backend IL: array parallelo grafico richiede una variabile.");
        }
    }

    private void EmitImagePutsStatement(ImagePutsStmt stmt)
    {
        if (stmt.SpriteArray is not VariableTarget svt || stmt.PositionsArray is not VariableTarget pvt)
            throw new NotSupportedException("Backend IL: IMAGEPUTS richiede array di variabili.");
        EmitLoadGraphicsService();
        EmitLoadVariable(svt.SymbolId);
        _il.Emit(OpCodes.Call, IlRuntimeReadIntArrayMethod);
        EmitLoadVariable(pvt.SymbolId);
        _il.Emit(OpCodes.Call, IlRuntimeReadIntArrayMethod);
        if (stmt.Mode is not null) EmitInt32Arg(stmt.Mode); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, GraphicsImagePutsMethod);
    }

    private void EmitViewStatement(ViewStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X1); EmitInt32Arg(stmt.Y1); EmitInt32Arg(stmt.X2); EmitInt32Arg(stmt.Y2);
        _il.Emit(OpCodes.Callvirt, GraphicsViewPortMethod);
    }

    private void EmitBSaveStatement(BSaveStmt stmt)
    {
        // GRAPHICS.BSAVE(x1, y1, x2, y2 [, buffer]) → IGraphicsService.BSave(int,int,int,int).
        // Il buffer BASIC è IGNORATO: l'area viene salvata in un buffer interno del manager
        // (il byte[] come argomento object generava IL invalido nel backend).
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X1); EmitInt32Arg(stmt.Y1); EmitInt32Arg(stmt.X2); EmitInt32Arg(stmt.Y2);
        _il.Emit(OpCodes.Callvirt, GraphicsBSaveMethod);
    }

    private void EmitBLoadStatement(BLoadStmt stmt)
    {
        // GRAPHICS.BLOAD(x, y [, buffer]) → IGraphicsService.BLoad(int,int).
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.X); EmitInt32Arg(stmt.Y);
        _il.Emit(OpCodes.Callvirt, GraphicsBLoadMethod);
    }

    private void EmitCSaveScreenStatement(CSaveScreenStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitStringArg(stmt.Name);
        _il.Emit(OpCodes.Callvirt, GraphicsCSaveScreenMethod);
    }

    private void EmitCRestoreScreenStatement(CRestoreScreenStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitStringArg(stmt.Name);
        _il.Emit(OpCodes.Callvirt, GraphicsCRestoreScreenMethod);
    }

    private void EmitGraphicsPrintExprStatement(GraphicsPrintExprStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitExpression(stmt.Expression);
        _il.Emit(OpCodes.Call, IlRuntimeObjToStrMethod);
        _il.Emit(OpCodes.Callvirt, GraphicsCPrintMethod);
    }

    private void EmitGraphicsNewlineStatement(GraphicsNewlineStmt stmt)
    {
        EmitLoadGraphicsService();
        _il.Emit(OpCodes.Callvirt, GraphicsCPrintNewLineMethod);
    }

    private void EmitGraphicsTabStatement(GraphicsTabStmt stmt)
    {
        // TAB(col) nel PRINT grafico → IGraphicsService.GraphicsTab(col) (cursore X=col).
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.ColumnExpression);
        _il.Emit(OpCodes.Callvirt, GraphicsTabMethod);
    }

    private void EmitGraphicsTabZoneStatement(GraphicsTabZoneStmt stmt)
    {
        // Zona tab (',') nel PRINT grafico → avanza alla zona successiva.
        EmitLoadGraphicsService();
        _il.Emit(OpCodes.Callvirt, GraphicsTabZoneMethod);
    }

    private void EmitSpriteDefStatement(SpritedefStmt stmt)
    {
        // SPRITEDEF id, righe...: costruisce i pixel a COMPILE-TIME dalle righe ASCII e li
        // passa a IGraphicsService.SpriteDef (0 = trasparente, altrimenti 0xRRGGBB).
        // Mappatura CANONICA (parità PCodeVM/executor AST, Interpreter/Gpm.Graphics.cs):
        // '.' e ' ' trasparenti, '0'-'9' rampa di grigi, lettere rampa RGB (A=10, B=11, ...).
        var rows = stmt.Rows;
        int w = stmt.Width, h = stmt.Height;
        if (w <= 0 || h <= 0) return;
        var pixels = new int[w * h];
        for (int y = 0; y < h && y < rows.Length; y++)
            for (int x = 0; x < w && x < rows[y].Length; x++)
            {
                char c = rows[y][x];
                int packed = 0;
                if (c == '.' || c == ' ')
                    packed = 0;
                else if (c >= '0' && c <= '9')
                {
                    int v = (c - '0') * 16 + 15;
                    packed = (v << 16) | (v << 8) | v;
                }
                else
                {
                    int idx = char.ToUpperInvariant(c) - 'A' + 10;
                    packed = ((idx * 8 + 127) << 16) | ((idx * 4 + 63) << 8) | (idx * 2 + 31);
                }
                pixels[y * w + x] = packed;
            }
        EmitLoadGraphicsService();
        EmitInt32Arg(stmt.SpriteId);
        _il.Emit(OpCodes.Ldc_I4, w);
        _il.Emit(OpCodes.Ldc_I4, h);
        EmitIntArrayLiteral(pixels);
        _il.Emit(OpCodes.Callvirt, GraphicsSpriteDefMethod);
    }

    private void EmitIntArrayLiteral(int[] values)
    {
        _il.Emit(OpCodes.Ldc_I4, values.Length);
        _il.Emit(OpCodes.Newarr, typeof(int));
        for (int i = 0; i < values.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Ldc_I4, values[i]);
            _il.Emit(OpCodes.Stelem_I4);
        }
    }

    private void EmitSpriteCommandStatement(SpriteCommandStmt stmt)
    {
        // SPRITE LOAD "file", handleVar → IGraphicsService.SpriteLoad(string,int) → salva l'id
        // nel target (come LOAD TEXTURE INTO). Il target è Arguments[1].
        if (stmt.Command.Kind == TokenKind.SpriteLoad && stmt.Arguments.Length >= 2)
        {
            EmitThrowIfWorker("SPRITE LOAD");
            if (stmt.Arguments[1] is VariableExpr ve)
            {
                EmitLoadGraphicsService();
                EmitStringArg(stmt.Arguments[0]);
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Callvirt, GraphicsSpriteLoadMethod); // → int id
                // SpriteLoad ritorna int32: la variabile target puo' essere DOUBLE (DIM SpA senza tipo) o LONG;
                // senza conversione lo stloc scrive un int32 in un local float64 (IL non valido: ILOptim lo rifiuta).
                {
                    var spTgt = IsGlobalSymbol(ve.SymbolId) ? GlobalClrType(ve.SymbolId)
                        : (_locals.TryGetValue(ve.SymbolId, out var spLb) ? spLb.LocalType : typeof(double));
                    IlConversion.EmitConvert(_il, _types, new DoubleType(), typeof(int), spTgt);
                }
                EmitStoreVariable(ve.SymbolId, () => { });
            }
            else if (stmt.Arguments[1] is ArrayAccessExpr aae)
            {
                // SPRITE LOAD "f.png", SprID(I): carica l'id (SpriteLoad → int) e salva
                // l'ELEMENTO array. EmitArrayElementStore emette array+indici e poi il
                // callback produce il valore (l'id) da stelem.
                EmitArrayElementStore(
                    new ArrayElementTarget(aae.SymbolId, aae.NameSuffixed, aae.NameCleaned, aae.Indices),
                    () =>
                    {
                        EmitLoadGraphicsService();
                        EmitStringArg(stmt.Arguments[0]);
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Callvirt, GraphicsSpriteLoadMethod);
                    });
            }
            else
            {
                EmitLoadGraphicsService();
                EmitStringArg(stmt.Arguments[0]);
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Callvirt, GraphicsSpriteLoadMethod);
                _il.Emit(OpCodes.Pop);
            }
            return;
        }
        // SPRITE SET POSITION id, x, y → SpriteSetPosition(int, float, float).
        if (stmt.Command.Kind == TokenKind.SpriteSetPosition && stmt.Arguments.Length >= 3)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitFloatArg(stmt.Arguments[1]);
            EmitFloatArg(stmt.Arguments[2]);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetPositionMethod);
            return;
        }
        // SPRITE ON/OFF id → SpriteSetVisible(int, bool).
        if (stmt.Command.Kind is TokenKind.SpriteOn or TokenKind.SpriteOff && stmt.Arguments.Length >= 1)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            _il.Emit(stmt.Command.Kind == TokenKind.SpriteOn ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetVisibleMethod);
            return;
        }
        // SPRITE PRIORITY id, priority → SpriteSetPriority(int, int).
        if (stmt.Command.Kind == TokenKind.SpritePriority && stmt.Arguments.Length >= 2)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitInt32Arg(stmt.Arguments[1]);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetPriorityMethod);
            return;
        }
        if (stmt.Command.Kind == TokenKind.SpriteTransform && stmt.Arguments.Length >= 2)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitExpression(stmt.Arguments[1]);
            _il.Emit(OpCodes.Conv_I8);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetTransformMethod);
            return;
        }
        if (stmt.Command.Kind == TokenKind.SpriteSetMatrix && stmt.Arguments.Length >= 2)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitExpression(stmt.Arguments[1]);
            _il.Emit(OpCodes.Conv_I8);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetMatrixMethod);
            return;
        }
        // SPRITE SET SCALE id, sx, sy → IGraphicsService.SpriteSetScale(int, float, float).
        if (stmt.Command.Kind == TokenKind.Scale && stmt.Arguments.Length >= 3)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitFloatArg(stmt.Arguments[1]);
            EmitFloatArg(stmt.Arguments[2]);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetScaleMethod);
            return;
        }
        // SPRITE SET ROTATION id, gradi → IGraphicsService.SpriteSetRotation(int, float).
        if (stmt.Command.Kind == TokenKind.Rotate && stmt.Arguments.Length >= 2)
        {
            EmitLoadGraphicsService();
            EmitInt32Arg(stmt.Arguments[0]);
            EmitFloatArg(stmt.Arguments[1]);
            _il.Emit(OpCodes.Callvirt, GraphicsSpriteSetRotationMethod);
            return;
        }
        // SPRITE GETPIXELS src, dstArray → IGraphicsService.SpriteGetPixels(int) → int[]
        // ([w, h, indice_palette...]) copiato nel target (schema PCodeVM SPRITE_GETPIXELS).
        if (stmt.Command.Kind == TokenKind.SpriteGetPixels && stmt.Arguments.Length >= 2)
        {
            if (stmt.Arguments[1] is VariableExpr gpDst)
            {
                EmitLoadGraphicsService();
                EmitInt32Arg(stmt.Arguments[0]);
                _il.Emit(OpCodes.Callvirt, GraphicsSpriteGetPixelsMethod);
                EmitLoadVariable(gpDst.SymbolId);
                _il.Emit(OpCodes.Call, IlRuntimeCopyIntArrayToMethod);
            }
            else
            {
                // destinazione non variabile: valuta src e scarta.
                EmitExpression(stmt.Arguments[0]);
                _il.Emit(OpCodes.Pop);
            }
            return;
        }
    }

    private void EmitTilemapCommandStatement(TilemapCommandStmt stmt)
    {
        // TILEMAP LOAD/SET/OFFSET/SCROLL/DRAW → IGraphicsService (schema PCodeVM TILEMAP_* →
        // CommonGraphics.TilemapManager). Headless NullGraphicsService → no-op; SDL reale.
        var args = stmt.Arguments;
        Expr? A(int i) => i < args.Length ? args[i] : null;
        switch (stmt.Command.Kind)
        {
            case TokenKind.TilemapLoad when A(0) != null && A(6) != null:
                // TILEMAP LOAD texId, tileW, tileH, tilesPerRow, mapW, mapH, tiles()
                EmitLoadGraphicsService();
                EmitInt32Arg(A(0)!); EmitInt32Arg(A(1)!); EmitInt32Arg(A(2)!); EmitInt32Arg(A(3)!); EmitInt32Arg(A(4)!); EmitInt32Arg(A(5)!);
                EmitObjectBoxedArgument(A(6)!);
                _il.Emit(OpCodes.Call, IlRuntimeArrayToIntArrayMethod);
                _il.Emit(OpCodes.Callvirt, GraphicsTilemapLoadMethod);
                break;
            case TokenKind.TilemapSet when A(0) != null:
                // TILEMAP SET row, col, tileIndex (parità TilemapManager.SetTile(row, col, idx))
                EmitLoadGraphicsService();
                EmitInt32Arg(A(0)!); EmitInt32Arg(A(1)!); EmitInt32Arg(A(2)!);
                _il.Emit(OpCodes.Callvirt, GraphicsTilemapSetMethod);
                break;
            case TokenKind.TilemapOffset when A(0) != null:
                EmitLoadGraphicsService();
                EmitFloatArg(A(0)!); EmitFloatArg(A(1)!);
                _il.Emit(OpCodes.Callvirt, GraphicsTilemapSetOffsetMethod);
                break;
            case TokenKind.TilemapScroll when A(0) != null:
                EmitLoadGraphicsService();
                EmitFloatArg(A(0)!); EmitFloatArg(A(1)!);
                _il.Emit(OpCodes.Callvirt, GraphicsTilemapScrollMethod);
                break;
            case TokenKind.TilemapDraw:
                EmitLoadGraphicsService();
                _il.Emit(OpCodes.Callvirt, GraphicsTilemapDrawMethod);
                break;
            default:
                // sottocomando sconosciuto: valuta gli argomenti (side-effect) e scarta.
                foreach (var a in args) { EmitExpression(a); _il.Emit(OpCodes.Pop); }
                break;
        }
    }

    private void EmitTickStatement(TickStmt stmt)
    {
        // TICK rate: registra l'inizio frame (schema PCodeVM TICK loop).
        _il.Emit(OpCodes.Ldc_I4, stmt.TickRate);
        _il.Emit(OpCodes.Call, IlRuntimeTickBeginMethod);
    }

    private void EmitTickEndStatement(TickEndStmt stmt)
    {
        // END TICK: attende il tempo residuo per mantenere il frame rate target.
        _il.Emit(OpCodes.Ldc_I4, stmt.TickRate);
        _il.Emit(OpCodes.Call, IlRuntimeTickEndMethod);
    }
    private void EmitUnloadResourceStatement(UnloadResourceStmt stmt) { /* headless no-op */ }

    /// <summary>SOUND [ON voice,] freq, dur [, vol ...] → IAudioService.Sound(freq, dur, vol).</summary>
    private void EmitSoundStatement(SoundStmt stmt)
    {
        EmitLoadAudioService();
        EmitDoubleArgument(stmt.Frequency);
        EmitDoubleArgument(stmt.Duration);
        if (stmt.Volume is not null) EmitInt32Arg(stmt.Volume); else _il.Emit(OpCodes.Ldc_I4, 100);
        _il.Emit(OpCodes.Callvirt, AudioSoundMethod);
    }

    private void EmitSetFilterStatement(SetFilterStmt stmt)
    {
        EmitLoadAudioService();
        EmitInt32Arg(stmt.Voice); EmitInt32Arg(stmt.Type);
        EmitDoubleArgument(stmt.Cutoff); EmitDoubleArgument(stmt.Resonance);
        _il.Emit(OpCodes.Callvirt, AudioSetFilterMethod);
    }

    /// <summary>SOUND LOAD "file" AS #handle → assegna l'handle dal servizio.</summary>
    private void EmitSoundLoadStatement(SoundLoadStmt stmt)
    {
        EmitLoadAudioService();
        EmitStringArg(stmt.FilePath);
        _il.Emit(OpCodes.Callvirt, AudioLoadSoundMethod); // → int handle
        if (stmt.Handle is VariableTarget hvt)
        {
            var local = GetOrDeclareLocal(hvt.SymbolId);
            var clr = local.LocalType;
            if (clr == typeof(long)) _il.Emit(OpCodes.Conv_I8);
            else if (clr == typeof(short)) _il.Emit(OpCodes.Conv_I2);
            else if (clr == typeof(double)) _il.Emit(OpCodes.Conv_R8);
            else if (clr == typeof(float)) _il.Emit(OpCodes.Conv_R4);
            _il.Emit(OpCodes.Stloc, local);
        }
        else
        {
            _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitSoundPlayStatement(SoundPlayStmt stmt)
    {
        EmitLoadAudioService();
        EmitInt32Arg(stmt.Handle);
        if (stmt.Channel is not null) EmitInt32Arg(stmt.Channel); else _il.Emit(OpCodes.Ldc_I4, 0);
        if (stmt.Loops is not null) EmitInt32Arg(stmt.Loops); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, AudioPlaySoundMethod);
    }

    private void EmitSoundVolumeStatement(SoundVolumeStmt stmt)
    {
        EmitLoadAudioService();
        EmitInt32Arg(stmt.Volume);
        // SetVolume(int volume, int? channel): il secondo parametro è Nullable<int>.
        if (stmt.Channel is not null)
        {
            EmitInt32Arg(stmt.Channel);
            _il.Emit(OpCodes.Newobj, typeof(int?).GetConstructor([typeof(int)])!);
        }
        else
        {
            var loc = _il.DeclareLocal(typeof(int?));
            _il.Emit(OpCodes.Ldloca, loc);
            _il.Emit(OpCodes.Initobj, typeof(int?));
            _il.Emit(OpCodes.Ldloc, loc);
        }
        _il.Emit(OpCodes.Callvirt, AudioSetVolumeMethod);
    }

    /// <summary>PLAY musicString → IAudioService.Play (schema PCodeVM, senza concorrenza).</summary>
    private void EmitPlayStatement(PlayStmt stmt)
    {
        switch (stmt.Action)
        {
            case StandardPlayAction spa:
            case QueuePlayAction qpa:
                EmitLoadAudioService();
                EmitStringArg((stmt.Action as StandardPlayAction)?.MusicString ?? ((QueuePlayAction)stmt.Action).MusicString);
                _il.Emit(OpCodes.Callvirt, AudioPlayMethod);
                break;
            case NamedPlayAction npa:
            case LoopPlayAction lpa:
            case ConcurrentPlayAction cpa:
                // Varianti PLAY avanzate (nome/loop/concorrenza): riproducono la prima stringa
                // (il backend SdlAudioService.Play gestisce una sequenza singola).
                var first = stmt.Action switch
                {
                    NamedPlayAction n => n.MusicString,
                    LoopPlayAction l => l.MusicString,
                    ConcurrentPlayAction c => c.MusicStrings.IsEmpty ? null : c.MusicStrings[0],
                    _ => null
                };
                if (first is not null)
                {
                    EmitLoadAudioService();
                    EmitStringArg(first);
                    _il.Emit(OpCodes.Callvirt, AudioPlayMethod);
                }
                break;
            case ControlAudioAction _:
                // PLAY STOP/PAUSE/RESUME — non gestito qui (AUDIO control).
                break;
            default:
                break;
        }
    }

    /// <summary>SOUND/PLAY STOP|PAUSE|RESUME|CLEAR [, arg] → IAudioService.AudioControl.
    /// arg può essere numerico o un nome di canale stringa (SOUND STOP "ch1").</summary>
    private void EmitAudioControlStatement(AudioControlStmt stmt)
    {
        int action = stmt.Action.Kind switch
        {
            TokenKind.Stop => 0,
            TokenKind.Pause => 1,
            TokenKind.Resume => 2,
            TokenKind.Clear => 3,
            _ => 0,
        };
        EmitLoadAudioService();
        _il.Emit(OpCodes.Ldc_I4, action);
        if (stmt.Argument is not null) EmitObjectBoxedArgument(stmt.Argument); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, AudioControlMethod);
    }
}
