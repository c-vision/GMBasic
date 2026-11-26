// FILE: Interpreter.Parser.Stmt.Scene.cs
/*
------------------------------------------------------------------------------------------------------------------------
       DOMINIO SCENE (motore 3D, renderer mesh) — COMANDI LEGACY A SPAZIO
------------------------------------------------------------------------------------------------------------------------
`SCENE` è il namespace del renderer mesh. I suoi verbi sono COMANDI (§1): niente parentesi, verbo
dopo `SCENE`, argomenti separati da virgola. Le letture sono FUNZIONI qualificate con parentesi
(`SCENE.VERTS()`, `SCENE.PIXEL(x, y)`), registrate in StdLib.Scene.cs.

Forme (facciata `gmb_scene_*`, piano §4.2 ridotto a ciò che il kernel P3 espone):
   SCENE RESET                                 ' azzera geometria e camera
   SCENE VIEW w, h, fov                        ' target + campo visivo VERTICALE in gradi
   SCENE TEXTURE celle                         ' texture a scacchi procedurale (1..64 celle)
   SCENE TEXTURE "file.png"                    ' texture da PNG su disco (stesso verbo, argomento stringa)
   SCENE OBJECT r, g, b                        ' apre un oggetto e ne fissa la tinta
   SCENE VERTEX x, y, z, u, v                  ' vertice dell'oggetto corrente
   SCENE QUAD a, b, c, d                       ' due triangoli sui vertici dichiarati
   SCENE BOX x0, y0, z0, x1, y1, z1             ' 24 vertici, 12 triangoli, UV per faccia
   SCENE OBJPOS x, y, z, yaw                   ' posizione/rotazione dell'oggetto corrente
   SCENE CAMERA x, y, z, yaw, pitch            ' yaw 0 = sguardo verso +z, pitch < 0 = in basso
   SCENE LIGHT dx, dy, dz, livello              ' luce direzionale per faccia (dx,dy,dz verso la luce)
   SCENE SMOOTH on|off                         ' bilineare (default on) / nearest
   SCENE ALPHA on|off                          ' alpha-test: alpha 0 scartato (default on)
   SCENE TWOSIDED on|off                       ' niente back-face cull (default off)
   SCENE PICK on|off                           ' buffer di id per pixel, additivo (default off)
   SCENE PRESENT                               ' copia il target 3D nella pagina 2D corrente (GPM)
   SCENE RENDER                                ' rasterizza nel target
   SCENE SAVE path$                            ' PNG del frame corrente

NORMALIZZAZIONE DEL VERBO. `VIEW`, `TEXTURE`, `QUAD`, `BOX` e `SAVE` sono già parole del
linguaggio; `RESET`, `OBJECT`, `VERTEX`, `OBJPOS`, `CAMERA` e `RENDER` no, e riservarli
romperebbe i programmi che li usano come variabili. Il lessema del verbo si traduce quindi QUI,
una volta sola, nei TokenKind `Scene*` dedicati (stesso schema di `CLOCK FPS`): a valle — AST,
linker, compiler, emitter — il dispatch è per TokenKind, mai per lessema (§12).
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher dei comandi del dominio <c>SCENE</c> (motore 3D, renderer mesh), sintassi legacy
    /// a spazio. Consuma la keyword <c>SCENE</c> e delega al verbo con il suo TokenKind dedicato.
    /// </summary>
    public static Stmt DispatchSceneStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Scene, "Internal parser error: Expected 'SCENE' token.");
        var verb = parser.Peek();
        var kind = SceneVerbKind(verb);

        if (kind == TokenKind.Unknown)
            throw CreateError(verb, $"Sottocomando 'SCENE {verb.Lexeme}' non riconosciuto.");

        // Il verbo nel nodo porta il Kind DEDICATO: è la chiave di dispatch dei backend.
        var command = verb with { Kind = kind };
        parser.Advance();

        return kind switch
        {
            TokenKind.SceneReset => SceneCmd(command, SceneNoArgs(parser, "SCENE RESET non accetta argomenti")),
            TokenKind.SceneView => SceneCmd(command, SceneArgs(parser, 3, 3, "SCENE VIEW w, h, fov")),
            TokenKind.SceneTexture => SceneCmd(command, SceneArgs(parser, 1, 1, "SCENE TEXTURE celle | \"file.png\"")),
            TokenKind.SceneObject => SceneCmd(command, SceneArgsOneOf(parser,
                "SCENE OBJECT r, g, b [, flags [, luce]]", 3, 5)),
            TokenKind.SceneVertex => SceneCmd(command, SceneArgsOneOf(parser,
                "SCENE VERTEX x, y, z, u, v [, nx, ny, nz]", 5, 8)),
            TokenKind.SceneQuad => SceneCmd(command, SceneArgs(parser, 4, 4, "SCENE QUAD a, b, c, d")),
            TokenKind.SceneBox => SceneCmd(command, SceneArgs(parser, 6, 6, "SCENE BOX x0, y0, z0, x1, y1, z1")),
            TokenKind.SceneObjPos => SceneCmd(command, SceneArgs(parser, 4, 4, "SCENE OBJPOS x, y, z, yaw")),
            TokenKind.SceneCamera => SceneCmd(command, SceneArgs(parser, 5, 5, "SCENE CAMERA x, y, z, yaw, pitch")),
            TokenKind.SceneLight => SceneCmd(command, SceneArgs(parser, 4, 4, "SCENE LIGHT dx, dy, dz, livello")),
            // I tre flag di materiale hanno la stessa forma (un ON/OFF): una sola riga.
            TokenKind.SceneSmooth or TokenKind.SceneAlpha or TokenKind.SceneTwoSided
                or TokenKind.SceneAa
                => SceneCmd(command, SceneFlagArg(parser, "SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off")),
            // Buffer di id per pixel: un flag ON/OFF, additivo e spento di default.
            TokenKind.ScenePick => SceneCmd(command, SceneFlagArg(parser, "SCENE PICK on|off")),
            // P4: lampada puntiforme (6 argomenti) o spot (10), fog, FAR, cielo,
            // colormap a 32 livelli e thing.
            TokenKind.SceneLamp => SceneCmd(command, SceneArgsOneOf(parser,
                "SCENE LAMP n, x, y, z, raggio, quantità [, dx, dy, dz, angolo]", 6, 10)),
            TokenKind.SceneFog => SceneCmd(command, SceneArgs(parser, 1, 1, "SCENE FOG distanza")),
            TokenKind.SceneFar => SceneCmd(command, SceneArgs(parser, 1, 1, "SCENE FAR distanza")),
            TokenKind.SceneSky => SceneCmd(command, SceneArgs(parser, 2, 2, "SCENE SKY repeat, shift")),
            TokenKind.SceneColormap => SceneCmd(command, SceneNoArgs(parser, "SCENE COLORMAP non accetta argomenti")),
            TokenKind.SceneThing => SceneCmd(command, SceneArgs(parser, 6, 6, "SCENE THING n, x, y, z, w, h")),
            TokenKind.ScenePresent => SceneCmd(command, SceneNoArgs(parser, "SCENE PRESENT non accetta argomenti")),
            TokenKind.SceneRender => SceneCmd(command, SceneNoArgs(parser, "SCENE RENDER non accetta argomenti")),
            TokenKind.SceneSave => SceneCmd(command, SceneArgs(parser, 1, 1, "SCENE SAVE: manca il percorso del PNG")),
            _ => throw CreateError(verb, $"Sottocomando 'SCENE {verb.Lexeme}' non riconosciuto.")
        };
    }

    /// <summary>
    /// Lessema del verbo → TokenKind dedicato. Unica sede della traduzione (il parser C fa la
    /// stessa cosa in <c>gmb_parser_stmt_domain.c</c>); a valle nessuno guarda più il lessema.
    /// </summary>
    private static TokenKind SceneVerbKind(Token verb) => verb.Lexeme.ToUpperInvariant() switch
    {
        "RESET" => TokenKind.SceneReset,
        "VIEW" => TokenKind.SceneView,
        "TEXTURE" => TokenKind.SceneTexture,
        "OBJECT" => TokenKind.SceneObject,
        "VERTEX" => TokenKind.SceneVertex,
        "QUAD" => TokenKind.SceneQuad,
        "BOX" => TokenKind.SceneBox,
        "OBJPOS" => TokenKind.SceneObjPos,
        "CAMERA" => TokenKind.SceneCamera,
        "LIGHT" => TokenKind.SceneLight,
        "SMOOTH" => TokenKind.SceneSmooth,
        "ALPHA" => TokenKind.SceneAlpha,
        "TWOSIDED" => TokenKind.SceneTwoSided,
        "PRESENT" => TokenKind.ScenePresent,
        "LAMP" => TokenKind.SceneLamp,
        "FOG" => TokenKind.SceneFog,
        "FAR" => TokenKind.SceneFar,
        "SKY" => TokenKind.SceneSky,
        "COLORMAP" => TokenKind.SceneColormap,
        "THING" => TokenKind.SceneThing,
        "AA" => TokenKind.SceneAa,
        "PICK" => TokenKind.ScenePick,
        "RENDER" => TokenKind.SceneRender,
        "SAVE" => TokenKind.SceneSave,
        _ => TokenKind.Unknown
    };

    private static Stmt SceneCmd(Token verb, ImmutableArray<Expr> args) =>
        new SceneCommandStmt(verb, args, verb);

    /// <summary>
    /// Argomenti di un verbo SCENE: espressioni separate da virgola, fra <paramref name="min"/> e
    /// <paramref name="max"/>. Meno del minimo è un errore esplicito (nessun comando a metà).
    /// </summary>
    private static ImmutableArray<Expr> SceneArgs(Parser parser, int min, int max, string usage)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        do
        {
            /* Fine riga (o ':'): NON è un argomento. Prima ParseExpression a fine
             * statement produceva un letterale 0, quindi `SCENE TEXTURE` senza celle
             * eseguiva con 0 invece di essere rifiutato, e la gamba C# divergeva da
             * C99 (errore di emissione). Stesso difetto della revisione SCENE. */
            if (parser.IsAtEndOfStatement()) break;
            args.Add(parser.ParseExpression());
            if (args.Count >= max || parser.Peek().Kind != TokenKind.Comma) break;
            parser.Consume(TokenKind.Comma, "Expected ','.");
        } while (true);

        /* Arità ESATTA: meno del minimo o argomenti in più sono lo stesso errore,
         * con il testo canonico della forma (lo stesso che stampa l'emitter C99). */
        if (args.Count < min || !parser.IsAtEndOfStatement())
            throw CreateError(parser.Peek(), usage);

        return args.ToImmutable();
    }

    /// <summary>
    /// Argomenti SCENE con arità ammesse <b>esatte</b> (`LAMP` 6 o 10, `OBJECT` 3 o 5): le forme
    /// intermedie non esistono nel kernel, e accettarle lascerebbe argomenti non consumati dalle
    /// gambe a stack fisso (PCode/VM-C).
    /// </summary>
    private static ImmutableArray<Expr> SceneArgsOneOf(Parser parser, string usage, params int[] allowed)
    {
        var args = SceneArgs(parser, allowed[0], allowed[allowed.Length - 1], usage);
        foreach (var n in allowed)
            if (args.Length == n) return args;
        throw CreateError(parser.Peek(), usage);
    }

    /// <summary>
    /// Argomento ON/OFF di un flag SCENE (idea #5): <c>ON</c> e <c>OFF</c> sono keyword del
    /// lessico, non espressioni, quindi qui si convertono nel letterale 1/0 — a valle il flag
    /// resta un normale argomento numerico, identico sulle quattro gambe. Accetta anche un
    /// numero esplicito: <c>SCENE SMOOTH 1</c>.
    /// </summary>
    /// <summary>
    /// Verbi SCENE senza argomenti (RESET, COLORMAP, PRESENT, RENDER): un argomento in più
    /// è un errore esplicito, con lo STESSO testo dell'emitter C99 — prima il parser C#
    /// lasciava il token e produceva "Token inatteso", divergendo dalla gamba nativa.
    /// </summary>
    private static ImmutableArray<Expr> SceneNoArgs(Parser parser, string usage)
    {
        if (!parser.IsAtEndOfStatement())
            throw CreateError(parser.Peek(), usage);
        return ImmutableArray<Expr>.Empty;
    }

    private static ImmutableArray<Expr> SceneFlagArg(Parser parser, string usage)
    {
        var t = parser.Peek();
        if (t.Kind is TokenKind.On or TokenKind.Off)
        {
            parser.Advance();
            /* Arità ESATTA anche per la forma ON/OFF: `SCENE PICK on, off` non deve
             * arrivare al lettore generico con "Token ',' inatteso" (messaggio diverso
             * dalla gamba C99): stesso testo canonico del flag. */
            if (!parser.IsAtEndOfStatement())
                throw CreateError(parser.Peek(), usage);
            return ImmutableArray.Create<Expr>(new NumberExpr(t.Kind == TokenKind.On ? 1.0 : 0.0, t));
        }
        return SceneArgs(parser, 1, 1, usage);
    }
}
