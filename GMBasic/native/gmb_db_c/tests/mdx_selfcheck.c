#include "gmb_db_port.h"
#include <stdio.h>

int main(void)
{
    int rc = MdxCompound_SelfCheck();
    if (rc != GMB_DB_OK) {
        fprintf(stderr, "MdxCompound_SelfCheck failed: %d\n", rc);
        return 1;
    }
    puts("MdxCompound_SelfCheck OK");
    return 0;
}
