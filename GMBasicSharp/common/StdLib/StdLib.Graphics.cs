// FILE: Interpreter.StdLib.Graphics.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Graphics — namespace 'GRAPHICS'
 * 15 funzioni | 35 comandi | 4 tipi | 0 costanti
 *
 * FUNZIONI (15):
 *   C2PX
 *   C2PY
 *   CELL
 *   CELLSTR
 *   COLLIDE
 *   COLLIDEBATCH
 *   P2CX
 *   P2CY
 *   Point
 *   RGB
 *   SCREEN
 *   ScreenInfo
 *   Screens
 *   TextHeight
 *   TextWidth
 * COMANDI (35):
 *   BLOAD
 *   BSAVE
 *   CCLS
 *   CIRCLE
 *   CLEAR
 *   CLOCATE
 *   CPALETTE
 *   CPRINT
 *   CRESTORESCREEN
 *   CSAVESCREEN
 *   DRAW
 *   FONT
 *   IMAGEGET
 *   IMAGEPUT
 *   IMAGEPUTS
 *   LINE
 *   LOAD
 *   PAINT
 *   PALETTE
 *   PCOPY
 *   PIXEL
 *   PLAY
 *   POINT
 *   POLYGON
 *   PRESET
 *   PSET
 *   SCREEN
 *   SCREENSET
 *   SOUND
 *   SPRITE
 *   SPRITEDEF
 *   SYNC
 *   TEXTMODE
 *   TILEMAP
 *   VIEW
 * TIPI (4):
 *   DisplayModeType
 *   RGBA
 *   RgbaColorType
 *   SPRITEINFO
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- GRAPHICS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'GRAPHICS'.
    /// </summary>
    private void CreateGraphicsNamespace()
    {
        const string namespaceName = "GRAPHICS";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // I comandi polimorfici come CLS, COLOR, LOCATE, PRINT sono gestiti da CreatePolymorphicCommands().

        RegisterCommand(TokenKind.Load, "LOAD", namespaceName,
            Parser.ParseLoadStatement);

        // Comandi registrati globalmente per la sintassi legacy (es. SPRITE ...)
        RegisterCommand(TokenKind.Sprite, "SPRITE", namespaceName,
            Parser.ParseSpriteStatement);

        // PIXEL BUFFER w, h, buffer (comando legacy, GRAFX-03 — sostituisce la funzione
        // PIXELBUFFER(w,h,px)). Niente forma qualificata (§2.0).
        RegisterCommand(TokenKind.Pixel, "PIXEL", namespaceName,
            Parser.ParsePixelBufferStatement);

        // -- TILEMAP --
        RegisterCommand(TokenKind.Tilemap, "TILEMAP", namespaceName,
            Parser.ParseTilemapStatement);

        // -- SPRITEDEF --
        RegisterCommand(TokenKind.Spritedef, "SPRITEDEF", namespaceName,
            Parser.ParseSpritedefStatement);

        // -- AUDIO COMMANDS --
        RegisterCommand(TokenKind.Sound, "SOUND", namespaceName, Parser.ParseSoundStatement);
        RegisterCommand(TokenKind.Play, "PLAY", namespaceName, Parser.ParsePlayStatement);

        // I comandi GRAPHICS sono SOLO legacy (sintassi spazio: PSET, LINE, CIRCLE, ...).
        // Non esiste sintassi qualificata per i comandi GRAPHICS (GRAPHICS.PSET non e' valida).
        // I comandi polimorfici (CLS/COLOR/LOCATE/PRINT) restano gestiti dai dispatcher
        // polimorfici in CONSOLE. Le funzioni che restituiscono valore restano qualificate.
        // Comandi Specifici di GRAPHICS - Puntano ai parser statici
        RegisterCommand(TokenKind.Screen, "SCREEN", namespaceName, Parser.ParseScreenStatement);
        RegisterCommand(TokenKind.TextMode, "TEXTMODE", namespaceName, Parser.ParseTextModeStatement);
        RegisterCommand(TokenKind.Palette, "PALETTE", namespaceName, Parser.ParsePaletteStatement);

        // Varianti FORZATE console (2026-08-06, spostate da CONSOLE): con lo screen grafico
        // attivo servono a forzare il comportamento console. I comandi polimorfici/ibridi
        // senza C (CLS/COLOR/LOCATE/PRINT/SAVESCREEN/RESTORESCREEN) stanno in CONSOLE.
        RegisterCommand(TokenKind.CCLS, "CCLS", namespaceName, Parser.ParseCCLSStatement);
        RegisterCommand(TokenKind.CLocate, "CLOCATE", namespaceName, Parser.ParseCLocateStatement);
        RegisterCommand(TokenKind.CPrint, "CPRINT", namespaceName, Parser.ParseCPrintStatement);
        RegisterCommand(TokenKind.CSaveScreen, "CSAVESCREEN", namespaceName, Parser.ParseCSaveScreenStatement);
        RegisterCommand(TokenKind.CRestoreScreen, "CRESTORESCREEN", namespaceName, Parser.ParseCRestoreScreenStatement);
        RegisterCommand(TokenKind.CPalette, "CPALETTE", namespaceName, Parser.ParseCPaletteStatement);
        RegisterCommand(TokenKind.Sync, "SYNC", namespaceName, Parser.ParseSyncStatement);
        RegisterCommand(TokenKind.PSet, "PSET", namespaceName, Parser.ParsePSetStatement);
        RegisterCommand(TokenKind.Preset, "PRESET", namespaceName, Parser.ParsePresetStatement);
        RegisterCommand(TokenKind.Line, "LINE", namespaceName, Parser.ParseLineStatement);
        RegisterCommand(TokenKind.Circle, "CIRCLE", namespaceName, Parser.ParseCircleStatement);
        RegisterCommand(TokenKind.Paint, "PAINT", namespaceName, Parser.ParsePaintStatement);
        RegisterCommand(TokenKind.Draw, "DRAW", namespaceName, Parser.ParseDrawCommandStatement);
        RegisterCommand(TokenKind.Font, "FONT", namespaceName, Parser.ParseFontStatement);
        RegisterCommand(TokenKind.PCopy, "PCOPY", namespaceName, Parser.ParsePCopyStatement);
        RegisterCommand(TokenKind.ScreenSet, "SCREENSET", namespaceName, Parser.ParseScreenSetStatement);
        RegisterCommand(TokenKind.ImageGet, "IMAGEGET", namespaceName, Parser.ParseImageGetStatement);
        RegisterCommand(TokenKind.ImagePut, "IMAGEPUT", namespaceName, Parser.ParseImagePutStatement);
        RegisterCommand(TokenKind.ImagePuts, "IMAGEPUTS", namespaceName, Parser.ParseImagePutsStatement);
        // SAVESCREEN/RESTORESCREEN spostati nel namespace CONSOLE (polimorfici: console buffer
        // oppure, con schermo grafico attivo, texture via GraphicsManager). Per la grafica
        // forzata restano CSAVESCREEN/CRESTORESCREEN (varianti C*).
        RegisterCommand(TokenKind.BSave, "BSAVE", namespaceName, Parser.ParseBSaveStatement);
        RegisterCommand(TokenKind.BLoad, "BLOAD", namespaceName, Parser.ParseBLoadStatement);
        RegisterCommand(TokenKind.Point, "POINT", namespaceName, Parser.ParsePointStatement);
        RegisterCommand(TokenKind.Clear, "CLEAR", namespaceName, Parser.ParseClearRectStatement);

        RegisterCommand(TokenKind.View, "VIEW", namespaceName, Parser.ParseViewStatement);

        RegisterCommand(TokenKind.Polygon, "POLYGON", namespaceName, Parser.ParsePolygonStatement);

        // --- DEFINIZIONE DELLE FUNZIONI NATIVE ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Point, "Point"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new IntegerType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.Point),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cell, "CELL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "row"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "column"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new IntegerType(), noFlags, null, false)
            ], new LongType(), TokenKind.Cell),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CellStr, "CELLSTR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "row"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "column"), new IntegerType(), noFlags, null, false)
            ], new StringType(), TokenKind.CellStr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Screens, "Screens"), [], new InferredType(),
                TokenKind.Screens),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ScreenInfo, "ScreenInfo"), [],
                new DisplayModeType(),
                TokenKind.ScreenInfo),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Screen, "SCREEN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new IntegerType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y"), new IntegerType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.Screen), // Funzione SCREEN() modalità console, SCREEN(x,y) pixel
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TextWidth, "TextWidth"),
                [new Parameter(-1, T(TokenKind.Identifier, "text"), new StringType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TextWidth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TextHeight, "TextHeight"), [], new DoubleType(),
                TokenKind.TextHeight),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Collide, "COLLIDE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "id1"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "id2"), new IntegerType(), optionalFlag, null, false)
            ], new InferredType(), TokenKind.Collide),
new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CollideBatch, "COLLIDEBATCH"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new ArrayType(new InferredType()), SymbolFlags.IsArray, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y"), new ArrayType(new InferredType()), SymbolFlags.IsArray, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "w"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "h"), new DoubleType(), noFlags, null, false)
            ], new ArrayType(new InferredType()), TokenKind.CollideBatch),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Rgb, "RGB"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "red"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "green"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "blue"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "alpha"), new IntegerType(), optionalFlag, null, false)
            ], new RgbaColorType(), TokenKind.Rgb),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.C2PX, "C2PX"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "col"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "size"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.C2PX),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.C2PY, "C2PY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "row"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "size"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.C2PY),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.P2CX, "P2CX"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "size"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.P2CX),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.P2CY, "P2CY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "size"), new DoubleType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.P2CY),
        };

        // --- DEFINIZIONE BUILT-IN DEL TIPO SPRITE ---
        var spriteFields = ImmutableArray.CreateBuilder<TypeField>();
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "TextureID"), new LongType(), [], null));
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "X"), new DoubleType(), [], null));
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Y"), new DoubleType(), [], null));
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Rotation"), new DoubleType(), [], null));
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Scale"), new DoubleType(), [], null));
        spriteFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Color"), new LongType(), [], null));
        
        var spriteUdt = new UserDefinedType(
            -1,
            T(TokenKind.Identifier, "SPRITEINFO"),
            spriteFields.ToImmutable(),
            SymbolFlags.IsPublic | SymbolFlags.IsStruct
        );
        
        // Registrazione dei tipi di dato del namespace
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "DISPLAYMODE", new DisplayModeType() },
            { "RGBA", new RgbaColorType() },
            { "SPRITEINFO", spriteUdt }
        };

        RegisterTypeSchema(typeof(DisplayModeType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ModeID", new LongType() },
            { "Width", new LongType() },
            { "Height", new LongType() },
            { "Bpp", new LongType() },
            { "Colors", new LongType() },
            { "Active", new BitType() },
            { "Description", new StringType() }
        });

        RegisterTypeSchema(typeof(RgbaColorType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Red", new IntegerType() },
            { "Green", new IntegerType() },
            { "Blue", new IntegerType() },
            { "Alpha", new IntegerType() }
        });

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
