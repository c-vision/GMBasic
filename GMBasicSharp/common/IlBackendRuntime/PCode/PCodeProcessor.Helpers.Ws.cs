// FILE: IlBackendRuntime/PCode/PCodeProcessor.Helpers.Ws.cs
// PARTIAL — dominio HELPERS/WEBSOCKET: ExecuteWebSocketConnect.
// Migrato da PCodeProcessor.Helpers.cs (2026-08-30 — split, file <= 1200 righe).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.Ledger;
using Compiler;
using System.Globalization;
using System.Linq;
using System.Net;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private object? ExecuteWebSocketConnect(PCodeTask task, object?[] args)
    {
        if (args.Length < 4) return null;
        var host = args[0]?.ToString() ?? "";
        var port = args[1] is double p ? (int)p : 80;
        var path = args[2]?.ToString() ?? "/";
        if (!path.StartsWith('/')) path = "/" + path;
        var timeoutMs = args[3] is double t && t > 0 ? (int)t : 30000;

        var uri = new Uri($"ws://{host}:{port}{path}");
        var globalsTemplate = task.Globals;
        ParkAsync(task, PCodeWaitKind.Network, ConnectAsync, connectedSocket =>
        {
            if (connectedSocket == null)
            {
                task.PushRef(null);
                return;
            }
            var wsHandle = new Basic.IlBackend.PCode.Http.PCodeWebSocketHandle { Socket = connectedSocket };
            StartReceiveLoop(connectedSocket, wsHandle);
            task.PushRef(wsHandle);
        }, timeoutMs, () => task.PushRef(null));
        return ParkedNetResult;

        async ValueTask<System.Net.WebSockets.ClientWebSocket?> ConnectAsync(CancellationToken ct)
        {
            var socket = new System.Net.WebSockets.ClientWebSocket();
            try
            {
                await socket.ConnectAsync(uri, ct).ConfigureAwait(false);
                return socket;
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                socket.Dispose();
                throw;
            }
            catch
            {
                socket.Dispose();
                return null;
            }
        }

        void StartReceiveLoop(System.Net.WebSockets.ClientWebSocket clientSocket,
            Basic.IlBackend.PCode.Http.PCodeWebSocketHandle wsHandle)
        {
            var cancellation = new CancellationTokenSource();
            var externalOperation = _scheduler?.RegisterExternalOperation(cancellation.Cancel, task);
            _ = ReceiveLoopAsync();

            async Task ReceiveLoopAsync()
            {
                try
                {
                    var buffer = new byte[8192];
                    var builder = new System.Text.StringBuilder();
                    var decoder = System.Text.Encoding.UTF8.GetDecoder();
                    while (clientSocket.State == System.Net.WebSockets.WebSocketState.Open &&
                           !cancellation.IsCancellationRequested)
                    {
                        // Non passare il token direttamente a ReceiveAsync: la cancellazione
                        // abortirebbe il WebSocket prima che sia possibile inviare il Close frame.
                        // La race esplicita mantiene il socket Open e rende il loop cancellabile
                        // dallo shutdown/nursery GPM senza trattenere worker dello scheduler.
                        var receiveTask = clientSocket.ReceiveAsync(buffer, CancellationToken.None);
                        var cancelTask = Task.Delay(Timeout.Infinite, cancellation.Token);
                        if (await Task.WhenAny(receiveTask, cancelTask).ConfigureAwait(false) != receiveTask)
                            break;

                        System.Net.WebSockets.WebSocketReceiveResult result;
                        try { result = await receiveTask.ConfigureAwait(false); }
                        catch { break; }

                        if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Close)
                        {
                            builder.Clear();
                            try { await clientSocket.CloseAsync(System.Net.WebSockets.WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false); } catch { }
                            if (wsHandle.OnCloseIp.HasValue && globalsTemplate != null &&
                                !cancellation.IsCancellationRequested)
                                SpawnHttpHandlerTask(globalsTemplate, wsHandle.OnCloseIp.Value, [wsHandle]);
                            return;
                        }
                        // Accumula i frammenti fino a EndOfMessage (un messaggio WS può attraversare
                        // più ReceiveAsync da 8KB — prima ogni frammento scatenava l'OnMessage:
                        // risposte grandi come Page.captureScreenshot arrivavano troncate/rotte).
                        if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Text &&
                            wsHandle.OnMessageIp.HasValue && globalsTemplate != null)
                        {
                            var charCount = decoder.GetCharCount(buffer, 0, result.Count);
                            var chars = new char[charCount];
                            decoder.GetChars(buffer, 0, result.Count, chars, 0);
                            builder.Append(chars);
                            if (result.EndOfMessage)
                            {
                                var text = builder.ToString();
                                builder.Clear();
                                SpawnHttpHandlerTask(globalsTemplate, wsHandle.OnMessageIp.Value, [wsHandle, text]);
                            }
                        }
                    }

                    if (clientSocket.State is System.Net.WebSockets.WebSocketState.Open or
                        System.Net.WebSockets.WebSocketState.CloseReceived)
                    {
                        try { await clientSocket.CloseAsync(System.Net.WebSockets.WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false); } catch { }
                    }
                }
                finally
                {
                    externalOperation?.Dispose();
                    cancellation.Dispose();
                }
            }
        }
    }

    /// <summary>
    /// Handshake WebSocket (calcolo Sec-WebSocket-Accept RFC6455 + risposta 101 scritta a mano)
    /// seguito dall'intera sessione: spawna l'handler della route come task "di connessione"
    /// (stessa firma a 3 argomenti req/res/ws dell'AST — "res" resta vestigiale, nessun sample
    /// lo usa per un handler WebSocket), attende che registri OnMessage/OnClose (side-effect
    /// sincrono di OP_STORE_FIELD su PCodeWebSocketHandle), poi guida un loop di ricezione
    /// asincrono che spawna un PCodeTask per ogni messaggio — stesso pattern generale delle
    /// altre fasi (nessun frame-decoder scritto a mano, WebSocket.CreateFromStream standard).
    /// </summary>
    private async Task HandleWebSocketUpgradeAsync(Basic.IlBackend.PCode.Http.PCodeHttpRouteMatch match, CommonLib.Http.HttpRequest request,
        Stream stream, object?[] globalsTemplate, CancellationToken ct)
    {
        var acceptKey = ComputeWebSocketAcceptKey(request);
        var handshake = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
        await stream.WriteAsync(System.Text.Encoding.ASCII.GetBytes(handshake), ct).ConfigureAwait(false);
        await stream.FlushAsync(ct).ConfigureAwait(false);

        var socket = System.Net.WebSockets.WebSocket.CreateFromStream(stream, isServer: true, subProtocol: null, keepAliveInterval: TimeSpan.FromSeconds(30));
        var wsHandle = new Basic.IlBackend.PCode.Http.PCodeWebSocketHandle { Socket = socket };

        var reqDict = BuildRequestDict(request, match.Parameters);
        var vestigialResponse = new PCodeHttpResponse();
        var connectTask = SpawnHttpHandlerTask(globalsTemplate, match.HandlerIp, [reqDict, vestigialResponse, wsHandle]);
        while (!connectTask.IsFinished && !ct.IsCancellationRequested) await Task.Delay(5, ct).ConfigureAwait(false);

        // IMPORTANTE: ReceiveAsync viene chiamata con CancellationToken.None, MAI con "ct" —
        // passare un token cancellabile direttamente a ReceiveAsync e poi cancellarlo (es. da
        // HTTP SERVER STOP) fa si' che .NET porti l'intero WebSocket in stato ABORTED non appena
        // la cancellazione viene osservata — e una volta Aborted, CloseAsync/SendAsync falliscono
        // sempre: e' troppo tardi per inviare un frame di Close in modo pulito (bug reale, trovato
        // con diagnostica temporanea: "finally reached ... socket.State=Aborted" a ~10ms dalla
        // Cancel(), PRIMA che qualunque codice potesse tentare una chiusura pulita — un precedente
        // fix che riusava "ct" per il CloseAsync nel finally non funzionava mai per questo motivo).
        // La cancellazione va invece "corsa in parallelo" (Task.WhenAny) contro la receive, non
        // iniettata dentro di essa: se la receive vince, si processa il messaggio normalmente; se
        // vince la cancellazione, il socket e' ANCORA Open (la receive resta pendente ma non
        // annullata), quindi si puo' davvero inviare un frame di Close (RFC 6455 §7.1.1 — senza
        // questo, qualunque client WebSocket standard/rigoroso, es. ClientWebSocket .NET o il
        // WebSocket nativo di Node.js, vede lo shutdown come "abnormal closure" code 1006 anche
        // quando e' un HTTP SERVER STOP del tutto legittimo — vedi TODO-PCODE-BUGS.md BUG 25).
        var buffer = new byte[8192];
        var builder = new System.Text.StringBuilder();
        var decoder = System.Text.Encoding.UTF8.GetDecoder();
        while (socket.State == System.Net.WebSockets.WebSocketState.Open)
        {
            var receiveTask = socket.ReceiveAsync(buffer, CancellationToken.None);
            if (!ct.IsCancellationRequested)
            {
                var cancelWaitTask = Task.Delay(Timeout.Infinite, ct);
                var winner = await Task.WhenAny(receiveTask, cancelWaitTask).ConfigureAwait(false);
                if (winner != receiveTask)
                {
                    // cancelWaitTask ha vinto: server.Cts e' stato cancellato (HTTP SERVER STOP) —
                    // il socket e' ancora Open (receiveTask resta pendente, abbandonata: verra'
                    // comunque chiusa/GC-ata insieme al resto quando lo stream sottostante viene
                    // chiuso poco dopo da StopAsync).
                    break;
                }
            }
            else break;

            System.Net.WebSockets.WebSocketReceiveResult result;
            try { result = await receiveTask.ConfigureAwait(false); }
            catch { break; }

            if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Close)
            {
                builder.Clear();
                try { await socket.CloseAsync(System.Net.WebSockets.WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false); } catch { }
                if (wsHandle.OnCloseIp.HasValue) SpawnHttpHandlerTask(globalsTemplate, wsHandle.OnCloseIp.Value, [wsHandle]);
                return;
            }
            if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Text && wsHandle.OnMessageIp.HasValue)
            {
                // Accumula i frammenti fino a EndOfMessage (stesso fix del loop client).
                var charCount = decoder.GetCharCount(buffer, 0, result.Count);
                var chars = new char[charCount];
                decoder.GetChars(buffer, 0, result.Count, chars, 0);
                builder.Append(chars);
                if (result.EndOfMessage)
                {
                    var text = builder.ToString();
                    builder.Clear();
                    SpawnHttpHandlerTask(globalsTemplate, wsHandle.OnMessageIp.Value, [wsHandle, text]);
                }
            }
        }

        // Uscita dal loop per cancellazione (HTTP SERVER STOP) o socket non piu' Open (errore/
        // disconnessione del client): il socket, in questo punto, non e' MAI stato messo Aborted
        // dalla nostra stessa cancellazione (vedi sopra) — se e' ancora in uno stato da cui si
        // puo' inviare un Close, lo si invia.
        if (socket.State is System.Net.WebSockets.WebSocketState.Open or System.Net.WebSockets.WebSocketState.CloseReceived)
        {
            try { await socket.CloseAsync(System.Net.WebSockets.WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false); } catch { }
        }
    }

    /// <summary>Crea e accoda un PCodeTask per un handler HTTP — stesso schema di LaunchGoTask
    /// (PCodeProcessor.cs, usato da OP_GO/OP_GO_FUNCTION), ma senza un task chiamante da cui
    /// estrarre gli argomenti: qui li riceviamo gia' pronti (costruiti dal loop di rete, non da
    /// bytecode in esecuzione). Stesso ordine di push (reverse-index) per garantire lo stesso
    /// binding posizionale che il prologo compilato della SUB si aspetta.</summary>
}
