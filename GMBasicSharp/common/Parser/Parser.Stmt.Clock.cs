// FILE: Interpreter.Parser.Stmt.Clock.cs
/*
------------------------------------------------------------------------------------------------------------------------
       DOMINIO CLOCK (tempo a passo fisso) — COMANDI LEGACY A SPAZIO
------------------------------------------------------------------------------------------------------------------------
`CLOCK` governa l'orologio a passo fisso del ciclo grafico. I verbi sono COMANDI (§1): niente
parentesi, verbo dopo `CLOCK`, argomento separato da virgola dove serve. Le letture sono
FUNZIONI qualificate con parentesi (`CLOCK.DT()`, `CLOCK.ALPHA()`, …), registrate in
StdLib.Clock.cs.

Forme:
   CLOCK PAUSE                  ' ferma la simulazione; la coda del tempo si azzera
   CLOCK RESUME                 ' riprende dal passo successivo
   CLOCK SCALE secondi          ' moltiplicatore del tempo reale (0.5 = slow motion, 0 = fermo)
   CLOCK FPS frequenza          ' passi di simulazione al secondo (1..1000)

`FPS` NON è una keyword del lessico: i programmi esistenti usano `fps` come nome di variabile
(`DIM fps AS INTEGER`). Si riconosce qui, una volta sola, e la catena a valle riceve il
TokenKind dedicato — mai il lessema (§12).
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher dei comandi del dominio <c>CLOCK</c> (tempo a passo fisso), sintassi legacy a
    /// spazio. Consuma la keyword <c>CLOCK</c> e delega al verbo con il suo TokenKind.
    /// </summary>
    public static Stmt DispatchClockStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Clock, "Internal parser error: Expected 'CLOCK' token.");
        var verb = parser.Peek();

        switch (verb.Kind)
        {
            case TokenKind.Pause:
                parser.Advance();
                return ClockCmd(verb, []);

            case TokenKind.Resume:
                parser.Advance();
                return ClockCmd(verb, []);

            case TokenKind.Scale:
                parser.Advance();
                return ClockCmd(verb, [parser.ParseExpression()]);
        }

        // FPS: soft keyword (nessun TokenKind nel lessico) → normalizzata nel token canonico.
        if (verb.Kind == TokenKind.Identifier
            && verb.Lexeme.Equals("FPS", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var fpsToken = new Token(TokenKind.Fps, verb.Lexeme, null, verb.Position);
            return ClockCmd(fpsToken, [parser.ParseExpression()]);
        }

        throw CreateError(verb,
            $"Sottocomando 'CLOCK {verb.Lexeme}' non riconosciuto: usa PAUSE, RESUME, SCALE o FPS.");
    }

    private static Stmt ClockCmd(Token verb, ImmutableArray<Expr> args) =>
        new ClockCommandStmt(verb, args, verb);
}
