// FILE: Parser.Stmt.Graph.cs
// Sintassi BRAINS (senza parentesi): comandi del knowledge graph L5 (ex GRAPH).
//   BRAINS BUILD <wikiDir> [, backend]       costruisce/aggiorna il grafo arricchito (8 campi)
//   BRAINS UPDATE <wikiDir> [, backend]      alias di BUILD (ricostruisce)
//   BRAINS FIND PATH <from> TO <to>          stampa il path più corto nel grafo del brain attivo
//   BRAINS RDD <MEMORY|JSON>                 seleziona il backend di storage (default MEMORY)
// I comandi usano SOLO keyword TokenKind esistenti (BUILD/FIND aggiunti per L5), niente parentesi.

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>Dispatch dei comandi legacy BRAINS (dopo la keyword GRAPH, senza punto).</summary>
    public static Stmt ParseGraphStatement(Parser parser, string expectedNamespace)
    {
        // `BRAINS BUILD/UPDATE/FIND PATH/RDD` (ex GRAPH, 2026-11-04). `GRAPH` resta il
        // keyword del tipo/torre dei grafi: i comandi legacy dei brain ora sono BRAINS.
        parser.Consume(TokenKind.Brains, "Expected 'BRAINS'.");
        var tok = parser.Peek();
        switch (tok.Kind)
        {
            case TokenKind.Build:
                return ParseGraphBuildArgs(parser, TokenKind.GraphBuild);
            case TokenKind.Update:
                return ParseGraphBuildArgs(parser, TokenKind.GraphUpdate);
            case TokenKind.Find:
            {
                parser.Advance(); // FIND
                parser.Consume(TokenKind.Path, "Expected 'PATH' after 'GRAPH FIND'.");
                var from = ParseGraphNameArg(parser);
                parser.Consume(TokenKind.To, "Expected 'TO' between path endpoints.");
                var to = ParseGraphNameArg(parser);
                return new ExpressionStmt(MakeGraphCommand(parser.Previous(), TokenKind.GraphFindPath, [from, to]));
            }
            case TokenKind.Rdd:
            {
                parser.Advance(); // RDD
                var backend = ParseGraphNameArg(parser);
                return new ExpressionStmt(MakeGraphCommand(parser.Previous(), TokenKind.GraphRdd, [backend]));
            }
            default:
                throw CreateError(tok, "Expected BUILD, UPDATE, FIND PATH or RDD after 'GRAPH'.");
        }
    }

    /// <summary>BRAINS BUILD/UPDATE <dir> [, backend] — dir come stringa o nome nudo.</summary>
    private static Stmt ParseGraphBuildArgs(Parser parser, TokenKind kind)
    {
        parser.Advance(); // BUILD | UPDATE
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseGraphNameArg(parser));
        if (parser.Match(TokenKind.Comma))
            args.Add(ParseGraphNameArg(parser));
        return new ExpressionStmt(MakeGraphCommand(parser.Previous(), kind, args.ToArray()));
    }

    /// <summary>Argomento: stringa letterale o identificatore nudo (diventa stringa).</summary>
    private static Expr ParseGraphNameArg(Parser parser)
    {
        var first = parser.ParseExpression();
        // Nome nudo NON dichiarato → letterale (es. GRAPH BUILD mydir). Se il nome è una
        // variabile DICHIARATA (DIM dir AS STRING), resta la VARIABILE: prima diventava il
        // letterale "dir" e GRAPH BUILD/UPDATE usava una cartella inesistente.
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme
            && !parser._declaredVariableNames.Contains(ve.NameCleaned.Lexeme))
            return new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        return first;
    }

    /// <summary>Costruisce un comando BRAINS (BuiltinFunctionCallExpr, nessuna parentesi).</summary>
    private static BuiltinFunctionCallExpr MakeGraphCommand(Token tok, TokenKind kind, Expr[] args) =>
        new(new Token(TokenKind.Identifier, "BRAINS", null, tok.Position),
            new Token(kind, "BRAINS", null, tok.Position), ImmutableArray.Create(args), new BitType());
}
