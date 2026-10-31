// FILE: Interpreter.Compiler.Compiler.Network.cs

// ReSharper disable UseUtf8StringLiteral



namespace Compiler;

/// <summary>
/// Contiene i metodi per compilare le istruzioni di rete (TCP, UDP, SSL) in P-Code.
/// </summary>
public sealed partial class Compiler
{
    // ========================================================================
    // --- Compilazione Comandi TCP ---
    // ========================================================================

    /// <summary>
    /// Compila un'istruzione TCP OPEN.
    /// </summary>
    private void VisitTcpOpenStatement(TcpOpenStmt stmt)
    {
        VisitExpression(stmt.Port);
        if (stmt.Engine is not null) VisitExpression(stmt.Engine); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_OPEN, 2);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione TCP CONNECT.
    /// </summary>
    private void VisitTcpConnectStatement(TcpConnectStmt stmt)
    {
        VisitExpression(stmt.Host);
        VisitExpression(stmt.Port);
        if (stmt.Engine is not null) VisitExpression(stmt.Engine); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_CONNECT, 4);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione TCP ACCEPT.
    /// </summary>
    private void VisitTcpAcceptStatement(TcpAcceptStmt stmt)
    {
        VisitExpression(stmt.ServerHandle);
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_ACCEPT, 2);
        AssignVariable(stmt.ClientHandle, isLet: false);
    }
    
    /// <summary>
    /// Compila un'istruzione TCP RECV.
    /// </summary>
    private void VisitTcpRecvStatement(TcpRecvStmt stmt)
    {
        VisitExpression(stmt.ClientHandle);
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_RECV, 2);
        
        AssignVariable(stmt.BytesReceived, isLet: false);
        AssignVariable(stmt.Buffer, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione TCP SEND.
    /// </summary>
    private void VisitTcpSendStatement(TcpSendStmt stmt)
    {
        VisitExpression(stmt.ClientHandle);
        VisitExpression(stmt.Data);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_SEND, 2);
    }

    /// <summary>
    /// Compila un'istruzione TCP CLOSE.
    /// </summary>
    private void VisitTcpCloseStatement(TcpCloseStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.TCP_CLOSE, 1);
    }

    // ========================================================================
    // --- Compilazione Comandi SSL/TLS ---
    // ========================================================================
    
    /// <summary>
    /// Compila un'istruzione SSL OPEN.
    /// </summary>
    private void VisitSslOpenStatement(SslOpenStmt stmt)
    {
        VisitExpression(stmt.Port);
        VisitExpression(stmt.CertificatePath);
        if (stmt.CertificatePassword is not null) VisitExpression(stmt.CertificatePassword); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.SSL_OPEN, 3);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione SSL CONNECT.
    /// </summary>
    private void VisitSslConnectStatement(SslConnectStmt stmt)
    {
        VisitExpression(stmt.Host);
        if (stmt.Port is not null) VisitExpression(stmt.Port); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.SSL_CONNECT, 3);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione WEBSOCKET CONNECT (client). Stesso schema di VisitSslConnectStatement
    /// sopra — prima era EmitExprAndPop (no-op silenzioso, vedi commento su NetSubOpCode.WS_CONNECT).
    /// </summary>
    private void VisitWebSocketConnectStatement(WebSocketConnectStmt stmt)
    {
        VisitExpression(stmt.Host);
        VisitExpression(stmt.Port);
        if (stmt.Path is not null) VisitExpression(stmt.Path); else EmitConstant("/");
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.WS_CONNECT, 4);
        AssignVariable(stmt.Handle, isLet: false);
    }

    // ========================================================================
    // --- Compilazione Comandi UDP ---
    // ========================================================================

    /// <summary>
    /// Compila un'istruzione UDP OPEN.
    /// </summary>
    private void VisitUdpOpenStatement(UdpOpenStmt stmt)
    {
        if (stmt.Port is not null) VisitExpression(stmt.Port); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Engine is not null) VisitExpression(stmt.Engine); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.UDP_OPEN, 2);
        AssignVariable(stmt.Handle, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione UDP SEND.
    /// </summary>
    private void VisitUdpSendStatement(UdpSendStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Host);
        VisitExpression(stmt.Port);
        VisitExpression(stmt.Data);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.UDP_SEND, 4);
    }

    /// <summary>
    /// Compila un'istruzione UDP RECV.
    /// </summary>
    private void VisitUdpRecvStatement(UdpRecvStmt stmt)
    {
        VisitExpression(stmt.Handle);
        if (stmt.Timeout is not null) VisitExpression(stmt.Timeout); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.UDP_RECV, 2);

        AssignVariable(stmt.RemotePort, isLet: false);
        AssignVariable(stmt.RemoteIp, isLet: false);
        AssignVariable(stmt.Buffer, isLet: false);
    }

    /// <summary>
    /// Compila un'istruzione UDP CLOSE.
    /// </summary>
    private void VisitUdpCloseStatement(UdpCloseStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.UDP_CLOSE, 1);
    }
    
    // ========================================================================
    // --- Compilazione Comandi di Rete Generici ---
    // ========================================================================

    /// <summary>
    /// Compila un'istruzione SHUTDOWN.
    /// </summary>
    private void VisitShutdownStatement(ShutdownStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Mode);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.SHUTDOWN, 2);
    }
    
    /// <summary>
    /// Compila un'istruzione SETOPTION.
    /// </summary>
    private void VisitSetOptionStatement(SetOptionStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Option);
        VisitExpression(stmt.Value);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.SETOPTION, 3);
    }
    
    /// <summary>
    /// Compila un'istruzione FETCH asincrona.
    /// </summary>
    /// <summary>
    /// Compila un'istruzione <c>FETCH url[, options] INTO Callback</c> (forma comando,
    /// sempre con callback — a differenza di <see cref="VisitFetchExpression"/>, dove la
    /// callback è opzionale). Stesso schema di push (Url → Options-o-nil → indirizzo Ip
    /// della callback, risolto da <see cref="Compiler._constantsToPatch"/> a fine
    /// compilazione), variant=1 (async con callback). OP_FETCH lascia sempre un valore
    /// sullo stack (qui un placeholder, dato che il vero risultato va alla SUB di
    /// callback, non al chiamante) — rimosso subito dopo per pulizia dello stack.
    /// </summary>
    private void VisitFetchStatement(HttpFetchStmt stmt)
    {
        VisitExpression(stmt.Url);

        if (stmt.Options is not null)
        {
            VisitExpression(stmt.Options);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        var callbackAddress = AddConstant(null); // Placeholder, patchato da PatchAllConstants.
        _constantsToPatch.Add((callbackAddress, stmt.CallbackName));
        EmitBytes((byte)OpCode.OP_PUSH_CONST);
        EmitShort(callbackAddress);

        EmitBytes((byte)OpCode.OP_FETCH, 1);
        EmitByte((byte)OpCode.OP_POP);
    }
    
    // ========================================================================
    // --- Compilazione Comandi HTTP (nuovi) ---
    // ========================================================================

    private void VisitHttpServerStatement(HttpServerStmt stmt)
    {
        if (stmt.IsStopCommand)
        {
            EmitConstant(true);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_SERVER, 5);
        }
        else
        {
            EmitConstant(false);
            if (stmt.Port != null) VisitExpression(stmt.Port);
            else EmitByte((byte)OpCode.OP_PUSH_NIL);

            if (stmt.Callback != null)
            {
                var callbackProcRef = CompileProcedureToCodeRef(stmt.Callback);
                EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
                EmitShort(callbackProcRef);
            }
            else
            {
                EmitByte((byte)OpCode.OP_PUSH_NIL);
            }

            if (stmt.CertificatePath is not null) VisitExpression(stmt.CertificatePath);
            else EmitByte((byte)OpCode.OP_PUSH_NIL);
            if (stmt.CertificatePassword is not null) VisitExpression(stmt.CertificatePassword);
            else EmitByte((byte)OpCode.OP_PUSH_NIL);

            EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_SERVER, 5);
        }
    }

    /// <summary>
    /// Compila un'istruzione HTTP ROUTE.
    /// </summary>
    private void VisitHttpRouteStatement(HttpRouteStmt stmt)
    {
        VisitExpression(stmt.Method); // Metodo HTTP (GET, POST, etc.)
        VisitExpression(stmt.Path);   // Path
        
        // Callback (stringa) - puntatore alla procedura
        var callbackProcRef = CompileProcedureToCodeRef(stmt.Callback);
        EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
        EmitShort(callbackProcRef);

        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_ROUTE, 3); // 3 argomenti
    }

    /// <summary>
    /// Compila un'istruzione HTTP USE (middleware).
    /// </summary>
    private void VisitHttpUseStatement(HttpUseStmt stmt)
    {
        if (stmt.Path is not null) VisitExpression(stmt.Path);
        else EmitByte((byte)OpCode.OP_PUSH_NIL); // Path opzionale
        
        // Callback (stringa) - puntatore alla procedura
        var callbackProcRef = CompileProcedureToCodeRef(stmt.Callback);
        EmitBytes((byte)OpCode.OP_PUSH_CODE_REF);
        EmitShort(callbackProcRef);

        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_USE, 2); // 2 argomenti
    }

    /// <summary>
    /// Compila un'istruzione HTTP USE NEXT.
    /// </summary>
    private void VisitHttpUseNextStatement(HttpUseNextStmt stmt)
    {
        VisitExpression(stmt.NextHandle); // Handle del middleware successivo
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_USE_NEXT, 1); // 1 argomento
    }

    /// <summary>
    /// Compila un'istruzione HTTP CONFIG.
    /// </summary>
    private void VisitHttpConfigStatement(HttpConfigStmt stmt)
    {
        VisitExpression(stmt.ConfigObject); // Oggetto di configurazione (HTTPOPTIONS)
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_CONFIG, 1); // 1 argomento
    }

    /// <summary>
    /// Compila un'istruzione HTTP STATIC. Prima di questo fix, Compiler.Statements.cs instradava
    /// HttpStaticStmt attraverso EmitExprAndPop (visita per riflessione delle proprieta' Expr,
    /// valutate e SCARTATE) — nessun opcode veniva mai emesso, quindi HTTP STATIC era un no-op
    /// silenzioso sia nell'AST-verso-P-Code sia nella VM. Vedi Compiler.Statements.cs::VisitStatement.
    /// </summary>
    private void VisitHttpStaticStatement(HttpStaticStmt stmt)
    {
        VisitExpression(stmt.UrlPath);
        VisitExpression(stmt.PhysicalPath);
        EmitBytes((byte)OpCode.OP_NET_CMD, (byte)NetSubOpCode.HTTP_STATIC, 2);
    }

    
}