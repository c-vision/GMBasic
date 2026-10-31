// FILE: Interpreter.StdLib.Connector.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Connector — namespace 'AI'
 * 11 funzioni | 1 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (11):
 *   CONNECTORCALL
 *   CONNECTORLIST
 *   CONNECTORMARKETPLACEGET
 *   CONNECTORMARKETPLACELIST
 *   CONNECTORPOLICYGET
 *   CONNECTORPROMPTS
 *   CONNECTORRESOURCES
 *   CONNECTORSEARCH
 *   CONNECTORSECRETGET
 *   CONNECTORSTATUS
 *   CONNECTORTOOLS
 * COMANDI (1):
 *   CONNECTOR
    ==== GMB-STDLIB-AUDIT-END ==== */

namespace Basic;

public sealed partial class StdLib
{
    private void CreateConnectorNamespace()
    {
        // Namespace = AI (non CONNECTOR) — i connettori sono un sottodominio di AI, come MCP/AGENT/SKILLS.
        const string namespaceName = "AI";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // Nessun RegisterCommand top-level per CONNECTOR: il dispatch è via AI CONNECTOR (Parser.Stmt.Ai).
        // Manteniamo la compatibilità legacy CONNECTOR ... come alias che inoltra ad AI.
        RegisterCommand(TokenKind.Connector, "CONNECTOR", namespaceName, Parser.ParseConnectorStatement);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorList, "CONNECTORLIST"), [], new StringType(), TokenKind.ConnectorList),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorStatus, "CONNECTORSTATUS"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorStatus),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorTools, "CONNECTORTOOLS"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorTools),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorSearch, "CONNECTORSEARCH"),
                [new Parameter(-1, T(TokenKind.Identifier, "capability"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorSearch),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorMarketplaceList, "CONNECTORMARKETPLACELIST"), [], new StringType(), TokenKind.ConnectorMarketplaceList),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorMarketplaceGet, "CONNECTORMARKETPLACEGET"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorMarketplaceGet),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorPolicyGet, "CONNECTORPOLICYGET"),
                [new Parameter(-1, T(TokenKind.Identifier, "connector"), new StringType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "tool"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorPolicyGet),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorSecretGet, "CONNECTORSECRETGET"),
                [new Parameter(-1, T(TokenKind.Identifier, "connector"), new StringType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "key"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorSecretGet),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorCall, "CONNECTORCALL"),
                [new Parameter(-1, T(TokenKind.Identifier, "connector"), new StringType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "tool"), new StringType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "args"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorCall),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorResources, "CONNECTORRESOURCES"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorResources),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.ConnectorPrompts, "CONNECTORPROMPTS"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), noFlags, null, false)], new StringType(), TokenKind.ConnectorPrompts),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
