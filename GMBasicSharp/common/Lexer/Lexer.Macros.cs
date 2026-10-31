// FILE: Interpreter.Lexer.Macros.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Rappresenta la definizione di una macro.
/// Contiene il nome, la lista dei nomi dei parametri (che può essere vuota) 
/// e la sequenza di token che costituisce il corpo della macro.
/// </summary>
/// <param name="Name">Il nome della macro, usato per identificarla.</param>
/// <param name="Parameters">La lista immutabile dei nomi dei parametri. È vuota per le macro di semplice sostituzione.</param>
/// <param name="Body">La sequenza immutabile di token che costituisce il corpo di sostituzione della macro.</param>
internal sealed record MacroDefinition(
    string Name,
    ImmutableArray<string> Parameters,
    ImmutableArray<Token> Body);

/// <summary>
/// Esegue la passata di espansione delle macro. Opera su un flusso di token grezzo prodotto dal Lexer
/// e restituisce un nuovo flusso di token con le definizioni #DEF rimosse e le chiamate a macro espanse.
/// </summary>
internal sealed class MacroExpander
{
    private readonly ImmutableArray<Token> _inputTokens;
    private readonly List<Token> _outputTokens = new();
    private readonly Dictionary<string, MacroDefinition> _macros = new(StringComparer.OrdinalIgnoreCase);
    private int _current;

    /// <summary>
    /// Inizializza una nuova istanza di MacroExpander.
    /// </summary>
    /// <param name="tokens">La sequenza di token grezza proveniente dal Lexer.</param>
    private MacroExpander(ImmutableArray<Token> tokens)
    {
        _inputTokens = tokens;
    }

    /// <summary>
    /// Punto di ingresso statico per eseguire l'espansione delle macro.
    /// </summary>
    /// <param name="tokens">La sequenza di token grezza.</param>
    /// <returns>Una nuova sequenza di token con le macro espanse.</returns>
    public static ImmutableArray<Token> Expand(ImmutableArray<Token> tokens)
    {
        var expander = new MacroExpander(tokens);
        return expander.Process();
    }

    /// <summary>
    /// Processa l'intero flusso di token di input, distinguendo tra definizioni di macro,
    /// chiamate a macro e altri token.
    /// Questa versione corretta garantisce che il token Eof (End of File) venga sempre
    /// aggiunto alla fine del flusso di token restituito.
    /// </summary>
    /// <returns>Un array immutabile di token con le macro espanse.</returns>
    private ImmutableArray<Token> Process()
    {
        while (!IsAtEnd())
        {
            var token = Peek();
            
            // Se troviamo un token #DEFINE, lo parsiamo come definizione di macro.
            if (token.Kind == TokenKind.Define)
            {
                ParseMacroDefinition();
            }
            // Se troviamo un identificatore che corrisponde a una macro definita...
            else if (token.Kind == TokenKind.Identifier && _macros.TryGetValue(token.Lexeme, out var definition))
            {
                // ...dobbiamo determinare se è una chiamata valida.
                if (definition.Parameters.IsEmpty)
                {
                    // È una macro senza parametri: sostituzione semplice.
                    Advance(); // Consuma il nome della macro.
                    _outputTokens.AddRange(definition.Body);
                }
                else
                {
                    // È una macro con parametri. È una chiamata valida solo se seguita da '('.
                    if (CheckNextNonWhitespace()?.Kind == TokenKind.LeftParen)
                    {
                        ExpandMacroCall(definition);
                    }
                    else
                    {
                        // È solo un identificatore con lo stesso nome, non una chiamata.
                        _outputTokens.Add(Advance());
                    }
                }
            }
            else
            {
                // Token normale, aggiungilo all'output.
                _outputTokens.Add(Advance());
            }
        }

        // Aggiunge il token Eof alla fine del flusso di output.
        // L'ultimo token nel flusso di input è sempre Eof, e il loop `while` si ferma
        // prima di processarlo. Lo aggiungiamo esplicitamente qui per garantire
        // che ogni flusso di token sia terminato correttamente.
        if (_current < _inputTokens.Length && _inputTokens[_current].Kind == TokenKind.Eof)
        {
            _outputTokens.Add(_inputTokens[_current]);
        }

        return [.._outputTokens];
    }

    /// <summary>
    /// Parsa una definizione di macro `#DEF NOME(A, B) (A + B)` o `#DEF NOME corpo`.
    /// Consuma i token della definizione dall'input senza aggiungerli all'output.
    /// </summary>
    private void ParseMacroDefinition()
    {
        Advance(); // Consuma #DEF

        var nameToken = Consume(TokenKind.Identifier, "Expected macro name after #DEF.");
        var parameters = ImmutableArray.CreateBuilder<string>();

        // Controlla se è una macro con parametri.
        if (Check(TokenKind.LeftParen))
        {
            Advance(); // Consuma '('

            if (!Check(TokenKind.RightParen))
            {
                do
                {
                    parameters.Add(Consume(TokenKind.Identifier, "Expected parameter name.").Lexeme);
                } while (Match(TokenKind.Comma));
            }

            Consume(TokenKind.RightParen, "Expected ')' after macro parameters.");
        }

        // Parsa il corpo della macro fino alla fine della riga.
        var body = ImmutableArray.CreateBuilder<Token>();
        while (!IsAtEnd() && Peek().Kind != TokenKind.Newline)
        {
            body.Add(Advance());
        }

        _macros[nameToken.Lexeme] = new MacroDefinition(nameToken.Lexeme, parameters.ToImmutable(), body.ToImmutable());
    }

    /// <summary>
    /// Riconosce ed espande una chiamata a macro con parametri, sostituendola con il corpo della macro
    /// in cui i parametri sono stati rimpiazzati dagli argomenti.
    /// </summary>
    /// <param name="definition">La definizione della macro da espandere.</param>
    private void ExpandMacroCall(MacroDefinition definition)
    {
        var macroToken = Advance(); // Consuma il nome della macro

        Consume(TokenKind.LeftParen, $"Expected '(' after macro call to '{macroToken.Lexeme}'.");

        var arguments = new List<ImmutableArray<Token>>();
        if (!Check(TokenKind.RightParen))
        {
            do
            {
                arguments.Add(ParseMacroArgument());
            } while (Match(TokenKind.Comma));
        }

        Consume(TokenKind.RightParen, "Expected ')' to close macro arguments.");

        if (arguments.Count != definition.Parameters.Length)
        {
            throw new BasicException(macroToken,
                $"Macro '{macroToken.Lexeme}' expects {definition.Parameters.Length} arguments, but {arguments.Count} were provided.");
        }

        // Sostituzione
        foreach (var bodyToken in definition.Body)
        {
            var paramIndex = -1;
            if (bodyToken.Kind == TokenKind.Identifier)
            {
                paramIndex = definition.Parameters.IndexOf(bodyToken.Lexeme);
            }

            if (paramIndex != -1)
            {
                // Questo è un parametro da sostituire. Aggiungi i token dell'argomento corrispondente.
                _outputTokens.AddRange(arguments[paramIndex]);
            }
            else
            {
                // Questo è un token letterale del corpo della macro. Aggiungilo così com'è.
                _outputTokens.Add(bodyToken);
            }
        }
    }
    
    /// <summary>
    /// Parsa un singolo argomento di una chiamata a macro. Un argomento è una sequenza di token
    /// che termina con una virgola o una parentesi chiusa, rispettando le parentesi annidate.
    /// </summary>
    /// <returns>Un array immutabile di token che costituisce l'argomento.</returns>
    private ImmutableArray<Token> ParseMacroArgument()
    {
        var argumentTokens = ImmutableArray.CreateBuilder<Token>();
        var parenDepth = 0;

        while (!IsAtEnd())
        {
            var token = Peek();
            if (token.Kind == TokenKind.LeftParen) parenDepth++;
            if (token.Kind == TokenKind.RightParen) parenDepth--;

            if (parenDepth < 0 ||
                (parenDepth == 0 && (token.Kind == TokenKind.Comma || token.Kind == TokenKind.RightParen)))
            {
                break;
            }

            argumentTokens.Add(Advance());
        }

        if (argumentTokens.Count == 0)
        {
            throw new BasicException(Peek(), "Macro argument cannot be empty.");
        }

        return argumentTokens.ToImmutable();
    }

    // --- Metodi Helper del Parser ---

    /// <summary>
    /// Verifica se il parser ha raggiunto la fine del flusso di token.
    /// </summary>
    /// <returns>True se non ci sono più token da processare; altrimenti false.</returns>
    private bool IsAtEnd() => _current >= _inputTokens.Length || _inputTokens[_current].Kind == TokenKind.Eof;
    
    /// <summary>
    /// Restituisce il token corrente senza consumarlo (lookahead di 0).
    /// </summary>
    /// <returns>Il token alla posizione corrente.</returns>
    private Token Peek() => _inputTokens[_current];
    
    /// <summary>
    /// Consuma il token corrente e avanza il puntatore al token successivo.
    /// </summary>
    /// <returns>Il token che è stato appena consumato.</returns>
    private Token Advance() => _inputTokens[_current++];

    /// <summary>
    /// Controlla se il token corrente è del tipo specificato, senza consumarlo.
    /// </summary>
    /// <param name="kind">Il tipo di token da controllare.</param>
    /// <returns>True se il token corrente corrisponde al tipo; altrimenti false.</returns>
    private bool Check(TokenKind kind) => !IsAtEnd() && Peek().Kind == kind;

    /// <summary>
    /// Controlla se il token corrente corrisponde al tipo specificato. Se sì, lo consuma.
    /// </summary>
    /// <param name="kind">Il tipo di token da far corrispondere.</param>
    /// <returns>True se il token è stato trovato e consumato; altrimenti false.</returns>
    private bool Match(TokenKind kind)
    {
        if (Check(kind))
        {
            Advance();
            return true;
        }

        return false;
    }

    /// <summary>
    /// Consuma il token corrente se corrisponde al tipo atteso. Altrimenti, lancia un'eccezione.
    /// </summary>
    /// <param name="kind">Il tipo di token atteso.</param>
    /// <param name="message">Il messaggio di errore da lanciare in caso di mancata corrispondenza.</param>
    /// <returns>Il token consumato.</returns>
    /// <exception cref="BasicException">Lanciata se il token corrente non è del tipo atteso.</exception>
    private Token Consume(TokenKind kind, string message)
    {
        if (Check(kind)) return Advance();
        throw new BasicException(Peek(), message, ErrorCode.SyntaxError);
    }
    
    /// <summary>
    /// Esegue un lookahead per il prossimo token che non sia un newline, senza consumare token.
    /// </summary>
    /// <returns>Il prossimo token significativo, o null se si raggiunge la fine.</returns>
    private Token? CheckNextNonWhitespace()
    {
        var tempCurrent = _current;
        while (tempCurrent < _inputTokens.Length)
        {
            var token = _inputTokens[tempCurrent];
            if (token.Kind is not TokenKind.Newline)
            {
                return token;
            }
            tempCurrent++;
        }
        return null;
    }
}