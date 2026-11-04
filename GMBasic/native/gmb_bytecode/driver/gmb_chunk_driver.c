/* native/gmb_compiler_c/driver/gmb_chunk_driver.c — compila un .bas in chunk A1/GMBC con il
 * pipeline 100% C (lexer -> macro -> parser -> gmb_compile). Specchio minimale della
 * compilazione chunk di native/executor (gmb_compile_c_chunk), senza aggregazione progetto.
 * Uso: gmb_chunk_driver <file.bas> [out.chunk]   (default out.chunk)
 * Il chunk prodotto e' eseguibile da gmb_vm_host (gmb_vm_c): verifica E2E dei builtin/statement
 * nativi (es. AI SERVER, func_code 3505..3509).
 */
#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_parser.h"
#include "gmb_compiler.h"
#include "gmb_inc_resolve.h"
#include "gmb_bsp.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int pkg_cache_is_dir(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char** argv) {
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        fprintf(stderr,
            "uso: %s <file.bas> [out.chunk] [--map map.json]\n"
            "  compila il sorgente in bytecode A1/GMBC per la VM/C;\n"
            "  una pagina .bsp viene prima espansa in BASIC completo (con IMPORT).\n",
            argv[0]);
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "uso: %s <file.bas> [out.chunk] [--map map.json]\n", argv[0]);
        return 2;
    }
    const char* in = argv[1];
    const char* out = (argc > 2 && strcmp(argv[2], "--map") != 0) ? argv[2] : "out.chunk";
    const char* map_path = NULL;
    for (int a = 2; a < argc; a++) {
        if (strcmp(argv[a], "--map") == 0 && a + 1 < argc) map_path = argv[++a];
    }

    FILE* f = fopen(in, "rb");
    if (!f) { fprintf(stderr, "file non trovato: %s\n", in); return 2; }
    fseek(f, 0, SEEK_END); long alen = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)alen + 1);
    if (alen > 0 && fread(src, 1, (size_t)alen, f) != (size_t)alen) { fclose(f); free(src); return 2; }
    src[alen] = 0; fclose(f);

    /* `.bsp`: pagina BSP → BASIC COMPLETO (con IMPORT) → chunk A1 per la VM/C.
     * È il compilatore BSP in C completo: la pagina usa tutto il linguaggio
     * (FUNCTION/SUB dell'include, classi, ecc.), non il sottoinsieme di gmb_expr.
     * Il BASIC viene scritto in un `.bas` TEMPORANEO accanto alla pagina: il resolver
     * degli include inferisce la root del progetto dall'ingresso e con l'estensione
     * `.bsp` non la trovava (DASHBOARD non risolto). */
    char resolve_in[2048];
    snprintf(resolve_in, sizeof resolve_in, "%s", in);
    char bsp_tmp[2048] = "";
    { size_t il = strlen(in);
      if (il > 4 && strcmp(in + il - 4, ".bsp") == 0) {
          char* bas = NULL;
          if (gmb_bsp_compile_full(src, &bas) != GMB_EXPR_OK || !bas) {
              fprintf(stderr, "bsp: compilazione BSP -> BASIC fallita\n");
              free(src);
              return 4;
          }
          snprintf(bsp_tmp, sizeof bsp_tmp, "%s.gmb_tmp.bas", in);
          FILE* tf = fopen(bsp_tmp, "wb");
          if (!tf) {
              fprintf(stderr, "bsp: impossibile scrivere %s\n", bsp_tmp);
              free(bas); free(src); return 4;
          }
          fwrite(bas, 1, strlen(bas), tf);
          fclose(tf);
          free(src);
          src = bas;
          snprintf(resolve_in, sizeof resolve_in, "%s", bsp_tmp);
      } }

    /* Moduli `include/*.inc` (IMPORT <ns>): prepend — parità col driver C99Native.
     * Con --map si conservano i segmenti per riportare le righe del chunk ai file
     * originali (breakpoint e debug). */
    GMB_SRC_MAP srcmap;
    gmb_src_map_init(&srcmap);
    { char* rs = gmb_resolve_includes_mapped(src, resolve_in, map_path ? &srcmap : NULL); free(src); src = rs; }
    /* Cache pacchetti = cartella `packages/` del PROGETTO (GMBasic/packages). */
    {
        static char pkgcache[2048];
        if (gmb_resolve_install_root(pkgcache, sizeof pkgcache) && pkgcache[0]) {
            size_t pl = strlen(pkgcache);
            snprintf(pkgcache + pl, sizeof pkgcache - pl, "/packages");
            /* UNA sola cartella per repo: se il progetto non ha packages/ (monorepo: la
             * cartella e alla RADICE del monorepo, non dentro GMBasic/), si usa il parent. */
            if (!pkg_cache_is_dir(pkgcache)) {
                snprintf(pkgcache + pl, sizeof pkgcache - pl, "/../packages");
            }
            gmb_pkg_set_cache_dir(pkgcache);
        } else {
            /* CLIENT (nessun checkout sorgente): cache utente ~/.gmbasic/packages — RADICE UNICA
             * dei dati utente. Qui c'era `~/.gmb/packages`: con quella radice un pacchetto
             * installato dal package manager (`~/.gmbasic/packages`) non si trovava dal driver
             * nativo, e il driver ricreava la vecchia cartella. */
            const char* home = getenv("HOME");
            if (home && *home) {
                snprintf(pkgcache, sizeof pkgcache, "%s/.gmbasic/packages", home);
                gmb_pkg_set_cache_dir(pkgcache);
            }
        }
    }
    /* REQUIRE: prepend degli export dei pacchetti risolti. */
    { char* reqsrc = gmb_pkg_collect_requires(src, proj_dir_of(resolve_in));
 if (gmb_pkg_require_failed()) {
     fprintf(stderr, "errore: uno o piu' pacchetti REQUIRE non risolti (vedi sopra).\n");
     free(src);
     return 6;
 }

      if (reqsrc) { size_t rl = strlen(reqsrc), sl = strlen(src);
        int shift = 1;
        for (const char* q = reqsrc; *q; q++) if (*q == '\n') shift++;
        char* comb = (char*)malloc(rl + sl + 2);
        if (comb) { memcpy(comb, reqsrc, rl); comb[rl] = '\n'; memcpy(comb + rl + 1, src, sl + 1); free(src); src = comb; }
        gmb_src_map_shift(&srcmap, shift);
        free(reqsrc); } }

    if (map_path) {
        FILE* mf = fopen(map_path, "wb");
        if (mf) {
            fputs("{\"segments\":[", mf);
            for (int m = 0; m < srcmap.n; m++) {
                if (m) fputc(',', mf);
                fprintf(mf, "{\"start\":%d,\"file\":\"", srcmap.segs[m].line);
                for (const char* q = srcmap.segs[m].file; q && *q; q++) {
                    if (*q == '\\' || *q == '"') fputc('\\', mf);
                    fputc(*q, mf);
                }
                fputs("\"}", mf);
            }
            fputs("]}", mf);
            fclose(mf);
        }
    }

    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (perr) { fprintf(stderr, "parse error: %s\n", perr); free(perr); goto fail; }

    uint8_t* data = NULL; size_t len = 0; char* cerr = NULL;
    if (gmb_compile(&prog, &data, &len, &cerr) != 0) {
        fprintf(stderr, "compilazione: %s\n", cerr ? cerr : "errore");
        free(cerr);
        gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
        return 4;
    }
    FILE* of = fopen(out, "wb");
    if (!of) { fprintf(stderr, "impossibile scrivere: %s\n", out); free(data); goto fail; }
    fwrite(data, 1, len, of);
    fclose(of);
    printf("chunk -> %s (%zu bytes)\n", out, len);
    free(data);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
    if (bsp_tmp[0]) unlink(bsp_tmp);
    return 0;
fail:
    gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
    if (bsp_tmp[0]) unlink(bsp_tmp);
    return 3;
}
