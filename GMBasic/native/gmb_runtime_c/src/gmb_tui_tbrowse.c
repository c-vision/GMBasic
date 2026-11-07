/* ============================================================================
 * gmb_tui_tbrowse.c — TBrowse (porting fedele di ClipperLib/TBrowse/TBrowse.cs).
 * ============================================================================ */
#include "gmb_tui_tbrowse.h"
#include "gmb_ui_gfx.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <strings.h>

/* Wrapper Control (TBrowse : Control) */
static void bc_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf);
static bool bc_key(void* u, gmb_control_t* c, int key);
static bool bc_click(void* u, gmb_control_t* c, int x, int y);
static bool bc_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a);

/* ===================== VecDataSource ===================== */

void gmb_vec_ds_make(gmb_vec_ds_t* v, int rows, int cols)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    if (rows > 64) rows = 64;
    if (cols > GMB_BROWSE_MAX_COLS) cols = GMB_BROWSE_MAX_COLS;
    if (rows < 0) rows = 0;
    if (cols < 0) cols = 0;
    v->rows = rows;
    v->cols = cols;
    v->current = 0;
}

void gmb_vec_ds_set(gmb_vec_ds_t* v, int row, int col, const char* val)
{
    if (!v || row < 0 || row >= v->rows || col < 0 || col >= v->cols) return;
    snprintf(v->cells[row][col], sizeof(v->cells[row][col]), "%s", val ? val : "");
}

static int vec_get_value(void* user, int row, int col, char* out, int cap)
{
    gmb_vec_ds_t* v = (gmb_vec_ds_t*)user;
    if (!out || cap <= 0) return 0;
    if (row < v->rows && col < v->cols)
        snprintf(out, cap, "%s", v->cells[row][col]);
    else
        out[0] = '\0';
    return (int)strlen(out);
}

static int vec_row_count(void* user)
{
    return ((gmb_vec_ds_t*)user)->rows;
}

static int vec_skip(void* user, int delta)
{
    gmb_vec_ds_t* v = (gmb_vec_ds_t*)user;
    int new_pos = v->current + delta;
    int max = v->rows > 0 ? v->rows - 1 : 0;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > max) new_pos = max;
    int actual = new_pos - v->current;
    v->current = new_pos;
    return actual;
}

static void vec_go_top(void* user) { ((gmb_vec_ds_t*)user)->current = 0; }

static void vec_go_bottom(void* user)
{
    gmb_vec_ds_t* v = (gmb_vec_ds_t*)user;
    if (v->rows > 0) v->current = v->rows - 1;
}

static int vec_bookmark(void* user) { return ((gmb_vec_ds_t*)user)->current; }

static void vec_go_to(void* user, int n)
{
    gmb_vec_ds_t* v = (gmb_vec_ds_t*)user;
    if (v->rows > 0) v->current = n < v->rows - 1 ? n : v->rows - 1;
    else v->current = 0;
}

static void vec_set_value(void* user, int row, int col, const char* val)
{
    gmb_vec_ds_t* v = (gmb_vec_ds_t*)user;
    if (!v || row < 0 || row >= v->rows || col < 0 || col >= v->cols) return;
    snprintf(v->cells[row][col], sizeof(v->cells[row][col]), "%s", val ? val : "");
}

void gmb_vec_ds_datasource(gmb_vec_ds_t* v, gmb_browse_datasource_t* out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->user = v;
    out->get_value = vec_get_value;
    out->row_count = vec_row_count;
    out->skip = vec_skip;
    out->go_top = vec_go_top;
    out->go_bottom = vec_go_bottom;
    out->bookmark = vec_bookmark;
    out->go_to = vec_go_to;
    out->set_value = vec_set_value;
}

/* ===================== TBrowse core ===================== */

/* Render PIXEL del TBrowse: stessa geometria del render a celle (header, separatore,
 * righe, riga corrente evidenziata) ma con testo proporzionale su card del tema. */
static void tbrowse_draw_px(void* u, gmb_control_t* c)
{
    gmb_browse_t* b = (gmb_browse_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    int x = c->x, y = c->y, w = c->width, h = c->height;
    if (w < 24 || h < 24) return;
    int soft = (st.shape == 1 && st.radius >= 12);
    int rr = soft ? 12 : 4;
    unsigned fill = (st.skin == 2) ? st.fill : 0xFFFFFFFFu;
    gmb_gfx_fill_rounded_rect(x, y, w, h, rr, fill);
    gmb_gfx_stroke_rounded_rect(x, y, w, h, rr, st.border);

    int rh = (st.row_h > 0 ? st.row_h : 22);
    if (rh < 12) rh = 12;
    int pad = soft ? 8 : 4;
    float sc = gmb_gfx_render_scale(); if (sc <= 0) sc = 1;
    int th = (int)(gmb_gfx_text_height() / sc);
    int ty = y + pad;
    char line[2048];

    if (b->show_header)
    {
        gmb_gfx_fill_rounded_rect(x + 4, ty, w - 8, rh, 2, (st.border & 0xFFFFFFu) | 0x33000000u);
        gmb_browse_render_header(b, line, (int)sizeof(line));
        gmb_gfx_draw_text(line, x + pad, ty + (rh - th) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        ty += rh;
        gmb_gfx_fill_rounded_rect(x + pad, ty, w - 2 * pad, 1, 0, st.border);
    }

    int cur_disp = b->current_row - b->top_row;
    int dr = 0;
    while (1)
    {
        int ry = ty + dr * rh;
        if (ry + rh > y + h - pad) break;
        char row[2048];
        if (gmb_browse_render_row(b, dr, row, (int)sizeof(row)) <= 0) break;
        if (dr == cur_disp && b->highlight_row)
            gmb_gfx_fill_rounded_rect(x + 4, ry, w - 8, rh, 2, (st.accent & 0xFFFFFFu) | 0x33000000u);
        gmb_gfx_draw_text(row, x + pad, ry + (rh - th) / 2, 0,
                          (int)(0x40000000u | (st.text & 0xFFFFFFu)));
        dr++;
        if (b->top_row + dr >= gmb_browse_row_count(b)) break;
    }
}

gmb_browse_t* gmb_browse_new(const gmb_rect_t* area, int col_count, const gmb_browse_column_t* cols,
                             const gmb_browse_datasource_t* ds)
{
    gmb_browse_t* b = (gmb_browse_t*)calloc(1, sizeof(gmb_browse_t));
    if (!b) return NULL;
    if (area) b->area = *area;
    if (col_count > GMB_BROWSE_MAX_COLS) col_count = GMB_BROWSE_MAX_COLS;
    b->col_count = col_count < 0 ? 0 : col_count;
    if (cols) for (int i = 0; i < b->col_count; i++) { b->columns[i] = cols[i]; b->columns[i].visible = true; }
    if (ds) b->ds = *ds;
    b->sort_col = -1;
    b->sort_dir = 0;
    b->sort_nkeys = 0;
    b->mouse_enabled = true;
    b->show_header = true;
    b->show_border = true;
    b->highlight_row = true;
    b->header_colors.foreground = GMB_CELLCOLOR_WHITE;
    b->header_colors.background = GMB_CELLCOLOR_BLUE;
    b->header_highlight_colors.foreground = GMB_CELLCOLOR_BLACK;
    b->header_highlight_colors.background = GMB_CELLCOLOR_WHITE;
    b->row_colors.foreground = GMB_CELLCOLOR_WHITE;
    b->row_colors.background = GMB_CELLCOLOR_BLACK;
    b->highlight_colors.foreground = GMB_CELLCOLOR_BLACK;
    b->highlight_colors.background = GMB_CELLCOLOR_CYAN;
    b->auto_lite = true;
    strcpy(b->col_sep, "\xE2\x94\x82");   /* │ */
    strcpy(b->head_sep, "\xE2\x94\x80");  /* ─ */
    strcpy(b->foot_sep, "\xE2\x94\x80");  /* ─ */
    gmb_control_init(&b->ctrl, "", b->area.x, b->area.y, b->area.w, b->area.h, bc_draw, bc_key);
    b->ctrl.ctrl_kind = GMB_CK_TBROWSE;   /* per CONTROL SET/GET (showheader/toprow/sort/filter…) */
    b->ctrl.draw_px_fn = tbrowse_draw_px; /* grafica: card + header + righe, no fallback a celle */
    b->ctrl.user = b;
    b->ctrl.handle_click_fn = bc_click;
    b->ctrl.handle_mouse_fn = bc_mouse;
    return b;
}

void gmb_browse_free(gmb_browse_t* b)
{
    if(!b) return;
    free(b->view);
    free(b);
}

int gmb_browse_page_size(const gmb_browse_t* b)
{
    int available = b->area.h;
    if (b->show_header) available -= 2;
    if (b->show_border) available -= 2;
    if (b->has_footer) available -= 2;
    if (available < 1) available = 1;
    return available;
}


/* ===== view layer: sort/filter (indice display->data row) ===== */
static int b_row(const gmb_browse_t* b, int i){ return (b->view && i>=0 && i<b->view_count) ? b->view[i] : i; }

static void b_get(const gmb_browse_t* b, int drow, int col, char* out, int cap){
    out[0]=0; if(!b->ds.get_value) return;
    b->ds.get_value(b->ds.user, b_row(b,drow), col, out, cap);
}
static int ci_contains(const char* hay, const char* needle){
    if(!needle||!*needle) return 1; if(!hay) return 0;
    size_t nl=strlen(needle); if(!nl) return 1;
    for(const char* p=hay; *p; p++){ size_t k=0; while(k<nl && p[k] && tolower((unsigned char)p[k])==tolower((unsigned char)needle[k])) k++; if(k==nl) return 1; }
    return 0;
}
static int cmp_cell(const gmb_browse_t* b, int drow, int col, int other){
    char a[256], c[256]; a[0]=c[0]=0;
    if(b->ds.get_value){ b->ds.get_value(b->ds.user,drow,col,a,sizeof a); b->ds.get_value(b->ds.user,other,col,c,sizeof c); }
    gmb_data_type_t dt = (col>=0 && col<b->col_count) ? b->columns[col].data_type : GMB_DT_TEXT;
    if(dt==GMB_DT_INT||dt==GMB_DT_FLOAT||dt==GMB_DT_BOOL||dt==GMB_DT_DATE){
        double x=atof(a), y=atof(c); if(x<y) return -1; if(x>y) return 1; return 0;
    }
    return strcasecmp(a,c);
}
static void b_rebuild_view(gmb_browse_t* b){
    int total = b->ds.row_count ? b->ds.row_count(b->ds.user) : 0;
    free(b->view); b->view=NULL; b->view_count=0;
    if(total<=0) return;
    b->view=(int*)malloc(sizeof(int)*(size_t)total); if(!b->view) return;
    int n=0;
    for(int r=0;r<total;r++){
        if(b->filter[0]){
            int ok=0;
            for(int col=0; col<b->col_count && !ok; col++){
                if(!b->columns[col].visible) continue;
                char v[256]; b->ds.get_value(b->ds.user, r, col, v, sizeof v);
                if(ci_contains(v, b->filter)) ok=1;
            }
            if(!ok) continue;
        }
        b->view[n++]=r;
    }
    b->view_count=n;
    if(b->sort_nkeys>0){
        for(int i=1;i<b->view_count;i++){          /* insertion sort multi-chiave (view piccola) */
            int key=b->view[i], j=i-1;
            while(j>=0){
                int c=0;
                for(int k=0;k<b->sort_nkeys && c==0;k++){
                    c=cmp_cell(b, b->view[j], b->sort_cols[k], key);
                    if(b->sort_dirs[k]) c=-c;
                }
                if(c>0) { b->view[j+1]=b->view[j]; j--; } else break;
            }
            b->view[j+1]=key;
        }
    }
}
void gmb_browse_sort(gmb_browse_t* b, int col, int desc){
    if(!b) return;
    if(col<0){ b->sort_nkeys=0; b->sort_col=-1; b->sort_dir=0; }
    else {
        b->sort_cols[0]=col; b->sort_dirs[0]=desc?1:0; b->sort_nkeys=1;
        b->sort_col=col; b->sort_dir=desc?1:0;
    }
    b_rebuild_view(b); b->current_row=0; b->top_row=0;
}
/* multi-colonna: "col[:asc|desc],col[:asc|desc],..." (fino a GMB_BROWSE_MAX_SORT chiavi). */
void gmb_browse_set_sortkeys(gmb_browse_t* b, const char* spec){
    if(!b) return;
    b->sort_nkeys=0;
    const char* p = spec ? spec : "";
    while(*p && b->sort_nkeys < GMB_BROWSE_MAX_SORT){
        while(*p==' '||*p==','||*p==';') p++;
        if(!*p) break;
        int col = atoi(p);
        while(*p && *p!=',' && *p!=';' && *p!=':') p++;
        int dir = 0;
        if(*p==':'){
            p++;
            if(!strncasecmp(p,"desc",4) || *p=='1' || *p=='d' || *p=='D') dir=1;
            while(*p && *p!=',' && *p!=';') p++;
        }
        b->sort_cols[b->sort_nkeys]=col;
        b->sort_dirs[b->sort_nkeys]=dir;
        b->sort_nkeys++;
    }
    b->sort_col = b->sort_nkeys>0 ? b->sort_cols[0] : -1;
    b->sort_dir = b->sort_nkeys>0 ? b->sort_dirs[0] : 0;
    b_rebuild_view(b); b->current_row=0; b->top_row=0;
}
int gmb_browse_sortkeys(const gmb_browse_t* b, char* out, int cap){
    if(!b) return 0;
    if(out && cap>0){
        int off=0; out[0]=0;
        for(int k=0;k<b->sort_nkeys && off<cap-1;k++){
            off += snprintf(out+off, (size_t)(cap-off), "%s%d:%s", k?",":"", b->sort_cols[k],
                            b->sort_dirs[k]?"desc":"asc");
        }
        if(off>cap-1) out[cap-1]=0;
    }
    return b->sort_nkeys;
}
/* CSV della vista (tutte le colonne), header incluso; quoting minimale (RFC4180). */
static int csv_emit(char* out, int cap, int off, const char* v){
    const char* s = v ? v : "";
    int need_q = 0;
    for(const char* p=s; *p; p++) if(*p==','||*p=='"'||*p=='\n'||*p=='\r'){ need_q=1; break; }
    if(off<cap-1 && need_q) out[off++]='"';
    for(const char* p=s; *p && off<cap-1; p++){
        if(*p=='"'){ if(off<cap-2){ out[off++]='"'; out[off++]='"'; } }
        else out[off++]=*p;
    }
    if(off<cap-1 && need_q) out[off++]='"';
    if(off<cap) out[off]=0;
    return off;
}
int gmb_browse_csv(const gmb_browse_t* b, char* out, int cap){
    if(!b||!out||cap<2) return 0;
    int off=0; out[0]=0;
    for(int col=0; col<b->col_count; col++){
        if(off>0 && off<cap-1) out[off++]=',';
        off = csv_emit(out, cap, off, b->columns[col].header);
    }
    if(off<cap-1) out[off++]='\n';
    if(off<cap) out[off]=0;
    int rc = gmb_browse_row_count(b);
    for(int r=0;r<rc;r++){
        int first=1;
        for(int col=0; col<b->col_count; col++){
            if(!first && off<cap-1) out[off++]=',';
            first=0;
            char v[256]; v[0]=0;
            if(b->ds.get_value) b->ds.get_value(b->ds.user, b_row(b,r), col, v, sizeof v);
            off = csv_emit(out, cap, off, v);
        }
        if(off<cap-1) out[off++]='\n';
        if(off<cap) out[off]=0;
        if(off>=cap-1) break;
    }
    return off;
}
void gmb_browse_set_filter(gmb_browse_t* b, const char* text){
    if(!b) return; snprintf(b->filter,sizeof b->filter,"%s", text?text:""); b_rebuild_view(b); b->current_row=0; b->top_row=0;
}
void gmb_browse_clear_filter(gmb_browse_t* b){ gmb_browse_set_filter(b, ""); }
void gmb_browse_resize_col(gmb_browse_t* b, int col, int w){
    if(!b||col<0||col>=b->col_count) return; if(w<2)w=2; b->columns[col].width=w;
}
int  gmb_browse_sort_col(const gmb_browse_t* b){ return b?b->sort_col:-1; }
int  gmb_browse_sort_dir(const gmb_browse_t* b){ return b?b->sort_dir:0; }
const char* gmb_browse_filter(const gmb_browse_t* b){ return b?b->filter:""; }

int gmb_browse_row_count(const gmb_browse_t* b)
{
    if(!b) return 0;
    if(!b->view){
        int total = b->ds.row_count ? b->ds.row_count(b->ds.user) : 0;
        if(!b->filter[0] && b->sort_col<0) return total;   /* nessuna view necessaria */
        b_rebuild_view((gmb_browse_t*)b);
        return b->view_count;
    }
    return b->view_count;
}

int gmb_browse_current_value(const gmb_browse_t* b, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!b->ds.get_value) { out[0] = '\0'; return 0; }
    return b->ds.get_value(b->ds.user, b_row(b, b->current_row), b->current_col, out, cap);
}

int gmb_browse_inner_width(const gmb_browse_t* b)
{
    return b->area.w - (b->show_border ? 2 : 0);
}

int gmb_browse_visible_col_count(const gmb_browse_t* b)
{
    int width_used = 0;
    int count = 0;
    int inner = gmb_browse_inner_width(b);
    for (int i = 0; i < b->col_count; i++)
    {
        if (!b->columns[i].visible) continue;
        if (i < b->frozen_cols) width_used += b->columns[i].width + 1;
    }
    for (int i = b->frozen_cols + b->left_col; i < b->col_count; i++)
    {
        if (!b->columns[i].visible) continue;
        width_used += b->columns[i].width + 1;
        if (width_used > inner) break;
        count++;
    }
    return count + b->frozen_cols;
}

int gmb_browse_displayed_cols(const gmb_browse_t* b, int* out, int cap)
{
    int n = 0;
    int width = 0;
    int max = gmb_browse_inner_width(b);
    for (int i = 0; i < b->col_count && i < b->frozen_cols; i++)
    {
        if (!b->columns[i].visible) continue;
        if (n < cap) out[n++] = i;
        width += b->columns[i].width + 1;
    }
    int scroll_start = b->frozen_cols + b->left_col;
    for (int i = b->frozen_cols; i < b->col_count; i++)
    {
        if (!b->columns[i].visible) continue;
        if (i < scroll_start) continue;
        int needed = b->columns[i].width + 1;
        if (width + needed > max && n > 0) break;
        if (n < cap) out[n++] = i;
        width += needed;
    }
    return n;
}

int gmb_browse_max_left_col(const gmb_browse_t* b)
{
    int scrollable[GMB_BROWSE_MAX_COLS];
    int sc = 0;
    for (int i = 0; i < b->col_count; i++)
    {
        if (!b->columns[i].visible) continue;
        if (i >= b->frozen_cols) scrollable[sc++] = i;
    }
    if (sc == 0) return 0;
    int frozen_w = 0;
    for (int i = 0; i < b->col_count && i < b->frozen_cols; i++)
    {
        if (!b->columns[i].visible) continue;
        frozen_w += b->columns[i].width + 1;
    }
    int usable = gmb_browse_inner_width(b) - frozen_w;
    if (usable <= 0) return 0;
    int used = 0;
    int fit = 0;
    for (int i = 0; i < sc; i++)
    {
        int w = b->columns[scrollable[i]].width + 1;
        if (used + w > usable) break;
        used += w;
        fit++;
    }
    if (fit < 1) fit = 1;
    int m = sc - fit;
    return m < 0 ? 0 : m;
}

int gmb_browse_scrollable_page(const gmb_browse_t* b)
{
    int shown[GMB_BROWSE_MAX_COLS];
    int n = gmb_browse_displayed_cols(b, shown, GMB_BROWSE_MAX_COLS);
    int frozen_shown = 0;
    for (int i = 0; i < n; i++)
        if (shown[i] < b->frozen_cols) frozen_shown++;
    int p = n - frozen_shown;
    return p < 1 ? 1 : p;
}

/* ===================== navigazione ===================== */

void gmb_browse_move_down(gmb_browse_t* b)
{
    int total = gmb_browse_row_count(b);
    if (total == 0) return;
    if (b->current_row < total - 1)
    {
        b->current_row++;
        if (b->current_row >= b->top_row + gmb_browse_page_size(b))
            b->top_row++;
    }
}

void gmb_browse_move_up(gmb_browse_t* b)
{
    if (b->current_row > 0)
    {
        b->current_row--;
        if (b->current_row < b->top_row)
            b->top_row = b->current_row;
    }
}

void gmb_browse_page_down(gmb_browse_t* b)
{
    int page = gmb_browse_page_size(b);
    int total = gmb_browse_row_count(b);
    if (total == 0) return;
    b->current_row = b->current_row + page < total - 1 ? b->current_row + page : total - 1;
    int max_top = total - page;
    if (max_top < 0) max_top = 0;
    b->top_row = b->top_row + page < max_top ? b->top_row + page : max_top;
}

void gmb_browse_page_up(gmb_browse_t* b)
{
    int page = gmb_browse_page_size(b);
    b->current_row = b->current_row - page > 0 ? b->current_row - page : 0;
    b->top_row = b->top_row - page > 0 ? b->top_row - page : 0;
}

void gmb_browse_go_top(gmb_browse_t* b)
{
    if (b->ds.go_top) b->ds.go_top(b->ds.user);
    b->current_row = 0;
    b->top_row = 0;
}

void gmb_browse_go_bottom(gmb_browse_t* b)
{
    if (b->ds.go_bottom) b->ds.go_bottom(b->ds.user);
    int total = gmb_browse_row_count(b);
    if (total > 0)
    {
        b->current_row = total - 1;
        int max_top = total - gmb_browse_page_size(b);
        b->top_row = max_top > 0 ? max_top : 0;
    }
}

void gmb_browse_move_right(gmb_browse_t* b)
{
    int vis = 0;
    for (int i = 0; i < b->col_count; i++)
        if (b->columns[i].visible) vis++;
    if (b->current_col < vis - 1)
    {
        b->current_col++;
        if (b->current_col >= b->left_col + gmb_browse_visible_col_count(b))
            b->left_col++;
    }
}

void gmb_browse_move_left(gmb_browse_t* b)
{
    if (b->current_col > 0)
    {
        b->current_col--;
        if (b->current_col < b->left_col + b->frozen_cols)
        {
            if (b->left_col > 0)
                b->left_col--;
        }
    }
}

void gmb_browse_up(gmb_browse_t* b, int n)
{
    for (int i = 0; i < n; i++) gmb_browse_move_up(b);
}

void gmb_browse_down(gmb_browse_t* b, int n)
{
    for (int i = 0; i < n; i++) gmb_browse_move_down(b);
}

void gmb_browse_left(gmb_browse_t* b, int n) { gmb_browse_pan_left(b, n); }
void gmb_browse_right(gmb_browse_t* b, int n) { gmb_browse_pan_right(b, n); }
void gmb_browse_page_left(gmb_browse_t* b) { gmb_browse_pan_left(b, gmb_browse_scrollable_page(b)); }
void gmb_browse_page_right(gmb_browse_t* b) { gmb_browse_pan_right(b, gmb_browse_scrollable_page(b)); }

void gmb_browse_top(gmb_browse_t* b) { b->current_row = b->top_row; }

void gmb_browse_bottom(gmb_browse_t* b)
{
    int total = gmb_browse_row_count(b);
    if (total == 0) return;
    int last = b->top_row + gmb_browse_page_size(b) - 1;
    b->current_row = last < total - 1 ? last : total - 1;
}

void gmb_browse_goto(gmb_browse_t* b, int row, int col)
{
    int total = gmb_browse_row_count(b);
    b->current_row = total == 0 ? 0 : (row < total - 1 ? row : total - 1);
    int vcols = 0;
    for (int i = 0; i < b->col_count; i++)
        if (b->columns[i].visible) vcols++;
    b->current_col = vcols == 0 ? 0 : (col < vcols - 1 ? col : vcols - 1);
}

int gmb_browse_row_pos(const gmb_browse_t* b) { return b->current_row; }

void gmb_browse_set_row_pos(gmb_browse_t* b, int r)
{
    int total = gmb_browse_row_count(b);
    b->current_row = total == 0 ? 0 : (r < total - 1 ? r : total - 1);
}

int gmb_browse_col_pos(const gmb_browse_t* b) { return b->current_col; }

void gmb_browse_set_col_pos(gmb_browse_t* b, int c)
{
    int vcols = 0;
    for (int i = 0; i < b->col_count; i++)
        if (b->columns[i].visible) vcols++;
    b->current_col = vcols == 0 ? 0 : (c < vcols - 1 ? c : vcols - 1);
}

int gmb_browse_left_visible(const gmb_browse_t* b)
{
    int d[GMB_BROWSE_MAX_COLS];
    int n = gmb_browse_displayed_cols(b, d, GMB_BROWSE_MAX_COLS);
    return n > 0 ? d[0] : 0;
}

int gmb_browse_right_visible(const gmb_browse_t* b)
{
    int d[GMB_BROWSE_MAX_COLS];
    int n = gmb_browse_displayed_cols(b, d, GMB_BROWSE_MAX_COLS);
    return n > 0 ? d[n - 1] : 0;
}

int gmb_browse_top_visible(const gmb_browse_t* b) { return b->top_row; }

int gmb_browse_bottom_visible(const gmb_browse_t* b)
{
    int total = gmb_browse_row_count(b);
    if (total == 0) return 0;
    int last = b->top_row + gmb_browse_page_size(b) - 1;
    return last < total - 1 ? last : total - 1;
}

/* ===================== pan ===================== */

void gmb_browse_pan_home(gmb_browse_t* b) { b->left_col = 0; }
void gmb_browse_pan_end(gmb_browse_t* b) { b->left_col = gmb_browse_max_left_col(b); }
void gmb_browse_pan_left(gmb_browse_t* b, int n)
{
    b->left_col = b->left_col - n > 0 ? b->left_col - n : 0;
}
void gmb_browse_pan_right(gmb_browse_t* b, int n)
{
    int m = gmb_browse_max_left_col(b);
    b->left_col = b->left_col + n < m ? b->left_col + n : m;
}
void gmb_browse_hill_up(gmb_browse_t* b)
{
    int h = gmb_browse_page_size(b) / 2;
    if (h < 1) h = 1;
    gmb_browse_up(b, h);
}
void gmb_browse_hill_down(gmb_browse_t* b)
{
    int h = gmb_browse_page_size(b) / 2;
    if (h < 1) h = 1;
    gmb_browse_down(b, h);
}
void gmb_browse_hill_left(gmb_browse_t* b)
{
    int h = gmb_browse_scrollable_page(b) / 2;
    if (h < 1) h = 1;
    gmb_browse_pan_left(b, h);
}
void gmb_browse_hill_right(gmb_browse_t* b)
{
    int h = gmb_browse_scrollable_page(b) / 2;
    if (h < 1) h = 1;
    gmb_browse_pan_right(b, h);
}

/* ===================== colonne ===================== */

void gmb_browse_add_column(gmb_browse_t* b, const gmb_browse_column_t* col)
{
    if (b->col_count >= GMB_BROWSE_MAX_COLS) return;
    b->columns[b->col_count] = *col;
    /* Il riferimento C# (ClipperLib BrowseColumn.Visible) ha default true; i chiamanti
       azzerano la struct con memset → allineiamo il default C (altrimenti colonne invisibili:
       render vuoto, filter e col_count sbagliati). Per nascondere: set_column(...,false). */
    b->columns[b->col_count].visible = true;
    b->col_count++;
}
void gmb_browse_del_column(gmb_browse_t* b, int pos)
{
    if (pos < b->col_count)
    {
        for (int i = pos; i < b->col_count - 1; i++) b->columns[i] = b->columns[i + 1];
        b->col_count--;
    }
}
void gmb_browse_ins_column(gmb_browse_t* b, int pos, const gmb_browse_column_t* col)
{
    if (b->col_count >= GMB_BROWSE_MAX_COLS) return;
    if (pos > b->col_count) pos = b->col_count;
    for (int i = b->col_count; i > pos; i--) b->columns[i] = b->columns[i - 1];
    b->columns[pos] = *col;
    b->col_count++;
}
gmb_browse_column_t* gmb_browse_get_column(gmb_browse_t* b, int pos)
{
    return pos < b->col_count ? &b->columns[pos] : NULL;
}
void gmb_browse_set_column(gmb_browse_t* b, int pos, const gmb_browse_column_t* col)
{
    if (pos < b->col_count) b->columns[pos] = *col;
}
int gmb_browse_col_count(const gmb_browse_t* b)
{
    int n = 0;
    for (int i = 0; i < b->col_count; i++)
        if (b->columns[i].visible) n++;
    return n;
}

/* ===================== cell lock ===================== */

void gmb_browse_lock_cell(gmb_browse_t* b, int row, int col)
{
    if (b->locked_count >= 64) return;
    for (int i = 0; i < b->locked_count; i++)
        if (b->locked_cells[i][0] == row && b->locked_cells[i][1] == col) return;
    b->locked_cells[b->locked_count][0] = row;
    b->locked_cells[b->locked_count][1] = col;
    b->locked_count++;
}
void gmb_browse_unlock_cell(gmb_browse_t* b, int row, int col)
{
    for (int i = 0; i < b->locked_count; i++)
    {
        if (b->locked_cells[i][0] == row && b->locked_cells[i][1] == col)
        {
            for (int j = i; j < b->locked_count - 1; j++)
            {
                b->locked_cells[j][0] = b->locked_cells[j + 1][0];
                b->locked_cells[j][1] = b->locked_cells[j + 1][1];
            }
            b->locked_count--;
            return;
        }
    }
}
bool gmb_browse_is_cell_locked(const gmb_browse_t* b, int row, int col)
{
    for (int i = 0; i < b->locked_count; i++)
        if (b->locked_cells[i][0] == row && b->locked_cells[i][1] == col) return true;
    if (b->cell_lock_fn) return b->cell_lock_fn(b->cell_lock_ctx, row, col);
    return false;
}
void gmb_browse_set_cell_lock(gmb_browse_t* b, bool (*f)(void*, int, int), void* ctx)
{
    b->cell_lock_fn = f;
    b->cell_lock_ctx = ctx;
}
bool gmb_browse_cell_editable(const gmb_browse_t* b, int row, int col)
{
    if (col < b->col_count && b->columns[col].editable)
        return !gmb_browse_is_cell_locked(b, row, col);
    return false;
}

/* ===================== freeze / stability ===================== */

int gmb_browse_freeze(gmb_browse_t* b, int n)
{
    int max = gmb_browse_col_count(b);
    b->frozen_cols = n < max ? n : max;
    return n;
}
int gmb_browse_freeze_count(const gmb_browse_t* b) { return b->frozen_cols; }
bool gmb_browse_stable(const gmb_browse_t* b) { return b->stabilized; }
bool gmb_browse_stable_with_block(gmb_browse_t* b)
{
    if (b->stable_block) return b->stable_block(b->stable_block_ctx, b);
    return b->stabilized;
}
void gmb_browse_force_stable(gmb_browse_t* b)
{
    int d[GMB_BROWSE_MAX_COLS];
    gmb_browse_displayed_cols(b, d, GMB_BROWSE_MAX_COLS);
    b->invalidated = false;
    b->stabilized = true;
}
void gmb_browse_set_stable_block(gmb_browse_t* b, bool (*f)(void*, gmb_browse_t*), void* ctx)
{
    b->stable_block = f;
    b->stable_block_ctx = ctx;
}
bool gmb_browse_moved(const gmb_browse_t* b)
{
    if (!b->has_last_render_pos || !b->has_last_render_top) return true;
    return b->last_render_row != b->current_row || b->last_render_col != b->current_col
        || b->last_render_top != b->top_row || b->last_render_left != b->left_col;
}
void gmb_browse_invalidate(gmb_browse_t* b) { b->invalidated = true; }
bool gmb_browse_invalid(const gmb_browse_t* b) { return b->invalidated; }

/* ===================== auto-lite / hilite ===================== */

bool gmb_browse_auto_lite(const gmb_browse_t* b) { return b->auto_lite; }
void gmb_browse_set_auto_lite(gmb_browse_t* b, bool v) { b->auto_lite = v; }
void gmb_browse_hilite(gmb_browse_t* b) { b->force_hilite = true; }
void gmb_browse_dehilite(gmb_browse_t* b) { b->force_hilite = false; }

/* ===================== color / separatori ===================== */

gmb_cell_color_t gmb_clipper_parse_color(const char* s)
{
    bool bright = strchr(s, '+') != NULL;
    char core[32];
    int n = 0;
    for (const char* p = s; *p && n < (int)sizeof(core) - 1; p++)
        if (*p != '+') core[n++] = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
    core[n] = '\0';
    if (strcmp(core, "N") == 0)  return bright ? GMB_CELLCOLOR_GREY : GMB_CELLCOLOR_BLACK;
    if (strcmp(core, "B") == 0)  return bright ? GMB_CELLCOLOR_BLUE : GMB_CELLCOLOR_DARKBLUE;
    if (strcmp(core, "G") == 0)  return bright ? GMB_CELLCOLOR_GREEN : GMB_CELLCOLOR_DARKGREEN;
    if (strcmp(core, "BG") == 0) return bright ? GMB_CELLCOLOR_CYAN : GMB_CELLCOLOR_DARKCYAN;
    if (strcmp(core, "R") == 0)  return bright ? GMB_CELLCOLOR_RED : GMB_CELLCOLOR_DARKRED;
    if (strcmp(core, "RB") == 0) return bright ? GMB_CELLCOLOR_MAGENTA : GMB_CELLCOLOR_DARKMAGENTA;
    if (strcmp(core, "GR") == 0) return bright ? GMB_CELLCOLOR_YELLOW : GMB_CELLCOLOR_DARKYELLOW;
    if (strcmp(core, "W") == 0)  return bright ? GMB_CELLCOLOR_WHITE : GMB_CELLCOLOR_GREY;
    return GMB_CELLCOLOR_WHITE;
}

gmb_color_pair_t gmb_browse_parse_color_pair(const char* token)
{
    gmb_color_pair_t cp;
    cp.foreground = GMB_CELLCOLOR_WHITE;
    cp.background = GMB_CELLCOLOR_BLACK;
    const char* slash = strchr(token, '/');
    char fg[64], bg[64];
    if (slash)
    {
        int fn = (int)(slash - token);
        for (int i = 0; i < fn && i < 63; i++) fg[i] = token[i];
        fg[fn < 63 ? fn : 63] = '\0';
        snprintf(bg, sizeof(bg), "%s", slash + 1);
    }
    else
    {
        snprintf(fg, sizeof(fg), "%s", token);
        strcpy(bg, "N");
    }
    char fgt[64];
    {
        int i = 0, j = 0;
        while (fg[i] == ' ' || fg[i] == '\t') i++;
        while (fg[i] && j < 63) fgt[j++] = fg[i++];
        fgt[j] = '\0';
    }
    cp.foreground = gmb_clipper_parse_color(fgt);
    char bgt[64];
    {
        int i = 0, j = 0;
        while (bg[i] == ' ' || bg[i] == '\t') i++;
        while (bg[i] && j < 63) bgt[j++] = bg[i++];
        bgt[j] = '\0';
    }
    cp.background = gmb_clipper_parse_color(bgt);
    return cp;
}

const char* gmb_browse_color_spec(const gmb_browse_t* b) { return b->color_spec_str; }

void gmb_browse_set_color_spec(gmb_browse_t* b, const char* spec)
{
    snprintf(b->color_spec_str, sizeof(b->color_spec_str), "%s", spec ? spec : "");
    /* parts[0]=header, parts[2]=rows, parts[4]=highlight (indici clipper) */
    char copy[512];
    snprintf(copy, sizeof(copy), "%s", spec ? spec : "");
    char* parts[8];
    int np = 0;
    char* tok = strtok(copy, ",");
    while (tok && np < 8)
    {
        parts[np++] = tok;
        tok = strtok(NULL, ",");
    }
    if (np > 0) b->header_colors = gmb_browse_parse_color_pair(parts[0]);
    if (np > 2) b->row_colors = gmb_browse_parse_color_pair(parts[2]);
    if (np > 4) b->highlight_colors = gmb_browse_parse_color_pair(parts[4]);
}

const char* gmb_browse_col_sep(const gmb_browse_t* b) { return b->col_sep; }
void gmb_browse_set_col_sep(gmb_browse_t* b, const char* s) { snprintf(b->col_sep, sizeof(b->col_sep), "%s", s ? s : "|"); }
const char* gmb_browse_head_sep(const gmb_browse_t* b) { return b->head_sep; }
void gmb_browse_set_head_sep(gmb_browse_t* b, const char* s) { snprintf(b->head_sep, sizeof(b->head_sep), "%s", s ? s : "-"); }
const char* gmb_browse_foot_sep(const gmb_browse_t* b) { return b->foot_sep; }
void gmb_browse_set_foot_sep(gmb_browse_t* b, const char* s) { snprintf(b->foot_sep, sizeof(b->foot_sep), "%s", s ? s : "-"); }
bool gmb_browse_has_footer(const gmb_browse_t* b) { return b->has_footer; }
const char* gmb_browse_footer_text(const gmb_browse_t* b) { return b->has_footer ? b->footer_text : NULL; }
void gmb_browse_set_footer(gmb_browse_t* b, const char* text)
{
    if (text)
    {
        snprintf(b->footer_text, sizeof(b->footer_text), "%s", text);
        b->has_footer = true;
    }
    else
    {
        b->footer_text[0] = '\0';
        b->has_footer = false;
    }
}

/* ===================== cella / bookmark ===================== */

int gmb_browse_cell_value(const gmb_browse_t* b, int row, int col, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!b->ds.get_value) { out[0] = '\0'; return 0; }
    return b->ds.get_value(b->ds.user, b_row(b, row), col, out, cap);
}

void gmb_browse_bookmark(gmb_browse_t* b, int n)
{
    if (b->ds.go_to) b->ds.go_to(b->ds.user, n);
    if (b->ds.bookmark) b->current_row = b->ds.bookmark(b->ds.user);
}

void gmb_browse_set_cargo(gmb_browse_t* b, void* v) { b->cargo = v; }
void* gmb_browse_get_cargo(gmb_browse_t* b) { return b->cargo; }

/* ===================== focus / hooks / key ===================== */

void gmb_browse_set_focus(gmb_browse_t* b) { b->focused = true; }
void gmb_browse_kill_focus(gmb_browse_t* b) { b->focused = false; }
void gmb_browse_register_hook(gmb_browse_t* b, void (*f)(void*, gmb_browse_t*), void* ctx)
{
    b->hook_fn = f;
    b->hook_ctx = ctx;
    b->has_hook = f != NULL;
}
void gmb_browse_call_hook(gmb_browse_t* b)
{
    if (b->has_hook && b->hook_fn) b->hook_fn(b->hook_ctx, b);
}

gmb_browse_action_t gmb_browse_apply_key(gmb_browse_t* b, int k)
{
    switch (k)
    {
        case 5: gmb_browse_up(b, 1); return GMB_BA_REFRESHED;   /* K_UP */
        case 24: gmb_browse_down(b, 1); return GMB_BA_REFRESHED;/* K_DOWN */
        case 19: gmb_browse_left(b, 1); return GMB_BA_REFRESHED;/* K_LEFT */
        case 4: gmb_browse_right(b, 1); return GMB_BA_REFRESHED;/* K_RIGHT */
        case 18: gmb_browse_page_up(b); return GMB_BA_REFRESHED; /* K_PGUP */
        case 3: gmb_browse_page_down(b); return GMB_BA_REFRESHED;/* K_PGDN */
        case 1: gmb_browse_go_top(b); return GMB_BA_REFRESHED;  /* K_HOME */
        case 6: gmb_browse_go_bottom(b); return GMB_BA_REFRESHED;/* K_END */
        case 13: return GMB_BA_SELECTED;                        /* K_ENTER */
        case 27: return GMB_BA_CANCELLED;                       /* K_ESC */
        default: return GMB_BA_NONE;
    }
}

gmb_browse_action_t gmb_browse_navigate(gmb_browse_t* b, int key)
{
    switch (key)
    {
        case GMB_K_DOWN: gmb_browse_move_down(b); return GMB_BA_REFRESHED;
        case GMB_K_UP: gmb_browse_move_up(b); return GMB_BA_REFRESHED;
        case GMB_K_PGDN: gmb_browse_page_down(b); return GMB_BA_REFRESHED;
        case GMB_K_PGUP: gmb_browse_page_up(b); return GMB_BA_REFRESHED;
        case GMB_K_HOME: gmb_browse_go_top(b); return GMB_BA_REFRESHED;
        case GMB_K_END: gmb_browse_go_bottom(b); return GMB_BA_REFRESHED;
        case GMB_K_RIGHT: gmb_browse_move_right(b); return GMB_BA_REFRESHED;
        case GMB_K_LEFT: gmb_browse_move_left(b); return GMB_BA_REFRESHED;
        case GMB_K_ENTER: return GMB_BA_SELECTED;
        case GMB_K_ESC: return GMB_BA_CANCELLED;
        default: return GMB_BA_NONE;
    }
}

/* ===================== mouse ===================== */

void gmb_browse_handle_click(gmb_browse_t* b, int x, int y)
{
    int header_offset = b->show_header ? 1 : 0;
    int border_offset = b->show_border ? 1 : 0;
    int data_start_y = b->area.y + border_offset + header_offset + header_offset;
    (void)header_offset;

    if (y >= data_start_y)
    {
        int clicked_row = y - data_start_y + b->top_row;
        if (clicked_row < gmb_browse_row_count(b))
            b->current_row = clicked_row;
    }

    int content_x = b->area.x + (b->show_border ? 1 : 0);
    int cx = x - content_x;
    if (cx >= 0)
    {
        int idxs[GMB_BROWSE_MAX_COLS];
        int n = gmb_browse_displayed_cols(b, idxs, GMB_BROWSE_MAX_COLS);
        int acc = 0;
        int inner_w = gmb_browse_inner_width(b);
        for (int ci = 0; ci < n; ci++)
        {
            int i = idxs[ci];
            bool is_last = ci == n - 1;
            int cw = is_last ? (inner_w - acc > 0 ? inner_w - acc : 0) : b->columns[i].width;
            if (cx >= acc && cx < acc + cw)
            {
                b->current_col = i;
                break;
            }
            acc += cw + 1;
        }
    }
}

/* ===================== editing inline ===================== */

static int tbrowse_days_in_month(int year, int month)
{
    switch (month)
    {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
        default: return 30;
    }
}

static bool tbrowse_parse_date(const char* s, int* y, int* m, int* d)
{
    *y = *m = *d = 0;
    int y1 = 0, m1 = 0, d1 = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", s);
    char* p1 = strchr(buf, '-');
    if (!p1) return false;
    *p1 = '\0';
    char* p2 = strchr(p1 + 1, '-');
    if (!p2) return false;
    *p2 = '\0';
    y1 = atoi(buf);
    m1 = atoi(p1 + 1);
    d1 = atoi(p2 + 1);
    if (m1 < 1 || m1 > 12 || d1 < 1 || d1 > tbrowse_days_in_month(y1, m1)) return false;
    *y = y1; *m = m1; *d = d1;
    return true;
}

bool gmb_browse_begin_edit(gmb_browse_t* b)
{
    if (!gmb_browse_cell_editable(b, b->current_row, b->current_col))
        return false;
    int col = b->current_col;
    int row = b->current_row;
    gmb_data_type_t dt = b->columns[col].data_type;
    char initial[GMB_BROWSE_MAX_BUF];
    b->ds.get_value(b->ds.user, b_row(b, row), col, initial, sizeof(initial));

    if (dt == GMB_DT_DATE)
    {
        int y, m, d;
        if (!tbrowse_parse_date(initial, &y, &m, &d))
            dt = GMB_DT_TEXT;
    }

    b->editing = true;
    b->edit_row = row;
    b->edit_col = col;
    snprintf(b->edit_buf, sizeof(b->edit_buf), "%s", initial);
    b->edit_cursor = (int)strlen(initial);
    b->edit_dtype = dt;
    snprintf(b->edit_initial, sizeof(b->edit_initial), "%s", initial);
    b->edit_date_field = 2;
    return true;
}

static gmb_browse_action_t tbrowse_edit_move(gmb_browse_t* b, int d_row, int d_col)
{
    if (b->editing)
    {
        b->ds.set_value(b->ds.user, b_row(b, b->edit_row), b->edit_col, b->edit_buf);
        if (b->pre_validator) b->pre_validator(b->pre_validator_ctx, b->edit_row, b->edit_col);
        if (b->post_validator) b->post_validator(b->post_validator_ctx, b->edit_row, b->edit_col);
        b->current_row = b->edit_row;
        b->current_col = b->edit_col;
        b->editing = false;
    }
    /* FindEditable */
    int n_rows = gmb_browse_row_count(b);
    int n_cols = b->col_count;
    int found_row = -1, found_col = -1;
    if (d_col > 0)
    {
        for (int r = b->current_row; r < n_rows && found_row < 0; r++)
        {
            int c_start = r == b->current_row ? b->current_col + 1 : 0;
            for (int c = c_start; c < n_cols; c++)
                if (gmb_browse_cell_editable(b, r, c)) { found_row = r; found_col = c; break; }
        }
    }
    else if (d_col < 0)
    {
        for (int r = b->current_row; r >= 0 && found_row < 0; r--)
        {
            int c_start = r == b->current_row ? (b->current_col - 1 > 0 ? b->current_col - 1 : 0) : n_cols - 1;
            for (int c = c_start; c >= 0; c--)
                if (gmb_browse_cell_editable(b, r, c)) { found_row = r; found_col = c; break; }
        }
    }
    else if (d_row > 0)
    {
        for (int r = b->current_row + 1; r < n_rows; r++)
            if (gmb_browse_cell_editable(b, r, b->current_col)) { found_row = r; found_col = b->current_col; break; }
    }
    else if (d_row < 0)
    {
        for (int r = b->current_row - 1; r >= 0; r--)
            if (gmb_browse_cell_editable(b, r, b->current_col)) { found_row = r; found_col = b->current_col; break; }
    }

    if (found_row >= 0)
    {
        b->current_row = found_row;
        b->current_col = found_col;
        gmb_browse_begin_edit(b);
        return GMB_BA_NONE;
    }
    return GMB_BA_SELECTED;
}

static void tbrowse_edit_text(gmb_browse_t* b, int key)
{
    if (key == GMB_K_BS)
    {
        if (b->edit_cursor > 0)
        {
            memmove(b->edit_buf + b->edit_cursor - 1, b->edit_buf + b->edit_cursor,
                    strlen(b->edit_buf + b->edit_cursor) + 1);
            b->edit_cursor--;
        }
    }
    else if (key == GMB_K_DEL)
    {
        if (b->edit_cursor < (int)strlen(b->edit_buf))
            memmove(b->edit_buf + b->edit_cursor, b->edit_buf + b->edit_cursor + 1,
                    strlen(b->edit_buf + b->edit_cursor + 1) + 1);
    }
    else if (key == GMB_K_LEFT) b->edit_cursor = b->edit_cursor - 1 > 0 ? b->edit_cursor - 1 : 0;
    else if (key == GMB_K_RIGHT) b->edit_cursor = b->edit_cursor + 1 < (int)strlen(b->edit_buf) ? b->edit_cursor + 1 : (int)strlen(b->edit_buf);
    else if (key == GMB_K_HOME) b->edit_cursor = 0;
    else if (key == GMB_K_END) b->edit_cursor = (int)strlen(b->edit_buf);
    else if (key >= 32 && key <= 126)
    {
        int len = (int)strlen(b->edit_buf);
        if (len + 1 < GMB_BROWSE_MAX_BUF)
        {
            memmove(b->edit_buf + b->edit_cursor + 1, b->edit_buf + b->edit_cursor, (size_t)(len - b->edit_cursor) + 1);
            b->edit_buf[b->edit_cursor] = (char)key;
            b->edit_cursor++;
        }
    }
}

static void tbrowse_edit_numeric(gmb_browse_t* b, int key, bool is_float)
{
    if (key == GMB_K_BS)
    {
        if (b->edit_cursor > 0)
        {
            memmove(b->edit_buf + b->edit_cursor - 1, b->edit_buf + b->edit_cursor,
                    strlen(b->edit_buf + b->edit_cursor) + 1);
            b->edit_cursor--;
        }
    }
    else if (key == GMB_K_LEFT) b->edit_cursor = b->edit_cursor - 1 > 0 ? b->edit_cursor - 1 : 0;
    else if (key == GMB_K_RIGHT) b->edit_cursor = b->edit_cursor + 1 < (int)strlen(b->edit_buf) ? b->edit_cursor + 1 : (int)strlen(b->edit_buf);
    else if (key == GMB_K_HOME) b->edit_cursor = 0;
    else if (key == GMB_K_END) b->edit_cursor = (int)strlen(b->edit_buf);
    else if (key >= 32 && key <= 126)
    {
        char c = (char)key;
        bool ok;
        if (is_float)
            ok = (c >= '0' && c <= '9') || (c == '.' && !strchr(b->edit_buf, '.'));
        else
            ok = (c >= '0' && c <= '9') || (c == '-' && b->edit_cursor == 0 && !strchr(b->edit_buf, '-'));
        if (ok)
        {
            int len = (int)strlen(b->edit_buf);
            if (len + 1 < GMB_BROWSE_MAX_BUF)
            {
                memmove(b->edit_buf + b->edit_cursor + 1, b->edit_buf + b->edit_cursor, (size_t)(len - b->edit_cursor) + 1);
                b->edit_buf[b->edit_cursor] = c;
                b->edit_cursor++;
            }
        }
    }
}

static void tbrowse_toggle_bool(gmb_browse_t* b)
{
    bool cur = strcmp(b->edit_buf, "Y") == 0 || strcmp(b->edit_buf, "true") == 0;
    snprintf(b->edit_buf, sizeof(b->edit_buf), "%s", cur ? "N" : "Y");
    b->edit_cursor = (int)strlen(b->edit_buf);
}

static void tbrowse_edit_date(gmb_browse_t* b, int key)
{
    int y = 1970, m = 1, d = 1;
    tbrowse_parse_date(b->edit_buf, &y, &m, &d);
    if (key == GMB_K_LEFT) b->edit_date_field = b->edit_date_field - 1 > 0 ? b->edit_date_field - 1 : 0;
    else if (key == GMB_K_RIGHT) b->edit_date_field = b->edit_date_field + 1 < 2 ? b->edit_date_field + 1 : 2;
    else if (key == GMB_K_UP)
    {
        switch (b->edit_date_field)
        {
            case 0: y++; break;
            case 1: m = m + 1 < 12 ? m + 1 : 12; break;
            case 2: { int md = tbrowse_days_in_month(y, m); d = d + 1 < md ? d + 1 : md; } break;
        }
    }
    else if (key == GMB_K_DOWN)
    {
        switch (b->edit_date_field)
        {
            case 0: y = y - 1 > 0 ? y - 1 : 0; break;
            case 1: m = m - 1 > 1 ? m - 1 : 1; break;
            case 2: d = d <= 1 ? tbrowse_days_in_month(y, m) : d - 1; break;
        }
    }
    if (m < 1) m = 1;
    if (m > 12) m = 12;
    int md = tbrowse_days_in_month(y, m);
    if (d < 1) d = 1;
    if (d > md) d = md;
    snprintf(b->edit_buf, sizeof(b->edit_buf), "%04d-%02d-%02d", y, m, d);
    b->edit_cursor = (int)strlen(b->edit_buf);
}

gmb_browse_action_t gmb_browse_apply_edit_key(gmb_browse_t* b, int key)
{
    if (!b->editing)
        return GMB_BA_NONE;
    if (key == GMB_K_ESC)
    {
        b->editing = false;
        return GMB_BA_CANCELLED;
    }
    if (key == GMB_K_TAB) return tbrowse_edit_move(b, 0, 1);
    if (key == GMB_K_BTAB) return tbrowse_edit_move(b, 0, -1);
    if (key == GMB_K_ENTER)
    {
        /* CommitEdit */
        int row = b->edit_row, col = b->edit_col;
        char val[GMB_BROWSE_MAX_BUF];
        snprintf(val, sizeof(val), "%s", b->edit_buf);
        b->editing = false;
        b->ds.set_value(b->ds.user, b_row(b, row), col, val);
        bool ok = true;
        if (b->pre_validator) ok = b->pre_validator(b->pre_validator_ctx, row, col) && ok;
        if (b->post_validator) ok = b->post_validator(b->post_validator_ctx, row, col) && ok;
        return ok ? GMB_BA_SELECTED : GMB_BA_REFRESHED;
    }
    if (key == GMB_K_UP || key == GMB_K_DOWN)
    {
        if (b->edit_dtype != GMB_DT_CHOICE && b->edit_dtype != GMB_DT_DATE)
            return tbrowse_edit_move(b, key == GMB_K_UP ? -1 : 1, 0);
    }
    if (key == GMB_K_LEFT || key == GMB_K_RIGHT)
    {
        if (b->edit_dtype != GMB_DT_TEXT && b->edit_dtype != GMB_DT_DATE)
            return tbrowse_edit_move(b, 0, key == GMB_K_LEFT ? -1 : 1);
    }
    switch (b->edit_dtype)
    {
        case GMB_DT_TEXT: tbrowse_edit_text(b, key); break;
        case GMB_DT_INT: tbrowse_edit_numeric(b, key, false); break;
        case GMB_DT_FLOAT: tbrowse_edit_numeric(b, key, true); break;
        case GMB_DT_BOOL:
            if (key == ' ' || key == GMB_K_ENTER || key == GMB_K_UP || key == GMB_K_DOWN)
                tbrowse_toggle_bool(b);
            break;
        case GMB_DT_DATE: tbrowse_edit_date(b, key); break;
        case GMB_DT_CHOICE:
            if (key == GMB_K_UP || key == GMB_K_DOWN || key == GMB_K_TAB)
                tbrowse_toggle_bool(b);
            break;
        default: break;
    }
    return GMB_BA_NONE;
}

/* ===================== render ===================== */

void gmb_browse_format_cell(const char* value, int width, gmb_browse_alignment_t align,
                            char* out, int cap)
{
    if (!out || cap <= 0) return;
    if (!value) value = "";
    int vl = (int)strlen(value);
    if (vl >= width) { snprintf(out, cap, "%.*s", width, value); return; }
    int padding = width - vl;
    int n = 0;
    switch (align)
    {
        case GMB_ALIGN_RIGHT:
            for (int i = 0; i < padding && n < cap - 1; i++) out[n++] = ' ';
            for (int i = 0; i < vl && n < cap - 1; i++) out[n++] = value[i];
            break;
        case GMB_ALIGN_CENTER:
        {
            int half = padding / 2;
            for (int i = 0; i < half && n < cap - 1; i++) out[n++] = ' ';
            for (int i = 0; i < vl && n < cap - 1; i++) out[n++] = value[i];
            for (int i = half; i < padding && n < cap - 1; i++) out[n++] = ' ';
            break;
        }
        default:
            for (int i = 0; i < vl && n < cap - 1; i++) out[n++] = value[i];
            for (int i = 0; i < padding && n < cap - 1; i++) out[n++] = ' ';
            break;
    }
    out[n] = '\0';
}

static void cell_fn_text(void* ctx, int col, int width, char* out, int cap)
{
    const gmb_browse_t* b = (const gmb_browse_t*)ctx;
    gmb_browse_format_cell(b->columns[col].header, width, b->columns[col].align, out, cap);
}

static int utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x06) return 2;
    if ((c >> 4) == 0x0E) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

int gmb_browse_render_inner(const gmb_browse_t* b, const char* sep, char* out, int cap,
                            void* ctx, void (*cell_fn)(void* ctx, int col, int width, char* out, int cap))
{
    int inner_w = gmb_browse_inner_width(b);
    int idxs[GMB_BROWSE_MAX_COLS];
    int n = gmb_browse_displayed_cols(b, idxs, GMB_BROWSE_MAX_COLS);
    if (!cell_fn) cell_fn = cell_fn_text;
    int chars = 0, o = 0, x = 0;

    #define PUTS(s) do { for (const char* _p = (s); *_p && o < cap - 1; _p++) out[o++] = *_p; } while (0)
    #define PUTS_UTF8(s) do { const char* _p = (s); while (*_p && chars < inner_w && o < cap - 1) { int _l = utf8_char_len((unsigned char)*_p); if (chars + 1 > inner_w) break; for (int _i = 0; _i < _l && o < cap - 1; _i++) out[o++] = *_p++; chars++; } } while (0)

    for (int k = 0; k < n; k++)
    {
        int i = idxs[k];
        int cw = b->columns[i].width;
        if (k + 1 < n)
        {
            int block = cw + 1;
            if (x + block > inner_w)
            {
                int rem = inner_w - x;
                if (rem < 0) rem = 0;
                if (rem > 0)
                {
                    char cell[GMB_BROWSE_MAX_BUF];
                    cell_fn(ctx, i, rem, cell, (int)sizeof(cell));
                    PUTS_UTF8(cell);
                }
                break;
            }
            char cell[GMB_BROWSE_MAX_BUF];
            cell_fn(ctx, i, cw, cell, (int)sizeof(cell));
            PUTS_UTF8(cell);
            PUTS_UTF8(sep);
            x += block;
        }
        else
        {
            int rem = inner_w - x;
            if (rem < 0) rem = 0;
            if (rem > 0)
            {
                char cell[GMB_BROWSE_MAX_BUF];
                cell_fn(ctx, i, rem, cell, (int)sizeof(cell));
                PUTS_UTF8(cell);
            }
            break;
        }
    }
    if (chars < inner_w)
    {
        for (int i = chars; i < inner_w && o < cap - 1; i++) out[o++] = ' ';
        chars = inner_w;
    }
    else if (chars > inner_w)
    {
        /* tronca a inner_w caratteri */
        int c2 = 0;
        int o2 = 0;
        while (c2 < inner_w && o2 < o)
        {
            int l = utf8_char_len((unsigned char)out[o2]);
            o2 += l;
            c2++;
        }
        o = o2;
    }
    out[o] = '\0';
    #undef PUTS
    #undef PUTS_UTF8
    return chars;
}

/* cell_fn per righe dati (rispetta editing inline) */
typedef struct gmb_row_cell_ctx {
    const gmb_browse_t* b;
    int data_row;
} gmb_row_cell_ctx_t;

static void row_cell_fn(void* ctx, int col, int width, char* out, int cap)
{
    gmb_row_cell_ctx_t* c = (gmb_row_cell_ctx_t*)ctx;
    const gmb_browse_t* b = c->b;
    char value[GMB_BROWSE_MAX_BUF];
    if (b->editing && b->edit_row == c->data_row && b->edit_col == col)
    {
        /* ClipDisplay */
        int n = (int)strlen(b->edit_buf);
        int cursor = b->edit_cursor;
        if (cursor > n) cursor = n;
        if (n <= width)
        {
            gmb_browse_format_cell(b->edit_buf, width, b->columns[col].align, out, cap);
            return;
        }
        int offset = cursor - (width - 1);
        if (offset < 0) offset = 0;
        if (offset + width > n) offset = n - width;
        char disp[GMB_BROWSE_MAX_BUF];
        snprintf(disp, sizeof(disp), "%.*s", width, b->edit_buf + offset);
        gmb_browse_format_cell(disp, width, b->columns[col].align, out, cap);
        return;
    }
    b->ds.get_value(b->ds.user, b_row(b, c->data_row), col, value, sizeof(value));
    /* C# RenderRow: FormatCell(ApplyCase(_data.GetValue(...))) nel ramo non-editing. */
    char cased[GMB_BROWSE_MAX_BUF];
    gmb_control_apply_case((gmb_control_t*)&b->ctrl, value, cased, (int)sizeof(cased));
    gmb_browse_format_cell(cased, width, b->columns[col].align, out, cap);
}

int gmb_browse_render_row(const gmb_browse_t* b, int display_row, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int total = gmb_browse_row_count(b);
    int inner_w = gmb_browse_inner_width(b);
    int data_row = b->top_row + display_row;
    int o = 0;
    if (data_row >= total)
    {
        for (int i = 0; i < inner_w && o < cap - 1; i++) out[o++] = ' ';
    }
    else
    {
        gmb_row_cell_ctx_t ctx = { b, data_row };
        char line[2048];
        gmb_browse_render_inner(b, b->col_sep, line, (int)sizeof(line), &ctx, row_cell_fn);
        for (char* p = line; *p && o < cap - 1; p++) out[o++] = *p;
    }
    out[o] = '\0';
    return o;
}

int gmb_browse_render_rows(const gmb_browse_t* b, char* out, int cap)
{
    int page = gmb_browse_page_size(b);
    int o = 0;
    for (int display_row = 0; display_row < page; display_row++)
    {
        char line[2048];
        int n = gmb_browse_render_row(b, display_row, line, (int)sizeof(line));
        for (int i = 0; i < n && o < cap - 1; i++) out[o++] = line[i];
        if (display_row < page - 1 && o < cap - 1) out[o++] = '\n';
    }
    out[o] = '\0';
    return o;
}

int gmb_browse_render_header(const gmb_browse_t* b, char* out, int cap)
{
    return gmb_browse_render_inner(b, b->col_sep, out, cap, (void*)b, cell_fn_text);
}

/* emette 'count' copie del primo carattere UTF-8 di 's' */
static void put_char_repeat(char* out, int cap, int* idx, const char* s, int count)
{
    int l = utf8_char_len((unsigned char)s[0]);
    for (int i = 0; i < count; i++)
        for (int j = 0; j < l && *idx < cap - 1; j++)
            out[(*idx)++] = s[j];
}

static void sep_cell_fn(void* ctx, int col, int width, char* out, int cap)
{
    (void)ctx; (void)col;
    const char* fill = (const char*)ctx;
    int l = utf8_char_len((unsigned char)fill[0]);
    int o = 0;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < l && o < cap - 1; j++)
            out[o++] = fill[j];
    out[o] = '\0';
}

int gmb_browse_render_separator(const gmb_browse_t* b, char* out, int cap)
{
    return gmb_browse_render_inner(b, "\xE2\x94\xBC", out, cap, (void*)b->head_sep, sep_cell_fn);
}

int gmb_browse_render_footer_separator(const gmb_browse_t* b, char* out, int cap)
{
    return gmb_browse_render_inner(b, "\xE2\x94\xBC", out, cap, (void*)b->foot_sep, sep_cell_fn);
}

int gmb_browse_render(const gmb_browse_t* b, char* out, int cap)
{
    if (b->invalidated == false && 0)
        ;
    if (!out || cap <= 0) return 0;
    int inner_w = gmb_browse_inner_width(b);
    int w = b->area.w;
    int o = 0;

    #define APPENDN(s, n) do { for (int _i = 0; _i < (n) && o < cap - 1; _i++) out[o++] = (s)[_i]; } while (0)
    #define APPEND(s) APPENDN((s), (int)strlen(s))
    #define NL() do { if (o < cap - 1) out[o++] = '\n'; } while (0)

    if (b->show_border && w >= 2)
    {
        APPEND("\xE2\x94\x8C");            /* ┌ */
        put_char_repeat(out, cap, &o, b->head_sep, inner_w);
        APPEND("\xE2\x94\x90"); NL();      /* ┐ */
    }

    if (b->show_header)
    {
        char hdr[2048];
        gmb_browse_render_header(b, hdr, (int)sizeof(hdr));
        if (b->show_border) { APPEND("\xE2\x94\x82"); APPEND(hdr); APPEND("\xE2\x94\x82"); }
        else APPEND(hdr);
        NL();

        char sep[2048];
        gmb_browse_render_separator(b, sep, (int)sizeof(sep));
        if (b->show_border) { APPEND("\xE2\x94\x82"); APPEND(sep); APPEND("\xE2\x94\x82"); }
        else APPEND(sep);
        NL();
    }

    char rows[8192];
    gmb_browse_render_rows(b, rows, (int)sizeof(rows));
    char* line = rows;
    while (*line)
    {
        char* eol = strchr(line, '\n');
        if (eol) *eol = '\0';
        if (b->show_border) { APPEND("\xE2\x94\x82"); APPEND(line); APPEND("\xE2\x94\x82"); }
        else APPEND(line);
        NL();
        if (!eol) break;
        line = eol + 1;
    }

    if (b->has_footer)
    {
        char sep[2048];
        gmb_browse_render_footer_separator(b, sep, (int)sizeof(sep));
        if (b->show_border) { APPEND("\xE2\x94\x82"); APPEND(sep); APPEND("\xE2\x94\x82"); }
        else APPEND(sep);
        NL();
        char f[512];
        gmb_browse_format_cell(b->footer_text, inner_w, GMB_ALIGN_LEFT, f, (int)sizeof(f));
        if ((int)strlen(f) > inner_w) f[inner_w] = '\0';
        if (b->show_border) { APPEND("\xE2\x94\x82"); APPEND(f); APPEND("\xE2\x94\x82"); }
        else APPEND(f);
        NL();
    }

    if (b->show_border && w >= 2)
    {
        APPEND("\xE2\x94\x94");            /* └ */
        put_char_repeat(out, cap, &o, b->head_sep, inner_w);
        APPEND("\xE2\x94\x98"); NL();      /* ┘ */
    }

    #undef APPENDN
    #undef APPEND
    #undef NL
    if (o > 0 && out[o - 1] == '\n') o--;
    out[o] = '\0';
    return o;
}

int gmb_browse_refresh(gmb_browse_t* b, char* out, int cap)
{
    b->last_render_row = b->current_row;
    b->last_render_col = b->current_col;
    b->last_render_top = b->top_row;
    b->last_render_left = b->left_col;
    b->has_last_render_pos = true;
    b->has_last_render_top = true;
    b->stabilized = true;
    return gmb_browse_render(b, out, cap);
}

int gmb_browse_refresh_all(gmb_browse_t* b, char* out, int cap)
{
    b->invalidated = false;
    int d[GMB_BROWSE_MAX_COLS];
    gmb_browse_displayed_cols(b, d, GMB_BROWSE_MAX_COLS);
    return gmb_browse_refresh(b, out, cap);
}

/* ===================== Draw ===================== */

void gmb_browse_draw(gmb_browse_t* b, gmb_screen_buffer_t* buffer)
{
    int ax = b->area.x;
    int ay = b->area.y;
    int w = b->area.w;
    int inner_w = gmb_browse_inner_width(b);
    int bh = gmb_screen_height(buffer);
    int y = ay;

    gmb_cell_color_t border_fg = b->header_colors.foreground;
    gmb_cell_color_t border_bg = b->header_colors.background;

    gmb_screen_sink_t* sk = gmb_screen_get_sink(buffer);
    if (sk && sk->is_gfx)
    {
        if (gmb_ui_gfx.browse_draw_px) { gmb_ui_gfx.browse_draw_px(b, buffer); }
        return;
    }

    char line[8192];

    if (b->show_border && w >= 2)
    {
        if (y < bh)
        {
            int n = 0;
            line[n++] = '\xE2'; line[n++] = '\x94'; line[n++] = '\x8C';
            for (int i = 0; i < inner_w; i++) {
                int _l = utf8_char_len((unsigned char)b->head_sep[0]);
                for (int j = 0; j < _l && n < (int)sizeof(line) - 1; j++) line[n++] = b->head_sep[j];
            }
            line[n++] = '\xE2'; line[n++] = '\x94'; line[n++] = '\x90';
            line[n] = '\0';
            gmb_screen_setstring(buffer, ax, y, line, border_fg, border_bg);
        }
        y++;
    }

    if (b->show_header)
    {
        char inner[2048];
        gmb_browse_render_header(b, inner, (int)sizeof(inner));
        if (y < bh)
        {
            if (b->show_border)
            {
                char full[4096];
                snprintf(full, sizeof(full), "\xE2\x94\x82%s\xE2\x94\x82", inner);
                gmb_screen_setstring(buffer, ax, y, full, border_fg, border_bg);
            }
            else
                gmb_screen_setstring(buffer, ax, y, inner, border_fg, border_bg);

            if (b->current_col >= 0)
            {
                int idxs[GMB_BROWSE_MAX_COLS];
                int n = gmb_browse_displayed_cols(b, idxs, GMB_BROWSE_MAX_COLS);
                int content_x = b->show_border ? ax + 1 : ax;
                int hx = content_x;
                for (int k = 0; k < n; k++)
                {
                    int ci = idxs[k];
                    bool is_last = k == n - 1;
                    int cw = is_last ? (inner_w - (hx - content_x) > 0 ? inner_w - (hx - content_x) : 0) : b->columns[ci].width;
                    if (ci == b->current_col)
                    {
                        char text[512];
                        gmb_browse_format_cell(b->columns[ci].header, cw, b->columns[ci].align, text, (int)sizeof(text));
                        gmb_screen_setstring(buffer, hx, y, text,
                            b->header_highlight_colors.foreground, b->header_highlight_colors.background);
                        break;
                    }
                    hx += cw + 1;
                }
            }
        }
        y++;

        char sep[2048];
        gmb_browse_render_separator(b, sep, (int)sizeof(sep));
        if (y < bh)
        {
            if (b->show_border)
            {
                char full[4096];
                snprintf(full, sizeof(full), "\xE2\x94\x82%s\xE2\x94\x82", sep);
                gmb_screen_setstring(buffer, ax, y, full, border_fg, border_bg);
            }
            else
                gmb_screen_setstring(buffer, ax, y, sep, border_fg, border_bg);
        }
        y++;
    }

    int page = gmb_browse_page_size(b);
    int total = gmb_browse_row_count(b);
    char rows[8192];
    gmb_browse_render_rows(b, rows, (int)sizeof(rows));
    char* row_line = rows;
    int display_row = 0;
    while (row_line && display_row < page)
    {
        if (y >= bh) break;
        int data_row = b->top_row + display_row;
        char* eol = strchr(row_line, '\n');
        if (eol) *eol = '\0';

        bool is_cur = data_row == b->current_row && data_row < total && (b->highlight_row || b->force_hilite);
        gmb_cell_color_t fg = is_cur ? b->highlight_colors.foreground : b->row_colors.foreground;
        gmb_cell_color_t bg = is_cur ? b->highlight_colors.background : b->row_colors.background;

        if (b->show_border)
        {
            int bw = gmb_screen_width(buffer);
            gmb_screen_setstring(buffer, ax, y, "\xE2\x94\x82", border_fg, border_bg);
            gmb_screen_setstring(buffer, ax + 1, y, row_line, fg, bg);
            /* bordo destro: coordinata ASSOLUTA (ax + 1 + inner_w), confrontata con la
             * larghezza del BUFFER. Prima si confrontava con b->area.w (larghezza del
             * controllo) e si finiva col disegnare il `│` a x = area.w-1, cioè DENTRO la
             * riga: "Mario" diventava "Ma│io" (visibile solo con ax != 0). */
            int rx = ax + 1 + inner_w;
            if (rx < bw) gmb_screen_setstring(buffer, rx, y, "\xE2\x94\x82", border_fg, border_bg);
        }
        else
            gmb_screen_setstring(buffer, ax, y, row_line, fg, bg);

        if (!eol) break;
        row_line = eol + 1;
        display_row++;
        y++;
    }
    (void)bh;
    (void)line;
}

/* ===================== Control (TBrowse : Control) ===================== */
static void bc_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    (void)c;
    gmb_browse_draw((gmb_browse_t*)u, buf);
}
static bool bc_key(void* u, gmb_control_t* c, int key)
{
    gmb_browse_t* b = (gmb_browse_t*)u;
    if (!c->enabled) return false;
    if (b->editing) return gmb_browse_apply_edit_key(b, key) != GMB_BA_NONE;
    return gmb_browse_navigate(b, key) != GMB_BA_NONE;
}
static bool bc_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_browse_t* b = (gmb_browse_t*)u;
    if (!c->enabled) return false;
    if (x < b->area.x || x >= b->area.x + b->area.w || y < b->area.y || y >= b->area.y + b->area.h) return false;
    if (!b->mouse_enabled) return false;
    gmb_browse_handle_click(b, x, y);
    return true;
}
static bool bc_mouse(void* u, gmb_control_t* c, const gmb_mouse_action_t* a)
{
    gmb_browse_t* b = (gmb_browse_t*)u;
    if (!c->enabled || !b->mouse_enabled) return false;
    switch (a->kind)
    {
        case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK: return bc_click(u, c, a->x, a->y);
        case GMB_MA_SCROLLUP: gmb_browse_move_up(b); return true;
        case GMB_MA_SCROLLDOWN: gmb_browse_move_down(b); return true;
        default: return false;
    }
}
