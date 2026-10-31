
#include "gmb_db_port.h"
#include "gmb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ABI DBF estesa (Fase 3): append_blank/append_from/copy_to/sort_to/copy_structure/zap/
 * set_filter/set_deleted/set_exact/transazioni/gruppi. */
static int DbfAbiFeatures_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/dbf_abi_features.dbf";
    int64_t h;
    remove(path);
    if (gmb_db_dbf_create(path, "ID:N:4,NAME:C:10") != GMB_DB_OK) return -2;
    h = gmb_db_dbf_open(path);
    if (h <= 0) return -3;
    if (gmb_db_dbf_append(h, "1,ALICE") != GMB_DB_OK) { gmb_db_dbf_close(h); return -4; }
    if (gmb_db_dbf_append(h, "2,BOB") != GMB_DB_OK) { gmb_db_dbf_close(h); return -5; }
    if (gmb_db_dbf_append(h, "3,CAROL") != GMB_DB_OK) { gmb_db_dbf_close(h); return -6; }
    if (gmb_db_dbf_lastrec(h) != 3) { gmb_db_dbf_close(h); return -7; }

    if (gmb_db_dbf_append_blank(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -8; }
    if (gmb_db_dbf_lastrec(h) != 4) { gmb_db_dbf_close(h); return -9; }

    if (gmb_db_dbf_copy_to(h, "native/gmb_db_c/build/dbf_abi_copy.dbf") != GMB_DB_OK) { gmb_db_dbf_close(h); return -10; }
    if (gmb_db_dbf_copy_structure(h, "native/gmb_db_c/build/dbf_abi_struct.dbf") != GMB_DB_OK) { gmb_db_dbf_close(h); return -11; }
    if (gmb_db_dbf_sort_to(h, "native/gmb_db_c/build/dbf_abi_sort.dbf", "NAME", 0) != GMB_DB_OK) { gmb_db_dbf_close(h); return -12; }

    if (gmb_db_dbf_set_deleted(h, 1) != GMB_DB_OK) { gmb_db_dbf_close(h); return -13; }
    if (gmb_db_dbf_set_exact(h, 1) != GMB_DB_OK) { gmb_db_dbf_close(h); return -14; }
    if (gmb_db_dbf_set_filter(h, "") != GMB_DB_OK) { gmb_db_dbf_close(h); return -15; }
    if (gmb_db_dbf_set_filter(h, NULL) != GMB_DB_OK) { gmb_db_dbf_close(h); return -16; }

    /* transazioni: rollback non persiste, commit si */
    if (gmb_db_dbf_begin_transaction(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -17; }
    if (gmb_db_dbf_append(h, "5,DANA") != GMB_DB_OK) { gmb_db_dbf_close(h); return -18; }
    if (gmb_db_dbf_rollback(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -19; }
    if (gmb_db_dbf_lastrec(h) != 4) { gmb_db_dbf_close(h); return -20; }
    if (gmb_db_dbf_begin_transaction(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -21; }
    if (gmb_db_dbf_append(h, "6,ERIN") != GMB_DB_OK) { gmb_db_dbf_close(h); return -22; }
    if (gmb_db_dbf_commit(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -23; }
    if (gmb_db_dbf_lastrec(h) != 5) { gmb_db_dbf_close(h); return -24; }

    if (gmb_db_dbf_begin_group_commit(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -25; }
    if (gmb_db_dbf_append(h, "7,FRED") != GMB_DB_OK) { gmb_db_dbf_close(h); return -26; }
    if (gmb_db_dbf_end_group_commit(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -27; }
    if (gmb_db_dbf_lastrec(h) != 6) { gmb_db_dbf_close(h); return -28; }

    if (gmb_db_dbf_zap(h) != GMB_DB_OK) { gmb_db_dbf_close(h); return -29; }
    if (gmb_db_dbf_lastrec(h) != 0) { gmb_db_dbf_close(h); return -30; }

    gmb_db_dbf_close(h);
    return GMB_DB_OK;
}

int main(void)
{
    int rc = DbfEngine_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "DbfEngine_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("DbfEngine_SelfCheck OK");
    rc = DbfAbiFeatures_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "DbfAbiFeatures_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("DbfAbiFeatures_SelfCheck OK");
    return 0;
}
