// FILE: Interpreter.Ast.Graphics.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Tipi di eventi finestra supportati.
/// </summary>
public enum WindowEventKind
{
    Close,
    Resize,
    Quit,
    Move,
    Focus,
    Minimize,
    Restore,
    Open,
    Paste
}

/// <summary>
/// Rappresenta l'istruzione CIRCLE per disegnare un cerchio, arco o ellisse.
/// </summary>
public sealed record CircleStmt(Expr X, Expr Y, Expr Radius, Expr? Color, Expr? Start, Expr? End, Expr? Aspect, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y}), {Radius}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CircleStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CircleStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CircleStmt(
            (Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Expr)Radius.DeepClone(),
            (Expr?)Color?.DeepClone(), (Expr?)Start?.DeepClone(), (Expr?)End?.DeepClone(), (Expr?)Aspect?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione DRAW per disegnare figure complesse.
/// </summary>
public sealed record DrawStmt(Expr CommandString, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => CommandString.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo DrawStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DrawStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DrawStmt((Expr)CommandString.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione FONT per impostare il font di testo grafico.
/// </summary>
public sealed record FontStmt(Expr FontPath, Expr Size, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{FontPath}, {Size}";

    /// <summary>
    /// Crea una copia profonda di questo nodo FontStmt.
    /// </summary>
    /// <returns>Una nuova istanza di FontStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new FontStmt((Expr)FontPath.DeepClone(), (Expr)Size.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione LINE per disegnare linee o rettangoli.
/// </summary>
public sealed record LineStmt(Expr? X1, Expr? Y1, Expr X2, Expr Y2, Expr? Color, Expr? Style, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X1}, {Y1})-({X2}, {Y2})";

    /// <summary>
    /// Crea una copia profonda di questo nodo LineStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LineStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LineStmt(
            (Expr?)X1?.DeepClone(), (Expr?)Y1?.DeepClone(),
            (Expr)X2.DeepClone(), (Expr)Y2.DeepClone(),
            (Expr?)Color?.DeepClone(), (Expr?)Style?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione PAINT per riempire un'area grafica.
/// </summary>
public sealed record PaintStmt(Expr X, Expr Y, Expr? FillColor, Expr? BorderColor, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y})";

    /// <summary>
    /// Crea una copia profonda di questo nodo PaintStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PaintStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PaintStmt(
            (Expr)X.DeepClone(), (Expr)Y.DeepClone(),
            (Expr?)FillColor?.DeepClone(), (Expr?)BorderColor?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione PCOPY per copiare pagine grafiche.
/// </summary>
public sealed record PCopyStmt(Expr SourcePage, Expr DestinationPage, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{SourcePage}, {DestinationPage}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PCopyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PCopyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PCopyStmt((Expr)SourcePage.DeepClone(), (Expr)DestinationPage.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione PRESET per cancellare un pixel.
/// </summary>
public sealed record PresetStmt(Expr X, Expr Y, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y})";

    /// <summary>
    /// Crea una copia profonda di questo nodo PresetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PresetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PresetStmt((Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione PSET per disegnare un pixel.
/// </summary>
public sealed record PSetStmt(Expr X, Expr Y, Expr? Color, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y}){(Color is not null ? $", {Color}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PSetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PSetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PSetStmt((Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Expr?)Color?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione SCREEN per impostare la modalità grafica.
/// </summary>
public sealed record ScreenStmt(Expr Mode, Expr? ActivePage, Expr? VisiblePage, Expr? CloseRepl, Token Token, Expr? WindowModeExpr = null,
    int CoordMode = 0)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Spazio coordinate DICHIARATO per i controlli (`SCREEN ... USE CELLS|PIXELS`):
    /// 0 = AUTO (nessuna clausola: pixel in grafica, celle in TUI), 1 = CELLE, 2 = PIXEL.
    /// </summary>
    public int CoordModeValue => CoordMode;

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Mode.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo ScreenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ScreenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ScreenStmt(
            (Expr)Mode.DeepClone(), (Expr?)ActivePage?.DeepClone(), (Expr?)VisiblePage?.DeepClone(), (Expr?)CloseRepl?.DeepClone(),
            (Token)Token.DeepClone(), (Expr?)WindowModeExpr?.DeepClone(), CoordMode
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione TEXTMODE per selezionare la modalita' console
/// (0=%SDL, 1=%C64, 2=%ANSI, 3=%SYSTEM).
/// </summary>
public sealed record TextModeStmt(Expr Mode, Token Token) : Stmt(Token.Position)
{
    /// <summary>Rappresentazione testuale compatta del nodo.</summary>
    public override string ToString() => Mode.ToString();

    /// <summary>Crea una copia profonda di questo nodo TextModeStmt.</summary>
    public override AstNode DeepClone() =>
        new TextModeStmt((Expr)Mode.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione SCREENSET per cambiare le pagine grafiche.
/// </summary>
public sealed record ScreenSetStmt(Expr? VisiblePage, Expr? ActivePage, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{VisiblePage}, {ActivePage}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ScreenSetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ScreenSetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ScreenSetStmt((Expr?)VisiblePage?.DeepClone(), (Expr?)ActivePage?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// PIXEL BUFFER w, h, buffer — comando legacy (GRAFX-03, AGENTS.md §1/§2.0: nessuna
/// parentesi, nessuna forma qualificata): disegna una texture da un array di pixel.
/// Sostituisce la forma funzione <c>PIXELBUFFER(w, h, px)</c>.
/// </summary>
public sealed record PixelBufferStmt(Expr Width, Expr Height, Expr Buffer, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"PIXEL BUFFER {Width}, {Height}, {Buffer}";

    public override AstNode DeepClone()
    {
        return new PixelBufferStmt((Expr)Width.DeepClone(), (Expr)Height.DeepClone(), (Expr)Buffer.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione IMAGEGET per catturare un'immagine (sprite) dallo schermo.
/// </summary>
public sealed record ImageGetStmt(Expr X1, Expr Y1, Expr X2, Expr Y2, Target Array, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X1}, {Y1})-({X2}, {Y2}), {Array}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ImageGetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ImageGetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ImageGetStmt(
            (Expr)X1.DeepClone(), (Expr)Y1.DeepClone(), (Expr)X2.DeepClone(), (Expr)Y2.DeepClone(),
            (Target)Array.DeepClone(), (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione IMAGEPUT per disegnare un'immagine (sprite) sullo schermo.
/// </summary>
public sealed record ImagePutStmt(Expr X, Expr Y, Target Array, Expr? Mode, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y}), {Array}{(Mode is not null ? $", {Mode}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ImagePutStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ImagePutStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ImagePutStmt(
            (Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Target)Array.DeepClone(), (Expr?)Mode?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione SAVE SCREEN per archiviare lo stato di uno schermo grafico.
/// </summary>
public sealed record SaveScreenStmt(Expr Name, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Name.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo SaveScreenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SaveScreenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SaveScreenStmt((Expr)Name.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione RESTORE SCREEN per ripristinare uno schermo grafico salvato.
/// </summary>
public sealed record RestoreScreenStmt(Expr Name, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Name.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo RestoreScreenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RestoreScreenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new RestoreScreenStmt((Expr)Name.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione POINT per disegnare un punto (pixel o carattere).
/// </summary>
public sealed record PointStmt(Expr X, Expr Y, Expr? Color, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y}){(Color is not null ? $", {Color}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PointStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PointStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PointStmt((Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Expr?)Color?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione BSAVE (x1,y1)-(x2,y2), buffer$ per salvare un'area dello schermo.
/// </summary>
public sealed record BSaveStmt(Expr X1, Expr Y1, Expr X2, Expr Y2, Target Buffer, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X1}, {Y1})-({X2}, {Y2}), {Buffer}";

    /// <summary>
    /// Crea una copia profonda di questo nodo BSaveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di BSaveStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new BSaveStmt(
            (Expr)X1.DeepClone(), (Expr)Y1.DeepClone(), (Expr)X2.DeepClone(), (Expr)Y2.DeepClone(),
            (Target)Buffer.DeepClone(), (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione BLOAD (x,y), buffer$ per disegnare un'area di schermo da una variabile.
/// </summary>
public sealed record BLoadStmt(Expr X, Expr Y, Expr Buffer, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X}, {Y}), {Buffer}";

    /// <summary>
    /// Crea una copia profonda di questo nodo BLoadStmt.
    /// </summary>
    /// <returns>Una nuova istanza di BLoadStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new BLoadStmt((Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Expr)Buffer.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione PALETTE per modificare un colore nella tavolozza grafica.
/// </summary>
public sealed record PaletteStmt(Expr Index, Expr R, Expr G, Expr B, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Index}, {R}, {G}, {B}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PaletteStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PaletteStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new PaletteStmt(
            (Expr)Index.DeepClone(), (Expr)R.DeepClone(), (Expr)G.DeepClone(), (Expr)B.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// PALETTE USE &quot;name&quot; — attiva una palette registrata per nome.
/// </summary>
public sealed record PaletteUseStmt(Expr Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"USE {Name}";
    public override AstNode DeepClone() => new PaletteUseStmt((Expr)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// PALETTE DEFINE &quot;name&quot;, array — definisce una nuova palette.
/// </summary>
public sealed record PaletteDefineStmt(Expr Name, Expr Colors, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"DEFINE {Name}";
    public override AstNode DeepClone() => new PaletteDefineStmt((Expr)Name.DeepClone(), (Expr)Colors.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// PALETTE REPLACE &quot;name&quot;, array — sostituisce una palette esistente.
/// </summary>
public sealed record PaletteReplaceStmt(Expr Name, Expr Colors, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"REPLACE {Name}";
    public override AstNode DeepClone() => new PaletteReplaceStmt((Expr)Name.DeepClone(), (Expr)Colors.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// PALETTE SAVE &quot;file&quot; — salva la palette attuale su file JSON.
/// </summary>
public sealed record PaletteSaveStmt(Expr FileName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SAVE {FileName}";
    public override AstNode DeepClone() => new PaletteSaveStmt((Expr)FileName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// PALETTE LOAD &quot;file&quot; — carica una palette da file JSON.
/// </summary>
public sealed record PaletteLoadStmt(Expr FileName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"LOAD {FileName}";
    public override AstNode DeepClone() => new PaletteLoadStmt((Expr)FileName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione IMAGEPUTS per il disegno in batch.
/// </summary>
public sealed record ImagePutsStmt(Target SpriteArray, Target PositionsArray, Expr? Mode, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{SpriteArray}, {PositionsArray}{(Mode is not null ? $", {Mode}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ImagePutsStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ImagePutsStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ImagePutsStmt(
            (Target)SpriteArray.DeepClone(), (Target)PositionsArray.DeepClone(), (Expr?)Mode?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando per caricare un'immagine in una texture accelerata.
/// Gestisce sia il caricamento singolo che quello di uno spritesheet.
/// </summary>
public sealed record LoadTextureStmt(
    Target Destination, 
    Expr FileName, 
    Expr? CellWidth, 
    Expr? CellHeight, 
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        if (CellWidth != null && CellHeight != null)
        {
            return $"LOAD TEXTURE {FileName} INTO {Destination} CELL({CellWidth}, {CellHeight})";
        }
        return $"LOAD TEXTURE {FileName} INTO {Destination}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo LoadTextureStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LoadTextureStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LoadTextureStmt(
            (Target)Destination.DeepClone(), 
            (Expr)FileName.DeepClone(),
            (Expr?)CellWidth?.DeepClone(),
            (Expr?)CellHeight?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `DRAW TEXTURES` per il rendering in batch.
/// Sintassi: DRAW TEXTURES textureID, x_array, y_array, rot_array, scale_array
/// </summary>
[JsonDerivedType(typeof(DrawTexturesStmt), nameof(DrawTexturesStmt))]
public sealed record DrawTexturesStmt(
    // Argomenti per entrambe le modalità
    Token Token,
    
    // Modalità Oggetto-Sprite (nuova)
    Target? SpritesArray,

    // Modalità Array Paralleli (legacy)
    Expr? TextureId,
    Target? XArray,
    Target? YArray,
    Target? RotArray,
    Target? ScaleArray
) : Stmt(Token.Position)
{
    public override string ToString()
    {
        if (SpritesArray != null)
        {
            return $"DRAW TEXTURES {SpritesArray}";
        }
        return $"DRAW TEXTURES {TextureId}, ...";
    }

    public override AstNode DeepClone()
    {
        return new DrawTexturesStmt(
            (Token)Token.DeepClone(),
            (Target?)SpritesArray?.DeepClone(),
            (Expr?)TextureId?.DeepClone(),
            (Target?)XArray?.DeepClone(),
            (Target?)YArray?.DeepClone(),
            (Target?)RotArray?.DeepClone(),
            (Target?)ScaleArray?.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando per disegnare una texture accelerata a schermo.
/// </summary>
public sealed record DrawTextureStmt(Expr TextureId, Expr X, Expr Y, Expr? Rotation, Expr? ScaleX, Expr? ScaleY, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{TextureId}, ({X}, {Y})";

    /// <summary>
    /// Crea una copia profonda di questo nodo DrawTextureStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DrawTextureStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DrawTextureStmt(
            (Expr)TextureId.DeepClone(), (Expr)X.DeepClone(), (Expr)Y.DeepClone(),
            (Expr?)Rotation?.DeepClone(), (Expr?)ScaleX?.DeepClone(), (Expr?)ScaleY?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta un comando generico che inizia con 'SPRITE'.
/// </summary>
/// <param name="Command">Il token del sottocomando con il Kind specifico (es. SpriteLoad, SpriteSetPosition).</param>
/// <param name="Arguments">La lista di argomenti che seguono il comando.</param>
public sealed record SpriteCommandStmt(Token Command, ImmutableArray<Expr> Arguments, Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SPRITE {Command.Lexeme} {string.Join(", ", Arguments)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SpriteCommandStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SpriteCommandStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new SpriteCommandStmt((Token)Command.DeepClone(), clonedArguments, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Tipo di risorsa grafica da scaricare.
/// </summary>
public enum UnloadResourceKind { Texture, Font }

/// <summary>
/// Rappresenta il comando UNLOAD TEXTURE id / UNLOAD FONT id.
/// Rilascia una risorsa grafica (texture SDL o font TTF) dalla memoria GPU.
/// </summary>
public sealed record UnloadResourceStmt(UnloadResourceKind ResourceKind, Expr Handle, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"UNLOAD {ResourceKind} {Handle}";
    public override AstNode DeepClone() => new UnloadResourceStmt(ResourceKind, (Expr)Handle.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Allineamento testo per DRAW TEXT.
/// </summary>
public enum TextAlign { Left, Center, Right }

/// <summary>
/// Rappresenta il comando DRAW TEXT "string", x, y [, rot] [, align] [, color].
/// </summary>
public sealed record DrawTextStmt(Expr Text, Expr X, Expr Y, Expr? Rotation, TextAlign Align, Expr? Color, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"{Text}, ({X}, {Y})";
    public override AstNode DeepClone() => new DrawTextStmt(
        (Expr)Text.DeepClone(), (Expr)X.DeepClone(), (Expr)Y.DeepClone(),
        (Expr?)Rotation?.DeepClone(), Align, (Expr?)Color?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta il comando `SYNC`.
/// </summary>
public sealed record SyncStmt(Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "SYNC";

    /// <summary>
    /// Crea una copia profonda di questo nodo SyncStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SyncStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new SyncStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Istruzione TICK: registra l'inizio del frame (tempo).
/// Inserita dal parser all'inizio del blocco TICK...END TICK.
/// </summary>
public sealed record TickStmt(int TickRate, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"TICK {TickRate}";

    public override AstNode DeepClone()
    {
        return new TickStmt(TickRate, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Istruzione END TICK: attende il tempo rimanente del frame.
/// Inserita dal parser alla fine del blocco TICK...END TICK.
/// </summary>
public sealed record TickEndStmt(int TickRate, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"END TICK (rate={TickRate})";

    public override AstNode DeepClone()
    {
        return new TickEndStmt(TickRate, (Token)Token.DeepClone());
    }
}

// ========================================================================
// --- NUOVI NODI STMT ATOMICI PER L'ESPANSIONE DINAMICA DEL PRINT GRAFICO ---
// ========================================================================

/// <summary>
/// Istruzione atomica che rappresenta il disegno del risultato di una singola espressione
/// in modalità grafica.
/// </summary>
/// <param name="Expression">L'espressione da valutare e stampare.</param>
public sealed record GraphicsPrintExprStmt(Expr Expression) : Stmt(Expression.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo GraphicsPrintExprStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GraphicsPrintExprStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new GraphicsPrintExprStmt((Expr)Expression.DeepClone());
    }
}

/// <summary>
/// Istruzione atomica che rappresenta l'avanzamento alla prossima "print zone"
/// in modalità grafica (corrisponde al separatore ',').
/// </summary>
/// <param name="Token">Un token di riferimento per la posizione.</param>
public sealed record GraphicsTabZoneStmt(Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo GraphicsTabZoneStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GraphicsTabZoneStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new GraphicsTabZoneStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Istruzione atomica che rappresenta l'avanzamento a una nuova riga
/// in modalità grafica.
/// </summary>
/// <param name="Token">Un token di riferimento per la posizione.</param>
public sealed record GraphicsNewlineStmt(Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo GraphicsNewlineStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GraphicsNewlineStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new GraphicsNewlineStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Istruzione atomica che rappresenta l'esecuzione di una funzione TAB()
/// in modalità grafica.
/// </summary>
/// <param name="ColumnExpression">L'espressione che definisce la colonna di destinazione.</param>
public sealed record GraphicsTabStmt(Expr ColumnExpression)  : Stmt(ColumnExpression.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo GraphicsTabStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GraphicsTabStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new GraphicsTabStmt((Expr)ColumnExpression.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione ON WINDOW evento azione.
/// Esempio: ON WINDOW CLOSE GOSUB Cleanup
/// </summary>
[JsonDerivedType(typeof(OnWindowStmt), nameof(OnWindowStmt))]
public sealed record OnWindowStmt(WindowEventKind EventKind, OnKeyAction Action, Token Token, Expr? Of = null)
    : Stmt(Token.Position)
{
    public override string ToString() => $"ON WINDOW {EventKind.ToString().ToUpper()}{(Of != null ? $" OF {Of}" : "")} {Action}";

    public override AstNode DeepClone()
    {
        return new OnWindowStmt(EventKind, Action.DeepClone(), (Token)Token.DeepClone(), (Expr?)Of?.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione VIEW per definire un viewport grafico.
/// Sintassi: VIEW [SCREEN] (x1, y1)-(x2, y2) [, [color] [, border]]
/// </summary>
[JsonDerivedType(typeof(ViewStmt), nameof(ViewStmt))]
public sealed record ViewStmt(
    bool Screen,
    Expr X1, Expr Y1, Expr X2, Expr Y2,
    Expr? Color,
    Expr? Border,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VIEW {(Screen ? "SCREEN " : "")}({X1}, {Y1})-({X2}, {Y2})";

    public override AstNode DeepClone()
    {
        return new ViewStmt(
            Screen,
            (Expr)X1.DeepClone(), (Expr)Y1.DeepClone(), (Expr)X2.DeepClone(), (Expr)Y2.DeepClone(),
            (Expr?)Color?.DeepClone(), (Expr?)Border?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione WINDOW per definire un sistema di coordinate personalizzato.
/// Sintassi: WINDOW [SCREEN] (x1, y1)-(x2, y2)
/// </summary>
[JsonDerivedType(typeof(ViewPortStmt), nameof(ViewPortStmt))]
public sealed record ViewPortStmt(
    bool Screen,
    Expr X1, Expr Y1, Expr X2, Expr Y2,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {(Screen ? "SCREEN " : "")}({X1}, {Y1})-({X2}, {Y2})";

    public override AstNode DeepClone()
    {
        return new ViewPortStmt(
            Screen,
            (Expr)X1.DeepClone(), (Expr)Y1.DeepClone(), (Expr)X2.DeepClone(), (Expr)Y2.DeepClone(),
            (Token)Token.DeepClone());
    }
}

// Incolla questa nuova classe in fondo al file, insieme agli altri Stmt.
/// <summary>
/// Rappresenta il comando POLYGON per disegnare un poligono pieno.
/// Sintassi: POLYGON points_array, color
/// </summary>
[JsonDerivedType(typeof(PolygonStmt), nameof(PolygonStmt))]
public sealed record PolygonStmt(
    Target PointsArray,
    Expr Color,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"POLYGON {PointsArray}, {Color}";

    public override AstNode DeepClone()
    {
        return new PolygonStmt(
            (Target)PointsArray.DeepClone(),
            (Expr)Color.DeepClone(),
            (Token)Token.DeepClone());
    }
}

// ========================================================================
// --- TILEMAP ENGINE ---
// ========================================================================

[JsonDerivedType(typeof(TilemapCommandStmt), nameof(TilemapCommandStmt))]
public sealed record TilemapCommandStmt(
    Token Command,
    ImmutableArray<Expr> Arguments,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"TILEMAP {Command.Lexeme} ({Arguments.Length} args)";

    public override AstNode DeepClone()
    {
        return new TilemapCommandStmt(
            (Token)Command.DeepClone(),
            Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray(),
            (Token)Token.DeepClone());
    }
}

// ========================================================================
// --- SPRITEDEF ---
// ========================================================================

[JsonDerivedType(typeof(SpritedefStmt), nameof(SpritedefStmt))]
public sealed record SpritedefStmt(
    Expr SpriteId,
    ImmutableArray<string> Rows,
    Token Token) : Stmt(Token.Position)
{
    public int Width => Rows.Length > 0 ? Rows[0].Length : 0;
    public int Height => Rows.Length;

    public override string ToString() => $"SPRITEDEF id={SpriteId} {Width}x{Height}";

    public override AstNode DeepClone()
    {
        return new SpritedefStmt(
            (Expr)SpriteId.DeepClone(),
            [..Rows],
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione VIEW PRINT per limitare l'area di scroll della console.
/// </summary>
public sealed record ViewPrintStmt(Expr? TopLine, Expr? BottomLine, Token Token)
    : Stmt(Token.Position)
{
    public override AstNode DeepClone()
    {
        return new ViewPrintStmt(
            (Expr?)TopLine?.DeepClone(), (Expr?)BottomLine?.DeepClone(), (Token)Token.DeepClone()
        );
    }
}

/// <summary>Stile della cornice della finestra per AST.</summary>
public enum WindowFrameStyle { Double, Single, Rounded }

/// <summary>Rappresenta DEFINE WINDOW name FROM top,left TO bottom,right [TITLE "t"] [DOUBLE] [SINGLE] [ROUNDED] [SHADOW] [PIXEL] [DESKTOP n].</summary>
[JsonDerivedType(typeof(DefineWindowStmt), nameof(DefineWindowStmt))]
public sealed record DefineWindowStmt(
    Token Name, Expr Top, Expr Left, Expr Bottom, Expr Right,
    Expr? Title, bool Double, bool Shadow, bool Pixel, Token Token,
    WindowFrameStyle Style = WindowFrameStyle.Double, Expr? Desktop = null) : Stmt(Token.Position)
{
    public override string ToString() => $"DEFINE WINDOW {Name.Lexeme} FROM {Top},{Left} TO {Bottom},{Right}{(Pixel ? " PIXEL" : "")}";
    public override AstNode DeepClone() => new DefineWindowStmt(
        (Token)Name.DeepClone(), (Expr)Top.DeepClone(), (Expr)Left.DeepClone(),
        (Expr)Bottom.DeepClone(), (Expr)Right.DeepClone(),
        (Expr?)Title?.DeepClone(), Double, Shadow, Pixel, (Token)Token.DeepClone(),
        Style, (Expr?)Desktop?.DeepClone());
}

/// <summary>Rappresenta ACTIVATE WINDOW name.</summary>
[JsonDerivedType(typeof(ActivateWindowStmt), nameof(ActivateWindowStmt))]
public sealed record ActivateWindowStmt(Token Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"ACTIVATE WINDOW {Name.Lexeme}";
    public override AstNode DeepClone() => new ActivateWindowStmt((Token)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Rappresenta DEACTIVATE WINDOW name.</summary>
[JsonDerivedType(typeof(DeactivateWindowStmt), nameof(DeactivateWindowStmt))]
public sealed record DeactivateWindowStmt(Token Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"DEACTIVATE WINDOW {Name.Lexeme}";
    public override AstNode DeepClone() => new DeactivateWindowStmt((Token)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Rappresenta RELEASE WINDOW name.</summary>
[JsonDerivedType(typeof(ReleaseWindowStmt), nameof(ReleaseWindowStmt))]
public sealed record ReleaseWindowStmt(Token Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RELEASE WINDOW {Name.Lexeme}";
    public override AstNode DeepClone() => new ReleaseWindowStmt((Token)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Azioni sulle finestre.</summary>
public enum WindowAction { Activate, Deactivate, Release, Close, Minimize, Restore, Show, Hide }

/// <summary>Rappresenta WINDOW name ACTION (activate/deactivate/release/close/minimize/restore).</summary>
[JsonDerivedType(typeof(WindowActionStmt), nameof(WindowActionStmt))]
public sealed record WindowActionStmt(Token Name, WindowAction Action, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} {Action}";
    public override AstNode DeepClone() => new WindowActionStmt((Token)Name.DeepClone(), Action, (Token)Token.DeepClone());
}

/// <summary>WINDOW SHOW | WINDOW HIDE — mostra/nasconde la finestra dello screen grafico
/// (forma globale, senza nome).</summary>
[JsonDerivedType(typeof(WindowVisibilityStmt), nameof(WindowVisibilityStmt))]
/// <summary>`WINDOW PUMP <ms>` — esegue un frame del loop form (COMANDO, due token).</summary>
public sealed record WindowPumpStmt(Expr Ms, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW PUMP {Ms}";
    public override AstNode DeepClone() => new WindowPumpStmt((Expr)Ms.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>`WINDOW CLOSE` — richiede la chiusura della finestra (COMANDO, senza nome = finestra SDL).</summary>
public sealed record WindowRequestCloseStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "WINDOW CLOSE";
    public override AstNode DeepClone() => new WindowRequestCloseStmt((Token)Token.DeepClone());
}

public sealed record WindowVisibilityStmt(bool Show, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => Show ? "WINDOW SHOW" : "WINDOW HIDE";
    public override AstNode DeepClone() => new WindowVisibilityStmt(Show, (Token)Token.DeepClone());
}

/// <summary>WINDOW SET TITLE "titolo" — imposta il titolo della finestra attiva.</summary>
[JsonDerivedType(typeof(WindowSetTitleStmt), nameof(WindowSetTitleStmt))]
public sealed record WindowSetTitleStmt(Expr Title, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW SET TITLE {Title}";
    public override AstNode DeepClone() => new WindowSetTitleStmt((Expr)Title.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome PRINT "testo" — aggiunge una riga al buffer della finestra.</summary>
[JsonDerivedType(typeof(WindowAppendLineStmt), nameof(WindowAppendLineStmt))]
public sealed record WindowAppendLineStmt(Token Name, Expr Text, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} PRINT {Text}";
    public override AstNode DeepClone() => new WindowAppendLineStmt((Token)Name.DeepClone(), (Expr)Text.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome TEXT lines() — imposta il buffer della finestra (array di stringhe).</summary>
[JsonDerivedType(typeof(WindowSetContentStmt), nameof(WindowSetContentStmt))]
public sealed record WindowSetContentStmt(Token Name, Expr Lines, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} TEXT {Lines}";
    public override AstNode DeepClone() => new WindowSetContentStmt((Token)Name.DeepClone(), (Expr)Lines.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome CLEAR — svuota il buffer della finestra.</summary>
[JsonDerivedType(typeof(WindowClearStmt), nameof(WindowClearStmt))]
public sealed record WindowClearStmt(Token Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} CLEAR";
    public override AstNode DeepClone() => new WindowClearStmt((Token)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome SCROLL n — scrolla il buffer di n righe.</summary>
[JsonDerivedType(typeof(WindowScrollStmt), nameof(WindowScrollStmt))]
public sealed record WindowScrollStmt(Token Name, Expr Delta, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} SCROLL {Delta}";
    public override AstNode DeepClone() => new WindowScrollStmt((Token)Name.DeepClone(), (Expr)Delta.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome EDIT ON/OFF — abilita/disabilita l'editing del buffer.</summary>
[JsonDerivedType(typeof(WindowEditStmt), nameof(WindowEditStmt))]
public sealed record WindowEditStmt(Token Name, bool Enable, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} EDIT {(Enable ? "ON" : "OFF")}";
    public override AstNode DeepClone() => new WindowEditStmt((Token)Name.DeepClone(), Enable, (Token)Token.DeepClone());
}

/// <summary>WINDOW SELECT nome|0|NONE — seleziona la finestra attiva (viewport corrente di
/// output). 0/NONE = schermo base (nessuna finestra). I comandi polimorfici (PRINT/LOCATE/
/// CLS/@SAY/@GET/BOX/...) usano coordinate relative alla viewport della finestra attiva.</summary>
[JsonDerivedType(typeof(WindowSelectStmt), nameof(WindowSelectStmt))]
public sealed record WindowSelectStmt(Expr Target, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW SELECT {Target}";
    public override AstNode DeepClone() => new WindowSelectStmt((Expr)Target.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW THEME "nome" — imposta il tema visivo globale delle finestre.</summary>
[JsonDerivedType(typeof(WindowThemeStmt), nameof(WindowThemeStmt))]
public sealed record WindowThemeStmt(Expr ThemeName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW THEME {ThemeName}";
    public override AstNode DeepClone() => new WindowThemeStmt((Expr)ThemeName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW DESKTOP n — passa al desktop virtuale indicato.</summary>
[JsonDerivedType(typeof(WindowDesktopStmt), nameof(WindowDesktopStmt))]
public sealed record WindowDesktopStmt(Expr Desktop, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW DESKTOP {Desktop}";
    public override AstNode DeepClone() => new WindowDesktopStmt((Expr)Desktop.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW nome DESKTOP n — sposta la finestra sul desktop virtuale indicato.</summary>
[JsonDerivedType(typeof(WindowMoveDesktopStmt), nameof(WindowMoveDesktopStmt))]
public sealed record WindowMoveDesktopStmt(Token Name, Expr Desktop, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WINDOW {Name.Lexeme} DESKTOP {Desktop}";
    public override AstNode DeepClone() => new WindowMoveDesktopStmt((Token)Name.DeepClone(), (Expr)Desktop.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>WINDOW [nome] FRAME DOUBLE|SINGLE|ROUNDED — imposta lo stile della cornice.</summary>
[JsonDerivedType(typeof(WindowFrameStmt), nameof(WindowFrameStmt))]
public sealed record WindowFrameStmt(Token? Name, WindowFrameStyle Style, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => Name != null ? $"WINDOW {Name.Lexeme} FRAME {Style}" : $"WINDOW FRAME {Style}";
    public override AstNode DeepClone() => new WindowFrameStmt((Token?)Name?.DeepClone(), Style, (Token)Token.DeepClone());
}

/// <summary>WINDOW [nome] SHADOW ON|OFF — abilita/disabilita l'ombreggiatura 3D.</summary>
[JsonDerivedType(typeof(WindowShadowStmt), nameof(WindowShadowStmt))]
public sealed record WindowShadowStmt(Token? Name, bool Enable, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => Name != null ? $"WINDOW {Name.Lexeme} SHADOW {(Enable ? "ON" : "OFF")}" : $"WINDOW SHADOW {(Enable ? "ON" : "OFF")}";
    public override AstNode DeepClone() => new WindowShadowStmt((Token?)Name?.DeepClone(), Enable, (Token)Token.DeepClone());
}


