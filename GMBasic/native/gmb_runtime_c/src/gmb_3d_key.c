/* gmb_3d_key.c — keyframe TRS degli oggetti della scena (M-KEY, piano §4.3).
 *
 *  - `SCENE KEY o, t, x, y, z, yaw, pitch, roll` dichiara una chiave: la
 *    trasformazione dell'oggetto `o` (1..nobj, ordine di dichiarazione come
 *    SCENE.PICK) al tempo `t` in secondi. Le chiavi di un oggetto si ordinano da
 *    sole per tempo, quindi si possono dichiarare in qualunque ordine.
 *  - `SCENE TIME t` fissa il tempo di scena. Non avanza da sé: il programma lo
 *    decide (stessa idea di T-FIXED per il mondo a settori), così lo stesso
 *    programma dà sempre gli stessi frame.
 *  - `RENDER` campiona le piste al tempo di scena e scrive la trasformazione
 *    NELL'oggetto: da lì in poi il renderer e le quote la leggono come se fosse
 *    stata data con OBJPOS. Prima della prima chiave vale la prima, dopo
 *    l'ultima vale l'ultima; fra due chiavi si interpola linearmente su tutte e
 *    sei le componenti (gradi compresi, senza avvolgimento: 350 → 10 passa per
 *    180, è una scelta dichiarata in guida).
 *
 * Perché un modulo a parte: le piste sono uno stato proprio (array ordinato +
 * tempo) e non allargano la facciata dei comandi; `gmb_3d_scene_buf.c` resta il
 * proprietario dei buffer della geometria.
 *
 * Nessuna allocazione per frame: il campionamento legge solo array già
 * dimensionati e scrive negli oggetti; le allocazioni avvengono alla
 * dichiarazione delle chiavi (e il banco lo verifica con `allocs`).
 */
#include "gmb_3d_internal.h"

#include <stdlib.h>
#include <string.h>

/* Chiavi per oggetto e totali: tetti dichiarati (guida §SCENE KEY). Il totale
 * è la guardia di memoria (65536 chiavi = 2 MiB), il per-oggetto tiene corta la
 * ricerca della coppia da interpolare. */
#define GMB_SCENE_KEYS_PER_OBJ 32
#define GMB_SCENE_KEYS_TOTAL   (1 << 16)
#define GMB_SCENE_KEYS_INIT    64

typedef struct gmb_3d_key {
    int   obj;                 /* 1..nobj, ordine di dichiarazione */
    float t;                   /* tempo di scena, secondi */
    float x, y, z, yaw, pitch, roll;
} gmb_3d_key_t;

static gmb_3d_key_t* g_keys;
static int g_nkeys, g_cap_keys;
static float g_time;

static int key_err(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

void gmb_3d_key_reset(void)
{
    /* Capacità conservata: RESET non alloca (come i buffer della geometria). */
    g_nkeys = 0;
    g_time = 0.0f;
}

/* Confini del run di `obj`: le chiavi sono ordinate per (oggetto, tempo), quindi
 * quelle di un oggetto occupano un intervallo contiguo e si trovano con due
 * ricerche binarie. Cosi' la dichiarazione di una chiave costa O(log n) più lo
 * spostamento dell'inserimento, invece di ripercorrere tutte le chiavi della
 * scena (con molte chiavi diventava quadratica). */
static int key_run_start(int obj)
{
    int lo = 0, hi = g_nkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_keys[mid].obj < obj) lo = mid + 1; else hi = mid;
    }
    return lo;
}

static int key_run_end(int obj)
{
    int lo = 0, hi = g_nkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_keys[mid].obj <= obj) lo = mid + 1; else hi = mid;
    }
    return lo;
}

int gmb_3d_key_count(int obj)
{
    return key_run_end(obj) - key_run_start(obj);
}

/* Posizione della PRIMA chiave che va dopo (obj, t): l'array resta ordinato per
 * (oggetto, tempo), quindi il campionamento è una scansione sola per frame.
 * Ricerca binaria sul confronto (oggetto, tempo), lo stesso ordine del memmove. */
static int key_slot(int obj, float t)
{
    int lo = 0, hi = g_nkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_keys[mid].obj < obj || (g_keys[mid].obj == obj && g_keys[mid].t < t))
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static int key_reserve(int need)
{
    if (need <= g_cap_keys) return GMB_3D_OK;
    if (need > GMB_SCENE_KEYS_TOTAL) return key_err("SCENE KEY: massimo 65536 chiavi per scena");
    int cap = g_cap_keys > 0 ? g_cap_keys : GMB_SCENE_KEYS_INIT;
    while (cap < need) cap *= 2;
    if (cap > GMB_SCENE_KEYS_TOTAL) cap = GMB_SCENE_KEYS_TOTAL;
    gmb_3d_key_t* p = (gmb_3d_key_t*)realloc(g_keys, (size_t)cap * sizeof(gmb_3d_key_t));
    if (p == NULL) return key_err("SCENE KEY: memoria esaurita");
    g_keys = p;
    g_cap_keys = cap;
    gmb_3d_note_alloc(1);
    return GMB_3D_OK;
}

int64_t gmb_scene_key(int64_t o, double t, double x, double y, double z,
                      double yaw, double pitch, double roll)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (o < 1 || o > g->nobj) return key_err("SCENE KEY: oggetto inesistente (1..nobj)");
    if (!(t >= 0.0) || t > 1.0e9) return key_err("SCENE KEY: tempo non valido (>= 0)");
    if (gmb_3d_key_count((int)o) >= GMB_SCENE_KEYS_PER_OBJ)
        return key_err("SCENE KEY: massimo 32 chiavi per oggetto");
    int rc = key_reserve(g_nkeys + 1);
    if (rc != GMB_3D_OK) return rc;

    int at = key_slot((int)o, (float)t);
    if (at < g_nkeys)
        memmove(&g_keys[at + 1], &g_keys[at], (size_t)(g_nkeys - at) * sizeof(gmb_3d_key_t));
    gmb_3d_key_t* k = &g_keys[at];
    k->obj = (int)o;
    k->t = (float)t;
    k->x = (float)x; k->y = (float)y; k->z = (float)z;
    k->yaw = (float)yaw; k->pitch = (float)pitch; k->roll = (float)roll;
    g_nkeys++;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_time(double t)
{
    if (!(t >= 0.0) || t > 1.0e9) return key_err("SCENE TIME: tempo non valido (>= 0)");
    g_time = (float)t;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

double gmb_scene_time_now(void)
{
    return (double)g_time;
}

/* Campiona UNA pista (chiavi [i, j), stesso oggetto) al tempo `t`. */
static void key_sample_run(gmb_scene_obj_t* o, int i, int j)
{
    if (i >= j) return;
    const float t = g_time;
    if (t <= g_keys[i].t)
    {
        /* Prima della prima chiave: la prima (clamp, nessuna estrapolazione). */
        o->x = g_keys[i].x; o->y = g_keys[i].y; o->z = g_keys[i].z;
        o->yaw = g_keys[i].yaw; o->pitch = g_keys[i].pitch; o->roll = g_keys[i].roll;
        return;
    }
    if (t >= g_keys[j - 1].t)
    {
        o->x = g_keys[j - 1].x; o->y = g_keys[j - 1].y; o->z = g_keys[j - 1].z;
        o->yaw = g_keys[j - 1].yaw; o->pitch = g_keys[j - 1].pitch; o->roll = g_keys[j - 1].roll;
        return;
    }
    int k = i;
    while (k + 1 < j && g_keys[k + 1].t <= t) k++;
    const gmb_3d_key_t* a = &g_keys[k];
    const gmb_3d_key_t* b = &g_keys[k + 1];
    float dt = b->t - a->t;
    /* Due chiavi allo stesso tempo (t dichiarato uguale): vince la seconda, che
     * è quella che il programma ha scritto per ultima. */
    float u = dt > 0.0f ? (t - a->t) / dt : 1.0f;
    o->x = a->x + (b->x - a->x) * u;
    o->y = a->y + (b->y - a->y) * u;
    o->z = a->z + (b->z - a->z) * u;
    o->yaw = a->yaw + (b->yaw - a->yaw) * u;
    o->pitch = a->pitch + (b->pitch - a->pitch) * u;
    o->roll = a->roll + (b->roll - a->roll) * u;
}

void gmb_3d_key_sample(void)
{
    gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    int i = 0;
    while (i < g_nkeys)
    {
        int obj = g_keys[i].obj, j = i;
        while (j < g_nkeys && g_keys[j].obj == obj) j++;
        if (obj >= 1 && obj <= g->nobj) key_sample_run(&g->obj[obj - 1], i, j);
        i = j;
    }
}
