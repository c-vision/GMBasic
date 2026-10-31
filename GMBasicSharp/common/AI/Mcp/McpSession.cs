// FILE: Basic.Mcp.McpSession
using System.Text.Json.Nodes;

namespace Basic.Mcp;

/// <summary>MCP session state after handshake.</summary>
public sealed class McpSession
{
    public bool IsInitialized { get; private set; }

    public string ClientName { get; private set; } = "unknown";

    public string ProtocolVersion { get; private set; } = McpConstants.ProtocolVersion;

    public JsonObject ServerCapabilities { get; } = new()
    {
        ["tools"] = new JsonObject { ["listChanged"] = true },
        ["resources"] = new JsonObject { ["subscribe"] = true, ["listChanged"] = true },
        ["prompts"] = new JsonObject { ["listChanged"] = true },
        ["logging"] = new JsonObject()
    };

    /// <summary>Apply initialize params and mark session ready.</summary>
    public void Initialize(JsonObject? clientParams)
    {
        if (clientParams?.TryGetPropertyValue("protocolVersion", out var pvNode) == true &&
            pvNode is JsonValue pv && pv.TryGetValue<string>(out var pvStr) && !string.IsNullOrWhiteSpace(pvStr))
            ProtocolVersion = pvStr;

        if (clientParams?.TryGetPropertyValue("clientInfo", out var ciNode) == true &&
            ciNode is JsonObject ci && ci.TryGetPropertyValue("name", out var nameNode) &&
            nameNode is JsonValue nameVal && nameVal.TryGetValue<string>(out var nameStr))
            ClientName = nameStr;

        IsInitialized = true;
    }

    /// <summary>Build initialize result payload.</summary>
    public JsonObject CreateInitializeResult() => new()
    {
        ["protocolVersion"] = ProtocolVersion,
        ["capabilities"] = ServerCapabilities.DeepClone(),
        ["serverInfo"] = new JsonObject
        {
            ["name"] = "gmbasic-mcp",
            ["version"] = "1.0.0"
        }
    };
}
