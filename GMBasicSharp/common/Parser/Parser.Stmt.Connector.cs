// FILE: Parser.Stmt.Connector.cs
// Comandi CONNECTOR (Connector Runtime F0, spec MCP SYSTEM.txt §24-35):
//   CONNECTOR LIST | STATUS name | SEARCH cap | MARKETPLACE LIST | MARKETPLACE GET name
//   CONNECTOR REGISTER name, kind [, configJson] | UNREGISTER name | LOAD name | ENABLE name | DISABLE name
//   CONNECTOR CALL connector, tool, argsJson | REFRESH name
//   CONNECTOR POLICY SET connector, tool, policy | CONNECTOR SECRET SET connector, key, value
//   CONNECTOR AUTH connector, authJson
// Sintassi legacy a spazio (senza parentesi) — direttiva AGENTS.md §2.0.

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    public static Stmt ParseConnectorStatement(Parser parser, string expectedNamespace)
    {
        var connectorTok = parser.Consume(TokenKind.Connector, "Expected 'CONNECTOR'.");
        var verb = parser.Peek();

        // CONNECTOR LIST — lista connettori
        if (verb.Kind == TokenKind.List)
        {
            parser.Advance();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorList);
        }

        // CONNECTOR SEARCH cap — cerca per capability
        if (verb.Kind == TokenKind.Search)
        {
            parser.Advance();
            var cap = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorSearch, cap);
        }

        // CONNECTOR MARKETPLACE LIST | GET name
        if (verb.Kind == TokenKind.AiSubMarketplace || verb.Lexeme.Equals("MARKETPLACE", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var sub = parser.Peek();
            if (sub.Kind == TokenKind.List || sub.Lexeme.Equals("LIST", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorMarketplaceList);
            }
            if (sub.Kind == TokenKind.Get || sub.Lexeme.Equals("GET", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                var name = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorMarketplaceGet, name);
            }
            throw CreateError(sub, "Expected LIST or GET after 'CONNECTOR MARKETPLACE'.");
        }

        // CONNECTOR POLICY SET connector, tool, policy | GET
        if (verb.Lexeme.Equals("POLICY", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var op = parser.Peek();
            if (op.Kind == TokenKind.Set || op.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                var conn = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
                var tool = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after tool name.");
                var policy = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorPolicySet, conn, tool, policy);
            }
            if (op.Kind == TokenKind.Get || op.Lexeme.Equals("GET", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                var conn = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
                var tool = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorPolicyGet, conn, tool);
            }
            throw CreateError(op, "Expected SET or GET after 'CONNECTOR POLICY'.");
        }

        // CONNECTOR SECRET SET connector, key, value | GET connector, key
        if (verb.Lexeme.Equals("SECRET", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var op = parser.Peek();
            if (op.Kind == TokenKind.Set || op.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                var conn = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
                var key = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after key.");
                var val = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorSecretSet, conn, key, val);
            }
            if (op.Kind == TokenKind.Get || op.Lexeme.Equals("GET", StringComparison.OrdinalIgnoreCase))
            {
                parser.Advance();
                var conn = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
                var key = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorSecretGet, conn, key);
            }
            throw CreateError(op, "Expected SET or GET after 'CONNECTOR SECRET'.");
        }

        // CONNECTOR AUTH connector, authJson — configura OAuth
        if (verb.Kind == TokenKind.ConnectorAuth || verb.Lexeme.Equals("AUTH", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var conn = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
            var authJson = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorAuth, conn, authJson);
        }

        // CONNECTOR REGISTER name, kind [, configJson]
        if (verb.Kind == TokenKind.Register || verb.Lexeme.Equals("REGISTER", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
            var kind = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
            {
                var cfg = parser.ParseExpression();
                return MakeConnectorCommand(connectorTok, TokenKind.ConnectorRegister, name, kind, cfg);
            }
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorRegister, name, kind);
        }

        // CONNECTOR UNREGISTER name
        if (verb.Kind == TokenKind.AiSubUnregister || verb.Lexeme.Equals("UNREGISTER", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorUnregister, name);
        }

        // CONNECTOR LOAD name — alias di REGISTER+ENABLE (compatibilità doc)
        if (verb.Kind == TokenKind.Load || verb.Lexeme.Equals("LOAD", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorLoad, name);
        }

        // CONNECTOR ENABLE / DISABLE
        if (verb.Kind == TokenKind.Enable)
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorEnable, name);
        }
        if (verb.Kind == TokenKind.Disable)
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorDisable, name);
        }

        // CONNECTOR STATUS name
        if (verb.Kind == TokenKind.AiSubStatus || verb.Lexeme.Equals("STATUS", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorStatus, name);
        }

        // CONNECTOR CALL connector, tool, argsJson
        if (verb.Kind == TokenKind.Call || verb.Lexeme.Equals("CALL", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var conn = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after connector name.");
            var tool = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after tool name.");
            var args = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorCall, conn, tool, args);
        }

        // CONNECTOR REFRESH name
        if (verb.Kind == TokenKind.Refresh || verb.Lexeme.Equals("REFRESH", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorRefresh, name);
        }

        // CONNECTOR TOOLS name — alias di STATUS con tools
        if (verb.Lexeme.Equals("TOOLS", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorTools, name);
        }

        // CONNECTOR RESOURCES / PROMPTS (discovery)
        if (verb.Lexeme.Equals("RESOURCES", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorResources, name);
        }
        if (verb.Lexeme.Equals("PROMPTS", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var name = parser.ParseExpression();
            return MakeConnectorCommand(connectorTok, TokenKind.ConnectorPrompts, name);
        }

        throw CreateError(verb, "Expected LIST, REGISTER, UNREGISTER, LOAD, ENABLE, DISABLE, STATUS, CALL, REFRESH, SEARCH, POLICY, SECRET, AUTH or MARKETPLACE after 'CONNECTOR'.");
    }

    private static Stmt MakeConnectorCommand(Token connectorTok, TokenKind kind, params Expr[] args)
    {
        // Namespace = AI (i connettori sono sottodominio di AI, come MCP/AGENT). Il parser CONNECTOR
        // top-level resta come alias, ma la risoluzione è sempre su AI (unico punto di verità).
        var nsTok = new Token(TokenKind.Identifier, "AI", null, connectorTok.Position);
        var nameTok = new Token(kind, "AI", null, connectorTok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok, [.. args], new BitType());
        return new ExpressionStmt(call);
    }
}
