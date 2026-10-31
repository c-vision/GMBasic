/* FILE: native/gmb_db_c/src/Replication/ReplicationTcpReceiver.c */
/* Port 1:1 di modules/Database/Replication/ReplicationTcpReceiver.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationTcpReceiver
//
//using System.Net;
//using System.Net.Sockets;
//using System.Text;
//
//namespace Basic.Databases.Replication;
//
///// <summary>
///// Secondary-side TCP receiver: accepts one connection, applies replication lines via callback.
///// </summary>
//internal static class ReplicationTcpReceiver
//{
//    public static int ReceiveOnce(
//        int port,
//        Func<ReplicationApplyState, string, bool> applyLine,
//        string? checkpointPath = null,
//        TimeSpan? acceptTimeout = null)
//    {
//        ArgumentNullException.ThrowIfNull(applyLine);
//        if (port is <= 0 or > 65535)
//        {
//            throw new ArgumentOutOfRangeException(nameof(port), port, "Port must be 1-65535.");
//        }
//
//        acceptTimeout ??= TimeSpan.FromSeconds(30);
//        using var listener = new TcpListener(IPAddress.Loopback, port);
//        listener.Server.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
//        listener.Start();
//        try
//        {
//            using var acceptCts = new CancellationTokenSource(acceptTimeout.Value);
//            using var client = listener.AcceptTcpClientAsync(acceptCts.Token).GetAwaiter().GetResult();
//            using var stream = client.GetStream();
//            return ReceiveSession(stream, applyLine, checkpointPath);
//        }
//        finally
//        {
//            listener.Stop();
//        }
//    }
//
//    internal static int ReceiveSession(
//        Stream stream,
//        Func<ReplicationApplyState, string, bool> applyLine,
//        string? checkpointPath)
//    {
//        ArgumentNullException.ThrowIfNull(stream);
//        ArgumentNullException.ThrowIfNull(applyLine);
//
//        using var lineReader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
//        var helloLine = lineReader.ReadLine()
//                        ?? throw new InvalidDataException("Shipper closed before HELLO.");
//        if (!ReplicationTcpProtocol.TryParseHello(helloLine, out _))
//        {
//            throw new InvalidDataException($"Expected HELLO, got: {helloLine}");
//        }
//
//        checkpointPath ??= string.Empty;
//        var (byteOffset, state) = string.IsNullOrWhiteSpace(checkpointPath)
//            ? (0L, new ReplicationApplyState())
//            : ReplicationApplyState.LoadCheckpoint(checkpointPath);
//
//        ReplicationTcpProtocol.WriteLine(stream, $"{ReplicationTcpProtocol.Ack}\t{byteOffset}");
//
//        var applied = 0;
//        string? line;
//        while ((line = lineReader.ReadLine()) is not null)
//        {
//            if (ReplicationTcpProtocol.IsBye(line))
//            {
//                break;
//            }
//
//            if (ReplicationTcpProtocol.IsPing(line))
//            {
//                ReplicationTcpProtocol.WriteLine(stream, ReplicationTcpProtocol.Pong);
//                continue;
//            }
//
//            if (ReplicationTcpProtocol.IsControlLine(line))
//            {
//                continue;
//            }
//
//            if (string.IsNullOrWhiteSpace(line))
//            {
//                continue;
//            }
//
//            if (applyLine(state, line))
//            {
//                applied++;
//            }
//
//            byteOffset += ReplicationTcpProtocol.LineByteCount(line);
//            if (!string.IsNullOrWhiteSpace(checkpointPath))
//            {
//                state.SaveCheckpoint(checkpointPath, byteOffset);
//            }
//        }
//
//        return applied;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* C#: ReplicationTcpReceiver — un accept, sessione HELLO/ACK/linee/BYE. */

static int rtr_session(int fd, ReplicationApplyLineFn applyLine, void* ctx, const char* checkpointPath)
{
    ReplicationApplyState state;
    char line[2048], ack[64];
    int64_t dummy = 0, byteOffset = 0;
    int applied = 0;
    if (ReplicationTcpProtocol_ReadLine(fd, line, (int)sizeof(line)) != 0)
        return GMB_DB_ERR;
    if (!ReplicationTcpProtocol_TryParseHello(line, &dummy))
        return GMB_DB_ERR;
    if (checkpointPath && checkpointPath[0])
        byteOffset = ReplicationApplyState_LoadCheckpoint(&state, checkpointPath);
    else
        ReplicationApplyState_Init(&state);
    snprintf(ack, sizeof(ack), "ACK\t%lld", (long long)byteOffset);
    if (ReplicationTcpProtocol_WriteLine(fd, ack) != GMB_DB_OK) return GMB_DB_ERR;
    for (;;) {
        int rc = ReplicationTcpProtocol_ReadLine(fd, line, (int)sizeof(line));
        if (rc == 1) break;
        if (rc != 0) return GMB_DB_ERR;
        if (ReplicationTcpProtocol_IsBye(line)) break;
        if (ReplicationTcpProtocol_IsPing(line)) {
            if (ReplicationTcpProtocol_WriteLine(fd, "PONG") != GMB_DB_OK) return GMB_DB_ERR;
            continue;
        }
        if (ReplicationTcpProtocol_IsControlLine(line) || !line[0]) continue;
        rc = applyLine(&state, line, ctx);
        if (rc < 0) return GMB_DB_ERR;
        if (rc > 0) applied++;
        byteOffset += ReplicationTcpProtocol_LineByteCount(line);
        if (checkpointPath && checkpointPath[0])
            ReplicationApplyState_SaveCheckpoint(&state, checkpointPath, byteOffset);
    }
    return applied;
}

int ReplicationTcpReceiver_ReceiveOnce(int port, ReplicationApplyLineFn applyLine, void* ctx,
                                       const char* checkpointPath, int acceptTimeoutSec)
{
    int listenFd, clientFd, one = 1, rc;
    struct sockaddr_in addr;
    fd_set rfds;
    struct timeval tv;
    if (!applyLine) return GMB_DB_ARG;
    if (port <= 0 || port > 65535) return GMB_DB_ARG;
    if (acceptTimeoutSec <= 0) acceptTimeoutSec = 30;
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) return GMB_DB_ERR;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(listenFd, 1) != 0) {
        close(listenFd);
        return GMB_DB_ERR;
    }
    FD_ZERO(&rfds);
    FD_SET(listenFd, &rfds);
    tv.tv_sec = acceptTimeoutSec;
    tv.tv_usec = 0;
    rc = select(listenFd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        close(listenFd);
        return GMB_DB_TIMEOUT;
    }
    clientFd = accept(listenFd, NULL, NULL);
    close(listenFd);
    if (clientFd < 0) return GMB_DB_ERR;
    rc = rtr_session(clientFd, applyLine, ctx, checkpointPath);
    close(clientFd);
    return rc;
}

int ReplicationTcpReceiver_ReceiveSession(int fd, ReplicationApplyLineFn applyLine, void* ctx,
                                          const char* checkpointPath)
{
    if (fd < 0 || !applyLine) return GMB_DB_ARG;
    return rtr_session(fd, applyLine, ctx, checkpointPath);
}
