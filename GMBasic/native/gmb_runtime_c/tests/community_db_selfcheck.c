/* FILE: native/gmb_runtime_c/tests/community_db_selfcheck.c */
#include "gmb_community_db.h"
#include <stdio.h>

int main(void)
{
    int rc = gmb_community_db_selfcheck();
    if (rc != 0) {
        fprintf(stderr, "gmb_community_db_selfcheck failed: %d\n", rc);
        return 1;
    }
    puts("gmb_community_db_selfcheck OK");
    return 0;
}
