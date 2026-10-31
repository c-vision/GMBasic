/* ============================================================================
 * gmb_tui_mouse.h — layer mouse (specchio ClipperLib/Clipper Mouse.cs).
 * MouseAction/MouseEvent/MouseDecoder. TTY-free: solo parsing di stringhe.
 * ============================================================================ */
#ifndef GMB_TUI_MOUSE_H
#define GMB_TUI_MOUSE_H

#include "gmb_tui.h"
#include "gmb_tui_clipper.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- KeyModifiers (specchio ConCodes.cs) ---- */
#define GMB_MODKEY_NONE    0u
#define GMB_MODKEY_SHIFT   1u
#define GMB_MODKEY_CONTROL 2u
#define GMB_MODKEY_ALT     4u
#define GMB_MODKEY_META    8u
#define GMB_MODKEY_SUPER   16u
#define GMB_MODKEY_HYPER   32u
typedef uint8_t gmb_key_modifiers_t;

/* ---- MouseButton (specchio ConCodes.cs) ---- */
enum { GMB_MB_LEFT = 0, GMB_MB_RIGHT, GMB_MB_MIDDLE, GMB_MB_ANY };

/* ---- ScrollDirection (specchio Mouse.cs) ---- */
enum { GMB_SCROLL_UP = 0, GMB_SCROLL_DOWN, GMB_SCROLL_LEFT, GMB_SCROLL_RIGHT, GMB_SCROLL_NONE = -1 };

/* ---- MouseActionKind (specchio Mouse.cs) ---- */
enum {
    GMB_MA_CLICK = 0, GMB_MA_DOUBLECLICK, GMB_MA_RIGHTCLICK, GMB_MA_MIDDLECLICK,
    GMB_MA_DRAG, GMB_MA_SCROLLUP, GMB_MA_SCROLLDOWN, GMB_MA_SCROLLLEFT,
    GMB_MA_SCROLLRIGHT, GMB_MA_RELEASE, GMB_MA_NONE, GMB_MA_MOVE, GMB_MA_TRIPLECLICK,
    GMB_MA_COUNT
};

#define GMB_MOUSE_BASE (-1000)

typedef struct gmb_mouse_action {
    int kind;
    int x, y;
    gmb_key_modifiers_t mods;
} gmb_mouse_action_t;

/* InkeyCode = K_MOUSE_BASE - kind */
int gmb_mouse_action_inkey(const gmb_mouse_action_t* a);
/* FromInkeyCode: legge coordinate/modificatori dallo stato Clipper (passato via st). */
int gmb_mouse_action_from_inkey(int inkey, gmb_clipper_state_t* st, gmb_mouse_action_t* out);
/* variante senza stato: coordinate 0. */
int gmb_mouse_action_from_inkey_stub(int inkey, gmb_mouse_action_t* out);

typedef struct gmb_mouse_event {
    int button;            /* GMB_MB_* */
    int x, y;
    gmb_key_modifiers_t mods;
    bool released;
    bool drag;
    int scroll;            /* GMB_SCROLL_* o GMB_SCROLL_NONE */
} gmb_mouse_event_t;

/* ---- MouseDecoder (specchio Mouse.cs) ---- */
int gmb_mouse_decode_sequence(const char* seq, gmb_mouse_event_t* out);   /* 1 ok, 0 non mouse */
int gmb_mouse_from_encoded(int cb, int x, int y, bool released, gmb_mouse_event_t* out);
int gmb_mouse_process_event(const gmb_mouse_event_t* e, gmb_mouse_action_t* out);
void gmb_mouse_reset_click_state(void);
int gmb_mouse_decode_and_process(const char* seq, gmb_mouse_action_t* out);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_MOUSE_H */
