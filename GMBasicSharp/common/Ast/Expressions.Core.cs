// FILE: Ast.Expressions.Core.cs

namespace Basic;

public sealed record QualifiedNameExpr(ImmutableArray<Token> Parts)
    : Expr(Parts.FirstOrDefault()?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce la stringa completa del nome qualificato (es. "System.Console").
    /// </summary>
    public string FullName => string.Join(".", Parts.Select(p => p.Lexeme));

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => FullName;

    /// <summary>
    /// Crea una copia profonda di questo nodo QualifiedNameExpr.
    /// </summary>
    /// <returns>Una nuova istanza di QualifiedNameExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new QualifiedNameExpr([..Parts.Select(p => (Token)p.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta un riferimento a una variabile, identificata da un ID univoco.
/// </summary>
public sealed record VariableExpr(int SymbolId, Token NameSuffixed, Token NameCleaned)
    : Expr(NameSuffixed.Position)
{
    /// <inheritdoc />
    public override string ToString() => $"{NameSuffixed.Lexeme}#{SymbolId}";

    /// <inheritdoc />
    public override AstNode DeepClone() =>
        new VariableExpr(SymbolId, (Token)NameSuffixed.DeepClone(), (Token)NameCleaned.DeepClone());
}


/*
 * =================================================================================
 *   Guida ai Nodi di Espressione dell'Abstract Syntax Tree (AST)
 * =================================================================================
 *
 * Questo file definisce tutte le classi (record) che rappresentano le espressioni
 * nel linguaggio GM-BASIC. Un'espressione è una qualsiasi porzione di codice
 * che, una volta valutata, produce un valore. Questo è in contrasto con uno
 * "statement" (Stmt), che rappresenta un'azione da eseguire (es. PRINT, IF, FOR).
 *
 * --- Classe Base ---
 *
 * record Expr(SourcePosition Position) : AstNode(Position)
 *      Tutte le espressioni ereditano da questa classe base astratta. Fornisce
 *      le informazioni fondamentali sulla posizione nel codice sorgente,
 *      essenziali per il reporting degli errori.
 *
 */
/// <summary>
/// Classe base astratta per tutti i nodi di espressione nell'AST.
/// </summary>

#region JsonDerivedType Attributes for Expr

[JsonDerivedType(typeof(BinaryExpr), nameof(BinaryExpr))]
[JsonDerivedType(typeof(FunctionExpr), nameof(FunctionExpr))]
[JsonDerivedType(typeof(QualifiedNameExpr), nameof(QualifiedNameExpr))]
[JsonDerivedType(typeof(BuiltinFunctionCallExpr), nameof(BuiltinFunctionCallExpr))]
[JsonDerivedType(typeof(GroupingExpr), nameof(GroupingExpr))]
[JsonDerivedType(typeof(NumberExpr), nameof(NumberExpr))]
[JsonDerivedType(typeof(StringExpr), nameof(StringExpr))]
[JsonDerivedType(typeof(CheckedStringExpr), nameof(CheckedStringExpr))]
[JsonDerivedType(typeof(BooleanExpr), nameof(BooleanExpr))]
[JsonDerivedType(typeof(UnaryExpr), nameof(UnaryExpr))]
[JsonDerivedType(typeof(VariableExpr), nameof(VariableExpr))]
[JsonDerivedType(typeof(ArrayAccessExpr), nameof(ArrayAccessExpr))]
[JsonDerivedType(typeof(GoFunctionExpr), nameof(GoFunctionExpr))]
[JsonDerivedType(typeof(IndexedAccessExpr), nameof(IndexedAccessExpr))]
[JsonDerivedType(typeof(ArrayLiteralExpr), nameof(ArrayLiteralExpr))]
[JsonDerivedType(typeof(NewExpr), nameof(NewExpr))]
[JsonDerivedType(typeof(HttpFetchExpr), nameof(HttpFetchExpr))]
[JsonDerivedType(typeof(ResolveExpr), nameof(ResolveExpr))]
[JsonDerivedType(typeof(FieldAccessExpr), nameof(FieldAccessExpr))]
[JsonDerivedType(typeof(FunctionReferenceExpr), nameof(FunctionReferenceExpr))]
[JsonDerivedType(typeof(MethodCallExpr), nameof(MethodCallExpr))]
[JsonDerivedType(typeof(PropertyAccessExpr), nameof(PropertyAccessExpr))]
[JsonDerivedType(typeof(ListComprehensionExpr), nameof(ListComprehensionExpr))]
[JsonDerivedType(typeof(GeneratorExpr), nameof(GeneratorExpr))]
[JsonDerivedType(typeof(NilExpr), nameof(NilExpr))]
[JsonDerivedType(typeof(BaseExpr), nameof(BaseExpr))]
[JsonDerivedType(typeof(LambdaExpr), nameof(LambdaExpr))]
[JsonDerivedType(typeof(MacroExpr), nameof(MacroExpr))]
[JsonDerivedType(typeof(EvalExpr), nameof(EvalExpr))]
[JsonDerivedType(typeof(DictionaryLiteralExpr), nameof(DictionaryLiteralExpr))]
[JsonDerivedType(typeof(JsonToObjectExpr), nameof(JsonToObjectExpr))]
[JsonDerivedType(typeof(UnresolvedCallExpr), nameof(UnresolvedCallExpr))]
[JsonDerivedType(typeof(ListLiteralExpr), nameof(ListLiteralExpr))]
[JsonDerivedType(typeof(AssignmentExpr), nameof(AssignmentExpr))]
[JsonDerivedType(typeof(TernaryExpr), nameof(TernaryExpr))]
[JsonDerivedType(typeof(ArraySliceExpr), nameof(ArraySliceExpr))]
[JsonDerivedType(typeof(PrefixIncrementExpr), nameof(PrefixIncrementExpr))]
[JsonDerivedType(typeof(PostfixIncrementExpr), nameof(PostfixIncrementExpr))]
[JsonDerivedType(typeof(TypeExpr), nameof(TypeExpr))]
[JsonDerivedType(typeof(NamedArgumentExpr), nameof(NamedArgumentExpr))]
[JsonDerivedType(typeof(InterpolatedStringExpr), nameof(InterpolatedStringExpr))]
[JsonDerivedType(typeof(TupleLiteralExpr), nameof(TupleLiteralExpr))]
[JsonDerivedType(typeof(AwaitExpr), nameof(AwaitExpr))]
[JsonDerivedType(typeof(DirectCastExpr), nameof(DirectCastExpr))]
[JsonDerivedType(typeof(WithMemberAccessExpr), nameof(WithMemberAccessExpr))]
[JsonDerivedType(typeof(NullableHasValueExpr), nameof(NullableHasValueExpr))]
[JsonDerivedType(typeof(NullableValueExpr), nameof(NullableValueExpr))]
[JsonDerivedType(typeof(NullConditionalAccessExpr), nameof(NullConditionalAccessExpr))]
[JsonDerivedType(typeof(AsyncFunctionExpr), nameof(AsyncFunctionExpr))]
[JsonDerivedType(typeof(LinqQueryExpr), nameof(LinqQueryExpr))]
[JsonDerivedType(typeof(SwitchExpr), nameof(SwitchExpr))]
[JsonDerivedType(typeof(ForeignAmountExpr), nameof(ForeignAmountExpr))]
[JsonDerivedType(typeof(SafeCastExpr), nameof(SafeCastExpr))]
#endregion

#region JsonDerivedType Attributes for InterpolatedStringSegment
[JsonDerivedType(typeof(InterpolatedStringSegment), nameof(InterpolatedStringSegment))]
[JsonDerivedType(typeof(InterpolatedStringLiteralSegment), nameof(InterpolatedStringLiteralSegment))]
[JsonDerivedType(typeof(InterpolatedStringExprSegment), nameof(InterpolatedStringExprSegment))]
#endregion

// Expr definito in Basic.Types/Foundation.cs

/// <summary>
/// Rappresenta un'espressione binaria, come 'A + B'.
/// </summary>
public sealed record BinaryExpr(Expr Left, Token Op, Expr Right) : Expr(Op.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Left} {Op.Lexeme} {Right})";

    /// <summary>
    /// Crea una copia profonda di questo nodo BinaryExpr.
    /// </summary>
    /// <returns>Una nuova istanza di BinaryExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new BinaryExpr((Expr)Left.DeepClone(), (Token)Op.DeepClone(), (Expr)Right.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione raggruppata tra parentesi, come '(A + B)'.
/// </summary>
public sealed record GroupingExpr(Expr Expression) : Expr(Expression.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Expression})";

    /// <summary>
    /// Crea una copia profonda di questo nodo GroupingExpr.
    /// </summary>
    /// <returns>Una nuova istanza di GroupingExpr con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new GroupingExpr((Expr)Expression.DeepClone());
    }
}

/// <summary>
/// Rappresenta un letterale numerico.
/// </summary>
/// <summary>Importo con valuta estera: <c>100.00 USD @ 0.95</c> (tasso opzionale, default 1).</summary>
public sealed record ForeignAmountExpr(Expr Amount, Token CurrencyToken, Expr? ExchangeRate)
    : Expr(Amount.Position)
{
    public override string ToString() =>
        ExchangeRate is null
            ? $"{Amount} {CurrencyToken.Lexeme}"
            : $"{Amount} {CurrencyToken.Lexeme} @ {ExchangeRate}";

    public override AstNode DeepClone() =>
        new ForeignAmountExpr(
            (Expr)Amount.DeepClone(),
            (Token)CurrencyToken.DeepClone(),
            (Expr?)ExchangeRate?.DeepClone());
}

public sealed record NumberExpr(double Value, Token Token) : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => Value.ToString(CultureInfo.InvariantCulture);

    /// <summary>
    /// Crea una copia profonda di questo nodo NumberExpr.
    /// </summary>
    /// <returns>Una nuova istanza di NumberExpr con gli stessi valori.</returns>
    public override AstNode DeepClone()
    {
        return this with { };
    }
}

/// <summary>
/// Espressione stringa da validare a compile-time nel contesto work area DBF (CHECKED "...").
/// Il Rewriter la espande in un <see cref="Expr"/> parsato e risolto contro <see cref="AstRewriteContext.CurrentUdtScopeId"/>.
/// </summary>
public sealed record CheckedStringExpr(string Value, Token CheckedToken, Token StringToken) : Expr(CheckedToken.Position)
{
    public override string ToString() => $"CHECKED \"{Value}\"";

    public override AstNode DeepClone() =>
        new CheckedStringExpr(Value, (Token)CheckedToken.DeepClone(), (Token)StringToken.DeepClone());
}

/// <summary>
/// Rappresenta un letterale stringa.
/// </summary>
public sealed record StringExpr(string Value, Token Token) : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"\"{Value}\"";

    /// <summary>
    /// Crea una copia profonda di questo nodo StringExpr.
    /// </summary>
    /// <returns>Una nuova istanza di StringExpr con gli stessi valori.</returns>
    public override AstNode DeepClone()
    {
        return this with { };
    }
}

/// <summary>
/// Rappresenta un segmento di una stringa interpolata.
/// </summary>
public abstract record InterpolatedStringSegment(SourcePosition Position) : AstNode(Position);

/// <summary>
/// Segmento letterale di una stringa interpolata.
/// </summary>
public sealed record InterpolatedStringLiteralSegment(string Text, Token Token)
    : InterpolatedStringSegment(Token.Position)
{
    public override string ToString() => Text;
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Segmento espressione di una stringa interpolata (il contenuto di {expr}).
/// </summary>
public sealed record InterpolatedStringExprSegment(Expr Expression, string? Format, Token OpenBrace)
    : InterpolatedStringSegment(OpenBrace.Position)
{
    public override string ToString() => $"{{{(Format != null ? ":" + Format : "")}}}";
    public override AstNode DeepClone() => new InterpolatedStringExprSegment((Expr)Expression.DeepClone(), Format, (Token)OpenBrace.DeepClone());
}

/// <summary>
/// Rappresenta una stringa interpolata $"..." con il template raw.
/// La segmentazione in letterali ed espressioni avviene a runtime nell'esecutore.
/// </summary>
public sealed record InterpolatedStringExpr(Token DollarToken, ImmutableArray<InterpolatedStringSegment> Segments, Token CloseQuote)
    : Expr(DollarToken.Position)
{
    /// <summary>
    /// Il template raw dalla lexer (con caratteri speciali per escaped braces).
    /// </summary>
    public string Template { get; init; } = string.Empty;

    public override string ToString()
    {
        if (!string.IsNullOrEmpty(Template))
            return $"$\"{Template}\"";
        var sb = new System.Text.StringBuilder("\"");
        foreach (var seg in Segments)
            sb.Append(seg.ToString());
        sb.Append("\"");
        return sb.ToString();
    }

    public override AstNode DeepClone()
    {
        return new InterpolatedStringExpr(
            (Token)DollarToken.DeepClone(),
            Segments.Select(s => (InterpolatedStringSegment)s.DeepClone()).ToImmutableArray(),
            (Token)CloseQuote.DeepClone())
        {
            Template = Template
        };
    }
}

/// <summary>
/// Rappresenta un singolo elemento di un literal tuple, con nome opzionale.
/// </summary>
/// <param name="Value">L'espressione del valore.</param>
/// <param name="Name">Il token con il nome del campo (null per elementi anonimi).</param>
public sealed record NamedTupleElement(Expr Value, Token? Name);

/// <summary>
/// Rappresenta un valore letterale di tuple: (expr1, expr2, ...) con nomi opzionali.
/// </summary>
public sealed record TupleLiteralExpr(ImmutableArray<NamedTupleElement> Elements, Token OpenParen, Token CloseParen)
    : Expr(OpenParen.Position)
{
    public override string ToString() => $"({string.Join(", ", Elements)})";
    public override AstNode DeepClone() => new TupleLiteralExpr(
        Elements.Select(e => new NamedTupleElement((Expr)e.Value.DeepClone(), e.Name is not null ? (Token)e.Name.DeepClone() : null)).ToImmutableArray(),
        (Token)OpenParen.DeepClone(),
        (Token)CloseParen.DeepClone());
}

/// <summary>
/// Rappresenta un'espressione AWAIT (es. AWAIT task1).
/// </summary>
public sealed record AwaitExpr(Expr Target, Token AwaitToken) : Expr(AwaitToken.Position)
{
    public override string ToString() => $"AWAIT {Target}";
    public override AstNode DeepClone() => new AwaitExpr((Expr)Target.DeepClone(), (Token)AwaitToken.DeepClone());
}

/// <summary>
/// Rappresenta un'espressione per la dichiarazione inline di funzione asincrona.
/// </summary>
public sealed record AsyncFunctionExpr(ProcedureDefinition Definition, Token AsyncToken)
    : Expr(AsyncToken.Position)
{
    public override string ToString() => $"ASYNC {Definition}";
    public override AstNode DeepClone() => new AsyncFunctionExpr(
        (ProcedureDefinition)Definition.DeepClone(),
        (Token)AsyncToken.DeepClone());
}

/// <summary>
/// Rappresenta un letterale booleano (TRUE o FALSE).
/// </summary>
public sealed record BooleanExpr(bool Value, Token Token) : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => Token.Lexeme;

    /// <summary>
    /// Crea una copia profonda di questo nodo BooleanExpr.
    /// </summary>
    /// <returns>Una nuova istanza di BooleanExpr con gli stessi valori.</returns>
    public override AstNode DeepClone()
    {
        return this with { };
    }
}

/// <summary>
/// Rappresenta un'espressione unaria, come '-A'.
/// </summary>
public sealed record UnaryExpr(Token Op, Expr Right) : Expr(Op.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Op.Lexeme} {Right})";

    /// <summary>
    /// Crea una copia profonda di questo nodo UnaryExpr.
    /// </summary>
    /// <returns>Una nuova istanza di UnaryExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UnaryExpr((Token)Op.DeepClone(), (Expr)Right.DeepClone());
    }
}

/// <summary>
/// Rappresenta un riferimento a una variabile, identificata da un ID univoco.
/// Contiene sia il nome originale (con suffisso) per la deduzione del tipo,
/// sia il nome pulito per il linking.
/// </summary>
// VariableExpr definito in Basic.Types/Expressions.Core.cs

/// <summary>
/// Rappresenta un accesso indicizzato (con parentesi) a un'espressione complessa.
/// Gestisce casi come 'myJson.courses(0)' o 'GetArray()(1)'.
/// </summary>
/// <param name="Base">L'espressione a cui viene applicato l'indice (es. 'myJson.courses').</param>
/// <param name="Indices">La lista di espressioni per gli indici.</param>
public sealed record IndexedAccessExpr(Expr Base, ImmutableArray<Expr> Indices, bool IsSafe = false)
    : Expr(Base.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"{Base}[{string.Join(", ", Indices)}]{(IsSafe ? "?" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IndexedAccessExpr.
    /// </summary>
    /// <returns>Una nuova istanza di IndexedAccessExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IndexedAccessExpr((Expr)Base.DeepClone(), [..Indices.Select(i => (Expr)i.DeepClone())], IsSafe);
    }
}

/// <summary>
/// Rappresenta un nome qualificato multi-parte, come "System.Console" o "Trading.Options".
/// </summary>
// QualifiedNameExpr definito in Basic.Types/Expressions.Core.cs

/// <summary>
/// Rappresenta una chiamata alla funzione built-in FETCH.
/// </summary>
/// <param name="Url">L'espressione dell'URL da contattare.</param>
/// <param name="Options">L'espressione opzionale che fornisce un oggetto HTTPOPTIONS.</param>
/// <param name="CallbackName">Il token opzionale con il nome della subroutine di callback per l'esecuzione asincrona.</param>
public sealed record HttpFetchExpr(Expr Url, Expr? Options, Token? CallbackName) : Expr(Url.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() =>
        $"(FETCH {Url} {(Options is not null ? Options : "")} {(CallbackName is not null ? CallbackName.Lexeme : "")})";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpFetchExpr.
    /// </summary>
    /// <returns>Una nuova istanza di HttpFetchExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpFetchExpr((Expr)Url.DeepClone(), (Expr?)Options?.DeepClone(), (Token?)CallbackName?.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'operazione di risoluzione di un FutureValue.
/// </summary>
/// <param name="FutureToResolve">L'espressione che restituisce il FutureValue da risolvere.</param>
public sealed record ResolveExpr(Expr FutureToResolve) : Expr(FutureToResolve.Position)
{
    /// <summary>
    /// Crea una copia profonda di questo nodo ResolveExpr.
    /// </summary>
    /// <returns>Una nuova istanza di ResolveExpr con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new ResolveExpr((Expr)FutureToResolve.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'accesso a un campo di una variabile di tipo definito dall'utente (es. P.Nome).
/// Contiene sia il Token originale del campo per l'accesso basato su nome (tipi built-in, fallback)
/// sia il SymbolId risolto per l'accesso performante (UDT, Classi).
/// </summary>
public sealed record FieldAccessExpr(Expr Structure, Token Field)
    : Expr(Structure.Position)
{
    /// <summary>
    /// L'ID del simbolo del campo, risolto dal Linker.
    /// È -1 se il campo appartiene a un tipo built-in o non è stato risolto.
    /// </summary>
    public int FieldSymbolId { get; init; } = -1;

    /// <summary>
    /// Il tipo di dato del campo, inferito e annotato dal Rewriter.
    /// </summary>
    public DataType? ResolvedType { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo, includendo l'ID se risolto.
    /// </summary>
    public override string ToString()
    {
        return FieldSymbolId >= 0
            ? $"{Structure}.{Field.Lexeme}#{FieldSymbolId}"
            : $"{Structure}.{Field.Lexeme}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo FieldAccessExpr.
    /// </summary>
    /// <returns>Una nuova istanza di FieldAccessExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new FieldAccessExpr((Expr)Structure.DeepClone(), (Token)Field.DeepClone())
        {
            FieldSymbolId = FieldSymbolId,
            ResolvedType = ResolvedType?.DeepClone() as DataType
        };
    }
}

/// <summary>
/// Rappresenta il valore letterale NIL.
/// </summary>
public sealed record NilExpr(Token Token) : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => "NIL";

    /// <summary>
    /// Crea una copia profonda di questo nodo NilExpr.
    /// </summary>
    /// <returns>Una nuova istanza di NilExpr con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new NilExpr((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta la keyword 'BASE', usata per accedere ai membri della classe base.
/// </summary>
/// <param name="Token">Il token 'BASE' per riferimento alla posizione.</param>
public sealed record BaseExpr(Token Token) : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce la rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => Token.Lexeme;

    /// <summary>
    /// Crea una copia profonda di questo nodo BaseExpr.
    /// </summary>
    /// <returns>Una nuova istanza di BaseExpr con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new BaseExpr((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione di macro (es. &cVarName).
/// </summary>
/// <param name="Expression">L'espressione che restituisce la stringa di codice da valutare.</param>
/// <param name="Token">Il token '&' per riferimento alla posizione.</param>
public sealed record MacroExpr(Expr Expression, Token Token)
    : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"&({Expression})";

    /// <summary>
    /// Crea una copia profonda di questo nodo MacroExpr.
    /// </summary>
    /// <returns>Una nuova istanza di MacroExpr con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new MacroExpr((Expr)Expression.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta una chiamata <c>EVAL</c> con dispatch per TIPO del primo argomento (semantica
/// CA-Clipper 5.3 + estensione GMBasic): <c>EVAL(&lt;stringa&gt;)</c> è il macro-evaluator
/// (come <see cref="MacroExpr"/>), <c>EVAL(&lt;codeblock&gt; [, args...])</c> invoca il block con
/// gli argomenti (Clipper). Il Linker risolve staticamente i casi noti (target stringa →
/// <see cref="MacroExpr"/>; target function pointer → <see cref="FunctionExpr"/> indiretto);
/// i casi dinamici (target Any/Inferred: block prodotti da macro o parametri) restano
/// <see cref="EvalExpr"/> ed eseguiti dai backend con dispatch a runtime.
/// </summary>
/// <param name="Target">L'espressione che produce la stringa (macro) o il code block da invocare.</param>
/// <param name="Args">Argomenti dell'invocazione del block (vuoto per il macro-eval).</param>
/// <param name="Token">Il token 'EVAL' per riferimento alla posizione.</param>
public sealed record EvalExpr(Expr Target, ImmutableArray<Expr> Args, Token Token)
    : Expr(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Args.IsDefaultOrEmpty
        ? $"EVAL({Target})"
        : $"EVAL({Target}, {string.Join(", ", Args)})";

    /// <summary>
    /// Crea una copia profonda di questo nodo EvalExpr.
    /// </summary>
    /// <returns>Una nuova istanza di EvalExpr con copie di target e argomenti.</returns>
    public override AstNode DeepClone()
    {
        return new EvalExpr(
            (Expr)Target.DeepClone(),
            Args.IsDefault ? default : [..Args.Select(a => (Expr)a.DeepClone())],
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un accesso a un elemento di un array, come 'A(I, J)', identificato da un SymbolId.
/// Contiene sia il nome originale (con suffisso) per la deduzione del tipo,
/// sia il nome pulito per il linking.
/// </summary>
public sealed record ArrayAccessExpr(int SymbolId, Token NameSuffixed, Token NameCleaned, ImmutableArray<Expr> Indices, bool IsSafe = false)
    : Expr(NameSuffixed.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() =>
        $"{NameSuffixed.Lexeme}{(SymbolId >= 0 ? $"#{SymbolId}" : "#-1")}({string.Join(", ", Indices)}){(IsSafe ? "?" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ArrayAccessExpr.
    /// </summary>
    /// <returns>Una nuova istanza di ArrayAccessExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ArrayAccessExpr(
            SymbolId,
            (Token)NameSuffixed.DeepClone(),
            (Token)NameCleaned.DeepClone(),
            [..Indices.Select(i => (Expr)i.DeepClone())],
            IsSafe
        );
    }
}

/// <summary>
/// Rappresenta un letterale array definito da una lista di valori tra parentesi graffe.
/// </summary>
/// <param name="Elements">La lista immutabile di espressioni che costituiscono gli elementi dell'array.</param>
/// <param name="OpenBrace">Il token della parentesi graffa aperta '{', per riferimento a riga/colonna.</param>
public sealed record ArrayLiteralExpr(ImmutableArray<Expr> Elements, Token OpenBrace)
    : Expr(OpenBrace.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"{{ {string.Join(", ", Elements)} }}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ArrayLiteralExpr.
    /// </summary>
    /// <returns>Una nuova istanza di ArrayLiteralExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ArrayLiteralExpr([..Elements.Select(e => (Expr)e.DeepClone())], (Token)OpenBrace.DeepClone());
    }
}

/// <summary>
/// Rappresenta una singola clausola di generazione (FOR ... IN ... [IF ...]) 
/// all'interno di una comprensione di lista.
/// </summary>
[JsonDerivedType(typeof(ComprehensionClause), nameof(ComprehensionClause))]
public sealed record ComprehensionClause(
    int IteratorSymbolId,
    Token IteratorVariable,
    Expr SourceExpression,
    Expr? ConditionExpression)
{
    public ComprehensionClause DeepClone()
    {
        return new ComprehensionClause(
            IteratorSymbolId,
            (Token)IteratorVariable.DeepClone(),
            (Expr)SourceExpression.DeepClone(),
            (Expr?)ConditionExpression?.DeepClone());
    }

    public override string ToString()
    {
        var iteratorStr = $"{IteratorVariable.Lexeme}#{IteratorSymbolId}";
        var ifPart = ConditionExpression != null ? $" IF {ConditionExpression}" : "";
        return $"FOR {iteratorStr} IN {SourceExpression}{ifPart}";
    }
}

/// <summary>
/// <summary>
/// Rappresenta un'espressione di comprensione di lista.
/// Questa versione è stata aggiornata per includere il SymbolId della variabile iteratore.
/// </summary>
/// <param name="OutputExpression">L'espressione da calcolare per ogni elemento generato.</param>
/// <param name="Clauses">La sequenza di clausole di generazione (da sinistra a destra).</param>
/// <param name="OpenBracket">Il token '[' iniziale, per la posizione nel codice.</param>
public sealed record ListComprehensionExpr(
    Expr OutputExpression,
    ImmutableArray<ComprehensionClause> Clauses,
    Token OpenBracket,
    ImmutableArray<LinqOrderByClause> OrderByClauses
) : Expr(OpenBracket.Position)
{
    /// <summary>
    /// Fornisce una rappresentazione testuale compatta della comprensione di lista.
    /// </summary>
    public override string ToString()
    {
        var orderStr = OrderByClauses.Length > 0
            ? " ORDER BY " + string.Join(", ", OrderByClauses.Select(o => o.Expression.ToString()))
            : "";
        return $"[{OutputExpression} {string.Join(" ", Clauses)}{orderStr}]";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedClauses = Clauses.Select(c => c.DeepClone()).ToImmutableArray();
        return new ListComprehensionExpr(
            (Expr)OutputExpression.DeepClone(),
            clonedClauses,
            (Token)OpenBracket.DeepClone(),
            OrderByClauses);
    }
}

/// <summary>
/// Espressione generatore lazy: <c>(expr FOR var IN source [IF cond])</c>.
/// A differenza di ListComprehensionExpr (eager), produce un oggetto GeneratorType
/// che genera valori on-demand tramite FOR EACH o NEXT().
/// </summary>
public sealed record GeneratorExpr(
    Expr OutputExpression,
    ImmutableArray<ComprehensionClause> Clauses,
    Token OpenParen
) : Expr(OpenParen.Position)
{
    public override string ToString() => $"({OutputExpression} {string.Join(" ", Clauses)})";

    public override AstNode DeepClone()
    {
        var clonedClauses = Clauses.Select(c => c.DeepClone()).ToImmutableArray();
        return new GeneratorExpr((Expr)OutputExpression.DeepClone(), clonedClauses, (Token)OpenParen.DeepClone());
    }
}

/// <summary>
/// Rappresenta una chiamata a funzione creata dall'utente, identificata da un SymbolId.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo della funzione da chiamare.</param>
/// <param name="OriginalToken">Il token originale del nome della funzione, per la diagnostica.</param>
/// <param name="Args">La lista degli argomenti.</param>
/// <param name="ReturnType">Il tipo di dato restituito, risolto dal Linker.</param>
public sealed record FunctionExpr(int SymbolId, Token OriginalToken, ImmutableArray<Expr> Args, DataType? ReturnType)
    : Expr(OriginalToken.Position)
{
    /// <summary>
    /// Riferimento alla definizione della procedura, popolato dal Linker.
    /// </summary>
    [JsonIgnore]
    public ProcedureDefinition? ResolvedDefinition { get; set; }

    /// <summary>
    /// Indica se questa chiamata a funzione è indiretta (via funzione anonima).
    /// </summary>
    [JsonIgnore]
    public bool IsIndirect { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() =>
        $"{OriginalToken.Lexeme}#{SymbolId}({string.Join(", ", Args)}) AS {ReturnType?.ToString() ?? "VOID"}]";

    /// <summary>
    /// Crea una copia profonda di questo nodo FunctionExpr.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clone = new FunctionExpr(
            SymbolId,
            (Token)OriginalToken.DeepClone(),
            [..Args.Select(a => (Expr)a.DeepClone())],
            (DataType?)ReturnType?.DeepClone()
        )
        {
            ResolvedDefinition = ResolvedDefinition
        };
        return clone;
    }
}

/// <summary>
/// Rappresenta una chiamata a una funzione built-in risolta.
/// </summary>
/// <param name="Namespace">Il token del namespace della funzione.</param>
/// <param name="Name">Il token della funzione. Il suo 'Kind' è la chiave per la VM.</param>
/// <param name="Args">La lista degli argomenti.</param>
/// <param name="ReturnType">Il tipo di dato restituito.</param>
public sealed record BuiltinFunctionCallExpr(
    Token Namespace,
    Token Name,
    ImmutableArray<Expr> Args,
    DataType ReturnType)
    : Expr(Name.Position)
{
    /// <summary>
    /// Contiene il token originale del nome della funzione.
    /// </summary>
    public Token OriginalToken { get; init; } = Name;

    /// <summary>
    /// Fornisce una rappresentazione testuale della chiamata.
    /// </summary>
    public override string ToString()
    {
        // Usa ricorsivamente il ToString() degli argomenti, che ora saranno formattati correttamente.
        return $"{Namespace.Lexeme}.{OriginalToken.Lexeme}({string.Join(", ", Args.Select(arg => arg.ToString()))})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new BuiltinFunctionCallExpr(
            (Token)Namespace.DeepClone(),
            (Token)Name.DeepClone(),
            [..Args.Select(a => (Expr)a.DeepClone())],
            (DataType)ReturnType.DeepClone()
        )
        {
            OriginalToken = (Token)OriginalToken.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta una chiamata a funzione concorrente.
/// </summary>
/// <param name="SymbolId">L'ID univoco della funzione da chiamare.</param>
/// <param name="OriginalToken">Il token originale del nome della funzione.</param>
public sealed record GoFunctionExpr(
    int SymbolId,
    Token OriginalToken,
    ImmutableArray<Expr> Args,
    Expr? WaitGroup,
    DataType ReturnType)
    : Expr(OriginalToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() =>
        $"GO {OriginalToken.Lexeme}#{SymbolId}({string.Join(", ", Args)}) AS {ReturnType}";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new GoFunctionExpr(
            SymbolId,
            (Token)OriginalToken.DeepClone(),
            [..Args.Select(a => (Expr)a.DeepClone())],
            (Expr?)WaitGroup?.DeepClone(),
            (DataType)ReturnType.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta una funzione anonima (lambda).
/// </summary>
public sealed record LambdaExpr(
    ImmutableArray<Parameter> Parameters,
    DataType ReturnType,
    ImmutableArray<Stmt>? BlockBody,
    Expr? ExpressionBody,
    Token FuncToken
) : Expr(FuncToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString()
    {
        var paramsStr = string.Join(", ", Parameters.Select(p => $"{p.Name.Lexeme} AS {p.DataType}"));
        var bodyStr = ExpressionBody?.ToString() ?? $"{{ {BlockBody?.Length ?? 0} stmts }}";
        return $"(Lambda({paramsStr}) => {bodyStr})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedParameters = Parameters.Select(p => (Parameter)p.DeepClone()).ToImmutableArray();
        var clonedBlockBody = BlockBody?.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedExpressionBody = (Expr?)ExpressionBody?.DeepClone();
        return new LambdaExpr(clonedParameters, (DataType)ReturnType.DeepClone(), clonedBlockBody, clonedExpressionBody,
            (Token)FuncToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta un riferimento a una funzione.
/// </summary>
/// <param name="SymbolId">L'ID univoco della funzione a cui si fa riferimento.</param>
/// <param name="OriginalToken">Il token originale del nome della funzione.</param>
public sealed record FunctionReferenceExpr(int SymbolId, Token OriginalToken) : Expr(OriginalToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"(ref {OriginalToken.Lexeme}#{SymbolId})";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new FunctionReferenceExpr(SymbolId, (Token)OriginalToken.DeepClone());
    }
}

/// <summary>
/// Nodo SINTETICO (mai prodotto dal parser): "dammi il box BYREF write-through per questo
/// simbolo", come valore. Usato SOLO dal Linker (PerformHeapPromotion) per inizializzare il
/// campo box di una __ClosureCtx con lo STESSO box già ricevuto da un parametro BYREF
/// catturato — mai il valore, il box stesso (identità condivisa col chiamante). Il
/// compilatore lo compila come OP_MAKE_BYREF/OP_MAKE_BYREF_GLOBAL sullo slot del simbolo
/// (stessa emissione già usata per un argomento BYREF a un call-site, generalizzata).
/// </summary>
/// <param name="SymbolId">L'ID del simbolo (parametro BYREF) di cui creare/condividere il box.</param>
/// <param name="OriginalToken">Il token del simbolo originale (per diagnostica/posizione).</param>
public sealed record MakeByRefExpr(int SymbolId, Token OriginalToken) : Expr(OriginalToken.Position)
{
    public override string ToString() => $"(byref-box {OriginalToken.Lexeme}#{SymbolId})";

    public override AstNode DeepClone()
    {
        return new MakeByRefExpr(SymbolId, (Token)OriginalToken.DeepClone());
    }
}

/// <summary>
/// Specifica il motivo di una chiamata non risolta.
/// </summary>
public enum UnresolvedReason
{
    FunctionNotFound,
    BuiltinNamespaceNotImported,
    AmbiguousBuiltinFunctions,
    AmbiguousUserFunctions,
    AmbiguousUserAndBuiltinFunction
}

/// <summary>
/// Rappresenta una chiamata a funzione non risolta dal parser.
/// </summary>
public sealed record UnresolvedCallExpr(
    Token Name,
    ImmutableArray<Expr> Arguments,
    DataType ReturnType,
    UnresolvedReason Reason,
    ImmutableArray<object> Candidates
) : Expr(Name.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString()
    {
        var candidateStrings =
            Candidates.Select(c => c is ProcedureDefinition p ? $"UDF:{p.OriginalName.Lexeme}" : c.ToString());
        return
            $"UnresolvedCall[{Name.Lexeme}({Arguments.Length} args), Reason: {Reason}, Candidates: {string.Join(", ", candidateStrings)}]";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new UnresolvedCallExpr(
            (Token)Name.DeepClone(),
            [..Arguments.Select(a => (Expr)a.DeepClone())],
            (DataType)ReturnType.DeepClone(),
            Reason,
            Candidates
        );
    }
}

/// <summary>
/// Rappresenta un'espressione di istanziazione (es. `NEW MyType`).
/// </summary>
public sealed record NewExpr(
    DataType ParsedType,
    ImmutableArray<Expr>? ConstructorArgs,
    ImmutableArray<Expr>? InitialValues,
    Token NewToken
) : Expr(NewToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString()
    {
        var typeString = ParsedType.ToString();
        var argsString = "";
        if (ConstructorArgs.HasValue)
        {
            argsString = $"({string.Join(", ", ConstructorArgs.Value)})";
        }

        var initString = "";
        if (InitialValues.HasValue)
        {
            initString = $" {{ {string.Join(", ", InitialValues.Value)} }}";
        }

        return $"(NEW {typeString}{argsString}{initString})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedConstructorArgs = ConstructorArgs?.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        var clonedInitialValues = InitialValues?.Select(v => (Expr)v.DeepClone()).ToImmutableArray();
        return new NewExpr((DataType)ParsedType.DeepClone(), clonedConstructorArgs, clonedInitialValues,
            (Token)NewToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta un letterale lista (es. `[1, 2, 3]`).
/// </summary>
public sealed record ListLiteralExpr(ImmutableArray<Expr> Elements, Token OpenBracket)
    : Expr(OpenBracket.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"[ {string.Join(", ", Elements)} ]";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new ListLiteralExpr([..Elements.Select(e => (Expr)e.DeepClone())], (Token)OpenBracket.DeepClone());
    }
}

/// <summary>
/// Rappresenta un letterale dizionario (es. `{ "a": 1 }`).
/// </summary>
[JsonDerivedType(typeof(DictionaryLiteralExpr), nameof(DictionaryLiteralExpr))]
public sealed record DictionaryLiteralExpr(ImmutableArray<DictionaryEntry> Entries, Token OpenBrace)
    : Expr(OpenBrace.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"{{ {string.Join(", ", Entries)} }}";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new DictionaryLiteralExpr(
            [..Entries.Select(e => e.DeepClone())],
            (Token)OpenBrace.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta una chiamata a un metodo su un'istanza.
/// </summary>
public sealed record MethodCallExpr(Expr Instance, Token MethodName, ImmutableArray<Expr> Args)
    : Expr(Instance.Position)
{
    /// <summary>
    /// Riferimento alla definizione della procedura, popolato dal Linker.
    /// </summary>
    [JsonIgnore]
    public ProcedureDefinition? ResolvedDefinition { get; set; }

    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"{Instance}.{MethodName.Lexeme}({string.Join(", ", Args)})";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clone = new MethodCallExpr(
            (Expr)Instance.DeepClone(),
            (Token)MethodName.DeepClone(),
            [..Args.Select(a => (Expr)a.DeepClone())]
        )
        {
            ResolvedDefinition = ResolvedDefinition
        };
        return clone;
    }
}

/// <summary>
/// Rappresenta l'accesso a una proprietà di un'istanza.
/// </summary>
public sealed record PropertyAccessExpr(Expr Instance, Token PropertyName)
    : Expr(Instance.Position)
{
    /// <summary>
    /// Riferimento alla definizione della proprietà, popolato dal Linker.
    /// </summary>
    [JsonIgnore]
    public PropertyDefinition? ResolvedDefinition { get; set; }

    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"{Instance}.{PropertyName.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new PropertyAccessExpr((Expr)Instance.DeepClone(), (Token)PropertyName.DeepClone())
        {
            ResolvedDefinition = ResolvedDefinition
        };
    }
}

/// <summary>
/// Rappresenta una chiamata alla funzione speciale `JSON.ToObject`.
/// </summary>
[JsonDerivedType(typeof(JsonToObjectExpr), nameof(JsonToObjectExpr))]
public sealed record JsonToObjectExpr(DataType TargetType, Expr JsonSource, Token ToObjectToken)
    : Expr(ToObjectToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del nodo.
    /// </summary>
    public override string ToString() => $"JSON.ToObject({TargetType}, {JsonSource})";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new JsonToObjectExpr((DataType)TargetType.DeepClone(), (Expr)JsonSource.DeepClone(),
            (Token)ToObjectToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione di assegnazione inline (es. `x := 10`).
/// A differenza di LetStmt, questa è un'espressione che restituisce il valore assegnato.
/// </summary>
/// <param name="Target">La destinazione dell'assegnazione (L-value).</param>
/// <param name="Value">L'espressione da valutare e assegnare.</param>
/// <param name="Operator">Il token dell'operatore ':='.</param>
public sealed record AssignmentExpr(Target Target, Expr Value, Token Operator) : Expr(Operator.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Target} := {Value})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AssignmentExpr.
    /// </summary>
    /// <returns>Una nuova istanza di AssignmentExpr con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new AssignmentExpr((Target)Target.DeepClone(), (Expr)Value.DeepClone(), (Token)Operator.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione ternaria (condizione ? vero : falso).
/// </summary>
public sealed record TernaryExpr(Expr Condition, Expr TruePart, Expr FalsePart) 
    : Expr(Condition.Position)
{
    public override string ToString() => $"({Condition} ? {TruePart} : {FalsePart})";

    public override AstNode DeepClone()
    {
        return new TernaryExpr(
            (Expr)Condition.DeepClone(), 
            (Expr)TruePart.DeepClone(), 
            (Expr)FalsePart.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'operazione di slicing su un'espressione indicizzabile (es. array[start:end]).
/// </summary>
/// <param name="Base">L'espressione dell'array o della collezione da cui estrarre la porzione.</param>
/// <param name="Start">L'espressione per l'indice di inizio (inclusivo). Può essere null.</param>
/// <param name="End">L'espressione per l'indice di fine (esclusivo). Può essere null.</param>
public sealed record ArraySliceExpr(Expr Base, Expr? Start, Expr? End) 
    : Expr(Base.Position)
{
    public override string ToString() => $"{Base}[{Start} : {End}]";

    public override AstNode DeepClone()
    {
        return new ArraySliceExpr(
            (Expr)Base.DeepClone(), 
            (Expr?)Start?.DeepClone(), 
            (Expr?)End?.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'operazione di incremento/decremento prefisso (es. ++i, --i).
/// </summary>
/// <param name="Operator">Il token dell'operatore ('++' o '--').</param>
/// <param name="Target">La destinazione da modificare (variabile, campo, etc.).</param>
public sealed record PrefixIncrementExpr(Token Operator, Target Target) : Expr(Operator.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Operator.Lexeme}{Target})";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new PrefixIncrementExpr((Token)Operator.DeepClone(), (Target)Target.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'operazione di incremento/decremento postfisso (es. i++, i--).
/// </summary>
/// <param name="Target">La destinazione da modificare (variabile, campo, etc.).</param>
/// <param name="Operator">Il token dell'operatore ('++' o '--').</param>
public sealed record PostfixIncrementExpr(Target Target, Token Operator) : Expr(Target.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"({Target}{Operator.Lexeme})";

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new PostfixIncrementExpr((Target)Target.DeepClone(), (Token)Operator.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione di comprensione di dizionario (es. { k:v FOR ... }).
/// </summary>
[JsonDerivedType(typeof(DictComprehensionExpr), nameof(DictComprehensionExpr))]
public sealed record DictComprehensionExpr(
    Expr KeyExpression,
    Expr ValueExpression,
    ImmutableArray<ComprehensionClause> Clauses,
    Token OpenBrace
) : Expr(OpenBrace.Position)
{
    public override string ToString() => $"{{ {KeyExpression}:{ValueExpression} {string.Join(" ", Clauses)} }}";

    public override AstNode DeepClone()
    {
        return new DictComprehensionExpr(
            (Expr)KeyExpression.DeepClone(),
            (Expr)ValueExpression.DeepClone(),
            [.. Clauses.Select(c => c.DeepClone())],
            (Token)OpenBrace.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'espressione di comprensione di set (es. { x FOR ... }).
/// </summary>
[JsonDerivedType(typeof(SetComprehensionExpr), nameof(SetComprehensionExpr))]
public sealed record SetComprehensionExpr(
    Expr ElementExpression,
    ImmutableArray<ComprehensionClause> Clauses,
    Token OpenBrace
) : Expr(OpenBrace.Position)
{
    public override string ToString() => $"{{ {ElementExpression} {string.Join(" ", Clauses)} }}";

    public override AstNode DeepClone()
    {
        return new SetComprehensionExpr(
            (Expr)ElementExpression.DeepClone(),
            [.. Clauses.Select(c => c.DeepClone())],
            (Token)OpenBrace.DeepClone());
    }
}

/// <summary>
/// Rappresenta un tipo di dato utilizzato come espressione, tipicamente come operando destro
/// dell'operatore 'IS' per il controllo del tipo a runtime (TODO #84).
/// </summary>
/// <param name="Type">Il tipo di dato da confrontare.</param>
public sealed record TypeExpr(DataType Type) : Expr(Type.Position)
{
    /// <summary>
    /// Restituisce la rappresentazione testuale del tipo contenuto.
    /// </summary>
    public override string ToString() => Type.ToString();

    /// <summary>
    /// Crea una copia profonda del nodo e del tipo contenuto.
    /// </summary>
    public override AstNode DeepClone() => new TypeExpr((DataType)Type.DeepClone());
}

/// <summary>
/// Rappresenta un argomento nominale in una chiamata a procedura (es. 'Altezza := 100') (TODO #81).
/// Questo nodo viene utilizzato temporaneamente dal Parser per catturare la sintassi nominale
/// e viene risolto dal Linker per riordinare gli argomenti in base alla posizione reale.
/// </summary>
/// <param name="Name">Il token dell'identificatore che specifica il nome del parametro.</param>
/// <param name="Value">L'espressione fornita come valore per l'argomento.</param>
public sealed record NamedArgumentExpr(Token Name, Expr Value) : Expr(Name.Position)
{
    /// <summary>
    /// Restituisce la rappresentazione testuale dell'argomento nominale.
    /// </summary>
    public override string ToString() => $"{Name.Lexeme} := {Value}";

    /// <summary>
    /// Crea una copia profonda del nodo, clonando ricorsivamente il nome e il valore.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new NamedArgumentExpr((Token)Name.DeepClone(), (Expr)Value.DeepClone());
    }
}

/// <summary>
/// Rappresenta un cast diretto senza boxing: DIRECTCAST(expr, TypeName).
/// A runtime verrà verificato che l'oggetto è del tipo richiesto.
/// </summary>
public sealed record DirectCastExpr(Expr Expression, DataType TargetType, Token CastToken) : Expr(CastToken.Position)
{
    public override string ToString() => $"DIRECTCAST({Expression}, {TargetType})";
    public override AstNode DeepClone() => new DirectCastExpr((Expr)Expression.DeepClone(), (DataType)TargetType.DeepClone(), (Token)CastToken.DeepClone());
}

/// <summary>
/// Rappresenta un accesso membro implicito dentro un blocco WITH...END WITH.
/// `.Member` o `.Method(args)` viene risolto come `WithTarget.Member` a runtime.
/// </summary>
public sealed record WithMemberAccessExpr(Token MemberNameToken, Token DotToken, ImmutableArray<Expr>? Arguments = null) : Expr(DotToken.Position)
{
    public override string ToString() => Arguments is null ? $".{MemberNameToken.Lexeme}" : $".{MemberNameToken.Lexeme}(...)";
    public override AstNode DeepClone() => this with
    {
        DotToken = (Token)DotToken.DeepClone(),
        MemberNameToken = (Token)MemberNameToken.DeepClone(),
        Arguments = Arguments?.Select(a => (Expr)a.DeepClone()).ToImmutableArray()
    };
}

/// <summary>
/// Rappresenta il controllo <c>expr HAS VALUE</c> su un tipo nullable.
/// Restituisce TRUE se il valore non è NIL/NULL.
/// </summary>
/// <param name="Expression">L'espressione nullable da controllare.</param>
/// <param name="HasToken">Il token HAS.</param>
public sealed record NullableHasValueExpr(Expr Expression, Token HasToken) : Expr(HasToken.Position)
{
    public override string ToString() => $"({Expression} HAS VALUE)";
    public override AstNode DeepClone() => new NullableHasValueExpr((Expr)Expression.DeepClone(), (Token)HasToken.DeepClone());
}

/// <summary>
/// Rappresenta l'accesso <c>expr.VALUE</c> per estrarre il valore sottostante di un nullable.
/// Equivale a un accesso al campo VALUE su un tipo nullable.
/// </summary>
/// <param name="Expression">L'espressione nullable.</param>
/// <param name="ValueToken">Il token VALUE.</param>
public sealed record NullableValueExpr(Expr Expression, Token ValueToken) : Expr(ValueToken.Position)
{
    public override string ToString() => $"({Expression}.VALUE)";
    public override AstNode DeepClone() => new NullableValueExpr((Expr)Expression.DeepClone(), (Token)ValueToken.DeepClone());
}

/// <summary>Tipo di accesso null-conditional.</summary>
public enum NullConditionalKind { Member, Call, Index }

/// <summary>Accesso null-conditional: <c>expr?.member</c>, <c>expr?.(args)</c>, <c>expr?[idx]</c>.</summary>
public sealed record NullConditionalAccessExpr(
    Expr Expression,
    NullConditionalKind Kind,
    Token? MemberToken,
    ImmutableArray<Expr>? Arguments
) : Expr(Expression.Position)
{
    public override string ToString() => Kind switch
    {
        NullConditionalKind.Member => $"({Expression}?.{MemberToken?.Lexeme})",
        NullConditionalKind.Call => $"({Expression}?.(...))",
        NullConditionalKind.Index => $"({Expression}?[...])",
        _ => $"({Expression}?.?)"
    };

    public override AstNode DeepClone() => new NullConditionalAccessExpr(
        (Expr)Expression.DeepClone(),
        Kind,
        (Token?)MemberToken?.DeepClone(),
        Arguments?.Select(a => (Expr)a.DeepClone()).ToImmutableArray()
    );
}

/// <summary>
/// Safe cast: <c>expr AS? TypeName</c> — returns converted value or NIL on failure.
/// Result type is <c>NullableType(TargetType)</c>.
/// </summary>
public sealed record SafeCastExpr(Expr Expression, DataType TargetType, Token AsToken) : Expr(AsToken.Position)
{
    public override string ToString() => $"({Expression} AS? {TargetType})";
    public override AstNode DeepClone() =>
        new SafeCastExpr((Expr)Expression.DeepClone(), (DataType)TargetType.DeepClone(), (Token)AsToken.DeepClone());
}

public sealed record LinqOrderByClause(Expr Expression, bool Descending = false) : AstNode(Expression.Position)
{
    public override string ToString() => $"ORDER BY {Expression}{(Descending ? " DESC" : " ASC")}";
    public override AstNode DeepClone() => new LinqOrderByClause((Expr)Expression.DeepClone(), Descending);
}

/// <summary>
/// Rappresenta una query LINQ nel linguaggio BASIC.
/// Sintassi: FROM item IN source [WHERE condition] [ORDER BY expr [ASC|DESC]] [SELECT expr]
/// </summary>
public sealed record LinqQueryExpr(
    Token FromToken,
    Token ItemVar,
    int ItemSymbolId,
    Expr Source,
    Expr? WhereClause = null,
    ImmutableArray<LinqOrderByClause>? OrderBy = null,
    Expr? SelectClause = null,
    Expr? GroupByClause = null) : Expr(FromToken.Position)
{
    public override string ToString()
    {
        var parts = new List<string> { $"FROM {ItemVar.Lexeme} IN {Source}" };
        if (WhereClause != null) parts.Add($"WHERE {WhereClause}");
        if (OrderBy is { } orderByClauses)
        {
            var orderParts = new List<string>();
            foreach (var o in orderByClauses)
                orderParts.Add(o.ToString());
            parts.Add(string.Join(", ", orderParts));
        }
        if (GroupByClause != null) parts.Add($"GROUP BY {GroupByClause}");
        if (SelectClause != null) parts.Add($"SELECT {SelectClause}");
        return string.Join(" ", parts);
    }

    public override AstNode DeepClone() => new LinqQueryExpr(
        (Token)FromToken.DeepClone(),
        (Token)ItemVar.DeepClone(),
        ItemSymbolId,
        (Expr)Source.DeepClone(),
        (Expr?)WhereClause?.DeepClone(),
        OrderBy?.Select(o => (LinqOrderByClause)o.DeepClone()).ToImmutableArray(),
        (Expr?)SelectClause?.DeepClone(),
        (Expr?)GroupByClause?.DeepClone());
}

/// <summary>
/// Rappresenta una singola clausola CASE all'interno di un'espressione SWITCH.
/// </summary>
/// <param name="Condition">Il valore da confrontare con l'input dello SWITCH.</param>
/// <param name="Result">Il risultato restituito se il CASE corrisponde.</param>
public sealed record SwitchCaseClause(Expr Condition, Expr Result) : AstNode(Condition.Position)
{
    public override AstNode DeepClone() => new SwitchCaseClause(
        (Expr)Condition.DeepClone(),
        (Expr)Result.DeepClone());
}

/// <summary>
/// Espressione SWITCH: valuta un'espressione e restituisce il risultato del CASE corrispondente.
/// Sintassi: SWITCH(expr) { CASE val1: result1, CASE val2: result2, ELSE: default }
/// </summary>
public sealed record SwitchExpr(Expr Value, ImmutableArray<SwitchCaseClause> Cases, Expr? ElseCase) : Expr(Value.Position)
{
    public override string ToString()
    {
        var cases = string.Join(", ", Cases.Select(c => $"CASE {c.Condition}: {c.Result}"));
        var elsePart = ElseCase != null ? $", ELSE: {ElseCase}" : "";
        return $"SWITCH({Value}) {{{cases}{elsePart}}}";
    }

    public override AstNode DeepClone() => new SwitchExpr(
        (Expr)Value.DeepClone(),
        [..Cases.Select(c => (SwitchCaseClause)c.DeepClone())],
        (Expr?)ElseCase?.DeepClone());
}
