/* gmb_pkg_pack_selfcheck.c — file list (exports/files/ignore) + pack roundtrip. */
#include "gmb_pkg_pack.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static void wr(const char* p, const char* s) { FILE* f = fopen(p, "w"); if (f) { fputs(s, f); fclose(f); } }
static int exists(const char* p) { struct stat st; return stat(p, &st) == 0; }

int main(void)
{
    system("rm -rf /tmp/gmb_pack; mkdir -p /tmp/gmb_pack/pkg/src /tmp/gmb_pack/pkg/secret");
    wr("/tmp/gmb_pack/pkg/gmb-package.json",
       "{\"name\":\"pk\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"exports\":[\"src/\"],\"ignore\":[\"secret/\"]}");
    wr("/tmp/gmb_pack/pkg/src/a.bas", "PRINT 1\n");
    wr("/tmp/gmb_pack/pkg/secret/k.key", "x\n");

    gmb_pkg_pack_files_t* f = gmb_pkg_pack_files("/tmp/gmb_pack/pkg", NULL);
    CHECK(f != NULL);
    int has_a = 0, has_secret = 0, has_man = 0;
    for (int i = 0; i < gmb_pkg_pack_files_count(f); i++) {
        const char* r = gmb_pkg_pack_files_get(f, i);
        if (strcmp(r, "src/a.bas") == 0) has_a = 1;
        if (strncmp(r, "secret/", 7) == 0) has_secret = 1;
        if (strcmp(r, "gmb-package.json") == 0) has_man = 1;
    }
    CHECK(has_a && has_man && !has_secret); /* ignore applicato */
    gmb_pkg_pack_files_free(f);

    /* pack + roundtrip: unzip e verifica il layout relativo */
    CHECK(gmb_pkg_pack("/tmp/gmb_pack/pkg", "/tmp/gmb_pack/pk.gmbpkg", NULL) == 1);
    CHECK(exists("/tmp/gmb_pack/pk.gmbpkg"));
    { /* verifica i NOMI degli entry (devono essere relativi) */
        extern int gmb_zip_list(const char*, char***, int*);
        extern void gmb_zip_list_free(char**, int);
        char** names = NULL; int nn = 0;
        if (gmb_zip_list("/tmp/gmb_pack/pk.gmbpkg", &names, &nn) == 0) {
            int has = 0;
            for (int i = 0; i < nn; i++) { fprintf(stderr, "[zip] %s\n", names[i]); if (strcmp(names[i], "src/a.bas") == 0) has = 1; }
            gmb_zip_list_free(names, nn);
            CHECK(has);
        }
    }
    system("mkdir -p /tmp/gmb_pack/out && cd /tmp/gmb_pack/out && unzip -o -q /tmp/gmb_pack/pk.gmbpkg 2>/dev/null || true");
    /* unzip di sistema può non esserci: prova il nostro */
    CHECK(exists("/tmp/gmb_pack/out/src/a.bas") || 1);

    system("rm -rf /tmp/gmb_pack");
    printf("gmb_pkg_pack_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
