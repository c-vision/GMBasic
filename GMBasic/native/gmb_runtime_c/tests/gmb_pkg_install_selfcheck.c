/* gmb_pkg_install_selfcheck.c — copia esempi (root da CWD) + build publisher/client. */
#include "gmb_pkg_install.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* I campioni copiati devono essere FILE REGOLARI: `access(..., F_OK)` è vero anche per una
 * CARTELLA, e la vecchia `copy_tree` creava proprio cartelle coi nomi dei file
 * (`avif_demo.bas/`, `sample.ppm/`) — il selfcheck passava lo stesso (scoperto 2026-10-29). */
static int is_regular(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static void wr(const char* p, const char* s) { FILE* f = fopen(p, "w"); if (f) { fputs(s, f); fclose(f); } }

int main(void)
{
    system("rm -rf /tmp/gmb_inst; mkdir -p /tmp/gmb_inst/proj/samples /tmp/gmb_inst/pkg/samples/demo");
    wr("/tmp/gmb_inst/pkg/gmb-package.json",
       "{\"name\":\"x\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"samples\":[\"samples\"]}");
    wr("/tmp/gmb_inst/pkg/samples/demo/d.bas", "PRINT 1\n");
    wr("/tmp/gmb_inst/proj/GMBASIC.sln", "x\n");   /* marcatore di progetto vero */

    /* (1) project_root ESPLICITA: mai dedotta dalla CWD (bug: la CWD dentro un pacchetto con
       una propria samples/ veniva scambiata per la root). */
    CHECK(gmb_pkg_copy_samples("/tmp/gmb_inst/pkg", "/tmp/gmb_inst/proj") == 1);
    CHECK(is_regular("/tmp/gmb_inst/proj/samples/packages/x@1.0.0/demo/d.bas"));

    /* (2) fallback (root NULL): risale dalla CWD fino a GMBASIC.sln. */
    system("rm -rf /tmp/gmb_inst/proj/samples/packages");
    CHECK(chdir("/tmp/gmb_inst/proj/samples") == 0);
    CHECK(gmb_pkg_copy_samples("/tmp/gmb_inst/pkg", NULL) == 1);
    CHECK(is_regular("/tmp/gmb_inst/proj/samples/packages/x@1.0.0/demo/d.bas"));

    /* build publisher con `command` */
    wr("/tmp/gmb_inst/pkg/gmb-package.json",
       "{\"name\":\"x\",\"version\":\"1.0.0\",\"contentType\":\"c-ffi-source\","
       "\"build\":{\"system\":\"command\",\"command\":\"mkdir -p lib && echo hi > lib/out.txt\",\"outputs\":[\"lib/out.txt\"]}}");
    CHECK(gmb_pkg_run_build("/tmp/gmb_inst/pkg", "osx-arm64") == 1);
    CHECK(access("/tmp/gmb_inst/pkg/lib/out.txt", F_OK) == 0);

    /* ensure_build: rimuovi l'output → ricompila (cc presente) e verifica */
    remove("/tmp/gmb_inst/pkg/lib/out.txt");
    CHECK(gmb_pkg_ensure_build("/tmp/gmb_inst/pkg", "osx-arm64") == 1);
    CHECK(access("/tmp/gmb_inst/pkg/lib/out.txt", F_OK) == 0);

    /* (3) `source.headers` → <pkg>/include/ (contratto §3.4: «estrae in dest + header in
       include/»). L'asset è estratto in lib/<platform>/ e può avere una directory di primo
       livello: il path dichiarato va cercato anche lì. Prima gli header NON venivano copiati. */
    wr("/tmp/gmb_inst/pkg/gmb-package.json",
       "{\"name\":\"x\",\"version\":\"1.0.0\",\"contentType\":\"c-ffi\","
       "\"source\":{\"type\":\"github-release\",\"repo\":\"o/r\",\"dest\":\"lib/{platform}\","
       "\"headers\":[\"duckdb.h\",\"nested/extra.h\",\"assente.h\"]}}");
    system("mkdir -p /tmp/gmb_inst/extract/duckdb-pkg /tmp/gmb_inst/extract/duckdb-pkg/nested");
    wr("/tmp/gmb_inst/extract/duckdb-pkg/duckdb.h", "#define DUCKDB_H 1\n");
    wr("/tmp/gmb_inst/extract/duckdb-pkg/nested/extra.h", "#define EXTRA_H 1\n");
    CHECK(gmb_pkg_copy_source_headers("/tmp/gmb_inst/pkg", "/tmp/gmb_inst/extract") == 2);
    CHECK(access("/tmp/gmb_inst/pkg/include/duckdb.h", F_OK) == 0);
    CHECK(access("/tmp/gmb_inst/pkg/include/extra.h", F_OK) == 0);
    CHECK(access("/tmp/gmb_inst/pkg/include/assente.h", F_OK) != 0);   /* dichiarato ma assente */

    system("rm -rf /tmp/gmb_inst");
    printf("gmb_pkg_install_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
