// FILE: Interpreter.Compiler.SubOpCode.cs

// ReSharper disable InconsistentNaming
namespace Compiler;

/// <summary>
/// Contiene gli enumeratori per i sottocomandi usati dagli opcode "prefisso" (es. OP_NET_CMD).
/// </summary>

/// <summary>
/// Sottocomandi per OP_FILE_CMD.
/// </summary>
public enum FileSubOpCode : byte 
{ 
    /// <summary>
    /// Sottocomando per l'istruzione OPEN, che ha molteplici varianti.
    /// </summary>
    OPEN 
}

/// <summary>
/// Sottocomandi per OP_NET_CMD.
/// </summary>
public enum NetSubOpCode : byte 
{ 
    // Comandi TCP
    TCP_OPEN, TCP_CONNECT, TCP_ACCEPT, TCP_SEND, TCP_RECV, TCP_CLOSE, 
    
    // Comandi UDP
    UDP_OPEN, UDP_SEND, UDP_RECV, UDP_CLOSE, 
    
    // Comandi SSL/TLS
    SSL_OPEN, SSL_CONNECT,
    
    // Comandi Generici
    SHUTDOWN, SETOPTION,
    
    // Comandi HTTP (AGGIUNTI)
    HTTP_SERVER,
    HTTP_ROUTE,
    HTTP_USE,
    HTTP_USE_NEXT,
    HTTP_CONFIG,
    // HTTP_STATIC: aggiunto per il backend P-CODE — VisitStatement instradava HttpStaticStmt
    // via EmitExprAndPop (riflessione generica sulle proprieta' Expr del record), che valuta e
    // scarta UrlPath/PhysicalPath senza mai emettere un opcode: HTTP STATIC era percio' un
    // no-op silenzioso nella VM (mai stato cablato, non un caso di regressione).
    HTTP_STATIC,
    // WS_CONNECT: WEBSOCKET CONNECT lato client — stesso identico bug di HTTP_STATIC sopra
    // (WebSocketConnectStmt passava per EmitExprAndPop, no-op silenzioso: la variabile handle
    // restava sempre nil, ws.Send()/ws.Close() erano no-op senza eccezione visibile).
    WS_CONNECT,
    HTTP_BSP,
    // TCP_PEEK: NET.PEEK(handle, count) instradato qui (park-based, ParkAsync) invece del
    // dispatch BuiltInFunction sincrono — vedi Bugs/(solved) -
    // NET-TcpPeek-Bloccante-Hang-Test-Goroutine.md. Prima chiamava IlRuntime.TcpPeek ->
    // NetworkService.TcpPeek (Socket.Receive sincrono) direttamente sul worker GPM: con
    // GMBASIC_GOMAXPROCS=1 l'unico worker restava bloccato per sempre in assenza di dati
    // (nessun timeout nella firma BASIC di NET.PEEK) -> WaitForAllTasks mai ritorna.
    TCP_PEEK,
}

/// <summary>
/// Sottocomandi per OP_AUDIO_CMD.
/// </summary>
public enum AudioSubOpCode : byte 
{ 
    SOUND, PLAY, FILTER 
}

/// <summary>
/// Sottocomandi per OP_CLIP_CMD.
/// </summary>
public enum ClipSubOpCode : byte 
{ 
    GET, SET, CLEAR 
}

/// <summary>
/// Sottocomandi per OP_MATRIX_CMD.
/// </summary>
public enum MatrixSubOpCode : byte 
{ 
    CREATE, IDENTITY, TRANSLATE, ROTATE, SCALE, MULTIPLY,
    PUSH, POP, WORLD, VIEW, PROJECTION
}

/// <summary>
/// Sottocomandi per OP_VECTOR_CMD.
/// </summary>
public enum VectorSubOpCode : byte 
{ 
    ADD, SUB, MUL, DIV, SET, FILLRANDOM, WRAP, ADDWRAP, BOUNCE2D 
}