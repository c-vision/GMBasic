#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* C#: ReplicationTcpProtocol — righe UTF-8 + '\\n', no TLS. */

int ReplicationTcpProtocol_AllocateLoopbackPort(void)
{
    int fd, port = -1;
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    if (getsockname(fd, (struct sockaddr*)&addr, &alen) == 0)
        port = (int)ntohs(addr.sin_port);
    close(fd);
    return port;
}

int64_t ReplicationTcpProtocol_LineByteCount(const char* line)
{
    return line ? (int64_t)strlen(line) + 1 : 1;
}

int ReplicationTcpProtocol_WriteLine(int fd, const char* line)
{
    size_t n;
    ssize_t w;
    char nl = '\n';
    if (fd < 0 || !line) return GMB_DB_ARG;
    n = strlen(line);
    w = write(fd, line, n);
    if (w < 0 || (size_t)w != n) return GMB_DB_ERR;
    if (write(fd, &nl, 1) != 1) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int ReplicationTcpProtocol_ReadLine(int fd, char* out, int cap)
{
    int n = 0;
    if (fd < 0 || !out || cap <= 1) return GMB_DB_ARG;
    for (;;) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0) {
            out[n] = 0;
            return n == 0 ? 1 : 0; /* 1 = EOF */
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            return GMB_DB_ERR;
        }
        if (c == '\n') {
            if (n > 0 && out[n - 1] == '\r') n--;
            out[n] = 0;
            return 0;
        }
        if (n < cap - 1) out[n++] = c;
    }
}

static int rtp_parse_offset(const char* line, const char* prefix, int64_t* off)
{
    size_t pl;
    if (off) *off = 0;
    if (!line || !prefix) return 0;
    pl = strlen(prefix);
    if (strncmp(line, prefix, pl) != 0 || line[pl] != '\t') return 0;
    if (off) *off = (int64_t)atoll(line + pl + 1);
    return 1;
}

int ReplicationTcpProtocol_TryParseHello(const char* line, int64_t* byteOffset)
{
    return rtp_parse_offset(line, "HELLO", byteOffset);
}

int ReplicationTcpProtocol_TryParseAck(const char* line, int64_t* byteOffset)
{
    return rtp_parse_offset(line, "ACK", byteOffset);
}

int ReplicationTcpProtocol_IsPing(const char* line)
{
    return line && strcmp(line, "PING") == 0;
}

int ReplicationTcpProtocol_IsBye(const char* line)
{
    return line && strcmp(line, "BYE") == 0;
}

int ReplicationTcpProtocol_IsControlLine(const char* line)
{
    if (!line) return 0;
    if (strncmp(line, "HELLO\t", 6) == 0) return 1;
    if (strncmp(line, "ACK\t", 4) == 0) return 1;
    if (strcmp(line, "PING") == 0 || strcmp(line, "PONG") == 0 || strcmp(line, "BYE") == 0) return 1;
    return 0;
}
