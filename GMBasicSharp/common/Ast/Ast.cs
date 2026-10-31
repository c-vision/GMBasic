// FILE: Interpreter.Ast.cs

using System.Text;

// Aggiunto per la serializzazione polimorfica

// ReSharper disable InvalidXmlDocComment
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming
namespace Basic;






/// <summary>
/// Rappresenta un'espressione che viene usata come un'istruzione.
/// Il risultato dell'espressione viene scartato dopo la valutazione.
/// Questo è tipicamente usato per chiamate a funzioni o metodi il cui valore di ritorno è ignorato.
/// </summary>
/// <param name="Expression">L'espressione da valutare.</param>
public sealed record ExpressionStmt(Expr Expression) : Stmt(Expression.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => Expression.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo ExpressionStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ExpressionStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new ExpressionStmt((Expr)Expression.DeepClone());
    }
}


/// <summary>
/// Definisce la modalità di un'istruzione IMPORT.
/// </summary>
/// <summary>
/// Rappresenta un'istruzione interna della VM, non generata direttamente dal parser,
/// il cui unico scopo è orchestrare la valutazione di una lista di espressioni di indici.
/// Funge da "chiamata a sub-routine" per l'helper a stati ExecuteGetIndicesStep.
/// </summary>
public sealed record EvaluateIndicesStmt(ImmutableArray<Expr> IndexExpressions)
    : Stmt(IndexExpressions.FirstOrDefault()?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"(VM-Internal: Evaluate {IndexExpressions.Length} indices)";

    /// <summary>
    /// Crea una copia profonda di questo nodo EvaluateIndicesStmt.
    /// </summary>
    /// <returns>Una nuova istanza di EvaluateIndicesStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedExpressions = IndexExpressions.Select(e => (Expr)e.DeepClone()).ToImmutableArray();
        return new EvaluateIndicesStmt(clonedExpressions);
    }
}

/// <summary>
/// Contiene le informazioni complete sulla posizione di un'etichetta nel codice.
/// </summary>
/// <param name="BlockStatements">
/// Se l'etichetta si trova all'interno di un blocco annidato (corpo di IF/CASE/TRY/USING/WITH),
/// contiene il riferimento all'array di statement locale a quel blocco (lo stesso array usato
/// per creare il frame di blocco a runtime — vedi <see cref="Executor.CallFrame.LocalBlockBody"/>).
/// <c>StatementIndex</c> in questo caso è relativo a QUESTO array, non a <c>Procedure.Body</c>.
/// È <c>null</c> per le etichette a livello di procedura/modulo (il caso più comune).
/// </param>
public sealed record LabelLocation(ProcedureDefinition? Procedure, int Line, int StatementIndex, ImmutableArray<Stmt>? BlockStatements = null);

/// <summary>
/// Rappresenta un'etichetta definita dall'utente nel codice.
/// </summary>
public sealed record LabelStmt(Token Label) : Stmt(Label.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Label.Lexeme}:";

    /// <summary>
    /// Crea una copia profonda di questo nodo LabelStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LabelStmt con una copia del suo token Label.</returns>
    public override AstNode DeepClone()
    {
        return new LabelStmt((Token)Label.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione LET.
/// </summary>
public sealed record LetStmt(ImmutableArray<Assignment> Assignments, Token? LetToken)
    : Stmt(LetToken?.Position ?? Assignments[0].Target.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Join(", ", Assignments);

    /// <summary>
    /// Crea una copia profonda di questo nodo LetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedAssignments = Assignments
            .Select(a => new Assignment((Target)a.Target.DeepClone(), (Expr)a.Value.DeepClone())).ToImmutableArray();
        var clonedToken = (Token?)LetToken?.DeepClone();
        return new LetStmt(clonedAssignments, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione REDIM per ridimensionare un array esistente.
/// VERSIONE AGGIORNATA: Utilizza un Target, supporta PRESERVE e una
/// clausola di inizializzazione opzionale.
/// </summary>
public sealed record RedimStmt(
    Target ArrayTarget,
    Token? Preserve,
    ImmutableArray<Expr> Dimensions,
    Expr? Initializer,
    Token RedimToken
) : Stmt(RedimToken.Position)
{
    // Il SymbolId è ora implicitamente parte del Target e verrà risolto dal Linker.

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder();
        sb.Append("REDIM ");
        if (Preserve is not null) sb.Append("PRESERVE ");
        sb.Append(ArrayTarget);
        if (!Dimensions.IsEmpty)
        {
            sb.Append($"({string.Join(", ", Dimensions)})");
        }
        if (Initializer is not null)
        {
            sb.Append($" = {Initializer}");
        }
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo RedimStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RedimStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedTarget = (Target)ArrayTarget.DeepClone();
        var clonedPreserve = (Token?)Preserve?.DeepClone();
        var clonedDimensions = Dimensions.Select(d => (Expr)d.DeepClone()).ToImmutableArray();
        var clonedInitializer = (Expr?)Initializer?.DeepClone();
        var clonedRedimToken = (Token)RedimToken.DeepClone();
        return new RedimStmt(clonedTarget, clonedPreserve, clonedDimensions, clonedInitializer, clonedRedimToken);
    }
}

/// <summary>
/// Rappresenta un'istruzione di dichiarazione completa come DIM, LOCAL, GLOBAL, o STATIC.
/// </summary>
public sealed record DeclarationStmt(Token Keyword, ImmutableArray<VariableDeclaration> Declarations)
    : Stmt(Keyword.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Keyword.Lexeme} {string.Join(", ", Declarations)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DeclarationStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DeclarationStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedKeyword = (Token)Keyword.DeepClone();
        var clonedDeclarations = Declarations.Select(d => (VariableDeclaration)d.DeepClone()).ToImmutableArray();
        return new DeclarationStmt(clonedKeyword, clonedDeclarations);
    }
}

/// <summary>
/// Rappresenta l'istruzione READ.
/// </summary>
public sealed record ReadStmt(ImmutableArray<Target> Variables, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Join(", ", Variables);

    /// <summary>
    /// Crea una copia profonda di questo nodo ReadStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ReadStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedVariables = Variables.Select(v => (Target)v.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new ReadStmt(clonedVariables, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione DATA.
/// </summary>
public sealed record DataStmt(ImmutableArray<Expr> Values, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Join(", ", Values);

    /// <summary>
    /// Crea una copia profonda di questo nodo DataStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DataStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedValues = Values.Select(v => (Expr)v.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new DataStmt(clonedValues, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione RESTORE [etichetta | numero_linea].
/// </summary>
public sealed record RestoreStmt(Token Token, Token? Target) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Target is null ? "RESTORE" : $"RESTORE {Target.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RestoreStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RestoreStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedToken = (Token)Token.DeepClone();
        var clonedTarget = (Token?)Target?.DeepClone();
        return new RestoreStmt(clonedToken, clonedTarget);
    }
}

/// <summary>
/// Rappresenta l'istruzione SWAP per scambiare il contenuto di due variabili.
/// </summary>
public sealed record SwapStmt(Target Left, Target Right, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Left}, {Right}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SwapStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SwapStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedLeft = (Target)Left.DeepClone();
        var clonedRight = (Target)Right.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new SwapStmt(clonedLeft, clonedRight, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione RANDOMIZE per inizializzare il generatore di numeri casuali.
/// </summary>
public sealed record RandomizeStmt(Expr? Seed, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Seed?.ToString() ?? string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo RandomizeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RandomizeStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSeed = (Expr?)Seed?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new RandomizeStmt(clonedSeed, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione SYSTEM.SHELL (o legacy SHELL) — esegue un comando di sistema.
/// Sintassi: SHELL "comando" (opt-in: richiede SET SHELL ON).
/// </summary>
public sealed record SystemShellStmt(Expr Command, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SHELL {Command}";

    public override AstNode DeepClone() => new SystemShellStmt((Expr)Command.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione REM.
/// </summary>
public sealed record RemStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Token.Literal as string ?? string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo RemStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RemStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new RemStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione END.
/// </summary>
public sealed record EndStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo EndStmt.
    /// </summary>
    /// <returns>Una nuova istanza di EndStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new EndStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione KEYGEN per generare una coppia di chiavi asimmetriche.
/// </summary>
/// <param name="Algorithm">L'espressione che definisce l'algoritmo (es. "RSA").</param>
/// <param name="PrivateKeyHandle">La variabile di destinazione per l'handle della chiave privata.</param>
/// <param name="PublicKeyHandle">La variabile di destinazione per l'handle della chiave pubblica.</param>
/// <param name="KeySize">L'espressione opzionale per la dimensione della chiave (solo per RSA).</param>
/// <param name="Token">Il token KEYGEN per riferimento.</param>
public sealed record KeyGenStmt(
    Expr Algorithm,
    Target PrivateKeyHandle,
    Target PublicKeyHandle,
    Expr? KeySize,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var keySizeString = KeySize != null ? $", KeySize={KeySize}" : "";
        return $"KEYGEN {Algorithm}, Private={PrivateKeyHandle}, Public={PublicKeyHandle}{keySizeString}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo KeyGenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di KeyGenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedAlgorithm = (Expr)Algorithm.DeepClone();
        var clonedPrivateKeyHandle = (Target)PrivateKeyHandle.DeepClone();
        var clonedPublicKeyHandle = (Target)PublicKeyHandle.DeepClone();
        var clonedKeySize = (Expr?)KeySize?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new KeyGenStmt(clonedAlgorithm, clonedPrivateKeyHandle, clonedPublicKeyHandle, clonedKeySize,
            clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione KEYCLOSE per chiudere un handle di chiave asimmetrica.
/// </summary>
/// <param name="KeyHandle">L'espressione che definisce l'handle della chiave da chiudere.</param>
/// <param name="Token">Il token KEYCLOSE per riferimento.</param>
public sealed record KeyCloseStmt(Expr KeyHandle, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"KEYCLOSE {KeyHandle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo KeyCloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di KeyCloseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedKeyHandle = (Expr)KeyHandle.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new KeyCloseStmt(clonedKeyHandle, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione STOP, che mette in pausa l'esecuzione del programma.
/// </summary>
/// <param name="Token">Il token 'STOP' per riferimento.</param>
public sealed record StopStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "STOP";

    /// <summary>
    /// Crea una copia profonda di questo nodo StopStmt.
    /// </summary>
    /// <returns>Una nuova istanza di StopStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new StopStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `RUN command` o `! command`.
/// Esegue un comando esterno del sistema operativo.
/// </summary>
/// <param name="Command">L'espressione che restituisce la stringa del comando da eseguire.</param>
/// <param name="Token">Il token `RUN` o `!` usato per il reporting degli errori.</param>
public sealed record RunStmt(Expr Command, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"RUN {Command}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RunStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RunStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCommand = (Expr)Command.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new RunStmt(clonedCommand, clonedToken);
    }
}

/// <summary>
/// Rappresenta il comando `JSON.SAVE sorgente, percorso$`.
/// Salva una variabile (tipicamente un oggetto JSON, UDT o DICT) in un file di testo in formato JSON.
/// </summary>
/// <param name="Source">L'espressione che restituisce il dato da salvare.</param>
/// <param name="FilePath">L'espressione che restituisce il percorso del file di destinazione.</param>
/// <param name="Token">Il token `SAVE` per riferimento.</param>
[JsonDerivedType(typeof(JsonSaveStmt), nameof(JsonSaveStmt))]
public sealed record JsonSaveStmt(Expr Source, Expr FilePath, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"JSON.SAVE {Source}, {FilePath}";

    /// <summary>
    /// Crea una copia profonda di questo nodo JsonSaveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di JsonSaveStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSource = (Expr)Source.DeepClone();
        var clonedFilePath = (Expr)FilePath.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new JsonSaveStmt(clonedSource, clonedFilePath, clonedToken);
    }
}

/// <summary>
/// Rappresenta il comando `JSON.WriteValue key$, value [, section$]`.
/// Scrive un singolo valore nel file di configurazione app.settings.json.
/// </summary>
/// <param name="Key">L'espressione che definisce la chiave dell'impostazione.</param>
/// <param name="Value">L'espressione che definisce il valore da scrivere.</param>
/// <param name="Section">L'espressione opzionale per la sezione di destinazione.</param>
/// <param name="Token">Il token `WriteValue` per riferimento.</param>
public sealed record JsonWriteValueStmt(Expr Key, Expr Value, Expr? Section, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"JSON.WriteValue {Key}, {Value}{(Section is not null ? $", {Section}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo JsonWriteValueStmt.
    /// </summary>
    /// <returns>Una nuova istanza di JsonWriteValueStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedKey = (Expr)Key.DeepClone();
        var clonedValue = (Expr)Value.DeepClone();
        var clonedSection = (Expr?)Section?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new JsonWriteValueStmt(clonedKey, clonedValue, clonedSection, clonedToken);
    }
}

/// <summary>
/// Rappresenta il comando `JSON.WriteSection source_udt_o_dict [, section$]`.
/// Scrive i valori da una variabile UDT o DICT in una sezione di configurazione.
/// </summary>
/// <param name="Source">L'espressione (variabile UDT o DICT) da cui leggere i dati.</param>
/// <param name="Section">L'espressione opzionale per la sezione di destinazione.</param>
/// <param name="Token">Il token `WriteSection` per riferimento.</param>
public sealed record JsonWriteSectionStmt(Expr Source, Expr? Section, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"JSON.WriteSection {Source}{(Section is not null ? $", {Section}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo JsonWriteSectionStmt.
    /// </summary>
    /// <returns>Una nuova istanza di JsonWriteSectionStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSource = (Expr)Source.DeepClone();
        var clonedSection = (Expr?)Section?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new JsonWriteSectionStmt(clonedSource, clonedSection, clonedToken);
    }
}

/// <summary>
/// Rappresenta un'istruzione di dichiarazione di costante (CONST).
/// </summary>
[JsonDerivedType(typeof(ConstStmt), nameof(ConstStmt))]
public sealed record ConstStmt(Token Keyword, ImmutableArray<ConstDeclaration> Declarations) : Stmt(Keyword.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Keyword.Lexeme} {string.Join(", ", Declarations)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConstStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConstStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedKeyword = (Token)Keyword.DeepClone();
        var clonedDeclarations = Declarations.Select(d => (ConstDeclaration)d.DeepClone()).ToImmutableArray();
        return new ConstStmt(clonedKeyword, clonedDeclarations);
    }
}

/// <summary>
/// Rappresenta un comando non qualificato che è sintatticamente valido ma semanticamente ambiguo
/// al momento del parsing iniziale (es. GET, che potrebbe appartenere a CONSOLE o DBF).
/// Questo nodo è un segnaposto temporaneo che deve essere risolto dal Linker/Rewriter
/// nella fase di compilazione successiva.
/// </summary>
/// <param name="CommandToken">Il token del comando ambiguo (es. 'GET').</param>
/// <param name="Arguments">La lista di argomenti così come sono stati parsati.</param>
[JsonDerivedType(typeof(AmbiguousMethodStmt), nameof(AmbiguousMethodStmt))]
public sealed record AmbiguousMethodStmt(Token CommandToken, ImmutableArray<Expr> Arguments)
    : Stmt(CommandToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"Ambiguous: {CommandToken.Lexeme}({string.Join(", ", Arguments)})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AmbiguousMethodStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AmbiguousMethodStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new AmbiguousMethodStmt(
            (Token)CommandToken.DeepClone(),
            [..Arguments.Select(a => (Expr)a.DeepClone())]
        );
    }
}

/// <summary>
/// Rappresenta un'istruzione di macro di comando (es. &cCommand).
/// L'Executor valuterà l'espressione, compilerà la stringa risultante in un nuovo Stmt
/// e lo eseguirà nello scope corrente.
/// </summary>
/// <param name="Expression">L'espressione che restituisce la stringa di codice da eseguire.</param>
/// <param name="Token">Il token '&' per riferimento alla posizione.</param>
public sealed record MacroStmt(Expr Expression, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"&{Expression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo MacroStmt.
    /// </summary>
    /// <returns>Una nuova istanza di MacroStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new MacroStmt((Expr)Expression.DeepClone(), (Token)Token.DeepClone());
    }
}