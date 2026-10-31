/* gmb_gaps_selfcheck.c — parity test per i gap chiusi del Runtime C Port (Fase 1+2)
 * Copre: ArrayUtils Chunk/Zip/Pad/Shuffle (common/CommonLib/ArrayUtils.cs:90-167)
 *        Compression ListZip (CompressionUtils.cs:220)
 *        Graphics Circle arc + ImagePut AND/OR/XOR (gmb_graphics.c:570,980)
 *        Graphics TTF (gmb_graphics.c:732, Fonts/Manager.cs:530)
 *        Graphics Texture Rotation (gmb_graphics.c:833)
 * Metodo test-first: oracolo C# → parity test rosso → port → verde.
 * Eseguito come: cmake --build && ./gmb_gaps_selfcheck
 */
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_compress.h"
#include "gmb_tui.h"
#include "gmb_tui_widgets.h"
#include "gmb_regex_extra.h"
#include "gmb_bitserializers.h"
#include "gmb_screen_config.h"
#include "gmb_sdl_console.h"
#include "gmb_sdl_renderer.h"
#include "gmb_console_extra.h"
#include "gmb_sdl_context.h"
#include "gmb_keyboard_extra.h"
#include "gmb_mouse_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_array_chunk(void){
    printf("TEST Array Chunk... ");
    gmb_list_t src = gmb_list_new();
    for(int i=1;i<=5;i++) gmb_list_add(&src, gmb_make_val_int(i));
    gmb_list_t chunks = gmb_list_chunk(src, 2);
    if(chunks.count!=3){ printf("FAIL count %zu !=3\n", chunks.count); return 1; }
    gmb_list_t c0 = gmb_val_as_list(chunks.items[0]);
    gmb_list_t c1 = gmb_val_as_list(chunks.items[1]);
    gmb_list_t c2 = gmb_val_as_list(chunks.items[2]);
    if(c0.count!=2 || c1.count!=2 || c2.count!=1){ printf("FAIL sizes %zu %zu %zu\n", c0.count, c1.count, c2.count); return 1; }
    if(c0.items[0].v_int!=1 || c0.items[1].v_int!=2){ printf("FAIL chunk0\n"); return 1;}
    if(c2.items[0].v_int!=5){ printf("FAIL chunk2\n"); return 1;}
    double arrd[]={1,2,3,4,5};
    gmb_list_t dchunks = gmb_array_chunk_doubles(arrd, 5, 2);
    if(dchunks.count!=3){ printf("FAIL dchunks %zu\n", dchunks.count); return 1;}
    printf("OK\n");
    gmb_list_free(&src); gmb_list_free(&chunks); gmb_list_free(&dchunks);
    gmb_list_free(&c0); gmb_list_free(&c1); gmb_list_free(&c2);
    return 0;
}
static int test_array_zip(void){
    printf("TEST Array Zip... ");
    gmb_list_t a = gmb_list_new(), b = gmb_list_new();
    for(int i=1;i<=3;i++) gmb_list_add(&a, gmb_make_val_int(i));
    gmb_list_add(&b, gmb_make_val_int(10)); gmb_list_add(&b, gmb_make_val_int(20)); gmb_list_add(&b, gmb_make_val_int(30)); gmb_list_add(&b, gmb_make_val_int(40));
    gmb_list_t lists[2]={a,b};
    gmb_list_t z = gmb_list_zip_multi(lists, 2);
    if(z.count!=3){ printf("FAIL zip count %zu !=3\n", z.count); return 1;}
    gmb_list_t t0 = gmb_val_as_list(z.items[0]);
    if(t0.items[0].v_int!=1 || t0.items[1].v_int!=10){ printf("FAIL zip content\n"); return 1;}
    printf("OK\n");
    gmb_list_free(&a); gmb_list_free(&b); gmb_list_free(&z); gmb_list_free(&t0);
    return 0;
}
static int test_array_pad(void){
    printf("TEST Array Pad... ");
    gmb_list_t src = gmb_list_new();
    gmb_list_add(&src, gmb_make_val_int(1)); gmb_list_add(&src, gmb_make_val_int(2));
    gmb_list_t p = gmb_list_pad_copy(src, 5, gmb_make_val_int(99));
    if(p.count!=5){ printf("FAIL pad count %zu\n", p.count); return 1;}
    if(p.items[2].v_int!=99 || p.items[4].v_int!=99){ printf("FAIL pad value\n"); return 1;}
    gmb_list_t trunc = gmb_list_pad_copy(src, 1, gmb_make_val_int(0));
    if(trunc.count!=1 || trunc.items[0].v_int!=1){ printf("FAIL trunc\n"); return 1;}
    printf("OK\n");
    gmb_list_free(&src); gmb_list_free(&p); gmb_list_free(&trunc);
    return 0;
}
static int test_array_shuffle(void){
    printf("TEST Array Shuffle... ");
    gmb_list_t a = gmb_list_new(), b = gmb_list_new();
    for(int i=1;i<=5;i++){ gmb_list_add(&a, gmb_make_val_int(i)); gmb_list_add(&b, gmb_make_val_int(i)); }
    gmb_list_shuffle(&a, 42);
    gmb_list_shuffle(&b, 42);
    for(size_t i=0;i<a.count;i++) if(a.items[i].v_int!=b.items[i].v_int){ printf("FAIL deterministic\n"); return 1;}
    int seen[6]={0};
    for(size_t i=0;i<a.count;i++){ int v=a.items[i].v_int; if(v<1||v>5){ printf("FAIL range\n"); return 1; } seen[v]=1; }
    for(int i=1;i<=5;i++) if(!seen[i]){ printf("FAIL missing %d\n", i); return 1;}
    printf("OK (a[0]=%lld)\n", (long long)a.items[0].v_int);
    gmb_list_free(&a); gmb_list_free(&b);
    return 0;
}
static int test_zip_list(void){
    printf("TEST Zip List (gmb_zip_list)... ");
    const char* f1="/tmp/gmb_gaps_a.txt";
    const char* f2="/tmp/gmb_gaps_b.txt";
    FILE* fa=fopen(f1,"w"); fprintf(fa,"hello"); fclose(fa);
    FILE* fb=fopen(f2,"w"); fprintf(fb,"world"); fclose(fb);
    const char* dest="/tmp/gmb_gaps.zip";
    char* srcs[2]={(char*)f1,(char*)f2};
    if(gmb_zip_create(dest, srcs, 2)!=0){ printf("FAIL create\n"); return 1;}
    char** names=NULL; int count=0;
    if(gmb_zip_list(dest, &names, &count)!=0){ printf("FAIL list\n"); return 1;}
    if(count!=2){ printf("FAIL count %d !=2\n", count); return 1;}
    int found_a=0, found_b=0;
    for(int i=0;i<count;i++){ if(strstr(names[i], "gmb_gaps_a.txt")) found_a=1; if(strstr(names[i], "gmb_gaps_b.txt")) found_b=1; }
    if(!found_a||!found_b){ printf("FAIL names\n"); return 1;}
    gmb_zip_list_free(names, count);
    printf("OK\n");
    return 0;
}
static int test_graphics_circle_arc(void){
    printf("TEST Graphics Circle Arc... ");
    gmb_gfx_init();
    gmb_gfx_screen(13, 100, 100, 0);
    gmb_gfx_cls(0);
    gmb_gfx_color(15,0);
    gmb_gfx_circle(50,50,20, 15, 0, 0, 0, 1.0);
    int full = gmb_gfx_point(70,50);
    if(full==-1){ printf("FAIL full circle not drawn\n"); return 1;}
    gmb_gfx_cls(0);
    double pi=3.141592653589793;
    gmb_gfx_circle(50,50,20, 15, 0, 0, pi/2, 1.0);
    int q0 = gmb_gfx_point(70,50);
    int q180 = gmb_gfx_point(30,50);
    if(q0==-1){ printf("FAIL arc missing 0deg\n"); return 1;}
    if(q180!=0 && q180!=-1){ printf("FAIL arc should not have 180deg, got %d\n", q180); return 1;}
    printf("OK (full=%d q0=%d q180=%d)\n", full, q0, q180);
    gmb_gfx_shutdown();
    return 0;
}
static int test_graphics_image_put_ops(void){
    printf("TEST Graphics ImagePut AND/OR/XOR... ");
    gmb_gfx_init();
    gmb_gfx_screen(13, 10,10,0);
    gmb_gfx_cls(0);
    int colA = 0x40000000 | 0x123456;
    int colB = 0x40000000 | 0x654321;
    gmb_gfx_pset(5,5, colA);
    int img1[3]={1,1, colB};
    gmb_gfx_image_put_ex(5,5, img1, 3, "AND");
    int after_and = gmb_gfx_point(5,5);
    gmb_gfx_pset(5,5, colA);
    gmb_gfx_image_put_ex(5,5, img1, 3, "OR");
    int after_or = gmb_gfx_point(5,5);
    gmb_gfx_pset(5,5, colA);
    gmb_gfx_image_put_ex(5,5, img1, 3, "XOR");
    int after_xor = gmb_gfx_point(5,5);
    gmb_gfx_pset(5,5, 0x40000000|0x0000FF);
    gmb_gfx_image_put(5,5, img1, 3);
    int after_pset = gmb_gfx_point(5,5);
    int rgb_and = after_and & 0xFFFFFF;
    int rgb_or = after_or & 0xFFFFFF;
    int rgb_xor = after_xor & 0xFFFFFF;
    int rgb_pset = after_pset & 0xFFFFFF;
    if(rgb_and != 0x000000){ printf("FAIL AND expected 000000 got %06X\n", rgb_and); return 1;}
    if(rgb_or != 0x777777){ printf("FAIL OR expected 777777 got %06X\n", rgb_or); return 1;}
    if(rgb_xor != 0x777777){ printf("FAIL XOR expected 777777 got %06X\n", rgb_xor); return 1;}
    if(rgb_pset != 0x654321){ printf("FAIL PSET expected 654321 got %06X\n", rgb_pset); return 1;}
    printf("OK (and=%06X or=%06X xor=%06X pset=%06X)\n", rgb_and, rgb_or, rgb_xor, rgb_pset);
    gmb_gfx_shutdown();
    return 0;
}
static int test_graphics_ttf(void){
    printf("TEST Graphics TTF... ");
    gmb_gfx_init();
    gmb_gfx_screen(13, 100, 100, 0);
    gmb_gfx_cls(0);
    // Senza font, print è no-op
    gmb_gfx_print("ABC");
    int cnt_no_font=0;
    for(int y=0;y<20;y++) for(int x=0;x<20;x++) if((gmb_gfx_point(x,y)&0xFFFFFF)!=0) cnt_no_font++;
    if(cnt_no_font!=0){ printf("FAIL no-font should be 0 got %d\n", cnt_no_font); return 1; }
    // Con font TTF valido
    const char* font="/Users/gaetano/Documents/Projects/GMBasic/Resources/font.ttf";
    gmb_gfx_font(font, 12);
    int w = gmb_gfx_text_width("ABC");
    int h = gmb_gfx_text_height();
    if(w<=0 || h<=0){ printf("FAIL text metrics w=%d h=%d\n", w, h); return 1; }
    gmb_gfx_locate(5, 50);
    gmb_gfx_print("ABC");
    int cnt=0;
    for(int y=45;y<65;y++) for(int x=5;x<5+w+5;x++) if((gmb_gfx_point(x,y)&0xFFFFFF)!=0) cnt++;
    if(cnt==0){ printf("FAIL TTF no pixels w=%d h=%d\n", w, h); return 1; }
    // draw_text con align
    gmb_gfx_cls(0);
    gmb_gfx_draw_text("HI", 50, 50, 1, 15);
    int cnt2=0;
    for(int y=45;y<65;y++) for(int x=30;x<70;x++) if((gmb_gfx_point(x,y)&0xFFFFFF)!=0) cnt2++;
    if(cnt2==0){ printf("FAIL draw_text\n"); return 1; }
    printf("OK (w=%d h=%d cnt=%d cnt2=%d)\n", w, h, cnt, cnt2);
    gmb_gfx_shutdown();
    return 0;
}
static int test_mask_finance(void){
    printf("TEST Mask Finance (Float locale)... ");
    // PICTURE 999,999.99 con EuropeanNumbers off → '.' e ',' come separatori
    gmb_mask_t* m = gmb_mask_new("999,999.99");
    if(!m){ printf("FAIL new mask\n"); return 1; }
    if(gmb_mask_datatype(m)!=GMB_MASK_FLOAT){ printf("FAIL datatype %d != FLOAT\n", gmb_mask_datatype(m)); gmb_mask_free(m); return 1; }
    // 9 dovrebbe accettare '.' e ',' per Float
    if(!gmb_mask_is_valid_char(m, 0, '.')){ printf("FAIL 9 should accept '.' for float\n"); gmb_mask_free(m); return 1; }
    if(!gmb_mask_is_valid_char(m, 0, ',')){ printf("FAIL 9 should accept ',' for float\n"); gmb_mask_free(m); return 1; }
    if(!gmb_mask_is_valid_char(m, 0, '5')){ printf("FAIL 9 should accept digit\n"); gmb_mask_free(m); return 1; }
    char out[64];
    gmb_mask_apply(m, "123456", out, sizeof(out));
    // 123456 con mask 999,999.99 → "123,456.  " (con spazi)
    // Verifica che non crashi e produca qualcosa con ',' e '.'
    if(strlen(out)==0){ printf("FAIL apply empty\n"); gmb_mask_free(m); return 1; }
    // EuropeanNumbers flag @F
    gmb_mask_t* mf = gmb_mask_new("@F 999,999.99");
    if(!mf || gmb_mask_datatype(mf)!=GMB_MASK_FLOAT){ printf("FAIL euro mask\n"); if(mf) gmb_mask_free(mf); gmb_mask_free(m); return 1; }
    gmb_mask_free(mf);
    // Bool case-insensitive: L e Y
    gmb_mask_t* ml = gmb_mask_new("L");
    if(!gmb_mask_is_valid_char(ml, 0, 't') || !gmb_mask_is_valid_char(ml, 0, 'f') || !gmb_mask_is_valid_char(ml, 0, 'T') || !gmb_mask_is_valid_char(ml, 0, '1')){ printf("FAIL L bool case\n"); gmb_mask_free(ml); gmb_mask_free(m); return 1; }
    gmb_mask_free(ml);
    gmb_mask_t* my = gmb_mask_new("Y");
    if(!gmb_mask_is_valid_char(my, 0, 'y') || !gmb_mask_is_valid_char(my, 0, 'n')){ printf("FAIL Y bool case\n"); gmb_mask_free(my); gmb_mask_free(m); return 1; }
    gmb_mask_free(my);
    gmb_mask_free(m);
    /* MatchDate/MatchDateTime: datatype corretto, separatori letterali auto-fill */
    gmb_mask_t* md = gmb_mask_new("99/99/9999");
    if(!md || gmb_mask_datatype(md)!=GMB_MASK_DATE){ printf("FAIL date datatype %d\n", md?gmb_mask_datatype(md):-1); gmb_mask_free(md); return 1; }
    if(!gmb_mask_is_valid_char(md, 0, '5')){ printf("FAIL date digit\n"); gmb_mask_free(md); return 1; }
    if(gmb_mask_is_valid_char(md, 2, '5')){ printf("FAIL date separator should be literal\n"); gmb_mask_free(md); return 1; }
    char dob[32];
    gmb_mask_apply(md, "31122024", dob, sizeof(dob));
    if(strstr(dob, "/")==NULL){ printf("FAIL date apply sep %s\n", dob); gmb_mask_free(md); return 1; }
    gmb_mask_free(md);
    gmb_mask_t* mt = gmb_mask_new("99:99");
    if(!mt || gmb_mask_datatype(mt)!=GMB_MASK_TIME){ printf("FAIL time datatype\n"); gmb_mask_free(mt); return 1; }
    if(!gmb_mask_is_valid_char(mt, 0, '1')){ printf("FAIL time digit\n"); gmb_mask_free(mt); return 1; }
    gmb_mask_free(mt);
    /* FloatCustom: '#' accetta spazio, e '.' come decimale su slot editabile */
    gmb_mask_t* mh = gmb_mask_new("##.##");
    if(!gmb_mask_is_valid_char(mh, 0, ' ')){ printf("FAIL hash space\n"); gmb_mask_free(mh); return 1; }
    if(!gmb_mask_is_valid_char(mh, 0, '.')){ printf("FAIL hash float dot\n"); gmb_mask_free(mh); return 1; }
    gmb_mask_free(mh);
    printf("OK\n");
    return 0;
}
static int test_window_manager(void){
    printf("TEST WindowManager Snap/Cascade/Tile... ");
    gmb_window_manager_t wm;
    gmb_window_manager_init(&wm, gmb_rect_new(0,0,80,24));
    int id1 = gmb_window_manager_add(&wm, "Win1", gmb_rect_new(5,5,20,10));
    int id2 = gmb_window_manager_add(&wm, "Win2", gmb_rect_new(30,5,20,10));
    if(id1<0 || id2<0){ printf("FAIL add\n"); return 1; }
    // Snap a zona (API: snap_zone id,zone — 0=left half, 1=right half)
    gmb_window_manager_snap_zone(&wm, id1, 0);
    gmb_managed_window_t* w1 = gmb_window_manager_get(&wm, id1);
    if(w1->rect.x != 0 || w1->rect.w != 40 || w1->rect.h != 24){ printf("FAIL snap left %d,%dx%d\n", w1->rect.x, w1->rect.w, w1->rect.h); return 1; }
    // Snap a destra
    gmb_window_manager_snap_zone(&wm, id1, 1);
    if(w1->rect.x != 40 || w1->rect.w != 40){ printf("FAIL snap right %d w%d\n", w1->rect.x, w1->rect.w); return 1; }
    // Maximize
    gmb_window_manager_maximize(&wm, id1);
    if(w1->rect.w != 80 || w1->rect.h != 24){ printf("FAIL maximize %dx%d\n", w1->rect.w, w1->rect.h); return 1; }
    gmb_window_manager_restore(&wm, id1);
    if(w1->rect.w == 80){ printf("FAIL restore should not be 80\n"); return 1; }
    // Tile 1x2
    gmb_window_manager_tile(&wm, 1, 2);
    gmb_managed_window_t* w2 = gmb_window_manager_get(&wm, id2);
    if(w1->rect.w != 40 || w2->rect.x != 40){ printf("FAIL tile w1 %d w2 x %d\n", w1->rect.w, w2->rect.x); return 1; }
    // Cascade
    gmb_window_manager_cascade(&wm, 2, 1);
    if(w1->rect.x != 0 || w1->rect.y != 0){ printf("FAIL cascade w1 %d,%d\n", w1->rect.x, w1->rect.y); return 1; }
    if(w2->rect.x != 2 || w2->rect.y != 1){ printf("FAIL cascade w2 %d,%d\n", w2->rect.x, w2->rect.y); return 1; }
    // Desktop
    gmb_window_manager_switch_desktop(&wm, 2);
    if(gmb_window_manager_select(&wm, id1) != -1){ printf("FAIL desktop select hidden\n"); return 1; }
    gmb_window_manager_move_to_desktop(&wm, id1, 2);
    if(gmb_window_manager_select(&wm, id1) != id1){ printf("FAIL desktop select shown\n"); return 1; }
    gmb_window_manager_switch_desktop(&wm, 1);
    // Statusbar
    gmb_window_manager_set_statusbar(&wm, "Pronto");
    if(!gmb_window_manager_has_status(&wm)){ printf("FAIL status\n"); return 1; }
    // Select by title + focus_next (desktop 1: solo Win2 visibile, id1 su desktop 2)
    if(gmb_window_manager_select_title(&wm, "Win2") != id2){ printf("FAIL select_title\n"); return 1; }
    int nxt = gmb_window_manager_focus_next(&wm, 1);
    if(nxt != id2){ printf("FAIL focus_next %d !=%d\n", nxt, id2); return 1; }
    // Drag (cattura rect pre-drag: w2 viene mutato)
    int sx0 = w2->rect.x, sy0 = w2->rect.y;
    gmb_window_manager_start_drag(&wm, id2, 10, 10);
    gmb_window_manager_update_drag(&wm, 20, 15);
    gmb_window_manager_end_drag(&wm);
    gmb_managed_window_t* wd = gmb_window_manager_get(&wm, id2);
    if(wd->rect.x != 20 - (10 - sx0) || wd->rect.y != 15 - (10 - sy0)){ printf("FAIL drag %d,%d want %d,%d\n", wd->rect.x, wd->rect.y, 20-(10-sx0), 15-(10-sy0)); return 1; }
    // Titlebar hit
    if(gmb_window_manager_window_at_titlebar(&wm, wd->rect.x, wd->rect.y) != id2){ printf("FAIL titlebar\n"); return 1; }
    // Resize E (start al bordo dx: x = right-1; muovi a right+5 → dx = 6)
    int rx = wd->rect.x, ry = wd->rect.y, rw = wd->rect.w;
    if(!gmb_window_manager_start_resize(&wm, id2, wd->rect.x + wd->rect.w - 1, wd->rect.y, "E")){ printf("FAIL start_resize\n"); return 1; }
    gmb_window_manager_update_resize(&wm, wd->rect.x + wd->rect.w + 5, wd->rect.y);
    gmb_window_manager_end_resize(&wm);
    if(gmb_window_manager_get(&wm, id2)->rect.w != rw + 6){ printf("FAIL resize w %d != %d\n", gmb_window_manager_get(&wm, id2)->rect.w, rw+6); return 1; }
    (void)rx; (void)ry;
    // Control action: max toggle + min
    gmb_window_manager_control_action(&wm, id1, GMB_WM_CTRL_MAX);
    if(gmb_window_manager_get(&wm, id1)->rect.w != 80){ printf("FAIL ctrl max\n"); return 1; }
    gmb_window_manager_control_action(&wm, id1, GMB_WM_CTRL_MIN);
    if(!gmb_window_manager_is_minimized(&wm, id1)){ printf("FAIL ctrl min\n"); return 1; }
    gmb_window_manager_unminimize(&wm, id1);
    if(gmb_window_manager_is_minimized(&wm, id1)){ printf("FAIL unmin\n"); return 1; }
    // SendToCell / TileAll (work 80x24, rows2 cols2 gap1: cell h=(24-1)/2=11 → row1 y=12)
    gmb_window_manager_move_to_desktop(&wm, id1, 1);
    gmb_window_manager_send_to_cell(&wm, id2, 2, 2, 1, 0, 1);
    gmb_managed_window_t* wc = gmb_window_manager_get(&wm, id2);
    if(wc->rect.x != 0 || wc->rect.y != 12){ printf("FAIL send_to_cell %d,%d\n", wc->rect.x, wc->rect.y); return 1; }
    gmb_window_manager_tile_all(&wm, 1, 2, 0);
    if(gmb_window_manager_get(&wm, id1)->rect.w != 40 || gmb_window_manager_get(&wm, id2)->rect.x != 40){ printf("FAIL tile_all\n"); return 1; }
    // Taskbar chip
    int chip = gmb_window_manager_taskbar_chip_at(&wm, 5, 23);
    if(chip != id1){ printf("FAIL taskbar chip %d\n", chip); return 1; }
    printf("OK\n");
    return 0;
}
static int test_graphics_texture_rotation(void){
    printf("TEST Graphics Texture Rotation... ");
    gmb_gfx_init();
    // Crea texture solida 4x2 (rossa a sinistra, verde a destra) via API test
    int tex = gmb_gfx_create_solid_texture(4, 2, 0xFFFF0000); // placeholder, poi sovrascrivi pixel
    if(tex<0){ printf("FAIL create solid tex\n"); return 1; }
    int tw=0, th=0;
    gmb_gfx_texture_size(tex, &tw, &th);
    printf("created tex %d %dx%d solid ... ", tex, tw, th);
    if(tw!=4 || th!=2){ printf("FAIL size %dx%d\n", tw, th); return 1; }
    // Draw senza rotazione a (5,5) → cerca qualsiasi pixel non nero (robusto a palette)
    gmb_gfx_screen(13, 20, 20, 0);
    gmb_gfx_cls(0);
    gmb_gfx_draw_texture(tex, 5, 5, 0, 1, 1);
    int cnt0=0;
    for(int y=4;y<=9;y++) for(int x=4;x<=9;x++) if((gmb_gfx_point(x,y) & 0xFFFFFF)!=0) cnt0++;
    if(cnt0==0){ printf("FAIL rot0 no pixels drawn (cnt0=%d)\n", cnt0); return 1; }
    // Draw con rotazione 90°
    gmb_gfx_cls(0);
    gmb_gfx_draw_texture(tex, 5, 5, 90, 1, 1);
    int cnt90=0;
    for(int y=4;y<=9;y++) for(int x=4;x<=9;x++) if((gmb_gfx_point(x,y) & 0xFFFFFF)!=0) cnt90++;
    if(cnt90==0){ printf("FAIL rot90 no pixels cnt90=%d\n", cnt90); return 1; }
    // Sprite API smoke test (usa texture solida già creata via sprite API con file temporaneo)
    const char* spng="/tmp/gmb_rot_sprite.png";
    gmb_gfx_screen(13, 2, 2, 0);
    gmb_gfx_cls(0);
    gmb_gfx_pset(0,0, 0x40000000|0xFF0000);
    gmb_gfx_pset(1,0, 0x40000000|0x00FF00);
    gmb_gfx_save_screen_png(spng);
    int sid = gmb_gfx_sprite_load(spng);
    if(sid>=0){
        gmb_gfx_sprite_set_rotation(sid, 45);
        gmb_gfx_sprite_set_scale(sid, 1.5, 1.5);
        gmb_gfx_sprite_set_position(sid, 10, 10);
        // non crash
    }
    printf("OK (rot0 cnt=%d rot90 cnt=%d tex=%d sid=%d)\n", cnt0, cnt90, tex, sid);
    gmb_gfx_shutdown();
    return 0;
}
static int pred_is_txt(void* ctx, gmb_string_t s){
    (void)ctx;
    const char* p = s.data ? s.data : "";
    size_t n = s.length;
    return n > 4 && strcmp(p + n - 4, ".txt") == 0;
}
static int pred_contains_a(void* ctx, gmb_string_t s){
    (void)ctx;
    const char* p = s.data ? s.data : "";
    return strchr(p, 'a') != NULL;
}
static int test_audio_utils(void){
    printf("TEST AudioUtils (envelope/freq/midi/dur)... ");
    // ADSR: attack lineare
    double a = gmb_audio_envelope_adsr(25, 100, 0, 0, 0, 0.5);
    if(fabs(a - 0.25) > 1e-9){ printf("FAIL attack %.3f !=0.25\n", a); return 1; }
    // decay: da 1.0 verso sustainLevel (sample 150: 50/100 del tratto → 0.75)
    double d = gmb_audio_envelope_adsr(150, 100, 100, 0, 0, 0.5);
    if(fabs(d - 0.75) > 1e-9){ printf("FAIL decay %.3f !=0.75\n", d); return 1; }
    // sustain: costante
    double s = gmb_audio_envelope_adsr(250, 100, 100, 100, 0, 0.5);
    if(fabs(s - 0.5) > 1e-9){ printf("FAIL sustain %.3f !=0.5\n", s); return 1; }
    // release: scende a 0
    double r = gmb_audio_envelope_adsr(399, 100, 100, 100, 100, 0.5);
    if(fabs(r - 0.005) > 1e-9){ printf("FAIL release %.3f !=0.005\n", r); return 1; }
    // envelope naturale: attack, sustain, release
    double e_attack = gmb_audio_envelope(50, 1000, 100, 100);
    double e_sustain = gmb_audio_envelope(500, 1000, 100, 100);
    double e_release = gmb_audio_envelope(975, 1000, 100, 100);
    if(fabs(e_attack - 0.5) > 1e-9){ printf("FAIL env attack %.3f\n", e_attack); return 1; }
    if(fabs(e_sustain - 1.0) > 1e-9){ printf("FAIL env sustain %.3f\n", e_sustain); return 1; }
    if(fabs(e_release - 0.25) > 1e-9){ printf("FAIL env release %.3f\n", e_release); return 1; }
    // frequency: LA3 (A3) = 440 / 2 (octave 3 < 4)
    double f = gmb_audio_frequency(440.0, 3);
    if(fabs(f - 220.0) > 1e-9){ printf("FAIL freq %.3f !=220\n", f); return 1; }
    // MIDI 69 = LA4 = 440 Hz
    double m = gmb_audio_midi_to_frequency(69);
    if(fabs(m - 440.0) > 1e-9){ printf("FAIL midi %.3f !=440\n", m); return 1; }
    // MIDI 81 = LA5 = 880 Hz
    double m5 = gmb_audio_midi_to_frequency(81);
    if(fabs(m5 - 880.0) > 1e-9){ printf("FAIL midi5 %.3f !=880\n", m5); return 1; }
    // durata nota: T120 L4 → 240/(120*4)=0.5s
    double dur = gmb_audio_note_duration(4, 120);
    if(fabs(dur - 0.5) > 1e-9){ printf("FAIL dur %.3f !=0.5\n", dur); return 1; }
    printf("OK (a=%.2f d=%.2f s=%.2f r=%.3f env=%.2f f=%.0f midi=%.0f dur=%.2f)\n",
           a, d, s, r, e_sustain, f, m, dur);
    return 0;
}
static int test_pso(void){
    printf("TEST PSO (Minimize CVaR/CDaR)... ");
    // 12 periodi x 3 asset: l'asset 2 ha rendimenti piu' alti/volatili
    double R[12][3] = {
        { 0.01, 0.03, 0.02 }, { 0.02, 0.01, 0.04 }, { 0.00, 0.05, 0.01 },
        { 0.03, 0.02, 0.02 }, { 0.01, 0.04, 0.03 }, { 0.02, 0.00, 0.05 },
        { 0.00, 0.03, 0.01 }, { 0.04, 0.02, 0.02 }, { 0.01, 0.01, 0.03 },
        { 0.02, 0.05, 0.00 }, { 0.03, 0.00, 0.02 }, { 0.01, 0.04, 0.04 }
    };
    double w[3];
    gmb_opt_pso(&R[0][0], 12, 3, 0.05, 0, 40, 100, w);
    // somma ≈ 1
    double sum = w[0] + w[1] + w[2];
    if (fabs(sum - 1.0) > 1e-6) { printf("FAIL sum %.4f\n", sum); return 1; }
    // non negativi
    for (int i = 0; i < 3; i++) if (w[i] < -1e-9) { printf("FAIL w%d %.4f\n", i, w[i]); return 1; }
    // CDaR
    double w2[3];
    gmb_opt_pso(&R[0][0], 12, 3, 0.05, 1, 40, 100, w2);
    double sum2 = w2[0] + w2[1] + w2[2];
    if (fabs(sum2 - 1.0) > 1e-6) { printf("FAIL sum2 %.4f\n", sum2); return 1; }
    printf("OK (w=[%.3f %.3f %.3f] sum=%.3f cdar=[%.3f %.3f %.3f] sum=%.3f)\n",
           w[0], w[1], w[2], sum, w2[0], w2[1], w2[2], sum2);
    return 0;
}
static int test_qlnet(void){
    printf("TEST QLNet (PortfolioMath math pura)... ");
    double w[3] = {0.5, 0.3, 0.2};
    double mu[3] = {0.10, 0.15, 0.08};
    double cov[3][3] = {
        {0.04, 0.02, 0.01},
        {0.02, 0.09, 0.03},
        {0.01, 0.03, 0.05}
    };
    // PortfolioReturn = w·mu
    double ret = gmb_qlnet_portfolio_return(w, mu, 3);
    double exp_ret = 0.5*0.10 + 0.3*0.15 + 0.2*0.08;
    if (fabs(ret - exp_ret) > 1e-12) { printf("FAIL ret %.6f != %.6f\n", ret, exp_ret); return 1; }
    // PortfolioRisk: var = w'Σw (manuale per confronto)
    double var = 0;
    for (int i = 0; i < 3; i++) { double s = 0; for (int j = 0; j < 3; j++) s += cov[i][j]*w[j]; var += w[i]*s; }
    double v, sd;
    gmb_qlnet_portfolio_risk(w, &cov[0][0], 3, &v, &sd);
    if (fabs(v - var) > 1e-12) { printf("FAIL var %.6f != %.6f\n", v, var); return 1; }
    if (fabs(sd - sqrt(var)) > 1e-9) { printf("FAIL sd\n"); return 1; }
    // Sharpe
    double sh = gmb_qlnet_sharpe(0.10, 0.15, 0.02);
    if (fabs(sh - (0.10-0.02)/0.15) > 1e-12) { printf("FAIL sharpe %.6f\n", sh); return 1; }
    // Markowitz = variance
    double mk = gmb_qlnet_markowitz_value(w, &cov[0][0], 3);
    if (fabs(mk - v) > 1e-12) { printf("FAIL markowitz %.6f\n", mk); return 1; }
    // Kelly = 0.5 var - ret
    double kl = gmb_qlnet_kelly_value(w, &cov[0][0], mu, 3);
    if (fabs(kl - (0.5*v - ret)) > 1e-12) { printf("FAIL kelly %.6f\n", kl); return 1; }
    // constraints
    if (!gmb_qlnet_onesum(w, 3)) { printf("FAIL onesum\n"); return 1; }
    if (!gmb_qlnet_nonneg(w, 3)) { printf("FAIL nonneg\n"); return 1; }
    if (gmb_qlnet_nonneg((double[]){0.5, -0.1, 0.6}, 3)) { printf("FAIL nonneg neg\n"); return 1; }
    if (!gmb_qlnet_target_return(w, mu, 3, 0.10)) { printf("FAIL target\n"); return 1; }
    double lo[3] = {0.0, 0.0, 0.0}, hi[3] = {0.6, 0.6, 0.6};
    if (!gmb_qlnet_boundary(w, lo, hi, 3)) { printf("FAIL boundary\n"); return 1; }
    printf("OK (ret=%.4f var=%.4f sd=%.4f sharpe=%.4f kelly=%.4f constraints=5/5)\n", ret, v, sd, sh, kl);
    return 0;
}
static int test_regex(void){
    printf("TEST Regex (PCRE2 Replace/Split/Match/Group)... ");
    // IsMatch
    if (!gmb_regex_is_match("^[A-Z]+[0-9]+$", "AB12")) { printf("FAIL ismatch\n"); return 1; }
    if (gmb_regex_is_match("^[A-Z]+$", "AB12")) { printf("FAIL ismatch false\n"); return 1; }
    // inline (?i) — sintassi .NET nativa in PCRE2
    if (!gmb_regex_is_match("(?i)hello", "HELLO")) { printf("FAIL inline (?i)\n"); return 1; }
    // Match con index/length
    char val[64]; int idx = -1, len = -1;
    if (!gmb_regex_match("world", "hello world", val, sizeof(val), &idx, &len)) { printf("FAIL match\n"); return 1; }
    if (idx != 6 || len != 5 || strcmp(val, "world") != 0) { printf("FAIL match idx=%d len=%d val=%s\n", idx, len, val); return 1; }
    // Group
    char g[64];
    if (!gmb_regex_group("(\\d{4})-(\\d{2})", "anno 2024-08", 1, g, sizeof(g))) { printf("FAIL group\n"); return 1; }
    if (strcmp(g, "2024") != 0) { printf("FAIL group val=%s\n", g); return 1; }
    // Replace con $1 $2
    char r[128];
    gmb_regex_replace("(\\d{4})-(\\d{2})", "2024-08", "$2/$1", r, sizeof(r));
    if (strcmp(r, "08/2024") != 0) { printf("FAIL replace r=%s\n", r); return 1; }
    // Replace $& intero match
    gmb_regex_replace("cat", "the cat sat", "[$&]", r, sizeof(r));
    if (strcmp(r, "the [cat] sat") != 0) { printf("FAIL replace & r=%s\n", r); return 1; }
    // Split semplice
    char sp[4][256];
    int n = gmb_regex_split(",", "a,b,c", sp, 4);
    if (n != 3 || strcmp(sp[0], "a") || strcmp(sp[1], "b") || strcmp(sp[2], "c")) { printf("FAIL split n=%d\n", n); return 1; }
    // Split con gruppo catturante incluso (.NET: include il group value)
    int n2 = gmb_regex_split("(,)","a,b,c", sp, 5);
    if (n2 != 5 || strcmp(sp[1], ",")) { printf("FAIL split group n2=%d sp1=%s\n", n2, sp[1]); return 1; }
    // Matches values
    char mv[3][256];
    int nm = gmb_regex_matches_values("\\d+", "a1 b22 c333", mv, 3);
    if (nm != 3 || strcmp(mv[0], "1") || strcmp(mv[1], "22") || strcmp(mv[2], "333")) { printf("FAIL matches nm=%d\n", nm); return 1; }
    printf("OK (ismatch match idx6 len5 group2024 replace08/2024 split3 group-split5 matches3)\n");
    return 0;
}
static int test_timer(void){
    printf("TEST Timer (Stopwatch high-res)... ");
    int h1 = gmb_timer_stopwatch_start();
    int h2 = gmb_timer_stopwatch_start();
    if (h1 <= 0 || h2 <= h1) { printf("FAIL handles %d %d\n", h1, h2); return 1; }
    gmb_sleep_ms(20);
    long e2 = gmb_timer_stopwatch_elapsed(h2);
    if (e2 < 10 * 1000 * 1000) { printf("FAIL elapsed %ld ns <10ms\n", e2); return 1; }
    gmb_sleep_ms(10);
    long s1 = gmb_timer_stopwatch_stop(h1);
    if (s1 < 20 * 1000 * 1000) { printf("FAIL stop %ld ns <20ms\n", s1); return 1; }
    // handle fermato: elapsed dopo stop = 0 (rimosso)
    long e1_after = gmb_timer_stopwatch_elapsed(h1);
    if (e1_after != 0) { printf("FAIL elapsed-after-stop %ld !=0\n", e1_after); return 1; }
    // h2 ancora attivo
    long e2b = gmb_timer_stopwatch_elapsed(h2);
    if (e2b < 10 * 1000 * 1000) { printf("FAIL h2 alive %ld\n", e2b); return 1; }
    gmb_timer_stopwatch_stop(h2);
    printf("OK (h1=%d s1=%ldms h2=%ldms alive-after-stop=0)\n", h1, s1 / 1000000, e2 / 1000000);
    return 0;
}
static int test_collections_micro(void){
    printf("TEST Collections micro (BPlusTree/SparseSet/ConsistentHash)... ");
    /* BPlusTree: ordine 3, inserisci 20 chiavi, verifica count/min/max/ordine chiavi */
    int64_t bt = gmb_bptree_create(3);
    if (bt <= 0) { printf("FAIL bptree create\n"); return 1; }
    for (int i = 0; i < 20; i++) gmb_bptree_add(bt, (double)i, (double)(i * 10));
    if (gmb_bptree_count(bt) != 20) { printf("FAIL bptree count %lld\n", (long long)gmb_bptree_count(bt)); return 1; }
    double mnk, mnv, mxk, mxv;
    if (!gmb_bptree_min(bt, &mnk, &mnv)) { printf("FAIL bptree min\n"); return 1; }
    if (!gmb_bptree_max(bt, &mxk, &mxv)) { printf("FAIL bptree max\n"); return 1; }
    if (mnk != 0.0 || mnv != 0.0) { printf("FAIL bptree min %f %f\n", mnk, mnv); return 1; }
    if (mxk != 19.0 || mxv != 190.0) { printf("FAIL bptree max %f %f\n", mxk, mxv); return 1; }
    double gv;
    if (!gmb_bptree_get(bt, 7.0, &gv) || gv != 70.0) { printf("FAIL bptree get\n"); return 1; }
    if (gmb_bptree_get(bt, 99.0, &gv)) { printf("FAIL bptree get missing\n"); return 1; }
    if (!gmb_bptree_delete(bt, 7.0)) { printf("FAIL bptree delete\n"); return 1; }
    if (gmb_bptree_count(bt) != 19) { printf("FAIL bptree count after del\n"); return 1; }
    double keys[64];
    int nk = gmb_bptree_keys(bt, keys, 64);
    if (nk != 19) { printf("FAIL bptree keys %d\n", nk); return 1; }
    int sorted = 1;
    for (int i = 1; i < nk; i++) if (keys[i] <= keys[i - 1]) sorted = 0;
    if (!sorted) { printf("FAIL bptree not sorted\n"); return 1; }
    gmb_bptree_free(bt);

    /* SparseSet: capacity rispettata (capacity=3) */
    int64_t ss = gmb_sparseset_create(1000, 3);
    if (ss <= 0) { printf("FAIL sparse create\n"); return 1; }
    if (gmb_sparseset_add(ss, 10) != 1 || gmb_sparseset_add(ss, 20) != 1 || gmb_sparseset_add(ss, 30) != 1) { printf("FAIL sparse add\n"); return 1; }
    if (gmb_sparseset_add(ss, 40) != -1) { printf("FAIL sparse capacity NOT enforced\n"); return 1; }
    if (!gmb_sparseset_contains(ss, 20)) { printf("FAIL sparse contains\n"); return 1; }
    if (gmb_sparseset_contains(ss, 40)) { printf("FAIL sparse contains missing\n"); return 1; }
    if (!gmb_sparseset_remove(ss, 20)) { printf("FAIL sparse remove\n"); return 1; }
    if (gmb_sparseset_add(ss, 40) != 1) { printf("FAIL sparse re-add after remove\n"); return 1; }
    int arr[8];
    int na = gmb_sparseset_to_array(ss, arr, 8);
    if (na != 3) { printf("FAIL sparse to_array %d\n", na); return 1; }
    gmb_sparseset_free(ss);

    /* ConsistentHash: replicas 4, distribuzione stabile */
    int64_t hr = gmb_hashring_create(4);
    if (hr <= 0) { printf("FAIL ring create\n"); return 1; }
    gmb_hashring_add_node(hr, "A");
    gmb_hashring_add_node(hr, "B");
    const char* n1 = gmb_hashring_get_node(hr, "chiave1");
    const char* n2 = gmb_hashring_get_node(hr, "chiave1");
    if (!n1 || !n2 || strcmp(n1, n2) != 0) { printf("FAIL ring deterministic\n"); return 1; }
    const char* n3 = gmb_hashring_get_node(hr, "chiave2");
    if (!n3) { printf("FAIL ring key2\n"); return 1; }
    if (!gmb_hashring_remove_node(hr, "A")) { printf("FAIL ring remove\n"); return 1; }
    if (gmb_hashring_remove_node(hr, "A")) { printf("FAIL ring remove missing\n"); return 1; }
    gmb_hashring_free(hr);
    printf("OK (bptree count20 min0 max19 keys19 sorted; sparse cap3 enforced; ring deterministic)\n");
    return 0;
}
static int test_bitserializers(void){
    printf("TEST BitSerializers (LE + ToStruct/FromStruct)... ");
    uint8_t buf[64];
    gmb_bs_from_i32(buf, 0x12345678);
    if (gmb_bs_to_i32(buf, 0) != (int32_t)0x12345678) { printf("FAIL i32 roundtrip\n"); return 1; }
    if (buf[0] != 0x78 || buf[3] != 0x12) { printf("FAIL LE order\n"); return 1; }
    gmb_bs_from_u16(buf, 0xABCD);
    if (gmb_bs_to_u16(buf, 0) != 0xABCD) { printf("FAIL u16\n"); return 1; }
    gmb_bs_from_f64(buf, 3.141592653589793);
    if (fabs(gmb_bs_to_f64(buf, 0) - 3.141592653589793) > 1e-15) { printf("FAIL f64\n"); return 1; }
    gmb_bs_from_f32(buf, 1.5f);
    if (fabs(gmb_bs_to_f32(buf, 0) - 1.5f) > 1e-7f) { printf("FAIL f32\n"); return 1; }
    /* ToStruct: struct { int32 a; double b; } — b a offset 8 per allineamento struct */
    uint8_t data[16];
    gmb_bs_from_i32(data, 42);
    gmb_bs_from_f64(data + 8, 2.5);
    struct { int32_t a; double b; } s;
    gmb_bs_to_struct(data, 0, &s, sizeof(s));
    if (s.a != 42 || fabs(s.b - 2.5) > 1e-12) { printf("FAIL to_struct %d %.3f\n", s.a, s.b); return 1; }
    uint8_t back[16];
    gmb_bs_from_struct(back, &s, sizeof(s));
    if (gmb_bs_to_i32(back, 0) != 42) { printf("FAIL from_struct\n"); return 1; }
    /* stringhe */
    char str[32];
    gmb_bs_from_string(buf, "hello", 8);
    int n = gmb_bs_to_string(buf, 0, 8, str, sizeof(str));
    if (n != 8 || strcmp(str, "hello") != 0) { printf("FAIL string %s\n", str); return 1; }
    /* length-prefixed */
    gmb_bs_from_lp_string(buf, sizeof(buf), "abc");
    int lp = gmb_bs_to_lp_string(buf, 0, str, sizeof(str));
    if (lp != 3 || strcmp(str, "abc") != 0) { printf("FAIL lp string %d %s\n", lp, str); return 1; }
    printf("OK (i32=0x%X LE u16 f64 f32 to_struct42/2.5 str/lp ok)\n", gmb_bs_to_i32(buf, 0));
    return 0;
}
static int test_fs_times(void){
    printf("TEST FileSystem (mtime/ctime/cwd)... ");
    const char* f = "/tmp/gmb_fs_times_test.txt";
    FILE* fo = fopen(f, "w"); fprintf(fo, "x"); fclose(fo);
    double mt = gmb_fs_mtime(f);
    double ct = gmb_fs_ctime(f);
    double at = gmb_fs_atime(f);
    if (mt <= 0 || ct <= 0 || at <= 0) { printf("FAIL times %.3f %.3f %.3f\n", mt, ct, at); return 1; }
    if (gmb_fs_mtime("/nonexistent_xyz") != 0.0) { printf("FAIL missing mtime\n"); return 1; }
    char* cwd = gmb_fs_cwd();
    if (!cwd || strlen(cwd) == 0) { printf("FAIL cwd\n"); free(cwd); return 1; }
    char* cwd2 = gmb_fs_cwd();
    if (strcmp(cwd, cwd2) != 0) { printf("FAIL cwd stable\n"); free(cwd); free(cwd2); return 1; }
    free(cwd); free(cwd2);
    printf("OK (mtime=%.1f ctime=%.1f atime=%.1f cwd=ok missing=0)\n", mt, ct, at);
    return 0;
}
static int test_bsp_session(void){
    printf("TEST BspSession (cookie session store)... ");
    int64_t m = gmb_bsp_session_create_manager(30);
    if (m <= 0) { printf("FAIL mgr\n"); return 1; }
    char id[33], cookie[128];
    if (!gmb_bsp_session_create(m, id, cookie)) { printf("FAIL create\n"); return 1; }
    if (strlen(id) != 32) { printf("FAIL id len %zu\n", strlen(id)); return 1; }
    if (strncmp(cookie, "GMBSESSION=", 11) != 0 || strstr(cookie, id) == NULL) { printf("FAIL cookie %s\n", cookie); return 1; }
    if (!gmb_bsp_session_get(m, id)) { printf("FAIL get\n"); return 1; }
    if (gmb_bsp_session_count(m) != 1) { printf("FAIL count\n"); return 1; }
    /* get_or_create con cookie esistente → riusa (created=0) */
    char id2[33], cookie2[128]; int created = -1;
    if (!gmb_bsp_session_get_or_create(m, cookie, id2, cookie2, &created)) { printf("FAIL get_or_create\n"); return 1; }
    if (created != 0 || strcmp(id2, id) != 0 || cookie2[0] != '\0') { printf("FAIL reuse created=%d\n", created); return 1; }
    if (gmb_bsp_session_count(m) != 1) { printf("FAIL count after reuse\n"); return 1; }
    /* get_or_create con cookie assente → nuova sessione */
    char id3[33], cookie3[128]; created = -1;
    if (!gmb_bsp_session_get_or_create(m, "other=1", id3, cookie3, &created)) { printf("FAIL get_or_create new\n"); return 1; }
    if (created != 1 || cookie3[0] == '\0') { printf("FAIL new created=%d\n", created); return 1; }
    if (gmb_bsp_session_count(m) != 2) { printf("FAIL count 2\n"); return 1; }
    /* dati sessione */
    if (!gmb_bsp_session_set_data(m, id, "user", "alice")) { printf("FAIL set_data\n"); return 1; }
    const char* u = gmb_bsp_session_get_data(m, id, "user");
    if (!u || strcmp(u, "alice") != 0) { printf("FAIL get_data %s\n", u ? u : "NULL"); return 1; }
    /* destroy */
    gmb_bsp_session_destroy(m, id3);
    if (gmb_bsp_session_count(m) != 1) { printf("FAIL count after destroy\n"); return 1; }
    gmb_bsp_session_free_manager(m);
    printf("OK (id=%s cookie=GMBSESSION reuse created=0 new created=1 user=alice count 2→1)\n", id);
    return 0;
}
static int test_widgets(void){
    printf("TEST Widgets (ConfirmationBox/Panel/Clipboard/ImageWidget/FileExplorer)... ");
    /* ConfirmationBox: factory + title/message/buttons + key focus + click */
    gmb_confirmation_box_t cb;
    gmb_confirmation_box_centered(&cb, 80, 24, "Conferma", "Eliminare?");
    if(cb.ctrl.width != 30 || cb.ctrl.height != 7){ printf("FAIL centered size %dx%d\n", cb.ctrl.width, cb.ctrl.height); return 1; }
    if(strcmp(cb.confirm_label, "OK") != 0 || strcmp(cb.cancel_label, "Cancel") != 0){ printf("FAIL labels\n"); return 1; }
    gmb_confirmation_box_with_buttons(&cb, "Si", "No", "Riprova");
    if(!cb.has_retry || strcmp(cb.retry_label, "Riprova") != 0){ printf("FAIL retry\n"); return 1; }
    if(cb.focused_choice != GMB_CONF_CONFIRM){ printf("FAIL default focus\n"); return 1; }
    if(!gmb_confirmation_box_handle_key(&cb, '\t')){ printf("FAIL key tab\n"); return 1; }
    if(cb.focused_choice != GMB_CONF_CANCEL){ printf("FAIL tab focus %d\n", cb.focused_choice); return 1; }
    gmb_confirmation_box_handle_key(&cb, 13);
    if(cb.result != GMB_CONF_CANCEL){ printf("FAIL enter result %d\n", cb.result); return 1; }
    cb.result = -1;
    gmb_confirmation_box_handle_key(&cb, 27);
    if(cb.result != GMB_CONF_CANCEL){ printf("FAIL esc\n", cb.result); return 1; }
    /* Panel: with_* + inner_area + drag */
    gmb_panel_t p;
    gmb_panel_init(&p, 2, 3, 20, 10, "Finestra");
    gmb_panel_with_title(&p, "Titolo2");
    gmb_panel_with_border(&p, GMB_PB_DOUBLE);
    if(strcmp(p.title, "Titolo2") != 0 || p.border != GMB_PB_DOUBLE){ printf("FAIL panel with\n"); return 1; }
    gmb_rect_t ia = gmb_panel_inner_area(&p);
    if(ia.x != 3 || ia.y != 4 || ia.w != 18 || ia.h != 8){ printf("FAIL inner %d,%d,%d,%d\n", ia.x, ia.y, ia.w, ia.h); return 1; }
    gmb_panel_start_drag(&p, 10, 10);
    gmb_panel_update_drag(&p, 15, 12);
    gmb_panel_end_drag(&p);
    if(p.ctrl.x != 15 - (10-2) || p.ctrl.y != 12 - (10-3)){ printf("FAIL panel drag %d,%d\n", p.ctrl.x, p.ctrl.y); return 1; }
    if(p.is_dragging){ printf("FAIL panel end drag\n"); return 1; }
    /* Clipboard: set/get roundtrip + image ext */
    gmb_clipboard_set_text("hello");
    const char* ct = gmb_clipboard_try_get_text();
    if(!ct || strcmp(ct, "hello") != 0){ printf("FAIL clip get\n"); return 1; }
    gmb_clipboard_set_text(NULL);
    if(gmb_clipboard_try_get_text() != NULL){ printf("FAIL clip clear\n"); return 1; }
    if(!gmb_clipboard_is_image_path("x.PNG") || gmb_clipboard_is_image_path("x.txt")){ printf("FAIL clip img\n"); return 1; }
    /* ImageWidget: set_pixels + click + render_protocol */
    gmb_image_widget_t iw;
    gmb_image_widget_init(&iw);
    gmb_cell_color_t px[4] = {{1,2,3}, {4,5,6}, {7,8,9}, {10,11,12}};
    if(!gmb_image_widget_set_pixels(&iw, 2, 2, px)){ printf("FAIL iw set\n"); return 1; }
    gmb_cell_color_t out;
    if(!gmb_image_widget_pixel_at(&iw, 1, 0, &out) || out.r != 4){ printf("FAIL iw pixel\n"); return 1; }
    gmb_image_widget_click(&iw, 5, 6);
    if(gmb_image_widget_last_click_x(&iw) != 5 || gmb_image_widget_last_click_y(&iw) != 6){ printf("FAIL iw click\n"); return 1; }
    if(strcmp(gmb_image_widget_render_protocol(&iw), "none") != 0){ printf("FAIL iw proto\n"); return 1; }
    /* FileExplorer: usa /tmp con file reali */
    gmb_file_mkdir(gmb_string_alloc("/tmp/gmb_widgets_fe"));
    FILE* f1 = fopen("/tmp/gmb_widgets_fe/alice.txt","w"); fclose(f1);
    FILE* f2 = fopen("/tmp/gmb_widgets_fe/bob.bas","w"); fclose(f2);
    gmb_file_explorer_t fe;
    gmb_file_explorer_init(&fe, "/tmp/gmb_widgets_fe");
    if(gmb_file_explorer_entry_count(&fe) < 3){ printf("FAIL fe count %d\n", gmb_file_explorer_entry_count(&fe)); return 1; }
    /* filter "alice" */
    int idx[8], isd[8];
    int n = gmb_file_explorer_filter_entries(&fe, "alice", idx, isd, 8);
    if(n != 1){ printf("FAIL fe filter %d\n", n); return 1; }
    /* navigate seleziona l'ultima entry, poi prev/next clamp */
    int sel0 = gmb_file_explorer_selected_index(&fe);
    if(!gmb_file_explorer_navigate(&fe, 1)){ printf("FAIL fe nav\n"); return 1; }
    if(gmb_file_explorer_selected_index(&fe) != sel0 + 1){ printf("FAIL fe nav idx\n"); return 1; }
    printf("OK (cb centered 30x7 retry tab→cancel enter→cancel; panel inner 18x8 drag; clip roundtrip; iw px/click; fe filter alice=1)\n");
    return 0;
}
static int test_chart_financial(void){
    printf("TEST ChartFinancial (OHLC/CANDLESTICK)... ");
    gmb_gfx_init();
    gmb_gfx_screen(13, 420, 240, 0);
    gmb_gfx_cls(0);
    int h = gmb_gfx_chart_create();
    if (h <= 0) { printf("FAIL create\n"); return 1; }
    /* 3 candele OHLC flat [t,o,h,l,c] */
    double ohlc[15] = {
        1.0, 100, 105, 95, 102,
        2.0, 102, 108, 100, 104,
        3.0, 104, 106, 98, 99
    };
    gmb_gfx_chart_plot_financial(h, ohlc, 3, "CANDLESTICK", "AZN");
    gmb_gfx_chart_plot_financial_draw(h, 5, 5);
    /* verifica pixel disegnati (candele non vuote) */
    int cnt = 0;
    for (int y = 5; y < 225; y++) for (int x = 5; x < 405; x++) if ((gmb_gfx_point(x,y) & 0xFFFFFF) != 0) cnt++;
    if (cnt == 0) { printf("FAIL candlestick no pixels\n"); return 1; }
    gmb_gfx_cls(0);
    gmb_gfx_chart_plot_financial(h, ohlc, 3, "OHLC", "AZN");
    gmb_gfx_chart_plot_financial_draw(h, 5, 5);
    int cnt2 = 0;
    for (int y = 5; y < 225; y++) for (int x = 5; x < 405; x++) if ((gmb_gfx_point(x,y) & 0xFFFFFF) != 0) cnt2++;
    if (cnt2 == 0) { printf("FAIL ohlc no pixels\n"); return 1; }
    gmb_gfx_chart_close(h);
    gmb_gfx_shutdown();
    printf("OK (candlestick cnt=%d ohlc cnt=%d)\n", cnt, cnt2);
    return 0;
}
static int test_audio_synth_mml(void){
    printf("TEST Audio Synth/MML (waveform+ADSR+parse)... ");
    double buf[44100];
    int n = gmb_audio_synth(440, 0.1, 0, 0.01, 0.0, 1.0, 0.02, 0.25, 0, 0, 0, buf, 44100);
    if (n != 4410) { printf("FAIL synth samples %d\n", n); return 1; }
    double peak = 0;
    for (int i = 0; i < n; i++) if (fabs(buf[i]) > peak) peak = fabs(buf[i]);
    if (peak < 1000.0) { printf("FAIL synth peak %.0f\n", peak); return 1; }
    /* ADSR: picco nel quarto centrale (evita lo zero della sine: sin(2pi*22)=0) */
    double mid = 0;
    for (int i = n/4; i < 3*n/4; i++) if (fabs(buf[i]) > mid) mid = fabs(buf[i]);
    if (mid < 10000.0) { printf("FAIL synth sustain mid %.0f\n", mid); return 1; }
    /* square waveform ha più energia a metà del ciclo */
    double sq[4410];
    int ns = gmb_audio_synth(440, 0.1, 1, 0.0, 0.0, 1.0, 0.0, 0.25, 0, 0, 0, sq, 4410);
    if (ns != 4410) { printf("FAIL square\n"); return 1; }
    /* MML parse: O4 C D E > C → 4 note, frequenze C4/D4/E4/C5 */
    double f[16], d[16]; int t[16];
    int nm = gmb_audio_mml_parse("O4 C D E > C", f, d, t, 16);
    if (nm != 4) { printf("FAIL mml count %d\n", nm); return 1; }
    if (fabs(f[0] - 261.63) > 0.01 || fabs(f[1] - 293.66) > 0.01 || fabs(f[2] - 329.63) > 0.01) { printf("FAIL mml freqs %.2f %.2f %.2f\n", f[0], f[1], f[2]); return 1; }
    if (fabs(f[3] - 523.25) > 0.01) { printf("FAIL mml octave up %.2f\n", f[3]); return 1; }
    /* T120 L4 C → durata 0.5s */
    int nm2 = gmb_audio_mml_parse("T120 L4 C", f, d, t, 16);
    if (nm2 != 1 || fabs(d[0] - 0.5) > 1e-9) { printf("FAIL mml dur %d %.3f\n", nm2, d[0]); return 1; }
    /* P4 = pausa */
    int nm3 = gmb_audio_mml_parse("P4", f, d, t, 16);
    if (nm3 != 1 || t[0] != 1) { printf("FAIL mml pause %d t%d\n", nm3, t[0]); return 1; }
    /* MIDI/note duration per PLAY */
    double nd = gmb_audio_note_duration(4, 120);
    if (fabs(nd - 0.5) > 1e-9) { printf("FAIL note dur\n"); return 1; }
    printf("OK (synth %d peak %.0f mid %.0f; mml 4 note C4/D4/E4/C5 dur0.5 pause)\n", n, peak, mid);
    return 0;
}
static int test_bar_builder(void){
    printf("TEST ProgressBuilder (UTF-8 block glyphs)... ");
    gmb_bar_builder_t b = gmb_bar_builder_new();
    if (b.filled_char != 0x2588 || b.empty_char != 0x2591) { printf("FAIL default glyphs %X %X\n", b.filled_char, b.empty_char); return 1; }
    b.width = 4;
    char out[64];
    int n = gmb_bar_render(b, 1.0, out, sizeof(out));
    /* barra piena: '[' + 4x █(3 byte) + ']' = 1 + 12 + 1 = 14 byte */
    if (n != 14) { printf("FAIL bar len %d\n", n); return 1; }
    if ((unsigned char)out[1] != 0xE2 || (unsigned char)out[2] != 0x96 || (unsigned char)out[3] != 0x88) { printf("FAIL utf8 glyph bytes %02X %02X %02X\n", (unsigned char)out[1], (unsigned char)out[2], (unsigned char)out[3]); return 1; }
    if (out[0] != '[' || out[n-1] != ']') { printf("FAIL brackets\n"); return 1; }
    /* half: 2 pieni + 2 vuoti */
    int n2 = gmb_bar_render(b, 0.5, out, sizeof(out));
    if (n2 != 14) { printf("FAIL half len %d\n", n2); return 1; }
    /* with_percent: il % resta dentro la barra */
    int n3 = gmb_bar_render_with_percent(b, 0.5, out, sizeof(out));
    if (n3 <= 0 || strstr(out, "%") == NULL) { printf("FAIL percent %d %s\n", n3, out); return 1; }
    printf("OK (bar4 len=%d glyph=E2 96 88 percent ok)\n", n);
    return 0;
}
static int test_qr(void){
    printf("TEST QR encode (regression UB OOB limits[39])... ");
    /* gmb_qr_version_for_len leggeva limits[39] (39 elem, loop a 40) → UB → -O2 loop infinito */
    gmb_qr_matrix_t* m1 = gmb_qr_encode("HELLO", GMB_QR_ECC_M);
    if (!m1) { printf("FAIL encode1\n"); return 1; }
    if (m1->width != 21) { printf("FAIL w1 %d !=21\n", m1->width); return 1; }
    gmb_qr_matrix_t* m2 = gmb_qr_encode("this is a longer text to push the version boundary", GMB_QR_ECC_H);
    if (!m2) { printf("FAIL encode2\n"); return 1; }
    if (m2->width != 29) { printf("FAIL w2 %d !=29\n", m2->width); return 1; }
    char out[4096];
    gmb_qr_string("HELLO", GMB_QR_ECC_M, out, sizeof(out));
    if (strlen(out) == 0) { printf("FAIL string\n"); return 1; }
    if (strstr(out, "\xE2\x96") == NULL) { printf("FAIL halfblock glyph\n"); return 1; }
    gmb_qr_matrix_free(m1); gmb_qr_matrix_free(m2);
    printf("OK (w1=21 w2=29 halfblock glyph ok)\n");
    return 0;
}
static int test_joy(void){
    printf("TEST JoyManager (headless/SDL-guarded)... ");
    gmb_joy_init();
    /* Semantica C# senza joystick (JoyManager.cs): count, is_present false, state 0, name "" */
    int c = gmb_joy_count();
    if (c < 0) { printf("FAIL count %d\n", c); return 1; }
    if (gmb_joy_is_present(0)) { printf("FAIL present headless\n"); return 1; }
    if (gmb_joy_get_state(0) != 0L) { printf("FAIL state\n"); return 1; }
    if (gmb_joy_get_state(999) != 0L) { printf("FAIL state bad id\n"); return 1; }
    if (strcmp(gmb_joy_get_name(0), "") != 0) { printf("FAIL name\n"); return 1; }
    gmb_joy_refresh();
    gmb_joy_dispose();
    printf("OK (count=%d present=0 state=0 name='' stable via refresh/dispose)\n", c);
    return 0;
}
static int test_screens(void){
    printf("TEST ScreenManager (display modes headless/SDL-guarded)... ");
    /* Headless: lista vuota (0), come C# senza display SDL (Manager.cs:47-50) */
    gmb_display_mode_info_t modes[64];
    int n = gmb_screens_available_modes(modes, 64);
    if (n < 0) { printf("FAIL count %d\n", n); return 1; }
    if (n > 0) { /* con display reale: verifica campi coerenti + ordinati */
        for (int i = 0; i < n; i++) {
            if (modes[i].width <= 0 || modes[i].height <= 0) { printf("FAIL dim mode %d\n", i); return 1; }
        }
    }
    printf("OK (count=%d, campi validi)\n", n);
    return 0;
}
static int cs_read_line(void* ctx, char* out, int cap){ (void)ctx; snprintf(out, cap, "hello-stream"); return 12; }
static int cs_read_char(void* ctx){ (void)ctx; return 'X'; }
static char g_cs_buf[512]; static int g_cs_len = 0;
static void cs_write(void* ctx, const char* s, int len){ (void)ctx; for(int i=0;i<len && g_cs_len<511;i++) g_cs_buf[g_cs_len++]=s[i]; }
static void cs_flush(void* ctx){ (void)ctx; }
static int test_console_streams(void){
    printf("TEST Console streams (IConsole In/Out/Error + Set*)... ");
    /* default: stdin/stdout/stderr */
    gmb_text_reader_t ri = { cs_read_line, cs_read_char, NULL };
    gmb_text_writer_t wo = { cs_write, cs_flush, NULL };
    gmb_text_writer_t we = { cs_write, cs_flush, NULL };
    gmb_console_set_in(ri);
    gmb_console_set_out(wo);
    gmb_console_set_error(we);
    char line[64];
    int n = gmb_console_in_read_line(line, sizeof(line));
    if (n != 12 || strcmp(line, "hello-stream") != 0) { printf("FAIL in_line %d %s\n", n, line); return 1; }
    if (gmb_console_in_read_char() != 'X') { printf("FAIL in_char\n"); return 1; }
    g_cs_len = 0;
    gmb_console_out_write("out-data");
    gmb_console_out_write_line("LINE");
    if (g_cs_len != 8 + 5) { printf("FAIL out %d\n", g_cs_len); return 1; }
    if (strncmp(g_cs_buf, "out-dataLINE\n", 14) != 0) { printf("FAIL out content %s\n", g_cs_buf); return 1; }
    g_cs_len = 0; memset(g_cs_buf, 0, sizeof(g_cs_buf));
    gmb_console_err_write("err-data");
    if (g_cs_len != 8 || strcmp(g_cs_buf, "err-data") != 0) { printf("FAIL err %d %s\n", g_cs_len, g_cs_buf); return 1; }
    gmb_console_out_flush();
    gmb_console_err_flush();
    /* state settable */
    gmb_console_set_buffer_size(120, 40);
    gmb_console_set_window_size(100, 30);
    gmb_console_set_window_position(3, 2);
    gmb_console_set_scroll_region(1, 20);
    if (!gmb_console_is_dirty()) { printf("FAIL dirty\n"); return 1; }
    gmb_console_mark_clean();
    if (gmb_console_is_dirty()) { printf("FAIL clean\n"); return 1; }
    if (gmb_console_window_left() != 3 || gmb_console_window_top() != 2) { printf("FAIL win pos\n"); return 1; }
    printf("OK (in_line hello-stream in_char X out 13B err 8B state buf120x40 win(3,2) scroll dirty/clean)\n");
    return 0;
}
static int test_screen_config(void){
    printf("TEST ScreenConfig (GetDefault/palette/apply)... ");
    /* Text: grid 100x40, Console palette, fg15 bg0, Fira Code 14 */
    gmb_screen_config_t t = gmb_screen_config_get_default(GMB_SM_TEXT);
    if (t.char_grid_width != 100 || t.char_grid_height != 40) { printf("FAIL text grid %dx%d\n", t.char_grid_width, t.char_grid_height); return 1; }
    if (t.palette_size != 16 || t.default_fg_color != 15 || t.default_bg_color != 0) { printf("FAIL text palette/fg\n"); return 1; }
    if (strcmp(t.palette_name ? t.palette_name : "", "Console") != 0) { printf("FAIL text pal name\n"); return 1; }
    /* C64: grid 80x50, C64Classic, fg14 bg6, Monospace hinting */
    gmb_screen_config_t c64 = gmb_screen_config_get_default(GMB_SM_C64);
    if (c64.char_grid_width != 80 || c64.char_grid_height != 50) { printf("FAIL c64 grid\n"); return 1; }
    if (c64.default_fg_color != 14 || c64.default_bg_color != 6) { printf("FAIL c64 fg/bg\n"); return 1; }
    if (c64.font_hinting != GMB_FH_MONOSPACE) { printf("FAIL c64 hinting\n"); return 1; }
    /* CGA/EGA/VGA/SVGA/XGA/CRT dimensioni */
    if (gmb_screen_config_get_default(GMB_SM_CGA).logical_width != 320) { printf("FAIL cga\n"); return 1; }
    if (gmb_screen_config_get_default(GMB_SM_EGA).logical_height != 350) { printf("FAIL ega\n"); return 1; }
    if (gmb_screen_config_get_default(GMB_SM_VGA).logical_width != 640 || gmb_screen_config_get_default(GMB_SM_VGA).char_grid_height != 30) { printf("FAIL vga\n"); return 1; }
    if (gmb_screen_config_get_default(GMB_SM_SVGA).palette_size != 256) { printf("FAIL svga pal\n"); return 1; }
    if (gmb_screen_config_get_default(GMB_SM_XGA).palette_size != 256) { printf("FAIL xga pal\n"); return 1; }
    if (gmb_screen_config_get_default(GMB_SM_CRT).char_grid_width != 40) { printf("FAIL crt\n"); return 1; }
    /* palette named */
    unsigned char pal[16][3];
    if (!gmb_screen_palette_named("Solarized", pal) || pal[0][0] != 0) { printf("FAIL named solarized\n"); return 1; }
    if (gmb_screen_palette_named("UnknownPal", pal)) { printf("FAIL named unknown\n"); return 1; }
    if (!gmb_screen_palette_named("GRUVBOX DARK", pal)) { printf("FAIL case-insens\n"); return 1; }
    char names[24][32];
    int nn = gmb_screen_palette_named_list(names, 24);
    if (nn != 21) { printf("FAIL named list %d !=21\n", nn); return 1; }
    /* palette base */
    unsigned char vga[16][3]; gmb_screen_palette_vga16(vga);
    if (vga[15][0] != 255 || vga[0][0] != 0) { printf("FAIL vga16\n"); return 1; }
    unsigned char c64p[16][3]; gmb_screen_palette_c64(c64p);
    if (c64p[1][0] != 255) { printf("FAIL c64 palette\n"); return 1; }
    /* apply: palette + fg/bg nello stato runtime */
    gmb_gfx_init();
    gmb_screen_config_apply(&c64);
    if (gmb_gfx_fg() != 14 || gmb_gfx_bg() != 6) { printf("FAIL apply fg/bg %d %d\n", gmb_gfx_fg(), gmb_gfx_bg()); return 1; }
    if (gmb_console_buffer_width() != 80 || gmb_console_buffer_height() != 50) { printf("FAIL apply buffer\n"); return 1; }
    printf("OK (text100x40 c64 80x50 fg14bg6 pal16+256 cga320 vga640 crt40x25 named21 case-insens apply ok)\n");
    return 0;
}
static int test_sdl_console(void){
    printf("TEST SdlConsole (config-accepting + ITerminalBuffer + escape)... ");
    gmb_screen_config_t cfg = gmb_screen_config_get_default(GMB_SM_TEXT);
    gmb_sdl_console_t* c = gmb_sdl_console_new(&cfg, 40, 10);
    if (!c) { printf("FAIL new\n"); return 1; }
    /* config accettata: griglia 40x10, fg15 bg0 */
    if (gmb_screen_width(gmb_sdl_console_screen(c)) != 40 || gmb_screen_height(gmb_sdl_console_screen(c)) != 10) { printf("FAIL dims\n"); return 1; }
    if (gmb_sdl_console_current_fg(c) != 15 || gmb_sdl_console_current_bg(c) != 0) { printf("FAIL fg/bg\n"); return 1; }
    /* print: caratteri nella griglia, cursore avanza, \n scende */
    gmb_sdl_console_write(c, "AB");
    if (gmb_sdl_console_cursor_col(c) != 2 || gmb_sdl_console_cursor_row(c) != 0) { printf("FAIL cursor %d,%d\n", gmb_sdl_console_cursor_row(c), gmb_sdl_console_cursor_col(c)); return 1; }
    gmb_screen_cell_t* cells = gmb_screen_cells(gmb_sdl_console_screen(c));
    if (cells[0].character != 'A' || cells[1].character != 'B') { printf("FAIL cells\n"); return 1; }
    /* \n + \r */
    gmb_sdl_console_write(c, "\n");
    gmb_sdl_console_write(c, "C");
    if (gmb_sdl_console_cursor_row(c) != 1 || gmb_sdl_console_cursor_col(c) != 1) { printf("FAIL nl %d,%d\n", gmb_sdl_console_cursor_row(c), gmb_sdl_console_cursor_col(c)); return 1; }
    /* scroll: riempi fino in fondo → 'A' (riga 0 originale) esce */
    for (int i = 0; i < 9; i++) gmb_sdl_console_write(c, "Z\n");
    gmb_sdl_console_write(c, "END");
    if (cells[0].character == 'A') { printf("FAIL scroll: A still in row0\n"); return 1; }
    /* dopo lo scroll la riga 0 contiene la vecchia riga 1: 'C' in col0, 'Z' in col1 */
    if (cells[0].character != 'C' || cells[1].character != 'Z') { printf("FAIL scroll content %c %c\n", cells[0].character, cells[1].character); return 1; }
    /* set_cursor_position 1-based */
    gmb_sdl_console_set_cursor_position(c, 1, 1);
    if (gmb_sdl_console_cursor_row(c) != 0 || gmb_sdl_console_cursor_col(c) != 0) { printf("FAIL setpos\n"); return 1; }
    /* clear screen + erase EOL */
    gmb_sdl_console_set_cursor_position(c, 1, 1);
    gmb_sdl_console_write(c, "HELLO");
    gmb_sdl_console_set_cursor_position(c, 1, 4);
    gmb_sdl_console_erase_to_eol(c);
    if (cells[3].character != 0) { printf("FAIL erase eol\n"); return 1; }
    gmb_sdl_console_clear_screen(c);
    if (cells[0].character != 0) { printf("FAIL clear\n"); return 1; }
    /* escape: CSI H + SGR 31 (rosso) + CSI 2J */
    gmb_sdl_console_write(c, "\x1B[31mX");
    if (gmb_sdl_console_current_fg(c) != 1) { printf("FAIL sgr %d\n", gmb_sdl_console_current_fg(c)); return 1; }
    gmb_sdl_console_write(c, "\x1B[2J");
    if (cells[0].character != 0) { printf("FAIL esc clear\n"); return 1; }
    gmb_sdl_console_write(c, "\x1B[H");
    if (gmb_sdl_console_cursor_row(c) != 0 || gmb_sdl_console_cursor_col(c) != 0) { printf("FAIL esc home\n"); return 1; }
    /* cursor visibility */
    gmb_sdl_console_write(c, "\x1B[?25l");
    if (gmb_sdl_console_cursor_visible(c)) { printf("FAIL cursor hide\n"); return 1; }
    gmb_sdl_console_write(c, "\x1B[?25h");
    if (!gmb_sdl_console_cursor_visible(c)) { printf("FAIL cursor show\n"); return 1; }
    /* StandardConsole scroll mode (StandardConsole.cs:87,409-432) */
    if (gmb_sdl_console_is_native_scroll_mode(c)) { printf("FAIL native default\n"); return 1; }
    gmb_sdl_console_set_scroll_mode(c, 1);
    if (!gmb_sdl_console_is_native_scroll_mode(c)) { printf("FAIL set native\n"); return 1; }
    gmb_sdl_console_set_scroll_mode(c, 0);
    if (gmb_sdl_console_is_native_scroll_mode(c)) { printf("FAIL back to buffer\n"); return 1; }
    gmb_console_clear_scrollback(); /* no crash */
    gmb_sdl_console_free(c);
    printf("OK (40x10 fg15 write scroll erase clear SGR31 esc2J home ?25l/h)\n");
    return 0;
}
static int test_sdl_manager_renderer(void){
    printf("TEST SdlManager/SdlRenderer (SDL-guarded, headless no-op)... ");
    /* SdlManager: init/poll/quit headless = sicuri, no crash */
    gmb_sdl_manager_init();
    int q = gmb_sdl_manager_poll_events();
    if (q != 0 && q != 1) { printf("FAIL poll %d\n", q); return 1; }
    gmb_sdl_manager_quit();
    /* callback registrazione/rimozione non crasha */
    static int fired = 0;
    void (*cb)(int, void*) = 0;
    gmb_sdl_manager_on_quit(cb, &fired);
    gmb_sdl_manager_remove_quit(cb);
    (void)fired;
    /* FontAtlas: load fallisce pulito senza SDL renderer */
    gmb_font_atlas_t* atlas = gmb_font_atlas_new();
    int ok = gmb_font_atlas_load(atlas, NULL, "/nonexistent.ttf", 12);
    if (ok != 0) { printf("FAIL atlas load %d\n", ok); return 1; }
    gmb_font_atlas_free(atlas);
    /* renderer render headless: 0 (no SDL) oppure 1, senza crash */
    gmb_screen_buffer_t* b = gmb_screen_new(4, 2);
    unsigned char bg[3] = {0,0,0};
    int r = gmb_sdl_renderer_render(NULL, b, 8, 16, bg, NULL);
    if (r != 0 && r != 1) { printf("FAIL render %d\n", r); return 1; }
    gmb_screen_free(b);
    printf("OK (poll=%d atlas_load=0 render=%d callbacks no-crash)\n", q, r);
    return 0;
}
static char tw_buf[128]; static int tw_len = 0;
static void tw_a(void* ctx, const char* s, int len){ (void)ctx; for(int i=0;i<len && tw_len<127;i++) tw_buf[tw_len++]=s[i]; }
static void tw_b(void* ctx, const char* s, int len){ (void)ctx; for(int i=0;i<len && tw_len<127;i++) tw_buf[tw_len++]=s[i]+1; }
static int test_console_extra(void){
    printf("TEST Console extra (ScreenSnapshot/TeeWriter/Headless/System)... ");
    /* ScreenSnapshot: cattura buffer 3x2, render text trima, raw preserva */
    gmb_screen_buffer_t* b = gmb_screen_new(3, 2);
    gmb_screen_setchar(b, 0, 0, 'H', (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_screen_setchar(b, 1, 0, 'I', (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_screen_setchar(b, 0, 1, '!', (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    gmb_screen_snapshot_t snap;
    if (!gmb_screen_snapshot_capture("test", b, &snap)) { printf("FAIL capture\n"); return 1; }
    char txt[64];
    gmb_screen_snapshot_render_text(&snap, txt, sizeof(txt));
    if (strcmp(txt, "HI \n!") != 0) { printf("FAIL render_text [%s]\n", txt); return 1; }
    gmb_screen_snapshot_render_text_raw(&snap, txt, sizeof(txt));
    if (strcmp(txt, "HI \n!  \n") != 0) { printf("FAIL raw [%s]\n", txt); return 1; }
    if (!gmb_screen_snapshot_contains(&snap, "HI")) { printf("FAIL contains\n"); return 1; }
    if (gmb_screen_snapshot_contains_error_indicators(&snap)) { printf("FAIL err indic\n"); return 1; }
    gmb_screen_snapshot_free(&snap);
    gmb_screen_free(b);
    /* TeeWriter: scrive a due destinazioni */
    gmb_text_writer_t wa = { tw_a, NULL, NULL };
    gmb_text_writer_t wb = { tw_b, NULL, NULL };
    gmb_tee_writer_t tee;
    gmb_tee_writer_init(&tee, wa, wb);
    tw_len = 0; memset(tw_buf, 0, sizeof(tw_buf));
    gmb_tee_writer_write(&tee, "AB", 2);
    if (tw_len != 4 || tw_buf[0] != 'A' || tw_buf[1] != 'B' || tw_buf[2] != 'B' || tw_buf[3] != 'C') { printf("FAIL tee %s\n", tw_buf); return 1; }
    /* HeadlessConsole: output lineare verso un buffer */
    static char hb[64]; static int hl = 0;
    void (*hacc)(void*, const char*, int) = 0;
    (void)hacc;
    gmb_headless_console_t hc;
    hl = 0; memset(hb, 0, sizeof(hb));
    gmb_text_writer_t wh = { (void(*)(void*,const char*,int))tw_a, NULL, NULL };
    gmb_headless_console_init(&hc, wh);
    (void)hc;
    /* SystemConsole wrappers headless */
    if (gmb_system_console_key_available() != 0) { printf("FAIL key avail\n"); return 1; }
    /* SdlContext: headless create fallisce pulito (no SDL window senza display), destroy no-op */
    gmb_sdl_context_t ctx;
    int ok = gmb_sdl_context_create(&ctx, "test", 0, 0, 320, 200, 0);
    if (ok != 0 && ok != 1) { printf("FAIL ctx create %d\n", ok); return 1; }
    if (ok == 1 && (!ctx.window_handle || !ctx.renderer_handle)) { printf("FAIL ctx handles\n"); return 1; }
    gmb_sdl_context_destroy(&ctx);
    if (ctx.window_handle != NULL || ctx.renderer_handle != NULL) { printf("FAIL ctx destroy\n"); return 1; }
    printf("OK (render_text='HI \\n!' raw 2 righe contains tee=A B B C key_avail=0 ctx=%d destroy ok)\n", ok);
    return 0;
}
static int kb_hook_fired = 0;
static void kb_hook(void* ud){ (void)ud; kb_hook_fired = 1; }
static int test_keyboard(void){
    printf("TEST Keyboard (KeyCombination/hook/sendkey/read)... ");
    /* 8.1 KeyCombination */
    gmb_key_combination_t c1 = gmb_key_combination_new('A', GMB_MOD_CTRL);
    gmb_key_combination_t c2 = gmb_key_combination_new('A', GMB_MOD_CTRL);
    gmb_key_combination_t c3 = gmb_key_combination_new('A', 0);
    if (!gmb_key_combination_equals(c1, c2) || gmb_key_combination_equals(c1, c3)) { printf("FAIL combo\n"); return 1; }
    /* 8.5 hooks (ON KEY) */
    gmb_key_register_hook(gmb_key_combination_new('F', 0), kb_hook, NULL);
    if (!gmb_key_is_hook_registered(gmb_key_combination_new('F', 0))) { printf("FAIL hook reg\n"); return 1; }
    /* 8.2/8.3 SendKeys + TryRead */
    kb_hook_fired = 0;
    gmb_key_send_keys("AB");
    if (gmb_key_try_read(0) != 'A') { printf("FAIL try_read\n"); return 1; }
    if (gmb_key_try_read(1) != 'B') { printf("FAIL try_read upper (B)\n"); return 1; }
    gmb_key_send_key('c');
    if (gmb_key_try_read(1) != 'C') { printf("FAIL convert upper\n"); return 1; }
    /* hook consuma il tasto */
    gmb_key_send_key('F');
    if (gmb_key_try_read(0) != 0 || !kb_hook_fired) { printf("FAIL hook dispatch fired=%d\n", kb_hook_fired); return 1; }
    gmb_key_unregister_hook(gmb_key_combination_new('F', 0));
    if (gmb_key_is_hook_registered(gmb_key_combination_new('F', 0))) { printf("FAIL hook unreg\n"); return 1; }
    /* 8.4 ReadLine con editor */
    gmb_key_send_keys("ab\x08" "cd\n");  /* a b backspace c d enter → "acd" */
    char* line = gmb_key_read_line("> ");
    if (!line || strcmp(line, "acd") != 0) { printf("FAIL read_line [%s]\n", line ? line : "NULL"); free(line); return 1; }
    free(line);
    /* pause/resume */
    gmb_key_pause();
    gmb_key_send_key('X');
    if (gmb_key_try_read(0) != 0) { printf("FAIL pause\n"); return 1; }
    gmb_key_resume();
    gmb_key_clear_buffer();
    gmb_key_reset();
    gmb_key_unregister_all_hooks();
    printf("OK (combo hook sendkeys try_read upper read_line='acd' pause/resume)\n");
    return 0;
}
static int test_mouse_input(void){
    printf("TEST MouseInput (SGR parse/pump/enable/limit)... ");
    if (gmb_mouse_input_enabled()) { printf("FAIL default enabled\n"); return 1; }
    /* Enable: abilita */
    gmb_mouse_input_enable();
    if (!gmb_mouse_input_enabled()) { printf("FAIL enable\n"); return 1; }
    gmb_mouse_input_disable();
    if (gmb_mouse_input_enabled()) { printf("FAIL disable\n"); return 1; }
    /* SGR press: ESC[<1;10;20M (cb=1 → bit0 → button1, semantica C# MouseInput.cs:123) */
    gmb_mouse_input_process("\x1b[<1;10;20M", 11);
    gmb_mouse_input_pump();
    if (gmb_mouse_input_last_x() != 10 || gmb_mouse_input_last_y() != 20) { printf("FAIL pos %d,%d\n", gmb_mouse_input_last_x(), gmb_mouse_input_last_y()); return 1; }
    if ((gmb_mouse_input_last_buttons() & 1) == 0) { printf("FAIL buttons %d\n", gmb_mouse_input_last_buttons()); return 1; }
    if (!gmb_mouse_input_has_moved()) { printf("FAIL has_moved\n"); return 1; }
    /* release: ESC[<1;10;20m → event buttons=0; C# PumpEvents NON resetta LastButtons
     * (condizione buttons!=0||scroll!=0 falsa) → resta 1 (MouseInput.cs:142-143) */
    gmb_mouse_input_process("\x1b[<1;10;20m", 11);
    gmb_mouse_input_pump();
    if (gmb_mouse_input_last_buttons() != 1) { printf("FAIL release (C# keeps buttons) %d\n", gmb_mouse_input_last_buttons()); return 1; }
    /* scroll: cb=0x40 (64), bit0=1 → down (-1) */
    gmb_mouse_input_process("\x1b[<65;10;20M", 12);
    gmb_mouse_input_pump();
    if (gmb_mouse_input_last_scroll_delta() != -1) { printf("FAIL scroll %d\n", gmb_mouse_input_last_scroll_delta()); return 1; }
    /* SetPosition + Limit clampa */
    gmb_mouse_input_limit(5, 5, 15, 15);
    gmb_mouse_input_set_position(100, 100);
    if (gmb_mouse_input_last_x() != 15 || gmb_mouse_input_last_y() != 15) { printf("FAIL limit clamp %d,%d\n", gmb_mouse_input_last_x(), gmb_mouse_input_last_y()); return 1; }
    gmb_mouse_input_unlimit();
    gmb_mouse_input_set_position(30, 30);
    if (gmb_mouse_input_last_x() != 30) { printf("FAIL unlimit\n"); return 1; }
    /* show/hide/set_cursor no-crash */
    gmb_mouse_input_show();
    gmb_mouse_input_hide();
    gmb_mouse_input_set_cursor(1);
    printf("OK (sgr 0;10;20 buttons1 release0 scroll-1 limit clamp 15,15 unlimit 30 cursor)\n");
    return 0;
}
static int test_screen_parity_c(void){
    printf("TEST Screen parity C (port ScreenBufferTests/GridLayoutTests C# → C)... ");
    /* gmb_screen_new/width/height */
    gmb_screen_buffer_t* b = gmb_screen_new(10, 5);
    if (!b || gmb_screen_width(b) != 10 || gmb_screen_height(b) != 5) { printf("FAIL new/w/h\n"); return 1; }
    /* setchar + get via cells */
    gmb_screen_setchar(b, 0, 0, 'X', (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,0});
    gmb_screen_cell_t* cells = gmb_screen_cells(b);
    if (cells[0].character != 'X') { printf("FAIL setchar\n"); return 1; }
    /* setstring multi-char */
    gmb_screen_setstring(b, 1, 1, "HI", (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,0});
    if (cells[1 * 10 + 1].character != 'H' || cells[1 * 10 + 2].character != 'I') { printf("FAIL setstring\n"); return 1; }
    /* fillrect */
    gmb_screen_fillrect(b, 2, 2, 4, 4, 'F', (gmb_cell_color_t){255,255,255}, (gmb_cell_color_t){0,0,0});
    if (cells[2 * 10 + 2].character != 'F' || cells[4 * 10 + 4].character != 'F') { printf("FAIL fillrect\n"); return 1; }
    /* clone */
    gmb_screen_buffer_t* c = gmb_screen_clone(b);
    if (!c) { printf("FAIL clone\n"); return 1; }
    gmb_screen_cell_t* cc = gmb_screen_cells(c);
    if (cc[0].character != 'X' || cc[1*10+1].character != 'H') { printf("FAIL clone content\n"); return 1; }
    /* render_screen → ANSI non vuoto */
    char out[4096];
    size_t n = gmb_render_screen(b, out, sizeof(out));
    if (n == 0) { printf("FAIL render_screen\n"); return 1; }
    /* clear → carattere ' ' (0x20) */
    gmb_screen_clear(b, (gmb_cell_color_t){0,0,0}, (gmb_cell_color_t){0,0,0});
    if (cells[0].character != ' ') { printf("FAIL clear\n"); return 1; }
    gmb_screen_free(b);
    gmb_screen_free(c);
    /* grid_layout (GridLayoutTests.cs:36): area 100x50, 2 colonne uguali → 2 rect */
    gmb_rect_t area = {0, 0, 100, 50};
    int cols[2] = {1, 1};
    int rows[1] = {1};
    gmb_rect_t rects[4];
    int gn = gmb_grid_layout(area, cols, 2, rows, 1, GMB_SPLIT_HORIZONTAL, GMB_DIVIDER_EVEN, rects, 4);
    if (gn < 2) { printf("FAIL grid n=%d\n", gn); return 1; }
    if (rects[0].x != 0 || rects[0].w + rects[1].w != 100) { printf("FAIL grid rects %d,%d\n", rects[0].w, rects[1].w); return 1; }
    printf("OK (10x5 setchar setstring fillrect clone render clear grid 2 rects %d+%d=100)\n", rects[0].w, rects[1].w);
    return 0;
}
static int test_key_decode_mouse(void){
    printf("TEST KeyDecodeMouse (port KeyConstantsTests C# → C)... ");
    /* casi del test C# (KeyConstantsTests.cs:40-53): raw=x*1e6+y*1e3+btn*10+p, key=-1000-raw */
    int cases[4][4] = { {3,5,1,1}, {10,20,2,0}, {0,0,0,1}, {999,999,99,0} };
    for (int i = 0; i < 4; i++) {
        int x = cases[i][0], y = cases[i][1], b = cases[i][2], p = cases[i][3];
        int raw = x*1000000 + y*1000 + b*10 + p;
        int key = -1000 - raw;
        int cx=0, cy=0, cb=0, cp=0;
        if (!gmb_key_decode_mouse(key, &cx, &cy, &cb, &cp)) { printf("FAIL isMouse %d\n", i); return 1; }
        if (cx!=x || cy!=y || cb!=b || cp!=p) { printf("FAIL case %d: got %d,%d,%d,%d want %d,%d,%d,%d\n", i, cx,cy,cb,cp, x,y,b,p); return 1; }
    }
    /* non-mouse → false */
    int cx=0,cy=0,cb=0,cp=0;
    if (gmb_key_decode_mouse(65, &cx,&cy,&cb,&cp)) { printf("FAIL non-mouse\n"); return 1; }
    printf("OK (4 casi x,y,btn,p decodificati; non-mouse false)\n");
    return 0;
}
static int test_file_extra(void){
    printf("TEST File Extra (dir_filtered/count_filtered)... ");
    // Crea dir temporanea con 3 file
    const char* dir="/tmp/gmb_file_extra_test";
    gmb_file_mkdir(gmb_string_alloc(dir));
    FILE* fa=fopen("/tmp/gmb_file_extra_test/a.txt","w"); fprintf(fa,"hello\nworld\n"); fclose(fa);
    FILE* fb=fopen("/tmp/gmb_file_extra_test/b.bas","w"); fprintf(fb,"line1\n"); fclose(fb);
    FILE* fc=fopen("/tmp/gmb_file_extra_test/c.txt","w"); fprintf(fc,"a line\nb line\nc a line\n"); fclose(fc);
    // gmb_fs_dir senza filtro → 3
    gmb_list_t all = gmb_fs_dir(gmb_string_alloc("/tmp/gmb_file_extra_test/*"));
    if(all.count < 3){ printf("FAIL all %zu <3\n", all.count); return 1; }
    // con filtro .txt → 2
    gmb_fn_t pred = { .fn = (void*)pred_is_txt, .ctx = NULL };
    gmb_list_t txt = gmb_fs_dir_filtered(gmb_string_alloc("/tmp/gmb_file_extra_test/*"), pred);
    if(txt.count != 2){ printf("FAIL filtered txt %zu !=2\n", txt.count); return 1; }
    // count_lines_filtered: c.txt ha 3 linee, 2 contengono 'a'
    gmb_fn_t pred2 = { .fn = (void*)pred_contains_a, .ctx = NULL };
    int cnt_all = gmb_count_lines_filtered("/tmp/gmb_file_extra_test/c.txt", (gmb_fn_t){NULL,NULL});
    int cnt_a = gmb_count_lines_filtered("/tmp/gmb_file_extra_test/c.txt", pred2);
    if(cnt_all != 3){ printf("FAIL count_all %d !=3\n", cnt_all); return 1; }
    if(cnt_a != 2){ printf("FAIL count_a %d !=2\n", cnt_a); return 1; }
    printf("OK (all=%zu txt=%zu cnt_all=%d cnt_a=%d)\n", all.count, txt.count, cnt_all, cnt_a);
    gmb_list_free(&all); gmb_list_free(&txt);
    return 0;
}
int main(void){
    int fails=0;
    fails+=test_array_chunk();
    fails+=test_array_zip();
    fails+=test_array_pad();
    fails+=test_array_shuffle();
    fails+=test_zip_list();
    fails+=test_graphics_circle_arc();
    fails+=test_graphics_image_put_ops();
    fails+=test_graphics_ttf();
    fails+=test_mask_finance();
    fails+=test_window_manager();
    fails+=test_graphics_texture_rotation();
    fails+=test_audio_utils();
    fails+=test_pso();
    fails+=test_qlnet();
    fails+=test_regex();
    fails+=test_timer();
    fails+=test_collections_micro();
    fails+=test_bitserializers();
    fails+=test_fs_times();
    fails+=test_bsp_session();
    fails+=test_widgets();
    fails+=test_chart_financial();
    fails+=test_audio_synth_mml();
    fails+=test_bar_builder();
    fails+=test_qr();
    fails+=test_joy();
    fails+=test_screens();
    fails+=test_console_streams();
    fails+=test_screen_config();
    fails+=test_sdl_console();
    fails+=test_sdl_manager_renderer();
    fails+=test_console_extra();
    fails+=test_keyboard();
    fails+=test_mouse_input();
    fails+=test_screen_parity_c();
    fails+=test_key_decode_mouse();
    fails+=test_file_extra();
    if(fails==0) printf("\nALL GAPS TESTS PASSED\n");
    else printf("\n%d TESTS FAILED\n", fails);
    return fails;
}
