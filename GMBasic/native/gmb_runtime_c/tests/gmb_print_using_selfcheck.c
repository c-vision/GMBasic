/* ============================================================================
 * gmb_print_using_selfcheck.c — P7 blocco 5: formattatore di PRINT USING.
 *
 * Ogni check fissa una regola del contratto di `gmb_print_using_format`
 * (include/gmb_print_using.h): larghezza del campo, arrotondamento, migliaia,
 * dollaro flottante, riempimento con asterischi, segni, overflow con '%',
 * campi stringa, ripetizione della riga di formato ed ERRORI espliciti
 * (tipo incoerente, `^^^^`, buffer troppo piccolo) — mai una stampa silenziosa.
 *
 * Eseguito da: ctest -R gmb_print_using_selfcheck
 * ============================================================================ */
#include "gmb_print_using.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static int g_checks = 0;

#define N(v) { 0, (v), NULL }
#define S(s) { 1, 0.0, (s) }

/* Formatta e confronta con l'atteso; in caso di errore stampa cosa è uscito davvero. */
static void expect_at(const char* fmt, const gmb_using_value_t* v, int n, const char* want,
                      const char* what, int line)
{
    char out[256];
    int r;
    out[0] = 0;
    r = gmb_print_using_format(fmt, v, n, out, (int)sizeof(out));
    g_checks++;
    if (r < 0 || strcmp(out, want) != 0)
    {
        printf("FAIL riga %d: %s\n  formato \"%s\" -> \"%s\" (r=%d), atteso \"%s\" [%s]\n",
               line, what, fmt, out, r, want, gmb_print_using_error());
        g_fails++;
    }
}
#define EXPECT(fmt, vals, n, want, what) expect_at((fmt), (vals), (n), (want), (what), __LINE__)

/* Un caso che DEVE fallire, con messaggio non vuoto. */
static void expect_error_at(const char* fmt, const gmb_using_value_t* v, int n, int cap,
                            const char* what, int line)
{
    char out[256];
    int r = gmb_print_using_format(fmt, v, n, out, cap > 0 ? cap : (int)sizeof(out));
    g_checks++;
    if (r >= 0)
    {
        printf("FAIL riga %d: %s (uscita \"%s\")\n", line, what, out);
        g_fails++;
        return;
    }
    if (gmb_print_using_error()[0] == 0)
    {
        printf("FAIL riga %d: %s (errore senza messaggio)\n", line, what);
        g_fails++;
    }
}
#define EXPECT_ERROR(fmt, vals, n, cap, what) expect_error_at((fmt), (vals), (n), (cap), (what), __LINE__)

int main(void)
{
    /* ---- 1. cifre, arrotondamento, larghezza del campo ---- */
    {
        gmb_using_value_t v[] = { N(3.14159) };
        EXPECT("###.##", v, 1, "  3.14", "cifre decimali e allineamento a destra");
    }
    {
        gmb_using_value_t v[] = { N(12.346) };
        EXPECT("###.##", v, 1, " 12.35", "arrotondamento all'ultima cifra decimale");
    }
    {
        gmb_using_value_t v[] = { N(0.04) };
        EXPECT("#.#", v, 1, "0.0", "arrotondamento per difetto e campo pieno");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("###", v, 1, "  5", "intero senza decimali");
    }
    {
        gmb_using_value_t v[] = { N(-5) };
        EXPECT("###", v, 1, " -5", "il segno occupa una posizione del campo");
    }
    {
        gmb_using_value_t v[] = { N(-1.5) };
        EXPECT("###.##", v, 1, " -1.50", "negativo con decimali");
    }
    {
        gmb_using_value_t v[] = { N(3) };
        EXPECT("###.", v, 1, "  3.", "punto decimale senza cifre dopo");
    }

    /* ---- 2. separatore delle migliaia ---- */
    {
        gmb_using_value_t v[] = { N(12345.6) };
        EXPECT("##,###.##", v, 1, "12,345.60", "virgola delle migliaia nell'intero");
    }
    {
        gmb_using_value_t v[] = { N(7) };
        EXPECT("##,###", v, 1, "     7", "la larghezza del campo resta quella del formato");
    }

    /* ---- 3. dollaro flottante e riempimento con asterischi ---- */
    {
        gmb_using_value_t v[] = { N(12.34) };
        EXPECT("$$###.##", v, 1, "  $12.34", "$$ mette il dollaro davanti alla prima cifra");
    }
    {
        gmb_using_value_t v[] = { N(-12.34) };
        EXPECT("$$###.##", v, 1, " -$12.34", "segno prima del dollaro");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("**$###", v, 1, "****$5", "**$ riempie di asterischi fino al dollaro");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("**###", v, 1, "****5", "** riempie di asterischi");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("$###", v, 1, "$  5", "un '$' singolo è testo letterale");
    }

    /* ---- 4. segni espliciti ---- */
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("+###", v, 1, "  +5", "+ iniziale: il segno c'è sempre");
    }
    {
        gmb_using_value_t v[] = { N(-5) };
        EXPECT("+###", v, 1, "  -5", "+ iniziale con negativo");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("###+", v, 1, "  5+", "+ finale con positivo");
    }
    {
        gmb_using_value_t v[] = { N(-5) };
        EXPECT("###+", v, 1, "  5-", "+ finale: il segno segue le cifre (negativo)");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("###-", v, 1, "  5 ", "- finale: spazio per i positivi");
    }
    {
        gmb_using_value_t v[] = { N(-5) };
        EXPECT("###-", v, 1, "  5-", "- finale: il segno segue le cifre (negativo)");
    }

    /* ---- 5. overflow dichiarato con '%' ---- */
    {
        gmb_using_value_t v[] = { N(1234) };
        EXPECT("##", v, 1, "%1234", "cifre oltre il campo: '%' e numero completo, non troncamento");
    }

    /* ---- 6. testo letterale e coda ---- */
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("Tot: ### unità", v, 1, "Tot:   5 unità", "il testo intorno al campo è copiato");
    }
    {
        EXPECT("solo testo", NULL, 0, "solo testo", "senza valori il formato è tutto testo");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("### ###", v, 1, "  5 ", "coda fino al campo senza valore");
    }
    {
        gmb_using_value_t v[] = { N(5) };
        EXPECT("_#: ###", v, 1, "#:   5", "escape '_': il carattere dopo è letterale");
    }

    /* ---- 7. campi stringa ---- */
    {
        gmb_using_value_t v[] = { S("ciao") };
        EXPECT("&", v, 1, "ciao", "& = stringa intera");
    }
    {
        gmb_using_value_t v[] = { S("ciao") };
        EXPECT("!", v, 1, "c", "! = primo carattere");
    }
    {
        gmb_using_value_t v[] = { S("abcdef") };
        EXPECT("\\ \\", v, 1, "abc", "\\ \\ = tre caratteri (tronca)");
    }
    {
        gmb_using_value_t v[] = { S("ab") };
        EXPECT("\\   \\", v, 1, "ab   ", "\\   \\ = cinque caratteri (riempie di spazi)");
    }
    {
        gmb_using_value_t v[] = { S("") };
        EXPECT("!", v, 1, " ", "! su stringa vuota: spazio");
    }

    /* ---- 8. numeri e stringhe insieme ---- */
    {
        gmb_using_value_t v[] = { S("Ana"), N(7) };
        EXPECT("Nome: & (###)", v, 2, "Nome: Ana (  7)", "stringa e numero nello stesso formato");
    }

    /* ---- 9. ripetizione della riga di formato ---- */
    {
        gmb_using_value_t v[] = { N(1), N(2) };
        EXPECT("###", v, 2, "  1\n  2", "i campi finiti, il formato riparte a capo");
    }
    {
        gmb_using_value_t v[] = { N(1), N(2) };
        EXPECT("[###]", v, 2, "[  1]\n[  2]", "la ripetizione ricomincia dall'inizio del formato");
    }

    /* ---- 10. errori espliciti, mai stampe silenziose ---- */
    {
        gmb_using_value_t v[] = { N(1.5) };
        EXPECT_ERROR("^^^^", v, 1, 0, "campo scientifico ^^^^ rifiutato");
    }
    {
        gmb_using_value_t v[] = { S("x") };
        EXPECT_ERROR("###", v, 1, 0, "stringa contro campo numerico: errore");
    }
    {
        gmb_using_value_t v[] = { N(1) };
        EXPECT_ERROR("&", v, 1, 0, "numero contro campo stringa: errore");
    }
    {
        gmb_using_value_t v[] = { N(1), S("x") };
        EXPECT_ERROR("###", v, 2, 0, "secondo valore di tipo diverso: errore");
    }
    {
        gmb_using_value_t v[] = { S("ciao") };
        EXPECT_ERROR("&", v, 1, 4, "buffer di uscita troppo piccolo: errore");
    }

    printf("P7-PRINT-USING ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
