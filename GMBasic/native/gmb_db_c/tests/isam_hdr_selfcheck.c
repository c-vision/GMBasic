#include "gmb_db_port.h"
#include "gmb_db.h"
#include <dirent.h>
#include <unistd.h>

/* Rimozione ricorsiva best-effort (per la dir di backup incrementale tra run). */
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

#include <stdio.h>
#include <string.h>

/* JSON/LOB heap round-trip (port 1:1): campo JsonType con payload con virgole e doppi apici. */
static int IsamJsonLob_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/isam_json_selfcheck.isdb";
    char buf[256];
    int64_t h;
    remove(path);
    if (gmb_db_isam_create(path, "NAME:C:20,DATA:J") != GMB_DB_OK) return -2;
    h = gmb_db_isam_open(path);
    if (h <= 0) return -3;
    if (gmb_db_isam_insert(h, "ALICE,\"{\"\"k\"\":7,\"\"v\"\":\"\"x\"\"}\"") != GMB_DB_OK) {
        gmb_db_isam_close(h); return -4;
    }
    if (gmb_db_isam_insert(h, "BOB,\"{\"\"k\"\":42}\"") != GMB_DB_OK) {
        gmb_db_isam_close(h); return -5;
    }
    if (gmb_db_isam_go(h, 1) != GMB_DB_OK) { gmb_db_isam_close(h); return -6; }
    if (gmb_db_isam_read(h, 1, buf, (int)sizeof(buf)) != GMB_DB_OK) {
        gmb_db_isam_close(h); return -7;
    }
    if (strcmp(buf, "{\"k\":7,\"v\":\"x\"}") != 0) {
        fprintf(stderr, "JSON roundtrip mismatch: '%s'\n", buf);
        gmb_db_isam_close(h); return -8;
    }
    if (gmb_db_isam_go(h, 2) != GMB_DB_OK) { gmb_db_isam_close(h); return -9; }
    if (gmb_db_isam_read(h, 1, buf, (int)sizeof(buf)) != GMB_DB_OK) {
        gmb_db_isam_close(h); return -10;
    }
    if (strcmp(buf, "{\"k\":42}") != 0) {
        fprintf(stderr, "JSON roundtrip2 mismatch: '%s'\n", buf);
        gmb_db_isam_close(h); return -11;
    }
    gmb_db_isam_close(h);
    return GMB_DB_OK;
}

/* Legge la FIXTURE REALE items.isdb (samples/Fixtures) e verifica i valori attesi
 * (stessi del test C# RddRealFileDriversTests: Widget/Gadget). */
static int IsamRealFixture_SelfCheck(void)
{
    char buf[256];
    int64_t h = gmb_db_isam_open("samples/Fixtures/items.isdb");
    if (h <= 0) return -2;
    if (gmb_db_isam_go(h, 1) != GMB_DB_OK || gmb_db_isam_read(h, 1, buf, (int)sizeof(buf)) != GMB_DB_OK) {
        gmb_db_isam_close(h); return -3;
    }
    if (strstr(buf, "Widget") == NULL) {
        fprintf(stderr, "fixture items.isdb rec1 field1 mismatch: '%s'\n", buf);
        gmb_db_isam_close(h); return -4;
    }
    if (gmb_db_isam_go(h, 2) != GMB_DB_OK || gmb_db_isam_read(h, 1, buf, (int)sizeof(buf)) != GMB_DB_OK) {
        gmb_db_isam_close(h); return -5;
    }
    if (strstr(buf, "Gadget") == NULL) {
        fprintf(stderr, "fixture items.isdb rec2 field1 mismatch: '%s'\n", buf);
        gmb_db_isam_close(h); return -6;
    }
    gmb_db_isam_close(h);
    return GMB_DB_OK;
}

/* ISAM.INTEGRITY(): file valido -> valid:true; header free-list corrotto -> valid:false. */
/* CREATE INDEX fa crescere lo schema -> lo slot va espanso e i puntatori LOB JSON nei
 * record vanno riposizionati (port di FileHandle.EnsureSchemaSlot+RewriteLobPointersInRecords). */
static int IsamSchemaGrowth_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/isam_schema_growth.isdb";
    char buf[512], out[1024];
    int64_t h;
    remove(path);
    if (gmb_db_isam_create(path, "NAME:C:20,DATA:J") != GMB_DB_OK) return -2;
    h = gmb_db_isam_open(path);
    if (h <= 0) return -3;
    if (gmb_db_isam_insert(h, "ALICE,\"{\"\"k\"\":1}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -4; }
    if (gmb_db_isam_insert(h, "BOB,\"{\"\"k\"\":2}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -5; }
    if (gmb_db_isam_create_index(h, "BYNAME", "NAME", 20) != GMB_DB_OK) { gmb_db_isam_close(h); return -6; }
    if (gmb_db_isam_integrity(h, out, (int)sizeof(out)) != GMB_DB_OK) {
        fprintf(stderr, "SchemaGrowth integrity: %s\n", out);
        gmb_db_isam_close(h); return -7;
    }
    if (gmb_db_isam_go_top(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -8; }
    if (gmb_db_isam_read(h, 0, buf, (int)sizeof(buf)) != GMB_DB_OK || strstr(buf, "ALICE") == NULL) { gmb_db_isam_close(h); return -9; }
    gmb_db_isam_close(h);
    return GMB_DB_OK;
}

static int IsamAbiFeatures_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/isam_abi_features.isdb";
    char buf[512], out[1024];
    int64_t h, h2;
    int i, n;
    remove(path);
    remove("native/gmb_db_c/build/isam_abi_copy.isdb");
    remove("native/gmb_db_c/build/isam_abi_sort.isdb");
    if (gmb_db_isam_create(path, "NAME:C:20,DATA:J") != GMB_DB_OK) return -2;
    h = gmb_db_isam_open(path);
    if (h <= 0) return -3;
    if (gmb_db_isam_insert(h, "ALICE,\"{\"\"k\"\":1}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -4; }
    if (gmb_db_isam_insert(h, "BOB,\"{\"\"k\"\":2}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -5; }
    if (gmb_db_isam_insert(h, "CAROL,\"{\"\"k\"\":3}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -6; }
    if (gmb_db_isam_count(h) != 3) { gmb_db_isam_close(h); return -7; }

    /* go_top / go_bottom / bof / recno */
    if (gmb_db_isam_go_top(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -8; }
    if (gmb_db_isam_bof(h) != 0) { gmb_db_isam_close(h); return -9; }
    if (gmb_db_isam_recno(h) != 1) { gmb_db_isam_close(h); return -10; }
    if (gmb_db_isam_read(h, 0, buf, (int)sizeof(buf)) != GMB_DB_OK || strstr(buf, "ALICE") == NULL) { gmb_db_isam_close(h); return -11; }
    if (gmb_db_isam_go_bottom(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -12; }
    if (gmb_db_isam_read(h, 0, buf, (int)sizeof(buf)) != GMB_DB_OK || strstr(buf, "CAROL") == NULL) { gmb_db_isam_close(h); return -13; }

    /* put: aggiorna il record corrente (CAROL) */
    if (gmb_db_isam_put(h, "CAROL2,\"{\"\"k\"\":30}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -14; }
    if (gmb_db_isam_read(h, 0, buf, (int)sizeof(buf)) != GMB_DB_OK || strstr(buf, "CAROL2") == NULL) { gmb_db_isam_close(h); return -15; }

    /* lock / unlock (mentre il cursore e' su un record valido) */
    if (gmb_db_isam_lock(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -19; }
    if (gmb_db_isam_unlock(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -20; }

    /* validate + info (prima del delete: la coerenza conteggio tiene le entry Deleted
     * nell'indice, come in C# — ValidateIntegrity dopo un delete riporta mismatch) */
    if (gmb_db_isam_validate(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -29; }
    if (gmb_db_isam_info(h, out, (int)sizeof(out)) != GMB_DB_OK || strstr(out, "\"active_records\":3") == NULL) { gmb_db_isam_close(h); return -30; }

    /* delete (decrementa) + recall (no-op: il cursore e' reset come in C#) */
    if (gmb_db_isam_delete(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -16; }
    if (gmb_db_isam_count(h) != 2) { gmb_db_isam_close(h); return -18; }
    if (gmb_db_isam_recall(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -17; }
    if (gmb_db_isam_count(h) != 2) { gmb_db_isam_close(h); return -118; }

    /* appendmany: 2 righe (newline-separate) */
    if (gmb_db_isam_appendmany(h, "DAVE,\"{\"\"k\"\":4}\"\nEVE,\"{\"\"k\"\":5}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -21; }
    if (gmb_db_isam_count(h) != 4) { gmb_db_isam_close(h); return -22; }

    /* create_index + set_order + seek */
    if (gmb_db_isam_create_index(h, "BYNAME", "NAME", 20) != GMB_DB_OK) { gmb_db_isam_close(h); return -23; }
    if (gmb_db_isam_set_order(h, "BYNAME") != GMB_DB_OK) { gmb_db_isam_close(h); return -24; }
    if (gmb_db_isam_seek(h, "DAVE") != GMB_DB_OK) { gmb_db_isam_close(h); return -25; }
    if (gmb_db_isam_read(h, 0, buf, (int)sizeof(buf)) != GMB_DB_OK || strstr(buf, "DAVE") == NULL) { gmb_db_isam_close(h); return -26; }
    if (gmb_db_isam_drop_index(h, "BYNAME") != GMB_DB_OK) { gmb_db_isam_close(h); return -27; }
    if (gmb_db_isam_set_order(h, "") != GMB_DB_OK) { gmb_db_isam_close(h); return -28; }


    /* backup + restore su file nuovo */
    selfcheck_rmrf("native/gmb_db_c/build/isam_abi_bakdir");
    if (gmb_db_isam_backup(h, "native/gmb_db_c/build/isam_abi_bakdir") != 1) { gmb_db_isam_close(h); return -31; }
    if (gmb_db_isam_restore("native/gmb_db_c/build/isam_abi_bakdir", "native/gmb_db_c/build/isam_abi_restored.isdb") != GMB_DB_OK) { gmb_db_isam_close(h); return -32; }
    h2 = gmb_db_isam_open("native/gmb_db_c/build/isam_abi_restored.isdb");
    if (h2 <= 0 || gmb_db_isam_count(h2) != 4) { if (h2 > 0) gmb_db_isam_close(h2); gmb_db_isam_close(h); return -33; }
    gmb_db_isam_close(h2);

    /* copy_to / sort_to */
    if (gmb_db_isam_copy_to(h, "native/gmb_db_c/build/isam_abi_copy.isdb", "") != GMB_DB_OK) { gmb_db_isam_close(h); return -34; }
    if (gmb_db_isam_sort_to(h, "native/gmb_db_c/build/isam_abi_sort.isdb", "NAME", 0, "") != GMB_DB_OK) { gmb_db_isam_close(h); return -35; }
    h2 = gmb_db_isam_open("native/gmb_db_c/build/isam_abi_copy.isdb");
    if (h2 <= 0 || gmb_db_isam_count(h2) != 4) { if (h2 > 0) gmb_db_isam_close(h2); gmb_db_isam_close(h); return -36; }
    gmb_db_isam_close(h2);

    /* transazioni: rollback non persiste, commit si */
    if (gmb_db_isam_begin_transaction(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -37; }
    if (gmb_db_isam_insert(h, "FRANK,\"{\"\"k\"\":6}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -38; }
    if (gmb_db_isam_rollback(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -39; }
    if (gmb_db_isam_count(h) != 4) { gmb_db_isam_close(h); return -40; }
    if (gmb_db_isam_begin_transaction(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -41; }
    if (gmb_db_isam_insert(h, "GRACE,\"{\"\"k\"\":7}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -42; }
    if (gmb_db_isam_commit(h) != GMB_DB_OK) { gmb_db_isam_close(h); return -43; }
    if (gmb_db_isam_count(h) != 5) { gmb_db_isam_close(h); return -44; }

    gmb_db_isam_close(h);
    return GMB_DB_OK;
}

static int IsamIntegrity_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/isam_integrity_selfcheck.isdb";
    char out[1024];
    int64_t h;
    int rc;
    FILE* f;
    long long bad = 999999;
    remove(path);
    if (gmb_db_isam_create(path, "NAME:C:20,DATA:J") != GMB_DB_OK) return -2;
    h = gmb_db_isam_open(path);
    if (h <= 0) return -3;
    if (gmb_db_isam_insert(h, "ALICE,\"{\"\"k\"\":7}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -4; }
    if (gmb_db_isam_insert(h, "BOB,\"{\"\"k\"\":42}\"") != GMB_DB_OK) { gmb_db_isam_close(h); return -5; }
    rc = gmb_db_isam_integrity(h, out, (int)sizeof(out));
    if (rc != GMB_DB_OK) { fprintf(stderr, "integrity valido fallito: %d %s\n", rc, out); gmb_db_isam_close(h); return -6; }
    if (!strstr(out, "\"valid\":true")) { fprintf(stderr, "valid:true atteso: %s\n", out); gmb_db_isam_close(h); return -7; }
    gmb_db_isam_close(h);

    /* corrompi PageFreeListHeadOffset (byte 88, pack=1) -> fuori heap -> valid:false */
    f = fopen(path, "r+b");
    if (!f) return -8;
    if (fseek(f, 88, SEEK_SET) != 0 || fwrite(&bad, 1, sizeof(bad), f) != sizeof(bad)) { fclose(f); return -9; }
    fclose(f);
    h = gmb_db_isam_open(path);
    if (h <= 0) return -10;
    rc = gmb_db_isam_integrity(h, out, (int)sizeof(out));
    gmb_db_isam_close(h);
    if (rc != GMB_DB_ERR) { fprintf(stderr, "integrita' corrotta doveva fallire: rc=%d %s\n", rc, out); return -11; }
    if (!strstr(out, "\"valid\":false")) { fprintf(stderr, "valid:false atteso: %s\n", out); return -12; }
    return GMB_DB_OK;
}

int main(void)
{
    int rc = IsamFileHeader_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamFileHeader_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamFileHeader_SelfCheck OK");
    rc = IsamFileHandle_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamFileHandle_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamFileHandle_SelfCheck OK");
    rc = BPlusTreeManager_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "BPlusTreeManager_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("BPlusTreeManager_SelfCheck OK");
    rc = Engine_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "Engine_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("Engine_SelfCheck OK");
    rc = IsamQuery_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamQuery_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamQuery_SelfCheck OK");
    rc = IsamBackupRestore_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamBackupRestore_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamBackupRestore_SelfCheck OK");
    rc = IsamRepair_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamRepair_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamRepair_SelfCheck OK");
    rc = IsamExportImport_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamExportImport_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamExportImport_SelfCheck OK");
    rc = SnapshotManager_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "SnapshotManager_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("SnapshotManager_SelfCheck OK");
    rc = IsamThreadCursor_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamThreadCursor_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamThreadCursor_SelfCheck OK");
    rc = IsamExpressionBridge_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamExpressionBridge_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamExpressionBridge_SelfCheck OK");
    rc = IsamReplicationApplier_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamReplicationApplier_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamReplicationApplier_SelfCheck OK");
    rc = IsamMemStore_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamMemStore_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamMemStore_SelfCheck OK");
    rc = IsamOfflineMigrator_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamOfflineMigrator_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamOfflineMigrator_SelfCheck OK");
    rc = IsamDriver_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamDriver_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamDriver_SelfCheck OK");
    rc = Engine_CopySort_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "Engine_CopySort_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("Engine_CopySort_SelfCheck OK");
    rc = IsamOfflineToolkit_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamOfflineToolkit_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamOfflineToolkit_SelfCheck OK");
    rc = IsamJsonLob_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamJsonLob_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamJsonLob_SelfCheck OK");
    rc = IsamRealFixture_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamRealFixture_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamRealFixture_SelfCheck OK");
    rc = IsamIntegrity_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamIntegrity_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamIntegrity_SelfCheck OK");
    rc = IsamAbiFeatures_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamAbiFeatures_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamAbiFeatures_SelfCheck OK");
    rc = IsamSchemaGrowth_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IsamSchemaGrowth_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IsamSchemaGrowth_SelfCheck OK");
    return 0;
}
