/* gmb_tui_inputmask.h — InputMask.cs (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_INPUTMASK_H
#define GMB_TUI_INPUTMASK_H
#ifdef __cplusplus
extern "C" {
#endif
#define GMB_IM_NONE             0u
#define GMB_IM_EXIT_ON_ENTER    (1u << 0)
#define GMB_IM_EXIT_ON_ESC      (1u << 1)
#define GMB_IM_EXIT_ON_TAB      (1u << 2)
#define GMB_IM_EXIT_ON_BACKTAB  (1u << 3)
#define GMB_IM_EXIT_ON_DOWN     (1u << 4)
#define GMB_IM_EXIT_ON_UP       (1u << 5)
#define GMB_IM_EXIT_ON_CTRL_END (1u << 6)
#define GMB_IM_EXIT_ON_CTRL_HOME (1u << 7)
#define GMB_IM_UPPER_CASE       (1u << 8)
#define GMB_IM_LOWER_CASE       (1u << 9)
#define GMB_IM_AUTO_CANCEL      (1u << 10)
typedef unsigned int gmb_input_mask_t;
gmb_input_mask_t gmb_inputmask_default(void);
#ifdef __cplusplus
}
#endif
#endif
