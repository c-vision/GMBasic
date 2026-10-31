/* ABI gmb_db.h → DbfEngine. Run from repo root. */
#include "gmb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int main(void)
{
    char dir[] = "native/gmb_db_c/build/abisc_XXXXXX";
    char dbf[PATH_MAX], ntx[PATH_MAX], ndx[PATH_MAX];
    int64_t h;
    char buf[64];

    if (!mkdtemp(dir)) {
        fprintf(stderr, "mkdtemp\n");
        return 2;
    }
    snprintf(dbf, sizeof(dbf), "%s/t.dbf", dir);
    snprintf(ntx, sizeof(ntx), "%s/byname.ntx", dir);
    snprintf(ndx, sizeof(ndx), "%s/id.ndx", dir);

    if (gmb_db_version() != GMB_DB_VERSION) return 3;
    if (gmb_db_dbf_create(dbf, "NAME:C:10,AGE:N:4:0") != GMB_DB_OK) return 4;
    h = gmb_db_dbf_open(dbf);
    if (h <= 0) return 5;
    if (gmb_db_dbf_append(h, "BOB,21") != GMB_DB_OK) return 6;
    if (gmb_db_dbf_append(h, "ALICE,30") != GMB_DB_OK) return 7;
    if (gmb_db_dbf_gotop(h) != GMB_DB_OK || gmb_db_dbf_recno(h) != 1) return 8;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "BOB") != 0) return 9;
    if (gmb_db_dbf_skip(h, 1) != GMB_DB_OK) return 10;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "ALICE") != 0) return 11;
    if (gmb_db_dbf_delete(h) != GMB_DB_OK || !gmb_db_dbf_deleted(h)) return 12;
    if (gmb_db_dbf_pack(h) != GMB_DB_OK || gmb_db_dbf_lastrec(h) != 1) return 13;
    if (gmb_db_dbf_gotop(h) != GMB_DB_OK) return 14;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "BOB") != 0) return 15;
    if (gmb_db_dbf_append(h, "ALICE,30") != GMB_DB_OK) return 16;
    if (gmb_db_dbf_index_on(h, "NAME", ntx, 0) != GMB_DB_OK) return 17;
    if (gmb_db_dbf_seek(h, "ALICE") != GMB_DB_OK || !gmb_db_dbf_found(h)) return 18;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "ALICE") != 0) return 19;
    if (gmb_db_dbf_set_order(h, 1) != GMB_DB_OK) return 20;
    if (gmb_db_dbf_gotop(h) != GMB_DB_OK) return 21;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "ALICE") != 0) return 22;
    if (gmb_db_dbf_skip(h, 1) != GMB_DB_OK) return 23;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "BOB") != 0) return 24;
    if (gmb_db_dbf_index_on(h, "NAME", ndx, 0) != GMB_DB_OK) return 25;
    if (gmb_db_dbf_seek(h, "BOB") != GMB_DB_OK || !gmb_db_dbf_found(h)) return 26;
    if (gmb_db_dbf_replace_field(h, 0, "ZED") != GMB_DB_OK) return 28;
    if (gmb_db_dbf_get_field(h, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "ZED") != 0) return 29;
    if (gmb_db_dbf_seek(h, "ZED") != GMB_DB_OK || !gmb_db_dbf_found(h)) return 30;
    if (gmb_db_dbf_close(h) != GMB_DB_OK) return 27;

    unlink(ntx);
    unlink(ndx);
    unlink(dbf);
    {
        char fpt[PATH_MAX];
        snprintf(fpt, sizeof(fpt), "%s/t.fpt", dir);
        unlink(fpt);
    }
    rmdir(dir);

    {
        char idir[] = "native/gmb_db_c/build/abiisam_XXXXXX";
        char isam[PATH_MAX], idbf[PATH_MAX];
        int64_t ih;
        if (!mkdtemp(idir)) return 40;
        snprintf(isam, sizeof(isam), "%s/t.isdb", idir);
        snprintf(idbf, sizeof(idbf), "%s/imp.dbf", idir);
        if (gmb_db_isam_create(isam, "NAME:C:20,AGE:N:8:0") != GMB_DB_OK) return 41;
        ih = gmb_db_isam_open(isam);
        if (ih <= 0) return 42;
        if (gmb_db_isam_insert(ih, "ALICE,30") != GMB_DB_OK) return 43;
        if (gmb_db_isam_insert(ih, "BOB,21") != GMB_DB_OK) return 44;
        if (gmb_db_isam_count(ih) != 2) return 45;
        if (gmb_db_isam_seek(ih, "BOB") != GMB_DB_OK) return 46;
        if (gmb_db_isam_read(ih, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "BOB") != 0) return 47;
        if (gmb_db_isam_reindex(ih) != GMB_DB_OK) return 48;
        if (gmb_db_isam_close(ih) != GMB_DB_OK) return 49;
        ih = gmb_db_isam_open(isam);
        if (ih <= 0) return 50;
        if (gmb_db_isam_seek(ih, "ALICE") != GMB_DB_OK) return 51;
        if (gmb_db_isam_read(ih, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "ALICE") != 0) return 52;
        if (gmb_db_dbf_create(idbf, "NAME:C:20,AGE:N:8:0") != GMB_DB_OK) return 53;
        {
            int64_t dh = gmb_db_dbf_open(idbf);
            if (dh <= 0) return 54;
            if (gmb_db_dbf_append(dh, "CAROL,40") != GMB_DB_OK) return 55;
            if (gmb_db_dbf_close(dh) != GMB_DB_OK) return 56;
        }
        if (gmb_db_isam_import_dbf(ih, idbf) != GMB_DB_OK) return 57;
        if (gmb_db_isam_count(ih) != 3) return 58;
        if (gmb_db_isam_seek(ih, "CAROL") != GMB_DB_OK) return 59;
        if (gmb_db_isam_read(ih, 0, buf, sizeof(buf)) != GMB_DB_OK || strcmp(buf, "CAROL") != 0) return 60;
        if (gmb_db_isam_close(ih) != GMB_DB_OK) return 61;
        unlink(isam);
        unlink(idbf);
        {
            char fpt[PATH_MAX];
            snprintf(fpt, sizeof(fpt), "%s/imp.fpt", idir);
            unlink(fpt);
        }
        rmdir(idir);
    }

    {
        char sdir[] = "native/gmb_db_c/build/abispd_XXXXXX";
        int64_t sh;
        char out[64];
        int vlen;
        if (!mkdtemp(sdir)) return 70;
        sh = gmb_db_speed_open(sdir);
        if (sh <= 0) return 71;
        if (gmb_db_speed_set(sh, 1, "user", 4, "gaetano", 7) != GMB_DB_OK) return 72;
        vlen = (int)sizeof(out);
        if (gmb_db_speed_get(sh, 1, "user", 4, out, &vlen) != GMB_DB_OK || vlen != 7) return 73;
        if (memcmp(out, "gaetano", 7) != 0) return 74;
        if (gmb_db_speed_delete(sh, 1, "user", 4) != GMB_DB_OK) return 75;
        vlen = (int)sizeof(out);
        if (gmb_db_speed_get(sh, 1, "user", 4, out, &vlen) != GMB_DB_NOTFOUND) return 76;
        if (gmb_db_speed_compact(sh) != GMB_DB_OK) return 77;
        if (gmb_db_speed_close(sh) != GMB_DB_OK) return 78;
    }

    {
        char ldir[] = "native/gmb_db_c/build/abildg_XXXXXX";
        char journal[PATH_MAX];
        int64_t lh, id;
        char date[16], desc[64];
        FILE* jf;
        char jtxt[256];
        if (!mkdtemp(ldir)) return 80;
        snprintf(journal, sizeof(journal), "%s/book.dat", ldir);
        lh = gmb_db_ledger_open(journal);
        if (lh <= 0) return 81;
        id = gmb_db_ledger_add_csv(lh, "2026-08-20", "seed", "Cash,Equity", "100,-100");
        if (id != 1) return 82;
        if (gmb_db_ledger_count(lh) != 1) return 83;
        if (gmb_db_ledger_get(lh, 1, date, (int)sizeof(date), desc, (int)sizeof(desc)) != GMB_DB_OK) return 84;
        if (strcmp(date, "2026-08-20") != 0 || strcmp(desc, "seed") != 0) return 85;
        if (gmb_db_ledger_add_csv(lh, "2026-08-20", "bad", "Cash", "1") >= 0) return 86;
        if (gmb_db_ledger_close(lh) != GMB_DB_OK) return 87;
        jf = fopen(journal, "rb");
        if (!jf) return 88;
        jtxt[0] = 0;
        fread(jtxt, 1, sizeof(jtxt) - 1, jf);
        fclose(jf);
        if (!strstr(jtxt, "2026-08-20 seed")) return 89;
        lh = gmb_db_ledger_open(journal);
        if (lh <= 0) return 90;
        if (gmb_db_ledger_count(lh) != 1) return 91;
        if (gmb_db_ledger_close(lh) != GMB_DB_OK) return 92;
        unlink(journal);
        {
            char idx[PATH_MAX];
            snprintf(idx, sizeof(idx), "%s/book.idx", ldir);
            unlink(idx);
        }
        rmdir(ldir);
    }

    puts("gmb_db_abi_selfcheck OK");
    return 0;
}
