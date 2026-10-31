/* gmb_bsp_session.c — parità CommonLib/Http/BspSessionManager.cs:103 (cookie session store)
 * CreateSession/GetSession/GetOrCreate/Destroy/Count + dati JSON per sessione (dict string).
 * Isolato: non tocca gmb_bsp.c:300. Id = 32 hex (come Guid.NewGuid("N")), cookie GMBSESSION.
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define GMB_SESS_MAX 256

typedef struct gmb_session {
    char* id;
    gmb_dict_t data;
    time_t last_access;
    struct gmb_session* next;
} gmb_session_t;

typedef struct {
    gmb_session_t* head;
    int count;
    int timeout_min; /* 0 = nessun timeout */
} gmb_session_mgr_t;

static gmb_session_mgr_t* g_sess_mgr[GMB_SESS_MAX];

static uint64_t g_sess_seed = 0x2545F4914F6CDD1DULL;
static uint64_t g_sess_rnd(void)
{
    uint64_t x = g_sess_seed;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    g_sess_seed = x;
    return x * 2685821657736338717ULL;
}

static void g_sess_gen_id(char* out32)
{
    static const char hexd[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) out32[i] = hexd[(g_sess_rnd() >> (i % 8)) & 0x0F];
    out32[32] = '\0';
}

static gmb_session_t* sess_find(gmb_session_mgr_t* m, const char* id)
{
    if (!m || !id) return NULL;
    for (gmb_session_t* s = m->head; s; s = s->next)
        if (strcmp(s->id, id) == 0) return s;
    return NULL;
}

int64_t gmb_bsp_session_create_manager(int timeout_minutes)
{
    for (int i = 0; i < GMB_SESS_MAX; i++) {
        if (!g_sess_mgr[i]) {
            gmb_session_mgr_t* m = (gmb_session_mgr_t*)calloc(1, sizeof(gmb_session_mgr_t));
            if (!m) return -1;
            m->timeout_min = timeout_minutes > 0 ? timeout_minutes : 30;
            g_sess_mgr[i] = m;
            return i + 1;
        }
    }
    return -1;
}

int gmb_bsp_session_create(int64_t h, char* id_out, char* cookie_out)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)]) return 0;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    char id[33];
    g_sess_gen_id(id);
    gmb_session_t* s = (gmb_session_t*)calloc(1, sizeof(gmb_session_t));
    if (!s) return 0;
    s->id = (char*)malloc(33);
    memcpy(s->id, id, 33);
    s->data = gmb_dict_new();
    s->last_access = time(NULL);
    s->next = m->head;
    m->head = s;
    m->count++;
    if (id_out) memcpy(id_out, id, 33);
    if (cookie_out) snprintf(cookie_out, 128, "GMBSESSION=%s; Path=/; HttpOnly; SameSite=Lax", id);
    return 1;
}

int gmb_bsp_session_get(int64_t h, const char* session_id)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)] || !session_id) return 0;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    gmb_session_t* s = sess_find(m, session_id);
    if (!s) return 0;
    if (m->timeout_min > 0 && difftime(time(NULL), s->last_access) > m->timeout_min * 60) {
        gmb_bsp_session_destroy(h, session_id);
        return 0;
    }
    s->last_access = time(NULL);
    return 1;
}

int gmb_bsp_session_get_or_create(int64_t h, const char* cookie_header,
                                  char* id_out, char* cookie_out, int* created)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)]) return 0;
    /* estrai GMBSESSION=<id> dal cookie header */
    const char* p = cookie_header;
    char existing[33] = {0};
    while (p && *p) {
        const char* semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        const char* eq = strstr(p, "=");
        if (eq && eq < p + len) {
            size_t keylen = (size_t)(eq - p);
            if (keylen == 10 && (strncasecmp(p, "GMBSESSION", 10) == 0)) {
                size_t vlen = len - keylen - 1;
                if (vlen >= 32) { memcpy(existing, eq + 1, 32); existing[32] = '\0'; break; }
            }
        }
        if (!semi) break;
        p = semi + 1;
    }
    if (existing[0] && gmb_bsp_session_get(h, existing)) {
        if (id_out) memcpy(id_out, existing, 33);
        if (created) *created = 0;
        if (cookie_out) cookie_out[0] = '\0';
        return 1;
    }
    int ok = gmb_bsp_session_create(h, id_out, cookie_out);
    if (created) *created = 1;
    return ok;
}

void gmb_bsp_session_destroy(int64_t h, const char* session_id)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)] || !session_id) return;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    gmb_session_t** pp = &m->head;
    while (*pp) {
        gmb_session_t* s = *pp;
        if (strcmp(s->id, session_id) == 0) {
            *pp = s->next;
            gmb_dict_free(&s->data);
            free(s->id);
            free(s);
            m->count--;
            return;
        }
        pp = &s->next;
    }
}

int gmb_bsp_session_count(int64_t h)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)]) return 0;
    return g_sess_mgr[(int)(h - 1)]->count;
}

int gmb_bsp_session_set_data(int64_t h, const char* session_id, const char* key, const char* value)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)] || !session_id || !key) return 0;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    gmb_session_t* s = sess_find(m, session_id);
    if (!s) return 0;
    gmb_dict_set(&s->data, key, gmb_make_val_string(value ? value : ""));
    return 1;
}

const char* gmb_bsp_session_get_data(int64_t h, const char* session_id, const char* key)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)] || !session_id || !key) return NULL;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    gmb_session_t* s = sess_find(m, session_id);
    if (!s) return NULL;
    gmb_val_t v = gmb_dict_get(s->data, key);
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return NULL;
}

void gmb_bsp_session_free_manager(int64_t h)
{
    if (h <= 0 || h > GMB_SESS_MAX || !g_sess_mgr[(int)(h - 1)]) return;
    gmb_session_mgr_t* m = g_sess_mgr[(int)(h - 1)];
    while (m->head) {
        gmb_session_t* s = m->head;
        m->head = s->next;
        gmb_dict_free(&s->data);
        free(s->id);
        free(s);
    }
    free(m);
    g_sess_mgr[(int)(h - 1)] = NULL;
}
