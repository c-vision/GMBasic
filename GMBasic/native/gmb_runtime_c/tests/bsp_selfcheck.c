/* Golden BSP: HTML + <%= %> + <% FOR %> → output catturato. */
#include "gmb_bsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

static void fail_msg(const char* name, const char* detail)
{
    fprintf(stderr, "FAIL %s: %s\n", name, detail);
    g_fail = 1;
}

int main(void)
{
    gmb_dict_t env = gmb_dict_new();
    gmb_dict_set(&env, "n", gmb_make_val_double(7));

    char* html = NULL;
    if (gmb_bsp_render("<html><%= 1+2 %></html>", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &html) != GMB_EXPR_OK
        || !html || strcmp(html, "<html>3</html>") != 0)
        fail_msg("html_expr", html ? html : "null");
    free(html);

    html = NULL;
    if (gmb_bsp_render("<%\nFOR i = 1 TO 3\nPRINT i;\nNEXT\n%>", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &html) != GMB_EXPR_OK
        || !html || strcmp(html, "123") != 0)
        fail_msg("for_page", html ? html : "null");
    free(html);

    html = NULL;
    if (gmb_bsp_render("<%@ IMPORT HTTP %><%= n %>", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &html) != GMB_EXPR_OK
        || !html || strcmp(html, "7") != 0)
        fail_msg("directive_skip", html ? html : "null");
    free(html);

    char* bas = NULL;
    if (gmb_bsp_compile("<b>x</b>", &bas) != GMB_EXPR_OK || !bas || !strstr(bas, "PRINT"))
        fail_msg("compile", bas ? bas : "null");
    free(bas);

    gmb_dict_free(&env);

    {
        char tmpl[] = "/tmp/gmb_bsp_XXXXXX";
        char* d = mkdtemp(tmpl);
        char fpath[512];
        FILE* pf;
        char* page = NULL;
        int st = 0;
        if (!d) {
            fail_msg("try_serve_mkdtemp", "null");
        } else {
            snprintf(fpath, sizeof(fpath), "%s/hi.bsp", d);
            pf = fopen(fpath, "wb");
            if (!pf) {
                fail_msg("try_serve_write", fpath);
            } else {
                fputs("ok <%= q %>", pf);
                fclose(pf);
                if (!gmb_bsp_try_serve(d, "/hi.bsp", "q=ok", NULL, &page, &st)
                    || st != 200 || !page || strcmp(page, "ok ok") != 0)
                    fail_msg("try_serve_query", page ? page : "null");
                free(page);
                page = NULL;
                if (gmb_bsp_try_serve(d, "/missing.bsp", "", NULL, &page, &st))
                    fail_msg("try_serve_missing", "handled");
                free(page);
                page = NULL;
                if (gmb_bsp_try_serve(d, "/hi.html", "", NULL, &page, &st))
                    fail_msg("try_serve_not_bsp", "handled");
                free(page);
                page = NULL;
                if (!gmb_bsp_try_serve(d, "/../hi.bsp", "", NULL, &page, &st) || st != 403)
                    fail_msg("try_serve_dotdot", "not 403");
                free(page);
                unlink(fpath);
            }
            rmdir(d);
        }
    }

    if (g_fail) {
        fprintf(stderr, "gmb_bsp_selfcheck FAILED\n");
        return 1;
    }
    puts("gmb_bsp_selfcheck OK");
    return 0;
}
