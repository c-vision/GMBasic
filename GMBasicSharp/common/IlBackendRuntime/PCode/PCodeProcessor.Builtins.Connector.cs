// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Connector.cs
// P-CODE VM: ExecuteBuiltIn Connector Runtime F0 (MCP SYSTEM spec)

using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private static readonly HashSet<BuiltInFunction> ConnectorBuiltInFunctions = new()
    {
        BuiltInFunction.CONNECTORLIST,
        BuiltInFunction.CONNECTORSTATUS,
        BuiltInFunction.CONNECTORTOOLS,
        BuiltInFunction.CONNECTORSEARCH,
        BuiltInFunction.CONNECTORMARKETPLACELIST,
        BuiltInFunction.CONNECTORMARKETPLACEGET,
        BuiltInFunction.CONNECTORREGISTER,
        BuiltInFunction.CONNECTORUNREGISTER,
        BuiltInFunction.CONNECTORLOAD,
        BuiltInFunction.CONNECTORENABLE,
        BuiltInFunction.CONNECTORDISABLE,
        BuiltInFunction.CONNECTORCALL,
        BuiltInFunction.CONNECTORREFRESH,
        BuiltInFunction.CONNECTORPOLICYGET,
        BuiltInFunction.CONNECTORPOLICYSET,
        BuiltInFunction.CONNECTORSECRETGET,
        BuiltInFunction.CONNECTORSECRETSET,
        BuiltInFunction.CONNECTORAUTH,
        BuiltInFunction.CONNECTORRESOURCES,
        BuiltInFunction.CONNECTORPROMPTS,
    };

    private object? ExecuteBuiltInConnector(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            case BuiltInFunction.CONNECTORLIST: return Basic.IlBackend.ConnectorRuntime.ConnectorList();
            case BuiltInFunction.CONNECTORSTATUS: return Basic.IlBackend.ConnectorRuntime.ConnectorStatus(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORTOOLS: return Basic.IlBackend.ConnectorRuntime.ConnectorTools(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORSEARCH: return Basic.IlBackend.ConnectorRuntime.ConnectorSearch(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORMARKETPLACELIST: return Basic.IlBackend.ConnectorRuntime.ConnectorMarketplaceList();
            case BuiltInFunction.CONNECTORMARKETPLACEGET: return Basic.IlBackend.ConnectorRuntime.ConnectorMarketplaceGet(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORREGISTER:
                return args.Length >= 3
                    ? Basic.IlBackend.ConnectorRuntime.ConnectorRegister(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString())
                    : Basic.IlBackend.ConnectorRuntime.ConnectorRegister(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORUNREGISTER: return Basic.IlBackend.ConnectorRuntime.ConnectorUnregister(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORLOAD: return Basic.IlBackend.ConnectorRuntime.ConnectorLoad(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORENABLE: return Basic.IlBackend.ConnectorRuntime.ConnectorEnable(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORDISABLE: return Basic.IlBackend.ConnectorRuntime.ConnectorDisable(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORCALL: return Basic.IlBackend.ConnectorRuntime.ConnectorCall(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORREFRESH: return Basic.IlBackend.ConnectorRuntime.ConnectorRefresh(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORPOLICYGET: return Basic.IlBackend.ConnectorRuntime.ConnectorPolicyGet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORPOLICYSET: return Basic.IlBackend.ConnectorRuntime.ConnectorPolicySet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORSECRETGET: return Basic.IlBackend.ConnectorRuntime.ConnectorSecretGet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORSECRETSET: return Basic.IlBackend.ConnectorRuntime.ConnectorSecretSet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORAUTH: return Basic.IlBackend.ConnectorRuntime.ConnectorAuth(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORRESOURCES: return Basic.IlBackend.ConnectorRuntime.ConnectorResources(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.CONNECTORPROMPTS: return Basic.IlBackend.ConnectorRuntime.ConnectorPrompts(ArgOr(args, 0)?.ToString() ?? "");
            default: throw new InvalidOperationException($"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInConnector (fn={fn}).");
        }
    }
}
