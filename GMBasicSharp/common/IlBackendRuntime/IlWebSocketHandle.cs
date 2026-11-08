// FILE: Basic.IlBackend.IlWebSocketHandle.cs
// Handle WebSocket REALE per il backend IL (prima `WEBSOCKET CONNECT` apriva solo un socket
// TCP nudo, senza handshake né framing). Avvolge un System.Net.WebSockets.WebSocket standard
// (niente frame-decoder scritto a mano), esattamente come la controparte P-CODE (PCodeWebSocketHandle):
//   - client: ClientWebSocket (handshake RFC 6455 fatto dal BCL, vedi IlRuntime.WsConnect)
//   - server: WebSocket prodotto da HttpListenerContext.AcceptWebSocketAsync (vedi HttpService)
// Esposto a BASIC come oggetto dinamico (tipo WEBSOCKET → object in DataTypeMapper):
//   - Send/SendBytes/Close → dispatchati via CollectionInvoke (reflection case-insensitive)
//   - OnMessage/OnClose/OnError → stringhe col NOME della SUB, assegnate via JsonFieldSet.
// Il dispatch dei messaggi usa i symbolId risolti a COMPILE-TIME (IlRuntime.WsSetHandler, stesso
// pattern di HTTP ROUTE/USE — il nome di procedura non è mai risolvibile a runtime nel modello IL,
// i metodi generati sono Proc_<symbolId>); se il symbolId non è disponibile (assegnazione con
// stringa non costante, rifiutata a compile-time in EmitFieldAssignment), nessun handler parte.

using System.Net.WebSockets;
using System.Text;

namespace Basic.IlBackend;

/// <summary>Handle WebSocket reale per il backend IL (vedi header del file).</summary>
public sealed class IlWebSocketHandle
{
    /// <summary>WebSocket sottostante (ClientWebSocket per il client, AcceptWebSocketAsync per il
    /// server). Esclusivamente interno: il codice BASIC non lo tocca mai direttamente.</summary>
    public required WebSocket Socket { get; init; }

    /// <summary>Scheduler GPM della run che ha aperto la sessione (catturato a HTTP SERVER,
    /// contesto esplicito §0ter punto 4): il dispatch dei messaggi lo usa, MAI il
    /// IlGpmRuntime.CurrentScheduler riletto a dispatch-time (azzerato a fine run). Parità
    /// PCodeVM (task.Scheduler = _scheduler).</summary>
    internal Basic.Runtime.Gpm.PCodeScheduler? Scheduler { get; set; }

    /// <summary>Nome della SUB richiamata a ogni messaggio di testo (es. "WsMessageHandler").</summary>
    public string OnMessage { get; set; } = "";

    /// <summary>Nome della SUB richiamata alla chiusura della connessione.</summary>
    public string OnClose { get; set; } = "";

    /// <summary>Nome della SUB richiamata su errore di lettura.</summary>
    public string OnError { get; set; } = "";

    /// <summary>SymbolId delle SUB risolti a compile-time da <see cref="IlRuntime.WsSetHandler"/>.
    /// -1 = non risolto (nessun handler attivo per quell'evento).</summary>
    internal int OnMessageSymbolId { get; set; } = -1;
    internal int OnCloseSymbolId { get; set; } = -1;
    internal int OnErrorSymbolId { get; set; } = -1;

    /// <summary>MethodInfo della SUB risolto e CACHATO da WsSetHandler (dall'assembly del programma
    /// corrente) — la risoluzione a runtime al momento del messaggio troverebbe la prima
    /// Proc_&lt;symbolId&gt; fra tutti gli assembly caricati, che può appartenere a un altro
    /// programma con lo stesso symbolId.</summary>
    internal System.Reflection.MethodInfo? OnMessageMethod { get; set; }
    internal System.Reflection.MethodInfo? OnCloseMethod { get; set; }
    internal System.Reflection.MethodInfo? OnErrorMethod { get; set; }

    /// <summary>true se è la connessione lato SERVER (usato per distinguere l'upgrade HTTP dal
    /// client — il client gestisce già la chiusura a proprio conto nel loop di ricezione).</summary>
    internal bool IsServerSide { get; init; }

    /// <summary>Invia un messaggio di testo (fire-and-forget, come il VM: errore → ignorato).</summary>
    public void Send(string text)
    {
        try
        {
            if (Socket is null) return;
            Socket.SendAsync(Encoding.UTF8.GetBytes(text ?? ""), WebSocketMessageType.Text, true, CancellationToken.None)
                .GetAwaiter().GetResult();
        }
        catch { /* connessione già chiusa lato remoto: nessuna azione */ }
    }

    /// <summary>Invia un messaggio binario.</summary>
    public void SendBytes(byte[] data)
    {
        try
        {
            Socket.SendAsync(data, WebSocketMessageType.Binary, true, CancellationToken.None).GetAwaiter().GetResult();
        }
        catch { }
    }

    /// <summary>Chiude la connessione in modo pulito (NormalClosure).</summary>
    public void Close(string? reason = null)
    {
        try
        {
            Socket.CloseAsync(WebSocketCloseStatus.NormalClosure, reason ?? "", CancellationToken.None).GetAwaiter().GetResult();
        }
        catch { }
    }

    /// <summary>
    /// Loop di ricezione su thread in background: a ogni messaggio completo (EndOfMessage) invoca
    /// <see cref="IlRuntime.DispatchWsMessage"/> con la SUB OnMessage; alla chiusura remota invoca
    /// OnClose. Fire-and-forget — il chiamante non attende la fine della sessione.
    /// </summary>
    internal void StartReceiveLoop()
    {
        // La lettura del socket gira su un thread .NET (Task.Run), MAI come task GPM verde:
        // ReceiveAsync è bloccante e un read dentro un worker cooperativo fermerebbe lo
        // scheduler. Il DISPATCH delle SUB OnMessage/OnClose/OnError invece è sempre un
        // TASK GPM (SpawnGoSub, vedi DispatchMessage/DispatchError): la SUB gira su un
        // worker con host+scope globale già impostati a inizio worker — niente iniezione
        // esterna nel GPM, niente EnsureContext/[ThreadStatic] fragile a ogni resume.
        // Fallback deterministico (invocazione diretta col contesto catturato al CONNECT)
        // SOLO quando il loop parte fuori da un run GPM (nessuno scheduler attivo).
        var host = IlRuntime.Current;
        var globalScope = IlRuntime.CurrentGlobalScope;
        void EnsureContext()
        {
            IlRuntime.SetCurrentForThread(host);
            if (globalScope != null) IlRuntime.BeginGlobalScope(globalScope);
        }
        _ = Task.Run(async () =>
        {
            EnsureContext();
            var buffer = new byte[8192];
            var builder = new StringBuilder();
            var decoder = Encoding.UTF8.GetDecoder();
            try
            {
                while (Socket.State == WebSocketState.Open)
                {
                    var result = await Socket.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None)
                        .ConfigureAwait(false);
                    EnsureContext();
                    if (result.MessageType == WebSocketMessageType.Close)
                    {
                        DispatchMessage(null, WebSocketMessageType.Close);
                        // Completa l'handshake di chiusura: il peer ha mandato Close, va risposto
                        // con un frame Close (CloseOutputAsync, senza attendere oltre) — altrimenti
                        // il CloseAsync del peer resterebbe appeso per sempre (hang osservato in
                        // test end-to-end col server che rompeva il loop senza rispondere).
                        try { await Socket.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None).ConfigureAwait(false); }
                        catch { }
                        break;
                    }
                    // Decoder UTF-8 stateful: un carattere multi-byte può essere tagliato a metà
                    // da un frammento da 8KB — GetString per-frammento lo corromperebbe.
                    var charCount = decoder.GetCharCount(buffer, 0, result.Count);
                    var chars = new char[charCount];
                    decoder.GetChars(buffer, 0, result.Count, chars, 0);
                    builder.Append(chars);
                    if (result.EndOfMessage)
                    {
                        var text = builder.ToString();
                        builder.Clear();
                        EnsureContext();
                        DispatchMessage(text, result.MessageType);
                    }
                }
            }
            catch (Exception ex)
            {
                EnsureContext();
                DispatchError(ex);
            }
        });
    }

    /// <summary>Dispatch di un messaggio/close WS: TASK GPM (SpawnGoSub) quando lo scheduler
    /// del programma è attivo — la SUB OnMessage/OnClose gira su un worker in-context;
    /// fallback deterministico (invocazione diretta) solo senza GPM.</summary>
    private void DispatchMessage(object? text, System.Net.WebSockets.WebSocketMessageType type)
    {
        var scheduler = Scheduler ?? IlGpmRuntime.CurrentScheduler;
        if (scheduler != null)
        {
            if (type == System.Net.WebSockets.WebSocketMessageType.Close)
            {
                if (OnCloseSymbolId >= 0)
                    IlGpmRuntime.SpawnGoSub(scheduler, OnCloseSymbolId, [this], null);
            }
            else if (type == System.Net.WebSockets.WebSocketMessageType.Text && text is string t)
            {
                if (OnMessageSymbolId >= 0)
                    IlGpmRuntime.SpawnGoSub(scheduler, OnMessageSymbolId, [this, t], null);
            }
            return;
        }
        IlRuntime.DispatchWsMessage(this, text as string, type);
    }

    /// <summary>Dispatch di un errore di lettura WS → SUB OnError (come task GPM se attivo).</summary>
    private void DispatchError(Exception ex)
    {
        var scheduler = Scheduler ?? IlGpmRuntime.CurrentScheduler;
        if (scheduler != null)
        {
            if (OnErrorSymbolId >= 0)
                IlGpmRuntime.SpawnGoSub(scheduler, OnErrorSymbolId, [this, ex.Message], null);
            return;
        }
        IlRuntime.DispatchWsError(this, ex);
    }
}
