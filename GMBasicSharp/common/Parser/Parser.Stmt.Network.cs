// FILE: Interpreter.Parser.Stmt.Network.cs
/*
------------------------------------------------------------------------------------------------------------------------
        GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI NET/HTTP (LEGACY E QUALIFICATA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file sono stati aggiornati per supportare due stili di sintassi per ogni comando,
garantendo la retrocompatibilità e introducendo al contempo un'API moderna e non ambigua per le operazioni
di rete e HTTP.

La sintassi "Legacy" o "Non Qualificata" è quella tradizionale, che richiede l'import del
namespace (`IMPORT NET` o `IMPORT HTTP`) e usa una sintassi a più parole (`TCP OPEN ...`).

La sintassi "Moderna" o "Qualificata Fusa" usa il nome del namespace per invocare il comando come un metodo,
eliminando ogni ambiguità, non richiedendo import globali e riusando gli STESSI argomenti della legacy
SENZA parentesi (direttiva AGENTS.md §2.2).

--- Esempi di Doppia Sintassi ---

1.  TCP OPEN
    Legacy:  TCP OPEN 8080, hndl
    Moderna: NET.TCPOPEN 8080, hndl

2.  TCP CONNECT
    Legacy:  TCP CONNECT "sito.com", 80, hndl
    Moderna: NET.TCPCONNECT "sito.com", 80, hndl

3.  TCP ACCEPT
    Legacy:  TCP ACCEPT serverHndl, clientHndl
    Moderna: NET.TCPACCEPT serverHndl, clientHndl

4.  TCP SEND
    Legacy:  TCP SEND clientHndl, "dati"
    Moderna: NET.TCPSEND clientHndl, "dati"

5.  TCP RECV
    Legacy:  TCP RECV clientHndl, buffer$, bytesRead
    Moderna: NET.TCPRECV clientHndl, buffer$, bytesRead

6.  TCP CLOSE
    Legacy:  TCP CLOSE hndl
    Moderna: NET.TCPCLOSE hndl

7.  UDP OPEN
    Legacy:  UDP OPEN 9000, hndl
    Moderna: NET.UDPOPEN 9000, hndl

8.  SHUTDOWN
    Legacy:  SHUTDOWN hndl, 1
    Moderna: NET.SHUTDOWN hndl, 1

9.  SETOPTION
    Legacy:  SETOPTION hndl, opt, val
    Moderna: NET.SETOPTION hndl, opt, val

10. SSL OPEN
    Legacy:  SSL OPEN 443, hndl, "cert.pfx", "pass"
    Moderna: NET.SSLOPEN 443, hndl, "cert.pfx", "pass"

11. HTTP SERVER
    Legacy:  HTTP SERVER 80
    Moderna: HTTP.SERVER 80

12. HTTP ROUTE
    Legacy:  HTTP ROUTE "GET", "/api/users", GetUsersHandler()
    Moderna: HTTP.ROUTE "GET", "/api/users", GetUsersHandler()

13. FETCH (comando)
    Legacy:  FETCH "url" INTO CallbackHandler()
    Moderna: Non applicabile (la versione qualificata è una funzione, non un comando)

NOTA: Le funzioni di lettura del namespace (es. NET.REMOTEIP(handle), NET.CONN(handle),
HTTP.FETCH(url)) restano funzioni con parentesi (AGENTS.md §2.3).

------------------------------------------------------------------------------------------------------------------------
*/

// ReSharper disable UnusedParameter.Local

// ReSharper disable ParameterOnlyUsedForPreconditionCheck.Local
// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
// ReSharper disable ParameterOnlyUsedForPreconditionCheck.Global
namespace Basic;

/// <summary>
/// Contiene i metodi di parsing per le istruzioni di rete (TCP, UDP, SSL, HTTP).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi che iniziano con la keyword `TCP`.
    /// Gestisce esclusivamente la sintassi legacy.
    /// </summary>
    private static Stmt ParseTcpStatement(Parser parser, string expectedNamespace)
    {

        parser.Consume(TokenKind.Tcp, "Internal parser error: Expected 'TCP'.");

        var commandToken = parser.Peek();

        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return commandToken.Kind switch
        {
            TokenKind.Open => ParseTcpOpenStatement(parser, expectedNamespace),
            TokenKind.Accept => ParseTcpAcceptStatement(parser, expectedNamespace),
            TokenKind.Recv => ParseTcpRecvStatement(parser, expectedNamespace),
            TokenKind.Send => ParseTcpSendStatement(parser, expectedNamespace),
            TokenKind.Close => ParseTcpCloseStatement(parser, expectedNamespace),
            TokenKind.Connect => ParseTcpConnectStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken, "Expected OPEN, ACCEPT, RECV, SEND, CONNECT, or CLOSE after TCP.")
        };
    }

    /// <summary>
    /// Parsa un'istruzione `TCP OPEN` per creare un server in ascolto.
    /// Gestisce sia la sintassi legacy (`TCP OPEN ...`) che quella moderna (`NET.TCPOPEN(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpOpenStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// TCP OPEN 8080, serverHandle
    /// TCP OPEN 8080, serverHandle, "ASYNC"
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPOPEN 8080, serverHandle
    /// NET.TCPOPEN 8080, serverHandle, "ASYNC"
    /// </code>
    /// </example>
    public static TcpOpenStmt ParseTcpOpenStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Open, "Expected 'OPEN'.");

        Expr port;
        Target handle;
        Expr? engine = null;

        // Sintassi unificata (legacy e qualificata fusa): TCP OPEN / NET.TCPOPEN port [, handle] [, engine]
        port = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after port expression in TCP OPEN.");
        handle = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            engine = parser.ParseExpression();
        }

        return new TcpOpenStmt(port, handle, engine, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `TCP ACCEPT` per accettare una connessione client in entrata.
    /// Gestisce sia la sintassi legacy (`TCP ACCEPT ...`) che quella moderna (`NET.TCPACCEPT(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpAcceptStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// ' Accetta una connessione, attesa infinita
    /// TCP ACCEPT serverHandle, clientHandle
    /// ' Accetta una connessione con un timeout di 10 secondi
    /// TCP ACCEPT serverHandle, clientHandle, 10000
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPACCEPT serverHandle, clientHandle
    /// NET.TCPACCEPT serverHandle, clientHandle, 10000
    /// </code>
    /// </example>
    public static Stmt ParseTcpAcceptStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Accept, "Expected 'ACCEPT'.");

        Expr serverHandle;
        Target clientHandle;
        Expr? timeout = null;

        // Sintassi unificata (legacy e qualificata fusa): TCP ACCEPT / NET.TCPACCEPT serverHandle [, clientHandle] [, timeout]
        serverHandle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after server handle in TCP ACCEPT.");
        clientHandle = parser.ParseTarget();
        // Aggiunta la logica di parsing per il timeout opzionale.
        if (parser.Match(TokenKind.Comma))
        {
            timeout = parser.ParseExpression();
        }

        return new TcpAcceptStmt(serverHandle, clientHandle, timeout, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `TCP RECV` per ricevere dati da una connessione.
    /// Gestisce sia la sintassi legacy (`TCP RECV ...`) che quella moderna (`NET.TCPRECV(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpRecvStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// TCP RECV clientHandle, buffer$, bytesRead
    /// TCP RECV clientHandle, buffer$, bytesRead, 5000 ' Con timeout
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPRECV clientHandle, buffer$, bytesRead
    /// NET.TCPRECV clientHandle, buffer$, bytesRead, 5000 ' Con timeout
    /// </code>
    /// </example>
    public static Stmt ParseTcpRecvStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Recv, "Expected 'RECV'.");

        Expr clientHandle;
        Target buffer;
        Target bytesReceived;
        Expr? timeout = null;

        // Sintassi unificata (legacy e qualificata fusa): TCP RECV / NET.TCPRECV clientHandle, buffer$, bytesRead [, timeout]
        clientHandle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after client handle in TCP RECV.");
        buffer = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after buffer variable in TCP RECV.");
        bytesReceived = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            timeout = parser.ParseExpression();
        }

        return new TcpRecvStmt(clientHandle, buffer, bytesReceived, timeout, commandToken);
    }

    /// <summary>
    /// Parsa `TCP SEND` e `NET.TCPSEND` per inviare dati su una connessione.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpSendStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// TCP SEND clientHandle, "Dati da inviare"
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPSEND clientHandle, "Dati da inviare"
    /// </code>
    /// </example>
    public static Stmt ParseTcpSendStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Send, "Expected 'SEND'.");

        Expr clientHandle;
        Expr data;

        // Sintassi unificata (legacy e qualificata fusa): TCP SEND / NET.TCPSEND clientHandle, data
        clientHandle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after client handle in TCP SEND.");
        data = parser.ParseExpression();

        return new TcpSendStmt(clientHandle, data, commandToken);
    }

    /// <summary>
    /// Parsa `TCP CLOSE` e `NET.TCPCLOSE` per chiudere una connessione o un listener.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpCloseStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// TCP CLOSE handle
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPCLOSE handle
    /// </code>
    /// </example>
    public static TcpCloseStmt ParseTcpCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");

        Expr handle;

        // Sintassi unificata (legacy e qualificata fusa): TCP CLOSE / NET.TCPCLOSE handle
        handle = parser.ParseExpression();

        return new TcpCloseStmt(handle, commandToken);
    }

    /// <summary>
    /// Parsa `TCP CONNECT` e `NET.TCPCONNECT` per stabilire una connessione client.
    /// La sintassi è stata aggiornata per avere `timeout` come primo argomento opzionale.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="TcpConnectStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// TCP CONNECT "sito.com", 80, handle
    /// TCP CONNECT "sito.com", 80, handle, 5000, "ASYNC"
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.TCPCONNECT "sito.com", 80, handle
    /// NET.TCPCONNECT "sito.com", 80, handle, 5000, "ASYNC"
    /// </code>
    /// </example>
    public static Stmt ParseTcpConnectStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Connect, "Expected 'CONNECT'.");

        Expr? engine = null;
        Expr? timeout = null;

        // Sintassi unificata (legacy e qualificata fusa): TCP CONNECT / NET.TCPCONNECT host, port, handle [, timeout] [, engine]
        var host = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after host expression.");
        var port = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after port expression.");
        var handle = parser.ParseTarget();

        if (parser.Match(TokenKind.Comma))
        {
            // Il primo argomento opzionale è ora il timeout.
            timeout = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
            {
                // Il secondo argomento opzionale è l'engine.
                engine = parser.ParseExpression();
            }
        }

        return new TcpConnectStmt(host, port, handle, engine, timeout, commandToken);
    }

    /// <summary>
    /// Dispatcher per i comandi che iniziano con la keyword `UDP`.
    /// Gestisce esclusivamente la sintassi legacy (`UDP OPEN`, `UDP SEND`, etc.).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace (dovrebbe essere `null` per la sintassi legacy).</param>
    /// <returns>Il nodo AST per l'istruzione UDP parsata.</returns>
    private static Stmt ParseUdpStatement(Parser parser, string expectedNamespace)
    {

        parser.Consume(TokenKind.Udp, "Internal parser error: Expected 'UDP'.");
        var commandToken = parser.Peek();

        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return commandToken.Kind switch
        {
            TokenKind.Open => ParseUdpOpenStatement(parser, expectedNamespace),
            TokenKind.Send => ParseUdpSendStatement(parser, expectedNamespace),
            TokenKind.Recv => ParseUdpRecvStatement(parser, expectedNamespace),
            TokenKind.Close => ParseUdpCloseStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken, "Expected OPEN, SEND, RECV, or CLOSE after UDP.")
        };
    }

    /// <summary>
    /// Parsa `UDP OPEN` e `NET.UDPOPEN` per creare un socket UDP.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="UdpOpenStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// ' Crea un server UDP in ascolto sulla porta 9090
    /// UDP OPEN 9090, handle
    /// ' Crea un client UDP con una porta scelta dal sistema
    /// UDP OPEN , handle 
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.UDPOPEN 9090, handle
    /// NET.UDPOPEN 0, handle
    /// </code>
    /// </example>
    public static UdpOpenStmt ParseUdpOpenStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Open, "Expected 'OPEN'.");

        Expr? port = null;
        Target handle;
        Expr? engine = null;

        // Sintassi unificata (legacy e qualificata fusa): UDP OPEN / NET.UDPOPEN [port,] handle [, engine]
        if (!parser.Check(TokenKind.Comma))
        {
            port = parser.ParseExpression();
        }

        parser.Consume(TokenKind.Comma, "Expected ',' after port expression or at the start of UDP OPEN.");
        handle = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            engine = parser.ParseExpression();
        }

        return new UdpOpenStmt(port, handle, engine, commandToken);
    }

    /// <summary>
    /// Parsa `UDP SEND` e `NET.UDPSEND` per inviare un datagramma.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="UdpSendStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// UDP SEND handle, "127.0.0.1", 9090, "Messaggio"
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.UDPSEND handle, "127.0.0.1", 9090, "Messaggio"
    /// </code>
    /// </example>
    public static Stmt ParseUdpSendStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Send, "Expected 'SEND'.");

        Expr handle;
        Expr host;
        Expr port;
        Expr data;

        // Sintassi unificata (legacy e qualificata fusa): UDP SEND / NET.UDPSEND handle, host, port, data
        handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle in UDP SEND.");
        host = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after host in UDP SEND.");
        port = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after port in UDP SEND.");
        data = parser.ParseExpression();

        return new UdpSendStmt(handle, host, port, data, commandToken);
    }

    /// <summary>
    /// Parsa `UDP RECV` e `NET.UDPRECV` per ricevere un datagramma.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="UdpRecvStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// UDP RECV handle, buffer$, ip$, port, 5000
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.UDPRECV handle, buffer$, ip$, port, 5000
    /// </code>
    /// </example>
    public static Stmt ParseUdpRecvStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Recv, "Expected 'RECV'.");

        Expr handle;
        Target buffer;
        Target remoteIp;
        Target remotePort;
        Expr? timeout = null;

        // Sintassi unificata (legacy e qualificata fusa): UDP RECV / NET.UDPRECV handle, buffer$, ip$, port [, timeout]
        handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle in UDP RECV.");
        buffer = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after buffer variable in UDP RECV.");
        remoteIp = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after remote IP variable in UDP RECV.");
        remotePort = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            timeout = parser.ParseExpression();
        }

        return new UdpRecvStmt(handle, buffer, remoteIp, remotePort, timeout, commandToken);
    }

    /// <summary>
    /// Parsa `UDP CLOSE` e `NET.UDPCLOSE` per chiudere un socket UDP.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="UdpCloseStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// UDP CLOSE handle
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.UDPCLOSE handle
    /// </code>
    /// </example>
    public static UdpCloseStmt ParseUdpCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");

        Expr handle;

        // Sintassi unificata (legacy e qualificata fusa): UDP CLOSE / NET.UDPCLOSE handle
        handle = parser.ParseExpression();

        return new UdpCloseStmt(handle, commandToken);
    }

    /// <summary>
    /// Parsa `SHUTDOWN` e `NET.SHUTDOWN` per chiudere parzialmente o totalmente una connessione.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="ShutdownStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// SHUTDOWN handle, 1 ' Disabilita l'invio
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.SHUTDOWN handle, 1
    /// </code>
    /// </example>
    public static Stmt ParseShutdownStatement(Parser parser, string expectedNamespace)
    {
        var shutdownToken = parser.Consume(TokenKind.Shutdown, "Internal parser error: Expected 'SHUTDOWN'.");
        Expr handle;
        Expr mode;

        // Sintassi unificata (legacy e qualificata fusa): SHUTDOWN / NET.SHUTDOWN handle, mode
        handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle in SHUTDOWN statement.");
        mode = parser.ParseExpression();

        return new ShutdownStmt(handle, mode, shutdownToken);
    }

    /// <summary>
    /// Parsa `SETOPTION` e `NET.SETOPTION` per configurare un socket.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="SetOptionStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// SETOPTION handle, "TCP_NODELAY", 1
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.SETOPTION handle, "TCP_NODELAY", 1
    /// </code>
    /// </example>
    public static Stmt ParseSetOptionStatement(Parser parser, string expectedNamespace)
    {
        var setOptionToken = parser.Consume(TokenKind.SetOption, "Internal parser error: Expected 'SETOPTION'.");
        Expr handle;
        Expr option;
        Expr value;

        // Sintassi unificata (legacy e qualificata fusa): SETOPTION / NET.SETOPTION handle, option, value
        handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle in SETOPTION statement.");
        option = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after option in SETOPTION statement.");
        value = parser.ParseExpression();

        return new SetOptionStmt(handle, option, value, setOptionToken);
    }

    /// <summary>
    /// Dispatcher per i comandi che iniziano con la keyword `SSL`.
    /// Gestisce esclusivamente la sintassi legacy (`SSL OPEN`, `SSL CONNECT`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace (dovrebbe essere `null` per la sintassi legacy).</param>
    /// <returns>Il nodo AST per l'istruzione SSL parsata.</returns>
    private static Stmt ParseSslStatement(Parser parser, string expectedNamespace)
    {

        parser.Consume(TokenKind.Ssl, "Expected 'SSL'.");

        var commandToken = parser.Peek();
        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return commandToken.Kind switch
        {
            TokenKind.Open => ParseSslOpenStatement(parser, expectedNamespace),
            TokenKind.Connect => ParseSslConnectStatement(parser, expectedNamespace),
            _ => throw CreateError(parser.Peek(), "Expected 'OPEN' or 'CONNECT' after 'SSL'.")
        };
    }

    /// <summary>
    /// Parsa un'istruzione `SSL OPEN` per creare un server SSL/TLS in ascolto.
    /// Gestisce sia la sintassi legacy (`SSL OPEN ...`) che quella moderna (`NET.SSLOPEN(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="SslOpenStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// SSL OPEN 8443, handle, "cert.pfx", "password"
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.SSLOPEN 8443, handle, "cert.pfx", "password"
    /// </code>
    /// </example>
    public static SslOpenStmt ParseSslOpenStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Open, "Expected 'OPEN'.");

        Expr port;
        Target handle;
        Expr certPath;
        Expr? certPassword = null;

        // Sintassi unificata (legacy e qualificata fusa): SSL OPEN / NET.SSLOPEN port, handle, certPath [, certPassword]
        port = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after port expression.");
        handle = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle variable.");
        certPath = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma))
        {
            certPassword = parser.ParseExpression();
        }

        return new SslOpenStmt(port, handle, certPath, certPassword, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `SSL CONNECT` per stabilire una connessione client sicura.
    /// Gestisce sia la sintassi legacy (`SSL CONNECT ...`) che quella moderna (`NET.SSLCONNECT(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("NET") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="SslConnectStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// ' Connessione standard
    /// SSL CONNECT "api.server.com", handle
    /// ' Con porta e timeout
    /// SSL CONNECT "api.server.com", handle, 443, 10000
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// NET.SSLCONNECT "api.server.com", handle
    /// NET.SSLCONNECT "api.server.com", handle, 443, 10000
    /// </code>
    /// </example>
    public static Stmt ParseSslConnectStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Connect, "Expected 'CONNECT'.");

        Expr host;
        Target handle;
        Expr? port = null;
        Expr? timeout = null;

        // Sintassi unificata (legacy e qualificata fusa): SSL CONNECT / NET.SSLCONNECT host, handle [, port] [, timeout]
        host = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after host expression.");
        handle = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            port = parser.ParseExpression();
        }

        if (parser.Match(TokenKind.Comma))
        {
            timeout = parser.ParseExpression();
        }

        return new SslConnectStmt(host, port, handle, timeout, commandToken);
    }

    /// <summary>
    /// Parsa l'istruzione FETCH asincrona con callback (solo sintassi legacy).
    /// Questa forma è un'istruzione, non una funzione, e richiede una clausola `INTO`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace (dovrebbe essere `null` per la sintassi legacy).</param>
    /// <returns>Un nodo AST <see cref="HttpFetchStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// FETCH "https://api.ipify.org" INTO HandleResponse()
    /// FETCH "https://httpbin.org/post", myOptions INTO HandleResponse()
    /// </code>
    /// </example>
    public static Stmt ParseFetchStatement(Parser parser, string expectedNamespace)
    {
        var fetchToken = parser.Consume(TokenKind.Fetch, "Expected 'FETCH' keyword.");
        var url = parser.ParseExpression();
        Expr? options = null;
        if (parser.Match(TokenKind.Comma))
        {
            if (!parser.Check(TokenKind.Into))
            {
                options = parser.ParseExpression();
            }
        }
        parser.Consume(TokenKind.Into, "Expected 'INTO' to specify a callback for the FETCH statement.");
        var callbackToken = parser.Consume(TokenKind.Identifier, "Expected a subroutine name for the callback.");
        // La sintassi della callback può avere o meno le parentesi.
        if (parser.Match(TokenKind.LeftParen))
        {
            parser.Consume(TokenKind.RightParen, "Expected ')' after callback name.");
        }
        return new HttpFetchStmt(url, options, callbackToken, fetchToken);
    }

    /// <summary>
    /// Dispatcher per i comandi che iniziano con la keyword `HTTP`.
    /// Gestisce esclusivamente la sintassi legacy (`HTTP SERVER`, `HTTP ROUTE`, etc.).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace (dovrebbe essere `null` per la sintassi legacy).</param>
    /// <returns>Il nodo AST per l'istruzione HTTP parsata.</returns>
    public static Stmt ParseHttpStatement(Parser parser, string expectedNamespace)
    {

        parser.Consume(TokenKind.Http, "Expected 'HTTP'.");

        var commandToken = parser.Peek();
        return ParseHttpSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Parsa `HTTP CONFIG` e `HTTP.CONFIG` per applicare una configurazione al server.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="HttpConfigStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// HTTP CONFIG myConfigObject
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// HTTP.CONFIG myConfigObject
    /// </code>
    /// </example>
    public static Stmt ParseHttpConfigStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (CONFIG) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Config, "Expected 'CONFIG'.");

        Expr configObject;

        // Sintassi unificata (legacy e qualificata fusa): HTTP CONFIG / HTTP.CONFIG configObject
        configObject = parser.ParseExpression();

        return new HttpConfigStmt(configObject, commandToken);
    }
    internal static Stmt ParseHttpSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Server => ParseHttpServerStatement(parser, expectedNamespace),
            TokenKind.Route => ParseHttpRouteStatement(parser, expectedNamespace),
            TokenKind.Use => parser.At(1).Kind == TokenKind.Next
                ? ParseHttpUseNextStatement(parser, expectedNamespace)
                : ParseHttpUseStatement(parser, expectedNamespace),
            TokenKind.Config => ParseHttpConfigStatement(parser, expectedNamespace),
            TokenKind.Static => ParseHttpStaticStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken, "Expected 'SERVER', 'ROUTE', 'USE', 'CONFIG', or 'STATIC' after 'HTTP'.")
        };
    }



    public static Stmt ParseWebSocketStatement(Parser parser, string expectedNamespace)
    {

        parser.Consume(TokenKind.WebSocket, "Internal parser error: Expected 'WEBSOCKET'.");

        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Connect => ParseWebSocketConnectStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken, "Expected 'CONNECT' after 'WEBSOCKET'.")
        };
    }

    public static Stmt ParseWebSocketConnectStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (CONNECT) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Connect, "Expected 'CONNECT'.");

        Expr host;
        Expr port;
        Expr? pathExpr = null;
        Expr? timeoutExpr = null;

        // Sintassi unificata (legacy e qualificata fusa): WEBSOCKET CONNECT / NET.WSCONNECT host, port, handle [, path] [, timeout]
        host = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after host.");
        port = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after port.");
        var handleTarget = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            pathExpr = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
                timeoutExpr = parser.ParseExpression();
        }
        return new WebSocketConnectStmt(host, port, handleTarget, pathExpr, timeoutExpr, commandToken);
    }

    public static Stmt ParseHttpStaticStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (STATIC) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Static, "Expected 'STATIC'.");

        Expr urlPath;
        Expr physicalPath;

        // Sintassi unificata (legacy e qualificata fusa): HTTP STATIC / HTTP.STATIC urlPath, physicalPath
        urlPath = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after URL path.");
        physicalPath = parser.ParseExpression();

        return new HttpStaticStmt(urlPath, physicalPath, commandToken);
    }

    /// <summary>
    /// Parsa `HTTP USE` e `HTTP.USE` per registrare un middleware.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="HttpUseStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// HTTP USE LoggerMiddleware()
    /// HTTP USE "/api", AuthMiddleware()
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// HTTP.USE LoggerMiddleware()
    /// HTTP.USE "/api", AuthMiddleware()
    /// </code>
    /// </example>
    public static HttpUseStmt ParseHttpUseStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (USE) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Use, "Expected 'USE'.");

        Expr? path = null;
        Token callback;

        // Sintassi unificata (legacy e qualificata fusa): HTTP USE / HTTP.USE [path,] callbackName()
        if (parser.At(1).Kind != TokenKind.LeftParen)
        {
            path = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after path expression.");
        }

        callback = parser.Consume(TokenKind.Identifier, "Expected middleware subroutine name.");
        parser.Consume(TokenKind.LeftParen, "Expected '()' after callback name.");
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        return new HttpUseStmt(path, callback);
    }

    /// <summary>
    /// Parsa `HTTP SERVER` e `HTTP.SERVER` per avviare o fermare il server web.
    /// VERSIONE CORRETTA: Gestisce tutti i parametri come opzionali e non richiede più un handler,
    /// allineandosi all'architettura basata su `HTTP ROUTE`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="HttpServerStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// Avvio con configurazione globale (definita da `HTTP CONFIG`):
    /// <code>
    /// HTTP SERVER
    /// </code>
    /// 
    /// Avvio con override della porta e handler globale (sconsigliato):
    /// <code>
    /// HTTP SERVER 8080, MainHandler()
    /// </code>
    /// 
    /// Avvio HTTPS con override di porta e certificato:
    /// <code>
    /// HTTP SERVER 8443, "path/to/cert.pfx", "password"
    /// </code>
    /// 
    /// Arresto del server:
    /// <code>
    /// HTTP SERVER STOP
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// Avvio con configurazione globale:
    /// <code>
    /// HTTP.SERVER
    /// </code>
    /// 
    /// Avvio con override della porta:
    /// <code>
    /// HTTP.SERVER 9000
    /// </code>
    /// 
    /// Avvio HTTPS con override completo:
    /// <code>
    /// HTTP.SERVER 8443, "path/to/cert.pfx", "password"
    /// </code>
    /// 
    /// Arresto del server:
    /// <code>
    /// HTTP.SERVER STOP
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseHttpServerStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (SERVER) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Server, "Expected 'SERVER'.");

        // --- Sintassi unificata (legacy e qualificata fusa): HTTP SERVER / HTTP.SERVER ... ---

        // Caso 1: HTTP SERVER STOP
        if (parser.Match(TokenKind.Stop))
        {
            return new HttpServerStmt(null, null, null, null, true);
        }

        // Caso 2: HTTP SERVER (senza argomenti) - Avvio con configurazione globale
        if (parser.IsAtEndOfStatement())
        {
            return new HttpServerStmt(null, null, null, null, false);
        }

        // Caso 3: HTTP SERVER [port] HANDLER handler() [, cert] [, pass]
        Expr? portLegacy = null;
        Token? callbackLegacy = null;
        Expr? certPathLegacy = null;
        Expr? certPasswordLegacy = null;

        // Il primo argomento è quasi sempre la porta.
        if (parser.Peek().Kind != TokenKind.Handler)
        {
            portLegacy = parser.ParseExpression();
            parser.Match(TokenKind.Comma); // La virgola dopo la porta è opzionale
        }

        // Controlla la clausola HANDLER
        if (parser.Match(TokenKind.Handler))
        {
            // Il gestore DEVE essere una chiamata a funzione (sintatticamente un ArrayAccessExpr in Fase 1).
            var handlerExpression = parser.ParseExpression();

            // In Fase 1 (Parsing Puro), una chiamata come `MioHandler()` viene parsata come un `ArrayAccessExpr`.
            // Verifichiamo che il nodo sia di questo tipo e ne estraiamo il nome.
            if (handlerExpression is ArrayAccessExpr callExpr)
            {
                // La chiamata è sintatticamente corretta. Estraiamo il nome.
                callbackLegacy = callExpr.NameCleaned;
            }
            else
            {
                // Se non è una chiamata (es. solo un nome come `HandleRequest`), lanciamo un errore
                // richiedendo esplicitamente la sintassi con le parentesi.
                throw CreateError(new Token(TokenKind.Unknown, handlerExpression.ToString(), null, handlerExpression.Position), 
                    "La sintassi per il gestore richiede le parentesi. Es: 'HANDLER MioHandler()'.");
            }
        }

        // Se ci sono altri argomenti, sono per il certificato (HTTPS)
        if (parser.Match(TokenKind.Comma))
        {
            certPathLegacy = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
            {
                certPasswordLegacy = parser.ParseExpression();
            }
        }

        return new HttpServerStmt(portLegacy, callbackLegacy, certPathLegacy, certPasswordLegacy, false);
    }
    
    /// <summary>
    /// Parsa `HTTP ROUTE` e `HTTP.ROUTE` per definire un endpoint specifico.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="HttpRouteStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// HTTP ROUTE "GET", "/users", GetUsersHandler()
    /// </code>
    /// Sintassi Moderna/Qualificata Fusa:
    /// <code>
    /// HTTP.ROUTE "GET", "/users", GetUsersHandler()
    /// </code>
    /// </example>
    public static Stmt ParseHttpRouteStatement(Parser parser, string expectedNamespace)
    {
        // Il verbo fuso (ROUTE) è una keyword: lo consumiamo identico per legacy e qualificata fusa.
        var commandToken = parser.Consume(TokenKind.Route, "Expected 'ROUTE'.");

        Expr method;
        Expr path;
        Token callback;

        // Sintassi unificata (legacy e qualificata fusa): HTTP ROUTE / HTTP.ROUTE method, path, callbackName()
        method = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after HTTP method.");
        path = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after path expression.");

        // La callback è un nome di subroutine, seguito opzionalmente da ()
        callback = parser.Consume(TokenKind.Identifier, "Expected callback subroutine name.");
        if (parser.Match(TokenKind.LeftParen))
        {
            parser.Consume(TokenKind.RightParen, "Expected ')' after callback name.");
        }

        return new HttpRouteStmt(method, path, callback);
    }
    
    /// <summary>
    /// Parsa `HTTP USE NEXT` e `HTTP.USENEXT` per passare il controllo al middleware successivo.
    /// Questa versione supporta sia la sintassi a due parole (`USE NEXT`) che quella a parola singola
    /// (`USENEXT`), e gestisce il caso in cui l'handle del middleware successivo viene omesso.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("HTTP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="HttpUseNextStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// Passaggio esplicito dell'handle:
    /// <code>
    /// SUB MyMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    ///     ' ... logica ...
    ///     HTTP USE NEXT next
    /// END SUB
    /// </code>
    /// 
    /// Passaggio implicito (senza argomenti):
    /// <code>
    /// SUB MyMiddleware(req AS HTTPREQUEST, res AS HTTPRESPONSE, next AS NEXTHANDLER)
    ///     ' ... logica ...
    ///     HTTP USE NEXT
    /// END SUB
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// Passaggio esplicito dell'handle:
    /// <code>
    /// HTTP.USENEXT next
    /// </code>
    /// 
    /// Passaggio implicito (senza argomenti):
    /// <code>
    /// HTTP.USENEXT
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseHttpUseNextStatement(Parser parser, string expectedNamespace)
    {
        Expr nextHandleExpr;

        // --- GESTIONE SINTASSI LEGACY: HTTP USE NEXT [handle] ---
        {
            // Il dispatcher ha già verificato la sequenza "USE NEXT". La consumiamo.
            var useToken = parser.Consume(TokenKind.Use, "Internal parser error: Expected 'USE'.");
            parser.Consume(TokenKind.Next, "Internal parser error: Expected 'NEXT'.");

            // Controlla se l'handle è omesso (fine dell'istruzione).
            if (parser.IsAtEndOfStatement())
            {
                // L'handle è omesso. Creiamo un riferimento implicito.
                var implicitNextToken = new Token(TokenKind.Identifier, "next", null, useToken.Position);
                nextHandleExpr = new VariableExpr(-1, implicitNextToken, implicitNextToken);
            }
            else
            {
                // L'handle è fornito esplicitamente.
                nextHandleExpr = parser.ParseExpression();
            }
        }

        return new HttpUseNextStmt(nextHandleExpr);
    }
}