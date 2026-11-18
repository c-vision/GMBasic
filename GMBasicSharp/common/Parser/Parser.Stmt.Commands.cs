// FILE: Interpreter.Parser.Stmt.Commands.cs

// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene le versioni dei metodi di parsing per i comandi principali,
/// specifiche per la Fase 1 (Parsing Puro). Questi metodi costruiscono l'AST
/// senza interagire con la SymbolTable.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Helper per consumare un'intera riga logica, trattandola come un commento.
    /// Usato da ParseStatement per ignorare le direttive (IMPORT, REQUIRE) durante
    /// la passata di parsing principale, dato che sono già state processate dalla pre-scansione.
    /// </summary>
    /// <returns>Un nodo RemStmt che funge da segnaposto no-op.</returns>
    private RemStmt ParseDirectiveAsComment()
    {
        var remToken = Advance(); // Consuma 'REM' o l'apostrofo (che verrà gestito dal lexer)
        // Avanza finché non trova la fine della riga o del file.
        // Non consuma il Newline, che agisce da separatore di istruzioni.
        while (!Check(TokenKind.Newline) && !Check(TokenKind.Colon) && !IsAtEnd())
        {
            Advance();
        }

        return new RemStmt(remToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione di assegnazione (LET esplicito o implicito).
    /// Ora gestisce la sintassi `LET var AS TYPE = value`, che è una
    /// dichiarazione con inizializzazione, restituendo un DeclarationStmt in quel caso.
    /// </summary>
    /// <returns>Una lista di nodi Stmt. Di solito contiene un singolo LetStmt o DeclarationStmt.</returns>
    private IReadOnlyList<Stmt> ParseLetStatement()
    {
        Log($"[ParseLetStatement] ENTER. Current: {Peek()}");
        var letToken = Match(TokenKind.Let) ? Previous() : null;

        /* `LET` è un'assegnazione allo SCOPE CORRENTE (specifica Variables-Scope §3.1): fuori da
         * una procedura non c'è uno scope corrente più interno di quello di modulo, e la spec
         * dice esplicitamente «a livello di Namespace genera un errore di sintassi». Prima il
         * parser lo accettava e i backend lo trattavano come un'assegnazione (scarto fra
         * documento e implementazione, 2026-11-12): qui diventa un errore di compilazione, con lo
         * stesso testo sul parser C (`gmb_parser_core.c`). */
        if (letToken is not null && _functionContextStack.Count == 0)
            throw CreateError(letToken,
                "LET è valido solo dentro una procedura (SUB/FUNCTION/GENERATOR): a livello di modulo usare un'assegnazione (x = ...) oppure DIM/GLOBAL.");

        // --- LOOKAHEAD per determinare se è un'assegnazione o una dichiarazione ---
        var savedPosition = _current;
        var isDeclaration = false;
        try
        {
            ParseTarget(); // Consuma il target potenziale
            if (Check(TokenKind.As))
            {
                isDeclaration = true;
            }
        }
        catch (BasicException)
        {
            // Se il parsing del target fallisce, non può essere né l'uno né l'altro in questo contesto.
            // Lasciamo che il parser reale fallisca e lanci l'errore.
        }
        finally
        {
            _current = savedPosition; // Ripristina sempre la posizione
        }

        if (isDeclaration)
        {
            // È una dichiarazione con inizializzazione (LET var AS T = val)
            // Deleghiamo al parser di dichiarazioni che sa come gestire 'AS' e '='.
            var declarations = ParseDeclarationList();
            // Il parser di dichiarazioni non consuma la keyword 'LET', quindi usiamo 'DIM' come placeholder
            // se 'letToken' è nullo (assegnazione implicita). Il Linker potrebbe dover gestire questo.
            var keyword = letToken ?? new Token(TokenKind.Dim, "DIM", null, declarations[0].Position);
            var declStmt = new DeclarationStmt(keyword, declarations);
            Log($"[ParseLetStatement] EXIT. Parsed as DeclarationStmt. Next: {Peek()}");
            return [declStmt];
        }
        
        // È una semplice assegnazione (LET var = val)
        var assignments = new List<Assignment>();
        do
        {
            assignments.Add(ParseSingleSimpleAssignment());
        } while (Match(TokenKind.Comma));

        var result = new LetStmt([..assignments], letToken);
        Log($"[ParseLetStatement] EXIT. Created LetStmt with {assignments.Count} assignment(s). Next: {Peek()}");
        return [result];
    }

    /// <summary>
    /// [FASE 1] Metodo helper che parsa una singola assegnazione semplice (es. `x = 10`),
    /// utilizzando il parser di target specifico per la Fase 1.
    /// La sua logica è: `Target = Espressione`.
    /// </summary>
    /// <returns>Un oggetto `Assignment` che rappresenta la singola assegnazione.</returns>
    private Assignment ParseSingleSimpleAssignment()
    {
        Log($"[ParseSingleSimpleAssignment] ENTER. Current: {Peek()}");
        // 1. Parsa il lato sinistro (L-value) usando la logica "non golosa" di Fase 1.
        var target = ParseTarget();
        Log(
            $"[ParseSingleSimpleAssignment]   -> Parsed Target: {target.GetType().Name} ('{target}'). Next: {Peek()}");

        // 2. Consuma l'operatore di assegnazione.
        Consume(TokenKind.Equal, "Expected '=' in assignment statement.");
        Log($"[ParseSingleSimpleAssignment]   -> Consumed '='. Next: {Peek()}");

        // 3. Parsa il lato destro (R-value) usando la catena di espressioni di Fase 1.
        var value = ParseExpression();
        var assignment = new Assignment(target, value);
        Log($"[ParseSingleSimpleAssignment] EXIT. Parsed Value: {value.GetType().Name}. Next: {Peek()}");
        return assignment;
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione REDIM, utilizzando un Target per l'array.
    /// Supporta una clausola di inizializzazione opzionale (`= { ... }`).
    /// </summary>
    private RedimStmt ParseRedimStatement()
    {
        var redimToken = Consume(TokenKind.Redim, "Internal parser error.");
        var preserveToken = Match(TokenKind.Preserve) ? Previous() : null;
        
        var target = ParseTargetBase();

        // Le dimensioni sono ora opzionali se è presente un inizializzatore.
        var dimensions = ImmutableArray<Expr>.Empty;
        if (Match(TokenKind.LeftParen))
        {
            dimensions = ParseExpressionList(TokenKind.RightParen);
            Consume(TokenKind.RightParen, "Expected ')' after array dimensions.");
        }

        Expr? initializer = null;
        if (Match(TokenKind.Equal))
        {
            initializer = ParseExpression();
        }

        // Validazione: Se non ci sono né dimensioni né un inizializzatore, è un errore.
        if (dimensions.IsEmpty && initializer == null)
        {
            // Usa il nuovo helper CreateError che accetta un Target.
            throw CreateError(target, "REDIM requires either dimensions or an initializer expression.");
        }

        return new RedimStmt(target, preserveToken, dimensions, initializer, redimToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione SWAP.
    /// </summary>
    private SwapStmt ParseSwapStatement()
    {
        var swapToken = Consume(TokenKind.Swap, "Internal parser error.");
        var left = ParseTarget();
        Consume(TokenKind.Comma, "Expected ',' to separate variables in SWAP statement.");
        var right = ParseTarget();
        return new SwapStmt(left, right, swapToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione STOP.
    /// </summary>
    private StopStmt ParseStopStatement()
    {
        var stopToken = Consume(TokenKind.Stop, "Internal parser error: Expected STOP keyword.");
        return new StopStmt(stopToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione RUN comando o ! comando.
    /// </summary>
    private RunStmt ParseRunStatement()
    {
        var runToken = Advance(); // Consuma RUN o !
        var commandExpression = ParseExpression();
        return new RunStmt(commandExpression, runToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione END.
    /// </summary>
    private Stmt ParseEndStatement()
    {
        var endToken = Consume(TokenKind.End, "Internal parser error: Expected 'END'.");
        // END WORKFLOW — chiude la definizione del workflow corrente (prima non
        // riconosciuto → WorkflowEndStmt non era mai generato, il workflow restava
        // in definizione e STATUS/STEPS tornavano vuoti).
        if (Peek().Kind == TokenKind.WorkflowStatement)
        {
            Advance();
            return new WorkflowEndStmt(endToken);
        }
        return new EndStmt(endToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione REQUIRE.
    /// </summary>
    private RequireStmt ParseRequireStatement()
    {
        var requireToken = Consume(TokenKind.Require, "Internal parser error: Expected 'REQUIRE' keyword.");
        if (Peek().Kind != TokenKind.StringLiteral)
            throw CreateError(Peek(), "Expected a package name as a string literal after REQUIRE.");
        var packageName = new StringExpr((string)Peek().Literal!, Peek());
        Advance();

        StringExpr? versionSpecifier = null;
        if (!Match(TokenKind.Version)) return new RequireStmt(packageName, versionSpecifier, requireToken);
        if (Peek().Kind != TokenKind.StringLiteral)
            throw CreateError(Peek(), "Expected a version specifier as a string literal after 'VERSION'.");
        versionSpecifier = new StringExpr((string)Peek().Literal!, Peek());
        Advance();

        return new RequireStmt(packageName, versionSpecifier, requireToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione YIELD espressione.
    /// </summary>
    private YieldStmt ParseYieldStatement()
    {
        var yieldToken = Consume(TokenKind.Yield, "Internal parser error: Expected 'YIELD' keyword.");
        var valueToYield = ParseExpression();
        return new YieldStmt(valueToYield, yieldToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione di macro di comando.
    /// </summary>
    private MacroStmt ParseMacroStatement()
    {
        var ampersandToken = Consume(TokenKind.Ampersand, "Internal parser error: Expected '&'.");
        var expression = ParseExpression();
        return new MacroStmt(expression, ampersandToken);
    }

    // ========================================================================
    // --- STATIC COMMANDS ---
    // ========================================================================

    /// <summary>
    /// [FASE 1] Parsa un'istruzione RANDOMIZE [espressione].
    /// </summary>
    public static RandomizeStmt ParseRandomizeStatement(Parser parser)
    {
        var randomizeToken = parser.Consume(TokenKind.Randomize, "Internal parser error: Expected 'RANDOMIZE'.");
        Expr? seed = null;
        if (!parser.Check(TokenKind.Newline) && !parser.Check(TokenKind.Colon) && !parser.IsAtEnd())
        {
            seed = parser.ParseExpression();
        }

        return new RandomizeStmt(seed, randomizeToken);
    }

    /// <summary>
    /// Parsa SYSTEM.SHELL "comando" (o legacy SHELL "comando") — comando di sistema.
    /// Opt-in: richiede SET SHELL ON (il runtime lancia PermissionDenied se disabilitato).
    /// </summary>
    public static Stmt ParseSystemShellStatement(Parser parser, string? ns)
    {
        Token shellToken;
        if (parser.Peek().Kind == TokenKind.Shell)
            shellToken = parser.Advance();
        else
            shellToken = parser.Previous();
        if (parser.Check(TokenKind.Newline) || parser.Check(TokenKind.Colon) || parser.IsAtEnd())
            throw CreateError(shellToken, "SHELL richiede un comando: SHELL \"comando\"");
        var commandExpr = parser.ParseExpression();
        return new SystemShellStmt(commandExpr, shellToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione di controllo audio (es. SOUND STOP "bgm").
    /// </summary>
    private static AudioControlStmt ParseAudioControlStatement(Parser parser)
    {
        var commandToken = parser.Advance(); // SOUND o PLAY
        var actionToken = parser.Advance(); // STOP, PAUSE, o RESUME
        Expr? argument = null;
        if (actionToken.Kind == TokenKind.Stop && parser.Match(TokenKind.All))
        {
            // No argument for 'ALL'
        }
        else if (!parser.IsAtEndOfStatement())
        {
            argument = parser.ParseExpression();
        }

        return new AudioControlStmt(commandToken, actionToken, argument);
    }

    /// <summary>
    /// [FASE 1] Parsa la sintassi SOUND.
    /// SOUND [ON voice,] freq, dur [, vol [, wave [, att, dec, sus, rel [, pw]]]]
    /// </summary>
    public static Stmt ParseSoundStatement(Parser parser, string? ns)
    {
        var soundToken = parser.Consume(TokenKind.Sound, "Internal parser error: Expected 'SOUND' keyword.");
        Expr? voice = null;

        // SOUND ON voice, ...
        if (parser.Match(TokenKind.On))
        {
            voice = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after voice in SOUND ON.");
        }

        var frequency = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after frequency in SOUND statement.");
        var duration = parser.ParseExpression();
        Expr? volume = null;
        Expr? waveform = null;
        Expr? attack = null;
        Expr? decay = null;
        Expr? sustain = null;
        Expr? release = null;
        Expr? pulseWidth = null;

        // Parse optional trailing args: [, vol [, wave [, att, dec, sus, rel [, pw]]]]
        if (parser.Match(TokenKind.Comma))
        {
            volume = parser.ParseOptionalExpression();
            if (parser.Match(TokenKind.Comma))
            {
                waveform = parser.ParseOptionalExpression();
                if (parser.Match(TokenKind.Comma))
                {
                    attack = parser.ParseOptionalExpression();
                    if (parser.Match(TokenKind.Comma))
                    {
                        decay = parser.ParseOptionalExpression();
                        if (parser.Match(TokenKind.Comma))
                        {
                            sustain = parser.ParseOptionalExpression();
                            if (parser.Match(TokenKind.Comma))
                            {
                                release = parser.ParseOptionalExpression();
                                if (parser.Match(TokenKind.Comma))
                                {
                                    pulseWidth = parser.ParseOptionalExpression();
                                }
                            }
                        }
                    }
                }
            }
        }

        return new SoundStmt(frequency, duration, volume, waveform,
            attack, decay, sustain, release, voice, pulseWidth, soundToken);
    }

    /// <summary>
    /// [FASE 1] Parsa FILTER voice, type, cutoff, resonance.
    /// type: 0=Off, 1=LowPass, 2=HighPass, 3=BandPass
    /// cutoff: frequenza in Hz (20-20000)
    /// resonance: 0-100%
    /// </summary>
    public static Stmt ParseFilterStatement(Parser parser)
    {
        var token = parser.Consume(TokenKind.Filter, "Expected 'FILTER' keyword.");
        var voice = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after voice in FILTER.");
        var type = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after filter type.");
        var cutoff = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after cutoff frequency.");
        var resonance = parser.ParseExpression();
        return new SetFilterStmt(voice, type, cutoff, resonance, token);
    }

    public static Stmt ParseSoundLoadStatement(Parser parser)
    {
        parser.Consume(TokenKind.Sound, "Expected 'SOUND'.");
        parser.Consume(TokenKind.Load, "Expected 'LOAD'.");
        var filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' after file path.");
        parser.Consume(TokenKind.Hash, "Expected '#' after AS.");
        var handle = parser.ParseTarget();
        return new SoundLoadStmt(filePath, handle, parser.Previous());
    }

    public static Stmt ParseSoundPlayFileStatement(Parser parser)
    {
        parser.Consume(TokenKind.Sound, "Expected 'SOUND'.");
        parser.Consume(TokenKind.Play, "Expected 'PLAY'.");
        parser.Consume(TokenKind.Hash, "Expected '#'.");
        var handle = parser.ParseExpression();
        Expr? channel = null;
        Expr? loops = null;
        if (parser.Match(TokenKind.Comma))
        {
            channel = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
                loops = parser.ParseExpression();
        }
        return new SoundPlayStmt(handle, channel, loops, parser.Previous());
    }

    public static Stmt ParseSoundVolumeStatement(Parser parser)
    {
        var soundToken = parser.Consume(TokenKind.Sound, "Expected 'SOUND'.");
        parser.Consume(TokenKind.Volume, "Expected 'VOLUME'.");
        var volume = parser.ParseExpression();
        Expr? channel = null;
        if (parser.Match(TokenKind.Comma))
            channel = parser.ParseExpression();
        return new SoundVolumeStmt(volume, channel, soundToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PLAY.
    /// </summary>
    public static PlayStmt ParsePlayStatement(Parser parser, string? ns)
    {
        var playToken = parser.Consume(TokenKind.Play, "Internal parser error: Expected 'PLAY' token.");
        AudioAction action;

        // Sintassi unificata (legacy e qualificata fusa): PLAY CONCURRENT | QUEUE | LOOP | <string> [ON ch]
        if (parser.Match(TokenKind.Concurrent))
        {
            var strings = parser.ParseExpressionList(TokenKind.Newline);
            if (strings.IsEmpty)
                throw CreateError(parser.Previous(), "PLAY CONCURRENT requires at least one music string.");
            action = new ConcurrentPlayAction(strings);
        }
        else if (parser.Match(TokenKind.Queue))
        {
                action = new QueuePlayAction(parser.ParseExpression());
            }
            else if (parser.Match(TokenKind.Loop))
            {
                var musicString = parser.ParseExpression();
                parser.Consume(TokenKind.On, "Expected 'ON' after music string in PLAY LOOP.");
                var channel = parser.ParseExpression();
                var times = parser.Match(TokenKind.Times) ? parser.ParseOptionalExpression() : null;
                action = new LoopPlayAction(musicString, channel, times);
            }
            else
            {
                var musicString = parser.ParseExpression();
                if (parser.Match(TokenKind.On))
                {
                    var channel = parser.ParseExpression();
                    action = new NamedPlayAction(musicString, channel);
                }
                else
                {
                    action = new StandardPlayAction(musicString);
                }
            }

        return new PlayStmt(action, playToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione per il clipboard (CLIP.GET, CLIP.SET, CLIP.CLEAR).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto dal dispatcher ("CLIP").</param>
    /// <param name="qual">Il token qualificatore (sarà il token 'CLIP').</param>
    /// <returns>Un nodo <see cref="ClipStmt"/>.</returns>
    public static Stmt ParseClipStatement(Parser parser, string? ns)
    {
        // Il dispatcher ha trovato 'CLIP', quindi lo consumiamo.
        var clipToken = parser.Consume(TokenKind.Clip, "Internal parser error: Expected 'CLIP' token.");

        // Sintassi canonica (§2.0 AGENTS.md): comando a SPAZIO `CLIP SET/GET/CLEAR`.
        // Il punto (`CLIP.SET`) è accettato per retro-compatibilità ma non è canonico.
        if (parser.At(0).Kind == TokenKind.Dot)
            parser.Advance();

        // Legge il sotto-comando e ne gestisce la logica specifica.
        var commandToken = parser.Advance();
        switch (commandToken.Kind)
        {
            case TokenKind.Get:
                // Sintassi: CLIP.GET destinazione
                var destination = parser.ParseTarget();
                return new ClipStmt(commandToken, null, destination, clipToken);

            case TokenKind.Set:
                // Sintassi: CLIP.SET espressione
                var argument = parser.ParseExpression();
                return new ClipStmt(commandToken, argument, null, clipToken);

            case TokenKind.Clear:
                // Sintassi: CLIP.CLEAR
                return new ClipStmt(commandToken, null, null, clipToken);

            default:
                throw CreateError(commandToken, "Expected GET, SET, or CLEAR after 'CLIP.'.");
        }
    }
}