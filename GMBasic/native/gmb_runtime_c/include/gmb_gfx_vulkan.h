/* ============================================================================
 * gmb_gfx_vulkan.h — backend Vulkan opzionale (present path) del motore C.
 *
 * Port in C di CommonGraphics/Vulkan (C#), reso FUNZIONANTE: presenta il
 * framebuffer software (gmb_gfx_visible_pixels) su una swapchain Vulkan via
 * MoltenVK, con gli shader sprite.vert/frag (SPIR-V precompilato embedded).
 *
 * Compilato solo se il toolchain Vulkan è disponibile (CMake GMB_GFX_VULKAN):
 * in quel caso gmb_gfx_present_sdl.c seleziona il backend con
 * GMB_GFX_BACKEND=vulkan|sdl|auto (default sdl) e fa fallback a SDL se
 * l'inizializzazione Vulkan fallisce.
 * ============================================================================ */
#ifndef GMB_GFX_VULKAN_H
#define GMB_GFX_VULKAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 se il backend è compilato (la disponibilità reale si verifica a start). */
int gmb_gfx_vk_available(void);

/* Crea istanza/device/swapchain/pipeline per la finestra SDL data e la
 * risoluzione iniziale. Ritorna 0 su successo, -1 su errore (con messaggio su
 * stderr). Idempotente: se già attivo, ritorna 0. */
int gmb_gfx_vk_start(void* sdl_window, int width, int height);

/* Presenta il framebuffer (pixel 0xAARRGGBB, riga-major) alla finestra.
 * Gestisce resize e swapchain out-of-date. No-op se non attivo. */
void gmb_gfx_vk_present(const unsigned int* pixels, int width, int height);

/* Distrugge tutto (idempotente). */
void gmb_gfx_vk_stop(void);

/* 1 se il backend è attivo (start riuscito e non stoppato). */
int gmb_gfx_vk_active(void);

/* Nome del device Vulkan (per log/diagnostica). */
const char* gmb_gfx_vk_device_name(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GFX_VULKAN_H */
