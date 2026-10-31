// FILE: Interpreter.Ast.Networking.cs

namespace Basic;

/// <summary>
/// Rappresenta l'istruzione TCP OPEN per avviare un server in ascolto.
/// </summary>
public sealed record TcpOpenStmt(Expr Port, Target Handle, Expr? Engine, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"TCP OPEN {Port}, {Handle}{(Engine is not null ? $", {Engine}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpOpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpOpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpOpenStmt((Expr)Port.DeepClone(), (Target)Handle.DeepClone(), (Expr?)Engine?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione TCP CONNECT per connettersi a un server.
/// </summary>
public sealed record TcpConnectStmt(Expr Host, Expr Port, Target Handle, Expr? Engine, Expr? Timeout, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"TCP CONNECT {Host}, {Port}, {Handle}{(Engine is not null ? $", {Engine}" : "")}{(Timeout is not null ? $", {Timeout}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpConnectStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpConnectStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpConnectStmt((Expr)Host.DeepClone(), (Expr)Port.DeepClone(), (Target)Handle.DeepClone(),
            (Expr?)Engine?.DeepClone(), (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione TCP ACCEPT per accettare una connessione in entrata.
/// </summary>
public sealed record TcpAcceptStmt(Expr ServerHandle, Target ClientHandle, Expr? Timeout, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"TCP ACCEPT {ServerHandle}, {ClientHandle}{(Timeout is not null ? $", {Timeout}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpAcceptStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpAcceptStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpAcceptStmt((Expr)ServerHandle.DeepClone(), (Target)ClientHandle.DeepClone(),
            (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione TCP RECV per ricevere dati da un client.
/// </summary>
public sealed record TcpRecvStmt(Expr ClientHandle, Target Buffer, Target BytesReceived, Expr? Timeout, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"TCP RECV {ClientHandle}, {Buffer}, {BytesReceived}{(Timeout is not null ? $", {Timeout}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpRecvStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpRecvStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpRecvStmt((Expr)ClientHandle.DeepClone(), (Target)Buffer.DeepClone(),
            (Target)BytesReceived.DeepClone(), (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione TCP SEND per inviare dati a un client.
/// </summary>
public sealed record TcpSendStmt(Expr ClientHandle, Expr Data, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"TCP SEND {ClientHandle}, {Data}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpSendStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpSendStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpSendStmt((Expr)ClientHandle.DeepClone(), (Expr)Data.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione TCP CLOSE per chiudere una connessione o un server.
/// </summary>
public sealed record TcpCloseStmt(Expr Handle, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"TCP CLOSE {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TcpCloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TcpCloseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new TcpCloseStmt((Expr)Handle.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione SSL OPEN per avviare un server sicuro in ascolto.
/// </summary>
/// <param name="Port">La porta su cui ascoltare.</param>
/// <param name="Handle">La variabile che riceverà l'handle del server.</param>
/// <param name="CertificatePath">Il percorso del file del certificato (es. .pfx).</param>
/// <param name="CertificatePassword">La password opzionale per il certificato.</param>
/// <param name="Token">Il token 'SSL' per riferimento.</param>
public sealed record SslOpenStmt(Expr Port, Target Handle, Expr CertificatePath, Expr? CertificatePassword, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SSL OPEN {Port}, {Handle}, {CertificatePath}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SslOpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SslOpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SslOpenStmt((Expr)Port.DeepClone(), (Target)Handle.DeepClone(), (Expr)CertificatePath.DeepClone(),
            (Expr?)CertificatePassword?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione SSL CONNECT per connettersi a un server sicuro.
/// </summary>
/// <param name="Host">L'hostname o IP del server a cui connettersi.</param>
/// <param name="Port">La porta del server (opzionale, default 443).</param>
/// <param name="Handle">La variabile che riceverà l'handle della connessione.</param>
/// <param name="Timeout">Il timeout opzionale per la connessione in millisecondi.</param>
/// <param name="Token">Il token 'SSL' per riferimento.</param>
public sealed record SslConnectStmt(Expr Host, Expr? Port, Target Handle, Expr? Timeout, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SSL CONNECT {Host}, {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SslConnectStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SslConnectStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SslConnectStmt((Expr)Host.DeepClone(), (Expr?)Port?.DeepClone(), (Target)Handle.DeepClone(),
            (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione UDP OPEN per creare un socket UDP.
/// </summary>
public sealed record UdpOpenStmt(Expr? Port, Target Handle, Expr? Engine, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"UDP OPEN {(Port is not null ? Port.ToString() : "")}, {Handle}{(Engine is not null ? $", {Engine}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UdpOpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di UdpOpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UdpOpenStmt((Expr?)Port?.DeepClone(), (Target)Handle.DeepClone(), (Expr?)Engine?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione UDP SEND per inviare un datagramma.
/// </summary>
public sealed record UdpSendStmt(Expr Handle, Expr Host, Expr Port, Expr Data, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UDP SEND {Handle}, {Host}, {Port}, {Data}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UdpSendStmt.
    /// </summary>
    /// <returns>Una nuova istanza di UdpSendStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UdpSendStmt((Expr)Handle.DeepClone(), (Expr)Host.DeepClone(), (Expr)Port.DeepClone(),
            (Expr)Data.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione UDP RECV per ricevere un datagramma.
/// </summary>
public sealed record UdpRecvStmt(
    Expr Handle,
    Target Buffer,
    Target RemoteIp,
    Target RemotePort,
    Expr? Timeout,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"UDP RECV {Handle}, {Buffer}, {RemoteIp}, {RemotePort}{(Timeout is not null ? $", {Timeout}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UdpRecvStmt.
    /// </summary>
    /// <returns>Una nuova istanza di UdpRecvStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UdpRecvStmt((Expr)Handle.DeepClone(), (Target)Buffer.DeepClone(), (Target)RemoteIp.DeepClone(),
            (Target)RemotePort.DeepClone(), (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione UDP CLOSE per chiudere un socket UDP.
/// </summary>
public sealed record UdpCloseStmt(Expr Handle, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UDP CLOSE {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UdpCloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di UdpCloseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UdpCloseStmt((Expr)Handle.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione FETCH usata in modalità comando asincrono con callback.
/// Sostituisce il vecchio e ambiguo FetchStmt.
/// </summary>
/// <param name="Url">L'espressione dell'URL da contattare.</param>
/// <param name="Options">L'espressione opzionale che fornisce un oggetto HTTPOPTIONS.</param>
/// <param name="CallbackName">Il token con il nome della subroutine di callback (obbligatorio in questa modalità).</param>
/// <param name="Token">Il token 'FETCH' per riferimento.</param>
public sealed record HttpFetchStmt(Expr Url, Expr? Options, Token CallbackName, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"FETCH {Url}{(Options is not null ? $", {Options}" : "")}, {CallbackName.Lexeme}()";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpFetchStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpFetchStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpFetchStmt((Expr)Url.DeepClone(), (Expr?)Options?.DeepClone(), (Token)CallbackName.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione SHUTDOWN per chiudere parzialmente una connessione.
/// </summary>
public sealed record ShutdownStmt(Expr Handle, Expr Mode, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SHUTDOWN {Handle}, {Mode}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ShutdownStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ShutdownStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ShutdownStmt((Expr)Handle.DeepClone(), (Expr)Mode.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione SETOPTION per configurare un socket.
/// </summary>
public sealed record SetOptionStmt(Expr Handle, Expr Option, Expr Value, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SETOPTION {Handle}, {Option}, {Value}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SetOptionStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SetOptionStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SetOptionStmt((Expr)Handle.DeepClone(), (Expr)Option.DeepClone(), (Expr)Value.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione HTTP SERVER per avviare o fermare il server web.
/// </summary>
/// <param name="Port">L'espressione della porta su cui ascoltare (per l'avvio).</param>
/// <param name="Callback">Il riferimento alla subroutine di gestione delle richieste.</param>
/// <param name="CertificatePath">L'espressione opzionale del percorso del certificato per HTTPS.</param>
/// <param name="CertificatePassword">L'espressione opzionale della password del certificato.</param>
/// <param name="IsStopCommand">True se il comando è HTTP SERVER STOP.</param>
public sealed record HttpServerStmt(
    Expr? Port,
    Token? Callback,
    Expr? CertificatePath,
    Expr? CertificatePassword,
    bool IsStopCommand)
    : Stmt(Port?.Position ?? Callback?.Position ?? SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return IsStopCommand ? "HTTP SERVER STOP" : $"HTTP SERVER {Port} HANDLER {Callback?.Lexeme}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpServerStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpServerStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpServerStmt((Expr?)Port?.DeepClone(), (Token?)Callback?.DeepClone(),
            (Expr?)CertificatePath?.DeepClone(), (Expr?)CertificatePassword?.DeepClone(), IsStopCommand);
    }
}

/// <summary>
/// Rappresenta l'istruzione HTTP ROUTE per definire un gestore per un percorso specifico.
/// </summary>
/// <param name="Method">L'espressione del metodo HTTP (es. "GET").</param>
/// <param name="Path">L'espressione del template del percorso (es. "/users/{id}").</param>
/// <param name="Callback">Il token con il nome della subroutine di callback.</param>
public sealed record HttpRouteStmt(Expr Method, Expr Path, Token Callback) : Stmt(Method.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"HTTP ROUTE {Method} {Path} TO {Callback.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpRouteStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpRouteStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpRouteStmt((Expr)Method.DeepClone(), (Expr)Path.DeepClone(), (Token)Callback.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione HTTP USE per registrare una subroutine di middleware.
/// </summary>
/// <param name="Path">L'espressione opzionale del percorso base a cui applicare il middleware.</param>
/// <param name="Callback">Il token con il nome della subroutine di middleware.</param>
public sealed record HttpUseStmt(Expr? Path, Token Callback) 
    : Stmt(Path?.Position ?? Callback.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"HTTP USE {(Path is not null ? $"{Path} " : "")}{Callback.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpUseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpUseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpUseStmt((Expr?)Path?.DeepClone(), (Token)Callback.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione USE NEXT per passare il controllo al middleware successivo.
/// </summary>
/// <param name="NextHandle">L'espressione che contiene l'handle del gestore "next".</param>
public sealed record HttpUseNextStmt(Expr NextHandle) : Stmt(NextHandle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"USE NEXT {NextHandle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpUseNextStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpUseNextStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new HttpUseNextStmt((Expr)NextHandle.DeepClone());
    }
}

/// <summary>
/// Istruzione interna fittizia che rappresenta il ciclo di vita del task listener HTTP.
/// Non viene generata dal parser, ma creata al volo dal comando HTTP SERVER.
/// </summary>
public sealed record HttpListenerLoopStmt()
    : Stmt(SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "(VM-Internal: HTTP Listener Loop)";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpListenerLoopStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpListenerLoopStmt.</returns>
    public override AstNode DeepClone()
    {
        return new HttpListenerLoopStmt();
    }
}

/// <summary>
/// Istruzione interna fittizia che rappresenta il ciclo di vita di un task 
/// che gestisce una singola connessione client.
/// </summary>
public sealed record HttpConnectionHandlerStmt() 
    : Stmt(SourcePosition.Unknown)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "(VM-Internal: HTTP Connection Handler)";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpConnectionHandlerStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpConnectionHandlerStmt.</returns>
    public override AstNode DeepClone()
    {
        return new HttpConnectionHandlerStmt();
    }
}

/// <summary>
/// Rappresenta l'istruzione HTTP STATIC per servire file statici.
/// </summary>
public sealed record HttpStaticStmt(Expr UrlPath, Expr PhysicalPath, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"HTTP STATIC {UrlPath} FROM {PhysicalPath}";
    public override AstNode DeepClone() =>
        new HttpStaticStmt((Expr)UrlPath.DeepClone(), (Expr)PhysicalPath.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione HTTP CONFIG configObject per applicare una configurazione.
/// </summary>
public sealed record HttpConfigStmt(Expr ConfigObject, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"HTTP CONFIG {ConfigObject}";

    /// <summary>
    /// Crea una copia profonda di questo nodo HttpConfigStmt.
    /// </summary>
    /// <returns>Una nuova istanza di HttpConfigStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new HttpConfigStmt((Expr)ConfigObject.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione WEBSOCKET CONNECT per connettersi a un server WebSocket remoto.
/// </summary>
public sealed record WebSocketConnectStmt(Expr Host, Expr Port, Target Handle, Expr? Path, Expr? Timeout, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        $"WEBSOCKET CONNECT {Host}, {Port}, {Handle}{(Path is not null ? $", {Path}" : "")}{(Timeout is not null ? $", {Timeout}" : "")}";
    public override AstNode DeepClone()
    {
        return new WebSocketConnectStmt(
            (Expr)Host.DeepClone(), (Expr)Port.DeepClone(), (Target)Handle.DeepClone(),
            (Expr?)Path?.DeepClone(), (Expr?)Timeout?.DeepClone(), (Token)Token.DeepClone());
    }
}