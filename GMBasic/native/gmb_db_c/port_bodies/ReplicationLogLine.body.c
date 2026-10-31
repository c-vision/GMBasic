#include <stdio.h>
#include <ctype.h>
#include <strings.h>

/* C#: ReplicationLogLine — "OP\\trecNo[\\tjson]". JSON valori tutti stringa. */

static int rll_parse_op(const char* name, ReplicationOp* op)
{
    if (!name || !op) return 0;
    if (gmb_strcasecmp(name, "TXN_BEGIN") == 0) { *op = ReplicationOp_TxnBegin; return 1; }
    if (gmb_strcasecmp(name, "TXN_COMMIT") == 0) { *op = ReplicationOp_TxnCommit; return 1; }
    if (gmb_strcasecmp(name, "APPEND") == 0) { *op = ReplicationOp_Append; return 1; }
    if (gmb_strcasecmp(name, "PUT") == 0) { *op = ReplicationOp_Put; return 1; }
    if (gmb_strcasecmp(name, "DELETE") == 0) { *op = ReplicationOp_Delete; return 1; }
    if (gmb_strcasecmp(name, "RECALL") == 0) { *op = ReplicationOp_Recall; return 1; }
    if (gmb_strcasecmp(name, "PACK") == 0) { *op = ReplicationOp_Pack; return 1; }
    if (gmb_strcasecmp(name, "SCHEMA") == 0) { *op = ReplicationOp_Schema; return 1; }
    if (gmb_strcasecmp(name, "REINDEX") == 0) { *op = ReplicationOp_Reindex; return 1; }
    return 0;
}

static const char* rll_op_name(ReplicationOp op)
{
    switch (op) {
    case ReplicationOp_Append: return "APPEND";
    case ReplicationOp_Put: return "PUT";
    case ReplicationOp_Delete: return "DELETE";
    case ReplicationOp_Recall: return "RECALL";
    case ReplicationOp_TxnBegin: return "TXN_BEGIN";
    case ReplicationOp_TxnCommit: return "TXN_COMMIT";
    case ReplicationOp_Pack: return "PACK";
    case ReplicationOp_Schema: return "SCHEMA";
    case ReplicationOp_Reindex: return "REINDEX";
    default: return "APPEND";
    }
}

void ReplicationLogLine_FormatTxnMarker(ReplicationOp op, char* out, int cap)
{
    if (op == ReplicationOp_TxnBegin) snprintf(out, (size_t)cap, "TXN_BEGIN\t0");
    else snprintf(out, (size_t)cap, "TXN_COMMIT\t0");
}

void ReplicationLogLine_FormatPack(int64_t generation, char* out, int cap)
{
    snprintf(out, (size_t)cap, "PACK\t%lld", (long long)generation);
}

void ReplicationLogLine_FormatReindex(int64_t generation, char* out, int cap)
{
    snprintf(out, (size_t)cap, "REINDEX\t%lld", (long long)generation);
}

void ReplicationLogLine_Format(ReplicationOp op, int64_t recNo, const char* jsonValues, char* out, int cap)
{
    if (op == ReplicationOp_TxnBegin || op == ReplicationOp_TxnCommit) {
        ReplicationLogLine_FormatTxnMarker(op, out, cap);
        return;
    }
    if (!jsonValues || !jsonValues[0])
        snprintf(out, (size_t)cap, "%s\t%lld", rll_op_name(op), (long long)recNo);
    else
        snprintf(out, (size_t)cap, "%s\t%lld\t%s", rll_op_name(op), (long long)recNo, jsonValues);
}

int ReplicationLogLine_TryParse(const char* line, ReplicationOp* op, int64_t* recNo, char* jsonOut, int jsonCap)
{
    char name[32];
    const char* p;
    const char* tab2;
    long long n = 0;
    if (jsonOut && jsonCap > 0) jsonOut[0] = 0;
    if (!line || !op || !recNo) return 0;
    while (*line == ' ' || *line == '\t' || *line == '\n') line++;
    if (!*line) return 0;
    p = strchr(line, '\t');
    if (!p) return 0;
    {
        int nl = (int)(p - line);
        if (nl <= 0 || nl >= (int)sizeof(name)) return 0;
        memcpy(name, line, (size_t)nl);
        name[nl] = 0;
    }
    if (!rll_parse_op(name, op)) return 0;
    p++;
    tab2 = strchr(p, '\t');
    if (tab2) {
        char num[32];
        int nl = (int)(tab2 - p);
        if (nl <= 0 || nl >= (int)sizeof(num)) return 0;
        memcpy(num, p, (size_t)nl);
        num[nl] = 0;
        n = strtoll(num, NULL, 10);
        if (jsonOut && jsonCap > 0) snprintf(jsonOut, (size_t)jsonCap, "%s", tab2 + 1);
    } else {
        n = strtoll(p, NULL, 10);
    }
    *recNo = (int64_t)n;
    if (*recNo <= 0 && *op != ReplicationOp_TxnBegin && *op != ReplicationOp_TxnCommit && *op != ReplicationOp_Schema)
        return 0;
    return 1;
}
