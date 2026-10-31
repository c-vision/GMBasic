/* FILE: native/gmb_runtime_c/tests/rdd_db_selfcheck.c */
#include "gmb_rdd_db.h"
#include <stdio.h>

int main(void)
{
    int rc = gmb_rdd_db_selfcheck();
    if (rc != 0) {
        fprintf(stderr, "gmb_rdd_db_selfcheck failed: %d\n", rc);
        return 1;
    }
    puts("gmb_rdd_db_selfcheck OK");
    return 0;
}
