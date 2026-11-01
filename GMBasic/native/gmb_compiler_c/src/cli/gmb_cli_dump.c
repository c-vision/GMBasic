/* gmb_cli_dump.c — dump richiesti dalla REPL, dietro OPZIONI del compilatore (F1).
 *
 *   gmb dump ast    <file.bas>                → AST + riepilogo linker (stdout)
 *   gmb dump chunk  <file.bas> [--out f.chunk]→ bytecode A1/GMBC
 *   gmb dump c      <file.bas> [--out f.c]    → C emesso (C99Native)
 *   gmb dump disasm <file.bas>                → disassemblaggio del bytecode
 *   gmb eval        "<istruzioni>"            → compila ed esegue uno snippet
 *
 * Regola (TODO §6): la REPL NON include lexer/parser: chiama queste opzioni.
 * Qui dentro si usa il front-end (gmb_frontend) e i driver già esistenti; nessuna
 * logica duplicata nella REPL.
 */
#include "gmb_cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ── helper: percorso del binario accanto a `gmb` ─────────────────────────────────── */
static int exe_dir(char* out, size_t cap)
{
    char self[1024];
    if (readlink("/proc/self/exe", self, sizeof self - 1) > 0) {           /* Linux */
        char* s = strrchr(self, '/'); if (s) *s = 0; snprintf(out, cap, "%s", self); return 1;
    }
    /* macOS non ha /proc: si usa _NSGetExecutablePath se disponibile, altrimenti argv0 */
    extern int _NSGetExecutablePath(char*, uint32_t*);
    char buf[2048]; uint32_t n = sizeof buf;
    if (_NSGetExecutablePath(buf, &n) == 0) {
        char resolved[2048];
        if (realpath(buf, resolved)) { char* s = strrchr(resolved, '/'); if (s) *s = 0; snprintf(out, cap, "%s", resolved); return 1; }
    }
    snprintf(out, cap, ".");
    return 0;
}

static int run_tool_ex(const char* tool, const char* a1, const char* a2, const char* extra)
{
    char dir[1024], path[2048], cmd[4096];
    exe_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/%s", dir, tool);
    if (access(path, X_OK) != 0) { fprintf(stderr, "gmb: %s non trovato accanto a gmb (%s)\n", tool, path); return 1; }
    if (a2 && extra) snprintf(cmd, sizeof cmd, "'%s' '%s' '%s' %s", path, a1, a2, extra);
    else if (a2)     snprintf(cmd, sizeof cmd, "'%s' '%s' '%s'", path, a1, a2);
    else             snprintf(cmd, sizeof cmd, "'%s' '%s'", path, a1);
    int rc = system(cmd);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
}

static int run_tool(const char* tool, const char* a1, const char* a2)
{ return run_tool_ex(tool, a1, a2, NULL); }

/* ── dump ast (in-process, front-end) ────────────────────────────────────────────── */
int gmb_cli_dump_ast(const char* file)
{
    FILE* f = fopen(file, "rb");
    if (!f) { fprintf(stderr, "gmb: file non trovato: %s\n", file); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); fprintf(stderr, "gmb: file vuoto: %s\n", file); return 2; }
    char* src = (char*)malloc((size_t)n + 1);
    if (fread(src, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(src); return 2; }
    src[n] = 0; fclose(f);

    gmb_lex_result_t lx = gmb_lexer_scan(src, (int)n);
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    int rc = 0;
    if (perr) { fprintf(stderr, "gmb: parse error: %s\n", perr); free(perr); rc = 3; }
    else {
        printf("; AST di %s (%d statement)\n", file, prog.n);
        gmb_ast_dump(prog.stmts, prog.n, stdout);
        gmb_link_result_t lr;
        gmb_link_resolve(&lr, prog.stmts, prog.n);
        printf("; Simboli: %d (undefined %d, duplicati %d)\n", lr.count, lr.undefined_count, lr.duplicate_count);
        if (lr.error) printf("; Errore linker: %s (riga %d)\n", lr.error, lr.error_line);
        gmb_link_free(&lr);
    }
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(src);
    return rc;
}

/* ── dump chunk / c / disasm: delega ai driver (unica implementazione) ───────────── */
int gmb_cli_dump_chunk(const char* file, const char* out)
{
    char tmp[1024];
    if (!out) { snprintf(tmp, sizeof tmp, "/tmp/gmb_dump_%d.chunk", (int)getpid()); out = tmp; }
    int rc = run_tool("gmb_chunk_driver", file, out);
    if (rc == 0 && out == tmp) { printf("%s\n", out); }
    return rc;
}

int gmb_cli_dump_c(const char* file, const char* out)
{
    char tmp[1024];
    if (!out) { snprintf(tmp, sizeof tmp, "/tmp/gmb_dump_%d.c", (int)getpid()); out = tmp; }
    int rc = run_tool_ex("gmb_c99native_driver", file, out, "--no-exe");
    if (rc == 0 && out == tmp) { printf("%s\n", out); }
    return rc;
}

/* ── eval: compila ed esegue uno snippet LOCALE al processo (stesso compilatore C) ── */
int gmb_cli_eval(const char* code)
{
    char bas[1024], chunk[1024];
    snprintf(bas, sizeof bas, "/tmp/gmb_eval_%d.bas", (int)getpid());
    snprintf(chunk, sizeof chunk, "/tmp/gmb_eval_%d.chunk", (int)getpid());
    FILE* f = fopen(bas, "w");
    if (!f) { fprintf(stderr, "gmb: impossibile scrivere %s\n", bas); return 1; }
    fprintf(f, "SUB Main()\n%s\nEND SUB\n", code);
    fclose(f);
    int rc = run_tool("gmb_chunk_driver", bas, chunk);
    if (rc == 0) {
        char dir[1024], host[2048];
        exe_dir(dir, sizeof dir);
        snprintf(host, sizeof host, "%s/../../gmb_vm_c/build/gmb_vm_host", dir);
        /* il VM/C host può stare altrove nel monorepo: si prova anche accanto */
        if (access(host, X_OK) != 0) snprintf(host, sizeof host, "%s/gmb_vm_host", dir);
        if (access(host, X_OK) == 0) {
            char cmd[4096];
            snprintf(cmd, sizeof cmd, "'%s' '%s'", host, chunk);
            int r2 = system(cmd);
            rc = WIFEXITED(r2) ? WEXITSTATUS(r2) : 1;
        } else {
            fprintf(stderr, "gmb: gmb_vm_host non trovato per eval\n");
            rc = 1;
        }
    }
    unlink(bas); unlink(chunk);
    return rc;
}
