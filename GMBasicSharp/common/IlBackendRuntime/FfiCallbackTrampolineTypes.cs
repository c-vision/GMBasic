// FILE: IlBackendRuntime/FfiCallbackTrampolineTypes.cs
// Tipi di delegate CONDIVISI tra PCodeVM (PCodeProcessor.FfiCallback.cs) e IL
// (IlRuntime.FfiCallback.cs) per il trampolino nativo del Callback come parametro FFI
// (2026-09-29, v1: arità 0-4, firma interna omogenea per kind, BYVAL — vedi AiWiki
// Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §3bis). 19 tipi pre-
// dichiarati (5 arità 0-4 × 2 kind INT/DOUBLE per le FUNCTION, + arità 0 condivisa e 4
// arità 1-4 × 2 kind per le SUB) — ognuno taggato UnmanagedFunctionPointer(Cdecl): il CLR
// genera un vero thunk nativo PER OGNI ISTANZA di un delegate così taggato (Marshal.
// GetFunctionPointerForDelegate), richiamabile in modo sincrono e rientrante da codice
// nativo — nessun pool di trampolini pre-compilati serve qui (a differenza di VM/C, che è
// un interprete puro senza generazione di codice a runtime).

using System.Runtime.InteropServices;

namespace Basic.IlBackend;

internal static class FfiCallbackTrampolineTypes
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate long CbFnI0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate long CbFnI1(long a0);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate long CbFnI2(long a0, long a1);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate long CbFnI3(long a0, long a1, long a2);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate long CbFnI4(long a0, long a1, long a2, long a3);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate double CbFnD0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate double CbFnD1(double a0);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate double CbFnD2(double a0, double a1);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate double CbFnD3(double a0, double a1, double a2);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate double CbFnD4(double a0, double a1, double a2, double a3);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSub0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubI1(long a0);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubI2(long a0, long a1);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubI3(long a0, long a1, long a2);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubI4(long a0, long a1, long a2, long a3);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubD1(double a0);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubD2(double a0, double a1);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubD3(double a0, double a1, double a2);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void CbSubD4(double a0, double a1, double a2, double a3);

    internal static long ToI64(object? v) => v switch { long l => l, int i => i, double d => (long)d, null => 0L, _ => Convert.ToInt64(v) };
    internal static double ToF64(object? v) => v switch { double d => d, long l => l, int i => i, null => 0.0, _ => Convert.ToDouble(v) };
}
