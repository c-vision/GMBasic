#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = NtxFile_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "NtxFile_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("NtxFile_SelfCheck OK");
    return 0;
}
