// FILE: Basic.IlBackend.PCode.Http.PCodeWebSocketHandle.cs
// Handle WebSocket esposto al codice BASIC via OP_CALL_MEMBER (Send/SendBytes/Close) e
// OP_LOAD_FIELD/OP_STORE_FIELD (OnMessage/OnClose/OnError — sintassi legacy per nome di
// procedura, es. ws.OnMessage = "WsMessageHandler", risolta a indirizzo Ip tramite
// CompiledChunk.ProcedureAddresses). Usa System.Net.WebSockets.WebSocket standard (niente
// frame-decoder scritto a mano come nell'AST).

using System.Net.WebSockets;
using System.Text;

namespace Basic.IlBackend.PCode.Http;

internal sealed class PCodeWebSocketHandle
{
    public required WebSocket Socket { get; init; }
    public int? OnMessageIp { get; set; }
    public int? OnCloseIp { get; set; }
    public int? OnErrorIp { get; set; }

    public async ValueTask<bool> SendAsync(string text, CancellationToken cancellationToken)
    {
        try
        {
            await Socket.SendAsync(Encoding.UTF8.GetBytes(text), WebSocketMessageType.Text, true, cancellationToken)
                .ConfigureAwait(false);
            return true;
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested) { throw; }
        catch { return false; }
    }

    public async ValueTask<bool> SendBytesAsync(byte[] data, CancellationToken cancellationToken)
    {
        try
        {
            await Socket.SendAsync(data, WebSocketMessageType.Binary, true, cancellationToken)
                .ConfigureAwait(false);
            return true;
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested) { throw; }
        catch { return false; }
    }

    public async ValueTask<bool> CloseAsync(string? reason, CancellationToken cancellationToken)
    {
        try
        {
            await Socket.CloseAsync(WebSocketCloseStatus.NormalClosure, reason ?? "", cancellationToken)
                .ConfigureAwait(false);
            return true;
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested) { throw; }
        catch { return false; }
    }
}
