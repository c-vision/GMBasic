/* FILE: native/gmb_db_c/src/Replication/ReplicationTcpShipper.c */
/* Port 1:1 di modules/Database/Replication/ReplicationTcpShipper.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationTcpShipper
//
//using System.Net.Sockets;
//using System.Text;
//
//namespace Basic.Databases.Replication;
//
///// <summary>
///// Primary-side TCP shipper: tails a <c>.repl</c> log and streams new lines to a receiver.
///// </summary>
//internal static class ReplicationTcpShipper
//{
//    public static int ShipOnce(
//        string replLogPath,
//        string host,
//        int port,
//        string? shipCheckpointPath = null,
//        TimeSpan? connectTimeout = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentException.ThrowIfNullOrWhiteSpace(host);
//        if (port is <= 0 or > 65535)
//        {
//            throw new ArgumentOutOfRangeException(nameof(port), port, "Port must be 1-65535.");
//        }
//
//        if (!File.Exists(replLogPath))
//        {
//            throw new FileNotFoundException("Replication log not found.", replLogPath);
//        }
//
//        shipCheckpointPath ??= ReplicationShipState.DefaultCheckpointPath(replLogPath);
//        connectTimeout ??= TimeSpan.FromSeconds(10);
//
//        using var client = ConnectWithRetry(host, port, connectTimeout.Value);
//        using var stream = client.GetStream();
//        using var ackReader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
//        var shipOffset = ReplicationShipState.LoadOffset(shipCheckpointPath);
//        ReplicationTcpProtocol.WriteLine(stream, $"{ReplicationTcpProtocol.Hello}\t{shipOffset}");
//
//        var ackLine = ackReader.ReadLine()
//                      ?? throw new InvalidDataException("Receiver closed before ACK.");
//        if (!ReplicationTcpProtocol.TryParseAck(ackLine, out var receiverOffset))
//        {
//            throw new InvalidDataException($"Expected ACK, got: {ackLine}");
//        }
//
//        var startOffset = ReplicationLogEncoding.AdjustFileOffset(replLogPath, receiverOffset);
//        var shipped = 0;
//        var byteOffset = startOffset;
//        using var fs = new FileStream(replLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
//        fs.Seek(startOffset, SeekOrigin.Begin);
//        using var logReader = new StreamReader(fs, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
//        string? line;
//        while ((line = logReader.ReadLine()) is not null)
//        {
//            if (string.IsNullOrWhiteSpace(line))
//            {
//                continue;
//            }
//
//            ReplicationTcpProtocol.WriteLine(stream, line);
//            shipped++;
//            byteOffset += ReplicationTcpProtocol.LineByteCount(line);
//            ReplicationShipState.SaveOffset(shipCheckpointPath, byteOffset);
//        }
//
//        ReplicationTcpProtocol.WriteLine(stream, ReplicationTcpProtocol.Bye);
//        stream.Flush();
//        return shipped;
//    }
//
//    // ponytail: tests start receiver via Task.Run — brief retry avoids connection refused races.
//    private static TcpClient ConnectWithRetry(string host, int port, TimeSpan timeout)
//    {
//        var deadline = DateTime.UtcNow + timeout;
//        Exception? lastError = null;
//        while (DateTime.UtcNow < deadline)
//        {
//            var client = new TcpClient();
//            try
//            {
//                var remaining = deadline - DateTime.UtcNow;
//                var slice = remaining < TimeSpan.FromMilliseconds(200) ? remaining : TimeSpan.FromMilliseconds(200);
//                if (!client.ConnectAsync(host, port).Wait(slice))
//                {
//                    client.Dispose();
//                    Thread.Sleep(25);
//                    continue;
//                }
//
//                return client;
//            }
//            catch (Exception ex) when (IsTransientConnectFailure(ex))
//            {
//                lastError = ex;
//                client.Dispose();
//                Thread.Sleep(25);
//            }
//        }
//
//        throw new TimeoutException($"Timed out connecting to {host}:{port}.", lastError);
//    }
//
//    private static bool IsTransientConnectFailure(Exception ex) =>
//        ex switch
//        {
//            SocketException { SocketErrorCode: SocketError.ConnectionRefused } => true,
//            AggregateException { InnerException: SocketException { SocketErrorCode: SocketError.ConnectionRefused } } => true,
//            _ => false,
//        };
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* C#: ReplicationTcpShipper.ShipOnce — HELLO, ACK, linee log, BYE. */

static int rts_connect_retry(const char* host, int port, int timeoutSec)
{
    time_t deadline = time(NULL) + timeoutSec;
    struct sockaddr_in addr;
    struct hostent* he;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_aton(host, &addr.sin_addr) == 0) {
        he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) return -1;
        memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    }
    while (time(NULL) < deadline) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
            return fd;
        close(fd);
        if (errno != ECONNREFUSED && errno != EINTR && errno != ETIMEDOUT)
            return -1;
        usleep(25000);
    }
    return -1;
}

int ReplicationTcpShipper_ShipOnce(const char* replLogPath, const char* host, int port,
                                   const char* shipCheckpointPath)
{
    char ckpt[1024], hello[64], ack[128], line[2048];
    int64_t shipOffset, receiverOffset, startOffset, byteOffset;
    int fd, shipped = 0;
    FILE* logf;
    if (!replLogPath || !host || !*host) return GMB_DB_ARG;
    if (port <= 0 || port > 65535) return GMB_DB_ARG;
    if (access(replLogPath, R_OK) != 0) return GMB_DB_ERR;
    if (!shipCheckpointPath || !shipCheckpointPath[0]) {
        ReplicationShipState_DefaultCheckpointPath(replLogPath, ckpt, (int)sizeof(ckpt));
        shipCheckpointPath = ckpt;
    }
    fd = rts_connect_retry(host, port, 10);
    if (fd < 0) return GMB_DB_TIMEOUT;
    shipOffset = ReplicationShipState_LoadOffset(shipCheckpointPath);
    snprintf(hello, sizeof(hello), "HELLO\t%lld", (long long)shipOffset);
    if (ReplicationTcpProtocol_WriteLine(fd, hello) != GMB_DB_OK) {
        close(fd);
        return GMB_DB_ERR;
    }
    if (ReplicationTcpProtocol_ReadLine(fd, ack, (int)sizeof(ack)) != 0 ||
        !ReplicationTcpProtocol_TryParseAck(ack, &receiverOffset)) {
        close(fd);
        return GMB_DB_ERR;
    }
    startOffset = ReplicationLogEncoding_AdjustFileOffset(replLogPath, receiverOffset);
    byteOffset = startOffset;
    logf = fopen(replLogPath, "rb");
    if (!logf) {
        close(fd);
        return GMB_DB_ERR;
    }
    if (fseek(logf, (long)startOffset, SEEK_SET) != 0) {
        fclose(logf);
        close(fd);
        return GMB_DB_ERR;
    }
    while (fgets(line, (int)sizeof(line), logf)) {
        int n = (int)strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        if (ReplicationTcpProtocol_WriteLine(fd, line) != GMB_DB_OK) {
            fclose(logf);
            close(fd);
            return GMB_DB_ERR;
        }
        shipped++;
        byteOffset += ReplicationTcpProtocol_LineByteCount(line);
        ReplicationShipState_SaveOffset(shipCheckpointPath, byteOffset);
    }
    fclose(logf);
    ReplicationTcpProtocol_WriteLine(fd, "BYE");
    close(fd);
    return shipped;
}

typedef struct {
    int port;
    int applied;
    int rc;
    char lines[256];
} RtsRecvJob;

static int rts_sc_apply(ReplicationApplyState* state, const char* line, void* ctx)
{
    RtsRecvJob* job = (RtsRecvJob*)ctx;
    size_t used, add;
    (void)state;
    if (!job || !line) return GMB_DB_ERR;
    used = strlen(job->lines);
    add = strlen(line);
    if (used + add + 2 < sizeof(job->lines)) {
        memcpy(job->lines + used, line, add);
        job->lines[used + add] = '|';
        job->lines[used + add + 1] = 0;
    }
    job->applied++;
    return 1;
}

static void* rts_sc_recv_th(void* arg)
{
    RtsRecvJob* job = (RtsRecvJob*)arg;
    job->rc = ReplicationTcpReceiver_ReceiveOnce(job->port, rts_sc_apply, job, NULL, 8);
    return NULL;
}

typedef struct {
    int fd;
    int ackOk;
    char pong[16];
} RtsPingJob;

static int rts_sc_apply_none(ReplicationApplyState* state, const char* line, void* ctx)
{
    (void)state;
    (void)line;
    (void)ctx;
    return GMB_DB_ERR; /* PING/BYE must not reach apply */
}

static void* rts_sc_ping_th(void* arg)
{
    RtsPingJob* job = (RtsPingJob*)arg;
    char ack[64];
    int64_t off = 0;
    job->pong[0] = 0;
    job->ackOk = 0;
    if (ReplicationTcpProtocol_WriteLine(job->fd, "HELLO\t0") != GMB_DB_OK)
        return NULL;
    if (ReplicationTcpProtocol_ReadLine(job->fd, ack, (int)sizeof(ack)) != 0)
        return NULL;
    if (!ReplicationTcpProtocol_TryParseAck(ack, &off))
        return NULL;
    job->ackOk = 1;
    if (ReplicationTcpProtocol_WriteLine(job->fd, "PING") != GMB_DB_OK)
        return NULL;
    if (ReplicationTcpProtocol_ReadLine(job->fd, job->pong, (int)sizeof(job->pong)) != 0)
        return NULL;
    ReplicationTcpProtocol_WriteLine(job->fd, "BYE");
    return NULL;
}

int ReplicationTcp_SelfCheck(void)
{
    int64_t off = 0;
    int port, rc, shipped;
    pthread_t th;
    char dir[] = "/tmp/gmb_repl_tcp_XXXXXX";
    char logPath[192], shipCkpt[208], applyCkpt[208];
    FILE* f;
    RtsRecvJob job;
    RtsPingJob ping;
    ReplicationApplyState st;
    int sv[2];

    if (!ReplicationTcpProtocol_TryParseHello("HELLO\t12", &off) || off != 12) return -2;
    if (ReplicationTcpProtocol_TryParseHello("ACK\t1", &off)) return -3;
    if (!ReplicationTcpProtocol_TryParseAck("ACK\t7", &off) || off != 7) return -4;
    if (!ReplicationTcpProtocol_IsPing("PING") || ReplicationTcpProtocol_IsPing("PONG")) return -5;
    if (!ReplicationTcpProtocol_IsBye("BYE") || ReplicationTcpProtocol_IsBye("PING")) return -6;
    if (!ReplicationTcpProtocol_IsControlLine("HELLO\t0") ||
        !ReplicationTcpProtocol_IsControlLine("ACK\t0") ||
        !ReplicationTcpProtocol_IsControlLine("PONG") ||
        ReplicationTcpProtocol_IsControlLine("APPEND\t1")) return -7;
    if (ReplicationTcpProtocol_LineByteCount("ab") != 3) return -8;
    if (ReplicationTcpProtocol_AllocateLoopbackPort() <= 0) return -9;

    if (ReplicationTcpShipper_ShipOnce(NULL, "127.0.0.1", 1, NULL) != GMB_DB_ARG) return -10;
    if (ReplicationTcpShipper_ShipOnce("/no/such.repl", "127.0.0.1", 9, NULL) != GMB_DB_ERR) return -11;
    if (ReplicationTcpReceiver_ReceiveOnce(0, rts_sc_apply, NULL, NULL, 1) != GMB_DB_ARG) return -12;

    if (!mkdtemp(dir)) return -13;
    snprintf(logPath, sizeof(logPath), "%s/log.repl", dir);
    snprintf(shipCkpt, sizeof(shipCkpt), "%s/log.repl.ship.ckpt", dir);
    snprintf(applyCkpt, sizeof(applyCkpt), "%s/apply.ckpt", dir);
    f = fopen(logPath, "wb");
    if (!f) { rmdir(dir); return -14; }
    fputs("APPEND\t1\n\nPUT\t1\n", f);
    fclose(f);

    ReplicationApplyState_Init(&st);
    ReplicationApplyState_SetRecMap(&st, 1, 99);
    ReplicationApplyState_SaveCheckpoint(&st, applyCkpt, 42);
    ReplicationApplyState_Init(&st);
    if (ReplicationApplyState_LoadCheckpoint(&st, applyCkpt) != 42) {
        unlink(logPath); unlink(applyCkpt); rmdir(dir); return -15;
    }
    if (ReplicationApplyState_ResolveRecNo(&st, 1) != 99) {
        unlink(logPath); unlink(applyCkpt); rmdir(dir); return -16;
    }

    ReplicationShipState_SaveOffset(shipCkpt, 0);
    if (ReplicationShipState_LoadOffset(shipCkpt) != 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -17;
    }

    port = ReplicationTcpProtocol_AllocateLoopbackPort();
    if (port <= 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -18;
    }
    memset(&job, 0, sizeof(job));
    job.port = port;
    job.rc = GMB_DB_ERR;
    if (pthread_create(&th, NULL, rts_sc_recv_th, &job) != 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -19;
    }
    usleep(80000);
    shipped = ReplicationTcpShipper_ShipOnce(logPath, "127.0.0.1", port, shipCkpt);
    pthread_join(th, NULL);
    if (shipped != 2) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -20;
    }
    if (job.rc != 2 || job.applied != 2) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -21;
    }
    if (strcmp(job.lines, "APPEND\t1|PUT\t1|") != 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -22;
    }
    if (ReplicationShipState_LoadOffset(shipCkpt) <= 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -23;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -24;
    }
    memset(&ping, 0, sizeof(ping));
    ping.fd = sv[1];
    if (pthread_create(&th, NULL, rts_sc_ping_th, &ping) != 0) {
        close(sv[0]); close(sv[1]);
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -25;
    }
    rc = ReplicationTcpReceiver_ReceiveSession(sv[0], rts_sc_apply_none, NULL, NULL);
    pthread_join(th, NULL);
    close(sv[0]);
    close(sv[1]);
    if (rc != 0 || !ping.ackOk || strcmp(ping.pong, "PONG") != 0) {
        unlink(logPath); unlink(applyCkpt); unlink(shipCkpt); rmdir(dir); return -26;
    }

    unlink(logPath);
    unlink(applyCkpt);
    unlink(shipCkpt);
    rmdir(dir);
    return GMB_DB_OK;
}
