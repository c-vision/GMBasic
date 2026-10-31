// FILE: IlProgramCompiler.Bits.cs — emissione IL del namespace BITS (2026-09-16).
// §0quater: file di dominio (mai nuovi case nel dispatcher centrale oltre l'hook).
// I metodi runtime stanno in IlRuntime.Bits.cs (firma: tutti i parametri long,
// ritorno long/bool) → ogni argomento passa da EmitInt64Argument.

using System.Reflection.Emit;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    /// <summary>Chiamata a un metodo BITS con `nargs` argomenti interi (long).</summary>
    private void EmitBitsCall(BuiltinFunctionCallExpr expr, string methodName, int nargs)
    {
        if (expr.Args.Length != nargs)
            throw new NotSupportedException(
                $"Backend IL: BITS.{expr.Name.Lexeme.ToUpperInvariant()} richiede {nargs} argomento/i.");
        for (int i = 0; i < nargs; i++) EmitInt64Argument(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, Enumerable.Repeat(typeof(long), nargs).ToArray()));
    }

    /// <summary>True se il TokenKind appartiene al namespace BITS (hook del dispatcher).</summary>
    private bool TryEmitBitsCall(BuiltinFunctionCallExpr expr)
    {
        switch (expr.Name.Kind)
        {
            // Tier 1 — elementari
            case TokenKind.BitCount: EmitBitsCall(expr, nameof(IlRuntime.BitCount), 1); return true;
            case TokenKind.BitGet: EmitBitsCall(expr, nameof(IlRuntime.BitGet), 2); return true;
            case TokenKind.BitSet: EmitBitsCall(expr, nameof(IlRuntime.BitSet), 2); return true;
            case TokenKind.BitClear: EmitBitsCall(expr, nameof(IlRuntime.BitClear), 2); return true;
            case TokenKind.BitToggle: EmitBitsCall(expr, nameof(IlRuntime.BitToggle), 2); return true;
            case TokenKind.BitTest: EmitBitsCall(expr, nameof(IlRuntime.BitTest), 2); return true;
            // Tier 2 — campi, rotazioni, codifiche
            case TokenKind.BitField: EmitBitsCall(expr, nameof(IlRuntime.BitField), 3); return true;
            case TokenKind.BitReverse: EmitBitsCall(expr, nameof(IlRuntime.BitReverse), 2); return true;
            case TokenKind.BitRotateLeft: EmitBitsCall(expr, nameof(IlRuntime.BitRotateLeft), 3); return true;
            case TokenKind.BitRotateRight: EmitBitsCall(expr, nameof(IlRuntime.BitRotateRight), 3); return true;
            case TokenKind.GrayCode: EmitBitsCall(expr, nameof(IlRuntime.GrayCode), 1); return true;
            case TokenKind.FromGray: EmitBitsCall(expr, nameof(IlRuntime.FromGray), 1); return true;
            case TokenKind.BitParity: EmitBitsCall(expr, nameof(IlRuntime.BitParity), 1); return true;
            case TokenKind.BitScanFwd: EmitBitsCall(expr, nameof(IlRuntime.BitScanFwd), 1); return true;
            case TokenKind.BitScanRev: EmitBitsCall(expr, nameof(IlRuntime.BitScanRev), 1); return true;
            case TokenKind.ByteSwap: EmitBitsCall(expr, nameof(IlRuntime.ByteSwap), 1); return true;
            // Tier 3 — algoritmi
            case TokenKind.HammingDist: EmitBitsCall(expr, nameof(IlRuntime.HammingDist), 2); return true;
            case TokenKind.BitInterleave: EmitBitsCall(expr, nameof(IlRuntime.BitInterleave), 2); return true;
            case TokenKind.BitDeinterleave: EmitBitsCall(expr, nameof(IlRuntime.BitDeinterleave), 2); return true;
            case TokenKind.NextPow2: EmitBitsCall(expr, nameof(IlRuntime.NextPow2), 1); return true;
            case TokenKind.IsPow2: EmitBitsCall(expr, nameof(IlRuntime.IsPow2), 1); return true;
            default: return false;
        }
    }
}
