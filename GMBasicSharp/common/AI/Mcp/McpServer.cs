// FILE: Basic.Mcp.McpServer
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.AI.Browser;

namespace Basic.Mcp;

/// <summary>MCP JSON-RPC server: initialize, tools/list, tools/call.</summary>
public sealed class McpServer
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull
    };

    private readonly McpSession _session = new();
    private readonly McpToolRegistry _registry;
    private readonly IMcpToolDispatcher? _dispatcher;
    private readonly McpAuditLog _audit;
    private readonly int _toolTimeoutMs;
    private readonly Basic.Agents.AgentPermissionLevel _permissionLevel;
    private readonly bool _readOnlyServer;

    public McpServer(
        McpToolRegistry? registry = null,
        IMcpToolDispatcher? dispatcher = null,
        McpAuditLog? audit = null,
        int toolTimeoutMs = McpConstants.DefaultToolTimeoutMs,
        Basic.Agents.AgentPermissionLevel permissionLevel = Basic.Agents.AgentPermissionLevel.YOLO,
        bool readOnlyServer = false)
    {
        _registry = registry ?? McpToolRegistry.Instance;
        _dispatcher = dispatcher;
        _audit = audit ?? McpAuditLog.CreateDefault();
        _toolTimeoutMs = toolTimeoutMs;
        _permissionLevel = permissionLevel;
        _readOnlyServer = readOnlyServer;
    }

    /// <summary>Process one JSON-RPC request line and return response JSON (null for notifications).</summary>
    public async Task<string?> HandleRequestAsync(string requestLine, CancellationToken cancellationToken)
    {
        JsonRpcRequest request;
        try
        {
            request = JsonSerializer.Deserialize<JsonRpcRequest>(requestLine, JsonOptions)
                      ?? throw new JsonException("null request");
        }
        catch (JsonException ex)
        {
            return Serialize(new JsonRpcResponse
            {
                Id = null,
                Error = new JsonRpcError
                {
                    Code = JsonRpcErrorCodes.ParseError,
                    Message = "Parse error",
                    Data = ex.Message
                }
            });
        }

        if (!string.Equals(request.JsonRpc, "2.0", StringComparison.Ordinal))
        {
            return SerializeError(request.Id, JsonRpcErrorCodes.InvalidRequest, "Invalid Request: jsonrpc must be 2.0");
        }

        if (string.IsNullOrWhiteSpace(request.Method))
            return SerializeError(request.Id, JsonRpcErrorCodes.InvalidRequest, "Invalid Request: missing method");

        if (request.Method == McpConstants.MethodInitialized)
            return null;

        var result = await DispatchMethodAsync(request, cancellationToken).ConfigureAwait(false);
        if (request.IsNotification)
            return null;

        return Serialize(new JsonRpcResponse { Id = request.Id, Result = result.Result, Error = result.Error });
    }

    private async Task<(object? Result, JsonRpcError? Error)> DispatchMethodAsync(
        JsonRpcRequest request,
        CancellationToken cancellationToken)
    {
        try
        {
            return request.Method switch
            {
                McpConstants.MethodInitialize => (HandleInitialize(request), null),
                McpConstants.MethodToolsList => (HandleToolsList(request), null),
                McpConstants.MethodToolsCall => await HandleToolsCallAsync(request, cancellationToken).ConfigureAwait(false),
                McpConstants.MethodResourcesList => (HandleResourcesList(), null),
                McpConstants.MethodResourcesRead => (HandleResourcesRead(request), null),
                McpConstants.MethodResourcesSubscribe => (HandleResourcesSubscribe(request), null),
                McpConstants.MethodResourcesUnsubscribe => (HandleResourcesUnsubscribe(request), null),
                McpConstants.MethodPromptsList => (HandlePromptsList(), null),
                McpConstants.MethodPromptsGet => (HandlePromptsGet(request), null),
                McpConstants.MethodPing => (HandlePing(), null),
                McpConstants.MethodSamplingCreateMessage => (HandleSamplingCreateMessage(request), null),
                _ => (null, new JsonRpcError
                {
                    Code = JsonRpcErrorCodes.MethodNotFound,
                    Message = $"Method not found: {request.Method}"
                })
            };
        }
        catch (Exception ex)
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InternalError,
                Message = "Internal error",
                Data = ex.Message
            });
        }
    }

    private object HandleInitialize(JsonRpcRequest request)
    {
        JsonObject? clientParams = null;
        if (request.Params is { ValueKind: JsonValueKind.Object } p)
            clientParams = JsonNode.Parse(p.GetRawText()) as JsonObject;

        _session.Initialize(clientParams);
        return _session.CreateInitializeResult();
    }

    private object HandleToolsList(JsonRpcRequest? request = null)
    {
        var all = _registry.ToMcpToolList();
        // Pagination con cursor (F4, SYSTEM.txt §34): cursor = index base64, pageSize 20
        string? cursor = null;
        if (request?.Params is { ValueKind: JsonValueKind.Object } p)
        {
            try
            {
                var obj = JsonNode.Parse(p.GetRawText()) as JsonObject;
                cursor = obj?["cursor"]?.GetValue<string>();
            }
            catch { }
        }
        if (!string.IsNullOrWhiteSpace(cursor))
        {
            try
            {
                var idx = int.Parse(System.Text.Encoding.UTF8.GetString(Convert.FromBase64String(cursor)));
                var page = new JsonArray();
                for (int i = idx; i < Math.Min(idx + 20, all.Count); i++) page.Add(all[i]?.DeepClone());
                var res = new JsonObject { ["tools"] = page };
                if (idx + 20 < all.Count) res["nextCursor"] = Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes((idx + 20).ToString()));
                return res;
            }
            catch { }
        }
        // Senza cursor: ritorna tutti + nextCursor se >20 (per compatibilità)
        if (all.Count > 20)
        {
            var page = new JsonArray();
            for (int i = 0; i < 20; i++) page.Add(all[i]?.DeepClone());
            return new JsonObject { ["tools"] = page, ["nextCursor"] = Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes("20")) };
        }
        return new JsonObject { ["tools"] = all };
    }

    private object HandleResourcesList()
    {
        var tools = _registry.List();
        var arr = new JsonArray();
        foreach (var tool in tools)
        {
            arr.Add(new JsonObject
            {
                ["uri"] = $"mcp://tool/{tool.Name}",
                ["name"] = tool.Name,
                ["description"] = tool.Description
            });
        }
        // G11: resources browser:// vive
        arr.Add(new JsonObject { ["uri"] = "browser://sessions", ["name"] = "Browser sessions", ["description"] = "Elenco sessioni browser forkate" });
        arr.Add(new JsonObject { ["uri"] = "browser://console", ["name"] = "Browser console", ["description"] = "Ultimi messaggi console del browser" });
        arr.Add(new JsonObject { ["uri"] = "browser://network", ["name"] = "Browser network", ["description"] = "Ultime richieste network del browser" });
        return new JsonObject { ["resources"] = arr };
    }

    private object HandlePromptsList()
    {
        var arr = new JsonArray();
        // Add registered agents as prompts
        try
        {
            var agents = Agents.AgentServer.Instance.List();
            foreach (var agent in agents)
            {
                arr.Add(new JsonObject
                {
                    ["name"] = $"chat-with-{agent.Name}",
                    ["description"] = $"Chat with agent {agent.Name}"
                });
            }
        }
        catch { /* Agents subsystem not available */ }
        return new JsonObject { ["prompts"] = arr };
    }

    private static object HandleNotImplementedList(string kind) => new JsonObject
    {
        [kind] = new JsonArray()
    };

    private object HandlePing() => new JsonObject();

    private object HandleSamplingCreateMessage(JsonRpcRequest request)
    {
        // F4 sampling/createMessage — l'MCP server chiede al client LLM di generare un messaggio
        // Per F1, ritorna un messaggio stub (echo del prompt) — in prod inoltrerebbe al LLM del client
        if (request.Params is not { ValueKind: JsonValueKind.Object } p) return new JsonObject { ["model"] = "gmbasic", ["role"] = "assistant", ["content"] = new JsonObject { ["type"] = "text", ["text"] = "sampling stub" } };
        try
        {
            var obj = JsonNode.Parse(p.GetRawText()) as JsonObject;
            var messages = obj?["messages"]?.ToString() ?? "[]";
            return new JsonObject
            {
                ["model"] = "gmbasic",
                ["role"] = "assistant",
                ["content"] = new JsonObject { ["type"] = "text", ["text"] = $"sampling echo: {messages[..Math.Min(200, messages.Length)]}" }
            };
        }
        catch { return new JsonObject { ["model"] = "gmbasic", ["role"] = "assistant", ["content"] = new JsonObject { ["type"] = "text", ["text"] = "sampling stub" } }; }
    }

    private object HandleResourcesRead(JsonRpcRequest request)
    {
        if (request.Params is not { ValueKind: JsonValueKind.Object } p) return new JsonObject { ["contents"] = new JsonArray() };
        var obj = JsonNode.Parse(p.GetRawText()) as JsonObject;
        var uri = obj?["uri"]?.GetValue<string>() ?? "";

        // G11: browser:// resources
        if (uri.StartsWith("browser://", StringComparison.OrdinalIgnoreCase))
        {
            var resource = uri["browser://".Length..];
            string text = resource switch
            {
                "sessions" => "sessions: default (sessione primaria CDP)",
                "console" => Basic.AI.Browser.BrowserRuntime.ConsoleAsync(false).GetAwaiter().GetResult().Data ?? "[]",
                "network" => Basic.AI.Browser.BrowserRuntime.NetLogAsync(false).GetAwaiter().GetResult().Data ?? "[]",
                _ => $"resource sconosciuta: {resource}"
            };
            return new JsonObject
            {
                ["contents"] = new JsonArray
                {
                    new JsonObject { ["uri"] = uri, ["mimeType"] = "application/json", ["text"] = text }
                }
            };
        }

        // Estrae il tool dal uri mcp://tool/{name} o connector://{conn}/{tool}
        var toolName = uri.Contains('/') ? uri.Split('/').Last() : uri;
        var tool = _registry.Get(toolName);
        if (tool is null) return new JsonObject { ["contents"] = new JsonArray() };
        return new JsonObject
        {
            ["contents"] = new JsonArray
            {
                new JsonObject
                {
                    ["uri"] = uri,
                    ["mimeType"] = "text/plain",
                    ["text"] = $"Tool {tool.Name}: {tool.Description} (procedure {tool.QualifiedProcedureName})"
                }
            }
        };
    }

    private object HandleResourcesSubscribe(JsonRpcRequest request) => new JsonObject();
    private object HandleResourcesUnsubscribe(JsonRpcRequest request) => new JsonObject();

    private object HandlePromptsGet(JsonRpcRequest request)
    {
        if (request.Params is not { ValueKind: JsonValueKind.Object } p) return new JsonObject { ["messages"] = new JsonArray() };
        var obj = JsonNode.Parse(p.GetRawText()) as JsonObject;
        var name = obj?["name"]?.GetValue<string>() ?? "";
        // Ritorna un prompt fittizio per il nome richiesto (es. chat-with-agent)
        var promptName = name.StartsWith("chat-with-") ? name["chat-with-".Length..] : name;
        return new JsonObject
        {
            ["description"] = $"Prompt {name}",
            ["messages"] = new JsonArray
            {
                new JsonObject
                {
                    ["role"] = "user",
                    ["content"] = new JsonObject { ["type"] = "text", ["text"] = $"You are agent {promptName}. Respond helpfully." }
                }
            }
        };
    }

    private async Task<(object? Result, JsonRpcError? Error)> HandleToolsCallAsync(
        JsonRpcRequest request,
        CancellationToken cancellationToken)
    {
        if (!_session.IsInitialized)
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InvalidRequest,
                Message = "Server not initialized"
            });
        }

        if (_dispatcher is null)
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InternalError,
                Message = "No tool dispatcher registered"
            });
        }

        if (request.Params is not { ValueKind: JsonValueKind.Object } paramsElement)
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InvalidParams,
                Message = "tools/call requires object params"
            });
        }

        var paramObj = JsonNode.Parse(paramsElement.GetRawText()) as JsonObject;
        var toolName = paramObj?["name"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(toolName))
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InvalidParams,
                Message = "tools/call requires name"
            });
        }

        var tool = _registry.Get(toolName);
        if (tool is null)
        {
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InvalidParams,
                Message = $"Unknown tool: {toolName}"
            });
        }

        // READ-ONLY enforcement (pattern MCP-001): un server capability che espone tool di
        // consultazione pura può rifiutare in blocco i tool mutanti (serverMode read-only).
        // Complementare ai permessi Sandbox/Supervised/YOLO (che classificano per rischio):
        // questa è una classificazione DICHIARATA dal registrante, non un'euristica.
        if (_readOnlyServer && !tool.ReadOnly)
        {
            _audit.Record(toolName, tool.QualifiedProcedureName, false,
                "read_only_server: mutating tool rejected", 0);
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InvalidRequest,
                Message = $"Read-only server: tool '{tool.Name}' is declared mutating"
            });
        }

        // PERMESSI (Sandbox/Supervised/YOLO): il server MCP è una superficie REMOTA — un
        // client può far invocare tool rischiosi. Gate con lo stesso classificatore del
        // tool-loop locale (AgentPermissionService.IsRisky). Supervised su JSON-RPC non può
        // attendere una UI → errore esplicito (il chiamante usa il flusso HTTP dedicato).
        if (Basic.Agents.AgentPermissionService.IsRisky(tool.Name))
        {
            if (_permissionLevel == Basic.Agents.AgentPermissionLevel.Sandbox)
            {
                return (null, new JsonRpcError
                {
                    Code = JsonRpcErrorCodes.InvalidRequest,
                    Message = $"Permission denied (sandbox): tool '{tool.Name}' is risky"
                });
            }
            if (_permissionLevel == Basic.Agents.AgentPermissionLevel.Supervised)
            {
                return (null, new JsonRpcError
                {
                    Code = JsonRpcErrorCodes.InvalidRequest,
                    Message = $"Permission required: tool '{tool.Name}' needs human approval (not available over MCP)"
                });
            }
        }

        var argumentsNode = paramObj?["arguments"];
        var argumentsJson = argumentsNode?.ToJsonString() ?? "{}";

        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeoutCts.CancelAfter(_toolTimeoutMs);

        var sw = System.Diagnostics.Stopwatch.StartNew();
        McpToolCallResult callResult;
        try
        {
            callResult = await _dispatcher
                .DispatchAsync(tool.QualifiedProcedureName, argumentsJson, timeoutCts.Token)
                .ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            sw.Stop();
            _audit.Record(toolName, tool.QualifiedProcedureName, false, "timeout", (int)sw.ElapsedMilliseconds);
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InternalError,
                Message = $"Tool call timed out after {_toolTimeoutMs}ms"
            });
        }
        catch (Exception ex)
        {
            sw.Stop();
            _audit.Record(toolName, tool.QualifiedProcedureName, false, ex.Message, (int)sw.ElapsedMilliseconds);
            return (null, new JsonRpcError
            {
                Code = JsonRpcErrorCodes.InternalError,
                Message = ex.Message
            });
        }

        sw.Stop();
        _audit.Record(toolName, tool.QualifiedProcedureName, !callResult.IsError, callResult.TextContent,
            (int)sw.ElapsedMilliseconds);

        var content = new JsonArray
        {
            new JsonObject
            {
                ["type"] = "text",
                ["text"] = callResult.TextContent
            }
        };

        return (new JsonObject
        {
            ["content"] = content,
            ["isError"] = callResult.IsError
        }, null);
    }

    private static string Serialize(JsonRpcResponse response) =>
        JsonSerializer.Serialize(response, JsonOptions);

    private static string SerializeError(JsonElement? id, int code, string message) =>
        Serialize(new JsonRpcResponse
        {
            Id = id,
            Error = new JsonRpcError { Code = code, Message = message }
        });
}
