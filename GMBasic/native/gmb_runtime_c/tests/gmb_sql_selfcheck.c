/* FILE: native/gmb_runtime_c/tests/gmb_sql_selfcheck.c */
#include <stdio.h>

int gmb_sql_selfcheck(void);

int main(void)
{
    int rc = gmb_sql_selfcheck();
    if (rc != 0) {
        fprintf(stderr, "gmb_sql_selfcheck failed: %d\n", rc);
        return 1;
    }
    puts("gmb_sql_selfcheck OK");
    return 0;
}
