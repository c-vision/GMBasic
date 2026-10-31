// FILE: PCodeProcessor.Builtins.Bits.cs — namespace BITS (2026-09-16).
// Dispatch a TABELLA (Dictionary), non switch (§0quater). Delega a IlRuntime.Bits.cs:
// implementazione gestita unica, STESSA semantica del runtime C gmb_bits.c (§0bis).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>BITS: funzioni del namespace manipolazione bit.</summary>
    private static readonly HashSet<BuiltInFunction> BITSBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.BITCOUNT,
        BuiltInFunction.BITGET,
        BuiltInFunction.BITSET,
        BuiltInFunction.BITCLEAR,
        BuiltInFunction.BITTOGGLE,
        BuiltInFunction.BITTEST,
        BuiltInFunction.BITFIELD,
        BuiltInFunction.BITREVERSE,
        BuiltInFunction.BITROTATEL,
        BuiltInFunction.BITROTATER,
        BuiltInFunction.GRAYCODE,
        BuiltInFunction.FROMGRAY,
        BuiltInFunction.BITPARITY,
        BuiltInFunction.BITSCANFWD,
        BuiltInFunction.BITSCANREV,
        BuiltInFunction.BYTESWAP,
        BuiltInFunction.HAMMINGDIST,
        BuiltInFunction.BITINTERLEAVE,
        BuiltInFunction.BITDEINTERLEAVE,
        BuiltInFunction.NEXTPOW2,
        BuiltInFunction.ISPOW2,
    };

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _bitsDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BITSDispatch
        => _bitsDispatch ??= BuildBitsDispatch();

    private static long BitsArg(object?[] args, int i) => (long)SafeDouble(ArgOr(args, i));

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildBitsDispatch() => new()
    {
        [BuiltInFunction.BITCOUNT] = (t, a) => IlRuntime.BitCount(BitsArg(a, 0)),
        [BuiltInFunction.BITGET] = (t, a) => IlRuntime.BitGet(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITSET] = (t, a) => IlRuntime.BitSet(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITCLEAR] = (t, a) => IlRuntime.BitClear(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITTOGGLE] = (t, a) => IlRuntime.BitToggle(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITTEST] = (t, a) => IlRuntime.BitTest(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITFIELD] = (t, a) => IlRuntime.BitField(BitsArg(a, 0), BitsArg(a, 1), BitsArg(a, 2)),
        [BuiltInFunction.BITREVERSE] = (t, a) => IlRuntime.BitReverse(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITROTATEL] = (t, a) => IlRuntime.BitRotateLeft(BitsArg(a, 0), BitsArg(a, 1), BitsArg(a, 2)),
        [BuiltInFunction.BITROTATER] = (t, a) => IlRuntime.BitRotateRight(BitsArg(a, 0), BitsArg(a, 1), BitsArg(a, 2)),
        [BuiltInFunction.GRAYCODE] = (t, a) => IlRuntime.GrayCode(BitsArg(a, 0)),
        [BuiltInFunction.FROMGRAY] = (t, a) => IlRuntime.FromGray(BitsArg(a, 0)),
        [BuiltInFunction.BITPARITY] = (t, a) => IlRuntime.BitParity(BitsArg(a, 0)),
        [BuiltInFunction.BITSCANFWD] = (t, a) => IlRuntime.BitScanFwd(BitsArg(a, 0)),
        [BuiltInFunction.BITSCANREV] = (t, a) => IlRuntime.BitScanRev(BitsArg(a, 0)),
        [BuiltInFunction.BYTESWAP] = (t, a) => IlRuntime.ByteSwap(BitsArg(a, 0)),
        [BuiltInFunction.HAMMINGDIST] = (t, a) => IlRuntime.HammingDist(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITINTERLEAVE] = (t, a) => IlRuntime.BitInterleave(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.BITDEINTERLEAVE] = (t, a) => IlRuntime.BitDeinterleave(BitsArg(a, 0), BitsArg(a, 1)),
        [BuiltInFunction.NEXTPOW2] = (t, a) => IlRuntime.NextPow2(BitsArg(a, 0)),
        [BuiltInFunction.ISPOW2] = (t, a) => IlRuntime.IsPow2(BitsArg(a, 0)),
    };

    private object? ExecuteBuiltInBits(PCodeTask task, BuiltInFunction fn, object?[] args)
        => BITSDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInBits (fn={fn}).");
}
