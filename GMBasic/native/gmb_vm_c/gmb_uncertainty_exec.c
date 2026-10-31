/* native/gmb_vm_c/gmb_uncertainty_exec.c — handler VM/C per Self-Consistency /
 * Semantic Entropy (func_code 3538-3540, BuiltInFunction.AICONSISTENCY/
 * AILEXICALENTROPY/AICONSISTENCYDETAIL).
 *
 * §0bis: nessuna logica qui — ogni handler CHIAMA il runtime C condiviso
 * (`gmb_uncertainty.c`: gmb_consistency / gmb_lexical_entropy /
 * gmb_consistency_detail_json). Il chunk arriva dal compiler (C# o C) con questi func_code,
 * quindi la VM/C deve implementarli: senza, l'esecuzione di un programma che usa
 * AI.LEXICALENTROPY moriva con "builtin 3539 non implementato" (divergenza da IL/PCode).
 */
#include "gmb_vm_core.h"
#include "gmb_uncertainty.h"

#include <stdlib.h>

void vmc_bf_uncertainty(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                        gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    switch (func_code)
    {
    case 3538: /* AI.CONSISTENCY(prompt [, options]) → risk 0..1 (NaN senza provider) */
        bout->result = gmb_consistency(vm_builtin_sarg(args, argc, 0),
                                       argc >= 2 ? vm_builtin_sarg(args, argc, 1) : NULL);
        break;
    case 3539: /* AI.LEXICALENTROPY(responses$) → entropia di Shannon (bit) */
        bout->result = gmb_lexical_entropy(vm_builtin_sarg(args, argc, 0));
        break;
    case 3540: /* AI.CONSISTENCYDETAIL$() → JSON dell'ultimo calcolo */
    {
        char* s = gmb_consistency_detail_json();
        gmb_obj_string_t* r = gmb_str_cstr((s && *s) ? s : "{}");
        free(s);
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)r;
        bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    default:
        bout->result = 0;
        break;
    }
}
