/*
 * gor_debug.h — flag di debug di gmbortools (NIENTE env: si impostano via API).
 * Sostituiscono GMB_DBG / GMB_DBG_NT / GMB_NO_NT.
 */
#ifndef GMBORTOOLS_GOR_DEBUG_H
#define GMBORTOOLS_GOR_DEBUG_H
#ifdef __cplusplus
extern "C" {
#endif
extern int gor_debug;      /* trace generiche (ex GMB_DBG) */
extern int gor_debug_nt;   /* trace NT scaling (ex GMB_DBG_NT) */
extern int gor_no_nt;      /* disabilita NT (ex GMB_NO_NT) */
void gor_set_debug(int lvl);
void gor_set_debug_nt(int on);
void gor_set_no_nt(int on);
#ifdef __cplusplus
}
#endif
#endif
