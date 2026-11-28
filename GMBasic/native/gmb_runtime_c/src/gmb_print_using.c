/* gmb_print_using.c — formattatore di PRINT USING (campi GW-BASIC, §0bis: una sola sede).
 *
 * Perché un file a sé e non `gmb_format.c`: `FORMAT` è il formattatore .NET (una funzione,
 * un formato, un valore), questo è un formattatore a CAMPI con stato (più valori, ripetizione
 * per riga, campi numerici/stringa). Sono due domini di formattazione diversi.
 *
 * Tutte le scelte di comportamento sono quelle dichiarate in include/gmb_print_using.h: qui
 * non c'è niente di implicito — ogni caso non rappresentabile ritorna -1 con un messaggio.
 */
#include "gmb_print_using.h"
#include "gmb_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PU_MAX_DECIMALS 9

enum { PU_NOT_FIELD = 0, PU_ERROR = -1 };

typedef struct {
    int is_string;
    int width;        /* caratteri consumati nel formato (= larghezza del campo) */
    /* numerico */
    int digits_i;
    int digits_d;
    int has_dot;
    int commas;
    int dollar;
    int stars;
    int plus_before;
    int plus_after;
    int minus_after;
    /* stringa */
    int skind;        /* 0 = '&', 1 = '!', 2 = '\...\' */
    int s_width;      /* larghezza per '\...\' */
} pu_field_t;

static char g_pu_err[160];

static void pu_error(const char* msg)
{
    snprintf(g_pu_err, sizeof(g_pu_err), "%s", msg);
}

const char* gmb_print_using_error(void) { return g_pu_err; }

/* Riconosce un campo che parte in `fmt[i]`. Ritorna i caratteri consumati (>0), 0 se lì non
 * comincia un campo, -1 se il campo c'è ma non è supportato (errore esplicito). */
static int pu_parse_field(const char* fmt, int i, pu_field_t* f)
{
    int j = i, digits = 0, commas = 0;
    memset(f, 0, sizeof(*f));

    if (fmt[j] == '&') { f->is_string = 1; f->skind = 0; f->width = 1; f->s_width = -1; return 1; }
    if (fmt[j] == '!') { f->is_string = 1; f->skind = 1; f->width = 1; f->s_width = 1; return 1; }
    if (fmt[j] == '\\')
    {
        int k = j + 1;
        while (fmt[k] && fmt[k] != '\\') k++;
        if (fmt[k] == '\\')
        {
            f->is_string = 1;
            f->skind = 2;
            f->width = k - j + 1;
            f->s_width = (k - j - 1) + 2;
            return f->width;
        }
        return PU_NOT_FIELD;   /* '\' non chiuso: è testo */
    }
    if (fmt[j] == '^')
    {
        pu_error("campo ^^^^ (scientifico) non supportato: usa FORMAT");
        return PU_ERROR;
    }

    if (fmt[j] == '*' && fmt[j + 1] == '*') { f->stars = 1; j += 2; }
    if (fmt[j] == '$')
    {
        if (fmt[j + 1] == '$') { f->dollar = 1; j += 2; }
        else if (f->stars) { f->dollar = 1; j += 1; }   /* **$ è il riempimento con dollaro */
        /* un '$' singolo senza ** è testo letterale */
    }
    if (fmt[j] == '+') { f->plus_before = 1; j++; }
    while (fmt[j] == '#' || fmt[j] == ',')
    {
        if (fmt[j] == '#') digits++;
        else commas = 1;
        j++;
    }
    if (digits == 0) return PU_NOT_FIELD;   /* modificatori senza cifre: testo letterale */
    f->digits_i = digits;
    f->commas = commas;
    if (fmt[j] == '.')
    {
        f->has_dot = 1;
        j++;
        while (fmt[j] == '#') { f->digits_d++; j++; }
    }
    if (fmt[j] == '+') { f->plus_after = 1; j++; }
    else if (fmt[j] == '-') { f->minus_after = 1; j++; }
    if (fmt[j] == '^')
    {
        pu_error("campo ^^^^ (scientifico) non supportato: usa FORMAT");
        return PU_ERROR;
    }
    f->width = j - i;
    return f->width;
}

/* Cifre decimali già arrotondate, senza segno: `ip` intero, `frac` con zeri iniziali. */
static int pu_split_number(double value, int decimals, unsigned long long* ip,
                           unsigned long long* frac)
{
    double av = fabs(value);
    double scale = 1.0;
    double scaled;
    for (int k = 0; k < decimals; k++) scale *= 10.0;
    scaled = av * scale;
    if (!(scaled >= 0.0) || scaled > 1.0e15) return -1;   /* NaN/inf o numero troppo grande */
    {
        unsigned long long total = (unsigned long long)(scaled + 0.5);
        unsigned long long s = (unsigned long long)scale;
        *ip = s ? total / s : total;
        *frac = s ? total % s : 0;
    }
    return 0;
}

/* Rende un campo numerico in `buf` (cap): numero di caratteri scritti, -1 se non ci sta. */
static int pu_render_number(const pu_field_t* f, double value, char* buf, int cap)
{
    unsigned long long ip = 0, frac = 0;
    char digits[64], grouped[96], content[128];
    int n = 0, dn = 0;
    int negative = (value < 0.0);
    int trailing = (f->plus_after || f->minus_after) ? 1 : 0;

    if (f->digits_d > PU_MAX_DECIMALS) { pu_error("troppe cifre decimali nel campo"); return -1; }
    if (pu_split_number(value, f->digits_d, &ip, &frac) != 0)
    {
        pu_error("numero fuori dall'intervallo del campo");
        return -1;
    }
    snprintf(digits, sizeof(digits), "%llu", ip);
    if (f->commas)
    {
        int len = (int)strlen(digits);
        for (int k = 0; k < len; k++)
        {
            if (k > 0 && ((len - k) % 3) == 0) grouped[dn++] = ',';
            grouped[dn++] = digits[k];
        }
        grouped[dn] = 0;
    }
    else
    {
        memcpy(grouped, digits, strlen(digits) + 1);
        dn = (int)strlen(digits);
    }

    /* segno + dollaro flottante + cifre (+ decimali); con un segno FINALE ('###+', '###-')
       il segno è stampato in coda, quindi qui non si ripete */
    if (!trailing)
    {
        if (negative) content[n++] = '-';
        else if (f->plus_before) content[n++] = '+';
    }
    if (f->dollar) content[n++] = '$';
    memcpy(content + n, grouped, (size_t)dn);
    n += dn;
    if (f->has_dot)
    {
        content[n++] = '.';
        if (f->digits_d > 0)
        {
            char fracbuf[16];
            snprintf(fracbuf, sizeof(fracbuf), "%0*llu", f->digits_d, frac);
            for (int k = 0; fracbuf[k] && n < (int)sizeof(content) - 2; k++) content[n++] = fracbuf[k];
        }
    }

    if (trailing)
    {
        content[n++] = f->plus_after ? (negative ? '-' : '+') : (negative ? '-' : ' ');
    }
    content[n] = 0;

    /* larghezza del campo; se non basta, overflow dichiarato con '%' in testa */
    if (n > f->width)
    {
        {
            char tmp[132];
            tmp[0] = '%';
            memcpy(tmp + 1, content, (size_t)n + 1);
            n++;
            memcpy(content, tmp, (size_t)n + 1);
        }
    }
    else
    {
        int pad = f->width - n;
        char filled[132];
        int k = 0;
        for (int p = 0; p < pad; p++) filled[k++] = f->stars ? '*' : ' ';
        memcpy(filled + k, content, (size_t)n + 1);
        memcpy(content, filled, (size_t)(k + n) + 1);
        n += pad;
    }
    if (n + 1 > cap) { pu_error("buffer di uscita troppo piccolo"); return -1; }
    memcpy(buf, content, (size_t)n + 1);
    return n;
}

/* Rende un campo stringa in `buf`: caratteri scritti, -1 se non ci sta. */
static int pu_render_string(const pu_field_t* f, const char* str, char* buf, int cap)
{
    const char* s = str ? str : "";
    int len = (int)strlen(s);
    int n;
    if (f->skind == 0) n = len;
    else if (f->skind == 1) n = 1;          /* '!' = un carattere, spazio se la stringa è vuota */
    else n = f->s_width;
    if (n + 1 > cap) { pu_error("buffer di uscita troppo piccolo"); return -1; }
    if (f->skind == 0)
    {
        memcpy(buf, s, (size_t)n);
    }
    else if (f->skind == 1)
    {
        buf[0] = len > 0 ? s[0] : ' ';
    }
    else
    {
        int copy = len < n ? len : n;
        memcpy(buf, s, (size_t)copy);
        for (int k = copy; k < n; k++) buf[k] = ' ';
    }
    buf[n] = 0;
    return n;
}

int gmb_print_using_format(const char* format, const gmb_using_value_t* values, int n,
                           char* out, int cap)
{
    int o = 0, pos = 0;
    pu_error("");
    if (!format || !out || cap <= 0) { pu_error("argomenti non validi"); return -1; }
    if (n < 0 || (n > 0 && !values)) { pu_error("lista valori non valida"); return -1; }

#define PU_PUT(ch)                                                        \
    do {                                                                  \
        if (o + 2 > cap) { pu_error("buffer di uscita troppo piccolo"); return -1; } \
        out[o++] = (char)(ch);                                            \
    } while (0)

    if (n == 0)
    {
        /* nessun valore: il formato è tutto testo (i campi restano letterali), ma l'escape
         * `_` vale anche qui: senza, `PRINT USING "[_#]"` stampava il carattere di escape
         * invece del carattere protetto (trovato dal MiniTest del linguaggio). */
        int i = 0;
        while (format[i])
        {
            if (format[i] == '_' && format[i + 1]) { PU_PUT(format[i + 1]); i += 2; continue; }
            PU_PUT(format[i]);
            i++;
        }
        out[o] = 0;
        return o;
    }

    for (int vi = 0; vi < n; vi++)
    {
        int want_string = values[vi].is_string ? 1 : 0;
        int placed = 0, restarted = 0;
        while (!placed)
        {
            pu_field_t f;
            int adv;
            if (!format[pos])
            {
                if (restarted) { pu_error("nessun campo del tipo del valore"); return -1; }
                PU_PUT('\n');
                pos = 0;
                restarted = 1;
                continue;
            }
            if (format[pos] == '_' && format[pos + 1])
            {
                PU_PUT(format[pos + 1]);
                pos += 2;
                continue;
            }
            adv = pu_parse_field(format, pos, &f);
            if (adv == PU_ERROR) return -1;
            if (adv == PU_NOT_FIELD)
            {
                PU_PUT(format[pos]);
                pos++;
                continue;
            }
            if (f.is_string != want_string)
            {
                pu_error("tipo del valore non coerente col campo del formato");
                return -1;
            }
            {
                char piece[192];
                int pn = want_string
                    ? pu_render_string(&f, values[vi].str, piece, (int)sizeof(piece))
                    : pu_render_number(&f, values[vi].num, piece, (int)sizeof(piece));
                if (pn < 0) return -1;
                for (int k = 0; k < pn; k++) PU_PUT(piece[k]);
            }
            pos += adv;
            placed = 1;
        }
    }

    /* coda letterale dopo l'ultimo valore */
    while (format[pos])
    {
        pu_field_t f;
        int adv;
        if (format[pos] == '_' && format[pos + 1]) { PU_PUT(format[pos + 1]); pos += 2; continue; }
        adv = pu_parse_field(format, pos, &f);
        if (adv == PU_ERROR) return -1;
        if (adv > 0) break;   /* campo senza valore: ci si ferma, non si inventa nulla */
        PU_PUT(format[pos]);
        pos++;
    }
    out[o] = 0;
    return o;
#undef PU_PUT
}

/* ---- innesto nel linguaggio: `PRINT USING <fmt>; v1; v2 …` (contratto in gmb_print_using.h) ----
 *
 * L'emitter C99 emette UNA chiamata a questa funzione: niente logica di formato nel codice
 * generato e niente tabella di tipi duplicata per gamba (§0bis). La capienza del buffer è
 * calcolata dai valori reali (formato + 32 byte per valore numerico + la lunghezza delle
 * stringhe), quindi `&` non può troncare e l'unico errore possibile è quello di formato. */
int gmb_print_using_stmt(gmb_string_t fmt, const gmb_val_t* vals, int n, gmb_string_t* out)
{
    gmb_using_value_t local[8];
    gmb_using_value_t* uv = local;
    const char* f = fmt.data ? fmt.data : "";
    char* buf;
    size_t cap;
    int i, len;

    pu_error("");
    if (!out || n < 0 || (n > 0 && !vals)) { pu_error("argomenti non validi"); return -1; }
    if (n > (int)(sizeof(local) / sizeof(local[0])))
    {
        uv = (gmb_using_value_t*)malloc((size_t)n * sizeof(*uv));
        if (!uv) { pu_error("memoria esaurita"); return -1; }
    }

    cap = strlen(f) + 2;
    for (i = 0; i < n; i++)
    {
        if (vals[i].kind == GMB_TYPE_STRING)
        {
            const char* s = vals[i].v_string.data ? vals[i].v_string.data : "";
            uv[i].is_string = 1;
            uv[i].num = 0.0;
            uv[i].str = s;
            cap += strlen(s);
        }
        else
        {
            uv[i].is_string = 0;
            uv[i].num = gmb_val_to_double(vals[i]);
            uv[i].str = NULL;
        }
        cap += 32;   /* segno, cifre, separatori, dollaro/asterischi: abbondante */
    }
    if (cap > (size_t)1 << 24) { if (uv != local) free(uv); pu_error("formato troppo grande"); return -1; }

    buf = (char*)malloc(cap);
    if (!buf) { if (uv != local) free(uv); pu_error("memoria esaurita"); return -1; }
    len = gmb_print_using_format(f, uv, n, buf, (int)cap);
    if (uv != local) free(uv);
    if (len < 0) { free(buf); return -1; }
    *out = gmb_string_from_bytes(buf, (gmb_int_t)len);
    free(buf);
    return 0;
}
