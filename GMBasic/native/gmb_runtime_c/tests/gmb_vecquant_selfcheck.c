/* FILE: native/gmb_runtime_c/tests/gmb_vecquant_selfcheck.c */
#include "gmb_vecquant.h"
#include <stdio.h>

int main(void)
{
    int rc = gmb_vecquant_selfcheck();
    if (rc != 0) {
        fprintf(stderr, "gmb_vecquant_selfcheck failed: %d\n", rc);
        return 1;
    }
    puts("gmb_vecquant_selfcheck OK");
    return 0;
}
