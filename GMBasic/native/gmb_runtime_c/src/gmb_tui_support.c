/* ============================================================================
 * gmb_tui_support.c — Support.cs (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_support.h"

int gmb_support_adjust_text_len(int len, int max_len)
{
    if (max_len == 0)
    {
        int l = len > 20 ? len : 20;
        return l;
    }
    return max_len;
}

gmb_input_mask_t gmb_support_adjust_key_mask(gmb_input_mask_t mask)
{
    if (mask == GMB_IM_NONE) return gmb_inputmask_default();
    return mask;
}
