/* gmb_brain_fetch_cli.c — porta i CERVELLI dichiarati da un programma accanto all'eseguibile.
 *
 * PERCHÉ. Un programma compilato gira su una macchina senza compilatore e senza la cartella
 * `shared/`: i suoi cervelli devono stare accanto all'eseguibile (`<exe>/brains/...`), che la
 * risoluzione consulta PRIMA della cache. Il build emette `<exe>.brains.txt` (una voce per riga,
 * `nome<TAB>versione`) insieme allo snapshot di `brains-manifest.json` in `<exe>/settings/`:
 * questo comando scarica le voci mancanti con il motore `gmb_fetch` (resume + sha256 verificato).
 *
 * Uso:
 *   gmb_brain_fetch <eseguibile | cartella> [--only <nome>] [--token-file <file>] [--quiet]
 *
 * Exit: 0 = tutto a posto (anche "niente da fare"), 1 = una o più voci sono fallite.
 */
#include "gmb_brains.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int is_dir(const char* p)
{
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

static char* read_token_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) buf[--n] = '\0';
    if (!n) return NULL;
    char* out = (char*)malloc(n + 1);
    if (out) memcpy(out, buf, n + 1);
    return out;
}

int main(int argc, char** argv)
{
    /* modalità "genera indice": la usa il build (bundle) per scrivere <exe>.brains.txt */
    if (argc >= 5 && strcmp(argv[1], "--emit-index") == 0) {
        int n = gmb_brains_emit_index(argv[2], argv[3], argv[4]);
        if (n < 0) {
            fprintf(stderr, "gmb_brain_fetch --emit-index: %s\n", gmb_brain_strerror(n));
            return 1;
        }
        if (n > 0) printf("gmb_brain_fetch: indice con %d cervelli -> %s\n", n, argv[4]);
        return 0;
    }

    const char* target = NULL;
    const char* only = NULL;
    const char* token_file = NULL;
    int quiet = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
        else if (strcmp(argv[i], "--token-file") == 0 && i + 1 < argc) token_file = argv[++i];
        else if (strcmp(argv[i], "--quiet") == 0) quiet = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            fprintf(stderr, "uso: gmb_brain_fetch <eseguibile|cartella> [--only <nome>] [--token-file <file>] [--quiet]\n"
                            "     gmb_brain_fetch --emit-index <app.settings.json> <brains-manifest.json> <out.brains.txt>\n");
            return 0;
        }
        else if (!target) target = argv[i];
    }
    if (!target) {
        fprintf(stderr, "uso: gmb_brain_fetch <eseguibile|cartella> [--only <nome>] [--token-file <file>] [--quiet]\n"
                        "     gmb_brain_fetch --emit-index <app.settings.json> <brains-manifest.json> <out.brains.txt>\n");
        return 1;
    }

    char dir[4096], index[4200];
    if (is_dir(target)) {
        snprintf(dir, sizeof dir, "%s", target);
        snprintf(index, sizeof index, "%s/brains.txt", dir);
    } else {
        snprintf(dir, sizeof dir, "%s", target);
        char* slash = strrchr(dir, '/');
        if (slash) *slash = '\0';
        else snprintf(dir, sizeof dir, ".");
        snprintf(index, sizeof index, "%s.brains.txt", target);
    }

    const gmb_fetch_transport* tr = gmb_fetch_transport_curl();
    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = quiet;

    char* token = token_file ? read_token_file(token_file) : NULL;
    if (token_file && !token) {
        fprintf(stderr, "gmb_brain_fetch: impossibile leggere il token da %s\n", token_file);
        return 1;
    }

    char err[512];
    int n = gmb_brains_fetch_index(index, dir, only, token, tr, &op, err, sizeof err);
    if (n < 0) {
        fprintf(stderr, "gmb_brain_fetch: %d voci fallite%s%s\n", -n, err[0] ? " — " : "", err);
        free(token);
        return 1;
    }
    if (!quiet) {
        if (n == 0) printf("gmb_brain_fetch: niente da fare (cervelli già presenti in %s/brains)\n", dir);
        else printf("gmb_brain_fetch: %d cervelli installati in %s/brains\n", n, dir);
    }
    free(token);
    return 0;
}
