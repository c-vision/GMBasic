#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = NdxFile_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "NdxFile_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("NdxFile_SelfCheck OK");
    return 0;
}
