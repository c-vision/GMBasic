#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = WriteAheadLog_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "WriteAheadLog_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("WriteAheadLog_SelfCheck OK");
    return 0;
}
