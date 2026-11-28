/* gmb_graphics_sprite.c — sprite (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================ Sprite ================================ */

int gmb_gfx_sprite_load(const char* path)
{
    gmb_gfx_init();
    /* Anche in null mode lo stato sprite e' software (PNG decodificato, nessuna SDL): serve a rilevare le
     * collisioni al SYNC (ON SPRITE COLLISION) con id e rettangoli veri, uguali a quelli della grafica reale. */
    if (!path) return -1;
    if (g_gfx.next_sprite_id >= GMB_GFX_MAX_SPRITES) return -1;
    int id = g_gfx.next_sprite_id++;
    g_gfx.sprites[id].tex_id = -1;
    g_gfx.sprites[id].x = 0;
    g_gfx.sprites[id].y = 0;
    g_gfx.sprites[id].visible = 1;
    g_gfx.sprites[id].priority = 0;
    g_gfx.sprites[id].rotation = 0;
    g_gfx.sprites[id].scale_x = 1;
    g_gfx.sprites[id].scale_y = 1;
    g_gfx.sprites[id].in_use = 1;
    int tex = gmb_gfx_load_texture(path);
    g_gfx.sprites[id].tex_id = tex;
    return id;
}

/* Le impostazioni scritte su un MEMBRO di un gruppo sono ignorate: comanda la testa
 * (riferimento, «Sprite groups»). Sulla testa si ridispone il blocco. */
void gmb_gfx_sprite_set_position(int id, float x, float y)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    /* una posizione scritta dal programma sostituisce il MOVETO in corso */
    gmb_gfx_motion_cancel(id);
    g_gfx.sprites[id].x = x;
    g_gfx.sprites[id].y = y;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

void gmb_gfx_sprite_set_visible(int id, int visible)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    g_gfx.sprites[id].visible = visible;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

void gmb_gfx_sprite_set_priority(int id, int priority)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    g_gfx.sprites[id].priority = priority;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

void gmb_gfx_sprite_set_rotation(int id, double degrees)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    g_gfx.sprites[id].rotation = degrees;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

void gmb_gfx_sprite_set_scale(int id, double sx, double sy)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    g_gfx.sprites[id].scale_x = sx;
    g_gfx.sprites[id].scale_y = sy;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

/* Stato in lettura: 0 (o 0.0) se l'id non esiste, così i test non leggono memoria sporca. */
double gmb_gfx_sprite_x(int id)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0.0;
    return (double)g_gfx.sprites[id].x;
}

double gmb_gfx_sprite_y(int id)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0.0;
    return (double)g_gfx.sprites[id].y;
}

int gmb_gfx_sprite_visible(int id)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0;
    return g_gfx.sprites[id].visible;
}

int gmb_gfx_sprite_priority(int id)
{
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0;
    return g_gfx.sprites[id].priority;
}

/* Rotazione e scala EFFETTIVE di uno sprite: con una matrice affine 2x3 (a,b,c,d) vale la
 * sua decomposizione — rot = atan2(b,a), sx = |(a,b)|, sy = |(c,d)|, scala 0 → 1; la
 * traslazione (e,f) non si applica, la posizione resta s->x/s->y, perche' draw_texture
 * conosce solo rotazione e scala. Senza matrice valgono rotazione/scala esplicite.
 * UNICA sede della decomposizione (§0bis): la usano il disegno singolo, la collisione per
 * pixel (sprite_box) e la disposizione dei gruppi, quindi una matrice sulla testa dispone
 * il blocco con la stessa trasformazione con cui la testa viene disegnata. */
void gmb_gfx_sprite_rot_scale(const gmb_gfx_sprite_t* s, double* rot, double* sx, double* sy)
{
    if (s->has_matrix)
    {
        double a = s->matrix[0], b = s->matrix[1], c = s->matrix[2], d = s->matrix[3];
        double ex = sqrt(a * a + b * b);
        double ey = sqrt(c * c + d * d);
        *rot = atan2(b, a) * 180.0 / 3.14159265358979323846;
        *sx = ex > 0 ? ex : 1;
        *sy = ey > 0 ? ey : 1;
        return;
    }
    *rot = s->rotation;
    *sx = s->scale_x;
    *sy = s->scale_y;
}

void gmb_gfx_sprite_draw(int id)
{
    double rot, sx, sy;
    gmb_gfx_sprite_t* s;
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return;
    s = &g_gfx.sprites[id];
    if (!s->visible) return;
    if (gmb_gfx_group_is_member(id)) return;        /* lo disegna la testa */
    if (gmb_gfx_group_draw_block(id)) return;       /* testa di un gruppo: blocco intero */
    if (s->tex_id < 0) return;
    gmb_gfx_sprite_rot_scale(s, &rot, &sx, &sy);
    gmb_gfx_draw_texture(s->tex_id, (int)s->x, (int)s->y, rot, sx, sy);
}

int gmb_gfx_sprite_def(int id, int w, int h, const int* pixels)
{
    uint32_t* px;
    int tex;
    gmb_gfx_sprite_t* s;
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || w <= 0 || h <= 0 || !pixels) return -1;
    gmb_gfx_init();
    px = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!px) return -1;
    for (int i = 0; i < w * h; i++)
    {
        /* Contratto IGraphicsService.SpriteDef: 0 = trasparente, altrimenti 0xRRGGBB opaco.
         * Formato texture del motore: 0xAARRGGBB (png_decode). */
        unsigned int rgb = (unsigned int)pixels[i] & 0xFFFFFFu;
        px[i] = (rgb == 0) ? 0u : (0xFF000000u | rgb);
    }
    tex = gmb_gfx_register_rgba(px, w, h);
    if (tex < 0) { free(px); return -1; }
    s = &g_gfx.sprites[id];
    memset(s, 0, sizeof(*s));
    s->tex_id = tex;
    s->visible = 1;
    s->scale_x = 1;
    s->scale_y = 1;
    s->in_use = 1;
    if (id >= g_gfx.next_sprite_id) g_gfx.next_sprite_id = id + 1;
    return id;
}

/* La matrice si decompone in rotazione+scala (gmb_gfx_sprite_rot_scale), la stessa
 * trasformazione del disegno: su una TESTA di gruppo ridispone subito il blocco, come
 * rotazione e scala. Su un membro e' ignorata (comanda la testa), come rotazione/scala; la
 * matrice non viene salvata ne' ripristinata dallo scioglimento, ma resta immagazzinata e
 * torna a valere quando lo sprite esce dal gruppo. */
void gmb_gfx_sprite_set_matrix(int id, const double* m6)
{
    gmb_gfx_sprite_t* s;
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !m6 || !g_gfx.sprites[id].in_use) return;
    if (gmb_gfx_group_is_member(id)) return;
    s = &g_gfx.sprites[id];
    for (int i = 0; i < 6; i++) s->matrix[i] = m6[i];
    s->has_matrix = 1;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

int gmb_gfx_sprite_count(void)
{
    int n = 0;
    for (int i = 0; i < GMB_GFX_MAX_SPRITES; i++) if (g_gfx.sprites[i].in_use) n++;
    return n;
}

/* ============================ Sprite pixel / collisioni ============================ */

void gmb_gfx_sprite_get_pixels(int src_id, int dst_id)
{
    if (src_id <= 0 || src_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[src_id].in_use) return;
    if (dst_id <= 0 || dst_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[dst_id].in_use) return;
    int s = g_gfx.sprites[src_id].tex_id;
    int d = g_gfx.sprites[dst_id].tex_id;
    if (s <= 0 || s >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[s].in_use) return;
    if (d <= 0 || d >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[d].in_use) return;
    gmb_gfx_image_t* ds = &g_gfx.textures[s];
    gmb_gfx_image_t* dd = &g_gfx.textures[d];
    int w = ds->w < dd->w ? ds->w : dd->w;
    int h = ds->h < dd->h ? ds->h : dd->h;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            dd->pixels[(size_t)yy * dd->w + xx] = ds->pixels[(size_t)yy * ds->w + xx];
}

/* SPRITE GETPIXELS → array IMAGEGET [w, h, indici...]. Ritorna il numero di elementi. */
int gmb_gfx_sprite_pixels_to_array(int src_id, int* out, int max)
{
    if (gmb_gfx_null_mode()) {
        /* dummy VALIDO 1x1 bianco (parity NullGraphicsService.SpriteGetPixelsGpm) */
        (void)src_id;
        if (!out || max < 3) return 0;
        out[0] = 1; out[1] = 1; out[2] = 0x00FFFFFF;
        return 3;
    }
    if (src_id <= 0 || src_id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[src_id].in_use) return 0;
    int s = g_gfx.sprites[src_id].tex_id;
    if (s <= 0 || s >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[s].in_use) return 0;
    gmb_gfx_image_t* t = &g_gfx.textures[s];
    if (!t->pixels || max < 2 + t->w * t->h) return 0;
    out[0] = t->w;
    out[1] = t->h;
    {
        extern int gmb_gfx_find_color_index(uint32_t argb, int check_alpha);
        for (int yy = 0; yy < t->h; yy++)
            for (int xx = 0; xx < t->w; xx++)
            {
                /* Parità CommonGraphics.GetSpriteImagePixels: alpha<128 → -1 (trasparente),
                 * altrimenti indice palette più vicino (mai packed RGB → overflow SHORT). */
                uint32_t pix = t->pixels[(size_t)yy * t->w + xx];
                out[2 + yy * t->w + xx] = gmb_gfx_find_color_index(pix, 1);
            }
    }
    return 2 + t->w * t->h;
}

/* Inviluppo AABB di uno sprite.
 * Sprite SPENTO (SPRITE OFF) non partecipa alle collisioni: è il contratto già applicato dal
 * rilevamento al SYNC (`gfx_detect_collisions` salta !visible) e dalle due implementazioni
 * gestite (Sprites/Null: «0 = no collisione o sprite inesistente/nascosto»). Senza, `COLLIDE`
 * dopo `SPRITE OFF` dava -1 su C99/VM-C/PCode e 0 su IL.
 * tex_id: -1 = nessuna texture; gli id VALIDI partono da 0 (register_image fa
 * `next_tex_id++`). Il vecchio `> 0` scartava proprio la texture id 0, quindi il PRIMO
 * sprite caricato non collideva MAI. */
static int sprite_aabb(int id, gmb_gfx_sprite_t** out_s, gmb_gfx_image_t** out_t)
{
    gmb_gfx_sprite_t* s;
    gmb_gfx_image_t* t;
    if (id <= 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0;
    s = &g_gfx.sprites[id];
    if (!s->visible) return 0;
    if (s->tex_id < 0 || s->tex_id >= GMB_GFX_MAX_TEXTURES) return 0;
    t = &g_gfx.textures[s->tex_id];
    if (!t->in_use) return 0;
    *out_s = s;
    *out_t = t;
    return 1;
}

/* Estende l'oggetto di `*first` al blocco che lo contiene: una testa di gruppo vale w*h
 * sprite (un gruppo collide come un oggetto unico e le collisioni riferiscono la testa). */
static void object_span(int* first, int* count)
{
    int cols = 0, rows = 0;
    int head = gmb_gfx_group_head_of(*first);
    if (head != 0 && gmb_gfx_group_size(head, &cols, &rows) && cols > 0 && rows > 0)
    {
        *first = head;
        *count = cols * rows;
        return;
    }
    *count = 1;
}

int gmb_gfx_collide(int id1, int id2)
{
    int h1, h2, f1 = id1, n1 = 1, f2 = id2, n2 = 1;
    if (id1 <= 0 || id1 >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id1].in_use) return 0;
    if (id2 <= 0 || id2 >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id2].in_use) return 0;
    h1 = gmb_gfx_group_head_of(id1);
    h2 = gmb_gfx_group_head_of(id2);
    /* membri di uno stesso gruppo non collidono tra loro (e la testa con se stessa e' lo
     * stesso oggetto) */
    if (h1 != 0 && h1 == h2) return 0;
    if (h1 != 0) id1 = h1;
    if (h2 != 0) id2 = h2;
    object_span(&f1, &n1);
    object_span(&f2, &n2);
    for (int a = f1; a < f1 + n1; a++)
        for (int b = f2; b < f2 + n2; b++)
        {
            gmb_gfx_sprite_t *sa, *sb;
            gmb_gfx_image_t *ta, *tb;
            float aw, ah, bw, bh, ax, ay, bx, by;
            if (!sprite_aabb(a, &sa, &ta) || !sprite_aabb(b, &sb, &tb)) continue;
            /* (x,y) dello sprite è il suo CENTRO: il renderer disegna a x - w/2 (vedi
             * gmb_graphics_texture.c). Qui il rettangolo va quindi costruito attorno al
             * centro, come fanno il motore gestito (Sprites.BoundingBox) e il rilevamento
             * eventi. Prima si usava (x,y) come angolo in alto a sinistra: con texture di
             * dimensioni DIVERSE (aereo 32×32 vs esplosione 48×48) il rettangolo era
             * spostato di mezzo sprite → collisioni e fiamme fuori posto. */
            aw = (float)ta->w; ah = (float)ta->h;
            bw = (float)tb->w; bh = (float)tb->h;
            ax = sa->x - aw / 2.0f; ay = sa->y - ah / 2.0f;
            bx = sb->x - bw / 2.0f; by = sb->y - bh / 2.0f;
            if (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by) return -1;
        }
    return 0;
}

/* ================== Collisione per pixel OPACO ==================
 * `gmb_gfx_collide` qui sopra e' il test AABB ed e' il contratto delle 4 gambe (IL/PCode/VM-C/
 * C99Native): resta invariato. Questa variante serve al caso che l'AABB sbaglia — due sprite
 * con buchi trasparenti che si sovrappongono nel rettangolo ma non in nessun pixel VISIBILE.
 *
 * La geometria e' quella di `gmb_gfx_draw_texture` (centro intero, `dw = (int)(w*|scale|)`,
 * rotazione attorno al centro, decomposizione della matrice come `gmb_gfx_sprite_draw`) e la
 * soglia di opacita' e' quella del renderer (`alpha < 8` = trasparente). Un pixel e' opaco se
 * il texel piu' vicino lo e' (nearest, come il ramo non ruotato del renderer): la collisione
 * guarda il pixel che si vede, non l'inviluppo. */

/* Soglia IDENTICA al renderer (gmb_graphics_texture.c: `alpha < 8` salta il texel) e al
 * tilemap: con una soglia diversa si colliderebbe dove non si vede nulla. */
#define GMB_GFX_OPAQUE_ALPHA 8

typedef struct gmb_sprite_box {
    const gmb_gfx_image_t* tex;
    int cx, cy;      /* centro intero: sprite_draw passa (int)s->x a draw_texture */
    int dw, dh;      /* dimensione a schermo, stessa formula del renderer */
    int bw, bh;      /* inviluppo: dw/dh se non ruotato, altrimenti l'inviluppo ruotato */
    double rot;      /* gradi */
} gmb_sprite_box_t;

/* Geometria di disegno di uno sprite. 0 = lo sprite non puo' produrre pixel: id fuori
 * intervallo, spento (SPRITE OFF), senza texture, o scala 0 (il renderer esce subito). */
static int sprite_box(int id, gmb_sprite_box_t* out)
{
    gmb_gfx_sprite_t* s;
    gmb_gfx_image_t* t;
    double sx, sy, rot;
    int dw, dh;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (id < 0 || id >= GMB_GFX_MAX_SPRITES || !g_gfx.sprites[id].in_use) return 0;
    s = &g_gfx.sprites[id];
    if (!s->visible) return 0;
    if (s->tex_id < 0 || s->tex_id >= GMB_GFX_MAX_TEXTURES) return 0;
    t = &g_gfx.textures[s->tex_id];
    if (!t->in_use || !t->pixels) return 0;
    /* stessa decomposizione di gmb_gfx_sprite_draw (matrice -> rot+scala) */
    gmb_gfx_sprite_rot_scale(s, &rot, &sx, &sy);
    dw = (int)(t->w * fabs(sx));
    dh = (int)(t->h * fabs(sy));
    if (dw <= 0 || dh <= 0) return 0;
    out->tex = t;
    out->cx = (int)s->x;
    out->cy = (int)s->y;
    out->dw = dw;
    out->dh = dh;
    out->rot = rot;
    if (fabs(rot) < 0.001) { out->bw = dw; out->bh = dh; }
    else
    {
        double rad = rot * M_PI / 180.0;
        double cosR = cos(rad), sinR = sin(rad);
        out->bw = (int)ceil(fabs(dw * cosR) + fabs(dh * sinR));
        out->bh = (int)ceil(fabs(dw * sinR) + fabs(dh * cosR));
        if (out->bw <= 0) out->bw = dw;
        if (out->bh <= 0) out->bh = dh;
    }
    return 1;
}

/* Il pixel LOGICO (px,py), centro px+0.5 / py+0.5, e' OPACO in questo sprite? */
static int box_opaque(const gmb_sprite_box_t* b, int px, int py)
{
    const gmb_gfx_image_t* t = b->tex;
    double X = (double)px + 0.5;
    double Y = (double)py + 0.5;
    double u, v;   /* coordinate nel rettangolo NON ruotato, in pixel di destinazione */
    int tx, ty;
    if (fabs(b->rot) < 0.001)
    {
        u = X - ((double)b->cx - (double)b->dw / 2.0);
        v = Y - ((double)b->cy - (double)b->dh / 2.0);
    }
    else
    {
        /* inversa della rotazione del renderer: src_cx = rx*cos + ry*sin */
        double rad = b->rot * M_PI / 180.0;
        double cosR = cos(rad), sinR = sin(rad);
        double rx = X - (double)b->cx;
        double ry = Y - (double)b->cy;
        u = rx * cosR + ry * sinR + (double)b->dw / 2.0;
        v = -rx * sinR + ry * cosR + (double)b->dh / 2.0;
    }
    if (u < 0.0 || v < 0.0 || u >= (double)b->dw || v >= (double)b->dh) return 0;
    /* nearest come il ramo non ruotato del renderer: xx * t->w / dw con xx intero */
    tx = (int)floor(u) * t->w / b->dw;
    ty = (int)floor(v) * t->h / b->dh;
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= t->w) tx = t->w - 1;
    if (ty >= t->h) ty = t->h - 1;
    return ((t->pixels[(size_t)ty * t->w + tx] >> 24) & 0xFF) >= GMB_GFX_OPAQUE_ALPHA;
}

static int pixels_pair(int id1, int id2)
{
    gmb_sprite_box_t a, b;
    int ax0, ay0, bx0, by0;
    int ix0, iy0, ix1, iy1;
    if (!sprite_box(id1, &a) || !sprite_box(id2, &b)) return 0;
    ax0 = a.cx - a.bw / 2;
    ay0 = a.cy - a.bh / 2;
    bx0 = b.cx - b.bw / 2;
    by0 = b.cy - b.bh / 2;
    ix0 = ax0 > bx0 ? ax0 : bx0;
    iy0 = ay0 > by0 ? ay0 : by0;
    ix1 = (ax0 + a.bw) < (bx0 + b.bw) ? (ax0 + a.bw) : (bx0 + b.bw);
    iy1 = (ay0 + a.bh) < (by0 + b.bh) ? (ay0 + a.bh) : (by0 + b.bh);
    if (ix0 >= ix1 || iy0 >= iy1) return 0;
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++)
            if (box_opaque(&a, x, y) && box_opaque(&b, x, y)) return -1;
    return 0;
}

int gmb_gfx_collide_pixels(int id1, int id2)
{
    int h1, h2, f1 = id1, n1 = 1, f2 = id2, n2 = 1;
    h1 = gmb_gfx_group_head_of(id1);
    h2 = gmb_gfx_group_head_of(id2);
    /* stesso gruppo: i membri non collidono tra loro, e la testa e' il gruppo */
    if (h1 != 0 && h1 == h2) return 0;
    if (h1 != 0) id1 = h1;
    if (h2 != 0) id2 = h2;
    object_span(&f1, &n1);
    object_span(&f2, &n2);
    for (int a = f1; a < f1 + n1; a++)
        for (int b = f2; b < f2 + n2; b++)
            if (pixels_pair(a, b)) return -1;
    return 0;
}

