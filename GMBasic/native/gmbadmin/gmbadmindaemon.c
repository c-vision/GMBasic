/* gmbadmindaemon.c — `gmb daemon …` in C (servizi BASIC come job launchd su macOS).
 *
 * Porting C di CliDaemon.cs. Il vecchio front C# lanciava `dotnet REPL.dll`, rimosso con
 * la migrazione: qui il job lancia la **REPL C** (`gmb_repl <file.bas>`), così il daemon non
 * dipende più dal mondo C#.
 *
 *   gmb daemon install <name> <file.bas> [--port N] [--no-start]
 *   gmb daemon start|stop|restart <name>
 *   gmb daemon status <name> | list
 *   gmb daemon logs <name> [--follow]
 *   gmb daemon uninstall <name> [--purge]
 *
 * Registry: ~/.gmbasic/daemons/<name>/ (manifest.json + logs/); launchd:
 * ~/Library/LaunchAgents/it.gmb.<name>.plist, label it.gmb.<name>.
 * Linux/Windows: errore esplicito (systemd/SCM non ancora portati).
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

/* ── percorsi ────────────────────────────────────────────────────────────── */

static void home_dir(char* out, size_t cap)
{
    const char* h = getenv("HOME");
    snprintf(out, cap, "%s", (h && *h) ? h : ".");
}

static void daemon_dir(const char* name, char* out, size_t cap)
{
    char h[1024];
    home_dir(h, sizeof h);
    snprintf(out, cap, "%s/.gmbasic/daemons/%s", h, name);
}

static void plist_path(const char* name, char* out, size_t cap)
{
    char h[1024];
    home_dir(h, sizeof h);
    snprintf(out, cap, "%s/Library/LaunchAgents/it.gmb.%s.plist", h, name);
}

static void label_of(const char* name, char* out, size_t cap)
{
    snprintf(out, cap, "it.gmb.%s", name);
}

static int uid_num(void)
{
    return (int)getuid();
}

/* Percorso della REPL C (`gmb_repl [<file.bas>]`): è il runner dei job launchd.
 * Ordine: PATH, poi i sibling del checkout (REPL/build/gmb_repl). */
static int find_gmb(char* out, size_t cap)
{
    const char* pe = getenv("PATH");
    if (pe) {
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", pe);
        for (char* d = strtok(tmp, ":"); d; d = strtok(NULL, ":")) {
            snprintf(out, cap, "%s/gmb_repl", d);
            if (access(out, X_OK) == 0) return 1;
        }
    }
    char exe[4096] = "";
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) exe[n] = 0;
    }
#endif
    if (exe[0]) {
        char dir[4096];
        snprintf(dir, sizeof dir, "%s", exe);
        char* sl = strrchr(dir, '/');
        if (sl) {
            *sl = 0;
            static const char* rel[] = {
                "../../../REPL/build/gmb_repl",
                "../../../../REPL/build/gmb_repl",
                "../../../../../REPL/build/gmb_repl",
                "../../REPL/build/gmb_repl",
                NULL
            };
            char cand[4600];
            for (int i = 0; rel[i]; i++) {
                snprintf(cand, sizeof cand, "%s/%s", dir, rel[i]);
                if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
            }
        }
    }
    return 0;
}

/* ── launchctl ───────────────────────────────────────────────────────────── */

/* Esegue un comando e cattura stdout (facoltativo). Ritorna exit code del figlio. */
static int run_capture(char* const argv[], char* out, size_t cap)
{
    int pipefd[2];
    if (out && cap && pipe(pipefd) == 0) {
        out[0] = 0;
    } else if (out) {
        out[0] = 0;
        pipefd[0] = pipefd[1] = -1;
    }
    pid_t pid = fork();
    if (pid < 0) { if (out && pipefd[0] >= 0) { close(pipefd[0]); close(pipefd[1]); } return -1; }
    if (pid == 0) {
        if (out && pipefd[0] >= 0) {
            dup2(pipefd[1], 1);
            int devnull = open("/dev/null", 1);
            if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
            close(pipefd[0]); close(pipefd[1]);
        } else {
            int devnull = open("/dev/null", 1);
            if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    if (out && pipefd[0] >= 0) {
        close(pipefd[1]);
        size_t off = 0;
        ssize_t r;
        while ((r = read(pipefd[0], out + off, cap - 1 - off)) > 0 && off + (size_t)r < cap - 1) off += (size_t)r;
        out[off] = 0;
        close(pipefd[0]);
    }
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int launchctl(char* const args[], char* out, size_t cap)
{
    char* av[16];
    int n = 0;
    av[n++] = "launchctl";
    for (int i = 0; args[i] && n < 15; i++) av[n++] = args[i];
    av[n] = NULL;
    return run_capture(av, out, cap);
}

/* PID del job launchd (0 = non in esecuzione). */
static int job_pid(const char* name)
{
    char label[512], dom[600], out[8192];
    label_of(name, label, sizeof label);
    snprintf(dom, sizeof dom, "gui/%d/%s", uid_num(), label);
    char* av[] = { "print", dom, NULL };
    if (launchctl(av, out, sizeof out) != 0) return 0;
    const char* p = out;
    while ((p = strstr(p, "pid = ")) != NULL) {
        p += 6;
        int v = atoi(p);
        if (v > 0) return v;
    }
    return 0;
}

/* ── scoperta dell'eseguibile gmb per il job ─────────────────────────────── */

static void json_escape(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = in; *p && o + 3 < cap; p++) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else if (*p == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

static void xml_escape(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = in; *p && o + 8 < cap; p++) {
        if (*p == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
        else if (*p == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
        else if (*p == '>') { memcpy(out + o, "&gt;", 4); o += 4; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

static int write_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t l = strlen(text);
    int ok = fwrite(text, 1, l, f) == l;
    fclose(f);
    return ok;
}

static char* read_text(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)n, f);
    b[rd] = 0;
    fclose(f);
    return b;
}

/* Basilico: file del daemon dal manifest (campo "file"). */
static int manifest_file(const char* name, char* out, size_t cap)
{
    char d[2048], mf[2400];
    daemon_dir(name, d, sizeof d);
    snprintf(mf, sizeof mf, "%s/manifest.json", d);
    char* js = read_text(mf);
    if (!js) return 0;
    const char* k = strstr(js, "\"file\": \"");
    if (!k) k = strstr(js, "\"file\":\"");
    if (k) {
        k = strchr(k + 7, '"');
        const char* e = k ? strchr(k + 1, '"') : NULL;
        if (k && e) {
            size_t l = (size_t)(e - k - 1);
            if (l >= cap) l = cap - 1;
            memcpy(out, k + 1, l);
            out[l] = 0;
            free(js);
            return 1;
        }
    }
    free(js);
    return 0;
}

/* ── comandi ─────────────────────────────────────────────────────────────── */

static int daemon_install(int argc, char** argv)
{
    const char* name = (argc > 3) ? argv[3] : NULL;
    const char* file = (argc > 4 && argv[4][0] != '-') ? argv[4] : NULL;
    if (!name || !file || argv[3][0] == '-') {
        fprintf(stderr, "?uso: gmb daemon install <name> <file.bas> [--port N] [--no-start]\n");
        return 2;
    }
    char bas[4096];
    if (realpath(file, bas) == NULL) {
        fprintf(stderr, "?daemon: file non trovato: %s\n", file);
        return 1;
    }
    if (access(bas, R_OK) != 0) { fprintf(stderr, "?daemon: file non leggibile: %s\n", bas); return 1; }

    int no_start = 0, port = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--no-start")) no_start = 1;
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strncmp(argv[i], "--port=", 7)) port = atoi(argv[i] + 7);
    }

    char gmb_raw[4096], gmb[4096];
    if (!find_gmb(gmb_raw, sizeof gmb_raw)) {
        fprintf(stderr, "?daemon install: REPL C `gmb_repl` non trovata (compila REPL/ o mettila nel PATH)\n");
        return 2;
    }
    if (realpath(gmb_raw, gmb) == NULL) snprintf(gmb, sizeof gmb, "%s", gmb_raw);

    char d[2048], logs[2400], pl[2400], label[512];
    daemon_dir(name, d, sizeof d);
    gmb_fs_mkdir_p(d);
    snprintf(logs, sizeof logs, "%s/logs", d);
    gmb_fs_mkdir_p(logs);
    plist_path(name, pl, sizeof pl);
    label_of(name, label, sizeof label);

    char log_out[2500], log_err[2500];
    snprintf(log_out, sizeof log_out, "%s/out.log", logs);
    snprintf(log_err, sizeof log_err, "%s/err.log", logs);

    char workdir[4096];
    snprintf(workdir, sizeof workdir, "%s", bas);
    char* sl = strrchr(workdir, '/');
    if (sl) *sl = 0;

    char created[64];
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    strftime(created, sizeof created, "%Y-%m-%dT%H:%M:%SZ", &tmv);

    char es_file[8192], es_work[8192];
    json_escape(bas, es_file, sizeof es_file);
    json_escape(workdir, es_work, sizeof es_work);
    char mf[2400], body[20000];
    snprintf(mf, sizeof mf, "%s/manifest.json", d);
    snprintf(body, sizeof body,
             "{\n"
             "  \"name\": \"%s\",\n"
             "  \"file\": \"%s\",\n"
             "  \"workdir\": \"%s\",\n"
             "  \"port\": %d,\n"
             "  \"plist\": \"%s\",\n"
             "  \"log_out\": \"%s\",\n"
             "  \"log_err\": \"%s\",\n"
             "  \"runner\": \"%s\",\n"
             "  \"created_at\": \"%s\"\n"
             "}\n",
             name, es_file, es_work, port, pl, log_out, log_err, gmb, created);
    if (!write_file(mf, body)) { fprintf(stderr, "?daemon: scrittura manifest fallita (%s)\n", mf); return 1; }

    char e_gmb[8192], e_bas[8192], e_work[8192], e_out[8192], e_err[8192];
    xml_escape(gmb, e_gmb, sizeof e_gmb);
    xml_escape(bas, e_bas, sizeof e_bas);
    xml_escape(workdir, e_work, sizeof e_work);
    xml_escape(log_out, e_out, sizeof e_out);
    xml_escape(log_err, e_err, sizeof e_err);
    char plist[24000];
    snprintf(plist, sizeof plist,
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n"
             "<dict>\n"
             "  <key>Label</key><string>%s</string>\n"
             "  <key>ProgramArguments</key>\n"
             "  <array>\n"
             "    <string>%s</string>\n"
             "    <string>%s</string>\n"
             "  </array>\n"
             "  <key>WorkingDirectory</key><string>%s</string>\n"
             "  <key>RunAtLoad</key><true/>\n"
             "  <key>StandardOutPath</key><string>%s</string>\n"
             "  <key>StandardErrorPath</key><string>%s</string>\n"
             "  <key>ProcessType</key><string>Background</string>\n"
             "</dict>\n"
             "</plist>\n",
             label, e_gmb, e_bas, e_work, e_out, e_err);
    char h[1024], agents[1200];
    home_dir(h, sizeof h);
    snprintf(agents, sizeof agents, "%s/Library/LaunchAgents", h);
    gmb_fs_mkdir_p(agents);
    if (!write_file(pl, plist)) { fprintf(stderr, "?daemon: scrittura plist fallita (%s)\n", pl); return 1; }

    char dom[600], domlabel[700];
    snprintf(dom, sizeof dom, "gui/%d", uid_num());
    snprintf(domlabel, sizeof domlabel, "gui/%d/%s", uid_num(), label);
    { char* av[] = { "bootout", domlabel, NULL }; launchctl(av, NULL, 0); }

    printf("[daemon] installato: %s (label %s)\n", name, label);
    printf("[daemon] manifest: %s\n", mf);
    printf("[daemon] plist: %s\n", pl);
    if (!no_start) {
        char* b[] = { "bootstrap", dom, pl, NULL };
        int rc = launchctl(b, NULL, 0);
        if (rc != 0) fprintf(stderr, "?daemon: launchctl bootstrap ha ritornato %d\n", rc);
        else {
            char* k[] = { "kickstart", "-k", domlabel, NULL };
            launchctl(k, NULL, 0);
            sleep(2);
        }
    }
    printf("[daemon] stato: %s\n", job_pid(name) > 0 ? "running" : "loaded");
    return 0;
}

static int daemon_start(const char* name, int restart)
{
    char pl[2400], label[512], dom[600], domlabel[700];
    plist_path(name, pl, sizeof pl);
    label_of(name, label, sizeof label);
    snprintf(dom, sizeof dom, "gui/%d", uid_num());
    snprintf(domlabel, sizeof domlabel, "gui/%d/%s", uid_num(), label);
    if (access(pl, R_OK) != 0) { fprintf(stderr, "[daemon] non installato: %s\n", name); return 1; }
    if (restart) { char* b[] = { "bootout", domlabel, NULL }; launchctl(b, NULL, 0); }
    { char* b[] = { "bootstrap", dom, pl, NULL }; launchctl(b, NULL, 0); }
    { char* k[] = { "kickstart", "-k", domlabel, NULL };
      if (launchctl(k, NULL, 0) != 0) { fprintf(stderr, "?daemon start: kickstart fallito (%s)\n", domlabel); return 1; } }
    sleep(2);
    printf("[daemon] %s: %s (pid %d)\n", restart ? "restart" : "start", name, job_pid(name));
    return 0;
}

static int daemon_stop(const char* name)
{
    char label[512], domlabel[700];
    label_of(name, label, sizeof label);
    snprintf(domlabel, sizeof domlabel, "gui/%d/%s", uid_num(), label);
    char* b[] = { "bootout", domlabel, NULL };
    int rc = launchctl(b, NULL, 0);
    printf("[daemon] stop %s: %s\n", name, rc == 0 ? "fermato" : "non era in esecuzione");
    return 0;
}

static int daemon_status(const char* name)
{
    char pl[2400], label[512];
    plist_path(name, pl, sizeof pl);
    label_of(name, label, sizeof label);
    int loaded = access(pl, R_OK) == 0;
    int pid = job_pid(name);
    printf("%-20s loaded=%d running=%d%s label=%s\n", name, loaded, pid > 0 ? 1 : 0,
           pid > 0 ? "" : "", label);
    return 0;
}

static int daemon_list(void)
{
    char h[1024], root[1200];
    home_dir(h, sizeof h);
    snprintf(root, sizeof root, "%s/.gmbasic/daemons", h);
    DIR* d = opendir(root);
    if (!d) { printf("(nessun daemon)\n"); return 0; }
    struct dirent* de;
    int n = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char p[1500];
        snprintf(p, sizeof p, "%s/%s", root, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        char pl[2400], label[512];
        plist_path(de->d_name, pl, sizeof pl);
        label_of(de->d_name, label, sizeof label);
        printf("%-20s loaded=%d running=%d\n", de->d_name, access(pl, R_OK) == 0, job_pid(de->d_name) > 0);
        n++;
    }
    closedir(d);
    if (!n) printf("(nessun daemon)\n");
    return 0;
}

static int daemon_logs(const char* name, int follow)
{
    char d[2048], out[2400], err[2400];
    daemon_dir(name, d, sizeof d);
    snprintf(out, sizeof out, "%s/logs/out.log", d);
    snprintf(err, sizeof err, "%s/logs/err.log", d);
    if (follow) {
        if (access(out, R_OK) != 0 || access(err, R_OK) != 0) {
            fprintf(stderr, "?daemon logs: log assenti per %s (%s)\n", name, d);
            return 1;
        }
        char* av[] = { "tail", "-f", out, err, NULL };
        execvp("tail", av);
        perror("?daemon logs: tail");
        return 1;
    }
    char* a = read_text(out);
    if (a) { printf("%s", a); free(a); }
    char* b = read_text(err);
    if (b) { fprintf(stderr, "%s", b); free(b); }
    if (!a && !b) { fprintf(stderr, "?daemon logs: nessun log per %s\n", name); return 1; }
    return 0;
}

static int daemon_uninstall(const char* name, int purge)
{
    char label[512], domlabel[700], pl[2400], d[2048];
    label_of(name, label, sizeof label);
    snprintf(domlabel, sizeof domlabel, "gui/%d/%s", uid_num(), label);
    { char* b[] = { "bootout", domlabel, NULL }; launchctl(b, NULL, 0); }
    plist_path(name, pl, sizeof pl);
    unlink(pl);
    if (purge) {
        daemon_dir(name, d, sizeof d);
        gmb_fs_rmtree(d);
    }
    printf("[daemon] rimosso: %s%s\n", name, purge ? " (purge)" : " (artefatti/log mantenuti)");
    return 0;
}

int gmbadmin_daemon(int argc, char** argv)
{
#if !defined(__APPLE__)
    (void)argc; (void)argv;
    fprintf(stderr, "?daemon: solo macOS (launchd) in C; systemd/Windows non ancora portati (F9c)\n");
    return 2;
#else
    const char* sub = (argc > 2) ? argv[2] : "help";
    if (!strcmp(sub, "install")) return daemon_install(argc, argv);
    if (!strcmp(sub, "uninstall") || !strcmp(sub, "delete")) {
        const char* name = (argc > 3) ? argv[3] : NULL;
        if (!name) { fprintf(stderr, "?uso: gmb daemon uninstall <name> [--purge]\n"); return 2; }
        int purge = 0;
        for (int i = 4; i < argc; i++) if (!strcmp(argv[i], "--purge")) purge = 1;
        return daemon_uninstall(name, purge);
    }
    if (!strcmp(sub, "start") || !strcmp(sub, "stop") || !strcmp(sub, "restart") ||
        !strcmp(sub, "status") || !strcmp(sub, "logs")) {
        const char* name = (argc > 3) ? argv[3] : NULL;
        if (!name) { fprintf(stderr, "?uso: gmb daemon %s <name>\n", sub); return 2; }
        if (!strcmp(sub, "start")) return daemon_start(name, 0);
        if (!strcmp(sub, "restart")) return daemon_start(name, 1);
        if (!strcmp(sub, "stop")) return daemon_stop(name);
        if (!strcmp(sub, "status")) return daemon_status(name);
        int follow = 0;
        for (int i = 4; i < argc; i++) if (!strcmp(argv[i], "--follow")) follow = 1;
        return daemon_logs(name, follow);
    }
    if (!strcmp(sub, "list") || !strcmp(sub, "help") || !strcmp(sub, "--help") || !strcmp(sub, "-h")) {
        if (!strcmp(sub, "list")) return daemon_list();
        printf("uso: gmb daemon <install|start|stop|restart|status|logs|list|uninstall> …\n"
               "  install <name> <file.bas> [--port N] [--no-start]  → job launchd (gmb_repl)\n"
               "  start|stop|restart|status <name> · logs <name> [--follow]\n"
               "  uninstall <name> [--purge] · list\n");
        return 0;
    }
    fprintf(stderr, "?uso: gmb daemon <install|start|stop|restart|status|logs|list|uninstall> …\n");
    return 2;
#endif
}
