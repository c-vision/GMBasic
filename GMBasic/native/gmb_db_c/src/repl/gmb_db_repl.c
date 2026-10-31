/* FILE: gmb_db_repl.c — append-only replica log (DBF/ISAM) */
#include "gmb_db_internal.h"

int gmb_db_repl_append(const char* log_path, const char* line)
{
    FILE* f;
    if (!log_path || !line) return GMB_DB_ERR;
    f = fopen(log_path, "ab");
    if (!f) return GMB_DB_ERR;
    fprintf(f, "%s\n", line);
    fclose(f);
    return GMB_DB_OK;
}

int gmb_db_repl_count(const char* log_path)
{
    FILE* f;
    char buf[1024];
    int n = 0;
    if (!log_path) return 0;
    f = fopen(log_path, "rb");
    if (!f) return 0;
    while (fgets(buf, sizeof(buf), f)) n++;
    fclose(f);
    return n;
}
