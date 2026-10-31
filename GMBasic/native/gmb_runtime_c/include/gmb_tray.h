/* gmb_tray.h — icona nella barra del SO (menu bar macOS / system tray Windows / SNI Linux).
 *
 * Libreria del runtime C condiviso: la usano TUTTI i backend (IL/PCodeVM via P/Invoke,
 * VMC/C99/C99Native direttamente). Implementazione per OS:
 *   macOS   -> src/gmb_tray_macos.m  (NSStatusItem)
 *   Windows -> src/gmb_tray_win.c    (Shell_NotifyIcon)
 *   Linux   -> src/gmb_tray_linux.c  (StatusNotifierItem D-Bus; no-op finché non implementato)
 *   altro   -> no-op (gmb_tray_available()==0)
 *
 * Contratto thread: create/item/tooltip/remove e poll vanno chiamati sul MAIN THREAD.
 * poll() pompa gli eventi UI pendenti (per daemon headless); su macOS va richiamato
 * periodicamente dal loop dell'host (nessun run loop proprio).
 * Gli handler BASIC collegati al callback devono essere dispatchati come task GPM.
 */
#ifndef GMB_TRAY_H
#define GMB_TRAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Callback: item_id = id passato a gmb_tray_item(); user = contesto del chiamante. */
typedef void (*gmb_tray_cb)(int item_id, void *user);

/* 1 se esiste un bridge per questo OS e il desktop è disponibile (headless Linux -> 0). */
int  gmb_tray_available(void);
/* Nome del backend attivo: "macos" | "win" | "linux" | "none". */
const char *gmb_tray_backend(void);

/* Crea l'icona (icon_path può essere NULL -> usa il titolo/testo). 0 = ok. */
int  gmb_tray_create(const char *icon_path, const char *tooltip);
/* Aggiunge una voce di menu con id (ritornato al callback) e label. 0 = ok. */
int  gmb_tray_item(int id, const char *label);
/* Aggiunge un separatore. 0 = ok. */
int  gmb_tray_sep(void);
/* Aggiorna il tooltip. 0 = ok. */
int  gmb_tray_tooltip(const char *text);
/* Registra il callback degli item (dispatchare come task GPM lato BASIC). */
void gmb_tray_set_callback(gmb_tray_cb cb, void *user);
/* Pompa gli eventi UI pendenti sul main thread. Ritorna il numero di eventi processati. */
int  gmb_tray_poll(void);
/* Rimuove icona e menu. */
void gmb_tray_remove(void);
/* Ultimo item cliccato (read-and-clear, -1 se nessuno). Da leggere nel loop dopo poll(). */
int  gmb_tray_last(void);
/* Autotest (per i conformance test): 1 se il bridge risponde, 0 altrimenti. */
int  gmb_tray_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TRAY_H */
