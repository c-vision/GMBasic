#include "gmb_db_port.h"
#include "gmb_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ABI LEDGER estesa (Fase 3): accounts/balance/register (output JSON). */
static int LedgerAbiFeatures_SelfCheck(void)
{
    const char* jpath = "native/gmb_db_c/build/ledger_abi_features.ledger";
    int64_t h;
    char out[2048];
    remove(jpath);
    { char idx[1024]; snprintf(idx, sizeof(idx), "%s", jpath);
      char* ext = strrchr(idx, '.'); if (ext) strcpy(ext, ".idx"); remove(idx); }
    h = gmb_db_ledger_open(jpath);
    if (h <= 0) return -2;
    if (gmb_db_ledger_add_csv(h, "2026-01-01", "Apertura", "Cassa,Banca", "100,-100") < 0) { gmb_db_ledger_close(h); return -3; }
    if (gmb_db_ledger_add_csv(h, "2026-01-02", "Acquisto", "Cassa,Merce", "-30,30") < 0) { gmb_db_ledger_close(h); return -4; }
    if (gmb_db_ledger_count(h) != 2) { gmb_db_ledger_close(h); return -5; }

    if (gmb_db_ledger_accounts(h, NULL, out, (int)sizeof(out)) != GMB_DB_OK || strstr(out, "Cassa") == NULL || strstr(out, "Banca") == NULL) {
        fprintf(stderr, "accounts: %s\n", out); gmb_db_ledger_close(h); return -6;
    }
    if (gmb_db_ledger_balance(h, "", out, (int)sizeof(out)) != GMB_DB_OK || strstr(out, "Cassa") == NULL) {
        fprintf(stderr, "balance: %s\n", out); gmb_db_ledger_close(h); return -7;
    }
    if (gmb_db_ledger_register(h, "", out, (int)sizeof(out)) != GMB_DB_OK || strstr(out, "TransactionId") == NULL) {
        fprintf(stderr, "register: %s\n", out); gmb_db_ledger_close(h); return -8;
    }

    gmb_db_ledger_close(h);
    return GMB_DB_OK;
}

int main(void)
{
    int rc = LedgerManager_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerManager_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerManager_SelfCheck OK");
    rc = LedgerManager_Query_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerManager_Query_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerManager_Query_SelfCheck OK");
    rc = LedgerManager_Budget_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerManager_Budget_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerManager_Budget_SelfCheck OK");
    rc = LedgerManager_Csv_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerManager_Csv_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerManager_Csv_SelfCheck OK");
    rc = LedgerManager_Audit_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerManager_Audit_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerManager_Audit_SelfCheck OK");
    rc = LedgerAbiFeatures_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "LedgerAbiFeatures_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("LedgerAbiFeatures_SelfCheck OK");
    return 0;
}
