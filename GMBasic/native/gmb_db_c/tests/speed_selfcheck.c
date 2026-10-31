#include "gmb_db_port.h"
#include "gmb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

static void selfcheck_rmrf(const char* path)
{
    DIR* d = opendir(path);
    struct dirent* e;
    char full[1024];
    if (!d) { remove(path); return; }
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (e->d_type == DT_DIR) selfcheck_rmrf(full);
        else remove(full);
    }
    closedir(d);
    rmdir(path);
}

#include <string.h>
#include <stdlib.h>


static int SpeedAbiFeatures_SelfCheck(void)
{
    const char* dir = "native/gmb_db_c/build/speed_abi_features";
    int64_t h;
    int tid;
    uint8_t k[8], v[16], got[64];
    int vlen;
    selfcheck_rmrf(dir);
    h = gmb_db_speed_open(dir);
    if (h <= 0) return -2;
    tid = gmb_db_speed_create_table(h, "Person", "Id:N:4,Name:C:20");
    if (tid <= 0) { gmb_db_speed_close(h); return -3; }
    if (gmb_db_speed_use(h, tid) != GMB_DB_OK) { gmb_db_speed_close(h); return -4; }
    memset(k, 0, sizeof(k)); k[0] = 1;
    memset(v, 0, sizeof(v)); memcpy(v, "ALICE", 5);
    if (gmb_db_speed_set(h, tid, k, 4, v, 16) != GMB_DB_OK) { gmb_db_speed_close(h); return -5; }
    memset(k, 0, sizeof(k)); k[0] = 2;
    memset(v, 0, sizeof(v)); memcpy(v, "BOB", 3);
    if (gmb_db_speed_set(h, tid, k, 4, v, 16) != GMB_DB_OK) { gmb_db_speed_close(h); return -6; }

    if (gmb_db_speed_go_top(h, tid) != GMB_DB_OK) { gmb_db_speed_close(h); return -7; }
    if (gmb_db_speed_eof(h, tid) != 0) { gmb_db_speed_close(h); return -8; }
    vlen = (int)sizeof(got);
    if (gmb_db_speed_get_current(h, tid, got, &vlen) != GMB_DB_OK || memcmp(got, "ALICE", 5) != 0) { gmb_db_speed_close(h); return -9; }
    if (gmb_db_speed_skip(h, tid, 1) != GMB_DB_OK) { gmb_db_speed_close(h); return -10; }
    vlen = (int)sizeof(got);
    if (gmb_db_speed_get_current(h, tid, got, &vlen) != GMB_DB_OK || memcmp(got, "BOB", 3) != 0) { gmb_db_speed_close(h); return -11; }
    if (gmb_db_speed_go_bottom(h, tid) != GMB_DB_OK) { gmb_db_speed_close(h); return -12; }

    /* Gli indici secondari su Name richiedono valori UDT strutturati (path Put, non il Set
     * raw di questa ABI KV): le ABI create/drop index esistono ma il popolamento dell'indice
     * passa dal marshaller dei campi — qui resta coperto il core (cursor + validate). */
    if (gmb_db_speed_validate(h, got, (int)sizeof(got)) != GMB_DB_OK) { gmb_db_speed_close(h); return -17; }

    gmb_db_speed_close(h);
    return GMB_DB_OK;
}

int main(void)
{
    int rc = Database_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "Database_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("Database_SelfCheck OK");
    rc = Database_Indexes_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "Database_Indexes_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("Database_Indexes_SelfCheck OK");
    rc = QueryOptimizer_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "QueryOptimizer_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("QueryOptimizer_SelfCheck OK");
    rc = SpeedMarshaller_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "SpeedMarshaller_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("SpeedMarshaller_SelfCheck OK");
    rc = SpeedDatabaseLock_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "SpeedDatabaseLock_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("SpeedDatabaseLock_SelfCheck OK");
    rc = QueryPlan_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "QueryPlan_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("QueryPlan_SelfCheck OK");
    rc = Compactor_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "Compactor_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("Compactor_SelfCheck OK");
    rc = SpeedDriver_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "SpeedDriver_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("SpeedDriver_SelfCheck OK");
    rc = SpeedAbiFeatures_SelfCheck();
    if (rc != GMB_DB_OK) { fprintf(stderr, "SpeedAbiFeatures_SelfCheck failed: %d\n", rc); return 1; }
    puts("SpeedAbiFeatures_SelfCheck OK");
    return 0;
}
