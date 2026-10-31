#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = ReplicationTcp_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "ReplicationTcp_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("ReplicationTcp_SelfCheck OK");
    return 0;
}
