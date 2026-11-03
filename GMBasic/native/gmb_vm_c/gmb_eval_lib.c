/* gmb_eval_lib.c — libreria CONDIVISA del valutatore a runtime (libgmb_eval).
 *
 * Contiene il compilatore C + la VM minima del driver EVAL: è caricata su
 * richiesta (dlopen) dai contesti che non hanno una VM nel proprio processo
 * (runtime condiviso per filtri DBF/SPEED, programmi C99Native). Così il
 * compilatore NON è linkato in ogni binario: viene usato solo quando serve
 * davvero valutare un frammento a runtime.
 */
#include "gmb_vm_eval.h"
#include "gmb_runtime.h"

#if defined(_WIN32)
#define GMB_EVAL_EXPORT __declspec(dllexport)
#else
#define GMB_EVAL_EXPORT __attribute__((visibility("default")))
#endif

/* Stessa firma di gmb_eval_env_fn: il runtime la usa via dlsym. */
GMB_EVAL_EXPORT int gmb_eval_run_env(const char* src, int as_stmt,
                                     gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                                     void* env, gmb_val_t* out)
{
    return gmb_vm_eval_text_standalone(src, as_stmt, get, set, env, out);
}

/* Versione/ABI: il runtime rifiuta una dylib con ABI diversa (mai semantica
 * silenziosamente diversa). */
GMB_EVAL_EXPORT int gmb_eval_abi(void) { return 1; }
