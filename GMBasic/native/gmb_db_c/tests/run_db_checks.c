/* FILE: run_db_checks.c — compile-and-run gate for gmb_db_c */
#include "gmb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void fail(const char* m)
{
    fprintf(stderr, "FAIL: %s\n", m);
    exit(1);
}

int main(void)
{
    char dbf[256], ntx[256];
    int64_t h;
    char buf[64];
    snprintf(dbf, sizeof(dbf), "/tmp/gmb_db_runner_%d.dbf", (int)getpid());
    snprintf(ntx, sizeof(ntx), "/tmp/gmb_db_runner_%d.ntx", (int)getpid());
    if (gmb_db_version() != GMB_DB_VERSION) fail("version");
    if (gmb_db_dbf_create(dbf, "NAME:C:10,AGE:N:4:0") != 0) fail("create");
    h = gmb_db_dbf_open(dbf);
    if (h <= 0) fail("open");
    if (gmb_db_dbf_append(h, "ALICE,30") != 0) fail("append");
    if (gmb_db_dbf_gotop(h) != 0) fail("gotop");
    if (gmb_db_dbf_recno(h) != 1) fail("recno");
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != 0) fail("get");
    if (strcmp(buf, "ALICE") != 0) fail("alice");
    if (gmb_db_dbf_index_on(h, "NAME", ntx, 0) != 0) fail("index");
    if (gmb_db_dbf_seek(h, "ALICE") != 0) fail("seek");
    gmb_db_dbf_close(h);
    printf("OK: gmb_db runner\n");
    return 0;
}
