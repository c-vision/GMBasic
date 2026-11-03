/* gmb_eval_hook.c — registro del valutatore a runtime (compilatore+VM). */
#include "gmb_runtime.h"

static gmb_eval_env_fn g_eval_env = NULL;

void gmb_eval_register_env(gmb_eval_env_fn fn) { g_eval_env = fn; }

int gmb_eval_env(const char* src, int as_stmt,
                 gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                 void* env, gmb_val_t* out)
{
    if (!g_eval_env) return -1;
    return g_eval_env(src, as_stmt, get, set, env, out);
}
