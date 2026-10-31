/* check_abi.c — verifica l'ABI di una dylib C contro un elenco di simboli.
 * Uso: check_abi <dylib> <file-simboli>
 * Ogni riga non vuota (senza '#') è un simbolo richiesto: dlopen + dlsym.
 * Exit 0 = tutti presenti; 1 = almeno uno mancante; 2 = uso/dylib non apribile.
 * Parte del contratto runtime-abi: il lato Sharp P/Invoke richiede questi entry point. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc != 3) { fprintf(stderr, "uso: %s <dylib> <simboli>\n", argv[0]); return 2; }
    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen %s: %s\n", argv[1], dlerror()); return 2; }
    FILE* f = fopen(argv[2], "r");
    if (!f) { fprintf(stderr, "fopen %s\n", argv[2]); dlclose(h); return 2; }
    int total = 0, missing = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        total++;
        if (!dlsym(h, line)) { fprintf(stderr, "MANCANTE: %s\n", line); missing++; }
    }
    fclose(f);
    dlclose(h);
    printf("%s: %d/%d simboli presenti\n", argv[1], total - missing, total);
    return missing ? 1 : 0;
}
