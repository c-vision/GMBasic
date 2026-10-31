/* gmb_c99_emit_bits.c — namespace BITS (manipolazione bit a 64 bit) nel C99Native.
 * §0quater: file di dominio, tabella dichiarativa (nessuno switch monolitico).
 * §0bis: l'implementazione vive SOLO nel runtime condiviso (gmb_bits.c); qui si
 * emette la chiamata. Ritorno come gmb_double_t (numerici BASIC = double),
 * argomenti come gmb_int_t (le gmb_bit_* lavorano a 64 bit).
 */

#include "gmb_c99_emit.h"
#include "gmb_c99_emit_internal.h"

/* Argomento i-esimo come gmb_int_t (0 se assente: la firma StdLib è fissa). */
static void bits_arg(ctx_t* c, gmb_expr_t** a, int n, int i) {
    sb_put(&c->out, "(gmb_int_t)(");
    if (i < n && a[i]) emit_expr(c, a[i]); else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
}

static int bits_n(ctx_t* c, gmb_expr_t** a, int n, int want, const char* fn) {
    if (n < want) return 0;
    sb_put(&c->out, "((gmb_double_t)");
    sb_put(&c->out, fn);
    sb_put(&c->out, "(");
    for (int i = 0; i < want; i++) {
        if (i) sb_put(&c->out, ", ");
        bits_arg(c, a, n, i);
    }
    sb_put(&c->out, "))");
    return 1;
}

static int bt1(ctx_t* c, gmb_expr_t** a, int n, const char* fn) { return bits_n(c, a, n, 1, fn); }
static int bt2(ctx_t* c, gmb_expr_t** a, int n, const char* fn) { return bits_n(c, a, n, 2, fn); }
static int bt3(ctx_t* c, gmb_expr_t** a, int n, const char* fn) { return bits_n(c, a, n, 3, fn); }

typedef struct { gmb_tokenkind_t tk; int (*sp)(ctx_t*, gmb_expr_t**, int, const char*); const char* fn; } bits_row_t;
static const bits_row_t BITS_TBL[] = {
    /* Tier 1 */
    { GMB_TOK_BitCount,        bt1, "gmb_bit_count" },
    { GMB_TOK_BitGet,          bt2, "gmb_bit_get" },
    { GMB_TOK_BitSet,          bt2, "gmb_bit_set" },
    { GMB_TOK_BitClear,        bt2, "gmb_bit_clear" },
    { GMB_TOK_BitToggle,       bt2, "gmb_bit_toggle" },
    { GMB_TOK_BitTest,         bt2, "gmb_bit_test" },
    /* Tier 2 */
    { GMB_TOK_BitField,        bt3, "gmb_bit_field" },
    { GMB_TOK_BitReverse,      bt2, "gmb_bit_reverse" },
    { GMB_TOK_BitRotateLeft,   bt3, "gmb_bit_rotate_left" },
    { GMB_TOK_BitRotateRight,  bt3, "gmb_bit_rotate_right" },
    { GMB_TOK_GrayCode,        bt1, "gmb_gray_code" },
    { GMB_TOK_FromGray,        bt1, "gmb_from_gray" },
    { GMB_TOK_BitParity,       bt1, "gmb_bit_parity" },
    { GMB_TOK_BitScanFwd,      bt1, "gmb_bit_scan_fwd" },
    { GMB_TOK_BitScanRev,      bt1, "gmb_bit_scan_rev" },
    { GMB_TOK_ByteSwap,        bt1, "gmb_byte_swap" },
    /* Tier 3 */
    { GMB_TOK_HammingDist,     bt2, "gmb_hamming_dist" },
    { GMB_TOK_BitInterleave,   bt2, "gmb_bit_interleave" },
    { GMB_TOK_BitDeinterleave, bt2, "gmb_bit_deinterleave" },
    { GMB_TOK_NextPow2,        bt1, "gmb_next_pow2" },
    { GMB_TOK_IsPow2,          bt1, "gmb_is_pow2" },
};

int emit_bf_bits(ctx_t* c, const char* name, const char* ns,
                 gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    c->uses_bits = 1;   /* prologo: #include "gmb_bits.h" */
    for (size_t i = 0; i < sizeof(BITS_TBL) / sizeof(BITS_TBL[0]); i++)
        if (BITS_TBL[i].tk == tk) return BITS_TBL[i].sp(c, args, nargs, BITS_TBL[i].fn);
    return 0;
}
