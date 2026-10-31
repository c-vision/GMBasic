/* run_conformance.c — verifica MiniTest/sample .bas con la gamba C99Native.
 * Estrae la riga `' EXPECT:` dal file, esegue tools/gmb_c99native_run.sh e confronta
 * l'output normalizzato (righe unite con '|') con l'oracolo.
 * Uso: run_conformance <repo-root> <file.bas> [file2.bas ...]
 * Exit 0 = PASS/SKIP, 1 = almeno un FAIL, 2 = uso/setup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_expect(const char* path, char* out, size_t cap)
{
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\'') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "EXPECT:", 7) != 0) continue;
        p += 7;
        while (*p == ' ' || *p == '\t') p++;
        size_t n = strlen(p);
        while (n > 0 && (p[n-1] == '\n' || p[n-1] == '\r')) p[--n] = 0;
        snprintf(out, cap, "%s", p);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void normalize(char* s)
{
    char out[8192];
    size_t j = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\r') continue;
        if (s[i] == '\n') {
            while (j > 0 && (out[j-1] == ' ' || out[j-1] == '\t')) j--;
            out[j++] = '|';
        } else if (j + 1 < sizeof out) out[j++] = s[i];
    }
    if (j > 0 && out[j-1] == '|') j--;
    out[j] = 0;
    snprintf(s, 8192, "%s", out);
}

static int run_file(const char* root, const char* path)
{
    char absfile[4096];
    if (!realpath(path, absfile)) { perror("realpath"); return 1; }
    char expect[8192];
    if (!read_expect(absfile, expect, sizeof expect)) {
        printf("SKIP (nessun EXPECT): %s\n", absfile);
        return 0;
    }
    if (chdir(root) != 0) { perror("chdir"); return 1; }
    char cmd[8192];
    snprintf(cmd, sizeof cmd, "bash tools/gmb_c99native_run.sh '%s' conf 2>/dev/null", absfile);
    FILE* p = popen(cmd, "r");
    if (!p) { perror("popen"); return 1; }
    char actual[8192] = {0};
    size_t used = 0;
    char buf[2048];
    while (fgets(buf, sizeof buf, p) && used + strlen(buf) + 1 < sizeof actual) {
        strcpy(actual + used, buf);
        used += strlen(buf);
    }
    int rc = pclose(p);
    normalize(actual);
    if (rc != 0) {
        printf("FAIL (run rc=%d): %s\n  atteso: %s\n  ottenuto: %s\n", rc, absfile, expect, actual);
        return 1;
    }
    if (strcmp(expect, actual) != 0) {
        printf("FAIL: %s\n  atteso: %s\n  ottenuto: %s\n", absfile, expect, actual);
        return 1;
    }
    printf("PASS: %s\n", absfile);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <repo-root> <file.bas> [file2.bas ...]\n", argv[0]); return 2; }
    int fails = 0, total = 0;
    char* root = realpath(argv[1], NULL);
    if (!root) { perror("realpath root"); free(root); return 2; }
    char** paths = calloc((size_t)(argc - 2), sizeof(char*));
    if (!paths) return 2;
    for (int i = 2; i < argc; i++) {
        paths[i - 2] = realpath(argv[i], NULL);
        if (!paths[i - 2]) { perror("realpath"); fails++; total++; }
    }
    for (int i = 2; i < argc; i++) {
        if (!paths[i - 2]) continue;
        fails += run_file(root, paths[i - 2]); total++;
        free(paths[i - 2]);
    }
    free(paths);
    free(root);
    printf("Conformance: %d file, %d FAIL\n", total, fails);
    return fails ? 1 : 0;
}
