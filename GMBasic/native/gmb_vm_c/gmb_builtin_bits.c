/* gmb_builtin_bits.c — handler VMC del namespace BITS (manipolazione bit a 64 bit).
 * §0quater: file di dominio (i 21 case non stanno nel dispatcher centrale).
 * §0bis: nessuna logica inline — si chiama il runtime condiviso gmb_bits.c, la
 * stessa implementazione usata da C99Native.
 *
 * Il VMC dispatcha per func_code = valore dell'enum BuiltInFunction (BITCOUNT=3571 …
 * ISPOW2=3591): la tabella in gmb_builtin.c mappa ogni codice a gmb_bf_bits.
 * Convenzione argomenti: args[i].num (i numerici BASIC sono double); i predicati
 * (BITTEST, ISPOW2) ritornano la convenzione Bit -1/0.
 */
#include "gmb_vm_core.h"
#include "gmb_bits.h"

void gmb_bf_bits(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                 gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    gmb_int_t a0 = (argc >= 1) ? (gmb_int_t)args[0].num : 0;
    gmb_int_t a1 = (argc >= 2) ? (gmb_int_t)args[1].num : 0;
    gmb_int_t a2 = (argc >= 3) ? (gmb_int_t)args[2].num : 0;

    switch (func_code) {
        /* ── Tier 1 ─────────────────────────────────────────────────────────── */
        case 3571: bout->result = (double)gmb_bit_count(a0); break;
        case 3572: bout->result = (double)gmb_bit_get(a0, a1); break;
        case 3573: bout->result = (double)gmb_bit_set(a0, a1); break;
        case 3574: bout->result = (double)gmb_bit_clear(a0, a1); break;
        case 3575: bout->result = (double)gmb_bit_toggle(a0, a1); break;
        case 3576: bout->result = (double)gmb_bit_test(a0, a1); break;
        /* ── Tier 2 ─────────────────────────────────────────────────────────── */
        case 3577: bout->result = (double)gmb_bit_field(a0, a1, a2); break;
        case 3578: bout->result = (double)gmb_bit_reverse(a0, a1); break;
        case 3579: bout->result = (double)gmb_bit_rotate_left(a0, a1, a2); break;
        case 3580: bout->result = (double)gmb_bit_rotate_right(a0, a1, a2); break;
        case 3581: bout->result = (double)gmb_gray_code(a0); break;
        case 3582: bout->result = (double)gmb_from_gray(a0); break;
        case 3583: bout->result = (double)gmb_bit_parity(a0); break;
        case 3584: bout->result = (double)gmb_bit_scan_fwd(a0); break;
        case 3585: bout->result = (double)gmb_bit_scan_rev(a0); break;
        case 3586: bout->result = (double)gmb_byte_swap(a0); break;
        /* ── Tier 3 ─────────────────────────────────────────────────────────── */
        case 3587: bout->result = (double)gmb_hamming_dist(a0, a1); break;
        case 3588: bout->result = (double)gmb_bit_interleave(a0, a1); break;
        case 3589: bout->result = (double)gmb_bit_deinterleave(a0, a1); break;
        case 3590: bout->result = (double)gmb_next_pow2(a0); break;
        case 3591: bout->result = (double)gmb_is_pow2(a0); break;
        default: bout->result = 0; break;
    }
}
