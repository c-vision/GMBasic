#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = ClipperNtxFile_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "ClipperNtxFile_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("ClipperNtxFile_SelfCheck OK");
    return 0;
}
