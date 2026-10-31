/* gmb_pkg_graph_selfcheck.c — albero dipendenze + peer mancanti. */
#include "gmb_pkg_graph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static void wr(const char* p, const char* s) { FILE* f = fopen(p, "w"); if (f) { fputs(s, f); fclose(f); } }

int main(void)
{
    system("rm -rf /tmp/gmb_g; mkdir -p /tmp/gmb_g/a /tmp/gmb_g/b");
    wr("/tmp/gmb_g/a/gmb-package.json",
       "{\"name\":\"a\",\"version\":\"1.0.0\",\"dependencies\":{\"b\":\"^1.0.0\"},\"peerDependencies\":{\"missing\":\"^2.0.0\"}}");
    wr("/tmp/gmb_g/b/gmb-package.json", "{\"name\":\"b\",\"version\":\"1.0.0\"}");

    char* names[2] = {(char*)"a", (char*)"b"};
    char* dirs[2] = {(char*)"/tmp/gmb_g/a", (char*)"/tmp/gmb_g/b"};
    gmb_pkg_tree_t* t = gmb_pkg_dep_tree(2, names, dirs);
    char p[64], ch[256];
    CHECK(gmb_pkg_tree_count(t) == 2);
    gmb_pkg_tree_row(t, 0, p, sizeof(p), ch, sizeof(ch));
    CHECK(strcmp(p, "a") == 0 && strcmp(ch, "b") == 0);
    gmb_pkg_tree_row(t, 1, p, sizeof(p), ch, sizeof(ch));
    CHECK(strcmp(p, "b") == 0 && ch[0] == 0);
    gmb_pkg_tree_free(t);

    char peers[512];
    CHECK(gmb_pkg_check_peers(2, names, dirs, peers, sizeof(peers)) == 1);
    CHECK(strstr(peers, "missing") != NULL);

    system("rm -rf /tmp/gmb_g");
    printf("gmb_pkg_graph_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
