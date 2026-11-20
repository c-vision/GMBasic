// FILE: Interpreter.Parser.Stmt.World.cs
/*
------------------------------------------------------------------------------------------------------------------------
       DOMINIO WORLD (motore 3D) — COMANDI LEGACY A SPAZIO
------------------------------------------------------------------------------------------------------------------------
`WORLD` è il namespace del motore 3D. I suoi verbi sono COMANDI (§1): niente parentesi, verbo
dopo `WORLD`, argomenti separati da virgola. Le letture sono FUNZIONI qualificate con parentesi
(`WORLD.MOVING(1)`, `WORLD.TIME()`), registrate in StdLib.World.cs.

Forme (specifica §2.4 del piano 3D):
   WORLD LOAD "mappa"                        ' mondo corrente, load atomico
   WORLD CLOSE                               ' libera il mondo corrente
   WORLD TICK secondi
   WORLD SECMOVE settore, velocità, pavimento[, soffitto]
   WORLD HEIGHT settore                      ' ferma il mover
   WORLD ANIM settore, prima, quante, fps
   WORLD SCROLL settore, what, u/s, v/s      ' what: 0 = piani, 1 = pareti
   WORLD LIGHTFX settore, modo, amount, rate
   WORLD LAMP x, y, z, raggio, livello

Ogni verbo porta il PROPRIO TokenKind nel nodo (`Load`, `SecMove`, `Anim`, …): è quello la chiave
di dispatch in tutti i backend, mai il lessema (§12).
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher dei comandi del dominio <c>WORLD</c> (motore 3D), sintassi legacy a spazio.
    /// Consuma la keyword <c>WORLD</c> e delega al verbo con il suo TokenKind.
    /// </summary>
    public static Stmt DispatchWorldStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.World, "Internal parser error: Expected 'WORLD' token.");
        var verb = parser.Peek();

        switch (verb.Kind)
        {
            case TokenKind.Load:
                parser.Advance();
                return WorldCmd(verb, [parser.ParseExpression()]);

            case TokenKind.Close:
                parser.Advance();
                return WorldCmd(verb, []);

            case TokenKind.Tick:
                parser.Advance();
                return WorldCmd(verb, [parser.ParseExpression()]);

            case TokenKind.SecMove:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 3, 4,
                    "WORLD SECMOVE settore, velocità, pavimento[, soffitto]"));

            case TokenKind.Height:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 1, 1, "WORLD HEIGHT settore"));

            case TokenKind.Anim:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 4, 4, "WORLD ANIM settore, prima, quante, fps"));

            case TokenKind.Scroll:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 4, 4, "WORLD SCROLL settore, what, u/s, v/s"));

            case TokenKind.LightFx:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 4, 4, "WORLD LIGHTFX settore, modo, amount, rate"));

            case TokenKind.Lamp:
                parser.Advance();
                return WorldCmd(verb, WorldArgs(parser, 5, 5, "WORLD LAMP x, y, z, raggio, livello"));

            default:
                throw CreateError(verb, $"Sottocomando 'WORLD {verb.Lexeme}' non riconosciuto.");
        }
    }

    private static Stmt WorldCmd(Token verb, ImmutableArray<Expr> args) =>
        new WorldCommandStmt(verb, args, verb);

    /// <summary>
    /// Argomenti di un verbo WORLD: espressioni separate da virgola, fra <paramref name="min"/> e
    /// <paramref name="max"/>. Meno del minimo è un errore esplicito (nessun comando a metà).
    /// </summary>
    private static ImmutableArray<Expr> WorldArgs(Parser parser, int min, int max, string usage)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        do
        {
            args.Add(parser.ParseExpression());
            if (args.Count >= max || parser.Peek().Kind != TokenKind.Comma) break;
            parser.Consume(TokenKind.Comma, "Expected ','.");
        } while (true);

        if (args.Count < min)
            throw CreateError(parser.Peek(), usage);

        return args.ToImmutable();
    }
}
