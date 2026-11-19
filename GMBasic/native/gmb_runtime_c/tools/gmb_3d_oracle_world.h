/* gmb_3d_oracle_world.h — oracolo P2: scene a settori e prove sul LOAD.
 *
 * Tenuto separato da gmb_3d_oracle.c per fase: le scene P1 (triangoli) stanno
 * là, qui settori e mondo caricato. Un solo binario, un solo gate.
 */
#ifndef GMB_3D_ORACLE_WORLD_H
#define GMB_3D_ORACLE_WORLD_H

#include "gmb_3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scene a settori: rendono nel target (0 = ok). */
int w_scene_room(gmb_3d_target_t* t);
int w_scene_door(gmb_3d_target_t* t);
int w_scene_pitch(gmb_3d_target_t* t);

/* Scrive in `dir` i documenti di prova del load (buono e invalido). */
void w_prepare(const char* dir);

/* R11 dentro un task GPM: il load parca e resta atomico. */
void w_world_checks(int* ok, int* fail);

/* R11 fuori da un task: load sincrono, nessun park. */
void w_world_t3_checks(int* ok, int* fail);

/* W-MOVE: 10 casi headless (muro frontale/obliquo/angolo, gradini, portali,
 * W-EYE-CLAMP) + vista del giocatore dopo il movimento. Usa i documenti di
 * w_prepare: va chiamata dopo. */
void w_collision_checks(int* ok, int* fail);

/* Mappa demo (--demo): 3 stanze, porta, ascensore, finestra traslucida.
 * La scena la carica e la anima; il bench ne misura il frame time. */
void w_set_demo_path(const char* path);
int  w_scene_demo(gmb_3d_target_t* t);
int  w_bench_demo(int w_px, int h_px, int frames, double limit_ms);

/* T-*: orologio a passo fisso, SECMOVE, ANIM, SCROLL, LIGHTFX, LAMP, LIGHTNOW.
 * Usa la mappa demo (va impostata con w_set_demo_path). */
void w_time_checks(int* ok, int* fail);

#ifdef __cplusplus
}
#endif

#endif /* GMB_3D_ORACLE_WORLD_H */
