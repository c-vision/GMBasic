// FILE: Interpreter.Ast.Console.cs



// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Rappresenta un argomento in un'istruzione PRINT, che può essere un'espressione o un separatore.
/// </summary>
public readonly record struct PrintArg(Expr? Expression, string? Separator)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Expression?.ToString() ?? Separator ?? string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo argomento di stampa.
    /// </summary>
    /// <returns>Una nuova istanza di PrintArg con una copia dell'espressione contenuta.</returns>
    public PrintArg DeepClone()
    {
        return this with { Expression = (Expr?)Expression?.DeepClone() };
    }
}

/// <summary>
/// Rappresenta l'istruzione GET.
/// </summary>
public sealed record GetKeyboardStmt(Target Variable, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Variable.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo GetKeyboardStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GetKeyboardStmt con copie dei suoi token.</returns>
    public override AstNode DeepClone()
    {
        return new GetKeyboardStmt((Target)Variable.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione INKEY.
/// </summary>
public sealed record InkeyStmt(Target Variable, Expr? TimeoutExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Variable}{(TimeoutExpr is not null ? $", {TimeoutExpr}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo InkeyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di InkeyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new InkeyStmt((Target)Variable.DeepClone(), (Expr?)TimeoutExpr?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione WAITKEY.
/// </summary>
public sealed record WaitKeyStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo WaitKeyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di WaitKeyStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new WaitKeyStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione INPUT, ora con supporto per un timeout opzionale
/// nella sintassi qualificata.
/// </summary>
/// <param name="Variables">La lista di variabili in cui memorizzare l'input.</param>
/// <param name="Prompt">L'espressione stringa opzionale da visualizzare come prompt.</param>
/// <param name="TimeoutExpr">L'espressione opzionale che definisce il timeout in millisecondi (solo per sintassi qualificata).</param>
/// <param name="Token">Il token 'INPUT' per riferimento.</param>
public sealed record InputStmt(ImmutableArray<Target> Variables, Expr? Prompt, Expr? TimeoutExpr, Expr? MaskExpr, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{(Prompt is not null ? $"{Prompt}; " : "")}{string.Join(", ", Variables)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo InputStmt.
    /// </summary>
    /// <returns>Una nuova istanza di InputStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedVariables = Variables.Select(v => (Target)v.DeepClone()).ToImmutableArray();
        var clonedPrompt = (Expr?)Prompt?.DeepClone();
        var clonedTimeout = (Expr?)TimeoutExpr?.DeepClone();
        var clonedMask = (Expr?)MaskExpr?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new InputStmt(clonedVariables, clonedPrompt, clonedTimeout, clonedMask, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CLS.
/// </summary>
public sealed record ClsStmt(Expr? Color, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Color is not null ? Color.ToString() : string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo ClsStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ClsStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ClsStmt((Expr?)Color?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione COLOR.
/// </summary>
public sealed record ColorStmt(Expr? Foreground, Expr? Background, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Foreground}{(Background is not null ? $", {Background}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ColorStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ColorStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedForeground = (Expr?)Foreground?.DeepClone();
        var clonedBackground = (Expr?)Background?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ColorStmt(clonedForeground, clonedBackground, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione LINE INPUT, ora con supporto per un timeout opzionale
/// nella sintassi qualificata.
/// </summary>
/// <param name="Prompt">L'espressione stringa opzionale da visualizzare come prompt.</param>
/// <param name="Variable">La variabile di destinazione per l'intera riga di input.</param>
/// <param name="TimeoutExpr">L'espressione opzionale che definisce il timeout in millisecondi (solo per sintassi qualificata).</param>
/// <param name="Token">Il token 'LINE INPUT' per riferimento.</param>
public sealed record LineInputStmt(Expr? Prompt, Target Variable, Expr? TimeoutExpr, Expr? MaskExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{(Prompt is not null ? $"{Prompt}; " : "")}{Variable}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LineInputStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LineInputStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedPrompt = (Expr?)Prompt?.DeepClone();
        var clonedVariable = (Target)Variable.DeepClone();
        var clonedTimeout = (Expr?)TimeoutExpr?.DeepClone();
        var clonedMask = (Expr?)MaskExpr?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new LineInputStmt(clonedPrompt, clonedVariable, clonedTimeout, clonedMask, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione LOCATE.
/// </summary>
public sealed record LocateStmt(Expr LineExpr, Expr ColumnExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{LineExpr}, {ColumnExpr}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LocateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LocateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedLineExpr = (Expr)LineExpr.DeepClone();
        var clonedColumnExpr = (Expr)ColumnExpr.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new LocateStmt(clonedLineExpr, clonedColumnExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione PRINT.
/// </summary>
public sealed record PrintStmt(ImmutableArray<PrintArg> Arguments, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Join("", Arguments);

    /// <summary>
    /// Crea una copia profonda di questo nodo PrintStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PrintStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => a.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new PrintStmt(clonedArguments, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CLOCATE per posizionare il cursore sulla console di testo.
/// </summary>
public sealed record CLocateStmt(Expr LineExpr, Expr ColumnExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{LineExpr}, {ColumnExpr}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CLocateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CLocateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedLineExpr = (Expr)LineExpr.DeepClone();
        var clonedColumnExpr = (Expr)ColumnExpr.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new CLocateStmt(clonedLineExpr, clonedColumnExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CPRINT per stampare sulla console di testo.
/// </summary>
public sealed record CPrintStmt(ImmutableArray<PrintArg> Arguments, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Join(" ", Arguments);

    /// <summary>
    /// Crea una copia profonda di questo nodo CPrintStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CPrintStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => a.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new CPrintStmt(clonedArguments, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CCLS per pulire la console di testo emulata.
/// </summary>
public sealed record CCLSStmt(Expr? Color, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Color?.ToString() ?? string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo CCLSStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CCLSStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CCLSStmt((Expr?)Color?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CSAVESCREEN: forza SAVE SCREEN a operare sul buffer della
/// console testuale (IConsole.Get) anche quando è attivo uno schermo grafico, esattamente
/// come CCLS/CPRINT/CLOCATE forzano le rispettive controparti verso la console.
/// </summary>
public sealed record CSaveScreenStmt(Expr Name, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Name.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo CSaveScreenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CSaveScreenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CSaveScreenStmt((Expr)Name.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CRESTORESCREEN: forza RESTORE SCREEN a operare sul buffer della
/// console testuale (IConsole.Put) anche quando è attivo uno schermo grafico, esattamente
/// come CCLS/CPRINT/CLOCATE forzano le rispettive controparti verso la console.
/// </summary>
public sealed record CRestoreScreenStmt(Expr Name, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Name.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo CRestoreScreenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CRestoreScreenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CRestoreScreenStmt((Expr)Name.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CLEAR (x1,y1)-(x2,y2) per pulire un'area rettangolare.
/// </summary>
public sealed record ClearRectStmt(Expr X1, Expr Y1, Expr X2, Expr Y2, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"({X1}, {Y1})-({X2}, {Y2})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ClearRectStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ClearRectStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedX1 = (Expr)X1.DeepClone();
        var clonedY1 = (Expr)Y1.DeepClone();
        var clonedX2 = (Expr)X2.DeepClone();
        var clonedY2 = (Expr)Y2.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ClearRectStmt(clonedX1, clonedY1, clonedX2, clonedY2, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione SET attributo ON/OFF.
/// </summary>
public sealed record SetAttributeStmt(Token Attribute, Token State, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Attribute.Lexeme} {State.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SetAttributeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SetAttributeStmt con copie dei suoi token.</returns>
    public override AstNode DeepClone()
    {
        return new SetAttributeStmt((Token)Attribute.DeepClone(), (Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CURSOR ON/OFF.
/// </summary>
public sealed record CursorStateStmt(Token State, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => State.Lexeme;

    /// <summary>
    /// Crea una copia profonda di questo nodo CursorStateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CursorStateStmt con copie dei suoi token.</returns>
    public override AstNode DeepClone()
    {
        return new CursorStateStmt((Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CURSOR SHAPE stile$.
/// </summary>
public sealed record CursorShapeStmt(Expr Shape, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SHAPE {Shape}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CursorShapeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CursorShapeStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CursorShapeStmt((Expr)Shape.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un comando per il clipboard, come CLIP.GET, CLIP.SET, CLIP.CLEAR.
/// </summary>
public sealed record ClipStmt(Token Command, Expr? Argument, Target? Destination, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $".{Command.Lexeme} {(Argument is not null ? Argument.ToString() : Destination?.ToString())}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ClipStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ClipStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCommand = (Token)Command.DeepClone();
        var clonedArgument = (Expr?)Argument?.DeepClone();
        var clonedDestination = (Target?)Destination?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ClipStmt(clonedCommand, clonedArgument, clonedDestination, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CONSOLE.CAPTION per impostare il titolo della finestra.
/// </summary>
/// <param name="TitleExpression">L'espressione che restituisce la stringa del nuovo titolo.</param>
/// <param name="Token">Il token 'CAPTION' per riferimento.</param>
public sealed record ConsoleCaptionStmt(Expr TitleExpression, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CAPTION {TitleExpression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConsoleCaptionStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConsoleCaptionStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ConsoleCaptionStmt((Expr)TitleExpression.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CONSOLE.SENDKEY per simulare l'input da tastiera.
/// Supporta solo la sintassi qualificata.
/// </summary>
/// <param name="KeysExpression">L'espressione che restituisce la stringa di tasti da inviare.</param>
/// <param name="Token">Il token 'SENDKEY' per riferimento.</param>
public sealed record ConsoleSendKeyStmt(Expr KeysExpression, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SENDKEY {KeysExpression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConsoleSendKeyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConsoleSendKeyStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new ConsoleSendKeyStmt((Expr)KeysExpression.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CONSOLE.SCROLLMODE per commutare tra la modalità a buffer
/// fisso (default, con supporto VIEW PRINT) e la modalità "nativa" a scorrimento lineare
/// che sfrutta lo scrollback reale del terminale (come SystemConsole). Solo StandardConsole
/// supporta questa distinzione; per le altre console è un no-op.
/// Supporta solo la sintassi qualificata.
/// </summary>
/// <param name="ModeExpression">L'espressione stringa: "NATIVE" o "BUFFERED".</param>
/// <param name="Token">Il token 'SCROLLMODE' per riferimento.</param>
public sealed record ConsoleScrollModeStmt(Expr ModeExpression, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SCROLLMODE {ModeExpression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConsoleScrollModeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConsoleScrollModeStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new ConsoleScrollModeStmt((Expr)ModeExpression.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione @ row, col SAY expr [PICTURE pic].
/// Output formattato in stile Clipper sulla console TUI.
/// </summary>
public sealed record AtSayStmt(
    Expr Row,
    Expr Col,
    Expr Expression,
    Expr? Picture,
    Token Token
) : Stmt(Token.Position)
{
    public override string ToString() => $"@ {Row}, {Col} SAY {Expression}{(Picture != null ? $" PICTURE {Picture}" : "")}";

    public override AstNode DeepClone()
    {
        return new AtSayStmt(
            (Expr)Row.DeepClone(),
            (Expr)Col.DeepClone(),
            (Expr)Expression.DeepClone(),
            (Expr?)Picture?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione @ row, col GET variable [PICTURE pic] [VALID expr] [RANGE lo, hi]
/// [COLOR "fg/bg"] [TYPE "control"] [ITEMS expr] [SIZE w, h] [CAPTION "text"].
/// Input field in stile Clipper con supporto per controlli avanzati.
/// </summary>
public sealed record AtGetStmt(
    Expr Row,
    Expr Col,
    Target Variable,
    Expr? Picture,
    Expr? Valid,
    Expr? RangeLo,
    Expr? RangeHi,
    Expr? Color,
    string? ControlType,
    Expr? Items,
    Expr? ControlWidth,
    Expr? ControlHeight,
    Expr? Caption,
    Expr? StepValue,
    Expr? MinValue,
    Expr? MaxValue,
    SourcePosition Position,
    Expr? When = null,
    Expr? Message = null,
    Expr? Error = null
) : Stmt(Position)
{
    public override string ToString() => $"@ {Row}, {Col} GET {Variable}";

    public override AstNode DeepClone()
    {
        return new AtGetStmt(
            (Expr)Row.DeepClone(),
            (Expr)Col.DeepClone(),
            (Target)Variable.DeepClone(),
            (Expr?)Picture?.DeepClone(),
            (Expr?)Valid?.DeepClone(),
            (Expr?)RangeLo?.DeepClone(),
            (Expr?)RangeHi?.DeepClone(),
            (Expr?)Color?.DeepClone(),
            ControlType,
            (Expr?)Items?.DeepClone(),
            (Expr?)ControlWidth?.DeepClone(),
            (Expr?)ControlHeight?.DeepClone(),
            (Expr?)Caption?.DeepClone(),
            (Expr?)StepValue?.DeepClone(),
            (Expr?)MinValue?.DeepClone(),
            (Expr?)MaxValue?.DeepClone(),
            Position
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione READ: avvia il loop interattivo per tutti i GET accumulati.
/// (Clipper-style READ, diverso dal DATA/READ di Ast.cs)
/// </summary>
public sealed record ClipperReadStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "CLIPPER READ";

    public override AstNode DeepClone()
    {
        return new ClipperReadStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// BOX top, left, bottom, right [SINGLE|DOUBLE]
/// Disegna un rettangolo sul buffer della console.
/// </summary>
public sealed record BoxStmt(
    Expr Top, Expr Left, Expr Bottom, Expr Right,
    Token? BorderStyle,
    Token Token
) : Stmt(Token.Position)
{
    public override string ToString() => $"BOX {Top},{Left},{Bottom},{Right}";
    public override AstNode DeepClone() => new BoxStmt(
        (Expr)Top.DeepClone(), (Expr)Left.DeepClone(),
        (Expr)Bottom.DeepClone(), (Expr)Right.DeepClone(),
        BorderStyle != null ? (Token)BorderStyle.DeepClone() : null,
        (Token)Token.DeepClone());
}

/// <summary>
/// SCROLL top, left, bottom, right, n
/// Scorre una regione della console di n righe.
/// </summary>
public sealed record ScrollStmt(
    Expr Top, Expr Left, Expr Bottom, Expr Right, Expr Count,
    Token Token
) : Stmt(Token.Position)
{
    public override string ToString() => $"SCROLL {Top},{Left},{Bottom},{Right},{Count}";
    public override AstNode DeepClone() => new ScrollStmt(
        (Expr)Top.DeepClone(), (Expr)Left.DeepClone(),
        (Expr)Bottom.DeepClone(), (Expr)Right.DeepClone(),
        (Expr)Count.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>ALERT "messaggio" — mostra un popup di avviso.</summary>
/// <summary>CONFIRM "messaggio" TO var — mostra un popup di conferma.</summary>
[JsonDerivedType(typeof(ConfirmStmt), nameof(ConfirmStmt))]
public sealed record ConfirmStmt(Expr Message, Target? ResultVar, Expr? Width, string? Align, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"CONFIRM {Message}";
    public override AstNode DeepClone() => new ConfirmStmt((Expr)Message.DeepClone(), (Target?)ResultVar?.DeepClone(), (Expr?)Width?.DeepClone(), Align, (Token)Token.DeepClone());
}
