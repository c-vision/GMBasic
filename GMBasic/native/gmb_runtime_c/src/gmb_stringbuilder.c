/* ============================================================================
 * gmb_stringbuilder.c — STRINGBUILDER: builder mutabile di stringhe.
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 *
 * Rappresentazione: handle opaco (gmb_stringbuilder_t*) con un buffer NUL-terminato
 * a capacità variabile. La concatenazione ripetuta (`sb.Append(...)`) è O(1)
 * AMMORTIZZATO grazie a una politica di crescita che raddoppia il buffer finché
 * resta piccolo e cresce poi a blocchi lineari per i buffer grandi (evita spreco
 * di memoria e ricopiazioni continue dell'intera stringa, come farebbe `a = a + b`).
 *
 * Unica sorgente (§0bis): C99Native e VM/C chiamano queste gmb_sb_*.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* Soglia oltre la quale la crescita non raddoppia più ma procede a blocchi lineari:
 * sotto, il raddoppio dà O(1) ammortizzato senza sprecare troppo; sopra, raddoppiare
 * un buffer già grande sprecherebbe memoria, quindi si aggiunge una quota fissa. */
#define GMB_SB_LINEAR_GROW  (1024u * 1024u)   /* 1 MiB */
/* Capacità iniziale del builder senza capacità esplicita: 16 byte, come il
 * StringBuilder() .NET (IL/PCode/VM-C). `sb.Capacity` deve coincidere sulle 4 gambe. */
#define GMB_SB_DEFAULT_CAP  16u

struct gmb_stringbuilder {
    char*  buf;   /* buffer NUL-terminato, allocato cap+1 byte */
    size_t len;   /* byte usati (escluso il NUL) */
    size_t cap;   /* capacità in byte utili (escluso il NUL) */
};

/* Garantisce spazio per almeno `need` byte utili (+ NUL). Ritorna 0 su successo,
 * -1 su OOM/overflow. Il buffer non viene mai condiviso: una sola copia per builder. */
static int gmb_sb_reserve(gmb_stringbuilder_t* sb, size_t need)
{
    char* nb;
    size_t newcap;
    if (!sb) return -1;
    if (need <= sb->cap) return 0;
    if (need > (size_t)-1 - 1) return -1;       /* impossibile allocare need+1 */

    newcap = need;
    if (newcap < GMB_SB_LINEAR_GROW)
    {
        /* RADDOPPIO della capacità corrente, come StringBuilder.EnsureCapacity/ExpandByABlock
         * .NET: `newCapacity = max(required, 2 * old)`. Prima si raddoppiava la RICHIESTA
         * (`need * 2`): dopo un Append che portava 16 → 21 la capacità diventava 42 invece di
         * 32, e `sb.Capacity` divergeva da IL/PCode/VM-C. */
        size_t doubled = sb->cap * 2;
        if (doubled > newcap) newcap = doubled;
    }
    else
        newcap += GMB_SB_LINEAR_GROW;           /* buffer grandi: crescita a blocchi */
    if (newcap < need) newcap = need;           /* guardia overflow del raddoppio */

    nb = (char*)realloc(sb->buf, newcap + 1);   /* +1 per il terminatore */
    if (!nb) return -1;
    sb->buf = nb;
    sb->cap = newcap;
    return 0;
}

gmb_stringbuilder_t* gmb_sb_new(size_t initial_cap)
{
    gmb_stringbuilder_t* sb = (gmb_stringbuilder_t*)calloc(1, sizeof(*sb));
    if (!sb) return NULL;
    if (initial_cap > 0) {
        sb->buf = (char*)malloc(initial_cap + 1);
        if (!sb->buf) { free(sb); return NULL; }
        sb->cap = initial_cap;
    } else {
        /* Capacità iniziale di DEFAULT 16 byte, come il StringBuilder .NET (IL/PCode/VM-C).
         * Prima era 0 e cresceva al primo Append raddoppiando la dimensione richiesta:
         * `DIM sb : sb.Append("abcd")` dava Capacity 8 su C99 e 16 sulle gambe managed. */
        sb->buf = (char*)malloc(GMB_SB_DEFAULT_CAP + 1);
        if (!sb->buf) { free(sb); return NULL; }
        sb->cap = GMB_SB_DEFAULT_CAP;
    }
    sb->buf[0] = '\0';
    sb->len = 0;
    return sb;
}

void gmb_sb_free(gmb_stringbuilder_t* sb)
{
    if (!sb) return;
    free(sb->buf);
    free(sb);
}

void gmb_sb_clear(gmb_stringbuilder_t* sb)
{
    if (!sb || !sb->buf) return;
    sb->len = 0;
    sb->buf[0] = '\0';   /* riusa il buffer già allocato */
}

/* Append di `n` byte (può contenere NUL): unica primitiva di concatenazione. */
int gmb_sb_append_n(gmb_stringbuilder_t* sb, const char* s, size_t n)
{
    if (!sb) return -1;
    if (!s || n == 0) return 0;
    if (gmb_sb_reserve(sb, sb->len + n) != 0) return -1;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
    return 0;
}

int gmb_sb_append(gmb_stringbuilder_t* sb, const char* s)
{
    return s ? gmb_sb_append_n(sb, s, strlen(s)) : 0;
}

int gmb_sb_append_char(gmb_stringbuilder_t* sb, char c)
{
    return gmb_sb_append_n(sb, &c, 1);
}

/* Append + newline (una sola operazione → UNA riscrittura del terminatore). */
int gmb_sb_append_line(gmb_stringbuilder_t* sb, const char* s)
{
    if (s && gmb_sb_append(sb, s) != 0) return -1;
    return gmb_sb_append_n(sb, "\n", 1);
}

int gmb_sb_append_printf(gmb_stringbuilder_t* sb, const char* fmt, ...)
{
    va_list ap, ap2;
    int n;
    if (!sb || !fmt) return -1;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);          /* prima passata: misura la lunghezza */
    va_end(ap);
    if (n < 0) { va_end(ap2); return -1; }

    if (gmb_sb_reserve(sb, sb->len + (size_t)n) != 0) { va_end(ap2); return -1; }
    vsnprintf(sb->buf + sb->len, (size_t)n + 1, fmt, ap2);   /* seconda passata: scrive */
    va_end(ap2);
    sb->len += (size_t)n;
    return 0;
}

int gmb_sb_insert(gmb_stringbuilder_t* sb, size_t pos, const char* s)
{
    size_t n;
    if (!sb || !s) return -1;
    n = strlen(s);
    if (n == 0) return 0;
    if (pos > sb->len) pos = sb->len;             /* clamp oltre la fine */
    if (gmb_sb_reserve(sb, sb->len + n) != 0) return -1;
    /* sposta la coda a destra (incluso il terminatore), poi copia in mezzo */
    memmove(sb->buf + pos + n, sb->buf + pos, sb->len - pos + 1);
    memcpy(sb->buf + pos, s, n);
    sb->len += n;
    return 0;
}

/* Sostituzione di TUTTE le occorrenze: se `to` è più corto il buffer si accorcia,
 * se è più lungo si riserva la differenza in un'unica crescita preventiva. */
int gmb_sb_replace(gmb_stringbuilder_t* sb, const char* from, const char* to)
{
    size_t fl, tl, hits = 0;
    char* p;
    if (!sb || !from || !*from) return -1;
    if (!to) to = "";
    fl = strlen(from);
    tl = strlen(to);

    if (tl > fl) {
        for (p = sb->buf; (p = strstr(p, from)) != NULL; p += fl) hits++;
        if (hits > 0 && gmb_sb_reserve(sb, sb->len + hits * (tl - fl)) != 0) return -1;
    }

    p = sb->buf;
    for (;;) {
        char* hit = strstr(p, from);
        size_t at;
        if (!hit) break;
        at = (size_t)(hit - sb->buf);
        /* memmove tollera overlap (tl può essere < o > fl): la coda scorre */
        memmove(sb->buf + at + tl, sb->buf + at + fl, sb->len - at - fl + 1);
        if (tl > 0) memcpy(sb->buf + at, to, tl);
        sb->len += tl - fl;
        p = sb->buf + at + tl;   /* riparte dopo la sostituzione (no ri-espansione) */
    }
    return 0;
}

int gmb_sb_remove(gmb_stringbuilder_t* sb, size_t pos, size_t n)
{
    if (!sb) return -1;
    if (pos >= sb->len) return 0;
    if (n > sb->len - pos) n = sb->len - pos;    /* clamp */
    memmove(sb->buf + pos, sb->buf + pos + n, sb->len - pos - n + 1);
    sb->len -= n;
    return 0;
}

/* Assegna il contenuto (rimpiazza tutto, riusando il buffer già allocato). */
int gmb_sb_assign(gmb_stringbuilder_t* sb, const char* s)
{
    if (!sb) return -1;
    sb->len = 0;
    if (sb->buf) sb->buf[0] = '\0';
    return gmb_sb_append(sb, s);
}

/* Append di `s` ripetuto `n` volte. Tecnica a RADDOPPIO LOGARITMICO: si copia una
 * volta il seme e poi si replica la porzione GIÀ scritta (1x, 2x, 4x, …) completando
 * col resto → O(log n) memcpy invece di n copie. */
int gmb_sb_append_repeat(gmb_stringbuilder_t* sb, const char* s, size_t n)
{
    size_t sl, total, written;
    if (!sb) return -1;
    if (!s || !*s || n == 0) return 0;
    sl = strlen(s);
    if (sl > (size_t)-1 / n) return -1;          /* overflow del totale */
    total = sl * n;
    if (gmb_sb_reserve(sb, sb->len + total) != 0) return -1;

    memcpy(sb->buf + sb->len, s, sl);            /* seme */
    written = sl;
    while (written < total) {
        size_t chunk = written;
        if (chunk > total - written) chunk = total - written;
        memcpy(sb->buf + sb->len + written, sb->buf + sb->len, chunk);
        written += chunk;
    }
    sb->len += total;
    sb->buf[sb->len] = '\0';
    return 0;
}

/* Posizione della prima occorrenza di `needle`, o -1 se assente. */
long gmb_sb_index_of(const gmb_stringbuilder_t* sb, const char* needle)
{
    const char* hit;
    if (!sb || !sb->buf || !needle || !*needle) return -1;
    hit = strstr(sb->buf, needle);
    return hit ? (long)(hit - sb->buf) : -1;
}

/* AppendFormat in stile posizionale `{i}`: sostituisce i segnaposto `{0}`, `{1}`, … con
 * l'argomento i-esimo (renderizzato come stringa). `{{`/`}}` = graffe letterali; un
 * segnaposto malformato o fuori range è copiato letteralmente. */
int gmb_sb_append_format_vals(gmb_stringbuilder_t* sb, const char* fmt, int n, const gmb_val_t* vals)
{
    const char* p;
    if (!sb || !fmt) return -1;
    for (p = fmt; *p; ) {
        if (*p == '{' && *(p + 1) == '{') { if (gmb_sb_append_n(sb, "{", 1) != 0) return -1; p += 2; continue; }
        if (*p == '}' && *(p + 1) == '}') { if (gmb_sb_append_n(sb, "}", 1) != 0) return -1; p += 2; continue; }
        if (*p == '{') {
            const char* q = p + 1;
            int idx = 0, digits = 0;
            while (*q >= '0' && *q <= '9') { idx = idx * 10 + (*q - '0'); q++; digits++; }
            if (digits > 0 && *q == '}') {
                if (idx >= 0 && idx < n) {
                    gmb_string_t sv = gmb_val_to_string(vals[idx]);
                    if (gmb_sb_append_n(sb, sv.data, sv.length) != 0) return -1;
                }
                p = q + 1;
                continue;
            }
        }
        if (gmb_sb_append_n(sb, p, 1) != 0) return -1;
        p++;
    }
    return 0;
}

/* Inverte il contenuto sul posto (due indici che convergono). */
void gmb_sb_reverse(gmb_stringbuilder_t* sb)
{
    size_t i, j;
    if (!sb || !sb->buf) return;
    for (i = 0, j = sb->len ? sb->len - 1 : 0; i < j; i++, j--) {
        char t = sb->buf[i]; sb->buf[i] = sb->buf[j]; sb->buf[j] = t;
    }
}

/* Rimuove gli spazi (spazio/tab/newline) ai due estremi, in-place. */
void gmb_sb_trim(gmb_stringbuilder_t* sb)
{
    size_t a = 0, b;
    if (!sb || !sb->buf) return;
    while (a < sb->len && (unsigned char)sb->buf[a] <= ' ') a++;
    b = sb->len;
    while (b > a && (unsigned char)sb->buf[b - 1] <= ' ') b--;
    if (a == 0 && b == sb->len) return;
    memmove(sb->buf, sb->buf + a, b - a);
    sb->len = b - a;
    sb->buf[sb->len] = '\0';
}

const char* gmb_sb_data(const gmb_stringbuilder_t* sb)
{
    return (sb && sb->buf) ? sb->buf : "";
}

size_t gmb_sb_length(const gmb_stringbuilder_t* sb)
{
    return sb ? sb->len : 0;
}

size_t gmb_sb_capacity(const gmb_stringbuilder_t* sb)
{
    return sb ? sb->cap : 0;
}

gmb_string_t gmb_sb_to_string(const gmb_stringbuilder_t* sb)
{
    return gmb_string_alloc((sb && sb->buf) ? sb->buf : "");
}
