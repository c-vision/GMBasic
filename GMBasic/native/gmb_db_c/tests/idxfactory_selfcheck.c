#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = IndexFactory_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "IndexFactory_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("IndexFactory_SelfCheck OK");
    return 0;
}
