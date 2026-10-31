#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
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
