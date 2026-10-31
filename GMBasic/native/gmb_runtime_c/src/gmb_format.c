/* ============================================================================
 * gmb_format.c — FORMAT (BuiltInFunction.FORMAT del C#), emulazione .NET.
 * Supporta: custom numeric format (0/#/./,/percent/scientifico/sezioni;literal)
 *           formati data/ora (yyyy/MM/dd/dddd/HH/mm/ss/tt/...)
 *           fallback standard numeric .NET (G/F/E/N/D/X...).
 * Usata sia dalla VM/C sia dal codice transpilato (emitter C# → gmb_format).
 * ============================================================================ */

#include "gmb_runtime.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static int gmb_fmt_is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static void gmb_fmt_civil_from_days(long z, int* y, int* m, int* d) {
    z += 719468;
    int era = (int)((z >= 0 ? z : z - 146096) / 146097);
    unsigned doe = (unsigned)(z - (long)era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int yy = (int)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    *d = (int)(doy - (153*mp+2)/5 + 1);
    *m = (int)(mp + (mp < 10 ? 3 : -9));
    *y = yy + (*m <= 2);
}
/* DOW 1=dom..7=sab; 1970-01-01 gio (days%7=0 → 5). */
static int gmb_fmt_dow(long days) {
    int w = (int)(days % 7); if (w < 0) w += 7;
    static const int dow[] = {5,6,7,1,2,3,4};
    return dow[w];
}
static const char* gmb_fmt_month_names[] = {"January","February","March","April","May","June",
    "July","August","September","October","November","December"};
static const char* gmb_fmt_dow_names[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

/* --- OADate → y/m/d e frazione di giorno --- */
static void gmb_oa_to_ymd(double oa, int* y, int* m, int* d) {
    long days = (long)floor(oa);
    gmb_fmt_civil_from_days(days - 25569, y, m, d); /* OADate 0 = 1899-12-30 = days -25569 da 1970 */
}
static void gmb_oa_to_hms(double oa, int* h, int* mi, int* s) {
    double frac = oa - floor(oa);
    long tot = (long)llround(frac * 86400.0);
    if (tot < 0) tot = 0;
    if (tot >= 86400) tot = 86399;
    *h = (int)(tot / 3600);
    *mi = (int)((tot % 3600) / 60);
    *s = (int)(tot % 60);
}

/* --- Data format .NET (yyyy/yy/MMM/MMMM/MM/M/ddd/dddd/dd/d/HH/H/hh/h/mm/m/nn/n/ss/s/tt/t/AM/PM) — token case-sensitive, nomi in cultura invariante --- */
static char* gmb_fmt_datetime(double oa, const char* fmt) {
    int y, mo, d, h, mi, s;
    gmb_oa_to_ymd(oa, &y, &mo, &d);
    gmb_oa_to_hms(oa, &h, &mi, &s);
    int dow = gmb_fmt_dow((long)floor(oa) - 25569);
    char* out = (char*)malloc(512);
    if (!out) return NULL;
    out[0] = 0;
    size_t oi = 0;
    size_t fl = strlen(fmt);
    for (size_t i = 0; i < fl && oi < 500; ) {
        char c = fmt[i];
        /* NB: ogni ramo mantiene `out` terminato (out[oi] = 0): se il formato FINISCE con un letterale/escape l'ultimo
         * passaggio non e' il ramo dei token, e senza terminatore il risultato conteneva memoria non inizializzata. */
        if (c == '\\' && i + 1 < fl) { out[oi++] = fmt[i+1]; out[oi] = 0; i += 2; continue; }
        if (c == '\'' || c == '"') {
            char q = c; i++;
            while (i < fl) {
                if (fmt[i] == q) {
                    if (i + 1 < fl && fmt[i + 1] == q) { if (oi < 500) out[oi++] = q; i += 2; continue; }   /* '' dentro il letterale = apostrofo (canone) */
                    break;
                }
                if (oi < 500) out[oi++] = fmt[i];   /* letterale piu' lungo del buffer: tronca, ma consuma fino alla chiusura */
                i++;
            }
            out[oi] = 0;
            if (i < fl) i++;
            continue;
        }
        if ((c == 'A' || c == 'a') && i + 5 <= fl && strncasecmp(fmt + i, "AM/PM", 5) == 0) {   /* designatore AM/PM (canone StringsFormat) */
            const char* ap = (h < 12) ? "AM" : "PM";
            if (oi + 2 < 510) { out[oi++] = ap[0]; out[oi++] = ap[1]; out[oi] = 0; }
            i += 5; continue;
        }
        size_t run = 1;
        while (i + run < fl && fmt[i + run] == c) run++;
        char tmp[64];
        if (c == 'y') {
            if (run >= 4) snprintf(tmp, sizeof(tmp), "%04d", y);
            else if (run >= 2) snprintf(tmp, sizeof(tmp), "%02d", y % 100);
            else snprintf(tmp, sizeof(tmp), "%d", y % 10);
        }
        else if (c == 'M') {
            if (run >= 4) snprintf(tmp, sizeof(tmp), "%s", gmb_fmt_month_names[mo-1]);
            else if (run == 3) snprintf(tmp, sizeof(tmp), "%.3s", gmb_fmt_month_names[mo-1]);
            else if (run == 2) snprintf(tmp, sizeof(tmp), "%02d", mo);
            else snprintf(tmp, sizeof(tmp), "%d", mo);
        }
        else if (c == 'd') {
            if (run >= 4) snprintf(tmp, sizeof(tmp), "%s", gmb_fmt_dow_names[dow-1]);
            else if (run == 3) snprintf(tmp, sizeof(tmp), "%.3s", gmb_fmt_dow_names[dow-1]);
            else if (run == 2) snprintf(tmp, sizeof(tmp), "%02d", d);
            else snprintf(tmp, sizeof(tmp), "%d", d);
        }
        else if (c == 'H') { if (run == 1) snprintf(tmp, sizeof(tmp), "%d", h); else snprintf(tmp, sizeof(tmp), "%02d", h); }
        else if (c == 'h') { int h12 = h % 12; if (h12 == 0) h12 = 12; if (run == 1) snprintf(tmp, sizeof(tmp), "%d", h12); else snprintf(tmp, sizeof(tmp), "%02d", h12); }
        else if (c == 'm' || c == 'n') { if (run == 1) snprintf(tmp, sizeof(tmp), "%d", mi); else snprintf(tmp, sizeof(tmp), "%02d", mi); }
        else if (c == 's') { if (run == 1) snprintf(tmp, sizeof(tmp), "%d", s); else snprintf(tmp, sizeof(tmp), "%02d", s); }
        else if (c == 't') { if (run >= 2) snprintf(tmp, sizeof(tmp), "%s", (h < 12) ? "AM" : "PM"); else snprintf(tmp, sizeof(tmp), "%s", (h < 12) ? "A" : "P"); }
        else { snprintf(tmp, sizeof(tmp), "%c", c); run = 1; }
        size_t tl = strlen(tmp);
        if (oi + tl < 510) { memcpy(out + oi, tmp, tl); oi += tl; out[oi] = 0; }
        i += run;
    }
    out[oi] = 0;
    return out;
}

/* --- Round half away from zero --- */
static double gmb_fmt_round(double v, int dec) {
    double p = pow(10.0, dec);
    return floor(v * p + 0.5) / p;
}

/* --- Formatta la parte intera con gruppi da 3 (separatore ',') --- */
/* total_width: larghezza totale del mask intero (conta '0'+'#', senza virgole) — i
 * placeholder '#' NON riempiti da una cifra reale né dallo zero-pad (zero_pad, conta solo
 * '0') si riempiono con uno SPAZIO invece di essere omessi (parità PCode
 * FormatUtils.FormatNumberWithMask: '#' = cifra opzionale — spazio se assente, a
 * differenza di '0' che è sempre zero-riempito). Prima gmb_fmt_custom_section contava
 * SOLO i caratteri '0' del mask (ignorando '#' del tutto) e questa funzione non
 * applicava mai un padding a spazi: FORMAT(255, "#,##0.00") dava "255.00" invece di
 * " 255.00" — non uno scenario CURRENCY-specifico (Bugs/Arrays-Sweep-Residui-Minori #1:
 * inizialmente frainteso come "solo CURRENCY POSITIVI", in realtà QUALSIASI numero con
 * meno cifre del totale placeholder '#'+'0' del mask, qualunque tipo). Il padding a
 * spazi va calcolato PRIMA di inserire le virgole di raggruppamento (total_width non le
 * conta), altrimenti "remaining % 3" del raggruppamento sballerebbe includendo gli spazi. */
static void gmb_fmt_intpart(char* out, long long val, int zero_pad, int use_sep, int total_width) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", val < 0 ? -val : val);
    size_t len = strlen(buf);
    /* zero-pad */
    if (zero_pad > (int)len) {
        char tmp[64];
        memset(tmp, '0', (size_t)zero_pad - len);
        tmp[zero_pad - len] = 0;
        strcat(tmp, buf);
        strcpy(buf, tmp);
        len = strlen(buf);
    }
    int pad = total_width - (int)len;
    if (pad < 0) pad = 0;
    char grouped[80];
    size_t o = 0;
    if (use_sep) {
        for (size_t i = 0; i < len; i++) {
            size_t remaining = len - i;
            if (i > 0 && remaining % 3 == 0) grouped[o++] = ',';
            grouped[o++] = buf[i];
        }
    } else {
        memcpy(grouped, buf, len); o = len;
    }
    grouped[o] = 0;
    memset(out, ' ', (size_t)pad);
    strcpy(out + pad, grouped);
}

/* --- Custom numeric format .NET (singola sezione, senza ';') --- */
static char* gmb_fmt_custom_section(double value, const char* sec) {
    /* rileva scientifico E/e (non parte di stringa literal) */
    int has_sci = 0; size_t sci_pos = 0;
    int in_q = 0; char qc = 0;
    for (size_t i = 0; sec[i]; i++) {
        char c = sec[i];
        if (c == '\\') { i++; continue; }
        if (c == '"' || c == '\'') { if (!in_q) { in_q = 1; qc = c; } else if (qc == c) in_q = 0; continue; }
        if (!in_q && (c == 'E' || c == 'e') && (sec[i+1] == '+' || sec[i+1] == '-' || (sec[i+1] >= '0' && sec[i+1] <= '9'))) { has_sci = 1; sci_pos = i; break; }
    }

    if (has_sci) {
        /* mantissa sec[0..sci_pos) ; esponente sec[sci_pos+1..) */
        char* mant = (char*)malloc(sci_pos + 2);
        if (!mant) return NULL;
        memcpy(mant, sec, sci_pos); mant[sci_pos] = 0;
        char* exp_part = strdup(sec + sci_pos + 1);
        /* esponente minimo = numero di 0 nel exp_part */
        int exp_min = 0;
        for (char* p = exp_part; *p; p++) if (*p == '0') exp_min++;
        int sign = (sec[sci_pos + 1] == '-' || sec[sci_pos + 1] == '+') ? 1 : 0;
        /* calcola esponente */
        double av = fabs(value);
        int e10 = 0;
        if (av != 0) e10 = (int)floor(log10(av));
        double mantissa = av / pow(10.0, e10);
        /* round mantissa ai decimali del mant */
        const char* md = strchr(mant, '.');
        int mdec = md ? (int)strlen(md + 1) : 0;
        mantissa = gmb_fmt_round(mantissa, mdec);
        if (mantissa >= 10.0) { mantissa /= 10.0; e10++; }
        /* formatta mantissa */
        char* ms = gmb_fmt_custom_section(value < 0 ? -mantissa : mantissa, mant);
        if (!ms) { free(mant); free(exp_part); return NULL; }
        /* esponente: "E+" → sempre segno, "E-" → segno solo se negativo */
        char* res = (char*)malloc(128);
        if (!res) { free(ms); free(mant); free(exp_part); return NULL; }
        if (sec[sci_pos + 1] == '-') {
            snprintf(res, 128, "%sE%s%0*d", ms, e10 < 0 ? "-" : "", exp_min, abs(e10));
        } else {
            snprintf(res, 128, "%sE%c%0*d", ms, e10 < 0 ? '-' : '+', exp_min, abs(e10));
        }
        free(ms); free(mant); free(exp_part);
        return res;
    }

    /* percentuale nella sezione → value *= 100 e '%' in output */
    int has_pct = 0;
    for (const char* p = sec; *p; p++) if (*p == '%') { has_pct = 1; break; }
    double v = has_pct ? value * 100.0 : value;
    int neg = v < 0;
    double av = neg ? -v : v;

    /* conta placeholder interi (0/#) prima del punto */
    const char* dot = NULL;
    int in_q2 = 0; char qc2 = 0;
    for (const char* p = sec; *p; p++) {
        char c = *p;
        if (c == '\\') { p++; continue; }
        if (c == '"' || c == '\'') { if (!in_q2) { in_q2 = 1; qc2 = c; } else if (qc2 == c) in_q2 = 0; continue; }
        if (!in_q2 && c == '.') { dot = p; break; }
    }
    const char* int_part = sec;
    const char* dec_part = dot ? dot + 1 : NULL;
    int nint = 0, nhash = 0, ndec = 0;
    /* Conteggio placeholder '0'/'#' escape/quote-aware (stessa logica del loop che
     * cerca `dot` sopra) — senza, un '#' o '0' LETTERALE (escapato con '\' o dentro
     * "..."/'...') veniva contato come un vero placeholder: mask "Numero \##0" (solo
     * "#0" è il vero mask, "\#" è un carattere letterale "#") calcolava nhash=2 invece
     * di 1, aggiungendo uno spazio di padding spurio ("Numero # 42" invece di
     * "Numero #42"). */
    { int inq = 0; char qch = 0;
      for (const char* p = int_part; p && *p && (dot == NULL || p < dot); p++) {
          char c = *p;
          if (c == '\\' && p[1]) { p++; continue; }
          if (c == '"' || c == '\'') { if (!inq) { inq = 1; qch = c; } else if (qch == c) inq = 0; continue; }
          if (inq) continue;
          if (c == '0') nint++;
          else if (c == '#') nhash++;
      }
    }
    if (dec_part) {
        int inq = 0; char qch = 0;
        for (const char* p = dec_part; *p; p++) {
            char c = *p;
            if (c == '\\' && p[1]) { p++; continue; }
            if (c == '"' || c == '\'') { if (!inq) { inq = 1; qch = c; } else if (qch == c) inq = 0; continue; }
            if (inq) continue;
            if (c == '0' || c == '#') ndec++;
        }
    }

    /* separatori: c'è una virgola nella parte intera? */
    int use_sep = 0;
    for (const char* p = int_part; p && *p && (dot == NULL || p < dot); p++) if (*p == ',') { use_sep = 1; break; }

    /* arrotonda e spezza */
    av = gmb_fmt_round(av, ndec);
    long long ip = (long long)floor(av);
    double dp = av - floor(av);
    long long frac = (long long)llround(dp * pow(10.0, ndec));
    if (frac >= (long long)pow(10.0, ndec)) { ip++; frac = 0; }

    char* res = (char*)malloc(256);
    if (!res) return NULL;
    res[0] = 0;
    {
        char ip_fmt[64], dp_digits[64] = "";
        /* intera: tutte le cifre con separatori (gruppi 3), padding minimo = '0' interi,
         * padding a spazi fino a nint+nhash per i placeholder '#' non riempiti. */
        gmb_fmt_intpart(ip_fmt, ip, nint, use_sep, nint + nhash);
        if (ndec > 0) {
            char fmt2[32];
            snprintf(fmt2, sizeof(fmt2), "%%0%dlld", ndec);
            snprintf(dp_digits, sizeof(dp_digits), fmt2, frac);
        }
        size_t dpi = 0;
        size_t dpn = strlen(dp_digits);
        in_q = 0; qc = 0;
        int in_dec = 0;
        int copied_int = 0;
        size_t ins = 0;
        /* segno negativo automatico solo se il pattern non contiene già '-' letterale */
        int has_minus = (strchr(sec, '-') != NULL);
        if (neg && !has_minus) res[ins++] = '-';
        for (const char* p = sec; *p && ins < 250; p++) {
            char c = *p;
            if (c == '\\' && p[1]) { res[ins++] = p[1]; p++; continue; }
            if (c == '"' || c == '\'') { if (!in_q) { in_q = 1; qc = c; } else if (qc == c) in_q = 0; continue; }
            if (in_q) { res[ins++] = c; continue; }
            if (c == '0' || c == '#') {
                if (in_dec) {
                    if (dpi < dpn) { res[ins++] = dp_digits[dpi++]; continue; }
                    continue;
                }
                if (!copied_int) {
                    /* copia l'intera parte intera formattata — per un valore NEGATIVO,
                     * salta gli spazi di padding iniziali di ip_fmt (parità PCode
                     * FormatNumberWithMask: `if (number &lt; 0) return "-" +
                     * finalString.TrimStart();` — il trim dipende SOLO dal segno del
                     * VALORE, mai da come/se il mask mostra il "-": sia quando il "-" è
                     * auto-prepended sopra (mask senza '-' letterale) sia quando il "-" è
                     * già letterale nella sezione "neg" del rewrite pos;neg;pos di un
                     * mask con '+', vedi gmb_format() sopra — senza, "-299.71" diventava
                     * "- 299.71" e "-5,678.99" diventava "- 5,678.99"). */
                    const char* q = ip_fmt;
                    if (neg) { while (*q == ' ') q++; }
                    for (; *q; q++) { if (ins < 250) res[ins++] = *q; }
                    copied_int = 1;
                }
                continue;
            }
            if (c == '.') { in_dec = 1; if (ndec > 0) res[ins++] = '.'; continue; }
            if (c == ',') { if (!copied_int) { res[ins++] = ','; } continue; }
            if (c == '%') { res[ins++] = '%'; continue; }
            res[ins++] = c;
        }
        res[ins] = 0;
    }
    return res;
}

/* --- split in sezioni ; (rispettando string literal ed escape) --- */
static int gmb_fmt_split_sections(const char* fmt, char* secs[3]) {
    int n = 0;
    char buf[256];
    size_t b = 0;
    int in_q = 0; char qc = 0;
    int saw_semi = 0;
    for (const char* p = fmt; *p; p++) {
        char c = *p;
        if (c == '\\' && p[1]) { if (b < 255) buf[b++] = c; if (b < 255) buf[b++] = p[1]; p++; continue; }
        if (c == '"' || c == '\'') {
            if (!in_q) { in_q = 1; qc = c; } else if (qc == c) in_q = 0;
            if (b < 255) buf[b++] = c; continue;
        }
        if (c == ';' && !in_q) {
            if (n < 3) { buf[b] = 0; secs[n] = strdup(buf); n++; b = 0; }
            saw_semi = 1;
            continue;
        }
        saw_semi = 0;
        if (b < 255) buf[b++] = c;
    }
    /* se finisce con ';' o c'è contenuto, aggiungi l'ultima sezione (anche vuota) */
    if (n < 3 && (b > 0 || saw_semi || n == 0)) { buf[b] = 0; secs[n] = strdup(buf); n++; }
    return n;
}

/* --- se una sezione contiene placeholder numerici --- */
static int gmb_fmt_has_num(const char* s) {
    int in_q = 0; char qc = 0;
    for (const char* p = s; *p; p++) {
        char c = *p;
        if (c == '\\') { p++; continue; }
        if (c == '"' || c == '\'') { if (!in_q) { in_q = 1; qc = c; } else if (qc == c) in_q = 0; continue; }
        if (!in_q) {
            if (c == '0' || c == '#' || c == '.' || c == ',') return 1;
            if (c == 'E' || c == 'e') {
                if (p[1] == '+' || p[1] == '-' || (p[1] >= '0' && p[1] <= '9')) return 1;
            }
            if (c == '%') return 1;
        }
    }
    return 0;
}

/* --- strip string literal quotes da una sezione testuale --- */
static char* gmb_fmt_plain(const char* s) {
    char* out = (char*)malloc(strlen(s) + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (const char* p = s; *p; p++) {
        if (*p == '\\' && p[1]) { out[o++] = p[1]; p++; continue; }
        if (*p == '"' || *p == '\'') continue;
        out[o++] = *p;
    }
    out[o] = 0;
    return out;
}

/* --- Standard numeric .NET (G/F/E/N/D/X/C/P) --- */
static char* gmb_fmt_standard(double value, const char* fmt) {
    char spec = fmt[0];
    int dec = 0;
    if (fmt[1]) { char* end = NULL; dec = (int)strtol(fmt + 1, &end, 10); if (end == fmt + 1) dec = -1; }
    char buf[128];
    switch (spec) {
        case 'G':
        case 'g':
            if (dec >= 0) { snprintf(buf, sizeof(buf), "%%.%dg", dec); char tmp[128]; snprintf(tmp, sizeof(tmp), buf, value); strcpy(buf, tmp); }
            else snprintf(buf, sizeof(buf), "%g", value);
            break;
        case 'F': case 'f':
            if (dec < 0) dec = 2;
            { snprintf(buf, sizeof(buf), "%%.%df", dec); char tmp[128]; snprintf(tmp, sizeof(tmp), buf, value); strcpy(buf, tmp); }
            break;
        case 'N': case 'n':
            if (dec < 0) dec = 2;
            { double r = gmb_fmt_round(value, dec); long long ip = (long long)floor(fabs(r)); char ipb[64]; gmb_fmt_intpart(ipb, ip, 0, 1, 0); double dp = fabs(r) - floor(fabs(r)); long long frac = (long long)llround(dp * pow(10.0, dec)); if (frac >= (long long)pow(10.0, dec)) { ip++; frac = 0; gmb_fmt_intpart(ipb, ip, 0, 1, 0); } if (dec > 0) snprintf(buf, sizeof(buf), "%s%s.%0*lld", r < 0 ? "-" : "", ipb, dec, frac); else snprintf(buf, sizeof(buf), "%s%s", r < 0 ? "-" : "", ipb); }
            break;
        case 'E': case 'e':
            if (dec < 0) dec = 6;
            { snprintf(buf, sizeof(buf), "%%.%dE", dec); char tmp[128]; snprintf(tmp, sizeof(tmp), buf, value); strcpy(buf, tmp); }
            break;
        case 'D': case 'd': {
            long long iv = (long long)value;
            char ipb[64]; gmb_fmt_intpart(ipb, iv, dec < 0 ? 0 : dec, 0, 0);
            snprintf(buf, sizeof(buf), "%s", ipb);
            break;
        }
        case 'X': case 'x':
            snprintf(buf, sizeof(buf), spec == 'X' ? "%llX" : "%llx", (long long)value);
            if (dec > 0) {
                size_t blen = strlen(buf);
                if ((size_t)dec > blen) {
                    char tmp[128];
                    size_t pad = (size_t)dec - blen;
                    memset(tmp, '0', pad);
                    strcpy(tmp + pad, buf);
                    strcpy(buf, tmp);
                }
            }
            break;
        case 'C': case 'c':
            if (dec < 0) dec = 2;
            { snprintf(buf, sizeof(buf), "%%.%df", dec); char tmp[128]; snprintf(tmp, sizeof(tmp), buf, value); snprintf(buf, sizeof(buf), "$%s", tmp); }
            break;
        case 'P': case 'p':
            if (dec < 0) dec = 2;
            { snprintf(buf, sizeof(buf), "%%.%df%%", dec); char tmp[128]; snprintf(tmp, sizeof(tmp), buf, value * 100.0); snprintf(buf, sizeof(buf), "%s", tmp); }
            break;
        default:
            snprintf(buf, sizeof(buf), "%g", value);
    }
    return strdup(buf);
}

/* --- selezione sezione: numerica → .NET (segno); testuale → booleano (0/non-0) --- */
static int gmb_fmt_pick_section(double value, int n, int numeric) {
    if (numeric) {
        if (value > 0) return 0;
        if (value < 0) return (n >= 2) ? 1 : 0;
        return (n >= 3) ? 2 : 0;
    }
    return (value != 0) ? 0 : 1;
}

/* ============================================================================
 * gmb_format(value, format) — replicazione BuiltInFunction.FORMAT (C#).
 * 1. fmt termina con '%' → (val*100).ToString("F" + dec) + "%"
 * 2. fmt contiene ';' → sezioni (testuali = booleani; numeriche = segno)
 * 3. fmt contiene # o 0 → custom numeric .NET (con fix "+..." → sezioni)
 * 4. fmt contiene token data → data format (value = OADate)
 * 5. altrimenti → val.ToString(fmt) standard .NET
 * ============================================================================ */
gmb_string_t gmb_format(gmb_double_t value, gmb_string_t format) {
    const char* fmt = format.data ? format.data : "";
    if (!fmt[0]) return gmb_double_to_string(value);

    /* 1. percentuale finale */
    size_t fl = strlen(fmt);
    if (fmt[fl - 1] == '%' && !strchr(fmt, ';')) {
        int d = 0;
        const char* dot = strchr(fmt, '.');
        if (dot) d = (int)strlen(dot + 1) - 1; /* toglie il '%' */
        if (d < 0) d = 0;
        char fmtbuf[32], tmp[128];
        snprintf(fmtbuf, sizeof(fmtbuf), "%%.%df", d);
        snprintf(tmp, sizeof(tmp), fmtbuf, value * 100.0);
        char res[144];
        snprintf(res, sizeof(res), "%s%%", tmp);
        return gmb_string_alloc(res);
    }

    /* 2. sezioni ';' (booleani "Yes;No" o custom pos;neg;zero) — PRIMA del data check
       (il C# sbaglia qui catturando "Yes;No" nel ramo data: l'intento è il testo a sezioni) */
    if (strchr(fmt, ';')) {
        char* secs[3] = {NULL, NULL, NULL};
        int n = gmb_fmt_split_sections(fmt, secs);
        int numeric = 0;
        for (int i = 0; i < n; i++) if (gmb_fmt_has_num(secs[i])) { numeric = 1; break; }
        int idx = gmb_fmt_pick_section(value, n, numeric);
        char* result = NULL;
        if (numeric) {
            result = gmb_fmt_custom_section(value, secs[idx]);
        } else {
            result = gmb_fmt_plain(secs[idx]);
        }
        for (int i = 0; i < 3; i++) free(secs[i]);
        gmb_string_t res = gmb_string_alloc(result ? result : "");
        free(result);
        return res;
    }

    /* 3. custom numeric (contiene # o 0) */
    if (strchr(fmt, '#') || strchr(fmt, '0')) {
        /* fix "+##,##0.00" → sezioni pos;neg;pos */
        char work[256];
        snprintf(work, sizeof(work), "%s", fmt);
        if (work[0] == '+' && !strchr(work, ';')) {
            char pos[256], neg[256];
            snprintf(pos, sizeof(pos), "%s", work);
            snprintf(neg, sizeof(neg), "-%s", work + 1);
            snprintf(work, sizeof(work), "%s;%s;%s", pos, neg, pos);
        }
        char* secs[3] = {NULL, NULL, NULL};
        int n = gmb_fmt_split_sections(work, secs);
        int numeric = 0;
        for (int i = 0; i < n; i++) if (gmb_fmt_has_num(secs[i])) { numeric = 1; break; }
        int idx = gmb_fmt_pick_section(value, n, numeric);
        char* result = NULL;
        if (gmb_fmt_has_num(secs[idx])) {
            result = gmb_fmt_custom_section(value, secs[idx]);
        } else {
            result = gmb_fmt_plain(secs[idx]);
        }
        for (int i = 0; i < 3; i++) free(secs[i]);
        gmb_string_t res = gmb_string_alloc(result ? result : "");
        free(result);
        return res;
    }

    /* 4. token data/ora */
    int has_dt = 0;
    int in_q = 0; char qc = 0;
    for (const char* p = fmt; *p; p++) {
        char c = *p;
        if (c == '\\') { p++; continue; }
        if (c == '"' || c == '\'') { if (!in_q) { in_q = 1; qc = c; } else if (qc == c) in_q = 0; continue; }
        if (!in_q && (c == 'y' || c == 'M' || c == 'd' || c == 'H' || c == 'h' || c == 'm' || c == 'n' || c == 's' || c == 't')) { has_dt = 1; break; }
    }
    if (has_dt) {
        char* r = gmb_fmt_datetime(value, fmt);
        gmb_string_t res = gmb_string_alloc(r ? r : "");
        free(r);
        return res;
    }

    /* 5. fallback: standard numeric .NET */
    {
        char* r = gmb_fmt_standard(value, fmt);
        gmb_string_t res = gmb_string_alloc(r ? r : "");
        free(r);
        return res;
    }
}

/* ============================================================================
 * gmb_csv_value — appende UN valore (quotato stile CSV) al buffer in *pos.
 * Stessa logica di vm_val_csv nel VM/C: stringhe quotate con doppie virgolette,
 * numeri via %g, bool come 1/0. Ritorna 0 ok, -1 se il buffer non basta.
 * Usata dall'emitter C# per serializzare istanze UDT in record CSV (APPEND/PUT).
 * ============================================================================ */
int gmb_csv_value(char* out, int cap, int* pos, gmb_val_t v)
{
    int p, n;
    char num[64];
    if (!out || cap <= 0 || !pos || *pos < 0 || *pos >= cap) return -1;
    p = *pos;
    switch (v.kind) {
        case GMB_TYPE_STRING: {
            const char* s = v.v_string.data ? v.v_string.data : "";
            size_t len = v.v_string.length;
            if (p + 2 >= cap) return -1;
            out[p++] = '"';
            for (size_t i = 0; i < len; i++) {
                if (s[i] == '"') { if (p + 2 >= cap) return -1; out[p++] = '"'; }
                if (p + 1 >= cap) return -1;
                out[p++] = s[i];
            }
            if (p + 1 >= cap) return -1;
            out[p++] = '"'; out[p] = 0;
            *pos = p;
            return 0;
        }
        case GMB_TYPE_INT:
            n = snprintf(num, sizeof(num), "%lld", (long long)v.v_int);
            break;
        case GMB_TYPE_DOUBLE:
            n = snprintf(num, sizeof(num), "%g", v.v_double);
            break;
        case GMB_TYPE_BOOL:
            num[0] = v.v_bool ? '1' : '0'; num[1] = 0; n = 1;
            break;
        default:
            num[0] = 0; n = 0;
            break;
    }
    if (p + n >= cap) return -1;
    for (int i = 0; i < n; i++) out[p++] = num[i];
    out[p] = 0;
    *pos = p;
    return 0;
}

/* Estrae l'i-esimo campo CSV (0-based) da una stringa CSV. Gestisce stringhe quotate
 * con doppie virgolette raddoppiate. Ritorna la lunghezza scritta o -1 se fuori range. */
int gmb_csv_field(const char* csv, int idx, char* out, int cap)
{
    if (!csv || !out || cap <= 0) return -1;
    int field = 0, o = 0;
    const char* p = csv;
    for (;;) {
        if (field == idx) {
            if (*p == '"') {
                p++;
                while (*p && o < cap - 1) {
                    if (*p == '"') {
                        if (p[1] == '"') { out[o++] = '"'; p += 2; }
                        else { p++; break; }
                    } else out[o++] = *p++;
                }
            } else {
                while (*p && *p != ',' && o < cap - 1) out[o++] = *p++;
            }
            out[o] = 0;
            return o;
        }
        if (*p == '"') {
            p++;
            while (*p) {
                if (*p == '"') { if (p[1] == '"') p += 2; else { p++; break; } }
                else p++;
            }
        } else {
            while (*p && *p != ',') p++;
        }
        if (!*p) return -1;
        p++;
        field++;
    }
}
