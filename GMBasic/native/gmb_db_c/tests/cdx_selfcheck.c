#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = CdxCompound_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "CdxCompound_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("CdxCompound_SelfCheck OK");
    rc = CdxPageCache_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "CdxPageCache_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("CdxPageCache_SelfCheck OK");
    return 0;
}
