// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>MCP: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> MCPBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.MCPCALL,
        BuiltInFunction.MCPLISTTOOLS,
        BuiltInFunction.MCPLISTTOOLSSERVER,
        BuiltInFunction.MCPPROMPTS,
        BuiltInFunction.MCPREGISTERAGENT,
        BuiltInFunction.MCPREGISTERTOOL,
        BuiltInFunction.MCPRESOURCES,
        BuiltInFunction.MCPSERVE,
        BuiltInFunction.MCPSERVERADD,
        BuiltInFunction.MCPSERVERDISABLE,
        BuiltInFunction.MCPSERVERENABLE,
        BuiltInFunction.MCPSERVERLIST,
        BuiltInFunction.MCPSERVERMARKETPLACEGET,
        BuiltInFunction.MCPSERVERMARKETPLACELIST,
        BuiltInFunction.MCPSERVERREFRESH,
        BuiltInFunction.MCPSERVERREMOVE,
        BuiltInFunction.MCPSERVERSTATUS,
        BuiltInFunction.MCPSTOP,
        BuiltInFunction.MCPUNREGISTERTOOL,
    };

    /// <summary>Flag numerico da un argomento che può arrivare come BOOLEAN (il `TRUE` di
    /// BASIC resta un bool C# nel percorso P-Code in-process, mentre il chunk serializzato
    /// lo rende numerico) — `SafeDouble` NON gestisce i bool (TryParse("True") → false → 0),
    /// quindi senza questo helper il READONLY dichiarato andava perso su PCode (divergenza
    /// da IL/VM-C/C99 su `AI.MCPLISTTOOLS()`).</summary>
    private static double ArgFlag(object?[] args, int idx)
    {
        if (idx >= args.Length) return 0;
        var v = args[idx];
        if (v is bool b) return b ? 1 : 0;
        return SafeDouble(v) != 0 ? 1 : 0;
    }

    private object? ExecuteBuiltInMcp(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            case BuiltInFunction.MCPREGISTERTOOL:
            // ABI unica dei produttori di chunk/programmi:
            //   (tool, proc, desc, readOnly, inputSchema, outputSchema)
            // L'inputSchema è INIETTATO dal compiler (Compiler.Expressions.cs,
            // McpInputSchemaJsonFor) perché sul percorso P-Code/VM-C la reflection non
            // conosce le procedure del chunk (prima restava `{}` → divergenza da IL/C99).
            // Runtime comune (IlRuntime.McpRegisterToolWithSchema): una sola implementazione.
            return Basic.IlBackend.IlRuntime.McpRegisterToolWithSchema(
            ArgOr(args, 0)?.ToString() ?? "",
            ArgOr(args, 1)?.ToString() ?? "",
            args.Length > 2 ? ArgOr(args, 2)?.ToString() ?? "" : "",
            ArgFlag(args, 3) != 0,
            args.Length > 4 ? ArgOr(args, 4)?.ToString() ?? "" : "",
            args.Length > 5 ? ArgOr(args, 5)?.ToString() ?? "" : "");
            case BuiltInFunction.MCPUNREGISTERTOOL:
            return Basic.IlBackend.IlRuntime.McpUnregisterTool(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPLISTTOOLS:
            return Basic.IlBackend.IlRuntime.McpListTools();
            case BuiltInFunction.MCPSERVE:
            // Runtime comune (IlRuntime): stdio (con guardia GPM) e http (McpHttpListenerHost)
            // — unica implementazione. 3° arg opzionale = readOnly (server rifiuta i tool
            // non dichiarati READONLY — pattern MCP-001).
            return Basic.IlBackend.IlRuntime.McpServe(
            args.Length == 0 ? "stdio" : ArgOr(args, 0)?.ToString() ?? "stdio",
            args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : Basic.Mcp.McpHttpOptions.DefaultPort,
            args.Length > 2 && SafeDouble(ArgOr(args, 2)) != 0);
            case BuiltInFunction.MCPSTOP:
            return Basic.IlBackend.IlRuntime.McpStop();
            case BuiltInFunction.MCPRESOURCES:
            // Runtime comune (IlRuntime): stessa logica — unica implementazione.
            return Basic.IlBackend.IlRuntime.McpResources();
            case BuiltInFunction.MCPPROMPTS:
            // Runtime comune (IlRuntime): stessa logica — unica implementazione.
            return Basic.IlBackend.IlRuntime.McpPrompts();
            case BuiltInFunction.MCPREGISTERAGENT:
            // Runtime comune (IlRuntime): stessa logica — unica implementazione.
            return Basic.IlBackend.IlRuntime.McpRegisterAgent(
            ArgOr(args, 0)?.ToString() ?? "");

            case BuiltInFunction.MCPSERVERADD:
            return Basic.IlBackend.IlRuntime.McpServerAdd(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
            case BuiltInFunction.MCPSERVERREMOVE:
            return Basic.IlBackend.IlRuntime.McpServerRemove(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPSERVERENABLE:
            return Basic.IlBackend.IlRuntime.McpServerEnable(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPSERVERDISABLE:
            return Basic.IlBackend.IlRuntime.McpServerDisable(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPSERVERREFRESH:
            return Basic.IlBackend.IlRuntime.McpServerRefresh();
            case BuiltInFunction.MCPSERVERLIST:
            return Basic.IlBackend.IlRuntime.McpServerList();
            case BuiltInFunction.MCPSERVERSTATUS:
            return Basic.IlBackend.IlRuntime.McpServerStatus(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPSERVERMARKETPLACELIST:
            return Basic.IlBackend.IlRuntime.McpServerMarketplaceList();
            case BuiltInFunction.MCPSERVERMARKETPLACEGET:
            return Basic.IlBackend.IlRuntime.McpServerMarketplaceGet(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPLISTTOOLSSERVER:
            return Basic.IlBackend.IlRuntime.McpListToolsServer(ArgOr(args, 0)?.ToString() ?? "");
            case BuiltInFunction.MCPCALL:
            return Basic.IlBackend.IlRuntime.McpCall(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
            default:
                throw new InvalidOperationException(
                    $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInMcp (fn={fn}).");
        }
    }
}
