/* gmb_ax.h — Accessibility (AX) macOS reale per UI.* semantico (CUA §7,§21).
 * Solo macOS (compilato quando __APPLE__). Le funzioni *json ritornano heap, free(). */
#ifndef GMB_AX_H
#define GMB_AX_H
#include <stddef.h>

#if defined(__APPLE__)
int   gmb_ax_available(void);                    /* AXIsProcessTrusted */
char *gmb_ax_find_json(const char *target);      /* {"found":bool,"role","title","level":"AX"} */
char *gmb_ax_findall_json(const char *target);   /* array JSON dei match */
char *gmb_ax_click_json(const char *target);     /* trova e preme/clicca davvero */
char *gmb_ax_type_json(const char *target, const char *text); /* focus + digita */
char *gmb_ax_tree_json(void);                    /* snapshot AX della app frontmost */
#endif

#endif
