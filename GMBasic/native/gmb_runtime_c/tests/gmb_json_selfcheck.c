/* gmb_json_selfcheck.c — stabilita' del parser JSON C (gmb_dict.c), su un manifest realistico.
 * Copre: oggetto, stringhe (con escape), array, oggetti annidati, mappa annidata, numeri. */
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static const char* s(gmb_val_t v) { return v.kind == GMB_TYPE_STRING ? v.v_string.data : "(non-stringa)"; }

int main(void)
{
    const char* js =
        "{"
        "\"name\":\"duckdb\",\"version\":\"1.5.6\",\"contentType\":\"c-ffi\","
        "\"backends\":[\"il\",\"c99\"],"
        "\"ffi\":{\"api\":\"c-abi\",\"headers\":[\"include/duckdb.h\"]},"
        "\"dependencies\":{\"gmb-sqlite3\":\"^1.0.0\",\"gmb-z\":\"*\"},"
        "\"pi\":3.5,\"n\":42,\"esc\":\"a\\tb\\n\""
        "}";

    gmb_val_t v = gmb_json_parse(js);
    CHECK(v.kind == GMB_TYPE_OBJECT);

    CHECK(strcmp(s(gmb_val_dict_get(v, "name")), "duckdb") == 0);
    CHECK(strcmp(s(gmb_val_dict_get(v, "version")), "1.5.6") == 0);
    CHECK(strcmp(s(gmb_val_dict_get(v, "contentType")), "c-ffi") == 0);

    gmb_list_t be = gmb_val_as_list(gmb_val_dict_get(v, "backends"));
    CHECK(gmb_list_count(be) == 2);
    CHECK(strcmp(s(gmb_list_get(be, 0)), "il") == 0);
    CHECK(strcmp(s(gmb_list_get(be, 1)), "c99") == 0);

    gmb_val_t ffi = gmb_val_dict_get(v, "ffi");
    CHECK(strcmp(s(gmb_val_dict_get(ffi, "api")), "c-abi") == 0);
    gmb_list_t hd = gmb_val_as_list(gmb_val_dict_get(ffi, "headers"));
    CHECK(gmb_list_count(hd) == 1);
    CHECK(strcmp(s(gmb_list_get(hd, 0)), "include/duckdb.h") == 0);

    gmb_val_t deps = gmb_val_dict_get(v, "dependencies");
    CHECK(strcmp(s(gmb_val_dict_get(deps, "gmb-sqlite3")), "^1.0.0") == 0);
    CHECK(strcmp(s(gmb_val_dict_get(deps, "gmb-z")), "*") == 0);

    gmb_val_t n = gmb_val_dict_get(v, "n");
    CHECK(n.kind == GMB_TYPE_DOUBLE && n.v_double == 42.0);
    gmb_val_t pi = gmb_val_dict_get(v, "pi");
    CHECK(pi.kind == GMB_TYPE_DOUBLE && pi.v_double == 3.5);

    CHECK(strcmp(s(gmb_val_dict_get(v, "esc")), "a\tb\n") == 0); /* escape \t \n */

    printf("gmb_json_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
