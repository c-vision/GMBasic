/* gmb_graphics_sprite_group.c — P7: gruppi di sprite (Engine1999 «SPRITE n, SIZE w, h»).
 *
 * Comportamento replicato dal riferimento (docs/graphics-subsystem.md, «Sprite groups»):
 *  - `SIZE w, h` (w, h 1..8, w*h <= 64) rende lo sprite n la TESTA di un blocco w×h di
 *    sprite consecutivi, riga per riga; il blocco si muove, si mostra e collide come un
 *    oggetto unico e le collisioni riferiscono la testa;
 *  - i membri di uno stesso gruppo non collidono tra loro;
 *  - le impostazioni dei membri (posizione, visibilita', priorita', rotazione, scala) sono
 *    ignorate mentre appartengono al gruppo e vengono ripristinate quando lo scioglie
 *    (SIZE 1,1);
 *  - la testa non puo' essere un membro e un membro non puo' essere testa di un altro
 *    gruppo.
 * Differenza dichiarata: nel riferimento la cella e' fissa 16×24 px (sprite di una sprite
 * map); qui gli sprite sono texture di dimensione libera, quindi la cella e' `cell_w/cell_h`
 * della texture della testa (`w/h` se non e' un foglio di celle). Rotazione e scala della
 * testa dispongono il blocco come un corpo rigido: gli offset si scalano e si ruotano, e i
 * membri ereditano rotazione/scala della testa. Vale anche la matrice affine della testa:
 * si decompone in rotazione+scala con `gmb_gfx_sprite_rot_scale` (la stessa trasformazione
 * del disegno), quindi testa e membri restano d'accordo su dove finisce ogni cella.
 */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { int w, h; } gmb_group_t;

/* Stato del membro salvato alla creazione del gruppo (ripristinato allo scioglimento). */
typedef struct {
    float x, y;
    int visible, priority;
    double rotation, scale_x, scale_y;
} gmb_saved_t;

static gmb_group_t g_groups[GMB_GFX_MAX_SPRITES];
static gmb_saved_t g_saved[GMB_GFX_MAX_SPRITES];
static unsigned char g_saved_ok[GMB_GFX_MAX_SPRITES];
/* 0 = nessun gruppo; == id quando l'id e' la testa; == testa quando l'id e' un membro.
 * Indice inverso O(1): il loop di SYNC lo interroga per ogni sprite, un scan del gruppo
 * costerebbe 1024×1024 per frame. */
static int g_head[GMB_GFX_MAX_SPRITES];

static int id_ok(int id) { return id >= 0 && id < GMB_GFX_MAX_SPRITES; }

int gmb_gfx_group_head_of(int id)
{
    return id_ok(id) ? g_head[id] : 0;
}

int gmb_gfx_group_is_member(int id)
{
    return id_ok(id) && g_head[id] != 0 && g_head[id] != id;
}

int gmb_gfx_group_size(int head, int* cols, int* rows)
{
    if (!id_ok(head) || g_groups[head].w <= 0) return 0;
    if (cols) *cols = g_groups[head].w;
    if (rows) *rows = g_groups[head].h;
    return 1;
}

/* Cella del blocco: `cell_w/cell_h` della texture della testa (foglio di celle), altrimenti
 * la dimensione dell'immagine. 0 = nessuna texture: i membri restano sulla testa. */
static void head_cell(int head, int* tw, int* th)
{
    *tw = 0;
    *th = 0;
    if (!id_ok(head)) return;
    {
        gmb_gfx_sprite_t* s = &g_gfx.sprites[head];
        gmb_gfx_image_t* t;
        if (s->tex_id < 0 || s->tex_id >= GMB_GFX_MAX_TEXTURES) return;
        t = &g_gfx.textures[s->tex_id];
        if (!t->in_use || !t->pixels) return;
        *tw = t->cell_w > 0 ? t->cell_w : t->w;
        *th = t->cell_h > 0 ? t->cell_h : t->h;
    }
}

/* Dispone i membri intorno alla testa. Idempotente e senza allocazioni: la chiamano il
 * settaggio di posizione/rotazione/scala della testa, MOVETO e i path. */
int gmb_gfx_group_relayout(int head)
{
    int w, h, tw, th;
    gmb_gfx_sprite_t* hs;
    double rad, cosR, sinR, rot, sx, sy;
    if (!id_ok(head)) return -1;
    w = g_groups[head].w;
    h = g_groups[head].h;
    if (w <= 0 || h <= 0) return 0;
    head_cell(head, &tw, &th);
    hs = &g_gfx.sprites[head];
    gmb_gfx_sprite_rot_scale(hs, &rot, &sx, &sy);
    rad = rot * M_PI / 180.0;
    cosR = cos(rad);
    sinR = sin(rad);
    for (int k = 0; k < w * h; k++)
    {
        int id = head + k;
        gmb_gfx_sprite_t* s;
        if (id >= GMB_GFX_MAX_SPRITES) break;
        s = &g_gfx.sprites[id];
        if (k > 0)
        {
            double ox = (double)(k % w) * (double)tw * sx;
            double oy = (double)(k / w) * (double)th * sy;
            s->x = hs->x + (float)(ox * cosR - oy * sinR);
            s->y = hs->y + (float)(ox * sinR + oy * cosR);
            s->rotation = rot;
            s->scale_x = sx;
            s->scale_y = sy;
        }
        s->visible = hs->visible;
        s->priority = hs->priority;
    }
    return 0;
}

/* Disegna il blocco. Ritorna 1 se la testa ha davvero un gruppo (e il blocco e' stato
 * disegnato qui), 0 se non e' una testa di gruppo: i chiamanti ricadono sul disegno singolo. */
int gmb_gfx_group_draw_block(int head)
{
    int w, h;
    if (!id_ok(head)) return 0;
    w = g_groups[head].w;
    h = g_groups[head].h;
    if (w <= 0 || h <= 0) return 0;
    for (int k = 0; k < w * h; k++)
    {
        int id = head + k;
        double rot, sx, sy;
        gmb_gfx_sprite_t* s;
        if (id >= GMB_GFX_MAX_SPRITES) break;
        s = &g_gfx.sprites[id];
        if (!s->in_use) continue;
        if (s->tex_id < 0 || s->tex_id >= GMB_GFX_MAX_TEXTURES) continue;
        if (!g_gfx.textures[s->tex_id].in_use || !g_gfx.textures[s->tex_id].pixels) continue;
        if (k == 0)
        {
            /* la testa puo' avere una matrice: stessa decomposizione del disegno singolo */
            gmb_gfx_sprite_rot_scale(s, &rot, &sx, &sy);
        }
        else
        {
            /* i membri usano la rotazione/scala che `relayout` ha copiato dalla testa: una
             * loro matrice resta ignorata (le impostazioni dei membri comanda la testa) */
            rot = s->rotation;
            sx = s->scale_x;
            sy = s->scale_y;
        }
        gmb_gfx_draw_texture(s->tex_id, (int)s->x, (int)s->y, rot, sx, sy);
    }
    return 1;
}

static void group_dissolve(int head)
{
    int w = g_groups[head].w, h = g_groups[head].h;
    for (int k = 1; k < w * h; k++)
    {
        int id = head + k;
        if (id >= GMB_GFX_MAX_SPRITES) break;
        if (g_saved_ok[id])
        {
            gmb_gfx_sprite_t* s = &g_gfx.sprites[id];
            s->x = g_saved[id].x;
            s->y = g_saved[id].y;
            s->visible = g_saved[id].visible;
            s->priority = g_saved[id].priority;
            s->rotation = g_saved[id].rotation;
            s->scale_x = g_saved[id].scale_x;
            s->scale_y = g_saved[id].scale_y;
            g_saved_ok[id] = 0;
        }
        g_head[id] = 0;
    }
    g_groups[head].w = 0;
    g_groups[head].h = 0;
    g_head[head] = 0;
}

int gmb_gfx_sprite_set_size(int head, int w, int h)
{
    if (w < 1 || h < 1 || w > 8 || h > 8 || w * h > 64) return -1;
    if (!id_ok(head) || !g_gfx.sprites[head].in_use) return -1;
    if (head + w * h > GMB_GFX_MAX_SPRITES) return -1;
    /* la testa non puo' essere un membro di un altro gruppo */
    if (g_head[head] != 0 && g_head[head] != head) return -1;
    /* i membri non possono appartenere ad altri gruppi, ne' essere teste di altri gruppi */
    for (int k = 1; k < w * h; k++)
    {
        int id = head + k;
        if (g_head[id] != 0 && g_head[id] != head) return -1;
        if (g_groups[id].w > 0) return -1;
    }
    /* ridimensionare o sciogliere: prima si ripristinano i membri del gruppo precedente */
    if (g_groups[head].w > 0) group_dissolve(head);
    if (w == 1 && h == 1) return 0;
    g_groups[head].w = w;
    g_groups[head].h = h;
    for (int k = 0; k < w * h; k++)
    {
        int id = head + k;
        gmb_gfx_sprite_t* s = &g_gfx.sprites[id];
        if (k > 0)
        {
            g_saved[id].x = s->x;
            g_saved[id].y = s->y;
            g_saved[id].visible = s->visible;
            g_saved[id].priority = s->priority;
            g_saved[id].rotation = s->rotation;
            g_saved[id].scale_x = s->scale_x;
            g_saved[id].scale_y = s->scale_y;
            g_saved_ok[id] = 1;
        }
        g_head[id] = head;
    }
    gmb_gfx_group_relayout(head);
    return 0;
}

int gmb_gfx_sprite_cols(int head)
{
    return (id_ok(head) && g_groups[head].w > 0) ? g_groups[head].w : 1;
}

int gmb_gfx_sprite_rows(int head)
{
    return (id_ok(head) && g_groups[head].h > 0) ? g_groups[head].h : 1;
}

int gmb_gfx_sprite_group_head(int id)
{
    return gmb_gfx_group_head_of(id);
}

int gmb_gfx_group_reset(void)
{
    for (int h = 0; h < GMB_GFX_MAX_SPRITES; h++)
        if (g_groups[h].w > 0) group_dissolve(h);
    memset(g_groups, 0, sizeof(g_groups));
    memset(g_head, 0, sizeof(g_head));
    memset(g_saved_ok, 0, sizeof(g_saved_ok));
    return 0;
}
