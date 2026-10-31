/* gmb_cua_runtime.h — Wrapper BASIC-value delle azioni CUA per i backend C (C99/C99Native).
 * Ritornano gmb_string_t/gmb_double_t (i tipi del runtime GMBasic) così il codice C
 * emesso può chiamarli direttamente come qualsiasi builtin gmb_*. Tutto in C (§0bis).
 * L'implementazione delega ai moduli CUA (tool_registry/computer_provider/ax/cdp). */
#ifndef GMB_CUA_RUNTIME_H
#define GMB_CUA_RUNTIME_H
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TOOL */
gmb_string_t gmb_cua_tool_search(const char* query);
int gmb_cua_tool_register(const char* name, const char* desc, const char* provider);
gmb_string_t gmb_llm_cache_stats(const char* name);   /* AI.CACHESTATS(name) */

/* COMPUTER (L0) */
gmb_string_t gmb_cua_computer_screenshot(const char* path);   /* JSON {"ok",path} */
gmb_string_t gmb_cua_computer_windows(void);                  /* JSON array */
gmb_double_t gmb_cua_computer_click(double x, double y);      /* 0 ok / -1 */
gmb_double_t gmb_cua_computer_move(double x, double y);
gmb_double_t gmb_cua_computer_type(const char* text);
gmb_double_t gmb_cua_computer_key(const char* keys);
gmb_double_t gmb_cua_computer_double_click(double x, double y); /* CUA #7 ASTRA */
gmb_double_t gmb_cua_computer_hotkey(const char* combo);        /* "ctrl+alt+k" → 0 ok / -1 */
/* Azioni AI OBSERVE (todo AI-Runtime P1): id osservazione (>0) o -1. Il registry dei
 * ContextObject vive nel runtime C condiviso (gmb_observation.c); SCREEN riusa lo screenshot
 * del provider e registra il PNG. */
gmb_double_t gmb_cua_observe_file(const char* path);
gmb_double_t gmb_cua_observe_log(const char* path, double tail_lines);
gmb_double_t gmb_cua_observe_screen(const char* out_path);
gmb_string_t gmb_cua_computer_screenrecord(const char* path, double fps);       /* JSON {"ok",path} */
gmb_string_t gmb_cua_computer_screenrecord_start(const char* path, double fps); /* JSON {"ok",path} */
gmb_string_t gmb_cua_computer_screenrecord_stop(void);                          /* JSON {path,durationMs,frames,size} */
gmb_string_t gmb_cua_computer_screenrecord_status(void);                        /* JSON stato */

/* UI (L1 AX) */
gmb_string_t gmb_cua_ui_find(const char* target);             /* JSON */
gmb_string_t gmb_cua_ui_click(const char* target);            /* JSON ok */
gmb_double_t gmb_cua_ui_click_ok(const char* target);         /* 1 se ok, 0 altrimenti */
gmb_string_t gmb_cua_ui_type(const char* target, const char* text);
gmb_double_t gmb_cua_ui_type_ok(const char* target, const char* text); /* 1 se ok, 0 altrimenti (parita' Bit C#/C99) */
gmb_string_t gmb_cua_ui_findall_arr(const char* target);   /* JSON-array: "[]" o [envelope] (parita' Array C#) */

/* BROWSER */
gmb_string_t gmb_cua_browser_navigate(const char* url);       /* "navigated"|json err */
gmb_string_t gmb_cua_browser_snapshot(void);
gmb_string_t gmb_cua_browser_evaluate(const char* expr);      /* valore */
gmb_string_t gmb_cua_browser_click(const char* selector);
gmb_string_t gmb_cua_browser_fill(const char* selector, const char* value);
gmb_string_t gmb_cua_browser_start(const char* provider, int port, const char* profile);
/* BROWSER.* headless (2026-09-16): OBSERVE/FIND/NETLOG/CONSOLE riportano l'errore di
 * connessione CDP esplicito (il client vive solo nel mondo C#); AUTH("list") e'
 * filesystem puro. Parita' con BrowserRuntime/AuthProfileStore C# senza browser. */
gmb_string_t gmb_cua_browser_observe(const char* preset);
gmb_string_t gmb_cua_browser_find(const char* text, int regex);
gmb_string_t gmb_cua_browser_netlog(int clear);
gmb_string_t gmb_cua_browser_console(int clear);
gmb_string_t gmb_cua_browser_auth(const char* op, const char* name);   /* verifica/attiva CDP, "connected" + setenv profile per launch_chromium.sh */
const char* gmb_cua_browser_engine(int engine);                                            /* %AUTO/CHROMIUM/OBSCURA -> "auto"/"chromium"/"obscura" */
gmb_string_t gmb_cua_browser_access_tree(void); /* snapshot accessibility compatto @eN (CDP getFullAXTree) */
gmb_string_t gmb_cua_browser_screenshot(const char* path); /* Page.captureScreenshot → PNG file, ritorna path o base64 */
gmb_string_t gmb_cua_browser_get_cookies(void);
gmb_string_t gmb_cua_browser_set_cookie(const char* name, const char* value, const char* url);
gmb_string_t gmb_cua_browser_clear_cookies(void);
gmb_string_t gmb_cua_browser_set_header(const char* name, const char* value);
gmb_string_t gmb_cua_browser_set_user_agent(const char* ua);
gmb_string_t gmb_cua_browser_human_click(const char* selector);
gmb_string_t gmb_cua_browser_human_fill(const char* selector, const char* value);
gmb_string_t gmb_cua_browser_human_scroll(int dy);

/* AGENT.RUN — loop Brain→Azione (log string; errore esplicito se nessun brain) */
gmb_string_t gmb_cua_agent_run(const char* goal, int max_steps);

#ifdef __cplusplus
}
#endif

#endif
