/* gmb_3d_skin.c — skinning a 64 ossa (P5, M-SKIN, piano §4.3).
 *
 *  - `SCENE BONES n` dichiara n ossa (0..64). Azzera le pose e TUTTI i pesi:
 *    cambiare scheletro invalida le legature precedenti (niente riferimenti a
 *    ossa che non esistono più).
 *  - `SCENE POSEBONE b, x, y, z, yaw [, pitch, roll]` dà la posa dell'osso `b`
 *    (1..n): una TRS nello spazio dell'OGGETTO, con la stessa convenzione di
 *    OBJPOS/KEY. La posa di default è l'identità, quindi una scena senza
 *    POSEBONE disegna gli stessi byte di sempre: non c'è una bind pose da
 *    indovinare, la matrice dell'osso È la trasformazione dei suoi vertici.
 *  - `SCENE WEIGHT o, v, bone, w` lega il vertice `v` (0-based, relativo
 *    all'oggetto `o`, come gli indici di QUAD) all'osso `bone` con peso `w`
 *    (0..1, clampato). Massimo DUE influenze per vertice; la terza è un errore
 *    esplicito, non una caduta silenziosa.
 *
 * Perché un modulo a parte: matrici, pesi e fusione devono stare in UN solo
 * posto, letto dal disegno, e la facciata dei comandi (`gmb_3d_cmd.c`) è al
 * limite delle 1200 righe (§0quater).
 *
 * Scelte dichiarate (specifica M-SKIN):
 *  - i pesi si normalizzano alla SOMMA: `w = 0` non muove il vertice, due pesi
 *    uguali danno il punto medio, un peso solo (anche diverso da 1) dà la
 *    matrice intera — è la fusione lineare, senza sorprese di scala;
 *  - si skinano le POSIZIONI: la luce per faccia (M-LIGHT) legge le posizioni
 *    skinnate, le normali dichiarate (M-LIGHT-N) restano quelle del vertice;
 *  - AABB mondo, quote, raggi e collisione restano quelli della geometria
 *    DICHIARATA di `o`, come LOD e INSTANCE (M-SKIN-VIS): una posa che esce
 *    dalla scatola non sposta il mondo sotto i piedi del giocatore;
 *  - i pesi sono 4 byte per vertice (due ossi da un byte + due pesi da un byte)
 *    e i buffer esistono SOLO per le scene che usano WEIGHT: le altre non
 *    pagano né memoria né un passaggio in più (M-SKIN-ALLOC);
 *  - il frame non alloca: le 64 matrici si calcolano una volta per RENDER
 *    nello scratch già riservato.
 */
#include "gmb_3d_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Ossa: TRS nello spazio dell'oggetto. La posa di default è (0,0,0, 0,0,0) e
 * la sua matrice è l'identità, quindi non c'è una tabella di bind pose. */
static gmb_3d_bone_t g_bone[GMB_SCENE_MAX_BONES];
static int g_nbones;
static float g_bmat[GMB_SCENE_MAX_BONES * 16];

/* Piste di chiavi degli ossi (M-SKIN-KEY) — `SCENE ANIM b, t, x, y, z, yaw
 * [, pitch, roll]`: le stesse regole di `SCENE KEY` per gli oggetti. Le chiavi
 * si ordinano da sole per (osso, tempo), quindi si dichiarano in qualunque
 * ordine; RENDER campiona al tempo di scena e scrive la posa NELL'osso, che poi
 * compone la sua matrice come farebbe un POSEBONE. Prima della prima chiave vale
 * la prima, dopo l'ultima vale l'ultima; fra due chiavi si interpola linearmente
 * su tutte e sei le componenti. Tetti dichiarati come per gli oggetti: 32 chiavi
 * per osso, 65536 in tutto (guardia di memoria). */
#define GMB_SCENE_BKEYS_PER_BONE 32
#define GMB_SCENE_BKEYS_TOTAL    (1 << 16)
#define GMB_SCENE_BKEYS_INIT     64

typedef struct gmb_3d_bkey {
    int   bone;                /* 1..64 */
    float t;                   /* tempo di scena, secondi */
    float x, y, z, yaw, pitch, roll;
} gmb_3d_bkey_t;

static gmb_3d_bkey_t* g_bkeys;
static int g_nbkeys, g_cap_bkeys;

static int skin_err(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

/* Ossa e pose a zero, matrici a identità, piste degli ossi svuotate (la
 * capacità resta: RESET non alloca). NON tocca i pesi: quelli stanno nel buffer
 * dei vertici e li azzera chi ha in mano la scena. Le chiavi si azzerano perché
 * cambiare scheletro cambia il significato di «osso 3»: meglio nessuna pista
 * che una pista appesa a un osso che ora è un altro. */
static void bones_clear(void)
{
    g_nbones = 0;
    g_nbkeys = 0;
    memset(g_bone, 0, sizeof(g_bone));
    for (int i = 0; i < GMB_SCENE_MAX_BONES; i++)
    {
        float* m = g_bmat + (size_t)i * 16;
        memset(m, 0, 16 * sizeof(float));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
}

void gmb_3d_skin_reset(void)
{
    /* Prima i pesi: la loro estensione è `nv`, che la scena sta per azzerare. */
    gmb_3d_scene_skin_clear(gmb_3d_scene_geom()->nv);
    bones_clear();
}

int64_t gmb_scene_bones(int64_t n)
{
    if (n < 0 || n > GMB_SCENE_MAX_BONES)
        return skin_err("SCENE BONES: numero di ossa tra 0 e 64");

    gmb_3d_scene_skin_clear(gmb_3d_scene_geom()->nv);
    bones_clear();
    g_nbones = (int)n;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* POSEBONE, nucleo comune alle due forme (5 e 7 argomenti). */
static int64_t bone_set(int64_t b, double x, double y, double z,
                        double yaw, double pitch, double roll)
{
    if (b < 1 || b > g_nbones)
        return skin_err("SCENE POSEBONE: osso inesistente (1..BONES)");
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) ||
        !isfinite(yaw) || !isfinite(pitch) || !isfinite(roll))
        return skin_err("SCENE POSEBONE: componenti non finite");
    gmb_3d_bone_t* bo = &g_bone[b - 1];
    bo->x = (float)x; bo->y = (float)y; bo->z = (float)z;
    bo->yaw = (float)yaw; bo->pitch = (float)pitch; bo->roll = (float)roll;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_posebone(int64_t b, double x, double y, double z, double yaw)
{
    return bone_set(b, x, y, z, yaw, 0.0, 0.0);
}

int64_t gmb_scene_posebone_trs(int64_t b, double x, double y, double z,
                               double yaw, double pitch, double roll)
{
    return bone_set(b, x, y, z, yaw, pitch, roll);
}

/* ---- piste di chiavi degli ossi: ricerca binaria sul run dell'osso -------- */
static int bkey_run_start(int bone)
{
    int lo = 0, hi = g_nbkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_bkeys[mid].bone < bone) lo = mid + 1; else hi = mid;
    }
    return lo;
}

static int bkey_run_end(int bone)
{
    int lo = 0, hi = g_nbkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_bkeys[mid].bone <= bone) lo = mid + 1; else hi = mid;
    }
    return lo;
}

/* Posizione della PRIMA chiave che va dopo (bone, t): l'array resta ordinato per
 * (osso, tempo), lo stesso ordine del memmove dell'inserimento. */
static int bkey_slot(int bone, float t)
{
    int lo = 0, hi = g_nbkeys;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (g_bkeys[mid].bone < bone || (g_bkeys[mid].bone == bone && g_bkeys[mid].t < t))
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static int bkey_reserve(int need)
{
    if (need <= g_cap_bkeys) return GMB_3D_OK;
    if (need > GMB_SCENE_BKEYS_TOTAL) return skin_err("SCENE ANIM: massimo 65536 chiavi per scena");
    int cap = g_cap_bkeys > 0 ? g_cap_bkeys : GMB_SCENE_BKEYS_INIT;
    while (cap < need) cap *= 2;
    if (cap > GMB_SCENE_BKEYS_TOTAL) cap = GMB_SCENE_BKEYS_TOTAL;
    gmb_3d_bkey_t* p = (gmb_3d_bkey_t*)realloc(g_bkeys, (size_t)cap * sizeof(gmb_3d_bkey_t));
    if (p == NULL) return skin_err("SCENE ANIM: memoria esaurita");
    g_bkeys = p;
    g_cap_bkeys = cap;
    gmb_3d_note_alloc(1);
    return GMB_3D_OK;
}

/* ANIM, nucleo comune alle due forme (6 e 8 argomenti). */
static int64_t bone_key_add(int64_t b, double t, double x, double y, double z,
                            double yaw, double pitch, double roll)
{
    if (b < 1 || b > g_nbones)
        return skin_err("SCENE ANIM: osso inesistente (1..BONES)");
    if (!(t >= 0.0) || t > 1.0e9) return skin_err("SCENE ANIM: tempo non valido (>= 0)");
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) ||
        !isfinite(yaw) || !isfinite(pitch) || !isfinite(roll))
        return skin_err("SCENE ANIM: componenti non finite");
    if (bkey_run_end((int)b) - bkey_run_start((int)b) >= GMB_SCENE_BKEYS_PER_BONE)
        return skin_err("SCENE ANIM: massimo 32 chiavi per osso");
    int rc = bkey_reserve(g_nbkeys + 1);
    if (rc != GMB_3D_OK) return rc;

    int at = bkey_slot((int)b, (float)t);
    if (at < g_nbkeys)
        memmove(&g_bkeys[at + 1], &g_bkeys[at], (size_t)(g_nbkeys - at) * sizeof(gmb_3d_bkey_t));
    gmb_3d_bkey_t* k = &g_bkeys[at];
    k->bone = (int)b;
    k->t = (float)t;
    k->x = (float)x; k->y = (float)y; k->z = (float)z;
    k->yaw = (float)yaw; k->pitch = (float)pitch; k->roll = (float)roll;
    g_nbkeys++;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_anim(int64_t b, double t, double x, double y, double z, double yaw)
{
    return bone_key_add(b, t, x, y, z, yaw, 0.0, 0.0);
}

int64_t gmb_scene_anim_trs(int64_t b, double t, double x, double y, double z,
                           double yaw, double pitch, double roll)
{
    return bone_key_add(b, t, x, y, z, yaw, pitch, roll);
}

/* Campiona UNA pista (chiavi [i, j), stesso osso) al tempo di scena. */
static void bkey_sample_run(gmb_3d_bone_t* bo, int i, int j)
{
    if (i >= j) return;
    const float t = (float)gmb_scene_time_now();
    int k = i;
    while (k + 1 < j && g_bkeys[k + 1].t <= t) k++;
    const gmb_3d_bkey_t* a = &g_bkeys[k];
    if (t <= g_bkeys[i].t || k + 1 >= j)
    {
        /* Clamp: prima della prima chiave vale la prima, dopo l'ultima l'ultima. */
        bo->x = a->x; bo->y = a->y; bo->z = a->z;
        bo->yaw = a->yaw; bo->pitch = a->pitch; bo->roll = a->roll;
        return;
    }
    const gmb_3d_bkey_t* b = &g_bkeys[k + 1];
    float dt = b->t - a->t;
    /* Due chiavi allo stesso tempo: vince la seconda (l'ultima dichiarata). */
    float u = dt > 0.0f ? (t - a->t) / dt : 1.0f;
    bo->x = a->x + (b->x - a->x) * u;
    bo->y = a->y + (b->y - a->y) * u;
    bo->z = a->z + (b->z - a->z) * u;
    bo->yaw = a->yaw + (b->yaw - a->yaw) * u;
    bo->pitch = a->pitch + (b->pitch - a->pitch) * u;
    bo->roll = a->roll + (b->roll - a->roll) * u;
}

static void bkey_sample_all(void)
{
    int i = 0;
    while (i < g_nbkeys)
    {
        int bone = g_bkeys[i].bone, j = i;
        while (j < g_nbkeys && g_bkeys[j].bone == bone) j++;
        if (bone >= 1 && bone <= g_nbones) bkey_sample_run(&g_bone[bone - 1], i, j);
        i = j;
    }
}

int64_t gmb_scene_weight(int64_t o, int64_t v, int64_t bone, double w)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (o < 1 || o > g->nobj) return skin_err("SCENE WEIGHT: oggetto inesistente (1..nobj)");
    const gmb_scene_obj_t* ob = &g->obj[o - 1];
    if (v < 0 || v >= ob->nvert)
        return skin_err("SCENE WEIGHT: vertice inesistente (0..nvert-1 dell'oggetto)");
    if (bone < 1 || bone > g_nbones)
        return skin_err("SCENE WEIGHT: osso inesistente (1..BONES)");
    if (!isfinite(w)) return skin_err("SCENE WEIGHT: peso non finito");

    if (gmb_3d_scene_skin_touch() != GMB_3D_OK) return GMB_3D_E_ARG;

    gmb_3d_scene_geom_t* gw = gmb_3d_scene_geom();
    gmb_3d_skin_t* s = &gw->skin[(size_t)ob->first_vert + (size_t)v];
    unsigned char b = (unsigned char)bone;
    /* 0..1 → 0..255: la quantizzazione è la stessa su tutte le gambe (è C). */
    unsigned char q = (unsigned char)(w <= 0.0 ? 0 : (w >= 1.0 ? 255 : (int)(w * 255.0 + 0.5)));

    if (s->b0 == GMB_SKIN_NONE || s->b0 == b) { s->b0 = b; s->w0 = q; }
    else if (s->b1 == GMB_SKIN_NONE || s->b1 == b) { s->b1 = b; s->w1 = q; }
    else return skin_err("SCENE WEIGHT: massimo 2 influenze per vertice");

    gw->obj[o - 1].has_skin = 1;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* Le matrici degli ossi, una volta per frame: RENDER la chiama prima dei pass.
 * Prima si campionano le piste (se ci sono): la posa del frame È quella scritta
 * dalle chiavi, come per gli oggetti in M-KEY. */
void gmb_3d_skin_pose_all(void)
{
    bkey_sample_all();
    /* Sempre, anche per la posa di default: una posa riportata a zero deve
     * rimettere l'identità, non lasciare la matrice del frame precedente. */
    for (int i = 0; i < g_nbones; i++)
    {
        const gmb_3d_bone_t* b = &g_bone[i];
        gmb_3d_m4_model_trs(g_bmat + (size_t)i * 16, b->x, b->y, b->z,
                            b->yaw, b->pitch, b->roll);
    }
}

/* Posizioni da disegnare per l'oggetto `oi`: la geometria dichiarata quando non
 * ci sono pesi, altrimenti lo scratch con la fusione lineare. L'indice assoluto
 * del vertice è lo stesso nei due buffer (`first_vert`), quindi il chiamante
 * legge sempre allo stesso modo. NULL solo per un indice fuori scena. */
const float* gmb_3d_skin_pos(int oi)
{
    const gmb_3d_scene_geom_t* g = gmb_3d_scene_geom();
    if (oi < 0 || oi >= g->nobj) return NULL;
    const gmb_scene_obj_t* o = &g->obj[oi];
    if (!o->has_skin || g->skin == NULL || g->sk_pos == NULL || o->nvert <= 0) return g->pos;

    float* dst = g->sk_pos + (size_t)o->first_vert * 3;
    const float* src = g->pos + (size_t)o->first_vert * 3;
    for (int i = 0; i < o->nvert; i++)
    {
        const gmb_3d_skin_t* s = &g->skin[(size_t)o->first_vert + (size_t)i];
        const float* p = src + (size_t)i * 3;
        /* Un osso cancellato (BONES più piccolo) vale come assenza di influenza. */
        int n0 = (s->b0 == GMB_SKIN_NONE || s->b0 > g_nbones) ? -1 : (int)s->b0 - 1;
        int n1 = (s->b1 == GMB_SKIN_NONE || s->b1 > g_nbones) ? -1 : (int)s->b1 - 1;
        int w0 = (n0 < 0) ? 0 : (int)s->w0;
        int w1 = (n1 < 0) ? 0 : (int)s->w1;
        int wsum = w0 + w1;
        if (wsum == 0)
        {
            dst[i * 3] = p[0]; dst[i * 3 + 1] = p[1]; dst[i * 3 + 2] = p[2];
            continue;
        }

        double ax = 0.0, ay = 0.0, az = 0.0;
        if (w0 > 0)
        {
            const float* m = g_bmat + (size_t)n0 * 16;
            ax += (double)w0 * (m[0] * p[0] + m[4] * p[1] + m[8]  * p[2] + m[12]);
            ay += (double)w0 * (m[1] * p[0] + m[5] * p[1] + m[9]  * p[2] + m[13]);
            az += (double)w0 * (m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]);
        }
        if (w1 > 0)
        {
            const float* m = g_bmat + (size_t)n1 * 16;
            ax += (double)w1 * (m[0] * p[0] + m[4] * p[1] + m[8]  * p[2] + m[12]);
            ay += (double)w1 * (m[1] * p[0] + m[5] * p[1] + m[9]  * p[2] + m[13]);
            az += (double)w1 * (m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]);
        }
        dst[i * 3]     = (float)(ax / (double)wsum);
        dst[i * 3 + 1] = (float)(ay / (double)wsum);
        dst[i * 3 + 2] = (float)(az / (double)wsum);
    }
    return g->sk_pos;
}
