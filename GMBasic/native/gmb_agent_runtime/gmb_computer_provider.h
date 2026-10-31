/* gmb_computer_provider.h — IComputerProvider cross-OS (CUA §5, §33)
 * ABI stabile per Universal Action Runtime. Ogni OS implementa la stessa
 * tabella di funzioni; il registry sceglie a runtime l'impl corretta.
 * Regola §0bis: runtime condiviso, mai logica inline nei backend. */
#ifndef GMB_COMPUTER_PROVIDER_H
#define GMB_COMPUTER_PROVIDER_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *name; // "mac" | "windows" | "linux" | "remote"
    int (*screenshot)(void *impl, const char *out_path); // PNG in out_path
    int (*click)(void *impl, double x, double y);
    int (*double_click)(void *impl, double x, double y);
    int (*move)(void *impl, double x, double y);
    int (*drag)(void *impl, double x1, double y1, double x2, double y2);
    int (*scroll)(void *impl, double x, double y, double dx, double dy);
    int (*type)(void *impl, const char *text);
    int (*key)(void *impl, const char *keys);
    int (*wait)(void *impl, uint64_t ms);
    int (*windows)(void *impl, char *out_json, size_t cap);
    int (*focus)(void *impl, const char *window_id);
    int (*clipboard_get)(void *impl, char *out, size_t cap);
    int (*clipboard_set)(void *impl, const char *text);
    int (*screenrecord_start)(void *impl, const char *out_path, int fps, const char *region); // 0 ok
    int (*screenrecord_stop)(void *impl, char *out_json, size_t cap); // JSON {path,durationMs,frames,size}
    int (*screenrecord_status)(void *impl, char *out_json, size_t cap);
    /* CUA #7 ASTRA (append-only in coda all'ABI): combo di modificatori "ctrl+alt+k". */
    int (*hotkey)(void *impl, const char *combo);
} gmb_computer_provider_vt;

typedef struct {
    gmb_computer_provider_vt *vt;
    void *impl;
    char name[16];
} gmb_computer_provider_t;

/* Registry — cross-OS */
gmb_computer_provider_t *gmb_computer_provider_resolve(const char *requested); // "auto"|"mac"|"windows"|"linux"|"remote"
const char *gmb_computer_provider_available(void); // JSON lista provider disponibili

/* Factory per OS — ogni file .c implementa uno */
gmb_computer_provider_t *gmb_computer_provider_mac_new(void);
gmb_computer_provider_t *gmb_computer_provider_windows_new(void);
gmb_computer_provider_t *gmb_computer_provider_linux_new(void);
gmb_computer_provider_t *gmb_computer_provider_remote_new(const char *url);

void gmb_computer_provider_free(gmb_computer_provider_t *p);

/* ---- export macOS (riusati da gmb_ax_mac e dal provider stesso) ---- */
#if defined(__APPLE__)
int gmb_mac_post_click(double x, double y);     /* CGEvent click (Accessibilità check) */
int gmb_mac_post_text(const char *text);        /* digita testo (Accessibilità check) */
int gmb_mac_clipboard_get(char *out, size_t cap);
int gmb_mac_clipboard_set(const char *text);
#endif

#endif
