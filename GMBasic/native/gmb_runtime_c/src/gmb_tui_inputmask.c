/* ============================================================================
 * gmb_tui_inputmask.c — InputMask.cs (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_inputmask.h"

gmb_input_mask_t gmb_inputmask_default(void)
{
    return GMB_IM_EXIT_ON_ENTER | GMB_IM_EXIT_ON_ESC | GMB_IM_EXIT_ON_TAB
         | GMB_IM_EXIT_ON_BACKTAB | GMB_IM_EXIT_ON_DOWN | GMB_IM_EXIT_ON_UP
         | GMB_IM_EXIT_ON_CTRL_END | GMB_IM_EXIT_ON_CTRL_HOME;
}
