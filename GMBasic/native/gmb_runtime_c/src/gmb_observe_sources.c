/* gmb_observe_sources.c — A5 (asse A, fase 2): fonti OBSERVE DATABASE/PROCESS/BROWSER.
 *
 * SPECCHIO ESATTO del C# `Basic.AI.Agents.ObservationSources` (common/AI/Agents/ObservationSources.cs):
 * STESSO artefatto JSON (ordine di chiavi incluso), stessi path relativi deterministici, stessi
 * errori espliciti (-1 → il comando alza un errore catturabile a monte). Registra solo il
 * RIFERIMENTO del ContextObject (id/type/source/artifact/hash/size): mai il contenuto nel context.
 *
 * TEST sta in `gmb_observe_test.c`: quello usa il compilatore C e va tenuto in un'unità separata
 * perché il linker non lo tiri dentro a chi usa solo queste fonti (lezione del 2026-09-16 su
 * gmb_verify_require.c).
 */
#include "gmb_runtime.h"
#include "gmb_observation.h"
#include "gmb_community_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

/* Path degli artefatti: IDENTICI al C# (così AI.OBSERVATIONFILE$ è lo stesso testo su 4 gambe). */
#define GMD_OBS_DB_ARTIFACT     "work/ai_observe_database.json"
#define GMD_OBS_PROC_ARTIFACT   "work/ai_observe_process.json"
#define GMD_OBS_BROWSER_ARTIFACT "work/ai_observe_browser.png"

/* Scrive l'artefatto creando la cartella padre (deterministico, senza dipendenze). */
static int obs_write_artifact(const char* path, const char* content)
{
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path ? path : "");
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = 0; if (dir[0]) mkdir(dir, 0755); }
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    return 0;
}

/* ── AI OBSERVE DATABASE "<path>", {"query":"SELECT …"} ────────────────────────────────────────
 * Apre il database (alias interno "obs"), esegue la query e registra il riferimento con il numero
 * di righe. -1 se il DB non si apre o la query fallisce (errore esplicito a monte). */
double gmb_observe_database(const char* path, const char* query)
{
    if (!path || !*path) return -1;
    const char* sql = (query && *query) ? query : "SELECT name FROM sqlite_master";
    if (gmb_sql_open(path, "obs") != 0) return -1;   /* 0 = successo (contratto di gmb_sql_open) */
    int64_t rs = gmb_sql_query("obs", sql);
    if (rs == 0) { gmb_sql_close("obs"); return -1; }
    int rows = gmb_sql_row_count(rs);
    gmb_sql_close_rs(rs);
    gmb_sql_close("obs");
    char json[4096];
    snprintf(json, sizeof(json), "{\"path\":\"%s\",\"query\":\"%s\",\"rows\":%d}\n", path, sql, rows);
    if (obs_write_artifact(GMD_OBS_DB_ARTIFACT, json) != 0) return -1;
    return (double)gmb_obs_record_typed("database", path, GMD_OBS_DB_ARTIFACT);
}

/* ── AI OBSERVE PROCESS "<nome>" ─────────────────────────────────────────────────────────────
 * Verifica che un processo con quel nome esista (confronto sul BASENAME di `ps -Ao comm=`, stessa
 * semantica di Process.GetProcessesByName) e registra la query come riferimento. -1 se NON esiste:
 * un'osservazione su un processo inesistente non va inventata. */
static int obs_process_exists(const char* name)
{
    if (!name || !*name) return 0;
    FILE* p = popen("ps -Ao comm=", "r");
    if (!p) return 0;
    char line[1024];
    int found = 0;
    while (!found && fgets(line, sizeof(line), p))
    {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = 0;
        const char* base = strrchr(line, '/');
        base = base ? base + 1 : line;
        if (strcasecmp(base, name) == 0) found = 1;
    }
    pclose(p);
    return found;
}

double gmb_observe_process(const char* name)
{
    if (!name || !*name) return -1;
    if (!obs_process_exists(name)) return -1;
    char json[1024];
    snprintf(json, sizeof(json), "{\"query\":\"%s\"}\n", name);
    if (obs_write_artifact(GMD_OBS_PROC_ARTIFACT, json) != 0) return -1;
    return (double)gmb_obs_record_typed("process", name, GMD_OBS_PROC_ARTIFACT);
}

/* ── AI OBSERVE BROWSER "<url>" ──────────────────────────────────────────────────────────────
 * Naviga l'URL via BROWSER runtime (CDP) e registra lo screenshot come riferimento. -1 se il
 * browser/CDP non è disponibile o la cattura fallisce (errore esplicito a monte). */
gmb_string_t gmb_cua_browser_navigate(const char* url);   /* native/gmb_agent_runtime */
gmb_string_t gmb_cua_browser_screenshot(const char* path);

/* Probe TCP rapido sul port CDP (env GMBASIC_CDP_PORT, default 9222): senza browser attivo la
 * cattura fallirebbe DOPO un tentativo di connessione lungo → qui si fallisce SUBITO (l'errore
 * esplicito resta, ma in millisecondi: un comando non deve bloccare il run). */
static int obs_cdp_port_open(void)
{
    const char* env = getenv("GMBASIC_CDP_PORT");
    int port = (env && *env) ? atoi(env) : 9222;
    if (port <= 0) port = 9222;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rc = connect(fd, (struct sockaddr*)&a, sizeof(a));
    if (rc != 0 && errno != EINPROGRESS) { close(fd); return 0; }
    fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000;   /* 200 ms */
    int sel = select(fd + 1, NULL, &wf, NULL, &tv);
    int ok = 0;
    if (sel > 0) {
        int soerr = 0; socklen_t sl = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) == 0 && soerr == 0) ok = 1;
    }
    close(fd);
    return ok;
}

double gmb_observe_browser(const char* url)
{
    if (!url || !*url) return -1;
    if (!obs_cdp_port_open()) return -1;   /* nessun CDP in ascolto → errore esplicito IMMEDIATO */
    gmb_string_t nav = gmb_cua_browser_navigate(url);
    int nav_ok = nav.data && strncmp(nav.data, "error", 5) != 0;
    gmb_string_free(&nav);
    if (!nav_ok) return -1;
    gmb_string_t shot = gmb_cua_browser_screenshot(GMD_OBS_BROWSER_ARTIFACT);
    int shot_ok = shot.data && strncmp(shot.data, "error", 5) != 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s", shot.data ? shot.data : "");
    gmb_string_free(&shot);
    if (!shot_ok) return -1;
    return (double)gmb_obs_record_typed("browser", url, path);
}
