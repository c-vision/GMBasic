// FILE: Interpreter.Parser.Stmt.Mcp.cs
// Comandi MCP SERVER (lista client, prefisso AI — direttiva sintassi AGENTS.md):
//   Legacy:    AI MCP SERVER ADD/REMOVE/ENABLE/DISABLE/REFRESH ... | AI MCP CALL ...
//   Qualificata: AI.MCPSERVERADD ... | AI.MCPCALL ...
//   Funzioni:    AI.MCPSERVERLIST() | AI.MCPSERVERSTATUS(nome)
using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>AI MCP ... — comandi (direttiva sintassi AGENTS.md, namespace AI):
    /// SERVE / STOP / REGISTER TOOL / UNREGISTER TOOL / REGISTER AGENT / SERVER ADD|REMOVE|
    /// ENABLE|DISABLE|REFRESH. Il token corrente è già MCP (consumato dal dispatcher AI).</summary>
    public static Stmt ParseMcpStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Mcp, "Expected 'MCP' after 'AI'.");
        if (parser.Match(TokenKind.Server))
        {
            if (parser.Match(TokenKind.AiSubMarketplace))
            {
                if (parser.Match(TokenKind.Get))
                    return ParseMcpCommandArgs(parser, TokenKind.McpServerMarketplaceGet, 1);
                throw CreateError(parser.Peek(), "Expected 'GET' after 'AI MCP SERVER MARKETPLACE' (LIST e' una funzione: AI.MCPSERVERMARKETPLACELIST()).");
            }
            var verb = parser.Peek();
            var (tokKind, minArgs) = verb.Kind switch
            {
                TokenKind.Add => (TokenKind.McpServerAdd, 3),
                TokenKind.Remove => (TokenKind.McpServerRemove, 1),
                TokenKind.Enable => (TokenKind.McpServerEnable, 1),
                TokenKind.Disable => (TokenKind.McpServerDisable, 1),
                TokenKind.Refresh => (TokenKind.McpServerRefresh, 0),
                _ => throw CreateError(verb,
                    "Expected ADD, REMOVE, ENABLE, DISABLE or REFRESH after 'AI MCP SERVER'.")
            };
            parser.Advance(); // verbo
            return ParseMcpCommandArgs(parser, tokKind, minArgs);
        }
        if (parser.Match(TokenKind.AiSubServe))
            return ParseMcpCommandArgs(parser, TokenKind.McpServe, 1);
        if (parser.Match(TokenKind.Stop))
            return ParseMcpCommandArgs(parser, TokenKind.McpStop, 0);
        if (parser.Match(TokenKind.Register))
        {
            if (parser.Match(TokenKind.AiSubTool))
                return ParseMcpCommandArgs(parser, TokenKind.McpRegisterTool, 2);
            if (parser.Match(TokenKind.Agent))
                return ParseMcpCommandArgs(parser, TokenKind.McpRegisterAgent, 1);
            throw CreateError(parser.Peek(), "Expected 'TOOL' or 'AGENT' after 'AI MCP REGISTER'.");
        }
        if (parser.Match(TokenKind.AiSubUnregister))
        {
            parser.Consume(TokenKind.AiSubTool, "Expected 'TOOL' after 'AI MCP UNREGISTER'.");
            return ParseMcpCommandArgs(parser, TokenKind.McpUnregisterTool, 1);
        }
        throw CreateError(parser.Peek(), "Expected 'SERVER', 'SERVE', 'STOP', 'REGISTER' or 'UNREGISTER' after 'AI MCP'.");
    }

    /// <summary>Dispatcher della forma qualificata fusa "AI.MCPSERVE ..." (namespace AI,
    /// nome fuso già letto dal DispatchRegisteredCommand).</summary>

    /// <summary>Legge gli argomenti separati da virgola e costruisce la chiamata comando
    /// (BuiltinFunctionCallExpr, nessuna parentesi — direttiva sintassi).</summary>
    private static Stmt ParseMcpCommandArgs(Parser parser, TokenKind tokKind, int minArgs)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        for (var i = 0; i < minArgs; i++)
        {
            if (i > 0) parser.Consume(TokenKind.Comma, "Expected ',' between arguments.");
            args.Add(parser.ParseExpression());
        }
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "AI", null, tok.Position),
            args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }
}
