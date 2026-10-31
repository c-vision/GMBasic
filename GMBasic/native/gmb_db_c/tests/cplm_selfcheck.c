#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = CrossProcessLockManager_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "CrossProcessLockManager_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("CrossProcessLockManager_SelfCheck OK");
    return 0;
}
