/* gmbadminbrain.c — `gmb brain …` completo in C (registry + manifest + toolchain).
 *
 * Ogni operazione chiama il runtime C (`gmb_brain_tool_*`, `gmb_brain_reg_*`): nessun
 * comando del Second Brain vive più nel mondo C#.
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_brain_tools.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char* bopt(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) {
        if (!strcmp(argv[i], name) && i + 1 < argc) return argv[i + 1];
        size_t n = strlen(name);
        if (!strncmp(argv[i], name, n) && argv[i][n] == '=') return argv[i] + n + 1;
    }
    return NULL;
}

static int bflag(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

static const char* bpos(int argc, char** argv, int start, int n)
{
    int seen = 0;
    for (int i = start; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strchr(argv[i], '=') && i + 1 < argc && argv[i + 1][0] != '-') i++;
            continue;
        }
        if (seen++ == n) return argv[i];
    }
    return NULL;
}

/* Stampa l'esito e ritorna l'exit code (0 ok, 1 errore). */
static int emit(gmb_brain_result_t r, int on_fail_exit)
{
    int ok = r.ok;   /* gmb_brain_result_free azzera la struct */
    if (r.text && r.text[0]) printf("%s\n", r.text);
    if (!ok) {
        if (r.error && r.error[0]) fprintf(stderr, "?%s\n", r.error);
        else fprintf(stderr, "?operazione fallita\n");
    }
    gmb_brain_result_free(&r);
    return ok ? 0 : on_fail_exit;
}

static void brain_help(void)
{
    printf("uso: gmb brain <comando> [opzioni]\n"
           "\n"
           "  list | info <nome> | use <nome[,nome]> | detach <nome> | resolve <nome>\n"
           "  install <nome> [<sorgente>] | uninstall <nome> | login --github [token] | logout\n"
           "  stats <nome> [--json]\n"
           "  verify <nome> [--json] | verify <nome> --golden <file> [--topk N] [--min-p1 X]\n"
           "  create <nome> --from <dir> [--mode rag|graph|both] [--register] [--activate] [--dry-run]\n"
           "  register <nome> [--mode rag|graph|both] [--source s] [--activate]\n"
           "  forget <nome> | buildvault [--out <dir>]\n"
           "  optimize <nome> [--dry-run] [--reindex-fts]\n"
           "  rename <nome> <nuovo> [--dry-run]\n"
           "  backup <nome> [--out file.zip] | export <nome> [--out file.zip]\n"
           "  sign <nome> <cartella> [--out file.zip]\n"
           "  dedupe <nome> [--apply] [--min-chars N] [--dry-run]\n"
           "  extract <nome> --out <nuovo> [--source s] [--node n] [--hops N] [--dry-run]\n"
           "  merge <a> <b> [<c>…] --out <nuovo> [--on-conflict skip|replace] [--dry-run]\n"
           "  convert <nome> --to rag|graph [--out nuovo] [--in-place] [--dry-run] [--min-edges N]\n"
           "  diff <a> <b> [--json]\n"
           "  doctor <nome>\n");
}

int gmbadmin_brain(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "help";
    if (!strcasecmp(sub, "help") || !strcmp(sub, "-h") || !strcmp(sub, "--help")) { brain_help(); return 0; }

    if (!strcasecmp(sub, "list")) {
        char* l = gmb_brain_registry_list();
        if (l && l[0]) printf("%s\n", l);
        else printf("(nessun cervello registrato)\n");
        free(l);
        return 0;
    }
    if (!strcasecmp(sub, "use")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain use <nome[,nome...]>\n"); return 2; }
        int rc = gmb_brain_use(name);
        printf(rc ? "collegati: %s\n" : "?brain use fallito\n", name);
        return rc ? 0 : 1;
    }
    if (!strcasecmp(sub, "detach")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain detach <nome>\n"); return 2; }
        int rc = gmb_brain_detach(name);
        printf(rc ? "scollegato: %s\n" : "?brain detach fallito\n", name);
        return rc ? 0 : 1;
    }
    if (!strcasecmp(sub, "login") || !strcasecmp(sub, "logout")) {
        /* Token GitHub per gli asset privati di GMBasicBrains: <configDir>/github.token (0600).
         * Letto dal runtime (gmb_brains_github_token) con fail-closed sui permessi. */
        char cfg[1024], path[1200];
        gmb_paths_config_dir(cfg, sizeof cfg);
        snprintf(path, sizeof path, "%s/github.token", cfg);
        if (!strcasecmp(sub, "logout")) {
            if (unlink(path) != 0) { fprintf(stderr, "?brain logout: nessun token in %s\n", path); return 1; }
            printf("token GitHub rimosso (%s)\n", path);
            return 0;
        }
        const char* tok = bopt(argc, argv, 3, "--token");
        if (!tok) {
            /* `--github` è un flag BOOLEANO: non consuma l'argomento successivo */
            for (int i = 3; i < argc; i++) {
                if (argv[i][0] != '-') { tok = argv[i]; break; }
            }
        }
        if (!tok || !*tok) {
            fprintf(stderr, "?uso: gmb brain login --github [<token>|--token T]\n"
                            "  (crea un token su https://github.com/settings/tokens con scope 'repo' —\n"
                            "   serve per il repo privato c-vision/GMBasicBrains)\n");
            return 2;
        }
        gmb_fs_mkdir_p(cfg);
        FILE* f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "?brain login: scrittura fallita (%s)\n", path); return 1; }
        fprintf(f, "%s\n", tok);
        fclose(f);
        chmod(path, 0600);
        printf("token GitHub salvato in %s (chmod 600)\n", path);
        return 0;
    }
    if (!strcasecmp(sub, "install") || !strcasecmp(sub, "uninstall")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain %s <nome> [<sorgente>]\n", sub); return 2; }
        if (!strcasecmp(sub, "uninstall")) {
            double ok = gmb_brain_uninstall(name);
            if (ok > 0) {
                /* coerente col registry: niente voce "registrata ma vuota" dopo la rimozione */
                gmb_brain_detach(name);
                gmb_brain_reg_unregister(name);
                printf("rimosso: %s\n", name);
                return 0;
            }
            fprintf(stderr, "?brain uninstall fallito\n");
            return 1;
        }
        const char* src = bpos(argc, argv, 3, 1);
        double ok = gmb_brain_install(name, (src && *src) ? src : NULL);
        if (ok <= 0) return 1;
        printf("installato e collegato: %s\n", name);
        return 0;
    }
    if (!strcasecmp(sub, "info") || !strcasecmp(sub, "resolve")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain %s <nome>\n", sub); return 2; }
        gmb_brain_rec_t rec;
        if (!gmb_brain_reg_info(name, &rec)) { fprintf(stderr, "?cervello '%s' non registrato\n", name); return 1; }
        printf("name:       %s\nmode:       %s\navailable:  %s\nattached:   %s\n",
               rec.name, rec.mode[0] ? rec.mode : "-", rec.available[0] ? rec.available : "-",
               rec.attached ? "true" : "false");
        if (rec.version[0]) printf("version:    %s\n", rec.version);
        if (rec.docs) printf("docs:       %ld\n", rec.docs);
        if (rec.checksum[0]) printf("checksum:   %s\n", rec.checksum);
        if (rec.source[0]) printf("source:     %s\n", rec.source);
        if (rec.vault_path[0]) printf("vaultPath:  %s\n", rec.vault_path);
        char rag[2048], graph[2400];
        if (gmb_brain_reg_rag_db(name, rag, sizeof rag)) printf("rag:        %s\n", rag);
        if (gmb_brain_reg_graph_json(name, graph, sizeof graph)) printf("graph:      %s\n", graph);
        return 0;
    }

    if (!strcasecmp(sub, "stats")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain stats <nome> [--json]\n"); return 2; }
        return emit(gmb_brain_tool_stats(name, bflag(argc, argv, 3, "--json")), 1);
    }
    if (!strcasecmp(sub, "verify")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain verify <nome> [--json] [--golden file]\n"); return 2; }
        const char* golden = bopt(argc, argv, 3, "--golden");
        if (golden && *golden) {
            int topk = 5;
            const char* tk = bopt(argc, argv, 3, "--topk");
            if (!tk) tk = bopt(argc, argv, 3, "-t");
            if (tk) topk = atoi(tk);
            const char* mp = bopt(argc, argv, 3, "--min-p1");
            double minp = 0.0 / 0.0;
            if (mp) minp = atof(mp);
            return emit(gmb_brain_tool_verify_golden(name, golden, topk, minp), 1);
        }
        return emit(gmb_brain_tool_verify(name, bflag(argc, argv, 3, "--json")), 1);
    }
    if (!strcasecmp(sub, "create")) {
        const char* name = bpos(argc, argv, 3, 0);
        const char* from = bopt(argc, argv, 3, "--from");
        if (!name || !from) { fprintf(stderr, "?uso: gmb brain create <nome> --from <dir> [--mode …]\n"); return 2; }
        return emit(gmb_brain_tool_create(name, from, bopt(argc, argv, 3, "--mode"),
                                          bflag(argc, argv, 3, "--register"), bflag(argc, argv, 3, "--activate"),
                                          bflag(argc, argv, 3, "--dry-run")), 1);
    }
    if (!strcasecmp(sub, "register")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain register <nome> [--mode rag|graph|both]\n"); return 2; }
        return emit(gmb_brain_tool_register(name, bopt(argc, argv, 3, "--mode"),
                                            bopt(argc, argv, 3, "--source"),
                                            bflag(argc, argv, 3, "--activate")), 1);
    }
    if (!strcasecmp(sub, "forget")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain forget <nome>\n"); return 2; }
        int rc = gmb_brain_reg_unregister(name);
        printf(rc ? "rimosso dal registry: %s\n" : "?brain forget: '%s' non registrato\n", name);
        return rc ? 0 : 1;
    }
    if (!strcasecmp(sub, "buildvault"))
        return emit(gmb_brain_tool_buildvault(bopt(argc, argv, 3, "--out")), 1);

    if (!strcasecmp(sub, "optimize")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain optimize <nome> [--dry-run] [--reindex-fts]\n"); return 2; }
        return emit(gmb_brain_tool_optimize(name, bflag(argc, argv, 3, "--dry-run"),
                                            bflag(argc, argv, 3, "--reindex-fts")), 1);
    }
    if (!strcasecmp(sub, "rename")) {
        const char* name = bpos(argc, argv, 3, 0);
        const char* nn = bpos(argc, argv, 3, 1);
        if (!name || !nn) { fprintf(stderr, "?uso: gmb brain rename <nome> <nuovo> [--dry-run]\n"); return 2; }
        return emit(gmb_brain_tool_rename(name, nn, bflag(argc, argv, 3, "--dry-run")), 1);
    }
    if (!strcasecmp(sub, "backup") || !strcasecmp(sub, "export")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain %s <nome> [--out file.zip]\n", sub); return 2; }
        const char* out = bopt(argc, argv, 3, "--out");
        return emit(!strcasecmp(sub, "backup") ? gmb_brain_tool_backup(name, out)
                                               : gmb_brain_tool_export(name, out), 1);
    }
    if (!strcasecmp(sub, "sign")) {
        const char* name = bpos(argc, argv, 3, 0);
        const char* dir = bpos(argc, argv, 3, 1);
        if (!name || !dir) { fprintf(stderr, "?uso: gmb brain sign <nome> <cartella> [--out zip]\n"); return 2; }
        return emit(gmb_brain_tool_sign(name, dir, bopt(argc, argv, 3, "--out")), 1);
    }
    if (!strcasecmp(sub, "dedupe")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain dedupe <nome> [--apply] [--min-chars N]\n"); return 2; }
        int minchars = 40;
        const char* mc = bopt(argc, argv, 3, "--min-chars");
        if (mc) minchars = atoi(mc);
        return emit(gmb_brain_tool_dedupe(name, bflag(argc, argv, 3, "--apply"), minchars,
                                          bflag(argc, argv, 3, "--dry-run")), 1);
    }
    if (!strcasecmp(sub, "extract")) {
        const char* name = bpos(argc, argv, 3, 0);
        const char* out = bopt(argc, argv, 3, "--out");
        if (!name || !out) { fprintf(stderr, "?uso: gmb brain extract <nome> --out <nuovo> [--source s] [--node n] [--hops N]\n"); return 2; }
        int hops = 1;
        const char* hp = bopt(argc, argv, 3, "--hops");
        if (hp) hops = atoi(hp);
        return emit(gmb_brain_tool_extract(name, out, bopt(argc, argv, 3, "--source"),
                                           bopt(argc, argv, 3, "--node"), hops,
                                           bflag(argc, argv, 3, "--dry-run")), 1);
    }
    if (!strcasecmp(sub, "merge")) {
        const char* out = bopt(argc, argv, 3, "--out");
        const char* names[64];
        int n = 0;
        for (int i = 3; i < argc && n < 64; i++) {
            if (argv[i][0] == '-') {
                if (!strchr(argv[i], '=') && i + 1 < argc && argv[i + 1][0] != '-') i++;
                continue;
            }
            names[n++] = argv[i];
        }
        if (n < 2 || !out) { fprintf(stderr, "?uso: gmb brain merge <a> <b> [<c>…] --out <nuovo>\n"); return 2; }
        return emit(gmb_brain_tool_merge(names, n, out, bopt(argc, argv, 3, "--on-conflict"),
                                         bflag(argc, argv, 3, "--dry-run")), 1);
    }
    if (!strcasecmp(sub, "convert")) {
        const char* name = bpos(argc, argv, 3, 0);
        const char* to = bopt(argc, argv, 3, "--to");
        if (!name || !to) { fprintf(stderr, "?uso: gmb brain convert <nome> --to rag|graph [--out n] [--in-place]\n"); return 2; }
        int min_edges = 1;
        const char* me = bopt(argc, argv, 3, "--min-edges");
        if (me) min_edges = atoi(me);
        return emit(gmb_brain_tool_convert(name, to, bopt(argc, argv, 3, "--out"),
                                           bflag(argc, argv, 3, "--in-place"),
                                           bflag(argc, argv, 3, "--dry-run"), min_edges), 1);
    }
    if (!strcasecmp(sub, "diff")) {
        const char* a = bpos(argc, argv, 3, 0);
        const char* b = bpos(argc, argv, 3, 1);
        if (!a || !b) { fprintf(stderr, "?uso: gmb brain diff <a> <b> [--json]\n"); return 2; }
        return emit(gmb_brain_tool_diff(a, b, bflag(argc, argv, 3, "--json")), 1);
    }
    if (!strcasecmp(sub, "doctor")) {
        const char* name = bpos(argc, argv, 3, 0);
        if (!name) { fprintf(stderr, "?uso: gmb brain doctor <nome>\n"); return 2; }
        return emit(gmb_brain_tool_doctor(name), 1);
    }
    fprintf(stderr, "?brain: comando sconosciuto '%s' (gmb brain help)\n", sub);
    return 2;
}
