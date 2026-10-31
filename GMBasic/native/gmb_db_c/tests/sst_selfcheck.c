#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = SstReader_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "SstReader_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("SstReader_SelfCheck OK");
    return 0;
}
