/* Selfcheck dello scope dinamico BASIC (DYNAMIC) — mirror di
 * samples/Community/Clipper/ClipperDynamicScope.bas (cascata, shadowing, ripristino).
 * Linka il runtime vero (gmb_runtime), quindi usa gmb_make_val_string/gmb_string_free reali. */
#include "gmb_dscope.h"
#include <stdio.h>
#include <string.h>

static int g_fails = 0;

static const char* cur(const char* name)
{
    gmb_val_t v;
    if (!gmb_dscope_get(name, &v)) return "<undef>";
    return v.v_string.data ? v.v_string.data : "<null>";
}

static void ck_str(const char* what, const char* got, const char* want)
{
    int ok = got && want && strcmp(got, want) == 0;
    printf("%s %-34s got='%s' want='%s'\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fails++;
}

static void ck_int(const char* what, int got, int want)
{
    int ok = got == want;
    printf("%s %-34s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fails++;
}

int main(void)
{
    gmb_dscope_reset();
    ck_int("depth(empty)", gmb_dscope_depth(), 0);
    ck_str("undef default", gmb_dscope_get("nope", NULL) ? "yes" : "<undef>", "<undef>");

    gmb_dscope_push();                                    /* Main */
    gmb_dscope_declare("messaggio", gmb_make_val_string("Valore originale da Main"));
    ck_str("Main get", cur("messaggio"), "Valore originale da Main");

    gmb_dscope_push();                                    /* LivelloIntermedio */
    ck_str("Intermedio cascade", cur("messaggio"), "Valore originale da Main");
    gmb_dscope_declare("messaggio", gmb_make_val_string("Questa e' una variabile diversa"));
    ck_str("Intermedio shadow", cur("messaggio"), "Questa e' una variabile diversa");

    gmb_dscope_push();                                    /* LivelloProfondo */
    ck_str("Profondo cascade", cur("messaggio"), "Questa e' una variabile diversa");
    gmb_dscope_set("messaggio", gmb_make_val_string("Valore modificato da LivelloProfondo"));
    ck_str("Profondo set", cur("messaggio"), "Valore modificato da LivelloProfondo");
    ck_int("depth peak", gmb_dscope_depth(), 3);
    gmb_dscope_pop();

    ck_str("Intermedio after pop", cur("messaggio"), "Valore modificato da LivelloProfondo");
    gmb_dscope_pop();

    ck_str("Main preserved", cur("messaggio"), "Valore originale da Main");
    ck_int("has present", gmb_dscope_has("messaggio"), 1);
    ck_int("has absent", gmb_dscope_has("nope"), 0);

    gmb_dscope_pop();
    gmb_dscope_reset();
    ck_int("depth after reset", gmb_dscope_depth(), 0);

    printf("\n%s (%d fail)\n", g_fails ? "SELFCHECK FAIL" : "SELFCHECK OK", g_fails);
    return g_fails ? 1 : 0;
}
