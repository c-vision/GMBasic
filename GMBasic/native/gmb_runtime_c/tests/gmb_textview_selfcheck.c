/* ============================================================================
 * gmb_textview_selfcheck.c — vista testuale virtuale (plain / diff / markdown).
 *
 * Prova la MATEMATICA della vista, senza disegnare: righe, stili simbolici,
 * wrapping UTF-8, tabelle, numero di righe e ri-layout (il costo del disegno
 * dipende solo dalle righe visibili, e questa è la garanzia che le righe sono
 * quelle attese).
 *
 * Sorgenti: gli stessi casi del MiniTest `shared/minitests/controls/ui_textview.bas`,
 * così il comportamento C e quello BASIC non possono divergere in silenzio.
 * ============================================================================ */
#include "gmb_textview.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond, fmt, ...) do { \
    g_checks++; \
    if (!(cond)) { g_fail++; fprintf(stderr, "FAIL %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } \
} while (0)

static int count_rows(const gmb_textview_t* tv, int style)
{
    int n = 0;
    for (int i = 0; i < gmb_textview_rows(tv); i++)
        if (gmb_textview_row_style(tv, i) == style) n++;
    return n;
}


static int cp_len(const char* s)
{
    int k = 0;
    for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) k++;
    return k;
}

/* ── plain / lista grande ─────────────────────────────────────────────── */
static void test_plain(void)
{
    gmb_textview_t* tv = gmb_textview_new("a\nb\nc\n", GMB_TV_PLAIN, 40);
    CHECK(tv != NULL, "new plain");
    CHECK(gmb_textview_rows(tv) == 3, "plain \"a\\nb\\nc\\n\" = 3 righe (non 4), got %d", gmb_textview_rows(tv));
    CHECK(!strcmp(gmb_textview_row(tv, 0), "a"), "plain riga 0 = \"a\", got \"%s\"", gmb_textview_row(tv, 0));
    CHECK(!strcmp(gmb_textview_row(tv, 2), "c"), "plain riga 2 = \"c\", got \"%s\"", gmb_textview_row(tv, 2));
    CHECK(!strcmp(gmb_textview_row(tv, 99), ""), "fuori range = \"\"");
    CHECK(gmb_textview_row_style(tv, 1) == GMB_TV_S_NORMAL, "plain riga 1 stile NORMAL");
    /* la riga vuota VOLUTA resta */
    gmb_textview_free(tv);
    tv = gmb_textview_new("a\n\nb", GMB_TV_PLAIN, 40);
    CHECK(gmb_textview_rows(tv) == 3, "plain \"a\\n\\nb\" = 3 righe, got %d", gmb_textview_rows(tv));
    CHECK(!strcmp(gmb_textview_row(tv, 1), ""), "riga vuota centrale = \"\"");
    gmb_textview_free(tv);
    /* sorgente vuota = nessuna riga */
    tv = gmb_textview_new("", GMB_TV_PLAIN, 40);
    CHECK(gmb_textview_rows(tv) == 0, "sorgente vuota = 0 righe, got %d", gmb_textview_rows(tv));
    gmb_textview_free(tv);
    /* plain non ha wrapping: la larghezza non cambia il numero di righe */
    tv = gmb_textview_new("una riga molto lunga che non si impagina mai", GMB_TV_PLAIN, 80);
    int n = gmb_textview_rows(tv);
    CHECK(gmb_textview_layout(tv, 10) == n, "plain: layout a 10 colonne non cambia le righe");
    gmb_textview_free(tv);
    /* lista GRANDE: nessun limite a 32/64 voci */
    {
        char* big = (char*)malloc(20000 * 8 + 1);
        int o = 0;
        for (int i = 0; i < 20000; i++) o += snprintf(big + o, 8, "r%04d\n", i);
        tv = gmb_textview_new(big, GMB_TV_PLAIN, 60);
        CHECK(gmb_textview_rows(tv) == 20000, "lista grande = 20000 righe, got %d", gmb_textview_rows(tv));
        CHECK(!strcmp(gmb_textview_row(tv, 19999), "r19999"), "ultima riga \"r19999\", got \"%s\"",
              gmb_textview_row(tv, 19999));
        gmb_textview_free(tv);
        free(big);
    }
}

/* ── diff unificato ───────────────────────────────────────────────────── */
static void test_diff(void)
{
    const char* d =
        "diff --git a/x b/x\n"
        "index 111..222\n"
        "--- a/x\n"
        "+++ b/x\n"
        "@@ -1,2 +1,2 @@\n"
        " invariata\n"
        "-vecchia\n"
        "+nuova\n";
    gmb_textview_t* tv = gmb_textview_new(d, GMB_TV_DIFF, 60);
    CHECK(gmb_textview_rows(tv) == 8, "diff = 8 righe, got %d", gmb_textview_rows(tv));
    CHECK(gmb_textview_row_style(tv, 0) == GMB_TV_S_META, "diff riga 0 (diff --git) META");
    CHECK(gmb_textview_row_style(tv, 1) == GMB_TV_S_META, "diff riga 1 (index) META");
    CHECK(gmb_textview_row_style(tv, 2) == GMB_TV_S_META, "diff riga 2 (---) META");
    CHECK(gmb_textview_row_style(tv, 3) == GMB_TV_S_META, "diff riga 3 (+++) META");
    CHECK(gmb_textview_row_style(tv, 4) == GMB_TV_S_HUNK, "diff riga 4 (@@) HUNK");
    CHECK(gmb_textview_row_style(tv, 5) == GMB_TV_S_NORMAL, "diff riga 5 (contesto) NORMAL");
    CHECK(gmb_textview_row_style(tv, 6) == GMB_TV_S_DEL, "diff riga 6 (-vecchia) DEL");
    CHECK(gmb_textview_row_style(tv, 7) == GMB_TV_S_ADD, "diff riga 7 (+nuova) ADD");
    CHECK(!strcmp(gmb_textview_row(tv, 6), "-vecchia"), "diff riga 6 testo, got \"%s\"", gmb_textview_row(tv, 6));
    CHECK(!strcmp(gmb_textview_row(tv, 7), "+nuova"), "diff riga 7 testo, got \"%s\"", gmb_textview_row(tv, 7));
    /* gli stili non contano in una lista plain */
    gmb_textview_free(tv);
    tv = gmb_textview_new(d, GMB_TV_PLAIN, 60);
    CHECK(gmb_textview_row_style(tv, 7) == GMB_TV_S_NORMAL, "plain: nessuno stile diff");
    gmb_textview_free(tv);
}

/* ── Markdown ─────────────────────────────────────────────────────────── */
static void test_markdown(void)
{
    const char* md =
        "# Titolo\n"
        "\n"
        "Un paragrafo con abbastanza parole per andare a capo piu di una volta davvero.\n"
        "\n"
        "> Una citazione.\n"
        "\n"
        "- primo\n"
        "- secondo\n"
        "\n"
        "1. uno\n"
        "2. due\n"
        "\n"
        "```basic\n"
        "PRINT \"ciao\"\n"
        "x = 1\n"
        "```\n"
        "\n"
        "| nome | eta |\n"
        "|------|-----|\n"
        "| anna | 30  |\n";
    gmb_textview_t* tv = gmb_textview_new(md, GMB_TV_MARKDOWN, 60);
    CHECK(tv != NULL, "new markdown");
    int rows = gmb_textview_rows(tv);
    CHECK(rows > 10, "markdown: righe prodotte (%d)", rows);
    /* il primo titolo deve avere stile HEAD1 e contenere il testo */
    CHECK(gmb_textview_row_style(tv, 0) == GMB_TV_S_HEAD1, "markdown riga 0 = HEAD1");
    CHECK(!strcmp(gmb_textview_row(tv, 0), "Titolo"), "titolo senza '#': \"%s\"", gmb_textview_row(tv, 0));
    CHECK(count_rows(tv, GMB_TV_S_HEAD1) == 1, "un solo HEAD1");
    CHECK(count_rows(tv, GMB_TV_S_QUOTE) >= 1, "citazione presente");
    CHECK(count_rows(tv, GMB_TV_S_CODE) >= 3, "blocco codice + tabella in CODE (%d)",
          count_rows(tv, GMB_TV_S_CODE));
    CHECK(count_rows(tv, GMB_TV_S_RULE) == 1, "una sola riga separatrice di tabella, got %d",
          count_rows(tv, GMB_TV_S_RULE));
    /* liste con prefisso */
    int bullets = 0, numbered = 0;
    for (int i = 0; i < rows; i++)
    {
        const char* r = gmb_textview_row(tv, i);
        if (!strncmp(r, "• primo", 9)) bullets++;
        if (!strncmp(r, "1. uno", 12)) numbered++;
    }
    CHECK(bullets == 1, "voce puntata \"• primo\"");
    CHECK(numbered == 1, "voce numerata \"1. uno\" (numerazione autonoma)");
    /* nessun carattere '#' o '|' di markup grezzo nelle righe rese (tranne la tabella) */
    int hashes = 0;
    for (int i = 0; i < rows; i++)
        if (gmb_textview_row_style(tv, i) != GMB_TV_S_CODE && strchr(gmb_textview_row(tv, i), '#')) hashes++;
    CHECK(hashes == 0, "nessun '#' grezzo fuori dal codice (%d righe)", hashes);
    /* tabella: intestazione + 1 riga dati */
    int table_rows = 0;
    for (int i = 0; i < rows; i++)
        if (strstr(gmb_textview_row(tv, i), " | ")) table_rows++;
    CHECK(table_rows >= 2, "tabella resa in righe con separatore ( %d )", table_rows);

    /* il wrapping rispetta la larghezza: nessuna riga visibile supera `width` codepoint */
    for (int i = 0; i < rows; i++)
    {
        const char* r = gmb_textview_row(tv, i);
        int cp = 0;
        for (const char* q = r; *q; q++) if (((unsigned char)*q & 0xC0) != 0x80) cp++;
        CHECK(cp <= 60, "riga %d entro 60 colonne (ha %d): \"%s\"", i, cp, r);
    }
    gmb_textview_free(tv);

    /* re-layout: a larghezza piu stretta il paragrafo si spezza in piu righe */
    tv = gmb_textview_new(md, GMB_TV_MARKDOWN, 60);
    int wide = gmb_textview_rows(tv);
    int narrow = gmb_textview_layout(tv, 20);
    CHECK(narrow > wide, "re-layout a 20 colonne aumenta le righe (%d -> %d)", wide, narrow);
    CHECK(gmb_textview_layout(tv, 20) == narrow, "secondo layout a pari larghezza = no-op");
    int again = gmb_textview_layout(tv, 60);
    CHECK(again == wide, "ritorno a 60 colonne ritrova il numero di righe (%d vs %d)", again, wide);
    gmb_textview_free(tv);

    /* sorgente assente/vuota: nessuna riga, nessun crash */
    tv = gmb_textview_new(NULL, GMB_TV_MARKDOWN, 40);
    CHECK(tv != NULL && gmb_textview_rows(tv) == 0, "markdown vuoto = 0 righe");
    gmb_textview_free(tv);
}

/* ── parsing del modo (STYLE) ─────────────────────────────────────────── */
static void test_modes(void)
{
    CHECK(gmb_textview_parse_mode("markdown") == GMB_TV_MARKDOWN, "mode markdown");
    CHECK(gmb_textview_parse_mode("MD") == GMB_TV_MARKDOWN, "mode MD (case-insensitive)");
    CHECK(gmb_textview_parse_mode("diff") == GMB_TV_DIFF, "mode diff");
    CHECK(gmb_textview_parse_mode("plain") == GMB_TV_PLAIN, "mode plain");
    CHECK(gmb_textview_parse_mode("qualcosa") == -1, "modo sconosciuto = -1");
    CHECK(gmb_textview_parse_mode("") == GMB_TV_PLAIN, "modo vuoto = plain");
    CHECK(!strcmp(gmb_textview_mode_name(GMB_TV_DIFF), "diff"), "nome diff");
    CHECK(!strcmp(gmb_textview_mode_name(GMB_TV_MARKDOWN), "markdown"), "nome markdown");

    /* dallo STYLE: mode=… e virtual=1 */
    CHECK(gmb_textview_style_mode("role=info mode=diff", GMB_TV_PLAIN) == GMB_TV_DIFF, "style mode=diff");
    CHECK(gmb_textview_style_mode("mode=markdown fg=red", GMB_TV_PLAIN) == GMB_TV_MARKDOWN, "style mode=markdown");
    CHECK(gmb_textview_style_mode("role=info", GMB_TV_MARKDOWN) == GMB_TV_MARKDOWN, "style senza mode = invariato");
    CHECK(gmb_textview_style_mode("mode=xyz", GMB_TV_DIFF) == GMB_TV_DIFF, "style mode sconosciuto = invariato");
    CHECK(gmb_textview_style_mode("virtual=1", GMB_TV_DIFF) == GMB_TV_PLAIN, "style virtual=1 = plain");
}

int main(void)
{
    test_plain();
    test_diff();
    test_markdown();
    test_modes();
    printf("gmb_textview_selfcheck: %d controlli, %d falliti\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}