/* gmb_pkg_contentaddr_selfcheck.c — dedup (blob) + GC. */
#include "gmb_pkg_contentaddr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static void wr(const char* path, const char* s) { FILE* f = fopen(path, "w"); if (f) { fputs(s, f); fclose(f); } }

static int count_files(const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) n += count_files(full);
        else n++;
    }
    closedir(d);
    return n;
}

int main(void)
{
    system("rm -rf /tmp/gmb_ca; mkdir -p /tmp/gmb_ca/cache/p1/1.0.0 /tmp/gmb_ca/cache/p2/1.0.0");
    const char* content = "/tmp/gmb_ca/content";
    wr("/tmp/gmb_ca/cache/p1/1.0.0/f.txt", "hello");
    wr("/tmp/gmb_ca/cache/p2/1.0.0/f.txt", "hello");

    char f1[] = "/tmp/gmb_ca/cache/p1/1.0.0/f.txt", f2[] = "/tmp/gmb_ca/cache/p2/1.0.0/f.txt", sha1[65], sha2[65];
    CHECK(gmb_pkg_content_intern(content, f1, sha1, sizeof(sha1)) == 1);
    CHECK(gmb_pkg_content_intern(content, f2, sha2, sizeof(sha2)) == 1);
    CHECK(strcmp(sha1, sha2) == 0);
    CHECK(count_files(content) == 1); /* un solo blob per contenuto identico */

    char b[64];
    FILE* g = fopen(f1, "r"); CHECK(g && fgets(b, sizeof(b), g)); if (g) fclose(g);
    CHECK(strcmp(b, "hello") == 0); /* link trasparente */
    g = fopen(f2, "r"); CHECK(g && fgets(b, sizeof(b), g)); if (g) fclose(g);
    CHECK(strcmp(b, "hello") == 0);

    system("rm -rf /tmp/gmb_ca/cache/p2");
    CHECK(gmb_pkg_content_gc(content, "/tmp/gmb_ca/cache") == 0); /* ancora referenziato da p1 */
    system("rm -rf /tmp/gmb_ca/cache/p1");
    CHECK(gmb_pkg_content_gc(content, "/tmp/gmb_ca/cache") == 1); /* ora non referenziato */

    system("rm -rf /tmp/gmb_ca");
    printf("gmb_pkg_contentaddr_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
