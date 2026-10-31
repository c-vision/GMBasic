// FILE: StdLib.Bits.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Bits — namespace 'BITS'
 * 21 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (21):
 *   BITCLEAR
 *   BITCOUNT
 *   BITDEINTERLEAVE
 *   BITFIELD
 *   BITGET
 *   BITINTERLEAVE
 *   BITREVERSE
 *   BITROTATEL
 *   BITROTATER
 *   BITSCANFWD
 *   BITSCANREV
 *   BITSET
 *   BITTEST
 *   BITTOGGLE
 *   BYTESWAP
 *   FROMGRAY
 *   GRAYCODE
 *   HAMMINGDIST
 *   ISPOW2
 *   NEXTPOW2
 *   PARITY
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- BITS NAMESPACE (manipolazione bit a 64 bit) ---
    // Implementazione unica nel runtime condiviso C (native/gmb_runtime_c/src/
    // gmb_bits.c) per VM/C, C99Native; controparte gestita in
    // CommonLib/Numerics/BitsFunctions.cs per IL/PCodeVM — STESSA semantica
    // (indici 0-based, predicati → -1/0, casi limite definiti).
    // ========================================================================

    private void CreateBitsNamespace()
    {
        const string namespaceName = "BITS";
        const SymbolFlags pub = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;

        var longParam = new Parameter(-1, T(TokenKind.Identifier, "x"), new LongType(), noFlags, null, false);
        var nParam = new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(), noFlags, null, false);
        var loParam = new Parameter(-1, T(TokenKind.Identifier, "lo"), new LongType(), noFlags, null, false);
        var hiParam = new Parameter(-1, T(TokenKind.Identifier, "hi"), new LongType(), noFlags, null, false);
        var widthParam = new Parameter(-1, T(TokenKind.Identifier, "width"), new LongType(), noFlags, null, false);
        var whichParam = new Parameter(-1, T(TokenKind.Identifier, "which"), new LongType(), noFlags, null, false);
        var aParam = new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false);
        var bParam = new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            // ── Tier 1: operazioni elementari ──────────────────────────────────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitCount, "BITCOUNT"),
                [longParam], new LongType(), TokenKind.BitCount),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitGet, "BITGET"),
                [longParam, nParam], new LongType(), TokenKind.BitGet),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitSet, "BITSET"),
                [longParam, nParam], new LongType(), TokenKind.BitSet),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitClear, "BITCLEAR"),
                [longParam, nParam], new LongType(), TokenKind.BitClear),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitToggle, "BITTOGGLE"),
                [longParam, nParam], new LongType(), TokenKind.BitToggle),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitTest, "BITTEST"),
                [longParam, nParam], new BitType(), TokenKind.BitTest),

            // ── Tier 2: campi, rotazioni, codifiche ────────────────────────────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitField, "BITFIELD"),
                [longParam, loParam, hiParam], new LongType(), TokenKind.BitField),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitReverse, "BITREVERSE"),
                [longParam, widthParam], new LongType(), TokenKind.BitReverse),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitRotateLeft, "BITROTATEL"),
                [longParam, nParam, widthParam], new LongType(), TokenKind.BitRotateLeft),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitRotateRight, "BITROTATER"),
                [longParam, nParam, widthParam], new LongType(), TokenKind.BitRotateRight),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.GrayCode, "GRAYCODE"),
                [longParam], new LongType(), TokenKind.GrayCode),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.FromGray, "FROMGRAY"),
                [longParam], new LongType(), TokenKind.FromGray),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitParity, "PARITY"),
                [longParam], new LongType(), TokenKind.BitParity),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitScanFwd, "BITSCANFWD"),
                [longParam], new LongType(), TokenKind.BitScanFwd),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitScanRev, "BITSCANREV"),
                [longParam], new LongType(), TokenKind.BitScanRev),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.ByteSwap, "BYTESWAP"),
                [longParam], new LongType(), TokenKind.ByteSwap),

            // ── Tier 3: algoritmi ─────────────────────────────────────────────
            new NativeProcedureDefinition(-1, pub, T(TokenKind.HammingDist, "HAMMINGDIST"),
                [aParam, bParam], new LongType(), TokenKind.HammingDist),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitInterleave, "BITINTERLEAVE"),
                [aParam, bParam], new LongType(), TokenKind.BitInterleave),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BitDeinterleave, "BITDEINTERLEAVE"),
                [longParam, whichParam], new LongType(), TokenKind.BitDeinterleave),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.NextPow2, "NEXTPOW2"),
                [longParam], new LongType(), TokenKind.NextPow2),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.IsPow2, "ISPOW2"),
                [longParam], new BitType(), TokenKind.IsPow2),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
