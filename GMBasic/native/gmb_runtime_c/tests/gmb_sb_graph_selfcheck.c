/* gmb_sb_graph_selfcheck.c — parity test per gmb_graph_brain_query (GRATH query sui brain).
 * Crea un WikiGraph JSON in <root>/graphs/testgraph.json (gmb_paths) e verifica ISVALID/STATS/
 * PATH/PAGERANK/BIPARTITE via il runtime C condiviso. Eseguito: cmake --build && ./gmb_sb_graph_selfcheck
 */
#include "gmb_runtime.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("ok: %s\n", msg); } } while (0)

static void ensure_graph_json(void) {
    char dir[1024], path[2048];
    gmb_paths_graphs_dir(dir, sizeof(dir));
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/testgraph.json", dir);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fputs("{\"nodes\":[{\"id\":\"a.md\",\"title\":\"A\"},{\"id\":\"b.md\",\"title\":\"B\"},"
          "{\"id\":\"c.md\",\"title\":\"C\"}],"
          "\"edges\":[{\"from\":\"a.md\",\"to\":\"b.md\"},{\"from\":\"b.md\",\"to\":\"c.md\"}],"
          "\"dangling\":[],\"folderCounts\":{}}", f);
    fclose(f);
    /* registra testgraph come vault nel db (per gmb_brain_registry_list) */
    char dbp[1024];
    gmb_paths_vaults_db(dbp, sizeof(dbp));
    sqlite3* db = NULL;
    if (sqlite3_open(dbp, &db) == SQLITE_OK) {
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS vaults (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, root_path TEXT NOT NULL, pattern TEXT NOT NULL);", NULL, NULL, NULL);
        char sql[512];
        snprintf(sql, sizeof(sql), "INSERT INTO vaults(name,root_path,pattern) VALUES('testgraph','%s','*.md') ON CONFLICT(name) DO UPDATE SET root_path='%s';", dir, dir);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
        sqlite3_close(db);
    }
}

int main(void) {
    ensure_graph_json();
    printf("== gmb_graph_brain_query selfcheck ==\n");
    fflush(stdout);

    printf("... ISVALID\n"); fflush(stdout);
    char* r = gmb_graph_brain_query("testgraph", "ISVALID", NULL, NULL, 0);
    CHECK(r && strcmp(r, "1") == 0, "ISVALID=1"); free(r);

    printf("... STATS\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "STATS", NULL, NULL, 0);
    CHECK(r && strstr(r, "\"nodes\":3"), "STATS nodes=3"); free(r);

    printf("... PATH\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "PATH", "a.md", "c.md", 0);
    printf("  PATH result=[%s]\n", r ? r : "null"); fflush(stdout);
    CHECK(r && strstr(r, "a.md") && strstr(r, "c.md"), "PATH a→c"); free(r);

    printf("... PAGERANK\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "PAGERANK", NULL, NULL, 3);
    CHECK(r && strstr(r, "rank"), "PAGERANK"); free(r);

    printf("... BIPARTITE\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "BIPARTITE", NULL, NULL, 0);
    CHECK(r && strcmp(r, "1") == 0, "BIPARTITE=1"); free(r);

    printf("... GHOST\n"); fflush(stdout);
    r = gmb_graph_brain_query("ghost", "ISVALID", NULL, NULL, 0);
    CHECK(r && strstr(r, "Error"), "brain ignoto → errore esplicito"); free(r);

    printf("... NEIGHBORS\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "NEIGHBORS", "a.md", NULL, 0);
    CHECK(r && strstr(r, "b.md"), "NEIGHBORS a → b"); free(r);

    printf("... CONNECTED\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "CONNECTED", NULL, NULL, 0);
    CHECK(r && strstr(r, "a.md"), "CONNECTED"); free(r);

    printf("... TOPOSORT\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "TOPOSORT", NULL, NULL, 0);
    CHECK(r && strstr(r, "a.md"), "TOPOSORT"); free(r);

    printf("... BETWEENNESS\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "BETWEENNESS", NULL, NULL, 0);
    CHECK(r && strstr(r, "value"), "BETWEENNESS"); free(r);

    printf("... CLUSTERING\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "CLUSTERING", "a.md", NULL, 0);
    CHECK(r && *r, "CLUSTERING"); free(r);

    printf("... DIAMETER\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "DIAMETER", NULL, NULL, 0);
    CHECK(r && *r, "DIAMETER"); free(r);

    printf("... DFS\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "DFS", "a.md", NULL, 0);
    CHECK(r && strstr(r, "a.md") && strstr(r, "c.md"), "DFS"); free(r);

    printf("... BFS\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "BFS", "a.md", NULL, 0);
    CHECK(r && strstr(r, "a.md") && strstr(r, "c.md"), "BFS"); free(r);

    printf("... DIJKSTRA\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "DIJKSTRA", "a.md", "c.md", 0);
    CHECK(r && strstr(r, "a.md") && strstr(r, "c.md"), "DIJKSTRA"); free(r);

    printf("... HUBS (grado: b.md al centro della catena a→b→c)\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "HUBS", "10", NULL, 10);
    printf("  HUBS result=[%s]\n", r ? r : "null"); fflush(stdout);
    CHECK(r && strstr(r, "\"hubs\"") && strstr(r, "b.md") && strstr(r, "\"degree\":2"),
          "HUBS b.md grado 2 (in+out)"); free(r);

    printf("... HASCYCLE (path lineare -> 0)\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "HASCYCLE", NULL, NULL, 0);
    CHECK(r && strcmp(r, "0") == 0, "HASCYCLE=0"); free(r);

    printf("... ECCENTRICITY\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "ECCENTRICITY", "a.md", NULL, 0);
    CHECK(r && *r && atof(r) == 2.0, "ECCENTRICITY(a)=2"); free(r);

    printf("... SCC\n"); fflush(stdout);
    r = gmb_graph_brain_query("testgraph", "SCC", NULL, NULL, 0);
    CHECK(r && *r, "SCC"); free(r);

    printf("... LLM_COMPLETE\n"); fflush(stdout);
    r = gmb_llm_complete("Sistema", "Domanda");
    CHECK(r && *r, "gmb_llm_complete"); free(r);

    printf("... BRAIN_LIST\n"); fflush(stdout);
    r = gmb_brain_registry_list();
    CHECK(r && strstr(r, "testgraph"), "gmb_brain_registry_list"); free(r);

    printf(failures == 0 ? "ALL PASS\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
